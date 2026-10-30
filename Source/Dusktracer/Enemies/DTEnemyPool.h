// Struct-of-arrays enemy simulation (02_Systems/enemies.md, 07_Technical §4). Plain C++ so tests drive it directly.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Core/DTSimTypes.h"
#include "Core/DTSpatialHash.h"

class FDTRunState;

// EDTEnemyRole lives in Core/DTSimTypes.h (shared by FDTKill, FDTHeroHit, FDTDropInMarker). S0 spawns the swarm only.

/** Per-role movement and body values, read from the generated constants (R-TEC-07). */
struct FDTEnemyRoleDef
{
	float Radius = 0.0f;
	float Speed = 0.0f;
	float MaxHp = 0.0f;

	static FDTEnemyRoleDef Get(EDTEnemyRole Role);
};

/** Kills per second over a sliding window of fixed steps (density LOD input, 04_Presentation R-PRE-06). */
class DUSKTRACER_API FDTKillRateCounter
{
public:
	void Reset();
	/** Records the kills of one step. StepSeconds sizes the window on first use. */
	void RecordStep(int32 Kills, float StepSeconds);
	float GetKillsPerSecond() const;

private:
	TArray<int32> Ring;
	int32 Cursor = 0;
	int32 WindowSum = 0;
	float WindowSeconds = 0.0f;
};

/**
 * Every live enemy, dense: slot i of each array is enemy i. Indices are stable from the Enemies phase to the end of
 * ResolveHits and change only in Compact (swap-remove), after every consumer of the step has run.
 */
class DUSKTRACER_API FDTEnemyPool
{
public:
	FDTEnemyPool();

	/** Removes every enemy; counters (kills, refusals) are kept unless bResetCounters. */
	void Reset(bool bResetCounters = true);

	/** Adds one enemy at Position. Returns its index, or INDEX_NONE (counted) when the pool is at C.ENEMIES.CAP. */
	int32 Spawn(EDTEnemyRole Role, const FVector2f& Position);

	/** Spawns up to Count enemies on a ring of Radius around Center; angles from Rng (the Spawn stream, R-TEC-02). Returns the number spawned. */
	int32 SpawnRing(EDTEnemyRole Role, int32 Count, const FVector2f& Center, float Radius, FRandomStream& Rng);

	/** EDTSimPhase::Enemies: steer and move every enemy, then publish positions, radii and the hash into Context.View. */
	void StepEnemies(const FDTSimContext& Context);

	/** Moves every enemy by one step of Dt towards the nearest of Heroes, with separation. */
	void Move(float Dt, TConstArrayView<FVector2f> Heroes);

	/** Writes positions, radii, serials and the rebuilt hash into View (dense; index = enemy slot). */
	void Publish(FDTWorldView& View) const;

	/**
	 * EDTSimPhase::ResolveHits: applies every hit in Context.View->Hits, emits Hit and Kill events through Context.Run,
	 * clears the hits, then compacts. Returns the number of kills.
	 */
	int32 ResolveHits(const FDTSimContext& Context);

	/**
	 * Applies Hits to the pool and emits events into Run; marks deaths but does not compact. Returns kills.
	 * A hit whose EnemySerial is non-zero and no longer matches its index is stale and ignored (R-ENM-23).
	 */
	int32 ApplyHits(TConstArrayView<FDTHit> Hits, FDTRunState& Run);

	/** Swap-removes every dead enemy. Returns the number removed. */
	int32 Compact();

	int32 Num() const { return Pos.Num(); }
	int32 GetCapacity() const { return Capacity; }
	int64 GetRefusedSpawns() const { return RefusedSpawns; }
	int64 GetTotalKills() const { return TotalKills; }
	const FDTKillRateCounter& GetKillRate() const { return KillRate; }

	// Read access to the arrays (render, tests).
	TConstArrayView<FVector2f> GetPositions() const { return Pos; }
	TConstArrayView<FVector2f> GetPrevPositions() const { return PrevPos; }
	TConstArrayView<FVector2f> GetVelocities() const { return Vel; }
	TConstArrayView<float> GetRadii() const { return Radius; }
	TConstArrayView<float> GetHp() const { return Hp; }
	TConstArrayView<float> GetMaxHp() const { return MaxHp; }
	TConstArrayView<float> GetSpeeds() const { return Speed; }
	TConstArrayView<EDTEnemyRole> GetRoles() const { return Role; }
	/** Run-unique, never zero, kept through swap-removal (R-ENM-23). */
	TConstArrayView<uint32> GetSerials() const { return Serial; }
	/** True while Handle still names the same enemy (its serial at its index matches). */
	bool IsHandleValid(const FDTEnemyHandle& Handle) const { return Serial.IsValidIndex(Handle.Index) && Handle.Serial != 0 && Serial[Handle.Index] == Handle.Serial; }
	FDTEnemyHandle GetHandle(int32 Index) const { return FDTEnemyHandle{ Index, Serial[Index] }; }
	bool IsDead(int32 Index) const { return Dead[Index] != 0; }

	/** Test hooks: place or wound an enemy directly. */
	void SetPosition(int32 Index, const FVector2f& Position) { Pos[Index] = Position; PrevPos[Index] = Position; }
	void SetHp(int32 Index, float Value) { Hp[Index] = Value; }

private:
	void SwapRemove(int32 Index);

	int32 Capacity = 0;
	TArray<FVector2f> Pos;
	TArray<FVector2f> PrevPos;
	TArray<FVector2f> Vel;
	TArray<float> Radius;
	TArray<float> Hp;
	TArray<float> MaxHp;
	TArray<float> Speed;
	TArray<EDTEnemyRole> Role;
	TArray<uint8> Dead;
	TArray<uint32> Serial;
	uint32 NextSerial = 1; // never zero; reset only with the counters (a new run)

	FDTSpatialHash NeighbourHash;
	TArray<int32> Candidates;
	TArray<FVector2f> NextVel;

	int64 RefusedSpawns = 0;
	int64 TotalKills = 0;
	FDTKillRateCounter KillRate;
};
