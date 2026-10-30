// Game mode of the arena: starts the run, spawns and possesses the hero, ends the run on Death (R-TEC-02, R-CMB-10).
#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameModeBase.h"
#include "DTGameMode.generated.h"

/**
 * Default pawn ADTHero (one per local player, OwnerId = local player index) at the origin, followed by the ADTCameraPawn
 * (player 0); HUD ADTHUD. On BeginPlay starts a run seeded from -DTSeed=<n> (default 0) and its telemetry session
 * (02_Systems/telemetry.md); EndPlay ends the session. When every hero is down the run ends with reason Death
 * (telemetry session ended "death"; the sim keeps stepping so debug and measurement paths still run).
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
	/** Ends the telemetry session with the reason (writes the last interval and run_end, R-TEL-08). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Spawns the player's hero at the origin (the arena has no player start); player 0 also gets the follow camera. */
	virtual void RestartPlayer(AController* NewPlayer) override;

	/** True once every player's hero is down (R-CMB-10). */
	bool IsRunOver() const { return bRunOver; }

	/** The run seed from the command line (-DTSeed=<n>), 0 when absent. */
	static int32 GetCommandLineSeed();

private:
	void HandleAllHeroesDown(int64 StepIndex);
	void EndRunSession();

	/** Smoke / perf hooks; empty in Shipping. */
	void StartMeasurementHooks();
	void TakeSmokeShot();
	void PollSmokeShot();

	FString SmokeShotPath;
	int64 SmokeShotLastSize = -1;
	FTimerHandle SmokeTimer;
	FTimerHandle CsvTimer;

	UPROPERTY(Transient)
	TObjectPtr<class ADTCameraPawn> FollowCamera;

	FDelegateHandle AllDownHandle;
	bool bRunOver = false;
};
