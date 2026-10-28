// Uniform grid on the XY plane, rebuilt every step by counting sort (07_Technical §4, R-TEC-13).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"

/**
 * Buckets points into square cells over [-HalfExtent, +HalfExtent]^2; points outside are clamped into the edge cells,
 * so nothing is ever lost. Build is O(N + cells); a query visits only the cells its circle overlaps. Indices are the
 * caller's (e.g. enemy slot indices); the grid stores no positions of its own.
 */
class DUSKTRACER_API FDTSpatialHash
{
public:
	explicit FDTSpatialHash(float InCellSize = DTConst::HASH_CELL_UU, float InHalfExtent = DTConst::ARENA_HALF_EXTENT_UU);

	/** Rebuilds from Positions; Indices[i] is stored for Positions[i]. Pass an empty Indices to store i itself. */
	void Build(TConstArrayView<FVector2f> Positions, TConstArrayView<int32> Indices = {});

	/** Appends every stored index whose cell overlaps the circle. Callers do the exact distance test. */
	void QueryCircle(const FVector2f& Center, float Radius, TArray<int32>& OutIndices) const;

	int32 GetCellsPerSide() const { return CellsPerSide; }
	float GetCellSize() const { return CellSize; }
	int32 Num() const { return Items.Num(); }

private:
	int32 CellCoord(float Value) const;

	float CellSize;
	float HalfExtent;
	int32 CellsPerSide;
	TArray<int32> CellStart; // CellsPerSide^2 + 1 prefix sums
	TArray<int32> Items;     // indices sorted by cell
	TArray<int32> CellOfPoint;
};
