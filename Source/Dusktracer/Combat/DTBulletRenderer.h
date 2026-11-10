// Draws every live bullet as one instance of one instanced static mesh component (07_Technical R-TEC-11).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Presentation/DTTracerArt.h"
#include "DTBulletRenderer.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Transient actor spawned by UDTBulletSubsystem. One per world, never one per bullet. Instance custom data:
 * 0-2 linear RGB, 3 emissive strength (read by /Game/DT/Materials/M_InstanceColor).
 * tracer_art.md: the same actor draws a player round's head (InitTracerHead, M_Tracer) and its tail (InitTail, M_Tracer_Tail, on the
 * engine plane with DTTracerArt::TailData floats); both take SetDim, one material parameter for every instance (R-TRC-06).
 */
UCLASS(NotPlaceable, Transient)
class DUSKTRACER_API ADTBulletRenderer : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 NumCustomData = 4;
	/** The most floats any mode writes per instance (a tail: body RGB, strength, tip RGB). */
	static constexpr int32 MaxCustomData = 7;

	ADTBulletRenderer();

	/** Loads the mesh and material; call once after spawning. */
	void InitRender(const FLinearColor& Color, float Emissive);
	/** Control cues: the unlit M_CueColor instead of the lit bullet material (R-PRE-11), when it exists; same custom data. */
	void UseCueMaterial();
	/** R-TRC-01: a player round's head, unlit on M_Tracer; without the asset it stays on its InitRender material. Same custom data. */
	void InitTracerHead(const FLinearColor& Color, float Emissive);
	/** R-TRC-02, R-TRC-03: a tail on the engine plane and M_Tracer_Tail. False, and nothing is drawn, without the asset. */
	bool InitTail(const FLinearColor& Body, const FLinearColor& Tip, float Strength);
	/** R-TRC-06: scales the emissive of every instance through the material's Dim parameter. No effect on a material without one. */
	void SetDim(float InDim);
	float GetDim() const { return Dim; }

	/** Makes the instance count equal Transforms.Num() (adding/removing only the tail), then batch-writes all transforms. */
	void SetInstances(TConstArrayView<FTransform> Transforms);

	/** Half-width of the mesh in uu: a bullet of radius R draws at scale R / GetMeshRadius(). */
	float GetMeshRadius() const { return MeshRadius; }
	int32 GetInstanceCount() const;
	UInstancedStaticMeshComponent* GetInstances() const { return Instances; }
	/** The floats a new instance takes (NumCustomData of them, DTTracerArt::TailData for a tail). */
	TConstArrayView<float> GetCustomRow() const { return TConstArrayView<float>(CustomData, DataCount); }
	bool IsUsingFallbackMaterial() const { return bFallbackMaterial; }

private:
	/** R-TEC-05 fallback when M_InstanceColor is missing: the engine basic-shape material (no per-instance colour). */
	static UMaterialInterface* Fallback_BulletMaterial();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Instances;

	/** Created by InitTracerHead and InitTail for the Dim parameter. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DimMaterial;

	float MeshRadius = 1.0f;
	bool bFallbackMaterial = false;
	float Dim = 1.0f;
	int32 DataCount = NumCustomData;
	float CustomData[MaxCustomData] = {};
};
