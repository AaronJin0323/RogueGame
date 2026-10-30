#include "Enemies/DTTelegraphRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Dusktracer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

namespace DTTelegraphRender
{
	// Asset paths (not tunables). The engine plane is a flat square in XY facing +Z.
	static const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	static const TCHAR* MaterialPackage = TEXT("/Game/DT/Materials/M_CueColor");
	static const TCHAR* MaterialPath = TEXT("/Game/DT/Materials/M_CueColor.M_CueColor");
	static const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	/** One quad centred at Center, its local X along Axis, XSize by YSize uu. */
	static FTransform Quad(const FVector2f& Center, const FVector2f& Axis, float XSize, float YSize, float MeshSize, float Z)
	{
		const float Yaw = Axis.IsNearlyZero() ? 0.0f : FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X));
		return FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Center.X, Center.Y, Z),
			FVector(FMath::Max(0.0f, XSize) / MeshSize, FMath::Max(0.0f, YSize) / MeshSize, 1.0f));
	}

	static bool IsFilledRing(const FDTTelegraph& T) { return T.Width <= 0.0f || T.Width >= T.Length; }

	static int32 ArcSegments(const FDTTelegraph& T)
	{
		const float Degrees = FMath::Clamp(T.Width, 0.0f, 360.0f);
		return FMath::Max(1, FMath::CeilToInt32(ADTTelegraphRenderer::RingSegments * Degrees / 360.0f));
	}

	static bool IsLaneShape(const FDTTelegraph& T) { return T.Shape == EDTTelegraphShape::RingGap || T.Shape == EDTTelegraphShape::SpiralLane; }

	/** Degrees a lane shape draws: the full turn minus its lane. */
	static float LaneShapeSpan(const FDTTelegraph& T) { return 360.0f - FMath::Clamp(T.Width, 0.0f, 360.0f); }

	static int32 LaneShapeSegments(const FDTTelegraph& T)
	{
		return FMath::Max(1, FMath::CeilToInt32(ADTTelegraphRenderer::RingSegments * LaneShapeSpan(T) / 360.0f));
	}
}

ADTTelegraphRenderer::ADTTelegraphRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	bReplicates = false;

	Instances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Telegraphs"));
	Instances->SetMobility(EComponentMobility::Movable);
	Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Instances->SetGenerateOverlapEvents(false);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetCastShadow(false); // projected light: it lights the ground, it casts nothing
	Instances->NumCustomDataFloats = NumCustomData;
	RootComponent = Instances;
}

UMaterialInterface* ADTTelegraphRenderer::Fallback_TelegraphMaterial()
{
	return LoadObject<UMaterialInterface>(nullptr, DTTelegraphRender::FallbackMaterialPath);
}

void ADTTelegraphRenderer::InitRender(const FLinearColor& Color, float Emissive)
{
	CustomData[0] = Color.R;
	CustomData[1] = Color.G;
	CustomData[2] = Color.B;
	CustomData[3] = Emissive;

	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, DTTelegraphRender::MeshPath))
	{
		Instances->SetStaticMesh(Mesh);
		MeshSizeUU = FMath::Max(UE_KINDA_SMALL_NUMBER, 2.0f * static_cast<float>(Mesh->GetBounds().BoxExtent.X));
	}
	else
	{
		UE_LOG(LogDT, Warning, TEXT("Telegraph mesh %s missing; telegraphs will not render"), DTTelegraphRender::MeshPath);
	}

	UMaterialInterface* Material = nullptr;
	if (FPackageName::DoesPackageExist(DTTelegraphRender::MaterialPackage))
	{
		Material = LoadObject<UMaterialInterface>(nullptr, DTTelegraphRender::MaterialPath);
	}
	bFallbackMaterial = Material == nullptr;
	if (bFallbackMaterial)
	{
		Material = Fallback_TelegraphMaterial();
		UE_LOG(LogDT, Log, TEXT("Fallback_TelegraphMaterial in use: %s not found"), DTTelegraphRender::MaterialPath);
	}
	if (Material)
	{
		Instances->SetMaterial(0, Material);
	}
}

int32 ADTTelegraphRenderer::InstancesFor(const FDTTelegraph& Telegraph)
{
	switch (Telegraph.Shape)
	{
	case EDTTelegraphShape::Ring:
		return RingSegments;
	case EDTTelegraphShape::Arc:
		return DTTelegraphRender::ArcSegments(Telegraph);
	case EDTTelegraphShape::RingGap:
	case EDTTelegraphShape::SpiralLane:
		return DTTelegraphRender::LaneShapeSegments(Telegraph);
	case EDTTelegraphShape::HazardRing:
		return RingSegments;
	default: // Band, AimLine
		return 1;
	}
}

void ADTTelegraphRenderer::AppendTransforms(const FDTTelegraph& T, float MeshSize, float Z, TArray<FTransform>& Out)
{
	using namespace DTTelegraphRender;
	const FVector2f Dir = T.Dir.IsNearlyZero() ? FVector2f(1.0f, 0.0f) : T.Dir.GetSafeNormal();
	if (T.Shape == EDTTelegraphShape::Band || T.Shape == EDTTelegraphShape::AimLine)
	{
		// The charge corridor or the aim line: from the origin along the direction, its full length and width.
		Out.Add(Quad(T.Origin + Dir * (0.5f * T.Length), Dir, T.Length, T.Width, MeshSize, Z));
		return;
	}

	if (IsLaneShape(T))
	{
		// The disc minus the lane: filled sectors from the far edge of the lane round to its near edge.
		const int32 Segments = InstancesFor(T);
		const float Span = FMath::DegreesToRadians(LaneShapeSpan(T));
		const float Step = Span / static_cast<float>(Segments);
		const float Tangent = 2.0f * T.Length * FMath::Tan(0.5f * Step);
		const float Start = FMath::Atan2(Dir.Y, Dir.X) + 0.5f * FMath::DegreesToRadians(FMath::Clamp(T.Width, 0.0f, 360.0f));
		for (int32 Segment = 0; Segment < Segments; ++Segment)
		{
			const float Angle = Start + (static_cast<float>(Segment) + 0.5f) * Step;
			const FVector2f Radial(FMath::Cos(Angle), FMath::Sin(Angle));
			Out.Add(Quad(T.Origin + Radial * (0.5f * T.Length), Radial, T.Length, Tangent, MeshSize, Z));
		}
		return;
	}
	if (T.Shape == EDTTelegraphShape::HazardRing)
	{
		// Outside the circle: each quad runs from the circle (its inner edge tangent to it, so nothing is drawn inside)
		// out past the arena corners, as wide as the outer rim's tangent so neighbours meet.
		const float Outer = FMath::Max(T.Length, static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU) * UE_SQRT_2);
		const float Thickness = Outer - T.Length;
		const float Step = UE_TWO_PI / static_cast<float>(RingSegments);
		const float Tangent = 2.0f * Outer * FMath::Tan(0.5f * Step);
		for (int32 Segment = 0; Segment < RingSegments; ++Segment)
		{
			const float Angle = (static_cast<float>(Segment) + 0.5f) * Step;
			const FVector2f Radial(FMath::Cos(Angle), FMath::Sin(Angle));
			Out.Add(Quad(T.Origin + Radial * (T.Length + 0.5f * Thickness), Radial, Thickness, Tangent, MeshSize, Z));
		}
		return;
	}

	const bool bArc = T.Shape == EDTTelegraphShape::Arc;
	const int32 Segments = InstancesFor(T);
	const float Span = bArc ? FMath::DegreesToRadians(FMath::Clamp(T.Width, 0.0f, 360.0f)) : UE_TWO_PI;
	const float Step = Span / static_cast<float>(Segments);
	const float Thickness = bArc || IsFilledRing(T) ? T.Length : T.Width;
	const float Mid = T.Length - 0.5f * Thickness;
	// Each segment is a quad radial-thick and as wide as the outer chord's tangent, so neighbours meet at the rim.
	const float Tangent = 2.0f * T.Length * FMath::Tan(0.5f * Step);
	const float Start = FMath::Atan2(Dir.Y, Dir.X) - (bArc ? 0.5f * Span : 0.0f);
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const float Angle = Start + (static_cast<float>(Segment) + 0.5f) * Step;
		const FVector2f Radial(FMath::Cos(Angle), FMath::Sin(Angle));
		Out.Add(Quad(T.Origin + Radial * Mid, Radial, Thickness, Tangent, MeshSize, Z));
	}
}

int32 ADTTelegraphRenderer::GetInstanceCount() const
{
	return Instances->GetInstanceCount();
}

void ADTTelegraphRenderer::SetTelegraphs(TConstArrayView<FDTTelegraph> Telegraphs)
{
	Transforms.Reset();
	for (const FDTTelegraph& Telegraph : Telegraphs)
	{
		AppendTransforms(Telegraph, MeshSizeUU, DrawZ, Transforms);
	}

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
