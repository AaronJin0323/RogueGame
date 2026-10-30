#include "Enemies/DTEnemyRenderer.h"
#include "Dusktracer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

namespace DTEnemyRender
{
	// Asset paths (not tunables). M_InstanceColor reads per-instance custom data: 0-2 linear RGB, 3 emissive strength.
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor");
	static const TCHAR* Fallback_EnemyMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

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
	}

	HullData = { HullColor.R, HullColor.G, HullColor.B, HullEmissive };
	OpticData = { OpticColor.R, OpticColor.G, OpticColor.B, OpticEmissive };
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

void ADTEnemyRenderer::UpdateInstances(TConstArrayView<FTransform> HullTransforms, TConstArrayView<FTransform> OpticTransforms)
{
	Sync(Hull, HullTransforms, HullData);
	Sync(Optic, OpticTransforms, OpticData);
}
