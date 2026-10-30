// Struct-of-arrays bullet storage and step (02_Systems/bullets.md, 07_Technical R-TEC-11, R-TEC-13).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"

struct FDTWorldView;

/** Everything needed to create one bullet. Pierce is how many distinct enemies it may hit (1 = no pierce). */
struct FDTBulletSpawn
{
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

private:
	void RemoveAtSwap(int32 Index);
	bool HasHit(int32 Index, uint32 EnemySerial) const;
	void RememberHit(int32 Index, uint32 EnemySerial);

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

	TArray<int32> Candidates; // scratch for hash queries
	FDTBulletCounters Counters;
};
