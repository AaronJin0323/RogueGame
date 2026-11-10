// The always-visible XP bar (02_Systems/progression.md R-PRG-11): one row per player, drawn by ADTHUD where the HUD's
// layout puts it (02_Systems/hud.md R-HUD-04), so it never hides behind the hand UI.
#pragma once

#include "CoreMinimal.h"

class FDTRunState;
class UWorld;
struct FDTHudLayout;
struct FDTUIPainter;

/** What one player's bar shows. */
struct FDTXPBarRow
{
	int32 OwnerId = INDEX_NONE;
	int32 Level = 1;
	int32 XP = 0;
	int32 XPToNext = 0;
	float Progress01 = 0.0f;
	int32 PendingHands = 0;
	/** Within the level-up flash window. */
	bool bFlash = false;
};

/** The lines of one row, from ST_UI Level.*. A line that is not shown now is empty. */
struct FDTXPBarText
{
	FText Level;
	FText Xp;
	FText Pending;
	FText LevelUp;
};

struct DUSKTRACER_API FDTXPBar
{
	/** One row per player of the run, always (R-PRG-11), whatever the hand queue or density. */
	static void BuildRows(const FDTRunState& Run, double SimTime, TConstArrayView<double> LastLevelUpTime, TArray<FDTXPBarRow>& Out);

	/** The rows of the world's run as they stand now. Empty without a run (no sim subsystem). */
	static void ReadRows(UWorld* World, TArray<FDTXPBarRow>& Out);

	/** What a row says: the level and the XP always, the pending count while hands wait, the flash line inside its window. */
	static FDTXPBarText Text(const FDTXPBarRow& Row);

	/** Draws one row into the rects the layout gave its player. */
	static void Draw(const FDTUIPainter& Painter, const FDTXPBarRow& Row, const FDTXPBarText& Text, const FDTHudLayout& Layout);
};
