// Hero tests (02_Systems/combat_core.md T-CMB-01..12). Drive FDTHeroSim through FDTHeroInput only, with a hand-built view.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Combat; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Engine/World.h"
#include "GameFramework/MovementComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Math/RandomStream.h"
#include "Player/DTHero.h"
#include "Player/DTHeroSim.h"
#include "Player/DTHeroSubsystem.h"
#include "Player/DTInputSet.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTHeroTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	// Position tolerance in uu for sums of float steps.
	constexpr float PosTolerance = 0.05f;

	static float StepDt() { return 1.0f / DTConst::SIM_FIXED_HZ; }
	static int32 StepsFor(float Seconds) { return FMath::RoundToInt(Seconds * DTConst::SIM_FIXED_HZ); }
	static float Limit() { return static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU - DTConst::HERO_RADIUS_UU); }

	static FDTHeroInput MoveInput(const FVector2f& Move)
	{
		FDTHeroInput In;
		In.Move = Move;
		return In;
	}

	static FDTHeroInput DashInput(const FVector2f& Move = FVector2f::ZeroVector)
	{
		FDTHeroInput In;
		In.Move = Move;
		In.bDashPress = true;
		return In;
	}

	static FDTHeroInput IntercessionInput()
	{
		FDTHeroInput In;
		In.bIntercessionPress = true;
		return In;
	}

	static FDTHeroHit MakeHit(int32 Player, float Damage, EDTHeroHitKind Kind = EDTHeroHitKind::Contact, uint32 Serial = 1,
		EDTEnemyRole Role = EDTEnemyRole::Swarm)
	{
		FDTHeroHit Hit;
		Hit.PlayerId = Player;
		Hit.Damage = Damage;
		Hit.Kind = Kind;
		Hit.EnemySerial = Serial;
		Hit.SourceRole = Role;
		return Hit;
	}

	/** A run, a view and the hero sim wired together; steps either with direct inputs or through the latch. */
	struct FHarness
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTHeroSim Hero;
		FDTHeroInputLatch Latch;
		FDTHeroInputPhase InputPhase;
		// Starts past step 0 so the Input phase does not reset state a test has set up.
		int64 StepIndex = 1;
		TArray<FDTHeroEvent> Events;

		explicit FHarness(int32 Players = 1)
		{
			Run.Start(1, Players);
			Hero.Reset(Players);
			InputPhase.Latch = &Latch;
			InputPhase.Hero = &Hero;
		}

		FDTSimContext MakeContext()
		{
			FDTSimContext Context;
			Context.StepSeconds = StepDt();
			Context.StepIndex = StepIndex;
			Context.Run = &Run;
			Context.View = &View;
			return Context;
		}

		void Finish(const FDTSimContext& Context)
		{
			Hero.Step(Context);
			Events.Append(Hero.GetStepEvents());
			++StepIndex;
		}

		/** One step with this input set directly (the test-harness path). Other players keep their last input. */
		void Step(const FDTHeroInput& In = FDTHeroInput(), int32 Owner = 0)
		{
			Hero.SetStepInput(Owner, In);
			Finish(MakeContext());
		}

		void Steps(int32 Count, const FDTHeroInput& In = FDTHeroInput(), int32 Owner = 0)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Step(In, Owner);
			}
		}

		/** One step through the Input phase (the pawn and bot path: whatever was latched since the last step). */
		void StepLatched()
		{
			const FDTSimContext Context = MakeContext();
			InputPhase.SimStep(Context);
			Finish(Context);
		}

		int32 Count(EDTHeroEventKind Kind, int32 Owner = 0) const
		{
			int32 N = 0;
			for (const FDTHeroEvent& Event : Events)
			{
				N += (Event.Kind == Kind && Event.OwnerId == Owner) ? 1 : 0;
			}
			return N;
		}

		const FDTHeroState& S(int32 Owner = 0) const { return Hero.GetState(Owner); }
	};

	/** Exact equality of every stepped field (R-CMB-12: bit-identical replays). */
	static bool SameState(const FDTHeroState& A, const FDTHeroState& B)
	{
		return A.Pos == B.Pos && A.PrevPos == B.PrevPos && A.Vel == B.Vel && A.LastMoveDir == B.LastMoveDir && A.AimDir == B.AimDir
			&& A.DashDir == B.DashDir && A.Hp == B.Hp && A.MaxHp == B.MaxHp && A.DashCharges == B.DashCharges && A.DashChargesMax == B.DashChargesMax
			&& A.DashRefillLeft == B.DashRefillLeft && A.DashLeft == B.DashLeft && A.HurtIFramesLeft == B.HurtIFramesLeft
			&& A.Intercession01 == B.Intercession01 && A.DashPressAge == B.DashPressAge && A.IntercessionPressAge == B.IntercessionPressAge
			&& A.bIntangible == B.bIntangible && A.bDown == B.bDown && A.Dashes == B.Dashes && A.HitsTaken == B.HitsTaken
			&& A.Intercessions == B.Intercessions;
	}

	/** Appends the given hits in EDTSimPhase::Enemies of one chosen step (what enemy contact and fire do). */
	struct FHitPublisher : IDTSimSystem
	{
		int64 AtStep = INDEX_NONE;
		TArray<FDTHeroHit> Hits;
		virtual void SimStep(const FDTSimContext& Context) override
		{
			if (Context.StepIndex == AtStep)
			{
				Context.View->HeroHits.Append(Hits);
			}
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbPerPlayerState, "Dusktracer.Combat.Hero.PerPlayerState", DTHeroTest::Flags) // T-CMB-01
bool FDTCmbPerPlayerState::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	FHarness H(2);
	H.Hero.SetStepInput(1, MoveInput(FVector2f(0.0f, -1.0f)));
	H.Steps(DTConst::SIM_FIXED_HZ, MoveInput(FVector2f(1.0f, 0.0f)), 0);
	const float Speed = static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S);
	TestEqual(TEXT("two heroes"), H.Hero.Num(), 2);
	TestEqual(TEXT("player 0 moved along +X"), H.S(0).Pos.X, Speed, PosTolerance);
	TestEqual(TEXT("player 1 moved along -Y"), H.S(1).Pos.Y, -Speed, PosTolerance);
	TestEqual(TEXT("player 0 did not move along Y"), H.S(0).Pos.Y, 0.0f);
	TestEqual(TEXT("HeroPositions holds both"), H.View.HeroPositions.Num(), 2);
	TestTrue(TEXT("HeroPositions[0] is player 0"), H.View.HeroPositions[0] == H.S(0).Pos);
	TestTrue(TEXT("HeroPositions[1] is player 1"), H.View.HeroPositions[1] == H.S(1).Pos);
	TestEqual(TEXT("HeroAimDirs per player"), H.View.HeroAimDirs.Num(), 2);
	TestEqual(TEXT("HeroIntangible per player"), H.View.HeroIntangible.Num(), 2);

	// The pawn owns no movement component (CDO subobjects), and its body never collides.
	const ADTHero* Default = GetDefault<ADTHero>();
	TArray<UObject*> Subobjects;
	GetObjectsWithOuter(Default, Subobjects, EGetObjectsFlags::IncludeNestedObjects);
	bool bHasMovement = false;
	for (const UObject* Sub : Subobjects)
	{
		bHasMovement |= Sub->IsA<UMovementComponent>();
	}
	TestFalse(TEXT("ADTHero has no movement component"), bHasMovement);
	TestNull(TEXT("ADTHero reports no movement component"), Default->GetMovementComponent());

	// Moving the pawn by hand changes nothing in the sim; the next draw puts it back.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}
	World->BeginPlay();
	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTHeroSubsystem* HeroSystem = World->GetSubsystem<UDTHeroSubsystem>();
	if (TestNotNull(TEXT("sim subsystem"), Sim) && TestNotNull(TEXT("hero subsystem"), HeroSystem))
	{
		Sim->StartRun(3, 1);
		FDTHeroInput In = MoveInput(FVector2f(0.0f, 1.0f));
		HeroSystem->LatchInput(0, In);
		Sim->StepNow(DTConst::SIM_FIXED_HZ / 2);
		const FVector2f Before = HeroSystem->GetState(0)->Pos;
		ADTHero* Pawn = World->SpawnActor<ADTHero>();
		if (TestNotNull(TEXT("pawn spawned"), Pawn))
		{
			Pawn->SetActorLocation(FVector(1234.0, -987.0, 0.0));
			TestTrue(TEXT("moving the pawn does not move the hero"), HeroSystem->GetState(0)->Pos == Before);
			TestTrue(TEXT("nor the published position"), Sim->GetView().HeroPositions[0] == Before);
			HeroSystem->LatchInput(0, FDTHeroInput());
			Sim->StepNow(1);
			TestTrue(TEXT("the next step ignores the pawn's location"), HeroSystem->GetState(0)->Pos == Before);
			HeroSystem->RegisterPawn(Pawn);
			HeroSystem->SimRender(1.0f);
			TestEqual(TEXT("the draw places the pawn at the sim position (X)"), static_cast<float>(Pawn->GetActorLocation().X), Before.X, PosTolerance);
			TestEqual(TEXT("the draw places the pawn at the sim position (Y)"), static_cast<float>(Pawn->GetActorLocation().Y), Before.Y, PosTolerance);
		}
	}
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbHeal, "Dusktracer.Combat.Hero.Heal", DTHeroTest::Flags) // T-OFF-16
bool FDTCmbHeal::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	FHarness H;
	H.Steps(1);
	const float Max = H.Hero.GetState(0).MaxHp;
	H.View.HeroHits.Add(MakeHit(0, 40.0f));
	H.Steps(1);
	TestEqual(TEXT("the hit lands"), H.Hero.GetState(0).Hp, Max - 40.0f);
	H.Hero.Heal(0, 15.0f);
	TestEqual(TEXT("a heal adds HP"), H.Hero.GetState(0).Hp, Max - 25.0f);
	H.Hero.Heal(0, 2.0f * Max);
	TestEqual(TEXT("never above max HP"), H.Hero.GetState(0).Hp, Max);

	FHarness Dead;
	Dead.Steps(1);
	Dead.View.HeroHits.Add(MakeHit(0, 10.0f * Max));
	Dead.Steps(1);
	TestTrue(TEXT("down"), Dead.Hero.GetState(0).bDown);
	const float Hp = Dead.Hero.GetState(0).Hp;
	Dead.Hero.Heal(0, Max);
	TestEqual(TEXT("a down hero stays down"), Dead.Hero.GetState(0).Hp, Hp);

	// Slam's dash_cooldown_reset (keepers.md).
	FHarness Dash;
	Dash.Steps(1);
	const int32 Charges = Dash.Hero.GetState(0).DashCharges;
	Dash.Step(DashInput(FVector2f(1.0f, 0.0f)));
	TestTrue(TEXT("a dash spends a charge"), Dash.Hero.GetState(0).DashCharges < Charges);
	Dash.Hero.RefillDash(0);
	TestEqual(TEXT("the refill returns every charge"), Dash.Hero.GetState(0).DashCharges, Charges);
	TestEqual(TEXT("no refill left to wait for"), Dash.Hero.GetState(0).DashRefillLeft, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbInputEdges, "Dusktracer.Combat.Hero.InputEdges", DTHeroTest::Flags) // T-CMB-02
bool FDTCmbInputEdges::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	FDTHeroStats Spare;
	Spare.DashChargesBonus = 2; // spare charges, so only edge handling can stop a repeat

	{
		// One press seen by several frames before one step: one dash.
		FHarness H;
		H.Hero.SetStats(0, Spare);
		H.Steps(1);
		for (int32 Frame = 0; Frame < 3; ++Frame)
		{
			H.Latch.Latch(0, DashInput(FVector2f(1.0f, 0.0f)));
		}
		H.StepLatched();
		TestEqual(TEXT("several frames, one step: one dash"), H.Count(EDTHeroEventKind::Dash), 1);
	}
	{
		// One press, then many steps in one frame (and after): still one dash.
		FHarness H;
		H.Hero.SetStats(0, Spare);
		H.Steps(1);
		H.Latch.Latch(0, DashInput(FVector2f(1.0f, 0.0f)));
		for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ; ++Step)
		{
			H.StepLatched();
		}
		TestEqual(TEXT("one press, many steps: one dash"), H.Count(EDTHeroEventKind::Dash), 1);
		TestEqual(TEXT("one charge spent"), H.S().DashCharges, FDTHeroSim::ComputeMaxCharges(Spare) - 1);
		TestEqual(TEXT("held move input stays latched"), H.S().Vel.X, static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S), PosTolerance);
	}

	// Press buffer: the charge returns C.DASH.COOLDOWN_S after the dash step.
	const int32 Refill = StepsFor(DTConst::DASH_COOLDOWN_S);
	const int32 Buffer = FMath::FloorToInt(DTConst::HERO_INPUT_BUFFER_S * DTConst::SIM_FIXED_HZ + 1e-3f);
	{
		FHarness H;
		H.Step(DashInput());                    // dash step s0
		H.Steps(Refill - Buffer - 1);           // up to s0 + Refill - Buffer - 1
		H.Step(DashInput());                    // press at s0 + Refill - Buffer: no charge yet
		TestEqual(TEXT("press without a charge does not dash yet"), H.Count(EDTHeroEventKind::Dash), 1);
		H.Steps(Buffer);                        // the charge returns at s0 + Refill
		TestEqual(TEXT("buffered press dashes when the charge returns inside the buffer"), H.Count(EDTHeroEventKind::Dash), 2);
	}
	{
		FHarness H;
		H.Step(DashInput());
		H.Steps(Refill - Buffer - 2);
		H.Step(DashInput());                    // one step earlier: the charge returns after the buffer ran out
		H.Steps(Buffer + DTConst::SIM_FIXED_HZ);
		TestEqual(TEXT("press older than the buffer is dropped"), H.Count(EDTHeroEventKind::Dash), 1);
		TestEqual(TEXT("the returned charge stays unused"), H.S().DashCharges, DTConst::DASH_CHARGES);
	}
	{
		// Radial dead zone: inside it nothing moves; just outside it the hero moves.
		FHarness H;
		const float Inside = DTConst::HERO_STICK_DEADZONE * 0.9f;
		H.Steps(DTConst::SIM_FIXED_HZ, MoveInput(FVector2f(Inside * 0.6f, Inside * 0.8f)));
		TestTrue(TEXT("stick inside the dead zone moves nothing"), H.S().Pos == FVector2f::ZeroVector);
		const float Outside = DTConst::HERO_STICK_DEADZONE + 0.1f;
		H.Steps(1, MoveInput(FVector2f(Outside, 0.0f)));
		TestTrue(TEXT("stick outside the dead zone moves"), H.S().Pos.X > 0.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbMove, "Dusktracer.Combat.Hero.Move", DTHeroTest::Flags) // T-CMB-03
bool FDTCmbMove::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	const float Speed = static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S);
	{
		FHarness H;
		H.Step(MoveInput(FVector2f(1.0f, 1.0f))); // a keyboard diagonal: both keys at full value
		TestEqual(TEXT("full speed on the first step"), H.S().Vel.Size(), Speed, 0.01f);
		TestEqual(TEXT("a diagonal is not faster"), H.S().Pos.Size(), Speed * StepDt(), 0.001f);
		TestEqual(TEXT("and goes diagonally"), H.S().Pos.X, H.S().Pos.Y, 0.001f);
	}
	{
		FHarness H;
		const int32 Cross = StepsFor(2.0f * DTConst::ARENA_HALF_EXTENT_UU / Speed);
		H.Steps(Cross, MoveInput(FVector2f(1.0f, -1.0f)));
		TestEqual(TEXT("stops at the +X edge less the radius"), H.S().Pos.X, Limit());
		TestEqual(TEXT("stops at the -Y edge less the radius"), H.S().Pos.Y, -Limit());
	}
	{
		FHarness H;
		H.View.EnemyPositions = { FVector2f(100.0f, 0.0f), FVector2f(200.0f, 0.0f), FVector2f(300.0f, 0.0f) };
		H.View.EnemyRadii = { 50.0f, 50.0f, 50.0f };
		H.View.EnemyHash.Build(H.View.EnemyPositions);
		const TArray<FVector2f> EnemiesBefore = H.View.EnemyPositions;
		H.Steps(DTConst::SIM_FIXED_HZ, MoveInput(FVector2f(1.0f, 0.0f)));
		TestEqual(TEXT("enemies in the path do not block the hero"), H.S().Pos.X, Speed, PosTolerance);
		TestTrue(TEXT("the hero does not push enemies"), H.View.EnemyPositions == EnemiesBefore);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbAim, "Dusktracer.Combat.Hero.Aim", DTHeroTest::Flags) // T-CMB-04
bool FDTCmbAim::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	{
		FHarness H;
		TestTrue(TEXT("Auto is the default"), H.Hero.GetAimMode(0) == EDTAimMode::Auto);
		H.View.EnemyPositions = { FVector2f(900.0f, 0.0f), FVector2f(0.0f, -300.0f), FVector2f(-2000.0f, 2000.0f) };
		H.View.EnemyHash.Build(H.View.EnemyPositions);
		H.Step();
		TestTrue(TEXT("Auto points at the nearest enemy"), H.S().AimDir.Equals(FVector2f(0.0f, -1.0f), 1e-5f));
		TestTrue(TEXT("published"), H.View.HeroAimDirs[0] == H.S().AimDir);
		H.View.EnemyPositions.Reset();
		H.View.EnemyHash.Build(H.View.EnemyPositions);
		H.Steps(10);
		TestTrue(TEXT("Auto holds the last direction with no enemy"), H.S().AimDir.Equals(FVector2f(0.0f, -1.0f), 1e-5f));

		// The hash query and a plain scan agree on a spread of points.
		TArray<FVector2f> Points;
		FRandomStream Rng(7);
		for (int32 Index = 0; Index < 200; ++Index)
		{
			Points.Add(FVector2f(Rng.FRandRange(-3500.0f, 3500.0f), Rng.FRandRange(-3500.0f, 3500.0f)));
		}
		FDTSpatialHash Hash;
		Hash.Build(Points);
		const FDTSpatialHash Empty;
		for (int32 Probe = 0; Probe < 20; ++Probe)
		{
			const FVector2f From(Rng.FRandRange(-3900.0f, 3900.0f), Rng.FRandRange(-3900.0f, 3900.0f));
			TestEqual(TEXT("hash nearest equals scan nearest"), FDTHeroSim::FindNearest(From, Points, Hash), FDTHeroSim::FindNearest(From, Points, Empty));
		}
	}
	{
		FHarness H;
		H.Hero.SetAimMode(0, EDTAimMode::Manual);
		H.View.EnemyPositions = { FVector2f(300.0f, 0.0f) };
		H.View.EnemyHash.Build(H.View.EnemyPositions);
		FDTHeroInput Cursor;
		Cursor.AimCursorWorld = FVector2f(0.0f, 1000.0f);
		Cursor.bAimCursorValid = true;
		H.Step(Cursor);
		TestTrue(TEXT("Manual follows the cursor, not the enemy"), H.S().AimDir.Equals(FVector2f(0.0f, 1.0f), 1e-5f));
		FDTHeroInput Stick = Cursor;
		Stick.AimStick = FVector2f(-1.0f, 0.0f);
		H.Step(Stick);
		TestTrue(TEXT("Manual follows the stick"), H.S().AimDir.Equals(FVector2f(-1.0f, 0.0f), 1e-5f));
		FDTHeroInput Neutral;
		Neutral.AimStick = FVector2f(DTConst::HERO_STICK_DEADZONE * 0.5f, 0.0f);
		H.Steps(10, Neutral);
		TestTrue(TEXT("a neutral stick holds the last direction"), H.S().AimDir.Equals(FVector2f(-1.0f, 0.0f), 1e-5f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbNoFireAction, "Dusktracer.Combat.Input.NoFireAction", DTHeroTest::Flags) // T-CMB-05
bool FDTCmbNoFireAction::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	UDTInputSet* Set = UDTInputSet::Create(GetTransientPackage());
	const TArray<UInputAction*> Actions = Set->GetActions();
	TArray<FString> Names;
	for (const UInputAction* Action : Actions)
	{
		Names.Add(Action ? Action->GetName() : FString());
	}
	TestEqual(TEXT("exactly the five actions"), Names,
		TArray<FString>{ TEXT("IA_Move"), TEXT("IA_Aim"), TEXT("IA_Dash"), TEXT("IA_Intercession"), TEXT("IA_Pause") });
	const TSet<const UInputAction*> Expected(Actions);
	for (const UInputMappingContext* Context : Set->GetContexts())
	{
		if (!TestNotNull(TEXT("context"), Context))
		{
			continue;
		}
		TSet<const UInputAction*> Bound;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			Bound.Add(Mapping.Action.Get());
			TestFalse(TEXT("no fire action"), Mapping.Action && Mapping.Action->GetName().Contains(TEXT("Fire")));
		}
		TestTrue(FString::Printf(TEXT("%s binds exactly the five actions"), *Context->GetName()),
			Bound.Num() == Expected.Num() && Bound.Includes(Expected));
	}
	TestEqual(TEXT("KBM context name"), Set->Kbm->GetName(), FString(TEXT("IMC_Gameplay_KBM")));
	TestEqual(TEXT("pad context name"), Set->Pad->GetName(), FString(TEXT("IMC_Gameplay_Pad")));

	// With no input at all the hero still publishes an aim direction for its weapons (they fire on their own cadence).
	FHarness H;
	H.View.EnemyPositions = { FVector2f(0.0f, 500.0f) };
	H.Steps(1);
	TestTrue(TEXT("aim published with no input"), H.View.HeroAimDirs[0].Equals(FVector2f(0.0f, 1.0f), 1e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbDash, "Dusktracer.Combat.Hero.Dash", DTHeroTest::Flags) // T-CMB-06
bool FDTCmbDash::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	const int32 DashSteps = StepsFor(DTConst::DASH_DURATION_S);
	const float Distance = static_cast<float>(DTConst::DASH_DISTANCE_UU);
	{
		FHarness H;
		H.Step(DashInput(FVector2f(0.0f, 1.0f)));
		TestTrue(TEXT("intangible while dashing"), H.View.HeroIntangible[0]);
		H.Steps(DashSteps - 1, MoveInput(FVector2f(1.0f, 0.0f))); // move input is ignored while dashing
		TestEqual(TEXT("dash covers the distance along the move input"), H.S().Pos.Y, Distance, PosTolerance);
		TestEqual(TEXT("move input ignored while dashing"), H.S().Pos.X, 0.0f);
		TestEqual(TEXT("dash over after its duration"), H.S().DashLeft, 0.0f);
		TestEqual(TEXT("one Dash event"), H.Count(EDTHeroEventKind::Dash), 1);
		TestEqual(TEXT("Dash event names its owner"), H.Events.Num() > 0 ? H.Events[0].OwnerId : INDEX_NONE, 0);
		TestTrue(TEXT("Dash event direction"), H.Events.Num() > 0 && H.Events[0].Dir.Equals(FVector2f(0.0f, 1.0f), 1e-5f));
		H.Step();
		TestFalse(TEXT("not intangible after the dash"), H.View.HeroIntangible[0]);
	}
	{
		FHarness H;
		H.Step(MoveInput(FVector2f(-1.0f, 0.0f)));
		const FVector2f Start = H.S().Pos;
		H.Step(DashInput());
		H.Steps(DashSteps - 1);
		TestEqual(TEXT("with no move input the dash follows the last move direction"), H.S().Pos.X, Start.X - Distance, PosTolerance);
		TestEqual(TEXT("and stays on that line"), H.S().Pos.Y, 0.0f);
	}
	{
		// Charges refill one per cooldown, one at a time.
		FHarness H;
		FDTHeroStats Two;
		Two.DashChargesBonus = 1;
		H.Hero.SetStats(0, Two);
		H.Step(DashInput(FVector2f(1.0f, 0.0f)));         // s0
		H.Steps(DashSteps);
		H.Step(DashInput(FVector2f(1.0f, 0.0f)));         // second charge
		TestEqual(TEXT("both charges spent"), H.S().DashCharges, 0);
		const int32 Refill = StepsFor(DTConst::DASH_COOLDOWN_S);
		H.Steps(Refill - DashSteps - 2);                  // s0 + Refill - 1
		TestEqual(TEXT("no charge before one cooldown"), H.S().DashCharges, 0);
		H.Step();                                          // s0 + Refill
		TestEqual(TEXT("one charge after one cooldown"), H.S().DashCharges, 1);
		H.Steps(Refill - 1);
		TestEqual(TEXT("still one before the second cooldown"), H.S().DashCharges, 1);
		H.Step();
		TestEqual(TEXT("two after two cooldowns"), H.S().DashCharges, 2);
		TestEqual(TEXT("one Dash event per dash"), H.Count(EDTHeroEventKind::Dash), 2);
	}
	{
		// Damage during the dash (and from the dash's last step) is discarded.
		FHarness H;
		H.Step(DashInput(FVector2f(1.0f, 0.0f)));
		for (int32 Step = 0; Step < DashSteps; ++Step)
		{
			H.View.HeroHits.Add(MakeHit(0, 10.0f));
			H.Step();
		}
		TestEqual(TEXT("no damage taken during the dash"), H.S().Hp, static_cast<float>(DTConst::HERO_MAX_HP));
		TestEqual(TEXT("no HeroDamaged during the dash"), H.Count(EDTHeroEventKind::HeroDamaged), 0);
		H.View.HeroHits.Add(MakeHit(0, 10.0f));
		H.Step();
		TestEqual(TEXT("hits after the dash land"), H.S().Hp, static_cast<float>(DTConst::HERO_MAX_HP) - 10.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbDashBudget, "Dusktracer.Combat.Hero.DashBudget", DTHeroTest::Flags) // T-CMB-07
bool FDTCmbDashBudget::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	const float Sustained = 60.0f / DTConst::DASH_COOLDOWN_S;
	TestTrue(FString::Printf(TEXT("base cooldown sustains %.1f dashes/min >= band max %d"), Sustained, DTConst::DASHES_BAND_MIN10_13_MAX),
		Sustained >= static_cast<float>(DTConst::DASHES_BAND_MIN10_13_MAX));
	// Pressing every step for a minute with base stats reaches the band's upper end.
	FHarness H;
	for (int32 Step = 0; Step < 60 * DTConst::SIM_FIXED_HZ; ++Step)
	{
		H.Step(DashInput(FVector2f(Step % 2 ? 1.0f : -1.0f, 0.0f)));
	}
	TestTrue(TEXT("a minute of pressing reaches the band's upper end"), H.Count(EDTHeroEventKind::Dash) >= DTConst::DASHES_BAND_MIN10_13_MAX);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbIntakeFromView, "Dusktracer.Combat.Hero.IntakeFromView", DTHeroTest::Flags) // T-CMB-08
bool FDTCmbIntakeFromView::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	// The real step order: Input -> Hero -> Enemies (a publisher stands in for enemy contact, rounds and hazards).
	UDTSimSubsystem* Sim = NewObject<UDTSimSubsystem>(GetTransientPackage());
	Sim->StartRun(4, 1);
	FDTHeroSim Hero;
	FDTHeroInputLatch Latch;
	FDTHeroInputPhase InputPhase;
	InputPhase.Latch = &Latch;
	InputPhase.Hero = &Hero;
	FDTHeroStepPhase StepPhase;
	StepPhase.Hero = &Hero;
	FHitPublisher Publisher;
	Sim->Register(EDTSimPhase::Input, &InputPhase);
	Sim->Register(EDTSimPhase::Hero, &StepPhase);
	Sim->Register(EDTSimPhase::Enemies, &Publisher);

	const EDTHeroHitKind Kinds[] = { EDTHeroHitKind::Contact, EDTHeroHitKind::Bullet, EDTHeroHitKind::Hazard };
	const float Amounts[] = { 7.0f, 11.0f, 13.0f };
	Sim->StepNow(1);
	float Expected = static_cast<float>(DTConst::HERO_MAX_HP);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Publisher.AtStep = Sim->GetStepIndex();
		Publisher.Hits = { MakeHit(0, Amounts[Index], Kinds[Index]) };
		Sim->StepNow(1);
		TestEqual(TEXT("published this step: not yet applied"), Hero.GetState(0).Hp, Expected);
		TestEqual(TEXT("HeroHits holds the published hit"), Sim->GetView().HeroHits.Num(), 1);
		Sim->StepNow(1);
		Expected -= Amounts[Index];
		TestEqual(FString::Printf(TEXT("kind %d deals its amount one step later"), Index), Hero.GetState(0).Hp, Expected);
		TestEqual(TEXT("HeroHits empty after the hero phase"), Sim->GetView().HeroHits.Num(), 0);
		Sim->StepNow(StepsFor(DTConst::HERO_HURT_IFRAMES_S) + 1);
	}
	TestEqual(TEXT("three hits taken"), Hero.GetState(0).HitsTaken, 3);

	// An enemy sitting on the hero publishes no hit: the hero never looks for contacts itself.
	FHarness H;
	H.View.EnemyPositions = { FVector2f::ZeroVector, FVector2f(10.0f, 0.0f) };
	H.View.EnemyRadii = { 50.0f, 50.0f };
	H.View.EnemyHash.Build(H.View.EnemyPositions);
	H.Steps(DTConst::SIM_FIXED_HZ);
	TestEqual(TEXT("no damage without a HeroHit"), H.S().Hp, static_cast<float>(DTConst::HERO_MAX_HP));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbHitWindow, "Dusktracer.Combat.Hero.HitWindow", DTHeroTest::Flags) // T-CMB-09
bool FDTCmbHitWindow::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	const float MaxHp = static_cast<float>(DTConst::HERO_MAX_HP);
	const int32 Window = StepsFor(DTConst::HERO_HURT_IFRAMES_S);
	{
		FHarness H;
		H.View.HeroHits = { MakeHit(0, 10.0f), MakeHit(0, 25.0f), MakeHit(0, 15.0f), MakeHit(0, 20.0f, EDTHeroHitKind::Bullet) };
		H.Step();
		TestEqual(TEXT("only the largest applies"), H.S().Hp, MaxHp - 25.0f);
		TestEqual(TEXT("one HeroDamaged"), H.Count(EDTHeroEventKind::HeroDamaged), 1);
		for (int32 Step = 1; Step < Window; ++Step)
		{
			H.View.HeroHits.Add(MakeHit(0, 5.0f));
			H.Step();
		}
		TestEqual(TEXT("nothing applies during hurt i-frames"), H.S().Hp, MaxHp - 25.0f);
		H.View.HeroHits.Add(MakeHit(0, 5.0f));
		H.Step();
		TestEqual(TEXT("applies again after C.HERO.HURT_IFRAMES_S"), H.S().Hp, MaxHp - 30.0f);
		TestEqual(TEXT("one HeroDamaged per applied hit"), H.Count(EDTHeroEventKind::HeroDamaged), 2);
	}
	{
		// Ties: contact before bullet before hazard, then the lower serial.
		FHarness H;
		H.View.HeroHits = { MakeHit(0, 9.0f, EDTHeroHitKind::Hazard, 1), MakeHit(0, 9.0f, EDTHeroHitKind::Bullet, 1),
			MakeHit(0, 9.0f, EDTHeroHitKind::Contact, 8, EDTEnemyRole::Tank), MakeHit(0, 9.0f, EDTHeroHitKind::Contact, 3, EDTEnemyRole::Rusher) };
		H.Step();
		TestTrue(TEXT("tie goes to contact"), H.Events.Num() == 1 && H.Events[0].HitKind == EDTHeroHitKind::Contact);
		TestTrue(TEXT("then to the lower serial"), H.Events.Num() == 1 && H.Events[0].SourceRole == EDTEnemyRole::Rusher);
	}
	{
		// A pile of enemies: at most one hit per window.
		FHarness H;
		const int32 Steps = 5 * Window;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			for (int32 Enemy = 0; Enemy < 50; ++Enemy)
			{
				H.View.HeroHits.Add(MakeHit(0, 1.0f, EDTHeroHitKind::Contact, Enemy + 1));
			}
			H.Step();
		}
		TestEqual(TEXT("one hit per window"), H.Count(EDTHeroEventKind::HeroDamaged), 5);
		TestEqual(TEXT("each hit is one contact"), H.S().Hp, MaxHp - 5.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbDeath, "Dusktracer.Combat.Hero.Death", DTHeroTest::Flags) // T-CMB-10
bool FDTCmbDeath::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	FHarness H(2);
	const float Lethal = static_cast<float>(DTConst::HERO_MAX_HP) * 2.0f;
	H.View.HeroHits = { MakeHit(0, Lethal) };
	H.Step();
	TestTrue(TEXT("lethal damage downs the hero"), H.S(0).bDown);
	TestEqual(TEXT("HeroDown once"), H.Count(EDTHeroEventKind::HeroDown, 0), 1);
	TestFalse(TEXT("one of two players down keeps the run going"), H.Hero.IsRunOver());

	const FVector2f DownAt = H.S(0).Pos;
	for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ; ++Step)
	{
		H.View.HeroHits = { MakeHit(0, Lethal) };
		FDTHeroInput In = DashInput(FVector2f(1.0f, 0.0f));
		In.bIntercessionPress = true;
		H.Step(In, 0);
	}
	TestEqual(TEXT("still HeroDown once"), H.Count(EDTHeroEventKind::HeroDown, 0), 1);
	TestEqual(TEXT("no more damage while down"), H.Count(EDTHeroEventKind::HeroDamaged, 0), 1);
	TestTrue(TEXT("a downed hero ignores move input"), H.S(0).Pos == DownAt);
	TestEqual(TEXT("a downed hero does not dash"), H.Count(EDTHeroEventKind::Dash, 0), 0);
	TestFalse(TEXT("still not over"), H.Hero.IsRunOver());

	H.View.HeroHits = { MakeHit(1, Lethal) };
	const int64 EndStep = H.StepIndex;
	H.Step(FDTHeroInput(), 1);
	TestTrue(TEXT("both down ends the run"), H.Hero.IsRunOver());
	TestEqual(TEXT("on the step the last hero went down"), H.Hero.GetRunEndStep(), EndStep);
	TestEqual(TEXT("player 1 HeroDown once"), H.Count(EDTHeroEventKind::HeroDown, 1), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbIntercession, "Dusktracer.Combat.Hero.Intercession", DTHeroTest::Flags) // T-CMB-11
bool FDTCmbIntercession::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	const int32 Charge = StepsFor(DTConst::HERO_INTERCESSION_CHARGE_S);
	{
		FHarness H;
		H.Run.GetPlayer(0).Keepers.Primary = EDTPath::Storm;
		TestEqual(TEXT("meter starts empty"), H.S().Intercession01, 0.0f);
		H.Step(IntercessionInput());
		TestEqual(TEXT("a press when not full is denied"), H.Count(EDTHeroEventKind::IntercessionDenied), 1);
		TestEqual(TEXT("and does not fire"), H.Count(EDTHeroEventKind::Intercession), 0);
		H.Steps(Charge - 2);
		TestTrue(TEXT("not full one step early"), H.S().Intercession01 < 1.0f);
		H.Step();
		TestEqual(TEXT("full after C.HERO.INTERCESSION_CHARGE_S"), H.S().Intercession01, 1.0f);
		H.Step(IntercessionInput());
		TestEqual(TEXT("a press when full fires once"), H.Count(EDTHeroEventKind::Intercession), 1);
		TestEqual(TEXT("and empties the meter"), H.S().Intercession01, 0.0f);
		TestTrue(TEXT("the event carries the Patron Path"), H.Events.Last().Path == EDTPath::Storm && H.Events.Last().OwnerId == 0);
		TestEqual(TEXT("only the one denial"), H.Count(EDTHeroEventKind::IntercessionDenied), 1);
	}
	{
		// A press just before the meter fills is buffered, not denied (R-CMB-02).
		FHarness H;
		H.Steps(Charge - 3);
		H.Step(IntercessionInput());
		H.Steps(3);
		TestEqual(TEXT("buffered press fires when the meter fills"), H.Count(EDTHeroEventKind::Intercession), 1);
		TestEqual(TEXT("without a denial"), H.Count(EDTHeroEventKind::IntercessionDenied), 0);
	}
	{
		// The meter is sim time: no step, no fill; the sim clock does not tick while the game is paused.
		FHarness H;
		H.Steps(10);
		const float Before = H.S().Intercession01;
		TestEqual(TEXT("no step, no fill"), H.S().Intercession01, Before);
		const UDTSimSubsystem* Sim = GetDefault<UDTSimSubsystem>();
		TestFalse(TEXT("the sim does not tick while paused"), Sim->IsTickableWhenPaused());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCmbDeterminism, "Dusktracer.Combat.Hero.Determinism", DTHeroTest::Flags) // T-CMB-12
bool FDTCmbDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTHeroTest;
	// A recorded sequence: inputs, the enemy view and hits published per step.
	struct FFrame
	{
		FDTHeroInput Input;
		TArray<FDTHeroHit> Hits;
		TArray<FVector2f> Enemies;
	};
	TArray<FFrame> Record;
	FRandomStream Rng(42);
	for (int32 Step = 0; Step < 20 * DTConst::SIM_FIXED_HZ; ++Step)
	{
		FFrame Frame;
		Frame.Input.Move = FVector2f(Rng.FRandRange(-1.2f, 1.2f), Rng.FRandRange(-1.2f, 1.2f));
		Frame.Input.AimStick = FVector2f(Rng.FRandRange(-1.0f, 1.0f), Rng.FRandRange(-1.0f, 1.0f));
		Frame.Input.bDashPress = Rng.FRand() < 0.05f;
		Frame.Input.bIntercessionPress = Rng.FRand() < 0.01f;
		if (Rng.FRand() < 0.1f)
		{
			Frame.Hits.Add(MakeHit(0, Rng.FRandRange(1.0f, 4.0f), EDTHeroHitKind::Contact, Rng.RandRange(1, 99)));
		}
		for (int32 Enemy = 0; Enemy < 8; ++Enemy)
		{
			Frame.Enemies.Add(FVector2f(Rng.FRandRange(-2000.0f, 2000.0f), Rng.FRandRange(-2000.0f, 2000.0f)));
		}
		Record.Add(MoveTemp(Frame));
	}

	auto Replay = [&Record](TArray<FDTHeroState>& OutStates)
	{
		FHarness H;
		for (const FFrame& Frame : Record)
		{
			H.View.HeroHits = Frame.Hits;
			H.View.EnemyPositions = Frame.Enemies;
			H.View.EnemyHash.Build(H.View.EnemyPositions);
			H.Step(Frame.Input);
			OutStates.Add(H.S());
		}
	};
	TArray<FDTHeroState> A;
	TArray<FDTHeroState> B;
	Replay(A);
	Replay(B);
	bool bSame = A.Num() == B.Num();
	for (int32 Index = 0; bSame && Index < A.Num(); ++Index)
	{
		bSame = SameState(A[Index], B[Index]);
	}
	TestTrue(TEXT("a replay is bit-identical"), bSame);
	TestTrue(TEXT("the sequence exercised dashes and hits"), A.Num() > 0 && A.Last().Dashes > 0 && A.Last().HitsTaken > 0);

	// The bot path: the same inputs latched each step and stepped by the real sim clock.
	UDTSimSubsystem* Sim = NewObject<UDTSimSubsystem>(GetTransientPackage());
	Sim->StartRun(1, 1);
	FDTHeroSim Hero;
	FDTHeroInputLatch Latch;
	FDTHeroInputPhase InputPhase;
	InputPhase.Latch = &Latch;
	InputPhase.Hero = &Hero;
	FDTHeroStepPhase StepPhase;
	StepPhase.Hero = &Hero;
	Sim->Register(EDTSimPhase::Input, &InputPhase);
	Sim->Register(EDTSimPhase::Hero, &StepPhase);
	bool bBotSame = true;
	for (int32 Index = 0; Index < Record.Num(); ++Index)
	{
		FDTWorldView& View = Sim->GetView();
		View.HeroHits = Record[Index].Hits;
		View.EnemyPositions = Record[Index].Enemies;
		View.EnemyHash.Build(View.EnemyPositions);
		Latch.Latch(0, Record[Index].Input);
		Sim->StepNow(1);
		bBotSame &= SameState(Hero.GetState(0), A[Index]);
	}
	TestTrue(TEXT("the latched (bot) path gives the same states as the harness"), bBotSame);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
