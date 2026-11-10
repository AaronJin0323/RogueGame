#include "Game/DTGameMode.h"
#include "Camera/PlayerCameraManager.h"
#include "CoreGlobals.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Boss/DTBossSubsystem.h"
#include "Director/DTDirector.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "Game/DTCameraPawn.h"
#include "Game/DTHUD.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "Player/DTHero.h"
#include "Player/DTHeroSubsystem.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Telemetry/DTTelemetrySubsystem.h"

#if !UE_BUILD_SHIPPING
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "UnrealClient.h"

namespace DTGameModeLocal
{
	// How often the smoke hook checks that the screenshot file is complete (size stable across two polls).
	constexpr float ShotPollSeconds = 0.5f;
	// Rendered frames between holding the sim on the shot's step and the screenshot: motion vectors and the temporal
	// upscaler's history settle on the still frame.
	constexpr int32 ShotSettleFrames = 30;}
#endif

ADTGameMode::ADTGameMode()
{
	DefaultPawnClass = ADTHero::StaticClass();
	HUDClass = ADTHUD::StaticClass();
}

FDTRunSeed ADTGameMode::GetRunSeed(const UWorld* World)
{
	UDTSimSubsystem* const Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
	return Sim ? ParseSeed(World->URL, Sim->GetLaunchSeed()) : FDTRunSeed::ForThisLaunch();
}

FDTRunSeed ADTGameMode::ParseSeed(const FURL& Url, const FDTRunSeed& Launch)
{
	const TCHAR* const Option = Url.GetOption(TEXT("DTSeed="), nullptr);
	if (!Option)
	{
		return Launch;
	}
	FDTRunSeed Result;
	Result.Source = FDTRunSeed::ParseValue(FString(Option), Result.Seed) ? EDTSeedSource::Explicit : EDTSeedSource::Rejected;
	return Result;
}

int32 ADTGameMode::NextSeed(int32 Current, uint32 Entropy)
{
	// One range for every fresh seed (07_Technical R-TEC-16); a hit on the old seed steps past it.
	const int32 Fresh = FDTRunSeed::FreshFromEntropy(HashCombine(GetTypeHash(Current), Entropy));
	return Fresh == Current ? (Current == FDTRunSeed::FreshMin ? FDTRunSeed::FreshMin + 1 : FDTRunSeed::FreshMin) : Fresh;
}

void ADTGameMode::AbandonRun()
{
	bRunOver = true;
}

void ADTGameMode::BeginPlay()
{
	Super::BeginPlay();
	UWorld* const MyWorld = GetWorld();
	UDTSimSubsystem* const Sim = MyWorld ? MyWorld->GetSubsystem<UDTSimSubsystem>() : nullptr;
	if (!Sim)
	{
		UE_LOG(LogDT, Error, TEXT("ADTGameMode: no UDTSimSubsystem in this world; run not started"));
		return;
	}
	const FDTRunSeed RunSeed = GetRunSeed(MyWorld);
	if (RunSeed.Source == EDTSeedSource::Rejected)
	{
		// 07_Technical R-TEC-16: no run on a seed nobody asked for. Only a forced exit carries the code on Windows, and with no
		// run started there is nothing to save; Play in the editor keeps the editor, and whatever it has unsaved, open.
		UE_LOG(LogDT, Error, TEXT("Run not started: -DTSeed must be a whole number from %d to %d"), MIN_int32, MAX_int32);
		if (!GIsEditor)
		{
			FPlatformMisc::RequestExitWithStatus(true, FDTRunSeed::RejectedExitCode, TEXT("DT rejected -DTSeed"));
		}
		return;
	}
	const int32 Seed = RunSeed.Seed;
	Sim->StartRun(Seed, 1);
	if (UDTTelemetrySubsystem* const Telemetry = MyWorld->GetSubsystem<UDTTelemetrySubsystem>())
	{
		Telemetry->StartSession(Seed, Sim->GetRun().GetNumPlayers());
	}
	if (UDTDirector* const Director = MyWorld->GetSubsystem<UDTDirector>())
	{
		Director->StartRun(); // director.md R-DIR-03: the timeline runs from run start (-DTNoDirector: off)
	}
	if (UDTHeroSubsystem* const Hero = MyWorld->GetSubsystem<UDTHeroSubsystem>())
	{
		AllDownHandle = Hero->OnAllHeroesDown.AddUObject(this, &ADTGameMode::HandleAllHeroesDown);
	}
	if (UDTBossSubsystem* const Boss = MyWorld->GetSubsystem<UDTBossSubsystem>())
	{
		// The boss system logs the victory and closes the telemetry session itself (boss.md); the run is over here.
		BossDefeatedHandle = Boss->OnBossDefeated.AddWeakLambda(this, [this](int64)
		{
			if (!bRunOver)
			{
				bRunOver = true;
				bVictory = true;
			}
		});
	}
	StartMeasurementHooks();
}

void ADTGameMode::HandleAllHeroesDown(int64 StepIndex)
{
	if (bRunOver)
	{
		return;
	}
	bRunOver = true;
	UE_LOG(LogDT, Log, TEXT("Run end reason=death step=%lld"), StepIndex);
	// Called from inside the step: the telemetry session closes on the next tick, after the step's own records.
	GetWorldTimerManager().SetTimerForNextTick(this, &ADTGameMode::EndRunSession);
}

void ADTGameMode::EndRunSession()
{
	UWorld* const MyWorld = GetWorld();
	if (UDTTelemetrySubsystem* const Telemetry = MyWorld ? MyWorld->GetSubsystem<UDTTelemetrySubsystem>() : nullptr)
	{
		Telemetry->EndSession(TEXT("death"));
	}
}

void ADTGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UWorld* const MyWorld = GetWorld();
	if (UDTHeroSubsystem* const Hero = MyWorld ? MyWorld->GetSubsystem<UDTHeroSubsystem>() : nullptr)
	{
		Hero->OnAllHeroesDown.Remove(AllDownHandle);
	}
	if (UDTBossSubsystem* const Boss = GetWorld() ? GetWorld()->GetSubsystem<UDTBossSubsystem>() : nullptr)
	{
		Boss->OnBossDefeated.Remove(BossDefeatedHandle);
	}
	if (UDTTelemetrySubsystem* const Telemetry = MyWorld ? MyWorld->GetSubsystem<UDTTelemetrySubsystem>() : nullptr)
	{
		const TCHAR* Reason = TEXT("end_play");
		switch (EndPlayReason)
		{
		case EEndPlayReason::Destroyed: Reason = TEXT("destroyed"); break;
		case EEndPlayReason::LevelTransition: Reason = TEXT("level_transition"); break;
		case EEndPlayReason::EndPlayInEditor: Reason = TEXT("end_pie"); break;
		case EEndPlayReason::RemovedFromWorld: Reason = TEXT("removed_from_world"); break;
		case EEndPlayReason::Quit: Reason = TEXT("quit"); break;
		}
		Telemetry->EndSession(Reason);
	}
	if (UDTSimSubsystem* const Sim = MyWorld ? MyWorld->GetSubsystem<UDTSimSubsystem>() : nullptr)
	{
		Sim->OnStepEnd.Remove(SmokeStepHandle);
	}
	if (SmokePollHandle.IsValid())
	{
		FTSTicker::RemoveTicker(SmokePollHandle);
		SmokePollHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void ADTGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer || NewPlayer->IsPendingKillPending())
	{
		return;
	}
	APlayerController* const PC = Cast<APlayerController>(NewPlayer);
	if (PC)
	{
		// The follow camera is the view target, not the possessed hero.
		PC->bAutoManageActiveCameraTarget = false;
	}
	RestartPlayerAtTransform(NewPlayer, FTransform::Identity);
	ADTHero* const Hero = Cast<ADTHero>(NewPlayer->GetPawn());
	if (!PC || !Hero || Hero->GetPlayerIndex() != 0)
	{
		return;
	}
	UWorld* const MyWorld = GetWorld();
	if (!FollowCamera && MyWorld)
	{
		FActorSpawnParameters Params;
		Params.ObjectFlags |= RF_Transient;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		FollowCamera = MyWorld->SpawnActor<ADTCameraPawn>(ADTCameraPawn::StaticClass(), ADTCameraPawn::FollowTransform(), Params);
	}
	if (FollowCamera)
	{
		// The offset from the hero is FollowTransform: the actor itself is the view (ADTCameraPawn::FollowTransform).
		FollowCamera->Follow(Hero);
		PC->SetViewTarget(FollowCamera);
		// The run-start Patron hand pauses the world before the first frame, and a paused world never updates the camera
		// manager: fill its cache now so the hand is drawn with the camera's field of view (the view point itself is the
		// actor's transform until time passes, see ADTCameraPawn::FollowTransform).
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->UpdateCamera(0.0f);
		}
	}
}

#if !UE_BUILD_SHIPPING
void ADTGameMode::StartMeasurementHooks()
{
	FTimerManager& Timers = GetWorldTimerManager();

	FString ShotPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("DTShot="), ShotPath) && !ShotPath.IsEmpty())
	{
		SmokeShotPath = FPaths::ConvertRelativePathToFull(ShotPath);
		IFileManager::Get().Delete(*SmokeShotPath, false, true, true);
		float After = static_cast<float>(DTConst::SMOKE_SECONDS);
		FParse::Value(FCommandLine::Get(), TEXT("DTShotAfter="), After);
		// Counted in sim steps, not world time: a hitch drops steps (FDTFixedStep), so a world timer fired on a sim state
		// that depended on the machine's load, and the contrast frame could miss a round or a telegraph (gate step `contrast`).
		UDTSimSubsystem* const Sim = GetWorld() ? GetWorld()->GetSubsystem<UDTSimSubsystem>() : nullptr;
		if (Sim)
		{
			SmokeShotStep = Sim->GetStepIndex() + FMath::Max<int64>(1, FMath::RoundToInt64(static_cast<double>(After) * DTConst::SIM_FIXED_HZ));
			SmokeStepHandle = Sim->OnStepEnd.AddUObject(this, &ADTGameMode::HandleSmokeStep);
			UE_LOG(LogDT, Log, TEXT("Smoke: screenshot of sim step %lld (%.1f s of sim time) to %s"), SmokeShotStep, After, *SmokeShotPath);
		}
	}

	float CsvAfter = -1.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("DTCsvAfter="), CsvAfter) && CsvAfter >= 0.0f)
	{
		Timers.SetTimer(CsvTimer, FTimerDelegate::CreateLambda([]()
		{
			CSV_EVENT_GLOBAL(TEXT("DTPerfStart"));
			UE_LOG(LogDT, Log, TEXT("Perf: CSV event DTPerfStart"));
		}), FMath::Max(CsvAfter, UE_KINDA_SMALL_NUMBER), false);
		UE_LOG(LogDT, Log, TEXT("Perf: CSV event DTPerfStart in %.1f s"), CsvAfter);
	}
}

void ADTGameMode::HandleSmokeStep()
{
	UDTSimSubsystem* const Sim = GetWorld() ? GetWorld()->GetSubsystem<UDTSimSubsystem>() : nullptr;
	if (!Sim || Sim->GetStepIndex() < SmokeShotStep)
	{
		return;
	}
	Sim->OnStepEnd.Remove(SmokeStepHandle);
	SmokeStepHandle.Reset();
	// Hold the sim on this step (it ends this frame's loop after the step, and every later frame renders the step itself)
	// until the file is written and the game quits. A frame in motion depends on the frame rate: at a low one the temporal
	// upscaler smears rounds and tracers into streaks and the contrast check finds fewer of them, or none. So the shot waits
	// until the still frame has settled. The world is not paused: a paused world keeps its last motion vectors.
	Sim->Hold();
	const uint64 ShootAtFrame = GFrameCounter + DTGameModeLocal::ShotSettleFrames;
	UE_LOG(LogDT, Log, TEXT("Smoke: sim step %lld reached and held; screenshot after %d settled frames"), Sim->GetStepIndex(),
		DTGameModeLocal::ShotSettleFrames);
	SmokePollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, ShootAtFrame](float)
	{
		if (GFrameCounter < ShootAtFrame)
		{
			return true;
		}
		TakeSmokeShot();
		return false;
	}));
}

void ADTGameMode::TakeSmokeShot()
{
	FScreenshotRequest::RequestScreenshot(SmokeShotPath, /*bInShowUI*/ true, /*bAddFilenameSuffix*/ false);
	SmokeShotLastSize = -1;
	// The core ticker, not a world timer: world timers stop while the world is held paused.
	SmokePollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ADTGameMode::PollSmokeShot), DTGameModeLocal::ShotPollSeconds);
}

bool ADTGameMode::PollSmokeShot(float DeltaTime)
{
	const int64 Size = IFileManager::Get().FileSize(*SmokeShotPath);
	if (Size > 0 && Size == SmokeShotLastSize)
	{
		SmokePollHandle.Reset();
		UE_LOG(LogDT, Log, TEXT("Smoke: screenshot written (%lld bytes); quitting"), Size);
		FPlatformMisc::RequestExit(false, TEXT("DT smoke screenshot written"));
		return false;
	}
	SmokeShotLastSize = Size;
	return true;
}
#else
void ADTGameMode::StartMeasurementHooks() {}
void ADTGameMode::HandleSmokeStep() {}
void ADTGameMode::TakeSmokeShot() {}
bool ADTGameMode::PollSmokeShot(float DeltaTime) { return false; }
#endif
