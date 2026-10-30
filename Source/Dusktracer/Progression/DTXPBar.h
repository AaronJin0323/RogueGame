// The always-visible XP bar (02_Systems/progression.md R-PRG-11): one row per player, drawn by ADTHUD on top of
// everything else so it never hides behind the hand UI.
#pragma once

#include "CoreMinimal.h"

class FDTRunState;
class UCanvas;
class UFont;
class UWorld;

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

struct DUSKTRACER_API FDTXPBar
{
	/** One row per player of the run, always (R-PRG-11), whatever the hand queue or density. */
	static void BuildRows(const FDTRunState& Run, double SimTime, TConstArrayView<double> LastLevelUpTime, TArray<FDTXPBarRow>& Out);

	/** Draws every player's row at the top centre of the canvas. No-op without a run (no sim subsystem). */
	static void Draw(UCanvas* Canvas, UWorld* World, UFont* Font);
};
