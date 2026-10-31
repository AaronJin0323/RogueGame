// What the HUD draws for Keeper state (keepers.md R-KEE-38): plain data, so tests read it without a canvas.
#pragma once

#include "CoreMinimal.h"
#include "Combat/DTWeaponSim.h"

/** One meter line: a label from ST_UI and a bar. */
struct FDTHudMeter
{
	/** ST_UI key; its text takes {Value} and {Max}. */
	FName Key;
	float Value = 0.0f;
	float Max = 0.0f;
	/** Zero to one; below zero no bar is drawn. */
	float Fill01 = -1.0f;
	/** A palette token (DTPal), never a literal. */
	FColor Color = FColor::White;
};

namespace DTKeeperMeters
{
	/** The lines of one player, in drawing order. DilationLeft is 0 with no scaling cast of theirs running. */
	DUSKTRACER_API void Build(const FDTKeeperMeterRead& Read, float DilationLeft, float DilationTotal, TArray<FDTHudMeter>& Out);
}
