#include "Enemies/DTEnemyBulletRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dusktracer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Misc/PackageName.h"
#include "Settings/DTUserSettings.h"

namespace DTEnemyBulletRender
{
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	static const TCHAR* MaterialPackage = TEXT("/Game/DT/Materials/M_EnemyRound");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_EnemyRound.M_EnemyRound");
	static const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	// Danger outlines (settings.md R-SET-10): a flat cylinder, unlit, so its colour stays achromatic.
	static const TCHAR* DiscMeshPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	static const TCHAR* DiscMaterialPackage = TEXT("/Game/DT/Materials/M_CueColor");
	static const TCHAR* DiscMaterialPath = TEXT("/Game/DT/Materials/M_CueColor.M_CueColor");
	constexpr float DiscHeightUU = 1.0f;
	constexpr float DiscStepUU = 1.0f; // the lighter disc sits this much over the darker
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

	Outlines = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Outlines"));
	Outlines->SetupAttachment(Instances);
	Outlines->SetMobility(EComponentMobility::Movable);
	Outlines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Outlines->SetGenerateOverlapEvents(false);
	Outlines->SetCanEverAffectNavigation(false);
	Outlines->SetCastShadow(false);
	Outlines->NumCustomDataFloats = NumOutlineData;
}

UMaterialInterface* ADTEnemyBulletRenderer::Fallback_EnemyFireMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, DTEnemyBulletRender::FallbackMaterialPath);
}

void ADTEnemyBulletRenderer::InitRender(const FLinearColor& Rim, float Emissive, const FLinearColor& Core)
{
	CustomData[0] = Rim.R;
	CustomData[1] = Rim.G;
	CustomData[2] = Rim.B;
	CustomData[3] = Emissive;
	CustomData[4] = Core.R;
	CustomData[5] = Core.G;
	CustomData[6] = Core.B;

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

	if (UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, DTEnemyBulletRender::DiscMeshPath))
	{
		Outlines->SetStaticMesh(Disc);
		DiscMeshRadius = FMath::Max(UE_KINDA_SMALL_NUMBER, static_cast<float>(Disc->GetBounds().BoxExtent.X));
		DiscMeshHeight = FMath::Max(UE_KINDA_SMALL_NUMBER, 2.0f * static_cast<float>(Disc->GetBounds().BoxExtent.Z));
	}
	if (FPackageName::DoesPackageExist(DTEnemyBulletRender::DiscMaterialPackage))
	{
		Outlines->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, DTEnemyBulletRender::DiscMaterialPath));
	}
}

int32 ADTEnemyBulletRenderer::GetInstanceCount() const
{
	return Instances->GetInstanceCount();
}

int32 ADTEnemyBulletRenderer::GetOutlineCount() const
{
	return Outlines->GetInstanceCount();
}

void ADTEnemyBulletRenderer::SetOutlines(TConstArrayView<FTransform> Rounds)
{
	using namespace DTEnemyBulletRender;
	const int32 Wanted = DTSettings::DangerOutline() ? 2 * Rounds.Num() : 0;
	const int32 Have = Outlines->GetInstanceCount();
	if (Wanted == 0 && Have == 0)
	{
		return;
	}
	DiscScratch.Reset(Wanted);
	if (Wanted > 0)
	{
		// Flat under the round, dark first so the light disc lies on it.
		const float Ratios[] = { DTConst::SETTINGS_OUTLINE_ROUND_DARK_RATIO, DTConst::SETTINGS_OUTLINE_ROUND_RATIO };
		const float ZScale = DiscHeightUU / DiscMeshHeight;
		for (const FTransform& Round : Rounds)
		{
			const float Radius = static_cast<float>(Round.GetScale3D().X) * MeshRadius;
			const FVector Under = Round.GetLocation() - FVector(0.0, 0.0, Radius);
			for (int32 Layer = 0; Layer < 2; ++Layer)
			{
				const float Scale = Radius * Ratios[Layer] / DiscMeshRadius;
				DiscScratch.Add(FTransform(FQuat::Identity, Under + FVector(0.0, 0.0, Layer * DiscStepUU), FVector(Scale, Scale, ZScale)));
			}
		}
	}
	if (Wanted > Have)
	{
		const TArray<FTransform> Added(DiscScratch.GetData() + Have, Wanted - Have);
		Outlines->AddInstances(Added, false, false, false);
		const FLinearColor Dark = DTPal::Linear(DTPal::Assist_Dark);
		const FLinearColor Light = DTPal::Linear(DTPal::Assist_Light);
		CustomScratch.Reset();
		for (int32 Index = Have; Index < Wanted; ++Index)
		{
			const FLinearColor& Color = Index % 2 == 0 ? Dark : Light;
			CustomScratch.Append({ Color.R, Color.G, Color.B, 1.0f });
		}
		Outlines->SetCustomData(Have, Wanted - 1, CustomScratch, false);
	}
	else if (Wanted < Have)
	{
		RemoveScratch.Reset();
		for (int32 Index = Have - 1; Index >= Wanted; --Index)
		{
			RemoveScratch.Add(Index);
		}
		Outlines->RemoveInstances(RemoveScratch, true);
	}
	if (Wanted > 0)
	{
		Outlines->BatchUpdateInstancesTransforms(0, DiscScratch, false, true, true);
	}
	else
	{
		Outlines->MarkRenderStateDirty();
	}
}

void ADTEnemyBulletRenderer::SetInstances(TConstArrayView<FTransform> Transforms)
{
	SetOutlines(Transforms);
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
