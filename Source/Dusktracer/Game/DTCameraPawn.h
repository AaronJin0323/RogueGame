// Top-down follow camera of player 0's hero (02_Systems/combat_core.md; shared co-op framing is coop.md).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "DTCameraPawn.generated.h"

class UCameraComponent;

/**
 * Looks down at C.ARENA.CAMERA_PITCH_DEG with a horizontal field of view of C.ARENA.CAMERA_FOV_DEG, placed so that a
 * circle of C.ARENA.CAMERA_FRAME_RADIUS_UU around its aim point fits the screen height at TargetAspect. The aim point is
 * C.ARENA.CAMERA_AIM_SHIFT_UU from its root toward the camera, so the root stands above the screen's centre and the reach
 * from it on the drawn plane is equal up and down the screen (06_UX R-UX-01; D-036 and D-080: every enemy attack starts on
 * screen). The height is what holds: a window of another shape keeps the vertical field of
 * view and shows more or less ground at the sides. The frame is fixed (R-UX-02): no tick, no input, no zoom. Not possessed:
 * ADTGameMode attaches it to player 0's ADTHero and makes it the view target, so it follows the hero exactly as drawn
 * (interpolated at the sim alpha), at FollowTransform from it. With no hero it stays at that offset from the arena origin.
 */
UCLASS()
class DUSKTRACER_API ADTCameraPawn : public APawn
{
	GENERATED_BODY()

public:
	ADTCameraPawn();

	/** The aspect the framing is solved for: the target resolution C.BUDGET.RES (1080p, 16:9). */
	static constexpr float TargetAspect = 16.0f / 9.0f;

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

	/** The camera's place for a frame of FrameRadius aimed Shift uu from the hero toward the camera, at the game's pitch and lens. */
	static FTransform FramingTransform(float FrameRadius, float Shift);

	/** The game's frame, the one place every world-to-screen sum reads: the camera's distance from its aim point, and the aim
	 *  point's shift from the hero toward the camera (C.ARENA.CAMERA_FRAME_RADIUS_UU, C.ARENA.CAMERA_AIM_SHIFT_UU; D-080). */
	static float FramingDistance();
	static float AimShift();

	/** Attaches the camera to Target at FollowTransform: from here it moves with Target and never relative to it (R-UX-02). */
	void Follow(AActor* Target);

private:
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;
};
