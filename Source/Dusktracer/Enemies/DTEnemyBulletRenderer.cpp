#include "Enemies/DTEnemyBulletRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dusktracer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

namespace DTEnemyBulletRender
{
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	static const TCHAR* MaterialPackage = TEXT("/Game/DT/Materials/M_InstanceColor");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor");
	static const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

ADTEnemyBulletRenderer::ADTEnemyBulletRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	bReplicates = false;

	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Rounds"));
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetGenerateOverlapEvents(false);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetCastShadow(false);
	Instances->NumCustomDataFloats = NumCustomData;
	RootComponent = Instances;
}

UMaterialInterface* ADTEnemyBulletRenderer::Fallback_EnemyFireMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, DTEnemyBulletRender::FallbackMaterialPath);
}

void ADTEnemyBulletRenderer::InitRender(const FLinearColor& Color, float Emissive)
{
	CustomData[0] = Color.R;
	CustomData[1] = Color.G;
	CustomData[2] = Color.B;
	CustomData[3] = Emissive;

	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTEnemyBulletRender::MeshPath))
	{
		Instances->SetStaticMesh(Mesh);
		MeshRadius = FMath::Max(UE_KINDA_SMALL_NUMBER, static_cast<float>(Mesh->GetBounds().BoxExtent.X));
	}
	else
	{
		UE_LOG(LogDT, Warning, TEXT("Enemy round mesh %s missing"), DTEnemyBulletRender::MeshPath);
	}

	UMaterialInterface* Material = nullptr;
	if (FPackageName::DoesPackageExist(DTEnemyBulletRender::MaterialPackage))
	{
		Material = LoadObject<UMaterialInterface>(nullptr, DTEnemyBulletRender::MaterialPath);
	}
	bFallbackMaterial = Material == nullptr;
	if (bFallbackMaterial)
	{
		Material = Fallback_EnemyFireMaterial();
		UE_LOG(LogDT, Log, TEXT("Fallback_EnemyFireMaterial in use: %s not found"), DTEnemyBulletRender::MaterialPath);
	}
	if (Material)
	{
		Instances->SetMaterial(0, Material);
	}
}

int32 ADTEnemyBulletRenderer::GetInstanceCount() const
{
	return Instances->GetInstanceCount();
}

void ADTEnemyBulletRenderer::SetInstances(TConstArrayView<FTransform> Transforms)
{
	const int32 Wanted = Transforms.Num();
	const int32 Have = Instances->GetInstanceCount();
	if (Wanted > Have)
	{
		const TArray<FTransform> Added(Transforms.GetData() + Have, Wanted - Have);
		Instances->AddInstances(Added, false, false, false);
		CustomScratch.SetNumUninitialized((Wanted - Have) * NumCustomData);
		for (int32 Slot = 0; Slot < CustomScratch.Num(); ++Slot)
		{
			CustomScratch[Slot] = CustomData[Slot % NumCustomData];
		}
		Instances->SetCustomData(Have, Wanted - 1, CustomScratch, false);
	}
	else if (Wanted < Have)
	{
		RemoveScratch.Reset();
		for (int32 Index = Have - 1; Index >= Wanted; --Index)
		{
			RemoveScratch.Add(Index);
		}
		Instances->RemoveInstances(RemoveScratch, true);
	}
	if (Wanted > 0)
	{
		Instances->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
	}
	else if (Have > 0)
	{
		Instances->MarkRenderStateDirty();
	}
}
