// Enemy rounds: a struct-of-arrays pool of their own (02_Systems/enemies.md R-ENM-15, R-ENM-16). Plain C++, testable
// without a world. Same layout, dense swap-removal, integration and expiry as FDTBulletPool; never shares its cap.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimTypes.h"

/** Everything needed to create one enemy round. */
struct FDTEnemyRoundSpawn
{
	FVector2f Position = FVector2f::ZeroVector;
	FVector2f Velocity = FVector2f::ZeroVector; // uu/s on the sim plane
	float Radius = 0.0f;
	/** Already scaled by the firing enemy's DmgScale. */
	float Damage = 0.0f;
	float Life = 0.0f; // seconds
	int32 TargetPlayer = INDEX_NONE;
	EDTEnemyRole SourceRole = EDTEnemyRole::Turret;
	FName AttackId;
	uint32 EnemySerial = 0;
};

/** Monotonic counters (DT.EnemyFire.Stats); reset only by Reset(). */
struct FDTEnemyRoundCounters
{
	int64 Spawned = 0;
	int64 Refused = 0;      // a spawn at C.ENEMY_FIRE.CAP (R-ENM-15): never recycles a live round
	int64 Expired = 0;
	int64 OutOfBounds = 0;
	int64 HeroHits = 0;
	int64 PassedThrough = 0; // steps a round overlapped an intangible hero and lived on (R-ENM-16)
};

class DUSKTRACER_API FDTEnemyBulletPool
{
public:
	explicit FDTEnemyBulletPool(int32 InCapacity = DTConst::ENEMY_FIRE_CAP);

	/** Adds a round; at capacity the spawn is refused and counted. Returns its index or INDEX_NONE. */
	int32 Spawn(const FDTEnemyRoundSpawn& Params);

	/**
	 * Saves PrevPos, integrates, expires (life, arena), then tests every round against every hero in
	 * View.HeroPositions (body radius C.HERO.RADIUS_UU): a touch appends one FDTHeroHit (Kind Bullet) to View.HeroHits
	 * and removes the round, unless View.HeroIntangible marks the hero, then the round passes through. Never touches
	 * View.Hits or the enemy hash.
	 */
	void Step(float Dt, FDTWorldView& View);

	/** Removes every round; counters are kept unless bResetCounters. */
	void Reset(bool bResetCounters = true);

	int32 Num() const { return Pos.Num(); }
	int32 GetCapacity() const { return Capacity; }
	const FDTEnemyRoundCounters& GetCounters() const { return Counters; }

	TConstArrayView<FVector2f> GetPositions() const { return Pos; }
	TConstArrayView<FVector2f> GetPrevPositions() const { return PrevPos; }
	TConstArrayView<FVector2f> GetVelocities() const { return Vel; }
	TConstArrayView<float> GetRadii() const { return Radius; }
	TConstArrayView<float> GetDamage() const { return Damage; }
	TConstArrayView<float> GetLifeLeft() const { return LifeLeft; }
	TConstArrayView<int32> GetTargets() const { return TargetPlayer; }

private:
	void RemoveAtSwap(int32 Index);

	int32 Capacity;
	TArray<FVector2f> Pos;
	TArray<FVector2f> PrevPos;
	TArray<FVector2f> Vel;
	TArray<float> Radius;
	TArray<float> Damage;
	TArray<float> LifeLeft;
	TArray<int32> TargetPlayer;
	TArray<EDTEnemyRole> SourceRole;
	TArray<FName> AttackId;
	TArray<uint32> EnemySerial;
	FDTEnemyRoundCounters Counters;
};
