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
	static constexpr int32 NumCustomData = 7;
	/** Outline discs (settings.md R-SET-10): 0-2 linear RGB, 3 emissive (M_CueColor). */
	static constexpr int32 NumOutlineData = 4;

	ADTEnemyBulletRenderer();

	/** Loads the mesh and material; call once after spawning. */
	/** Custom data: 0-2 rim linear RGB, 3 emissive strength, 4-6 core linear RGB (M_EnemyRound, R-ENM-17). */
	void InitRender(const FLinearColor& Rim, float Emissive, const FLinearColor& Core);

	/** Makes the instance count equal Transforms.Num() (adding/removing only the tail), then batch-writes all transforms. */
	void SetInstances(TConstArrayView<FTransform> Transforms);

	/** Half-width of the mesh in uu: a round of radius R draws at scale R / GetMeshRadius(). */
	float GetMeshRadius() const { return MeshRadius; }
	int32 GetInstanceCount() const;
	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }
	int32 GetOutlineCount() const;
	UInstancedStaticMeshComponent* GetOutlines() const { return Outlines; }
	bool IsUsingFallbackMaterial() const { return bFallbackMaterial; }

private:
	/** R-TEC-05 fallback when M_InstanceColor is missing: the engine basic-shape material (no per-instance colour). */
	static UMaterialInterface* Fallback_EnemyFireMaterial();
	void SetOutlines(TConstArrayView<FTransform> Rounds);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	/** Danger outlines: two flat discs under each round; empty while the setting is off. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Outlines;

	float MeshRadius = 1.0f;
	float DiscMeshRadius = 1.0f;
	float DiscMeshHeight = 1.0f;
	TArray<FTransform> DiscScratch;
	bool bFallbackMaterial = false;
	float CustomData[NumCustomData] = {};
	TArray<float> CustomScratch;
	TArray<int32> RemoveScratch;
};
