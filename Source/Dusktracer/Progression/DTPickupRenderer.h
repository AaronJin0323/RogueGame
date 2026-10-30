// Transient actor holding the instanced meshes gems and chests are drawn with (R-PRG-07: gems are never Actors).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTPickupRenderer.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(Transient, NotPlaceable)
class DUSKTRACER_API ADTPickupRenderer : public AActor
{
	GENERATED_BODY()

public:
	ADTPickupRenderer();

	/** Loads meshes and the instance-colour material; colours come from Pal.Pickup.Gem / Pal.Pickup.Chest. */
	void Setup();

	/** Resizes each instance list and writes every transform in one batch. */
	void UpdateGems(TConstArrayView<FTransform> Transforms);
	void UpdateChests(TConstArrayView<FTransform> Transforms);

	/** Edge length of the cube mesh in uu (scale = size / this). */
	float GetMeshSizeUU() const { return MeshSizeUU; }

	UInstancedStaticMeshComponent* GetGemInstances() const { return Gems; }
	UInstancedStaticMeshComponent* GetChestInstances() const { return Chests; }

private:
	void Update(UInstancedStaticMeshComponent* Instances, const TArray<float>& Custom, TConstArrayView<FTransform> Transforms);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Gems;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Chests;

	TArray<float> GemCustom;
	TArray<float> ChestCustom;
	TArray<int32> RemoveScratch;
	float MeshSizeUU = 1.0f;
};
