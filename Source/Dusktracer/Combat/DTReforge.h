// Catalog access for the L16 transformations (02_Systems/reforging.md, 09_Catalog/reforging.csv). Pure functions over the
// generated DTCat::Reforges table: no world, no randomness.
#pragma once

#include "CoreMinimal.h"
#include "Combat/DTWeaponCatalog.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTRunState.h"

/** What an evolved weapon does (reforging.csv `behaviour`, R-RFG-05..14). */
enum class EDTEvolve : uint8
{
	None,
	OrbitLaunch,
	EmbedBlast,
	ContactBlades,
	SweepBeam,
	CarpetLine,
	MoveSpin,
	Persist,
	PulseWall,
	Vortex,
	Merge
};

/** What a Path transformation does (reforging.csv `behaviour`, R-RFG-15..17). */
enum class EDTPathReforge : uint8
{
	None,
	Overload,
	Kiln,
	Glacier
};

struct FDTWeaponSpec;

namespace DTReforge
{
	/** Row of Id (Transform.<Path> / Evolve.<Weapon>), or INDEX_NONE. */
	DUSKTRACER_API int32 FindRow(FName Id);
	/** The Evolve row of a weapon (its `evolution` column), or INDEX_NONE. */
	DUSKTRACER_API int32 FindEvolve(int32 Weapon);
	/** The Path row of a Keeper, or INDEX_NONE (a Keeper without a row yields its slot, R-RFG-02). */
	DUSKTRACER_API int32 FindPath(EDTPath Path);
	DUSKTRACER_API FName RowId(int32 Row);
	/** A row's parameter by its `effect` name (0 when absent). */
	DUSKTRACER_API float Param(int32 Row, const TCHAR* Name);
	DUSKTRACER_API EDTEvolve Evolution(int32 Row);
	DUSKTRACER_API EDTPathReforge PathReforge(int32 Row);
	/** The row's declared caps (R-RFG-04). */
	DUSKTRACER_API FDTCapList Caps(int32 Row);
	/** A weapon's declared caps as it stands: the row's once evolved (R-RFG-04). */
	DUSKTRACER_API FDTCapList DeclaredCaps(const FDTWeaponInstance& Weapon);
	/** Slot of the primary weapon: load rank one (R-RFG-02); INDEX_NONE with no weapon. */
	DUSKTRACER_API int32 PrimarySlot(TConstArrayView<FDTWeaponInstance> Weapons);
	/** Applies a row between steps one and two of R-WPN-09 (R-RFG-04). */
	DUSKTRACER_API void ApplyEvolution(int32 Row, const DTCat::FWeaponLevelRow& Level, FDTWeaponSpec& Spec);
	/** Path row of PathKind this player owns, or INDEX_NONE. */
	DUSKTRACER_API int32 OwnedPathRow(const FDTPlayerRunState& Player, EDTPathReforge PathKind);
}
