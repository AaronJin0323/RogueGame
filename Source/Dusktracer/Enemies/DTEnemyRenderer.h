// Transient actor drawing every enemy as two instanced parts (R-TEC-11: enemies are not Actors; enemies.md R-ENM-24):
// a dark hull in Pal.Enemy.Body without emissive and a separate always-lit optic in Pal.Enemy.Optic.
// Each role's hull and optic are that role's own instanced mesh (enemy_art.md R-EAR-01); the boss hull stays the cube.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTElite.h"
#include "DTEnemyRenderer.generated.h"

class UInstancedStaticMeshComponent;

UCLASS(Transient, NotPlaceable)
class DUSKTRACER_API ADTEnemyRenderer : public AActor
{
	GENERATED_BODY()

public:
	static constexpr int32 NumCustomData = 4;
	static constexpr int32 NumRoles = static_cast<int32>(EDTEnemyRole::Count);

	ADTEnemyRenderer();

	/** Loads meshes and material and writes the part colours (custom data 0-2 linear RGB, 3 emissive strength). */
	void Setup(const FLinearColor& HullColor, float HullEmissive, const FLinearColor& OpticColor, float OpticEmissive);

	/**
	 * One hull per enemy and one optic per enemy that carries one, each frame, in one batch per component.
	 * HullTransforms and OpticTransforms are the grey box's: a cube of GetMeshSizeUU scaled to the hit diameter, centred on the sim
	 * plane, and the optic cube on it. Roles names each hull's role; OpticHulls names, for each optic, the index of its hull.
	 * A role mesh is drawn from its hull's transform alone, standing on the floor (R-EAR-02): its optic is modelled in the hull's frame.
	 */
	void UpdateInstances(TConstArrayView<FTransform> HullTransforms, TConstArrayView<EDTEnemyRole> Roles, TConstArrayView<FTransform> OpticTransforms,
		TConstArrayView<int32> OpticHulls);

	/** Elite light and affix parts (elites.md R-ELT-10, R-ELT-11): colours change per instance per frame. */
	void UpdateParts(TConstArrayView<FDTElitePart> InParts);

	/** Edge length of the cube in uu, and the length one unit of a role mesh imports as (scale = size / this). */
	float GetMeshSizeUU() const { return MeshSizeUU; }

	/** The grey box: one cube per enemy and one optic cube per optic, kept in step every frame and drawn only while IsGreyBox (R-EAR-06). */
	UInstancedStaticMeshComponent* GetHull() const { return Hull; }
	UInstancedStaticMeshComponent* GetOptic() const { return Optic; }
	UInstancedStaticMeshComponent* GetParts() const { return Parts; }
	/** What is drawn in play: the role's own hull and optic mesh; the cube for EDTEnemyRole::Boss. */
	UInstancedStaticMeshComponent* GetRoleHull(EDTEnemyRole Kind) const { return RoleHulls[static_cast<int32>(Kind)]; }
	UInstancedStaticMeshComponent* GetRoleOptic(EDTEnemyRole Kind) const { return RoleOptics[static_cast<int32>(Kind)]; }
	bool IsUsingFallbackMaterial() const { return bFallbackMaterial; }
	/** Fallback_EnemyRoleMesh (R-TEC-05): a role mesh is missing, so every enemy draws as the grey box. */
	bool IsUsingFallbackMesh() const { return bFallbackMesh; }
	/** True while the cubes are what is drawn: the mesh fallback, or dt.Exp.EnemyGreyBox. */
	bool IsGreyBox() const { return bGreyBox; }

	/** Asset path of a role's hull or optic mesh; empty for the boss, which has none. */
	static FString RoleMeshPath(EDTEnemyRole Kind, bool bOptic);

private:
	void Sync(UInstancedStaticMeshComponent* Part, TConstArrayView<FTransform> Transforms, const TArray<float>& Data);
	void ShowGreyBox(bool bShow);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Hull;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Optic;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Parts;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> RoleHulls[NumRoles];

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> RoleOptics[NumRoles];

	TArray<float> HullData;
	TArray<float> OpticData;
	TArray<int32> RemoveScratch;
	TArray<FTransform> PartTransforms;
	TArray<float> PartData;
	TArray<FTransform> RoleHullTransforms[NumRoles];
	TArray<FTransform> RoleOpticTransforms[NumRoles];
	float MeshSizeUU = 1.0f;
	bool bFallbackMaterial = false;
	bool bFallbackMesh = false;
	bool bGreyBox = false;
};
