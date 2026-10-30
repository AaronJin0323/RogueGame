#include "Progression/DTXPBar.h"
#include "CanvasItem.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Internationalization/Text.h"
#include "Progression/DTGemSubsystem.h"

namespace DTXPBarLocal
{
	// HUD layout (pixels at the drawn canvas size) and the flash window; presentation, not tuning.
	constexpr float TopPx = 24.0f;
	constexpr float WidthRatio = 0.4f;
	constexpr float BarHeightPx = 12.0f;
	constexpr float LabelGapPx = 10.0f;
	constexpr float RowGapPx = 8.0f;
	constexpr double FlashSeconds = 1.0;
}

void FDTXPBar::BuildRows(const FDTRunState& Run, double SimTime, TConstArrayView<double> LastLevelUpTime, TArray<FDTXPBarRow>& Out)
{
	Out.Reset();
	for (int32 Index = 0; Index < Run.GetNumPlayers(); ++Index)
	{
		const FDTPlayerRunState& P = Run.GetPlayer(Index);
		FDTXPBarRow& Row = Out.AddDefaulted_GetRef();
		Row.OwnerId = Index;
		Row.Level = P.Level;
		Row.XP = P.XP;
		Row.XPToNext = P.XPToNext;
		Row.Progress01 = P.XPToNext > 0 ? FMath::Clamp(static_cast<float>(P.XP) / static_cast<float>(P.XPToNext), 0.0f, 1.0f) : 0.0f;
		Row.PendingHands = P.Offer.PendingHands.Num();
		const double LastUp = LastLevelUpTime.IsValidIndex(Index) ? LastLevelUpTime[Index] : -1.0;
		Row.bFlash = LastUp >= 0.0 && SimTime - LastUp < DTXPBarLocal::FlashSeconds;
	}
}

void FDTXPBar::Draw(UCanvas* Canvas, UWorld* World, UFont* Font)
{
	UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
	if (!Canvas || !Sim || !Font)
	{
		return;
	}
	using namespace DTXPBarLocal;
	const UDTGemSubsystem* Gems = World->GetSubsystem<UDTGemSubsystem>();
	TArray<double> LastUp;
	for (int32 Index = 0; Index < Sim->GetRun().GetNumPlayers(); ++Index)
	{
		LastUp.Add(Gems ? Gems->GetLastLevelUpTime(Index) : -1.0);
	}
	TArray<FDTXPBarRow> Rows;
	BuildRows(Sim->GetRun(), Gems ? Gems->GetSimTime() : 0.0, LastUp, Rows);

	const FLinearColor TextColor = DTPal::Linear(DTPal::UI_Text);
	const FLinearColor Back = DTPal::Linear(DTPal::UI_Panel);
	const FLinearColor Fill = DTPal::Linear(DTPal::Pickup_Gem);
	const FLinearColor FlashFill = DTPal::Linear(DTPal::Player_Core);
	const FText LabelFormat = FText::FromStringTable(TEXT("ST_UI"), TEXT("Level.Label"));
	const FText XpFormat = FText::FromStringTable(TEXT("ST_UI"), TEXT("Level.Tooltip"));
	const FText PendingFormat = FText::FromStringTable(TEXT("ST_UI"), TEXT("Level.Pending"));
	const FText LevelUp = FText::FromStringTable(TEXT("ST_UI"), TEXT("Level.Up"));

	auto Measure = [Canvas, Font](const FText& Line, float& OutW, float& OutH)
	{
		Canvas->TextSize(Font, Line.ToString(), OutW, OutH, 1.0f, 1.0f);
	};
	auto Text = [Canvas, Font, &TextColor](const FText& Line, float X, float Y)
	{
		FCanvasTextItem Item(FVector2D(X, Y), Line, Font, TextColor);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	};
	auto Box = [Canvas](float X, float Y, float W, float H, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	};

	const float BarW = Canvas->ClipX * WidthRatio;
	const float BarX = (Canvas->ClipX - BarW) * 0.5f;
	float Y = TopPx;
	for (const FDTXPBarRow& Row : Rows)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("Level"), FText::AsNumber(Row.Level));
		Args.Add(TEXT("XP"), FText::AsNumber(Row.XP));
		Args.Add(TEXT("XPToNext"), FText::AsNumber(Row.XPToNext));
		Args.Add(TEXT("Count"), FText::AsNumber(Row.PendingHands));
		const FText Label = FText::Format(LabelFormat, Args);
		float LabelW = 0.0f;
		float LineH = 0.0f;
		Measure(Label, LabelW, LineH);
		const float RowH = FMath::Max(LineH, BarHeightPx);
		const float BarY = Y + (RowH - BarHeightPx) * 0.5f;

		Text(Label, BarX - LabelGapPx - LabelW, Y + (RowH - LineH) * 0.5f);
		Box(BarX, BarY, BarW, BarHeightPx, Back);
		Box(BarX, BarY, BarW * Row.Progress01, BarHeightPx, Row.bFlash ? FlashFill : Fill);

		float RightX = BarX + BarW + LabelGapPx;
		const FText Xp = FText::Format(XpFormat, Args);
		float XpW = 0.0f;
		float XpH = 0.0f;
		Measure(Xp, XpW, XpH);
		Text(Xp, RightX, Y + (RowH - XpH) * 0.5f);
		RightX += XpW + LabelGapPx;
		if (Row.PendingHands > 0)
		{
			const FText Pending = FText::Format(PendingFormat, Args);
			float PendingW = 0.0f;
			Measure(Pending, PendingW, XpH);
			Text(Pending, RightX, Y + (RowH - XpH) * 0.5f);
			RightX += PendingW + LabelGapPx;
		}
		if (Row.bFlash)
		{
			Text(LevelUp, RightX, Y + (RowH - XpH) * 0.5f);
		}
		Y += RowH + RowGapPx;
	}
}
