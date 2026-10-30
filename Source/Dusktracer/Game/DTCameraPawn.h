// Top-down follow camera of player 0's hero (02_Systems/combat_core.md; shared co-op framing is coop.md).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DTCameraPawn.generated.h"

class UCameraComponent;

/**
 * Looks down at C.ARENA.CAMERA_PITCH_DEG with a horizontal field of view of C.ARENA.CAMERA_FOV_DEG, placed so that a
 * circle of C.ARENA.CAMERA_FRAME_RADIUS_UU around its root fits the screen height (the stress swarm). Not possessed:
 * ADTGameMode attaches it to player 0's ADTHero and makes it the view target, so it follows the hero exactly as drawn
 * (interpolated at the sim alpha), at FollowTransform from it. With no hero it stays at that offset from the arena origin.
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

	/**
	 * Where the camera sits relative to what it follows; the camera is the root, so the actor's transform IS the view.
	 * It must be: APlayerController::GetPlayerViewPoint uses the view target's actor location and rotation until the
	 * camera manager's cache time is above zero, and the run-start Patron hand pauses the world before any time passes
	 * (a paused world never updates the camera manager). With the camera offset inside the actor, that hand was drawn
	 * from the hero's position at level pitch: a horizon and the hero's dark disc instead of the arena from above.
	 */
	static FTransform FollowTransform();

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;
};
