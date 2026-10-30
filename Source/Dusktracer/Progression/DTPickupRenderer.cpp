#include "Progression/DTPickupRenderer.h"
#include "Dusktracer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UObjectGlobals.h"

namespace DTPickupRender
{
	// Asset paths (not tunables). M_InstanceColor reads per-instance custom data: 0-2 linear RGB, 3 emissive strength.
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Cube.Cube");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor");
	constexpr int32 NumCustomFloats = 4;

	static UInstancedStaticMeshComponent* MakeInstances(AActor* Owner, const TCHAR* Name)
	{
		UInstancedStaticMeshComponent* Instances = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Instances->SetMobility(EComponentMobility::Movable);
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetGenerateOverlapEvents(false);
		Instances->SetCanEverAffectNavigation(false);
		Instances->SetCastShadow(false);
		Instances->NumCustomDataFloats = NumCustomFloats;
		return Instances;
	}

	static TArray<float> Custom(const FColor& Token)
	{
		const FLinearColor Color = DTPal::Linear(Token);
		return { Color.R, Color.G, Color.B, DTConst::PICKUP_EMISSIVE };
	}
}

ADTPickupRenderer::ADTPickupRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	bReplicates = false;

	Gems = DTPickupRender::MakeInstances(this, TEXT("Gems"));
	RootComponent = Gems;
	Chests = DTPickupRender::MakeInstances(this, TEXT("Chests"));
	Chests->SetupAttachment(Gems);
}

void ADTPickupRenderer::Setup()
{
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTPickupRender::MeshPath);
	if (Mesh)
	{
		Gems->SetStaticMesh(Mesh);
		Chests->SetStaticMesh(Mesh);
		MeshSizeUU = FMath::Max(UE_KINDA_SMALL_NUMBER, 2.0f * static_cast<float>(Mesh->GetBounds().BoxExtent.X));
	}
	else
	{
		UE_LOG(LogDT, Warning, TEXT("Pickup mesh %s missing; gems and chests will not render"), DTPickupRender::MeshPath);
	}
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, DTPickupRender::MaterialPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		Gems->SetMaterial(0, Material);
		Chests->SetMaterial(0, Material);
	}
	else
	{
		UE_LOG(LogDT, Warning, TEXT("Pickup material %s missing; gems and chests draw untinted"), DTPickupRender::MaterialPath);
	}
	GemCustom = DTPickupRender::Custom(DTPal::Pickup_Gem);
	ChestCustom = DTPickupRender::Custom(DTPal::Pickup_Chest);
}

void ADTPickupRenderer::UpdateGems(TConstArrayView<FTransform> Transforms)
{
	Update(Gems, GemCustom, Transforms);
}

void ADTPickupRenderer::UpdateChests(TConstArrayView<FTransform> Transforms)
{
	Update(Chests, ChestCustom, Transforms);
}

void ADTPickupRenderer::Update(UInstancedStaticMeshComponent* Instances, const TArray<float>& Custom, TConstArrayView<FTransform> Transforms)
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
			Instances->SetCustomData(Index, Custom, false);
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
