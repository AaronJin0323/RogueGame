#include "Enemies/DTEnemySubsystem.h"
#include "Dusktracer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Engine/World.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetrySubsystem.h"

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
	if (RoundRenderer)
	{
		RoundRenderer->Destroy();
		RoundRenderer = nullptr;
	}
	if (TelegraphRenderer)
	{
		TelegraphRenderer->Destroy();
		TelegraphRenderer = nullptr;
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

FLinearColor UDTEnemySubsystem::GetHullColor()
{
	return DTPal::Linear(DTPal::Enemy_Body);
}

FLinearColor UDTEnemySubsystem::GetOpticColor()
{
	return DTPal::Linear(DTPal::Enemy_Optic);
}

FLinearColor UDTEnemySubsystem::GetRoundColor()
{
	return DTPal::Linear(DTPal::Enemy_Bullet);
}

FLinearColor UDTEnemySubsystem::GetTelegraphColor()
{
	return DTPal::Linear(DTPal::Telegraph);
}

void UDTEnemySubsystem::ForwardThreatActions(const FDTEnemyPool& InPool, FDTTelemetryCore& Records)
{
	for (const FDTThreatAction& Action : InPool.GetStepThreatActions())
	{
		Records.RecordThreatAction(Action.Origin, Action.Role, Action.AttackId);
	}
}

void UDTEnemySubsystem::EnsureRenderer()
{
	UWorld* World = GetWorld();
	if (!World || (Renderer && RoundRenderer && TelegraphRenderer))
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (!Renderer)
	{
		Renderer = World->SpawnActor<ADTEnemyRenderer>(ADTEnemyRenderer::StaticClass(), FTransform::Identity, Params);
		if (Renderer)
		{
			// R-ENM-24: the hull is a dark silhouette without emissive; the separate optic carries readability.
			Renderer->Setup(GetHullColor(), 0.0f, GetOpticColor(), DTConst::ENEMIES_OPTIC_EMISSIVE);
		}
	}
	if (!RoundRenderer)
	{
		RoundRenderer = World->SpawnActor<ADTEnemyBulletRenderer>(ADTEnemyBulletRenderer::StaticClass(), FTransform::Identity, Params);
		if (RoundRenderer)
		{
			RoundRenderer->InitRender(GetRoundColor(), DTConst::ENEMY_FIRE_EMISSIVE);
		}
	}
	if (!TelegraphRenderer)
	{
		TelegraphRenderer = World->SpawnActor<ADTTelegraphRenderer>(ADTTelegraphRenderer::StaticClass(), FTransform::Identity, Params);
		if (TelegraphRenderer)
		{
			// R-ENM-13, R-PRE-03: red projected telegraphs, never dimmed by density (R-PRE-06).
			TelegraphRenderer->InitRender(GetTelegraphColor(), DTConst::ENEMIES_TELEGRAPH_EMISSIVE);
		}
	}
}

void UDTEnemySubsystem::QueueSpawn(const FDTEnemySpawn& InSpawn)
{
	QueuedSpawns.Add(InSpawn);
}

void UDTEnemySubsystem::RequestSpawn(int32 Count)
{
	PendingSpawns = FMath::Clamp(PendingSpawns + FMath::Max(0, Count), 0, DTConst::ENEMIES_CAP);
}

void UDTEnemySubsystem::RequestSpawnRole(EDTEnemyRole Role, int32 Count, bool bElite, bool bArrival)
{
	if (Count <= 0)
	{
		return;
	}
	FRoleRequest& Request = RoleRequests.AddDefaulted_GetRef();
	Request.Role = Role;
	Request.Count = FMath::Min(Count, DTConst::ENEMIES_CAP);
	Request.bElite = bElite;
	Request.bArrival = bArrival;
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
	RoleRequests.Reset();
	QueuedSpawns.Reset();
}

void UDTEnemySubsystem::RequestStatus(EDTStatus Status, int32 Count, float Magnitude)
{
	FStatusRequest& Request = StatusRequests.AddDefaulted_GetRef();
	Request.Status = Status;
	Request.Count = Count;
	Request.Magnitude = Magnitude;
}

void UDTEnemySubsystem::ApplyRequests(const FDTSimContext& Context)
{
	if (bClearPending)
	{
		Pool.Reset(false);
		bClearPending = false;
	}
	if (!Context.Run)
	{
		return;
	}
	FRandomStream& Rng = Context.Run->GetRng().Get(EDTRngStream::Spawn);

	// Director spawns first, in the order they were queued.
	for (const FDTEnemySpawn& Spec : QueuedSpawns)
	{
		Pool.Spawn(Spec, &Rng, true);
	}
	QueuedSpawns.Reset();

	const FVector2f Center = Context.View && Context.View->HeroPositions.Num() ? Context.View->HeroPositions[0] : FVector2f::ZeroVector;
	const float Ring = static_cast<float>(DTConst::ENM_SPAWN_RING_RADIUS_UU);
	for (const FRoleRequest& Request : RoleRequests)
	{
		FDTEnemySpawn Template;
		Template.Role = Request.Role;
		Template.bElite = Request.bElite;
		Template.bArrival = Request.bArrival;
		Pool.SpawnRing(Template, Request.Count, Center, Ring, Rng);
	}
	RoleRequests.Reset();

	int32 Wanted = PendingSpawns;
	PendingSpawns = 0;
	if (KeepPopulation > Pool.Num() + Wanted)
	{
		Wanted = KeepPopulation - Pool.Num();
	}
	if (Wanted > 0)
	{
		Pool.SpawnRing(EDTEnemyRole::Swarm, Wanted, Center, Ring, Rng);
	}

	// Debug statuses go through the same queue effects use (FDTWorldView::StatusApplies, R-ENM-21).
	if (Context.View)
	{
		for (const FStatusRequest& Request : StatusRequests)
		{
			const int32 Count = Request.Count > 0 ? FMath::Min(Request.Count, Pool.Num()) : Pool.Num();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FDTStatusApply& Apply = Context.View->StatusApplies.AddDefaulted_GetRef();
				Apply.Handle = Pool.GetHandle(Index);
				Apply.Status = Request.Status;
				Apply.Magnitude = Request.Magnitude;
				Apply.OwnerId = 0;
			}
		}
	}
	StatusRequests.Reset();
}

void UDTEnemySubsystem::SimStep(const FDTSimContext& Context)
{
	const bool bCleared = bClearPending;
	ApplyRequests(Context);
	// Telemetry hook (read-only): stamps the spawn step of new slots for swarm TTK (02_Systems/telemetry.md R-TEL-03).
	if (UDTTelemetrySubsystem* Telemetry = GetWorld() ? GetWorld()->GetSubsystem<UDTTelemetrySubsystem>() : nullptr)
	{
		Telemetry->OnEnemiesSpawned(Context.StepIndex, bCleared, Pool.Num());
	}
	Pool.StepEnemies(Context);
	// Threat actions started this step go to telemetry from here, on the game thread (enemies.md Telemetry).
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		ForwardThreatActions(Pool, Telemetry->Records());
	}
}

void UDTEnemySubsystem::StepResolveHits(const FDTSimContext& Context)
{
	// Telemetry hook (read-only): brackets the resolve so kills are matched to their hit's slot and source.
	UDTTelemetrySubsystem* Telemetry = GetWorld() ? GetWorld()->GetSubsystem<UDTTelemetrySubsystem>() : nullptr;
	if (Telemetry && Context.View)
	{
		Telemetry->BeginResolve(Context.StepIndex, Context.View->Hits);
	}
	Pool.ResolveHits(Context);
	if (Telemetry)
	{
		Telemetry->EndResolve(Pool.Num());
	}
}

void UDTEnemySubsystem::SimRender(float Alpha)
{
	CSV_CUSTOM_STAT(DT, LiveEnemies, Pool.Num(), ECsvCustomStatOp::Set);
	CSV_CUSTOM_STAT(DT, LiveEnemyRounds, Pool.GetRounds().Num(), ECsvCustomStatOp::Set);
	EnsureRenderer();
	const float Z = static_cast<float>(DTConst::SIM_PLANE_Z_UU);

	if (Renderer)
	{
		const int32 Count = Pool.Num();
		const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
		const TConstArrayView<FVector2f> Prev = Pool.GetPrevPositions();
		const TConstArrayView<FVector2f> Facing = Pool.GetFacings();
		const TConstArrayView<float> Radii = Pool.GetRadii();
		const TConstArrayView<EDTEnemyRole> Roles = Pool.GetRoles();
		const float MeshSize = Renderer->GetMeshSizeUU();
		const float OpticScale = DTConst::ENEMIES_OPTIC_SCALE;

		HullTransforms.SetNum(Count, EAllowShrinking::No);
		OpticTransforms.SetNum(Count, EAllowShrinking::No);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2f At = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
			const FVector2f& F = Facing[Index];
			const float Yaw = F.IsNearlyZero() ? 0.0f : FMath::RadiansToDegrees(FMath::Atan2(F.Y, F.X));
			const float R = Radii[Index];
			const FRotator Rot(0.0f, Yaw, 0.0f);
			HullTransforms[Index] = FTransform(Rot, FVector(At.X, At.Y, Z), FVector(2.0f * R / MeshSize));

			// The optic sits on top of the hull where the role's model language puts it (enemies.md Model language):
			// the nose (Swarm, Rusher) or over the plate (Tank) at the front edge; the mirror head (Turret) or the hull
			// centre (Boss) in the middle.
			const bool bCentred = Roles[Index] == EDTEnemyRole::Turret || Roles[Index] == EDTEnemyRole::Boss;
			const FVector2f OpticAt = bCentred ? At : At + F * ((1.0f - OpticScale) * R);
			OpticTransforms[Index] = FTransform(Rot, FVector(OpticAt.X, OpticAt.Y, Z + R), FVector(2.0f * R * OpticScale / MeshSize));
		}
		Renderer->UpdateInstances(HullTransforms, OpticTransforms);
	}

	if (RoundRenderer)
	{
		const FDTEnemyBulletPool& Rounds = Pool.GetRounds();
		const int32 Count = Rounds.Num();
		const TConstArrayView<FVector2f> Pos = Rounds.GetPositions();
		const TConstArrayView<FVector2f> Prev = Rounds.GetPrevPositions();
		const TConstArrayView<float> Radii = Rounds.GetRadii();
		const float InvMeshRadius = 1.0f / RoundRenderer->GetMeshRadius();
		RoundTransforms.SetNumUninitialized(Count, EAllowShrinking::No);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FVector2f P = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
			RoundTransforms[Index] = FTransform(FQuat::Identity, FVector(P.X, P.Y, Z), FVector(Radii[Index] * InvMeshRadius));
		}
		RoundRenderer->SetInstances(RoundTransforms);
	}

	if (TelegraphRenderer)
	{
		// Telegraphs are overwritten every step by their producers (enemies, boss); drawn as published, not interpolated.
		UDTSimSubsystem* SimSystem = Sim.Get();
		TelegraphRenderer->SetTelegraphs(SimSystem ? TConstArrayView<FDTTelegraph>(SimSystem->GetView().Telegraphs) : TConstArrayView<FDTTelegraph>());
	}
}
