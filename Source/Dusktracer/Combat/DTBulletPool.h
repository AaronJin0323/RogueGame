// Struct-of-arrays bullet storage and step (02_Systems/bullets.md, 07_Technical R-TEC-11, R-TEC-13).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"

struct FDTWorldView;

/**
 * How a round moves beyond a straight line (R-BUL-09): steering, an orbit before release, a return to the hero. All zero is
 * a straight round; straight rounds cost one byte test per step.
 */
struct FDTBulletMotion
{
	/** Homing: the turn limit; 0 = no steering. */
	float TurnDegS = 0.0f;
	/** Homing: how far it looks for a new target. */
	float SeekUu = 0.0f;
	/** Homing: the enemy it hunts (0 = the nearest within SeekUu, taken when it needs one). */
	uint32 TargetSerial = 0;
	/** Orbit: seconds left circling the owner's hero. */
	float OrbitLeft = 0.0f;
	float OrbitRadius = 0.0f;
	float OrbitDegS = 0.0f;
	float OrbitAngleDeg = 0.0f;
	/** Orbit: how far the release looks for the nearest enemy when no boss lives. */
	float ReleaseRange = 0.0f;
	/** Boomerang: turns home once LifeLeft falls to this, or at the hit that spends it. */
	bool bBoomerang = false;
	bool bReturning = false;
	float ReturnAt = 0.0f;
	/** Filled at spawn: the round's flight speed and pierce (a turn home restores the pierce). */
	float Speed = 0.0f;
	int32 Pierce = 1;
	/** Non-zero: the weapon system mirrors this round and reads its moves (GetRoundReports). */
	uint32 RoundId = 0;

	/** Vortex: the velocity turns this fast, always (reforging.md R-RFG-13). */
	float CurlDegS = 0.0f;
	/** Murmuration: drops its target after each hit and never seeks one it struck (R-RFG-11). */
	bool bRetarget = false;
	bool IsActive() const { return TurnDegS > 0.0f || OrbitLeft > 0.0f || bBoomerang || CurlDegS != 0.0f; }
};

/** Where a mirrored moving round is this step, before collision (the mirror cannot redo its steering). */
struct FDTRoundReport
{
	uint32 RoundId = 0;
	FVector2f Position = FVector2f::ZeroVector;
	FVector2f Velocity = FVector2f::ZeroVector;
	float LifeLeft = 0.0f;
	int32 PierceLeft = 0;
	bool bReturning = false;
};

/** Everything needed to create one bullet. Pierce is how many distinct enemies it may hit (1 = no pierce). */
struct FDTBulletSpawn
{
	FDTBulletMotion Motion;
	FVector2f Position = FVector2f::ZeroVector;
	FVector2f Velocity = FVector2f::ZeroVector; // uu/s on the sim plane
	float Radius = 0.0f;
	float Damage = 0.0f;
	float Life = 0.0f; // seconds
	int32 OwnerId = INDEX_NONE;
	int32 Pierce = 1;
	/** Telemetry source id (FDTTelemetrySources; 0 = Unknown), copied onto every FDTHit this bullet makes (R-BUL-08). */
	uint16 SourceId = 0;
	/** The owner's weapon slot, copied onto every hit (weapons.md R-WPN-12); INDEX_NONE for stress fire. */
	int8 WeaponSlot = INDEX_NONE;
	/** Rolled crit, copied onto every hit (R-WPN-12); Damage already includes it. */
	bool bCrit = false;
};

/** Monotonic counters; never reset except by Reset(). */
struct FDTBulletCounters
{
	int64 Spawned = 0;
	int64 Recycled = 0;    // a spawn at capacity replaced the oldest live bullet (R-BUL-03)
	int64 Expired = 0;     // life ran out
	int64 OutOfBounds = 0; // left the arena
	int64 Spent = 0;       // pierce exhausted
	int64 Hits = 0;
};

/**
 * Dense SoA bullets. Live bullets occupy [0, Num()); removal is swap-with-last, so iteration order changes but no
 * bullet is skipped or stepped twice. Plain C++: no UObject, unit-testable without a world.
 */
class DUSKTRACER_API FDTBulletPool
{
public:
	explicit FDTBulletPool(int32 InCapacity = DTConst::BULLETS_CAP);

	/** Adds a bullet; at capacity it replaces the oldest live bullet and counts a recycle. Returns its index. */
	int32 Spawn(const FDTBulletSpawn& Params);

	/**
	 * Saves PrevPos, integrates, expires, collides against View's enemies, appends to View.Hits. Only enemies with a
	 * published serial (View.EnemySerials) are collidable; hit memory is keyed by serial (R-BUL-04, enemies.md R-ENM-23).
	 */
	void Step(float Dt, FDTWorldView& View);

	void Reset();

	int32 Num() const { return Pos.Num(); }
	int32 GetCapacity() const { return Capacity; }
	const FDTBulletCounters& GetCounters() const { return Counters; }

	// Read-only SoA access for rendering and tests; index < Num().
	TConstArrayView<FVector2f> GetPositions() const { return Pos; }
	TConstArrayView<FVector2f> GetPrevPositions() const { return PrevPos; }
	TConstArrayView<float> GetRadii() const { return Radius; }
	TConstArrayView<int32> GetOwners() const { return OwnerId; }
	TConstArrayView<uint16> GetSources() const { return SourceId; }
	/** This step's moves of mirrored moving rounds (FDTBulletMotion::RoundId), written in Step. */
	TConstArrayView<FDTRoundReport> GetRoundReports() const { return RoundReports; }

private:
	void RemoveAtSwap(int32 Index);
	bool HasHit(int32 Index, uint32 EnemySerial) const;
	void RememberHit(int32 Index, uint32 EnemySerial);
	/** Applies the round's motion for this step (R-BUL-09): may set its velocity, its life, or turn it home. */
	void StepMotion(int32 Index, float Dt, const FDTWorldView& View, int32 NumEnemies);
	void TurnHome(int32 Index, bool bForget);
	/** The nearest enemy within Range; SkipHitsOf (a round index) leaves out what that round already struck. */
	bool FindSeekTarget(const FVector2f& From, float Range, const FDTWorldView& View, int32 NumEnemies, int32& OutEnemy, int32 SkipHitsOf = INDEX_NONE);

	int32 Capacity;
	uint64 NextSerial = 0;

	TArray<FVector2f> Pos;
	TArray<FVector2f> PrevPos;
	TArray<FVector2f> Vel;
	TArray<float> Radius;
	TArray<float> Damage;
	TArray<float> LifeLeft;
	TArray<int32> OwnerId;
	TArray<int32> PierceLeft;
	TArray<uint16> SourceId; // telemetry source of every hit this bullet makes (R-BUL-08)
	TArray<int8> WeaponSlot; // copied onto every hit (R-WPN-12)
	TArray<uint8> Crit;      // copied onto every hit as bCrit
	TArray<uint64> Serial;   // spawn order; the smallest is the oldest live bullet
	TArray<int32> HitCount;  // enemies this bullet has hit; ring slot = HitCount % C.BULLETS.HIT_MEMORY
	TArray<uint32> HitRing;  // C.BULLETS.HIT_MEMORY slots per bullet: serials of the last enemies it hit (R-BUL-04)
	TArray<uint8> Moves;     // 1 = Motion[Index] is active (R-BUL-09)
	TArray<FDTBulletMotion> Motion;

	TArray<int32> Candidates; // scratch for hash queries
	TArray<int32> SeekScratch;
	TMap<uint32, int32> SerialToEnemy; // built once per Step, on the first target lookup
	bool bSerialsBuilt = false;
	TArray<FDTRoundReport> RoundReports;
	FDTBulletCounters Counters;
};
