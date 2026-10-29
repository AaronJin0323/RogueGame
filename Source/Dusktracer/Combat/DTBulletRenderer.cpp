#include "Combat/DTBulletRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dusktracer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

namespace DTBulletRender
{
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	static const TCHAR* MaterialPackage = TEXT("/Game/DT/Materials/M_InstanceColor");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor");
	static const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

ADTBulletRenderer::ADTBulletRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Instances"));
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetGenerateOverlapEvents(false);
	Instances->SetCastShadow(false);
	Instances->NumCustomDataFloats = NumCustomData;
	RootComponent = Instances;
}

UMaterialInterface* ADTBulletRenderer::Fallback_BulletMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, DTBulletRender::FallbackMaterialPath);
}

void ADTBulletRenderer::InitRender(const FLinearColor& Color, float Emissive)
{
	CustomData[0] = Color.R;
	CustomData[1] = Color.G;
	CustomData[2] = Color.B;
	CustomData[3] = Emissive;

	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTBulletRender::MeshPath))
	{
		Instances->SetStaticMesh(Mesh);
		MeshRadius = FMath::Max(KINDA_SMALL_NUMBER, static_cast<float>(Mesh->GetBounds().BoxExtent.X));
	}
	else
	{
		UE_LOG(LogDT, Warning, TEXT("Bullet mesh %s missing"), DTBulletRender::MeshPath);
	}

	UMaterialInterface* Material = nullptr;
	if (FPackageName::DoesPackageExist(DTBulletRender::MaterialPackage))
	{
		Material = LoadObject<UMaterialInterface>(nullptr, DTBulletRender::MaterialPath);
	}
	bFallbackMaterial = Material == nullptr;
	if (bFallbackMaterial)
	{
		Material = Fallback_BulletMaterial();
		UE_LOG(LogDT, Log, TEXT("Fallback_BulletMaterial in use: %s not found"), DTBulletRender::MaterialPath);
	}
	if (Material)
	{
		Instances->SetMaterial(0, Material);
	}
}

int32 ADTBulletRenderer::GetInstanceCount() const
{
	return Instances->GetInstanceCount();
}

void ADTBulletRenderer::SetInstances(TConstArrayView<FTransform> Transforms)
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
		TArray<int32> Removed;
		Removed.Reserve(Have - Wanted);
		for (int32 Index = Have - 1; Index >= Wanted; --Index)
		{
			Removed.Add(Index);
		}
		Instances->RemoveInstances(Removed, true);
	}
	if (Wanted > 0)
	{
		Instances->BatchUpdateInstancesTransforms(0, Transforms, false, true, true);
	}
}
