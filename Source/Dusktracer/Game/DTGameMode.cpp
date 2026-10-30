#include "Game/DTGameMode.h"
#include "Camera/PlayerCameraManager.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "Game/DTCameraPawn.h"
#include "Game/DTHUD.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Player/DTHero.h"
#include "Player/DTHeroSubsystem.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Telemetry/DTTelemetrySubsystem.h"

#if !UE_BUILD_SHIPPING
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/Paths.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "UnrealClient.h"

namespace DTGameModeLocal
{
	// How often the smoke hook checks that the screenshot file is complete (size stable across two polls).
	constexpr float ShotPollSeconds = 0.5f;
}
#endif

ADTGameMode::ADTGameMode()
{
	DefaultPawnClass = ADTHero::StaticClass();
	HUDClass = ADTHUD::StaticClass();
}

int32 ADTGameMode::GetCommandLineSeed()
{
	int32 Seed = 0;
	FParse::Value(FCommandLine::Get(), TEXT("DTSeed="), Seed);
	return Seed;
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
	const int32 Seed = GetCommandLineSeed();
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
		// Keep the spawn transform as the offset from the hero: the actor itself is the view (ADTCameraPawn::FollowTransform).
		FollowCamera->AttachToActor(Hero, FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, false));
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
		Timers.SetTimer(SmokeTimer, this, &ADTGameMode::TakeSmokeShot, FMath::Max(After, UE_KINDA_SMALL_NUMBER), false);
		UE_LOG(LogDT, Log, TEXT("Smoke: screenshot in %.1f s to %s"), After, *SmokeShotPath);
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

void ADTGameMode::TakeSmokeShot()
{
	FScreenshotRequest::RequestScreenshot(SmokeShotPath, /*bInShowUI*/ true, /*bAddFilenameSuffix*/ false);
	SmokeShotLastSize = -1;
	GetWorldTimerManager().SetTimer(SmokeTimer, this, &ADTGameMode::PollSmokeShot, DTGameModeLocal::ShotPollSeconds, true);
}

void ADTGameMode::PollSmokeShot()
{
	const int64 Size = IFileManager::Get().FileSize(*SmokeShotPath);
	if (Size > 0 && Size == SmokeShotLastSize)
	{
		GetWorldTimerManager().ClearTimer(SmokeTimer);
		UE_LOG(LogDT, Log, TEXT("Smoke: screenshot written (%lld bytes); quitting"), Size);
		FPlatformMisc::RequestExit(false, TEXT("DT smoke screenshot written"));
		return;
	}
	SmokeShotLastSize = Size;
}
#else
void ADTGameMode::StartMeasurementHooks() {}
void ADTGameMode::TakeSmokeShot() {}
void ADTGameMode::PollSmokeShot() {}
#endif
