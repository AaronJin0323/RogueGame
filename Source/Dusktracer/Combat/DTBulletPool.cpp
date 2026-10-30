#include "Combat/DTBulletPool.h"
#include "Core/DTSimTypes.h"

FDTBulletPool::FDTBulletPool(int32 InCapacity)
	: Capacity(FMath::Max(1, InCapacity))
{
	Pos.Reserve(Capacity);
	PrevPos.Reserve(Capacity);
	Vel.Reserve(Capacity);
	Radius.Reserve(Capacity);
	Damage.Reserve(Capacity);
	LifeLeft.Reserve(Capacity);
	OwnerId.Reserve(Capacity);
	PierceLeft.Reserve(Capacity);
	SourceId.Reserve(Capacity);
	WeaponSlot.Reserve(Capacity);
	Crit.Reserve(Capacity);
	Serial.Reserve(Capacity);
	HitCount.Reserve(Capacity);
	HitRing.Reserve(Capacity * DTConst::BULLETS_HIT_MEMORY);
}

void FDTBulletPool::Reset()
{
	Pos.Reset();
	PrevPos.Reset();
	Vel.Reset();
	Radius.Reset();
	Damage.Reset();
	LifeLeft.Reset();
	OwnerId.Reset();
	PierceLeft.Reset();
	SourceId.Reset();
	WeaponSlot.Reset();
	Crit.Reset();
	Serial.Reset();
	HitCount.Reset();
	HitRing.Reset();
	NextSerial = 0;
	Counters = FDTBulletCounters();
}

int32 FDTBulletPool::Spawn(const FDTBulletSpawn& Params)
{
	int32 Index = Pos.Num();
	if (Index >= Capacity)
	{
		// Full: replace the oldest live bullet in place (never drop a spawn silently).
		Index = 0;
		for (int32 Other = 1; Other < Serial.Num(); ++Other)
		{
			if (Serial[Other] < Serial[Index])
			{
				Index = Other;
			}
		}
		++Counters.Recycled;
	}
	else
	{
		Pos.AddUninitialized();
		PrevPos.AddUninitialized();
		Vel.AddUninitialized();
		Radius.AddUninitialized();
		Damage.AddUninitialized();
		LifeLeft.AddUninitialized();
		OwnerId.AddUninitialized();
		PierceLeft.AddUninitialized();
		SourceId.AddUninitialized();
		WeaponSlot.AddUninitialized();
		Crit.AddUninitialized();
		Serial.AddUninitialized();
		HitCount.AddUninitialized();
		HitRing.AddUninitialized(DTConst::BULLETS_HIT_MEMORY);
	}
	Pos[Index] = Params.Position;
	PrevPos[Index] = Params.Position;
	Vel[Index] = Params.Velocity;
	Radius[Index] = Params.Radius;
	Damage[Index] = Params.Damage;
	LifeLeft[Index] = Params.Life;
	OwnerId[Index] = Params.OwnerId;
	PierceLeft[Index] = FMath::Max(1, Params.Pierce);
	SourceId[Index] = Params.SourceId;
	WeaponSlot[Index] = Params.WeaponSlot;
	Crit[Index] = Params.bCrit ? 1 : 0;
	Serial[Index] = NextSerial++;
	HitCount[Index] = 0;
	++Counters.Spawned;
	return Index;
}

void FDTBulletPool::RemoveAtSwap(int32 Index)
{
	const int32 Last = Pos.Num() - 1;
	if (Index != Last)
	{
		Pos[Index] = Pos[Last];
		PrevPos[Index] = PrevPos[Last];
		Vel[Index] = Vel[Last];
		Radius[Index] = Radius[Last];
		Damage[Index] = Damage[Last];
		LifeLeft[Index] = LifeLeft[Last];
		OwnerId[Index] = OwnerId[Last];
		PierceLeft[Index] = PierceLeft[Last];
		SourceId[Index] = SourceId[Last];
		WeaponSlot[Index] = WeaponSlot[Last];
		Crit[Index] = Crit[Last];
		Serial[Index] = Serial[Last];
		HitCount[Index] = HitCount[Last];
		FMemory::Memcpy(&HitRing[Index * DTConst::BULLETS_HIT_MEMORY], &HitRing[Last * DTConst::BULLETS_HIT_MEMORY],
			sizeof(uint32) * DTConst::BULLETS_HIT_MEMORY);
	}
	Pos.Pop(EAllowShrinking::No);
	PrevPos.Pop(EAllowShrinking::No);
	Vel.Pop(EAllowShrinking::No);
	Radius.Pop(EAllowShrinking::No);
	Damage.Pop(EAllowShrinking::No);
	LifeLeft.Pop(EAllowShrinking::No);
	OwnerId.Pop(EAllowShrinking::No);
	PierceLeft.Pop(EAllowShrinking::No);
	SourceId.Pop(EAllowShrinking::No);
	WeaponSlot.Pop(EAllowShrinking::No);
	Crit.Pop(EAllowShrinking::No);
	Serial.Pop(EAllowShrinking::No);
	HitCount.Pop(EAllowShrinking::No);
	HitRing.SetNum(Last * DTConst::BULLETS_HIT_MEMORY, EAllowShrinking::No);
}

bool FDTBulletPool::HasHit(int32 Index, uint32 EnemySerial) const
{
	const int32 Stored = FMath::Min(HitCount[Index], DTConst::BULLETS_HIT_MEMORY);
	const uint32* Ring = &HitRing[Index * DTConst::BULLETS_HIT_MEMORY];
	for (int32 Slot = 0; Slot < Stored; ++Slot)
	{
		if (Ring[Slot] == EnemySerial)
		{
			return true;
		}
	}
	return false;
}

void FDTBulletPool::RememberHit(int32 Index, uint32 EnemySerial)
{
	HitRing[Index * DTConst::BULLETS_HIT_MEMORY + HitCount[Index] % DTConst::BULLETS_HIT_MEMORY] = EnemySerial;
	++HitCount[Index];
}

void FDTBulletPool::Step(float Dt, FDTWorldView& View)
{
	PrevPos = Pos;

	const float HalfExtent = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU);
	const int32 NumEnemies = FMath::Min3(View.EnemyPositions.Num(), View.EnemyRadii.Num(), View.EnemySerials.Num());
	float MaxEnemyRadius = 0.0f;
	for (int32 Enemy = 0; Enemy < NumEnemies; ++Enemy)
	{
		MaxEnemyRadius = FMath::Max(MaxEnemyRadius, View.EnemyRadii[Enemy]);
	}
	const bool bCollide = NumEnemies > 0 && View.EnemyHash.Num() > 0;

	// Removal swaps the last bullet into Index; that bullet has not been stepped yet, so Index is not advanced.
	int32 Index = 0;
	while (Index < Pos.Num())
	{
		LifeLeft[Index] -= Dt;
		Pos[Index] += Vel[Index] * Dt;
		const FVector2f P = Pos[Index];
		if (LifeLeft[Index] <= 0.0f)
		{
			++Counters.Expired;
			RemoveAtSwap(Index);
			continue;
		}
		if (FMath::Abs(P.X) > HalfExtent || FMath::Abs(P.Y) > HalfExtent)
		{
			++Counters.OutOfBounds;
			RemoveAtSwap(Index);
			continue;
		}

		bool bSpent = false;
		if (bCollide)
		{
			const float R = Radius[Index];
			Candidates.Reset();
			View.EnemyHash.QueryCircle(P, R + MaxEnemyRadius, Candidates);
			for (const int32 Enemy : Candidates)
			{
				if (Enemy < 0 || Enemy >= NumEnemies)
				{
					continue;
				}
				const uint32 EnemySerial = View.EnemySerials[Enemy];
				if (HasHit(Index, EnemySerial))
				{
					continue;
				}
				const float Reach = R + View.EnemyRadii[Enemy];
				if (FVector2f::DistSquared(P, View.EnemyPositions[Enemy]) > Reach * Reach)
				{
					continue;
				}
				FDTHit& Hit = View.Hits.AddDefaulted_GetRef();
				Hit.EnemyIndex = Enemy;
				Hit.EnemySerial = EnemySerial;
				Hit.OwnerId = OwnerId[Index];
				Hit.Damage = Damage[Index];
				Hit.Position = P;
				Hit.SourceId = SourceId[Index];
				Hit.WeaponSlot = WeaponSlot[Index];
				Hit.bCrit = Crit[Index] != 0;
				// Spawned pierce = what is left + what was used (PierceLeft drops by one per remembered hit).
				Hit.bPierce = PierceLeft[Index] + HitCount[Index] > 1;
				++Counters.Hits;
				RememberHit(Index, EnemySerial);
				if (--PierceLeft[Index] <= 0)
				{
					bSpent = true;
					break;
				}
			}
		}
		if (bSpent)
		{
			++Counters.Spent;
			RemoveAtSwap(Index);
			continue;
		}
		++Index;
	}
}
