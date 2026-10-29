// Game mode of the S0 stress arena: spawns the fixed camera pawn and starts the run (07_Technical R-TEC-02).
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameModeBase.h"
#include "DTGameMode.generated.h"

/**
 * Default pawn ADTCameraPawn at the origin, HUD ADTHUD; on BeginPlay starts a run seeded from -DTSeed=<n> (default 0).
 *
 * Measurement hooks for Tools/Smoke.ps1 and Tools/Perf.ps1 (compiled out of Shipping, R-TEC-12):
 *   -DTShot=<file.png> [-DTShotAfter=<s>]  screenshot (with HUD) <s> seconds after run start (default C.SMOKE.SECONDS),
 *                                          then quit once the file is written
 *   -DTCsvAfter=<s>                        records the CSV profiler event "DTPerfStart" <s> seconds after run start
 *                                          (pair with -csvStartOnEvent=DTPerfStart -csvCaptureOnEventFrameCount=<n>)
 */
UCLASS()
class DUSKTRACER_API ADTGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADTGameMode();

	virtual void BeginPlay() override;

	/** Spawns the camera pawn at the origin; the arena has no player start. */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** The run seed from the command line (-DTSeed=<n>), 0 when absent. */
	static int32 GetCommandLineSeed();

private:
	/** Smoke / perf hooks; empty in Shipping. */
	void StartMeasurementHooks();
	void TakeSmokeShot();
	void PollSmokeShot();

	FString SmokeShotPath;
	int64 SmokeShotLastSize = -1;
	FTimerHandle SmokeTimer;
	FTimerHandle CsvTimer;
};
