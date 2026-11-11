// Tracer art tests (02_Systems/tracer_art.md T-TRC-01..07) and the three 04_Presentation tests package D of S4 wave 1 makes live:
// T-PRE-01 Dusktracer.Pre.PlayerTracer, T-PRE-03 Dusktracer.Pre.TelegraphCoverage, T-PRE-06 Dusktracer.Pre.DensityLod (the tracer half;
// the kill half belongs to impact_art.md). T-TRC-05's other half is the staged frame Tools/Scenes/tracer_crossing.json (gate step contrast).

#include "Misc/AutomationTest.h"
#include "Combat/DTBulletPool.h"
#include "Combat/DTBulletRenderer.h"
#include "Combat/DTBulletSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyBulletPool.h"
#include "Enemies/DTEnemyBulletRenderer.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTTelegraphRenderer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Keepers/DTKeeperSpec.h"
#include "Keepers/DTKeeperSubsystem.h"
#include "Materials/MaterialInterface.h"
#include "Presentation/DTTracerArt.h"
#include "Telemetry/DTTelemetrySources.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTTracerArtTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Hz = static_cast<float>(DTConst::SIM_FIXED_HZ);
	constexpr float Tolerance = 1.0e-3f;
	constexpr double Z = DTConst::SIM_PLANE_Z_UU;

	/** A headless game world with the sim, bullet, enemy and Keeper subsystems up and begun. */
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTTracerArtTestWorld")));
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

	/** The first blessing id of a Path, from the catalog. */
	static FName FirstBlessing(EDTPath Path)
	{
		for (int32 Row = 0; Row < DTCat::NumAllArchetypes; ++Row)
		{
			if (DTCat::Archetypes[Row].Path == Path)
			{
				return FName(DTCat::Archetypes[Row].BlessingIds[0]);
			}
		}
		return NAME_None;
	}

	/** A player's Keepers: Primary with one blessing owned (or none), and Second after the commitment (or none). */
	static FDTKeeperRunState Keepers(EDTPath Primary, bool bBlessed, EDTPath Second = EDTPath::None, bool bCommitted = false)
	{
		FDTKeeperRunState State;
		State.Primary = Primary;
		State.Second = Second;
		State.bCommitDone = bCommitted;
		if (bBlessed && Primary != EDTPath::None)
		{
			State.Owned.Add(FirstBlessing(Primary));
		}
		return State;
	}

	/** True when every instance of Part carries exactly Row in its custom data. */
	static bool InstancesCarry(const UInstancedStaticMeshComponent* Part, TConstArrayView<float> Row)
	{
		const int32 Stride = Part->NumCustomDataFloats;
		if (Stride != Row.Num() || Part->PerInstanceSMCustomData.Num() != Part->GetInstanceCount() * Stride)
		{
			return false;
		}
		for (int32 Slot = 0; Slot < Part->PerInstanceSMCustomData.Num(); ++Slot)
		{
			if (Part->PerInstanceSMCustomData[Slot] != Row[Slot % Stride])
			{
				return false;
			}
		}
		return true;
	}

	static FDTBulletSpawn PlayerRound(const FVector2f& At, const FVector2f& Velocity, float Radius, int32 Owner = 0)
	{
		FDTBulletSpawn Spawn;
		Spawn.Position = At;
		Spawn.Velocity = Velocity;
		Spawn.Radius = Radius;
		Spawn.Life = 10.0f;
		Spawn.OwnerId = Owner;
		return Spawn;
	}

	static bool ShapeOf(const TCHAR* Name, EDTTelegraphShape& Out)
	{
		static const TPair<const TCHAR*, EDTTelegraphShape> Names[] =
		{
			{ TEXT("Band"), EDTTelegraphShape::Band }, { TEXT("Ring"), EDTTelegraphShape::Ring }, { TEXT("Arc"), EDTTelegraphShape::Arc },
			{ TEXT("AimLine"), EDTTelegraphShape::AimLine }, { TEXT("RingGap"), EDTTelegraphShape::RingGap },
			{ TEXT("SpiralLane"), EDTTelegraphShape::SpiralLane }, { TEXT("HazardRing"), EDTTelegraphShape::HazardRing }
		};
		for (const TPair<const TCHAR*, EDTTelegraphShape>& Pair : Names)
		{
			if (FCString::Strcmp(Name, Pair.Key) == 0)
			{
				Out = Pair.Value;
				return true;
			}
		}
		return false;
	}
}

// T-PRE-01, T-TRC-03: the core is Pal.Player.Core for every weapon and every pair of Paths; the tail takes the leading Path's token and
// the second Path only changes the tip. T-TRC-08: tails are batched, never an actor per round.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPrePlayerTracer, "Dusktracer.Pre.PlayerTracer", DTTracerArtTest::Flags)
bool FDTPrePlayerTracer::RunTest(const FString& Parameters)
{
	using namespace DTTracerArtTest;
	const int32 NumPaths = static_cast<int32>(EDTPath::Count);
	TestTrue(TEXT("the catalog has weapons to walk"), DTCat::NumWeapons > 0);
	for (int32 Weapon = 0; Weapon < DTCat::NumWeapons; ++Weapon)
	{
		const FName WeaponId(DTCat::Weapons[Weapon].Id);
		TestTrue(TEXT("no Keeper: the core is Pal.Player.Core"), DTTracerArt::HeadToken(WeaponId, Keepers(EDTPath::None, false)) == DTPal::Player_Core);
		for (int32 First = 0; First < NumPaths; ++First)
		{
			const EDTPath Primary = static_cast<EDTPath>(First);
			const FDTKeeperRunState Alone = Keepers(Primary, true);
			TestTrue(TEXT("one Keeper: the core is Pal.Player.Core"), DTTracerArt::HeadToken(WeaponId, Alone) == DTPal::Player_Core);
			for (int32 Other = 0; Other < NumPaths; ++Other)
			{
				const FDTKeeperRunState Both = Keepers(Primary, true, static_cast<EDTPath>(Other), true);
				TestTrue(TEXT("two Keepers: the core is Pal.Player.Core"), DTTracerArt::HeadToken(WeaponId, Both) == DTPal::Player_Core);
			}
		}
	}

	// The tail: neutral before a Keeper is held, the leading Path's token after; a second Path changes the tip and nothing else.
	const FDTKeeperRunState None = Keepers(EDTPath::None, false);
	TestEqual(TEXT("no Keeper: the neutral slot"), DTTracerArt::BodySlot(None), DTTracerArt::NeutralSlot);
	TestEqual(TEXT("no Keeper: the tip is neutral too"), DTTracerArt::TipSlot(None), DTTracerArt::NeutralSlot);
	TestTrue(TEXT("the neutral slot is Pal.Player.TrailNeutral"), DTTracerArt::SlotToken(DTTracerArt::NeutralSlot) == DTPal::Player_TrailNeutral);
	const FColor Tokens[] = { DTPal::Keeper_Storm, DTPal::Keeper_Forge, DTPal::Keeper_Tide, DTPal::Keeper_Shade, DTPal::Keeper_Radiance, DTPal::Keeper_Quicksilver };
	static_assert(UE_ARRAY_COUNT(Tokens) == static_cast<int32>(EDTPath::Count), "one token per Path");
	for (int32 First = 0; First < NumPaths; ++First)
	{
		const EDTPath Primary = static_cast<EDTPath>(First);
		const FDTKeeperRunState Picked = Keepers(Primary, false);
		TestEqual(TEXT("a Patron with no blessing yet: still neutral"), DTTracerArt::BodySlot(Picked), DTTracerArt::NeutralSlot);
		const FDTKeeperRunState Alone = Keepers(Primary, true);
		TestTrue(TEXT("the body is the leading Path's token"), DTTracerArt::SlotToken(DTTracerArt::BodySlot(Alone)) == Tokens[First]);
		TestEqual(TEXT("one Keeper: the tip is the body's colour"), DTTracerArt::TipSlot(Alone), DTTracerArt::BodySlot(Alone));
		TestFalse(TEXT("no tail token is the core's"), DTTracerArt::SlotToken(DTTracerArt::BodySlot(Alone)) == DTPal::Player_Core);
		for (int32 Other = 0; Other < NumPaths; ++Other)
		{
			const FDTKeeperRunState Offered = Keepers(Primary, true, static_cast<EDTPath>(Other), false);
			TestEqual(TEXT("a second Path before the commitment changes nothing"), DTTracerArt::TipSlot(Offered), First);
			const FDTKeeperRunState Both = Keepers(Primary, true, static_cast<EDTPath>(Other), true);
			TestEqual(TEXT("a second Path never changes the body"), DTTracerArt::BodySlot(Both), First);
			TestTrue(TEXT("the tip takes the second Path's token"), DTTracerArt::SlotToken(DTTracerArt::TipSlot(Both)) == Tokens[Other]);
		}
	}

	// As drawn: the head renderer carries the core at the undimmed level; each pair of colours has its own tail renderer.
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTBulletSubsystem* Bullets = W.World->GetSubsystem<UDTBulletSubsystem>();
	UDTKeeperSubsystem* KeeperSys = W.World->GetSubsystem<UDTKeeperSubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("bullets"), Bullets) || !TestNotNull(TEXT("keepers"), KeeperSys) || !TestNotNull(TEXT("head renderer"), Bullets->GetRenderer()))
	{
		return false;
	}
	const FLinearColor Core = DTPal::Linear(DTPal::Player_Core);
	const float HeadRow[] = { Core.R, Core.G, Core.B, static_cast<float>(DTConst::BULLETS_EMISSIVE_PLAYER) };
	FDTRunState& Run = Sim->GetRun();
	if (!TestTrue(TEXT("a player"), Run.IsValidPlayer(0)))
	{
		return false;
	}
	const int32 ActorsBefore = W.CountActors();
	const int32 Rounds = 12;
	const auto Draw = [&]()
	{
		Bullets->GetPool().Reset();
		for (int32 Index = 0; Index < Rounds; ++Index)
		{
			// Clear of the hero's clear zone round the stress hero at the origin (R-TRC-01, D-068): every round here draws a head and a tail.
			Bullets->Spawn(PlayerRound(FVector2f(400.0f + 100.0f * Index, 0.0f), FVector2f(900.0f, 0.0f), 10.0f));
		}
		FDTWorldView View;
		Bullets->GetPool().Step(1.0f / Hz, View); // one step, so every round has travelled
		Bullets->SimRender(1.0f);
		KeeperSys->SimRender(1.0f);
	};
	Run.GetPlayer(0).Keepers = Keepers(EDTPath::None, false);
	Draw();
	TestEqual(TEXT("one head per round"), Bullets->GetRenderer()->GetInstanceCount(), Rounds);
	TestTrue(TEXT("heads carry Pal.Player.Core at C.BULLETS.EMISSIVE_PLAYER"), InstancesCarry(Bullets->GetRenderer()->GetInstances(), HeadRow));
	ADTBulletRenderer* Neutral = KeeperSys->GetTrailRenderer(DTTracerArt::NeutralSlot, DTTracerArt::NeutralSlot);
	if (TestNotNull(TEXT("R-PRE-01: the neutral tail is drawn before any Keeper is held"), Neutral))
	{
		const FLinearColor N = DTPal::Linear(DTPal::Player_TrailNeutral);
		const float Row[] = { N.R, N.G, N.B, static_cast<float>(DTConst::TRC_TAIL_EMISSIVE), N.R, N.G, N.B };
		TestEqual(TEXT("one neutral tail per round"), Neutral->GetInstanceCount(), Rounds);
		TestTrue(TEXT("neutral tails carry Pal.Player.TrailNeutral, body and tip"), InstancesCarry(Neutral->GetInstances(), Row));
	}
	Run.GetPlayer(0).Keepers = Keepers(EDTPath::Forge, true, EDTPath::Tide, true);
	Draw();
	TestTrue(TEXT("with two Keepers the heads still carry the core"), InstancesCarry(Bullets->GetRenderer()->GetInstances(), HeadRow));
	ADTBulletRenderer* Pair = KeeperSys->GetTrailRenderer(static_cast<int32>(EDTPath::Forge), static_cast<int32>(EDTPath::Tide));
	if (TestNotNull(TEXT("a tail renderer for Forge body, Tide tip"), Pair))
	{
		const FLinearColor B = DTPal::Linear(DTPal::Keeper_Forge);
		const FLinearColor T = DTPal::Linear(DTPal::Keeper_Tide);
		const float Row[] = { B.R, B.G, B.B, static_cast<float>(DTConst::TRC_TAIL_EMISSIVE), T.R, T.G, T.B };
		TestEqual(TEXT("one tail per round"), Pair->GetInstanceCount(), Rounds);
		TestTrue(TEXT("Forge on the body, Tide on the tip"), InstancesCarry(Pair->GetInstances(), Row));
	}
	if (Neutral)
	{
		TestEqual(TEXT("the neutral tails are gone once a Keeper is held"), Neutral->GetInstanceCount(), 0);
	}
	// Batched (07_Technical §5): two colour pairs are two more actors, whatever the round count.
	TestEqual(TEXT("no actor per round or per tail"), W.CountActors(), ActorsBefore + 2);
	return true;
}

// T-TRC-01, T-TRC-02: the head is drawn apart from the hit radius and stretched along the flight; the tail's length follows the speed.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTracerShapes, "Dusktracer.Tracer.Shapes", DTTracerArtTest::Flags)
bool FDTTracerShapes::RunTest(const FString& Parameters)
{
	using namespace DTTracerArtTest;
	const float Radius = 12.0f;
	const float InvMesh = 1.0f / 50.0f;
	const float InvPlane = 1.0f / DTTracerArt::PlaneSizeUU;
	const FVector2f At(300.0f, -200.0f);
	const float HeadHalf = Radius * static_cast<float>(DTConst::TRC_HEAD_WIDTH_RATIO);

	const FTransform Still = DTTracerArt::Head(At, FVector2f::ZeroVector, Radius, InvMesh, Z);
	TestTrue(TEXT("a round at rest is round, at C.TRC.HEAD_WIDTH_RATIO of the hit radius"), Still.GetScale3D().Equals(FVector(HeadHalf * InvMesh), Tolerance));
	TestTrue(TEXT("the head is drawn where the round is"), Still.GetLocation().Equals(FVector(At.X, At.Y, Z), Tolerance));
	for (const FVector2f& Dir : { FVector2f(1.0f, 0.0f), FVector2f(0.0f, 1.0f), FVector2f(-1.0f, 0.0f), FVector2f(0.0f, -1.0f), FVector2f(0.6f, -0.8f), FVector2f(-0.6f, 0.8f) })
	{
		const FVector2f Step = Dir * (1800.0f / Hz);
		const FTransform Head = DTTracerArt::Head(At, Step, Radius, InvMesh, Z);
		TestTrue(TEXT("the head is stretched by C.TRC.HEAD_STRETCH along its flight only"),
			Head.GetScale3D().Equals(FVector(HeadHalf * static_cast<float>(DTConst::TRC_HEAD_STRETCH) * InvMesh, HeadHalf * InvMesh, HeadHalf * InvMesh), Tolerance));
		TestTrue(TEXT("the head points down its flight"), Head.GetRotation().GetForwardVector().Equals(FVector(Dir.X, Dir.Y, 0.0), Tolerance));
		TestTrue(TEXT("the stretch never moves the head off the round"), Head.GetLocation().Equals(FVector(At.X, At.Y, Z), Tolerance));

		FTransform Tail;
		TestTrue(TEXT("a moving round has a tail"), DTTracerArt::Tail(At, Step, Radius, 1.0f, InvPlane, Z, Tail));
		const float Length = static_cast<float>(Tail.GetScale3D().X) * DTTracerArt::PlaneSizeUU;
		TestEqual(TEXT("tail length is C.TRC.TAIL_S of flight"), Length, DTTracerArt::TailLength(1800.0f, Radius), Tolerance);
		TestTrue(TEXT("the tail's head end points down the flight"), Tail.GetRotation().GetForwardVector().Equals(FVector(Dir.X, Dir.Y, 0.0), Tolerance));
		const FVector2f Mid(static_cast<float>(Tail.GetLocation().X), static_cast<float>(Tail.GetLocation().Y));
		TestTrue(TEXT("the tail runs from the head's centre back along the flight"), (Mid + Dir * (0.5f * Length)).Equals(At, 0.01f));
		TestEqual(TEXT("tail width is C.TRC.TAIL_WIDTH_RATIO of the head's"), static_cast<float>(Tail.GetScale3D().Y) * DTTracerArt::PlaneSizeUU,
			2.0f * HeadHalf * static_cast<float>(DTConst::TRC_TAIL_WIDTH_RATIO), Tolerance);
		FTransform Narrow;
		DTTracerArt::Tail(At, Step, Radius, 0.5f, InvPlane, Z, Narrow);
		TestEqual(TEXT("the width dim scales the width"), static_cast<float>(Narrow.GetScale3D().Y), 0.5f * static_cast<float>(Tail.GetScale3D().Y), Tolerance);
		TestEqual(TEXT("and leaves the length"), static_cast<float>(Narrow.GetScale3D().X), static_cast<float>(Tail.GetScale3D().X), Tolerance);
	}
	FTransform Unused;
	TestFalse(TEXT("a round at rest has no tail"), DTTracerArt::Tail(At, FVector2f::ZeroVector, Radius, 1.0f, InvPlane, Z, Unused));

	// Length: the speed's share between the shortest and the longest tail, never shorter for a faster round.
	const float Shortest = HeadHalf * static_cast<float>(DTConst::TRC_TAIL_MIN_RATIO);
	TestEqual(TEXT("a slow round keeps the shortest tail"), DTTracerArt::TailLength(1.0f, Radius), Shortest, Tolerance);
	TestEqual(TEXT("a very fast round stops at C.TRC.TAIL_MAX_UU"), DTTracerArt::TailLength(100000.0f, Radius), static_cast<float>(DTConst::TRC_TAIL_MAX_UU), Tolerance);
	const float Between = 0.5f * (Shortest + static_cast<float>(DTConst::TRC_TAIL_MAX_UU));
	TestEqual(TEXT("between them the tail is C.TRC.TAIL_S of flight"), DTTracerArt::TailLength(Between / static_cast<float>(DTConst::TRC_TAIL_S), Radius), Between, 0.01f);
	float Last = 0.0f;
	for (float Speed = 0.0f; Speed <= 4000.0f; Speed += 50.0f)
	{
		const float Length = DTTracerArt::TailLength(Speed, Radius);
		TestTrue(TEXT("a faster round never has a shorter tail"), Length >= Last);
		Last = Length;
	}
	return true;
}

// T-TRC-04, T-TRC-07, T-TRC-08: the enemy round's trail and muzzle flash, read from the pool and batched; the round itself is drawn as before.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTracerEnemyTrail, "Dusktracer.Tracer.EnemyTrail", DTTracerArtTest::Flags)
bool FDTTracerEnemyTrail::RunTest(const FString& Parameters)
{
	using namespace DTTracerArtTest;
	const float InvPlane = 1.0f / DTTracerArt::PlaneSizeUU;
	const float Radius = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
	const float Drawn = Radius * static_cast<float>(DTConst::ENEMY_FIRE_DRAW_RADIUS_RATIO);
	const float Speed = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_SPEED_UU_S);
	const FVector2f Dir(0.0f, 1.0f);
	const FVector2f At(500.0f, 100.0f);

	FTransform Trail;
	TestTrue(TEXT("a round at shot speed has a trail"), DTTracerArt::EnemyTrail(At, Dir * Speed, Radius, InvPlane, Z, Trail));
	const float Length = static_cast<float>(Trail.GetScale3D().X) * DTTracerArt::PlaneSizeUU;
	TestEqual(TEXT("the trail shows C.TRC.ENEMY_TRAIL_S of flight behind the drawn round"), Length, Drawn + Speed * static_cast<float>(DTConst::TRC_ENEMY_TRAIL_S), Tolerance);
	// A Turret's burst puts rounds C.ENEMY_FIRE.BURST_GAP_S apart on one line: a trail must end before the centre of the round behind it.
	TestTrue(TEXT("a trail never reaches the next round of a burst"), Length < Speed * static_cast<float>(DTConst::ENEMY_FIRE_BURST_GAP_S));
	const FVector2f Mid(static_cast<float>(Trail.GetLocation().X), static_cast<float>(Trail.GetLocation().Y));
	TestTrue(TEXT("the trail lies behind the round"), (Mid + Dir * (0.5f * Length)).Equals(At, 0.01f));
	TestTrue(TEXT("no halo and no ring: the trail is narrower than the round it follows"), static_cast<float>(Trail.GetScale3D().Y) * DTTracerArt::PlaneSizeUU < 2.0f * Drawn);
	TestFalse(TEXT("a round too slow to show a trail has none"), DTTracerArt::EnemyTrail(At, Dir * 10.0f, Radius, InvPlane, Z, Trail));
	TestFalse(TEXT("a round at rest has none"), DTTracerArt::EnemyTrail(At, FVector2f::ZeroVector, Radius, InvPlane, Z, Trail));

	// The flash: at the firing point, for C.TRC.MUZZLE_S, shrinking.
	const FVector2f Origin(500.0f, 0.0f);
	const float Lasts = static_cast<float>(DTConst::TRC_MUZZLE_S);
	FTransform Fresh;
	FTransform Old;
	TestTrue(TEXT("a round just fired flashes"), DTTracerArt::MuzzleFlash(Origin, Origin, Dir * Speed, Radius, InvPlane, Z, Fresh));
	TestEqual(TEXT("a fresh flash is C.TRC.MUZZLE_LENGTH_RATIO drawn radii long"), static_cast<float>(Fresh.GetScale3D().X) * DTTracerArt::PlaneSizeUU,
		Drawn * static_cast<float>(DTConst::TRC_MUZZLE_LENGTH_RATIO), Tolerance);
	TestTrue(TEXT("the flash points back at the muzzle"), Fresh.GetRotation().GetForwardVector().Equals(FVector(-Dir.X, -Dir.Y, 0.0), Tolerance));
	const float FreshLength = static_cast<float>(Fresh.GetScale3D().X) * DTTracerArt::PlaneSizeUU;
	const FVector2f FreshMid(static_cast<float>(Fresh.GetLocation().X), static_cast<float>(Fresh.GetLocation().Y));
	TestTrue(TEXT("its bright end is at the hull's edge, a Turret's radius down the flight"),
		(FreshMid - Dir * (0.5f * FreshLength)).Equals(Origin + Dir * static_cast<float>(DTConst::ROLE_TURRET_RADIUS_UU), 0.01f));
	TestTrue(TEXT("half way through it still flashes"), DTTracerArt::MuzzleFlash(Origin, Origin + Dir * (Speed * 0.5f * Lasts), Dir * Speed, Radius, InvPlane, Z, Old));
	TestTrue(TEXT("and has shrunk"), Old.GetScale3D().X < Fresh.GetScale3D().X && Old.GetScale3D().Y < Fresh.GetScale3D().Y);
	TestFalse(TEXT("after C.TRC.MUZZLE_S it is over"), DTTracerArt::MuzzleFlash(Origin, Origin + Dir * (Speed * 1.01f * Lasts), Dir * Speed, Radius, InvPlane, Z, Old));
	TestEqual(TEXT("age is the distance flown over the speed"), DTTracerArt::RoundAge(Origin, Origin + Dir * Speed, Dir * Speed), 1.0f, Tolerance);

	// As drawn: the renderer's two extra components, the tokens at their fixed strengths, no actor per round.
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("enemies"), Enemies) || !TestNotNull(TEXT("round renderer"), Enemies->GetRoundRenderer()))
	{
		return false;
	}
	ADTEnemyBulletRenderer* Renderer = Enemies->GetRoundRenderer();
	if (!TestTrue(TEXT("M_EnemyRound_Trail is in the build"), Renderer->HasTrailMaterial()))
	{
		return false;
	}
	FDTEnemyBulletPool& Rounds = Enemies->GetPool().GetRounds();
	const int32 Fast = 40;
	const int32 Slow = 24;
	const auto Spawn = [&](int32 Count, float RoundSpeed)
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = UE_TWO_PI * Index / Count;
			const FVector2f Out(FMath::Cos(Angle), FMath::Sin(Angle));
			FDTEnemyRoundSpawn Round;
			Round.Position = Out * 1500.0f; // away from the stress hero at the origin
			Round.Velocity = Out * RoundSpeed;
			Round.Radius = Radius;
			Round.Life = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_LIFE_S);
			Rounds.Spawn(Round);
		}
	};
	Spawn(Fast, Speed);
	Spawn(Slow, 10.0f);
	const int32 ActorsBefore = W.CountActors();
	Enemies->SimRender(0.0f);
	TestEqual(TEXT("one round instance per round, as before"), Renderer->GetInstanceCount(), Fast + Slow);
	TestEqual(TEXT("a trail behind each round fast enough to show one"), Renderer->GetTrails()->GetInstanceCount(), Fast);
	TestEqual(TEXT("a flash for every round just fired"), Renderer->GetFlashes()->GetInstanceCount(), Fast + Slow);
	const FLinearColor Rim = DTPal::Linear(DTPal::Enemy_Bullet);
	const float TrailRow[] = { Rim.R, Rim.G, Rim.B, static_cast<float>(DTConst::TRC_ENEMY_TRAIL_EMISSIVE), static_cast<float>(DTConst::TRC_ENEMY_TRAIL_OPACITY) };
	TestTrue(TEXT("trails carry Pal.Enemy.Bullet at their fixed strength"), InstancesCarry(Renderer->GetTrails(), TrailRow));
	const FLinearColor Muzzle = DTPal::Linear(DTPal::Enemy_Muzzle);
	const float FlashRow[] = { Muzzle.R, Muzzle.G, Muzzle.B, static_cast<float>(DTConst::TRC_MUZZLE_EMISSIVE), static_cast<float>(DTConst::TRC_MUZZLE_OPACITY) };
	TestTrue(TEXT("R-PRE-02: flashes carry Pal.Enemy.Muzzle"), InstancesCarry(Renderer->GetFlashes(), FlashRow));
	TestTrue(TEXT("the trail is dimmer than the rim it follows"), DTConst::TRC_ENEMY_TRAIL_EMISSIVE < DTConst::ENEMY_FIRE_EMISSIVE);
	FTransform First;
	Renderer->GetInstances()->GetInstanceTransform(0, First);
	TestEqual(TEXT("the round keeps C.ENEMY_FIRE.DRAW_RADIUS_RATIO"), static_cast<float>(First.GetScale3D().X) * Renderer->GetMeshRadius(), Drawn, 0.01f);

	// Half a second on, the flashes are over and the trails remain; the pool was only read.
	const TArray<FVector2f> Before(Rounds.GetPositions());
	Enemies->SimRender(1.0f);
	TestTrue(TEXT("drawing writes nothing to the pool"), Before == TArray<FVector2f>(Rounds.GetPositions()));
	Sim->StepNow(FMath::RoundToInt32(0.5f * Hz));
	Enemies->SimRender(0.0f);
	TestEqual(TEXT("flashes end"), Renderer->GetFlashes()->GetInstanceCount(), 0);
	TestEqual(TEXT("trails stay with their rounds"), Renderer->GetTrails()->GetInstanceCount(), Fast);
	TestEqual(TEXT("no actor per round, trail or flash"), W.CountActors(), ActorsBefore);
	Rounds.Reset();
	Enemies->SimRender(0.0f);
	TestEqual(TEXT("trails follow the count down"), Renderer->GetTrails()->GetInstanceCount(), 0);
	return true;
}

// T-TRC-05: tails, trails and flashes are drawn under every telegraph layer and every round, and the two plane materials are the ones
// that put them there. The frame itself is read by gate step contrast on Tools/Scenes/tracer_crossing.json.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTracerUnderTelegraphs, "Dusktracer.Tracer.UnderTelegraphs", DTTracerArtTest::Flags)
bool FDTTracerUnderTelegraphs::RunTest(const FString& Parameters)
{
	using namespace DTTracerArtTest;
	const float TailZ = static_cast<float>(DTConst::TRC_TAIL_Z_UU);
	const float Layer = static_cast<float>(DTConst::PRE_TELEGRAPH_LAYER_UU);
	// The lowest thing a telegraph draws is the Danger outlines band, one layer under the dark band (settings.md R-SET-10).
	TestTrue(TEXT("under the lowest telegraph layer"), TailZ < static_cast<float>(DTConst::ENEMIES_TELEGRAPH_Z_UU) - Layer);
	TestTrue(TEXT("over the control cues"), TailZ > static_cast<float>(DTConst::PRE_CUE_Z_UU));
	TestTrue(TEXT("over the zones"), TailZ > static_cast<float>(DTConst::PRE_ZONE_Z_UU + DTConst::PRE_ZONE_HALF_UU));
	const float RoundBottom = static_cast<float>(DTConst::SIM_PLANE_Z_UU) - static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU * DTConst::ENEMY_FIRE_DRAW_RADIUS_RATIO);
	TestTrue(TEXT("under the lowest point of a drawn enemy round"), TailZ < RoundBottom);

	const UMaterialInterface* Tail = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DT/Materials/M_Tracer_Tail.M_Tracer_Tail"));
	const UMaterialInterface* Trail = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DT/Materials/M_EnemyRound_Trail.M_EnemyRound_Trail"));
	const UMaterialInterface* Head = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DT/Materials/M_Tracer.M_Tracer"));
	if (!TestNotNull(TEXT("M_Tracer_Tail"), Tail) || !TestNotNull(TEXT("M_EnemyRound_Trail"), Trail) || !TestNotNull(TEXT("M_Tracer"), Head))
	{
		return false;
	}
	TestTrue(TEXT("the head is opaque: it writes depth, so it hides what is under it"), Head->GetBlendMode() == BLEND_Opaque);
	TestTrue(TEXT("the tail is additive"), Tail->GetBlendMode() == BLEND_Additive);
	TestTrue(TEXT("the enemy trail is translucent and not additive: trails that overlap do not add up to a glow"), Trail->GetBlendMode() == BLEND_Translucent);
	for (const UMaterialInterface* Material : { Tail, Trail, Head })
	{
		TestTrue(TEXT("unlit: the sun does not move it off its token"), Material->GetShadingModels().HasOnlyShadingModel(MSM_Unlit));
	}
	FMaterialParameterInfo Dim(TEXT("Dim"));
	float Value = 0.0f;
	TestTrue(TEXT("the head has the Dim parameter"), Head->GetScalarParameterValue(Dim, Value) && Value == 1.0f);
	TestTrue(TEXT("the tail has the Dim parameter"), Tail->GetScalarParameterValue(Dim, Value) && Value == 1.0f);
	TestFalse(TEXT("the enemy trail has none: nothing dims the enemy side"), Trail->GetScalarParameterValue(Dim, Value));
	return true;
}

// T-PRE-06 (the tracer half), T-TRC-06: player emissive and tail width fall with the live player-round count; enemy-side emissive is the
// same at every density.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPreDensityLod, "Dusktracer.Pre.DensityLod", DTTracerArtTest::Flags)
bool FDTPreDensityLod::RunTest(const FString& Parameters)
{
	using namespace DTTracerArtTest;
	TestEqual(TEXT("no rounds: undimmed"), DTTracerArt::HeadDim(0), 1.0f);
	TestEqual(TEXT("up to C.TRC.DIM_START_ROUNDS: undimmed"), DTTracerArt::HeadDim(DTConst::TRC_DIM_START_ROUNDS), 1.0f);
	TestEqual(TEXT("tails too"), DTTracerArt::TailDim(DTConst::TRC_DIM_START_ROUNDS), 1.0f);
	TestEqual(TEXT("and their width"), DTTracerArt::TailWidthDim(DTConst::TRC_DIM_START_ROUNDS), 1.0f);
	TestEqual(TEXT("from C.TRC.DIM_FULL_ROUNDS: the head's floor"), DTTracerArt::HeadDim(DTConst::TRC_DIM_FULL_ROUNDS), static_cast<float>(DTConst::TRC_DIM_EMISSIVE_MIN));
	TestEqual(TEXT("the tail's floor"), DTTracerArt::TailDim(DTConst::BULLETS_CAP), static_cast<float>(DTConst::TRC_DIM_TAIL_MIN));
	TestEqual(TEXT("the width's floor"), DTTracerArt::TailWidthDim(DTConst::BULLETS_CAP), static_cast<float>(DTConst::TRC_DIM_WIDTH_MIN));
	TestTrue(TEXT("the cap is past the full count, so the cap draws fully dimmed"), DTConst::BULLETS_CAP >= DTConst::TRC_DIM_FULL_ROUNDS);
	TestTrue(TEXT("every floor dims and none switches fire off"), DTConst::TRC_DIM_EMISSIVE_MIN > 0.0f && DTConst::TRC_DIM_EMISSIVE_MIN < 1.0f
		&& DTConst::TRC_DIM_TAIL_MIN > 0.0f && DTConst::TRC_DIM_TAIL_MIN < 1.0f && DTConst::TRC_DIM_WIDTH_MIN > 0.0f && DTConst::TRC_DIM_WIDTH_MIN < 1.0f);
	float LastHead = 1.0f;
	float LastTail = 1.0f;
	float LastWidth = 1.0f;
	for (int32 Live = 0; Live <= DTConst::BULLETS_CAP; Live += 25)
	{
		const float Head = DTTracerArt::HeadDim(Live);
		const float Tail = DTTracerArt::TailDim(Live);
		const float Width = DTTracerArt::TailWidthDim(Live);
		TestTrue(TEXT("more rounds never brighten or widen player fire"), Head <= LastHead && Tail <= LastTail && Width <= LastWidth);
		LastHead = Head;
		LastTail = Tail;
		LastWidth = Width;
	}

	// As drawn, at no player fire, at a few rounds and at the cap.
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTBulletSubsystem* Bullets = W.World->GetSubsystem<UDTBulletSubsystem>();
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	UDTKeeperSubsystem* KeeperSys = W.World->GetSubsystem<UDTKeeperSubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("bullets"), Bullets) || !TestNotNull(TEXT("enemies"), Enemies) || !TestNotNull(TEXT("keepers"), KeeperSys)
		|| !TestNotNull(TEXT("head renderer"), Bullets->GetRenderer()) || !TestNotNull(TEXT("round renderer"), Enemies->GetRoundRenderer())
		|| !TestNotNull(TEXT("telegraph renderer"), Enemies->GetTelegraphRenderer()))
	{
		return false;
	}
	FDTRunState& Run = Sim->GetRun();
	if (!TestTrue(TEXT("a player"), Run.IsValidPlayer(0)))
	{
		return false;
	}
	Run.GetPlayer(0).Keepers = Keepers(EDTPath::Storm, true);
	FDTEnemyBulletPool& Rounds = Enemies->GetPool().GetRounds();
	for (int32 Index = 0; Index < 64; ++Index)
	{
		const float Angle = UE_TWO_PI * Index / 64.0f;
		const FVector2f Out(FMath::Cos(Angle), FMath::Sin(Angle));
		FDTEnemyRoundSpawn Round;
		Round.Position = Out * 1500.0f;
		Round.Velocity = Out * static_cast<float>(DTConst::ENEMY_FIRE_SHOT_SPEED_UU_S);
		Round.Radius = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
		Round.Life = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_LIFE_S);
		Rounds.Spawn(Round);
	}
	FDTTelegraph Band;
	Band.Shape = EDTTelegraphShape::Band;
	Band.Length = 400.0f;
	Band.Width = 80.0f;
	const TArray<FDTTelegraph> Shown = { Band };

	ADTEnemyBulletRenderer* RoundRenderer = Enemies->GetRoundRenderer();
	TArray<float> RoundData;
	TArray<float> TrailData;
	TArray<float> TelegraphData;
	float TailWidthFew = 0.0f;
	for (const int32 PlayerRounds : { 0, 8, DTConst::BULLETS_CAP })
	{
		Bullets->GetPool().Reset();
		for (int32 Index = 0; Index < PlayerRounds; ++Index)
		{
			Bullets->Spawn(PlayerRound(FVector2f(0.0f, -3000.0f), FVector2f(900.0f, 0.0f), 10.0f));
		}
		FDTWorldView View;
		Bullets->GetPool().Step(1.0f / Hz, View);
		Bullets->SimRender(1.0f);
		KeeperSys->SimRender(1.0f);
		Enemies->SimRender(0.0f);
		Enemies->GetTelegraphRenderer()->SetTelegraphs(Shown);

		// Player side.
		const float WantHead = DTTracerArt::HeadDim(PlayerRounds);
		TestEqual(TEXT("the head's Dim follows the live count"), Bullets->GetRenderer()->GetDim(), WantHead, 0.005f);
		if (PlayerRounds > 0)
		{
			const FLinearColor Core = DTPal::Linear(DTPal::Player_Core);
			const float HeadRow[] = { Core.R, Core.G, Core.B, static_cast<float>(DTConst::BULLETS_EMISSIVE_PLAYER) };
			TestTrue(TEXT("C.BULLETS.EMISSIVE_PLAYER stays the instance's level: the dim is the material's"), InstancesCarry(Bullets->GetRenderer()->GetInstances(), HeadRow));
			ADTBulletRenderer* Tails = KeeperSys->GetTrailRenderer(static_cast<int32>(EDTPath::Storm), static_cast<int32>(EDTPath::Storm));
			if (TestNotNull(TEXT("tails drawn"), Tails) && TestEqual(TEXT("one tail per round"), Tails->GetInstanceCount(), PlayerRounds))
			{
				TestEqual(TEXT("the tail's Dim follows the live count"), Tails->GetDim(), DTTracerArt::TailDim(PlayerRounds), 0.005f);
				FTransform Tail;
				Tails->GetInstances()->GetInstanceTransform(0, Tail);
				const float Width = static_cast<float>(Tail.GetScale3D().Y);
				if (PlayerRounds < DTConst::TRC_DIM_START_ROUNDS)
				{
					TailWidthFew = Width;
				}
				else
				{
					TestEqual(TEXT("at the cap the tail is C.TRC.DIM_WIDTH_MIN as wide"), Width, TailWidthFew * static_cast<float>(DTConst::TRC_DIM_WIDTH_MIN), 1.0e-4f);
				}
			}
		}

		// Enemy side: byte-equal at every density (R-PRE-06).
		const TArray<float>& NowRounds = RoundRenderer->GetInstances()->PerInstanceSMCustomData;
		const TArray<float>& NowTrails = RoundRenderer->GetTrails()->PerInstanceSMCustomData;
		const TArray<float>& NowTelegraphs = Enemies->GetTelegraphRenderer()->GetInstances()->PerInstanceSMCustomData;
		TestTrue(TEXT("enemy rounds are drawn"), NowRounds.Num() > 0 && NowRounds[3] == static_cast<float>(DTConst::ENEMY_FIRE_EMISSIVE));
		TestTrue(TEXT("their trails are drawn"), !RoundRenderer->HasTrailMaterial() || NowTrails.Num() > 0);
		TestTrue(TEXT("the telegraph is drawn"), NowTelegraphs.Num() > 0);
		if (PlayerRounds == 0)
		{
			RoundData = NowRounds;
			TrailData = NowTrails;
			TelegraphData = NowTelegraphs;
		}
		else
		{
			TestTrue(TEXT("enemy round emissive is unchanged at this density"), NowRounds == RoundData);
			TestTrue(TEXT("enemy trail emissive is unchanged at this density"), NowTrails == TrailData);
			TestTrue(TEXT("telegraph emissive is unchanged at this density"), NowTelegraphs == TelegraphData);
		}
	}
	return true;
}

// T-PRE-03: every attack row of the catalogs names a telegraph shape the renderer draws; the red drawn is the published footprint in
// Pal.Telegraph; the role attacks publish the area their damage covers.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTPreTelegraphCoverage, "Dusktracer.Pre.TelegraphCoverage", DTTracerArtTest::Flags)
bool FDTPreTelegraphCoverage::RunTest(const FString& Parameters)
{
	using namespace DTTracerArtTest;
	TArray<EDTTelegraphShape> Named;
	const auto Check = [&](const TCHAR* Catalog, const TCHAR* RowName, const TCHAR* Attack, const TCHAR* Telegraph, bool bIsAttack)
	{
		EDTTelegraphShape Shape = EDTTelegraphShape::Band;
		if (bIsAttack)
		{
			if (TestTrue(*FString::Printf(TEXT("%s %s: attack %s names a telegraph shape the renderer draws ('%s')"), Catalog, RowName, Attack, Telegraph), ShapeOf(Telegraph, Shape)))
			{
				Named.AddUnique(Shape);
			}
		}
		else
		{
			TestEqual(*FString::Printf(TEXT("%s %s: no attack, no telegraph"), Catalog, RowName), FCString::Strlen(Telegraph), 0);
		}
	};
	for (const DTCat::FEnemyRow& R : DTCat::Enemies)
	{
		Check(TEXT("enemies.csv"), R.RoleTag, R.Attack, R.Telegraph, FCString::Strlen(R.Attack) > 0);
	}
	for (const DTCat::FAffixRow& R : DTCat::Affixes)
	{
		Check(TEXT("elites.csv"), R.Id, R.Attack, R.Telegraph, FCString::Strlen(R.Attack) > 0);
	}
	for (const DTCat::FBossAttackRow& R : DTCat::BossWarden)
	{
		// Adds is the one boss row that is not an attack (boss.md): it spawns Tanks and deals nothing itself.
		Check(TEXT("boss_warden.csv"), R.Attack, R.Attack, R.Telegraph, FCString::Strcmp(R.Attack, TEXT("Adds")) != 0);
	}
	TestTrue(TEXT("the catalogs name shapes"), Named.Num() > 0);

	// As drawn: for each named shape, the red rim is the published footprint itself, in Pal.Telegraph at C.ENEMIES.TELEGRAPH_EMISSIVE.
	TestEqual(TEXT("the telegraph colour is Pal.Telegraph"), UDTEnemySubsystem::GetTelegraphColor(), DTPal::Linear(DTPal::Telegraph));
	FTestWorld W;
	UDTEnemySubsystem* Enemies = W.World->GetSubsystem<UDTEnemySubsystem>();
	if (!TestNotNull(TEXT("enemies"), Enemies) || !TestNotNull(TEXT("telegraph renderer"), Enemies->GetTelegraphRenderer()))
	{
		return false;
	}
	ADTTelegraphRenderer* Renderer = Enemies->GetTelegraphRenderer();
	const FLinearColor Red = DTPal::Linear(DTPal::Telegraph);
	const float Emissive = static_cast<float>(DTConst::ENEMIES_TELEGRAPH_EMISSIVE);
	for (const EDTTelegraphShape Shape : Named)
	{
		FDTTelegraph T;
		T.Shape = Shape;
		T.Origin = FVector2f(120.0f, -80.0f);
		T.Dir = FVector2f(0.0f, 1.0f);
		T.Length = 600.0f;
		T.Width = Shape == EDTTelegraphShape::AimLine ? 2.0f * static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU) : 90.0f;
		T.Progress = 1.0f;
		const TArray<FDTTelegraph> One = { T };
		Renderer->SetTelegraphs(One);
		TArray<FTransform> Footprint;
		const float RimZ = static_cast<float>(DTConst::ENEMIES_TELEGRAPH_Z_UU + DTConst::PRE_TELEGRAPH_LAYER_UU);
		ADTTelegraphRenderer::AppendTransforms(T, Renderer->GetMeshSizeUU(), RimZ, Footprint);
		const UInstancedStaticMeshComponent* Part = Renderer->GetInstances();
		const int32 Stride = Part->NumCustomDataFloats;
		int32 FullRed = 0;
		int32 OnFootprint = 0;
		bool bOnlyRedOrEdge = Part->PerInstanceSMCustomData.Num() == Part->GetInstanceCount() * Stride;
		const FLinearColor Edge = DTPal::Linear(DTPal::Telegraph_Edge);
		for (int32 Instance = 0; bOnlyRedOrEdge && Instance < Part->GetInstanceCount(); ++Instance)
		{
			const float* Data = &Part->PerInstanceSMCustomData[Instance * Stride];
			const bool bRed = Data[0] == Red.R && Data[1] == Red.G && Data[2] == Red.B;
			const bool bEdge = Data[0] == Edge.R && Data[1] == Edge.G && Data[2] == Edge.B;
			bOnlyRedOrEdge &= bRed || bEdge;
			FTransform Drawn;
			Part->GetInstanceTransform(Instance, Drawn, true);
			if (bRed && Data[3] == Emissive && FMath::IsNearlyEqual(static_cast<float>(Drawn.GetLocation().Z), RimZ, 1.0e-3f))
			{
				++FullRed;
				OnFootprint += Footprint.ContainsByPredicate([&Drawn](const FTransform& Q) { return Q.Equals(Drawn, 1.0e-2); }) ? 1 : 0;
			}
		}
		const FString Label = FString::Printf(TEXT("shape %d"), static_cast<int32>(Shape));
		TestTrue(*(Label + TEXT(": every quad is Pal.Telegraph or its dark edge")), bOnlyRedOrEdge);
		TestEqual(*(Label + TEXT(": the rim is drawn at full emissive, one quad per footprint quad")), FullRed, Footprint.Num());
		TestEqual(*(Label + TEXT(": the rim is the published footprint, not grown and not shrunk")), OnFootprint, Footprint.Num());
	}
	Renderer->SetTelegraphs(TConstArrayView<FDTTelegraph>());

	// The role attacks publish what their damage covers: the Rusher's band is its charge, the Turret's lines are its rounds' paths.
	{
		FDTRunState Run;
		Run.Start(1, 1);
		FDTWorldView View;
		View.HeroPositions = { FVector2f(600.0f, 0.0f) };
		View.HeroIntangible.Init(false, 1);
		FDTSimContext Context;
		Context.StepSeconds = 1.0f / Hz;
		Context.Run = &Run;
		Context.View = &View;
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Rusher, FVector2f::ZeroVector);
		Context.StepIndex = 0;
		Pool.StepEnemies(Context);
		if (TestEqual(TEXT("Rusher: a band from the first step of the wind-up"), View.Telegraphs.Num(), 1))
		{
			const FDTTelegraph& Band = View.Telegraphs[0];
			TestEqual(TEXT("Rusher: band shape"), Band.Shape, EDTTelegraphShape::Band);
			TestEqual(TEXT("Rusher: as wide as the hull that hits"), Band.Width, 2.0f * static_cast<float>(DTConst::ROLE_RUSHER_RADIUS_UU));
			TestEqual(TEXT("Rusher: as long as the charge"), Band.Length, static_cast<float>(DTConst::ROLE_RUSHER_CHARGE_SPEED_UU_S * DTConst::ROLE_RUSHER_CHARGE_S), 0.5f);
		}
	}
	{
		FDTRunState Run;
		Run.Start(1, 1);
		FDTWorldView View;
		View.HeroPositions = { FVector2f(1000.0f, 0.0f) };
		View.HeroIntangible.Init(false, 1);
		FDTSimContext Context;
		Context.StepSeconds = 1.0f / Hz;
		Context.Run = &Run;
		Context.View = &View;
		FDTEnemyPool Pool;
		Pool.Spawn(EDTEnemyRole::Turret, FVector2f::ZeroVector);
		Pool.SetPattern(0, EDTFirePattern::Burst);
		Pool.SetFacing(0, FVector2f(1.0f, 0.0f));
		TArray<FDTTelegraph> Lines;
		for (int32 Step = 0; Step < FMath::RoundToInt32(2.0f * Hz) && Pool.GetRounds().Num() == 0; ++Step)
		{
			Context.StepIndex = Step;
			Pool.StepEnemies(Context);
			Pool.ResolveHits(Context);
			if (View.Telegraphs.Num() > 0)
			{
				Lines = View.Telegraphs;
			}
		}
		if (TestTrue(TEXT("Turret: a round was fired after an aim line"), Pool.GetRounds().Num() > 0 && Lines.Num() > 0))
		{
			const FVector2f Flight = Pool.GetRounds().GetVelocities()[0].GetSafeNormal();
			TestTrue(TEXT("Turret: the round flies down a line shown, as wide as the round"), Lines.ContainsByPredicate([&Flight](const FDTTelegraph& T)
			{
				return T.Shape == EDTTelegraphShape::AimLine && T.Dir.Equals(Flight, 1.0e-3f) && T.Width == 2.0f * static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
			}));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
