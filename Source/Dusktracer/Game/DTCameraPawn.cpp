#include "Game/DTCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Core/DTConstantsGen.h"

ADTCameraPawn::ADTCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// The camera is the root: the actor's own transform is the view (see FollowTransform).
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	SetRootComponent(Camera);
	Camera->bConstrainAspectRatio = false;
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG));
	// R-UX-01: a wider window shows more ground at the sides, never less height. The engine's default keeps the horizontal
	// field of view and so cuts height from a wide window; keep the vertical one the target aspect gives instead.
	Camera->SetAspectRatio(TargetAspect);
	Camera->SetAspectRatioAxisConstraint(AspectRatio_MaintainYFOV);
	Camera->bOverrideAspectRatioAxisConstraint = true;
}

FTransform ADTCameraPawn::FollowTransform()
{
	// D-080: the camera aims at a floor point C.ARENA.CAMERA_AIM_SHIFT_UU from the hero toward itself, so the hero stands
	// above the screen's centre and the reach up the screen equals the reach down (R-UX-01).
	return FramingTransform(static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU), AimShift());
}

FTransform ADTCameraPawn::FramingTransform(float FrameRadius, float Shift)
{
	const float PitchRad = FMath::DegreesToRadians(static_cast<float>(DTConst::ARENA_CAMERA_PITCH_DEG));
	const float Distance = ComputeFramingDistance(FrameRadius, static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG), TargetAspect);
	return FTransform(FRotator(-static_cast<float>(DTConst::ARENA_CAMERA_PITCH_DEG), 0.0f, 0.0f),
		FVector(-Distance * FMath::Cos(PitchRad) - Shift, 0.0, Distance * FMath::Sin(PitchRad)));
}

float ADTCameraPawn::FramingDistance()
{
	return ComputeFramingDistance(static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU), static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG), TargetAspect);
}

float ADTCameraPawn::AimShift()
{
	return static_cast<float>(DTConst::ARENA_CAMERA_AIM_SHIFT_UU);
}

void ADTCameraPawn::Follow(AActor* Target)
{
	if (Target)
	{
		SetActorRelativeTransform(FollowTransform());
		AttachToActor(Target, FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, false));
	}
}

float ADTCameraPawn::ComputeFramingDistance(float FrameRadius, float HorizontalFovDeg, float Aspect)
{
	const float TanHalfHorizontal = FMath::Tan(FMath::DegreesToRadians(HorizontalFovDeg * 0.5f));
	const float TanHalfVertical = TanHalfHorizontal / FMath::Max(Aspect, UE_KINDA_SMALL_NUMBER);
	return FrameRadius / FMath::Max(TanHalfVertical, UE_KINDA_SMALL_NUMBER);
}
