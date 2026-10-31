#include "Enemies/DTEnemyBulletPool.h"

FDTEnemyBulletPool::FDTEnemyBulletPool(int32 InCapacity)
	: Capacity(FMath::Max(1, InCapacity))
{
	Pos.Reserve(Capacity);
	PrevPos.Reserve(Capacity);
	Vel.Reserve(Capacity);
	Radius.Reserve(Capacity);
	Damage.Reserve(Capacity);
	LifeLeft.Reserve(Capacity);
	TargetPlayer.Reserve(Capacity);
	SourceRole.Reserve(Capacity);
	AttackId.Reserve(Capacity);
	EnemySerial.Reserve(Capacity);
}

int32 FDTEnemyBulletPool::RemoveInCircle(const FVector2f& Center, float CircleRadius)
{
	int32 Removed = 0;
	for (int32 Index = Pos.Num() - 1; Index >= 0; --Index)
	{
		const float Reach = CircleRadius + Radius[Index];
		if (FVector2f::DistSquared(Pos[Index], Center) <= Reach * Reach)
		{
			RemoveAtSwap(Index);
			++Removed;
		}
	}
	return Removed;
}

void FDTEnemyBulletPool::Reset(bool bResetCounters)
{
	Pos.Reset();
	PrevPos.Reset();
	Vel.Reset();
	Radius.Reset();
	Damage.Reset();
	LifeLeft.Reset();
	TargetPlayer.Reset();
	SourceRole.Reset();
	AttackId.Reset();
	EnemySerial.Reset();
	if (bResetCounters)
	{
		Counters = FDTEnemyRoundCounters();
	}
}

int32 FDTEnemyBulletPool::Spawn(const FDTEnemyRoundSpawn& Params)
{
	if (Pos.Num() >= Capacity)
	{
		++Counters.Refused;
		return INDEX_NONE;
	}
	Pos.Add(Params.Position);
	PrevPos.Add(Params.Position);
	Vel.Add(Params.Velocity);
	Radius.Add(Params.Radius);
	Damage.Add(Params.Damage);
	LifeLeft.Add(Params.Life);
	TargetPlayer.Add(Params.TargetPlayer);
	SourceRole.Add(Params.SourceRole);
	AttackId.Add(Params.AttackId);
	EnemySerial.Add(Params.EnemySerial);
	++Counters.Spawned;
	return Pos.Num() - 1;
}

void FDTEnemyBulletPool::RemoveAtSwap(int32 Index)
{
	Pos.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	PrevPos.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Vel.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Radius.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Damage.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	LifeLeft.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	TargetPlayer.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	SourceRole.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	AttackId.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	EnemySerial.RemoveAtSwap(Index, 1, EAllowShrinking::No);
}

void FDTEnemyBulletPool::Step(float Dt, FDTWorldView& View)
{
	PrevPos = Pos;
	const float HalfExtent = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU);
	const float HeroRadius = static_cast<float>(DTConst::HERO_RADIUS_UU);
	const int32 NumHeroes = View.HeroPositions.Num();

	// Removal swaps the last round into Index; that round has not been stepped yet, so Index is not advanced.
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

		bool bHit = false;
		const float Reach = Radius[Index] + HeroRadius;
		for (int32 Hero = 0; Hero < NumHeroes; ++Hero)
		{
			if (FVector2f::DistSquared(View.HeroPositions[Hero], P) > Reach * Reach)
			{
				continue;
			}
			if (View.HeroIntangible.IsValidIndex(Hero) && View.HeroIntangible[Hero])
			{
				++Counters.PassedThrough;
				continue;
			}
			FDTHeroHit& Hit = View.HeroHits.AddDefaulted_GetRef();
			Hit.PlayerId = Hero;
			Hit.Damage = Damage[Index];
			Hit.Kind = EDTHeroHitKind::Bullet;
			Hit.SourceRole = SourceRole[Index];
			Hit.AttackId = AttackId[Index];
			Hit.EnemySerial = EnemySerial[Index];
			Hit.Position = P;
			++Counters.HeroHits;
			bHit = true;
			break; // one round, one hero hit
		}
		if (bHit)
		{
			RemoveAtSwap(Index);
			continue;
		}
		++Index;
	}
}
