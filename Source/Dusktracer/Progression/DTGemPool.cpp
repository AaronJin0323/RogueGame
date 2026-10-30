#include "Progression/DTGemPool.h"
#include "Core/DTConstantsGen.h"

namespace DTGemPoolLocal
{
	static bool IsPresent(const FDTPickupHeroes& Heroes, int32 Player)
	{
		return Heroes.Positions.IsValidIndex(Player) && (!Heroes.Present.IsValidIndex(Player) || Heroes.Present[Player]);
	}

	static float RadiusOf(const FDTPickupHeroes& Heroes, int32 Player)
	{
		const float Mul = Heroes.RadiusMul.IsValidIndex(Player) ? Heroes.RadiusMul[Player] : 1.0f;
		return static_cast<float>(DTConst::PICKUP_RADIUS_UU) * FMath::Max(0.0f, Mul);
	}
}

FDTGemPool::FDTGemPool()
{
	Pos.Reserve(DTConst::PICKUP_GEM_CAP);
	PrevPos.Reserve(DTConst::PICKUP_GEM_CAP);
	Vel.Reserve(DTConst::PICKUP_GEM_CAP);
	Value.Reserve(DTConst::PICKUP_GEM_CAP);
	BoundTo.Reserve(DTConst::PICKUP_GEM_CAP);
	SpawnStep.Reserve(DTConst::PICKUP_GEM_CAP);
	Collected.Reserve(DTConst::PICKUP_GEM_CAP);
}

void FDTGemPool::Reset()
{
	Pos.Reset();
	PrevPos.Reset();
	Vel.Reset();
	Value.Reset();
	BoundTo.Reset();
	SpawnStep.Reset();
	Collected.Reset();
	ChestPos.Reset();
	TotalSpawned = 0;
	TotalMerged = 0;
	TotalCollected = 0;
	TotalChests = 0;
}

EDTGemSpawnResult FDTGemPool::Spawn(const FDTGemSpawn& Gem, int64 StepIndex)
{
	if (Gem.Value <= 0)
	{
		return EDTGemSpawnResult::Rejected;
	}
	++TotalSpawned;
	if (Pos.Num() >= DTConst::PICKUP_GEM_CAP && Pos.Num() > 0)
	{
		// R-PRG-07: at the cap the value goes to the oldest live gem (index 0: the pool keeps spawn order).
		Value[0] = static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Value[0]) + Gem.Value, MAX_int32));
		++TotalMerged;
		return EDTGemSpawnResult::Merged;
	}
	Pos.Add(Gem.Position);
	PrevPos.Add(Gem.Position);
	Vel.Add(FVector2f::ZeroVector);
	Value.Add(Gem.Value);
	BoundTo.Add(INDEX_NONE);
	SpawnStep.Add(StepIndex);
	Collected.Add(false);
	return EDTGemSpawnResult::Added;
}

int32 FDTGemPool::SpawnChest(const FVector2f& Position)
{
	return ChestPos.Add(Position);
}

int32 FDTGemPool::MagnetAll(int32 OwnerId)
{
	if (OwnerId < 0)
	{
		return 0;
	}
	int32 Bound = 0;
	for (int32 Index = 0; Index < BoundTo.Num(); ++Index)
	{
		if (BoundTo[Index] == INDEX_NONE && !Collected[Index])
		{
			BoundTo[Index] = OwnerId;
			++Bound;
		}
	}
	return Bound;
}

int64 FDTGemPool::GetFloorXP() const
{
	int64 Sum = 0;
	for (const int32 V : Value)
	{
		Sum += V;
	}
	return Sum;
}

void FDTGemPool::BindFree(const FDTPickupHeroes& Heroes)
{
	using namespace DTGemPoolLocal;
	FreePos.Reset();
	FreeIdx.Reset();
	for (int32 Index = 0; Index < Pos.Num(); ++Index)
	{
		if (BoundTo[Index] == INDEX_NONE)
		{
			FreePos.Add(Pos[Index]);
			FreeIdx.Add(Index);
		}
	}
	if (FreeIdx.Num() == 0)
	{
		return;
	}
	FreeHash.Build(FreePos, FreeIdx);
	BestDistSq.Init(TNumericLimits<float>::Max(), Pos.Num());
	BestPlayer.Init(INDEX_NONE, Pos.Num());
	// Players in index order; a strictly nearer player wins, so ties go to the lower index (deterministic).
	for (int32 Player = 0; Player < Heroes.Positions.Num(); ++Player)
	{
		if (!IsPresent(Heroes, Player))
		{
			continue;
		}
		const float Radius = RadiusOf(Heroes, Player);
		const FVector2f Hero = Heroes.Positions[Player];
		Candidates.Reset();
		FreeHash.QueryCircle(Hero, Radius, Candidates);
		for (const int32 Index : Candidates)
		{
			const float DistSq = FVector2f::DistSquared(Pos[Index], Hero);
			if (DistSq <= Radius * Radius && DistSq < BestDistSq[Index])
			{
				BestDistSq[Index] = DistSq;
				BestPlayer[Index] = Player;
			}
		}
	}
	for (const int32 Index : FreeIdx)
	{
		if (BestPlayer[Index] != INDEX_NONE)
		{
			BoundTo[Index] = BestPlayer[Index];
		}
	}
}

void FDTGemPool::Step(float Dt, const FDTPickupHeroes& Heroes, FDTPickupStepResult& Out)
{
	using namespace DTGemPoolLocal;
	PrevPos = Pos;

	// Chests (R-PRG-09): the first present hero in player order within reach collects it, then magnetises every free gem.
	const float ChestReach = static_cast<float>(DTConst::PICKUP_COLLECT_RADIUS_UU + DTConst::HERO_RADIUS_UU);
	for (int32 Chest = 0; Chest < ChestPos.Num();)
	{
		int32 Collector = INDEX_NONE;
		for (int32 Player = 0; Player < Heroes.Positions.Num(); ++Player)
		{
			if (IsPresent(Heroes, Player) && FVector2f::DistSquared(ChestPos[Chest], Heroes.Positions[Player]) <= ChestReach * ChestReach)
			{
				Collector = Player;
				break;
			}
		}
		if (Collector == INDEX_NONE)
		{
			++Chest;
			continue;
		}
		Out.ChestCollectors.Add(Collector);
		++TotalChests;
		ChestPos.RemoveAt(Chest);
		MagnetAll(Collector);
	}

	// R-PRG-08: a bound gem whose player left is freed.
	for (int32 Index = 0; Index < Pos.Num(); ++Index)
	{
		if (BoundTo[Index] != INDEX_NONE && !IsPresent(Heroes, BoundTo[Index]))
		{
			BoundTo[Index] = INDEX_NONE;
			Vel[Index] = FVector2f::ZeroVector;
		}
	}

	BindFree(Heroes);

	// Bound gems accelerate along the line to their player's current position, up to the top speed.
	const float Accel = static_cast<float>(DTConst::PICKUP_MAGNET_ACCEL_UU_S2);
	const float SpeedMax = static_cast<float>(DTConst::PICKUP_MAGNET_SPEED_MAX_UU_S);
	const float Collect = static_cast<float>(DTConst::PICKUP_COLLECT_RADIUS_UU);
	bool bAnyCollected = false;
	for (int32 Index = 0; Index < Pos.Num(); ++Index)
	{
		const int32 Player = BoundTo[Index];
		if (Player == INDEX_NONE)
		{
			continue;
		}
		const FVector2f Hero = Heroes.Positions[Player];
		FVector2f ToHero = Hero - Pos[Index];
		float Dist = ToHero.Size();
		if (Dist > Collect)
		{
			const FVector2f Dir = ToHero / Dist;
			const float Speed = FMath::Min(Vel[Index].Size() + Accel * Dt, SpeedMax);
			const float Travel = Speed * Dt;
			Vel[Index] = Dir * Speed;
			Pos[Index] = Travel >= Dist ? Hero : Pos[Index] + Dir * Travel;
			Dist = FVector2f::Distance(Hero, Pos[Index]);
		}
		if (Dist <= Collect)
		{
			Collected[Index] = true;
			bAnyCollected = true;
			Out.Gems.Add({ Player, Value[Index] });
			++TotalCollected;
		}
	}
	if (bAnyCollected)
	{
		Compact();
	}
}

void FDTGemPool::Compact()
{
	// Stable: index 0 stays the oldest live gem (R-PRG-07 merge target).
	int32 Write = 0;
	for (int32 Read = 0; Read < Pos.Num(); ++Read)
	{
		if (Collected[Read])
		{
			continue;
		}
		if (Write != Read)
		{
			Pos[Write] = Pos[Read];
			PrevPos[Write] = PrevPos[Read];
			Vel[Write] = Vel[Read];
			Value[Write] = Value[Read];
			BoundTo[Write] = BoundTo[Read];
			SpawnStep[Write] = SpawnStep[Read];
			Collected[Write] = false;
		}
		++Write;
	}
	const int32 Removed = Pos.Num() - Write;
	if (Removed > 0)
	{
		Pos.SetNum(Write, EAllowShrinking::No);
		PrevPos.SetNum(Write, EAllowShrinking::No);
		Vel.SetNum(Write, EAllowShrinking::No);
		Value.SetNum(Write, EAllowShrinking::No);
		BoundTo.SetNum(Write, EAllowShrinking::No);
		SpawnStep.SetNum(Write, EAllowShrinking::No);
		Collected.SetNum(Write, EAllowShrinking::No);
	}
}
