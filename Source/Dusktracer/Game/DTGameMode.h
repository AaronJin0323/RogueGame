// Game mode of the arena: starts the run, spawns and possesses the hero, ends the run on Death (R-TEC-02, R-CMB-10).
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Core/DTRunSeed.h"
#include "Engine/TimerHandle.h"
#include "GameFramework/GameModeBase.h"
#include "DTGameMode.generated.h"

/**
 * Default pawn ADTHero (one per local player, OwnerId = local player index) at the origin, followed by the ADTCameraPawn
 * (player 0); HUD ADTHUD. On BeginPlay starts a run on GetRunSeed (07_Technical R-TEC-16) and its telemetry session
 * (02_Systems/telemetry.md) and the director (UDTDirector; -DTNoDirector keeps it off); EndPlay ends the session. When every hero is down the run ends with reason Death
 * (telemetry session ended "death"; the sim keeps stepping so debug and measurement paths still run).
 *
 * Measurement hooks for Tools/Smoke.ps1 and Tools/Perf.ps1 (compiled out of Shipping, R-TEC-12):
 *   -DTShot=<file.png> [-DTShotAfter=<s>]  screenshot (with HUD) of the sim step <s> seconds of sim time after run start
 *                                          (default C.SMOKE.SECONDS), then quit once the file is written. The sim is held
 *                                          on that step (UDTSimSubsystem::Hold) and shot once the still frame has settled,
 *                                          so neither a hitch (dropped steps) nor the frame rate (temporal smear) moves
 *                                          what the frame shows (the contrast step reads rounds and telegraphs off it)
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

	/** True once the boss is defeated (boss.md R-BOS; the run ends in victory). */
	bool IsVictory() const { return bVictory; }

	/** The seed of this world's run: the map's DTSeed option (a restart, settings.md R-SET-24), else UDTSimSubsystem::GetLaunchSeed. */
	static FDTRunSeed GetRunSeed(const UWorld* World);
	/** Url's DTSeed option, read by the rule -DTSeed is (07_Technical R-TEC-16); with none, Launch. */
	static FDTRunSeed ParseSeed(const FURL& Url, const FDTRunSeed& Launch);

	/** A fresh seed from the wall clock, never Current (R-SET-26). */
	static int32 NextSeed(int32 Current, uint32 Entropy);

	/** The player gave the run up: it now reads as over (R-SET-22); the menu closes its telemetry. */
	void AbandonRun();

private:
	void HandleAllHeroesDown(int64 StepIndex);
	void EndRunSession();

	/** Smoke / perf hooks; empty in Shipping. */
	void StartMeasurementHooks();
	void HandleSmokeStep();
	void TakeSmokeShot();
	bool PollSmokeShot(float DeltaTime);

	FString SmokeShotPath;
	int64 SmokeShotLastSize = -1;
	int64 SmokeShotStep = -1;
	FDelegateHandle SmokeStepHandle;
	FTSTicker::FDelegateHandle SmokePollHandle;
	FTimerHandle CsvTimer;

	UPROPERTY(Transient)
	TObjectPtr<class ADTCameraPawn> FollowCamera;

	FDelegateHandle AllDownHandle;
	FDelegateHandle BossDefeatedHandle;
	bool bRunOver = false;
	bool bVictory = false;
};
