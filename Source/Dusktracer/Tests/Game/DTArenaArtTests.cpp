// Arena art tests (02_Systems/arena_art.md, S4 wave 1 package A): T-ARN-01 to T-ARN-04.
// They read the committed assets, never a rendered frame: the plate's relief, the apron's reach against every frame the hero can
// stand in, that nothing of the ground collides, and which material each part wears. Frames are judged by Tools/arena_check.py.
// Run headless: & Tools\RunTests.ps1 -Filter Dusktracer.Arena

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Game/DTCameraPawn.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTArenaArtTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	const TCHAR* const PlatePath = TEXT("/Game/DT/Meshes/SM_Env_Plate.SM_Env_Plate");
	const TCHAR* const ApronPath = TEXT("/Game/DT/Meshes/SM_Env_Apron.SM_Env_Apron");
	const TCHAR* const GroundMaterial = TEXT("/Game/DT/Materials/M_Ground.M_Ground");
	const TCHAR* const StoneMaterial = TEXT("/Game/DT/Materials/M_Env_Stone.M_Env_Stone");
	const TCHAR* const MapPath = TEXT("/Game/DT/Maps/L_Arena_Test.L_Arena_Test");
	// uu; a mesh comes through FBX in single floats.
	constexpr double SizeTolerance = 0.05;

	/** Where the four corners of the frame land on the plane Z, for a hero standing at Hero and a camera framing Radius in a window of Aspect. */
	static TArray<FVector2D> FrameCorners(const FVector2D& Hero, double Radius, double Aspect, double Z)
	{
		const double Pitch = FMath::DegreesToRadians(static_cast<double>(DTConst::ARENA_CAMERA_PITCH_DEG));
		const double TanTarget = FMath::Tan(FMath::DegreesToRadians(static_cast<double>(DTConst::ARENA_CAMERA_FOV_DEG) * 0.5));
		// The height holds and the sides follow the window (R-UX-01).
		const double TanVertical = TanTarget / static_cast<double>(ADTCameraPawn::TargetAspect);
		const double TanHorizontal = TanVertical * Aspect;
		const double Distance = static_cast<double>(ADTCameraPawn::ComputeFramingDistance(static_cast<float>(Radius),
			static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG), ADTCameraPawn::TargetAspect));
		// The camera follows the hero's actor, which is drawn on the floor (ADTCameraPawn::FollowTransform).
		const FVector Eye(Hero.X - Distance * FMath::Cos(Pitch), Hero.Y, Distance * FMath::Sin(Pitch));
		const FVector Forward(FMath::Cos(Pitch), 0.0, -FMath::Sin(Pitch));
		const FVector Right(0.0, 1.0, 0.0);
		const FVector Up(FMath::Sin(Pitch), 0.0, FMath::Cos(Pitch));
		TArray<FVector2D> Out;
		for (const double SideSign : { -1.0, 1.0 })
		{
			for (const double UpSign : { -1.0, 1.0 })
			{
				const FVector Ray = Forward + Right * (SideSign * TanHorizontal) + Up * (UpSign * TanVertical);
				// Every ray of this camera points down: the pitch is steeper than half the vertical field of view (T-UX-01).
				const double T = (Z - Eye.Z) / Ray.Z;
				Out.Add(FVector2D(Eye.X + Ray.X * T, Eye.Y + Ray.Y * T));
			}
		}
		return Out;
	}

	/** The places the clamp lets a hero stand that show the most past the plate: its four corners and the middle of each side. */
	static TArray<FVector2D> EdgeStands()
	{
		const double Limit = static_cast<double>(DTConst::ARENA_HALF_EXTENT_UU - DTConst::HERO_RADIUS_UU);
		TArray<FVector2D> Out;
		for (const double X : { -Limit, 0.0, Limit })
		{
			for (const double Y : { -Limit, 0.0, Limit })
			{
				if (X != 0.0 || Y != 0.0)
				{
					Out.Add(FVector2D(X, Y));
				}
			}
		}
		return Out;
	}

	static bool HasCollision(const UStaticMesh* Mesh)
	{
		const UBodySetup* Body = Mesh ? Mesh->GetBodySetup() : nullptr;
		return Body && Body->AggGeom.GetElementCount() > 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArenaPlateRelief, "Dusktracer.Arena.PlateRelief", DTArenaArtTest::Flags) // T-ARN-01
bool FDTArenaPlateRelief::RunTest(const FString& Parameters)
{
	using namespace DTArenaArtTest;
	const UStaticMesh* Plate = LoadObject<UStaticMesh>(nullptr, PlatePath);
	if (!TestNotNull(TEXT("SM_Env_Plate"), Plate))
	{
		return false;
	}
	const FBox Box = Plate->GetBoundingBox();
	const double Half = static_cast<double>(DTConst::ARENA_HALF_EXTENT_UU);
	// The plate is the sim's arena, to the edge and no further.
	TestEqual(TEXT("the plate starts at -half extent in X"), Box.Min.X, -Half, SizeTolerance);
	TestEqual(TEXT("the plate ends at +half extent in X"), Box.Max.X, Half, SizeTolerance);
	TestEqual(TEXT("the plate starts at -half extent in Y"), Box.Min.Y, -Half, SizeTolerance);
	TestEqual(TEXT("the plate ends at +half extent in Y"), Box.Max.Y, Half, SizeTolerance);
	// Relief is cut down from the floor, never raised: the top is the floor the hero, the cues and the telegraphs are drawn over.
	TestEqual(TEXT("the slab tops are the floor, Z 0"), Box.Max.Z, 0.0, SizeTolerance);
	TestTrue(TEXT("the floor stays under the lowest cue layer"), Box.Max.Z < static_cast<double>(DTConst::PRE_ZONE_Z_UU));
	// Shallow: the joints and the bed that closes them, and nothing deeper.
	const double Deepest = static_cast<double>(DTConst::ARN_JOINT_DEPTH_UU) + 1.0;
	TestTrue(FString::Printf(TEXT("the relief is the joints' depth: lowest point %.2f uu, at most %.2f under the floor"), Box.Min.Z, Deepest),
		Box.Min.Z >= -Deepest && Box.Min.Z <= -static_cast<double>(DTConst::ARN_JOINT_DEPTH_UU) + SizeTolerance);
	// A joint is shallower than it is wide by far: a chamfer the low sun still lights, not a trench that holds shadow.
	TestTrue(TEXT("a joint's chamfer is flatter than the sun is low"),
		FMath::RadiansToDegrees(FMath::Atan2(static_cast<double>(DTConst::ARN_JOINT_DEPTH_UU), static_cast<double>(DTConst::ARN_JOINT_HALF_WIDTH_UU)))
			< static_cast<double>(DTConst::ARENA_SUN_ELEVATION_DEG));
	TestEqual(TEXT("one material slot"), Plate->GetStaticMaterials().Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArenaApronReach, "Dusktracer.Arena.ApronReach", DTArenaArtTest::Flags) // T-ARN-02
bool FDTArenaApronReach::RunTest(const FString& Parameters)
{
	using namespace DTArenaArtTest;
	const UStaticMesh* Apron = LoadObject<UStaticMesh>(nullptr, ApronPath);
	if (!TestNotNull(TEXT("SM_Env_Apron"), Apron))
	{
		return false;
	}
	const FBox Box = Apron->GetBoundingBox();
	const double Half = static_cast<double>(DTConst::ARENA_HALF_EXTENT_UU);
	// No wall on any side, the camera's least of all: nothing of the apron stands above the floor.
	TestTrue(FString::Printf(TEXT("the apron's highest point %.2f uu is not above the floor"), Box.Max.Z), Box.Max.Z <= SizeTolerance);
	TestTrue(TEXT("the apron falls: its lowest point is under the floor"), Box.Min.Z < -static_cast<double>(DTConst::ARN_APRON_RISE_UU));
	const double Outer = Half + static_cast<double>(DTConst::ARN_APRON_REACH_UU);
	TestEqual(TEXT("the skirt runs the apron's reach past the plate"), Box.Max.X, Outer, SizeTolerance);
	TestEqual(TEXT("on every side"), FMath::Min3(-Box.Min.X, -Box.Min.Y, Box.Max.Y), Outer, SizeTolerance);

	// Today's frame and the one D-036 reverts to, the target window and the widest one: every corner of every frame a hero at the
	// clamp can make lands on the apron. The lowest surface is where a ray lands farthest out, so it is the plane judged.
	const double Radii[] = { static_cast<double>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU), static_cast<double>(DTConst::ARN_FRAME_RADIUS_MAX_UU) };
	const double Aspects[] = { static_cast<double>(ADTCameraPawn::TargetAspect), static_cast<double>(DTConst::ARN_ASPECT_MAX) };
	double Tightest = TNumericLimits<double>::Max();
	int32 PastPlate = 0;
	for (const double Radius : Radii)
	{
		for (const double Aspect : Aspects)
		{
			for (const FVector2D& Hero : EdgeStands())
			{
				for (const FVector2D& Corner : FrameCorners(Hero, Radius, Aspect, Box.Min.Z))
				{
					const double Margin = FMath::Min(FMath::Min(Corner.X - Box.Min.X, Box.Max.X - Corner.X), FMath::Min(Corner.Y - Box.Min.Y, Box.Max.Y - Corner.Y));
					Tightest = FMath::Min(Tightest, Margin);
					if (Margin < 0.0)
					{
						AddError(FString::Printf(TEXT("frame radius %.0f, aspect %.3f, hero (%.0f,%.0f): a corner lands at (%.0f,%.0f), %.0f uu past the apron"),
							Radius, Aspect, Hero.X, Hero.Y, Corner.X, Corner.Y, -Margin));
					}
				}
				for (const FVector2D& Corner : FrameCorners(Hero, Radius, Aspect, 0.0))
				{
					PastPlate += FMath::Max(FMath::Abs(Corner.X), FMath::Abs(Corner.Y)) > Half ? 1 : 0;
				}
			}
		}
	}
	AddInfo(FString::Printf(TEXT("the tightest frame corner lands %.0f uu inside the apron's edge"), Tightest));
	// The check has teeth: the plate alone does not hold these frames, which is the void the apron is there to fill.
	TestTrue(TEXT("frames at the clamp reach past the plate"), PastPlate > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArenaGroundNoCollision, "Dusktracer.Arena.GroundDoesNotCollide", DTArenaArtTest::Flags) // T-ARN-03, T-ARN-04
bool FDTArenaGroundNoCollision::RunTest(const FString& Parameters)
{
	using namespace DTArenaArtTest;
	const UStaticMesh* Plate = LoadObject<UStaticMesh>(nullptr, PlatePath);
	const UStaticMesh* Apron = LoadObject<UStaticMesh>(nullptr, ApronPath);
	const UMaterialInterface* Ground = LoadObject<UMaterialInterface>(nullptr, GroundMaterial);
	const UMaterialInterface* Stone = LoadObject<UMaterialInterface>(nullptr, StoneMaterial);
	const UWorld* Map = LoadObject<UWorld>(nullptr, MapPath);
	if (!TestNotNull(TEXT("SM_Env_Plate"), Plate) || !TestNotNull(TEXT("SM_Env_Apron"), Apron) || !TestNotNull(TEXT("M_Ground"), Ground)
		|| !TestNotNull(TEXT("M_Env_Stone"), Stone) || !TestNotNull(TEXT("L_Arena_Test"), Map) || !TestNotNull(TEXT("its level"), Map->PersistentLevel.Get()))
	{
		return false;
	}
	// T-ARN-03: the meshes carry no collision shape, and the arena's actors ask for none. The sim's clamp is the only edge.
	TestFalse(TEXT("SM_Env_Plate has no collision shape"), HasCollision(Plate));
	TestFalse(TEXT("SM_Env_Apron has no collision shape"), HasCollision(Apron));
	int32 Plates = 0;
	int32 Aprons = 0;
	for (const AActor* Actor : Map->PersistentLevel->Actors)
	{
		const AStaticMeshActor* MeshActor = Cast<AStaticMeshActor>(Actor);
		const UStaticMeshComponent* Component = MeshActor ? MeshActor->GetStaticMeshComponent() : nullptr;
		if (!Component)
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s does not collide"), *MeshActor->GetName()), Component->BodyInstance.GetCollisionEnabled(false) == ECollisionEnabled::NoCollision);
		TestTrue(FString::Printf(TEXT("%s sits where its generator built it"), *MeshActor->GetName()),
			Component->GetRelativeLocation().IsNearlyZero() && Component->GetRelativeRotation().IsNearlyZero() && Component->GetRelativeScale3D().Equals(FVector::OneVector));
		// T-ARN-04: the plate wears M_Ground (R-PRE-05: it stays Pal.Ground.Lit), the apron the stone master.
		if (Component->GetStaticMesh() == Plate)
		{
			++Plates;
			TestTrue(TEXT("the plate wears M_Ground"), Component->GetMaterial(0) == Ground);
		}
		else if (Component->GetStaticMesh() == Apron)
		{
			++Aprons;
			TestTrue(TEXT("the apron wears M_Env_Stone"), Component->GetMaterial(0) == Stone);
		}
		else
		{
			AddError(FString::Printf(TEXT("%s is a ground mesh this spec does not know"), *MeshActor->GetName()));
		}
	}
	TestEqual(TEXT("one plate in the arena"), Plates, 1);
	TestEqual(TEXT("one apron in the arena"), Aprons, 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
