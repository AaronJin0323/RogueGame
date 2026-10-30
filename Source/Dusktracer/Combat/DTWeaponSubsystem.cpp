#include "Combat/DTWeaponSubsystem.h"
#include "Combat/DTBulletSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Dusktracer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/World.h"

int32 DTWeaponPalette::NumBuckets()
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
	return Bucket == 1 ? ZoneToken(EDTPath::None) : ZoneToken(static_cast<EDTPath>(Bucket - 2));
}

int32 DTWeaponPalette::BucketOf(const FDTWeaponVisual& Visual)
{
	if (Visual.Kind == FDTWeaponVisual::EKind::Drone || Visual.Kind == FDTWeaponVisual::EKind::Mine)
	{
		return 0;
	}
	return Visual.Path == EDTPath::None || Visual.Path >= EDTPath::Count ? 1 : 2 + static_cast<int32>(Visual.Path);
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
	return false;
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
			const float Emissive = Bucket == 0 ? DTConst::BULLETS_EMISSIVE_PLAYER : 1.0f;
			Renderer->InitRender(DTPal::Linear(DTWeaponPalette::BucketToken(Bucket)), Emissive);
		}
		Renderers[Bucket] = Renderer;
	}
	return Renderers[Bucket];
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
	const double Z = DTConst::SIM_PLANE_Z_UU;
	for (const FDTWeaponVisual& Visual : Visuals)
	{
		TArray<FTransform>& Out = BucketTransforms[DTWeaponPalette::BucketOf(Visual)];
		switch (Visual.Kind)
		{
		case FDTWeaponVisual::EKind::Drone:
		case FDTWeaponVisual::EKind::Mine:
			Out.Add(FTransform(FQuat::Identity, FVector(Visual.Position.X, Visual.Position.Y, Z), FVector(Visual.Radius)));
			break;
		case FDTWeaponVisual::EKind::MineZone:
		case FDTWeaponVisual::EKind::Field:
			// A flat disc on the sim plane, one unit thick.
			Out.Add(FTransform(FQuat::Identity, FVector(Visual.Position.X, Visual.Position.Y, Z), FVector(Visual.Radius, Visual.Radius, 1.0f)));
			break;
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
}
