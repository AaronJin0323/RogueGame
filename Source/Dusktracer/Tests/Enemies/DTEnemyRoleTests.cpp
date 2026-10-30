// S1 enemy role, aim-token and status tests (02_Systems/enemies.md T-ENM-09..14, T-ENM-18..22). Drive FDTEnemyPool
// directly with a hand-built view and run state; no world.

#include "Misc/AutomationTest.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyPool.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTEnemyRoleTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Hz = static_cast<float>(DTConst::SIM_FIXED_HZ);
	constexpr float Far = 3500.0f; // a hero this far is out of every trigger, range and contact

	/** A run, a view and a context wired together, with heroes at the given points. */
	struct FHarness
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTSimContext Context;
		int64 Step = 0;

		explicit FHarness(TArray<FVector2f> Heroes = { FVector2f::ZeroVector }, int32 Players = 1, int32 Seed = 1)
		{
			Run.Start(Seed, Players);
			View.HeroPositions = MoveTemp(Heroes);
			View.HeroIntangible.Init(false, View.HeroPositions.Num());
			Context.StepSeconds = 1.0f / Hz;
			Context.Run = &Run;
			Context.View = &View;
		}

		/** One full enemy step: Enemies then ResolveHits. */
		void Tick(FDTEnemyPool& Pool)
		{
			Context.StepIndex = Step++;
			Pool.StepEnemies(Context);
			Pool.ResolveHits(Context);
		}

		void Ticks(FDTEnemyPool& Pool, int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Tick(Pool);
			}
		}

		int32 CountHeroHits(EDTHeroHitKind Kind) const
		{
			int32 Count = 0;
			for (const FDTHeroHit& Hit : View.HeroHits)
			{
				Count += Hit.Kind == Kind ? 1 : 0;
			}
			return Count;
		}
	};

	static const DTCat::FEnemyRow* Row(EDTEnemyRole Role)
	{
		for (const DTCat::FEnemyRow& R : DTCat::Enemies)
		{
			if (R.Role == Role)
			{
				return &R;
			}
		}
		return nullptr;
	}

	static FDTHit MakeHit(const FDTEnemyPool& Pool, int32 Enemy, int32 Owner, float Damage, const FVector2f& At)
	{
		FDTHit Hit;
		Hit.EnemyIndex = Enemy;
		Hit.EnemySerial = Pool.GetSerials()[Enemy];
		Hit.OwnerId = Owner;
		Hit.Damage = Damage;
		Hit.Position = At;
		return Hit;
	}

	static FDTStatusApply MakeStatus(const FDTEnemyPool& Pool, int32 Enemy, EDTStatus Status, float Magnitude, int32 Owner = 0)
	{
		FDTStatusApply Apply;
		Apply.Handle = Pool.GetHandle(Enemy);
		Apply.Status = Status;
		Apply.Magnitude = Magnitude;
		Apply.OwnerId = Owner;
		return Apply;
	}

	static int32 Steps(float Seconds) { return FMath::RoundToInt32(Seconds * Hz); }

	static float AngleBetween(const FVector2f& A, const FVector2f& B)
	{
		return FMath::Abs(FMath::Atan2(A.X * B.Y - A.Y * B.X, FVector2f::DotProduct(A, B)));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmRoleDefs, "Dusktracer.Enemies.RoleDefs", DTEnemyRoleTest::Flags) // T-ENM-09
bool FDTEnmRoleDefs::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	FDTEnemyPool Pool;
	FRandomStream Rng(7);
	const float HpScale = 1.5f;
	const float DmgScale = 2.0f;
	for (const DTCat::FEnemyRow& R : DTCat::Enemies)
	{
		for (const bool bElite : { false, true })
		{
			FDTEnemySpawn Spec;
			Spec.Role = R.Role;
			Spec.Position = FVector2f(300.0f * Pool.Num(), 2000.0f);
			Spec.HpScale = HpScale;
			Spec.DmgScale = DmgScale;
			Spec.bElite = bElite;
			Spec.BeatId = 4;
			const int32 Index = Pool.Spawn(Spec, &Rng, true);
			if (!TestTrue(TEXT("spawned"), Index != INDEX_NONE))
			{
				return false;
			}
			const FString What = FString::Printf(TEXT("%s%s"), R.RoleTag, bElite ? TEXT(" elite") : TEXT(""));
			TestEqual(What + TEXT(" role"), Pool.GetRoles()[Index], R.Role);
			TestEqual(What + TEXT(" radius"), Pool.GetRadii()[Index], R.Radius_uu);
			TestEqual(What + TEXT(" speed"), Pool.GetSpeeds()[Index], R.Speed_uu_s);
			TestEqual(What + TEXT(" contact damage"), Pool.GetContactDamage()[Index], R.ContactDamage);
			const float Hp = R.Hp * HpScale * (bElite ? static_cast<float>(DTConst::ROLE_ELITE_HP_MULT) : 1.0f);
			TestEqual(What + TEXT(" HP = role x HpScale (x elite)"), Pool.GetMaxHp()[Index], Hp);
			TestEqual(What + TEXT(" full HP"), Pool.GetHp()[Index], Hp);
			TestEqual(What + TEXT(" DmgScale kept"), Pool.GetDmgScales()[Index], DmgScale);
			TestEqual(What + TEXT(" beat kept"), Pool.GetBeatIds()[Index], 4);
			TestTrue(What + TEXT(" director flag"), (Pool.GetFlags()[Index] & DTEnemyFlags::Director) != 0);
			TestEqual(What + TEXT(" elite flag"), (Pool.GetFlags()[Index] & DTEnemyFlags::Elite) != 0, bElite);
		}
	}
	const int32 Boss = Pool.Spawn(EDTEnemyRole::Boss, FVector2f(0.0f, -2000.0f));
	TestEqual(TEXT("boss hull radius"), Pool.GetRadii()[Boss], static_cast<float>(DTConst::BOSS_RADIUS_UU));
	TestEqual(TEXT("boss hull contact"), Pool.GetContactDamage()[Boss], static_cast<float>(DTConst::BOSS_CONTACT_DAMAGE));

	// Role machines advance only in the Enemies phase: ResolveHits alone leaves a Rusher in reach untouched.
	FHarness H({ FVector2f(500.0f, 0.0f) });
	FDTEnemyPool Rushers;
	Rushers.Spawn(EDTEnemyRole::Rusher, FVector2f::ZeroVector);
	for (int32 Step = 0; Step < Steps(2.0f); ++Step)
	{
		Rushers.ResolveHits(H.Context);
	}
	TestEqual(TEXT("no Enemies phase, no state change"), Rushers.GetAux(0).State, EDTEnemyState::Init);
	Rushers.StepEnemies(H.Context);
	TestEqual(TEXT("the Enemies phase starts the wind-up"), Rushers.GetAux(0).State, EDTEnemyState::WindUp);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmTankPlate, "Dusktracer.Enemies.TankPlate", DTEnemyRoleTest::Flags) // T-ENM-10
bool FDTEnmTankPlate::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	FDTRunState Run;
	Run.Start(1, 1);
	FDTEnemyPool Pool;
	const int32 Tank = Pool.Spawn(EDTEnemyRole::Tank, FVector2f::ZeroVector);
	Pool.SetFacing(Tank, FVector2f(1.0f, 0.0f));
	const float R = Pool.GetRadii()[Tank];
	const float Damage = 10.0f;
	const float Inside = FMath::DegreesToRadians(static_cast<float>(DTConst::ROLE_TANK_FRONT_HALF_ARC_DEG) - 5.0f);
	const float Outside = FMath::DegreesToRadians(static_cast<float>(DTConst::ROLE_TANK_FRONT_HALF_ARC_DEG) + 5.0f);

	auto Dealt = [&](const FVector2f& At, bool bPierce)
	{
		const float Before = Pool.GetHp()[Tank];
		FDTHit Hit = MakeHit(Pool, Tank, 0, Damage, At);
		Hit.bPierce = bPierce;
		Pool.ApplyHits(TArray<FDTHit>{ Hit }, Run);
		const float Taken = Before - Pool.GetHp()[Tank];
		Pool.SetHp(Tank, Pool.GetMaxHp()[Tank]);
		return Taken;
	};
	TestEqual(TEXT("head-on hit is reduced"), Dealt(FVector2f(R, 0.0f), false), Damage * DTConst::ROLE_TANK_FRONT_DAMAGE_MULT);
	TestEqual(TEXT("hit just inside the arc is reduced"), Dealt(FVector2f(FMath::Cos(Inside), FMath::Sin(Inside)) * R, false), Damage * DTConst::ROLE_TANK_FRONT_DAMAGE_MULT);
	TestEqual(TEXT("hit just outside the arc is full"), Dealt(FVector2f(FMath::Cos(Outside), -FMath::Sin(Outside)) * R, false), Damage);
	TestEqual(TEXT("hit from behind is full"), Dealt(FVector2f(-R, 0.0f), false), Damage);
	TestEqual(TEXT("a piercing round ignores the plate"), Dealt(FVector2f(R, 0.0f), true), Damage);

	// The facing turns toward a hero behind it, never faster than the limit.
	FHarness H({ FVector2f(-2000.0f, 10.0f) });
	FDTEnemyPool Turning;
	Turning.Spawn(EDTEnemyRole::Tank, FVector2f::ZeroVector);
	Turning.SetFacing(0, FVector2f(1.0f, 0.0f));
	const float MaxTurn = FMath::DegreesToRadians(static_cast<float>(DTConst::ROLE_TANK_TURN_DEG_S)) / Hz + 1.0e-4f;
	float WorstTurn = 0.0f;
	for (int32 Step = 0; Step < Steps(1.0f); ++Step)
	{
		const FVector2f Before = Turning.GetFacings()[0];
		H.Tick(Turning);
		WorstTurn = FMath::Max(WorstTurn, AngleBetween(Before, Turning.GetFacings()[0]));
	}
	TestTrue(FString::Printf(TEXT("turn per step %.5f <= limit %.5f"), WorstTurn, MaxTurn), WorstTurn <= MaxTurn);
	TestTrue(TEXT("the tank turned"), WorstTurn > 0.0f);
	TestTrue(TEXT("one second turns about the turn rate"), FMath::IsNearlyEqual(AngleBetween(FVector2f(1.0f, 0.0f), Turning.GetFacings()[0]),
		FMath::DegreesToRadians(static_cast<float>(DTConst::ROLE_TANK_TURN_DEG_S)), 0.02f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmRusherCharge, "Dusktracer.Enemies.RusherCharge", DTEnemyRoleTest::Flags) // T-ENM-11
bool FDTEnmRusherCharge::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	FHarness H({ FVector2f(600.0f, 0.0f) });
	FDTEnemyPool Pool;
	Pool.Spawn(EDTEnemyRole::Rusher, FVector2f::ZeroVector);

	TArray<EDTEnemyState> States;
	TArray<FVector2f> Positions;
	TArray<int32> Bands;
	FDTTelegraph Band;
	for (int32 Step = 0; Step < Steps(6.0f); ++Step)
	{
		if (Step == 5)
		{
			H.View.HeroPositions[0] = FVector2f(0.0f, 600.0f); // the hero moves after the wind-up started
		}
		H.Tick(Pool);
		States.Add(Pool.GetAux(0).State);
		Positions.Add(Pool.GetPositions()[0]);
		Bands.Add(H.View.Telegraphs.Num());
		if (H.View.Telegraphs.Num() == 1 && Step < Steps(DTConst::ROLE_RUSHER_WINDUP_S))
		{
			Band = H.View.Telegraphs[0]; // the first wind-up's band
		}
	}
	auto Run = [&](EDTEnemyState State, int32 From) { int32 N = 0; while (States.IsValidIndex(From + N) && States[From + N] == State) { ++N; } return N; };
	TestEqual(TEXT("wind-up starts at once in reach"), States[0], EDTEnemyState::WindUp);
	TestEqual(TEXT("wind-up lasts its duration"), Run(EDTEnemyState::WindUp, 0), Steps(DTConst::ROLE_RUSHER_WINDUP_S));
	const int32 ChargeStart = Steps(DTConst::ROLE_RUSHER_WINDUP_S);
	for (int32 Step = 1; Step < ChargeStart; ++Step)
	{
		TestEqual(TEXT("still during the wind-up"), Positions[Step], Positions[0]);
	}
	int32 Moving = 0;
	for (int32 Step = ChargeStart; Step < Positions.Num() && Positions[Step] != Positions[Step - 1]; ++Step)
	{
		++Moving;
	}
	TestEqual(TEXT("charge lasts its duration"), Moving, Steps(DTConst::ROLE_RUSHER_CHARGE_S));
	const int32 RecoverStart = ChargeStart + Moving - 1;
	TestEqual(TEXT("recover after the charge"), States[RecoverStart], EDTEnemyState::Recover);
	TestEqual(TEXT("recover lasts its duration"), Run(EDTEnemyState::Recover, RecoverStart), Steps(DTConst::ROLE_RUSHER_RECOVER_S));
	const int32 ApproachStart = RecoverStart + Steps(DTConst::ROLE_RUSHER_RECOVER_S);
	TestEqual(TEXT("then approach"), States[ApproachStart], EDTEnemyState::Approach);
	int32 NextWindUp = INDEX_NONE;
	for (int32 Step = ApproachStart; Step < States.Num() && NextWindUp == INDEX_NONE; ++Step)
	{
		NextWindUp = States[Step] == EDTEnemyState::WindUp ? Step : INDEX_NONE;
	}
	TestTrue(TEXT("a second wind-up comes"), NextWindUp != INDEX_NONE);
	TestTrue(TEXT("not before the cooldown"), NextWindUp - ApproachStart >= Steps(DTConst::ROLE_RUSHER_COOLDOWN_S));

	const FVector2f End = Positions[ChargeStart + Moving - 1];
	const float Expected = DTConst::ROLE_RUSHER_CHARGE_SPEED_UU_S * DTConst::ROLE_RUSHER_CHARGE_S;
	TestTrue(TEXT("charge length = speed x duration"), FMath::IsNearlyEqual(FVector2f::Distance(End, Positions[0]), Expected, 0.5f));
	TestEqual(TEXT("band shape"), Band.Shape, EDTTelegraphShape::Band);
	TestTrue(TEXT("band length = charge"), FMath::IsNearlyEqual(Band.Length, FVector2f::Distance(End, Positions[0]), 0.5f));
	TestEqual(TEXT("band width = collision width"), Band.Width, 2.0f * static_cast<float>(DTConst::ROLE_RUSHER_RADIUS_UU));
	TestTrue(TEXT("band starts at the Rusher"), Band.Origin.Equals(Positions[0], 1.0e-3f));
	TestTrue(TEXT("direction locked at wind-up start (+X), not the moved hero"), Band.Dir.Equals(FVector2f(1.0f, 0.0f), 1.0e-4f) && FMath::Abs(End.Y) < 1.0e-2f);
	TestTrue(TEXT("the band is on the ground for the whole wind-up"), Bands[0] == 1 && Bands[ChargeStart - 1] == 1 && Bands[ChargeStart] == 0);
	TestTrue(TEXT("a threat action per charge"), Pool.GetCounters().ThreatActions[static_cast<int32>(EDTEnemyRole::Rusher)] >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmTurretBurst, "Dusktracer.Enemies.TurretBurst", DTEnemyRoleTest::Flags) // T-ENM-12
bool FDTEnmTurretBurst::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	{
		FHarness H({ FVector2f(static_cast<float>(DTConst::ROLE_TURRET_FIRE_RANGE_UU) + 100.0f, 0.0f) });
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Turret, FVector2f::ZeroVector);
		bool bTold = false;
		for (int32 Step = 0; Step < Steps(5.0f); ++Step)
		{
			H.Tick(Pool);
			bTold |= Pool.GetAux(0).State != EDTEnemyState::Idle;
		}
		TestFalse(TEXT("no tell beyond range"), bTold);
		TestEqual(TEXT("no fire beyond range"), Pool.GetRounds().GetCounters().Spawned, static_cast<int64>(0));
	}

	FHarness H({ FVector2f(1000.0f, 0.0f) });
	FDTEnemyPool Pool;
	Pool.Spawn(EDTEnemyRole::Turret, FVector2f::ZeroVector);
	Pool.SetPattern(0, EDTFirePattern::Burst);
	Pool.SetFacing(0, FVector2f(1.0f, 0.0f));
	TArray<int32> TellStarts;
	TArray<int32> FireSteps;
	EDTEnemyState Last = EDTEnemyState::Idle;
	int64 Spawned = 0;
	bool bAlongAim = true;
	for (int32 Step = 0; Step < Steps(6.0f); ++Step) // ends between bursts
	{
		H.Tick(Pool);
		const EDTEnemyState Now = Pool.GetAux(0).State;
		if (Now == EDTEnemyState::Tell && Last != EDTEnemyState::Tell)
		{
			TellStarts.Add(Step);
		}
		Last = Now;
		const int64 Total = Pool.GetRounds().GetCounters().Spawned;
		if (Total > Spawned)
		{
			FireSteps.Add(Step);
			for (const FVector2f& V : Pool.GetRounds().GetVelocities())
			{
				bAlongAim &= V.GetSafeNormal().Equals(Pool.GetAux(0).AimDir, 1.0e-4f) && Pool.GetAux(0).AimDir.Equals(FVector2f(1.0f, 0.0f), 1.0e-3f);
			}
			Spawned = Total;
		}
	}
	TestTrue(TEXT("several tells"), TellStarts.Num() >= 3);
	for (int32 Index = 1; Index < TellStarts.Num(); ++Index)
	{
		TestTrue(TEXT("tell starts at least the period apart"), TellStarts[Index] - TellStarts[Index - 1] >= Steps(DTConst::ROLE_TURRET_PERIOD_S));
	}
	TestTrue(TEXT("first round after the full tell"), FireSteps.Num() > 0 && FireSteps[0] - TellStarts[0] >= Steps(DTConst::ROLE_TURRET_TELL_S));
	TestTrue(TEXT("rounds fly along the locked aim"), bAlongAim);
	TestEqual(TEXT("a burst per tell"), Pool.GetRounds().GetCounters().Spawned,
		static_cast<int64>(DTConst::ENEMY_FIRE_BURST_SHOTS) * Pool.GetCounters().ThreatActions[static_cast<int32>(EDTEnemyRole::Turret)]);
	TestTrue(TEXT("the rounds reach the hero"), Pool.GetRounds().GetCounters().HeroHits > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmTelegraphLead, "Dusktracer.Enemies.TelegraphLead", DTEnemyRoleTest::Flags) // T-ENM-13
bool FDTEnmTelegraphLead::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	const int32 Lead = FMath::CeilToInt32(DTConst::ENEMIES_TELEGRAPH_LEAD_MIN_S * Hz - 1.0e-3f);

	for (const DTCat::FEnemyRow& R : DTCat::Enemies)
	{
		if (FCString::Strlen(R.Attack) == 0)
		{
			TestEqual(FString::Printf(TEXT("%s has no telegraph without an attack"), R.RoleTag), FCString::Strlen(R.Telegraph), 0);
			continue;
		}
		if (R.Role == EDTEnemyRole::Rusher)
		{
			TestEqual(TEXT("Rusher telegraph is a band"), FString(R.Telegraph), FString(TEXT("Band")));
			FHarness H({ FVector2f(600.0f, 0.0f) });
			FDTEnemyPool Pool;
			Pool.Spawn(EDTEnemyRole::Rusher, FVector2f::ZeroVector);
			int32 FirstTelegraph = INDEX_NONE;
			int32 FirstMove = INDEX_NONE;
			FDTTelegraph Band;
			for (int32 Step = 0; Step < Steps(2.0f); ++Step)
			{
				const FVector2f Before = Pool.GetPositions()[0];
				H.Tick(Pool);
				if (H.View.Telegraphs.Num() > 0 && FirstTelegraph == INDEX_NONE)
				{
					FirstTelegraph = Step;
					Band = H.View.Telegraphs[0];
				}
				if (FirstMove == INDEX_NONE && Pool.GetPositions()[0] != Before)
				{
					FirstMove = Step;
				}
			}
			TestTrue(TEXT("Rusher: band at least the minimum lead before the charge"), FirstTelegraph != INDEX_NONE && FirstMove - FirstTelegraph >= Lead);
			TestEqual(TEXT("Rusher: band shape"), Band.Shape, EDTTelegraphShape::Band);
		}
		else if (R.Role == EDTEnemyRole::Turret)
		{
			TestEqual(TEXT("Turret telegraph is an aim line"), FString(R.Telegraph), FString(TEXT("AimLine")));
			for (const EDTFirePattern Pattern : { EDTFirePattern::Burst, EDTFirePattern::Fan })
			{
				FHarness H({ FVector2f(1000.0f, 0.0f) });
				FDTEnemyPool Pool;
				Pool.Spawn(EDTEnemyRole::Turret, FVector2f::ZeroVector);
				Pool.SetPattern(0, Pattern);
				Pool.SetFacing(0, FVector2f(1.0f, 0.0f));
				int32 FirstTelegraph = INDEX_NONE;
				int32 FirstRound = INDEX_NONE;
				TArray<FDTTelegraph> Lines;
				for (int32 Step = 0; Step < Steps(2.0f) && FirstRound == INDEX_NONE; ++Step)
				{
					H.Tick(Pool);
					if (H.View.Telegraphs.Num() > 0)
					{
						FirstTelegraph = FirstTelegraph == INDEX_NONE ? Step : FirstTelegraph;
						Lines = H.View.Telegraphs;
					}
					if (Pool.GetRounds().Num() > 0)
					{
						FirstRound = Step;
					}
				}
				// Footprint: every round of the burst flies along one of the lines shown, at their width.
				TArray<FVector2f> Dirs;
				bool bCovered = Lines.Num() > 0;
				for (int32 Volley = 0; Volley < DTConst::ENEMY_FIRE_BURST_SHOTS; ++Volley)
				{
					FDTEnemyPool::VolleyDirections(FVector2f(1.0f, 0.0f), Pattern, false, Volley, Dirs);
					for (const FVector2f& D : Dirs)
					{
						bCovered &= Lines.ContainsByPredicate([&D](const FDTTelegraph& T)
						{
							return T.Shape == EDTTelegraphShape::AimLine && T.Dir.Equals(D, 1.0e-3f)
								&& T.Width == 2.0f * DTConst::ENEMY_FIRE_SHOT_RADIUS_UU;
						});
					}
				}
				TestTrue(TEXT("Turret: aim line at least the minimum lead before the first round"), FirstTelegraph != INDEX_NONE && FirstRound - FirstTelegraph >= Lead);
				TestTrue(TEXT("Turret: the lines are the rounds' paths"), bCovered);
			}
		}
		else
		{
			AddError(FString::Printf(TEXT("attack %s has no telegraph test"), R.Attack));
		}
	}

	// Arrivals start their attack on their first step.
	{
		FHarness H({ FVector2f(1500.0f, 0.0f) });
		FDTEnemyPool Pool;
		FDTEnemySpawn Spec;
		Spec.Role = EDTEnemyRole::Rusher;
		Spec.bArrival = true;
		Pool.Spawn(Spec, nullptr, true);
		Spec.Role = EDTEnemyRole::Turret;
		Spec.Position = FVector2f(-1000.0f, 0.0f); // beyond fire range of the hero: an arrival tells anyway
		Pool.Spawn(Spec, nullptr, true);
		H.Tick(Pool);
		TestEqual(TEXT("arriving Rusher is in its wind-up"), Pool.GetAux(0).State, EDTEnemyState::WindUp);
		TestTrue(TEXT("aimed at its nearest hero"), Pool.GetAux(0).AimDir.Equals(FVector2f(1.0f, 0.0f), 1.0e-4f));
		TestEqual(TEXT("arriving Turret is in its tell"), Pool.GetAux(1).State, EDTEnemyState::Tell);
		TestTrue(TEXT("both telegraphs show"), H.View.Telegraphs.Num() >= 2);
	}

	// Killed mid-telegraph, an arrival still lands its attack.
	{
		FHarness H({ FVector2f(500.0f, 0.0f) });
		FDTEnemyPool Pool;
		FDTEnemySpawn Spec;
		Spec.Role = EDTEnemyRole::Rusher;
		Spec.bArrival = true;
		Spec.DmgScale = 2.0f;
		Pool.Spawn(Spec, nullptr, true);
		const int32 KillAt = 10;
		int32 Hazards = 0;
		int32 HazardStep = INDEX_NONE;
		for (int32 Step = 0; Step < Steps(2.0f); ++Step)
		{
			if (Step == KillAt)
			{
				H.View.Hits.Add(MakeHit(Pool, 0, 0, 1.0e6f, Pool.GetPositions()[0]));
			}
			H.Tick(Pool);
			if (Step > KillAt && Step < Steps(DTConst::ROLE_RUSHER_WINDUP_S))
			{
				TestEqual(TEXT("the band stays after the death"), H.View.Telegraphs.Num(), 1);
			}
			for (const FDTHeroHit& Hit : H.View.HeroHits)
			{
				if (Hit.Kind == EDTHeroHitKind::Hazard)
				{
					++Hazards;
					HazardStep = Step;
					TestEqual(TEXT("hazard damage = contact x DmgScale"), Hit.Damage, DTConst::ROLE_RUSHER_CONTACT_DAMAGE * 2.0f);
				}
			}
		}
		TestEqual(TEXT("the Rusher died"), Pool.Num(), 0);
		TestEqual(TEXT("its band lands once"), Hazards, 1);
		TestEqual(TEXT("at the step the charge would have started"), HazardStep, Steps(DTConst::ROLE_RUSHER_WINDUP_S));
	}
	{
		FHarness H({ FVector2f(1000.0f, 0.0f) });
		FDTEnemyPool Pool;
		FDTEnemySpawn Spec;
		Spec.Role = EDTEnemyRole::Turret;
		Spec.bArrival = true;
		Pool.Spawn(Spec, nullptr, true);
		Pool.SetPattern(0, EDTFirePattern::Burst);
		Pool.SetFacing(0, FVector2f(1.0f, 0.0f));
		for (int32 Step = 0; Step < Steps(1.5f); ++Step)
		{
			if (Step == 5)
			{
				H.View.Hits.Add(MakeHit(Pool, 0, 0, 1.0e6f, Pool.GetPositions()[0]));
			}
			H.Tick(Pool);
		}
		TestEqual(TEXT("the Turret died"), Pool.Num(), 0);
		TestEqual(TEXT("its burst still fired"), Pool.GetRounds().GetCounters().Spawned, static_cast<int64>(DTConst::ENEMY_FIRE_BURST_SHOTS));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmContact, "Dusktracer.Enemies.Contact", DTEnemyRoleTest::Flags) // T-ENM-14
bool FDTEnmContact::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	FHarness H({ FVector2f::ZeroVector });
	FDTEnemyPool Pool;
	Pool.SetFireEnabled(false);
	FDTEnemySpawn Spec;
	Spec.Role = EDTEnemyRole::Turret; // static, so the overlap holds
	Spec.Position = FVector2f(50.0f, 0.0f);
	Spec.DmgScale = 1.5f;
	Pool.Spawn(Spec, nullptr, false);
	Pool.Spawn(EDTEnemyRole::Turret, FVector2f(1000.0f, 1000.0f)); // never touches the hero
	for (int32 Step = 0; Step < 3; ++Step)
	{
		H.Tick(Pool);
		TestEqual(TEXT("one contact hit per step from the overlapping enemy"), H.CountHeroHits(EDTHeroHitKind::Contact), 1);
		if (H.View.HeroHits.Num() == 1)
		{
			const FDTHeroHit& Hit = H.View.HeroHits[0];
			TestEqual(TEXT("to the touched player"), Hit.PlayerId, 0);
			TestEqual(TEXT("role contact damage x DmgScale"), Hit.Damage, DTConst::ROLE_TURRET_CONTACT_DAMAGE * 1.5f);
			TestEqual(TEXT("source role"), Hit.SourceRole, EDTEnemyRole::Turret);
			TestEqual(TEXT("source serial"), Hit.EnemySerial, Pool.GetSerials()[0]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmAimTokens, "Dusktracer.Enemies.AimTokens", DTEnemyRoleTest::Flags) // T-ENM-18
bool FDTEnmAimTokens::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	for (const bool bThreat : { false, true })
	{
		FHarness H({ FVector2f::ZeroVector });
		H.View.bThreatBlock = bThreat;
		FDTEnemyPool Pool;
		FRandomStream Rng(3);
		const int32 Count = 12;
		Pool.SpawnRing(EDTEnemyRole::Turret, Count, FVector2f::ZeroVector, 1000.0f, Rng);
		const int32 Limit = bThreat ? DTConst::AIM_TOKENS_THREAT : DTConst::AIM_TOKENS;
		int32 Peak = 0;
		int32 Untokened = 0;
		for (int32 Step = 0; Step < Steps(20.0f); ++Step)
		{
			H.Tick(Pool);
			int32 Active = 0;
			for (int32 Index = 0; Index < Pool.Num(); ++Index)
			{
				const FDTEnemyAux& A = Pool.GetAux(Index);
				const bool bActive = A.State == EDTEnemyState::Tell || A.State == EDTEnemyState::Fire;
				Active += bActive ? 1 : 0;
				Untokened += bActive && !A.bToken ? 1 : 0;
			}
			Peak = FMath::Max(Peak, Active);
		}
		const FString Tag = bThreat ? TEXT("threat block: ") : TEXT("");
		TestTrue(Tag + FString::Printf(TEXT("never more than %d in tell or firing (peak %d)"), Limit, Peak), Peak <= Limit);
		TestEqual(Tag + TEXT("the token count is used"), Peak, Limit);
		TestEqual(Tag + TEXT("no unit tells or fires without a token"), Untokened, 0);
		TestTrue(Tag + TEXT("units without a token held"), Pool.GetCounters().TokenWaits > 0);
		int32 Fired = 0;
		for (int32 Index = 0; Index < Pool.Num(); ++Index)
		{
			Fired += Pool.GetAux(Index).LastFireStep >= 0 ? 1 : 0;
		}
		TestEqual(Tag + TEXT("every turret still fires"), Fired, Count);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmNoCloseFan, "Dusktracer.Enemies.NoCloseFan", DTEnemyRoleTest::Flags) // T-ENM-19
bool FDTEnmNoCloseFan::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	for (const bool bClose : { true, false })
	{
		const float Range = bClose ? DTConst::AIM_FAN_MIN_RANGE_UU * 0.5f : DTConst::AIM_FAN_MIN_RANGE_UU * 1.5f;
		FHarness H({ FVector2f(Range, 0.0f) });
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Turret, FVector2f::ZeroVector);
		Pool.SetPattern(0, EDTFirePattern::Fan);
		Pool.SetFacing(0, FVector2f(1.0f, 0.0f));
		for (int32 Step = 0; Step < Steps(1.5f); ++Step)
		{
			H.Tick(Pool);
		}
		const int64 PerVolley = bClose ? 1 : DTConst::ENEMY_FIRE_FAN_COUNT;
		TestEqual(bClose ? TEXT("inside the close range: single aimed rounds") : TEXT("outside it: a fan"),
			Pool.GetRounds().GetCounters().Spawned, PerVolley * DTConst::ENEMY_FIRE_BURST_SHOTS);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmAimedFans, "Dusktracer.Enemies.AimedFansHitStationary", DTEnemyRoleTest::Flags) // T-ENM-20
bool FDTEnmAimedFans::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	const float Allowed = static_cast<float>(DTConst::HERO_RADIUS_UU + DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
	const float Spread = static_cast<float>(DTConst::ENEMY_FIRE_FAN_SPREAD_DEG);
	TArray<FVector2f> Dirs;
	TArray<FVector2f> Other;
	float Worst = 0.0f;
	for (int32 Count = 1; Count <= DTConst::AIM_FAN_TEST_COUNT_MAX; ++Count)
	{
		for (int32 Volley = 0; Volley < 2; ++Volley) // both sides of an even fan
		{
			for (int32 Bearing = 0; Bearing < 8; ++Bearing)
			{
				const float A = UE_TWO_PI * Bearing / 8.0f + 0.1f;
				const FVector2f Aim(FMath::Cos(A), FMath::Sin(A));
				FDTEnemyPool::FanDirections(Aim, Count, Spread, Volley, Dirs);
				TestEqual(TEXT("one direction per round"), Dirs.Num(), Count);
				for (float Range = DTConst::AIM_TEST_RANGE_UU_MIN; Range <= DTConst::AIM_TEST_RANGE_UU_MAX; Range += 100.0f)
				{
					const FVector2f Hero = Aim * Range;
					float Miss = TNumericLimits<float>::Max();
					for (const FVector2f& D : Dirs)
					{
						const float Along = FVector2f::DotProduct(Hero, D);
						if (Along > 0.0f)
						{
							Miss = FMath::Min(Miss, FMath::Abs(Hero.X * D.Y - Hero.Y * D.X));
						}
					}
					Worst = FMath::Max(Worst, Miss);
				}
			}
			if (Count % 2 == 0)
			{
				// Alternating sides: the next volley is the mirror image about the aim, so the burst keeps its width.
				FDTEnemyPool::FanDirections(FVector2f(1.0f, 0.0f), Count, Spread, Volley, Dirs);
				FDTEnemyPool::FanDirections(FVector2f(1.0f, 0.0f), Count, Spread, Volley + 1, Other);
				bool bMirror = true;
				for (const FVector2f& D : Dirs)
				{
					bMirror &= Other.ContainsByPredicate([&D](const FVector2f& E) { return E.Equals(FVector2f(D.X, -D.Y), 1.0e-4f); });
				}
				TestTrue(FString::Printf(TEXT("even fan of %d alternates sides"), Count), bMirror);
			}
		}
	}
	TestTrue(FString::Printf(TEXT("nearest round's miss %.3f within hero + round radius %.1f"), Worst, Allowed), Worst <= Allowed);

	// End to end: a fan turret against a stationary hero lands a round every volley.
	FHarness H({ FVector2f(1000.0f, 300.0f) });
	FDTEnemyPool Pool;
	Pool.Spawn(EDTEnemyRole::Turret, FVector2f::ZeroVector);
	Pool.SetPattern(0, EDTFirePattern::Fan);
	int32 BulletHits = 0;
	for (int32 Step = 0; Step < Steps(4.0f); ++Step)
	{
		H.Tick(Pool);
		BulletHits += H.CountHeroHits(EDTHeroHitKind::Bullet);
	}
	TestTrue(TEXT("every volley puts a round on the stationary hero"), BulletHits >= DTConst::ENEMY_FIRE_BURST_SHOTS);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmStatusApply, "Dusktracer.Enemies.StatusApply", DTEnemyRoleTest::Flags) // T-ENM-21
bool FDTEnmStatusApply::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	FHarness H({ FVector2f(Far, Far) });
	FDTEnemyPool Pool;
	const int32 E = Pool.Spawn(EDTEnemyRole::Tank, FVector2f::ZeroVector);
	const uint8 ShockBit = 1 << static_cast<int32>(EDTStatus::Shock);

	// Shock: lasts its duration; a new one refreshes, never stacks.
	TestTrue(TEXT("shock applies"), Pool.ApplyStatus(MakeStatus(Pool, E, EDTStatus::Shock, 0.0f)));
	TestTrue(TEXT("shocked"), (Pool.GetStatusMask(E) & ShockBit) != 0);
	H.Ticks(Pool, Steps(DTConst::STATUS_SHOCK_S * 0.5f));
	Pool.ApplyStatus(MakeStatus(Pool, E, EDTStatus::Shock, 0.0f));
	TestEqual(TEXT("a new shock refreshes"), Pool.GetAux(E).ShockLeft, DTConst::STATUS_SHOCK_S);
	H.Ticks(Pool, Steps(DTConst::STATUS_SHOCK_S));
	TestEqual(TEXT("shock ends after its duration"), Pool.GetStatusMask(E) & ShockBit, 0);

	// Burn: one stack per application up to the cap; past it the oldest is replaced.
	for (int32 Stack = 1; Stack <= DTConst::STATUS_BURN_STACKS_MAX + 1; ++Stack)
	{
		Pool.ApplyStatus(MakeStatus(Pool, E, EDTStatus::Burn, 0.01f * Stack));
	}
	TestEqual(TEXT("burn stacks to its cap"), Pool.GetAux(E).NumBurnStacks(), DTConst::STATUS_BURN_STACKS_MAX);
	bool bOldestGone = true;
	for (const FDTBurnStack& Stack : Pool.GetAux(E).Burn)
	{
		bOldestGone &= !(Stack.Left > 0.0f && FMath::IsNearlyEqual(Stack.Dps, 0.01f));
	}
	TestTrue(TEXT("the oldest stack was replaced"), bOldestGone);
	FDTStatusApply Bonus = MakeStatus(Pool, E, EDTStatus::Burn, 0.01f);
	Bonus.StackBonus = 1;
	Pool.ApplyStatus(Bonus);
	TestEqual(TEXT("a stack bonus raises the cap"), Pool.GetAux(E).NumBurnStacks(), FMath::Min(DTConst::STATUS_BURN_STACKS_MAX + 1, DTConst::STATUS_BURN_STACK_SLOTS));
	Bonus.StackBonus = 100;
	for (int32 Stack = 0; Stack < DTConst::STATUS_BURN_STACK_SLOTS + 2; ++Stack)
	{
		Pool.ApplyStatus(Bonus);
	}
	TestEqual(TEXT("never past the slots"), Pool.GetAux(E).NumBurnStacks(), DTConst::STATUS_BURN_STACK_SLOTS);
	H.Ticks(Pool, Steps(DTConst::STATUS_BURN_S) + 1);
	TestEqual(TEXT("burn stacks expire"), Pool.GetAux(E).NumBurnStacks(), 0);

	// Chill: stacks reach Freeze (default threshold) and are consumed; Chill is refused while frozen.
	Pool.ApplyStatus(MakeStatus(Pool, E, EDTStatus::Chill, 1.0f));
	TestEqual(TEXT("one chill stack"), Pool.GetAux(E).ChillStacks, 1);
	Pool.ApplyStatus(MakeStatus(Pool, E, EDTStatus::Chill, static_cast<float>(DTConst::STATUS_FREEZE_STACKS - 1)));
	TestTrue(TEXT("threshold reached: frozen"), Pool.GetAux(E).IsFrozen());
	TestEqual(TEXT("stacks consumed"), Pool.GetAux(E).ChillStacks, 0);
	TestEqual(TEXT("freeze duration"), Pool.GetAux(E).FreezeLeft, DTConst::STATUS_FREEZE_S);
	TestFalse(TEXT("chill refused while frozen"), Pool.ApplyStatus(MakeStatus(Pool, E, EDTStatus::Chill, 1.0f)));
	TestEqual(TEXT("no stack while frozen"), Pool.GetAux(E).ChillStacks, 0);
	H.Ticks(Pool, Steps(DTConst::STATUS_FREEZE_S) + 1);
	TestFalse(TEXT("freeze ends"), Pool.GetAux(E).IsFrozen());

	// Lowered threshold and scaled freeze.
	FDTStatusApply Low = MakeStatus(Pool, E, EDTStatus::Chill, 2.0f);
	Low.FreezeAt = 2;
	Low.FreezeScale = 2.0f;
	Pool.ApplyStatus(Low);
	TestTrue(TEXT("a lowered threshold freezes"), Pool.GetAux(E).IsFrozen());
	TestEqual(TEXT("freeze scale"), Pool.GetAux(E).FreezeLeft, 2.0f * DTConst::STATUS_FREEZE_S);
	H.Ticks(Pool, Steps(2.0f * DTConst::STATUS_FREEZE_S) + 1);

	// Chill alone wears off.
	Pool.ApplyStatus(MakeStatus(Pool, E, EDTStatus::Chill, 1.0f));
	H.Ticks(Pool, Steps(DTConst::STATUS_CHILL_S) + 1);
	TestEqual(TEXT("chill expires"), Pool.GetAux(E).ChillStacks, 0);

	// Stale handles are ignored, also through the view.
	FDTStatusApply Stale = MakeStatus(Pool, E, EDTStatus::Shock, 0.0f);
	Stale.Handle.Serial += 1000;
	TestFalse(TEXT("stale handle ignored"), Pool.ApplyStatus(Stale));
	H.View.StatusApplies.Add(Stale);
	H.Tick(Pool);
	TestEqual(TEXT("stale view application ignored"), Pool.GetStatusMask(E) & ShockBit, 0);
	TestTrue(TEXT("applications cleared after applying"), H.View.StatusApplies.IsEmpty());
	H.View.StatusApplies.Add(MakeStatus(Pool, E, EDTStatus::Shock, 0.0f));
	H.Tick(Pool);
	TestTrue(TEXT("a valid view application applies next Enemies phase"), (Pool.GetStatusMask(E) & ShockBit) != 0);

	// The boss hull (boss.md R-BOS-11): Freeze refused, Chill capped, never frozen.
	const int32 Boss = Pool.Spawn(EDTEnemyRole::Boss, FVector2f(-2000.0f, 0.0f));
	TestFalse(TEXT("boss refuses freeze"), Pool.ApplyStatus(MakeStatus(Pool, Boss, EDTStatus::Freeze, 0.0f)));
	Pool.ApplyStatus(MakeStatus(Pool, Boss, EDTStatus::Chill, 10.0f));
	TestEqual(TEXT("boss chill capped"), Pool.GetAux(Boss).ChillStacks, DTConst::STATUS_BOSS_CHILL_STACK_MAX);
	TestFalse(TEXT("boss never frozen"), Pool.GetAux(Boss).IsFrozen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmStatusEffect, "Dusktracer.Enemies.StatusEffect", DTEnemyRoleTest::Flags) // T-ENM-22
bool FDTEnmStatusEffect::RunTest(const FString& Parameters)
{
	using namespace DTEnemyRoleTest;
	// Shock raises damage taken.
	{
		FDTRunState Run;
		Run.Start(1, 1);
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Tank, FVector2f::ZeroVector);
		Pool.SetFacing(0, FVector2f(1.0f, 0.0f));
		Pool.ApplyStatus(MakeStatus(Pool, 0, EDTStatus::Shock, 0.0f));
		const float Before = Pool.GetHp()[0];
		Pool.ApplyHits(TArray<FDTHit>{ MakeHit(Pool, 0, 0, 10.0f, FVector2f(-100.0f, 0.0f)) }, Run);
		TestTrue(TEXT("shocked enemy takes more damage"), FMath::IsNearlyEqual(Before - Pool.GetHp()[0], 10.0f * (1.0f + DTConst::STATUS_SHOCK_DAMAGE_TAKEN), 1.0e-4f));
	}
	// A full Burn deals damage per second times duration, as hits carrying the burner's owner and source.
	{
		FHarness H({ FVector2f(Far, Far) }, 2);
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Tank, FVector2f::ZeroVector);
		const float Dps = 4.0f;
		FDTStatusApply Burn = MakeStatus(Pool, 0, EDTStatus::Burn, Dps, 1);
		Burn.SourceId = 7;
		H.View.StatusApplies.Add(Burn);
		const float Before = Pool.GetHp()[0];
		int32 Ticks = 0;
		bool bOwned = true;
		for (int32 Step = 0; Step < Steps(DTConst::STATUS_BURN_S) + 5; ++Step)
		{
			H.Context.StepIndex = H.Step++;
			Pool.StepEnemies(H.Context);
			for (const FDTHit& Hit : H.View.Hits)
			{
				++Ticks;
				bOwned &= Hit.OwnerId == 1 && Hit.SourceId == 7;
			}
			Pool.ResolveHits(H.Context);
		}
		TestTrue(FString::Printf(TEXT("full burn = dps x duration (dealt %.4f)"), Before - Pool.GetHp()[0]),
			FMath::IsNearlyEqual(Before - Pool.GetHp()[0], Dps * DTConst::STATUS_BURN_S, 1.0e-3f));
		TestEqual(TEXT("one hit per tick"), Ticks, FMath::RoundToInt32(DTConst::STATUS_BURN_S / DTConst::STATUS_BURN_TICK_S));
		TestTrue(TEXT("ticks carry the burner's owner and source"), bOwned);
		TestEqual(TEXT("the damage is the burner's"), H.Run.GetPlayer(1).DamageDealt, static_cast<int64>(FMath::RoundToInt32(Dps * DTConst::STATUS_BURN_S)));
	}
	// A Burn kill goes to the burner.
	{
		FHarness H({ FVector2f(Far, Far) }, 2);
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Swarm, FVector2f::ZeroVector);
		FDTStatusApply Burn = MakeStatus(Pool, 0, EDTStatus::Burn, DTConst::ENM_SWARM_HP * 10.0f, 1);
		Burn.SourceId = 9;
		H.View.StatusApplies.Add(Burn);
		int32 KillsSeen = 0;
		for (int32 Step = 0; Step < Steps(1.0f); ++Step)
		{
			H.Tick(Pool);
			for (const FDTKill& Kill : H.View.Kills)
			{
				++KillsSeen;
				TestEqual(TEXT("kill owner = burner"), Kill.OwnerId, 1);
				TestEqual(TEXT("kill source = burn source"), static_cast<int32>(Kill.SourceId), 9);
			}
		}
		TestEqual(TEXT("one kill"), KillsSeen, 1);
		TestEqual(TEXT("kill event to the burner"), H.Run.GetPlayer(1).Kills, 1);
		TestEqual(TEXT("none to the other player"), H.Run.GetPlayer(0).Kills, 0);
	}
	// Chill slows movement per stack.
	{
		FHarness H({ FVector2f(0.0f, Far) });
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(-1500.0f, 0.0f));
		Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(1500.0f, 0.0f));
		Pool.ApplyStatus(MakeStatus(Pool, 1, EDTStatus::Chill, 2.0f));
		H.Ticks(Pool, Steps(1.0f));
		const float Normal = Pool.GetVelocities()[0].Size();
		const float Chilled = Pool.GetVelocities()[1].Size();
		const float Expected = DTConst::ENM_SWARM_SPEED_UU_S * (1.0f - 2.0f * DTConst::STATUS_CHILL_SLOW_PER_STACK);
		TestTrue(TEXT("unchilled at full speed"), FMath::IsNearlyEqual(Normal, static_cast<float>(DTConst::ENM_SWARM_SPEED_UU_S), 1.0f));
		TestTrue(FString::Printf(TEXT("two stacks slow to %.1f (got %.1f)"), Expected, Chilled), FMath::IsNearlyEqual(Chilled, Expected, 1.0f));
	}
	// Freeze stops movement and pauses a wind-up; contact damage is unchanged.
	{
		FHarness H({ FVector2f(500.0f, 0.0f) });
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Rusher, FVector2f::ZeroVector);
		Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(-1500.0f, 0.0f));
		H.Ticks(Pool, 10);
		TestEqual(TEXT("wind-up started"), Pool.GetAux(0).State, EDTEnemyState::WindUp);
		const float WindLeft = Pool.GetAux(0).StateLeft;
		const FVector2f SwarmAt = Pool.GetPositions()[1];
		Pool.ApplyStatus(MakeStatus(Pool, 0, EDTStatus::Freeze, 0.0f));
		Pool.ApplyStatus(MakeStatus(Pool, 1, EDTStatus::Freeze, 0.0f));
		H.Ticks(Pool, Steps(DTConst::STATUS_FREEZE_S) - 2);
		TestEqual(TEXT("frozen wind-up paused"), Pool.GetAux(0).StateLeft, WindLeft);
		TestEqual(TEXT("frozen enemy does not move"), Pool.GetPositions()[1], SwarmAt);
		H.Ticks(Pool, 5);
		TestTrue(TEXT("thawed wind-up resumes"), Pool.GetAux(0).StateLeft < WindLeft);

		FHarness C({ FVector2f::ZeroVector });
		FDTEnemyPool Touch;
		Touch.SetFireEnabled(false);
		Touch.Spawn(EDTEnemyRole::Turret, FVector2f(50.0f, 0.0f));
		Touch.ApplyStatus(MakeStatus(Touch, 0, EDTStatus::Freeze, 0.0f));
		C.Tick(Touch);
		TestTrue(TEXT("frozen"), Touch.GetAux(0).IsFrozen());
		TestEqual(TEXT("a frozen enemy still deals contact"), C.CountHeroHits(EDTHeroHitKind::Contact), 1);
		TestTrue(TEXT("unchanged contact damage"), C.View.HeroHits.Num() == 1 && C.View.HeroHits[0].Damage == static_cast<float>(DTConst::ROLE_TURRET_CONTACT_DAMAGE));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
