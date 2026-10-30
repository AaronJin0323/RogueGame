// Draws every live enemy round as one instance of its own instanced mesh (enemies.md R-ENM-17), never the player-bullet ISM.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTEnemyBulletRenderer.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * Transient actor spawned by UDTEnemySubsystem; one per world. Instance custom data: 0-2 linear RGB, 3 emissive
 * strength (read by /Game/DT/Materials/M_InstanceColor). Density LOD never dims it (R-PRE-06).
 */
UCLASS(NotPlaceable, Transient)
class DUSKTRACER_API ADTEnemyBulletRenderer : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 NumCustomData = 4;

	ADTEnemyBulletRenderer();

	/** Loads the mesh and material; call once after spawning. */
	void InitRender(const FLinearColor& Color, float Emissive);

	/** Makes the instance count equal Transforms.Num() (adding/removing only the tail), then batch-writes all transforms. */
	void SetInstances(TConstArrayView<FTransform> Transforms);

	/** Half-width of the mesh in uu: a round of radius R draws at scale R / GetMeshRadius(). */
	float GetMeshRadius() const { return MeshRadius; }
	int32 GetInstanceCount() const;
	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }
	bool IsUsingFallbackMaterial() const { return bFallbackMaterial; }

private:
	/** R-TEC-05 fallback when M_InstanceColor is missing: the engine basic-shape material (no per-instance colour). */
	static UMaterialInterface* Fallback_EnemyFireMaterial();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	float MeshRadius = 1.0f;
	bool bFallbackMaterial = false;
	float CustomData[NumCustomData] = {};
	TArray<float> CustomScratch;
	TArray<int32> RemoveScratch;
};
