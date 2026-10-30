#include "Enemies/DTEnemyPool.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"

namespace DTEnemyPoolImpl
{
	/** Timer comparisons on the fixed step: absorbs float error so a timer of N steps ends on step N. */
	constexpr float TimerEps = 1.0e-4f;

	static FVector2f Rotate(const FVector2f& V, float Radians)
	{
		const float C = FMath::Cos(Radians);
		const float S = FMath::Sin(Radians);
		return FVector2f(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}

	/** Turns From toward To by at most MaxRadians; From is a unit vector. */
	static FVector2f TurnToward(const FVector2f& From, const FVector2f& To, float MaxRadians)
	{
		const FVector2f T = To.GetSafeNormal();
		if (T.IsNearlyZero())
		{
			return From;
		}
		const float Cross = From.X * T.Y - From.Y * T.X;
		const float Dot = From.X * T.X + From.Y * T.Y;
		const float Angle = FMath::Atan2(Cross, Dot);
		if (FMath::Abs(Angle) <= MaxRadians)
		{
			return T;
		}
		return Rotate(From, FMath::Sign(Angle) * MaxRadians).GetSafeNormal();
	}

	/** R-ENM-13: an attack whose tell is shorter than the minimum lead never fires. */
	static bool ShowsFullLead(float TellSeconds)
	{
		return TellSeconds + TimerEps >= DTConst::ENEMIES_TELEGRAPH_LEAD_MIN_S;
	}

	static EDTFirePattern ParsePattern(const FString& Id)
	{
		if (Id == TEXT("Turret.Fan"))
		{
			return EDTFirePattern::Fan;
		}
		if (Id == TEXT("Turret.Burst"))
		{
			return EDTFirePattern::Burst;
		}
		return EDTFirePattern::Count;
	}

	static float DistToSegment(const FVector2f& P, const FVector2f& A, const FVector2f& Dir, float Length)
	{
		const float T = FMath::Clamp(FVector2f::DotProduct(P - A, Dir), 0.0f, Length);
		return FVector2f::Distance(P, A + Dir * T);
	}

	static float AimLineLength() { return static_cast<float>(DTConst::ENEMY_FIRE_SHOT_SPEED_UU_S) * DTConst::ENEMY_FIRE_SHOT_LIFE_S; }
	static float AimLineWidth() { return 2.0f * static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU); }
}

const FDTEnemyRoleDef& FDTEnemyRoleDef::Get(EDTEnemyRole InRole)
{
	static const TArray<FDTEnemyRoleDef> Defs = []()
	{
		TArray<FDTEnemyRoleDef> Out;
		Out.SetNum(static_cast<int32>(EDTEnemyRole::Count));
		for (const DTCat::FEnemyRow& Row : DTCat::Enemies)
		{
			FDTEnemyRoleDef& Def = Out[static_cast<int32>(Row.Role)];
			Def.Radius = Row.Radius_uu;
			Def.Speed = Row.Speed_uu_s;
			Def.MaxHp = Row.Hp;
			Def.ContactDamage = Row.ContactDamage;
			Def.AttackId = FCString::Strlen(Row.Attack) > 0 ? FName(Row.Attack) : NAME_None;
			TArray<FString> Parts;
			FString(Row.FirePattern).ParseIntoArray(Parts, TEXT("|"));
			for (const FString& Part : Parts)
			{
				const EDTFirePattern Pattern = DTEnemyPoolImpl::ParsePattern(Part.TrimStartAndEnd());
				if (Pattern != EDTFirePattern::Count)
				{
					Def.Patterns.Add(Pattern);
				}
			}
		}
		// The Warden hull (boss.md R-BOS-03): one slot of role Boss, moved by script.
		FDTEnemyRoleDef& Boss = Out[static_cast<int32>(EDTEnemyRole::Boss)];
		Boss.Radius = static_cast<float>(DTConst::BOSS_RADIUS_UU);
		Boss.Speed = 0.0f;
		Boss.MaxHp = static_cast<float>(DTConst::BOSS_HP_BASE);
		Boss.ContactDamage = static_cast<float>(DTConst::BOSS_CONTACT_DAMAGE);
		return Out;
	}();
	const int32 Index = static_cast<int32>(InRole);
	return Defs[Defs.IsValidIndex(Index) ? Index : 0];
}

int32 FDTEnemyAux::NumBurnStacks() const
{
	int32 Count = 0;
	for (const FDTBurnStack& Stack : Burn)
	{
		Count += Stack.Left > 0.0f ? 1 : 0;
	}
	return Count;
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
	Facing.Reserve(Capacity);
	Radius.Reserve(Capacity);
	Hp.Reserve(Capacity);
	MaxHp.Reserve(Capacity);
	Speed.Reserve(Capacity);
	ContactDamage.Reserve(Capacity);
	HpScale.Reserve(Capacity);
	DmgScale.Reserve(Capacity);
	Role.Reserve(Capacity);
	Flags.Reserve(Capacity);
	BeatId.Reserve(Capacity);
	Dead.Reserve(Capacity);
	Serial.Reserve(Capacity);
	Aux.Reserve(Capacity);
	PullReadyStep.Reserve(Capacity);
	NextVel.Reserve(Capacity);
}

void FDTEnemyPool::Reset(bool bResetCounters)
{
	Pos.Reset();
	PrevPos.Reset();
	Vel.Reset();
	Facing.Reset();
	Radius.Reset();
	Hp.Reset();
	MaxHp.Reset();
	Speed.Reset();
	ContactDamage.Reset();
	HpScale.Reset();
	DmgScale.Reset();
	Role.Reset();
	Flags.Reset();
	BeatId.Reset();
	Dead.Reset();
	Serial.Reset();
	Aux.Reset();
	PullReadyStep.Reset();
	Orphans.Reset();
	StepThreats.Reset();
	if (bResetCounters)
	{
		Rounds.Reset(true);
		NextSerial = 1;
		NextBurnOrder = 1;
		RefusedSpawns = 0;
		TotalKills = 0;
		KillRate.Reset();
		DamageCarry.Reset();
		Counters = FDTEnemyCounters();
	}
	else
	{
		Rounds.Reset(false); // a clear keeps the counters but no round outlives the enemies that fired it
	}
}

int32 FDTEnemyPool::Spawn(EDTEnemyRole InRole, const FVector2f& Position)
{
	FDTEnemySpawn Spec;
	Spec.Role = InRole;
	Spec.Position = Position;
	return Spawn(Spec, nullptr, false);
}

int32 FDTEnemyPool::Spawn(const FDTEnemySpawn& Spec, FRandomStream* Rng, bool bDirector)
{
	// A Turret draws its pattern even when refused, so the Spawn stream advances the same way whatever the population.
	const FDTEnemyRoleDef& Def = FDTEnemyRoleDef::Get(Spec.Role);
	int32 PatternIndex = 0;
	if (Def.Patterns.Num() > 1 && Rng)
	{
		PatternIndex = Rng->RandHelper(Def.Patterns.Num());
	}
	if (Num() >= Capacity)
	{
		++RefusedSpawns;
		return INDEX_NONE;
	}
	const float Limit = FMath::Max(0.0f, DTConst::ARENA_HALF_EXTENT_UU - Def.Radius);
	const FVector2f Clamped(FMath::Clamp(Spec.Position.X, -Limit, Limit), FMath::Clamp(Spec.Position.Y, -Limit, Limit));
	const float InHpScale = FMath::Max(0.0f, Spec.HpScale);
	const float InDmgScale = FMath::Max(0.0f, Spec.DmgScale);
	const float Life = Def.MaxHp * InHpScale * (Spec.bElite ? static_cast<float>(DTConst::ROLE_ELITE_HP_MULT) : 1.0f);

	Pos.Add(Clamped);
	PrevPos.Add(Clamped);
	Vel.Add(FVector2f::ZeroVector);
	Facing.Add(FVector2f(1.0f, 0.0f));
	Radius.Add(Def.Radius);
	Hp.Add(Life);
	MaxHp.Add(Life);
	Speed.Add(Def.Speed);
	ContactDamage.Add(Def.ContactDamage);
	HpScale.Add(InHpScale);
	DmgScale.Add(InDmgScale);
	Role.Add(Spec.Role);
	Flags.Add((Spec.bElite ? DTEnemyFlags::Elite : 0) | (Spec.bArrival ? DTEnemyFlags::Arrival : 0) | (bDirector ? DTEnemyFlags::Director : 0));
	BeatId.Add(Spec.BeatId);
	Dead.Add(0);
	Serial.Add(NextSerial);
	NextSerial = NextSerial == MAX_uint32 ? 1u : NextSerial + 1u; // 4e9 spawns: wrap past zero, never hand it out

	PullReadyStep.Add(0);

	FDTEnemyAux& State = Aux.AddDefaulted_GetRef();
	State.State = Spec.Role == EDTEnemyRole::Boss ? EDTEnemyState::Scripted : EDTEnemyState::Init;
	if (Def.Patterns.Num() > 0)
	{
		State.Pattern = Def.Patterns[PatternIndex];
	}
	if (Spec.bArrival)
	{
		State.LastFireStep = MIN_int64; // an arrival's attack comes first in the token order (R-ENM-13, R-ENM-18)
	}
	return Pos.Num() - 1;
}

int32 FDTEnemyPool::SpawnRing(EDTEnemyRole InRole, int32 Count, const FVector2f& Center, float RingRadius, FRandomStream& Rng)
{
	FDTEnemySpawn Template;
	Template.Role = InRole;
	return SpawnRing(Template, Count, Center, RingRadius, Rng);
}

int32 FDTEnemyPool::SpawnRing(const FDTEnemySpawn& Template, int32 Count, const FVector2f& Center, float RingRadius, FRandomStream& Rng)
{
	int32 Spawned = 0;
	FDTEnemySpawn Spec = Template;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		// Always draw, even when refused, so the stream advances the same way whatever the population.
		const float Angle = Rng.FRandRange(0.0f, UE_TWO_PI);
		Spec.Position = Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * RingRadius;
		if (Spawn(Spec, &Rng, false) != INDEX_NONE)
		{
			++Spawned;
		}
	}
	return Spawned;
}

int32 FDTEnemyPool::CountRole(EDTEnemyRole InRole) const
{
	int32 Count = 0;
	for (const EDTEnemyRole R : Role)
	{
		Count += R == InRole ? 1 : 0;
	}
	return Count;
}

uint8 FDTEnemyPool::GetStatusMask(int32 Index) const
{
	const FDTEnemyAux& A = Aux[Index];
	uint8 Mask = 0;
	Mask |= A.ShockLeft > 0.0f ? 1 << static_cast<int32>(EDTStatus::Shock) : 0;
	Mask |= A.NumBurnStacks() > 0 ? 1 << static_cast<int32>(EDTStatus::Burn) : 0;
	Mask |= A.ChillStacks > 0 ? 1 << static_cast<int32>(EDTStatus::Chill) : 0;
	Mask |= A.FreezeLeft > 0.0f ? 1 << static_cast<int32>(EDTStatus::Freeze) : 0;
	return Mask;
}

void FDTEnemyPool::SetFacing(int32 Index, const FVector2f& Dir)
{
	const FVector2f N = Dir.GetSafeNormal();
	if (!N.IsNearlyZero())
	{
		Facing[Index] = N;
		Aux[Index].bFacingSet = true;
	}
}

bool FDTEnemyPool::SetScriptedPosition(const FDTEnemyHandle& Handle, const FVector2f& Position)
{
	if (!IsHandleValid(Handle))
	{
		return false;
	}
	const float Limit = FMath::Max(0.0f, DTConst::ARENA_HALF_EXTENT_UU - Radius[Handle.Index]);
	Pos[Handle.Index] = FVector2f(FMath::Clamp(Position.X, -Limit, Limit), FMath::Clamp(Position.Y, -Limit, Limit));
	return true;
}

int32 FDTEnemyPool::NearestHero(const FVector2f& At, TConstArrayView<FVector2f> Heroes, FVector2f* OutDelta) const
{
	int32 Best = INDEX_NONE;
	float BestDistSq = TNumericLimits<float>::Max();
	for (int32 Hero = 0; Hero < Heroes.Num(); ++Hero)
	{
		const float DistSq = FVector2f::DistSquared(Heroes[Hero], At);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Hero;
		}
	}
	if (OutDelta)
	{
		*OutDelta = Best != INDEX_NONE ? Heroes[Best] - At : FVector2f::ZeroVector;
	}
	return Best;
}

float FDTEnemyPool::ChargeLengthFrom(const FVector2f& From, const FVector2f& Dir, float EnemyRadius) const
{
	// The band is the charge: full length, clipped where the arena wall would stop the Rusher.
	float Length = static_cast<float>(DTConst::ROLE_RUSHER_CHARGE_SPEED_UU_S) * DTConst::ROLE_RUSHER_CHARGE_S;
	const float Limit = FMath::Max(0.0f, DTConst::ARENA_HALF_EXTENT_UU - EnemyRadius);
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const float D = Dir[Axis];
		if (FMath::Abs(D) > UE_KINDA_SMALL_NUMBER)
		{
			const float Wall = D > 0.0f ? Limit : -Limit;
			Length = FMath::Min(Length, FMath::Max(0.0f, (Wall - From[Axis]) / D));
		}
	}
	return Length;
}

bool FDTEnemyPool::ApplyStatus(const FDTStatusApply& Apply)
{
	if (!IsHandleValid(Apply.Handle))
	{
		++Counters.StatusesIgnored; // stale handle (R-ENM-21, R-ENM-23)
		return false;
	}
	const int32 Index = Apply.Handle.Index;
	FDTEnemyAux& A = Aux[Index];
	const bool bBoss = Role[Index] == EDTEnemyRole::Boss;
	const float FreezeSeconds = DTConst::STATUS_FREEZE_S * FMath::Max(0.0f, Apply.FreezeScale);

	switch (Apply.Status)
	{
	case EDTStatus::Shock:
		A.ShockLeft = DTConst::STATUS_SHOCK_S; // refresh, never stack
		A.ShockOwner = Apply.OwnerId;
		A.ShockSource = Apply.SourceId;
		break;

	case EDTStatus::Burn:
	{
		const int32 Cap = FMath::Clamp(DTConst::STATUS_BURN_STACKS_MAX + FMath::Max(0, Apply.StackBonus), 1, DTConst::STATUS_BURN_STACK_SLOTS);
		A.BurnCap = Cap;
		int32 Live = 0;
		int32 Empty = INDEX_NONE;
		int32 Oldest = INDEX_NONE;
		for (int32 Slot = 0; Slot < DTConst::STATUS_BURN_STACK_SLOTS; ++Slot)
		{
			if (A.Burn[Slot].Left > 0.0f)
			{
				++Live;
				if (Oldest == INDEX_NONE || A.Burn[Slot].Order < A.Burn[Oldest].Order)
				{
					Oldest = Slot;
				}
			}
			else if (Empty == INDEX_NONE)
			{
				Empty = Slot;
			}
		}
		const int32 Slot = Live < Cap && Empty != INDEX_NONE ? Empty : Oldest; // past the cap: replace the oldest
		FDTBurnStack& Stack = A.Burn[Slot];
		Stack.Left = DTConst::STATUS_BURN_S;
		Stack.Dps = FMath::Max(0.0f, Apply.Magnitude);
		Stack.Accum = 0.0f;
		Stack.OwnerId = Apply.OwnerId;
		Stack.SourceId = Apply.SourceId;
		Stack.Order = NextBurnOrder++;
		break;
	}

	case EDTStatus::Chill:
	{
		if (A.IsFrozen())
		{
			++Counters.StatusesIgnored; // Chill is refused while frozen
			return false;
		}
		const int32 Stacks = FMath::RoundToInt32(Apply.Magnitude);
		if (Stacks <= 0)
		{
			++Counters.StatusesIgnored;
			return false;
		}
		A.ChillStacks += Stacks;
		A.ChillLeft = DTConst::STATUS_CHILL_S;
		A.ChillOwner = Apply.OwnerId;
		A.ChillSource = Apply.SourceId;
		if (bBoss)
		{
			A.ChillStacks = FMath::Min(A.ChillStacks, DTConst::STATUS_BOSS_CHILL_STACK_MAX); // boss.md R-BOS-11: never freezes
			break;
		}
		const int32 Threshold = Apply.FreezeAt > 0 ? Apply.FreezeAt : DTConst::STATUS_FREEZE_STACKS;
		if (A.ChillStacks >= Threshold)
		{
			A.ChillStacks = 0;
			A.ChillLeft = 0.0f;
			A.FreezeLeft = FreezeSeconds;
			A.FreezeOwner = Apply.OwnerId;
			A.FreezeSource = Apply.SourceId;
			++Counters.StatusesApplied[static_cast<int32>(EDTStatus::Freeze)];
		}
		break;
	}

	case EDTStatus::Freeze:
		if (bBoss)
		{
			++Counters.StatusesIgnored; // boss.md R-BOS-11
			return false;
		}
		A.ChillStacks = 0;
		A.ChillLeft = 0.0f;
		A.FreezeLeft = FMath::Max(A.FreezeLeft, FreezeSeconds);
		A.FreezeOwner = Apply.OwnerId;
		A.FreezeSource = Apply.SourceId;
		break;

	default:
		++Counters.StatusesIgnored;
		return false;
	}
	++Counters.StatusesApplied[static_cast<int32>(Apply.Status)];
	return true;
}

bool FDTEnemyPool::TryPull(const FDTEnemyHandle& Handle, int64 StepIndex, int64 IntervalSteps)
{
	if (!IsHandleValid(Handle) || StepIndex < PullReadyStep[Handle.Index])
	{
		return false;
	}
	PullReadyStep[Handle.Index] = StepIndex + IntervalSteps;
	return true;
}

void FDTEnemyPool::ApplyDisplacements(TArray<FDTDisplacement>& Displacements)
{
	const float HalfExtent = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU);
	for (const FDTDisplacement& D : Displacements)
	{
		const int32 Index = D.EnemyIndex;
		if (!Pos.IsValidIndex(Index) || (D.EnemySerial != 0 && Serial[Index] != D.EnemySerial) || Role[Index] == EDTEnemyRole::Boss)
		{
			++Counters.DisplacementsIgnored; // stale serial; the boss hull ignores every displacement (boss.md R-BOS-03)
			continue;
		}
		const float Limit = FMath::Max(0.0f, HalfExtent - Radius[Index]);
		const FVector2f Next = Pos[Index] + D.Delta;
		Pos[Index] = FVector2f(FMath::Clamp(Next.X, -Limit, Limit), FMath::Clamp(Next.Y, -Limit, Limit));
	}
	Displacements.Reset();
}

void FDTEnemyPool::TickStatuses(float Dt, TArray<FDTHit>* OutHits)
{
	using namespace DTEnemyPoolImpl;
	const float Tick = DTConst::STATUS_BURN_TICK_S;
	for (int32 Index = 0; Index < Num(); ++Index)
	{
		FDTEnemyAux& A = Aux[Index];
		// A status of N steps ends on its Nth step (float error absorbed).
		A.ShockLeft = A.ShockLeft - Dt > TimerEps ? A.ShockLeft - Dt : 0.0f;
		A.FreezeLeft = A.FreezeLeft - Dt > TimerEps ? A.FreezeLeft - Dt : 0.0f;
		if (A.ChillStacks > 0)
		{
			A.ChillLeft -= Dt;
			if (A.ChillLeft <= TimerEps)
			{
				A.ChillStacks = 0;
				A.ChillLeft = 0.0f;
			}
		}
		for (FDTBurnStack& Stack : A.Burn)
		{
			if (Stack.Left <= 0.0f)
			{
				continue;
			}
			const float Burnt = FMath::Min(Dt, Stack.Left);
			Stack.Left -= Burnt;
			Stack.Accum += Burnt;
			const int32 Owner = Stack.OwnerId;
			const uint16 Source = Stack.SourceId;
			float Damage = 0.0f;
			if (Stack.Accum >= Tick - TimerEps)
			{
				Damage += Stack.Dps * Tick;
				Stack.Accum = FMath::Max(0.0f, Stack.Accum - Tick);
			}
			if (Stack.Left <= TimerEps)
			{
				// Flush the last partial tick, so a full stack deals exactly its damage per second times its duration.
				Damage += Stack.Dps * Stack.Accum;
				Stack = FDTBurnStack();
			}
			if (Damage > 0.0f && OutHits)
			{
				FDTHit& Hit = OutHits->AddDefaulted_GetRef();
				Hit.EnemyIndex = Index;
				Hit.EnemySerial = Serial[Index];
				Hit.OwnerId = Owner; // the burner (R-ENM-22)
				Hit.Damage = Damage;
				Hit.Position = Pos[Index];
				Hit.SourceId = Source;
			}
		}
	}
}

void FDTEnemyPool::ResolveInit(TConstArrayView<FVector2f> Heroes)
{
	using namespace DTEnemyPoolImpl;
	for (int32 Index = 0; Index < Num(); ++Index)
	{
		FDTEnemyAux& A = Aux[Index];
		if (A.State != EDTEnemyState::Init)
		{
			continue;
		}
		FVector2f ToHero;
		const int32 Target = NearestHero(Pos[Index], Heroes, &ToHero);
		A.TargetPlayer = Target;
		if (!A.bFacingSet && !ToHero.IsNearlyZero())
		{
			Facing[Index] = ToHero.GetSafeNormal();
			A.bFacingSet = true;
		}
		const bool bArrival = (Flags[Index] & DTEnemyFlags::Arrival) != 0;
		switch (Role[Index])
		{
		case EDTEnemyRole::Rusher:
			A.State = EDTEnemyState::Approach;
			// Drop-in: its wind-up starts on this first step, aimed at its nearest hero (R-ENM-13).
			A.bArrivalAttack = bArrival;
			break;
		case EDTEnemyRole::Turret:
			A.State = EDTEnemyState::Idle;
			break;
		case EDTEnemyRole::Boss:
			A.State = EDTEnemyState::Scripted;
			break;
		default:
			A.State = EDTEnemyState::Approach;
			break;
		}
	}
}

void FDTEnemyPool::AssignTokens(TConstArrayView<FVector2f> Heroes, bool bThreatBlock)
{
	using namespace DTEnemyPoolImpl;
	const int32 NumHeroes = Heroes.Num();
	if (NumHeroes == 0)
	{
		return;
	}
	const int32 Limit = bThreatBlock ? DTConst::AIM_TOKENS_THREAT : DTConst::AIM_TOKENS;
	TArray<int32, TInlineAllocator<8>> Held;
	Held.Init(0, NumHeroes);
	TokenScratch.Reset();
	const float RangeSq = FMath::Square(static_cast<float>(DTConst::ROLE_TURRET_FIRE_RANGE_UU));
	for (int32 Index = 0; Index < Num(); ++Index)
	{
		if (Role[Index] != EDTEnemyRole::Turret)
		{
			continue;
		}
		FDTEnemyAux& A = Aux[Index];
		if (A.bToken)
		{
			if (Held.IsValidIndex(A.TargetPlayer))
			{
				++Held[A.TargetPlayer];
			}
			continue;
		}
		FVector2f ToHero;
		const int32 Target = NearestHero(Pos[Index], Heroes, &ToHero);
		const bool bFirstArrival = A.LastFireStep == MIN_int64;
		const bool bReady = bFireEnabled && A.State == EDTEnemyState::Idle && !A.IsFrozen() && A.Cooldown <= TimerEps
			&& Target != INDEX_NONE && (bFirstArrival || ToHero.SizeSquared() <= RangeSq) && ShowsFullLead(DTConst::ROLE_TURRET_TELL_S);
		if (bReady)
		{
			A.TargetPlayer = Target;
			TokenScratch.Add(Index);
		}
	}
	// Free tokens go to the ready units that fired least recently; ties by serial (R-ENM-18, R-ENM-23).
	TokenScratch.Sort([this](int32 L, int32 R)
	{
		const int64 FL = Aux[L].LastFireStep;
		const int64 FR = Aux[R].LastFireStep;
		return FL != FR ? FL < FR : Serial[L] < Serial[R];
	});
	for (const int32 Index : TokenScratch)
	{
		FDTEnemyAux& A = Aux[Index];
		if (Held[A.TargetPlayer] < Limit)
		{
			A.bToken = true;
			++Held[A.TargetPlayer];
		}
		else
		{
			++Counters.TokenWaits; // holds before its tell
		}
	}
}

void FDTEnemyPool::FanDirections(const FVector2f& Aim, int32 Count, float SpreadDeg, int32 VolleyIndex, TArray<FVector2f>& Out)
{
	Out.Reset();
	const int32 N = FMath::Max(1, Count);
	const float Step = FMath::DegreesToRadians(SpreadDeg);
	const float Half = 0.5f * static_cast<float>(N - 1);
	// Even count: shift half a step so one round flies exactly along the aim, alternating sides volley by volley.
	const float Shift = N % 2 == 0 ? (VolleyIndex % 2 == 0 ? 0.5f : -0.5f) : 0.0f;
	for (int32 Round = 0; Round < N; ++Round)
	{
		const float Offset = static_cast<float>(Round) - Half + Shift;
		Out.Add(Offset == 0.0f ? Aim : DTEnemyPoolImpl::Rotate(Aim, Offset * Step));
	}
}

void FDTEnemyPool::VolleyDirections(const FVector2f& Aim, EDTFirePattern InPattern, bool bCloseFan, int32 VolleyIndex, TArray<FVector2f>& Out)
{
	if (InPattern == EDTFirePattern::Fan && !bCloseFan)
	{
		FanDirections(Aim, DTConst::ENEMY_FIRE_FAN_COUNT, static_cast<float>(DTConst::ENEMY_FIRE_FAN_SPREAD_DEG), VolleyIndex, Out);
		return;
	}
	Out.Reset();
	Out.Add(Aim); // a burst, or a fan inside C.AIM.FAN_MIN_RANGE_UU (R-ENM-19): one aimed round
}

void FDTEnemyPool::FireVolley(const FVector2f& From, const FVector2f& Aim, EDTFirePattern InPattern, bool bClose, int32 Volley,
	int32 Target, float InDmgScale, uint32 InSerial)
{
	VolleyDirections(Aim, InPattern, bClose, Volley, DirScratch);
	const float ShotSpeed = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_SPEED_UU_S);
	FDTEnemyRoundSpawn Round;
	Round.Position = From;
	Round.Radius = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
	Round.Damage = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_DAMAGE) * InDmgScale;
	Round.Life = DTConst::ENEMY_FIRE_SHOT_LIFE_S;
	Round.TargetPlayer = Target;
	Round.SourceRole = EDTEnemyRole::Turret;
	Round.AttackId = FDTEnemyRoleDef::Get(EDTEnemyRole::Turret).AttackId;
	Round.EnemySerial = InSerial;
	for (const FVector2f& Dir : DirScratch)
	{
		Round.Velocity = Dir * ShotSpeed;
		Rounds.Spawn(Round);
	}
}

void FDTEnemyPool::StepRoles(float Dt, int64 StepIndex, TConstArrayView<FVector2f> Heroes)
{
	using namespace DTEnemyPoolImpl;
	const int32 Count = Num();
	if (Count == 0 || Dt <= 0.0f)
	{
		return;
	}

	const float SepRadius = static_cast<float>(DTConst::ENM_SEPARATION_RADIUS_UU);
	const float SepStrength = static_cast<float>(DTConst::ENM_SEPARATION_STRENGTH);
	const int32 MaxNeighbours = DTConst::ENM_MAX_NEIGHBOURS;
	const float MaxDeltaV = static_cast<float>(DTConst::ENM_STEER_ACCEL_UU_S2) * Dt;
	const float TankTurn = FMath::DegreesToRadians(static_cast<float>(DTConst::ROLE_TANK_TURN_DEG_S)) * Dt;
	const float HeadTurn = FMath::DegreesToRadians(static_cast<float>(DTConst::ROLE_TURRET_HEAD_TURN_DEG_S)) * Dt;
	const float TriggerSq = FMath::Square(static_cast<float>(DTConst::ROLE_RUSHER_TRIGGER_UU));
	const float ChargeStep = static_cast<float>(DTConst::ROLE_RUSHER_CHARGE_SPEED_UU_S) * Dt;
	const float FanMinSq = FMath::Square(static_cast<float>(DTConst::AIM_FAN_MIN_RANGE_UU));

	// Neighbours come from positions at the start of the step, so the result does not depend on update order.
	NeighbourHash.Build(Pos);
	NextVel.SetNumUninitialized(Count, EAllowShrinking::No);

	auto Separation = [&](int32 Index) -> FVector2f
	{
		const FVector2f Here = Pos[Index];
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
		return Push;
	};

	auto Steer = [&](int32 Index, const FVector2f& Heading, float Cap) -> FVector2f
	{
		FVector2f Desired = Heading + Separation(Index) * SepStrength;
		Desired = Desired.GetClampedToMaxSize(1.0f) * Cap;
		const FVector2f DeltaV = (Desired - Vel[Index]).GetClampedToMaxSize(MaxDeltaV);
		return (Vel[Index] + DeltaV).GetClampedToMaxSize(Cap);
	};

	for (int32 Index = 0; Index < Count; ++Index)
	{
		FDTEnemyAux& A = Aux[Index];
		NextVel[Index] = FVector2f::ZeroVector;
		if (A.IsFrozen())
		{
			continue; // neither moves, turns, nor advances its behaviour timers (R-ENM-22)
		}
		const float Slow = FMath::Max(0.0f, 1.0f - A.ChillStacks * DTConst::STATUS_CHILL_SLOW_PER_STACK);
		const float Cap = Speed[Index] * Slow;
		FVector2f ToHero;
		const int32 Nearest = NearestHero(Pos[Index], Heroes, &ToHero);
		const FVector2f Seek = ToHero.GetSafeNormal();

		switch (Role[Index])
		{
		case EDTEnemyRole::Tank:
			// Moves along its facing, which turns toward the target at a limited rate (R-ENM-10).
			if (!Seek.IsNearlyZero())
			{
				Facing[Index] = TurnToward(Facing[Index], Seek, TankTurn);
			}
			NextVel[Index] = Steer(Index, Facing[Index], Cap);
			break;

		case EDTEnemyRole::Rusher:
			switch (A.State)
			{
			case EDTEnemyState::WindUp:
				A.StateLeft -= Dt;
				if (A.StateLeft > TimerEps)
				{
					break; // standing still, direction locked, band on the ground
				}
				A.State = EDTEnemyState::Charge;
				A.ChargeLeft = A.ChargeLength;
				CountThreat(EDTEnemyRole::Rusher, Pos[Index]);
				// The charge starts this step.
				[[fallthrough]];
			case EDTEnemyState::Charge:
			{
				const float Travel = FMath::Min(ChargeStep * Slow, A.ChargeLeft);
				NextVel[Index] = A.AimDir * (Travel / Dt);
				A.ChargeLeft -= Travel;
				if (A.ChargeLeft <= TimerEps)
				{
					A.State = EDTEnemyState::Recover;
					A.StateLeft = DTConst::ROLE_RUSHER_RECOVER_S;
				}
				break;
			}
			case EDTEnemyState::Recover:
				A.StateLeft -= Dt;
				if (A.StateLeft <= TimerEps)
				{
					A.State = EDTEnemyState::Approach;
					A.Cooldown = DTConst::ROLE_RUSHER_COOLDOWN_S;
				}
				break;
			default:
				A.Cooldown = FMath::Max(0.0f, A.Cooldown - Dt);
				if (Nearest != INDEX_NONE && (A.bArrivalAttack || (ToHero.SizeSquared() <= TriggerSq && A.Cooldown <= TimerEps)))
				{
					A.bArrivalAttack = false;
					if (ShowsFullLead(DTConst::ROLE_RUSHER_WINDUP_S))
					{
						A.State = EDTEnemyState::WindUp;
						A.StateLeft = DTConst::ROLE_RUSHER_WINDUP_S;
						A.AimDir = Seek.IsNearlyZero() ? Facing[Index] : Seek;
						A.ChargeOrigin = Pos[Index];
						A.ChargeLength = ChargeLengthFrom(Pos[Index], A.AimDir, Radius[Index]);
						Facing[Index] = A.AimDir;
						break;
					}
					++Counters.AttacksSkippedForLead;
				}
				A.State = EDTEnemyState::Approach;
				NextVel[Index] = Steer(Index, Seek, Cap);
				break;
			}
			if (A.State == EDTEnemyState::WindUp || A.State == EDTEnemyState::Charge)
			{
				Facing[Index] = A.AimDir;
			}
			else if (!NextVel[Index].IsNearlyZero())
			{
				Facing[Index] = NextVel[Index].GetSafeNormal();
			}
			break;

		case EDTEnemyRole::Turret:
		{
			// Static; the head turns toward its target (the locked player once its tell starts) (R-ENM-12).
			A.Cooldown = FMath::Max(0.0f, A.Cooldown - Dt);
			const bool bLocked = A.State == EDTEnemyState::Tell || A.State == EDTEnemyState::Fire;
			const FVector2f TargetDelta = bLocked && Heroes.IsValidIndex(A.TargetPlayer) ? Heroes[A.TargetPlayer] - Pos[Index] : ToHero;
			if (!TargetDelta.IsNearlyZero())
			{
				Facing[Index] = TurnToward(Facing[Index], TargetDelta, HeadTurn);
			}
			if (A.State == EDTEnemyState::Idle)
			{
				if (A.bToken)
				{
					A.State = EDTEnemyState::Tell;
					A.StateLeft = DTConst::ROLE_TURRET_TELL_S;
					A.Cooldown = DTConst::ROLE_TURRET_PERIOD_S; // the next tell starts no sooner than the period
					A.bCloseFan = TargetDelta.SizeSquared() < FanMinSq;
				}
				break;
			}
			if (A.State == EDTEnemyState::Tell)
			{
				// R-ENM-19 is judged on the distance to the locked target; the telegraph shows the same footprint.
				A.bCloseFan = TargetDelta.SizeSquared() < FanMinSq;
				A.StateLeft -= Dt;
				if (A.StateLeft > TimerEps)
				{
					break;
				}
				// The aim locks at the end of the tell; the pattern fires along it.
				A.AimDir = Facing[Index];
				A.State = EDTEnemyState::Fire;
				A.StateLeft = 0.0f;
				A.VolleysLeft = DTConst::ENEMY_FIRE_BURST_SHOTS;
				A.VolleyIndex = 0;
				A.LastFireStep = StepIndex;
				CountThreat(EDTEnemyRole::Turret, Pos[Index]);
			}
			else
			{
				A.StateLeft -= Dt;
			}
			while (A.State == EDTEnemyState::Fire && A.StateLeft <= TimerEps && A.VolleysLeft > 0)
			{
				FireVolley(Pos[Index], A.AimDir, A.Pattern, A.bCloseFan, A.VolleyIndex, A.TargetPlayer, DmgScale[Index], Serial[Index]);
				++A.VolleyIndex;
				--A.VolleysLeft;
				A.StateLeft += DTConst::ENEMY_FIRE_BURST_GAP_S;
			}
			if (A.State == EDTEnemyState::Fire && A.VolleysLeft <= 0)
			{
				A.State = EDTEnemyState::Idle;
				A.bToken = false;
				A.LastFireStep = StepIndex;
			}
			break;
		}

		case EDTEnemyRole::Boss:
			break; // moved by script (boss.md R-BOS-03)

		default: // Swarm
			NextVel[Index] = Steer(Index, Seek, Cap);
			if (!NextVel[Index].IsNearlyZero())
			{
				Facing[Index] = NextVel[Index].GetSafeNormal();
			}
			break;
		}
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

void FDTEnemyPool::SpawnEliteChildren(FRandomStream* Rng)
{
}

void FDTEnemyPool::StepElites(float Dt, FDTWorldView& View)
{
}

void FDTEnemyPool::StepBlasts(float Dt, FDTWorldView& View)
{
}

void FDTEnemyPool::CountThreat(EDTEnemyRole InRole, const FVector2f& Origin)
{
	++Counters.ThreatActions[static_cast<int32>(InRole)];
	FDTThreatAction& Action = StepThreats.AddDefaulted_GetRef();
	Action.Origin = Origin;
	Action.Role = InRole;
	Action.AttackId = FDTEnemyRoleDef::Get(InRole).AttackId;
}

void FDTEnemyPool::StepOrphans(float Dt, FDTWorldView& View)
{
	using namespace DTEnemyPoolImpl;
	const float HeroRadius = static_cast<float>(DTConst::HERO_RADIUS_UU);
	for (int32 Index = Orphans.Num() - 1; Index >= 0; --Index)
	{
		FDTOrphanAttack& O = Orphans[Index];
		bool bDone = false;
		if (!O.bLanded)
		{
			O.Left -= Dt;
			if (O.Left > TimerEps)
			{
				continue;
			}
			O.bLanded = true;
			if (O.Role == EDTEnemyRole::Rusher)
			{
				// The band resolves as a hazard at the step the charge would have started (R-ENM-13).
				for (int32 Hero = 0; Hero < View.HeroPositions.Num(); ++Hero)
				{
					if (DistToSegment(View.HeroPositions[Hero], O.Origin, O.Dir, O.Length) <= 0.5f * O.Width + HeroRadius)
					{
						FDTHeroHit& Hit = View.HeroHits.AddDefaulted_GetRef();
						Hit.PlayerId = Hero;
						Hit.Damage = O.Damage;
						Hit.Kind = EDTHeroHitKind::Charge;
						Hit.SourceRole = EDTEnemyRole::Rusher;
						Hit.AttackId = FDTEnemyRoleDef::Get(EDTEnemyRole::Rusher).AttackId;
						Hit.EnemySerial = O.EnemySerial;
						Hit.Position = View.HeroPositions[Hero];
					}
				}
				CountThreat(EDTEnemyRole::Rusher, O.Origin);
				bDone = true;
			}
			else
			{
				O.GapLeft = 0.0f; // fires the pattern its telegraph showed
				CountThreat(EDTEnemyRole::Turret, O.Origin);
			}
		}
		else
		{
			O.GapLeft -= Dt;
		}
		while (!bDone && O.Role != EDTEnemyRole::Rusher && O.GapLeft <= TimerEps && O.VolleysLeft > 0)
		{
			FireVolley(O.Origin, O.Dir, O.Pattern, O.bCloseFan, O.VolleyIndex, O.TargetPlayer, O.DmgScale, O.EnemySerial);
			++O.VolleyIndex;
			--O.VolleysLeft;
			O.GapLeft += DTConst::ENEMY_FIRE_BURST_GAP_S;
		}
		if (bDone || (O.Role != EDTEnemyRole::Rusher && O.VolleysLeft <= 0))
		{
			Orphans.RemoveAt(Index, 1, EAllowShrinking::No);
		}
	}
}

void FDTEnemyPool::PublishTelegraphs(TArray<FDTTelegraph>& Out) const
{
	using namespace DTEnemyPoolImpl;
	Out.Reset();
	TArray<FVector2f> Dirs;
	TArray<FVector2f> Volley;
	auto AddAimLines = [&](const FVector2f& Origin, const FVector2f& Aim, EDTFirePattern InPattern, bool bClose, float Progress)
	{
		// The footprint is every line a round of this burst flies along: both sides of an even fan.
		Dirs.Reset();
		const int32 Distinct = InPattern == EDTFirePattern::Fan && !bClose ? FMath::Min(2, DTConst::ENEMY_FIRE_BURST_SHOTS) : 1;
		for (int32 V = 0; V < Distinct; ++V)
		{
			VolleyDirections(Aim, InPattern, bClose, V, Volley);
			for (const FVector2f& D : Volley)
			{
				if (!Dirs.ContainsByPredicate([&D](const FVector2f& E) { return FVector2f::DotProduct(D, E) > 1.0f - UE_KINDA_SMALL_NUMBER; }))
				{
					Dirs.Add(D);
				}
			}
		}
		for (const FVector2f& D : Dirs)
		{
			FDTTelegraph& T = Out.AddDefaulted_GetRef();
			T.Shape = EDTTelegraphShape::AimLine;
			T.Origin = Origin;
			T.Dir = D;
			T.Length = AimLineLength();
			T.Width = AimLineWidth();
			T.Progress = Progress;
		}
	};
	for (int32 Index = 0; Index < Num(); ++Index)
	{
		const FDTEnemyAux& A = Aux[Index];
		if (Role[Index] == EDTEnemyRole::Rusher && A.State == EDTEnemyState::WindUp)
		{
			FDTTelegraph& T = Out.AddDefaulted_GetRef();
			T.Shape = EDTTelegraphShape::Band;
			T.Origin = A.ChargeOrigin;
			T.Dir = A.AimDir;
			T.Length = A.ChargeLength;
			T.Width = 2.0f * Radius[Index]; // the Rusher's collision width
			T.Progress = FMath::Clamp(1.0f - A.StateLeft / DTConst::ROLE_RUSHER_WINDUP_S, 0.0f, 1.0f);
		}
		else if (Role[Index] == EDTEnemyRole::Turret && A.State == EDTEnemyState::Tell)
		{
			AddAimLines(Pos[Index], Facing[Index], A.Pattern, A.bCloseFan,
				FMath::Clamp(1.0f - A.StateLeft / DTConst::ROLE_TURRET_TELL_S, 0.0f, 1.0f));
		}
	}
	for (const FDTOrphanAttack& O : Orphans)
	{
		if (O.bLanded)
		{
			continue;
		}
		const float Progress = O.Total > 0.0f ? FMath::Clamp(1.0f - O.Left / O.Total, 0.0f, 1.0f) : 1.0f;
		if (O.Role == EDTEnemyRole::Rusher)
		{
			FDTTelegraph& T = Out.AddDefaulted_GetRef();
			T.Shape = EDTTelegraphShape::Band;
			T.Origin = O.Origin;
			T.Dir = O.Dir;
			T.Length = O.Length;
			T.Width = O.Width;
			T.Progress = Progress;
		}
		else
		{
			AddAimLines(O.Origin, O.Dir, O.Pattern, O.bCloseFan, Progress);
		}
	}
}

void FDTEnemyPool::AppendContacts(FDTWorldView& View) const
{
	const float HeroRadius = static_cast<float>(DTConst::HERO_RADIUS_UU);
	const int32 NumHeroes = View.HeroPositions.Num();
	for (int32 Index = 0; Index < Num(); ++Index)
	{
		const float Reach = Radius[Index] + HeroRadius;
		for (int32 Hero = 0; Hero < NumHeroes; ++Hero)
		{
			if (FVector2f::DistSquared(View.HeroPositions[Hero], Pos[Index]) >= Reach * Reach)
			{
				continue;
			}
			FDTHeroHit& Hit = View.HeroHits.AddDefaulted_GetRef();
			Hit.PlayerId = Hero;
			Hit.Damage = ContactDamage[Index] * DmgScale[Index];
			Hit.Kind = EDTHeroHitKind::Contact;
			Hit.SourceRole = Role[Index];
			Hit.EnemySerial = Serial[Index];
			Hit.Position = Pos[Index];
		}
	}
}

void FDTEnemyPool::StepEnemies(const FDTSimContext& Context)
{
	check(Context.View);
	FDTWorldView& View = *Context.View;
	const float Dt = Context.StepSeconds;
	PrevPos = Pos;

	// The hero read last step's hits in the Hero phase; the list holds only this step's from here (bounded with no hero).
	View.HeroHits.Reset();
	StepThreats.Reset();

	for (const FDTStatusApply& Apply : View.StatusApplies)
	{
		ApplyStatus(Apply);
	}
	View.StatusApplies.Reset();
	ApplyDisplacements(View.Displacements);
	TickStatuses(Dt, &View.Hits);

	ResolveInit(View.HeroPositions);
	AssignTokens(View.HeroPositions, View.bThreatBlock);
	StepRoles(Dt, Context.StepIndex, View.HeroPositions);
	StepOrphans(Dt, View);

	Publish(View);
	PublishTelegraphs(View.Telegraphs);
	Rounds.Step(Dt, View);
	AppendContacts(View);
}

void FDTEnemyPool::Move(float Dt, TConstArrayView<FVector2f> Heroes)
{
	PrevPos = Pos;
	StepThreats.Reset();
	ResolveInit(Heroes);
	StepRoles(Dt, 0, Heroes);
}

void FDTEnemyPool::Publish(FDTWorldView& View) const
{
	View.EnemyPositions = Pos;
	View.EnemyRadii = Radius;
	View.EnemySerials = Serial;
	View.EnemyHash.Build(Pos);
}

int32 FDTEnemyPool::ApplyHits(TConstArrayView<FDTHit> Hits, FDTRunState& Run, TArray<FDTKill>* OutKills)
{
	const float FrontCos = FMath::Cos(FMath::DegreesToRadians(static_cast<float>(DTConst::ROLE_TANK_FRONT_HALF_ARC_DEG)));
	int32 Kills = 0;
	for (const FDTHit& Hit : Hits)
	{
		if (!Pos.IsValidIndex(Hit.EnemyIndex) || Dead[Hit.EnemyIndex])
		{
			continue; // already killed by an earlier hit this step: one death, one Kill event
		}
		if (Hit.EnemySerial != 0 && Serial[Hit.EnemyIndex] != Hit.EnemySerial)
		{
			++Counters.StaleHitsIgnored;
			continue; // stale handle: the slot now holds another enemy (R-ENM-23)
		}
		const int32 Index = Hit.EnemyIndex;
		float Damage = FMath::Max(0.0f, Hit.Damage);
		if (Role[Index] == EDTEnemyRole::Tank && !Hit.bPierce)
		{
			// The plate: a contact point inside the front arc deals reduced damage; a piercing round ignores it (R-ENM-10).
			const FVector2f Rel = Hit.Position - Pos[Index];
			if (Rel.SizeSquared() > UE_KINDA_SMALL_NUMBER && FVector2f::DotProduct(Rel.GetSafeNormal(), Facing[Index]) >= FrontCos)
			{
				Damage *= DTConst::ROLE_TANK_FRONT_DAMAGE_MULT;
			}
		}
		if (Aux[Index].ShockLeft > 0.0f)
		{
			Damage *= 1.0f + DTConst::STATUS_SHOCK_DAMAGE_TAKEN; // R-ENM-22
		}
		const float Dealt = FMath::Min(Damage, FMath::Max(0.0f, Hp[Index]));
		Hp[Index] -= Damage;
		// The event holds whole points: the rest carries to this owner and source's next hit, so a field's ticks add up (R-TEL-05).
		float& Carry = DamageCarry.FindOrAdd((static_cast<uint32>(Hit.OwnerId) << 16) | Hit.SourceId);
		Carry += Dealt;
		const int32 Whole = FMath::RoundToInt32(Carry);
		Carry -= static_cast<float>(Whole);
		Run.ApplyEvent({ EDTGameEventKind::Hit, Hit.OwnerId, Whole });
		if (Hp[Index] <= 0.0f)
		{
			Dead[Index] = 1;
			++Kills;
			Run.ApplyEvent({ EDTGameEventKind::Kill, Hit.OwnerId, 0 });
			FDTEnemyDeath& Death = StepDeaths.AddDefaulted_GetRef();
			Death.Serial = Serial[Index];
			Death.StatusMask = GetStatusMask(Index);
			for (const FDTBurnStack& Stack : Aux[Index].Burn)
			{
				Death.BurnDps = Stack.Left > 0.0f ? FMath::Max(Death.BurnDps, Stack.Dps) : Death.BurnDps;
			}
			if (OutKills)
			{
				FDTKill& Kill = OutKills->AddDefaulted_GetRef();
				Kill.EnemySerial = Serial[Index];
				Kill.Role = Role[Index];
				Kill.Position = Pos[Index];
				Kill.OwnerId = Hit.OwnerId;
				Kill.SourceId = Hit.SourceId;
				Kill.WeaponSlot = Hit.WeaponSlot;
				Kill.bElite = (Flags[Index] & DTEnemyFlags::Elite) != 0;
				Kill.bDirector = (Flags[Index] & DTEnemyFlags::Director) != 0;
				Kill.BeatId = BeatId[Index];
			}
		}
	}
	TotalKills += Kills;
	return Kills;
}

int32 FDTEnemyPool::ResolveHits(const FDTSimContext& Context)
{
	check(Context.View && Context.Run);
	FDTWorldView& View = *Context.View;
	View.Kills.Reset();
	StepDeaths.Reset();
	const int32 Kills = ApplyHits(View.Hits, *Context.Run, &View.Kills);
	View.Hits.Reset();
	Compact();
	Publish(View); // indices changed: republish so Effects reads valid indices (R-ENM-07)
	KillRate.RecordStep(Kills, Context.StepSeconds);
	return Kills;
}

void FDTEnemyPool::SwapRemove(int32 Index)
{
	Pos.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	PrevPos.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Vel.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Facing.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Radius.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Hp.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	MaxHp.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Speed.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	ContactDamage.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	HpScale.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	DmgScale.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Role.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Flags.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	BeatId.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Dead.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Serial.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	Aux.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	PullReadyStep.RemoveAtSwap(Index, 1, EAllowShrinking::No);
}

int32 FDTEnemyPool::Compact()
{
	int32 Removed = 0;
	// Back to front: the element swapped in from the end has already been checked.
	for (int32 Index = Num() - 1; Index >= 0; --Index)
	{
		if (!Dead[Index])
		{
			continue;
		}
		const FDTEnemyAux& A = Aux[Index];
		if ((Flags[Index] & DTEnemyFlags::Arrival) != 0)
		{
			// An arrival's attack whose telegraph is showing completes even if the unit dies (R-ENM-13, D-013 §6).
			if (Role[Index] == EDTEnemyRole::Rusher && A.State == EDTEnemyState::WindUp)
			{
				FDTOrphanAttack& O = Orphans.AddDefaulted_GetRef();
				O.Role = EDTEnemyRole::Rusher;
				O.Origin = A.ChargeOrigin;
				O.Dir = A.AimDir;
				O.Length = A.ChargeLength;
				O.Width = 2.0f * Radius[Index];
				O.Left = A.StateLeft;
				O.Total = DTConst::ROLE_RUSHER_WINDUP_S;
				O.DmgScale = DmgScale[Index];
				O.Damage = ContactDamage[Index] * DmgScale[Index];
				O.EnemySerial = Serial[Index];
			}
			else if (Role[Index] == EDTEnemyRole::Turret && A.State == EDTEnemyState::Tell)
			{
				FDTOrphanAttack& O = Orphans.AddDefaulted_GetRef();
				O.Role = EDTEnemyRole::Turret;
				O.Origin = Pos[Index];
				O.Dir = Facing[Index];
				O.Length = DTEnemyPoolImpl::AimLineLength();
				O.Width = DTEnemyPoolImpl::AimLineWidth();
				O.Left = A.StateLeft;
				O.Total = DTConst::ROLE_TURRET_TELL_S;
				O.DmgScale = DmgScale[Index];
				O.Pattern = A.Pattern;
				O.bCloseFan = A.bCloseFan;
				O.TargetPlayer = A.TargetPlayer;
				O.VolleysLeft = DTConst::ENEMY_FIRE_BURST_SHOTS;
				O.EnemySerial = Serial[Index];
			}
		}
		SwapRemove(Index);
		++Removed;
	}
	return Removed;
}
