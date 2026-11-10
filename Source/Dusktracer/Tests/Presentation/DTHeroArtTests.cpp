// Hero art tests (02_Systems/hero_art.md): T-HAR-02 and T-HAR-04 (the parts as drawn), T-HAR-05 (the rigid pose), T-HAR-08 (reads, never writes).
// The silhouette itself is read without an engine by Tools/hero_outline.py (T-HAR-03, T-HAR-06).
// Run headless: & Tools\RunTests.ps1 -Filter Dusktracer.HeroArt

#include "Misc/AutomationTest.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Player/DTHero.h"
#include "Player/DTHeroSubsystem.h"
#include "Presentation/DTHeroFigure.h"
#include "Presentation/DTHeroPose.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTHeroArtTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	// uu and unit lengths; a pose is a handful of float products.
	constexpr double Tolerance = 0.01;
	// The turns a facing or a direction is tried at: every eighth of a circle.
	constexpr int32 NumDirs = 8;
	constexpr float LeftSide = -1.0f;
	constexpr float RightSide = 1.0f;

	static FVector2f Dir(int32 Index)
	{
		const float Radians = UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(NumDirs);
		return FVector2f(FMath::Cos(Radians), FMath::Sin(Radians));
	}

	static FVector Flat(const FVector2f& V)
	{
		return FVector(static_cast<double>(V.X), static_cast<double>(V.Y), 0.0);
	}

	static const FTransform& Part(const FDTHeroPose& Pose, EDTHeroPart Which)
	{
		return Pose.Parts[static_cast<int32>(Which)];
	}

	/** Where a point of the figure's own space is drawn, relative to the hero's point on the floor. */
	static FVector Drawn(const FDTHeroPose& Pose, EDTHeroPart Which, const FVector& InFigure)
	{
		return Part(Pose, Which).TransformPosition(InFigure);
	}

	static double Scale()
	{
		return static_cast<double>(DTConst::HERO_ART_DRAW_SCALE);
	}

	/** The crown of the hood and the soles, in the figure's own space: the points whose travel shows a lean and a stride. */
	static FVector Crown()
	{
		return DTHeroPose::Neck() + FVector(0.0, 0.0, static_cast<double>(DTConst::HERO_ART_NECK_Z_UU - DTConst::HERO_ART_SHOULDER_Z_UU));
	}

	static FVector Sole(float Side)
	{
		const FVector Hip = DTHeroPose::Hip(Side);
		return FVector(Hip.X, Hip.Y, 0.0);
	}

	static FVector Fist(float Side)
	{
		const FVector Shoulder = DTHeroPose::Shoulder(Side);
		return FVector(Shoulder.X, Shoulder.Y, static_cast<double>(DTConst::HERO_ART_WAIST_Z_UU));
	}
}

// T-HAR-05: the pose is a pure function of facing, velocity and dash; parts stay rigid and turn about their joints.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHeroArtPose, "Dusktracer.HeroArt.Pose", DTHeroArtTest::Flags)
bool FDTHeroArtPose::RunTest(const FString& Parameters)
{
	using namespace DTHeroArtTest;
	const float FullSpeed = static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S);

	// Idle: every part is the figure turned to its facing at the draw scale, and nothing else.
	for (int32 Facing = 0; Facing < NumDirs; ++Facing)
	{
		FDTHeroPoseInput Idle;
		Idle.AimDir = Dir(Facing);
		const FDTHeroPose Pose = DTHeroPose::Evaluate(Idle);
		const FVector Ahead = Flat(Idle.AimDir);
		for (int32 Index = 0; Index < DTHeroPose::NumParts; ++Index)
		{
			const FTransform& T = Pose.Parts[Index];
			TestTrue(TEXT("idle: a part is not moved off the hero's point"), T.GetLocation().IsNearlyZero(Tolerance));
			TestTrue(TEXT("idle: the figure's forward is the aim direction"), T.TransformVectorNoScale(FVector::ForwardVector).Equals(Ahead, Tolerance));
			TestTrue(TEXT("idle: the figure stands upright"), T.TransformVectorNoScale(FVector::UpVector).Equals(FVector::UpVector, Tolerance));
			TestTrue(TEXT("idle: one uniform draw scale"), T.GetScale3D().Equals(FVector(Scale()), Tolerance));
		}
	}

	// No aim direction keeps +X; a zero velocity is idle.
	{
		FDTHeroPoseInput Blank;
		Blank.AimDir = FVector2f::ZeroVector;
		const FDTHeroPose Pose = DTHeroPose::Evaluate(Blank);
		TestTrue(TEXT("no aim direction faces +X"), Part(Pose, EDTHeroPart::Hood).TransformVectorNoScale(FVector::ForwardVector).Equals(FVector::ForwardVector, Tolerance));
	}

	// Moving and dashing, at every facing and every direction of travel.
	for (int32 Facing = 0; Facing < NumDirs; ++Facing)
	{
		for (int32 Travel = 0; Travel < NumDirs; ++Travel)
		{
			FDTHeroPoseInput Idle;
			Idle.AimDir = Dir(Facing);
			FDTHeroPoseInput Moving = Idle;
			Moving.Vel = Dir(Travel) * FullSpeed;
			FDTHeroPoseInput Dashing = Idle;
			Dashing.bDashing = true;
			Dashing.DashDir = Dir(Travel);
			const FVector Along = Flat(Dir(Travel));
			const FDTHeroPose Still = DTHeroPose::Evaluate(Idle);
			const FDTHeroPose Walk = DTHeroPose::Evaluate(Moving);
			const FDTHeroPose Dash = DTHeroPose::Evaluate(Dashing);

			// Pure: the same input gives the same pose.
			const FDTHeroPose Again = DTHeroPose::Evaluate(Moving);
			for (int32 Index = 0; Index < DTHeroPose::NumParts; ++Index)
			{
				TestTrue(TEXT("the same input gives the same pose"), Again.Parts[Index].Equals(Walk.Parts[Index], 0.0));
				// Rigid: a part is turned and moved, never stretched.
				TestTrue(TEXT("moving keeps every part's shape"), Walk.Parts[Index].GetScale3D().Equals(FVector(Scale()), Tolerance));
				TestTrue(TEXT("dashing keeps every part's shape"), Dash.Parts[Index].GetScale3D().Equals(FVector(Scale()), Tolerance));
			}

			// Joints hold: what turns about a joint leaves the joint where its parent put it.
			TestTrue(TEXT("moving: the torso turns about the waist"), Drawn(Walk, EDTHeroPart::Torso, DTHeroPose::Waist()).Equals(Drawn(Still, EDTHeroPart::Torso, DTHeroPose::Waist()), Tolerance));
			TestTrue(TEXT("moving: the hood turns about the neck of the torso"), Drawn(Walk, EDTHeroPart::Hood, DTHeroPose::Neck()).Equals(Drawn(Walk, EDTHeroPart::Torso, DTHeroPose::Neck()), Tolerance));
			for (const float Side : { LeftSide, RightSide })
			{
				const EDTHeroPart Leg = Side < 0.0f ? EDTHeroPart::LegL : EDTHeroPart::LegR;
				const EDTHeroPart Arm = Side < 0.0f ? EDTHeroPart::ArmL : EDTHeroPart::ArmR;
				TestTrue(TEXT("moving: a leg turns about its hip"), Drawn(Walk, Leg, DTHeroPose::Hip(Side)).Equals(Drawn(Walk, EDTHeroPart::Hips, DTHeroPose::Hip(Side)), Tolerance));
				TestTrue(TEXT("moving: an arm turns about its shoulder"), Drawn(Walk, Arm, DTHeroPose::Shoulder(Side)).Equals(Drawn(Walk, EDTHeroPart::Torso, DTHeroPose::Shoulder(Side)), Tolerance));
				TestTrue(TEXT("dashing: a leg turns about its hip"), Drawn(Dash, Leg, DTHeroPose::Hip(Side)).Equals(Drawn(Dash, EDTHeroPart::Hips, DTHeroPose::Hip(Side)), Tolerance));
				TestTrue(TEXT("dashing: an arm turns about its shoulder"), Drawn(Dash, Arm, DTHeroPose::Shoulder(Side)).Equals(Drawn(Dash, EDTHeroPart::Torso, DTHeroPose::Shoulder(Side)), Tolerance));
			}

			// The body stays over the hero's point, so it still marks where the hero is hit: the pelvis never leaves the axis.
			const FVector PelvisWalk = Drawn(Walk, EDTHeroPart::Hips, FVector(0.0, 0.0, DTHeroPose::Hip(RightSide).Z));
			const FVector PelvisDash = Drawn(Dash, EDTHeroPart::Hips, FVector(0.0, 0.0, DTHeroPose::Hip(RightSide).Z));
			TestTrue(TEXT("moving: the pelvis stays over the hero's point"), FVector2D(PelvisWalk.X, PelvisWalk.Y).IsNearlyZero(Tolerance));
			TestTrue(TEXT("dashing: the pelvis stays over the hero's point"), FVector2D(PelvisDash.X, PelvisDash.Y).IsNearlyZero(Tolerance));
			TestEqual(TEXT("moving: the pelvis keeps its height"), PelvisWalk.Z, DTHeroPose::Hip(RightSide).Z * Scale(), Tolerance);
			TestEqual(TEXT("dashing: the pelvis drops by C.HERO_ART.DASH_DROP_UU"), PelvisDash.Z, (DTHeroPose::Hip(RightSide).Z - static_cast<double>(DTConst::HERO_ART_DASH_DROP_UU)) * Scale(), Tolerance);

			// Moving: the crown leans the way the hero travels; the left sole leads and the right trails; the arms swing against the legs.
			const double CrownWalk = FVector::DotProduct(Drawn(Walk, EDTHeroPart::Hood, Crown()) - Drawn(Still, EDTHeroPart::Hood, Crown()), Along);
			TestTrue(TEXT("moving: the crown leans along the velocity"), CrownWalk > Tolerance);
			TestTrue(TEXT("moving: the left sole leads"), FVector::DotProduct(Drawn(Walk, EDTHeroPart::LegL, Sole(LeftSide)) - Drawn(Still, EDTHeroPart::LegL, Sole(LeftSide)), Along) > Tolerance);
			TestTrue(TEXT("moving: the right sole trails"), FVector::DotProduct(Drawn(Walk, EDTHeroPart::LegR, Sole(RightSide)) - Drawn(Still, EDTHeroPart::LegR, Sole(RightSide)), Along) < -Tolerance);
			const double FistLeft = FVector::DotProduct(Drawn(Walk, EDTHeroPart::ArmL, Fist(LeftSide)) - Drawn(Walk, EDTHeroPart::Torso, Fist(LeftSide)), Along);
			const double FistRight = FVector::DotProduct(Drawn(Walk, EDTHeroPart::ArmR, Fist(RightSide)) - Drawn(Walk, EDTHeroPart::Torso, Fist(RightSide)), Along);
			TestTrue(TEXT("moving: the left arm swings back against the left leg"), FistLeft < -Tolerance);
			TestTrue(TEXT("moving: the right arm swings forward"), FistRight > Tolerance);

			// Dashing: a deeper lean than walking, both soles and both fists trailing.
			const double CrownDash = FVector::DotProduct(Drawn(Dash, EDTHeroPart::Hood, Crown()) - Drawn(Still, EDTHeroPart::Hood, Crown()), Along);
			TestTrue(TEXT("dashing: the crown leans further than moving"), CrownDash > CrownWalk + Tolerance);
			for (const float Side : { LeftSide, RightSide })
			{
				const EDTHeroPart Leg = Side < 0.0f ? EDTHeroPart::LegL : EDTHeroPart::LegR;
				const EDTHeroPart Arm = Side < 0.0f ? EDTHeroPart::ArmL : EDTHeroPart::ArmR;
				TestTrue(TEXT("dashing: a sole trails"), FVector::DotProduct(Drawn(Dash, Leg, Sole(Side)) - Drawn(Dash, EDTHeroPart::Hips, Sole(Side)), Along) < -Tolerance);
				TestTrue(TEXT("dashing: a fist trails"), FVector::DotProduct(Drawn(Dash, Arm, Fist(Side)) - Drawn(Dash, EDTHeroPart::Torso, Fist(Side)), Along) < -Tolerance);
			}

			// The facing is the aim direction whatever the travel: the hips are never turned by a stride or a dash.
			TestTrue(TEXT("moving: the hips face the aim direction"), Part(Walk, EDTHeroPart::Hips).TransformVectorNoScale(FVector::ForwardVector).Equals(Flat(Idle.AimDir), Tolerance));
			TestTrue(TEXT("dashing: the hips face the aim direction"), Part(Dash, EDTHeroPart::Hips).TransformVectorNoScale(FVector::ForwardVector).Equals(Flat(Idle.AimDir), Tolerance));
		}
	}

	// Speed past the hero's own (a dash's carry, a push) reads as full stride, never more; half speed is half way.
	{
		FDTHeroPoseInput Fast;
		Fast.Vel = FVector2f(FullSpeed * 3.0f, 0.0f);
		FDTHeroPoseInput Full;
		Full.Vel = FVector2f(FullSpeed, 0.0f);
		FDTHeroPoseInput Half;
		Half.Vel = FVector2f(FullSpeed * 0.5f, 0.0f);
		const FDTHeroPose FastPose = DTHeroPose::Evaluate(Fast);
		const FDTHeroPose FullPose = DTHeroPose::Evaluate(Full);
		const FDTHeroPose HalfPose = DTHeroPose::Evaluate(Half);
		TestTrue(TEXT("the stride is capped at the hero's own speed"), Part(FastPose, EDTHeroPart::LegL).Equals(Part(FullPose, EDTHeroPart::LegL), Tolerance));
		const double FullLead = Drawn(FullPose, EDTHeroPart::LegL, Sole(LeftSide)).X;
		const double HalfLead = Drawn(HalfPose, EDTHeroPart::LegL, Sole(LeftSide)).X;
		TestTrue(TEXT("half speed strides less than full speed"), HalfLead > Tolerance && HalfLead < FullLead - Tolerance);
	}

	// FromState reads the facing, the velocity and the dash from the sim's state.
	{
		FDTHeroState State;
		State.AimDir = FVector2f(0.0f, -1.0f);
		State.Vel = FVector2f(12.0f, 34.0f);
		State.DashDir = FVector2f(0.0f, 1.0f);
		FDTHeroPoseInput Read = DTHeroPose::FromState(State);
		TestTrue(TEXT("FromState: aim"), Read.AimDir == State.AimDir);
		TestTrue(TEXT("FromState: velocity"), Read.Vel == State.Vel);
		TestTrue(TEXT("FromState: dash direction"), Read.DashDir == State.DashDir);
		TestFalse(TEXT("FromState: not dashing with no dash time left"), Read.bDashing);
		State.DashLeft = static_cast<float>(DTConst::DASH_DURATION_S);
		Read = DTHeroPose::FromState(State);
		TestTrue(TEXT("FromState: dashing while dash time is left"), Read.bDashing);
	}
	return true;
}

// T-HAR-02, T-HAR-04, T-HAR-08: the pawn draws the figure from static meshes in tokens, and drawing leaves the sim as it was.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTHeroArtParts, "Dusktracer.HeroArt.Parts", DTHeroArtTest::Flags)
bool FDTHeroArtParts::RunTest(const FString& Parameters)
{
	using namespace DTHeroArtTest;

	// T-HAR-04: the armour is the token the contrast gate reads; the dark tokens are the only others.
	int32 ArmourParts = 0;
	for (int32 Index = 0; Index < DTHeroPose::NumParts; ++Index)
	{
		const EDTHeroPart Which = static_cast<EDTHeroPart>(Index);
		const FColor Token = DTHeroPose::Token(Which);
		TestTrue(*FString::Printf(TEXT("%s is drawn in a hero token"), DTHeroPose::PartName(Which)),
			Token == DTPal::Hero_Armour || Token == DTPal::Hero_DarkMetal || Token == DTPal::Hero_Undersuit);
		ArmourParts += Token == DTPal::Hero_Armour ? 1 : 0;
		TestTrue(*FString::Printf(TEXT("%s is an SM_Hero_ mesh"), DTHeroPose::PartName(Which)), FString(DTHeroPose::MeshPath(Which)).StartsWith(TEXT("/Game/DT/Meshes/SM_Hero_")));
	}
	TestTrue(TEXT("most parts are armour"), ArmourParts * 2 > DTHeroPose::NumParts);
	for (const EDTHeroPart Which : { EDTHeroPart::Torso, EDTHeroPart::Pauldrons, EDTHeroPart::Hood, EDTHeroPart::Hips, EDTHeroPart::ArmL, EDTHeroPart::ArmR, EDTHeroPart::LegL, EDTHeroPart::LegR })
	{
		TestTrue(*FString::Printf(TEXT("%s is Pal.Hero.Armour"), DTHeroPose::PartName(Which)), DTHeroPose::Token(Which) == DTPal::Hero_Armour);
	}

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}
	World->BeginPlay();
	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTHeroSubsystem* HeroSystem = World->GetSubsystem<UDTHeroSubsystem>();
	ADTHero* Pawn = World->SpawnActor<ADTHero>();
	if (TestNotNull(TEXT("sim subsystem"), Sim) && TestNotNull(TEXT("hero subsystem"), HeroSystem) && TestNotNull(TEXT("pawn spawned"), Pawn))
	{
		// T-HAR-02: one static mesh per part, instanced once, lit and not self-lit, with no collision; nothing skeletal.
		UDTHeroFigureComponent* Figure = Pawn->GetFigure();
		if (TestNotNull(TEXT("the pawn has a figure"), Figure))
		{
			// A bare test world has not begun play for the pawn, so the figure is built here as ADTHero::BeginPlay builds it.
			TestTrue(TEXT("every part's mesh loaded"), Figure->Build() && Figure->IsBuilt());
			const UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor"));
			for (int32 Index = 0; Index < DTHeroPose::NumParts; ++Index)
			{
				const EDTHeroPart Which = static_cast<EDTHeroPart>(Index);
				const UInstancedStaticMeshComponent* Instances = Figure->GetPart(Which);
				if (!TestNotNull(*FString::Printf(TEXT("%s is drawn"), DTHeroPose::PartName(Which)), Instances))
				{
					continue;
				}
				TestEqual(TEXT("one instance per part"), Instances->GetInstanceCount(), 1);
				TestTrue(TEXT("the part's mesh is its SM_Hero_ asset"), Instances->GetStaticMesh() && Instances->GetStaticMesh()->GetPathName() == DTHeroPose::MeshPath(Which));
				TestTrue(TEXT("no collision"), Instances->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
				TestTrue(TEXT("drawn on M_InstanceColor"), Material && Instances->GetMaterial(0) == Material);
				const FLinearColor Colour = DTPal::Linear(DTHeroPose::Token(Which));
				const TArray<float>& Custom = Instances->PerInstanceSMCustomData;
				if (TestEqual(TEXT("colour and emissive per instance"), Custom.Num(), 4))
				{
					TestEqual(TEXT("token red"), Custom[0], Colour.R);
					TestEqual(TEXT("token green"), Custom[1], Colour.G);
					TestEqual(TEXT("token blue"), Custom[2], Colour.B);
					TestEqual(TEXT("not self-lit"), Custom[3], 0.0f);
				}
			}
		}

		// T-HAR-08: drawing reads the state. A run is stepped, the pawn is drawn in it, and the hero's state is what it was.
		Sim->StartRun(9107, 1);
		FDTHeroInput In;
		In.Move = FVector2f(1.0f, 0.5f);
		In.bDashPress = true;
		HeroSystem->LatchInput(0, In);
		Sim->StepNow(3);
		HeroSystem->RegisterPawn(Pawn);
		const FDTHeroState Before = *HeroSystem->GetState(0);
		TestTrue(TEXT("the staged step is a dash"), Before.DashLeft > 0.0f);
		HeroSystem->SimRender(0.5f);
		const FDTHeroState& After = *HeroSystem->GetState(0);
		TestTrue(TEXT("the draw leaves the position"), After.Pos == Before.Pos && After.PrevPos == Before.PrevPos);
		TestTrue(TEXT("the draw leaves the velocity and the facing"), After.Vel == Before.Vel && After.AimDir == Before.AimDir && After.DashDir == Before.DashDir);
		TestTrue(TEXT("the draw leaves the dash and the hit points"), After.DashLeft == Before.DashLeft && After.DashCharges == Before.DashCharges && After.Hp == Before.Hp);
		// And the figure shows that state: the hood leans along the dash.
		if (Figure && Figure->GetPart(EDTHeroPart::Hood))
		{
			const FDTHeroPose Shown = DTHeroPose::Evaluate(DTHeroPose::FromState(Before));
			TestTrue(TEXT("the pawn's hood is where the pose puts it"), Figure->GetPart(EDTHeroPart::Hood)->GetRelativeTransform().Equals(Part(Shown, EDTHeroPart::Hood), Tolerance));
			FDTHeroPoseInput Idle;
			Idle.AimDir = Before.AimDir;
			TestFalse(TEXT("a dashing hero is not drawn idle"), Part(Shown, EDTHeroPart::Hood).Equals(Part(DTHeroPose::Evaluate(Idle), EDTHeroPart::Hood), Tolerance));
		}
	}
	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
