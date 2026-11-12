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
	// The top-right block the first visual wave gives the level and XP (D-080 item 11), on the run clock's margin and under it.
	constexpr float TopPx = 24.0f;      // ADTHUD's margin: the clock's top
	constexpr float MarginPx = 24.0f;   // ADTHUD's margin: the clock's right edge
	constexpr float BlockWidthPx = 360.0f;
	constexpr float ClockScale = 1.5f;  // ADTHUD's title scale, at which the clock is drawn
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

void FDTXPBar::Draw(UCanvas* Canvas, UWorld* World, UFont* Font, float TextScale)
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

	auto Measure = [Canvas, Font, TextScale](const FText& Line, float& OutW, float& OutH)
	{
		Canvas->TextSize(Font, Line.ToString(), OutW, OutH, TextScale, TextScale);
	};
	auto Text = [Canvas, Font, &TextColor, TextScale](const FText& Line, float X, float Y)
	{
		FCanvasTextItem Item(FVector2D(X, Y), Line, Font, TextColor);
		Item.Scale = FVector2D(TextScale, TextScale);
		Item.EnableShadow(FLinearColor::Black);
		Canvas->DrawItem(Item);
	};
	auto Box = [Canvas](float X, float Y, float W, float H, const FLinearColor& Color)
	{
		FCanvasTileItem Tile(FVector2D(X, Y), FVector2D(W, H), Color);
		Tile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Tile);
	};

	// The top-right block, under the run clock (ADTHUD draws it at TopPx, right-aligned, at ClockScale): out of the hero's column.
	const FBox2f Block = BlockSpan(Canvas->ClipX);
	const float BarX = Block.Min.X;
	const float BarW = Block.GetSize().X;
	float ClockW = 0.0f;
	float ClockH = 0.0f;
	Canvas->TextSize(Font, TEXT("00:00"), ClockW, ClockH, ClockScale, ClockScale);
	const float ClockLeft = Block.Max.X - ClockW;
	const float ClockBottom = TopPx + ClockH;
	float Y = ClockBottom + RowGapPx;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FDTXPBarRow& Row = Rows[Index];
		FFormatNamedArguments Args;
		Args.Add(TEXT("Level"), FText::AsNumber(Row.Level));
		Args.Add(TEXT("XP"), FText::AsNumber(Row.XP));
		Args.Add(TEXT("XPToNext"), FText::AsNumber(Row.XPToNext));
		Args.Add(TEXT("Count"), FText::AsNumber(Row.PendingHands));
		const FText Label = FText::Format(LabelFormat, Args);
		float LabelW = 0.0f;
		float LineH = 0.0f;
		Measure(Label, LabelW, LineH);

		// The level: beside the clock on its baseline for the first player, else on its own line.
		if (Index == 0 && BarX + LabelW + LabelGapPx <= ClockLeft && LineH <= ClockH)
		{
			Text(Label, BarX, ClockBottom - LineH);
		}
		else
		{
			Text(Label, BarX, Y);
			Y += LineH + RowGapPx;
		}
		Box(BarX, Y, BarW, BarHeightPx, Back);
		Box(BarX, Y, BarW * Row.Progress01, BarHeightPx, Row.bFlash ? FlashFill : Fill);
		Y += BarHeightPx + RowGapPx;

		// Under the bar: the XP, then the waiting hands and the level-up flash, each on the line while it fits the block.
		float RightX = BarX;
		float LineBottom = Y;
		auto Place = [&](const FText& Line)
		{
			float W = 0.0f;
			float H = 0.0f;
			Measure(Line, W, H);
			if (RightX > BarX && RightX + W > Block.Max.X)
			{
				RightX = BarX;
				Y = LineBottom;
			}
			Text(Line, RightX, Y);
			RightX += W + LabelGapPx;
			LineBottom = FMath::Max(LineBottom, Y + H);
		};
		Place(FText::Format(XpFormat, Args));
		if (Row.PendingHands > 0)
		{
			Place(FText::Format(PendingFormat, Args));
		}
		if (Row.bFlash)
		{
			Place(LevelUp);
		}
		Y = LineBottom + RowGapPx;
	}
}

FBox2f FDTXPBar::BlockSpan(float ClipX)
{
	using namespace DTXPBarLocal;
	return FBox2f(FVector2f(ClipX - MarginPx - BlockWidthPx, TopPx), FVector2f(ClipX - MarginPx, TopPx));
}
