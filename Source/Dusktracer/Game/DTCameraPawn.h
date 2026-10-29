// Fixed top-down camera over the arena origin (S0; player movement and follow come with the hero in S1).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DTCameraPawn.generated.h"

class UCameraComponent;

/**
 * Looks down at C.ARENA.CAMERA_PITCH_DEG with a horizontal field of view of C.ARENA.CAMERA_FOV_DEG, placed so that a
 * circle of C.ARENA.CAMERA_FRAME_RADIUS_UU around the pawn fits the screen height (the stress swarm).
 */
UCLASS()
class DUSKTRACER_API ADTCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ADTCameraPawn();

	/** Distance from the look-at point at which FrameRadius fills half the screen height (horizontal FOV, aspect w/h). */
	static float ComputeFramingDistance(float FrameRadius, float HorizontalFovDeg, float Aspect);

	UCameraComponent* GetCamera() const { return Camera; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;
};
