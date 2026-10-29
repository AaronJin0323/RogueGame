// Telemetry tests (02_Systems/telemetry.md). Drive FDTTelemetryCore directly with a run state, pools and samples.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Telemetry; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Combat/DTBulletPool.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyPool.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetrySources.h"
#include "Telemetry/DTTelemetryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTTelemetryTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A short interval so a test covers several rows in few steps. */
	constexpr float TestIntervalSeconds = 1.0f;

	static float StepSeconds() { return 1.0f / DTConst::SIM_FIXED_HZ; }
	static int32 StepsPerInterval() { return FMath::RoundToInt32(TestIntervalSeconds / StepSeconds()); }

	static FDTTelemetryConfig MakeConfig(int32 Seed, int32 Players)
	{
		FDTTelemetryConfig Config;
		Config.Seed = Seed;
		Config.NumPlayers = Players;
		Config.StepSeconds = StepSeconds();
		Config.IntervalSeconds = TestIntervalSeconds;
		Config.Build = TEXT("test");
		Config.Culture = TEXT("en");
		Config.WallStart = TEXT("2026-01-01T00:00:00.000Z");
		return Config;
	}

	/** Non-empty lines of Text. */
	static TArray<FString> Lines(const FString& Text)
	{
		TArray<FString> Out;
		Text.ParseIntoArrayLines(Out, true);
		return Out;
	}

	/** The cell of Column in a CSV data row, by the pinned header. */
	static FString Cell(const FString& Row, const TCHAR* Column)
	{
		TArray<FString> Header;
		FDTTelemetryCore::GetCsvHeader().ParseIntoArray(Header, TEXT(","), false);
		TArray<FString> Cells;
		Row.ParseIntoArray(Cells, TEXT(","), false);
		const int32 Index = Header.IndexOfByKey(FString(Column));
		return Cells.IsValidIndex(Index) ? Cells[Index] : FString(TEXT("<missing>"));
	}

	static int32 CountKind(const FString& Jsonl, const TCHAR* Kind)
	{
		const FString Needle = FString::Printf(TEXT("\"kind\":\"%s\""), Kind);
		int32 Count = 0;
		for (const FString& Line : Lines(Jsonl))
		{
			Count += Line.Contains(Needle) ? 1 : 0;
		}
		return Count;
	}

	/** Sum of `kills` over the kill records (one record per player per step carries the step's count, R-TEL-04). */
	static int32 SumKills(const FString& Jsonl)
	{
		const FString Kind = TEXT("\"kind\":\"kill\"");
		const FString Field = TEXT("\"kills\":");
		int32 Sum = 0;
		for (const FString& Line : Lines(Jsonl))
		{
			const int32 At = Line.Contains(Kind) ? Line.Find(Field) : INDEX_NONE;
			Sum += At != INDEX_NONE ? FCString::Atoi(*Line + At + Field.Len()) : 0;
		}
		return Sum;
	}

	/** A small real fight: a swarm ring closing on hero 0 and an emitter at the origin, every system hooked like the subsystems. */
	struct FFight
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTEnemyPool Enemies;
		FDTBulletPool Bullets;
		FDTTelemetryCore Core;
		FString Csv;
		FString Jsonl;

		void Play(int32 Seed, int32 Steps, const FString& Wall)
		{
			Run.Start(Seed, 1);
			Run.SetEventSink(&Core);
			View.HeroPositions = { FVector2f::ZeroVector };
			FDTTelemetryConfig Config = MakeConfig(Seed, 1);
			Config.WallStart = Wall;
			Core.Start(Config);

			FDTSimContext Context;
			Context.StepSeconds = StepSeconds();
			Context.Run = &Run;
			Context.View = &View;
			const float Ring = static_cast<float>(DTConst::ENM_SPAWN_RING_RADIUS_UU);
			const int32 Swarm = 64;
			const int32 Spokes = 32;
			const int32 FireEvery = 4;
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				Context.StepIndex = Step;
				if (Step == 0)
				{
					Enemies.SpawnRing(EDTEnemyRole::Swarm, Swarm, FVector2f::ZeroVector, Ring, Run.GetRng().Get(EDTRngStream::Spawn));
				}
				Core.OnEnemiesSpawned(Step, false, Enemies.Num());
				Enemies.StepEnemies(Context);
				if (Step % FireEvery == 0)
				{
					FDTBulletSpawn Spawn;
					Spawn.Radius = static_cast<float>(DTConst::BULLETS_STRESS_RADIUS_UU);
					Spawn.Damage = static_cast<float>(DTConst::ENM_SWARM_HP) * 0.5f;
					Spawn.Life = DTConst::BULLETS_STRESS_LIFE_S;
					Spawn.OwnerId = 0;
					for (int32 Spoke = 0; Spoke < Spokes; ++Spoke)
					{
						const float Angle = static_cast<float>(UE_TWO_PI) * (static_cast<float>(Spoke) + 0.5f * static_cast<float>(Step % 2)) / static_cast<float>(Spokes);
						Spawn.Velocity = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * static_cast<float>(DTConst::BULLETS_STRESS_SPEED_UU_S);
						Bullets.Spawn(Spawn);
					}
				}
				Bullets.Step(Context.StepSeconds, View);
				for (FDTHit& Hit : View.Hits)
				{
					Hit.SourceId = FDTTelemetrySources::Stress; // what a source-aware emitter will stamp
				}
				Core.BeginResolve(Step, View.Hits);
				Enemies.ResolveHits(Context);
				Core.EndResolve(Enemies.Num());

				FDTTelemetrySample Sample;
				Sample.LiveBullets = Bullets.Num();
				Sample.LiveEnemies = Enemies.Num();
				Sample.RecycledBulletsTotal = Bullets.GetCounters().Recycled;
				Sample.RefusedSpawnsTotal = Enemies.GetRefusedSpawns();
				Sample.Heroes = View.HeroPositions;
				Sample.Enemies = Enemies.GetPositions();
				Core.EndStep(Step, Sample);
			}
			Core.End(TEXT("test"));
			Core.TakeOutput(Csv, Jsonl);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelIntervalSimClock, "Dusktracer.Telemetry.IntervalSimClock", DTTelemetryTest::Flags) // T-TEL-01
bool FDTTelIntervalSimClock::RunTest(const FString& Parameters)
{
	using namespace DTTelemetryTest;
	FDTTelemetryCore Core;
	Core.Start(MakeConfig(7, 1));
	FString Csv;
	FString Jsonl;
	const int32 PerInterval = StepsPerInterval();
	for (int32 Step = 0; Step < PerInterval - 1; ++Step)
	{
		Core.EndStep(Step, FDTTelemetrySample());
	}
	Core.TakeOutput(Csv, Jsonl);
	TestTrue(TEXT("no row one step before the interval ends"), Csv.IsEmpty());

	Core.EndStep(PerInterval - 1, FDTTelemetrySample());
	Core.TakeOutput(Csv, Jsonl);
	TArray<FString> Rows = Lines(Csv);
	TestEqual(TEXT("header + one row on the interval's last step"), Rows.Num(), 2);
	if (Rows.Num() == 2)
	{
		TestEqual(TEXT("run time is sim time"), Cell(Rows[1], TEXT("run_time_s")), FString::Printf(TEXT("%.3f"), TestIntervalSeconds));
		TestEqual(TEXT("first interval is 0"), Cell(Rows[1], TEXT("interval")), FString(TEXT("0")));
	}

	// Two more intervals with no wall time passing between steps: rows follow steps, not the clock on the wall.
	for (int32 Step = PerInterval; Step < 3 * PerInterval; ++Step)
	{
		Core.EndStep(Step, FDTTelemetrySample());
	}
	Core.TakeOutput(Csv, Jsonl);
	Rows = Lines(Csv);
	TestEqual(TEXT("two more rows, no second header"), Rows.Num(), 2);
	if (Rows.Num() == 2)
	{
		TestEqual(TEXT("third row ends at three intervals"), Cell(Rows[1], TEXT("run_time_s")), FString::Printf(TEXT("%.3f"), 3.0f * TestIntervalSeconds));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelPerPlayer, "Dusktracer.Telemetry.PerPlayer", DTTelemetryTest::Flags) // T-TEL-02
bool FDTTelPerPlayer::RunTest(const FString& Parameters)
{
	using namespace DTTelemetryTest;
	FDTRunState Run;
	FDTTelemetryCore Core;
	Run.Start(3, 2);
	Run.SetEventSink(&Core);
	Core.Start(MakeConfig(3, 2));

	for (int32 Kill = 0; Kill < 3; ++Kill)
	{
		Run.ApplyEvent({ EDTGameEventKind::Kill, 0, 0 });
	}
	Run.ApplyEvent({ EDTGameEventKind::Kill, 1, 0 });
	Run.ApplyEvent({ EDTGameEventKind::Hit, 1, 40 });
	Run.ApplyEvent({ EDTGameEventKind::Kill, 5, 0 }); // not a player: rejected, in no row

	FDTTelemetrySample Sample;
	const TArray<int32> Levels = { 1, 2 };
	Sample.Levels = Levels;
	for (int32 Step = 0; Step < StepsPerInterval(); ++Step)
	{
		Core.EndStep(Step, Sample);
	}
	FString Csv;
	FString Jsonl;
	Core.TakeOutput(Csv, Jsonl);
	const TArray<FString> Rows = Lines(Csv);
	TestEqual(TEXT("header + one row per player"), Rows.Num(), 3);
	if (Rows.Num() == 3)
	{
		const double PerMin = DTConst::TEL_RATE_UNIT_S / TestIntervalSeconds;
		TestEqual(TEXT("row 1 is player 0"), Cell(Rows[1], TEXT("player")), FString(TEXT("0")));
		TestEqual(TEXT("row 2 is player 1"), Cell(Rows[2], TEXT("player")), FString(TEXT("1")));
		TestEqual(TEXT("P0 kills"), Cell(Rows[1], TEXT("kills_per_min")), FString::Printf(TEXT("%.3f"), 3.0 * PerMin));
		TestEqual(TEXT("P1 kills"), Cell(Rows[2], TEXT("kills_per_min")), FString::Printf(TEXT("%.3f"), 1.0 * PerMin));
		TestEqual(TEXT("P0 no damage"), Cell(Rows[1], TEXT("damage_dealt_per_min")), FString(TEXT("0.000")));
		TestEqual(TEXT("P1 damage"), Cell(Rows[2], TEXT("damage_dealt_per_min")), FString::Printf(TEXT("%.3f"), 40.0 * PerMin));
		TestEqual(TEXT("P1 level is its own"), Cell(Rows[2], TEXT("level")), FString(TEXT("2")));
	}
	TestEqual(TEXT("four kills, the rejected one is not among them"), SumKills(Jsonl), 4);
	TestEqual(TEXT("one kill record per player for the step"), CountKind(Jsonl, TEXT("kill")), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelSwarmTtk, "Dusktracer.Telemetry.SwarmTtk", DTTelemetryTest::Flags) // T-TEL-03
bool FDTTelSwarmTtk::RunTest(const FString& Parameters)
{
	using namespace DTTelemetryTest;
	FDTRunState Run;
	FDTWorldView View;
	FDTEnemyPool Pool;
	FDTTelemetryCore Core;
	Run.Start(1, 1);
	Run.SetEventSink(&Core);
	Core.Start(MakeConfig(1, 1));
	FDTSimContext Context;
	Context.StepSeconds = StepSeconds();
	Context.Run = &Run;
	Context.View = &View;

	// Slot 0 spawns at step 0, slots 1 and 2 at step 10; at step 30 slots 0 and 2 die (slot 2 through a stale-then-live order).
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f, 0.0f));
	Core.OnEnemiesSpawned(0, false, Pool.Num());
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(200.0f, 0.0f));
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(300.0f, 0.0f));
	Core.OnEnemiesSpawned(10, false, Pool.Num());

	const float Lethal = static_cast<float>(DTConst::ENM_SWARM_HP) * 2.0f;
	auto MakeHit = [](int32 Enemy, float Damage, uint16 Source)
	{
		FDTHit Hit;
		Hit.EnemyIndex = Enemy;
		Hit.OwnerId = 0;
		Hit.Damage = Damage;
		Hit.SourceId = Source;
		return Hit;
	};
	const uint16 Weapon = FDTTelemetrySources::FindOrAdd(TEXT("Weapon.TestLance"));
	View.Hits = { MakeHit(0, Lethal, Weapon), MakeHit(0, Lethal, Weapon), MakeHit(9, Lethal, Weapon), MakeHit(2, Lethal, FDTTelemetrySources::Stress) };
	Context.StepIndex = 30;
	Core.BeginResolve(30, View.Hits);
	Pool.ResolveHits(Context);
	Core.EndResolve(Pool.Num());
	TestEqual(TEXT("mirror stays in sync with the pool"), Core.GetMirrorDesyncs(), static_cast<int64>(0));
	Core.EndStep(30, FDTTelemetrySample());

	// The survivor (spawned at step 10) dies at step 70: its TTK must follow it through the compaction.
	View.Hits = { MakeHit(0, Lethal, Weapon) };
	Context.StepIndex = 70;
	Core.BeginResolve(70, View.Hits);
	Pool.ResolveHits(Context);
	Core.EndResolve(Pool.Num());
	TestEqual(TEXT("still in sync after compaction"), Core.GetMirrorDesyncs(), static_cast<int64>(0));

	Core.EndStep(70, FDTTelemetrySample());
	Core.End(TEXT("test"));
	FString Csv;
	FString Jsonl;
	Core.TakeOutput(Csv, Jsonl);
	// The kill records carry per-step TTK sums (and min/max) of the same per-kill samples the CSV percentile reads.
	const auto Ttk = [](int32 Steps) { return static_cast<float>(static_cast<double>(Steps) * StepSeconds()); };
	TestTrue(TEXT("step 30: slots 0 and 2 lived 30 and 20 steps"), Jsonl.Contains(FString::Printf(
		TEXT("\"ttk_n\":2,\"ttk_sum_s\":%.4f,\"ttk_min_s\":%.4f,\"ttk_max_s\":%.4f}"),
		static_cast<double>(Ttk(30)) + static_cast<double>(Ttk(20)), Ttk(20), Ttk(30))));
	TestTrue(TEXT("step 70: the survivor lived 60 steps"), Jsonl.Contains(FString::Printf(
		TEXT("\"ttk_n\":1,\"ttk_sum_s\":%.4f,\"ttk_min_s\":%.4f,\"ttk_max_s\":%.4f}"), static_cast<double>(Ttk(60)), Ttk(60), Ttk(60))));
	TestEqual(TEXT("three kills, the stale index and the overkill are none"), SumKills(Jsonl), 3);
	TestEqual(TEXT("one kill record per step with kills"), CountKind(Jsonl, TEXT("kill")), 2);
	const TArray<FString> Rows = Lines(Csv);
	TestEqual(TEXT("final partial row written at run end"), Rows.Num(), 2);
	if (Rows.Num() == 2)
	{
		TestEqual(TEXT("swarm TTK is the configured percentile (nearest rank)"), Cell(Rows[1], TEXT("swarm_ttk_s")),
			FString::Printf(TEXT("%.4f"), static_cast<float>(30.0 * StepSeconds())));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelHitsPerStep, "Dusktracer.Telemetry.HitsPerStep", DTTelemetryTest::Flags) // T-TEL-04
bool FDTTelHitsPerStep::RunTest(const FString& Parameters)
{
	using namespace DTTelemetryTest;
	FDTRunState Run;
	FDTTelemetryCore Core;
	Run.Start(2, 2);
	Run.SetEventSink(&Core);
	Core.Start(MakeConfig(2, 2));
	const int32 N = 250;
	for (int32 Hit = 0; Hit < N; ++Hit)
	{
		Run.ApplyEvent({ EDTGameEventKind::Hit, 0, 2 });
	}
	Core.EndStep(0, FDTTelemetrySample());
	Core.EndStep(1, FDTTelemetrySample()); // a step without hits writes no hit record
	FString Csv;
	FString Jsonl;
	Core.TakeOutput(Csv, Jsonl);
	TestEqual(TEXT("N hits in one step -> one record"), CountKind(Jsonl, TEXT("hit")), 1);
	TestTrue(TEXT("the record carries the count and the damage"),
		Jsonl.Contains(FString::Printf(TEXT("\"owner\":0,\"hits\":%d,\"damage\":%d"), N, 2 * N)));

	Run.ApplyEvent({ EDTGameEventKind::Hit, 0, 1 });
	Run.ApplyEvent({ EDTGameEventKind::Hit, 1, 1 });
	Core.EndStep(2, FDTTelemetrySample());
	Core.TakeOutput(Csv, Jsonl);
	TestEqual(TEXT("two owners in one step -> one record each"), CountKind(Jsonl, TEXT("hit")), 2);

	// Kills aggregate the same way: N kills by one player in one step -> one kill record with the count, the count by
	// source and the TTK sums. Three enemies spawn at step 0 and three at step 10; all six die at step 40.
	FDTRunState KillRun;
	FDTWorldView View;
	FDTEnemyPool Pool;
	FDTTelemetryCore KillCore;
	KillRun.Start(5, 1);
	KillRun.SetEventSink(&KillCore);
	KillCore.Start(MakeConfig(5, 1));
	const int32 Half = 3;
	for (int32 Index = 0; Index < 2 * Half; ++Index)
	{
		Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f * static_cast<float>(Index), 0.0f));
		if (Index == Half - 1)
		{
			KillCore.OnEnemiesSpawned(0, false, Pool.Num());
		}
	}
	KillCore.OnEnemiesSpawned(10, false, Pool.Num());
	const uint16 Weapon = FDTTelemetrySources::FindOrAdd(TEXT("Weapon.TestLance"));
	const int32 ByWeapon = 4;
	const float Lethal = static_cast<float>(DTConst::ENM_SWARM_HP) * 2.0f;
	View.Hits.Reset();
	for (int32 Index = 0; Index < 2 * Half; ++Index)
	{
		FDTHit KillHit;
		KillHit.EnemyIndex = Index;
		KillHit.OwnerId = 0;
		KillHit.Damage = Lethal;
		KillHit.SourceId = Index < ByWeapon ? Weapon : FDTTelemetrySources::Stress;
		View.Hits.Add(KillHit);
	}
	FDTSimContext Context;
	Context.StepSeconds = StepSeconds();
	Context.StepIndex = 40;
	Context.Run = &KillRun;
	Context.View = &View;
	KillCore.BeginResolve(40, View.Hits);
	Pool.ResolveHits(Context);
	KillCore.EndResolve(Pool.Num());
	KillCore.EndStep(40, FDTTelemetrySample());
	KillCore.TakeOutput(Csv, Jsonl);

	const auto Ttk = [](int32 Steps) { return static_cast<float>(static_cast<double>(Steps) * StepSeconds()); };
	double TtkSum = 0.0;
	for (int32 Index = 0; Index < 2 * Half; ++Index)
	{
		TtkSum += static_cast<double>(Ttk(Index < Half ? 40 : 30)); // in kill order, as the core sums them
	}
	TestEqual(TEXT("N kills in one step -> one kill record"), CountKind(Jsonl, TEXT("kill")), 1);
	TestEqual(TEXT("its kills field is N"), SumKills(Jsonl), 2 * Half);
	const FString Expected = FString::Printf(
		TEXT("\"owner\":0,\"kills\":%d,\"kills_by_source\":{\"Stress\":%d,\"Weapon.TestLance\":%d},\"ttk_n\":%d,\"ttk_sum_s\":%.4f,\"ttk_min_s\":%.4f,\"ttk_max_s\":%.4f}"),
		2 * Half, 2 * Half - ByWeapon, ByWeapon, 2 * Half, TtkSum, Ttk(30), Ttk(40));
	TestTrue(FString::Printf(TEXT("the record carries counts by source and TTK sums: %s"), *Expected), Jsonl.Contains(Expected));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelSources, "Dusktracer.Telemetry.Sources", DTTelemetryTest::Flags) // T-TEL-05
bool FDTTelSources::RunTest(const FString& Parameters)
{
	using namespace DTTelemetryTest;
	TestEqual(TEXT("id 0 is Unknown"), FDTTelemetrySources::GetName(FDTTelemetrySources::Unknown), FString(TEXT("Unknown")));
	TestEqual(TEXT("id 1 is Stress"), FDTTelemetrySources::GetName(FDTTelemetrySources::Stress), FString(TEXT("Stress")));
	const uint16 Keeper = FDTTelemetrySources::FindOrAdd(TEXT("Keeper.Storm"));
	TestEqual(TEXT("same name, same id"), FDTTelemetrySources::FindOrAdd(TEXT("Keeper.Storm")), Keeper);
	TestEqual(TEXT("an unregistered id reads Unknown"), FDTTelemetrySources::GetName(MAX_uint16), FString(TEXT("Unknown")));
	FDTHit Default;
	TestEqual(TEXT("a hit nobody tagged is Unknown"), Default.SourceId, FDTTelemetrySources::Unknown);

	// Two players, two sources each: totals land per (owner, source).
	FDTRunState Run;
	FDTWorldView View;
	FDTEnemyPool Pool;
	FDTTelemetryCore Core;
	Run.Start(4, 2);
	Run.SetEventSink(&Core);
	Core.Start(MakeConfig(4, 2));
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f * static_cast<float>(Index), 0.0f));
	}
	Core.OnEnemiesSpawned(0, false, Pool.Num());
	const float Lethal = static_cast<float>(DTConst::ENM_SWARM_HP) * 2.0f;
	auto MakeHit = [](int32 Enemy, int32 Owner, float Damage, uint16 Source)
	{
		FDTHit Hit;
		Hit.EnemyIndex = Enemy;
		Hit.OwnerId = Owner;
		Hit.Damage = Damage;
		Hit.SourceId = Source;
		return Hit;
	};
	View.Hits = { MakeHit(0, 0, 1.0f, FDTTelemetrySources::Stress), MakeHit(0, 0, Lethal, Keeper), MakeHit(1, 1, Lethal, FDTTelemetrySources::Stress),
		MakeHit(2, 1, 1.0f, Keeper) };
	FDTSimContext Context;
	Context.StepSeconds = StepSeconds();
	Context.StepIndex = 5;
	Context.Run = &Run;
	Context.View = &View;
	Core.BeginResolve(5, View.Hits);
	Pool.ResolveHits(Context);
	Core.EndResolve(Pool.Num());

	const FDTTelemetrySourceTotals* P0Keeper = Core.GetRunSources(0).Find(Keeper);
	const FDTTelemetrySourceTotals* P0Stress = Core.GetRunSources(0).Find(FDTTelemetrySources::Stress);
	const FDTTelemetrySourceTotals* P1Stress = Core.GetRunSources(1).Find(FDTTelemetrySources::Stress);
	const FDTTelemetrySourceTotals* P1Keeper = Core.GetRunSources(1).Find(Keeper);
	TestTrue(TEXT("all four (owner, source) cells exist"), P0Keeper && P0Stress && P1Stress && P1Keeper);
	if (P0Keeper && P0Stress && P1Stress && P1Keeper)
	{
		TestEqual(TEXT("P0 kill credited to the Keeper that landed it"), P0Keeper->Kills, 1);
		TestEqual(TEXT("P0 stress chip damage, no kill"), P0Stress->Kills, 0);
		TestEqual(TEXT("P0 stress damage"), P0Stress->Damage, static_cast<int64>(1));
		TestEqual(TEXT("P1 kill credited to Stress"), P1Stress->Kills, 1);
		TestEqual(TEXT("P1 Keeper chip, one hit"), P1Keeper->Hits, 1);
	}
	Core.EndStep(5, FDTTelemetrySample());
	Core.End(TEXT("test"));
	FString Csv;
	FString Jsonl;
	Core.TakeOutput(Csv, Jsonl);
	TestEqual(TEXT("one sources record per player"), CountKind(Jsonl, TEXT("sources")), 2);
	TestTrue(TEXT("kill record counts by source"), Jsonl.Contains(TEXT("\"kind\":\"kill\",\"owner\":0,\"kills\":1,\"kills_by_source\":{\"Keeper.Storm\":1}")));
	TestTrue(TEXT("P1 kill record counts by source"), Jsonl.Contains(TEXT("\"kind\":\"kill\",\"owner\":1,\"kills\":1,\"kills_by_source\":{\"Stress\":1}")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelCsvHeader, "Dusktracer.Telemetry.CsvHeader", DTTelemetryTest::Flags) // T-TEL-06
bool FDTTelCsvHeader::RunTest(const FString& Parameters)
{
	// Pinned: tools and validators read these names. Changing them is a RULE change in 02_Systems/telemetry.md §Data.
	const FString Expected = TEXT("interval,run_time_s,player,kills_per_min,swarm_ttk_s,damage_taken_per_min,dashes_per_min,level,"
		"live_bullets,live_enemies,recycled_bullets,refused_spawns,threat_actions_per_min,nearest_enemy_uu,hits_per_min,"
		"damage_dealt_per_min,seed,build,culture,input_device");
	TestEqual(TEXT("CSV header"), FDTTelemetryCore::GetCsvHeader(), Expected);

	FDTTelemetryCore Core;
	Core.Start(DTTelemetryTest::MakeConfig(1, 1));
	Core.End(TEXT("test"));
	FString Csv;
	FString Jsonl;
	Core.TakeOutput(Csv, Jsonl);
	TestTrue(TEXT("a session with no step writes no row"), Csv.IsEmpty());
	TestEqual(TEXT("run_start and run_end only"), DTTelemetryTest::Lines(Jsonl).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelFiles, "Dusktracer.Telemetry.Files", DTTelemetryTest::Flags) // T-TEL-07
bool FDTTelFiles::RunTest(const FString& Parameters)
{
	const FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	const FString Dir = FDTTelemetryWriter::GetDirectory();
	TestEqual(TEXT("directory is Saved/Playtests"), Dir, FPaths::Combine(Saved, TEXT("Playtests")));
	const FString Base = FDTTelemetryWriter::MakeBaseName(4242, FDateTime(2026, 10, 30, 9, 5, 7));
	TestEqual(TEXT("<seed>_<yyyymmdd_hhmmss>"), Base, FString(TEXT("4242_20261030_090507")));

	FDTTelemetryWriter Writer;
	const FString TestBase = FString::Printf(TEXT("dt_test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	Writer.Open(TestBase);
	TestTrue(TEXT("csv under Saved/Playtests"), Writer.GetCsvPath().StartsWith(Dir) && Writer.GetCsvPath().EndsWith(TEXT(".csv")));
	TestTrue(TEXT("jsonl under Saved/Playtests"), Writer.GetJsonlPath().StartsWith(Dir) && Writer.GetJsonlPath().EndsWith(TEXT(".jsonl")));
	Writer.Append(TEXT("a\n"), TEXT("{\"n\":1}\n"));
	Writer.Wait();
	TestTrue(TEXT("csv exists"), IFileManager::Get().FileExists(*Writer.GetCsvPath()));
	TestTrue(TEXT("jsonl exists"), IFileManager::Get().FileExists(*Writer.GetJsonlPath()));
	IFileManager::Get().Delete(*Writer.GetCsvPath(), false, true, true);
	IFileManager::Get().Delete(*Writer.GetJsonlPath(), false, true, true);
	Writer.Close();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelDeterminism, "Dusktracer.Telemetry.Determinism", DTTelemetryTest::Flags) // T-TEL-08
bool FDTTelDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTTelemetryTest;
	const int32 Steps = 4 * StepsPerInterval();
	FFight A;
	FFight B;
	FFight C;
	A.Play(11, Steps, TEXT("2026-01-01T00:00:00.000Z"));
	B.Play(11, Steps, TEXT("2026-06-30T23:59:59.000Z"));
	C.Play(12, Steps, TEXT("2026-01-01T00:00:00.000Z"));

	TestTrue(TEXT("the fight kills something"), SumKills(A.Jsonl) > 0);
	TestEqual(TEXT("no mirror desync in a real fight"), A.Core.GetMirrorDesyncs(), static_cast<int64>(0));
	TestEqual(TEXT("four interval rows"), Lines(A.Csv).Num(), 1 + 4);
	TestEqual(TEXT("same seed and steps -> identical CSV"), A.Csv, B.Csv);

	// JSONL: identical except the wall-clock field of run_start.
	TArray<FString> LinesA = Lines(A.Jsonl);
	TArray<FString> LinesB = Lines(B.Jsonl);
	TestEqual(TEXT("same number of JSONL records"), LinesA.Num(), LinesB.Num());
	if (LinesA.Num() > 1 && LinesA.Num() == LinesB.Num())
	{
		LinesA.RemoveAt(0);
		LinesB.RemoveAt(0);
		TestTrue(TEXT("identical JSONL after run_start"), LinesA == LinesB);
	}
	TestNotEqual(TEXT("another seed differs"), A.Csv, C.Csv);
	bool bAnyTtk = false;
	for (const FString& Row : Lines(A.Csv))
	{
		bAnyTtk |= Row != FDTTelemetryCore::GetCsvHeader() && !Cell(Row, TEXT("swarm_ttk_s")).IsEmpty();
	}
	TestTrue(TEXT("swarm TTK measured in some interval"), bAnyTtk);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelBuffered, "Dusktracer.Telemetry.Buffered", DTTelemetryTest::Flags) // T-TEL-09
bool FDTTelBuffered::RunTest(const FString& Parameters)
{
	using namespace DTTelemetryTest;
	FDTRunState Run;
	FDTTelemetryCore Core;
	Run.Start(9, 1);
	Run.SetEventSink(&Core);
	Core.Start(MakeConfig(9, 1));
	FString Csv;
	FString Jsonl;
	Core.TakeOutput(Csv, Jsonl);
	TestTrue(TEXT("run_start is buffered at start"), Jsonl.StartsWith(TEXT("{\"t\":0.0000,\"step\":0,\"kind\":\"run_start\"")));

	const int32 Half = StepsPerInterval() / 2;
	for (int32 Step = 0; Step < Half; ++Step)
	{
		Run.ApplyEvent({ EDTGameEventKind::Kill, 0, 0 });
		Core.EndStep(Step, FDTTelemetrySample());
	}
	Core.TakeOutput(Csv, Jsonl);
	TestTrue(TEXT("mid-interval: events buffered, no row"), Csv.IsEmpty() && SumKills(Jsonl) == Half);

	Core.End(TEXT("quit"));
	Core.TakeOutput(Csv, Jsonl);
	const TArray<FString> Rows = Lines(Csv);
	TestEqual(TEXT("run end writes the partial interval"), Rows.Num(), 2);
	if (Rows.Num() == 2)
	{
		TestEqual(TEXT("partial row rates use the elapsed time"), Cell(Rows[1], TEXT("kills_per_min")),
			FString::Printf(TEXT("%.3f"), static_cast<double>(Half) * DTConst::TEL_RATE_UNIT_S / (static_cast<double>(Half) * StepSeconds())));
	}
	TestTrue(TEXT("run_end names the reason"), Jsonl.Contains(TEXT("\"kind\":\"run_end\",\"owner\":-1,\"reason\":\"quit\"")));
	TestFalse(TEXT("session over"), Core.IsActive());

	// The writer appends in queue order.
	FDTTelemetryWriter Writer;
	Writer.Open(FString::Printf(TEXT("dt_test_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	Writer.Append(TEXT("first\n"), FString());
	Writer.Append(TEXT("second\n"), FString());
	Writer.Wait();
	FString OnDisk;
	FFileHelper::LoadFileToString(OnDisk, *Writer.GetCsvPath());
	TestEqual(TEXT("appends land in order"), OnDisk, FString(TEXT("first\nsecond\n")));
	IFileManager::Get().Delete(*Writer.GetCsvPath(), false, true, true);
	Writer.Close();
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
