#include "Presentation/DTHeroFigure.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTPaletteGen.h"
#include "Dusktracer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace DTHeroFigureLocal
{
	// Asset path and layout (not tunables): the instance-colour material of the arena (custom data 0-2 RGB, 3 emissive).
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor");
	constexpr int32 NumCustomFloats = 4;
	// The armour is lit by the scene, not self-lit (R-HAR-04): the emissive layer is held out of the silhouette grade.
	constexpr float Emissive = 0.0f;
}

UDTHeroFigureComponent::UDTHeroFigureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
}

bool UDTHeroFigureComponent::Build()
{
	if (bBuilt)
	{
		return true;
	}
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, DTHeroFigureLocal::MaterialPath);
	Parts.Reset();
	Parts.SetNum(DTHeroPose::NumParts);
	bool bAll = true;
	for (int32 Index = 0; Index < DTHeroPose::NumParts; ++Index)
	{
		const EDTHeroPart Part = static_cast<EDTHeroPart>(Index);
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTHeroPose::MeshPath(Part));
		if (!Mesh)
		{
			UE_LOG(LogDT, Warning, TEXT("Hero mesh %s missing; that part will not render (run git lfs checkout in a fresh worktree)"), DTHeroPose::MeshPath(Part));
			bAll = false;
			continue;
		}
		UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(Owner, *FString::Printf(TEXT("HeroPart_%s"), DTHeroPose::PartName(Part)), RF_Transient);
		Instances->SetMobility(EComponentMobility::Movable);
		Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Instances->SetGenerateOverlapEvents(false);
		Instances->SetCanEverAffectNavigation(false);
		Instances->NumCustomDataFloats = DTHeroFigureLocal::NumCustomFloats;
		Instances->SetStaticMesh(Mesh);
		// M_InstanceColor ships with the arena (S0); without it the mesh keeps its own material and the part draws untinted.
		if (Material)
		{
			Instances->SetMaterial(0, Material);
		}
		Instances->SetupAttachment(this);
		Instances->RegisterComponent();
		Instances->AddInstance(FTransform::Identity, false);
		const FLinearColor Colour = DTPal::Linear(DTHeroPose::Token(Part));
		Instances->SetCustomData(0, { Colour.R, Colour.G, Colour.B, DTHeroFigureLocal::Emissive }, true);
		Parts[Index] = Instances;
	}
	bBuilt = bAll;
	return bBuilt;
}

void UDTHeroFigureComponent::Apply(const FDTHeroPose& Pose)
{
	for (int32 Index = 0; Index < Parts.Num(); ++Index)
	{
		if (UInstancedStaticMeshComponent* Instances = Parts[Index])
		{
			Instances->SetRelativeTransform(Pose.Parts[Index]);
		}
	}
}

UInstancedStaticMeshComponent* UDTHeroFigureComponent::GetPart(EDTHeroPart Part) const
{
	const int32 Index = static_cast<int32>(Part);
	return Parts.IsValidIndex(Index) ? Parts[Index].Get() : nullptr;
}
