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
#if !UE_BUILD_SHIPPING
	static TAutoConsoleVariable<float> CVarDilationScale(TEXT("dt.Exp.DilationScale"), -1.0f,
		TEXT("Experiment, expires 2027-01-31: enemy time during Time Dilation (keepers.md R-KEE-37), 0 to 1; below 0 reads C.INTERCESSION.QUICKSILVER_TIME_SCALE"));
#endif
	/** D-015: every player starts owning this weapon at its first level, right after the Patron pick. */
	static const FName StartingWeapon(TEXT("Weapon.PulseRifle"));
	constexpr int32 StartingLevel = 1;
	/** One trail bucket per pair of body and tip colour (tracer_art.md R-TRC-03; the tip is the second Keeper's, R-KEE-09). */
	constexpr int32 TrailBuckets = DTTracerArt::NumSlots * DTTracerArt::NumSlots;
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

void UDTKeeperSubsystem::LogReactions() const
{
	const UDTWeaponSubsystem* Weapons = GetWorld() ? GetWorld()->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	const FDTRunState* Run = GetRun();
	for (int32 Owner = 0; Weapons && Run && Owner < Run->GetNumPlayers(); ++Owner)
	{
		for (int32 Row = 0; Row < DTCat::NumReactions; ++Row)
		{
			const FDTReactionCounters& Counters = Weapons->GetSim().GetReactionCounters(Owner, Row);
			if (Counters.Fires > 0)
			{
				UE_LOG(LogDT, Log, TEXT("Reactions p%d: %s fires %lld damage %.0f"), Owner, DTCat::Reactions[Row].Pair, Counters.Fires, Counters.Damage);
			}
		}
	}
}

void UDTKeeperSubsystem::Deinitialize()
{
	LogReactions();
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
#if !UE_BUILD_SHIPPING
	const float Experiment = DTKeeperSubsystemLocal::CVarDilationScale.GetValueOnGameThread();
	Sim.SetDilationScale(Experiment >= 0.0f ? FMath::Min(Experiment, 1.0f) : static_cast<float>(DTConst::INTERCESSION_QUICKSILVER_TIME_SCALE));
#endif
	UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this);
	if (Telemetry)
	{
		for (const FDTKeeperCastRecord& Cast : ClosingCasts)
		{
			Telemetry->Records().EndIntercession(Cast.OwnerId, Cast.Touched);
		}
	}
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
				const FDTDuoHero Duo = Weapons->GetSim().GetDuoHero(OwnerId); // reactions.md
				const int32 Charges = Riders.DashCharges + Duo.DashCharges;
				if (State->Stats.DashCooldownMul != Riders.DashCooldownMul || State->Stats.DashChargesBonus != Charges || State->Stats.MoveSpeedMul != Duo.MoveSpeedMul)
				{
					FDTHeroStats Stats = State->Stats;
					Stats.DashCooldownMul = Riders.DashCooldownMul;
					Stats.DashChargesBonus = Charges;
					Stats.MoveSpeedMul = Duo.MoveSpeedMul;
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
	ClosingCasts = Sim.TakeFinishedCasts();
	const TArray<FDTKeeperCastRecord> Opened = Sim.TakeStartedCasts();
	if (Telemetry)
	{
		if (Weapons)
		{
			for (const FDTReactionEvent& Event : Weapons->GetSim().TakeReactionEvents())
			{
				Telemetry->Records().RecordReaction(Event.OwnerId, FName(DTCat::Reactions[Event.Row].Pair), Event.Fires, Event.Damage, Event.bFirst);
			}
		}
		for (const FDTKeeperCastRecord& Cast : Opened)
		{
			Telemetry->Records().BeginIntercession(Cast.OwnerId, Cast.Record);
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

// ---- trails (R-KEE-09, R-PRE-01, tracer_art.md) ---------------------------------------------------------------------------------

int32 UDTKeeperSubsystem::GetTrailRendererCount() const
{
	int32 Count = 0;
	for (const ADTBulletRenderer* Renderer : TrailRenderers)
	{
		Count += Renderer ? 1 : 0;
	}
	return Count;
}

ADTBulletRenderer* UDTKeeperSubsystem::GetTrailRenderer(int32 Body, int32 Tip) const
{
	const int32 Bucket = Body * DTTracerArt::NumSlots + Tip;
	return Body >= 0 && Body < DTTracerArt::NumSlots && Tip >= 0 && Tip < DTTracerArt::NumSlots && TrailRenderers.IsValidIndex(Bucket) ? TrailRenderers[Bucket].Get() : nullptr;
}

ADTBulletRenderer* UDTKeeperSubsystem::EnsureTrailRenderer(int32 Bucket)
{
	if (TrailRenderers.Num() <= Bucket)
	{
		TrailRenderers.SetNum(Bucket + 1);
		TrailMissing.SetNumZeroed(Bucket + 1);
	}
	if (!TrailRenderers[Bucket] && !TrailMissing[Bucket] && GetWorld())
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ADTBulletRenderer* Renderer = GetWorld()->SpawnActor<ADTBulletRenderer>(ADTBulletRenderer::StaticClass(), FTransform::Identity, Params);
		// The tokens as authored (C.TRC.TAIL_EMISSIVE): the tail carries the hue, the core stays the brightest part of the round
		// (R-PRE-01, R-PRE-06).
		const FLinearColor Body = DTPal::Linear(DTTracerArt::SlotToken(Bucket / DTTracerArt::NumSlots));
		const FLinearColor Tip = DTPal::Linear(DTTracerArt::SlotToken(Bucket % DTTracerArt::NumSlots));
		if (Renderer && !Renderer->InitTail(Body, Tip, DTConst::TRC_TAIL_EMISSIVE))
		{
			Renderer->Destroy();
			Renderer = nullptr;
			TrailMissing[Bucket] = true;
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
	TrailTransforms.SetNum(TrailBuckets);
	for (TArray<FTransform>& Bucket : TrailTransforms)
	{
		Bucket.Reset();
	}
	int32 Live = 0;
	if (BulletSys && Run)
	{
		// Per player: the bucket of its tails, from the body colour (primary, once blessed; neutral before) and the tip colour (the
		// second from the commitment).
		TArray<int32, TInlineAllocator<4>> BucketOf;
		for (int32 Owner = 0; Owner < Run->GetNumPlayers(); ++Owner)
		{
			const FDTKeeperRunState& Keepers = Run->GetPlayer(Owner).Keepers;
			BucketOf.Add(DTTracerArt::BodySlot(Keepers) * DTTracerArt::NumSlots + DTTracerArt::TipSlot(Keepers));
		}
		const FDTBulletPool& Pool = BulletSys->GetPool();
		const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
		const TConstArrayView<FVector2f> Prev = Pool.GetPrevPositions();
		const TConstArrayView<float> Radii = Pool.GetRadii();
		const TConstArrayView<int32> Owners = Pool.GetOwners();
		const TConstArrayView<uint16> Sources = Pool.GetSources();
		Live = Pool.Num();
		// R-TRC-06: the tail narrows with the live player-round count; its strength falls through the material's Dim below.
		const float WidthDim = DTTracerArt::TailWidthDim(Live);
		const float InvPlane = 1.0f / DTTracerArt::PlaneSizeUU;
		const double Z = DTConst::SIM_PLANE_Z_UU;
		FTransform Tail;
		// R-TRC-02, D-068: no tail while the round is inside its hero's clear zone; once out, its tail is drawn whole.
		const DTTracerArt::FClearZone Clear = DTTracerArt::FClearZone::OfWorld(World, Alpha);
		for (int32 Index = 0; Index < Live; ++Index)
		{
			const int32 Owner = Owners[Index];
			if (!BucketOf.IsValidIndex(Owner) || Sources[Index] == FDTTelemetrySources::Stress)
			{
				continue;
			}
			// The tail runs from the head back along the round's flight (read from the pool, never written); a round at rest has none.
			const FVector2f Head = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
			if (Clear.Hides(Owner, Head))
			{
				continue;
			}
			if (DTTracerArt::Tail(Head, Pos[Index] - Prev[Index], Radii[Index], WidthDim, InvPlane, Z, Tail))
			{
				TrailTransforms[BucketOf[Owner]].Add(Tail);
			}
		}
	}
	const float Dim = DTTracerArt::TailDim(Live);
	for (int32 Bucket = 0; Bucket < TrailBuckets; ++Bucket)
	{
		// Renderers are spawned on the first tail of their colours, then kept (an empty bucket clears its instances).
		ADTBulletRenderer* Renderer = TrailTransforms[Bucket].Num() > 0 ? EnsureTrailRenderer(Bucket)
			: (TrailRenderers.IsValidIndex(Bucket) ? TrailRenderers[Bucket].Get() : nullptr);
		if (Renderer)
		{
			Renderer->SetInstances(TrailTransforms[Bucket]);
			Renderer->SetDim(Dim);
		}
	}
}
