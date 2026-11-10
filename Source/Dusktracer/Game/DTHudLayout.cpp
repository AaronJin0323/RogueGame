#include "Game/DTHudLayout.h"
#include "Core/DTConstantsGen.h"
#include "Game/DTCameraPawn.h"
#include "Game/DTUIStyle.h"
#include "Offers/DTOfferStyle.h"

namespace DTHudLayoutLocal
{
	// Reference pixels at a canvas DTUI::RefHeight lines tall (hud.md Numbers); presentation, not tuning.
	constexpr float MarginX = 40.0f;
	constexpr float MarginTop = 24.0f;
	constexpr float MarginBottom = 28.0f;
	constexpr float Gap = 6.0f;          // between the rows of one block
	constexpr float LineGap = 3.0f;      // between lines of text that belong together
	constexpr float BlockGap = 12.0f;    // between blocks
	constexpr float BarW = 360.0f;       // the HP bar and the XP bar: the width of a corner block
	constexpr float HpBarH = 8.0f;
	constexpr float XpBarH = 10.0f;
	constexpr float ColumnGap = 24.0f;   // between a corner block and the centre column
	constexpr float CentreTop = 12.0f;
	constexpr float BossBarW = 560.0f;
	constexpr float BossBarH = 12.0f;
	constexpr float BossNameGap = 12.0f;
	constexpr float Ring = 84.0f;
	constexpr float RingGap = 16.0f;
	constexpr float PipW = 34.0f;
	constexpr float PipH = 6.0f;
	constexpr float PipGap = 6.0f;
	constexpr float PanelPad = 8.0f;
	constexpr float DormantPad = 4.0f;
	constexpr float MeterBarW = 240.0f;
	constexpr float MeterBarH = 8.0f;
	constexpr float LeftColumnW = 360.0f; // the widest a line of the bottom-left block may run before it wraps
	constexpr float KeepOutStepPx = 1.0f; // spacing of the points tested along a rect's edge

	// Tools/contrast_check.py HUD_MASKS; Tools/tools_test.py HudMasks holds the two lists equal.
	const FVector4f Masks[] = { FVector4f(0.0f, 0.0f, 1.0f, 0.13f), FVector4f(0.0f, 0.77f, 0.23f, 1.0f) };

	/** The solo camera's lens and place (06_UX R-UX-01): a wider window keeps the height and shows more at the sides. */
	struct FLens
	{
		float Distance;
		float SinPitch;
		float CosPitch;
		float TanHalfVertical;
		float TanHalfHorizontal;

		explicit FLens(const FVector2f& View)
		{
			const float Fov = static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG);
			Distance = ADTCameraPawn::ComputeFramingDistance(static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU), Fov, ADTCameraPawn::TargetAspect);
			FMath::SinCos(&SinPitch, &CosPitch, FMath::DegreesToRadians(static_cast<float>(DTConst::ARENA_CAMERA_PITCH_DEG)));
			TanHalfVertical = FMath::Tan(FMath::DegreesToRadians(0.5f * Fov)) / ADTCameraPawn::TargetAspect;
			TanHalfHorizontal = TanHalfVertical * View.X / FMath::Max(View.Y, 1.0f);
		}
	};
}

const FDTHudRect* FDTHudLayout::Find(EDTHudElem Kind, int32 Player, int32 Index, int32 Line) const
{
	return Rects.FindByPredicate([=](const FDTHudRect& Rect) { return Rect.Kind == Kind && Rect.Player == Player && Rect.Index == Index && Rect.Line == Line; });
}

void DTHud::BossRow(const FVector2f& View, const FVector2f& NameSize, FBox2f& OutName, FBox2f& OutBar)
{
	using namespace DTHudLayoutLocal;
	const float S = DTUI::CanvasScale(View.Y);
	const float ColumnW = View.X - 2.0f * (MarginX + BarW + ColumnGap) * S;
	const float BarWidth = FMath::Max(0.0f, FMath::Min(BossBarW * S, ColumnW - NameSize.X - BossNameGap * S));
	const float RowW = NameSize.X + BossNameGap * S + BarWidth;
	const float RowH = FMath::Max(NameSize.Y, BossBarH * S);
	const float X = 0.5f * (View.X - RowW);
	const float Y = CentreTop * S;
	OutName = FBox2f(FVector2f(X, Y + 0.5f * (RowH - NameSize.Y)), FVector2f(X + NameSize.X, Y + 0.5f * (RowH + NameSize.Y)));
	const float BarX = X + NameSize.X + BossNameGap * S;
	OutBar = FBox2f(FVector2f(BarX, Y + 0.5f * (RowH - BossBarH * S)), FVector2f(BarX + BarWidth, Y + 0.5f * (RowH + BossBarH * S)));
}

FDTHudLayout DTHud::Layout(const FVector2f& View, float TextScale, const FDTHudContent& Content, FDTHudMeasure Measure)
{
	using namespace DTHudLayoutLocal;
	FDTHudLayout Out;
	const float S = DTUI::CanvasScale(View.Y);
	const auto Size = [&](const FText& Line, EDTType Type, float Mul = 1.0f)
	{
		return Measure(Line.ToString(), DTUI::TypeStep(Type) * S * TextScale * Mul);
	};
	/** The size of a line and the factor that holds it inside MaxWidth. */
	const auto Fitted = [&](const FText& Line, EDTType Type, float MaxWidth, float& OutMul)
	{
		const FVector2f Full = Size(Line, Type);
		OutMul = Full.X > MaxWidth && Full.X > 0.0f ? MaxWidth / Full.X : 1.0f;
		return OutMul < 1.0f ? Size(Line, Type, OutMul) : Full;
	};
	const auto Add = [&Out](EDTHudElem Kind, int32 Player, int32 Index, const FVector2f& Min, const FVector2f& Extent, int32 Parent = INDEX_NONE, float Mul = 1.0f)
	{
		FDTHudRect& Rect = Out.Rects.AddDefaulted_GetRef();
		Rect.Kind = Kind;
		Rect.Player = Player;
		Rect.Index = Index;
		Rect.Box = FBox2f(Min, Min + Extent);
		Rect.Parent = Parent;
		Rect.TextMul = Mul;
		return Out.Rects.Num() - 1;
	};
	const float Left = MarginX * S;
	const float Right = View.X - MarginX * S;
	const float BlockW = BarW * S;

	// ---- top left: HP, then the developer lines ---------------------------------------------------------------------------
	{
		float Y = MarginTop * S;
		for (int32 Player = 0; Player < Content.Players.Num(); ++Player)
		{
			const FDTHudPlayerContent& P = Content.Players[Player];
			if (P.Hp.IsEmpty())
			{
				continue;
			}
			float Mul = 1.0f;
			const FVector2f Text = Fitted(P.Hp, EDTType::Body, BlockW, Mul);
			Add(EDTHudElem::HpText, Player, 0, FVector2f(Left, Y), Text, INDEX_NONE, Mul);
			Y += Text.Y + Gap * S;
			Add(EDTHudElem::HpBar, Player, 0, FVector2f(Left, Y), FVector2f(BlockW, HpBarH * S));
			Y += HpBarH * S + BlockGap * S;
		}
		for (int32 Index = 0; Index < Content.DevLines.Num(); ++Index)
		{
			float Mul = 1.0f;
			const FVector2f Text = Fitted(Content.DevLines[Index], EDTType::Note, BlockW, Mul);
			Add(EDTHudElem::DevLine, 0, Index, FVector2f(Left, Y), Text, INDEX_NONE, Mul);
			Y += Text.Y + LineGap * S;
		}
	}

	// ---- top right: the clock, then level and XP ---------------------------------------------------------------------------
	{
		const float X0 = Right - BlockW;
		float Y = MarginTop * S;
		float ClockLeft = Right;
		float ClockBottom = Y;
		if (!Content.Clock.IsEmpty())
		{
			const FVector2f Clock = Size(Content.Clock, EDTType::Display);
			Add(EDTHudElem::Clock, 0, 0, FVector2f(Right - Clock.X, Y), Clock);
			ClockLeft = Right - Clock.X;
			ClockBottom = Y + Clock.Y;
		}
		for (int32 Player = 0; Player < Content.Players.Num(); ++Player)
		{
			const FDTHudPlayerContent& P = Content.Players[Player];
			if (P.Level.IsEmpty())
			{
				continue;
			}
			float Mul = 1.0f;
			const FVector2f Level = Fitted(P.Level, EDTType::Title, BlockW, Mul);
			if (Player == 0 && X0 + Level.X + Gap * S <= ClockLeft && Level.Y <= ClockBottom - Y)
			{
				// Beside the clock, on its baseline.
				Add(EDTHudElem::Level, Player, 0, FVector2f(X0, ClockBottom - Level.Y), Level, INDEX_NONE, Mul);
				Y = ClockBottom + Gap * S;
			}
			else
			{
				Y = Player == 0 ? ClockBottom + Gap * S : Y;
				Add(EDTHudElem::Level, Player, 0, FVector2f(X0, Y), Level, INDEX_NONE, Mul);
				Y += Level.Y + Gap * S;
			}
			Add(EDTHudElem::XpBar, Player, 0, FVector2f(X0, Y), FVector2f(BlockW, XpBarH * S));
			Y += XpBarH * S + Gap * S;
			if (!P.Xp.IsEmpty())
			{
				const FVector2f Xp = Fitted(P.Xp, EDTType::Body, BlockW, Mul);
				Add(EDTHudElem::XpText, Player, 0, FVector2f(X0, Y), Xp, INDEX_NONE, Mul);
				float RowH = Xp.Y;
				if (!P.LevelUp.IsEmpty())
				{
					float UpMul = 1.0f;
					const FVector2f Up = Fitted(P.LevelUp, EDTType::Body, BlockW, UpMul);
					if (Xp.X + BlockGap * S + Up.X <= BlockW)
					{
						Add(EDTHudElem::LevelUp, Player, 0, FVector2f(Right - Up.X, Y), Up, INDEX_NONE, UpMul);
						RowH = FMath::Max(RowH, Up.Y);
					}
					else
					{
						Y += RowH + LineGap * S;
						Add(EDTHudElem::LevelUp, Player, 0, FVector2f(X0, Y), Up, INDEX_NONE, UpMul);
						RowH = Up.Y;
					}
				}
				Y += RowH + LineGap * S;
			}
			if (!P.Pending.IsEmpty())
			{
				const FVector2f Pending = Fitted(P.Pending, EDTType::Body, BlockW, Mul);
				Add(EDTHudElem::Pending, Player, 0, FVector2f(X0, Y), Pending, INDEX_NONE, Mul);
				Y += Pending.Y + LineGap * S;
			}
			Y += BlockGap * S;
		}
	}

	// ---- top centre: the boss row, the director's banner, the reaction toast under it (reactions.md R-REA-12) ------------
	{
		const float ColumnW = View.X - 2.0f * (MarginX + BarW + ColumnGap) * S;
		float Y = CentreTop * S;
		if (!Content.BossName.IsEmpty())
		{
			// Never shrunk: UDTBossSubsystem::DrawBossBar draws the name at its step and gives the bar what the column has left.
			const FVector2f Name = Size(Content.BossName, EDTType::Body);
			FBox2f NameBox(ForceInit);
			FBox2f BarBox(ForceInit);
			BossRow(View, Name, NameBox, BarBox);
			Add(EDTHudElem::BossName, 0, 0, NameBox.Min, NameBox.GetSize());
			Add(EDTHudElem::BossBar, 0, 0, BarBox.Min, BarBox.GetSize());
			Y = FMath::Max(NameBox.Max.Y, BarBox.Max.Y) + Gap * S;
		}
		if (!Content.Banner.IsEmpty())
		{
			float Mul = 1.0f;
			const FVector2f Banner = Fitted(Content.Banner, EDTType::Title, ColumnW, Mul);
			Add(EDTHudElem::Banner, 0, 0, FVector2f(0.5f * (View.X - Banner.X), Y), Banner, INDEX_NONE, Mul);
			Y += Banner.Y + Gap * S;
		}
		if (!Content.Toast.IsEmpty())
		{
			float Mul = 1.0f;
			const FVector2f Toast = Fitted(Content.Toast, EDTType::Body, ColumnW, Mul);
			Add(EDTHudElem::Toast, 0, 0, FVector2f(0.5f * (View.X - Toast.X), Y), Toast, INDEX_NONE, Mul);
		}
	}

	// ---- bottom left, one block per player, built downward from zero and then set on the bottom margin --------------------
	float Bottom = View.Y - MarginBottom * S;
	for (int32 Player = 0; Player < Content.Players.Num(); ++Player)
	{
		const FDTHudPlayerContent& P = Content.Players[Player];
		const int32 First = Out.Rects.Num();
		const float ColumnW = LeftColumnW * S;
		float Y = 0.0f;

		// Dormant sockets on top, each on its own solid panel (modifiers.md R-MOD-08), wrapped to the column.
		for (int32 Index = 0; Index < P.Dormant.Num(); ++Index)
		{
			const float Scale = DTUI::TypeStep(EDTType::Body) * S * TextScale;
			const float Pad = DormantPad * S;
			const TArray<FString> Lines = DTOfferText::WrapLines(P.Dormant[Index].ToString(),
				[&](const FString& Line) { return Measure(Line, Scale).X; }, ColumnW - 2.0f * Pad);
			float Width = 0.0f;
			float Height = 0.0f;
			TArray<FVector2f> Sizes;
			for (const FString& Line : Lines)
			{
				const FVector2f LineSize = Measure(Line, Scale);
				Sizes.Add(LineSize);
				Width = FMath::Max(Width, LineSize.X);
				Height += LineSize.Y;
			}
			const int32 Panel = Add(EDTHudElem::DormantPanel, Player, Index, FVector2f(Left - Pad, Y), FVector2f(Width + 2.0f * Pad, Height + 2.0f * Pad));
			float LineY = Y + Pad;
			for (int32 Line = 0; Line < Lines.Num(); ++Line)
			{
				const int32 Rect = Add(EDTHudElem::DormantLine, Player, Index, FVector2f(Left, LineY), Sizes[Line], Panel);
				Out.Rects[Rect].Line = Line;
				Out.Rects[Rect].Wrapped = Lines[Line];
				LineY += Sizes[Line].Y;
			}
			Y += Height + 2.0f * Pad + Gap * S;
		}
		if (!P.AimMode.IsEmpty())
		{
			float Mul = 1.0f;
			const FVector2f Aim = Fitted(P.AimMode, EDTType::Body, ColumnW, Mul);
			Add(EDTHudElem::AimMode, Player, 0, FVector2f(Left, Y), Aim, INDEX_NONE, Mul);
			Y += Aim.Y + Gap * S;
		}
		// Keeper meters (keepers.md R-KEE-38): a label, and a bar under it where the meter has a cap.
		for (int32 Index = 0; Index < P.Meters.Num(); ++Index)
		{
			float Mul = 1.0f;
			const FVector2f Label = Fitted(P.Meters[Index].Label, EDTType::Body, ColumnW, Mul);
			Add(EDTHudElem::MeterLabel, Player, Index, FVector2f(Left, Y), Label, INDEX_NONE, Mul);
			Y += Label.Y + LineGap * S;
			if (P.Meters[Index].bBar)
			{
				Add(EDTHudElem::MeterBar, Player, Index, FVector2f(Left, Y), FVector2f(MeterBarW * S, MeterBarH * S));
				Y += MeterBarH * S;
			}
			Y += Gap * S;
		}
		if (P.Dormant.Num() > 0 || P.Meters.Num() > 0 || !P.AimMode.IsEmpty())
		{
			Y += BlockGap * S - Gap * S;
		}
		// The three tool counters on one panel.
		if (P.Tools.Num() > 0)
		{
			const float Pad = PanelPad * S;
			float Width = 0.0f;
			float Height = 0.0f;
			TArray<FVector2f> Sizes;
			TArray<float> Muls;
			for (const FText& Tool : P.Tools)
			{
				float Mul = 1.0f;
				const FVector2f Line = Fitted(Tool, EDTType::Body, ColumnW - 2.0f * Pad, Mul);
				Sizes.Add(Line);
				Muls.Add(Mul);
				Width = FMath::Max(Width, Line.X);
				Height += Line.Y;
			}
			Height += LineGap * S * (P.Tools.Num() - 1);
			const int32 Panel = Add(EDTHudElem::ToolPanel, Player, 0, FVector2f(Left, Y), FVector2f(Width + 2.0f * Pad, Height + 2.0f * Pad));
			float LineY = Y + Pad;
			for (int32 Index = 0; Index < P.Tools.Num(); ++Index)
			{
				Add(EDTHudElem::ToolLine, Player, Index, FVector2f(Left + Pad, LineY), Sizes[Index], Panel, Muls[Index]);
				LineY += Sizes[Index].Y + LineGap * S;
			}
			Y += Height + 2.0f * Pad + BlockGap * S;
		}
		// The Intercession ring, with its label and the dash beside it.
		{
			const float RingPx = Ring * S;
			const float TextX = Left + RingPx + RingGap * S;
			const float TextW = FMath::Max(ColumnW - RingPx - RingGap * S, 1.0f);
			float LabelMul = 1.0f;
			float DashMul = 1.0f;
			const FVector2f Label = P.Intercession.IsEmpty() ? FVector2f::ZeroVector : Fitted(P.Intercession, EDTType::Body, TextW, LabelMul);
			const FVector2f Dash = P.Dash.IsEmpty() ? FVector2f::ZeroVector : Fitted(P.Dash, EDTType::Body, TextW, DashMul);
			const float DashRowH = FMath::Max(Dash.Y, PipH * S);
			const float TextH = Label.Y + Gap * S + DashRowH;
			const float RowH = FMath::Max(RingPx, TextH);
			Add(EDTHudElem::Ring, Player, 0, FVector2f(Left, Y + 0.5f * (RowH - RingPx)), FVector2f(RingPx, RingPx));
			float TextY = Y + 0.5f * (RowH - TextH);
			if (!P.Intercession.IsEmpty())
			{
				Add(EDTHudElem::IntercessionLabel, Player, 0, FVector2f(TextX, TextY), Label, INDEX_NONE, LabelMul);
			}
			TextY += Label.Y + Gap * S;
			if (!P.Dash.IsEmpty())
			{
				Add(EDTHudElem::DashLabel, Player, 0, FVector2f(TextX, TextY + 0.5f * (DashRowH - Dash.Y)), Dash, INDEX_NONE, DashMul);
			}
			float PipX = TextX + Dash.X + 2.0f * PipGap * S;
			for (int32 Pip = 0; Pip < P.DashPips; ++Pip)
			{
				Add(EDTHudElem::DashPip, Player, Pip, FVector2f(PipX, TextY + 0.5f * (DashRowH - PipH * S)), FVector2f(PipW * S, PipH * S));
				PipX += (PipW + PipGap) * S;
			}
			Y += RowH;
		}
		const FVector2f Shift(0.0f, Bottom - Y);
		for (int32 Index = First; Index < Out.Rects.Num(); ++Index)
		{
			Out.Rects[Index].Box = Out.Rects[Index].Box.ShiftBy(Shift);
		}
		Bottom -= Y + BlockGap * S;
	}
	return Out;
}

float DTHud::KeepOutRadiusUU()
{
	const int32 Farthest = FMath::Max3(DTConst::ROLE_TURRET_FIRE_RANGE_UU + DTConst::ROLE_TURRET_RADIUS_UU,
		DTConst::DIR_THREAT_RING_MAX_UU, DTConst::BOSS_HAZARD_MIN_RADIUS_UU);
	return static_cast<float>(Farthest + 2 * DTConst::ROLE_TANK_RADIUS_UU);
}

FVector2f DTHud::ProjectGround(const FVector2f& View, const FVector2f& Ground)
{
	const DTHudLayoutLocal::FLens Lens(View);
	// The hero stands on the view axis; the floor point is Ground.X further along the camera's heading and Ground.Y to its right.
	const float Depth = FMath::Max(Lens.Distance + Ground.X * Lens.CosPitch, UE_KINDA_SMALL_NUMBER);
	const float Up = Ground.X * Lens.SinPitch;
	return FVector2f(0.5f * View.X * (1.0f + Ground.Y / (Depth * Lens.TanHalfHorizontal)), 0.5f * View.Y * (1.0f - Up / (Depth * Lens.TanHalfVertical)));
}

bool DTHud::UnprojectGround(const FVector2f& View, const FVector2f& Pixel, FVector2f& OutGround)
{
	const DTHudLayoutLocal::FLens Lens(View);
	const float NdcX = 2.0f * Pixel.X / FMath::Max(View.X, 1.0f) - 1.0f;
	const float NdcY = 1.0f - 2.0f * Pixel.Y / FMath::Max(View.Y, 1.0f);
	// Up = NdcY * TanHalfVertical * Depth, with Up = X * sin and Depth = Distance + X * cos.
	const float Slope = NdcY * Lens.TanHalfVertical;
	const float Denominator = Lens.SinPitch - Slope * Lens.CosPitch;
	if (Denominator <= UE_KINDA_SMALL_NUMBER)
	{
		return false; // at or above the horizon
	}
	OutGround.X = Slope * Lens.Distance / Denominator;
	OutGround.Y = NdcX * Lens.TanHalfHorizontal * (Lens.Distance + OutGround.X * Lens.CosPitch);
	return true;
}

bool DTHud::CoversKeepOut(const FVector2f& View, const FBox2f& Box)
{
	const float Radius = KeepOutRadiusUU();
	const auto Inside = [&](const FVector2f& Pixel)
	{
		FVector2f Ground;
		return UnprojectGround(View, Pixel, Ground) && Ground.SizeSquared() < Radius * Radius;
	};
	// The circle's image is convex. A rect that meets it either holds the hero or has a point of its edge inside.
	const FVector2f Hero = ProjectGround(View, FVector2f::ZeroVector);
	if (Box.IsInside(Hero))
	{
		return true;
	}
	const FVector2f Extent = Box.GetSize();
	const float Step = DTHudLayoutLocal::KeepOutStepPx;
	for (float X = 0.0f; X <= Extent.X; X += Step)
	{
		if (Inside(FVector2f(Box.Min.X + X, Box.Min.Y)) || Inside(FVector2f(Box.Min.X + X, Box.Max.Y)))
		{
			return true;
		}
	}
	for (float Y = 0.0f; Y <= Extent.Y; Y += Step)
	{
		if (Inside(FVector2f(Box.Min.X, Box.Min.Y + Y)) || Inside(FVector2f(Box.Max.X, Box.Min.Y + Y)))
		{
			return true;
		}
	}
	return Inside(Box.Max);
}

TConstArrayView<FVector4f> DTHud::ContrastMasks()
{
	return DTHudLayoutLocal::Masks;
}

bool DTHud::GrowsWithBuild(EDTHudElem Kind)
{
	return Kind == EDTHudElem::MeterLabel || Kind == EDTHudElem::MeterBar || Kind == EDTHudElem::AimMode
		|| Kind == EDTHudElem::DormantPanel || Kind == EDTHudElem::DormantLine;
}

FDTHudLowHp DTHud::LowHp(float Hp, float MaxHp, double SimSeconds, bool bAllowFlash)
{
	FDTHudLowHp Out;
	Out.bLow = MaxHp > 0.0f && Hp > 0.0f && Hp <= MaxHp * DTConst::HUD_LOW_HP_FRAC;
	if (Out.bLow && bAllowFlash)
	{
		// A cosine between one and the floor, starting full, on sim time: it holds still while the sim does.
		const double Phase = FMath::Fmod(SimSeconds, static_cast<double>(DTConst::HUD_LOW_HP_PULSE_S)) / DTConst::HUD_LOW_HP_PULSE_S;
		const float Wave = 0.5f * (1.0f + FMath::Cos(2.0f * UE_PI * static_cast<float>(Phase)));
		Out.FillAlpha = FMath::Lerp(static_cast<float>(DTConst::HUD_LOW_HP_PULSE_MIN), 1.0f, Wave);
	}
	return Out;
}
