// Transient actor holding the one instanced mesh every swarm enemy is drawn with (R-TEC-11: enemies are not Actors).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DTEnemyRenderer.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(Transient, NotPlaceable)
class DUSKTRACER_API ADTEnemyRenderer : public AActor
{
	GENERATED_BODY()

public:
	ADTEnemyRenderer();

	/** Loads mesh and material; InstanceColor is written into custom data 0-2, EmissiveStrength into 3. */
	void Setup(const FLinearColor& InstanceColor, float EmissiveStrength);

	/** Resizes the instance list to Transforms.Num() and writes every transform in one batch. */
	void UpdateInstances(TConstArrayView<FTransform> Transforms);

	/** Edge length of the mesh in uu (scale = diameter / this). */
	float GetMeshSizeUU() const { return MeshSizeUU; }

	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	TArray<float> CustomData;
	TArray<int32> RemoveScratch;
	float MeshSizeUU = 1.0f;
};
