// HUD tests (02_Systems/hud.md, T-HUD-01 to T-HUD-10; S4 wave 1 package F, D-043).
// The layout is a pure function, so every test here runs without a canvas: text is measured with the engine font the HUD
// draws with, through the same engine call, and the strings come from the table CSVs in both cultures.
// Run headless: & Tools\RunTests.ps1 -Filter Dusktracer.Hud

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRunState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Game/DTCameraPawn.h"
#include "Game/DTHUD.h"
#include "Game/DTHudLayout.h"
#include "Game/DTUIStyle.h"
#include "Keepers/DTKeeperSpec.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/Csv/CsvParser.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTHudTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	// Pixels: rects may touch, and a measured size is truncated to whole pixels.
	constexpr float Touch = 0.01f;
	constexpr float Inside = 0.51f;
	// The enemy hue band, as Tools/palette_lint.py and Dusktracer.Weapons.Palette draw it (T-PRE-02).
	constexpr float BandMinSat = 0.25f;
	constexpr float BandFromDeg = 290.0f;
	constexpr float BandToDeg = 20.0f;

	struct FView
	{
		const TCHAR* Name;
		FVector2f Size;
	};
	// The smoke window, the target (C.BUDGET.RES), 1440 lines, and two ultrawide windows.
	const FView Views[] = { { TEXT("1280x720"), FVector2f(1280.0f, 720.0f) }, { TEXT("1920x1080"), FVector2f(1920.0f, 1080.0f) },
		{ TEXT("2560x1440"), FVector2f(2560.0f, 1440.0f) }, { TEXT("2560x1080"), FVector2f(2560.0f, 1080.0f) }, { TEXT("3440x1440"), FVector2f(3440.0f, 1440.0f) } };
	const TCHAR* const Cultures[] = { TEXT("en"), TEXT("zh-Hans") };

	/** Every text size the setting offers (settings.md R-SET-12): the default, the steps between and Large. */
	static TArray<float> TextScales()
	{
		TArray<float> Out;
		for (int32 Pct = DTConst::SETTINGS_TEXT_MIN_PCT; Pct <= DTConst::SETTINGS_TEXT_MAX_PCT; Pct += DTConst::SETTINGS_TEXT_STEP_PCT)
		{
			Out.Add(static_cast<float>(Pct) / DTConst::SETTINGS_TEXT_MIN_PCT);
		}
		return Out;
	}

	/** A line as the HUD measures it: UCanvas::TextSize is this call. */
	static FVector2f Measure(const FString& Line, float Scale)
	{
		int32 Width = 0;
		int32 Height = 0;
		UCanvas::ClippedStrLen(GEngine ? GEngine->GetLargeFont() : nullptr, Scale, Scale, Width, Height, Line);
		return FVector2f(static_cast<float>(Width), static_cast<float>(Height));
	}

	/** A string table's source CSV: key to (en, zh-Hans). */
	struct FTable
	{
		TMap<FString, TPair<FString, FString>> Rows;

		explicit FTable(const TCHAR* Name)
		{
			FString Csv;
			if (FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DT/Text"), FString(Name) + TEXT(".csv"))))
			{
				const FCsvParser Parser(Csv);
				for (const TArray<const TCHAR*>& Row : Parser.GetRows())
				{
					if (Row.Num() >= 3)
					{
						Rows.Add(Row[0], TPair<FString, FString>(Row[1], Row[2]));
					}
				}
			}
		}

		bool Has(const TCHAR* Key) const { return Rows.Contains(Key); }
		FString Get(const FString& Key, int32 Culture) const
		{
			const TPair<FString, FString>* Row = Rows.Find(Key);
			return Row ? (Culture == 0 ? Row->Key : Row->Value) : FString();
		}
		/** The widest row whose key ends with Suffix, as drawn. */
		FString Widest(const TCHAR* Suffix, int32 Culture) const
		{
			FString Best;
			float BestWidth = -1.0f;
			for (const TPair<FString, TPair<FString, FString>>& Row : Rows)
			{
				if (Row.Key.EndsWith(Suffix))
				{
					const FString Text = Culture == 0 ? Row.Value.Key : Row.Value.Value;
					const float Width = Measure(Text, 1.0f).X;
					if (Width > BestWidth)
					{
						BestWidth = Width;
						Best = Text;
					}
				}
			}
			return Best;
		}
	};

	struct FArg
	{
		const TCHAR* Name;
		FString Value;
	};

	static FText Fill(const FString& Format, std::initializer_list<FArg> Values)
	{
		FFormatNamedArguments Args;
		for (const FArg& Value : Values)
		{
			Args.Add(Value.Name, FText::FromString(Value.Value));
		}
		return FText::Format(FTextFormat::FromString(Format), Args);
	}

	/** The tables the HUD reads. */
	struct FTables
	{
		FTable Ui{ TEXT("ST_UI") };
		FTable Enemies{ TEXT("ST_Enemies") };
		FTable Reactions{ TEXT("ST_Reactions") };
		FTable Modifiers{ TEXT("ST_Modifiers") };
		FTable Weapons{ TEXT("ST_Weapons") };

		/** The boss's name wherever the branch keeps it: ST_Enemies once boss.md's move has landed, ST_UI before it. */
		FString BossName(int32 Culture) const
		{
			return Enemies.Has(TEXT("Enemy.Boss.Name")) ? Enemies.Get(TEXT("Enemy.Boss.Name"), Culture) : Ui.Get(TEXT("Hud.BossName"), Culture);
		}
		FText Toast(const FString& Pair, int32 Culture) const
		{
			return Fill(Reactions.Get(TEXT("Toast.Format"), Culture), { { TEXT("Name"), Reactions.Get(Pair + TEXT(".Name"), Culture) }, { TEXT("Toast"), Reactions.Get(Pair + TEXT(".Toast"), Culture) } });
		}
		TArray<FString> ReactionPairs() const
		{
			TArray<FString> Out;
			for (const TPair<FString, TPair<FString, FString>>& Row : Reactions.Rows)
			{
				if (Row.Key.EndsWith(TEXT(".Toast")))
				{
					Out.Add(Row.Key.LeftChop(6));
				}
			}
			Out.Sort();
			return Out;
		}
		FText WidestToast(int32 Culture) const
		{
			FText Best;
			float BestWidth = -1.0f;
			for (const FString& Pair : ReactionPairs())
			{
				const FText Line = Toast(Pair, Culture);
				const float Width = Measure(Line.ToString(), 1.0f).X;
				if (Width > BestWidth)
				{
					BestWidth = Width;
					Best = Line;
				}
			}
			return Best;
		}
	};

	/**
	 * A run with every readout up at once and the widest text each can hold. bBuild adds what a build grows out of the
	 * bottom-left block: four Keeper meters, the aim line and three dormant sockets with the longest names.
	 */
	static FDTHudContent Content(const FTables& T, int32 Culture, bool bBuild, bool bBossBanner = true)
	{
		const auto Ui = [&](const TCHAR* Key) { return T.Ui.Get(Key, Culture); };
		FDTHudContent Out;
		FDTHudPlayerContent& P = Out.Players.AddDefaulted_GetRef();
		P.Hp = Fill(Ui(TEXT("Hud.Hp")), { { TEXT("Hp"), TEXT("1,000") }, { TEXT("MaxHp"), TEXT("1,000") } });
		P.Level = Fill(Ui(TEXT("Level.Label")), { { TEXT("Level"), TEXT("30") } });
		P.Xp = Fill(Ui(TEXT("Level.Tooltip")), { { TEXT("XP"), TEXT("1,234") }, { TEXT("XPToNext"), TEXT("5,678") } });
		P.LevelUp = FText::FromString(Ui(TEXT("Level.Up")));
		P.Pending = Fill(Ui(TEXT("Level.Pending")), { { TEXT("Count"), TEXT("12") } });
		P.Intercession = FText::FromString(Ui(TEXT("Hud.IntercessionReady")));
		P.Dash = FText::FromString(Ui(TEXT("Hud.Dash")));
		P.DashPips = DTConst::DASH_CHARGES + 2;
		for (const TCHAR* Key : { TEXT("Offer.Tool.Reroll"), TEXT("Offer.Tool.Banish"), TEXT("Offer.Tool.Lock") })
		{
			P.Tools.Add(Fill(Ui(TEXT("Hud.Tool")), { { TEXT("Tool"), Ui(Key) }, { TEXT("Count"), TEXT("9") } }));
		}
		if (bBuild)
		{
			for (const TCHAR* Key : { TEXT("Hud.Debt"), TEXT("Hud.Momentum"), TEXT("Hud.Shades"), TEXT("Hud.Dilation") })
			{
				P.Meters.Add({ Fill(Ui(Key), { { TEXT("Value"), TEXT("30") }, { TEXT("Max"), TEXT("30") } }), true });
			}
			P.AimMode = FText::FromString(Ui(TEXT("Hud.AimMode")));
			const FText Dormant = Fill(T.Modifiers.Get(TEXT("Dormant.Hud"), Culture),
				{ { TEXT("Modifier"), T.Modifiers.Widest(TEXT(".Name"), Culture) }, { TEXT("Weapon"), T.Weapons.Widest(TEXT(".Name"), Culture) } });
			P.Dormant.Init(Dormant, DTConst::MOD_SOCKETS_PER_WEAPON);
		}
		Out.DevLines.Add(FText::FromString(Ui(TEXT("Test.LocRoundTrip"))));
		Out.DevLines.Add(FText::FromString(TEXT("120 fps  8.33 ms frame  12.34 ms game")));
		Out.Clock = DTHudText::Clock(static_cast<int64>(88 * 60 + 88) * DTConst::SIM_FIXED_HZ);
		Out.BossName = FText::FromString(T.BossName(Culture));
		const FString Threat = Ui(TEXT("Hud.Threat"));
		const FString Incoming = Ui(TEXT("Hud.BossIncoming"));
		Out.Banner = FText::FromString(bBossBanner || Measure(Incoming, 1.0f).X > Measure(Threat, 1.0f).X ? Incoming : Threat);
		Out.Toast = T.WidestToast(Culture);
		return Out;
	}

	static const TCHAR* KindName(EDTHudElem Kind)
	{
		static const TCHAR* const Names[] = { TEXT("HpText"), TEXT("HpBar"), TEXT("DevLine"), TEXT("BossName"), TEXT("BossBar"), TEXT("Banner"), TEXT("Toast"),
			TEXT("Clock"), TEXT("Level"), TEXT("XpBar"), TEXT("XpText"), TEXT("LevelUp"), TEXT("Pending"), TEXT("Ring"), TEXT("IntercessionLabel"), TEXT("DashLabel"),
			TEXT("DashPip"), TEXT("ToolPanel"), TEXT("ToolLine"), TEXT("MeterLabel"), TEXT("MeterBar"), TEXT("AimMode"), TEXT("DormantPanel"), TEXT("DormantLine") };
		return Names[FMath::Clamp(static_cast<int32>(Kind), 0, static_cast<int32>(UE_ARRAY_COUNT(Names)) - 1)];
	}

	static FString Describe(const FDTHudRect& Rect)
	{
		return FString::Printf(TEXT("%s[%d.%d] (%.0f,%.0f)-(%.0f,%.0f)"), KindName(Rect.Kind), Rect.Index, Rect.Line, Rect.Box.Min.X, Rect.Box.Min.Y, Rect.Box.Max.X, Rect.Box.Max.Y);
	}

	static bool Overlap(const FBox2f& A, const FBox2f& B)
	{
		return A.Min.X < B.Max.X - Touch && B.Min.X < A.Max.X - Touch && A.Min.Y < B.Max.Y - Touch && B.Min.Y < A.Max.Y - Touch;
	}

	static bool Contains(const FBox2f& Outer, const FBox2f& Inner)
	{
		return Inner.Min.X >= Outer.Min.X - Inside && Inner.Min.Y >= Outer.Min.Y - Inside && Inner.Max.X <= Outer.Max.X + Inside && Inner.Max.Y <= Outer.Max.Y + Inside;
	}

	static bool InEnemyBand(const FColor& Token)
	{
		const FLinearColor Hsv = FLinearColor(Token.R / 255.0f, Token.G / 255.0f, Token.B / 255.0f).LinearRGBToHSV();
		return Hsv.G >= BandMinSat && (Hsv.R >= BandFromDeg || Hsv.R <= BandToDeg);
	}

	static float Luminance(const FColor& Token)
	{
		const FLinearColor Linear(Token);
		return 0.2126f * Linear.R + 0.7152f * Linear.G + 0.0722f * Linear.B;
	}

	/** True when the engine font measures here; every layout test needs it. */
	static bool FontMeasures(FAutomationTestBase& Test)
	{
		const FVector2f Latin = Measure(TEXT("Hg"), 1.0f);
		const FVector2f Han = Measure(TEXT("典狱"), 1.0f);
		return Test.TestTrue(TEXT("the engine font measures Latin text"), Latin.X > 0.0f && Latin.Y > 0.0f)
			&& Test.TestTrue(TEXT("the engine font measures Han text"), Han.X > 0.0f && Han.Y > 0.0f);
	}

	/** Runs Body on every window, culture and text size. */
	template <typename FBody>
	static void Walk(const FTables& T, bool bBuild, FBody Body)
	{
		for (const FView& View : Views)
		{
			for (int32 Culture = 0; Culture < UE_ARRAY_COUNT(Cultures); ++Culture)
			{
				for (const float Scale : TextScales())
				{
					const FDTHudContent Shown = Content(T, Culture, bBuild);
					const FDTHudLayout Layout = DTHud::Layout(View.Size, Scale, Shown, &Measure);
					Body(FString::Printf(TEXT("%s %s text %.0f%%: "), View.Name, Cultures[Culture], Scale * 100.0f), View, Culture, Scale, Layout);
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudOverlap, "Dusktracer.Hud.Layout.Overlap", DTHudTest::Flags) // T-HUD-01
bool FDTHudOverlap::RunTest(const FString& Parameters)
{
	using namespace DTHudTest;
	if (!FontMeasures(*this))
	{
		return false;
	}
	const FTables Tables;
	TestTrue(TEXT("the tables read"), Tables.Ui.Has(TEXT("Hud.Hp")) && Tables.Reactions.Has(TEXT("Toast.Format")) && Tables.Modifiers.Has(TEXT("Dormant.Hud")));
	int32 Cases = 0;
	Walk(Tables, true, [&](const FString& Tag, const FView& View, int32 Culture, float Scale, const FDTHudLayout& Layout)
	{
		++Cases;
		int32 Bad = 0;
		for (int32 A = 0; A < Layout.Rects.Num(); ++A)
		{
			const FDTHudRect& First = Layout.Rects[A];
			if (First.Box.Min.X < -Touch || First.Box.Min.Y < -Touch || First.Box.Max.X > View.Size.X + Touch || First.Box.Max.Y > View.Size.Y + Touch)
			{
				AddError(Tag + TEXT("off screen: ") + Describe(First));
				++Bad;
			}
			if (First.Box.GetSize().X <= 0.0f || First.Box.GetSize().Y <= 0.0f)
			{
				AddError(Tag + TEXT("empty: ") + Describe(First));
				++Bad;
			}
			if (First.Parent != INDEX_NONE && !Contains(Layout.Rects[First.Parent].Box, First.Box))
			{
				AddError(Tag + Describe(First) + TEXT(" leaves its panel ") + Describe(Layout.Rects[First.Parent]));
				++Bad;
			}
			for (int32 B = A + 1; B < Layout.Rects.Num(); ++B)
			{
				const FDTHudRect& Second = Layout.Rects[B];
				if (First.Parent == B || Second.Parent == A)
				{
					continue;
				}
				if (Overlap(First.Box, Second.Box))
				{
					AddError(Tag + Describe(First) + TEXT(" overlaps ") + Describe(Second));
					++Bad;
				}
			}
		}
		// Every readout of the content is placed: nothing is dropped to make room.
		const int32 Expected = 17 + (DTConst::DASH_CHARGES + 2) + 4 + 4 * 2 + 1 + 2 * DTConst::MOD_SOCKETS_PER_WEAPON;
		TestTrue(*(Tag + FString::Printf(TEXT("every readout is placed (%d rects, %d bad)"), Layout.Rects.Num(), Bad)), Layout.Rects.Num() >= Expected && Bad == 0);
	});
	TestEqual(TEXT("windows x cultures x text sizes"), Cases, static_cast<int32>(UE_ARRAY_COUNT(Views) * UE_ARRAY_COUNT(Cultures)) * TextScales().Num());

	// Every reaction's toast, at every window and text size, is drawn at its full size on one line.
	for (const FView& View : Views)
	{
		for (int32 Culture = 0; Culture < UE_ARRAY_COUNT(Cultures); ++Culture)
		{
			for (const FString& Pair : Tables.ReactionPairs())
			{
				FDTHudContent Shown = Content(Tables, Culture, false);
				Shown.Toast = Tables.Toast(Pair, Culture);
				const FDTHudLayout Layout = DTHud::Layout(View.Size, TextScales().Last(), Shown, &Measure);
				const FDTHudRect* Toast = Layout.Find(EDTHudElem::Toast);
				if (!Toast || Toast->TextMul < 1.0f)
				{
					AddError(FString::Printf(TEXT("%s %s: the toast of %s is shrunk or missing at the largest text"), View.Name, Cultures[Culture], *Pair));
				}
			}
		}
	}

	// Two players stack and still clear each other on the target window, with the readouts every run shows. The co-op frame
	// is coop.md's, not this test's: two full builds do not fit one left column.
	{
		FDTHudContent Two = Content(Tables, 0, false);
		const FDTHudPlayerContent Second = Two.Players[0];
		Two.Players.Add(Second);
		const FDTHudLayout Layout = DTHud::Layout(Views[1].Size, 1.0f, Two, &Measure);
		int32 Bad = 0;
		for (int32 A = 0; A < Layout.Rects.Num(); ++A)
		{
			for (int32 B = A + 1; B < Layout.Rects.Num(); ++B)
			{
				Bad += Layout.Rects[A].Parent != B && Layout.Rects[B].Parent != A && Overlap(Layout.Rects[A].Box, Layout.Rects[B].Box) ? 1 : 0;
			}
		}
		TestEqual(TEXT("two players: no rect overlaps another"), Bad, 0);
		TestNotNull(TEXT("two players: the second has its HP block"), Layout.Find(EDTHudElem::HpBar, 1));
		TestNotNull(TEXT("two players: and its XP bar"), Layout.Find(EDTHudElem::XpBar, 1));
		TestNotNull(TEXT("two players: and its ring"), Layout.Find(EDTHudElem::Ring, 1));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudKeepOut, "Dusktracer.Hud.Layout.KeepOut", DTHudTest::Flags) // T-HUD-02
bool FDTHudKeepOut::RunTest(const FString& Parameters)
{
	using namespace DTHudTest;
	if (!FontMeasures(*this))
	{
		return false;
	}
	// The circle is R-UX-01's: the farthest attack start plus a Tank's diameter.
	const int32 Farthest = FMath::Max3(DTConst::ROLE_TURRET_FIRE_RANGE_UU + DTConst::ROLE_TURRET_RADIUS_UU, DTConst::DIR_THREAT_RING_MAX_UU, DTConst::BOSS_HAZARD_MIN_RADIUS_UU);
	const float Radius = DTHud::KeepOutRadiusUU();
	TestEqual(TEXT("the keep-out radius is R-UX-01's reach"), Radius, static_cast<float>(Farthest + 2 * DTConst::ROLE_TANK_RADIUS_UU));

	// The projection is the camera's: the hero on the middle column above the centre by the aim shift (D-080), the floor's down-screen
	// reach on the bottom edge of the frame the target aspect solves for, and a pixel and its floor point name each other.
	for (const FView& View : Views)
	{
		const FVector2f Hero = DTHud::ProjectGround(View.Size, FVector2f::ZeroVector);
		TestTrue(*FString::Printf(TEXT("%s: the hero stands on the middle column, above the centre"), View.Name),
			FMath::IsNearlyEqual(Hero.X, 0.5f * View.Size.X, 0.01f) && Hero.Y < 0.5f * View.Size.Y);
		FVector2f Aim;
		TestTrue(*FString::Printf(TEXT("%s: the frame's centre is the aim point, the shift toward the camera"), View.Name),
			DTHud::UnprojectGround(View.Size, 0.5f * View.Size, Aim) && FMath::IsNearlyEqual(Aim.X, -ADTCameraPawn::AimShift(), 0.05f));
		const double Pitch = FMath::DegreesToRadians(static_cast<double>(DTConst::ARENA_CAMERA_PITCH_DEG));
		const double TanHalfVertical = FMath::Tan(FMath::DegreesToRadians(0.5 * DTConst::ARENA_CAMERA_FOV_DEG)) / ADTCameraPawn::TargetAspect;
		const double Distance = ADTCameraPawn::ComputeFramingDistance(static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU), static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG), ADTCameraPawn::TargetAspect);
		const double Shift = static_cast<double>(ADTCameraPawn::AimShift());
		const double Down = Distance * FMath::Cos(Pitch) + Shift - Distance * FMath::Sin(Pitch) / FMath::Tan(Pitch + FMath::Atan(TanHalfVertical));
		// R-UX-01 reads the plane bodies are drawn on (T-UX-01); telegraphs lie on the floor, where the reach is a little less.
		const double DownDrawn = Distance * FMath::Cos(Pitch) + Shift - (Distance * FMath::Sin(Pitch) - DTConst::SIM_PLANE_Z_UU) / FMath::Tan(Pitch + FMath::Atan(TanHalfVertical));
		const FVector2f Edge = DTHud::ProjectGround(View.Size, FVector2f(static_cast<float>(-Down), 0.0f));
		TestTrue(*FString::Printf(TEXT("%s: the down-screen reach lands on the bottom edge (y %.2f of %.0f)"), View.Name, Edge.Y, View.Size.Y), FMath::Abs(Edge.Y - View.Size.Y) < 0.5f);
		TestTrue(TEXT("the reach on the drawn plane covers the keep-out circle"), DownDrawn >= Radius);
		const FVector2f Point(0.31f * View.Size.X, 0.83f * View.Size.Y);
		FVector2f Ground;
		TestTrue(TEXT("a pixel below the horizon has a floor point"), DTHud::UnprojectGround(View.Size, Point, Ground));
		TestTrue(TEXT("and the floor point lands back on the pixel"), DTHud::ProjectGround(View.Size, Ground).Equals(Point, 0.05f));
		// What the check must catch: a rect on the hero, a sliver across the circle, and the bar's old place at the bottom centre.
		TestTrue(TEXT("a rect on the hero covers"), DTHud::CoversKeepOut(View.Size, FBox2f(0.5f * View.Size - FVector2f(4.0f), 0.5f * View.Size + FVector2f(4.0f))));
		TestTrue(TEXT("a rect around the whole circle covers"), DTHud::CoversKeepOut(View.Size, FBox2f(FVector2f(1.0f), View.Size - FVector2f(1.0f))));
		TestTrue(TEXT("a sliver across the frame covers"), DTHud::CoversKeepOut(View.Size, FBox2f(FVector2f(0.0f, 0.6f * View.Size.Y), FVector2f(View.Size.X, 0.6f * View.Size.Y + 2.0f))));
		const FVector2f OldBarMin(0.25f * View.Size.X, View.Size.Y - 62.0f);
		TestTrue(TEXT("the boss bar's old place at the bottom centre covered it"), DTHud::CoversKeepOut(View.Size, FBox2f(OldBarMin, OldBarMin + FVector2f(0.5f * View.Size.X, 14.0f))));
		TestFalse(TEXT("a corner does not"), DTHud::CoversKeepOut(View.Size, FBox2f(FVector2f(0.0f), FVector2f(16.0f))));
	}

	const FTables Tables;
	int32 Rects = 0;
	Walk(Tables, true, [&](const FString& Tag, const FView& View, int32 Culture, float Scale, const FDTHudLayout& Layout)
	{
		for (const FDTHudRect& Rect : Layout.Rects)
		{
			++Rects;
			if (DTHud::CoversKeepOut(View.Size, Rect.Box))
			{
				AddError(Tag + Describe(Rect) + TEXT(" covers the floor R-UX-01 keeps in view"));
			}
		}
	});
	TestTrue(TEXT("rects were tested"), Rects > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudMasks, "Dusktracer.Hud.Layout.Masks", DTHudTest::Flags) // T-HUD-03
bool FDTHudMasks::RunTest(const FString& Parameters)
{
	using namespace DTHudTest;
	if (!FontMeasures(*this))
	{
		return false;
	}
	const FTables Tables;
	const FView& Smoke = Views[0];
	TestEqual(TEXT("two masks, as Tools/contrast_check.py has them"), DTHud::ContrastMasks().Num(), 2);
	for (int32 Culture = 0; Culture < UE_ARRAY_COUNT(Cultures); ++Culture)
	{
		// The frame the contrast tool reads: the smoke window at the default text size. Readouts a build adds grow out of
		// the mask, as the dormant line always did, and the tool's scenes hold none.
		const FDTHudContent Shown = Content(Tables, Culture, false);
		const FDTHudLayout Layout = DTHud::Layout(Smoke.Size, 1.0f, Shown, &Measure);
		int32 Held = 0;
		for (const FDTHudRect& Rect : Layout.Rects)
		{
			TestFalse(TEXT("no readout of this content grows with a build"), DTHud::GrowsWithBuild(Rect.Kind));
			bool bMasked = false;
			for (const FVector4f& Mask : DTHud::ContrastMasks())
			{
				const FBox2f Area(FVector2f(Mask.X * Smoke.Size.X, Mask.Y * Smoke.Size.Y), FVector2f(Mask.Z * Smoke.Size.X, Mask.W * Smoke.Size.Y));
				bMasked |= Contains(Area, Rect.Box);
			}
			if (!bMasked)
			{
				AddError(FString::Printf(TEXT("%s: %s is outside the contrast tool's HUD masks"), Cultures[Culture], *Describe(Rect)));
			}
			Held += bMasked ? 1 : 0;
		}
		TestTrue(*FString::Printf(TEXT("%s: every readout is under a mask (%d of %d)"), Cultures[Culture], Held, Layout.Rects.Num()), Held == Layout.Rects.Num() && Held > 0);
	}
	TestTrue(TEXT("a meter grows with the build"), DTHud::GrowsWithBuild(EDTHudElem::MeterBar) && DTHud::GrowsWithBuild(EDTHudElem::DormantPanel) && DTHud::GrowsWithBuild(EDTHudElem::AimMode));
	TestFalse(TEXT("the ring does not"), DTHud::GrowsWithBuild(EDTHudElem::Ring) || DTHud::GrowsWithBuild(EDTHudElem::ToolPanel) || DTHud::GrowsWithBuild(EDTHudElem::HpBar));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudPlaces, "Dusktracer.Hud.Layout.Places", DTHudTest::Flags) // T-HUD-04
bool FDTHudPlaces::RunTest(const FString& Parameters)
{
	using namespace DTHudTest;
	if (!FontMeasures(*this))
	{
		return false;
	}
	const FTables Tables;
	Walk(Tables, true, [&](const FString& Tag, const FView& View, int32 Culture, float Scale, const FDTHudLayout& Layout)
	{
		const auto Box = [&](EDTHudElem Kind, int32 Index = 0) -> FBox2f
		{
			const FDTHudRect* Rect = Layout.Find(Kind, 0, Index);
			if (!Rect)
			{
				AddError(Tag + TEXT("no rect of kind ") + KindName(Kind));
				return FBox2f(ForceInit);
			}
			return Rect->Box;
		};
		const FVector2f Mid = 0.5f * View.Size;
		bool bOk = true;
		// Top left: HP, its bar, then the developer lines.
		bOk &= Box(EDTHudElem::HpText).Max.X < Mid.X && Box(EDTHudElem::HpText).Max.Y < Mid.Y;
		bOk &= Box(EDTHudElem::HpBar).Min.Y >= Box(EDTHudElem::HpText).Max.Y && Box(EDTHudElem::DevLine).Min.Y >= Box(EDTHudElem::HpBar).Max.Y;
		bOk &= Box(EDTHudElem::DevLine, 1).Min.Y >= Box(EDTHudElem::DevLine).Max.Y;
		// Top right: the clock on the right edge of its block, the level and the XP bar under it.
		bOk &= Box(EDTHudElem::Clock).Min.X > Mid.X && Box(EDTHudElem::Clock).Max.Y < Mid.Y;
		bOk &= FMath::IsNearlyEqual(Box(EDTHudElem::Clock).Max.X, Box(EDTHudElem::XpBar).Max.X, 0.01f);
		bOk &= Box(EDTHudElem::Level).Min.X > Mid.X && Box(EDTHudElem::XpBar).Min.Y >= Box(EDTHudElem::Level).Max.Y;
		bOk &= Box(EDTHudElem::XpText).Min.Y >= Box(EDTHudElem::XpBar).Max.Y && Box(EDTHudElem::Pending).Min.Y >= Box(EDTHudElem::XpText).Max.Y;
		// Top centre: the boss row, the banner under it, the toast under the banner (reactions.md R-REA-12), each centred.
		bOk &= Box(EDTHudElem::Banner).Min.Y >= Box(EDTHudElem::BossBar).Max.Y && Box(EDTHudElem::Banner).Min.Y >= Box(EDTHudElem::BossName).Max.Y;
		bOk &= Box(EDTHudElem::Toast).Min.Y >= Box(EDTHudElem::Banner).Max.Y;
		bOk &= FMath::Abs(Box(EDTHudElem::Banner).GetCenter().X - Mid.X) <= 1.0f && FMath::Abs(Box(EDTHudElem::Toast).GetCenter().X - Mid.X) <= 1.0f;
		// The row is centred at the default text size; a larger size grows the name leftward and leaves the bar where it was (D-080).
		bOk &= Scale != 1.0f || FMath::Abs(0.5f * (Box(EDTHudElem::BossName).Min.X + Box(EDTHudElem::BossBar).Max.X) - Mid.X) <= 1.0f;
		bOk &= Box(EDTHudElem::BossBar).Min.X > Box(EDTHudElem::BossName).Max.X && Box(EDTHudElem::BossBar).GetSize().X > 4.0f * Box(EDTHudElem::BossBar).GetSize().Y;
		// Bottom left, from the floor of the block up: the ring row, the tools, the meters, the aim line, the dormant sockets.
		bOk &= Box(EDTHudElem::Ring).Max.X < Mid.X && Box(EDTHudElem::Ring).Min.Y > Mid.Y;
		bOk &= FMath::IsNearlyEqual(Box(EDTHudElem::Ring).GetSize().X, Box(EDTHudElem::Ring).GetSize().Y, 0.01f);
		bOk &= Box(EDTHudElem::IntercessionLabel).Min.X >= Box(EDTHudElem::Ring).Max.X && Box(EDTHudElem::DashLabel).Min.Y >= Box(EDTHudElem::IntercessionLabel).Max.Y;
		bOk &= Box(EDTHudElem::DashPip).Min.X >= Box(EDTHudElem::DashLabel).Max.X && Box(EDTHudElem::DashPip, 1).Min.X >= Box(EDTHudElem::DashPip).Max.X;
		bOk &= Box(EDTHudElem::ToolPanel).Max.Y <= Box(EDTHudElem::Ring).Min.Y + Touch && Box(EDTHudElem::ToolPanel).Max.Y <= Box(EDTHudElem::IntercessionLabel).Min.Y + Touch;
		bOk &= Box(EDTHudElem::MeterBar, 3).Max.Y <= Box(EDTHudElem::ToolPanel).Min.Y && Box(EDTHudElem::MeterBar).Min.Y >= Box(EDTHudElem::MeterLabel).Max.Y;
		bOk &= Box(EDTHudElem::AimMode).Max.Y <= Box(EDTHudElem::MeterLabel).Min.Y && Box(EDTHudElem::DormantPanel, 2).Max.Y <= Box(EDTHudElem::AimMode).Min.Y;
		bOk &= Box(EDTHudElem::DormantPanel).Max.Y <= Box(EDTHudElem::DormantPanel, 1).Min.Y;
		TestTrue(*(Tag + TEXT("each readout is in its place")), bOk);
	});

	// A readout with nothing to say takes no place, and what is left does not move sideways.
	{
		FDTHudContent Quiet = Content(Tables, 0, false);
		Quiet.BossName = FText();
		Quiet.Banner = FText();
		Quiet.Toast = FText();
		Quiet.Players[0].Pending = FText();
		Quiet.Players[0].LevelUp = FText();
		const FDTHudLayout Layout = DTHud::Layout(Views[1].Size, 1.0f, Quiet, &Measure);
		for (const EDTHudElem Kind : { EDTHudElem::BossName, EDTHudElem::BossBar, EDTHudElem::Banner, EDTHudElem::Toast, EDTHudElem::Pending, EDTHudElem::LevelUp,
			EDTHudElem::MeterLabel, EDTHudElem::AimMode, EDTHudElem::DormantPanel })
		{
			TestNull(*FString::Printf(TEXT("nothing to say, no %s"), KindName(Kind)), Layout.Find(Kind));
		}
		const FDTHudLayout Full = DTHud::Layout(Views[1].Size, 1.0f, Content(Tables, 0, true), &Measure);
		for (const EDTHudElem Kind : { EDTHudElem::HpBar, EDTHudElem::Clock, EDTHudElem::XpBar, EDTHudElem::Ring, EDTHudElem::ToolPanel })
		{
			const FDTHudRect* A = Layout.Find(Kind);
			const FDTHudRect* B = Full.Find(Kind);
			TestTrue(*FString::Printf(TEXT("%s stays where it is when a build adds lines"), KindName(Kind)), A && B && A->Box.Min.Equals(B->Box.Min, 0.01f));
		}
	}

	// A long dormant line wraps inside the left column instead of running under the hero's floor.
	{
		FDTHudContent Long = Content(Tables, 0, true);
		Long.Players[0].Dormant = { FText::FromString(TEXT("Overcharged Capacitor Array dormant on Experimental Spiral Projector Mark Two")) };
		const FDTHudLayout Layout = DTHud::Layout(Views[1].Size, 1.5f, Long, &Measure);
		TestNotNull(TEXT("a long dormant line takes a second line"), Layout.Find(EDTHudElem::DormantLine, 0, 0, 1));
		const FDTHudRect* Panel = Layout.Find(EDTHudElem::DormantPanel);
		TestTrue(TEXT("and its panel stays clear of the keep-out"), Panel && !DTHud::CoversKeepOut(Views[1].Size, Panel->Box));
	}

	// The product line is not drawn in a run (hud.md R-HUD-04): the HUD's source reads no such row.
	FString Source;
	TestTrue(TEXT("the HUD's source reads"), FFileHelper::LoadFileToString(Source, *FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer/Game/DTHUD.cpp"))));
	TestFalse(TEXT("no product line on the run HUD"), Source.Contains(TEXT("Product.Name")));
	TestTrue(TEXT("the language check line is still drawn outside Shipping"), Source.Contains(TEXT("Test.LocRoundTrip")) && Source.Contains(TEXT("!UE_BUILD_SHIPPING")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudLowHpTest, "Dusktracer.Hud.LowHp", DTHudTest::Flags) // T-HUD-05
bool FDTHudLowHpTest::RunTest(const FString& Parameters)
{
	const float Max = 200.0f;
	const float Edge = Max * DTConst::HUD_LOW_HP_FRAC;
	const double Period = DTConst::HUD_LOW_HP_PULSE_S;
	TestFalse(TEXT("full HP is not low"), DTHud::LowHp(Max, Max, 0.0, true).bLow);
	TestFalse(TEXT("just above the line is not low"), DTHud::LowHp(Edge + 1.0f, Max, 0.0, true).bLow);
	TestTrue(TEXT("on the line is low"), DTHud::LowHp(Edge, Max, 0.0, true).bLow);
	TestFalse(TEXT("a fallen hero is not pulsing"), DTHud::LowHp(0.0f, Max, 0.0, true).bLow);
	TestFalse(TEXT("no maximum, no state"), DTHud::LowHp(1.0f, 0.0f, 0.0, true).bLow);
	TestEqual(TEXT("above the line the fill is solid"), DTHud::LowHp(Max, Max, 0.25 * Period, true).FillAlpha, 1.0f);
	TestEqual(TEXT("a pulse starts solid"), DTHud::LowHp(Edge, Max, 0.0, true).FillAlpha, 1.0f, 1e-4f);
	TestEqual(TEXT("and bottoms out half way"), DTHud::LowHp(Edge, Max, 0.5 * Period, true).FillAlpha, static_cast<float>(DTConst::HUD_LOW_HP_PULSE_MIN), 1e-4f);
	TestEqual(TEXT("one period later it is where it was"), DTHud::LowHp(Edge, Max, 3.25 * Period, true).FillAlpha, DTHud::LowHp(Edge, Max, 0.25 * Period, true).FillAlpha, 1e-3f);
	for (double Time = 0.0; Time < 2.0 * Period; Time += Period / 16.0)
	{
		const float Alpha = DTHud::LowHp(1.0f, Max, Time, true).FillAlpha;
		TestTrue(TEXT("the pulse stays between its floor and solid"), Alpha >= DTConst::HUD_LOW_HP_PULSE_MIN - 1e-4f && Alpha <= 1.0f + 1e-4f);
	}
	// Screen flashes off (settings.md R-SET-09): the state stays, the motion goes.
	const FDTHudLowHp Still = DTHud::LowHp(Edge, Max, 0.5 * Period, false);
	TestTrue(TEXT("flashes off: still low"), Still.bLow);
	TestEqual(TEXT("flashes off: no pulse"), Still.FillAlpha, 1.0f);
	TestTrue(TEXT("the line is a share of the bar"), DTConst::HUD_LOW_HP_FRAC > 0.0f && DTConst::HUD_LOW_HP_FRAC < 1.0f && DTConst::HUD_LOW_HP_PULSE_MIN > 0.0f && DTConst::HUD_LOW_HP_PULSE_MIN < 1.0f);
	// Bone, never red: the HP bar's duty is the text token.
	TestTrue(TEXT("the HP fill is Pal.UI.Text"), DTHudStyle::DutyToken(EDTHudDuty::Hp) == DTPal::UI_Text);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudRing, "Dusktracer.Hud.Ring", DTHudTest::Flags) // T-HUD-06
bool FDTHudRing::RunTest(const FString& Parameters)
{
	using namespace DTHudTest;
	// The ring takes the Patron's token, and the neutral trail tone with no Patron.
	struct FPathToken
	{
		EDTPath Key;
		FColor Value;
	};
	const FPathToken Paths[] = { { EDTPath::Storm, DTPal::Keeper_Storm }, { EDTPath::Forge, DTPal::Keeper_Forge }, { EDTPath::Tide, DTPal::Keeper_Tide },
		{ EDTPath::Shade, DTPal::Keeper_Shade }, { EDTPath::Radiance, DTPal::Keeper_Radiance }, { EDTPath::Quicksilver, DTPal::Keeper_Quicksilver } };
	for (const FPathToken& Path : Paths)
	{
		TestTrue(TEXT("a Patron's ring is its Keeper token"), DTKeepers::TrailToken(Path.Key) == Path.Value);
		TestFalse(TEXT("and no Keeper token is in the enemy band"), InEnemyBand(Path.Value));
	}
	TestTrue(TEXT("no Patron: the neutral trail tone"), DTKeepers::TrailToken(EDTPath::None) == DTHudStyle::DutyToken(EDTHudDuty::RingIdle));
	TestTrue(TEXT("the ready ring is the player's core tone"), DTHudStyle::DutyToken(EDTHudDuty::RingReady) == DTPal::Player_Core);
	// The dash: bone when ready, the same bone dimmed while it comes back.
	TestTrue(TEXT("a ready pip is bone"), DTHudStyle::DutyToken(EDTHudDuty::DashReady) == DTPal::UI_Text && DTHudStyle::DutyAlpha(EDTHudDuty::DashReady) == 1.0f);
	TestTrue(TEXT("a refilling pip is the same bone, dimmer"), DTHudStyle::DutyToken(EDTHudDuty::DashRefill) == DTPal::UI_Text && DTHudStyle::DutyAlpha(EDTHudDuty::DashRefill) < 1.0f);
	if (!FontMeasures(*this))
	{
		return false;
	}
	// One pip per charge the hero can hold, on the dash label's row, and the ring is as tall as its two lines of text or taller.
	const FTables Tables;
	for (int32 Pips = 0; Pips <= DTConst::DASH_CHARGES + 2; ++Pips)
	{
		FDTHudContent Shown = Content(Tables, 0, false);
		Shown.Players[0].DashPips = Pips;
		const FDTHudLayout Layout = DTHud::Layout(Views[1].Size, 1.0f, Shown, &Measure);
		int32 Found = 0;
		for (const FDTHudRect& Rect : Layout.Rects)
		{
			Found += Rect.Kind == EDTHudElem::DashPip ? 1 : 0;
		}
		TestEqual(TEXT("one pip per charge"), Found, Pips);
		const FDTHudRect* Label = Layout.Find(EDTHudElem::DashLabel);
		const FDTHudRect* Last = Pips > 0 ? Layout.Find(EDTHudElem::DashPip, 0, Pips - 1) : nullptr;
		TestTrue(TEXT("the pips sit on the label's row"), Pips == 0 || (Label && Last && Last->Box.Min.Y >= Label->Box.Min.Y - Touch && Last->Box.Max.Y <= Label->Box.Max.Y + Touch));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudTools, "Dusktracer.Hud.Tools", DTHudTest::Flags) // T-HUD-07
bool FDTHudTools::RunTest(const FString& Parameters)
{
	using namespace DTHudTest;
	const FTables Tables;
	// The counters read the player's offer state and nothing else: a fresh run holds the catalogue's counts.
	FDTPlayerOfferState Offer;
	TArray<FText> Lines;
	DTHudText::Tools(Offer, Lines);
	const TCHAR* const Keys[] = { TEXT("Offer.Tool.Reroll"), TEXT("Offer.Tool.Banish"), TEXT("Offer.Tool.Lock") };
	const int32 Fresh[] = { DTConst::OFFER_REROLLS, DTConst::OFFER_BANISH, DTConst::OFFER_LOCK };
	if (TestEqual(TEXT("three counters"), Lines.Num(), 3))
	{
		for (int32 Tool = 0; Tool < 3; ++Tool)
		{
			const FString Line = Lines[Tool].ToString();
			const FString Name = FText::FromStringTable(TEXT("ST_UI"), Keys[Tool]).ToString();
			TestTrue(*FString::Printf(TEXT("%s names its tool: %s"), Keys[Tool], *Line), !Name.IsEmpty() && Line.Contains(Name));
			TestTrue(*FString::Printf(TEXT("%s shows the uses left: %s"), Keys[Tool], *Line), Line.EndsWith(FString::FromInt(Fresh[Tool])));
			TestFalse(TEXT("the keys resolve"), Line.Contains(TEXT("MISSING")) || Line.Contains(TEXT("{")));
		}
	}
	Offer.RerollsLeft = 0;
	Offer.BanishLeft = 7;
	Offer.LockLeft = -1;
	DTHudText::Tools(Offer, Lines);
	TestTrue(TEXT("a spent tool reads zero"), Lines.Num() == 3 && Lines[0].ToString().EndsWith(TEXT("0")));
	TestTrue(TEXT("a granted use shows"), Lines.Num() == 3 && Lines[1].ToString().EndsWith(TEXT("7")));
	TestTrue(TEXT("never below zero"), Lines.Num() == 3 && Lines[2].ToString().EndsWith(TEXT("0")));
	// The one new row: a form, not a word, with both arguments in both cultures.
	for (int32 Culture = 0; Culture < UE_ARRAY_COUNT(Cultures); ++Culture)
	{
		const FString Row = Tables.Ui.Get(TEXT("Hud.Tool"), Culture);
		TestTrue(*FString::Printf(TEXT("Hud.Tool %s carries {Tool} and {Count}"), Cultures[Culture]), Row.Contains(TEXT("{Tool}")) && Row.Contains(TEXT("{Count}")));
	}
	TestEqual(TEXT("the clock reads minutes and seconds"), DTHudText::Clock(static_cast<int64>(8 * 60 + 42) * DTConst::SIM_FIXED_HZ).ToString(), FString(TEXT("08:42")));
	TestEqual(TEXT("the clock starts at zero"), DTHudText::Clock(0).ToString(), FString(TEXT("00:00")));
	// The staged goldens (gate step smoke) are scenes of this package.
	for (const TCHAR* Scene : { TEXT("hud_staged.json"), TEXT("hud_boss.json") })
	{
		TestTrue(*FString::Printf(TEXT("scene %s exists"), Scene), FPaths::FileExists(FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/Scenes"), Scene)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudType, "Dusktracer.Hud.Type", DTHudTest::Flags) // T-HUD-08
bool FDTHudType::RunTest(const FString& Parameters)
{
	using namespace DTHudTest;
	TestTrue(TEXT("the steps fall from the clock to the developer line"), DTUI::TypeStep(EDTType::Display) > DTUI::TypeStep(EDTType::Title)
		&& DTUI::TypeStep(EDTType::Title) > DTUI::TypeStep(EDTType::Body) && DTUI::TypeStep(EDTType::Body) > DTUI::TypeStep(EDTType::Note));
	TestEqual(TEXT("the canvas scale is one at the reference height"), DTUI::CanvasScale(DTUI::RefHeight), 1.0f);
	TestEqual(TEXT("and follows the window's height"), DTUI::CanvasScale(720.0f), 720.0f / DTUI::RefHeight);
	if (!FontMeasures(*this))
	{
		return false;
	}
	// The smallest window the tests walk, at the default text size: a line of player text is still C.HUD.TEXT_MIN_PX tall.
	const float Smallest = DTUI::TypeStep(EDTType::Body) * DTUI::CanvasScale(Views[0].Size.Y);
	const float Latin = Measure(TEXT("HP 100 / 100"), Smallest).Y;
	const float Han = Measure(TEXT("生命 100 / 100"), Smallest).Y;
	TestTrue(*FString::Printf(TEXT("player text at 720 lines: %.0f px Latin, %.0f px Han, need %d"), Latin, Han, DTConst::HUD_TEXT_MIN_PX),
		Latin >= DTConst::HUD_TEXT_MIN_PX && Han >= DTConst::HUD_TEXT_MIN_PX);
	// Text size multiplies every step (settings.md R-SET-12).
	const FDTUIPainter Painter{ nullptr, nullptr, 0.5f, 1.5f };
	TestEqual(TEXT("a drawn scale is step x canvas x text size"), Painter.TextScale(EDTType::Title), DTUI::TypeStep(EDTType::Title) * 0.5f * 1.5f);
	// The HUD's files draw text through the table: none carries a scale of its own.
	const TCHAR* const Files[] = { TEXT("Game/DTHUD.cpp"), TEXT("Game/DTHudLayout.cpp"), TEXT("Progression/DTXPBar.cpp") };
	for (const TCHAR* File : Files)
	{
		FString Source;
		TestTrue(*FString::Printf(TEXT("%s reads"), File), FFileHelper::LoadFileToString(Source, *FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer"), File)));
		TestFalse(*FString::Printf(TEXT("%s has no type scale of its own"), File), Source.Contains(TEXT("TitleScale")) || Source.Contains(TEXT("NameScale")) || Source.Contains(TEXT("RunOverScale")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudPanels, "Dusktracer.Hud.Panels", DTHudTest::Flags) // T-HUD-09
bool FDTHudPanels::RunTest(const FString& Parameters)
{
	const FDTPanelLook Default = DTUI::PanelLook(EDTPanel::Default);
	const FDTPanelLook Focus = DTUI::PanelLook(EDTPanel::Focus);
	const FDTPanelLook Locked = DTUI::PanelLook(EDTPanel::Locked);
	const FDTPanelLook Solid = DTUI::PanelLook(EDTPanel::Solid);
	TestTrue(TEXT("default: translucent, a faint border"), Default.FillAlpha > 0.0f && Default.FillAlpha < 1.0f && Default.BorderAlpha > 0.0f && Default.BorderAlpha < Focus.BorderAlpha && !Default.bDashed);
	TestTrue(TEXT("focus: darker, a full border"), Focus.FillAlpha > Default.FillAlpha && Focus.BorderAlpha == 1.0f && !Focus.bDashed);
	TestTrue(TEXT("locked: dashed, and told apart from focus by more than its border"), Locked.bDashed && Locked.BorderAlpha > Default.BorderAlpha && Locked.FillAlpha == Default.FillAlpha);
	TestTrue(TEXT("solid: opaque, no border (modifiers.md R-MOD-08)"), Solid.FillAlpha == 1.0f && Solid.BorderAlpha == 0.0f);
	// A panel is Pal.UI.Panel under Pal.UI.Text and nothing else: text on a solid panel holds the readability contrast.
	const float Ratio = (DTHudTest::Luminance(DTPal::UI_Text) + 0.05f) / (DTHudTest::Luminance(DTPal::UI_Panel) + 0.05f);
	TestTrue(*FString::Printf(TEXT("text on a solid panel: %.2f"), Ratio), Ratio >= static_cast<float>(DTConst::PRE_CONTRAST_MIN));
	// The three painters repeated in the HUD's files are gone: one painter draws them.
	const TCHAR* const Files[] = { TEXT("Game/DTHUD.cpp"), TEXT("Progression/DTXPBar.cpp") };
	for (const TCHAR* File : Files)
	{
		FString Source;
		TestTrue(*FString::Printf(TEXT("%s reads"), File), FFileHelper::LoadFileToString(Source, *FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer"), File)));
		TestFalse(*FString::Printf(TEXT("%s builds no tile of its own"), File), Source.Contains(TEXT("FCanvasTileItem")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHudTokens, "Dusktracer.Hud.Tokens", DTHudTest::Flags) // T-HUD-10
bool FDTHudTokens::RunTest(const FString& Parameters)
{
	using namespace DTHudTest;
	// R-PRE-08: Pal.Enemy.* and Pal.Telegraph never appear in UI. Every duty's token is checked, by value and by hue.
	const FColor Barred[] = { DTPal::Enemy_Bullet, DTPal::Enemy_BulletCore, DTPal::Enemy_Muzzle, DTPal::Enemy_Body, DTPal::Enemy_Optic, DTPal::Elite_Light,
		DTPal::Telegraph, DTPal::Telegraph_Edge };
	for (int32 Duty = 0; Duty < static_cast<int32>(EDTHudDuty::Num); ++Duty)
	{
		const FColor Token = DTHudStyle::DutyToken(static_cast<EDTHudDuty>(Duty));
		for (const FColor& Enemy : Barred)
		{
			TestFalse(*FString::Printf(TEXT("duty %d is not an enemy or telegraph token"), Duty), Token == Enemy);
		}
		TestFalse(*FString::Printf(TEXT("duty %d is outside the enemy hue band (#%s)"), Duty, *Token.ToHex()), InEnemyBand(Token));
		TestTrue(*FString::Printf(TEXT("duty %d draws at an opacity"), Duty), DTHudStyle::DutyAlpha(static_cast<EDTHudDuty>(Duty)) > 0.0f);
	}
	// The boss bar has a token of its own, and it reads on the track.
	TestTrue(TEXT("the boss bar fills in Pal.UI.Boss"), DTHudStyle::DutyToken(EDTHudDuty::Boss) == DTPal::UI_Boss);
	const float Ratio = (Luminance(DTPal::UI_Boss) + 0.05f) / (Luminance(DTPal::UI_Panel) + 0.05f);
	TestTrue(*FString::Printf(TEXT("Pal.UI.Boss on its track: %.2f"), Ratio), Ratio >= static_cast<float>(DTConst::PRE_CONTRAST_MIN));
	TestTrue(TEXT("the XP bar keeps its token (progression.md R-PRG-11)"), DTHudStyle::DutyToken(EDTHudDuty::Xp) == DTPal::Pickup_Gem);
	TestTrue(TEXT("tracks are the panel tone"), DTHudStyle::DutyToken(EDTHudDuty::Track) == DTPal::UI_Panel);

	// No file that draws the HUD names an enemy or telegraph token at all.
	const TCHAR* const Files[] = { TEXT("Game/DTHUD.cpp"), TEXT("Game/DTHUD.h"), TEXT("Game/DTHudLayout.cpp"), TEXT("Game/DTHudLayout.h"), TEXT("Game/DTUIStyle.cpp"),
		TEXT("Game/DTUIStyle.h"), TEXT("Game/DTKeeperMeters.cpp"), TEXT("Progression/DTXPBar.cpp"), TEXT("Boss/DTBossSubsystem.cpp") };
	for (const TCHAR* File : Files)
	{
		FString Source;
		TestTrue(*FString::Printf(TEXT("%s reads"), File), FFileHelper::LoadFileToString(Source, *FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer"), File)));
		for (const TCHAR* Name : { TEXT("DTPal::Enemy_"), TEXT("DTPal::Elite_"), TEXT("DTPal::Telegraph") })
		{
			TestFalse(*FString::Printf(TEXT("%s names no %s token"), File, Name), Source.Contains(Name));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
