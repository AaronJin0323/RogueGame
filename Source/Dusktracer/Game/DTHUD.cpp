#include "Game/DTHUD.h"
#include "Core/DTCatalogGen.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Keepers/DTReactions.h"
#include "CanvasItem.h"
#include "ChartCreation.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Internationalization/Text.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Engine/World.h"
#include "Boss/DTBossSubsystem.h"
#include "Combat/DTWeaponCompose.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Enemies/DTElite.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Game/DTGameMode.h"
#include "Game/DTHudLayout.h"
#include "Game/DTUIStyle.h"
#include "Hub/DTHubSubsystem.h"
#include "Keepers/DTKeeperSpec.h"
#include "Keepers/DTKeeperSubsystem.h"
#include "Offers/DTOfferStyle.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHeroSubsystem.h"
#include "Player/DTInputSet.h"
#include "Progression/DTXPBar.h"
#include "Settings/DTMenuSubsystem.h"
#include "Settings/DTUserSettings.h"

namespace DTHUDLocal
{
	// Marks drawn inside the layout's rects, in reference pixels (hud.md Numbers); presentation, not tuning.
	constexpr float RingStroke = 6.0f;
	constexpr float ReadyStroke = 2.0f;
	constexpr float ReadyInset = 10.0f;
	constexpr float LowHpFrame = 2.0f;
	constexpr float LowHpFrameGap = 3.0f;
	// Exponential smoothing of the readout so it is legible; the measured numbers come from Tools/Perf.ps1.
	constexpr double Smoothing = 0.1;
	constexpr float EliteLabelGapPx = 26.0f; // under the hull at gameplay camera (elites.md R-ELT-12)

	const TCHAR* const ToolKeys[] = { TEXT("Offer.Tool.Reroll"), TEXT("Offer.Tool.Banish"), TEXT("Offer.Tool.Lock") };

	/** What the marks of one player's readouts show, beside the text the layout holds. */
	struct FPlayerMarks
	{
		float Hp = 0.0f;
		float MaxHp = 0.0f;
		int32 DashCharges = 0;
		float DashRefill01 = 0.0f;
		float Intercession01 = 0.0f;
		FColor RingToken = DTPal::Player_TrailNeutral;
		TArray<FDTHudMeter> Meters;
	};
}

void DTHudText::Tools(const FDTPlayerOfferState& Offer, TArray<FText>& Out)
{
	Out.Reset();
	const int32 Left[] = { Offer.RerollsLeft, Offer.BanishLeft, Offer.LockLeft };
	const FText Format = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Tool"));
	for (int32 Tool = 0; Tool < UE_ARRAY_COUNT(DTHUDLocal::ToolKeys); ++Tool)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Tool"), FText::FromStringTable(TEXT("ST_UI"), DTHUDLocal::ToolKeys[Tool]));
		Args.Add(TEXT("Count"), FText::AsNumber(FMath::Max(0, Left[Tool])));
		Out.Add(FText::Format(Format, Args));
	}
}

FText DTHudText::Clock(int64 StepIndex)
{
	const int32 Seconds = static_cast<int32>(StepIndex / DTConst::SIM_FIXED_HZ);
	return FText::AsCultureInvariant(FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60));
}

/** Smoothed frame and game-thread time from the engine's performance tracking (same source as stat unit). */
class FDTHUDFrameTimes : public IPerformanceDataConsumer
{
public:
	virtual void StartCharting() override {}
	virtual void StopCharting() override {}
	virtual void ProcessFrame(const FFrameData& FrameData) override
	{
		const double Alpha = bHasSample ? DTHUDLocal::Smoothing : 1.0;
		FrameMs += (FrameData.TrueDeltaSeconds * 1000.0 - FrameMs) * Alpha;
		GameThreadMs += (FrameData.GameThreadTimeSeconds * 1000.0 - GameThreadMs) * Alpha;
		bHasSample = true;
	}

	double FrameMs = 0.0;
	double GameThreadMs = 0.0;
	bool bHasSample = false;
};

void ADTHUD::BeginPlay()
{
	Super::BeginPlay();
	if (GEngine)
	{
		FrameTimes = MakeShared<FDTHUDFrameTimes>();
		GEngine->AddPerformanceDataConsumer(FrameTimes);
	}
}

void ADTHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GEngine && FrameTimes.IsValid())
	{
		GEngine->RemovePerformanceDataConsumer(FrameTimes);
	}
	FrameTimes.Reset();
	Super::EndPlay(EndPlayReason);
}

FText ADTHUD::KeyLabel(const FText& Label, const UDTInputSet* Set, const UInputAction* Action, EDTInputDevice Device)
{
	const FKey Key = Set ? Set->GetBoundKey(Action, Device) : FKey();
	if (!Key.IsValid())
	{
		return Label;
	}
	FFormatNamedArguments Args;
	Args.Add(TEXT("Name"), Label);
	Args.Add(TEXT("Keys"), UDTInputSet::KeyGlyph(Key));
	return FText::Format(FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.KeyLabel")), Args);
}

void ADTHUD::DrawRun()
{
	UWorld* const World = GetWorld();
	if (!World || !Canvas || !GEngine)
	{
		return;
	}
	using namespace DTHUDLocal;
	UFont* const Font = GEngine->GetLargeFont();
	const FDTUIPainter Painter{ Canvas, Font, DTUI::CanvasScale(Canvas->ClipY), DTSettings::TextScale() };
	const UDTHeroSubsystem* const Hero = World->GetSubsystem<UDTHeroSubsystem>();
	UDTSimSubsystem* const Sim = World->GetSubsystem<UDTSimSubsystem>();
	const UDTWeaponSubsystem* const Weapons = World->GetSubsystem<UDTWeaponSubsystem>();
	const UDTKeeperSubsystem* const Keepers = World->GetSubsystem<UDTKeeperSubsystem>();
	const UDTBossSubsystem* const Boss = World->GetSubsystem<UDTBossSubsystem>();
	const ADTGameMode* const Mode = World->GetAuthGameMode<ADTGameMode>();

	// ---- what the readouts say now: presentation reads sim state and never writes it (04_Presentation R-PRE-11) ----------
	FDTHudContent Content;
	TArray<FPlayerMarks> Marks;
	TArray<FDTXPBarRow> XpRows;
	TArray<FDTXPBarText> XpText;
	FDTXPBar::ReadRows(World, XpRows);
	const int32 NumHeroes = Hero ? Hero->GetNumHeroes() : 0;
	const int32 NumPlayers = FMath::Max(NumHeroes, XpRows.Num());
	Content.Players.SetNum(NumPlayers);
	Marks.SetNum(NumPlayers);
	XpText.SetNum(XpRows.Num());
	for (int32 OwnerId = 0; OwnerId < NumHeroes; ++OwnerId)
	{
		const FDTHeroState& State = *Hero->GetState(OwnerId);
		FDTHudPlayerContent& P = Content.Players[OwnerId];
		FPlayerMarks& M = Marks[OwnerId];
		FFormatNamedArguments Args;
		Args.Add(TEXT("Hp"), FText::AsNumber(FMath::CeilToInt(FMath::Max(0.0f, State.Hp))));
		Args.Add(TEXT("MaxHp"), FText::AsNumber(FMath::CeilToInt(State.MaxHp)));
		P.Hp = FText::Format(FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Hp")), Args);
		M.Hp = State.Hp;
		M.MaxHp = State.MaxHp;
		// The key or pad button of the device this player used last, beside the Dash and Intercession labels (R-CMB-13).
		const UDTInputSet* const Keys = Hero->GetInputSet(OwnerId);
		const EDTInputDevice Device = Hero->GetLastInputDevice(OwnerId);
		P.Dash = KeyLabel(FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Dash")), Keys, Keys ? Keys->Dash.Get() : nullptr, Device);
		P.DashPips = FDTHeroSim::ComputeMaxCharges(State.Stats);
		M.DashCharges = State.DashCharges;
		const float Cooldown = DTConst::DASH_COOLDOWN_S * State.Stats.DashCooldownMul;
		M.DashRefill01 = Cooldown > 0.0f ? FMath::Clamp(1.0f - State.DashRefillLeft / Cooldown, 0.0f, 1.0f) : 0.0f;
		M.Intercession01 = FMath::Clamp(State.Intercession01, 0.0f, 1.0f);
		P.Intercession = KeyLabel(FText::FromStringTable(TEXT("ST_UI"), M.Intercession01 >= 1.0f ? TEXT("Hud.IntercessionReady") : TEXT("Hud.Intercession")),
			Keys, Keys ? Keys->Intercession.Get() : nullptr, Device);
#if !UE_BUILD_SHIPPING
		if (OwnerId == 0 && !bLoggedKeyLabels)
		{
			bLoggedKeyLabels = true;
			UE_LOG(LogDT, Log, TEXT("HudKeys culture=%s dash=%s | intercession=%s"), *FInternationalization::Get().GetCurrentCulture()->GetName(),
				*P.Dash.ToString(), *P.Intercession.ToString());
		}
#endif // !UE_BUILD_SHIPPING
		if (Hero->GetAimMode(OwnerId) == EDTAimMode::Manual)
		{
			P.AimMode = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.AimMode"));
		}
		// Keeper meters of the player (keepers.md R-KEE-38).
		if (Weapons && Keepers)
		{
			const FDTKeeperSim& KeeperSim = Keepers->GetSim();
			DTKeeperMeters::Build(Weapons->GetSim().GetKeeperMeters(OwnerId), KeeperSim.GetDilationLeft(OwnerId), KeeperSim.GetDilationTotal(OwnerId), MeterScratch);
			M.Meters = MeterScratch;
			for (const FDTHudMeter& Meter : M.Meters)
			{
				FFormatNamedArguments MeterArgs;
				MeterArgs.Add(TEXT("Value"), FText::AsNumber(FMath::CeilToInt(Meter.Value)));
				MeterArgs.Add(TEXT("Max"), FText::AsNumber(FMath::CeilToInt(Meter.Max)));
				P.Meters.Add({ FText::Format(FText::FromStringTable(TEXT("ST_UI"), Meter.Key.ToString()), MeterArgs), Meter.Fill01 >= 0.0f });
			}
		}
		if (Sim && OwnerId < Sim->GetRun().GetNumPlayers())
		{
			const FDTPlayerRunState& Run = Sim->GetRun().GetPlayer(OwnerId);
			DTHudText::Tools(Run.Offer, P.Tools);
			// The ring takes the Patron's colour, and the neutral trail tone before one is chosen (R-HUD-06).
			M.RingToken = DTKeepers::TrailToken(Run.Offer.PatronPath);
			// Only a dormant modifier draws here, so a build without one shows the HUD it always did (modifiers.md R-MOD-08).
			for (const FDTDormantSocket& Socket : DTWeaponCompose::DormantSockets(Run))
			{
				P.Dormant.Add(DTOfferText::Dormant(TEXT("Dormant.Hud"), Socket));
			}
		}
	}
	for (int32 Row = 0; Row < XpRows.Num(); ++Row)
	{
		XpText[Row] = FDTXPBar::Text(XpRows[Row]);
		if (Content.Players.IsValidIndex(XpRows[Row].OwnerId))
		{
			FDTHudPlayerContent& P = Content.Players[XpRows[Row].OwnerId];
			P.Level = XpText[Row].Level;
			P.Xp = XpText[Row].Xp;
			P.Pending = XpText[Row].Pending;
			P.LevelUp = XpText[Row].LevelUp;
		}
	}
	// Developer lines, not player text: the Shipping build validators play draws neither (package.md R-PKG-02).
#if !UE_BUILD_SHIPPING
	const FText RoundTrip = FText::FromStringTable(TEXT("ST_UI"), TEXT("Test.LocRoundTrip"));
	Content.DevLines.Add(RoundTrip);
	if (!bLoggedRoundTrip)
	{
		bLoggedRoundTrip = true;
		UE_LOG(LogDT, Log, TEXT("LocRoundTrip culture=%s text=%s"), *FInternationalization::Get().GetCurrentCulture()->GetName(), *RoundTrip.ToString());
	}
	if (FrameTimes.IsValid() && FrameTimes->bHasSample)
	{
		const double Fps = FrameTimes->FrameMs > 0.0 ? 1000.0 / FrameTimes->FrameMs : 0.0;
		Content.DevLines.Add(FText::AsCultureInvariant(FString::Printf(TEXT("%.0f fps  %.2f ms frame  %.2f ms game"),
			Fps, FrameTimes->FrameMs, FrameTimes->GameThreadMs)));
	}
#endif // !UE_BUILD_SHIPPING
	// Run timer: sim time since run start (validators note moments against it; S1_kit.md).
	if (Sim)
	{
		Content.Clock = DTHudText::Clock(Sim->GetStepIndex());
	}
	// The boss row's name is read by the boss subsystem alone (boss.md Text), which places it with DTHud::BossRow. Here the
	// layout needs only the row's height, to set the banner under it: a line of the same step in the same culture stands in.
	const bool bBossBar = Boss && Boss->GetCore().HasHull();
	if (bBossBar)
	{
		Content.BossName = FText::FromStringTable(TEXT("ST_UI"), TEXT("Hud.Dash"));
	}
	// Director banners (director.md R-DIR-17).
	if (const UDTDirector* Director = World->GetSubsystem<UDTDirector>())
	{
		const EDTDirBanner Banner = Director->GetCore().GetBanner();
		if (Banner != EDTDirBanner::None)
		{
			Content.Banner = FText::FromStringTable(TEXT("ST_UI"), Banner == EDTDirBanner::Threat ? TEXT("Hud.Threat") : TEXT("Hud.BossIncoming"));
		}
	}
	// A reaction's first fire, under the banner (reactions.md R-REA-12).
	if (Weapons && Sim)
	{
		const int32 Row = Weapons->GetSim().GetToast(0, Sim->GetStepIndex());
		if (Row != INDEX_NONE)
		{
			const FString Pair(DTCat::Reactions[Row].Pair);
			FFormatNamedArguments Args;
			Args.Add(TEXT("Name"), FText::FromStringTable(TEXT("ST_Reactions"), Pair + TEXT(".Name")));
			Args.Add(TEXT("Toast"), FText::FromStringTable(TEXT("ST_Reactions"), Pair + TEXT(".Toast")));
			Content.Toast = FText::Format(FText::FromStringTable(TEXT("ST_Reactions"), TEXT("Toast.Format")), Args);
		}
	}

	// ---- where they sit (hud.md R-HUD-01) ------------------------------------------------------------------------------------
	const FVector2f View(Canvas->ClipX, Canvas->ClipY);
	const FDTHudLayout Layout = DTHud::Layout(View, Painter.T, Content, [this, Font](const FString& Line, float Scale)
	{
		float Width = 0.0f;
		float Height = 0.0f;
		Canvas->TextSize(Font, Line, Width, Height, Scale, Scale);
		return FVector2f(Width, Height);
	});

	// ---- draw ----------------------------------------------------------------------------------------------------------------
	const FLinearColor Ink = DTHudStyle::Duty(EDTHudDuty::Text);
	const auto Line = [&](EDTHudElem Kind, int32 Player, int32 Index, const FText& Words, EDTType Type)
	{
		if (const FDTHudRect* Rect = Words.IsEmpty() ? nullptr : Layout.Find(Kind, Player, Index))
		{
			Painter.Text(Words, Rect->Box.Min, Type, Ink, Rect->TextMul);
		}
	};
	const double SimSeconds = Sim ? static_cast<double>(Sim->GetStepIndex()) / DTConst::SIM_FIXED_HZ : 0.0;
	for (int32 Player = 0; Player < NumPlayers; ++Player)
	{
		const FDTHudPlayerContent& P = Content.Players[Player];
		const FPlayerMarks& M = Marks[Player];
		Line(EDTHudElem::HpText, Player, 0, P.Hp, EDTType::Body);
		if (const FDTHudRect* Bar = Layout.Find(EDTHudElem::HpBar, Player))
		{
			// R-HUD-05: bone, never red. Low HP pulses the fill and frames the bar, so the state does not rest on motion.
			const FDTHudLowHp Low = DTHud::LowHp(M.Hp, M.MaxHp, SimSeconds, DTSettings::AllowFlash());
			FLinearColor Fill = DTHudStyle::Duty(EDTHudDuty::Hp);
			Fill.A *= Low.FillAlpha;
			Painter.Bar(Bar->Box, M.MaxHp > 0.0f ? M.Hp / M.MaxHp : 0.0f, Fill);
			if (Low.bLow)
			{
				Painter.Frame(Bar->Box.ExpandBy((LowHpFrameGap + LowHpFrame) * Painter.S), Ink, LowHpFrame);
			}
		}

		if (const FDTHudRect* Ring = Layout.Find(EDTHudElem::Ring, Player))
		{
			// R-HUD-06: clockwise from the top; full, it gains an inner ring and its label says so.
			Painter.Arc(Ring->Box, 0.0f, 1.0f, DTHudStyle::Duty(EDTHudDuty::Track), RingStroke);
			Painter.Arc(Ring->Box, 0.0f, M.Intercession01, DTUI::Token(M.RingToken), RingStroke);
			if (M.Intercession01 >= 1.0f)
			{
				Painter.Arc(Ring->Box, 0.0f, 1.0f, DTHudStyle::Duty(EDTHudDuty::RingReady), ReadyStroke, ReadyInset);
			}
		}
		Line(EDTHudElem::IntercessionLabel, Player, 0, P.Intercession, EDTType::Body);
		Line(EDTHudElem::DashLabel, Player, 0, P.Dash, EDTType::Body);
		for (int32 Pip = 0; Pip < P.DashPips; ++Pip)
		{
			if (const FDTHudRect* Rect = Layout.Find(EDTHudElem::DashPip, Player, Pip))
			{
				// A ready pip is full; the one coming back fills from the left, dimmer.
				const bool bReady = Pip < M.DashCharges;
				Painter.Bar(Rect->Box, bReady ? 1.0f : (Pip == M.DashCharges ? M.DashRefill01 : 0.0f),
					DTHudStyle::Duty(bReady ? EDTHudDuty::DashReady : EDTHudDuty::DashRefill));
			}
		}

		if (const FDTHudRect* Panel = Layout.Find(EDTHudElem::ToolPanel, Player))
		{
			Painter.Panel(Panel->Box, EDTPanel::Default);
		}
		for (int32 Tool = 0; Tool < P.Tools.Num(); ++Tool)
		{
			Line(EDTHudElem::ToolLine, Player, Tool, P.Tools[Tool], EDTType::Body);
		}

		for (int32 Meter = 0; Meter < P.Meters.Num(); ++Meter)
		{
			Line(EDTHudElem::MeterLabel, Player, Meter, P.Meters[Meter].Label, EDTType::Body);
			if (const FDTHudRect* Bar = Layout.Find(EDTHudElem::MeterBar, Player, Meter))
			{
				Painter.Bar(Bar->Box, M.Meters[Meter].Fill01, DTUI::Token(M.Meters[Meter].Color));
			}
		}
		Line(EDTHudElem::AimMode, Player, 0, P.AimMode, EDTType::Body);
		for (int32 Socket = 0; Socket < P.Dormant.Num(); ++Socket)
		{
			if (const FDTHudRect* Panel = Layout.Find(EDTHudElem::DormantPanel, Player, Socket))
			{
				Painter.Panel(Panel->Box, EDTPanel::Solid);
			}
			for (int32 Wrapped = 0; const FDTHudRect* Rect = Layout.Find(EDTHudElem::DormantLine, Player, Socket, Wrapped); ++Wrapped)
			{
				Painter.Text(FText::AsCultureInvariant(Rect->Wrapped), Rect->Box.Min, EDTType::Body, Ink);
			}
		}
	}
	for (int32 Index = 0; Index < Content.DevLines.Num(); ++Index)
	{
		Line(EDTHudElem::DevLine, 0, Index, Content.DevLines[Index], EDTType::Note);
	}
	// The XP rows; only the offer screen draws over them (progression.md R-PRG-11).
	for (int32 Row = 0; Row < XpRows.Num(); ++Row)
	{
		FDTXPBar::Draw(Painter, XpRows[Row], XpText[Row], Layout);
	}
	Line(EDTHudElem::Clock, 0, 0, Content.Clock, EDTType::Display);
	if (bBossBar)
	{
		Boss->DrawBossBar(Canvas, Painter.S * Painter.T);
	}
	Line(EDTHudElem::Banner, 0, 0, Content.Banner, EDTType::Title);
	Line(EDTHudElem::Toast, 0, 0, Content.Toast, EDTType::Body);

	// The run's last line, in the middle of the screen: the run is over and nothing is left to hide (R-HUD-02). Never
	// Hud.RunOver over a win: a hero that fell on the step the Warden fell is in a won run (boss.md R-BOS-13).
	const bool bVictory = Mode && Mode->IsVictory();
	if (bVictory || (Hero && Hero->IsRunOver()))
	{
		const FText Last = FText::FromStringTable(TEXT("ST_UI"), bVictory ? TEXT("Hud.Victory") : TEXT("Hud.RunOver"));
		const FVector2f Size = Painter.Measure(Last.ToString(), EDTType::Title);
		Painter.Text(Last, 0.5f * (View - Size), EDTType::Title, Ink);
	}
}

void ADTHUD::DrawEliteLabels()
{
	const UDTEnemySubsystem* Enemies = GetWorld() ? GetWorld()->GetSubsystem<UDTEnemySubsystem>() : nullptr;
	const UDTHeroSubsystem* Hero = GetWorld() ? GetWorld()->GetSubsystem<UDTHeroSubsystem>() : nullptr;
	if (!Enemies || !Canvas || !GEngine)
	{
		return;
	}
	const FDTEnemyPool& Pool = Enemies->GetPool();
	const FDTHeroState* First = Hero && Hero->GetNumHeroes() > 0 ? Hero->GetState(0) : nullptr;
	DTElite::LabelledElites(Pool, First ? First->Pos : FVector2f::ZeroVector, LabelScratch);
	UFont* const Font = GEngine->GetSmallFont();
	const float Z = static_cast<float>(DTConst::SIM_PLANE_Z_UU);
	for (const int32 Index : LabelScratch)
	{
		const FVector2f At = Pool.GetPositions()[Index];
		const FVector Screen = Project(FVector(At.X, At.Y, Z));
		if (Screen.Z <= 0.0f || Screen.X < 0.0f || Screen.Y < 0.0f || Screen.X > Canvas->ClipX || Screen.Y > Canvas->ClipY)
		{
			continue;
		}
		const FText Label = DTElite::LabelText(Pool.GetAux(Index).AffixMask);
		float Width = 0.0f;
		float Height = 0.0f;
		const float TextScale = DTSettings::TextScale();
		Canvas->TextSize(Font, Label.ToString(), Width, Height, TextScale, TextScale);
		FCanvasTextItem Item(FVector2D(Screen.X - 0.5f * Width, Screen.Y + DTHUDLocal::EliteLabelGapPx), Label, Font, DTHudStyle::Duty(EDTHudDuty::Text));
		Item.Scale = FVector2D(TextScale, TextScale);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	}
}

void ADTHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine)
	{
		return;
	}
	if (const ADTGameMode* HubMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADTGameMode>() : nullptr; HubMode && HubMode->IsHub())
	{
		// The hub is the whole screen; nothing of a run exists to draw (vigil.md R-VIG-01). Settings opens over it.
		if (UDTHubSubsystem* Hub = GetWorld()->GetSubsystem<UDTHubSubsystem>())
		{
			Hub->Draw(Canvas);
		}
		if (UDTMenuSubsystem* HubMenu = GetWorld()->GetSubsystem<UDTMenuSubsystem>())
		{
			HubMenu->Draw(Canvas);
		}
		return;
	}

	DrawEliteLabels();
	DrawRun();

	// The offer screen draws over everything while a hand is open (offers.md R-OFF-13).
	if (UDTOfferSubsystem* Offers = GetWorld() ? GetWorld()->GetSubsystem<UDTOfferSubsystem>() : nullptr)
	{
		Offers->DrawScreen(Canvas);
	}
	// The pause screen opens over a hand too (settings.md R-SET-16).
	if (UDTMenuSubsystem* Menu = GetWorld() ? GetWorld()->GetSubsystem<UDTMenuSubsystem>() : nullptr)
	{
		Menu->Draw(Canvas);
	}
}
