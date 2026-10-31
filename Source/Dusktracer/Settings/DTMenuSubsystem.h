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

private:
	void HandleOutcome(const FDTMenuOutcome& Outcome);
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
#endif

	FDTMenuState State;
	TSet<FKey> Down;
	TFunction<void(bool)> PauseOverride;
	double OpenedWall = 0.0;
	bool bPausedByMenu = false;
};
