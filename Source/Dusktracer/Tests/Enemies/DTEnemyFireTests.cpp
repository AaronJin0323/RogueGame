// Enemy fire and enemy render tests (02_Systems/enemies.md T-ENM-15, T-ENM-16, T-ENM-17, T-ENM-24).

#include "Misc/AutomationTest.h"
#include "Combat/DTBulletPool.h"
#include "Combat/DTBulletSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/DTEnemyBulletPool.h"
#include "Enemies/DTEnemyBulletRenderer.h"
#include "Enemies/DTEnemyRenderer.h"
#include "Enemies/DTEnemySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTEnemyFireTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.0f / DTConst::SIM_FIXED_HZ;

	static FDTEnemyRoundSpawn Round(const FVector2f& At, const FVector2f& Velocity, float Life = DTConst::ENEMY_FIRE_SHOT_LIFE_S)
	{
		FDTEnemyRoundSpawn Spawn;
		Spawn.Position = At;
		Spawn.Velocity = Velocity;
		Spawn.Radius = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
		Spawn.Damage = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_DAMAGE);
		Spawn.Life = Life;
		Spawn.TargetPlayer = 0;
		Spawn.AttackId = FName(TEXT("Turret.Fire"));
		Spawn.EnemySerial = 42;
		return Spawn;
	}

	/** A headless game world with the sim, bullet and enemy subsystems up and begun. */
	struct FTestWorld
	{
		UWorld* World = nullptr;
		FWorldContext* Context = nullptr;

		FTestWorld()
		{
			Context = &GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTEnemyFireTestWorld")));
			Context->SetCurrentWorld(World);
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

		int32 CountActors() const
		{
			int32 Count = 0;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				++Count;
			}
			return Count;
		}
	};

	/** True when every instance of Part carries exactly Color and Emissive in custom data 0-3. */
	static bool InstancesCarry(const UInstancedStaticMeshComponent* Part, const FLinearColor& Color, float Emissive)
	{
		const int32 Stride = Part->NumCustomDataFloats;
		if (Stride < 4 || Part->PerInstanceSMCustomData.Num() != Part->GetInstanceCount() * Stride)
		{
			return false;
		}
		for (int32 Instance = 0; Instance < Part->GetInstanceCount(); ++Instance)
		{
			const float* Data = &Part->PerInstanceSMCustomData[Instance * Stride];
			if (Data[0] != Color.R || Data[1] != Color.G || Data[2] != Color.B || Data[3] != Emissive)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnemyFirePool, "Dusktracer.EnemyFire.Pool", DTEnemyFireTest::Flags) // T-ENM-15
bool FDTEnemyFirePool::RunTest(const FString& Parameters)
{
	using namespace DTEnemyFireTest;
	FDTWorldView View; // no heroes: nothing to hit
	{
		FDTEnemyBulletPool Pool;
		Pool.Spawn(Round(FVector2f::ZeroVector, FVector2f(600.0f, 0.0f), 1.0f));
		const int32 Half = DTConst::SIM_FIXED_HZ / 2;
		for (int32 Step = 0; Step < Half; ++Step)
		{
			Pool.Step(Dt, View);
		}
		TestTrue(TEXT("integrates velocity x time"), FMath::IsNearlyEqual(Pool.GetPositions()[0].X, 300.0f, 0.01f));
		TestTrue(TEXT("previous position is one step back"), FMath::IsNearlyEqual(Pool.GetPositions()[0].X - Pool.GetPrevPositions()[0].X, 600.0f * Dt, 0.01f));
		for (int32 Step = 0; Step < Half + 1; ++Step)
		{
			Pool.Step(Dt, View);
		}
		TestEqual(TEXT("expires when its life runs out"), Pool.Num(), 0);
		TestEqual(TEXT("expiry counted"), Pool.GetCounters().Expired, static_cast<int64>(1));

		const float Edge = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU) - 1.0f;
		Pool.Spawn(Round(FVector2f(Edge, 0.0f), FVector2f(600.0f, 0.0f)));
		Pool.Step(Dt, View);
		TestEqual(TEXT("leaving the arena removes it"), Pool.Num(), 0);
		TestEqual(TEXT("out of bounds counted"), Pool.GetCounters().OutOfBounds, static_cast<int64>(1));
	}

	FDTEnemyBulletPool Pool;
	TestEqual(TEXT("capacity is C.ENEMY_FIRE.CAP"), Pool.GetCapacity(), DTConst::ENEMY_FIRE_CAP);
	for (int32 Index = 0; Index < DTConst::ENEMY_FIRE_CAP; ++Index)
	{
		Pool.Spawn(Round(FVector2f(static_cast<float>(Index), 0.0f), FVector2f(0.0f, 10.0f)));
	}
	const TArray<FVector2f> Before(Pool.GetPositions());
	TestEqual(TEXT("a spawn at the cap is refused"), Pool.Spawn(Round(FVector2f::ZeroVector, FVector2f::ZeroVector)), static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("refusal counted"), Pool.GetCounters().Refused, static_cast<int64>(1));
	TestEqual(TEXT("still at the cap"), Pool.Num(), DTConst::ENEMY_FIRE_CAP);
	TestTrue(TEXT("no live round removed or replaced"), Before == TArray<FVector2f>(Pool.GetPositions()));

	// Player bullets at their cap recycle their own oldest; enemy rounds live in another pool.
	FDTBulletPool Player;
	FDTBulletSpawn Bullet;
	Bullet.Radius = 4.0f;
	Bullet.Life = 10.0f;
	for (int32 Index = 0; Index < DTConst::BULLETS_CAP + 16; ++Index)
	{
		Player.Spawn(Bullet);
	}
	TestTrue(TEXT("player bullets recycled at their cap"), Player.GetCounters().Recycled > 0);
	TestEqual(TEXT("enemy rounds untouched"), Pool.Num(), DTConst::ENEMY_FIRE_CAP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnemyFireHitHero, "Dusktracer.EnemyFire.HitHero", DTEnemyFireTest::Flags) // T-ENM-16
bool FDTEnemyFireHitHero::RunTest(const FString& Parameters)
{
	using namespace DTEnemyFireTest;
	FDTWorldView View;
	View.HeroPositions = { FVector2f(0.0f, 0.0f), FVector2f(0.0f, 1000.0f) };
	View.HeroIntangible = { false, true };
	// Enemies at the heroes: rounds never hit them nor touch the enemy hash.
	View.EnemyPositions = { FVector2f(-50.0f, 0.0f), FVector2f(-50.0f, 1000.0f) };
	View.EnemyRadii = { 30.0f, 30.0f };
	View.EnemySerials = { 1u, 2u };
	View.EnemyHash.Build(View.EnemyPositions);

	FDTEnemyBulletPool Pool;
	const float Speed = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_SPEED_UU_S);
	Pool.Spawn(Round(FVector2f(-200.0f, 0.0f), FVector2f(Speed, 0.0f)));
	Pool.Spawn(Round(FVector2f(-200.0f, 1000.0f), FVector2f(Speed, 0.0f)));
	for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ; ++Step)
	{
		Pool.Step(Dt, View);
	}
	if (TestEqual(TEXT("one hero hit"), View.HeroHits.Num(), 1))
	{
		const FDTHeroHit& Hit = View.HeroHits[0];
		TestEqual(TEXT("to the touched player"), Hit.PlayerId, 0);
		TestEqual(TEXT("kind Bullet"), Hit.Kind, EDTHeroHitKind::Bullet);
		TestEqual(TEXT("round damage"), Hit.Damage, static_cast<float>(DTConst::ENEMY_FIRE_SHOT_DAMAGE));
		TestEqual(TEXT("attack id"), Hit.AttackId, FName(TEXT("Turret.Fire")));
	}
	TestEqual(TEXT("the hitting round is removed, the other passed through"), Pool.Num(), 1);
	TestTrue(TEXT("the survivor flew past the intangible hero"), Pool.Num() == 1 && Pool.GetPositions()[0].Y == 1000.0f && Pool.GetPositions()[0].X > 200.0f);
	TestTrue(TEXT("passing through counted"), Pool.GetCounters().PassedThrough > 0);
	TestTrue(TEXT("rounds never append an FDTHit"), View.Hits.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnemyFireNoActors, "Dusktracer.EnemyFire.NoActors", DTEnemyFireTest::Flags) // T-ENM-17
bool FDTEnemyFireNoActors::RunTest(const FString& Parameters)
{
	using namespace DTEnemyFireTest;
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	UDTBulletSubsystem* Bullets = W.World->GetSubsystem<UDTBulletSubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("enemies"), Enemies) || !TestNotNull(TEXT("bullets"), Bullets))
	{
		return false;
	}
	ADTEnemyBulletRenderer* Renderer = Enemies->GetRoundRenderer();
	if (!TestNotNull(TEXT("one enemy-fire renderer"), Renderer))
	{
		return false;
	}
	TestTrue(TEXT("its own renderer, not the player-bullet ISM"), static_cast<AActor*>(Renderer) != static_cast<AActor*>(Bullets->GetRenderer()));
	const int32 ActorsBefore = W.CountActors();
	for (int32 Index = 0; Index < DTConst::ENEMY_FIRE_CAP; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / DTConst::ENEMY_FIRE_CAP;
		const FVector2f Dir(FMath::Cos(Angle), FMath::Sin(Angle));
		Enemies->GetPool().GetRounds().Spawn(Round(Dir * 1000.0f, Dir * 10.0f)); // away from the stress hero at the origin
	}
	Sim->StepNow(2);
	const FLinearColor Color = UDTEnemySubsystem::GetRoundColor();
	TestEqual(TEXT("round colour is the Pal.Enemy.Bullet token"), Color, DTPal::Linear(DTPal::Enemy_Bullet));
	const FLinearColor CoreColor = UDTEnemySubsystem::GetRoundCoreColor();
	TestEqual(TEXT("round core is the Pal.Enemy.BulletCore token"), CoreColor, DTPal::Linear(DTPal::Enemy_BulletCore));
	for (const int32 PlayerBullets : { 0, DTConst::BULLETS_CAP })
	{
		// Density: with no player fire and with player fire at its cap, enemy rounds draw the same.
		Bullets->GetPool().Reset();
		FDTBulletSpawn Bullet;
		Bullet.Radius = 4.0f;
		Bullet.Life = 10.0f;
		Bullet.Position = FVector2f(0.0f, -3000.0f);
		for (int32 Index = 0; Index < PlayerBullets; ++Index)
		{
			Bullets->Spawn(Bullet);
		}
		Enemies->SimRender(0.5f);
		Bullets->SimRender(0.5f);
		TestEqual(TEXT("rounds at the cap"), Enemies->GetPool().GetRounds().Num(), DTConst::ENEMY_FIRE_CAP);
		TestEqual(TEXT("one instance per round"), Renderer->GetInstanceCount(), DTConst::ENEMY_FIRE_CAP);
		FTransform First;
		Renderer->GetInstances()->GetInstanceTransform(0, First);
		TestEqual(TEXT("drawn at C.ENEMY_FIRE.DRAW_RADIUS_RATIO of the hit radius"), static_cast<float>(First.GetScale3D().X) * Renderer->GetMeshRadius(),
			static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU * DTConst::ENEMY_FIRE_DRAW_RADIUS_RATIO), 0.01f);
		TestTrue(TEXT("instance colours are the enemy tokens at full emissive"), InstancesCarry(Renderer->GetInstances(), Color, DTConst::ENEMY_FIRE_EMISSIVE));
		const TArray<float>& Data = Renderer->GetInstances()->PerInstanceSMCustomData;
		const int32 Stride = ADTEnemyBulletRenderer::NumCustomData;
		TestEqual(TEXT("every instance carries the core colour"), Data.Num(), DTConst::ENEMY_FIRE_CAP * Stride);
		if (Data.Num() >= Stride)
		{
			TestEqual(TEXT("the last instance's core is the token"), Data[Data.Num() - 1], CoreColor.B, 0.0001f);
		}
	}
	TestEqual(TEXT("no actor per round"), W.CountActors(), ActorsBefore);
	Enemies->GetPool().GetRounds().Reset();
	Enemies->SimRender(0.0f);
	TestEqual(TEXT("instances follow the count down"), Renderer->GetInstanceCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmOpticPart, "Dusktracer.Enemies.OpticPart", DTEnemyFireTest::Flags) // T-ENM-24
bool FDTEnmOpticPart::RunTest(const FString& Parameters)
{
	using namespace DTEnemyFireTest;
	FTestWorld W;
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	if (!TestNotNull(TEXT("enemies"), Enemies) || !TestNotNull(TEXT("enemy renderer"), Enemies->GetRenderer()))
	{
		return false;
	}
	const int32 ActorsBefore = W.CountActors();
	for (int32 Role = 0; Role < static_cast<int32>(EDTEnemyRole::Count); ++Role)
	{
		for (int32 Copy = 0; Copy < 3; ++Copy)
		{
			Enemies->GetPool().Spawn(static_cast<EDTEnemyRole>(Role), FVector2f(600.0f * Role - 1500.0f, 400.0f * Copy + 800.0f));
		}
	}
	Enemies->SimRender(0.5f);
	ADTEnemyRenderer* Renderer = Enemies->GetRenderer();
	const int32 Live = Enemies->GetLiveCount();
	TestEqual(TEXT("one hull per live enemy"), Renderer->GetHull()->GetInstanceCount(), Live);
	TestEqual(TEXT("one optic per live enemy"), Renderer->GetOptic()->GetInstanceCount(), Live);
	TestEqual(TEXT("no actor per enemy"), W.CountActors(), ActorsBefore);
	TestEqual(TEXT("hull colour token"), UDTEnemySubsystem::GetHullColor(), DTPal::Linear(DTPal::Enemy_Body));
	TestEqual(TEXT("optic colour token"), UDTEnemySubsystem::GetOpticColor(), DTPal::Linear(DTPal::Enemy_Optic));
	TestTrue(TEXT("hull = Pal.Enemy.Body with zero emissive"), InstancesCarry(Renderer->GetHull(), DTPal::Linear(DTPal::Enemy_Body), 0.0f));
	TestTrue(TEXT("optic = Pal.Enemy.Optic at the optic emissive"), InstancesCarry(Renderer->GetOptic(), DTPal::Linear(DTPal::Enemy_Optic), DTConst::ENEMIES_OPTIC_EMISSIVE));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
