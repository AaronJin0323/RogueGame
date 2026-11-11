#include "Presentation/DTImpactRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Dusktracer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"

namespace DTImpactRender
{
	static const TCHAR* PlaneMeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	static const TCHAR* SparkPackage = TEXT("/Game/DT/Materials/M_Fx_Spark");
	static const TCHAR* FlashPackage = TEXT("/Game/DT/Materials/M_Fx_Flash");
	static const TCHAR* RingPackage = TEXT("/Game/DT/Materials/M_Fx_Ring");
	static const TCHAR* DebrisPackage = TEXT("/Game/DT/Materials/M_Fx_Debris");
	static const TCHAR* ShardMeshes[DTImpactArt::NumShards] = { TEXT("/Game/DT/Meshes/SM_Fx_Debris_A"), TEXT("/Game/DT/Meshes/SM_Fx_Debris_B"), TEXT("/Game/DT/Meshes/SM_Fx_Debris_C") };
	static const FName NowParameter(TEXT("Now"));
	static const FName StreakParameter(TEXT("StreakRatio"));
	/**
	 * Bounds of a component, over those of its instances: the materials slide every vertex down its camera ray (R-IMP-04), a few percent of
	 * the camera distance, and a plane is drawn inside its own square; twice the instances' bounds holds both at every framing of the arena.
	 */
	constexpr float SlideBoundsScale = 2.0f;

	static FString ObjectPath(const TCHAR* Package)
	{
		return FString::Printf(TEXT("%s.%s"), Package, *FPackageName::GetShortName(Package));
	}

	template <typename T>
	static T* LoadIfThere(const TCHAR* Package)
	{
		return FPackageName::DoesPackageExist(Package) ? LoadObject<T>(nullptr, *ObjectPath(Package)) : nullptr;
	}

	/** Rotation about +Z that turns +X onto the unit direction Dir. */
	static FQuat YawTo(const FVector2f& Dir)
	{
		return FQuat(FVector::UpVector, FMath::Atan2(static_cast<double>(Dir.Y), static_cast<double>(Dir.X)));
	}

	static const FTransform& Hidden()
	{
		static const FTransform Zero(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector);
		return Zero;
	}
}

ADTImpactRenderer::ADTImpactRenderer()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);
	bReplicates = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	const auto MakePart = [this](const TCHAR* PartName, int32 Data)
	{
		UInstancedStaticMeshComponent* Part = CreateDefaultSubobject<UInstancedStaticMeshComponent>(PartName);
		Part->SetupAttachment(Root);
		Part->SetMobility(EComponentMobility::Movable);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false); // R-IMP-04: debris casts no shadow either
		Part->NumCustomDataFloats = Data;
		Part->SetBoundsScale(DTImpactRender::SlideBoundsScale);
		return Part;
	};
	Sparks = MakePart(TEXT("Sparks"), DTImpactArt::SparkData);
	Flashes = MakePart(TEXT("Flashes"), DTImpactArt::FlashData);
	Rings = MakePart(TEXT("Rings"), DTImpactArt::RingData);
	const TCHAR* ShardNames[DTImpactArt::NumShards] = { TEXT("DebrisA"), TEXT("DebrisB"), TEXT("DebrisC") };
	for (int32 Shard = 0; Shard < DTImpactArt::NumShards; ++Shard)
	{
		Debris.Add(MakePart(ShardNames[Shard], DTImpactArt::DebrisData));
	}
}

void ADTImpactRenderer::InitRender()
{
	using namespace DTImpactRender;
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, PlaneMeshPath);
	if (Plane)
	{
		const float Side = 2.0f * static_cast<float>(Plane->GetBounds().BoxExtent.X);
		if (!FMath::IsNearlyEqual(Side, DTImpactArt::PlaneSizeUU, 0.01f))
		{
			UE_LOG(LogDT, Warning, TEXT("Impact art: %s is %.1f uu wide, DTImpactArt::PlaneSizeUU says %.1f"), PlaneMeshPath, Side, DTImpactArt::PlaneSizeUU);
		}
	}
	const auto SetUp = [this, Plane](UInstancedStaticMeshComponent* Part, const TCHAR* Package, TObjectPtr<UMaterialInstanceDynamic>& Out)
	{
		UMaterialInterface* Material = LoadIfThere<UMaterialInterface>(Package);
		if (!Plane || !Material)
		{
			UE_LOG(LogDT, Log, TEXT("Impact art: %s not found; that effect is not drawn"), Package);
			return false;
		}
		Part->SetStaticMesh(Plane);
		Out = UMaterialInstanceDynamic::Create(Material, this);
		Part->SetMaterial(0, Out);
		return true;
	};
	bSpark = SetUp(Sparks, SparkPackage, SparkMaterial);
	if (SparkMaterial)
	{
		// R-IMP-05: the streaks' width at the root comes from the constant, so tuning it needs no asset rebuild.
		SparkMaterial->SetScalarParameterValue(StreakParameter, static_cast<float>(DTConst::IMP_SPARK_STREAK_RATIO));
	}
	bFlash = SetUp(Flashes, FlashPackage, FlashMaterial);
	bRing = SetUp(Rings, RingPackage, RingMaterial);

	UMaterialInterface* DebrisLook = LoadIfThere<UMaterialInterface>(DebrisPackage);
	bDebris = DebrisLook != nullptr;
	for (int32 Shard = 0; Shard < DTImpactArt::NumShards; ++Shard)
	{
		UStaticMesh* Mesh = LoadIfThere<UStaticMesh>(ShardMeshes[Shard]);
		if (!Mesh)
		{
			bDebris = false;
			UE_LOG(LogDT, Log, TEXT("Impact art: %s not found; debris is not drawn"), ShardMeshes[Shard]);
			continue;
		}
		Debris[Shard]->SetStaticMesh(Mesh);
		if (DebrisLook)
		{
			Debris[Shard]->SetMaterial(0, DebrisLook);
		}
	}
	if (!DebrisLook)
	{
		UE_LOG(LogDT, Log, TEXT("Impact art: %s not found; debris is not drawn"), DebrisPackage);
	}
}

float ADTImpactRenderer::GetSparkStreakRatio() const
{
	float Value = -1.0f;
	return SparkMaterial && SparkMaterial->GetScalarParameterValue(FMaterialParameterInfo(DTImpactRender::StreakParameter), Value) ? Value : -1.0f;
}

int32 ADTImpactRenderer::CountShown(const UInstancedStaticMeshComponent* Part)
{
	int32 Count = 0;
	if (Part)
	{
		FTransform At;
		for (int32 Index = 0; Index < Part->GetInstanceCount(); ++Index)
		{
			Part->GetInstanceTransform(Index, At);
			Count += At.GetScale3D().IsNearlyZero() ? 0 : 1;
		}
	}
	return Count;
}

void ADTImpactRenderer::EnsureSlots(UInstancedStaticMeshComponent* Part, int32 Count, TArray<FTransform>& Scratch)
{
	const int32 Have = Part->GetInstanceCount();
	if (Count <= Have)
	{
		return;
	}
	Scratch.Reset();
	Scratch.Init(DTImpactRender::Hidden(), Count - Have); // growth only: a component never shrinks while its world lives
	Part->AddInstances(Scratch, false, false, false);
}

void ADTImpactRenderer::Hide(UInstancedStaticMeshComponent* Part, int32 Slot)
{
	if (Slot < Part->GetInstanceCount())
	{
		Part->UpdateInstanceTransform(Slot, DTImpactRender::Hidden(), false, true, true);
	}
}

void ADTImpactRenderer::SetNow(UMaterialInstanceDynamic* Material, double NowSeconds)
{
	if (Material)
	{
		Material->SetScalarParameterValue(DTImpactRender::NowParameter, static_cast<float>(NowSeconds));
	}
}

void ADTImpactRenderer::Draw(FDTImpactEffects& Effects, double NowSeconds, TConstArrayView<FVector2f> Heroes)
{
	WriteSparks(Effects);
	WriteFlashes(Effects);
	WriteRings(Effects, Heroes);
	WriteDebris(Effects, NowSeconds);
	// One parameter write per component per frame, only while it shows something: a frame with nothing live touches no component.
	if (Effects.Sparks.NumLive > 0)
	{
		SetNow(SparkMaterial, NowSeconds);
	}
	if (Effects.Flashes.NumLive > 0)
	{
		SetNow(FlashMaterial, NowSeconds);
	}
	if (Effects.NumRingsLive() > 0)
	{
		SetNow(RingMaterial, NowSeconds);
	}
}

void ADTImpactRenderer::WriteSparks(FDTImpactEffects& Effects)
{
	TDTFxSlots<FDTFxSpark>& Slots = Effects.Sparks;
	if (Slots.Born.Num() == 0 && Slots.Died.Num() == 0)
	{
		return;
	}
	if (!bSpark)
	{
		Slots.Born.Reset();
		Slots.Died.Reset();
		return;
	}
	for (const int32 Slot : Slots.Died)
	{
		if (!Slots.IsLive(Slot))
		{
			Hide(Sparks, Slot);
		}
	}
	EnsureSlots(Sparks, Slots.NumSlots(), AddScratch);
	const FLinearColor Core = DTPal::Linear(DTPal::Player_Core);
	const float Length = static_cast<float>(DTConst::IMP_SPARK_LENGTH_UU);
	const float Width = static_cast<float>(DTConst::IMP_SPARK_WIDTH_UU);
	const float InvPlane = 1.0f / DTImpactArt::PlaneSizeUU;
	const double Z = DTConst::SIM_PLANE_Z_UU;
	for (const int32 Slot : Slots.Born)
	{
		if (!Slots.IsLive(Slot))
		{
			continue;
		}
		const FDTFxSpark& Spark = Slots.Items[Slot];
		// The quad's -X edge at the contact point, opening along Dir: the material draws the fan from there at its full size and shrinks it by age.
		const FVector2f Mid = Spark.Contact + Spark.Dir * (0.5f * Length);
		Sparks->UpdateInstanceTransform(Slot, FTransform(DTImpactRender::YawTo(Spark.Dir), FVector(Mid.X, Mid.Y, Z), FVector(Length * InvPlane, Width * InvPlane, 1.0f)), false, true, true);
		const float Row[DTImpactArt::SparkData] = { Core.R, Core.G, Core.B, static_cast<float>(DTConst::IMP_SPARK_EMISSIVE),
			static_cast<float>(static_cast<double>(Spark.Born) / DTConst::SIM_FIXED_HZ) };
		Sparks->SetCustomData(Slot, Row, true);
	}
	Slots.Born.Reset();
	Slots.Died.Reset();
}

void ADTImpactRenderer::WriteFlashes(FDTImpactEffects& Effects)
{
	TDTFxSlots<FDTFxFlash>& Slots = Effects.Flashes;
	if (Slots.Born.Num() == 0 && Slots.Died.Num() == 0)
	{
		return;
	}
	if (!bFlash)
	{
		Slots.Born.Reset();
		Slots.Died.Reset();
		return;
	}
	for (const int32 Slot : Slots.Died)
	{
		if (!Slots.IsLive(Slot))
		{
			Hide(Flashes, Slot);
		}
	}
	EnsureSlots(Flashes, Slots.NumSlots(), AddScratch);
	const FLinearColor Core = DTPal::Linear(DTPal::Player_Core);
	const float InvPlane = 1.0f / DTImpactArt::PlaneSizeUU;
	const double Z = DTConst::SIM_PLANE_Z_UU;
	const float Widest = static_cast<float>(DTConst::IMP_FLASH_RADIUS_MAX_UU);
	for (const int32 Slot : Slots.Born)
	{
		if (!Slots.IsLive(Slot))
		{
			continue;
		}
		const FDTFxFlash& Flash = Slots.Items[Slot];
		// The plane is the flash's final disc (never wider than C.IMP.FLASH_RADIUS_MAX_UU); the material grows the disc inside it by age.
		const DTImpactArt::FDTFlashShape Final = DTImpactArt::FlashAt(Flash.Drawn, static_cast<float>(DTConst::IMP_KILL_FLASH_S), Flash.bAllowFlash);
		const float Radius = FMath::Max(Final.Radius, 1.0f);
		const float Side = 2.0f * Radius * InvPlane;
		Flashes->UpdateInstanceTransform(Slot, FTransform(FQuat::Identity, FVector(Flash.Centre.X, Flash.Centre.Y, Z), FVector(Side, Side, 1.0f)), false, true, true);
		const float Start = Flash.bAllowFlash ? FMath::Min(Widest, Flash.Drawn * static_cast<float>(DTConst::IMP_KILL_FLASH_START_RATIO)) / Radius : 1.0f;
		const float End = Flash.bAllowFlash ? Flash.Drawn * static_cast<float>(DTConst::IMP_KILL_FLASH_END_RATIO) / Radius : 1.0f;
		const float Emissive = static_cast<float>(DTConst::IMP_KILL_FLASH_EMISSIVE) * (Flash.bAllowFlash ? 1.0f : static_cast<float>(DTConst::IMP_REDUCED_EMISSIVE_RATIO));
		const float Row[DTImpactArt::FlashData] = { Core.R, Core.G, Core.B, Emissive, static_cast<float>(static_cast<double>(Flash.Born) / DTConst::SIM_FIXED_HZ),
			Start, End, Flash.bAllowFlash ? 1.0f : 0.0f };
		Flashes->SetCustomData(Slot, Row, true);
	}
	Slots.Born.Reset();
	Slots.Died.Reset();
}

void ADTImpactRenderer::WriteRings(FDTImpactEffects& Effects, TConstArrayView<FVector2f> Heroes)
{
	if (!bRing)
	{
		Effects.RingsBorn.Reset();
		Effects.RingsDied.Reset();
		return;
	}
	if (Effects.RingsBorn.Num() == 0 && Effects.RingsDied.Num() == 0 && Effects.NumRingsLive() == 0)
	{
		return;
	}
	for (const int32 Player : Effects.RingsDied)
	{
		if (!Effects.RingLive.IsValidIndex(Player) || !Effects.RingLive[Player])
		{
			Hide(Rings, Player);
		}
	}
	EnsureSlots(Rings, Effects.Rings.Num(), AddScratch);
	const FLinearColor Muzzle = DTPal::Linear(DTPal::Enemy_Muzzle);
	const float Hero = static_cast<float>(DTConst::HERO_RADIUS_UU);
	const float End = static_cast<float>(DTConst::IMP_HERO_FLASH_END_RATIO);
	const float Outer = Hero * End;
	for (const int32 Player : Effects.RingsBorn)
	{
		if (!Effects.RingLive.IsValidIndex(Player) || !Effects.RingLive[Player])
		{
			continue;
		}
		const FDTFxRing& Ring = Effects.Rings[Player];
		const float Start = Ring.bAllowFlash ? static_cast<float>(DTConst::IMP_HERO_FLASH_START_RATIO) / End : 1.0f;
		const float Emissive = static_cast<float>(DTConst::IMP_HERO_FLASH_EMISSIVE) * (Ring.bAllowFlash ? 1.0f : static_cast<float>(DTConst::IMP_REDUCED_EMISSIVE_RATIO));
		const float Row[DTImpactArt::RingData] = { Muzzle.R, Muzzle.G, Muzzle.B, Emissive, static_cast<float>(static_cast<double>(Ring.Born) / DTConst::SIM_FIXED_HZ),
			Start, static_cast<float>(DTConst::IMP_HERO_FLASH_THICK_UU) / Outer, Ring.bAllowFlash ? 1.0f : 0.0f };
		Rings->SetCustomData(Player, Row, true);
	}
	// The ring follows its hero: its centre is written each frame while it shows (at most one per player).
	const float Side = 2.0f * Outer / DTImpactArt::PlaneSizeUU;
	for (int32 Player = 0; Player < Effects.RingLive.Num(); ++Player)
	{
		if (Effects.RingLive[Player] && Heroes.IsValidIndex(Player))
		{
			const FVector2f At = Heroes[Player];
			Rings->UpdateInstanceTransform(Player, FTransform(FQuat::Identity, FVector(At.X, At.Y, DTConst::SIM_PLANE_Z_UU), FVector(Side, Side, 1.0f)), false, true, true);
		}
	}
	Effects.RingsBorn.Reset();
	Effects.RingsDied.Reset();
}

void ADTImpactRenderer::WriteDebris(FDTImpactEffects& Effects, double NowSeconds)
{
	TDTFxSlots<FDTFxPiece>& Slots = Effects.Pieces;
	const bool bAny = Slots.NumLive > 0;
	Slots.Born.Reset();
	Slots.Died.Reset();
	if (!bDebris || (!bAny && !bDebrisShown))
	{
		return;
	}
	// A piece's path is a closed form of its age (DTImpactArt::PieceAt); the live pieces' transforms are written each frame, the rest hidden.
	const int32 Count = Slots.NumSlots();
	const double Z = DTConst::SIM_PLANE_Z_UU;
	for (int32 Shard = 0; Shard < DTImpactArt::NumShards; ++Shard)
	{
		DebrisScratch[Shard].SetNumUninitialized(Count, EAllowShrinking::No);
		for (FTransform& T : DebrisScratch[Shard])
		{
			T = DTImpactRender::Hidden();
		}
	}
	for (int32 Slot = 0; Slot < Count; ++Slot)
	{
		if (!Slots.IsLive(Slot))
		{
			continue;
		}
		const FDTFxPiece& Piece = Slots.Items[Slot];
		const float Age = DTImpactArt::AgeSeconds(Piece.Born, NowSeconds);
		const DTImpactArt::FDTPieceAt At = DTImpactArt::PieceAt(Piece.Piece, Age);
		if (At.bAlive && At.Scale > 0.0f)
		{
			DebrisScratch[Piece.Piece.Shard][Slot] = FTransform(FQuat(FVector::UpVector, At.Yaw), FVector(At.Position.X, At.Position.Y, Z), FVector(At.Scale));
		}
	}
	const FLinearColor Body = DTPal::Linear(DTPal::Enemy_Body);
	for (int32 Shard = 0; Shard < DTImpactArt::NumShards; ++Shard)
	{
		UInstancedStaticMeshComponent* Part = Debris[Shard];
		const int32 Have = Part->GetInstanceCount();
		if (Count > Have)
		{
			AddScratch.Reset();
			AddScratch.Append(DebrisScratch[Shard].GetData() + Have, Count - Have);
			Part->AddInstances(AddScratch, false, false, false);
			DataScratch.Reset();
			for (int32 Index = Have; Index < Count; ++Index)
			{
				DataScratch.Append({ Body.R, Body.G, Body.B });
			}
			Part->SetCustomData(Have, Count - 1, DataScratch, false);
		}
		if (Count > 0)
		{
			Part->BatchUpdateInstancesTransforms(0, DebrisScratch[Shard], false, true, true);
		}
	}
	bDebrisShown = bAny;
}
