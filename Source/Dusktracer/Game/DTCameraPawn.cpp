#include "Game/DTCameraPawn.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Core/DTConstantsGen.h"

namespace DTCameraPawnLocal
{
	// The framing is solved for the target resolution C.BUDGET.RES (1080p, 16:9); wider windows only show more ground.
	constexpr float FrameAspect = 16.0f / 9.0f;
}

ADTCameraPawn::ADTCameraPawn()
{
	PrimaryActorTick.bCanEverTick = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bConstrainAspectRatio = false;
	Camera->bUsePawnControlRotation = false;
	Camera->SetFieldOfView(static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG));

	const float PitchRad = FMath::DegreesToRadians(static_cast<float>(DTConst::ARENA_CAMERA_PITCH_DEG));
	const float Distance = ComputeFramingDistance(static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU),
		static_cast<float>(DTConst::ARENA_CAMERA_FOV_DEG), DTCameraPawnLocal::FrameAspect);
	Camera->SetRelativeLocation(FVector(-Distance * FMath::Cos(PitchRad), 0.0, Distance * FMath::Sin(PitchRad)));
	Camera->SetRelativeRotation(FRotator(-static_cast<float>(DTConst::ARENA_CAMERA_PITCH_DEG), 0.0f, 0.0f));
}

float ADTCameraPawn::ComputeFramingDistance(float FrameRadius, float HorizontalFovDeg, float Aspect)
{
	const float TanHalfHorizontal = FMath::Tan(FMath::DegreesToRadians(HorizontalFovDeg * 0.5f));
	const float TanHalfVertical = TanHalfHorizontal / FMath::Max(Aspect, UE_KINDA_SMALL_NUMBER);
	return FrameRadius / FMath::Max(TanHalfVertical, UE_KINDA_SMALL_NUMBER);
}
