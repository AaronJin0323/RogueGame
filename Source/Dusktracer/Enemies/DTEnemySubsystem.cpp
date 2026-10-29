#include "Enemies/DTEnemySubsystem.h"
#include "Dusktracer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Engine/World.h"

void FDTEnemyResolveHitsPhase::SimStep(const FDTSimContext& Context)
{
	if (Owner)
	{
		Owner->StepResolveHits(Context);
	}
}

bool UDTEnemySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTEnemySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	ResolvePhase.Owner = this;
}

void UDTEnemySubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
		SimSystem->Unregister(&ResolvePhase);
	}
	if (Renderer)
	{
		Renderer->Destroy();
		Renderer = nullptr;
	}
	ResolvePhase.Owner = nullptr;
	Super::Deinitialize();
}

void UDTEnemySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Sim.IsValid())
	{
		Sim = InWorld.GetSubsystem<UDTSimSubsystem>();
	}
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Register(EDTSimPhase::Enemies, this);
		SimSystem->Register(EDTSimPhase::ResolveHits, &ResolvePhase);
	}
	else
	{
		UE_LOG(LogDT, Error, TEXT("UDTEnemySubsystem: no UDTSimSubsystem in this world; enemies will not step"));
	}
	EnsureRenderer();
}

FLinearColor UDTEnemySubsystem::GetInstanceColor()
{
	const FLinearColor Body = DTPal::Linear(DTPal::Enemy_Body);
	const FLinearColor Optic = DTPal::Linear(DTPal::Enemy_Optic);
	return Body + (Optic - Body) * DTConst::ENM_OPTIC_TINT;
}

void UDTEnemySubsystem::EnsureRenderer()
{
	if (Renderer)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Renderer = World->SpawnActor<ADTEnemyRenderer>(ADTEnemyRenderer::StaticClass(), FTransform::Identity, Params);
	if (Renderer)
	{
		Renderer->Setup(GetInstanceColor(), DTConst::ENM_EMISSIVE_STRENGTH);
	}
}

void UDTEnemySubsystem::RequestSpawn(int32 Count)
{
	PendingSpawns = FMath::Clamp(PendingSpawns + FMath::Max(0, Count), 0, DTConst::ENEMIES_CAP);
}

void UDTEnemySubsystem::SetKeepPopulation(int32 Count)
{
	KeepPopulation = FMath::Clamp(Count, 0, DTConst::ENEMIES_CAP);
}

void UDTEnemySubsystem::RequestClear()
{
	bClearPending = true;
	PendingSpawns = 0;
	KeepPopulation = 0;
}

void UDTEnemySubsystem::ApplyRequests(const FDTSimContext& Context)
{
	if (bClearPending)
	{
		Pool.Reset(false);
		bClearPending = false;
	}
	int32 Wanted = PendingSpawns;
	PendingSpawns = 0;
	if (KeepPopulation > Pool.Num() + Wanted)
	{
		Wanted = KeepPopulation - Pool.Num();
	}
	if (Wanted <= 0 || !Context.Run)
	{
		return;
	}
	const FVector2f Center = Context.View && Context.View->HeroPositions.Num() ? Context.View->HeroPositions[0] : FVector2f::ZeroVector;
	FRandomStream& Rng = Context.Run->GetRng().Get(EDTRngStream::Spawn);
	Pool.SpawnRing(EDTEnemyRole::Swarm, Wanted, Center, static_cast<float>(DTConst::ENM_SPAWN_RING_RADIUS_UU), Rng);
}

void UDTEnemySubsystem::SimStep(const FDTSimContext& Context)
{
	ApplyRequests(Context);
	Pool.StepEnemies(Context);
}

void UDTEnemySubsystem::StepResolveHits(const FDTSimContext& Context)
{
	Pool.ResolveHits(Context);
}

void UDTEnemySubsystem::SimRender(float Alpha)
{
	CSV_CUSTOM_STAT(DT, LiveEnemies, Pool.Num(), ECsvCustomStatOp::Set);
	EnsureRenderer();
	if (!Renderer)
	{
		return;
	}
	const int32 Count = Pool.Num();
	const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
	const TConstArrayView<FVector2f> Prev = Pool.GetPrevPositions();
	const TConstArrayView<FVector2f> Vel = Pool.GetVelocities();
	const TConstArrayView<float> Radii = Pool.GetRadii();
	const float MeshSize = Renderer->GetMeshSizeUU();
	const float Z = static_cast<float>(DTConst::SIM_PLANE_Z_UU);

	RenderTransforms.SetNum(Count, EAllowShrinking::No);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FVector2f At = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
		const FVector2f& V = Vel[Index];
		const float Yaw = V.IsNearlyZero() ? 0.0f : FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
		const float Scale = 2.0f * Radii[Index] / MeshSize;
		RenderTransforms[Index] = FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(At.X, At.Y, Z), FVector(Scale));
	}
	Renderer->UpdateInstances(RenderTransforms);
}
