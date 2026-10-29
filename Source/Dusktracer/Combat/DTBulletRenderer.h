// Draws every live bullet as one instance of one instanced static mesh component (07_Technical R-TEC-11).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTBulletRenderer.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * Transient actor spawned by UDTBulletSubsystem. One per world, never one per bullet. Instance custom data:
 * 0-2 linear RGB, 3 emissive strength (read by /Game/DT/Materials/M_InstanceColor).
 */
UCLASS(NotPlaceable, Transient)
class DUSKTRACER_API ADTBulletRenderer : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 NumCustomData = 4;

	ADTBulletRenderer();

	/** Loads the mesh and material; call once after spawning. */
	void InitRender(const FLinearColor& Color, float Emissive);

	/** Makes the instance count equal Transforms.Num() (adding/removing only the tail), then batch-writes all transforms. */
	void SetInstances(TConstArrayView<FTransform> Transforms);

	/** Half-width of the mesh in uu: a bullet of radius R draws at scale R / GetMeshRadius(). */
	float GetMeshRadius() const { return MeshRadius; }
	int32 GetInstanceCount() const;
	bool IsUsingFallbackMaterial() const { return bFallbackMaterial; }

private:
	/** R-TEC-05 fallback when M_InstanceColor is missing: the engine basic-shape material (no per-instance colour). */
	static UMaterialInterface* Fallback_BulletMaterial();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	float MeshRadius = 1.0f;
	bool bFallbackMaterial = false;
	float CustomData[NumCustomData] = {};
	TArray<float> CustomScratch;
};
