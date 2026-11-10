#include "Enemies/DTEnemyRenderer.h"
#include "Dusktracer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Engine/StaticMesh.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

namespace DTEnemyRender
{
	// Asset paths (not tunables). M_InstanceColor reads per-instance custom data: 0-2 linear RGB, 3 emissive strength.
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor");
	static const TCHAR* Fallback_EnemyMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	// Role keys, never fiction (AGENTS non-negotiable 6): SM_Enemy_<Role> and SM_Enemy_<Role>_Optic (09_Catalog/licenses.csv). The boss has no mesh.
	static const TCHAR* RoleKeys[] = { TEXT("Swarm"), TEXT("Tank"), TEXT("Rusher"), TEXT("Turret"), nullptr };
	static_assert(UE_ARRAY_COUNT(RoleKeys) == ADTEnemyRenderer::NumRoles, "one mesh key per role");
	static const TCHAR* RoleHullNames[] = { TEXT("HullSwarm"), TEXT("HullTank"), TEXT("HullRusher"), TEXT("HullTurret"), TEXT("HullBoss") };
	static const TCHAR* RoleOpticNames[] = { TEXT("OpticSwarm"), TEXT("OpticTank"), TEXT("OpticRusher"), TEXT("OpticTurret"), TEXT("OpticBoss") };
	static_assert(UE_ARRAY_COUNT(RoleHullNames) == ADTEnemyRenderer::NumRoles && UE_ARRAY_COUNT(RoleOpticNames) == ADTEnemyRenderer::NumRoles, "one component per role");

#if !UE_BUILD_SHIPPING
	static TAutoConsoleVariable<int32> CVarGreyBox(TEXT("dt.Exp.EnemyGreyBox"), 0,
		TEXT("Experiment, expires 2027-01-31: 1 draws every enemy as the cube and optic cube the role meshes replaced (enemy_art.md R-EAR-06), for a before and after on one build"));
#endif

	static UInstancedStaticMeshComponent* MakePart(AActor* Owner, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Part = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->NumCustomDataFloats = ADTEnemyRenderer::NumCustomData;
		return Part;
	}
}

FString ADTEnemyRenderer::RoleMeshPath(EDTEnemyRole Kind, bool bOptic)
{
	const int32 Index = static_cast<int32>(Kind);
	if (Index < 0 || Index >= NumRoles || !DTEnemyRender::RoleKeys[Index])
	{
		return FString();
	}
	const FString Name = FString::Printf(TEXT("SM_Enemy_%s%s"), DTEnemyRender::RoleKeys[Index], bOptic ? TEXT("_Optic") : TEXT(""));
	return FString::Printf(TEXT("/Game/DT/Meshes/%s.%s"), *Name, *Name);
}

ADTEnemyRenderer::ADTEnemyRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	bReplicates = false;

	Hull = DTEnemyRender::MakePart(this, TEXT("Hull"));
	RootComponent = Hull;
	Optic = DTEnemyRender::MakePart(this, TEXT("Optic"));
	Optic->SetupAttachment(Hull);
	Optic->SetCastShadow(false);
	Parts = DTEnemyRender::MakePart(this, TEXT("Parts"));
	Parts->SetupAttachment(Hull);
	Parts->SetCastShadow(false);
	for (int32 Kind = 0; Kind < NumRoles; ++Kind)
	{
		RoleHulls[Kind] = DTEnemyRender::MakePart(this, DTEnemyRender::RoleHullNames[Kind]);
		RoleHulls[Kind]->SetupAttachment(Hull);
		RoleOptics[Kind] = DTEnemyRender::MakePart(this, DTEnemyRender::RoleOpticNames[Kind]);
		RoleOptics[Kind]->SetupAttachment(Hull);
		RoleOptics[Kind]->SetCastShadow(false);
	}
	// The grey box is kept in step and not drawn; Setup shows it if a role mesh is missing.
	Hull->SetVisibility(false, false);
	Optic->SetVisibility(false, false);
}

void ADTEnemyRenderer::Setup(const FLinearColor& HullColor, float HullEmissive, const FLinearColor& OpticColor, float OpticEmissive)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTEnemyRender::MeshPath);
	if (Mesh)
	{
		Hull->SetStaticMesh(Mesh);
		Optic->SetStaticMesh(Mesh);
		Parts->SetStaticMesh(Mesh);
		MeshSizeUU = FMath::Max(UE_KINDA_SMALL_NUMBER, 2.0f * static_cast<float>(Mesh->GetBounds().BoxExtent.X));
	}
	else
	{
		UE_LOG(LogDT, Warning, TEXT("Enemy mesh %s missing; enemies will not render"), DTEnemyRender::MeshPath);
	}

	bFallbackMesh = false;
	for (int32 Kind = 0; Kind < NumRoles; ++Kind)
	{
		const FString HullPath = RoleMeshPath(static_cast<EDTEnemyRole>(Kind), false);
		const FString OpticPath = RoleMeshPath(static_cast<EDTEnemyRole>(Kind), true);
		// A role without a mesh of its own keeps the cube: the boss hull (enemy_art.md R-EAR-01).
		UStaticMesh* RoleHull = HullPath.IsEmpty() ? Mesh : LoadObject<UStaticMesh>(nullptr, *HullPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		UStaticMesh* RoleOptic = OpticPath.IsEmpty() ? Mesh : LoadObject<UStaticMesh>(nullptr, *OpticPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!RoleHull || !RoleOptic)
		{
			// Fallback_EnemyRoleMesh (R-TEC-05): one missing role mesh puts every enemy back in the grey box, so no role draws at another grade.
			UE_LOG(LogDT, Warning, TEXT("Fallback_EnemyRoleMesh: %s missing, enemies draw as %s"), RoleHull ? *OpticPath : *HullPath, DTEnemyRender::MeshPath);
			bFallbackMesh = true;
			continue;
		}
		RoleHulls[Kind]->SetStaticMesh(RoleHull);
		RoleOptics[Kind]->SetStaticMesh(RoleOptic);
	}

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, DTEnemyRender::MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	bFallbackMaterial = Material == nullptr;
	if (!Material)
	{
		// Fallback_EnemyMaterial (R-TEC-05): until M_InstanceColor exists, enemies draw untinted.
		UE_LOG(LogDT, Warning, TEXT("Fallback_EnemyMaterial: %s missing, using %s"), DTEnemyRender::MaterialPath, DTEnemyRender::Fallback_EnemyMaterialPath);
		Material = LoadObject<UMaterialInterface>(nullptr, DTEnemyRender::Fallback_EnemyMaterialPath);
	}
	if (Material)
	{
		Hull->SetMaterial(0, Material);
		Optic->SetMaterial(0, Material);
		Parts->SetMaterial(0, Material);
		for (int32 Kind = 0; Kind < NumRoles; ++Kind)
		{
			RoleHulls[Kind]->SetMaterial(0, Material);
			RoleOptics[Kind]->SetMaterial(0, Material);
		}
	}

	HullData = { HullColor.R, HullColor.G, HullColor.B, HullEmissive };
	OpticData = { OpticColor.R, OpticColor.G, OpticColor.B, OpticEmissive };
	ShowGreyBox(bFallbackMesh);
}

void ADTEnemyRenderer::ShowGreyBox(bool bShow)
{
	bGreyBox = bShow;
	Hull->SetVisibility(bShow, false);
	Optic->SetVisibility(bShow, false);
	for (int32 Kind = 0; Kind < NumRoles; ++Kind)
	{
		RoleHulls[Kind]->SetVisibility(!bShow, false);
		RoleOptics[Kind]->SetVisibility(!bShow, false);
	}
}

void ADTEnemyRenderer::Sync(UInstancedStaticMeshComponent* Part, TConstArrayView<FTransform> Transforms, const TArray<float>& Data)
{
	const int32 Want = Transforms.Num();
	const int32 Have = Part->GetInstanceCount();

	if (Want < Have)
	{
		// Drop from the tail: transforms are rewritten every frame, so which instances go does not matter.
		RemoveScratch.Reset();
		for (int32 Index = Have - 1; Index >= Want; --Index)
		{
			RemoveScratch.Add(Index);
		}
		Part->RemoveInstances(RemoveScratch, true);
	}
	else if (Want > Have)
	{
		const TArray<FTransform> Added(Transforms.Slice(Have, Want - Have));
		Part->AddInstances(Added, false, true, false);
		for (int32 Index = Have; Index < Want; ++Index)
		{
			Part->SetCustomData(Index, Data, false);
		}
	}

	if (Want > 0)
	{
		Part->BatchUpdateInstancesTransforms(0, Transforms, true, true, false);
	}
	else if (Have > 0)
	{
		Part->MarkRenderStateDirty();
	}
}

void ADTEnemyRenderer::UpdateParts(TConstArrayView<FDTElitePart> InParts)
{
	PartTransforms.Reset();
	for (const FDTElitePart& Part : InParts)
	{
		PartTransforms.Add(Part.Transform);
	}
	PartData.SetNumZeroed(NumCustomData);
	Sync(Parts, PartTransforms, PartData);
	for (int32 Index = 0; Index < InParts.Num(); ++Index)
	{
		const FDTElitePart& Part = InParts[Index];
		PartData[0] = Part.Color.R;
		PartData[1] = Part.Color.G;
		PartData[2] = Part.Color.B;
		PartData[3] = Part.Emissive;
		Parts->SetCustomData(Index, PartData, false);
	}
	if (InParts.Num() > 0)
	{
		Parts->MarkRenderStateDirty();
	}
}

void ADTEnemyRenderer::UpdateInstances(TConstArrayView<FTransform> HullTransforms, TConstArrayView<EDTEnemyRole> Roles, TConstArrayView<FTransform> OpticTransforms,
	TConstArrayView<int32> OpticHulls)
{
	check(Roles.Num() == HullTransforms.Num() && OpticHulls.Num() == OpticTransforms.Num());
	bool bWantGreyBox = bFallbackMesh;
#if !UE_BUILD_SHIPPING
	bWantGreyBox = bWantGreyBox || DTEnemyRender::CVarGreyBox.GetValueOnGameThread() != 0;
#endif
	if (bWantGreyBox != bGreyBox)
	{
		ShowGreyBox(bWantGreyBox);
	}

	// The grey box: every hull and optic as the cube, as before the role meshes.
	Sync(Hull, HullTransforms, HullData);
	Sync(Optic, OpticTransforms, OpticData);

	// What is drawn: each enemy in its role's component. A role mesh stands on the floor, the sim plane's height under the cube's centre
	// (R-EAR-02); its optic is modelled in the hull's frame, so both take the hull's transform. The boss keeps the cube and its optic cube.
	const double PlaneZ = static_cast<double>(DTConst::SIM_PLANE_Z_UU);
	const int32 Boss = static_cast<int32>(EDTEnemyRole::Boss);
	for (int32 Kind = 0; Kind < NumRoles; ++Kind)
	{
		RoleHullTransforms[Kind].Reset();
		RoleOpticTransforms[Kind].Reset();
	}
	auto Standing = [PlaneZ](const FTransform& Box)
	{
		FTransform Out = Box;
		Out.AddToTranslation(FVector(0.0, 0.0, -PlaneZ));
		return Out;
	};
	for (int32 Index = 0; Index < HullTransforms.Num(); ++Index)
	{
		const int32 Kind = static_cast<int32>(Roles[Index]);
		RoleHullTransforms[Kind].Add(Kind == Boss ? HullTransforms[Index] : Standing(HullTransforms[Index]));
	}
	for (int32 Index = 0; Index < OpticTransforms.Num(); ++Index)
	{
		const int32 HullIndex = OpticHulls[Index];
		const int32 Kind = static_cast<int32>(Roles[HullIndex]);
		RoleOpticTransforms[Kind].Add(Kind == Boss ? OpticTransforms[Index] : Standing(HullTransforms[HullIndex]));
	}
	for (int32 Kind = 0; Kind < NumRoles; ++Kind)
	{
		Sync(RoleHulls[Kind], RoleHullTransforms[Kind], HullData);
		Sync(RoleOptics[Kind], RoleOpticTransforms[Kind], OpticData);
	}
}
