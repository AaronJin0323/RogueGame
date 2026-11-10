#include "Combat/DTBulletRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dusktracer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "Presentation/DTTracerArt.h"

namespace DTBulletRender
{
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Sphere.Sphere");
	static const TCHAR* CueMaterialPackage = TEXT("/Game/DT/Materials/M_CueColor");
	static const TCHAR* CueMaterialPath = TEXT("/Game/DT/Materials/M_CueColor.M_CueColor");
	static const TCHAR* MaterialPackage = TEXT("/Game/DT/Materials/M_InstanceColor");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_InstanceColor.M_InstanceColor");
	static const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	// tracer_art.md: built by Tools/assets/tracer_materials.py.
	static const TCHAR* HeadMaterialPackage = TEXT("/Game/DT/Materials/M_Tracer");
	static const TCHAR* HeadMaterialPath = TEXT("/Game/DT/Materials/M_Tracer.M_Tracer");
	static const TCHAR* TailMaterialPackage = TEXT("/Game/DT/Materials/M_Tracer_Tail");
	static const TCHAR* TailMaterialPath = TEXT("/Game/DT/Materials/M_Tracer_Tail.M_Tracer_Tail");
	static const TCHAR* PlanePath = TEXT("/Engine/BasicShapes/Plane.Plane");
	static const FName DimParameter(TEXT("Dim"));
	constexpr float DimStep = 0.004f; // a smaller change of Dim is not written, so the parameter is not rewritten every frame
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

void ADTBulletRenderer::UseCueMaterial()
{
	if (FPackageName::DoesPackageExist(DTBulletRender::CueMaterialPackage))
	{
		if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, DTBulletRender::CueMaterialPath))
		{
			Instances->SetMaterial(0, Material);
		}
	}
}

void ADTBulletRenderer::InitTracerHead(const FLinearColor& Color, float Emissive)
{
	InitRender(Color, Emissive);
	UMaterialInterface* Material = FPackageName::DoesPackageExist(DTBulletRender::HeadMaterialPackage)
		? LoadObject<UMaterialInterface>(nullptr, DTBulletRender::HeadMaterialPath) : nullptr;
	if (!Material)
	{
		UE_LOG(LogDT, Log, TEXT("Tracer head: %s not found; the round draws on its InitRender material"), DTBulletRender::HeadMaterialPath);
		return;
	}
	DimMaterial = Instances->CreateDynamicMaterialInstance(0, Material);
	bFallbackMaterial = false;
}

bool ADTBulletRenderer::InitTail(const FLinearColor& Body, const FLinearColor& Tip, float Strength)
{
	static_assert(DTTracerArt::TailData <= MaxCustomData, "a tail's custom data fits the renderer's row");
	const float Row[DTTracerArt::TailData] = { Body.R, Body.G, Body.B, Strength, Tip.R, Tip.G, Tip.B };
	DataCount = DTTracerArt::TailData;
	FMemory::Memcpy(CustomData, Row, sizeof(Row));
	Instances->NumCustomDataFloats = DataCount;
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTBulletRender::PlanePath);
	UMaterialInterface* Material = FPackageName::DoesPackageExist(DTBulletRender::TailMaterialPackage)
		? LoadObject<UMaterialInterface>(nullptr, DTBulletRender::TailMaterialPath) : nullptr;
	if (!Mesh || !Material)
	{
		UE_LOG(LogDT, Log, TEXT("Tracer tail: %s or %s not found; tails are not drawn"), DTBulletRender::PlanePath, DTBulletRender::TailMaterialPath);
		return false;
	}
	Instances->SetStaticMesh(Mesh);
	// For a tail this is the plane's full side: a tail of length L draws at X scale L / GetMeshRadius().
	MeshRadius = FMath::Max(KINDA_SMALL_NUMBER, 2.0f * static_cast<float>(Mesh->GetBounds().BoxExtent.X));
	if (!FMath::IsNearlyEqual(MeshRadius, DTTracerArt::PlaneSizeUU, 0.01f))
	{
		UE_LOG(LogDT, Warning, TEXT("Tracer tail: %s is %.1f uu wide, DTTracerArt::PlaneSizeUU says %.1f"), DTBulletRender::PlanePath, MeshRadius, DTTracerArt::PlaneSizeUU);
	}
	DimMaterial = Instances->CreateDynamicMaterialInstance(0, Material);
	return DimMaterial != nullptr;
}

void ADTBulletRenderer::SetDim(float InDim)
{
	if (DimMaterial && InDim != Dim && (FMath::Abs(InDim - Dim) >= DTBulletRender::DimStep || InDim == 1.0f))
	{
		Dim = InDim;
		DimMaterial->SetScalarParameterValue(DTBulletRender::DimParameter, Dim);
	}
}

int32 ADTBulletRenderer::GetInstanceCount() const
{
	return Instances->GetInstanceCount();
}

void ADTBulletRenderer::SetInstances(TConstArrayView<FTransform> Transforms)
{
	DTTracerArt::SyncInstances(Instances, Transforms, GetCustomRow());
}
