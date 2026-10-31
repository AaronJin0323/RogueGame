// Boss tests (02_Systems/boss.md Acceptance, T-BOS-01..12, plus a full headless fight at a fixed DPS and one world test).
// Drive FDTBossCore headless against a real FDTEnemyPool, FDTWorldView and FDTRunState, in the 07_Technical §4 order:
// queued spawns, enemy pool, boss (Enemies), a constant-DPS hit on the hull (Weapons), ResolveHits.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Boss; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Boss/DTBossCore.h"
#include "Boss/DTBossSubsystem.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTTelegraphRenderer.h"
#include "Engine/Engine.h"
#include "Telemetry/DTTelemetryRecords.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTBossTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.0f / static_cast<float>(DTConst::SIM_FIXED_HZ);
	constexpr float AngleEps = 1.0e-2f;

	static int32 Steps(float Seconds) { return FMath::CeilToInt32(Seconds * DTConst::SIM_FIXED_HZ); }

	struct FPhaseRec
	{
		int32 Phase = 0;
		float FightTime = 0.0f;
		float HpFrac = 0.0f;
		bool bByTime = false;
		int64 Step = 0;
	};

	/** Records every side effect; queued spawns are applied by the harness at the next step, as the enemy system does. */
	struct FRecSink : IDTBossSink
	{
		TArray<FDTEnemySpawn> Queued;
		TArray<FPhaseRec> Phases;
		TArray<FDTTelBossEnd> Ends;
		TArray<FName> Threats;
		int32 Starts = 0;
		int32 Calibrations = 0;
		float CalDps = 0.0f;
		float CalMax = 0.0f;
		bool bCalClamped = false;
		bool bDefeated = false;
		int32 Killer = INDEX_NONE;
		int64 DefeatStep = -1;
		int64 Now = 0;

		virtual void QueueSpawn(const FDTEnemySpawn& Spawn) override { Queued.Add(Spawn); }
		virtual void BossStart(FName Form, int32 Wins) override { ++Starts; }
		virtual void BossPhase(int32 Phase, float FightTime, float HpFrac, bool bByTime) override
		{
			Phases.Add(FPhaseRec{ Phase, FightTime, HpFrac, bByTime, Now });
		}
		virtual void BossCalibrated(float WindowDps, float EffectiveMax, bool bClamped) override
		{
			++Calibrations;
			CalDps = WindowDps;
			CalMax = EffectiveMax;
			bCalClamped = bClamped;
		}
		virtual void ThreatAction(const FVector2f& Origin, FName AttackId) override { Threats.Add(AttackId); }
		virtual void BossEnd(FDTTelBossEnd& End) override { Ends.Add(End); }
		virtual void BossDefeated(int64 StepIndex, int32 KillerOwner) override
		{
			bDefeated = true;
			Killer = KillerOwner;
			DefeatStep = StepIndex;
		}
	};

	/** One headless fight. */
	struct FHarness
	{
		FDTEnemyPool Pool;
		FDTWorldView View;
		FDTRunState RunState;
		FDTBossCore Core;
		FRecSink Sink;
		FRandomStream SpawnRng;
		int64 StepIndex = 0;
		/** Constant incoming DPS on the hull from DpsOwner (a stand-in for the weapons). */
		float Dps = 0.0f;
		int32 DpsOwner = 0;
		/** Scripted hero 0 (Hero phase); null keeps it where it is. */
		TFunction<FVector2f(int64)> HeroPath;
		TArray<FDTHeroHit> HeroHits;
		TArray<FDTKill> BossKills;

		explicit FHarness(int32 Seed, const FVector2f& Hero = FVector2f(0.0f, -1500.0f))
		{
			RunState.Start(Seed, 1);
			SpawnRng.Initialize(FDTRngStreams::DeriveSeed(Seed, EDTRngStream::Spawn));
			View.HeroPositions.Init(Hero, 1);
			View.HeroAimDirs.Init(FVector2f(1.0f, 0.0f), 1);
			View.HeroIntangible.Init(false, 1);
		}

		void Begin(int32 Wins = 0) { Core.Start(Wins, View.HeroPositions, Sink); }

		void Step()
		{
			Sink.Now = StepIndex;
			if (HeroPath)
			{
				View.HeroPositions[0] = HeroPath(StepIndex);
			}
			for (const FDTEnemySpawn& Spec : Sink.Queued)
			{
				Pool.Spawn(Spec, &SpawnRng, true);
			}
			Sink.Queued.Reset();
			FDTSimContext Context;
			Context.StepSeconds = Dt;
			Context.StepIndex = StepIndex;
			Context.Run = &RunState;
			Context.View = &View;
			Pool.StepEnemies(Context);
			FDTBossStepIn In;
			In.StepSeconds = Dt;
			In.TimeScale = View.EnemyTimeScale;
			In.StepIndex = StepIndex;
			In.View = &View;
			In.Pool = &Pool;
			In.Rng = &RunState.GetRng().Get(EDTRngStream::Boss);
			Core.Step(In, Sink);
			if (Dps > 0.0f && Core.HasHull() && Pool.IsHandleValid(Core.GetHull()))
			{
				FDTHit Hit;
				Hit.EnemyIndex = Core.GetHull().Index;
				Hit.EnemySerial = Core.GetHull().Serial;
				Hit.OwnerId = DpsOwner;
				Hit.Damage = Dps * Dt;
				Hit.Position = Pool.GetPositions()[Hit.EnemyIndex];
				View.Hits.Add(Hit);
			}
			HeroHits.Append(View.HeroHits);
			Pool.ResolveHits(Context);
			for (const FDTKill& Kill : View.Kills)
			{
				if (Kill.Role == EDTEnemyRole::Boss)
				{
					BossKills.Add(Kill);
				}
			}
			++StepIndex;
		}

		void RunSteps(int32 Count)
		{
			for (int32 Index = 0; Index < Count && !Core.IsEnded(); ++Index)
			{
				Step();
			}
		}

		/** Steps until the hull is in the pool (queued at Begin, spawned at the next Enemies phase). */
		bool Adopt()
		{
			for (int32 Guard = 0; Guard < 4 && !Core.HasHull(); ++Guard)
			{
				Step();
			}
			return Core.HasHull();
		}

		int32 HullIndex() const { return Core.GetHull().Index; }
	};

	/** A hero walking a circle of Radius around the arena centre (one turn per Period seconds). */
	static TFunction<FVector2f(int64)> Circle(float Radius, float Period)
	{
		return [Radius, Period](int64 Step)
		{
			const float A = UE_TWO_PI * static_cast<float>(Step) * Dt / Period;
			return FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius;
		};
	}

	static float HighestGapDeg(TArray<float> Slots)
	{
		Slots.Sort();
		float Best = 0.0f;
		for (int32 Index = 0; Index < Slots.Num(); ++Index)
		{
			const float Next = Index + 1 < Slots.Num() ? Slots[Index + 1] : Slots[0] + 360.0f;
			Best = FMath::Max(Best, Next - Slots[Index]);
		}
		return Best;
	}

	static const DTCat::FBossAttackRow* SignatureRow()
	{
		for (int32 Row = 0; Row < DTCat::NumBossAttacks; ++Row)
		{
			if (DTCat::BossWarden[Row].bSignature && FCString::Strcmp(DTCat::BossWarden[Row].Form, TEXT("I")) == 0)
			{
				return &DTCat::BossWarden[Row];
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossFormByWins, "Dusktracer.Boss.FormByWins", DTBossTest::Flags) // T-BOS-01
bool FDTBossFormByWins::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	const int32 II = DTConst::BOSS_FORM_II_WINS;
	const int32 III = DTConst::BOSS_FORM_III_WINS;
	TestTrue(TEXT("thresholds ordered"), 0 < II && II < III);
	TestTrue(TEXT("no wins: I"), FDTBossCore::FormForWins(0) == EDTBossForm::I);
	TestTrue(TEXT("below II: I"), FDTBossCore::FormForWins(II - 1) == EDTBossForm::I);
	TestTrue(TEXT("at II: II"), FDTBossCore::FormForWins(II) == EDTBossForm::II);
	TestTrue(TEXT("below III: II"), FDTBossCore::FormForWins(III - 1) == EDTBossForm::II);
	TestTrue(TEXT("at III: III"), FDTBossCore::FormForWins(III) == EDTBossForm::III);
	TestTrue(TEXT("above III: III"), FDTBossCore::FormForWins(III + 5) == EDTBossForm::III);
	// Heat and anything inside the run never change it: the form is a function of wins alone (no other input exists).
	TestTrue(TEXT("form I has rows"), FDTBossCore::HasRows(EDTBossForm::I));

	for (const int32 Wins : { 0, II, III })
	{
		FHarness H(3);
		H.Begin(Wins);
		const bool bRows = FDTBossCore::HasRows(FDTBossCore::FormForWins(Wins));
		TestTrue(*FString::Printf(TEXT("wins %d: plays a form with rows (form I without them)"), Wins),
			H.Core.GetForm() == (bRows ? FDTBossCore::FormForWins(Wins) : EDTBossForm::I));
		TestEqual(*FString::Printf(TEXT("wins %d: a missing form is counted"), Wins), H.Core.GetCounters().FormFallbacks, bRows ? 0 : 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossIdentify, "Dusktracer.Boss.Identify", DTBossTest::Flags) // T-BOS-02
bool FDTBossIdentify::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	const DTCat::FBossAttackRow* Signature = SignatureRow();
	if (!TestNotNull(TEXT("form I has a signature row"), Signature))
	{
		return false;
	}
	TestEqual(TEXT("the signature is phase One's"), FString(Signature->Phase), FString(FDTBossCore::GetPhaseDef(0).Name.ToString()));
	for (int32 Seed = 1; Seed <= 8; ++Seed)
	{
		FRandomStream Placement(Seed);
		const FVector2f Hero(Placement.FRandRange(-3000.0f, 3000.0f), Placement.FRandRange(-3000.0f, 3000.0f));
		FHarness H(Seed, Hero);
		H.Begin();
		if (!TestTrue(TEXT("hull adopted"), H.Adopt()))
		{
			return false;
		}
		const int64 FightStart = H.StepIndex - 1;
		H.RunSteps(Steps(DTConst::BOSS_IDENTIFY_S));
		const FDTBossAttackRec* First = H.Core.GetAttacks().Num() > 0 ? &H.Core.GetAttacks()[0] : nullptr;
		if (!TestNotNull(*FString::Printf(TEXT("seed %d: an attack inside the window"), Seed), First))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("seed %d: the opening attack is the signature"), Seed), First->AttackId == FName(Signature->Attack));
		TestTrue(*FString::Printf(TEXT("seed %d: the signature lands within C.BOSS.IDENTIFY_S"), Seed),
			First->LandStep >= 0 && (First->LandStep - FightStart) * Dt <= DTConst::BOSS_IDENTIFY_S);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossHullInPool, "Dusktracer.Boss.HullInPool", DTBossTest::Flags) // T-BOS-03
bool FDTBossHullInPool::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	FHarness H(7);
	H.Begin();
	TestEqual(TEXT("one queued hull"), H.Sink.Queued.Num(), 1);
	TestTrue(TEXT("role Boss, no director HP multiplier: the reserve"), H.Sink.Queued.Num() == 1
		&& H.Sink.Queued[0].Role == EDTEnemyRole::Boss && FMath::IsNearlyEqual(H.Sink.Queued[0].HpScale, DTConst::BOSS_HP_MAX_MULT));
	if (!TestTrue(TEXT("adopted"), H.Adopt()))
	{
		return false;
	}
	TestEqual(TEXT("one pool slot of role Boss"), H.Pool.CountRole(EDTEnemyRole::Boss), 1);
	TestEqual(TEXT("the reserve is HP_BASE x HP_MAX_MULT"), H.Core.GetReserveHp(),
		static_cast<float>(DTConst::BOSS_HP_BASE) * DTConst::BOSS_HP_MAX_MULT, 1.0f);
	TestEqual(TEXT("hull radius"), H.Pool.GetRadii()[H.HullIndex()], static_cast<float>(DTConst::BOSS_RADIUS_UU));
	TestTrue(TEXT("scripted state"), H.Pool.GetAux(H.HullIndex()).State == EDTEnemyState::Scripted);

	// A bullet hit damages it through ResolveHits.
	const float Before = H.Pool.GetHp()[H.HullIndex()];
	FDTHit Hit;
	Hit.EnemyIndex = H.HullIndex();
	Hit.EnemySerial = H.Core.GetHull().Serial;
	Hit.OwnerId = 0;
	Hit.Damage = 100.0f;
	H.View.Hits.Add(Hit);
	H.Step();
	TestEqual(TEXT("the hit lands"), H.Pool.GetHp()[H.HullIndex()], Before - 100.0f, 0.01f);

	// Its death is one Kill to the killing hit's owner, then the fight ends as a win.
	H.Dps = 6000.0f;
	H.DpsOwner = 1;
	H.HeroPath = Circle(900.0f, 12.0f);
	bool bInside = true;
	for (int32 Index = 0; Index < Steps(DTConst::RUN_BOSS_MAX) && !H.Core.IsEnded(); ++Index)
	{
		H.Step();
		if (H.Core.HasHull() && H.Core.IsHazardOn() && H.Pool.IsHandleValid(H.Core.GetHull()))
		{
			const float Reach = H.Pool.GetPositions()[H.HullIndex()].Size() + DTConst::BOSS_RADIUS_UU;
			bInside &= Reach <= H.Core.GetHazardRadius() + 1.0f;
		}
	}
	H.Step();
	TestTrue(TEXT("defeated"), H.Sink.bDefeated);
	TestEqual(TEXT("one Kill"), H.BossKills.Num(), 1);
	TestEqual(TEXT("to the killing owner"), H.BossKills.Num() > 0 ? H.BossKills[0].OwnerId : INDEX_NONE, 1);
	TestEqual(TEXT("reported to the sink"), H.Sink.Killer, 1);
	TestEqual(TEXT("no boss left in the pool"), H.Pool.CountRole(EDTEnemyRole::Boss), 0);
	TestTrue(TEXT("boss_end win"), H.Sink.Ends.Num() == 1 && H.Sink.Ends[0].Outcome == TEXT("win"));
	TestTrue(TEXT("the hull stays inside the hazard ring"), bInside);

	// Inside the hazard ring through a whole phase III with no damage.
	FHarness Slow(8, FVector2f(2600.0f, 0.0f));
	Slow.Begin();
	Slow.Adopt();
	Slow.Core.ForcePhase(3, Slow.Sink);
	bool bSlowInside = true;
	for (int32 Index = 0; Index < Steps(40.0f); ++Index)
	{
		Slow.Step();
		if (Slow.Core.IsHazardOn())
		{
			bSlowInside &= Slow.Pool.GetPositions()[Slow.HullIndex()].Size() + DTConst::BOSS_RADIUS_UU <= Slow.Core.GetHazardRadius() + 1.0f;
		}
	}
	TestTrue(TEXT("hazard on in phase III"), Slow.Core.IsHazardOn());
	TestTrue(TEXT("the hull stays inside the shrinking ring"), bSlowInside);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossPhases, "Dusktracer.Boss.Phases", DTBossTest::Flags) // T-BOS-04
bool FDTBossPhases::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	TestEqual(TEXT("form I has three phases"), FDTBossCore::NumPhases(), 3);

	// A burst through two thresholds enters both, in order, in one step; phases never go back.
	{
		FHarness H(11);
		H.Begin();
		H.Adopt();
		H.RunSteps(Steps(2.0f));
		TestEqual(TEXT("phase I"), H.Core.GetPhase(), 1);
		const float Burst = (1.0f - FDTBossCore::GetPhaseDef(2).EnterHpFrac + 0.05f) * DTConst::BOSS_HP_BASE;
		H.Pool.SetHp(H.HullIndex(), H.Pool.GetHp()[H.HullIndex()] - Burst);
		H.Step();
		TestEqual(TEXT("phase III after the burst"), H.Core.GetPhase(), 3);
		if (TestEqual(TEXT("three phase records"), H.Sink.Phases.Num(), 3))
		{
			TestEqual(TEXT("II then III"), H.Sink.Phases[1].Phase, 2);
			TestEqual(TEXT("III last"), H.Sink.Phases[2].Phase, 3);
			TestEqual(TEXT("same step"), H.Sink.Phases[1].Step, H.Sink.Phases[2].Step);
			TestFalse(TEXT("by HP"), H.Sink.Phases[2].bByTime);
		}
		H.RunSteps(Steps(DTConst::BOSS_CALIBRATE_S + 2.0f));
		TestTrue(TEXT("calibration raised the effective max"), H.Core.IsCalibrated());
		TestEqual(TEXT("never back"), H.Core.GetPhase(), 3);
		TestEqual(TEXT("no attack starts during a shift"), H.Core.GetCounters().AttacksStartedInShift, 0);
	}

	// By time, with no damage: each phase at its latest start.
	{
		FHarness H(12);
		H.Begin();
		H.Adopt();
		H.RunSteps(Steps(DTConst::RUN_BOSS_MAX));
		if (TestEqual(TEXT("every phase entered"), H.Sink.Phases.Num(), FDTBossCore::NumPhases()))
		{
			for (int32 Phase = 1; Phase < FDTBossCore::NumPhases(); ++Phase)
			{
				const FPhaseRec& Rec = H.Sink.Phases[Phase];
				TestEqual(TEXT("in order"), Rec.Phase, Phase + 1);
				TestTrue(TEXT("by time"), Rec.bByTime);
				TestEqual(TEXT("at its latest start"), Rec.FightTime, FDTBossCore::GetPhaseDef(Phase).LatestStart, 2.0f * Dt);
			}
		}
		TestEqual(TEXT("no attack starts during a shift"), H.Core.GetCounters().AttacksStartedInShift, 0);
		// Shift: from each phase change, no telegraph starts for C.BOSS.PHASE_SHIFT_S (crossing rings finish their pair).
		for (int32 Phase = 1; Phase < H.Sink.Phases.Num(); ++Phase)
		{
			const int64 From = H.Sink.Phases[Phase].Step;
			const int64 To = From + Steps(DTConst::BOSS_PHASE_SHIFT_S) - 1;
			for (const FDTBossAttackRec& Rec : H.Core.GetAttacks())
			{
				if (Rec.AttackId != FName(TEXT("CrossRing")) && Rec.TelegraphStep >= From && Rec.TelegraphStep < To)
				{
					AddError(FString::Printf(TEXT("%s started at step %lld inside the shift after phase %d"), *Rec.AttackId.ToString(), Rec.TelegraphStep, Phase + 1));
				}
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossTelegraphs, "Dusktracer.Boss.Telegraphs", DTBossTest::Flags) // T-BOS-05
bool FDTBossTelegraphs::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	const TCHAR* Shapes[] = { TEXT("Band"), TEXT("Arc"), TEXT("RingGap"), TEXT("SpiralLane"), TEXT("HazardRing") };
	for (int32 Row = 0; Row < DTCat::NumBossAttacks; ++Row)
	{
		const DTCat::FBossAttackRow& R = DTCat::BossWarden[Row];
		const bool bAttack = FCString::Strcmp(R.Attack, TEXT("Adds")) != 0;
		bool bKnown = false;
		for (const TCHAR* Shape : Shapes)
		{
			bKnown |= FCString::Strcmp(R.Telegraph, Shape) == 0;
		}
		TestTrue(*FString::Printf(TEXT("%s: %s"), R.Attack, bAttack ? TEXT("names a boss telegraph shape") : TEXT("not an attack, no telegraph")),
			bAttack ? bKnown : FCString::Strlen(R.Telegraph) == 0);
		if (bAttack)
		{
			TestTrue(*FString::Printf(TEXT("%s: wind-up at least the minimum lead"), R.Attack), R.Windup_s >= DTConst::ENEMIES_TELEGRAPH_LEAD_MIN_S - 1.0e-4f);
		}
	}

	// The boss shapes' footprints as drawn (Enemies/DTTelegraphRenderer).
	{
		const float Mesh = 100.0f;
		FDTTelegraph Gap;
		Gap.Shape = EDTTelegraphShape::RingGap;
		Gap.Origin = FVector2f(200.0f, -100.0f);
		Gap.Dir = FVector2f(0.0f, 1.0f);
		Gap.Length = 600.0f;
		Gap.Width = 36.0f;
		TArray<FTransform> Out;
		ADTTelegraphRenderer::AppendTransforms(Gap, Mesh, 0.0f, Out);
		TestEqual(TEXT("RingGap: the disc minus the lane in segments"), Out.Num(), ADTTelegraphRenderer::InstancesFor(Gap));
		bool bOutsideLane = Out.Num() > 0;
		for (const FTransform& Q : Out)
		{
			const FVector2f C(Q.GetLocation().X - Gap.Origin.X, Q.GetLocation().Y - Gap.Origin.Y);
			bOutsideLane &= FMath::IsNearlyEqual(C.Size(), 300.0f, 0.5f)
				&& FMath::Abs(FDTBossCore::WrapDeg(FMath::RadiansToDegrees(FMath::Atan2(C.Y, C.X)) - 90.0f)) > 0.5f * Gap.Width;
		}
		TestTrue(TEXT("RingGap: no quad in the lane"), bOutsideLane);
		FDTTelegraph Hazard;
		Hazard.Shape = EDTTelegraphShape::HazardRing;
		Hazard.Length = 1300.0f;
		Out.Reset();
		ADTTelegraphRenderer::AppendTransforms(Hazard, Mesh, 0.0f, Out);
		TestEqual(TEXT("HazardRing: RingSegments quads"), Out.Num(), ADTTelegraphRenderer::RingSegments);
		bool bOutside = Out.Num() > 0;
		for (const FTransform& Q : Out)
		{
			bOutside &= FVector2D(Q.GetLocation().X, Q.GetLocation().Y).Size() > Hazard.Length;
		}
		TestTrue(TEXT("HazardRing: drawn outside the safe circle"), bOutside);
	}

	FHarness H(21);
	H.HeroPath = Circle(800.0f, 9.0f);
	H.Dps = 300.0f;
	H.Begin();
	H.Adopt();
	H.RunSteps(Steps(DTConst::RUN_BOSS_MAX));
	TSet<FName> Seen;
	const int32 LeadSteps = FMath::FloorToInt32(DTConst::ENEMIES_TELEGRAPH_LEAD_MIN_S * DTConst::SIM_FIXED_HZ + 1.0e-3f);
	for (const FDTBossAttackRec& Rec : H.Core.GetAttacks())
	{
		if (Rec.LandStep < 0)
		{
			continue; // still telegraphing when the fight ended
		}
		Seen.Add(Rec.AttackId);
		TestTrue(*FString::Printf(TEXT("%s at %lld: telegraph shown at least the minimum lead"), *Rec.AttackId.ToString(), Rec.TelegraphStep),
			Rec.LandStep - Rec.TelegraphStep >= LeadSteps);
		const bool bBandDone = Rec.Shape != EDTTelegraphShape::Band || Rec.DamageLength > 0.0f;
		if (bBandDone)
		{
			TestEqual(*FString::Printf(TEXT("%s: footprint length = damage"), *Rec.AttackId.ToString()), Rec.FootprintLength, Rec.DamageLength, 0.5f);
			TestEqual(*FString::Printf(TEXT("%s: footprint width = damage"), *Rec.AttackId.ToString()), Rec.FootprintWidth, Rec.DamageWidth, 0.01f);
		}
	}
	for (const FDTBossRingShot& Shot : H.Core.GetRingShots())
	{
		TestEqual(TEXT("ring telegraph gap = its lane"), Shot.TelegraphGapDeg, Shot.LaneGapDeg, AngleEps);
		TestEqual(TEXT("ring telegraph lane = its lane"), Shot.TelegraphLaneDeg, Shot.LaneCentreDeg, AngleEps);
	}
	for (const TCHAR* Attack : { TEXT("Charge"), TEXT("Ring"), TEXT("CrossRing"), TEXT("DashCharge"), TEXT("Spiral"), TEXT("Hazard") })
	{
		TestTrue(*FString::Printf(TEXT("the fight showed %s"), Attack), Seen.Contains(FName(Attack)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossRingSlots, "Dusktracer.Boss.RingSlots", DTBossTest::Flags) // T-BOS-06
bool FDTBossRingSlots::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	auto Fight = [](int32 Seed)
	{
		TUniquePtr<FHarness> H = MakeUnique<FHarness>(Seed);
		H->HeroPath = Circle(1100.0f, 14.0f);
		H->Dps = 500.0f;
		H->Begin();
		H->Adopt();
		H->RunSteps(Steps(DTConst::RUN_BOSS_MAX));
		return H;
	};
	TUniquePtr<FHarness> A = Fight(31);
	TUniquePtr<FHarness> B = Fight(31);
	TestTrue(TEXT("rings fired"), A->Core.GetRingShots().Num() > 4);
	for (const FDTBossRingShot& Shot : A->Core.GetRingShots())
	{
		if (Shot.Fired == 0)
		{
			continue;
		}
		TestEqual(TEXT("count minus the lane"), Shot.Fired, Shot.Count - FDTBossCore::LaneSlots(Shot.Count));
		const float StepDeg = 360.0f / static_cast<float>(Shot.Count);
		for (const float Deg : Shot.SlotDeg)
		{
			const float Steps01 = FDTBossCore::WrapDeg(Deg - Shot.TargetBearingDeg) / StepDeg;
			TestTrue(TEXT("equal angles on the slot grid"), FMath::Abs(Steps01 - FMath::RoundToFloat(Steps01)) < 1.0e-3f);
		}
	}
	const bool bSame = A->Core.GetRingShots().Num() == B->Core.GetRingShots().Num();
	TestTrue(TEXT("same seed, same number of rings"), bSame);
	if (bSame)
	{
		for (int32 Index = 0; Index < A->Core.GetRingShots().Num(); ++Index)
		{
			const FDTBossRingShot& X = A->Core.GetRingShots()[Index];
			const FDTBossRingShot& Y = B->Core.GetRingShots()[Index];
			TestTrue(TEXT("same seed and inputs, same slots"), X.SlotDeg == Y.SlotDeg && X.Origin == Y.Origin && X.FireStep == Y.FireStep);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossDodgeLane, "Dusktracer.Boss.DodgeLane", DTBossTest::Flags) // T-BOS-07
bool FDTBossDodgeLane::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	const float Need = FDTBossCore::LaneChordRequired();
	TestEqual(TEXT("lane rule: hero diameters plus two round diameters"), Need,
		DTConst::BOSS_LANE_HERO_DIAMETERS * 2.0f * DTConst::HERO_RADIUS_UU + 4.0f * DTConst::BOSS_BULLET_RADIUS_UU, 0.01f);
	int32 Checked = 0;
	for (const int32 Seed : { 41, 42, 43 })
	{
		FHarness H(Seed);
		H.HeroPath = Circle(700.0f + 300.0f * (Seed - 41), 10.0f);
		H.Dps = 400.0f;
		H.Begin();
		H.Adopt();
		H.RunSteps(Steps(DTConst::RUN_BOSS_MAX));
		for (const FDTBossRingShot& Shot : H.Core.GetRingShots())
		{
			if (Shot.Fired == 0)
			{
				continue;
			}
			++Checked;
			TestTrue(*FString::Printf(TEXT("%s: lane chord %.0f >= %.0f uu"), *Shot.AttackId.ToString(), Shot.LaneChordUU, Need), Shot.LaneChordUU >= Need - 0.01f);
			const float Gap = HighestGapDeg(Shot.SlotDeg);
			TestEqual(TEXT("the free gap between the rounds is the lane"), Gap, Shot.LaneGapDeg, AngleEps);
			TestTrue(TEXT("the real chord holds the rule"), FDTBossCore::ChordAt(Gap, DTConst::BOSS_LANE_MEASURE_UU) >= Need - 0.01f);
			TestEqual(TEXT("the telegraph gap matches"), Shot.TelegraphGapDeg, Gap, AngleEps);
			if (!Shot.bCross)
			{
				TestTrue(TEXT("lane within the bearing limit of the target"),
					FMath::Abs(FDTBossCore::WrapDeg(Shot.LaneCentreDeg - Shot.TargetBearingDeg)) <= DTConst::BOSS_LANE_BEARING_MAX_DEG + AngleEps);
			}
			for (const float Deg : Shot.SlotDeg)
			{
				TestTrue(TEXT("no round inside the lane"), FMath::Abs(FDTBossCore::WrapDeg(Deg - Shot.LaneCentreDeg)) >= 0.5f * Shot.LaneGapDeg - AngleEps);
			}
		}
		const FDTBossCounters& C = H.Core.GetCounters();
		TestTrue(TEXT("the spiral fired"), C.SpiralEmissions > 0);
		TestTrue(TEXT("the spiral skipped its lane"), C.SpiralSkipped > 0);
		TestTrue(TEXT("no spiral round inside its rotating lane sector"), C.SpiralMinLaneDeg >= 0.5f * FDTBossCore::LaneAngleRequiredDeg() - AngleEps);
	}
	TestTrue(TEXT("rings checked"), Checked > 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossNoSafeSpot, "Dusktracer.Boss.NoSafeSpot", DTBossTest::Flags) // T-BOS-08
bool FDTBossNoSafeSpot::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	const float HeroR = static_cast<float>(DTConst::HERO_RADIUS_UU);
	const float RoundR = static_cast<float>(DTConst::BOSS_BULLET_RADIUS_UU);
	// A stationary hero anywhere in the arena: every bearing, near and far.
	for (int32 Bearing = 0; Bearing < 8; ++Bearing)
	{
		for (const float Dist : { 600.0f, 2000.0f, 3400.0f })
		{
			const float A = UE_TWO_PI * Bearing / 8.0f;
			const FVector2f Hero = FVector2f(FMath::Cos(A), FMath::Sin(A)) * Dist;
			FHarness H(50 + Bearing, Hero);
			H.Begin();
			H.Adopt();
			H.Core.ForcePhase(2, H.Sink);
			const float Shift = DTConst::BOSS_PHASE_SHIFT_S;
			H.RunSteps(Steps(Shift + 2.0f * DTConst::ENEMIES_TELEGRAPH_LEAD_MIN_S + DTConst::BOSS_CROSS_RING_DELAY_S + 0.1f));
			const FDTBossRingShot* First = nullptr;
			const FDTBossRingShot* Cross = nullptr;
			for (const FDTBossRingShot& Shot : H.Core.GetRingShots())
			{
				if (!Shot.bCross && !First && Shot.Fired > 0) { First = &Shot; }
				if (Shot.bCross && !Cross && Shot.Fired > 0) { Cross = &Shot; }
			}
			const FString Where = FString::Printf(TEXT("hero at bearing %d/8, %.0f uu"), Bearing, Dist);
			if (!TestNotNull(*(Where + TEXT(": a ring")), First) || !TestNotNull(*(Where + TEXT(": its crossing ring")), Cross))
			{
				continue;
			}
			const float Offset = FMath::Abs(Cross->CrossOffsetDeg);
			TestTrue(*(Where + FString::Printf(TEXT(": crossing lane offset %.1f in range"), Offset)),
				Offset >= DTConst::BOSS_CROSS_LANE_OFFSET_DEG_MIN - AngleEps && Offset <= DTConst::BOSS_CROSS_LANE_OFFSET_DEG_MAX + AngleEps);
			// Geometry: a round of the pair flies through the stationary hero's body.
			bool bOnRay = false;
			for (const FDTBossRingShot* Shot : { First, Cross })
			{
				const FVector2f To = Hero - Shot->Origin;
				for (const float Deg : Shot->SlotDeg)
				{
					const float Rad = FMath::DegreesToRadians(Deg);
					const FVector2f Dir(FMath::Cos(Rad), FMath::Sin(Rad));
					const float Along = FVector2f::DotProduct(To, Dir);
					const float Off = FMath::Abs(To.X * Dir.Y - To.Y * Dir.X);
					bOnRay |= Along > 0.0f && Off <= HeroR + RoundR;
				}
			}
			TestTrue(*(Where + TEXT(": a round of the pair crosses the stationary hero")), bOnRay);
			// Simulated: the rounds reach it.
			H.RunSteps(Steps(DTConst::BOSS_ROUND_LIFE_S));
			int32 RingHits = 0;
			for (const FDTHeroHit& Hit : H.HeroHits)
			{
				RingHits += Hit.AttackId == FName(TEXT("Ring")) || Hit.AttackId == FName(TEXT("CrossRing")) ? 1 : 0;
			}
			TestTrue(*(Where + TEXT(": hit by the pair")), RingHits > 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossCalibrate, "Dusktracer.Boss.Calibrate", DTBossTest::Flags) // T-BOS-09
bool FDTBossCalibrate::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	const float Base = static_cast<float>(DTConst::BOSS_HP_BASE);
	const float Min = DTConst::BOSS_HP_MIN_MULT * Base;
	const float Max = DTConst::BOSS_HP_MAX_MULT * Base;
	struct FCase { float Dps; float Expected; bool bClamped; };
	const FCase Cases[] =
	{
		{ 300.0f, 300.0f * DTConst::BOSS_TARGET_FIGHT_S, false },
		{ 10.0f, Min, true },
		{ 5000.0f, Max, true },
	};
	for (const FCase& Case : Cases)
	{
		FHarness H(61);
		H.Dps = Case.Dps;
		H.Begin();
		H.Adopt();
		H.RunSteps(Steps(DTConst::BOSS_CALIBRATE_S - 1.0f));
		TestFalse(TEXT("not before the window ends"), H.Core.IsCalibrated());
		TestEqual(TEXT("the fight starts on HP_BASE"), H.Core.GetEffectiveMax(), Base, 0.01f);
		H.RunSteps(Steps(1.0f) + 1);
		const FString Name = FString::Printf(TEXT("%.0f dps"), Case.Dps);
		if (!TestTrue(*(Name + TEXT(": calibrated")), H.Core.IsCalibrated()))
		{
			continue;
		}
		TestEqual(*(Name + TEXT(": effective max")), H.Core.GetEffectiveMax(), Case.Expected, Case.Expected * 0.01f);
		TestEqual(*(Name + TEXT(": window dps")), H.Sink.CalDps, Case.Dps, Case.Dps * 0.01f);
		TestTrue(*(Name + TEXT(": clamped flag")), H.Sink.bCalClamped == Case.bClamped);
		TestTrue(*(Name + TEXT(": within the clamp")), H.Core.GetEffectiveMax() >= Min - 0.01f && H.Core.GetEffectiveMax() <= Max + 0.01f);
		// Dealt damage carries over: the pool holds what is left of the effective max; nothing is added.
		if (H.Core.HasHull())
		{
			// (the pool took one more step of damage after the core last read it)
			TestEqual(*(Name + TEXT(": dealt carries over")), H.Pool.GetHp()[H.HullIndex()] + H.Core.GetDamageDealt(), H.Core.GetEffectiveMax(), Case.Dps * Dt + 1.0f);
			TestTrue(*(Name + TEXT(": no HP added")), H.Pool.GetHp()[H.HullIndex()] <= H.Core.GetReserveHp() - H.Core.GetDamageDealt() + 0.01f);
		}
		H.RunSteps(Steps(DTConst::RUN_BOSS_MAX));
		TestEqual(*(Name + TEXT(": calibrated once")), H.Sink.Calibrations, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossFightBound, "Dusktracer.Boss.FightBound", DTBossTest::Flags) // T-BOS-10
bool FDTBossFightBound::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	TestTrue(TEXT("target fight below the cap"), DTConst::BOSS_TARGET_FIGHT_S < DTConst::RUN_BOSS_MAX);
	for (int32 Phase = 0; Phase < FDTBossCore::NumPhases(); ++Phase)
	{
		TestTrue(TEXT("each latest start below the cap"), FDTBossCore::GetPhaseDef(Phase).LatestStart < DTConst::RUN_BOSS_MAX);
	}
	FHarness H(71, FVector2f(400.0f, 300.0f));
	H.Begin();
	H.Adopt();
	H.RunSteps(Steps(DTConst::RUN_BOSS_MAX) - 1);
	TestTrue(TEXT("the fight is still on with no damage (no HP wall, no instant kill)"), H.Core.IsActive());
	TestEqual(TEXT("at the cap"), H.Core.GetFightTime(), static_cast<float>(DTConst::RUN_BOSS_MAX), 2.0f * Dt);
	TestEqual(TEXT("every phase started"), H.Core.GetPhase(), FDTBossCore::NumPhases());
	for (const FPhaseRec& Rec : H.Sink.Phases)
	{
		TestTrue(TEXT("before the cap"), Rec.FightTime < DTConst::RUN_BOSS_MAX);
	}
	TestTrue(TEXT("hazard shrinking"), H.Core.IsHazardShrinking());
	TestEqual(TEXT("the hazard ring is at its minimum at C.RUN.BOSS_MAX"), H.Core.GetHazardRadius(), static_cast<float>(DTConst::BOSS_HAZARD_MIN_RADIUS_UU), 0.01f);
	TestEqual(TEXT("no HP added: effective max is the clamp floor"), H.Core.GetEffectiveMax(), DTConst::BOSS_HP_MIN_MULT * DTConst::BOSS_HP_BASE, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossStatuses, "Dusktracer.Boss.Statuses", DTBossTest::Flags) // T-BOS-11
bool FDTBossStatuses::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	FHarness H(81);
	H.Begin();
	H.Adopt();
	const FDTEnemyHandle Hull = H.Core.GetHull();
	auto Apply = [&H, &Hull](EDTStatus Status, float Magnitude)
	{
		FDTStatusApply Req;
		Req.Handle = Hull;
		Req.Status = Status;
		Req.Magnitude = Magnitude;
		Req.OwnerId = 0;
		return H.Pool.ApplyStatus(Req);
	};
	TestFalse(TEXT("Freeze refused"), Apply(EDTStatus::Freeze, 1.0f));
	TestFalse(TEXT("not frozen"), H.Pool.GetAux(Hull.Index).IsFrozen());
	TestTrue(TEXT("Chill applies"), Apply(EDTStatus::Chill, 10.0f));
	TestEqual(TEXT("Chill stops at its cap"), H.Pool.GetAux(Hull.Index).ChillStacks, DTConst::STATUS_BOSS_CHILL_STACK_MAX);
	TestFalse(TEXT("Chill never freezes it"), H.Pool.GetAux(Hull.Index).IsFrozen());
	TestTrue(TEXT("Shock applies"), Apply(EDTStatus::Shock, 0.0f));
	TestTrue(TEXT("shocked"), H.Pool.GetAux(Hull.Index).ShockLeft > 0.0f);
	TestTrue(TEXT("Burn applies"), Apply(EDTStatus::Burn, 50.0f));
	TestTrue(TEXT("burning"), H.Pool.GetAux(Hull.Index).NumBurnStacks() > 0);

	// Displacements naming the hull are ignored (R-BOS-03): only the script moves it.
	H.RunSteps(Steps(0.3f));
	const FVector2f Before = H.Pool.GetPositions()[Hull.Index];
	FDTDisplacement Push;
	Push.EnemyIndex = Hull.Index;
	Push.EnemySerial = Hull.Serial;
	Push.Delta = FVector2f(1000.0f, 0.0f);
	H.View.Displacements.Add(Push);
	H.Step();
	TestTrue(TEXT("displacement ignored"), FVector2f::Distance(H.Pool.GetPositions()[Hull.Index], Before) < 0.5f * DTConst::BOSS_WALK_SPEED_UU_S);

	// Chill slows the walk.
	auto WalkFrom = [](bool bChill)
	{
		FHarness W(82, FVector2f(3000.0f, 0.0f));
		W.Begin();
		W.Adopt();
		if (bChill)
		{
			FDTStatusApply Chill;
			Chill.Handle = W.Core.GetHull();
			Chill.Status = EDTStatus::Chill;
			Chill.Magnitude = 10.0f;
			W.Pool.ApplyStatus(Chill);
		}
		const FVector2f Start = W.Pool.GetPositions()[W.HullIndex()];
		W.RunSteps(Steps(0.5f));
		return FVector2f::Distance(W.Pool.GetPositions()[W.HullIndex()], Start);
	};
	TestTrue(TEXT("a chilled boss walks slower"), WalkFrom(true) < WalkFrom(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossDeterminism, "Dusktracer.Boss.Determinism", DTBossTest::Flags) // T-BOS-12
bool FDTBossDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	auto Trace = [](int32 Seed)
	{
		FHarness H(Seed);
		H.HeroPath = Circle(1200.0f, 11.0f);
		H.Dps = 350.0f;
		H.Begin();
		H.Adopt();
		TArray<uint32> Out;
		for (int32 Index = 0; Index < Steps(DTConst::RUN_BOSS_MAX) && !H.Core.IsEnded(); ++Index)
		{
			H.Step();
			uint32 Hash = 0;
			if (H.Core.HasHull() && H.Pool.IsHandleValid(H.Core.GetHull()))
			{
				const FVector2f P = H.Pool.GetPositions()[H.HullIndex()];
				const float Hp = H.Pool.GetHp()[H.HullIndex()];
				Hash = HashCombine(Hash, GetTypeHash(P.X));
				Hash = HashCombine(Hash, GetTypeHash(P.Y));
				Hash = HashCombine(Hash, GetTypeHash(Hp));
			}
			const FDTEnemyBulletPool& Rounds = H.Pool.GetRounds();
			Hash = HashCombine(Hash, GetTypeHash(Rounds.Num()));
			for (const FVector2f& R : Rounds.GetPositions())
			{
				Hash = HashCombine(Hash, GetTypeHash(R.X));
				Hash = HashCombine(Hash, GetTypeHash(R.Y));
			}
			Out.Add(Hash);
		}
		return Out;
	};
	const TArray<uint32> A = Trace(91);
	const TArray<uint32> B = Trace(91);
	TestTrue(TEXT("a whole fight traced"), A.Num() > Steps(DTConst::BOSS_CALIBRATE_S));
	TestTrue(TEXT("same seed and scripted hero: bit-identical positions, rounds and HP"), A == B);
	const TArray<uint32> C = Trace(92);
	TestTrue(TEXT("another seed draws other crossing lanes"), A != C);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossHitKinds, "Dusktracer.Boss.HitKinds", DTBossTest::Flags)
bool FDTBossHitKinds::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	// Telemetry tells the boss's direct attacks apart: a hero that crosses the arena meets Charge, Sweep and DashCharge, and
	// one standing past the ring in phase III meets the ring (combat_core.md Telemetry).
	TSet<FString> Seen;
	for (int32 Seed = 201; Seed < 206; ++Seed)
	{
		TUniquePtr<FHarness> H = MakeUnique<FHarness>(Seed);
		FHarness* Fight = H.Get();
		H->HeroPath = [Fight](int64 Step)
		{
			// At the hull's side a sweep is in reach and a charge lands; past phase III's opening the hero stands outside the ring.
			if (Step >= Steps(DTConst::BOSS_TARGET_FIGHT_S * 0.69f))
			{
				return FVector2f(3600.0f, 0.0f);
			}
			const bool bHull = Fight->Core.HasHull() && Fight->Pool.IsHandleValid(Fight->Core.GetHull());
			return (bHull ? Fight->Pool.GetPositions()[Fight->HullIndex()] : FVector2f::ZeroVector) + FVector2f(300.0f, 0.0f);
		};
		H->Dps = 350.0f;
		H->Begin();
		H->Adopt();
		H->RunSteps(Steps(DTConst::RUN_BOSS_MAX) + 2);
		for (const FDTHeroHit& Hit : H->HeroHits)
		{
			const FString Attack = Hit.AttackId.ToString();
			const FString Kind = DTTelNames::HeroHitKind(Hit.Kind);
			Seen.Add(Kind);
			if (Attack == TEXT("Charge") || Attack == TEXT("Sweep") || Attack == TEXT("DashCharge"))
			{
				TestEqual(TEXT("a direct attack is its own hit kind"), Kind, Attack);
			}
			else if (Attack == TEXT("Hazard"))
			{
				TestEqual(TEXT("the ring is the hazard ring"), Kind, FString(TEXT("HazardRing")));
			}
		}
	}
	for (const TCHAR* Kind : { TEXT("Charge"), TEXT("Sweep"), TEXT("DashCharge"), TEXT("HazardRing") })
	{
		TestTrue(*FString::Printf(TEXT("%s reached the hero"), Kind), Seen.Contains(Kind));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossFullFight, "Dusktracer.Boss.FullFight", DTBossTest::Flags)
bool FDTBossFullFight::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	const float Dps = 350.0f;
	auto Fight = [Dps](int32 Seed)
	{
		TUniquePtr<FHarness> H = MakeUnique<FHarness>(Seed);
		H->HeroPath = Circle(1000.0f, 10.0f);
		H->Dps = Dps;
		H->Begin();
		H->Adopt();
		H->RunSteps(Steps(DTConst::RUN_BOSS_MAX) + 2);
		return H;
	};
	TUniquePtr<FHarness> H = Fight(101);
	const float Min = DTConst::BOSS_HP_MIN_MULT * DTConst::BOSS_HP_BASE;
	const float Max = DTConst::BOSS_HP_MAX_MULT * DTConst::BOSS_HP_BASE;
	TestTrue(TEXT("calibrated once"), H->Sink.Calibrations == 1);
	TestTrue(TEXT("calibrated HP within the clamp"), H->Sink.CalMax >= Min && H->Sink.CalMax <= Max);
	TestEqual(TEXT("calibrated HP = window DPS x target fight"), H->Sink.CalMax, Dps * DTConst::BOSS_TARGET_FIGHT_S, Dps * DTConst::BOSS_TARGET_FIGHT_S * 0.01f);
	if (TestEqual(TEXT("phases I, II, III"), H->Sink.Phases.Num(), 3))
	{
		for (int32 Index = 0; Index < 3; ++Index)
		{
			TestEqual(TEXT("in order"), H->Sink.Phases[Index].Phase, Index + 1);
		}
	}
	int32 Rings = 0;
	for (const FDTBossRingShot& Shot : H->Core.GetRingShots())
	{
		if (Shot.Fired > 0)
		{
			++Rings;
			TestTrue(TEXT("every ring has a lane >= the rule"), Shot.LaneChordUU >= FDTBossCore::LaneChordRequired() - 0.01f);
		}
	}
	TestTrue(TEXT("rings fired"), Rings > 0);
	AddInfo(FString::Printf(TEXT("full fight at %.0f dps: effective max %.0f, phases at %.1f/%.1f s, %d rings (%d rounds, %d spiral), hazard hits %d, adds %d, ended %s at %.2f s"),
		Dps, H->Sink.CalMax, H->Sink.Phases.Num() > 1 ? H->Sink.Phases[1].FightTime : -1.0f, H->Sink.Phases.Num() > 2 ? H->Sink.Phases[2].FightTime : -1.0f,
		Rings, H->Core.GetCounters().RoundsFired, H->Core.GetCounters().SpiralEmissions, H->Core.GetCounters().HazardHits,
		H->Core.GetCounters().AddsQueued, *H->Core.GetOutcome(), H->Core.GetFightTime()));
	TestTrue(TEXT("the boss died"), H->Sink.bDefeated);
	TestTrue(TEXT("the fight ends before C.RUN.BOSS_MAX"), H->Core.GetFightTime() < DTConst::RUN_BOSS_MAX);
	TestEqual(TEXT("close to the target fight"), H->Core.GetFightTime(), static_cast<float>(DTConst::BOSS_TARGET_FIGHT_S), 2.0f);
	TestTrue(TEXT("boss_end win at zero HP"), H->Sink.Ends.Num() == 1 && H->Sink.Ends[0].Outcome == TEXT("win") && H->Sink.Ends[0].HpLeftFrac == 0.0f);
	TestTrue(TEXT("threat actions reported"), H->Sink.Threats.Num() > 0);

	TUniquePtr<FHarness> Again = Fight(101);
	TestEqual(TEXT("deterministic: same end step"), Again->Sink.DefeatStep, H->Sink.DefeatStep);
	TestEqual(TEXT("deterministic: same effective max"), Again->Sink.CalMax, H->Sink.CalMax);
	TestEqual(TEXT("deterministic: same rings"), Again->Core.GetRingShots().Num(), H->Core.GetRingShots().Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossWorld, "Dusktracer.Boss.World", DTBossTest::Flags)
bool FDTBossWorld::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTBossTestWorld")));
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTBossSubsystem* Boss = World->GetSubsystem<UDTBossSubsystem>();
	UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
	if (TestNotNull(TEXT("sim"), Sim) && TestNotNull(TEXT("boss"), Boss) && TestNotNull(TEXT("enemies"), Enemies))
	{
		Sim->StartRun(5, 1);
		Boss->StartFight();
		Sim->StepNow(3);
		TestTrue(TEXT("the hull entered the pool"), Boss->GetCore().HasHull());
		TestEqual(TEXT("one Boss slot"), Enemies->GetPool().CountRole(EDTEnemyRole::Boss), 1);
		Sim->StepNow(Steps(DTConst::BOSS_OPENING_WALK_S + 0.2f));
		// The boss steps after the enemy pool: its band survives the pool's telegraph publish.
		const bool bBand = Sim->GetView().Telegraphs.ContainsByPredicate([](const FDTTelegraph& T) { return T.Shape == EDTTelegraphShape::Band; });
		TestTrue(TEXT("the signature charge telegraph is published"), bBand);
		Boss->ForcePhase(3);
		Sim->StepNow(Steps(DTConst::BOSS_PHASE_SHIFT_S + 0.2f));
		const bool bHazard = Sim->GetView().Telegraphs.ContainsByPredicate([](const FDTTelegraph& T) { return T.Shape == EDTTelegraphShape::HazardRing; });
		TestTrue(TEXT("the hazard ring telegraph is published"), bHazard);
		Sim->StepNow(Steps(DTConst::ENEMIES_TELEGRAPH_LEAD_MIN_S + 0.1f));
		const TConstArrayView<FDTTelegraph> Telegraphs = Sim->GetView().Telegraphs;
		const bool bSpiral = Telegraphs.ContainsByPredicate([](const FDTTelegraph& T) { return T.Shape == EDTTelegraphShape::SpiralLane; });
		TestTrue(TEXT("the spiral lane telegraph is published"), bSpiral);
		// Drawn: one instance per quad of every published telegraph, boss shapes included.
		if (ADTTelegraphRenderer* Renderer = Enemies->GetTelegraphRenderer())
		{
			Enemies->SimRender(0.0f);
			int32 Expected = 0;
			for (const FDTTelegraph& T : Telegraphs)
			{
				Expected += ADTTelegraphRenderer::InstancesDrawn(T);
			}
			TestEqual(TEXT("the renderer draws every boss telegraph"), Renderer->GetInstanceCount(), Expected);
			TestTrue(TEXT("boss shapes are tessellated"), Expected > Telegraphs.Num());
		}
	}

	for (AActor* Actor : FActorRange(World))
	{
		if (Actor)
		{
			Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
		}
	}
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossDilation, "Dusktracer.Keepers.S2.Dilation.Boss", DTBossTest::Flags) // T-KEE-37, R-DIR-10
bool FDTBossDilation::RunTest(const FString& Parameters)
{
	using namespace DTBossTest;
	constexpr float Half = 0.5f;
	// Runs a fight until its first attack has landed: the lead in steps, and how far the hull walked before it.
	const auto Fight = [](float Scale, int32& OutLead, float& OutWalked)
	{
		FHarness H(5);
		H.View.EnemyTimeScale = Scale;
		H.Begin();
		H.Adopt();
		const FVector2f Start = H.Pool.GetPositions()[H.HullIndex()];
		for (int32 N = 0; N < 120 && H.Core.GetAttacks().Num() == 0; ++N)
		{
			H.Step();
			OutWalked = FVector2f::Distance(Start, H.Pool.GetPositions()[H.HullIndex()]);
		}
		for (int32 N = 0; N < 1200 && (H.Core.GetAttacks().Num() == 0 || H.Core.GetAttacks()[0].LandStep < 0); ++N)
		{
			H.Step();
		}
		const TConstArrayView<FDTBossAttackRec> Attacks = H.Core.GetAttacks();
		OutLead = Attacks.Num() > 0 ? static_cast<int32>(Attacks[0].LandStep - Attacks[0].TelegraphStep) : INDEX_NONE;
	};
	int32 LeadFull = 0;
	int32 LeadSlow = 0;
	float WalkFull = 0.0f;
	float WalkSlow = 0.0f;
	Fight(1.0f, LeadFull, WalkFull);
	Fight(Half, LeadSlow, WalkSlow);
	TestTrue(TEXT("the boss telegraphs"), LeadFull > 0);
	TestEqual(TEXT("a boss wind-up does not stretch"), LeadSlow, LeadFull);

	// The walk and the charge run in enemy time; the fight clock does not.
	FHarness Full(5);
	FHarness Slow(5);
	Slow.View.EnemyTimeScale = Half;
	Full.Begin();
	Slow.Begin();
	Full.Adopt();
	Slow.Adopt();
	const FVector2f StartFull = Full.Pool.GetPositions()[Full.HullIndex()];
	const FVector2f StartSlow = Slow.Pool.GetPositions()[Slow.HullIndex()];
	Full.RunSteps(20);
	Slow.RunSteps(20);
	const float MovedFull = FVector2f::Distance(StartFull, Full.Pool.GetPositions()[Full.HullIndex()]);
	const float MovedSlow = FVector2f::Distance(StartSlow, Slow.Pool.GetPositions()[Slow.HullIndex()]);
	TestTrue(TEXT("the hull moved"), MovedFull > 0.0f);
	TestTrue(TEXT("at half speed"), FMath::IsNearlyEqual(MovedSlow, MovedFull * Half, 1.0f));
	TestEqual(TEXT("the fight clock runs in sim time"), Slow.Core.GetFightTime(), Full.Core.GetFightTime());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
