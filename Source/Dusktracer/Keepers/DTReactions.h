// Duo reactions and duo blessings as data (02_Systems/reactions.md): the catalog's pair rows, read by offers, the Keeper layer
// and the weapon system's reaction step (Combat/DTWeaponSimDuos.cpp). Pure functions: no world, no randomness.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTRunState.h"

namespace DTReactions
{
	/** The pool id that puts reactions and duo cards in a run (R-REA-02). */
	DUSKTRACER_API FName UnlockId();
	/** Row of a Blessing.Duo.<Pair> id, INDEX_NONE for any other id. */
	DUSKTRACER_API int32 FindDuo(FName BlessingId);
	/** Row of the pair of these two Keepers in either order, INDEX_NONE for no pair. */
	DUSKTRACER_API int32 FindPair(EDTPath A, EDTPath B);
	/** Whether a duo card counts as a blessing of Path (R-REA-09). */
	DUSKTRACER_API bool DuoCountsFor(int32 Row, EDTPath Path);
	/** Telemetry source Keeper.Duo.<Pair> (R-REA-03). */
	DUSKTRACER_API uint16 Source(int32 Row);
	/** Bit per row: the pairs whose two Keepers both have a blessing owned (R-REA-01). */
	DUSKTRACER_API uint32 LiveMask(const FDTKeeperRunState& Keepers);
	/** Bit per row: the owned duo blessings. */
	DUSKTRACER_API uint32 DuoMask(const FDTKeeperRunState& Keepers);
	/** A number of the row's Reaction cell. */
	DUSKTRACER_API float Param(int32 Row, const TCHAR* Name, float Default = 0.0f);
	/** A number of the row's Duo cell. */
	DUSKTRACER_API float DuoParam(int32 Row, const TCHAR* Name, float Default = 0.0f);
}

/** Per owner, per pair (DT.Reactions.Stats, tests). Monotonic within a run. */
struct FDTReactionCounters
{
	int64 Fires = 0;
	double Damage = 0.0;
};

/** What a step's reactions tell telemetry: fires and damage of one pair for one owner. */
struct FDTReactionEvent
{
	int32 OwnerId = INDEX_NONE;
	int32 Row = INDEX_NONE;
	int32 Fires = 0;
	float Damage = 0.0f;
	/** This step held the pair's first fire of the run. */
	bool bFirst = false;
};
