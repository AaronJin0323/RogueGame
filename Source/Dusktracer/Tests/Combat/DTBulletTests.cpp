// Bullet system tests (02_Systems/bullets.md, 07_Technical R-TEC-11).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Bullets; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Combat/DTBulletPool.h"
#include "Combat/DTBulletRenderer.h"
#include "Combat/DTBulletSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTBulletTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Hz = static_cast<float>(DTConst::SIM_FIXED_HZ);
	constexpr float Dt = 1.0f / Hz;

	static FDTBulletSpawn Make(FVector2f Position, FVector2f Velocity, float Life, int32 Owner = 0, int32 Pierce = 1, float Radius = 5.0f, float Damage = 1.0f)
	{
		FDTBulletSpawn Spawn;
		Spawn.Position = Position;
		Spawn.Velocity = Velocity;
		Spawn.Life = Life;
		Spawn.OwnerId = Owner;
		Spawn.Pierce = Pierce;
		Spawn.Radius = Radius;
		Spawn.Damage = Damage;
		return Spawn;
	}

	static void SetEnemies(FDTWorldView& View, const TArray<FVector2f>& Positions, float Radius)
	{
		View.EnemyPositions = Positions;
		View.EnemyRadii.Init(Radius, Positions.Num());
		View.EnemyHash.Build(View.EnemyPositions);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBulletsIntegrateExpire, "Dusktracer.Bullets.IntegrateExpire", DTBulletTest::Flags) // T-BUL-01
bool FDTBulletsIntegrateExpire::RunTest(const FString& Parameters)
{
	using namespace DTBulletTest;
	FDTBulletPool Pool;
	FDTWorldView View;
	// Velocity of one uu per step; life of ten and a half steps: alive after 10 steps, gone after 11.
	Pool.Spawn(Make(FVector2f(0.0f, 0.0f), FVector2f(Hz, 0.0f), 10.5f * Dt));
	Pool.Step(Dt, View);
	TestTrue(TEXT("moved one step"), Pool.GetPositions()[0].Equals(FVector2f(1.0f, 0.0f), 1e-4f));
	TestTrue(TEXT("prev is the spawn point"), Pool.GetPrevPositions()[0].Equals(FVector2f(0.0f, 0.0f), 1e-4f));
	for (int32 Step = 1; Step < 10; ++Step)
	{
		Pool.Step(Dt, View);
	}
	TestEqual(TEXT("alive after 10 steps"), Pool.Num(), 1);
	TestTrue(TEXT("at x = 10"), Pool.GetPositions()[0].Equals(FVector2f(10.0f, 0.0f), 1e-3f));
	Pool.Step(Dt, View);
	TestEqual(TEXT("expired after 11 steps"), Pool.Num(), 0);
	TestEqual(TEXT("expiry counted"), Pool.GetCounters().Expired, static_cast<int64>(1));

	// Dense compaction: half the bullets expire in the same step; every survivor is stepped exactly once.
	FDTBulletPool Mixed;
	for (int32 Index = 0; Index < 100; ++Index)
	{
		const float Life = (Index % 2 == 0) ? 0.5f * Dt : 10.0f;
		Mixed.Spawn(Make(FVector2f(0.0f, static_cast<float>(Index)), FVector2f(Hz, 0.0f), Life));
	}
	Mixed.Step(Dt, View);
	TestEqual(TEXT("half expired"), Mixed.Num(), 50);
	int32 SteppedOnce = 0;
	TSet<int32> Rows;
	for (int32 Index = 0; Index < Mixed.Num(); ++Index)
	{
		const FVector2f P = Mixed.GetPositions()[Index];
		SteppedOnce += FMath::IsNearlyEqual(P.X, 1.0f, 1e-4f) ? 1 : 0;
		Rows.Add(FMath::RoundToInt32(P.Y));
	}
	TestEqual(TEXT("every survivor stepped exactly once"), SteppedOnce, 50);
	bool bOddRows = Rows.Num() == 50;
	for (const int32 Row : Rows)
	{
		bOddRows &= (Row % 2 == 1);
	}
	TestTrue(TEXT("survivors are exactly the long-lived bullets"), bOddRows);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBulletsArenaBounds, "Dusktracer.Bullets.ArenaBounds", DTBulletTest::Flags) // T-BUL-02
bool FDTBulletsArenaBounds::RunTest(const FString& Parameters)
{
	using namespace DTBulletTest;
	const float Half = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU);
	const float Speed = 10.0f * Hz; // ten uu per step
	FDTBulletPool Pool;
	FDTWorldView View;
	Pool.Spawn(Make(FVector2f(Half - 15.0f, 0.0f), FVector2f(Speed, 0.0f), 100.0f));
	Pool.Spawn(Make(FVector2f(0.0f, -Half + 5.0f), FVector2f(0.0f, -Speed), 100.0f));
	Pool.Spawn(Make(FVector2f(0.0f, 0.0f), FVector2f(Speed, 0.0f), 100.0f));
	Pool.Step(Dt, View);
	TestEqual(TEXT("y edge crossed in one step"), Pool.Num(), 2);
	Pool.Step(Dt, View);
	TestEqual(TEXT("x edge crossed in two steps; the inner bullet stays"), Pool.Num(), 1);
	TestEqual(TEXT("both counted as out of bounds"), Pool.GetCounters().OutOfBounds, static_cast<int64>(2));
	TestTrue(TEXT("the survivor is the inner one"), Pool.GetPositions()[0].Equals(FVector2f(20.0f, 0.0f), 1e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBulletsCapRecycle, "Dusktracer.Bullets.CapRecycle", DTBulletTest::Flags) // T-BUL-03
bool FDTBulletsCapRecycle::RunTest(const FString& Parameters)
{
	using namespace DTBulletTest;
	FDTBulletPool Pool;
	FDTWorldView View;
	TestEqual(TEXT("capacity is C.BULLETS.CAP"), Pool.GetCapacity(), DTConst::BULLETS_CAP);
	for (int32 Index = 0; Index < DTConst::BULLETS_CAP; ++Index)
	{
		// Bullet 0 dies this step, which swaps the newest bullet into slot 0: storage order is no longer age order.
		const float Life = Index == 0 ? 0.5f * Dt : 100.0f;
		// Half a uu apart keeps every bullet inside the arena.
		Pool.Spawn(Make(FVector2f(0.5f * static_cast<float>(Index), 0.0f), FVector2f::ZeroVector, Life));
	}
	TestEqual(TEXT("filled to the cap"), Pool.Num(), DTConst::BULLETS_CAP);
	TestEqual(TEXT("no recycle below the cap"), Pool.GetCounters().Recycled, static_cast<int64>(0));
	Pool.Step(Dt, View);
	TestEqual(TEXT("one expired"), Pool.Num(), DTConst::BULLETS_CAP - 1);

	Pool.Spawn(Make(FVector2f(-1.0f, 0.0f), FVector2f::ZeroVector, 100.0f));
	TestEqual(TEXT("free slot used, no recycle"), Pool.GetCounters().Recycled, static_cast<int64>(0));
	Pool.Spawn(Make(FVector2f(-2.0f, 0.0f), FVector2f::ZeroVector, 100.0f));
	TestEqual(TEXT("spawn at the cap recycles and counts"), Pool.GetCounters().Recycled, static_cast<int64>(1));
	TestEqual(TEXT("still at the cap"), Pool.Num(), DTConst::BULLETS_CAP);
	bool bOldestGone = true;
	bool bNewestPresent = false;
	bool bNextOldestPresent = false;
	for (const FVector2f& P : Pool.GetPositions())
	{
		bOldestGone &= !FMath::IsNearlyEqual(P.X, 0.5f);
		bNewestPresent |= FMath::IsNearlyEqual(P.X, -2.0f);
		bNextOldestPresent |= FMath::IsNearlyEqual(P.X, 1.0f);
	}
	TestTrue(TEXT("the oldest live bullet was replaced"), bOldestGone);
	TestTrue(TEXT("the new bullet is live"), bNewestPresent);
	TestTrue(TEXT("the second-oldest is untouched"), bNextOldestPresent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBulletsPierceDistinct, "Dusktracer.Bullets.PierceDistinct", DTBulletTest::Flags) // T-BUL-04
bool FDTBulletsPierceDistinct::RunTest(const FString& Parameters)
{
	using namespace DTBulletTest;
	FDTWorldView View;
	// Slow bullet (ten uu per step) overlaps each enemy for several steps; it must still hit each one once.
	SetEnemies(View, { FVector2f(100.0f, 0.0f), FVector2f(200.0f, 0.0f), FVector2f(300.0f, 0.0f), FVector2f(400.0f, 0.0f) }, 20.0f);
	FDTBulletPool Pool;
	Pool.Spawn(Make(FVector2f(0.0f, 0.0f), FVector2f(10.0f * Hz, 0.0f), 100.0f, 2, 3));
	for (int32 Step = 0; Step < 60 && Pool.Num() > 0; ++Step)
	{
		Pool.Step(Dt, View);
	}
	TArray<int32> Hit;
	for (const FDTHit& H : View.Hits)
	{
		Hit.Add(H.EnemyIndex);
	}
	TestEqual(TEXT("three distinct enemies, in path order"), Hit, TArray<int32>{ 0, 1, 2 });
	TestEqual(TEXT("despawned when pierce is exhausted"), Pool.Num(), 0);
	TestEqual(TEXT("counted as spent"), Pool.GetCounters().Spent, static_cast<int64>(1));
	TestTrue(TEXT("pierce within hit memory"), 3 <= DTConst::BULLETS_HIT_MEMORY);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBulletsSingleHit, "Dusktracer.Bullets.SingleHit", DTBulletTest::Flags) // T-BUL-05
bool FDTBulletsSingleHit::RunTest(const FString& Parameters)
{
	using namespace DTBulletTest;
	FDTWorldView View;
	SetEnemies(View, { FVector2f(100.0f, 0.0f) }, 20.0f);
	FDTBulletPool Pool;
	// Fifty uu per step: 50 is out of reach (5 + 20), 100 is a hit.
	Pool.Spawn(Make(FVector2f(0.0f, 0.0f), FVector2f(50.0f * Hz, 0.0f), 100.0f, 3, 1, 5.0f, 7.0f));
	Pool.Step(Dt, View);
	TestEqual(TEXT("no hit out of reach"), View.Hits.Num(), 0);
	Pool.Step(Dt, View);
	TestEqual(TEXT("exactly one hit"), View.Hits.Num(), 1);
	if (View.Hits.Num() == 1)
	{
		TestEqual(TEXT("hit the enemy"), View.Hits[0].EnemyIndex, 0);
		TestEqual(TEXT("owner carried"), View.Hits[0].OwnerId, 3);
		TestEqual(TEXT("damage carried"), View.Hits[0].Damage, 7.0f);
		TestTrue(TEXT("position is the bullet's"), View.Hits[0].Position.Equals(FVector2f(100.0f, 0.0f), 1e-3f));
	}
	TestEqual(TEXT("non-piercing bullet is gone"), Pool.Num(), 0);

	// A piercing bullet resting inside one enemy still hits it once.
	FDTWorldView Still;
	SetEnemies(Still, { FVector2f(0.0f, 0.0f) }, 20.0f);
	FDTBulletPool Resting;
	Resting.Spawn(Make(FVector2f(0.0f, 0.0f), FVector2f::ZeroVector, 100.0f, 1, 5));
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Resting.Step(Dt, Still);
	}
	TestEqual(TEXT("one hit while overlapping for many steps"), Still.Hits.Num(), 1);
	TestEqual(TEXT("piercing bullet survives"), Resting.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBulletsDeterminism, "Dusktracer.Bullets.Determinism", DTBulletTest::Flags) // T-BUL-06
bool FDTBulletsDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTBulletTest;
	auto Run = [](TArray<FVector2f>& OutPositions, TArray<int32>& OutHits)
	{
		FRandomStream Rng(99);
		FDTWorldView View;
		TArray<FVector2f> Enemies;
		for (int32 Index = 0; Index < 200; ++Index)
		{
			Enemies.Add(FVector2f(Rng.FRandRange(-1500.0f, 1500.0f), Rng.FRandRange(-1500.0f, 1500.0f)));
		}
		SetEnemies(View, Enemies, 30.0f);
		FDTBulletPool Pool;
		for (int32 Step = 0; Step < 120; ++Step)
		{
			for (int32 Shot = 0; Shot < 20; ++Shot)
			{
				const FVector2f Dir = FVector2f(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f)).GetSafeNormal();
				Pool.Spawn(Make(FVector2f::ZeroVector, Dir * 900.0f, Rng.FRandRange(0.5f, 3.0f), Shot % 2, 1 + Shot % 3));
			}
			Pool.Step(Dt, View);
		}
		OutPositions = TArray<FVector2f>(Pool.GetPositions());
		for (const FDTHit& Hit : View.Hits)
		{
			OutHits.Add(Hit.EnemyIndex);
		}
	};
	TArray<FVector2f> PosA, PosB;
	TArray<int32> HitsA, HitsB;
	Run(PosA, HitsA);
	Run(PosB, HitsB);
	TestTrue(TEXT("bullets alive"), PosA.Num() > 0);
	TestTrue(TEXT("some hits"), HitsA.Num() > 0);
	TestEqual(TEXT("same live count"), PosA.Num(), PosB.Num());
	TestTrue(TEXT("bit-identical positions"), PosA.Num() == PosB.Num() && FMemory::Memcmp(PosA.GetData(), PosB.GetData(), PosA.Num() * sizeof(FVector2f)) == 0);
	TestEqual(TEXT("identical hit sequence"), HitsA, HitsB);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTecBulletsNoActors, "Dusktracer.Tec.Bullets.NoActors", DTBulletTest::Flags) // T-TEC-11, T-BUL-07
bool FDTTecBulletsNoActors::RunTest(const FString& Parameters)
{
	using namespace DTBulletTest;
	// A real game world, headless: subsystems come up, begin play registers the bullet system and spawns its renderer.
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTBulletTestWorld")));
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTBulletSubsystem* Bullets = World->GetSubsystem<UDTBulletSubsystem>();
	TestNotNull(TEXT("sim subsystem in a game world"), Sim);
	TestNotNull(TEXT("bullet subsystem in a game world"), Bullets);
	if (Sim && Bullets)
	{
		TestNotNull(TEXT("one renderer actor"), Bullets->GetRenderer());
		int32 ActorsBefore = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++ActorsBefore;
		}
		const int32 Count = DTConst::BULLETS_CAP;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = static_cast<float>(UE_TWO_PI) * static_cast<float>(Index) / static_cast<float>(Count);
			Bullets->Spawn(Make(FVector2f::ZeroVector, FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * 100.0f, 100.0f));
		}
		Sim->StepNow(2);
		Bullets->SimRender(0.5f);
		int32 ActorsAfter = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++ActorsAfter;
		}
		TestEqual(TEXT("bullets stepped by the sim"), Bullets->GetPool().Num(), Count);
		TestEqual(TEXT("no actor per bullet"), ActorsAfter, ActorsBefore);
		if (ADTBulletRenderer* Renderer = Bullets->GetRenderer())
		{
			TestEqual(TEXT("one instance per bullet"), Renderer->GetInstanceCount(), Count);
			Bullets->GetPool().Reset();
			Bullets->SimRender(0.0f);
			TestEqual(TEXT("instances follow the count down"), Renderer->GetInstanceCount(), 0);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBulletsHitSource, "Dusktracer.Bullets.HitSource", DTBulletTest::Flags) // T-BUL-08
bool FDTBulletsHitSource::RunTest(const FString& Parameters)
{
	using namespace DTBulletTest;
	FDTWorldView View;
	// One enemy per tagged bullet, far enough apart that each resting bullet touches only its own enemy.
	SetEnemies(View, { FVector2f(0.0f, 0.0f), FVector2f(100.0f, 0.0f), FVector2f(200.0f, 0.0f), FVector2f(300.0f, 0.0f), FVector2f(400.0f, 0.0f) }, 10.0f);
	// Enemy index -> the source id the bullet resting on it was spawned with.
	const TArray<uint16> Expected = { 6, 7, 8, 9, 10 };
	auto Tagged = [](FVector2f Position, float Life, uint16 Source)
	{
		FDTBulletSpawn Spawn = Make(Position, FVector2f::ZeroVector, Life, 0, 100);
		Spawn.SourceId = Source;
		return Spawn;
	};

	FDTBulletPool Pool(4);
	TestEqual(TEXT("untagged spawn is Unknown"), FDTBulletSpawn().SourceId, static_cast<uint16>(0));
	// Slot 0 dies in the first step, so the newest bullet (source 8) is swapped into it before it is stepped.
	Pool.Spawn(Tagged(FVector2f(-1000.0f, 0.0f), 0.5f * Dt, 5));
	Pool.Spawn(Tagged(FVector2f(0.0f, 0.0f), 100.0f, 6));
	Pool.Spawn(Tagged(FVector2f(100.0f, 0.0f), 100.0f, 7));
	Pool.Spawn(Tagged(FVector2f(200.0f, 0.0f), 100.0f, 8));
	Pool.Step(Dt, View);
	TestEqual(TEXT("expiry compacted the pool"), Pool.Num(), 3);
	TestEqual(TEXT("the swapped bullet keeps its source"), Pool.GetSources()[0], static_cast<uint16>(8));
	TestEqual(TEXT("three hits after compaction"), View.Hits.Num(), 3);

	// Fill to the cap, then recycle the oldest live bullet (source 6) with a bullet of source 10.
	Pool.Spawn(Tagged(FVector2f(300.0f, 0.0f), 100.0f, 9));
	Pool.Spawn(Tagged(FVector2f(400.0f, 0.0f), 100.0f, 10));
	TestEqual(TEXT("the last spawn recycled"), Pool.GetCounters().Recycled, static_cast<int64>(1));
	Pool.Step(Dt, View);
	TestEqual(TEXT("two more hits after recycling"), View.Hits.Num(), 5);

	TArray<uint16> Live(Pool.GetSources());
	Live.Sort();
	TestEqual(TEXT("live sources after recycling"), Live, TArray<uint16>{ 7, 8, 9, 10 });
	for (const FDTHit& Hit : View.Hits)
	{
		if (TestTrue(TEXT("hit a tagged enemy"), Expected.IsValidIndex(Hit.EnemyIndex)))
		{
			TestEqual(*FString::Printf(TEXT("hit on enemy %d carries its bullet's source"), Hit.EnemyIndex), Hit.SourceId, Expected[Hit.EnemyIndex]);
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
