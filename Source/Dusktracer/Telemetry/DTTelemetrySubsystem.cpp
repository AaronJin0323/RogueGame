#include "Telemetry/DTTelemetrySubsystem.h"
#include "Combat/DTBulletPool.h"
#include "Combat/DTBulletSubsystem.h"
#include "Core/DTRunState.h"
#include "Dusktracer.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/World.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/DateTime.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#endif

// The build hash can be baked in by the build (PublicDefinitions DT_BUILD_HASH="<git short hash>"); unknown otherwise.
#ifndef DT_BUILD_HASH
#define DT_BUILD_HASH "unknown"
#endif

bool UDTTelemetrySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTTelemetrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
}

void UDTTelemetrySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Sim.IsValid())
	{
		Sim = InWorld.GetSubsystem<UDTSimSubsystem>();
	}
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Register(EDTSimPhase::Director, this);
		SimSystem->GetRun().SetEventSink(&Core);
	}
}

void UDTTelemetrySubsystem::Deinitialize()
{
	EndSession(TEXT("world_end"));
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
		if (SimSystem->GetRun().GetEventSink() == &Core)
		{
			SimSystem->GetRun().SetEventSink(nullptr);
		}
	}
	Sim.Reset();
	Writer.Close();
	Super::Deinitialize();
}

UDTTelemetrySubsystem* UDTTelemetrySubsystem::Find(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UDTTelemetrySubsystem>() : nullptr;
}

void UDTTelemetrySubsystem::BeginResolve(int64 StepIndex, TConstArrayView<FDTHit> Hits)
{
	UDTSimSubsystem* SimSystem = Sim.Get();
	Core.BeginResolve(StepIndex, Hits, SimSystem ? TConstArrayView<uint32>(SimSystem->GetView().EnemySerials) : TConstArrayView<uint32>());
}

FString UDTTelemetrySubsystem::GetBuildHash()
{
	return FString(TEXT(DT_BUILD_HASH));
}

void UDTTelemetrySubsystem::StartSession(int32 RunSeed, int32 NumPlayers)
{
	if (Core.IsActive())
	{
		EndSession(TEXT("restart"));
	}
	const FDateTime Now = FDateTime::Now();
	FDTTelemetryConfig Config;
	Config.Seed = RunSeed;
	Config.NumPlayers = NumPlayers;
	if (const UDTSimSubsystem* SimSystem = Sim.Get())
	{
		Config.StepSeconds = static_cast<float>(SimSystem->GetClock().GetStepSeconds());
	}
	Config.Build = GetBuildHash();
	Config.Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	Config.InputDevice = TEXT("unknown"); // S1: the hero's input reports the last device used
	Config.WallStart = Now.ToIso8601();

	Writer.Open(FDTTelemetryWriter::MakeBaseName(RunSeed, Now));
	Core.Start(Config);
	Flush();
	UE_LOG(LogDT, Log, TEXT("Telemetry: session seed=%d players=%d -> %s"), RunSeed, Config.NumPlayers, *Writer.GetCsvPath());
}

void UDTTelemetrySubsystem::EndSession(const FString& Reason)
{
	if (!Core.IsActive())
	{
		return;
	}
	Core.End(Reason);
	Flush();
	Writer.Wait();
	UE_LOG(LogDT, Log, TEXT("Telemetry: session end (%s), %lld steps -> %s"), *Reason, Core.GetStepsDone(), *Writer.GetJsonlPath());
}

void UDTTelemetrySubsystem::Flush()
{
	if (!Core.HasOutput())
	{
		return;
	}
	FString Csv;
	FString Jsonl;
	Core.TakeOutput(Csv, Jsonl);
	Writer.Append(MoveTemp(Csv), MoveTemp(Jsonl));
}

void UDTTelemetrySubsystem::SimStep(const FDTSimContext& Context)
{
	if (!Core.IsActive())
	{
		return;
	}
	UWorld* World = GetWorld();
	FDTTelemetrySample Sample;
	if (const UDTBulletSubsystem* Bullets = World ? World->GetSubsystem<UDTBulletSubsystem>() : nullptr)
	{
		Sample.LiveBullets = Bullets->GetPool().Num();
		Sample.RecycledBulletsTotal = Bullets->GetPool().GetCounters().Recycled;
	}
	if (const UDTEnemySubsystem* Enemies = World ? World->GetSubsystem<UDTEnemySubsystem>() : nullptr)
	{
		Sample.LiveEnemies = Enemies->GetLiveCount();
		Sample.RefusedSpawnsTotal = Enemies->GetRefusedSpawns();
		Sample.Enemies = Enemies->GetPool().GetPositions();
	}
	if (Context.View)
	{
		Sample.Heroes = Context.View->HeroPositions;
	}
	if (Context.Run)
	{
		LevelScratch.SetNum(Context.Run->GetNumPlayers(), EAllowShrinking::No);
		XpScratch.SetNum(Context.Run->GetNumPlayers(), EAllowShrinking::No);
		for (int32 Player = 0; Player < LevelScratch.Num(); ++Player)
		{
			LevelScratch[Player] = Context.Run->GetPlayer(Player).Level;
			XpScratch[Player] = Context.Run->GetPlayer(Player).XPTotal;
		}
		Sample.Levels = LevelScratch;
		Sample.XpTotals = XpScratch;
	}

	const int32 IntervalBefore = Core.GetIntervalIndex();
	Core.EndStep(Context.StepIndex, Sample);
	if (Core.GetIntervalIndex() != IntervalBefore)
	{
		Flush(); // one flush per interval (R-TEL-08); the JSONL in between waits in memory
	}
}

#if !UE_BUILD_SHIPPING
namespace DTTelemetryConsole
{
	static UDTTelemetrySubsystem* Find(UWorld* World)
	{
		UDTTelemetrySubsystem* Telemetry = World ? World->GetSubsystem<UDTTelemetrySubsystem>() : nullptr;
		if (!Telemetry)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Stats: no telemetry subsystem in this world (Game/PIE only)"));
		}
		return Telemetry;
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		const UDTTelemetrySubsystem* Telemetry = Find(World);
		if (!Telemetry)
		{
			return;
		}
		const FDTTelemetryCore& Core = Telemetry->GetCore();
		if (!Core.IsActive())
		{
			UE_LOG(LogDT, Log, TEXT("DT.Stats: no session (the game mode starts one with the run)"));
			return;
		}
		for (int32 Player = 0; Player < Core.GetNumPlayers(); ++Player)
		{
			const FDTTelemetryRow R = Core.ComputeRow(Player);
			const double PerUnit = R.ElapsedSeconds > 0.0 ? DTConst::TEL_RATE_UNIT_S / R.ElapsedSeconds : 0.0;
			UE_LOG(LogDT, Log, TEXT("DT.Stats interval=%d t=%.1fs (+%.1fs) player=%d kills/min=%.1f hits/min=%.1f dmg/min=%.1f ttk=%s "
				"nearest=%s level=%d taken/min=%.3f dashes/min=%.1f threats/min=%.1f device=%s live bullets=%d enemies=%d recycled=%lld refused=%lld"),
				R.Interval, R.RunTimeSeconds, R.ElapsedSeconds, R.Player, R.Kills * PerUnit, R.Hits * PerUnit, R.DamageDealt * PerUnit,
				R.bHasTtk ? *FString::Printf(TEXT("%.3fs"), R.SwarmTtkSeconds) : TEXT("-"),
				R.bHasNearest ? *FString::Printf(TEXT("%.0fuu"), R.NearestEnemyUU) : TEXT("-"),
				R.Level, R.DamageTakenShare * PerUnit, R.Dashes * PerUnit, R.ThreatActions * PerUnit, DTTelNames::InputDevice(R.InputDevice), R.LiveBullets, R.LiveEnemies, R.RecycledBullets, R.RefusedSpawns);
		}
		UE_LOG(LogDT, Log, TEXT("DT.Stats files: %s (.jsonl)"), *Telemetry->GetWriter().GetCsvPath());
	}

	static void FlushNow(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTTelemetrySubsystem* Telemetry = Find(World))
		{
			Telemetry->Flush();
			UE_LOG(LogDT, Log, TEXT("DT.Stats.Flush: queued -> %s"), *Telemetry->GetWriter().GetJsonlPath());
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs StatsCommand(TEXT("DT.Stats"),
		TEXT("Prints the telemetry numbers of the interval in progress, per player."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
	static FAutoConsoleCommandWithWorldAndArgs FlushCommand(TEXT("DT.Stats.Flush"),
		TEXT("Writes the buffered telemetry (CSV rows and JSONL events) to Saved/Playtests now."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FlushNow));
}
#endif
