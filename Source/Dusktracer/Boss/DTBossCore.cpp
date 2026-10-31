#include "Boss/DTBossCore.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Enemies/DTEnemyBulletPool.h"
#include "Enemies/DTEnemyPool.h"

namespace DTBossCoreLocal
{
	/** Float slack on timers that count down by whole steps. */
	constexpr float TimerEps = 1.0e-4f;

	static const TCHAR* const FormNames[] = { TEXT("I"), TEXT("II"), TEXT("III") };

	/** The phases of form I (the only form with rows in S1), built once from the generated catalogue. */
	static const TArray<FDTBossPhaseDef>& Phases()
	{
		static const TArray<FDTBossPhaseDef> Defs = []()
		{
			TArray<FDTBossPhaseDef> Out;
			for (int32 Row = 0; Row < DTCat::NumBossAttacks; ++Row)
			{
				const DTCat::FBossAttackRow& R = DTCat::BossWarden[Row];
				if (FCString::Strcmp(R.Form, FormNames[0]) != 0)
				{
					continue;
				}
				const FName Name(R.Phase);
				FDTBossPhaseDef* Def = Out.FindByPredicate([&Name](const FDTBossPhaseDef& D) { return D.Name == Name; });
				if (!Def)
				{
					Def = &Out.AddDefaulted_GetRef();
					Def->Name = Name;
					Def->EnterHpFrac = R.EnterHpFrac;
					Def->LatestStart = R.LatestStart_s;
				}
				Def->Rows.Add(Row);
			}
			return Out;
		}();
		return Defs;
	}

	static int32 RowOf(int32 PhaseIndex, const TCHAR* Attack)
	{
		const TArray<FDTBossPhaseDef>& Defs = Phases();
		if (!Defs.IsValidIndex(PhaseIndex))
		{
			return INDEX_NONE;
		}
		for (const int32 Row : Defs[PhaseIndex].Rows)
		{
			if (FCString::Strcmp(DTCat::BossWarden[Row].Attack, Attack) == 0)
			{
				return Row;
			}
		}
		return INDEX_NONE;
	}

	static bool IsAttack(int32 Row, const TCHAR* Attack)
	{
		return Row != INDEX_NONE && FCString::Strcmp(DTCat::BossWarden[Row].Attack, Attack) == 0;
	}

	static FName AttackId(int32 Row) { return FName(DTCat::BossWarden[Row].Attack); }
	static const FName AttackCharge(TEXT("Charge"));
	static const FName AttackSweep(TEXT("Sweep"));
	static const FName AttackDashCharge(TEXT("DashCharge"));

	/** R-ENM-13: never less than the minimum lead, whatever the row says. */
	static float Lead(int32 Row)
	{
		return FMath::Max(DTCat::BossWarden[Row].Windup_s, DTConst::ENEMIES_TELEGRAPH_LEAD_MIN_S);
	}

	static float HullRadius() { return static_cast<float>(DTConst::BOSS_RADIUS_UU); }
	static float Standoff() { return DTConst::BOSS_STANDOFF_HULL_RADII * HullRadius(); }

	static FVector2f DirDeg(float Deg)
	{
		const float Rad = FMath::DegreesToRadians(Deg);
		return FVector2f(FMath::Cos(Rad), FMath::Sin(Rad));
	}

	static float BearingDeg(const FVector2f& V) { return FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X)); }

	/** Integer hash for the pure lane choice of a first ring (R-BOS-06: a function of the fight step, never a draw). */
	static uint32 Mix(uint32 X)
	{
		X ^= X >> 16;
		X *= 0x7feb352dU;
		X ^= X >> 15;
		X *= 0x846ca68bU;
		X ^= X >> 16;
		return X;
	}
}

FDTBossCore::FDTBossCore() = default;

// ---- rules ------------------------------------------------------------------------------------------------------------

EDTBossForm FDTBossCore::FormForWins(int32 InWins)
{
	if (InWins >= DTConst::BOSS_FORM_III_WINS)
	{
		return EDTBossForm::III;
	}
	return InWins >= DTConst::BOSS_FORM_II_WINS ? EDTBossForm::II : EDTBossForm::I;
}

const TCHAR* FDTBossCore::FormName(EDTBossForm InForm)
{
	return DTBossCoreLocal::FormNames[FMath::Clamp(static_cast<int32>(InForm), 0, 2)];
}

bool FDTBossCore::HasRows(EDTBossForm InForm)
{
	for (int32 Row = 0; Row < DTCat::NumBossAttacks; ++Row)
	{
		if (FCString::Strcmp(DTCat::BossWarden[Row].Form, FormName(InForm)) == 0)
		{
			return true;
		}
	}
	return false;
}

float FDTBossCore::LaneChordRequired()
{
	const float HeroDiameter = 2.0f * static_cast<float>(DTConst::HERO_RADIUS_UU);
	const float RoundDiameter = 2.0f * static_cast<float>(DTConst::BOSS_BULLET_RADIUS_UU);
	return DTConst::BOSS_LANE_HERO_DIAMETERS * HeroDiameter + 2.0f * RoundDiameter;
}

float FDTBossCore::LaneAngleRequiredDeg()
{
	const float Half = LaneChordRequired() / (2.0f * static_cast<float>(DTConst::BOSS_LANE_MEASURE_UU));
	return FMath::RadiansToDegrees(2.0f * FMath::Asin(FMath::Min(1.0f, Half)));
}

int32 FDTBossCore::LaneSlots(int32 Count)
{
	const float StepDeg = 360.0f / static_cast<float>(FMath::Max(1, Count));
	// The bounding rounds of k empty slots are (k + 1) steps apart.
	return FMath::Max(1, FMath::CeilToInt32(LaneAngleRequiredDeg() / StepDeg - DTBossCoreLocal::TimerEps) - 1);
}

float FDTBossCore::ChordAt(float GapDeg, float Distance)
{
	return 2.0f * Distance * FMath::Sin(0.5f * FMath::DegreesToRadians(FMath::Clamp(GapDeg, 0.0f, 360.0f)));
}

float FDTBossCore::WrapDeg(float Deg)
{
	float Out = FMath::Fmod(Deg, 360.0f);
	if (Out > 180.0f)
	{
		Out -= 360.0f;
	}
	else if (Out <= -180.0f)
	{
		Out += 360.0f;
	}
	return Out;
}

int32 FDTBossCore::NumPhases()
{
	return DTBossCoreLocal::Phases().Num();
}

const FDTBossPhaseDef& FDTBossCore::GetPhaseDef(int32 PhaseIndex)
{
	return DTBossCoreLocal::Phases()[PhaseIndex];
}

int32 FDTBossCore::FindRow(int32 PhaseIndex, FName Attack)
{
	return DTBossCoreLocal::RowOf(PhaseIndex, *Attack.ToString());
}

// ---- fight ------------------------------------------------------------------------------------------------------------

void FDTBossCore::Reset()
{
	*this = FDTBossCore();
}

void FDTBossCore::Start(int32 InWins, TConstArrayView<FVector2f> Heroes, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	Reset();
	bStarted = true;
	Wins = InWins;
	Form = FormForWins(InWins);
	if (!HasRows(Form))
	{
		// R-BOS-01: a form without rows plays form I and is counted.
		++Counters.FormFallbacks;
		Form = EDTBossForm::I;
	}

	// The hull enters at the arena centre, or a standoff away from a hero standing on it.
	FVector2f At = FVector2f::ZeroVector;
	if (Heroes.Num() > 0 && FVector2f::Distance(Heroes[0], At) < Standoff())
	{
		const FVector2f Away = (At - Heroes[0]).GetSafeNormal();
		At = Heroes[0] + (Away.IsNearlyZero() ? FVector2f(1.0f, 0.0f) : Away) * Standoff();
	}
	FDTEnemySpawn Spawn;
	Spawn.Role = EDTEnemyRole::Boss;
	Spawn.Position = At;
	// R-BOS-09: the pool holds the reserve (C.BOSS.HP_BASE x C.BOSS.HP_MAX_MULT); the director's multiplier never applies.
	Spawn.HpScale = DTConst::BOSS_HP_MAX_MULT;
	Spawn.DmgScale = 1.0f;
	Sink.QueueSpawn(Spawn);
}

bool FDTBossCore::FindHull(const FDTEnemyPool& Pool)
{
	const TConstArrayView<uint32> Serials = Pool.GetSerials();
	const TConstArrayView<EDTEnemyRole> Roles = Pool.GetRoles();
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		const bool bMatch = bAdopted ? Serials[Index] == Hull.Serial : (Roles[Index] == EDTEnemyRole::Boss && !Pool.IsDead(Index));
		if (bMatch)
		{
			Hull = Pool.GetHandle(Index);
			return true;
		}
	}
	return false;
}

FVector2f FDTBossCore::HullPos(const FDTEnemyPool& Pool) const
{
	return Pool.GetPositions()[Hull.Index];
}

FVector2f FDTBossCore::Facing(const FDTEnemyPool& Pool) const
{
	return Pool.GetFacings()[Hull.Index];
}

int32 FDTBossCore::NearestHero(const FVector2f& At, const FDTWorldView& View) const
{
	int32 Best = INDEX_NONE;
	float BestSq = TNumericLimits<float>::Max();
	for (int32 Hero = 0; Hero < View.HeroPositions.Num(); ++Hero)
	{
		const float Sq = FVector2f::DistSquared(View.HeroPositions[Hero], At);
		if (Sq < BestSq)
		{
			BestSq = Sq;
			Best = Hero;
		}
	}
	return Best;
}

void FDTBossCore::Adopt(const FDTBossStepIn& In, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	FDTEnemyPool& Pool = *In.Pool;
	bAdopted = true;
	FightStartStep = In.StepIndex;
	FightTime = 0.0f;
	ReserveHp = Pool.GetHp()[Hull.Index];
	HpBase = ReserveHp;
	EffectiveMax = static_cast<float>(DTConst::BOSS_HP_BASE);
	Dealt = 0.0f;
	HpFrac = 1.0f;
	const int32 NumHeroes = In.View->HeroPositions.Num();
	DamageByAttack.SetNum(NumHeroes);
	RingHits.Init(0, NumHeroes);
	const int32 Target = NearestHero(HullPos(Pool), *In.View);
	if (Target != INDEX_NONE)
	{
		Pool.SetFacing(Hull.Index, In.View->HeroPositions[Target] - HullPos(Pool));
	}
	Sink.BossStart(FName(FormName(Form)), Wins);
	EnterPhase(0, false, Sink);
	// The opening walk, then the signature charge (R-BOS-02).
	Body = FBody();
	Body.State = EBody::Walk;
	Body.Left = DTConst::BOSS_OPENING_WALK_S;
}

void FDTBossCore::Finish(const FString& InOutcome, int64 StepIndex, IDTBossSink& Sink)
{
	bEnded = true;
	Outcome = InOutcome;
	FDTTelBossEnd EndRec;
	EndRec.Outcome = InOutcome;
	EndRec.FightTimeSeconds = FightTime;
	EndRec.HpLeftFrac = HpFrac;
	EndRec.Phase = Phase + 1;
	const int32 RingCount = FMath::Max(1, Counters.RingsFired);
	for (int32 Player = 0; Player < DamageByAttack.Num(); ++Player)
	{
		FDTTelBossPlayer& Rec = EndRec.PerPlayer.AddDefaulted_GetRef();
		Rec.DamageTakenByAttack = DamageByAttack[Player];
		Rec.HitsPerRing = static_cast<float>(RingHits.IsValidIndex(Player) ? RingHits[Player] : 0) / static_cast<float>(RingCount);
	}
	Sink.BossEnd(EndRec);
	Rings.Reset();
	Crosses.Reset();
	Spiral = FSpiral();
	Hazard.bOn = false;
	Body = FBody();
}

void FDTBossCore::End(const FString& InOutcome, IDTBossSink& Sink)
{
	if (IsActive())
	{
		Finish(InOutcome, INDEX_NONE, Sink);
	}
}

void FDTBossCore::ForcePhase(int32 InPhase, IDTBossSink& Sink)
{
	if (!bAdopted || bEnded)
	{
		return;
	}
	const int32 Target = FMath::Clamp(InPhase - 1, 0, NumPhases() - 1);
	while (Phase < Target)
	{
		EnterPhase(Phase + 1, false, Sink);
	}
}

void FDTBossCore::UpdateHp(FDTEnemyPool& Pool, IDTBossSink& Sink)
{
	const float Hp = Pool.GetHp()[Hull.Index];
	Dealt = FMath::Max(0.0f, HpBase - Hp);
	if (!bCalibrated && FightTime >= DTConst::BOSS_CALIBRATE_S - 1.0e-3f)
	{
		// R-BOS-09: once, from the damage over the window; damage dealt carries over, nothing is added (R-BOS-10).
		const float WindowDps = Dealt / DTConst::BOSS_CALIBRATE_S;
		const float Raw = WindowDps * static_cast<float>(DTConst::BOSS_TARGET_FIGHT_S);
		const float Min = DTConst::BOSS_HP_MIN_MULT * static_cast<float>(DTConst::BOSS_HP_BASE);
		const float Max = DTConst::BOSS_HP_MAX_MULT * static_cast<float>(DTConst::BOSS_HP_BASE);
		EffectiveMax = FMath::Clamp(Raw, Min, Max);
		const bool bClamped = Raw < Min || Raw > Max;
		const float Left = FMath::Min(Hp, FMath::Max(EffectiveMax - Dealt, UE_KINDA_SMALL_NUMBER));
		Pool.SetHp(Hull.Index, Left);
		HpBase = Left + Dealt;
		bCalibrated = true;
		Sink.BossCalibrated(WindowDps, EffectiveMax, bClamped);
	}
	HpFrac = EffectiveMax > 0.0f ? FMath::Clamp((EffectiveMax - Dealt) / EffectiveMax, 0.0f, 1.0f) : 0.0f;
}

void FDTBossCore::EnterPhase(int32 PhaseIndex, bool bByTime, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	Phase = PhaseIndex;
	// R-BOS-04: every change after the first starts a shift with no attack.
	ShiftLeft = PhaseIndex == 0 ? 0.0f : DTConst::BOSS_PHASE_SHIFT_S;
	Sink.BossPhase(PhaseIndex + 1, FightTime, HpFrac, bByTime);
	RingTimer = 0.0f;
	const int32 DashRow = RowOf(Phase, TEXT("DashCharge"));
	DashTimer = DashRow != INDEX_NONE ? DTCat::BossWarden[DashRow].Interval_s : 0.0f;
	AddsTimer = 0.0f;
	Spiral = FSpiral();
}

int32 FDTBossCore::BeginRec(FName AttackIdIn, EDTTelegraphShape Shape, int64 Step, float Length, float Width)
{
	FDTBossAttackRec& Rec = Attacks.AddDefaulted_GetRef();
	Rec.AttackId = AttackIdIn;
	Rec.Shape = Shape;
	Rec.Phase = Phase + 1;
	Rec.TelegraphStep = Step;
	Rec.FootprintLength = Length;
	Rec.FootprintWidth = Width;
	return Attacks.Num() - 1;
}

float FDTBossCore::TravelLimit(const FVector2f& From, const FVector2f& Dir) const
{
	using namespace DTBossCoreLocal;
	float Travel = TNumericLimits<float>::Max();
	const float Limit = FMath::Max(0.0f, static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU) - HullRadius());
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const float D = Dir[Axis];
		if (FMath::Abs(D) > UE_KINDA_SMALL_NUMBER)
		{
			const float Wall = D > 0.0f ? Limit : -Limit;
			Travel = FMath::Min(Travel, FMath::Max(0.0f, (Wall - From[Axis]) / D));
		}
	}
	if (Hazard.bOn)
	{
		const float Rc = FMath::Max(0.0f, Hazard.Radius - HullRadius());
		const float B = FVector2f::DotProduct(From, Dir);
		const float C = From.SizeSquared() - Rc * Rc;
		const float Disc = B * B - C;
		Travel = Disc >= 0.0f ? FMath::Min(Travel, FMath::Max(0.0f, -B + FMath::Sqrt(Disc))) : 0.0f;
	}
	return Travel;
}

FVector2f FDTBossCore::KeepInside(const FVector2f& At) const
{
	if (!Hazard.bOn)
	{
		return At;
	}
	const float Rc = FMath::Max(0.0f, Hazard.Radius - DTBossCoreLocal::HullRadius());
	const float Dist = At.Size();
	return Dist > Rc && Dist > UE_KINDA_SMALL_NUMBER ? At * (Rc / Dist) : At;
}

void FDTBossCore::AddHeroHit(FDTWorldView& View, int32 Player, float Damage, FName AttackIdIn, const FVector2f& At)
{
	FDTHeroHit& Hit = View.HeroHits.AddDefaulted_GetRef();
	Hit.PlayerId = Player;
	Hit.Damage = Damage;
	Hit.Kind = AttackIdIn == DTBossCoreLocal::AttackCharge ? EDTHeroHitKind::Charge
		: AttackIdIn == DTBossCoreLocal::AttackSweep ? EDTHeroHitKind::Sweep
		: AttackIdIn == DTBossCoreLocal::AttackDashCharge ? EDTHeroHitKind::DashCharge : EDTHeroHitKind::HazardRing;
	Hit.SourceRole = EDTEnemyRole::Boss;
	Hit.AttackId = AttackIdIn;
	Hit.EnemySerial = Hull.Serial;
	Hit.Position = At;
	if (DamageByAttack.IsValidIndex(Player))
	{
		DamageByAttack[Player].FindOrAdd(AttackIdIn) += Damage;
	}
}

void FDTBossCore::TallyHeroHits(const FDTWorldView& View, int32 From)
{
	// Contact and rounds were appended by the enemy pool earlier in this Enemies phase.
	for (int32 Index = From; Index < View.HeroHits.Num(); ++Index)
	{
		const FDTHeroHit& Hit = View.HeroHits[Index];
		if (Hit.SourceRole != EDTEnemyRole::Boss || !DamageByAttack.IsValidIndex(Hit.PlayerId))
		{
			continue;
		}
		DamageByAttack[Hit.PlayerId].FindOrAdd(Hit.AttackId) += Hit.Damage;
		const FString Attack = Hit.AttackId.ToString();
		if (Attack == TEXT("Ring") || Attack == TEXT("CrossRing"))
		{
			++RingHits[Hit.PlayerId];
		}
	}
}

void FDTBossCore::Step(const FDTBossStepIn& In, IDTBossSink& Sink)
{
	if (!bStarted || bEnded || !In.View || !In.Pool)
	{
		return;
	}
	FDTEnemyPool& Pool = *In.Pool;
	FDTWorldView& View = *In.View;
	if (!bAdopted)
	{
		if (!FindHull(Pool))
		{
			return; // queued: the hull enters at the next Enemies phase
		}
		Adopt(In, Sink);
	}
	else if (!Pool.IsHandleValid(Hull) && !FindHull(Pool))
	{
		// R-BOS-03: the hull's death is one Kill (ResolveHits, last step) to the killing hit's owner.
		for (const FDTKill& Kill : View.Kills)
		{
			if (Kill.EnemySerial == Hull.Serial)
			{
				Dealt = EffectiveMax;
				HpFrac = 0.0f;
				Finish(TEXT("win"), In.StepIndex, Sink);
				Sink.BossDefeated(In.StepIndex, Kill.OwnerId);
				return;
			}
		}
		Finish(TEXT("removed"), In.StepIndex, Sink);
		return;
	}

	TallyHeroHits(View, 0);
	UpdateHp(Pool, Sink);

	// R-BOS-04: phases only advance, each in order, by HP threshold or latest start, whichever comes first.
	while (Phase + 1 < NumPhases())
	{
		const FDTBossPhaseDef& Next = GetPhaseDef(Phase + 1);
		const bool bByHp = HpFrac <= Next.EnterHpFrac;
		const bool bByTime = FightTime >= Next.LatestStart - 1.0e-3f;
		if (!bByHp && !bByTime)
		{
			break;
		}
		EnterPhase(Phase + 1, bByTime && !bByHp, Sink);
	}

	StepBody(In, Sink);
	StepRings(In, Sink);
	StepSpiral(In, Sink);
	StepHazard(In, Sink);
	StepAdds(In, Sink);
	if (ShiftLeft > 0.0f)
	{
		ShiftLeft = ShiftLeft - In.EnemySeconds() > DTBossCoreLocal::TimerEps ? ShiftLeft - In.EnemySeconds() : 0.0f;
	}

	// R-BOS-03: always inside the hazard ring, however it got there.
	const FVector2f Here = HullPos(Pool);
	const FVector2f Inside = KeepInside(Here);
	if (!Inside.Equals(Here, 0.0f))
	{
		Pool.SetScriptedPosition(Hull, Inside);
	}
	PublishTelegraphs(View, Pool);
	FightTime += In.StepSeconds;
}

void FDTBossCore::Walk(const FDTBossStepIn& In)
{
	using namespace DTBossCoreLocal;
	FDTEnemyPool& Pool = *In.Pool;
	const FVector2f Pos = HullPos(Pool);
	const int32 Target = NearestHero(Pos, *In.View);
	if (Target == INDEX_NONE)
	{
		return;
	}
	const FVector2f To = In.View->HeroPositions[Target] - Pos;
	const float Dist = To.Size();
	const FVector2f Dir = Dist > UE_KINDA_SMALL_NUMBER ? To / Dist : Facing(Pool);
	// R-ENM-22 through R-BOS-11: each Chill stack slows the walk (the boss never freezes).
	const float Slow = FMath::Max(0.0f, 1.0f - Pool.GetAux(Hull.Index).ChillStacks * DTConst::STATUS_CHILL_SLOW_PER_STACK);
	const float Move = static_cast<float>(DTConst::BOSS_WALK_SPEED_UU_S) * Slow * In.EnemySeconds();
	// Towards the standoff ring around the target, never inside it (R-BOS-03): approach or back off.
	const float Along = FMath::Clamp(Dist - Standoff(), -Move, Move);
	Pool.SetScriptedPosition(Hull, KeepInside(Pos + Dir * Along));
	Pool.SetFacing(Hull.Index, Dir);
}

void FDTBossCore::StartAttack(const FDTBossStepIn& In, int32 Row, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	FDTEnemyPool& Pool = *In.Pool;
	const DTCat::FBossAttackRow& R = DTCat::BossWarden[Row];
	const FVector2f Pos = HullPos(Pool);
	const int32 Target = NearestHero(Pos, *In.View);
	const FVector2f To = Target != INDEX_NONE ? In.View->HeroPositions[Target] - Pos : FVector2f::ZeroVector;
	const FVector2f Dir = To.IsNearlyZero() ? Facing(Pool) : To.GetSafeNormal();
	Pool.SetFacing(Hull.Index, Dir);

	Body.Row = Row;
	Body.Dir = Dir;
	Body.Start = Pos;
	Body.Moved = 0.0f;
	Body.HitMask = 0;
	Body.Left = Lead(Row);
	Body.Total = Body.Left;
	if (IsAttack(Row, TEXT("Sweep")))
	{
		Body.State = EBody::SweepWindup;
		Body.Travel = 0.0f;
		Body.Rec = BeginRec(AttackId(Row), EDTTelegraphShape::Arc, In.StepIndex, R.Extent_uu, R.ArcDeg);
	}
	else
	{
		// Charge and DashCharge: the band is the hull's swept path, clipped to the wall and the hazard ring.
		Body.State = EBody::Windup;
		Body.Travel = FMath::Min(R.Extent_uu, TravelLimit(Pos, Dir));
		Body.Rec = BeginRec(AttackId(Row), EDTTelegraphShape::Band, In.StepIndex, Body.Travel + 2.0f * HullRadius(), 2.0f * HullRadius());
	}
	Counters.AttacksStarted.FindOrAdd(AttackId(Row))++;
	Counters.AttacksStartedInShift += IsShifting() ? 1 : 0;
	Sink.ThreatAction(Pos, AttackId(Row));
}

void FDTBossCore::StepBody(const FDTBossStepIn& In, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	FDTEnemyPool& Pool = *In.Pool;
	FDTWorldView& View = *In.View;
	const float Dt = In.EnemySeconds();
	const float HeroRadius = static_cast<float>(DTConst::HERO_RADIUS_UU);

	if (!IsShifting())
	{
		const int32 DashRow = RowOf(Phase, TEXT("DashCharge"));
		if (DashRow != INDEX_NONE)
		{
			DashTimer -= Dt;
		}
	}

	switch (Body.State)
	{
	case EBody::Walk:
	{
		Walk(In);
		Body.Left = FMath::Max(0.0f, Body.Left - Dt);
		if (Body.Left > TimerEps || IsShifting())
		{
			break;
		}
		const int32 ChargeRow = RowOf(Phase, TEXT("Charge"));
		const int32 SweepRow = RowOf(Phase, TEXT("Sweep"));
		const int32 DashRow = RowOf(Phase, TEXT("DashCharge"));
		if (ChargeRow != INDEX_NONE)
		{
			// Charge and Sweep alternate; the sweep only when the target is close, otherwise another charge.
			const int32 Target = NearestHero(HullPos(Pool), View);
			const bool bClose = Target != INDEX_NONE
				&& FVector2f::Distance(View.HeroPositions[Target], HullPos(Pool)) <= static_cast<float>(DTConst::BOSS_SWEEP_RANGE_UU);
			StartAttack(In, Body.bLastWasCharge && SweepRow != INDEX_NONE && bClose ? SweepRow : ChargeRow, Sink);
		}
		else if (DashRow != INDEX_NONE && DashTimer <= TimerEps)
		{
			DashTimer += FMath::Max(DTCat::BossWarden[DashRow].Interval_s, Dt);
			StartAttack(In, DashRow, Sink);
		}
		break;
	}

	case EBody::Windup:
		Body.Left -= In.StepSeconds; // a telegraph's lead runs in sim time (keepers.md R-KEE-37, R-DIR-10)
		if (Body.Left > TimerEps)
		{
			break;
		}
		Body.State = EBody::Charge;
		Attacks[Body.Rec].LandStep = In.StepIndex;
		// The charge moves from this step on.
		[[fallthrough]];

	case EBody::Charge:
	{
		const DTCat::FBossAttackRow& R = DTCat::BossWarden[Body.Row];
		const float Move = FMath::Min(R.Speed_uu_s * Dt, FMath::Max(0.0f, Body.Travel - Body.Moved));
		Body.Moved += Move;
		const FVector2f At = Body.Start + Body.Dir * Body.Moved;
		Pool.SetScriptedPosition(Hull, At);
		const float Reach = HullRadius() + HeroRadius;
		for (int32 Hero = 0; Hero < View.HeroPositions.Num() && Hero < 32; ++Hero)
		{
			const uint32 Bit = 1u << Hero;
			if ((Body.HitMask & Bit) == 0 && FVector2f::DistSquared(View.HeroPositions[Hero], At) <= Reach * Reach)
			{
				Body.HitMask |= Bit;
				AddHeroHit(View, Hero, R.Damage, AttackId(Body.Row), At);
			}
		}
		if (Body.Moved >= Body.Travel - TimerEps)
		{
			Attacks[Body.Rec].DamageLength = Body.Moved + 2.0f * HullRadius();
			Attacks[Body.Rec].DamageWidth = 2.0f * HullRadius();
			Body.bLastWasCharge = IsAttack(Body.Row, TEXT("Charge"));
			Body.State = EBody::Recover;
			Body.Left = DTConst::BOSS_CHARGE_RECOVER_S;
		}
		break;
	}

	case EBody::Recover:
		Body.Left -= Dt;
		if (Body.Left <= TimerEps)
		{
			Body.State = EBody::Walk;
			Body.Left = 0.0f;
		}
		break;

	case EBody::SweepWindup:
	{
		Body.Left -= In.StepSeconds;
		if (Body.Left > TimerEps)
		{
			break;
		}
		// Lands: every hero inside the sector (radius `extent_uu`, full angle `arc_deg`) is hit once.
		const DTCat::FBossAttackRow& R = DTCat::BossWarden[Body.Row];
		const float HalfCos = FMath::Cos(FMath::DegreesToRadians(0.5f * R.ArcDeg));
		for (int32 Hero = 0; Hero < View.HeroPositions.Num(); ++Hero)
		{
			const FVector2f Rel = View.HeroPositions[Hero] - Body.Start;
			const float Dist = Rel.Size();
			const bool bInside = Dist <= R.Extent_uu + HeroRadius
				&& (Dist <= UE_KINDA_SMALL_NUMBER || FVector2f::DotProduct(Rel / Dist, Body.Dir) >= HalfCos);
			if (bInside)
			{
				AddHeroHit(View, Hero, R.Damage, AttackId(Body.Row), View.HeroPositions[Hero]);
			}
		}
		FDTBossAttackRec& Rec = Attacks[Body.Rec];
		Rec.LandStep = In.StepIndex;
		Rec.DamageLength = R.Extent_uu;
		Rec.DamageWidth = R.ArcDeg;
		Body.bLastWasCharge = false;
		Body.State = EBody::Walk;
		Body.Left = 0.0f;
		break;
	}
	}
}

void FDTBossCore::StartRing(const FDTBossStepIn& In, int32 Row, bool bCross, float FirstLaneDeg, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	FDTEnemyPool& Pool = *In.Pool;
	const DTCat::FBossAttackRow& R = DTCat::BossWarden[Row];
	const int32 Count = FMath::Max(2, R.Count);
	const float StepDeg = 360.0f / static_cast<float>(Count);
	const int32 Empty = LaneSlots(Count);
	const float GapDeg = (Empty + 1) * StepDeg;
	const float Half = 0.5f * static_cast<float>(Empty + 1);

	// R-BOS-06: the rounds come from the shoulder mirror heads; the first ring from one, its crossing ring from the other.
	const FVector2f Pos = HullPos(Pool);
	const FVector2f F = Facing(Pool);
	const FVector2f Perp(-F.Y, F.X);
	const FVector2f Origin = Pos + Perp * (bCross ? -HullRadius() : HullRadius());
	const int32 Target = NearestHero(Origin, *In.View);
	const FVector2f TargetPos = Target != INDEX_NONE ? In.View->HeroPositions[Target] : Origin + F * Standoff();
	const FVector2f ToTarget = TargetPos - Origin;
	// Slot 0 flies exactly at the target: the slot phase is the target bearing.
	const float Theta = BearingDeg(ToTarget.IsNearlyZero() ? F : ToTarget);

	// The lane is Empty slots between slot N and slot N + Empty + 1, centred at (N + Half) steps from the target bearing.
	int32 N = 0;
	if (!bCross)
	{
		// R-BOS-07: centred within the bearing limit; which grid position is a pure function of the fight step.
		const float MaxDeg = static_cast<float>(DTConst::BOSS_LANE_BEARING_MAX_DEG);
		const int32 NMin = FMath::CeilToInt32(-MaxDeg / StepDeg - Half);
		const int32 NMax = FMath::FloorToInt32(MaxDeg / StepDeg - Half);
		if (NMax < NMin)
		{
			N = FMath::RoundToInt32(-Half);
		}
		else
		{
			const uint32 FightStep = static_cast<uint32>(In.StepIndex - FightStartStep);
			N = NMin + static_cast<int32>(Mix(FightStep) % static_cast<uint32>(NMax - NMin + 1));
		}
	}
	else
	{
		// R-BOS-08: the crossing lane sits C.BOSS.CROSS_LANE_OFFSET_DEG from the first (Boss stream), snapped to the slot
		// grid inside the range, and never over slot 0, so a hero standing still is hit.
		const float Lo = static_cast<float>(DTConst::BOSS_CROSS_LANE_OFFSET_DEG_MIN) + 0.5f * StepDeg;
		const float Hi = static_cast<float>(DTConst::BOSS_CROSS_LANE_OFFSET_DEG_MAX) - 0.5f * StepDeg;
		const float Mag = In.Rng ? In.Rng->FRandRange(Lo, FMath::Max(Lo, Hi)) : 0.5f * (Lo + Hi);
		const float Sign = In.Rng && In.Rng->RandHelper(2) == 0 ? -1.0f : 1.0f;
		const float Rel = WrapDeg(FirstLaneDeg + Sign * Mag - Theta);
		N = FMath::RoundToInt32(Rel / StepDeg - Half);
		if (N + 1 <= 0 && 0 <= N + Empty)
		{
			N = Rel >= 0.0f ? 0 : -Empty - 1;
		}
	}
	const float LaneDeg = WrapDeg(Theta + StepDeg * (static_cast<float>(N) + Half));

	FDTBossRingShot& Shot = RingShots.AddDefaulted_GetRef();
	Shot.AttackId = AttackId(Row);
	Shot.bCross = bCross;
	Shot.TelegraphStep = In.StepIndex;
	Shot.Origin = Origin;
	Shot.TargetPos = TargetPos;
	Shot.TargetBearingDeg = WrapDeg(Theta);
	Shot.LaneCentreDeg = LaneDeg;
	Shot.LaneGapDeg = GapDeg;
	Shot.LaneChordUU = ChordAt(GapDeg, static_cast<float>(DTConst::BOSS_LANE_MEASURE_UU));
	Shot.CrossOffsetDeg = bCross ? WrapDeg(LaneDeg - FirstLaneDeg) : 0.0f;
	Shot.TelegraphGapDeg = GapDeg;
	Shot.TelegraphLaneDeg = LaneDeg;
	Shot.Count = Count;

	FPendingRing& Ring = Rings.AddDefaulted_GetRef();
	Ring.Row = Row;
	Ring.Shot = RingShots.Num() - 1;
	Ring.Rec = BeginRec(AttackId(Row), EDTTelegraphShape::RingGap, In.StepIndex, static_cast<float>(DTConst::BOSS_LANE_MEASURE_UU), GapDeg);
	Ring.Count = Count;
	Ring.EmptyFrom = ((N + 1) % Count + Count) % Count;
	Ring.Empty = Empty;
	Ring.SlotPhaseDeg = Theta;
	Ring.Left = Lead(Row);
	Ring.Total = Ring.Left;
	Ring.Target = Target;

	Counters.AttacksStarted.FindOrAdd(AttackId(Row))++;
	Counters.AttacksStartedInShift += IsShifting() && !bCross ? 1 : 0;
	Sink.ThreatAction(Origin, AttackId(Row));

	if (!bCross)
	{
		const int32 CrossRow = RowOf(Phase, TEXT("CrossRing"));
		if (CrossRow != INDEX_NONE)
		{
			// Fires C.BOSS.CROSS_RING_DELAY_S after this ring, its own full lead before that.
			FCrossPlan& Plan = Crosses.AddDefaulted_GetRef();
			Plan.Row = CrossRow;
			Plan.StartLeft = FMath::Max(0.0f, Lead(Row) + DTConst::BOSS_CROSS_RING_DELAY_S - Lead(CrossRow));
			Plan.FirstLaneDeg = LaneDeg;
		}
	}
}

void FDTBossCore::FireRing(const FDTBossStepIn& In, FPendingRing& Ring)
{
	using namespace DTBossCoreLocal;
	const DTCat::FBossAttackRow& R = DTCat::BossWarden[Ring.Row];
	FDTBossRingShot& Shot = RingShots[Ring.Shot];
	const float StepDeg = 360.0f / static_cast<float>(Ring.Count);
	FDTEnemyBulletPool& Rounds = In.Pool->GetRounds();
	for (int32 Slot = 0; Slot < Ring.Count; ++Slot)
	{
		if (((Slot - Ring.EmptyFrom) % Ring.Count + Ring.Count) % Ring.Count < Ring.Empty)
		{
			continue; // the lane
		}
		const float Deg = Ring.SlotPhaseDeg + static_cast<float>(Slot) * StepDeg;
		FDTEnemyRoundSpawn Round;
		Round.Position = Shot.Origin;
		Round.Velocity = DirDeg(Deg) * R.Speed_uu_s;
		Round.Radius = static_cast<float>(DTConst::BOSS_BULLET_RADIUS_UU);
		Round.Damage = R.Damage;
		Round.Life = DTConst::BOSS_ROUND_LIFE_S;
		Round.TargetPlayer = Ring.Target;
		Round.SourceRole = EDTEnemyRole::Boss;
		Round.AttackId = Shot.AttackId;
		Round.EnemySerial = Hull.Serial;
		if (Rounds.Spawn(Round) == INDEX_NONE)
		{
			++Counters.RoundsRefused;
		}
		else
		{
			++Counters.RoundsFired;
		}
		Shot.SlotDeg.Add(WrapDeg(Deg));
		++Shot.Fired;
	}
	Shot.FireStep = In.StepIndex;
	Attacks[Ring.Rec].LandStep = In.StepIndex;
	Attacks[Ring.Rec].DamageLength = static_cast<float>(DTConst::BOSS_LANE_MEASURE_UU);
	Attacks[Ring.Rec].DamageWidth = Shot.LaneGapDeg;
	++Counters.RingsFired;
}

void FDTBossCore::StepRings(const FDTBossStepIn& In, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	const float Dt = In.EnemySeconds();
	// Rings telegraphed in earlier steps count down first, so a ring started this step shows its whole lead (R-ENM-13).
	for (int32 Index = 0; Index < Rings.Num();)
	{
		FPendingRing& Ring = Rings[Index];
		Ring.Left -= In.StepSeconds;
		if (Ring.Left <= TimerEps)
		{
			FireRing(In, Ring);
			Rings.RemoveAt(Index);
			continue;
		}
		++Index;
	}
	// A crossing ring completes its pair even through a phase shift: it is the second half of an attack already started.
	for (int32 Index = 0; Index < Crosses.Num();)
	{
		Crosses[Index].StartLeft -= Dt;
		if (Crosses[Index].StartLeft <= TimerEps)
		{
			const FCrossPlan Plan = Crosses[Index];
			Crosses.RemoveAt(Index);
			StartRing(In, Plan.Row, true, Plan.FirstLaneDeg, Sink);
			continue;
		}
		++Index;
	}
	if (!IsShifting())
	{
		const int32 RingRow = RowOf(Phase, TEXT("Ring"));
		if (RingRow != INDEX_NONE)
		{
			RingTimer -= Dt;
			if (RingTimer <= TimerEps)
			{
				RingTimer += FMath::Max(DTCat::BossWarden[RingRow].Interval_s, Dt);
				StartRing(In, RingRow, false, 0.0f, Sink);
			}
		}
	}
}

void FDTBossCore::StepSpiral(const FDTBossStepIn& In, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	const int32 Row = RowOf(Phase, TEXT("Spiral"));
	if (Row == INDEX_NONE)
	{
		Spiral = FSpiral();
		return;
	}
	FDTEnemyPool& Pool = *In.Pool;
	const DTCat::FBossAttackRow& R = DTCat::BossWarden[Row];
	const FVector2f Pos = HullPos(Pool);
	if (!Spiral.bOn)
	{
		if (IsShifting())
		{
			return;
		}
		const int32 Target = NearestHero(Pos, *In.View);
		const FVector2f To = Target != INDEX_NONE ? In.View->HeroPositions[Target] - Pos : Facing(Pool);
		Spiral.bOn = true;
		Spiral.Row = Row;
		// The lane opens on the target; the arms start a quarter turn off it.
		Spiral.LaneDeg = BearingDeg(To.IsNearlyZero() ? Facing(Pool) : To);
		Spiral.ArmDeg = WrapDeg(Spiral.LaneDeg + 90.0f);
		Spiral.WindLeft = Lead(Row);
		Spiral.Total = Spiral.WindLeft;
		Spiral.EmitAccum = 0.0f;
		Spiral.Rec = BeginRec(AttackId(Row), EDTTelegraphShape::SpiralLane, In.StepIndex,
			static_cast<float>(DTConst::BOSS_LANE_MEASURE_UU), LaneAngleRequiredDeg());
		Counters.AttacksStarted.FindOrAdd(AttackId(Row))++;
		Counters.AttacksStartedInShift += IsShifting() ? 1 : 0;
		Sink.ThreatAction(Pos, AttackId(Row));
		return;
	}
	if (Spiral.WindLeft > 0.0f)
	{
		Spiral.WindLeft -= In.StepSeconds;
		if (Spiral.WindLeft > TimerEps)
		{
			return;
		}
		Spiral.WindLeft = 0.0f;
		Attacks[Spiral.Rec].LandStep = In.StepIndex;
		Attacks[Spiral.Rec].DamageLength = static_cast<float>(DTConst::BOSS_LANE_MEASURE_UU);
		Attacks[Spiral.Rec].DamageWidth = LaneAngleRequiredDeg();
	}
	// `arc_deg` is the turn rate; the lane sector turns at C.BOSS.SPIRAL_LANE_RATE_RATIO of it (R-BOS-07).
	Spiral.ArmDeg = WrapDeg(Spiral.ArmDeg + R.ArcDeg * In.EnemySeconds());
	Spiral.LaneDeg = WrapDeg(Spiral.LaneDeg + R.ArcDeg * DTConst::BOSS_SPIRAL_LANE_RATE_RATIO * In.EnemySeconds());
	Spiral.EmitAccum += In.EnemySeconds();
	const float Interval = FMath::Max(R.Interval_s, In.EnemySeconds());
	const int32 Arms = FMath::Max(1, R.Count);
	const float HalfLane = 0.5f * LaneAngleRequiredDeg();
	FDTEnemyBulletPool& Rounds = Pool.GetRounds();
	while (Spiral.EmitAccum >= Interval - TimerEps)
	{
		Spiral.EmitAccum -= Interval;
		for (int32 Arm = 0; Arm < Arms; ++Arm)
		{
			const float Deg = Spiral.ArmDeg + Arm * 360.0f / static_cast<float>(Arms);
			const float FromLane = FMath::Abs(WrapDeg(Deg - Spiral.LaneDeg));
			if (FromLane < HalfLane)
			{
				++Counters.SpiralSkipped; // the rotating lane sector
				continue;
			}
			FDTEnemyRoundSpawn Round;
			Round.Position = Pos;
			Round.Velocity = DirDeg(Deg) * R.Speed_uu_s;
			Round.Radius = static_cast<float>(DTConst::BOSS_BULLET_RADIUS_UU);
			Round.Damage = R.Damage;
			Round.Life = DTConst::BOSS_ROUND_LIFE_S;
			Round.SourceRole = EDTEnemyRole::Boss;
			Round.AttackId = AttackId(Row);
			Round.EnemySerial = Hull.Serial;
			if (Rounds.Spawn(Round) == INDEX_NONE)
			{
				++Counters.RoundsRefused;
			}
			else
			{
				++Counters.RoundsFired;
			}
			Counters.SpiralMinLaneDeg = FMath::Min(Counters.SpiralMinLaneDeg, FromLane);
			++Counters.SpiralEmissions;
		}
	}
}

void FDTBossCore::StepHazard(const FDTBossStepIn& In, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	FDTWorldView& View = *In.View;
	if (!Hazard.bOn)
	{
		const int32 Row = RowOf(Phase, TEXT("Hazard"));
		if (Row == INDEX_NONE || IsShifting())
		{
			return;
		}
		// Shown at `extent_uu` for its wind-up, then shrinks at `speed_uu_s` to C.BOSS.HAZARD_MIN_RADIUS_UU (R-BOS-10).
		Hazard.bOn = true;
		Hazard.Row = Row;
		Hazard.Radius = DTCat::BossWarden[Row].Extent_uu;
		Hazard.WindLeft = Lead(Row);
		Hazard.Total = Hazard.WindLeft;
		Hazard.OutsideFor.Init(0.0f, View.HeroPositions.Num());
		Hazard.TickAccum.Init(0.0f, View.HeroPositions.Num());
		Hazard.bHitOnce.Init(false, View.HeroPositions.Num());
		Hazard.Rec = BeginRec(AttackId(Row), EDTTelegraphShape::HazardRing, In.StepIndex, Hazard.Radius, 0.0f);
		Counters.AttacksStarted.FindOrAdd(AttackId(Row))++;
		Sink.ThreatAction(FVector2f::ZeroVector, AttackId(Row));
		return;
	}
	const DTCat::FBossAttackRow& R = DTCat::BossWarden[Hazard.Row];
	if (Hazard.WindLeft > 0.0f)
	{
		Hazard.WindLeft -= In.StepSeconds;
		if (Hazard.WindLeft > TimerEps)
		{
			return;
		}
		Hazard.WindLeft = 0.0f;
		Attacks[Hazard.Rec].LandStep = In.StepIndex;
	}
	Hazard.Radius = FMath::Max(static_cast<float>(DTConst::BOSS_HAZARD_MIN_RADIUS_UU), Hazard.Radius - R.Speed_uu_s * In.EnemySeconds());
	Attacks[Hazard.Rec].DamageLength = Hazard.Radius;
	Attacks[Hazard.Rec].FootprintLength = Hazard.Radius; // drawn at the radius it damages from, every step
	const float Interval = FMath::Max(R.Interval_s, In.EnemySeconds());
	const int32 NumHeroes = FMath::Min(View.HeroPositions.Num(), Hazard.OutsideFor.Num());
	for (int32 Hero = 0; Hero < NumHeroes; ++Hero)
	{
		const FVector2f& At = View.HeroPositions[Hero];
		if (At.Size() <= Hazard.Radius)
		{
			Hazard.OutsideFor[Hero] = 0.0f;
			Hazard.TickAccum[Hero] = 0.0f;
			Hazard.bHitOnce[Hero] = false;
			continue;
		}
		Hazard.OutsideFor[Hero] += In.EnemySeconds();
		if (Hazard.OutsideFor[Hero] < DTConst::BOSS_HAZARD_GRACE_S - TimerEps)
		{
			continue; // a short crossing is forgiven
		}
		bool bHit = false;
		if (!Hazard.bHitOnce[Hero])
		{
			Hazard.bHitOnce[Hero] = true;
			Hazard.TickAccum[Hero] = 0.0f;
			bHit = true;
		}
		else
		{
			Hazard.TickAccum[Hero] += In.EnemySeconds();
			if (Hazard.TickAccum[Hero] >= Interval - TimerEps)
			{
				Hazard.TickAccum[Hero] -= Interval;
				bHit = true;
			}
		}
		if (bHit)
		{
			AddHeroHit(View, Hero, R.Damage, AttackId(Hazard.Row), At);
			++Counters.HazardHits;
		}
	}
}

void FDTBossCore::StepAdds(const FDTBossStepIn& In, IDTBossSink& Sink)
{
	using namespace DTBossCoreLocal;
	const int32 Row = RowOf(Phase, TEXT("Adds"));
	if (Row == INDEX_NONE || IsShifting())
	{
		return;
	}
	AddsTimer -= In.EnemySeconds();
	if (AddsTimer > TimerEps)
	{
		return;
	}
	const DTCat::FBossAttackRow& R = DTCat::BossWarden[Row];
	AddsTimer += FMath::Max(R.Interval_s, In.EnemySeconds());
	FDTEnemyPool& Pool = *In.Pool;
	const int32 Room = FMath::Max(0, DTConst::BOSS_ADDS_ALIVE_MAX - Pool.CountRole(EDTEnemyRole::Tank));
	const FVector2f Pos = HullPos(Pool);
	for (int32 Add = 0; Add < R.Count; ++Add)
	{
		// Always draw, so the Boss stream advances the same way whatever is alive (R-BOS-12).
		const float Angle = In.Rng ? In.Rng->FRandRange(0.0f, UE_TWO_PI) : 0.0f;
		if (Add >= Room)
		{
			continue;
		}
		FDTEnemySpawn Spawn;
		Spawn.Role = EDTEnemyRole::Tank;
		Spawn.Position = KeepInside(Pos + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Standoff());
		Sink.QueueSpawn(Spawn);
		++Counters.AddsQueued;
	}
}

void FDTBossCore::PublishTelegraphs(FDTWorldView& View, const FDTEnemyPool& Pool) const
{
	using namespace DTBossCoreLocal;
	auto Progress = [](float Left, float Total) { return Total > 0.0f ? FMath::Clamp(1.0f - Left / Total, 0.0f, 1.0f) : 1.0f; };
	if (Body.State == EBody::Windup || Body.State == EBody::Charge)
	{
		FDTTelegraph& T = View.Telegraphs.AddDefaulted_GetRef();
		T.Shape = EDTTelegraphShape::Band;
		T.Origin = Body.Start - Body.Dir * HullRadius();
		T.Dir = Body.Dir;
		T.Length = Body.Travel + 2.0f * HullRadius();
		T.Width = 2.0f * HullRadius();
		T.Progress = Body.State == EBody::Windup ? Progress(Body.Left, Body.Total) : 1.0f;
	}
	else if (Body.State == EBody::SweepWindup)
	{
		const DTCat::FBossAttackRow& R = DTCat::BossWarden[Body.Row];
		FDTTelegraph& T = View.Telegraphs.AddDefaulted_GetRef();
		T.Shape = EDTTelegraphShape::Arc;
		T.Origin = Body.Start;
		T.Dir = Body.Dir;
		T.Length = R.Extent_uu;
		T.Width = R.ArcDeg;
		T.Progress = Progress(Body.Left, Body.Total);
	}
	for (const FPendingRing& Ring : Rings)
	{
		const FDTBossRingShot& Shot = RingShots[Ring.Shot];
		FDTTelegraph& T = View.Telegraphs.AddDefaulted_GetRef();
		T.Shape = EDTTelegraphShape::RingGap;
		T.Origin = Shot.Origin;
		T.Dir = DirDeg(Shot.TelegraphLaneDeg);
		T.Length = static_cast<float>(DTConst::BOSS_LANE_MEASURE_UU);
		T.Width = Shot.TelegraphGapDeg;
		T.Progress = Progress(Ring.Left, Ring.Total);
	}
	if (Spiral.bOn)
	{
		FDTTelegraph& T = View.Telegraphs.AddDefaulted_GetRef();
		T.Shape = EDTTelegraphShape::SpiralLane;
		T.Origin = HullPos(Pool);
		T.Dir = DirDeg(Spiral.LaneDeg);
		T.Length = static_cast<float>(DTConst::BOSS_LANE_MEASURE_UU);
		T.Width = LaneAngleRequiredDeg();
		T.Progress = Progress(Spiral.WindLeft, Spiral.Total);
	}
	if (Hazard.bOn)
	{
		FDTTelegraph& T = View.Telegraphs.AddDefaulted_GetRef();
		T.Shape = EDTTelegraphShape::HazardRing;
		T.Origin = FVector2f::ZeroVector;
		T.Length = Hazard.Radius;
		T.Progress = Progress(Hazard.WindLeft, Hazard.Total);
	}
}
