#include "Keepers/DTKeeperSubsystem.h"
#include "Combat/DTBulletSubsystem.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Dusktracer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/World.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySources.h"
#include "Telemetry/DTTelemetrySubsystem.h"

namespace DTKeeperSubsystemLocal
{
	/** D-015: every player starts owning this weapon at its first level, right after the Patron pick. */
	static const FName StartingWeapon(TEXT("Weapon.PulseRifle"));
	constexpr int32 StartingLevel = 1;
	/** Trail parts per Path: the body (primary colour) and the tail tip (second colour, R-KEE-09). */
	constexpr int32 TrailParts = 2;
}

void FDTKeeperPhaseAdapter::SimStep(const FDTSimContext& Context)
{
	if (Owner)
	{
		Owner->StepPhase(Phase, Context);
	}
}

bool UDTKeeperSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTKeeperSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDTSimSubsystem>();
	Collection.InitializeDependency<UDTBulletSubsystem>();
	Collection.InitializeDependency<UDTWeaponSubsystem>();
	CapturePhase.Owner = this;
	CapturePhase.Phase = EDTSimPhase::Bullets;
	PickupsPhase.Owner = this;
	PickupsPhase.Phase = EDTSimPhase::Pickups;
}

void UDTKeeperSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// R-KEE-02, R-KEE-20: the catalog is validated once at load; a rejected blessing never grants flags.
	for (const FString& Error : DTKeepers::CatalogErrors())
	{
		UE_LOG(LogDT, Error, TEXT("Keepers: expression.csv rejected at load: %s"), *Error);
	}
	if (UDTSimSubsystem* SimSys = InWorld.GetSubsystem<UDTSimSubsystem>())
	{
		SimSubsystem = SimSys;
		SimSys->Register(EDTSimPhase::Effects, this);
		// The capture must run after the bullet pool stepped: registering the bullet system first is a no-op when it
		// already registered itself (AddUnique) and otherwise fixes the order inside the phase.
		if (UDTBulletSubsystem* BulletSys = InWorld.GetSubsystem<UDTBulletSubsystem>())
		{
			SimSys->Register(EDTSimPhase::Bullets, BulletSys);
		}
		SimSys->Register(EDTSimPhase::Bullets, &CapturePhase);
		SimSys->Register(EDTSimPhase::Pickups, &PickupsPhase);
	}
	if (UDTOfferSubsystem* Offers = InWorld.GetSubsystem<UDTOfferSubsystem>())
	{
		BlessingHandle = Offers->OnBlessingTaken.AddUObject(this, &UDTKeeperSubsystem::HandleBlessingTaken);
		KeeperHandle = Offers->OnKeeperChosen.AddUObject(this, &UDTKeeperSubsystem::HandleKeeperChosen);
	}
	if (UDTHeroSubsystem* Hero = InWorld.GetSubsystem<UDTHeroSubsystem>())
	{
		IntercessionHandle = Hero->OnIntercession.AddUObject(this, &UDTKeeperSubsystem::HandleIntercession);
	}
}

void UDTKeeperSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSys = SimSubsystem.Get())
	{
		SimSys->Unregister(this);
		SimSys->Unregister(&CapturePhase);
		SimSys->Unregister(&PickupsPhase);
	}
	if (UWorld* World = GetWorld())
	{
		if (UDTOfferSubsystem* Offers = World->GetSubsystem<UDTOfferSubsystem>())
		{
			Offers->OnBlessingTaken.Remove(BlessingHandle);
			Offers->OnKeeperChosen.Remove(KeeperHandle);
		}
		if (UDTHeroSubsystem* Hero = World->GetSubsystem<UDTHeroSubsystem>())
		{
			Hero->OnIntercession.Remove(IntercessionHandle);
		}
	}
	for (ADTBulletRenderer* Renderer : TrailRenderers)
	{
		if (Renderer)
		{
			Renderer->Destroy();
		}
	}
	TrailRenderers.Reset();
	SimSubsystem.Reset();
	Super::Deinitialize();
}

FDTRunState* UDTKeeperSubsystem::GetRun() const
{
	UDTSimSubsystem* SimSys = SimSubsystem.Get();
	if (!SimSys && GetWorld())
	{
		SimSys = GetWorld()->GetSubsystem<UDTSimSubsystem>();
	}
	return SimSys ? &SimSys->GetRun() : nullptr;
}

// ---- seams ------------------------------------------------------------------------------------------------------------------

void UDTKeeperSubsystem::Rebuild(int32 OwnerId)
{
	FDTRunState* Run = GetRun();
	UDTWeaponSubsystem* Weapons = GetWorld() ? GetWorld()->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	if (Run && Weapons && Run->IsValidPlayer(OwnerId))
	{
		Sim.RebuildFlags(*Run, OwnerId, Weapons->GetSim());
	}
}

bool UDTKeeperSubsystem::Bless(int32 OwnerId, FName BlessingId)
{
	FDTRunState* Run = GetRun();
	if (!Run || !Run->IsValidPlayer(OwnerId) || !DTOffers::AddBlessing(Run->GetPlayer(OwnerId).Keepers, BlessingId))
	{
		return false;
	}
	HandleBlessingTaken(OwnerId, BlessingId);
	return true;
}

void UDTKeeperSubsystem::HandleBlessingTaken(int32 OwnerId, FName BlessingId)
{
	// Owned, Favor, Tier and Leading are already updated by the offer state (R-KEE-06, R-KEE-07); the flags follow them.
	Rebuild(OwnerId);
}

void UDTKeeperSubsystem::HandleKeeperChosen(int32 OwnerId, EDTPath Path, bool bPatron)
{
	Rebuild(OwnerId); // a new run's Patron clears last run's sources; the commitment starts the second enchantment
	if (!bPatron)
	{
		return;
	}
	UDTWeaponSubsystem* Weapons = GetWorld() ? GetWorld()->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	FDTRunState* Run = GetRun();
	if (Weapons && Run && Run->IsValidPlayer(OwnerId) &&
		FDTWeaponSim::FindSlot(*Run, OwnerId, DTKeeperSubsystemLocal::StartingWeapon) == INDEX_NONE)
	{
		const EDTWeaponResult Result = Weapons->Give(OwnerId, DTKeeperSubsystemLocal::StartingWeapon, DTKeeperSubsystemLocal::StartingLevel);
		UE_LOG(LogDT, Log, TEXT("Keepers p%d: Patron %s; starting weapon %s L%d (%s)"), OwnerId, FDTTelemetrySources::PathName(Path),
			*DTKeeperSubsystemLocal::StartingWeapon.ToString(), DTKeeperSubsystemLocal::StartingLevel,
			Result == EDTWeaponResult::Ok ? TEXT("given") : TEXT("refused"));
	}
}

void UDTKeeperSubsystem::HandleIntercession(int32 OwnerId, EDTPath Path)
{
	Sim.QueueIntercession(OwnerId);
}

// ---- the step -----------------------------------------------------------------------------------------------------------------

void UDTKeeperSubsystem::SimStep(const FDTSimContext& Context)
{
	UWorld* World = GetWorld();
	UDTWeaponSubsystem* Weapons = World ? World->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	Sim.StepEffects(Context, Weapons ? &Weapons->GetSim() : nullptr);
	if (UDTHeroSubsystem* Hero = World ? World->GetSubsystem<UDTHeroSubsystem>() : nullptr)
	{
		for (const int32 OwnerId : Sim.TakeDashResets())
		{
			Hero->RefillDash(OwnerId);
		}
		// keepers.md R-KEE-34: what Quicksilver asks of the dash.
		FDTRunState* Run = GetRun();
		for (int32 OwnerId = 0; Weapons && Run && OwnerId < Run->GetNumPlayers(); ++OwnerId)
		{
			if (const FDTHeroState* State = Hero->GetState(OwnerId))
			{
				const FDTKeeperSim::FHeroRiders Riders = Sim.GetHeroRiders(OwnerId);
				if (State->Stats.DashCooldownMul != Riders.DashCooldownMul || State->Stats.DashChargesBonus != Riders.DashCharges)
				{
					FDTHeroStats Stats = State->Stats;
					Stats.DashCooldownMul = Riders.DashCooldownMul;
					Stats.DashChargesBonus = Riders.DashCharges;
					Hero->SetStats(OwnerId, Stats);
				}
				if (const float Refund = Weapons->GetSim().TakeDashRefund(OwnerId))
				{
					Hero->RefundDash(OwnerId, Refund);
				}
				for (int32 Refill = Weapons->GetSim().TakeDashRefills(OwnerId); Refill > 0; --Refill)
				{
					Hero->RefillDash(OwnerId);
				}
			}
		}
	}
	TArray<FDTKeeperCastRecord> Casts = Sim.TakeFinishedCasts();
	if (Casts.Num() == 0)
	{
		return;
	}
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		for (const FDTKeeperCastRecord& Cast : Casts)
		{
			Telemetry->Records().RecordIntercession(Cast.OwnerId, Cast.Record);
		}
	}
}

void UDTKeeperSubsystem::StepPhase(EDTSimPhase Phase, const FDTSimContext& Context)
{
	if (Phase == EDTSimPhase::Bullets)
	{
		UDTEnemySubsystem* Enemies = GetWorld() ? GetWorld()->GetSubsystem<UDTEnemySubsystem>() : nullptr;
		Sim.CaptureHits(Context, Enemies ? &Enemies->GetPool() : nullptr);
	}
	else if (Phase == EDTSimPhase::Pickups)
	{
		RecordStatuses(Context);
	}
}

void UDTKeeperSubsystem::RecordStatuses(const FDTSimContext& Context)
{
	// Every application written in this step's Effects phase (enchantment, flags, Intercession), per owner (Telemetry).
	UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this);
	if (!Telemetry || !Context.View || !Context.Run)
	{
		return;
	}
	TMap<TPair<int32, int32>, int32> Counts;
	for (const FDTStatusApply& Apply : Context.View->StatusApplies)
	{
		if (Context.Run->IsValidPlayer(Apply.OwnerId))
		{
			Counts.FindOrAdd(TPair<int32, int32>(Apply.OwnerId, static_cast<int32>(Apply.Status)))++;
		}
	}
	for (const TPair<TPair<int32, int32>, int32>& Count : Counts)
	{
		Telemetry->Records().RecordStatusApplied(Count.Key.Key, static_cast<EDTStatus>(Count.Key.Value), Count.Value);
	}
}

// ---- trails (R-KEE-09, R-PRE-01) -------------------------------------------------------------------------------------------------

int32 UDTKeeperSubsystem::GetTrailRendererCount() const
{
	int32 Count = 0;
	for (const ADTBulletRenderer* Renderer : TrailRenderers)
	{
		Count += Renderer ? 1 : 0;
	}
	return Count;
}

ADTBulletRenderer* UDTKeeperSubsystem::EnsureTrailRenderer(int32 Bucket)
{
	if (TrailRenderers.Num() <= Bucket)
	{
		TrailRenderers.SetNum(Bucket + 1);
	}
	if (!TrailRenderers[Bucket] && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADTBulletRenderer* Renderer = GetWorld()->SpawnActor<ADTBulletRenderer>(ADTBulletRenderer::StaticClass(), FTransform::Identity, Params);
		if (Renderer)
		{
			const EDTPath Path = static_cast<EDTPath>(Bucket / DTKeeperSubsystemLocal::TrailParts);
			// The token as authored (emissive x1): the trail carries the Keeper's hue, the core stays the brightest part
			// of the round (R-PRE-01, R-PRE-06).
			Renderer->InitRender(DTPal::Linear(DTKeepers::TrailToken(Path)), 1.0f);
		}
		TrailRenderers[Bucket] = Renderer;
	}
	return TrailRenderers[Bucket];
}

void UDTKeeperSubsystem::SimRender(float Alpha)
{
	using namespace DTKeeperSubsystemLocal;
	UWorld* World = GetWorld();
	UDTBulletSubsystem* BulletSys = World ? World->GetSubsystem<UDTBulletSubsystem>() : nullptr;
	FDTRunState* Run = GetRun();
	const int32 Buckets = static_cast<int32>(EDTPath::Count) * TrailParts;
	TrailTransforms.SetNum(Buckets);
	for (TArray<FTransform>& Bucket : TrailTransforms)
	{
		Bucket.Reset();
	}
	if (BulletSys && Run)
	{
		// Per player: the body colour (primary, from its first blessing) and the tip colour (second from the commitment).
		TArray<int32> Body;
		TArray<int32> Tip;
		Body.Init(INDEX_NONE, Run->GetNumPlayers());
		Tip.Init(INDEX_NONE, Run->GetNumPlayers());
		for (int32 Owner = 0; Owner < Run->GetNumPlayers(); ++Owner)
		{
			const FDTKeeperRunState& Keepers = Run->GetPlayer(Owner).Keepers;
			if (Keepers.Primary == EDTPath::None || Keepers.Primary >= EDTPath::Count || DTKeepers::OwnedOfPath(Keepers, Keepers.Primary) == 0)
			{
				continue;
			}
			Body[Owner] = static_cast<int32>(Keepers.Primary) * TrailParts;
			const bool bSecond = Keepers.bCommitDone && Keepers.Second != EDTPath::None && Keepers.Second < EDTPath::Count;
			Tip[Owner] = static_cast<int32>(bSecond ? Keepers.Second : Keepers.Primary) * TrailParts + 1;
		}
		const FDTBulletPool& Pool = BulletSys->GetPool();
		const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
		const TConstArrayView<FVector2f> Prev = Pool.GetPrevPositions();
		const TConstArrayView<float> Radii = Pool.GetRadii();
		const TConstArrayView<int32> Owners = Pool.GetOwners();
		const TConstArrayView<uint16> Sources = Pool.GetSources();
		const float InvMesh = BulletSys->GetRenderer() ? 1.0f / BulletSys->GetRenderer()->GetMeshRadius() : 0.0f;
		const double Z = DTConst::SIM_PLANE_Z_UU;
		for (int32 Index = 0; InvMesh > 0.0f && Index < Pool.Num(); ++Index)
		{
			const int32 Owner = Owners[Index];
			if (!Body.IsValidIndex(Owner) || Body[Owner] == INDEX_NONE || Sources[Index] == FDTTelemetrySources::Stress)
			{
				continue;
			}
			// The trail runs behind the head along the round's flight: body first, the tip at its end.
			const FVector2f Step = Pos[Index] - Prev[Index];
			const float Length = Step.Size();
			if (Length <= KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector2f Dir = Step / Length;
			const FVector2f Head = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
			const float R = Radii[Index];
			const FQuat Rot = FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)), 0.0f).Quaternion();
			// Each part spans the step's travel, at least the round's own diameter (slow children still read as trailed).
			const float Half = 0.5f * FMath::Max(Length, 2.0f * R);
			const FVector Scale(Half * InvMesh, R * InvMesh, R * InvMesh);
			const FVector2f BodyAt = Head - Dir * (R + Half);
			const FVector2f TipAt = Head - Dir * (R + 3.0f * Half);
			TrailTransforms[Body[Owner]].Emplace(Rot, FVector(BodyAt.X, BodyAt.Y, Z), Scale);
			TrailTransforms[Tip[Owner]].Emplace(Rot, FVector(TipAt.X, TipAt.Y, Z), Scale);
		}
	}
	for (int32 Bucket = 0; Bucket < Buckets; ++Bucket)
	{
		// Renderers are spawned on the first trail of their colour, then kept (an empty bucket clears its instances).
		ADTBulletRenderer* Renderer = TrailTransforms[Bucket].Num() > 0 ? EnsureTrailRenderer(Bucket)
			: (TrailRenderers.IsValidIndex(Bucket) ? TrailRenderers[Bucket].Get() : nullptr);
		if (Renderer)
		{
			Renderer->SetInstances(TrailTransforms[Bucket]);
		}
	}
}
