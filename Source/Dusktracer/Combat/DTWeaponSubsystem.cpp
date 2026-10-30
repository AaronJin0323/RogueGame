#include "Combat/DTWeaponSubsystem.h"
#include "Combat/DTBulletSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Dusktracer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/World.h"

int32 DTWeaponPalette::NumBuckets()
{
	return WallBucket() + 1;
}

int32 DTWeaponPalette::WallBucket()
{
	return 2 + static_cast<int32>(EDTPath::Count);
}

FColor DTWeaponPalette::CoreToken()
{
	return DTPal::Player_Core;
}

FColor DTWeaponPalette::ZoneToken(EDTPath Path)
{
	switch (Path)
	{
	case EDTPath::Storm: return DTPal::Keeper_Storm;
	case EDTPath::Forge: return DTPal::Keeper_Forge;
	case EDTPath::Tide: return DTPal::Keeper_Tide;
	case EDTPath::Shade: return DTPal::Keeper_Shade;
	case EDTPath::Radiance: return DTPal::Keeper_Radiance;
	case EDTPath::Quicksilver: return DTPal::Keeper_Quicksilver;
	default: return DTPal::Player_TrailNeutral;
	}
}

FColor DTWeaponPalette::BucketToken(int32 Bucket)
{
	if (Bucket <= 0)
	{
		return CoreToken();
	}
	if (Bucket == WallBucket())
	{
		return DTPal::Keeper_Tide;
	}
	return Bucket == 1 ? ZoneToken(EDTPath::None) : ZoneToken(static_cast<EDTPath>(Bucket - 2));
}

int32 DTWeaponPalette::BucketOf(const FDTWeaponVisual& Visual)
{
	if (Visual.Kind == FDTWeaponVisual::EKind::Drone || Visual.Kind == FDTWeaponVisual::EKind::Mine)
	{
		return 0;
	}
	if (Visual.Kind == FDTWeaponVisual::EKind::FrozenWall)
	{
		return WallBucket();
	}
	return Visual.Path == EDTPath::None || Visual.Path >= EDTPath::Count ? 1 : 2 + static_cast<int32>(Visual.Path);
}

bool DTWeaponCues::IsCue(const FDTWeaponVisual& Visual)
{
	switch (Visual.Kind)
	{
	case FDTWeaponVisual::EKind::StillAura:
	case FDTWeaponVisual::EKind::StillCharge:
	case FDTWeaponVisual::EKind::FrozenHold:
	case FDTWeaponVisual::EKind::FrozenPlain:
		return true;
	default:
		return false;
	}
}

FColor DTWeaponCues::Token(int32 Bucket)
{
	switch (Bucket)
	{
	case Hold: return DTPal::Player_Core;
	case Frozen: return DTPal::Player_TrailNeutral;
	default: return DTPal::Keeper_Tide;
	}
}

float DTWeaponCues::Emissive(int32 Bucket)
{
	return static_cast<float>(Bucket == Ground ? DTConst::PRE_CUE_HUE_EMISSIVE : DTConst::PRE_CUE_EMISSIVE);
}

float DTWeaponCues::DrawZ(int32 Bucket)
{
	return static_cast<float>(Bucket == Ground ? DTConst::PRE_CUE_Z_UU : DTConst::SIM_PLANE_Z_UU);
}

int32 DTWeaponCues::BucketOf(const FDTWeaponVisual& Visual)
{
	switch (Visual.Kind)
	{
	case FDTWeaponVisual::EKind::FrozenHold: return Hold;
	case FDTWeaponVisual::EKind::FrozenPlain: return Frozen;
	default: return Ground;
	}
}

namespace DTWeaponCuesLocal
{
	/** A closed outline of straight bars through Corners (offsets from Center). */
	static void AppendOutline(const FVector2f& Center, TConstArrayView<FVector2f> Corners, float Width, TArray<FDTTelegraph>& Out)
	{
		for (int32 Side = 0; Side < Corners.Num(); ++Side)
		{
			const FVector2f From = Corners[Side];
			const FVector2f To = Corners[(Side + 1) % Corners.Num()];
			FDTTelegraph& Bar = Out.AddDefaulted_GetRef();
			Bar.Shape = EDTTelegraphShape::Band;
			Bar.Origin = Center + From;
			Bar.Dir = (To - From).GetSafeNormal();
			Bar.Length = (To - From).Size();
			Bar.Width = Width;
		}
	}

	/** The Stillness dial: one ring per charge held, then the charge being gathered as ticks round the next ring. */
	static void AppendCharge(const FDTWeaponVisual& Visual, TArray<FDTTelegraph>& Out)
	{
		const float Width = static_cast<float>(DTConst::PRE_CHARGE_RING_UU);
		const float Gap = static_cast<float>(DTConst::PRE_CHARGE_RING_GAP_UU);
		for (int32 Index = 0; Index < Visual.Charges; ++Index)
		{
			FDTTelegraph& Ring = Out.AddDefaulted_GetRef();
			Ring.Shape = EDTTelegraphShape::Ring;
			Ring.Origin = Visual.Position;
			Ring.Length = Visual.Radius + Index * Gap;
			Ring.Width = Width;
		}
		const int32 Ticks = FMath::FloorToInt32(Visual.Charge01 * DTConst::PRE_CHARGE_TICKS);
		const float Radius = Visual.Radius + Visual.Charges * Gap - 0.5f * Width;
		const float Step = UE_TWO_PI / static_cast<float>(DTConst::PRE_CHARGE_TICKS);
		const float TickLength = Radius * Step * static_cast<float>(DTConst::PRE_CHARGE_TICK_FILL);
		for (int32 Tick = 0; Tick < Ticks; ++Tick)
		{
			float S = 0.0f;
			float C = 1.0f;
			FMath::SinCos(&S, &C, Tick * Step);
			const FVector2f Radial(C, S);
			const FVector2f Tangent(-S, C);
			FDTTelegraph& Bar = Out.AddDefaulted_GetRef();
			Bar.Shape = EDTTelegraphShape::Band;
			Bar.Origin = Visual.Position + Radial * Radius - Tangent * (0.5f * TickLength);
			Bar.Dir = Tangent;
			Bar.Length = TickLength;
			Bar.Width = Width;
		}
	}
}

void DTWeaponCues::Append(const FDTWeaponVisual& Visual, TArray<FDTTelegraph>& Out)
{
	using namespace DTWeaponCuesLocal;
	switch (Visual.Kind)
	{
	case FDTWeaponVisual::EKind::StillAura:
	{
		FDTTelegraph& Ring = Out.AddDefaulted_GetRef();
		Ring.Shape = EDTTelegraphShape::Ring;
		Ring.Origin = Visual.Position;
		Ring.Length = Visual.Radius;
		Ring.Width = static_cast<float>(DTConst::PRE_AURA_RING_UU) + Visual.Pulse01 * static_cast<float>(DTConst::PRE_AURA_PULSE_UU);
		break;
	}
	case FDTWeaponVisual::EKind::StillCharge:
		AppendCharge(Visual, Out);
		break;
	case FDTWeaponVisual::EKind::FrozenHold:
	{
		// A square pinned around the hull.
		const float Half = Visual.Radius * DTConst::PRE_HOLD_FRAME_RATIO;
		const FVector2f Corners[] = { FVector2f(-Half, -Half), FVector2f(Half, -Half), FVector2f(Half, Half), FVector2f(-Half, Half) };
		AppendOutline(Visual.Position, Corners, static_cast<float>(DTConst::PRE_HOLD_FRAME_UU), Out);
		break;
	}
	case FDTWeaponVisual::EKind::FrozenPlain:
	{
		// A diamond: the square turned 45 degrees, clear of the hull's corners.
		const float Half = Visual.Radius * DTConst::PRE_FROZEN_CUE_RATIO;
		const FVector2f Corners[] = { FVector2f(Half, 0.0f), FVector2f(0.0f, Half), FVector2f(-Half, 0.0f), FVector2f(0.0f, -Half) };
		AppendOutline(Visual.Position, Corners, static_cast<float>(DTConst::PRE_FROZEN_CUE_UU), Out);
		break;
	}
	default:
		break;
	}
}

void FDTWeaponPhaseAdapter::SimStep(const FDTSimContext& Context)
{
	if (Owner)
	{
		Owner->StepPhase(Phase, Context);
	}
}

bool UDTWeaponSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTWeaponSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDTSimSubsystem>();
	Collection.InitializeDependency<UDTBulletSubsystem>();
	BulletsPhase.Owner = this;
	BulletsPhase.Phase = EDTSimPhase::Bullets;
	EffectsPhase.Owner = this;
	EffectsPhase.Phase = EDTSimPhase::Effects;
	// keepers.md R-KEE-23: the Patron hand opens before any console command runs, so a launch names its Keepers up front.
	FString UnlockArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("-DTUnlock="), UnlockArg))
	{
		TArray<FString> Ids;
		UnlockArg.ParseIntoArray(Ids, TEXT(","));
		for (const FString& Id : Ids)
		{
			Unlock(FName(*Id));
		}
	}
}

void UDTWeaponSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UDTSimSubsystem* SimSys = InWorld.GetSubsystem<UDTSimSubsystem>();
	UDTBulletSubsystem* BulletSys = InWorld.GetSubsystem<UDTBulletSubsystem>();
	Bullets = BulletSys;
	if (UDTEnemySubsystem* EnemySys = InWorld.GetSubsystem<UDTEnemySubsystem>())
	{
		Sim.SetEnemies(&EnemySys->GetPool());
	}
	if (!SimSys)
	{
		return;
	}
	SimSubsystem = SimSys;
	SimSys->Register(EDTSimPhase::Weapons, this);
	// The observer must run after the pool stepped: registering the bullet system first is a no-op when it already
	// registered itself (AddUnique) and otherwise fixes the order inside the phase.
	if (BulletSys)
	{
		SimSys->Register(EDTSimPhase::Bullets, BulletSys);
	}
	SimSys->Register(EDTSimPhase::Bullets, &BulletsPhase);
	SimSys->Register(EDTSimPhase::Effects, &EffectsPhase);
}

void UDTWeaponSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSys = SimSubsystem.Get())
	{
		SimSys->Unregister(this);
		SimSys->Unregister(&BulletsPhase);
		SimSys->Unregister(&EffectsPhase);
	}
	SimSubsystem.Reset();
	Bullets.Reset();
	Renderers.Reset();
	CueRenderers.Reset();
	Super::Deinitialize();
}

FDTRunState* UDTWeaponSubsystem::GetRun() const
{
	UDTSimSubsystem* SimSys = SimSubsystem.Get();
	if (!SimSys)
	{
		if (const UWorld* World = GetWorld())
		{
			SimSys = World->GetSubsystem<UDTSimSubsystem>();
		}
	}
	return SimSys ? &SimSys->GetRun() : nullptr;
}

void UDTWeaponSubsystem::SimStep(const FDTSimContext& Context)
{
	UDTBulletSubsystem* BulletSys = Bullets.Get();
	Sim.StepWeapons(Context, BulletSys ? &BulletSys->GetPool() : nullptr);
}

void UDTWeaponSubsystem::StepPhase(EDTSimPhase Phase, const FDTSimContext& Context)
{
	if (Phase == EDTSimPhase::Bullets)
	{
		Sim.ObserveBullets(Context);
	}
	else if (Phase == EDTSimPhase::Effects)
	{
		UDTBulletSubsystem* BulletSys = Bullets.Get();
		Sim.StepEffects(Context, BulletSys ? &BulletSys->GetPool() : nullptr);
	}
}

EDTWeaponResult UDTWeaponSubsystem::Give(int32 OwnerId, FName WeaponId, int32 Level)
{
	FDTRunState* Run = GetRun();
	return Run ? Sim.Give(*Run, OwnerId, WeaponId, Level) : EDTWeaponResult::BadPlayer;
}

EDTWeaponResult UDTWeaponSubsystem::LevelUp(int32 OwnerId, FName WeaponId, int32 Level)
{
	FDTRunState* Run = GetRun();
	return Run ? Sim.LevelUp(*Run, OwnerId, FDTWeaponSim::FindSlot(*Run, OwnerId, WeaponId), Level) : EDTWeaponResult::BadPlayer;
}

EDTWeaponResult UDTWeaponSubsystem::Socket(int32 OwnerId, FName WeaponId, FName ModId)
{
	FDTRunState* Run = GetRun();
	return Run ? Sim.Socket(*Run, OwnerId, FDTWeaponSim::FindSlot(*Run, OwnerId, WeaponId), ModId) : EDTWeaponResult::BadPlayer;
}

bool UDTWeaponSubsystem::IsInPool(FName Id) const
{
	const int32 Weapon = DTWeapons::FindWeapon(Id);
	if (Weapon != INDEX_NONE)
	{
		return DTCat::Weapons[Weapon].bStarter || Unlocked.Contains(DTWeapons::WeaponId(Weapon));
	}
	const int32 Mod = DTWeapons::FindModifier(Id);
	if (Mod != INDEX_NONE)
	{
		return DTCat::Modifiers[Mod].bStarter || Unlocked.Contains(DTWeapons::ModifierId(Mod));
	}
	return Unlocked.Contains(Id); // a Keeper.<Path> (keepers.md R-KEE-23)
}

int32 UDTWeaponSubsystem::GetRendererCount() const
{
	int32 Count = 0;
	for (const TObjectPtr<ADTBulletRenderer>& Renderer : Renderers)
	{
		Count += Renderer ? 1 : 0;
	}
	return Count;
}

ADTBulletRenderer* UDTWeaponSubsystem::EnsureRenderer(int32 Bucket)
{
	if (Renderers.Num() < DTWeaponPalette::NumBuckets())
	{
		Renderers.SetNum(DTWeaponPalette::NumBuckets());
	}
	if (!Renderers[Bucket])
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADTBulletRenderer* Renderer = World->SpawnActor<ADTBulletRenderer>(ADTBulletRenderer::StaticClass(), FTransform::Identity, Params);
		if (Renderer)
		{
			// Cores glow like player rounds; zones stay at plain token brightness so they never outshine the fire.
			const float Emissive = Bucket == 0 ? DTConst::BULLETS_EMISSIVE_PLAYER : (Bucket == DTWeaponPalette::WallBucket() ? DTConst::PRE_CUE_HUE_EMISSIVE : 1.0f);
			Renderer->InitRender(DTPal::Linear(DTWeaponPalette::BucketToken(Bucket)), Emissive);
			if (Bucket == DTWeaponPalette::WallBucket())
			{
				Renderer->UseCueMaterial();
			}
		}
		Renderers[Bucket] = Renderer;
	}
	return Renderers[Bucket];
}

ADTTelegraphRenderer* UDTWeaponSubsystem::EnsureCueRenderer(int32 Bucket)
{
	CueRenderers.SetNum(DTWeaponCues::NumBuckets);
	if (!CueRenderers[Bucket])
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return nullptr;
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADTTelegraphRenderer* Renderer = World->SpawnActor<ADTTelegraphRenderer>(ADTTelegraphRenderer::StaticClass(), FTransform::Identity, Params);
		if (Renderer)
		{
			Renderer->InitRender(DTPal::Linear(DTWeaponCues::Token(Bucket)), DTWeaponCues::Emissive(Bucket));
			Renderer->SetDrawZ(DTWeaponCues::DrawZ(Bucket));
			Renderer->SetEdged(false);
		}
		CueRenderers[Bucket] = Renderer;
	}
	return CueRenderers[Bucket];
}

void UDTWeaponSubsystem::SimRender(float Alpha)
{
	UDTSimSubsystem* SimSys = SimSubsystem.Get();
	if (!SimSys)
	{
		return;
	}
	Visuals.Reset();
	Sim.GatherVisuals(SimSys->GetRun(), SimSys->GetView(), Visuals);
	const int32 Buckets = DTWeaponPalette::NumBuckets();
	BucketTransforms.SetNum(Buckets);
	for (TArray<FTransform>& Transforms : BucketTransforms)
	{
		Transforms.Reset();
	}
	for (TArray<FDTTelegraph>& Shapes : CueShapes)
	{
		Shapes.Reset();
	}
	const double Z = DTConst::SIM_PLANE_Z_UU;
	const double ZoneZ = DTConst::PRE_ZONE_Z_UU;
	const float ZoneHalf = static_cast<float>(DTConst::PRE_ZONE_HALF_UU);
	for (const FDTWeaponVisual& Visual : Visuals)
	{
		if (DTWeaponCues::IsCue(Visual))
		{
			DTWeaponCues::Append(Visual, CueShapes[DTWeaponCues::BucketOf(Visual)]);
			continue;
		}
		TArray<FTransform>& Out = BucketTransforms[DTWeaponPalette::BucketOf(Visual)];
		switch (Visual.Kind)
		{
		case FDTWeaponVisual::EKind::Drone:
		case FDTWeaponVisual::EKind::Mine:
			Out.Add(FTransform(FQuat::Identity, FVector(Visual.Position.X, Visual.Position.Y, Z), FVector(Visual.Radius)));
			break;
		case FDTWeaponVisual::EKind::MineZone:
		case FDTWeaponVisual::EKind::Field:
			// A flat disc on the ground, under the cues and telegraphs (R-PRE-11).
			Out.Add(FTransform(FQuat::Identity, FVector(Visual.Position.X, Visual.Position.Y, ZoneZ), FVector(Visual.Radius, Visual.Radius, ZoneHalf)));
			break;
		case FDTWeaponVisual::EKind::Marked:
		{
			const float Plate = Visual.Radius * DTConst::PRE_WALL_PLATE_RATIO;
			Out.Add(FTransform(FQuat::Identity, FVector(Visual.Position.X, Visual.Position.Y, ZoneZ), FVector(Plate, Plate, ZoneHalf)));
			break;
		}
		case FDTWeaponVisual::EKind::FrozenWall:
		{
			// A plate in the Tide zone colour under the hull: the same flat disc as a field, wider than the hull's corners.
			const float Plate = Visual.Radius * DTConst::PRE_WALL_PLATE_RATIO;
			Out.Add(FTransform(FQuat::Identity, FVector(Visual.Position.X, Visual.Position.Y, ZoneZ), FVector(Plate, Plate, ZoneHalf)));
			break;
		}
		case FDTWeaponVisual::EKind::StillAura:
		case FDTWeaponVisual::EKind::StillCharge:
		case FDTWeaponVisual::EKind::FrozenHold:
		case FDTWeaponVisual::EKind::FrozenPlain:
			break; // drawn as cues above
		case FDTWeaponVisual::EKind::Arc:
		{
			// Beads along the sweep's edge, one hero radius apart: reads as a cool arc, never a filled wedge.
			const float Bead = static_cast<float>(DTConst::HERO_RADIUS_UU);
			const float ArcLen = FMath::DegreesToRadians(Visual.ArcDeg) * Visual.Radius;
			const int32 Beads = FMath::Max(2, FMath::CeilToInt(ArcLen / Bead) + 1);
			for (int32 Index = 0; Index < Beads; ++Index)
			{
				const float T = static_cast<float>(Index) / static_cast<float>(Beads - 1);
				const float Angle = FMath::DegreesToRadians((T - 0.5f) * Visual.ArcDeg);
				float S = 0.0f;
				float C = 1.0f;
				FMath::SinCos(&S, &C, Angle);
				const FVector2f Dir(Visual.Dir.X * C - Visual.Dir.Y * S, Visual.Dir.X * S + Visual.Dir.Y * C);
				const FVector2f P = Visual.Position + Dir * Visual.Radius;
				Out.Add(FTransform(FQuat::Identity, FVector(P.X, P.Y, Z), FVector(0.5f * Bead, 0.5f * Bead, 1.0f)));
			}
			break;
		}
		}
	}
	for (int32 Bucket = 0; Bucket < Buckets; ++Bucket)
	{
		TArray<FTransform>& Transforms = BucketTransforms[Bucket];
		ADTBulletRenderer* Renderer = Transforms.Num() > 0 ? EnsureRenderer(Bucket) : (Renderers.IsValidIndex(Bucket) ? Renderers[Bucket].Get() : nullptr);
		if (!Renderer)
		{
			continue;
		}
		// Sizes above are in uu; the shared mesh has its own radius.
		const float InvMesh = 1.0f / Renderer->GetMeshRadius();
		for (FTransform& Transform : Transforms)
		{
			Transform.SetScale3D(Transform.GetScale3D() * InvMesh);
		}
		Renderer->SetInstances(Transforms);
	}
	for (int32 Bucket = 0; Bucket < DTWeaponCues::NumBuckets; ++Bucket)
	{
		ADTTelegraphRenderer* Renderer = CueShapes[Bucket].Num() > 0 ? EnsureCueRenderer(Bucket) : (CueRenderers.IsValidIndex(Bucket) ? CueRenderers[Bucket].Get() : nullptr);
		if (Renderer)
		{
			Renderer->SetTelegraphs(CueShapes[Bucket]);
		}
	}
}
