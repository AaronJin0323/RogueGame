// Transient actor drawing every enemy as two instanced parts (R-TEC-11: enemies are not Actors; enemies.md R-ENM-24):
// a dark hull in Pal.Enemy.Body without emissive and a separate always-lit optic in Pal.Enemy.Optic.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies/DTElite.h"
#include "DTEnemyRenderer.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(Transient, NotPlaceable)
class DUSKTRACER_API ADTEnemyRenderer : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 NumCustomData = 4;

	ADTEnemyRenderer();

	/** Loads mesh and material and writes the part colours (custom data 0-2 linear RGB, 3 emissive strength). */
	void Setup(const FLinearColor& HullColor, float HullEmissive, const FLinearColor& OpticColor, float OpticEmissive);

	/** Resizes each part's instance list to its transforms and writes every transform in one batch. */
	void UpdateInstances(TConstArrayView<FTransform> HullTransforms, TConstArrayView<FTransform> OpticTransforms);

	/** Elite light and affix parts (elites.md R-ELT-10, R-ELT-11): colours change per instance per frame. */
	void UpdateParts(TConstArrayView<FDTElitePart> InParts);

	/** Edge length of the mesh in uu (scale = size / this). */
	float GetMeshSizeUU() const { return MeshSizeUU; }

	UInstancedStaticMeshComponent* GetHull() const { return Hull; }
	UInstancedStaticMeshComponent* GetOptic() const { return Optic; }
	UInstancedStaticMeshComponent* GetParts() const { return Parts; }
	bool IsUsingFallbackMaterial() const { return bFallbackMaterial; }

private:
	void Sync(UInstancedStaticMeshComponent* Part, TConstArrayView<FTransform> Transforms, const TArray<float>& Data);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Hull;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Optic;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Parts;

	TArray<float> HullData;
	TArray<float> OpticData;
	TArray<int32> RemoveScratch;
	TArray<FTransform> PartTransforms;
	TArray<float> PartData;
	float MeshSizeUU = 1.0f;
	bool bFallbackMaterial = false;
};
