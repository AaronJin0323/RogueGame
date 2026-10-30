#include "Enemies/DTEnemyPool.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"

FDTEnemyRoleDef FDTEnemyRoleDef::Get(EDTEnemyRole InRole)
{
	FDTEnemyRoleDef Def;
	switch (InRole)
	{
	case EDTEnemyRole::Swarm:
	default:
		Def.Radius = static_cast<float>(DTConst::ENM_SWARM_RADIUS_UU);
		Def.Speed = static_cast<float>(DTConst::ENM_SWARM_SPEED_UU_S);
		Def.MaxHp = static_cast<float>(DTConst::ENM_SWARM_HP);
		break;
	}
	return Def;
}

void FDTKillRateCounter::Reset()
{
	Ring.Reset();
	Cursor = 0;
	WindowSum = 0;
	WindowSeconds = 0.0f;
}

void FDTKillRateCounter::RecordStep(int32 Kills, float StepSeconds)
{
	if (Ring.Num() == 0)
	{
		const float Step = StepSeconds > 0.0f ? StepSeconds : 1.0f / DTConst::SIM_FIXED_HZ;
		const int32 Steps = FMath::Max(1, FMath::RoundToInt32(DTConst::ENM_KILL_RATE_WINDOW_S / Step));
		Ring.Init(0, Steps);
		WindowSeconds = Steps * Step;
	}
	WindowSum += Kills - Ring[Cursor];
	Ring[Cursor] = Kills;
	Cursor = (Cursor + 1) % Ring.Num();
}

float FDTKillRateCounter::GetKillsPerSecond() const
{
	return WindowSeconds > 0.0f ? WindowSum / WindowSeconds : 0.0f;
}

FDTEnemyPool::FDTEnemyPool()
	: Capacity(DTConst::ENEMIES_CAP)
{
	Pos.Reserve(Capacity);
	PrevPos.Reserve(Capacity);
	Vel.Reserve(Capacity);
	Radius.Reserve(Capacity);
	Hp.Reserve(Capacity);
	MaxHp.Reserve(Capacity);
	Speed.Reserve(Capacity);
	Role.Reserve(Capacity);
	Dead.Reserve(Capacity);
	Serial.Reserve(Capacity);
	NextVel.Reserve(Capacity);
}

void FDTEnemyPool::Reset(bool bResetCounters)
{
	Pos.Reset();
	PrevPos.Reset();
	Vel.Reset();
	Radius.Reset();
	Hp.Reset();
	MaxHp.Reset();
	Speed.Reset();
	Role.Reset();
	Dead.Reset();
	Serial.Reset();
	if (bResetCounters)
	{
		NextSerial = 1;
		RefusedSpawns = 0;
		TotalKills = 0;
		KillRate.Reset();
	}
}

int32 FDTEnemyPool::Spawn(EDTEnemyRole InRole, const FVector2f& Position)
{
	if (Num() >= Capacity)
	{
		++RefusedSpawns;
		return INDEX_NONE;
	}
	const FDTEnemyRoleDef Def = FDTEnemyRoleDef::Get(InRole);
	const float Limit = FMath::Max(0.0f, DTConst::ARENA_HALF_EXTENT_UU - Def.Radius);
	const FVector2f Clamped(FMath::Clamp(Position.X, -Limit, Limit), FMath::Clamp(Position.Y, -Limit, Limit));
	Pos.Add(Clamped);
	PrevPos.Add(Clamped);
	Vel.Add(FVector2f::ZeroVector);
	Radius.Add(Def.Radius);
	Hp.Add(Def.MaxHp);
	MaxHp.Add(Def.MaxHp);
	Speed.Add(Def.Speed);
	Role.Add(InRole);
	Dead.Add(0);
	Serial.Add(NextSerial);
	NextSerial = NextSerial == MAX_uint32 ? 1u : NextSerial + 1u; // 4e9 spawns: wrap past zero, never hand it out
	return Pos.Num() - 1;
}

int32 FDTEnemyPool::SpawnRing(EDTEnemyRole InRole, int32 Count, const FVector2f& Center, float RingRadius, FRandomStream& Rng)
{
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Always draw, even when refused, so the stream advances the same way whatever the population.
		const float Angle = Rng.FRandRange(0.0f, UE_TWO_PI);
		const FVector2f At = Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius;
		if (Spawn(InRole, At) != INDEX_NONE)
		{
			++Spawned;
		}
	}
	return Spawned;
}

void FDTEnemyPool::StepEnemies(const FDTSimContext& Context)
{
	check(Context.View);
	Move(Context.StepSeconds, Context.View->HeroPositions);
	Publish(*Context.View);
}

void FDTEnemyPool::Move(float Dt, TConstArrayView<FVector2f> Heroes)
{
	const int32 Count = Num();
	PrevPos = Pos;
	if (Count == 0 || Dt <= 0.0f)
	{
		return;
	}

	const float SepRadius = static_cast<float>(DTConst::ENM_SEPARATION_RADIUS_UU);
	const float SepStrength = static_cast<float>(DTConst::ENM_SEPARATION_STRENGTH);
	const int32 MaxNeighbours = DTConst::ENM_MAX_NEIGHBOURS;
	const float MaxDeltaV = static_cast<float>(DTConst::ENM_STEER_ACCEL_UU_S2) * Dt;

	// Neighbours come from positions at the start of the step, so the result does not depend on update order.
	NeighbourHash.Build(Pos);
	NextVel.SetNumUninitialized(Count, EAllowShrinking::No);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2f Here = Pos[Index];

		// Seek the nearest hero.
		FVector2f Seek = FVector2f::ZeroVector;
		float BestDistSq = TNumericLimits<float>::Max();
		for (const FVector2f& Hero : Heroes)
		{
			const float DistSq = FVector2f::DistSquared(Hero, Here);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Seek = Hero - Here;
			}
		}
		Seek = Seek.GetSafeNormal();

		// Separation: push away from at most MaxNeighbours overlapping neighbours, stronger the closer they are.
		FVector2f Push = FVector2f::ZeroVector;
		Candidates.Reset();
		NeighbourHash.QueryCircle(Here, SepRadius, Candidates);
		int32 Used = 0;
		for (const int32 Other : Candidates)
		{
			if (Other == Index)
			{
				continue;
			}
			const FVector2f Away = Here - Pos[Other];
			const float DistSq = Away.SizeSquared();
			if (DistSq >= SepRadius * SepRadius)
			{
				continue;
			}
			const float Dist = FMath::Sqrt(DistSq);
			// Coincident enemies: split along X by slot order (deterministic, no randomness in the step).
			const FVector2f Dir = Dist > UE_KINDA_SMALL_NUMBER ? Away / Dist : FVector2f(Index < Other ? 1.0f : -1.0f, 0.0f);
			Push += Dir * (1.0f - Dist / SepRadius);
			if (++Used >= MaxNeighbours)
			{
				break;
			}
		}

		FVector2f Desired = Seek + Push * SepStrength;
		Desired = Desired.GetClampedToMaxSize(1.0f) * Speed[Index];
		const FVector2f DeltaV = (Desired - Vel[Index]).GetClampedToMaxSize(MaxDeltaV);
		NextVel[Index] = (Vel[Index] + DeltaV).GetClampedToMaxSize(Speed[Index]);
	}

	const float HalfExtent = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Vel[Index] = NextVel[Index];
		const float Limit = FMath::Max(0.0f, HalfExtent - Radius[Index]);
		FVector2f Next = Pos[Index] + Vel[Index] * Dt;
		Next.X = FMath::Clamp(Next.X, -Limit, Limit);
		Next.Y = FMath::Clamp(Next.Y, -Limit, Limit);
		Pos[Index] = Next;
	}
}

void FDTEnemyPool::Publish(FDTWorldView& View) const
{
	View.EnemyPositions = Pos;
	View.EnemyRadii = Radius;
	View.EnemySerials = Serial;
	View.EnemyHash.Build(Pos);
}

int32 FDTEnemyPool::ApplyHits(TConstArrayView<FDTHit> Hits, FDTRunState& Run)
{
	int32 Kills = 0;
	for (const FDTHit& Hit : Hits)
	{
		if (!Pos.IsValidIndex(Hit.EnemyIndex) || Dead[Hit.EnemyIndex])
		{
			continue; // already killed by an earlier hit this step: one death, one Kill event
		}
		if (Hit.EnemySerial != 0 && Serial[Hit.EnemyIndex] != Hit.EnemySerial)
		{
			continue; // stale handle: the slot now holds another enemy (R-ENM-23)
		}
		const int32 Index = Hit.EnemyIndex;
		const float Dealt = FMath::Min(FMath::Max(0.0f, Hit.Damage), Hp[Index]);
		Hp[Index] -= Hit.Damage;
		Run.ApplyEvent({ EDTGameEventKind::Hit, Hit.OwnerId, FMath::RoundToInt32(Dealt) });
		if (Hp[Index] <= 0.0f)
		{
			Dead[Index] = 1;
			++Kills;
			Run.ApplyEvent({ EDTGameEventKind::Kill, Hit.OwnerId, 0 });
		}
	}
	TotalKills += Kills;
	return Kills;
}

int32 FDTEnemyPool::ResolveHits(const FDTSimContext& Context)
{
	check(Context.View && Context.Run);
	const int32 Kills = ApplyHits(Context.View->Hits, *Context.Run);
	Context.View->Hits.Reset();
	Compact();
	KillRate.RecordStep(Kills, Context.StepSeconds);
	return Kills;
}

void FDTEnemyPool::SwapRemove(int32 Index)
{
	Pos.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	PrevPos.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Vel.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Radius.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Hp.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	MaxHp.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Speed.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Role.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Dead.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Serial.RemoveAtSwap(Index, 1, EAllowShrinking::No);
}

int32 FDTEnemyPool::Compact()
{
	int32 Removed = 0;
	// Back to front: the element swapped in from the end has already been checked.
	for (int32 Index = Num() - 1; Index >= 0; --Index)
	{
		if (Dead[Index])
		{
			SwapRemove(Index);
			++Removed;
		}
	}
	return Removed;
}
