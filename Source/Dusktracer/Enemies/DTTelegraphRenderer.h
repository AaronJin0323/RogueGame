// Draws every FDTWorldView::Telegraphs entry as red projected quads on the ground (enemies.md R-ENM-13, 04_Presentation R-PRE-03).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DTSimTypes.h"
#include "DTTelegraphRenderer.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * Transient actor spawned by UDTEnemySubsystem; one per world, never one per telegraph. Every telegraph becomes one or
 * more instances of a flat quad (the engine plane) at C.ENEMIES.TELEGRAPH_Z_UU: above the floor, below the hulls and
 * rounds at C.SIM.PLANE_Z_UU. Instance custom data: 0-2 linear RGB (Pal.Telegraph), 3 emissive strength
 * (C.ENEMIES.TELEGRAPH_EMISSIVE), read by /Game/DT/Materials/M_InstanceColor. Density LOD never dims it (R-PRE-06).
 *
 * Footprints (the shape's damage area):
 *   Band, AimLine: one quad from Origin along Dir, Length long and Width wide.
 *   Ring: an annulus around Origin, outer radius Length, thickness Width (Width <= 0 or >= Length: a filled disc),
 *         drawn as RingSegments quads.
 *   Arc: the same annulus restricted to a sector centred on Dir; Width is then the sector's full angle in degrees and
 *        the sector is filled from the centre (no producer yet; boss.md sweep sector).
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

	/** Rebuilds the instances from Telegraphs: count follows InstancesFor of each entry; transforms batch-written. */
	void SetTelegraphs(TConstArrayView<FDTTelegraph> Telegraphs);

	/** Number of quads one telegraph is drawn with. */
	static int32 InstancesFor(const FDTTelegraph& Telegraph);
	/** Appends the quad transforms of one telegraph (MeshSize = edge length of the quad mesh in uu). */
	static void AppendTransforms(const FDTTelegraph& Telegraph, float MeshSize, float Z, TArray<FTransform>& Out);

	int32 GetInstanceCount() const;
	float GetMeshSizeUU() const { return MeshSizeUU; }
	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }
	bool IsUsingFallbackMaterial() const { return bFallbackMaterial; }

private:
	/** R-TEC-05 fallback when M_InstanceColor is missing: the engine basic-shape material (no per-instance colour). */
	static UMaterialInterface* Fallback_TelegraphMaterial();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	float MeshSizeUU = 1.0f;
	bool bFallbackMaterial = false;
	float CustomData[NumCustomData] = {};
	TArray<FTransform> Transforms;
	TArray<float> CustomScratch;
	TArray<int32> RemoveScratch;
};
