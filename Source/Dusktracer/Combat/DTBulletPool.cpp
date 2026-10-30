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
	Moves.Reserve(Capacity);
	Motion.Reserve(Capacity);
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
	Moves.Reset();
	Motion.Reset();
	RoundReports.Reset();
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
		Moves.AddUninitialized();
		Motion.AddDefaulted();
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
	Moves[Index] = Params.Motion.IsActive() ? 1 : 0;
	if (Moves[Index])
	{
		Motion[Index] = Params.Motion;
		Motion[Index].Speed = Params.Velocity.Size();
		Motion[Index].Pierce = PierceLeft[Index];
	}
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
		Moves[Index] = Moves[Last];
		if (Moves[Index])
		{
			Motion[Index] = Motion[Last];
		}
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
	Moves.Pop(EAllowShrinking::No);
	Motion.Pop(EAllowShrinking::No);
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

bool FDTBulletPool::FindSeekTarget(const FVector2f& From, float Range, const FDTWorldView& View, int32 NumEnemies, int32& OutEnemy, int32 SkipHitsOf)
{
	OutEnemy = INDEX_NONE;
	if (NumEnemies == 0 || View.EnemyHash.Num() == 0 || Range <= 0.0f)
	{
		return false;
	}
	SeekScratch.Reset();
	View.EnemyHash.QueryCircle(From, Range, SeekScratch);
	float Best = Range * Range;
	for (const int32 Enemy : SeekScratch)
	{
		if (Enemy < 0 || Enemy >= NumEnemies || (SkipHitsOf != INDEX_NONE && HasHit(SkipHitsOf, View.EnemySerials[Enemy])))
		{
			continue;
		}
		const float D = FVector2f::DistSquared(From, View.EnemyPositions[Enemy]);
		if (D < Best || (D == Best && OutEnemy != INDEX_NONE && Enemy < OutEnemy))
		{
			Best = D;
			OutEnemy = Enemy;
		}
	}
	return OutEnemy != INDEX_NONE;
}

void FDTBulletPool::TurnHome(int32 Index, bool bForget)
{
	FDTBulletMotion& M = Motion[Index];
	M.bReturning = true;
	PierceLeft[Index] = M.Pierce;
	if (bForget)
	{
		HitCount[Index] = 0; // the way home strikes what the way out struck
	}
}

void FDTBulletPool::StepMotion(int32 Index, float Dt, const FDTWorldView& View, int32 NumEnemies)
{
	FDTBulletMotion& M = Motion[Index];
	const FVector2f Hero = View.HeroPositions.IsValidIndex(OwnerId[Index]) ? View.HeroPositions[OwnerId[Index]] : FVector2f::ZeroVector;
	if (M.OrbitLeft > 0.0f)
	{
		M.OrbitLeft -= Dt;
		M.OrbitAngleDeg += M.OrbitDegS * Dt;
		float S = 0.0f;
		float C = 1.0f;
		FMath::SinCos(&S, &C, FMath::DegreesToRadians(M.OrbitAngleDeg));
		if (M.OrbitLeft > 0.0f)
		{
			Vel[Index] = (Hero + FVector2f(C, S) * M.OrbitRadius - Pos[Index]) / Dt; // lands exactly on the ring, whatever the hero did
			return;
		}
		// Release: re-aimed once at the boss, else the nearest enemy in reach, else out along the orbit.
		FVector2f Dir(-S, C);
		const auto AimAt = [this, Index, &Dir](const FVector2f& Target)
		{
			const FVector2f To = Target - Pos[Index];
			Dir = To.SizeSquared() > UE_KINDA_SMALL_NUMBER ? To.GetSafeNormal() : Dir;
		};
		int32 Enemy = INDEX_NONE;
		if (View.bBossAlive)
		{
			AimAt(View.BossPosition);
		}
		else if (FindSeekTarget(Pos[Index], M.ReleaseRange, View, NumEnemies, Enemy))
		{
			AimAt(View.EnemyPositions[Enemy]);
		}
		Vel[Index] = Dir * M.Speed;
		return;
	}
	if (M.bBoomerang)
	{
		if (!M.bReturning && LifeLeft[Index] <= M.ReturnAt)
		{
			TurnHome(Index, true);
		}
		if (M.bReturning)
		{
			const FVector2f ToHero = Hero - Pos[Index];
			const float Dist = ToHero.Size();
			if (Dist <= M.Speed * Dt)
			{
				LifeLeft[Index] = 0.0f; // home: an expiry, so end-of-flight modifiers fire at the hero
			}
			else
			{
				Vel[Index] = ToHero / Dist * M.Speed;
			}
			return;
		}
	}
	if (M.CurlDegS != 0.0f)
	{
		Vel[Index] = Vel[Index].GetRotated(M.CurlDegS * Dt);
	}
	if (M.TurnDegS <= 0.0f)
	{
		return;
	}
	int32 Target = INDEX_NONE;
	if (M.TargetSerial != 0)
	{
		if (!bSerialsBuilt)
		{
			bSerialsBuilt = true;
			SerialToEnemy.Reset();
			for (int32 Enemy = 0; Enemy < NumEnemies; ++Enemy)
			{
				SerialToEnemy.Add(View.EnemySerials[Enemy], Enemy);
			}
		}
		const int32* Found = SerialToEnemy.Find(M.TargetSerial);
		Target = Found ? *Found : INDEX_NONE;
	}
	if (Target == INDEX_NONE)
	{
		M.TargetSerial = 0;
		if (!FindSeekTarget(Pos[Index], M.SeekUu, View, NumEnemies, Target, M.bRetarget ? Index : INDEX_NONE))
		{
			return; // nothing near: fly on
		}
		M.TargetSerial = View.EnemySerials[Target];
	}
	const FVector2f To = View.EnemyPositions[Target] - Pos[Index];
	const float Speed = Vel[Index].Size();
	if (To.SizeSquared() <= UE_KINDA_SMALL_NUMBER || Speed <= UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	const float Have = FMath::Atan2(Vel[Index].Y, Vel[Index].X);
	const float Want = FMath::Atan2(To.Y, To.X);
	const float Delta = FMath::FindDeltaAngleRadians(Have, Want);
	const float Limit = FMath::DegreesToRadians(M.TurnDegS) * Dt;
	const float Next = Have + FMath::Clamp(Delta, -Limit, Limit);
	float S = 0.0f;
	float C = 1.0f;
	FMath::SinCos(&S, &C, Next);
	Vel[Index] = FVector2f(C, S) * Speed;
}

void FDTBulletPool::Step(float Dt, FDTWorldView& View)
{
	PrevPos = Pos;
	RoundReports.Reset();
	bSerialsBuilt = false;

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
		if (Moves[Index])
		{
			StepMotion(Index, Dt, View, NumEnemies);
		}
		Pos[Index] += Vel[Index] * Dt;
		const FVector2f P = Pos[Index];
		if (Moves[Index] && Motion[Index].RoundId != 0)
		{
			RoundReports.Add(FDTRoundReport{ Motion[Index].RoundId, P, Vel[Index], LifeLeft[Index], PierceLeft[Index], Motion[Index].bReturning });
		}
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
				Hit.bPierce = (Moves[Index] ? Motion[Index].Pierce : PierceLeft[Index] + HitCount[Index]) > 1;
				++Counters.Hits;
				RememberHit(Index, EnemySerial);
				if (Moves[Index] && Motion[Index].bRetarget)
				{
					Motion[Index].TargetSerial = 0;
				}
				if (--PierceLeft[Index] <= 0)
				{
					if (Moves[Index] && Motion[Index].bBoomerang && !Motion[Index].bReturning)
					{
						TurnHome(Index, false); // the struck enemy stays remembered: it is hit again only by a later pass
						break;
					}
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
