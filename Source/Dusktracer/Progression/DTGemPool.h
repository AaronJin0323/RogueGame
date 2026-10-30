// Struct-of-arrays XP gems and chests (02_Systems/progression.md R-PRG-07..10). Plain C++ so tests drive it directly.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSpatialHash.h"

/** A gem the director drops (progression.md Data). */
struct FDTGemSpawn
{
	FVector2f Position = FVector2f::ZeroVector;
	int32 Value = 0;
	/** The killer; telemetry only, never gameplay. */
	int32 OwnerId = INDEX_NONE;
};

/** What a spawn did (R-PRG-07): added a gem, or added its value to the oldest live gem at C.PICKUP.GEM_CAP. */
enum class EDTGemSpawnResult : uint8
{
	Added,
	Merged,
	Rejected
};

/** The heroes the pool steps against: index = player. Absent players (down, left) neither bind nor keep gems. */
struct FDTPickupHeroes
{
	TConstArrayView<FVector2f> Positions;
	/** Missing entries count as present. */
	TConstArrayView<bool> Present;
	/** Pickup-radius stat per player (missing = 1). */
	TConstArrayView<float> RadiusMul;
};

/** One gem collected this step, in collection order. */
struct FDTGemCollect
{
	int32 PlayerId = INDEX_NONE;
	int32 Value = 0;
};

/** What one step collected. */
struct FDTPickupStepResult
{
	TArray<FDTGemCollect> Gems;
	/** One entry per chest collected this step: the collector. */
	TArray<int32> ChestCollectors;

	void Reset()
	{
		Gems.Reset();
		ChestCollectors.Reset();
	}
};

/**
 * Every live gem, dense and in spawn order (index 0 is the oldest): Pos PrevPos Vel Value BoundTo SpawnStep; chests in
 * a small array beside it. Collection compacts stably, so the order survives. Nothing here draws randomness (R-PRG-10).
 */
class DUSKTRACER_API FDTGemPool
{
public:
	FDTGemPool();

	/** Removes every gem and chest and zeroes the counters. */
	void Reset();

	/** R-PRG-07: adds a gem, or at C.PICKUP.GEM_CAP adds its value to the oldest live gem. Non-positive values are rejected. */
	EDTGemSpawnResult Spawn(const FDTGemSpawn& Gem, int64 StepIndex);

	/** R-PRG-09: a chest at Position (placed by the director, R-DIR-16). It never moves. Returns its index. */
	int32 SpawnChest(const FVector2f& Position);

	/** R-PRG-09: binds every live free gem to OwnerId (R-PRG-08 motion). Already-bound gems keep their player. Returns the number bound. */
	int32 MagnetAll(int32 OwnerId);

	/**
	 * EDTSimPhase::Pickups: chests are collected by the first present hero within C.PICKUP.COLLECT_RADIUS_UU plus
	 * C.HERO.RADIUS_UU (which then magnetises every free gem to them); bindings of absent players are freed; free gems
	 * inside a player's radius bind to the nearest such player (hash query, R-TEC-13); bound gems accelerate toward their
	 * player and are collected the step they come within C.PICKUP.COLLECT_RADIUS_UU.
	 */
	void Step(float Dt, const FDTPickupHeroes& Heroes, FDTPickupStepResult& Out);

	int32 Num() const { return Pos.Num(); }
	int32 NumChests() const { return ChestPos.Num(); }
	TConstArrayView<FVector2f> GetPositions() const { return Pos; }
	TConstArrayView<FVector2f> GetPrevPositions() const { return PrevPos; }
	TConstArrayView<FVector2f> GetVelocities() const { return Vel; }
	TConstArrayView<int32> GetValues() const { return Value; }
	TConstArrayView<int32> GetBoundTo() const { return BoundTo; }
	TConstArrayView<int64> GetSpawnSteps() const { return SpawnStep; }
	TConstArrayView<FVector2f> GetChestPositions() const { return ChestPos; }

	/** XP lying on the floor (sum of live gem values). */
	int64 GetFloorXP() const;
	int64 GetTotalSpawned() const { return TotalSpawned; }
	int64 GetTotalMerged() const { return TotalMerged; }
	int64 GetTotalCollected() const { return TotalCollected; }
	int64 GetTotalChestsCollected() const { return TotalChests; }

private:
	void BindFree(const FDTPickupHeroes& Heroes);
	void Compact();

	TArray<FVector2f> Pos;
	TArray<FVector2f> PrevPos;
	TArray<FVector2f> Vel;
	TArray<int32> Value;
	TArray<int32> BoundTo;
	TArray<int64> SpawnStep;
	TArray<bool> Collected;
	TArray<FVector2f> ChestPos;

	FDTSpatialHash FreeHash;
	TArray<FVector2f> FreePos;
	TArray<int32> FreeIdx;
	TArray<int32> Candidates;
	TArray<float> BestDistSq;
	TArray<int32> BestPlayer;

	int64 TotalSpawned = 0;
	int64 TotalMerged = 0;
	int64 TotalCollected = 0;
	int64 TotalChests = 0;
};
