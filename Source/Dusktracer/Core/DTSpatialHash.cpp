#include "Core/DTSpatialHash.h"

FDTSpatialHash::FDTSpatialHash(float InCellSize, float InHalfExtent)
	: CellSize(FMath::Max(1.0f, InCellSize))
	, HalfExtent(FMath::Max(1.0f, InHalfExtent))
{
	CellsPerSide = FMath::Max(1, FMath::CeilToInt32(2.0f * HalfExtent / CellSize));
	CellStart.Init(0, CellsPerSide * CellsPerSide + 1);
}

int32 FDTSpatialHash::CellCoord(float Value) const
{
	return FMath::Clamp(FMath::FloorToInt32((Value + HalfExtent) / CellSize), 0, CellsPerSide - 1);
}

void FDTSpatialHash::Build(TConstArrayView<FVector2f> Positions, TConstArrayView<int32> Indices)
{
	const int32 Count = Positions.Num();
	check(Indices.Num() == 0 || Indices.Num() == Count);
	const int32 NumCells = CellsPerSide * CellsPerSide;

	CellStart.SetNumZeroed(NumCells + 1, EAllowShrinking::No);
	FMemory::Memzero(CellStart.GetData(), CellStart.Num() * sizeof(int32));
	CellOfPoint.SetNumUninitialized(Count, EAllowShrinking::No);
	Items.SetNumUninitialized(Count, EAllowShrinking::No);

	for (int32 Point = 0; Point < Count; ++Point)
	{
		const int32 Cell = CellCoord(Positions[Point].Y) * CellsPerSide + CellCoord(Positions[Point].X);
		CellOfPoint[Point] = Cell;
		++CellStart[Cell + 1];
	}
	for (int32 Cell = 0; Cell < NumCells; ++Cell)
	{
		CellStart[Cell + 1] += CellStart[Cell];
	}
	// Scatter with a moving cursor per cell; CellStart is restored afterwards by shifting back.
	for (int32 Point = 0; Point < Count; ++Point)
	{
		const int32 Slot = CellStart[CellOfPoint[Point]]++;
		Items[Slot] = Indices.Num() ? Indices[Point] : Point;
	}
	for (int32 Cell = NumCells; Cell > 0; --Cell)
	{
		CellStart[Cell] = CellStart[Cell - 1];
	}
	CellStart[0] = 0;
}

void FDTSpatialHash::QueryCircle(const FVector2f& Center, float Radius, TArray<int32>& OutIndices) const
{
	const int32 MinX = CellCoord(Center.X - Radius);
	const int32 MaxX = CellCoord(Center.X + Radius);
	const int32 MinY = CellCoord(Center.Y - Radius);
	const int32 MaxY = CellCoord(Center.Y + Radius);
	for (int32 Y = MinY; Y <= MaxY; ++Y)
	{
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			const int32 Cell = Y * CellsPerSide + X;
			for (int32 Slot = CellStart[Cell]; Slot < CellStart[Cell + 1]; ++Slot)
			{
				OutIndices.Add(Items[Slot]);
			}
		}
	}
}
