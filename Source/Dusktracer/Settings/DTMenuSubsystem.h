// Runs the pause, settings and controls screens in a world (settings.md R-SET-16): polls keys, holds the pause, draws.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Settings/DTMenuState.h"
#include "Subsystems/WorldSubsystem.h"
#include "DTMenuSubsystem.generated.h"

class UCanvas;

UCLASS()
class DUSKTRACER_API UDTMenuSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** IA_Pause: opens the pause screen; ignored while one is open or the press that closed it is still down. */
	void RequestPause();
	/** Opens on the settings or controls screen, as the dev commands do. */
	void OpenAt(EDTMenuScreen Screen);
	void Close();
	/** The run ended: opens the run-over screen over anything (R-SET-21); Tick calls it when the game mode reads the run as over. */
	void OpenRunOver(EDTRunOverKind Kind);
	/** Closes the run if it is open and reloads the map on a new seed (R-SET-23); ForcedSeed is the proof runs' way to name it. */
	void RestartRun(TOptional<int32> ForcedSeed = TOptional<int32>());
	bool IsOpen() const { return State.Screen != EDTMenuScreen::None; }
	/** One command, as a key press gives it (tests and dev commands). */
	void Command(EDTMenuCommand Command);
	/** A key press during capture. */
	void CaptureKey(const FKey& Key);
	/** Runs a queued culture switch; called by Tick on the frame after the press (R-SET-06). */
	void RunLanguageSwitch();

	void Draw(UCanvas* Canvas);
	const FDTMenuState& GetState() const { return State; }
	/** Test seam: takes the world pause instead of the game mode. */
	void SetPauseOverride(TFunction<void(bool)> InOverride) { PauseOverride = MoveTemp(InOverride); }
	/** Test seams: receive the seed instead of reloading the map, count the exit instead of leaving, say whether Accept is down. */
	void SetRestartOverride(TFunction<void(int32)> InOverride) { RestartOverride = MoveTemp(InOverride); }
	void SetQuitOverride(TFunction<void()> InOverride) { QuitOverride = MoveTemp(InOverride); }
	void SetHeldOverride(TFunction<bool()> InOverride) { HeldOverride = MoveTemp(InOverride); }
#if !UE_BUILD_SHIPPING
	void ForceDevShot() { bDevShotForced = true; }
#endif

private:
	void HandleOutcome(const FDTMenuOutcome& Outcome);
	void AbandonRun();
	void QuitGame();
	void CloseRun(const TCHAR* Reason);
	void SnapshotKeys();
	bool IsAcceptHeld() const;
	void WatchRunOver();
	void FinishRestart();
	void SetPaused(bool bWanted);
	bool CanUnpause() const { return !IsOpen(); }
	void PollKeys();
	void RefreshHeroBindings();
#if !UE_BUILD_SHIPPING
	void TickDevShot();
	FString DevShotPath;
	double DevShotAt = -1.0;
	int64 DevShotLastSize = -1;
	bool bDevShotAsked = false;
	bool bDevShotForced = false;
#endif

	FDTMenuState State;
	TSet<FKey> Down;
	TFunction<void(bool)> PauseOverride;
	TFunction<void(int32)> RestartOverride;
	TFunction<void()> QuitOverride;
	TFunction<bool()> HeldOverride;
	double OpenedWall = 0.0;
	/** Seconds the run-over screen still drops commands (R-SET-21). */
	float RunOverGuardLeft = 0.0f;
	/** The run is closed and the reload waits for the key that chose it to come up; the menu takes nothing more. */
	bool bRestartPending = false;
	bool bLeaving = false;
	int32 PendingSeed = 0;
	bool bPausedByMenu = false;
};
