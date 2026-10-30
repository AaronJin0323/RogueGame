// Draws every FDTWorldView::Telegraphs entry as red projected quads on the ground (enemies.md R-ENM-13, 04_Presentation R-PRE-03).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimTypes.h"
#include "DTTelegraphRenderer.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * Transient actor spawned by UDTEnemySubsystem; one per world, never one per telegraph. Every telegraph becomes one or
 * more instances of a flat quad (the engine plane) at C.ENEMIES.TELEGRAPH_Z_UU: above the floor, below the hulls and
 * rounds at C.SIM.PLANE_Z_UU. Instance custom data: 0-2 linear RGB (Pal.Telegraph), 3 emissive strength
 * (C.ENEMIES.TELEGRAPH_EMISSIVE), read by the unlit /Game/DT/Materials/M_CueColor. Density LOD never dims it (R-PRE-06).
 *
 * Footprints (the shape's damage area):
 *   Band, AimLine: one quad from Origin along Dir, Length long and Width wide.
 *   Ring: an annulus around Origin, outer radius Length, thickness Width (Width <= 0 or >= Length: a filled disc),
 *         drawn as RingSegments quads.
 *   Arc: the same annulus restricted to a sector centred on Dir; Width is then the sector's full angle in degrees and
 *        the sector is filled from the centre (the boss sweep, boss.md).
 *   RingGap, SpiralLane (boss.md R-BOS-07): the disc of radius Length around Origin, filled from the centre, minus the
 *        dodge lane: a sector of Width degrees centred on Dir is left undrawn.
 *   HazardRing (boss.md hazard): everything outside the circle of radius Length around Origin, out past the arena
 *        corners (C.ARENA.HALF_EXTENT_UU times sqrt 2), drawn as RingSegments quads whose inner edges are tangent to it.
 *
 * Layers (R-PRE-05): by default each telegraph draws three times, floor up. A dark band in Pal.Telegraph.Edge, the shape
 * grown by C.PRE.TELEGRAPH_EDGE_UU; the rim, the footprint itself, always at full emissive; the fill, the shape shrunk
 * by C.PRE.TELEGRAPH_RIM_UU so the rim shows round it, whose emissive ramps with Progress from
 * C.PRE.TELEGRAPH_FILL_MIN of full (none on a shape too thin to shrink: aim lines). Each layer sits
 * C.PRE.TELEGRAPH_LAYER_UU over the last. Control cues share these shapes and draw one flat layer (SetEdged false).
 */
UCLASS(NotPlaceable, Transient)
class DUSKTRACER_API ADTTelegraphRenderer : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 NumCustomData = 4;
	/** Quads a full ring is drawn with (tessellation of the footprint, not a tunable). */
	static constexpr int32 RingSegments = 32;

	ADTTelegraphRenderer();

	/** Loads the quad mesh and the material and sets the colour every instance carries; call once after spawning. */
	void InitRender(const FLinearColor& Color, float Emissive);

	/** The drawn height (default C.ENEMIES.TELEGRAPH_Z_UU; control cues sit under telegraphs, R-PRE-11). */
	void SetDrawZ(float InZ) { DrawZ = InZ; }

	/** False: one flat layer in the InitRender colour (control cues, R-PRE-11); true (default): the telegraph layers. */
	void SetEdged(bool bInEdged) { bEdged = bInEdged; }

	/** Rebuilds the instances from Telegraphs: count follows the layers of each entry; transforms and colours batch-written. */
	void SetTelegraphs(TConstArrayView<FDTTelegraph> Telegraphs);

	/** Number of quads one shape is drawn with. */
	static int32 InstancesFor(const FDTTelegraph& Telegraph);
	/** Number of quads one telegraph is drawn with across its layers. */
	static int32 InstancesDrawn(const FDTTelegraph& Telegraph);
	/** Telegraph with its boundary moved outward by OutwardUU (negative: inward); false when nothing is left of it. */
	static bool OffsetShape(const FDTTelegraph& Telegraph, float OutwardUU, FDTTelegraph& Out);
	/** The fill's share of full emissive at Progress. */
	static float FillScale(float Progress);
	/** Appends the quad transforms of one telegraph (MeshSize = edge length of the quad mesh in uu). */
	static void AppendTransforms(const FDTTelegraph& Telegraph, float MeshSize, float Z, TArray<FTransform>& Out);

	int32 GetInstanceCount() const;
	float GetMeshSizeUU() const { return MeshSizeUU; }
	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }
	bool IsUsingFallbackMaterial() const { return bFallbackMaterial; }

private:
	/** R-TEC-05 fallback when M_CueColor is missing: the engine basic-shape material (no per-instance colour). */
	static UMaterialInterface* Fallback_TelegraphMaterial();

	/** Appends one telegraph's quads at height Z, each carrying the NumCustomData floats at Data. */
	void AddLayer(const FDTTelegraph& Telegraph, float Z, const float* Data);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	float MeshSizeUU = 1.0f;
	bool bFallbackMaterial = false;
	bool bEdged = true;
	float CustomData[NumCustomData] = {};
	float DrawZ = static_cast<float>(DTConst::ENEMIES_TELEGRAPH_Z_UU);
	TArray<FTransform> Transforms;
	TArray<float> CustomScratch;
	TArray<int32> RemoveScratch;
};
