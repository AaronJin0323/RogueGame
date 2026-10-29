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
	constexpr int32 NumCustomFloats = 4;
}

ADTEnemyRenderer::ADTEnemyRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	bReplicates = false;

	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetGenerateOverlapEvents(false);
	Instances->SetCanEverAffectNavigation(false);
	Instances->NumCustomDataFloats = DTEnemyRender::NumCustomFloats;
	RootComponent = Instances;
}

void ADTEnemyRenderer::Setup(const FLinearColor& InstanceColor, float EmissiveStrength)
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTEnemyRender::MeshPath);
	if (Mesh)
	{
		Instances->SetStaticMesh(Mesh);
		MeshSizeUU = FMath::Max(UE_KINDA_SMALL_NUMBER, 2.0f * static_cast<float>(Mesh->GetBounds().BoxExtent.X));
	}
	else
	{
		UE_LOG(LogDT, Warning, TEXT("Enemy mesh %s missing; enemies will not render"), DTEnemyRender::MeshPath);
	}

	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, DTEnemyRender::MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!Material)
	{
		// Fallback_EnemyMaterial (R-TEC-05): until M_InstanceColor exists, enemies draw untinted.
		UE_LOG(LogDT, Warning, TEXT("Fallback_EnemyMaterial: %s missing, using %s"), DTEnemyRender::MaterialPath, DTEnemyRender::Fallback_EnemyMaterialPath);
		Material = LoadObject<UMaterialInterface>(nullptr, DTEnemyRender::Fallback_EnemyMaterialPath);
	}
	if (Material)
	{
		Instances->SetMaterial(0, Material);
	}

	CustomData = { InstanceColor.R, InstanceColor.G, InstanceColor.B, EmissiveStrength };
}

void ADTEnemyRenderer::UpdateInstances(TConstArrayView<FTransform> Transforms)
{
	const int32 Want = Transforms.Num();
	const int32 Have = Instances->GetInstanceCount();

	if (Want < Have)
	{
		// Drop from the tail: transforms are rewritten every frame, so which instances go does not matter.
		RemoveScratch.Reset();
		for (int32 Index = Have - 1; Index >= Want; --Index)
		{
			RemoveScratch.Add(Index);
		}
		Instances->RemoveInstances(RemoveScratch, true);
	}
	else if (Want > Have)
	{
		const TArray<FTransform> Added(Transforms.Slice(Have, Want - Have));
		Instances->AddInstances(Added, false, true, false);
		for (int32 Index = Have; Index < Want; ++Index)
		{
			Instances->SetCustomData(Index, CustomData, false);
		}
	}

	if (Want > 0)
	{
		Instances->BatchUpdateInstancesTransforms(0, Transforms, true, true, false);
	}
	else if (Have > 0)
	{
		Instances->MarkRenderStateDirty();
	}
}
