#include "Progression/DTXPBar.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/World.h"
#include "Game/DTHudLayout.h"
#include "Game/DTUIStyle.h"
#include "Internationalization/Text.h"
#include "Progression/DTGemSubsystem.h"

namespace DTXPBarLocal
{
	// The flash window; presentation, not tuning. Where the row sits is the HUD layout's (hud.md R-HUD-04).
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

void FDTXPBar::ReadRows(UWorld* World, TArray<FDTXPBarRow>& Out)
{
	Out.Reset();
	UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
	if (!Sim)
	{
		return;
	}
	const UDTGemSubsystem* Gems = World->GetSubsystem<UDTGemSubsystem>();
	TArray<double> LastUp;
	for (int32 Index = 0; Index < Sim->GetRun().GetNumPlayers(); ++Index)
	{
		LastUp.Add(Gems ? Gems->GetLastLevelUpTime(Index) : -1.0);
	}
	BuildRows(Sim->GetRun(), Gems ? Gems->GetSimTime() : 0.0, LastUp, Out);
}

FDTXPBarText FDTXPBar::Text(const FDTXPBarRow& Row)
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("Level"), FText::AsNumber(Row.Level));
	Args.Add(TEXT("XP"), FText::AsNumber(Row.XP));
	Args.Add(TEXT("XPToNext"), FText::AsNumber(Row.XPToNext));
	Args.Add(TEXT("Count"), FText::AsNumber(Row.PendingHands));
	FDTXPBarText Out;
	Out.Level = FText::Format(FText::FromStringTable(TEXT("ST_UI"), TEXT("Level.Label")), Args);
	Out.Xp = FText::Format(FText::FromStringTable(TEXT("ST_UI"), TEXT("Level.Tooltip")), Args);
	if (Row.PendingHands > 0)
	{
		Out.Pending = FText::Format(FText::FromStringTable(TEXT("ST_UI"), TEXT("Level.Pending")), Args);
	}
	if (Row.bFlash)
	{
		Out.LevelUp = FText::FromStringTable(TEXT("ST_UI"), TEXT("Level.Up"));
	}
	return Out;
}

void FDTXPBar::Draw(const FDTUIPainter& Painter, const FDTXPBarRow& Row, const FDTXPBarText& Text, const FDTHudLayout& Layout)
{
	if (!Painter.Canvas || !Painter.Font)
	{
		return;
	}
	const FLinearColor Ink = DTHudStyle::Duty(EDTHudDuty::Text);
	const auto Line = [&](EDTHudElem Kind, const FText& Words, EDTType Type)
	{
		if (const FDTHudRect* Rect = Words.IsEmpty() ? nullptr : Layout.Find(Kind, Row.OwnerId))
		{
			Painter.Text(Words, Rect->Box.Min, Type, Ink, Rect->TextMul);
		}
	};
	Line(EDTHudElem::Level, Text.Level, EDTType::Title);
	if (const FDTHudRect* Bar = Layout.Find(EDTHudElem::XpBar, Row.OwnerId))
	{
		// R-PRG-11: a cool bar, and the flash is a second cool token so the level-up never reads as a warning.
		Painter.Bar(Bar->Box, Row.Progress01, DTHudStyle::Duty(Row.bFlash ? EDTHudDuty::XpFlash : EDTHudDuty::Xp));
	}
	Line(EDTHudElem::XpText, Text.Xp, EDTType::Body);
	Line(EDTHudElem::Pending, Text.Pending, EDTType::Body);
	Line(EDTHudElem::LevelUp, Text.LevelUp, EDTType::Body);
}
