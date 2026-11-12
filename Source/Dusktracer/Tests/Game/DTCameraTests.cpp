// Camera tests (06_UX.md §1, T-UX-01 and T-UX-02; decided in D-036, the raised frame in D-080).
// T-UX-01 is pure: the frame's reach on the drawn plane from ADTCameraPawn::ComputeFramingDistance, the constants and the
// HUD's own layout rows on the hero's column (UDTBossSubsystem::BossRowRect, FDTXPBar::BlockSpan), read as inputs.
// T-UX-02 follows a hero through a run in a headless world: run start, a Threat beat, a held pause and the boss fight.
// Run headless: & Tools\RunTests.ps1 -Filter Dusktracer.UX

#include "Misc/AutomationTest.h"
#include "Boss/DTBossSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraTypes.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DTCameraPawn.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "Player/DTHero.h"
#include "Player/DTHeroSubsystem.h"
#include "Player/DTInputSet.h"
#include "Progression/DTXPBar.h"
#include "SceneView.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTCameraTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	// uu; the reach sums are a few thousand uu in doubles.
	constexpr double ReachTolerance = 0.01;
	// uu; R-UX-01's equal reach up and down holds within about one pixel at the top of a 1080-row screen (3.65 uu at this frame).
	constexpr double EqualTolerance = 4.0;
	// uu of frame radius; the frame is the tightest to this step.
	constexpr double RadiusStep = 1.0;
	// uu and degrees; an attached actor's world transform is its parent's times a constant.
	constexpr double OffsetTolerance = 0.01;

	/** The reach from the hero on a plane PlaneZ above the floor, up the screen to row y = TopEdge and down to row y = BottomEdge
	 *  of a ViewH-row screen, on the hero's column, for a camera Distance from its aim point, which is Shift from the hero toward
	 *  the camera (06_UX R-UX-01, D-080). Rows are pixel edges: y 0 is the screen's top. */
	struct FPlaneReach
	{
		double Up = 0.0;
		double Down = 0.0;
	};
	static FPlaneReach PlaneReachOf(double Distance, double Shift, double PitchDeg, double TanHalfVertical, double TopEdge, double BottomEdge,
		double ViewH, double PlaneZ)
	{
		const double Pitch = FMath::DegreesToRadians(PitchDeg);
		const double CameraX = -Distance * FMath::Cos(Pitch) - Shift;
		const double Above = Distance * FMath::Sin(Pitch) - PlaneZ;
		// Where the ray through a row of the centre column meets the plane, along the camera's heading from the hero.
		const auto Land = [&](double Row)
		{
			const double Ndc = 1.0 - 2.0 * Row / ViewH;
			const double Below = Pitch - FMath::Atan(Ndc * TanHalfVertical);
			return Below > 0.0 ? CameraX + Above / FMath::Tan(Below) : TNumericLimits<double>::Max();
		};
		FPlaneReach Out;
		Out.Up = Land(TopEdge);
		Out.Down = -Land(BottomEdge);
		return Out;
	}

	/** R-UX-01: the farthest point an enemy attack can start, plus a Tank's diameter. */
	static double ReachNeeded()
	{
		const int32 Farthest = FMath::Max3(DTConst::ROLE_TURRET_FIRE_RANGE_UU + DTConst::ROLE_TURRET_RADIUS_UU,
			DTConst::DIR_THREAT_RING_MAX_UU, DTConst::BOSS_HAZARD_MIN_RADIUS_UU);
		return static_cast<double>(Farthest + 2 * DTConst::ROLE_TANK_RADIUS_UU);
	}

	/** The lens of a camera component as its GetCameraView hands it on, without a world (the class defaults have none). */
	static FMinimalViewInfo LensOf(const UCameraComponent* Camera)
	{
		FMinimalViewInfo View;
		View.FOV = Camera->FieldOfView;
		View.AspectRatio = Camera->AspectRatio;
		View.bConstrainAspectRatio = Camera->bConstrainAspectRatio;
		if (Camera->bOverrideAspectRatioAxisConstraint)
		{
			View.AspectRatioAxisConstraint = Camera->AspectRatioAxisConstraint;
		}
		return View;
	}

	/** Half field of view tangents (X vertical, Y horizontal) a view gives a window of this size. */
	static FVector2D HalfFovTangents(FMinimalViewInfo View, int32 Width, int32 Height)
	{
		FSceneViewProjectionData Projection;
		Projection.SetViewRectangle(FIntRect(0, 0, Width, Height));
		// The local player's default constraint; the camera's own, when it overrides, wins inside.
		FMinimalViewInfo::CalculateProjectionMatrixGivenViewRectangle(View, AspectRatio_MaintainXFOV, Projection.GetViewRect(), Projection);
		return FVector2D(1.0 / Projection.ProjectionMatrix.M[1][1], 1.0 / Projection.ProjectionMatrix.M[0][0]);
	}

	/** A headless game world with every subsystem up and begun. */
	struct FTestWorld
	{
		UWorld* World = nullptr;

		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTCameraTestWorld")));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTUxCameraReach, "Dusktracer.UX.CameraReach", DTCameraTest::Flags) // T-UX-01
bool FDTUxCameraReach::RunTest(const FString& Parameters)
{
	using namespace DTCameraTest;
	const double Radius = static_cast<double>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU);
	const double Fov = static_cast<double>(DTConst::ARENA_CAMERA_FOV_DEG);
	const double Pitch = static_cast<double>(DTConst::ARENA_CAMERA_PITCH_DEG);
	const double Aspect = static_cast<double>(ADTCameraPawn::TargetAspect);
	const double Distance = static_cast<double>(ADTCameraPawn::ComputeFramingDistance(static_cast<float>(Radius), static_cast<float>(Fov),
		ADTCameraPawn::TargetAspect));
	const double TanHorizontal = FMath::Tan(FMath::DegreesToRadians(Fov * 0.5));
	const double TanVertical = TanHorizontal / Aspect;

	// The frame radius is half the frame height at the hero, across the view axis.
	TestEqual(TEXT("the frame radius fills half the frame height"), Distance * TanVertical, Radius, 0.5);

	// The HUD's rows on the hero's column at the 1920x1080 target, read from the HUD's own layout values (inputs, not numbers
	// of this test): the boss row at the top centre in the fight; the XP block stands in the top-right corner, off the column.
	const double ViewW = 1920.0;
	const double ViewH = 1080.0;
	const double HeroColumn = 0.5 * ViewW; // the hero stands on the screen's vertical centre line
	const FBox2f BossRow = UDTBossSubsystem::BossRowRect(static_cast<float>(ViewW));
	const FBox2f XpBlock = FDTXPBar::BlockSpan(static_cast<float>(ViewW));
	TestTrue(FString::Printf(TEXT("the XP block (x %.0f to %.0f) is off the hero's column"), XpBlock.Min.X, XpBlock.Max.X),
		XpBlock.Min.X > HeroColumn || XpBlock.Max.X < HeroColumn);
	TestTrue(TEXT("the boss row crosses the hero's column"), BossRow.Min.X <= HeroColumn && BossRow.Max.X >= HeroColumn);
	const double TopEdge = static_cast<double>(BossRow.Max.Y);
	const double BottomEdge = ViewH; // nothing of the HUD crosses the column at the bottom
	const double PlaneZ = static_cast<double>(DTConst::SIM_PLANE_Z_UU);
	const double Shift = static_cast<double>(DTConst::ARENA_CAMERA_AIM_SHIFT_UU);
	const double Needed = ReachNeeded();
	const auto ReachAt = [&](double FrameRadius, double AimShift)
	{
		const double D = static_cast<double>(ADTCameraPawn::ComputeFramingDistance(static_cast<float>(FrameRadius), static_cast<float>(Fov), ADTCameraPawn::TargetAspect));
		return PlaneReachOf(D, AimShift, Pitch, TanVertical, TopEdge, BottomEdge, ViewH, PlaneZ);
	};
	// R-UX-01: equal up and down within a pixel at the top, and each at least the farthest attack start plus a Tank's diameter.
	const auto Meets = [&](double FrameRadius, double AimShift)
	{
		const FPlaneReach R = ReachAt(FrameRadius, AimShift);
		return FMath::Abs(R.Up - R.Down) <= EqualTolerance && R.Up >= Needed && R.Down >= Needed;
	};
	const FPlaneReach Reach = ReachAt(Radius, Shift);
	AddInfo(FString::Printf(TEXT("drawn plane: reach up %.2f to row %.1f, down %.2f to row %.1f uu; needed %.1f uu"), Reach.Up, TopEdge, Reach.Down, BottomEdge, Needed));
	TestTrue(FString::Printf(TEXT("the reach up (%.2f) equals the reach down (%.2f) within %.1f uu"), Reach.Up, Reach.Down, EqualTolerance),
		FMath::Abs(Reach.Up - Reach.Down) <= EqualTolerance);
	TestTrue(FString::Printf(TEXT("the reach up %.2f uu covers the farthest attack start plus a Tank's diameter, %.1f uu"), Reach.Up, Needed), Reach.Up >= Needed);
	TestTrue(FString::Printf(TEXT("the reach down %.2f uu covers it"), Reach.Down), Reach.Down >= Needed);
	// The tightest: a frame one step smaller, aimed for an equal reach (reach up falls and reach down rises one for one with the
	// shift), misses the line.
	const FPlaneReach Centred = ReachAt(Radius - RadiusStep, 0.0);
	const double BestTighter = 0.5 * (Centred.Up + Centred.Down);
	TestTrue(FString::Printf(TEXT("a frame %.1f uu tighter reaches at most %.2f uu, under the line"), RadiusStep, BestTighter), BestTighter < Needed);
	// The check has teeth: D-036's centred frame is not equal; the study's frame solved without the HUD and its frame with no body
	// margin (D-080 Context) fall short of the line.
	TestFalse(TEXT("D-036's centred frame (1500, no shift) fails the rule"), Meets(1500.0, 0.0));
	TestFalse(TEXT("the frame solved without the HUD (1213.3, 185.6) fails it"), Meets(1213.3, 185.6));
	TestFalse(TEXT("the frame with no body margin (1115, 168) fails it"), Meets(1115.0, 168.0));
	// The side at the hero's line is wider still.
	TestTrue(TEXT("the side reaches further than the line"), (Distance + Shift * FMath::Cos(FMath::DegreesToRadians(Pitch))) * TanHorizontal >= Needed);

	// The camera in the game is that camera: aimed at a floor point Shift from the hero toward the camera, at the framing
	// distance from it, at the pitch, its lens the field of view.
	const FTransform Follow = ADTCameraPawn::FollowTransform();
	const FVector Aim(-Shift, 0.0, 0.0);
	TestEqual(TEXT("the camera is the framing distance from its aim point"), (Follow.GetLocation() - Aim).Size(), Distance, ReachTolerance);
	TestEqual(TEXT("the follow offset is above at the pitch"), Follow.GetLocation().Z, Distance * FMath::Sin(FMath::DegreesToRadians(Pitch)), ReachTolerance);
	TestTrue(TEXT("the camera sits down-screen of the hero"), Follow.GetLocation().X < 0.0 && FMath::IsNearlyZero(Follow.GetLocation().Y));
	TestEqual(TEXT("the camera looks down at the pitch"), Follow.Rotator().Pitch, -Pitch, 1e-3);
	TestEqual(TEXT("no yaw"), Follow.Rotator().Yaw, 0.0, 1e-3);
	TestTrue(TEXT("the camera looks at its aim point"), Follow.GetRotation().GetForwardVector().Equals((Aim - Follow.GetLocation()).GetSafeNormal(), 1e-4));

	UCameraComponent* Lens = GetDefault<ADTCameraPawn>()->GetCamera();
	if (TestNotNull(TEXT("camera component"), Lens))
	{
		TestEqual(TEXT("the lens is the horizontal field of view"), static_cast<double>(Lens->FieldOfView), Fov, 1e-4);
		TestFalse(TEXT("no letterbox"), static_cast<bool>(Lens->bConstrainAspectRatio));
		// The target window, a wider one and a narrower one: the height in view never falls, the sides follow the window.
		const FVector2D Target = HalfFovTangents(LensOf(Lens), 1920, 1080);
		const FVector2D Wide = HalfFovTangents(LensOf(Lens), 2560, 1080);
		const FVector2D Narrow = HalfFovTangents(LensOf(Lens), 1440, 1080);
		TestEqual(TEXT("vertical field of view at the target aspect"), Target.X, TanVertical, 1e-4);
		TestEqual(TEXT("horizontal field of view at the target aspect"), Target.Y, TanHorizontal, 1e-4);
		TestEqual(TEXT("a wider window keeps the height"), Wide.X, TanVertical, 1e-4);
		TestTrue(TEXT("a wider window shows more ground at the sides"), Wide.Y > Target.Y);
		TestEqual(TEXT("a narrower window keeps the height"), Narrow.X, TanVertical, 1e-4);
		TestTrue(TEXT("the reach holds in a wider window"), PlaneReachOf(Distance, Shift, Pitch, Wide.X, TopEdge, BottomEdge, ViewH, PlaneZ).Down >= Needed);
		TestTrue(TEXT("the reach holds in a narrower window"), PlaneReachOf(Distance, Shift, Pitch, Narrow.X, TopEdge, BottomEdge, ViewH, PlaneZ).Down >= Needed);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTUxCameraFixed, "Dusktracer.UX.CameraFixed", DTCameraTest::Flags) // T-UX-02
bool FDTUxCameraFixed::RunTest(const FString& Parameters)
{
	using namespace DTCameraTest;

	// No zoom input: the pawn cannot tick, takes no input and has no arm, and the game's input set has nothing to zoom with.
	const ADTCameraPawn* Default = GetDefault<ADTCameraPawn>();
	TestFalse(TEXT("the camera pawn never ticks"), Default->PrimaryActorTick.bCanEverTick);
	TestTrue(TEXT("no player possesses it"), Default->AutoPossessPlayer == EAutoReceiveInput::Disabled);
	TestTrue(TEXT("it receives no input"), Default->AutoReceiveInput == EAutoReceiveInput::Disabled);
	TestTrue(TEXT("the camera is the root"), Default->GetRootComponent() == Default->GetCamera());
	UDTInputSet* Set = UDTInputSet::Create(GetTransientPackage());
	for (const UInputAction* Action : Set->GetActions())
	{
		if (TestNotNull(TEXT("action"), Action))
		{
			TestFalse(FString::Printf(TEXT("%s is no zoom or camera action"), *Action->GetName()),
				Action->GetName().Contains(TEXT("Zoom")) || Action->GetName().Contains(TEXT("Camera")));
		}
	}
	for (const UInputMappingContext* Context : Set->GetContexts())
	{
		if (!TestNotNull(TEXT("context"), Context))
		{
			continue;
		}
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			TestFalse(FString::Printf(TEXT("%s binds no mouse wheel"), *Context->GetName()),
				Mapping.Key == EKeys::MouseWheelAxis || Mapping.Key == EKeys::MouseScrollUp || Mapping.Key == EKeys::MouseScrollDown);
		}
	}

	if (!UDTDirector::IsEnabledByCVar())
	{
		AddWarning(TEXT("dt.Director.Enabled is 0 in this process; the run part of CameraFixed is skipped"));
		return true;
	}
	FTestWorld W;
	UDTSimSubsystem* Sim = W.World->GetSubsystem<UDTSimSubsystem>();
	UDTHeroSubsystem* Heroes = W.World->GetSubsystem<UDTHeroSubsystem>();
	UDTDirector* Director = W.World->GetSubsystem<UDTDirector>();
	UDTBossSubsystem* Boss = W.World->GetSubsystem<UDTBossSubsystem>();
	if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("heroes"), Heroes) || !TestNotNull(TEXT("director"), Director) || !TestNotNull(TEXT("boss"), Boss))
	{
		return false;
	}
	// The run as ADTGameMode starts it: sim, director, the hero's pawn and the camera following it.
	Sim->StartRun(9107, 1);
	Director->StartRun();
	Heroes->SetNoDamage(true); // the hero must live to the boss; damage is not what is read here
	ADTHero* Hero = W.World->SpawnActor<ADTHero>();
	ADTCameraPawn* Camera = W.World->SpawnActor<ADTCameraPawn>(ADTCameraPawn::StaticClass(), ADTCameraPawn::FollowTransform());
	if (!TestNotNull(TEXT("hero pawn"), Hero) || !TestNotNull(TEXT("camera pawn"), Camera))
	{
		return false;
	}
	Heroes->RegisterPawn(Hero);
	Camera->Follow(Hero);
	TestNull(TEXT("the spawned camera has no input component"), Camera->InputComponent.Get());
	TArray<USpringArmComponent*> Arms;
	Camera->GetComponents(Arms);
	TestEqual(TEXT("no spring arm"), Arms.Num(), 0);
	TArray<UCameraComponent*> Lenses;
	Camera->GetComponents(Lenses);
	TestEqual(TEXT("one camera component"), Lenses.Num(), 1);
	// The view the engine draws from this camera keeps its height in a wide window (R-UX-01), as the class defaults say.
	FMinimalViewInfo Drawn;
	Camera->GetCamera()->GetCameraView(0.0f, Drawn);
	const double TanVertical = FMath::Tan(FMath::DegreesToRadians(DTConst::ARENA_CAMERA_FOV_DEG * 0.5)) / ADTCameraPawn::TargetAspect;
	TestEqual(TEXT("the drawn view keeps the height at the target aspect"), HalfFovTangents(Drawn, 1920, 1080).X, TanVertical, 1e-4);
	TestEqual(TEXT("the drawn view keeps the height in a wide window"), HalfFovTangents(Drawn, 2560, 1080).X, TanVertical, 1e-4);

	const FTransform Follow = ADTCameraPawn::FollowTransform();
	const float Fov = static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG);
	TArray<FVector> HeroAt;
	// Steps the run with the hero walking along Move, draws it, and reads the view against the one frame.
	auto Read = [&](const TCHAR* When, const FVector2f& Move, int32 Steps)
	{
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			FDTHeroInput In;
			In.Move = Move;
			Heroes->LatchInput(0, In);
			Sim->StepNow(1);
		}
		Heroes->SimRender(1.0f);
		const FVector Offset = Camera->GetActorLocation() - Hero->GetActorLocation();
		TestTrue(FString::Printf(TEXT("%s: the offset from the hero is the follow offset (%s)"), When, *Offset.ToString()),
			Offset.Equals(Follow.GetLocation(), OffsetTolerance));
		TestTrue(FString::Printf(TEXT("%s: the view direction is the follow rotation"), When),
			Camera->GetActorRotation().Equals(Follow.Rotator(), OffsetTolerance));
		TestEqual(FString::Printf(TEXT("%s: the field of view"), When), Camera->GetCamera()->FieldOfView, Fov, 1e-4f);
		TestTrue(FString::Printf(TEXT("%s: the camera's scale is one"), When), Camera->GetActorScale3D().Equals(FVector::OneVector, 1e-6));
		HeroAt.Add(Hero->GetActorLocation());
	};

	Read(TEXT("run start"), FVector2f::ZeroVector, 1);
	TestTrue(TEXT("the run starts in Harvest"), Director->GetCore().GetBlock() == EDTDirBlock::Harvest);

	Read(TEXT("walking"), FVector2f(1.0f, 0.0f), DTConst::SIM_FIXED_HZ);
	TestTrue(TEXT("the hero moved, and the camera with it"), FVector::Dist(HeroAt[0], HeroAt[1]) > 100.0);

	Director->GetCore().ForceBlock(EDTDirBlock::Threat);
	Read(TEXT("Threat beat"), FVector2f(0.0f, -1.0f), DTConst::SIM_FIXED_HZ);
	TestTrue(TEXT("the director is in a Threat beat"), Director->GetCore().GetBlock() == EDTDirBlock::Threat);
	TestTrue(TEXT("the hero moved in the beat"), FVector::Dist(HeroAt[1], HeroAt[2]) > 100.0);

	// A held pause (an open hand in solo): frames pass, no step runs, the frame stays.
	Sim->RequestPause();
	TestEqual(TEXT("no step while paused"), Sim->AdvanceFrame(0.5f), 0);
	Read(TEXT("pause"), FVector2f::ZeroVector, 0);
	Sim->ClearPauseRequest();

	Director->GetCore().Skip(static_cast<float>(DTConst::RUN_LENGTH_TO_BOSS));
	Read(TEXT("boss entry"), FVector2f(-1.0f, 0.0f), DTConst::SIM_FIXED_HZ);
	TestTrue(TEXT("the director is in its Boss block"), Director->GetCore().GetBlock() == EDTDirBlock::Boss);
	TestTrue(TEXT("the Warden is in the fight"), Boss->GetCore().HasHull());
	Read(TEXT("boss fight"), FVector2f(0.0f, 1.0f), 5 * DTConst::SIM_FIXED_HZ);
	TestTrue(TEXT("the hero moved in the fight"), FVector::Dist(HeroAt[4], HeroAt[5]) > 100.0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
