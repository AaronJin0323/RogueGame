// The Reclaimer's rigid pose: a pure function from what the sim says to where each part is drawn (02_Systems/hero_art.md R-HAR-05).
// Presentation reads the sim and never the reverse (04_Presentation R-PRE-11): no sim file may include this header (Tools/tools_test.py).
#pragma once

#include "CoreMinimal.h"

struct FDTHeroState;

/** The rigid parts, one static mesh each (Tools/blender/hero_reclaimer.py PARTS, in this order). */
enum class EDTHeroPart : uint8
{
	Torso,
	Pauldrons,
	Hood,
	Visor,
	Undersuit,
	Hips,
	Belt,
	ArmL,
	ArmR,
	LegL,
	LegR,
	Num
};

/** Everything the pose depends on. Vectors are on the sim plane (world X, world Y). */
struct FDTHeroPoseInput
{
	/** Where the hero faces. Zero keeps +X. */
	FVector2f AimDir = FVector2f(1.0f, 0.0f);
	/** uu per second; its length against C.HERO.MOVE_SPEED_UU_S is how far into the moving pose the figure is. */
	FVector2f Vel = FVector2f::ZeroVector;
	/** Unit; read only while dashing. */
	FVector2f DashDir = FVector2f(1.0f, 0.0f);
	bool bDashing = false;
};

/** Where each part is drawn, relative to the hero's point on the floor. Every part keeps its shape: a turn, a move and one uniform scale. */
struct FDTHeroPose
{
	FTransform Parts[static_cast<int32>(EDTHeroPart::Num)];
};

namespace DTHeroPose
{
	constexpr int32 NumParts = static_cast<int32>(EDTHeroPart::Num);

	/** Reads the pose's input from a hero's sim state; writes nothing. */
	DUSKTRACER_API FDTHeroPoseInput FromState(const FDTHeroState& State);

	/** The pose. Pure: the same input gives the same pose, whatever was drawn before and wherever the hero stands. */
	DUSKTRACER_API FDTHeroPose Evaluate(const FDTHeroPoseInput& Input);

	/** The part's mesh, /Game/DT/Meshes/SM_Hero_<name>. */
	DUSKTRACER_API const TCHAR* MeshPath(EDTHeroPart Part);
	DUSKTRACER_API const TCHAR* PartName(EDTHeroPart Part);
	/** The part's palette token (04_Presentation §3): armour, dark metal or undersuit. */
	DUSKTRACER_API FColor Token(EDTHeroPart Part);

	/** The joints the parts turn about, in the figure's own space before the draw scale (C.HERO_ART.*; the mesh source reads the same keys). */
	DUSKTRACER_API FVector Waist();
	DUSKTRACER_API FVector Neck();
	/** Side is -1 for the left part, +1 for the right. */
	DUSKTRACER_API FVector Shoulder(float Side);
	DUSKTRACER_API FVector Hip(float Side);
}
