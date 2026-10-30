// Director tests (02_Systems/director.md Acceptance, T-DIR-03..16). Drive FDTDirectorCore headless: with a stub that
// keeps or instantly kills every spawn, or with a real FDTEnemyPool (threat actions, arrivals). One world test (T-DIR-03).
// T-DIR-01 needs Bot v1 (Tools/Bot, a later package) and T-DIR-02 is human-measured; both stay PLANNED.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Director; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Director/DTDirector.h"
#include "Director/DTDirectorCore.h"
#include "Enemies/DTEnemyPool.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTDirTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.0f / static_cast<float>(DTConst::SIM_FIXED_HZ);
	/** Distance slack for float positions on a ring. */
	constexpr float RingEps = 0.5f;

	enum class EKill : uint8
	{
		None,   // every spawn stays alive
		All,    // every enemy dies on the step it is stepped (the instant-kill debug build)
		Events  // only beat-tagged enemies die
	};

	struct FSpawnRec
	{
		FDTEnemySpawn Spawn;
		int64 Step = 0;
		double Time = 0.0;
		EDTDirBlock Block = EDTDirBlock::Idle;
		int32 RoleAlive = 0;  // alive of its role (with this step's earlier queued spawns) when it was queued
		int32 TotalAlive = 0;
	};

	struct FMarkerRec
	{
		FDTDropInMarker Marker;
		int64 Step = 0;
		double Time = 0.0;
		EDTDirBlock Block = EDTDirBlock::Idle;
	};

	struct FChestRec
	{
		FVector2f Position = FVector2f::ZeroVector;
		int64 Step = 0;
		double Time = 0.0;
		FVector2f LastKill = FVector2f::ZeroVector; // position of the last kill seen before the chest (this step)
	};

	/** A run, a view, a director and either a stub population or a real enemy pool. Records everything the sink gets. */
	struct FHarness : public IDTDirectorSink
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTDirectorCore Dir;
		FDTEnemyPool Pool;
		bool bPool = false;
		EKill Kill = EKill::None;
		int64 StepIdx = 0;
		uint32 NextStubSerial = 1;

		TArray<FDTEnemySpawn> Queue;  // applied at the start of the next step (the Enemies phase)
		TArray<FDTEnemySpawn> Alive;  // stub population
		TArray<FDTKill> ExtraKills;   // test-injected kills for the next step

		TArray<FSpawnRec> Spawns;
		TArray<FDTGemSpawn> Gems;
		TArray<FChestRec> Chests;
		TArray<double> BossTimes;
		TArray<FDTTelDirBlock> Blocks;
		TArray<FMarkerRec> Markers;
		TSet<int64> SeenLand;
		TArray<double> ThreatTimes;   // threat actions within the threat ring, by run time
		int32 MaxMarkersHarvest = 0;
		int32 MaxMarkersThreat = 0;
		int32 MarkersOutsideBlocks = 0;
		float MaxCredit = 0.0f;
		FVector2f LastKillPos = FVector2f::ZeroVector;

		FHarness(int32 Seed, bool bInPool, EKill InKill, FVector2f Hero = FVector2f::ZeroVector)
			: bPool(bInPool), Kill(InKill)
		{
			Run.Start(Seed, 1);
			View.HeroPositions = { Hero };
			View.HeroAimDirs = { FVector2f(1.0f, 0.0f) };
			View.HeroIntangible = { false };
		}

		void Start() { Dir.StartRun(); }
		void Start(TConstArrayView<DTCat::FTimelineRow> Rows) { Dir.StartRun(Rows); }

		void Count(const TArray<FDTEnemySpawn>& List, EDTEnemyRole Role, int32& OutRole, int32& OutTotal) const
		{
			for (const FDTEnemySpawn& S : List)
			{
				OutRole += S.Role == Role ? 1 : 0;
				++OutTotal;
			}
		}

		// IDTDirectorSink
		virtual void QueueSpawn(const FDTEnemySpawn& Spawn) override
		{
			FSpawnRec& R = Spawns.AddDefaulted_GetRef();
			R.Spawn = Spawn;
			R.Step = StepIdx;
			R.Time = Dir.GetTime();
			R.Block = Dir.GetBlock();
			if (bPool)
			{
				R.RoleAlive = Pool.CountRole(Spawn.Role);
				R.TotalAlive = Pool.Num();
			}
			else
			{
				Count(Alive, Spawn.Role, R.RoleAlive, R.TotalAlive);
			}
			Count(Queue, Spawn.Role, R.RoleAlive, R.TotalAlive);
			Queue.Add(Spawn);
		}
		virtual void SpawnGem(const FDTGemSpawn& Gem) override { Gems.Add(Gem); }
		virtual void SpawnChest(const FVector2f& Position) override
		{
			FChestRec& C = Chests.AddDefaulted_GetRef();
			C.Position = Position;
			C.Step = StepIdx;
			C.Time = Dir.GetTime();
			C.LastKill = LastKillPos;
		}
		virtual void BossTime(float RunSeconds) override { BossTimes.Add(RunSeconds); }
		virtual void BlockEnded(FDTTelDirBlock& Block) override { Blocks.Add(Block); }

		bool ShouldKill(const FDTEnemySpawn& S) const
		{
			return Kill == EKill::All || (Kill == EKill::Events && S.BeatId != INDEX_NONE);
		}

		void Step()
		{
			FDTSimContext Ctx;
			Ctx.StepSeconds = Dt;
			Ctx.StepIndex = StepIdx;
			Ctx.Run = &Run;
			Ctx.View = &View;
			View.Kills.Reset();
			FDTDirectorStepIn In;
			TArray<FVector2f> Threats;
			if (bPool)
			{
				for (const FDTEnemySpawn& S : Queue)
				{
					Pool.Spawn(S, &Run.GetRng().Get(EDTRngStream::Spawn), true);
				}
				Queue.Reset();
				Pool.StepEnemies(Ctx);
				for (const FDTThreatAction& A : Pool.GetStepThreatActions())
				{
					Threats.Add(A.Origin);
				}
				if (Kill != EKill::None)
				{
					const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
					const TConstArrayView<int32> Beats = Pool.GetBeatIds();
					for (int32 Index = 0; Index < Pool.Num(); ++Index)
					{
						if (Kill == EKill::All || Beats[Index] != INDEX_NONE)
						{
							FDTHit& Hit = View.Hits.AddDefaulted_GetRef();
							Hit.EnemyIndex = Index;
							Hit.EnemySerial = Pool.GetSerials()[Index];
							Hit.OwnerId = 0;
							Hit.Damage = Pool.GetMaxHp()[Index] * 10.0f + 1.0f;
							Hit.Position = Pos[Index];
							Hit.bPierce = true;
						}
					}
				}
				Pool.ResolveHits(Ctx);
				View.HeroHits.Reset(); // an invulnerable hero
				for (const EDTEnemyRole Role : Pool.GetRoles())
				{
					++In.AliveByRole[static_cast<int32>(Role)];
				}
				In.AliveTotal = Pool.Num();
			}
			else
			{
				for (const FDTEnemySpawn& S : Queue)
				{
					if (ShouldKill(S))
					{
						FDTKill& K = View.Kills.AddDefaulted_GetRef();
						K.EnemySerial = NextStubSerial++;
						K.Role = S.Role;
						K.Position = S.Position;
						K.OwnerId = 0;
						K.bElite = S.bElite;
						K.bDirector = true;
						K.BeatId = S.BeatId;
					}
					else
					{
						Alive.Add(S);
					}
				}
				Queue.Reset();
				for (const FDTEnemySpawn& S : Alive)
				{
					++In.AliveByRole[static_cast<int32>(S.Role)];
				}
				In.AliveTotal = Alive.Num();
			}
			View.Kills.Append(ExtraKills);
			ExtraKills.Reset();
			if (View.Kills.Num() > 0)
			{
				LastKillPos = View.Kills.Last().Position;
			}

			In.StepSeconds = Dt;
			In.StepIndex = StepIdx;
			In.Rng = &Run.GetRng().Get(EDTRngStream::Director);
			In.View = &View;
			In.ThreatOrigins = Threats;
			const double Before = Dir.GetTime();
			Dir.Step(In, *this);

			for (const FVector2f& O : Threats)
			{
				if (FVector2f::Distance(O, View.HeroPositions[0]) <= DTConst::DIR_THREAT_RING_MAX_UU)
				{
					ThreatTimes.Add(Before);
				}
			}
			for (const FDTDropInMarker& M : View.DropInMarkers)
			{
				if (!SeenLand.Contains(M.LandStep))
				{
					SeenLand.Add(M.LandStep);
					FMarkerRec& R = Markers.AddDefaulted_GetRef();
					R.Marker = M;
					R.Step = StepIdx;
					R.Time = Before;
					R.Block = Dir.GetBlock();
				}
			}
			const EDTDirBlock B = Dir.GetBlock();
			if (B == EDTDirBlock::Harvest)
			{
				MaxMarkersHarvest = FMath::Max(MaxMarkersHarvest, View.DropInMarkers.Num());
			}
			else if (B == EDTDirBlock::Threat)
			{
				MaxMarkersThreat = FMath::Max(MaxMarkersThreat, View.DropInMarkers.Num());
			}
			else
			{
				MarkersOutsideBlocks += View.DropInMarkers.Num();
			}
			MaxCredit = FMath::Max(MaxCredit, Dir.GetCredit());
			++StepIdx;
		}

		/** Steps until the director clock reaches Seconds. */
		void RunTo(double Seconds)
		{
			while (Dir.GetTime() < Seconds - 0.5 * Dt)
			{
				Step();
			}
		}

		/** The whole run plus a few seconds past boss time. */
		void RunAll()
		{
			RunTo(Dir.GetBossSeconds() + 3.0);
		}
	};

	static float RowThreatShare(const DTCat::FTimelineRow& Row)
	{
		const float Sum = Row.Weights[0] + Row.Weights[1] + Row.Weights[2] + Row.Weights[3];
		return Sum > 0.0f ? (Row.Weights[2] + Row.Weights[3]) / Sum : 0.0f;
	}

	static bool OnBand(const FVector2f& P, const FVector2f& Hero, float MinR, float MaxR)
	{
		const float D = FVector2f::Distance(P, Hero);
		return D >= MinR - RingEps && D <= MaxR + RingEps;
	}

	static bool AtArenaEdge(const FVector2f& P, EDTEnemyRole Role)
	{
		const float Limit = DTConst::ARENA_HALF_EXTENT_UU - FDTEnemyRoleDef::Get(Role).Radius;
		return FMath::IsNearlyEqual(FMath::Abs(P.X), Limit, 0.01f) || FMath::IsNearlyEqual(FMath::Abs(P.Y), Limit, 0.01f);
	}

	static int32 NumThreatRows()
	{
		int32 N = 0;
		for (const DTCat::FTimelineRow& Row : DTCat::TimelineArena1)
		{
			N += Row.Phase == DTCat::ETimelinePhase::Threat ? 1 : 0;
		}
		return N;
	}

	/** One Harvest row at 0 and the boss at BossAt (custom timelines for focused tests). */
	static DTCat::FTimelineRow HarvestRow(float Start, float Rate, int32 Cap, float W0, float W1, float W2, float W3, float DropIn = 0.0f)
	{
		return DTCat::FTimelineRow{ Start, DTCat::ETimelinePhase::Harvest, 0.0f, Rate, Cap, { W0, W1, W2, W3 }, DropIn, 0.7f, 0.3f,
			DTCat::ETimelineEvent::None, 0 };
	}

	static DTCat::FTimelineRow BossRow(float Start)
	{
		return DTCat::FTimelineRow{ Start, DTCat::ETimelinePhase::Boss, 0.0f, 0.0f, DTConst::ENEMIES_CAP, { 0.0f, 0.0f, 0.0f, 0.0f },
			0.0f, 0.0f, 0.0f, DTCat::ETimelineEvent::Boss, 1 };
	}

	/** A headless game world with every subsystem up and begun (T-DIR-03). */
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTDirectorTestWorld")));
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}

		~FTestWorld()
		{
			for (AActor* Actor : FActorRange(World))
			{
				if (Actor)
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirClockAndQueue, "Dusktracer.Director.ClockAndQueue", DTDirTest::Flags) // T-DIR-03
bool FDTDirClockAndQueue::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	if (!UDTDirector::IsEnabledByCVar())
	{
		AddWarning(TEXT("dt.Director.Enabled is 0 in this process; ClockAndQueue skipped"));
		return true;
	}
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTDirector* Director = W.World->GetSubsystem<UDTDirector>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("director"), Director) || !TestNotNull(TEXT("enemies"), Enemies))
	{
		return false;
	}
	Sim->StartRun(11, 1);
	Sim->StepNow(30);
	TestEqual(TEXT("before StartRun the director is idle: no spawn"), Enemies->GetLiveCount(), 0);
	TestFalse(TEXT("not running before StartRun"), Director->GetCore().IsRunning());

	Sim->StartRun(11, 1);
	Director->StartRun();
	Sim->StepNow(1);
	TestTrue(TEXT("running after the first step of the run"), Director->GetCore().IsRunning());
	TestEqual(TEXT("the clock is one step"), Director->GetCore().GetTime(), static_cast<double>(Dt), 1e-6);

	// A paused sim (no step) advances nothing, whatever the wall time.
	const double Held = Director->GetCore().GetTime();
	Sim->Tick(0.0f);
	TestEqual(TEXT("a zero-length frame runs no step"), Director->GetCore().GetTime(), Held, 1e-9);

	// The first regular spawn is queued in a Director phase and appears in the next Enemies phase.
	auto Spawned = [Director]()
	{
		int32 N = 0;
		for (const int32 V : Director->GetCore().GetTotals().Spawned)
		{
			N += V;
		}
		return N;
	};
	int32 Guard = 0;
	while (Spawned() == 0 && Guard++ < 10 * DTConst::SIM_FIXED_HZ)
	{
		Sim->StepNow(1);
	}
	TestEqual(TEXT("one spawn queued"), Spawned(), 1);
	TestEqual(TEXT("not in the pool in the step it was queued"), Enemies->GetLiveCount(), 0);
	Sim->StepNow(1);
	TestEqual(TEXT("in the pool after the next Enemies phase"), Enemies->GetLiveCount(), 1);
	if (Enemies->GetLiveCount() == 1)
	{
		TestTrue(TEXT("marked Director"), (Enemies->GetPool().GetFlags()[0] & DTEnemyFlags::Director) != 0);
	}
	const int64 Steps = Sim->GetStepIndex();
	TestEqual(TEXT("the clock follows sim steps, not wall time"), Director->GetCore().GetTime(), static_cast<double>(Steps) * Dt, 1e-4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirBlocks, "Dusktracer.Director.Blocks", DTDirTest::Flags) // T-DIR-04
bool FDTDirBlocks::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	// Data: rows sorted; every beat plus its Breath ends before the boss and before the next beat; the boss at C.RUN.LENGTH_TO_BOSS.
	float BossAt = -1.0f;
	float PrevStart = 0.0f;
	float BusyUntil = 0.0f;
	for (const DTCat::FTimelineRow& Row : DTCat::TimelineArena1)
	{
		TestTrue(TEXT("rows sorted by start"), Row.Start_s >= PrevStart);
		PrevStart = Row.Start_s;
		if (Row.Phase == DTCat::ETimelinePhase::Threat)
		{
			TestTrue(FString::Printf(TEXT("beat at %.1f starts after the previous beat and Breath"), Row.Start_s), Row.Start_s >= BusyUntil);
			BusyUntil = Row.Start_s + Row.Duration_s + DTConst::DIR_BREATH_S;
			TestTrue(FString::Printf(TEXT("beat at %.1f plus Breath ends before the boss"), Row.Start_s), BusyUntil < DTConst::RUN_LENGTH_TO_BOSS);
		}
		if (Row.Phase == DTCat::ETimelinePhase::Boss)
		{
			BossAt = Row.Start_s;
		}
	}
	TestEqual(TEXT("the boss row is C.RUN.LENGTH_TO_BOSS"), BossAt, static_cast<float>(DTConst::RUN_LENGTH_TO_BOSS));

	// Nothing dies: every beat ends by its duration, then a Breath of C.DIR.BREATH_S with no regular spawn and no drop-in.
	FHarness H(3, false, EKill::None);
	H.Start();
	H.RunAll();
	int32 Beats = 0;
	for (int32 Index = 0; Index < H.Blocks.Num(); ++Index)
	{
		const FDTTelDirBlock& B = H.Blocks[Index];
		if (B.Phase != TEXT("Threat"))
		{
			continue;
		}
		++Beats;
		TestEqual(TEXT("an uncleared beat ends by its duration"), B.EndedBy, FString(TEXT("duration")));
		if (TestTrue(TEXT("a block follows the beat"), H.Blocks.IsValidIndex(Index + 1)))
		{
			const FDTTelDirBlock& Breath = H.Blocks[Index + 1];
			TestEqual(TEXT("Breath follows every beat"), Breath.Phase, FString(TEXT("Breath")));
			TestEqual(TEXT("Breath lasts C.DIR.BREATH_S"), Breath.EndSeconds - Breath.StartSeconds, DTConst::DIR_BREATH_S, 2.0f * Dt);
			int32 Regular = 0;
			for (const int32 N : Breath.SpawnedByRole)
			{
				Regular += N;
			}
			TestEqual(TEXT("no spawn in Breath"), Regular, 0);
			TestEqual(TEXT("no drop-in in Breath"), Breath.DropIns, 0);
		}
	}
	TestEqual(TEXT("every Threat row ran as a beat"), Beats, NumThreatRows());
	for (const FSpawnRec& S : H.Spawns)
	{
		if (S.Block == EDTDirBlock::Breath)
		{
			AddError(FString::Printf(TEXT("spawn queued in Breath at %.2f s"), S.Time));
			break;
		}
	}

	// Event enemies all dead: the beat ends that step ("cleared").
	FHarness K(3, false, EKill::All); // regular enemies die too, so the cap never refuses the event
	K.Start();
	K.RunAll();
	for (const FDTTelDirBlock& B : K.Blocks)
	{
		if (B.Phase == TEXT("Threat"))
		{
			TestEqual(TEXT("a beat whose event enemies die ends cleared"), B.EndedBy, FString(TEXT("cleared")));
			TestTrue(TEXT("cleared within a few steps of its start"), B.EndSeconds - B.StartSeconds <= 3.0f * Dt);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirSpawns, "Dusktracer.Director.Spawns", DTDirTest::Flags) // T-DIR-05
bool FDTDirSpawns::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	for (const FVector2f Hero : { FVector2f::ZeroVector, FVector2f(DTConst::ARENA_HALF_EXTENT_UU - 200.0f, 0.0f) })
	{
		FHarness H(5, false, EKill::None, Hero);
		H.Start();
		H.RunTo(500.0);
		TestTrue(TEXT("the credit never exceeds one"), H.MaxCredit <= 1.0f);
		int32 Regular = 0;
		int32 Clamped = 0;
		for (const FSpawnRec& S : H.Spawns)
		{
			if (S.Spawn.BeatId != INDEX_NONE || S.Spawn.bArrival)
			{
				continue; // events and drop-ins are not regular spawns
			}
			++Regular;
			if (FDTDirectorCore::IsAtShareCap(S.Spawn.Role, S.RoleAlive, S.TotalAlive))
			{
				AddError(FString::Printf(TEXT("role %d drawn at its share cap (%d of %d) at %.2f s"), static_cast<int32>(S.Spawn.Role),
					S.RoleAlive, S.TotalAlive, S.Time));
				break;
			}
			const bool bThreatRing = S.Spawn.Role == EDTEnemyRole::Turret || (S.Block == EDTDirBlock::Threat && S.Spawn.Role == EDTEnemyRole::Rusher);
			const float MinR = bThreatRing ? DTConst::DIR_THREAT_RING_MIN_UU : DTConst::DIR_SPAWN_RING_MIN_UU;
			const float MaxR = bThreatRing ? DTConst::DIR_THREAT_RING_MAX_UU : DTConst::DIR_SPAWN_RING_MAX_UU;
			if (!OnBand(S.Spawn.Position, Hero, MinR, MaxR))
			{
				if (AtArenaEdge(S.Spawn.Position, S.Spawn.Role))
				{
					++Clamped;
				}
				else
				{
					AddError(FString::Printf(TEXT("spawn at (%.0f, %.0f) neither on its ring nor moved inside the arena"), S.Spawn.Position.X, S.Spawn.Position.Y));
					break;
				}
			}
		}
		TestTrue(TEXT("regular spawns happened"), Regular > 100);
		if (!Hero.IsZero())
		{
			TestTrue(TEXT("a hero at the edge gets spawns moved inside the arena"), Clamped > 0);
		}
	}

	// At the alive cap nothing spawns and nothing is banked.
	const DTCat::FTimelineRow Rows[] = { DTDirTest::HarvestRow(0.0f, 30.0f, 10, 1.0f, 0.0f, 0.0f, 0.0f), DTDirTest::BossRow(60.0f) };
	FHarness C(5, false, EKill::None);
	C.Start(Rows);
	C.RunTo(10.0);
	TestEqual(TEXT("the population stops at the row cap"), C.Alive.Num() + C.Queue.Num(), 10);
	const int32 Skipped = C.Dir.GetTotals().SkippedAtCap;
	TestTrue(TEXT("credits at the cap are spent (counted as skipped)"), Skipped > 0);
	C.Alive.Reset(); // a field cleared at once: no banked burst
	C.Step();
	TestTrue(TEXT("at most one spawn per step after a full field clears"), C.Queue.Num() <= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirThreatPlacement, "Dusktracer.Director.ThreatPlacement", DTDirTest::Flags) // T-DIR-06
bool FDTDirThreatPlacement::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	for (const DTCat::FTimelineRow& Row : DTCat::TimelineArena1)
	{
		if (Row.Phase == DTCat::ETimelinePhase::Threat)
		{
			TestTrue(FString::Printf(TEXT("beat at %.1f: Rusher + Turret >= C.DIR.THREAT_SHARE_MIN"), Row.Start_s),
				RowThreatShare(Row) >= DTConst::DIR_THREAT_SHARE_MIN);
		}
	}
	FHarness H(6, false, EKill::All);
	H.Start();
	H.RunAll();
	int32 Checked = 0;
	for (const FSpawnRec& S : H.Spawns)
	{
		const bool bThreatRole = S.Spawn.Role == EDTEnemyRole::Turret || S.Spawn.Role == EDTEnemyRole::Rusher;
		const bool bMust = S.Spawn.Role == EDTEnemyRole::Turret || (bThreatRole && S.Block == EDTDirBlock::Threat) || S.Spawn.bArrival;
		if (!bMust || S.Spawn.Role == EDTEnemyRole::Swarm || S.Spawn.Role == EDTEnemyRole::Tank)
		{
			continue;
		}
		++Checked;
		if (!OnBand(S.Spawn.Position, H.View.HeroPositions[0], DTConst::DIR_THREAT_RING_MIN_UU, DTConst::DIR_THREAT_RING_MAX_UU))
		{
			AddError(FString::Printf(TEXT("%s spawn at %.2f s off the threat ring (%.0f uu)"), S.Spawn.Role == EDTEnemyRole::Turret ? TEXT("Turret") : TEXT("Rusher"),
				S.Time, FVector2f::Distance(S.Spawn.Position, H.View.HeroPositions[0])));
			break;
		}
	}
	TestTrue(TEXT("threat-role spawns were checked"), Checked > 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirDropIn, "Dusktracer.Director.DropIn", DTDirTest::Flags) // T-DIR-07
bool FDTDirDropIn::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	const int32 Lead = FMath::RoundToInt(DTConst::DIR_DROPIN_LEAD_S * DTConst::SIM_FIXED_HZ);
	FHarness H(7, true, EKill::None);
	H.Start();
	H.RunTo(275.0);
	// Run until a marker is up, then check the pool has nothing there (bullets fired at it hit nothing).
	int32 Guard = 0;
	while (H.View.DropInMarkers.Num() == 0 && Guard++ < 30 * DTConst::SIM_FIXED_HZ)
	{
		H.Step();
	}
	if (!TestEqual(TEXT("a marker is published"), H.View.DropInMarkers.Num(), 1))
	{
		return false;
	}
	const FDTDropInMarker Marker = H.View.DropInMarkers[0];
	TestEqual(TEXT("it lands C.DIR.DROPIN_LEAD_S after publication"), Marker.LandStep - (H.StepIdx - 1), static_cast<int64>(Lead));
	int32 Near = 0;
	for (const FVector2f& P : H.Pool.GetPositions())
	{
		Near += FVector2f::Distance(P, Marker.Position) < 1.0f ? 1 : 0;
	}
	TestEqual(TEXT("before it lands a drop-in is not in the pool (nothing to hit)"), Near, 0);
	const int32 Before = H.Spawns.Num();
	while (H.StepIdx <= Marker.LandStep)
	{
		H.Step();
	}
	bool bFound = false;
	for (int32 Index = Before; Index < H.Spawns.Num(); ++Index)
	{
		const FSpawnRec& S = H.Spawns[Index];
		if (S.Spawn.bArrival)
		{
			bFound = true;
			TestEqual(TEXT("it spawns at its landing step"), S.Step, Marker.LandStep);
			TestTrue(TEXT("at the marker"), S.Spawn.Position.Equals(Marker.Position));
			TestTrue(TEXT("of the marker's role"), S.Spawn.Role == Marker.Role);
		}
	}
	TestTrue(TEXT("the drop-in spawned flagged Arrival"), bFound);
	H.Step();
	bool bInPool = false;
	for (int32 Index = 0; Index < H.Pool.Num(); ++Index)
	{
		bInPool |= (H.Pool.GetFlags()[Index] & DTEnemyFlags::Arrival) != 0 && FVector2f::Distance(H.Pool.GetPositions()[Index], Marker.Position) < 200.0f;
	}
	TestTrue(TEXT("the arrival is in the pool next step"), bInPool);

	// Cadence: the Harvest interval outside a beat, divided by C.DIR.DROPIN_THREAT_MULT inside one (no kills: beats last).
	FHarness C(7, false, EKill::None);
	C.Start();
	C.RunAll();
	// Between 270 and 337.5 the row's interval is 12 s; inside the SwarmRing beat (337.5-367.5) it is 6 s.
	int32 Harvest12 = 0;
	int32 Threat6 = 0;
	for (int32 Index = 1; Index < C.Markers.Num(); ++Index)
	{
		const FMarkerRec& A = C.Markers[Index - 1];
		const FMarkerRec& B = C.Markers[Index];
		const double Gap = B.Time - A.Time;
		if (A.Time >= 270.0 && B.Time < 337.5 && A.Block == EDTDirBlock::Harvest && B.Block == EDTDirBlock::Harvest)
		{
			Harvest12 += FMath::IsNearlyEqual(Gap, 12.0, 2.0 * Dt) ? 1 : 0;
			TestEqual(TEXT("Harvest cadence = the row's dropin_interval_s"), Gap, 12.0, 2.0 * Dt);
		}
		if (A.Time >= 337.5 && B.Time < 367.5 && A.Block == EDTDirBlock::Threat && B.Block == EDTDirBlock::Threat)
		{
			Threat6 += FMath::IsNearlyEqual(Gap, 6.0, 2.0 * Dt) ? 1 : 0;
			TestEqual(TEXT("beat cadence = interval / C.DIR.DROPIN_THREAT_MULT"), Gap, 12.0 / DTConst::DIR_DROPIN_THREAT_MULT, 2.0 * Dt);
		}
	}
	TestTrue(TEXT("Harvest gaps measured"), Harvest12 >= 3);
	TestTrue(TEXT("beat gaps measured"), Threat6 >= 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirDropInFairness, "Dusktracer.Director.DropInFairness", DTDirTest::Flags) // T-DIR-08
bool FDTDirDropInFairness::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	// Shipped timeline, every enemy killed at once and nobody killed: limits hold in both.
	for (const EKill Mode : { EKill::All, EKill::None })
	{
		FHarness H(8, false, Mode);
		H.Start();
		H.RunAll();
		TestTrue(TEXT("never more live markers than C.DIR.DROPIN_ACTIVE_MAX in Harvest"), H.MaxMarkersHarvest <= DTConst::DIR_DROPIN_ACTIVE_MAX);
		TestTrue(TEXT("never more than C.DIR.DROPIN_ACTIVE_MAX_THREAT in a beat"), H.MaxMarkersThreat <= DTConst::DIR_DROPIN_ACTIVE_MAX_THREAT);
		TestEqual(TEXT("no live marker in Breath or from boss time"), H.MarkersOutsideBlocks, 0);
		TArray<int64> Lands;
		for (const FSpawnRec& S : H.Spawns)
		{
			if (S.Spawn.bArrival)
			{
				Lands.Add(S.Step);
				if (S.Time >= H.Dir.GetBossSeconds())
				{
					AddError(TEXT("a drop-in landed from boss time on"));
				}
			}
		}
		for (int32 Index = 1; Index < Lands.Num(); ++Index)
		{
			if ((Lands[Index] - Lands[Index - 1]) * Dt < DTConst::DIR_DROPIN_GAP_MIN_S - 0.5f * Dt)
			{
				AddError(FString::Printf(TEXT("landings %.2f s apart"), (Lands[Index] - Lands[Index - 1]) * Dt));
				break;
			}
		}
		for (const FDTTelDirBlock& B : H.Blocks)
		{
			if (B.Phase == TEXT("Breath"))
			{
				TestEqual(TEXT("no drop-in published in Breath"), B.DropIns, 0);
			}
		}
		TestTrue(TEXT("drop-ins happened"), Lands.Num() > 20);
	}

	// A cadence faster than the limits allow: due drop-ins wait, only one waits, and nothing is banked.
	const float Interval = 0.25f;
	const DTCat::FTimelineRow Rows[] = { DTDirTest::HarvestRow(0.0f, 0.0f, 10, 1.0f, 0.0f, 0.0f, 0.0f, Interval), DTDirTest::BossRow(120.0f) };
	FHarness F(8, false, EKill::All);
	F.Start(Rows);
	F.RunTo(60.0);
	const int32 Published = F.Dir.GetTotals().DropInsPublished;
	const double Spacing = FMath::Max<double>(DTConst::DIR_DROPIN_GAP_MIN_S, DTConst::DIR_DROPIN_LEAD_S);
	TestTrue(FString::Printf(TEXT("the limits pace the drop-ins (%d in 60 s, at most %.0f)"), Published, 60.0 / Spacing + 1.0),
		Published <= FMath::FloorToInt(60.0 / Spacing) + 1);
	TestTrue(TEXT("far fewer than the raw cadence: due ones were not banked"), Published < FMath::FloorToInt(60.0f / Interval) / 2);
	// After the wait, the next landing comes as soon as the limits allow: a backlog would show as more than one at once.
	TestTrue(TEXT("at most one live marker"), F.MaxMarkersHarvest <= DTConst::DIR_DROPIN_ACTIVE_MAX);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirFullRun, "Dusktracer.Director.KillHorizon", DTDirTest::Flags) // T-DIR-09 (full headless run)
bool FDTDirFullRun::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	// A whole run on the real enemy pool, every enemy killed on the step it lands, an invulnerable hero.
	FHarness H(42, true, EKill::All);
	H.Start();
	H.RunAll();

	// Block sequence: Harvest ... (Threat, Breath, Harvest)* ... Boss; Breath after each beat.
	FString Seq;
	int32 Threats = 0;
	for (int32 Index = 0; Index < H.Blocks.Num(); ++Index)
	{
		const FDTTelDirBlock& B = H.Blocks[Index];
		Seq += B.Phase.Left(1);
		if (B.Phase == TEXT("Threat"))
		{
			++Threats;
			TestTrue(TEXT("Breath after each beat"), H.Blocks.IsValidIndex(Index + 1) && H.Blocks[Index + 1].Phase == TEXT("Breath"));
		}
		if (B.Phase == TEXT("Breath"))
		{
			TestTrue(TEXT("Harvest after each Breath"), H.Blocks.IsValidIndex(Index + 1) && H.Blocks[Index + 1].Phase == TEXT("Harvest"));
		}
	}
	AddInfo(FString::Printf(TEXT("block sequence %s"), *Seq));
	TestEqual(TEXT("one beat per Threat row"), Threats, NumThreatRows());
	TestTrue(TEXT("starts in Harvest"), Seq.StartsWith(TEXT("H")));
	TestTrue(TEXT("ends with the boss request"), Seq.EndsWith(TEXT("B")));
	TestFalse(TEXT("a beat never follows a beat"), Seq.Contains(TEXT("TT")));

	// Kill horizon (R-DIR-09): threat actions within the threat ring per minute over the band window.
	int32 InWindow = 0;
	for (const double T : H.ThreatTimes)
	{
		InWindow += (T >= DTConst::DIR_BAND_WINDOW_S_MIN && T < DTConst::DIR_BAND_WINDOW_S_MAX) ? 1 : 0;
	}
	const double PerMin = InWindow / ((DTConst::DIR_BAND_WINDOW_S_MAX - DTConst::DIR_BAND_WINDOW_S_MIN) / 60.0);
	AddInfo(FString::Printf(TEXT("threat actions in the band window: %d (%.1f per min); drop-ins %d"), InWindow, PerMin, H.Dir.GetTotals().DropInsLanded));
	TestTrue(FString::Printf(TEXT("threat actions per minute %.1f >= C.DIR.THREAT_ACTIONS_MIN"), PerMin), PerMin >= DTConst::DIR_THREAT_ACTIONS_MIN);

	// Drop-in cadence by run time: none before the first Harvest row with an interval, then about one per interval.
	double FirstInterval = -1.0;
	for (const DTCat::FTimelineRow& Row : DTCat::TimelineArena1)
	{
		if (Row.Phase == DTCat::ETimelinePhase::Harvest && Row.DropInInterval_s > 0.0f)
		{
			FirstInterval = Row.Start_s;
			break;
		}
	}
	TestTrue(TEXT("drop-ins happened"), H.Markers.Num() > 0);
	if (H.Markers.Num() > 0)
	{
		TestTrue(TEXT("no drop-in before the first row with dropin_interval_s"), H.Markers[0].Time >= FirstInterval);
	}

	// Chests (R-DIR-16) and the boss (R-DIR-13).
	TestEqual(TEXT("chests = C.OFFER.BUDGET_CHEST_HANDS"), H.Chests.Num(), DTConst::OFFER_BUDGET_CHEST_HANDS);
	if (TestEqual(TEXT("one boss time event"), H.BossTimes.Num(), 1))
	{
		TestEqual(TEXT("at C.RUN.LENGTH_TO_BOSS"), static_cast<double>(H.BossTimes[0]), static_cast<double>(DTConst::RUN_LENGTH_TO_BOSS), 1.5 * Dt);
	}

	// Determinism: the same seed gives the same spawn and drop-in sequence.
	FHarness Again(42, true, EKill::All);
	Again.Start();
	Again.RunAll();
	bool bSame = Again.Spawns.Num() == H.Spawns.Num() && Again.Markers.Num() == H.Markers.Num();
	for (int32 Index = 0; bSame && Index < H.Spawns.Num(); ++Index)
	{
		const FSpawnRec& A = H.Spawns[Index];
		const FSpawnRec& B = Again.Spawns[Index];
		bSame = A.Step == B.Step && A.Spawn.Role == B.Spawn.Role && A.Spawn.Position == B.Spawn.Position && A.Spawn.bElite == B.Spawn.bElite
			&& A.Spawn.bArrival == B.Spawn.bArrival && A.Spawn.BeatId == B.Spawn.BeatId;
	}
	for (int32 Index = 0; bSame && Index < H.Markers.Num(); ++Index)
	{
		bSame = H.Markers[Index].Marker.LandStep == Again.Markers[Index].Marker.LandStep && H.Markers[Index].Marker.Position == Again.Markers[Index].Marker.Position;
	}
	TestTrue(TEXT("same seed: identical spawn and drop-in sequence"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirScaling, "Dusktracer.Director.Scaling", DTDirTest::Flags) // T-DIR-10
bool FDTDirScaling::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	TestEqual(TEXT("HP scale at 10 min"), FDTDirectorCore::HpScaleAt(600.0), 1.0f + DTConst::SCALE_HP_PER_MIN * 10.0f, 1e-5f);
	TestEqual(TEXT("damage scale at 7.5 min"), FDTDirectorCore::DmgScaleAt(450.0), 1.0f + DTConst::SCALE_DMG_PER_MIN * 7.5f, 1e-5f);

	FHarness H(10, true, EKill::None);
	H.Start();
	H.RunTo(30.0);
	for (const FSpawnRec& S : H.Spawns)
	{
		if (!FMath::IsNearlyEqual(S.Spawn.HpScale, FDTDirectorCore::HpScaleAt(S.Time), 1e-5f)
			|| !FMath::IsNearlyEqual(S.Spawn.DmgScale, FDTDirectorCore::DmgScaleAt(S.Time), 1e-5f))
		{
			AddError(FString::Printf(TEXT("spawn at %.2f s stamped %.4f / %.4f"), S.Time, S.Spawn.HpScale, S.Spawn.DmgScale));
			break;
		}
	}
	if (!TestTrue(TEXT("enemies alive at 30 s"), H.Pool.Num() > 0))
	{
		return false;
	}
	const uint32 Serial = H.Pool.GetSerials()[0];
	const float Early = H.Pool.GetHpScales()[0];
	const float EarlyDmg = H.Pool.GetDmgScales()[0];
	H.RunTo(150.0);
	int32 Found = INDEX_NONE;
	for (int32 Index = 0; Index < H.Pool.Num(); ++Index)
	{
		Found = H.Pool.GetSerials()[Index] == Serial ? Index : Found;
	}
	if (TestTrue(TEXT("the early enemy is still alive"), Found != INDEX_NONE))
	{
		TestEqual(TEXT("it keeps its HP scale"), H.Pool.GetHpScales()[Found], Early);
		TestEqual(TEXT("it keeps its damage scale"), H.Pool.GetDmgScales()[Found], EarlyDmg);
	}
	const FSpawnRec& Late = H.Spawns.Last();
	TestTrue(TEXT("a later spawn carries a larger scale"), Late.Spawn.HpScale > Early && Late.Spawn.DmgScale > EarlyDmg);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirEvents, "Dusktracer.Director.Events", DTDirTest::Flags) // T-DIR-11
bool FDTDirEvents::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	FHarness H(11, false, EKill::All); // regular enemies die too, so the cap never refuses an event
	H.Start();
	H.RunAll();
	int32 Beat = 0;
	for (const DTCat::FTimelineRow& Row : DTCat::TimelineArena1)
	{
		if (Row.Phase != DTCat::ETimelinePhase::Threat)
		{
			continue;
		}
		int32 Count = 0;
		for (const FSpawnRec& S : H.Spawns)
		{
			if (S.Spawn.BeatId != Beat)
			{
				continue;
			}
			++Count;
			const float D = FVector2f::Distance(S.Spawn.Position, H.View.HeroPositions[0]);
			switch (Row.Event)
			{
			case DTCat::ETimelineEvent::EliteWave:
				TestTrue(TEXT("EliteWave spawns elites"), S.Spawn.bElite);
				TestTrue(TEXT("on the threat ring"), D >= DTConst::DIR_THREAT_RING_MIN_UU - RingEps && D <= DTConst::DIR_THREAT_RING_MAX_UU + RingEps);
				break;
			case DTCat::ETimelineEvent::SwarmRing:
				TestTrue(TEXT("SwarmRing spawns swarm"), S.Spawn.Role == EDTEnemyRole::Swarm);
				TestEqual(TEXT("at the threat ring's inner radius"), D, static_cast<float>(DTConst::DIR_THREAT_RING_MIN_UU), RingEps);
				break;
			case DTCat::ETimelineEvent::RusherStampede:
				TestTrue(TEXT("RusherStampede spawns rushers"), S.Spawn.Role == EDTEnemyRole::Rusher);
				TestTrue(TEXT("on the threat ring"), D >= DTConst::DIR_THREAT_RING_MIN_UU - RingEps && D <= DTConst::DIR_THREAT_RING_MAX_UU + RingEps);
				break;
			case DTCat::ETimelineEvent::TurretNest:
				TestTrue(TEXT("TurretNest spawns turrets"), S.Spawn.Role == EDTEnemyRole::Turret);
				TestTrue(TEXT("on the threat ring"), D >= DTConst::DIR_THREAT_RING_MIN_UU - RingEps && D <= DTConst::DIR_THREAT_RING_MAX_UU + RingEps);
				break;
			default:
				break;
			}
		}
		TestEqual(FString::Printf(TEXT("beat %d spawns event_count enemies tagged with the beat"), Beat), Count, Row.EventCount);
		++Beat;
	}
	int32 Cleared = 0;
	for (const FDTTelDirBlock& B : H.Blocks)
	{
		Cleared += (B.Phase == TEXT("Threat") && B.EndedBy == TEXT("cleared")) ? 1 : 0;
	}
	TestEqual(TEXT("killing every event enemy ends each beat early"), Cleared, NumThreatRows());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirGemDrops, "Dusktracer.Director.GemDrops", DTDirTest::Flags) // T-DIR-12
bool FDTDirGemDrops::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	const DTCat::FTimelineRow Rows[] = { DTDirTest::HarvestRow(0.0f, 0.0f, 10, 1.0f, 0.0f, 0.0f, 0.0f), DTDirTest::BossRow(120.0f) };
	FHarness H(12, false, EKill::None);
	H.Start(Rows);
	H.Step();
	struct FCase { EDTEnemyRole Role; bool bElite; bool bDirector; int32 Expected; };
	const FCase Cases[] = {
		{ EDTEnemyRole::Swarm, false, true, DTConst::DROP_XP_SWARM },
		{ EDTEnemyRole::Tank, false, true, DTConst::DROP_XP_TANK },
		{ EDTEnemyRole::Rusher, false, true, DTConst::DROP_XP_RUSHER },
		{ EDTEnemyRole::Turret, false, true, DTConst::DROP_XP_TURRET },
		{ EDTEnemyRole::Tank, true, true, DTConst::DROP_XP_TANK * DTConst::DROP_XP_ELITE_MULT },
		{ EDTEnemyRole::Swarm, false, false, 0 },
	};
	int32 Expected = 0;
	for (const FCase& C : Cases)
	{
		FDTKill& K = H.ExtraKills.AddDefaulted_GetRef();
		K.Role = C.Role;
		K.bElite = C.bElite;
		K.bDirector = C.bDirector;
		K.OwnerId = 0;
		K.Position = FVector2f(100.0f * (Expected + 1), 50.0f);
		Expected += C.bDirector ? 1 : 0;
	}
	H.Step();
	if (TestEqual(TEXT("one gem per director kill, none for debug kills"), H.Gems.Num(), Expected))
	{
		for (int32 Index = 0; Index < Expected; ++Index)
		{
			TestEqual(TEXT("gem value by role (elite factor)"), H.Gems[Index].Value, Cases[Index].Expected);
			TestTrue(TEXT("gem at the death position"), H.Gems[Index].Position.Equals(H.View.Kills[Index].Position));
			TestEqual(TEXT("owned by the killer"), H.Gems[Index].OwnerId, 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirChests, "Dusktracer.Director.Chests", DTDirTest::Flags) // T-DIR-16
bool FDTDirChests::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	TestEqual(TEXT("the shipped timeline has C.OFFER.BUDGET_CHEST_HANDS Threat rows"), NumThreatRows(), DTConst::OFFER_BUDGET_CHEST_HANDS);

	// Event enemies die: an EliteWave's chest drops at the last elite's death; other beats' at the Breath start.
	FHarness K(16, false, EKill::Events);
	K.Start();
	K.RunAll();
	TestEqual(TEXT("one chest per beat"), K.Chests.Num(), NumThreatRows());
	int32 Beat = 0;
	for (const DTCat::FTimelineRow& Row : DTCat::TimelineArena1)
	{
		if (Row.Phase != DTCat::ETimelinePhase::Threat || !K.Chests.IsValidIndex(Beat))
		{
			continue;
		}
		const FChestRec& C = K.Chests[Beat];
		if (Row.Event == DTCat::ETimelineEvent::EliteWave)
		{
			TestTrue(TEXT("EliteWave cleared: the chest lies at the last elite's death"), C.Position.Equals(C.LastKill));
		}
		else
		{
			TestEqual(TEXT("other beats: C.DIR.CHEST_DROP_UU from the hero"), FVector2f::Distance(C.Position, K.View.HeroPositions[0]),
				static_cast<float>(DTConst::DIR_CHEST_DROP_UU), RingEps);
		}
		++Beat;
	}

	// Nothing dies (beats end by duration): every chest at the Breath start, at the distance, inside the arena.
	const FVector2f Edge(DTConst::ARENA_HALF_EXTENT_UU - 100.0f, 0.0f);
	FHarness N(16, false, EKill::None, Edge);
	N.Start();
	N.RunAll();
	TestEqual(TEXT("one chest per beat (by duration)"), N.Chests.Num(), NumThreatRows());
	for (const FChestRec& C : N.Chests)
	{
		const float Limit = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU);
		const bool bInside = FMath::Abs(C.Position.X) <= Limit && FMath::Abs(C.Position.Y) <= Limit;
		const bool bAtDistance = FMath::IsNearlyEqual(FVector2f::Distance(C.Position, Edge), static_cast<float>(DTConst::DIR_CHEST_DROP_UU), RingEps);
		TestTrue(TEXT("inside the arena"), bInside);
		TestTrue(TEXT("at C.DIR.CHEST_DROP_UU, or moved inside the arena"), bAtDistance || FMath::IsNearlyEqual(FMath::Abs(C.Position.X), Limit, 0.01f));
	}
	for (const FDTTelDirBlock& B : N.Blocks)
	{
		if (B.Phase == TEXT("Breath"))
		{
			bool bChestAtStart = false;
			for (const FChestRec& C : N.Chests)
			{
				bChestAtStart |= FMath::IsNearlyEqual(static_cast<float>(C.Time), B.StartSeconds, 1e-3f);
			}
			TestTrue(TEXT("the chest comes at the Breath start"), bChestAtStart);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirBossHandoff, "Dusktracer.Director.BossHandoff", DTDirTest::Flags) // T-DIR-13
bool FDTDirBossHandoff::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	FHarness H(13, false, EKill::All);
	H.Start();
	// Stop just before boss time with a marker live, then cross it.
	H.RunTo(DTConst::RUN_LENGTH_TO_BOSS - 2.0);
	int32 Guard = 0;
	while (H.View.DropInMarkers.Num() == 0 && Guard++ < 2 * DTConst::SIM_FIXED_HZ - 2)
	{
		H.Step();
	}
	H.RunTo(DTConst::RUN_LENGTH_TO_BOSS + 10.0);
	if (TestEqual(TEXT("one boss request"), H.BossTimes.Num(), 1))
	{
		TestEqual(TEXT("at boss time"), static_cast<double>(H.BossTimes[0]), static_cast<double>(DTConst::RUN_LENGTH_TO_BOSS), 1.5 * Dt);
	}
	int32 After = 0;
	for (const FSpawnRec& S : H.Spawns)
	{
		After += S.Time >= DTConst::RUN_LENGTH_TO_BOSS ? 1 : 0;
	}
	TestEqual(TEXT("no spawn, beat or drop-in from boss time"), After, 0);
	TestEqual(TEXT("markers cleared"), H.View.DropInMarkers.Num(), 0);
	TestFalse(TEXT("no threat block"), H.View.bThreatBlock);
	TestTrue(TEXT("the director is in its Boss block"), H.Dir.GetBlock() == EDTDirBlock::Boss);
	TestTrue(TEXT("the boss request is a dir_block with phase Boss"), H.Blocks.Num() > 0 && H.Blocks.Last().Phase == TEXT("Boss"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirBanners, "Dusktracer.Director.Banners", DTDirTest::Flags) // T-DIR-17
bool FDTDirBanners::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	FHarness H(17, false, EKill::None);
	H.Start();
	H.RunTo(10.0);
	TestTrue(TEXT("no banner in a Harvest block"), H.Dir.GetBanner() == EDTDirBanner::None);
	const double Beat = DTCat::TimelineArena1[3].Start_s;
	H.RunTo(Beat + 0.5);
	TestTrue(TEXT("the beat's first seconds show Hud.Threat"), H.Dir.GetBlock() == EDTDirBlock::Threat && H.Dir.GetBanner() == EDTDirBanner::Threat);
	H.RunTo(Beat + DTConst::HUD_BANNER_S + 0.5);
	TestTrue(TEXT("the banner ends after C.HUD.BANNER_S, the beat goes on"), H.Dir.GetBlock() == EDTDirBlock::Threat && H.Dir.GetBanner() == EDTDirBanner::None);
	H.RunTo(DTConst::RUN_LENGTH_TO_BOSS + 0.5);
	TestTrue(TEXT("boss time shows Hud.BossIncoming"), H.Dir.GetBanner() == EDTDirBanner::BossIncoming);
	H.RunTo(DTConst::RUN_LENGTH_TO_BOSS + DTConst::HUD_BANNER_S + 0.5);
	TestTrue(TEXT("and it ends"), H.Dir.GetBanner() == EDTDirBanner::None);
	FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(FName(TEXT("ST_UI")));
	FString Text;
	TestTrue(TEXT("Hud.Threat exists"), Table.IsValid() && Table->GetSourceString(TEXT("Hud.Threat"), Text));
	TestTrue(TEXT("Hud.BossIncoming exists"), Table.IsValid() && Table->GetSourceString(TEXT("Hud.BossIncoming"), Text));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirDeterminism, "Dusktracer.Director.Determinism", DTDirTest::Flags) // T-DIR-14
bool FDTDirDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	auto Run = [](int32 Seed, TArray<FSpawnRec>& OutSpawns, TArray<FMarkerRec>& OutMarkers, int32& OutSpawnSeed)
	{
		FHarness H(Seed, false, EKill::All);
		H.Start();
		H.RunTo(600.0);
		OutSpawns = H.Spawns;
		OutMarkers = H.Markers;
		OutSpawnSeed = H.Run.GetRng().Get(EDTRngStream::Spawn).GetCurrentSeed();
	};
	auto Same = [](const TArray<FSpawnRec>& A, const TArray<FSpawnRec>& B, const TArray<FMarkerRec>& MA, const TArray<FMarkerRec>& MB)
	{
		if (A.Num() != B.Num() || MA.Num() != MB.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Step != B[Index].Step || A[Index].Spawn.Role != B[Index].Spawn.Role || A[Index].Spawn.Position != B[Index].Spawn.Position
				|| A[Index].Spawn.bElite != B[Index].Spawn.bElite || A[Index].Spawn.bArrival != B[Index].Spawn.bArrival)
			{
				return false;
			}
		}
		for (int32 Index = 0; Index < MA.Num(); ++Index)
		{
			if (MA[Index].Marker.LandStep != MB[Index].Marker.LandStep || MA[Index].Marker.Position != MB[Index].Marker.Position)
			{
				return false;
			}
		}
		return true;
	};
	TArray<FSpawnRec> A, B, C;
	TArray<FMarkerRec> MA, MB, MC;
	int32 SA = 0, SB = 0, SC = 0;
	Run(77, A, MA, SA);
	Run(77, B, MB, SB);
	Run(78, C, MC, SC);
	TestTrue(TEXT("same seed and inputs: identical spawn and drop-in sequence"), Same(A, B, MA, MB));
	TestFalse(TEXT("another seed differs"), Same(A, C, MA, MC));
	TestEqual(TEXT("director draws leave the Spawn stream untouched"), SA, FDTRngStreams(77).Get(EDTRngStream::Spawn).GetCurrentSeed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDirBlockTelemetry, "Dusktracer.Director.BlockTelemetry", DTDirTest::Flags) // T-DIR-15
bool FDTDirBlockTelemetry::RunTest(const FString& Parameters)
{
	using namespace DTDirTest;
	FHarness H(15, false, EKill::All);
	H.Start();
	H.RunAll();
	const FDTDirectorTotals& T = H.Dir.GetTotals();
	TestEqual(TEXT("one record per ended block"), H.Blocks.Num(), T.Blocks);
	int32 Spawned[static_cast<int32>(EDTEnemyRole::Count)] = {};
	int32 Kills = 0;
	int32 DropIns = 0;
	int32 Threats = 0;
	for (int32 Index = 0; Index < H.Blocks.Num(); ++Index)
	{
		const FDTTelDirBlock& B = H.Blocks[Index];
		TestEqual(TEXT("blocks numbered in order"), B.Block, Index);
		if (Index > 0 && B.Phase != TEXT("Boss"))
		{
			TestEqual(TEXT("blocks are contiguous"), B.StartSeconds, H.Blocks[Index - 1].EndSeconds, 1e-3f);
		}
		for (int32 Role = 0; Role < static_cast<int32>(EDTEnemyRole::Count); ++Role)
		{
			Spawned[Role] += B.SpawnedByRole[Role];
		}
		Kills += B.Kills;
		DropIns += B.DropIns;
		Threats += B.ThreatActions;
		TestTrue(TEXT("ended_by is set"), !B.EndedBy.IsEmpty());
	}
	// Kills after the boss request belong to no block; here every enemy dies the step after it spawns and nothing spawns
	// from boss time, so the blocks sum to the run totals.
	for (int32 Role = 0; Role < static_cast<int32>(EDTEnemyRole::Count); ++Role)
	{
		TestEqual(TEXT("spawned_by_role sums to the run total"), Spawned[Role], T.Spawned[Role]);
	}
	TestEqual(TEXT("drop-ins sum to the run total"), DropIns, T.DropInsPublished);
	TestEqual(TEXT("kills sum to the run total"), Kills, T.Kills);
	TestEqual(TEXT("threat actions sum to the run total"), Threats, T.ThreatActions);
	TestTrue(TEXT("kill horizon median set for a block with kills"), H.Blocks[0].Kills == 0 || H.Blocks[0].KillHorizonP50UU >= 0.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
