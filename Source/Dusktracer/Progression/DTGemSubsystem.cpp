#include "Progression/DTGemSubsystem.h"
#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Engine/World.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"

bool UDTGemSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTGemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
}

void UDTGemSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
	}
	if (Renderer)
	{
		Renderer->Destroy();
		Renderer = nullptr;
	}
	Super::Deinitialize();
}

void UDTGemSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Sim.IsValid())
	{
		Sim = InWorld.GetSubsystem<UDTSimSubsystem>();
	}
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Register(EDTSimPhase::Pickups, this);
		FDTProgression::EnsureCurve(SimSystem->GetRun());
	}
	else
	{
		UE_LOG(LogDT, Error, TEXT("UDTGemSubsystem: no UDTSimSubsystem in this world; gems will not step"));
	}
	EnsureRenderer();
}

void UDTGemSubsystem::EnsureRenderer()
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
	Renderer = World->SpawnActor<ADTPickupRenderer>(ADTPickupRenderer::StaticClass(), FTransform::Identity, Params);
	if (Renderer)
	{
		Renderer->Setup();
	}
}

void UDTGemSubsystem::CheckRunRestart(int64 StepIndex, bool bStepping)
{
	// UDTSimSubsystem::StartRun rewinds the step index: a new run starts with an empty floor. The Pickups phase sees a
	// rewind when its step does not advance; a drop (Spawn, SpawnChest) comes later in the same step (Director phase,
	// StepIndex == LastStepped) or between steps (LastStepped + 1), so only a smaller index is a rewind for it.
	if (LastStepped >= 0 && (bStepping ? StepIndex <= LastStepped : StepIndex < LastStepped))
	{
		Pool.Reset();
		LastStepped = -1;
		SimTime = 0.0;
		LastLevelUpTime.Reset();
	}
}

void UDTGemSubsystem::SyncPlayers(int32 NumPlayers)
{
	if (Stats.Num() != NumPlayers)
	{
		Stats.SetNum(NumPlayers);
	}
	if (LastLevelUpTime.Num() != NumPlayers)
	{
		LastLevelUpTime.Init(-1.0, NumPlayers);
	}
}

EDTGemSpawnResult UDTGemSubsystem::Spawn(const FDTGemSpawn& Gem)
{
	const int64 StepIndex = Sim.IsValid() ? Sim->GetStepIndex() : 0;
	CheckRunRestart(StepIndex, false);
	const EDTGemSpawnResult Result = Pool.Spawn(Gem, StepIndex);
	if (Result != EDTGemSpawnResult::Rejected)
	{
		if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
		{
			Telemetry->Records().RecordGemsSpawned(1);
			if (Result == EDTGemSpawnResult::Merged)
			{
				Telemetry->Records().RecordGemsMerged(1);
			}
			Telemetry->Records().SetGemsLive(Pool.Num()); // drops land after this step's Pickups phase
		}
	}
	return Result;
}

int32 UDTGemSubsystem::SpawnChest(const FVector2f& Position)
{
	CheckRunRestart(Sim.IsValid() ? Sim->GetStepIndex() : 0, false);
	return Pool.SpawnChest(Position);
}

int32 UDTGemSubsystem::MagnetAll(int32 OwnerId)
{
	UDTSimSubsystem* SimSystem = Sim.Get();
	if (SimSystem && !SimSystem->GetRun().IsValidPlayer(OwnerId))
	{
		return 0;
	}
	return Pool.MagnetAll(OwnerId);
}

void UDTGemSubsystem::SetStats(int32 OwnerId, const FDTProgressionStats& InStats)
{
	if (OwnerId < 0)
	{
		return;
	}
	if (Stats.Num() <= OwnerId)
	{
		Stats.SetNum(OwnerId + 1);
	}
	Stats[OwnerId] = InStats;
}

void UDTGemSubsystem::RaiseLevels()
{
	UDTSimSubsystem* SimSystem = Sim.Get();
	if (!SimSystem || LevelScratch.Num() == 0)
	{
		LevelScratch.Reset();
		return;
	}
	FDTRunState& Run = SimSystem->GetRun();
	SyncPlayers(Run.GetNumPlayers());
	UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this);
	for (const FDTLevelGain& Gain : LevelScratch)
	{
		const FDTPlayerRunState& Player = Run.GetPlayer(Gain.OwnerId);
		if (Telemetry)
		{
			Telemetry->Records().RecordLevelUp(Gain.OwnerId, Gain.Level, Player.XPTotal, Player.Offer.PendingHands.Num());
		}
		LastLevelUpTime[Gain.OwnerId] = SimTime;
		OnLevelUp.Broadcast(Gain.OwnerId, Gain.Level);
	}
	LevelScratch.Reset();
}

int32 UDTGemSubsystem::GrantXP(int32 OwnerId, int64 Amount)
{
	UDTSimSubsystem* SimSystem = Sim.Get();
	if (!SimSystem)
	{
		return 0;
	}
	FDTRunState& Run = SimSystem->GetRun();
	FDTProgression::EnsureCurve(Run);
	int32 Gained = 0;
	for (int32 Player = 0; Player < Run.GetNumPlayers(); ++Player)
	{
		if (OwnerId == INDEX_NONE || OwnerId == Player)
		{
			Gained += FDTProgression::AddXP(Run, Player, Amount, &LevelScratch);
		}
	}
	RaiseLevels();
	return Gained;
}

void UDTGemSubsystem::SimStep(const FDTSimContext& Context)
{
	CheckRunRestart(Context.StepIndex, true);
	LastStepped = Context.StepIndex;
	SimTime = static_cast<double>(Context.StepIndex + 1) * Context.StepSeconds;
	if (!Context.Run || !Context.View)
	{
		return;
	}
	FDTRunState& Run = *Context.Run;
	const int32 NumPlayers = Run.GetNumPlayers();
	FDTProgression::EnsureCurve(Run);
	SyncPlayers(NumPlayers);

	// A hero that is down or no longer in the run neither binds nor keeps gems (R-PRG-08).
	const UDTHeroSubsystem* Hero = GetWorld() ? GetWorld()->GetSubsystem<UDTHeroSubsystem>() : nullptr;
	const int32 NumHeroes = FMath::Min(NumPlayers, Context.View->HeroPositions.Num());
	PresentScratch.SetNum(NumHeroes);
	RadiusScratch.SetNum(NumHeroes);
	XpGainScratch.SetNum(NumPlayers);
	for (int32 Player = 0; Player < NumPlayers; ++Player)
	{
		XpGainScratch[Player] = Stats[Player].XpGainMul;
		if (Player < NumHeroes)
		{
			PresentScratch[Player] = !(Hero && Hero->IsDown(Player));
			RadiusScratch[Player] = Stats[Player].PickupRadiusMul;
		}
	}
	FDTPickupHeroes Heroes;
	Heroes.Positions = TConstArrayView<FVector2f>(Context.View->HeroPositions.GetData(), NumHeroes);
	Heroes.Present = PresentScratch;
	Heroes.RadiusMul = RadiusScratch;

	StepResult.Reset();
	Pool.Step(Context.StepSeconds, Heroes, StepResult);

	// Chest hands (offers.md R-OFF-14) queue behind the level hands; the pool already magnetised the collector's gems.
	for (const int32 Collector : StepResult.ChestCollectors)
	{
		FDTPlayerRunState& Player = Run.GetPlayer(Collector);
		FDTProgression::QueueHand(Player.Offer, EDTHandKind::Chest, Player.Level);
	}

	// R-PRG-01: each gem credits every player; telemetry aggregates per player per step.
	StepXp.Init(0, NumPlayers);
	StepGems.Init(0, NumPlayers);
	for (const FDTGemCollect& Gem : StepResult.Gems)
	{
		if (FDTProgression::CreditPickup(Run, Gem.PlayerId, Gem.Value, XpGainScratch, &LevelScratch, &CreditScratch))
		{
			++StepGems[Gem.PlayerId];
			for (int32 Player = 0; Player < NumPlayers; ++Player)
			{
				StepXp[Player] += CreditScratch[Player];
			}
		}
	}
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		for (int32 Player = 0; Player < NumPlayers; ++Player)
		{
			if (StepGems[Player] > 0 || StepXp[Player] > 0)
			{
				Telemetry->Records().RecordXp(Player, StepGems[Player], static_cast<int32>(FMath::Min<int64>(StepXp[Player], MAX_int32)));
			}
		}
		Telemetry->Records().SetGemsLive(Pool.Num());
	}
	RaiseLevels();
}

void UDTGemSubsystem::SimRender(float Alpha)
{
	EnsureRenderer();
	if (!Renderer)
	{
		return;
	}
	const float MeshSize = Renderer->GetMeshSizeUU();
	const float Z = static_cast<float>(DTConst::SIM_PLANE_Z_UU);

	// Gems: small cubes stood on a corner (a diamond read from the top-down camera).
	const TConstArrayView<FVector2f> Pos = Pool.GetPositions();
	const TConstArrayView<FVector2f> Prev = Pool.GetPrevPositions();
	const FQuat GemRotation = FQuat(FRotator(45.0f, 0.0f, 45.0f));
	const FVector GemScale(static_cast<float>(DTConst::PICKUP_GEM_SIZE_UU) / MeshSize);
	GemTransforms.SetNum(Pos.Num(), EAllowShrinking::No);
	for (int32 Index = 0; Index < Pos.Num(); ++Index)
	{
		const FVector2f At = FMath::Lerp(Prev[Index], Pos[Index], Alpha);
		GemTransforms[Index] = FTransform(GemRotation, FVector(At.X, At.Y, Z), GemScale);
	}
	Renderer->UpdateGems(GemTransforms);

	const TConstArrayView<FVector2f> Chests = Pool.GetChestPositions();
	const FVector ChestScale(static_cast<float>(DTConst::PICKUP_CHEST_SIZE_UU) / MeshSize);
	ChestTransforms.SetNum(Chests.Num(), EAllowShrinking::No);
	for (int32 Index = 0; Index < Chests.Num(); ++Index)
	{
		ChestTransforms[Index] = FTransform(FQuat::Identity, FVector(Chests[Index].X, Chests[Index].Y, Z), ChestScale);
	}
	Renderer->UpdateChests(ChestTransforms);
}
