#include "Director/DTDirector.h"
#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Engine/World.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTTelegraphRenderer.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/DTHeroSubsystem.h"
#include "Progression/DTGemSubsystem.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetrySubsystem.h"

namespace DTDirectorLocal
{
	static TAutoConsoleVariable<int32> CVarEnabled(
		TEXT("dt.Director.Enabled"), 1,
		TEXT("1 = the run director paces spawns, threats, drop-ins and drops (director.md); 0 = off (stress and smoke paths). ")
		TEXT("The command-line flag -DTNoDirector sets it to 0 at run start."),
		ECVF_Default);
}

bool UDTDirector::IsEnabledByCVar()
{
	return DTDirectorLocal::CVarEnabled.GetValueOnGameThread() != 0;
}

bool UDTDirector::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTDirector::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	Enemies = Collection.InitializeDependency<UDTEnemySubsystem>();
	Gems = Collection.InitializeDependency<UDTGemSubsystem>();
	Heroes = Collection.InitializeDependency<UDTHeroSubsystem>();
	Sink.Owner = this;
}

void UDTDirector::Deinitialize()
{
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
	}
	if (MarkerRenderer)
	{
		MarkerRenderer->Destroy();
		MarkerRenderer = nullptr;
	}
	Sink.Owner = nullptr;
	Super::Deinitialize();
}

void UDTDirector::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Sim.IsValid())
	{
		Sim = InWorld.GetSubsystem<UDTSimSubsystem>();
	}
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Register(EDTSimPhase::Director, this);
	}
	else
	{
		UE_LOG(LogDT, Error, TEXT("UDTDirector: no UDTSimSubsystem in this world; the director will not step"));
	}
}

void UDTDirector::StartRun()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("DTNoDirector")))
	{
		DTDirectorLocal::CVarEnabled->Set(0, ECVF_SetByCommandline);
	}
	bRunStarted = true;
	// Started now (not at the first step) so debug commands queued at load (-ExecCmds "DT.Dir.Time 300") find it running.
	bWasActive = IsEnabledByCVar();
	if (bWasActive)
	{
		Core.StartRun();
		LastDamageShare.Reset();
		LastDashes.Reset();
	}
	else
	{
		Core.Stop();
	}
	UE_LOG(LogDT, Log, TEXT("Director: run start (%s)"), IsEnabledByCVar() ? TEXT("on") : TEXT("off: dt.Director.Enabled 0"));
}

bool UDTDirector::IsActive() const
{
	return bRunStarted && IsEnabledByCVar();
}

void UDTDirector::SimStep(const FDTSimContext& Context)
{
	if (!Context.View || !Context.Run)
	{
		return;
	}
	const bool bActive = IsActive();
	if (!bActive)
	{
		if (bWasActive)
		{
			Core.Stop();
			Context.View->DropInMarkers.Reset();
			Context.View->bThreatBlock = false;
			UE_LOG(LogDT, Log, TEXT("Director: off"));
		}
		bWasActive = false;
		return;
	}
	if (!bWasActive)
	{
		Core.StartRun();
		LastDamageShare.Reset();
		LastDashes.Reset();
		bWasActive = true;
		UE_LOG(LogDT, Log, TEXT("Director: timeline arena1 started at step %lld (boss at %.0f s)"), Context.StepIndex, Core.GetBossSeconds());
	}

	FDTDirectorStepIn In;
	In.StepSeconds = Context.StepSeconds;
	In.StepIndex = Context.StepIndex;
	In.Rng = &Context.Run->GetRng().Get(EDTRngStream::Director);
	In.EliteRng = &Context.Run->GetRng().Get(EDTRngStream::Elite);
	In.View = Context.View;

	const int32 NumHeroes = Context.View->HeroPositions.Num();
	HeroAliveScratch.SetNum(NumHeroes);
	const UDTHeroSubsystem* HeroSystem = Heroes.Get();
	for (int32 Index = 0; Index < NumHeroes; ++Index)
	{
		HeroAliveScratch[Index] = !(HeroSystem && HeroSystem->IsDown(Index));
	}
	In.HeroAlive = HeroAliveScratch;

	ThreatScratch.Reset();
	if (const UDTEnemySubsystem* EnemySystem = Enemies.Get())
	{
		const FDTEnemyPool& Pool = EnemySystem->GetPool();
		for (const EDTEnemyRole Role : Pool.GetRoles())
		{
			++In.AliveByRole[static_cast<int32>(Role)];
		}
		In.AliveTotal = Pool.Num();
		for (const FDTThreatAction& Action : Pool.GetStepThreatActions())
		{
			ThreatScratch.Add(Action.Origin);
		}
	}
	In.ThreatOrigins = ThreatScratch;

	Core.Step(In, Sink);
}

void UDTDirector::FSink::QueueSpawn(const FDTEnemySpawn& Spawn)
{
	if (UDTEnemySubsystem* EnemySystem = Owner ? Owner->Enemies.Get() : nullptr)
	{
		EnemySystem->QueueSpawn(Spawn);
	}
}

void UDTDirector::FSink::SpawnGem(const FDTGemSpawn& Gem)
{
	if (UDTGemSubsystem* GemSystem = Owner ? Owner->Gems.Get() : nullptr)
	{
		GemSystem->Spawn(Gem);
	}
}

void UDTDirector::FSink::SpawnChest(const FVector2f& Position)
{
	if (UDTGemSubsystem* GemSystem = Owner ? Owner->Gems.Get() : nullptr)
	{
		GemSystem->SpawnChest(Position);
	}
	UE_LOG(LogDT, Log, TEXT("Director: chest at (%.0f, %.0f)"), Position.X, Position.Y);
}

void UDTDirector::FSink::BossTime(float RunSeconds)
{
	UE_LOG(LogDT, Log, TEXT("Director: boss time at %.1f s; regular spawns, beats and drop-ins stop (boss request)"), RunSeconds);
	if (Owner)
	{
		Owner->OnBossTime.Broadcast(RunSeconds);
	}
}

void UDTDirector::FSink::BlockEnded(FDTTelDirBlock& Block)
{
	UE_LOG(LogDT, Log, TEXT("Director: block %d %s %.1f-%.1f s ended_by=%s spawned=%d/%d/%d/%d skipped=%d dropins=%d threats=%d kills=%d alive_peak=%d"),
		Block.Block, *Block.Phase, Block.StartSeconds, Block.EndSeconds, *Block.EndedBy, Block.SpawnedByRole[0], Block.SpawnedByRole[1],
		Block.SpawnedByRole[2], Block.SpawnedByRole[3], Block.SkippedAtCap, Block.DropIns, Block.ThreatActions, Block.Kills, Block.AlivePeak);
	UDTTelemetrySubsystem* Telemetry = Owner ? UDTTelemetrySubsystem::Find(Owner) : nullptr;
	if (!Telemetry)
	{
		return;
	}
	FDTTelemetryCore& Records = Telemetry->Records();
	const int32 NumPlayers = Owner->Sim.IsValid() ? Owner->Sim->GetRun().GetNumPlayers() : 0;
	Owner->LastDamageShare.SetNumZeroed(NumPlayers);
	Owner->LastDashes.SetNumZeroed(NumPlayers);
	Block.DamageTakenShare.SetNum(NumPlayers);
	Block.Dashes.SetNum(NumPlayers);
	for (int32 Player = 0; Player < NumPlayers; ++Player)
	{
		const double Share = Records.GetRunDamageTakenShare(Player);
		const int32 Dashes = Records.GetRunDashes(Player);
		Block.DamageTakenShare[Player] = static_cast<float>(Share - Owner->LastDamageShare[Player]);
		Block.Dashes[Player] = Dashes - Owner->LastDashes[Player];
		Owner->LastDamageShare[Player] = Share;
		Owner->LastDashes[Player] = Dashes;
	}
	Records.RecordDirBlock(Block);
}

void UDTDirector::EnsureRenderer()
{
	UWorld* World = GetWorld();
	if (MarkerRenderer || !World)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MarkerRenderer = World->SpawnActor<ADTTelegraphRenderer>(ADTTelegraphRenderer::StaticClass(), FTransform::Identity, Params);
	if (MarkerRenderer)
	{
		// R-DIR-07, R-PRE-03: drop-in markers are red projected ground marks, like every telegraph.
		MarkerRenderer->InitRender(DTPal::Linear(DTPal::Telegraph), DTConst::ENEMIES_TELEGRAPH_EMISSIVE);
	}
}

void UDTDirector::SimRender(float Alpha)
{
	UDTSimSubsystem* SimSystem = Sim.Get();
	if (!SimSystem)
	{
		return;
	}
	const TArray<FDTDropInMarker>& Markers = SimSystem->GetView().DropInMarkers;
	if (Markers.Num() == 0 && !MarkerRenderer)
	{
		return; // nothing ever drawn: no actor
	}
	EnsureRenderer();
	if (!MarkerRenderer)
	{
		return;
	}
	// A marker is a filled disc where the body lands: its radius plus the separation it claims among its neighbours.
	// Progress runs from publication (0) to landing (1); the renderer reads it like any telegraph's.
	const float LeadSteps = FMath::Max(1.0f, FMath::RoundToFloat(DTConst::DIR_DROPIN_LEAD_S * static_cast<float>(DTConst::SIM_FIXED_HZ)));
	const int64 Now = SimSystem->GetStepIndex();
	MarkerScratch.Reset();
	for (const FDTDropInMarker& Marker : Markers)
	{
		FDTTelegraph& T = MarkerScratch.AddDefaulted_GetRef();
		T.Shape = EDTTelegraphShape::Ring;
		T.Origin = Marker.Position;
		T.Length = FDTEnemyRoleDef::Get(Marker.Role).Radius + static_cast<float>(DTConst::ENM_SEPARATION_RADIUS_UU);
		T.Width = 0.0f;
		T.Progress = FMath::Clamp(1.0f - static_cast<float>(Marker.LandStep - Now) / LeadSteps, 0.0f, 1.0f);
	}
	MarkerRenderer->SetTelegraphs(MarkerScratch);
}
