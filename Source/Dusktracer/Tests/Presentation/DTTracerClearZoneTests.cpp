// The hero's clear zone (02_Systems/tracer_art.md R-TRC-01, R-TRC-02, T-TRC-01, T-TRC-02; D-068): a player round inside it draws no head
// and no tail; once out, both are drawn whole; at a ratio of zero every round is drawn; the pool is only read.

#include "Misc/AutomationTest.h"
#include "Combat/DTBulletRenderer.h"
#include "Combat/DTBulletSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Keepers/DTKeeperSubsystem.h"
#include "Player/DTHeroSubsystem.h"
#include "Presentation/DTTracerArt.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTTracerClearZoneTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Hz = static_cast<float>(DTConst::SIM_FIXED_HZ);

	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTTracerClearZoneTestWorld")));
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

	/** Restores dt.Tracer.ClearZone however the test ends. */
	struct FKeepSetting
	{
		float Value = DTTracerArt::ClearZoneSetting();
		~FKeepSetting() { DTTracerArt::ClearZoneSetting() = Value; }
	};

	static FDTBulletSpawn Round(const FVector2f& At, const FVector2f& Velocity)
	{
		FDTBulletSpawn Spawn;
		Spawn.Position = At;
		Spawn.Velocity = Velocity;
		Spawn.Radius = 10.0f;
		Spawn.Life = 10.0f;
		Spawn.OwnerId = 0;
		return Spawn;
	}

	static int32 TailCount(UDTKeeperSubsystem* Keepers)
	{
		int32 Count = 0;
		for (int32 Body = 0; Body < DTTracerArt::NumSlots; ++Body)
		{
			for (int32 Tip = 0; Tip < DTTracerArt::NumSlots; ++Tip)
			{
				if (ADTBulletRenderer* Renderer = Keepers->GetTrailRenderer(Body, Tip))
				{
					Count += Renderer->GetInstanceCount();
				}
			}
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTracerClearZone, "Dusktracer.Tracer.ClearZone", DTTracerClearZoneTest::Flags) // T-TRC-01, T-TRC-02
bool FDTTracerClearZone::RunTest(const FString& Parameters)
{
	using namespace DTTracerClearZoneTest;
	FKeepSetting Keep;
	DTTracerArt::ClearZoneSetting() = -1.0f;
	const float Ratio = static_cast<float>(DTConst::TRC_CLEAR_ZONE_RATIO);
	const float Zone = Ratio * static_cast<float>(DTConst::HERO_RADIUS_UU);
	TestTrue(TEXT("the constant is in force by default"), DTTracerArt::ClearZoneRatio() == Ratio && Ratio > 0.0f);

	// The shape itself: inside and outside, a round of no hero's, and the zone off at a ratio of zero.
	const FVector2f Heroes[] = { FVector2f(100.0f, 200.0f) };
	const DTTracerArt::FClearZone On(Heroes, Ratio);
	TestTrue(TEXT("a round just inside the zone is hidden"), On.Hides(0, Heroes[0] + FVector2f(Zone - 1.0f, 0.0f)));
	TestFalse(TEXT("a round just outside it is drawn"), On.Hides(0, Heroes[0] + FVector2f(Zone + 1.0f, 0.0f)));
	TestTrue(TEXT("a round of no hero's is hidden inside any hero's zone"), On.Hides(INDEX_NONE, Heroes[0]));
	const DTTracerArt::FClearZone Off(Heroes, 0.0f);
	TestFalse(TEXT("at a ratio of zero nothing is hidden"), Off.IsOn() || Off.Hides(0, Heroes[0]));

	// As drawn, in a world: heads by UDTBulletSubsystem, tails by UDTKeeperSubsystem.
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTBulletSubsystem* Bullets = W.World->GetSubsystem<UDTBulletSubsystem>();
	UDTKeeperSubsystem* Keepers = W.World->GetSubsystem<UDTKeeperSubsystem>();
	UDTHeroSubsystem* HeroSys = W.World->GetSubsystem<UDTHeroSubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("bullets"), Bullets) || !TestNotNull(TEXT("keepers"), Keepers) || !TestNotNull(TEXT("heroes"), HeroSys)
		|| !TestNotNull(TEXT("head renderer"), Bullets->GetRenderer()) || !TestNotNull(TEXT("hero 0"), HeroSys->GetState(0)))
	{
		return false;
	}
	const FVector2f Hero = HeroSys->GetState(0)->Pos;
	const FVector2f Speed(600.0f, 0.0f);
	const int32 Inside = 3;
	const int32 Outside = 4;
	const auto Stage = [&]()
	{
		Bullets->GetPool().Reset();
		for (int32 Index = 0; Index < Inside; ++Index)
		{
			// Well inside the zone after one step of flight.
			Bullets->Spawn(Round(Hero + FVector2f(0.0f, 10.0f * Index), Speed));
		}
		for (int32 Index = 0; Index < Outside; ++Index)
		{
			// Well outside it, whatever the step: three zones and more away.
			Bullets->Spawn(Round(Hero + FVector2f(3.0f * Zone + 50.0f * Index, 0.0f), Speed));
		}
		FDTWorldView View;
		Bullets->GetPool().Step(1.0f / Hz, View); // one step, so every round has travelled and has a tail
	};
	Stage();
	const TArray<FVector2f> Before(Bullets->GetPool().GetPositions());
	const int32 Count = Bullets->GetPool().Num();
	Bullets->SimRender(1.0f);
	Keepers->SimRender(1.0f);
	TestEqual(TEXT("a round inside the zone has no head, one outside has one"), Bullets->GetRenderer()->GetInstanceCount(), Outside);
	TestEqual(TEXT("a round inside the zone has no tail, one that has left it has a whole one"), TailCount(Keepers), Outside);
	TestEqual(TEXT("the pool's count is as it was"), Bullets->GetPool().Num(), Count);
	TestTrue(TEXT("and its positions"), Before == TArray<FVector2f>(Bullets->GetPool().GetPositions()));
	FTransform Head;
	bool bAllOut = true;
	for (int32 Index = 0; Index < Bullets->GetRenderer()->GetInstanceCount(); ++Index)
	{
		Bullets->GetRenderer()->GetInstances()->GetInstanceTransform(Index, Head);
		bAllOut &= FVector2f::Distance(FVector2f(static_cast<float>(Head.GetLocation().X), static_cast<float>(Head.GetLocation().Y)), Hero) >= Zone;
	}
	TestTrue(TEXT("every head drawn lies outside the zone"), bAllOut);

	// The revert path (D-068): at a ratio of zero every round has its head and its tail.
	DTTracerArt::ClearZoneSetting() = 0.0f;
	Bullets->SimRender(1.0f);
	Keepers->SimRender(1.0f);
	TestEqual(TEXT("at a ratio of zero every round has a head"), Bullets->GetRenderer()->GetInstanceCount(), Inside + Outside);
	TestEqual(TEXT("and a tail"), TailCount(Keepers), Inside + Outside);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
