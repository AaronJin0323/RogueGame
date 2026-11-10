// Draws every live enemy round as one instance of its own instanced mesh (enemies.md R-ENM-17), never the player-bullet ISM.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTEnemyBulletRenderer.generated.h"

class FDTEnemyBulletPool;
class UInstancedStaticMeshComponent;
class UMaterialInterface;

/**
 * Transient actor spawned by UDTEnemySubsystem; one per world. Instance custom data: 0-2 linear RGB, 3 emissive
 * strength (read by /Game/DT/Materials/M_InstanceColor). Density LOD never dims it (R-PRE-06).
 * tracer_art.md: the same actor draws each round's trail (R-TRC-04) and the muzzle flash of a round just fired (R-TRC-07), as two
 * more instanced components on the engine plane and M_EnemyRound_Trail; neither takes a density either.
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
	/**
	 * Draws the trail behind every round of Rounds and the flash of every round fired within C.TRC.MUZZLE_S, at the interpolation
	 * Alpha and the height Z of the rounds. Reads the pool (positions, velocities, radii, firing points); writes nothing to it.
	 */
	void SetTrails(const FDTEnemyBulletPool& Rounds, float Alpha, double Z);

	/** Half-width of the mesh in uu: a round of radius R draws at scale R / GetMeshRadius(). */
	float GetMeshRadius() const { return MeshRadius; }
	int32 GetInstanceCount() const;
	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }
	int32 GetOutlineCount() const;
	UInstancedStaticMeshComponent* GetOutlines() const { return Outlines; }
	UInstancedStaticMeshComponent* GetTrails() const { return Trails; }
	UInstancedStaticMeshComponent* GetFlashes() const { return Flashes; }
	/** False without M_EnemyRound_Trail: rounds then draw with no trail and no flash. */
	bool HasTrailMaterial() const { return bTrailMaterial; }
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

	/** tracer_art.md R-TRC-04: one quad behind each round that flies fast enough to show one. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Trails;

	/** tracer_art.md R-TRC-07: one quad at the firing point of each round just fired. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Flashes;

	bool bTrailMaterial = false;
	TArray<FTransform> TrailScratch;
	TArray<FTransform> FlashScratch;
	float MeshRadius = 1.0f;
	float DiscMeshRadius = 1.0f;
	float DiscMeshHeight = 1.0f;
	TArray<FTransform> DiscScratch;
	bool bFallbackMaterial = false;
	float CustomData[NumCustomData] = {};
	TArray<float> CustomScratch;
	TArray<int32> RemoveScratch;
};
