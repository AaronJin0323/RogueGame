// Enemy pool tests (02_Systems/enemies.md). Drive FDTEnemyPool directly with a hand-built view and run state.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Enemies; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyPool.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTEnemyTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** A run, a view and a context wired together, with heroes at the given points. */
	struct FHarness
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTSimContext Context;

		explicit FHarness(int32 Seed = 1, int32 Players = 1, TArray<FVector2f> Heroes = { FVector2f::ZeroVector })
		{
			Run.Start(Seed, Players);
			View.HeroPositions = MoveTemp(Heroes);
			Context.StepSeconds = 1.0f / DTConst::SIM_FIXED_HZ;
			Context.Run = &Run;
			Context.View = &View;
		}

		void Steps(FDTEnemyPool& Pool, int32 Count)
		{
			for (int32 Step = 0; Step < Count; ++Step)
			{
				Context.StepIndex = Step;
				Pool.StepEnemies(Context);
				Pool.ResolveHits(Context);
			}
		}
	};

	static FDTHit MakeHit(int32 Enemy, int32 Owner, float Damage)
	{
		FDTHit Hit;
		Hit.EnemyIndex = Enemy;
		Hit.OwnerId = Owner;
		Hit.Damage = Damage;
		return Hit;
	}

	static float Lethal() { return static_cast<float>(DTConst::ENM_SWARM_HP) * 2.0f; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmCap, "Dusktracer.Enemies.Cap", DTEnemyTest::Flags) // T-ENM-01
bool FDTEnmCap::RunTest(const FString& Parameters)
{
	FDTEnemyPool Pool;
	TestEqual(TEXT("capacity is the cap"), Pool.GetCapacity(), DTConst::ENEMIES_CAP);
	for (int32 Index = 0; Index < DTConst::ENEMIES_CAP; ++Index)
	{
		Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(static_cast<float>(Index), 0.0f));
	}
	TestEqual(TEXT("fills to the cap"), Pool.Num(), DTConst::ENEMIES_CAP);
	TestEqual(TEXT("nothing refused yet"), Pool.GetRefusedSpawns(), static_cast<int64>(0));
	TestEqual(TEXT("spawn beyond the cap is refused"), Pool.Spawn(EDTEnemyRole::Swarm, FVector2f::ZeroVector), static_cast<int32>(INDEX_NONE));
	FRandomStream Rng(3);
	TestEqual(TEXT("ring beyond the cap spawns none"), Pool.SpawnRing(EDTEnemyRole::Swarm, 4, FVector2f::ZeroVector, 100.0f, Rng), 0);
	TestEqual(TEXT("refusals counted"), Pool.GetRefusedSpawns(), static_cast<int64>(5));
	TestEqual(TEXT("still at the cap"), Pool.Num(), DTConst::ENEMIES_CAP);

	DTEnemyTest::FHarness H;
	H.Steps(Pool, 2);
	TestEqual(TEXT("a full pool steps and publishes every enemy"), H.View.EnemyPositions.Num(), DTConst::ENEMIES_CAP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmSeek, "Dusktracer.Enemies.Seek", DTEnemyTest::Flags) // T-ENM-02
bool FDTEnmSeek::RunTest(const FString& Parameters)
{
	// Two heroes: each enemy seeks the nearer one.
	DTEnemyTest::FHarness H(1, 2, { FVector2f(0.0f, 0.0f), FVector2f(3000.0f, 0.0f) });
	FDTEnemyPool Pool;
	const int32 A = Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(1000.0f, 0.0f)); // nearer hero 0
	const int32 B = Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(2500.0f, 0.0f)); // nearer hero 1
	const float Limit = DTConst::ARENA_HALF_EXTENT_UU - DTConst::ENM_SWARM_RADIUS_UU;
	const int32 C = Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(Limit - 1.0f, Limit - 1.0f));
	H.View.HeroPositions.Add(FVector2f(Limit + 500.0f, Limit + 500.0f)); // a hero just beyond the arena corner pulls C outward

	H.Steps(Pool, DTConst::SIM_FIXED_HZ);
	TestTrue(TEXT("A moved toward hero 0"), Pool.GetPositions()[A].X < 1000.0f);
	TestTrue(TEXT("B moved toward hero 1"), Pool.GetPositions()[B].X > 2500.0f);
	TestTrue(TEXT("A stays on the hero line"), FMath::Abs(Pool.GetPositions()[A].Y) < 1.0f);

	const float MaxTravel = DTConst::ENM_SWARM_SPEED_UU_S * 1.0f + 1.0f;
	TestTrue(TEXT("speed cap: one second moves at most one second of speed"), 1000.0f - Pool.GetPositions()[A].X <= MaxTravel);
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		TestTrue(TEXT("velocity within the speed cap"), Pool.GetVelocities()[Index].Size() <= Pool.GetSpeeds()[Index] + 1e-3f);
	}
	TestTrue(TEXT("C stays inside the arena"), Pool.GetPositions()[C].X <= Limit && Pool.GetPositions()[C].Y <= Limit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmSeparation, "Dusktracer.Enemies.Separation", DTEnemyTest::Flags) // T-ENM-03
bool FDTEnmSeparation::RunTest(const FString& Parameters)
{
	// The hero is far along +Y; separation is the only force along X.
	DTEnemyTest::FHarness H(1, 1, { FVector2f(0.0f, 3000.0f) });
	FDTEnemyPool Pool;
	const float Start = DTConst::ENM_SWARM_RADIUS_UU * 0.25f;
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(0.0f, 0.0f));
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(Start, 0.0f));
	H.Steps(Pool, DTConst::SIM_FIXED_HZ / 2);
	const float Apart = FMath::Abs(Pool.GetPositions()[1].X - Pool.GetPositions()[0].X);
	TestTrue(TEXT("overlapping enemies are pushed apart"), Apart > Start * 2.0f);
	TestTrue(TEXT("pushed in opposite directions"), Pool.GetPositions()[0].X < 0.0f && Pool.GetPositions()[1].X > Start);

	FDTEnemyPool Stacked;
	Stacked.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f, 100.0f));
	Stacked.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f, 100.0f));
	H.Steps(Stacked, DTConst::SIM_FIXED_HZ / 2);
	TestTrue(TEXT("coincident enemies separate"), FVector2f::Distance(Stacked.GetPositions()[0], Stacked.GetPositions()[1]) > Start);

	FDTEnemyPool Lone;
	Lone.Spawn(EDTEnemyRole::Swarm, FVector2f(0.0f, 0.0f));
	H.Steps(Lone, DTConst::SIM_FIXED_HZ / 2);
	TestTrue(TEXT("a lone enemy has no sideways push"), FMath::Abs(Lone.GetPositions()[0].X) < 1e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmPublish, "Dusktracer.Enemies.Publish", DTEnemyTest::Flags) // T-ENM-04
bool FDTEnmPublish::RunTest(const FString& Parameters)
{
	DTEnemyTest::FHarness H(11);
	FDTEnemyPool Pool;
	Pool.SpawnRing(EDTEnemyRole::Swarm, 64, FVector2f::ZeroVector, 900.0f, H.Run.GetRng().Get(EDTRngStream::Spawn));
	const TArray<FVector2f> Before(Pool.GetPositions());
	Pool.StepEnemies(H.Context);

	TestEqual(TEXT("one published position per enemy"), H.View.EnemyPositions.Num(), Pool.Num());
	TestEqual(TEXT("one published radius per enemy"), H.View.EnemyRadii.Num(), Pool.Num());
	TestEqual(TEXT("hash holds every enemy"), H.View.EnemyHash.Num(), Pool.Num());
	int32 Moved = 0;
	int32 Mismatch = 0;
	int32 HashMisses = 0;
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		Moved += Before[Index] != Pool.GetPositions()[Index] ? 1 : 0;
		Mismatch += H.View.EnemyPositions[Index] != Pool.GetPositions()[Index] || H.View.EnemyRadii[Index] != Pool.GetRadii()[Index] ? 1 : 0;
		TArray<int32> Found;
		H.View.EnemyHash.QueryCircle(Pool.GetPositions()[Index], Pool.GetRadii()[Index], Found);
		HashMisses += Found.Contains(Index) ? 0 : 1;
	}
	TestEqual(TEXT("every enemy moved this step"), Moved, Pool.Num());
	TestEqual(TEXT("published data equals the moved positions"), Mismatch, 0);
	TestEqual(TEXT("hash finds every enemy at its moved position"), HashMisses, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmKillOwner, "Dusktracer.Enemies.KillOwner", DTEnemyTest::Flags) // T-ENM-05
bool FDTEnmKillOwner::RunTest(const FString& Parameters)
{
	DTEnemyTest::FHarness H(5, 2);
	FDTEnemyPool Pool;
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f, 0.0f));
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(-100.0f, 0.0f));
	const float Chip = DTConst::ENM_SWARM_HP * 0.5f;
	H.View.Hits.Add(DTEnemyTest::MakeHit(0, 1, DTEnemyTest::Lethal()));
	H.View.Hits.Add(DTEnemyTest::MakeHit(1, 0, Chip));

	TestEqual(TEXT("one kill"), Pool.ResolveHits(H.Context), 1);
	TestEqual(TEXT("the kill goes to the killing hit's owner"), H.Run.GetPlayer(1).Kills, 1);
	TestEqual(TEXT("the other player gets none"), H.Run.GetPlayer(0).Kills, 0);
	TestEqual(TEXT("damage routed to its owner"), H.Run.GetPlayer(0).DamageDealt, static_cast<int64>(FMath::RoundToInt32(Chip)));
	TestEqual(TEXT("damage capped at remaining HP"), H.Run.GetPlayer(1).DamageDealt, static_cast<int64>(DTConst::ENM_SWARM_HP));
	TestTrue(TEXT("hits consumed"), H.View.Hits.IsEmpty());
	TestEqual(TEXT("dead enemy removed"), Pool.Num(), 1);
	TestEqual(TEXT("survivor wounded"), Pool.GetHp()[0], DTConst::ENM_SWARM_HP - Chip);
	TestEqual(TEXT("total kills"), Pool.GetTotalKills(), static_cast<int64>(1));
	TestTrue(TEXT("kill rate counts the kill"), Pool.GetKillRate().GetKillsPerSecond() > 0.0f);

	// Now kill the survivor with player 0.
	H.View.Hits.Add(DTEnemyTest::MakeHit(0, 0, DTEnemyTest::Lethal()));
	Pool.ResolveHits(H.Context);
	TestEqual(TEXT("second kill to player 0"), H.Run.GetPlayer(0).Kills, 1);
	TestEqual(TEXT("player 1 unchanged"), H.Run.GetPlayer(1).Kills, 1);
	TestEqual(TEXT("pool empty"), Pool.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmDiesOnce, "Dusktracer.Enemies.DiesOnce", DTEnemyTest::Flags) // T-ENM-06
bool FDTEnmDiesOnce::RunTest(const FString& Parameters)
{
	DTEnemyTest::FHarness H(5, 2);
	FDTEnemyPool Pool;
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(0.0f, 0.0f));
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(500.0f, 0.0f));
	H.View.Hits.Add(DTEnemyTest::MakeHit(0, 0, DTEnemyTest::Lethal()));
	H.View.Hits.Add(DTEnemyTest::MakeHit(0, 1, DTEnemyTest::Lethal()));
	H.View.Hits.Add(DTEnemyTest::MakeHit(0, 1, DTEnemyTest::Lethal()));
	H.View.Hits.Add(DTEnemyTest::MakeHit(7, 1, DTEnemyTest::Lethal())); // stale index: ignored

	TestEqual(TEXT("three lethal hits, one kill"), Pool.ResolveHits(H.Context), 1);
	TestEqual(TEXT("first lethal hit owns the kill"), H.Run.GetPlayer(0).Kills, 1);
	TestEqual(TEXT("later hits on the dead enemy give nothing"), H.Run.GetPlayer(1).Kills, 0);
	TestEqual(TEXT("later hits deal no damage"), H.Run.GetPlayer(1).DamageDealt, static_cast<int64>(0));
	TestEqual(TEXT("one enemy left"), Pool.Num(), 1);
	TestEqual(TEXT("the untouched enemy survives"), Pool.GetPositions()[0], FVector2f(500.0f, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmCompaction, "Dusktracer.Enemies.Compaction", DTEnemyTest::Flags) // T-ENM-07
bool FDTEnmCompaction::RunTest(const FString& Parameters)
{
	DTEnemyTest::FHarness H(5, 1);
	FDTEnemyPool Pool;
	constexpr int32 Count = 10;
	TMap<int32, float> Expected; // x-position tag -> HP; each enemy is identified by its x
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f * Index, 0.0f));
		Pool.SetHp(Index, 1.0f + Index * 0.5f);
		Expected.Add(100 * Index, 1.0f + Index * 0.5f);
	}
	for (const int32 Victim : { 0, 4, 9 })
	{
		H.View.Hits.Add(DTEnemyTest::MakeHit(Victim, 0, 1000.0f));
		Expected.Remove(100 * Victim);
	}
	TestEqual(TEXT("three kills"), Pool.ResolveHits(H.Context), 3);
	TestEqual(TEXT("seven remain"), Pool.Num(), Count - 3);

	int32 Wrong = 0;
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		const int32 Tag = FMath::RoundToInt32(Pool.GetPositions()[Index].X);
		const float* Hp = Expected.Find(Tag);
		const bool bIntact = Hp && *Hp == Pool.GetHp()[Index] && Pool.GetPrevPositions()[Index] == Pool.GetPositions()[Index]
			&& Pool.GetRadii()[Index] == DTConst::ENM_SWARM_RADIUS_UU && Pool.GetMaxHp()[Index] == DTConst::ENM_SWARM_HP
			&& Pool.GetSpeeds()[Index] == DTConst::ENM_SWARM_SPEED_UU_S && Pool.GetRoles()[Index] == EDTEnemyRole::Swarm
			&& !Pool.IsDead(Index);
		Wrong += bIntact ? 0 : 1;
		Expected.Remove(Tag);
	}
	TestEqual(TEXT("every survivor keeps its own data"), Wrong, 0);
	TestEqual(TEXT("every survivor accounted for once"), Expected.Num(), 0);
	return true;
}

namespace DTEnemyTest
{
	static TArray<FVector2f> Simulate(int32 Seed)
	{
		FHarness H(Seed);
		FDTEnemyPool Pool;
		Pool.SpawnRing(EDTEnemyRole::Swarm, DTConst::ENEMIES_CAP / 2, FVector2f::ZeroVector,
			static_cast<float>(DTConst::ENM_SPAWN_RING_RADIUS_UU), H.Run.GetRng().Get(EDTRngStream::Spawn));
		H.Steps(Pool, DTConst::SIM_FIXED_HZ * 2);
		return TArray<FVector2f>(Pool.GetPositions());
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmDeterminism, "Dusktracer.Enemies.Determinism", DTEnemyTest::Flags) // T-ENM-08
bool FDTEnmDeterminism::RunTest(const FString& Parameters)
{
	const TArray<FVector2f> A = DTEnemyTest::Simulate(99);
	const TArray<FVector2f> B = DTEnemyTest::Simulate(99);
	const TArray<FVector2f> C = DTEnemyTest::Simulate(100);
	TestEqual(TEXT("same seed, same population"), A.Num(), B.Num());
	TestTrue(TEXT("same seed, bit-identical positions"), A.Num() == B.Num() && FMemory::Memcmp(A.GetData(), B.GetData(), A.Num() * sizeof(FVector2f)) == 0);
	TestTrue(TEXT("different seed, different positions"), A.Num() != C.Num() || FMemory::Memcmp(A.GetData(), C.GetData(), A.Num() * sizeof(FVector2f)) != 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
