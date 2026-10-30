// The fixed-step clock for the whole simulation (07_Technical R-TEC-03) and the run it advances (R-TEC-01, R-TEC-02).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTFixedStep.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "DTSimSubsystem.generated.h"

/**
 * Advances every registered IDTSimSystem in phase order, a whole number of fixed steps per frame, then lets each
 * render with the interpolation alpha. Gameplay never ticks on its own: it registers here.
 */
UCLASS()
class DUSKTRACER_API UDTSimSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Starts a new run: reseeds every stream, resets per-player state and the shared view. */
	void StartRun(int32 RunSeed, int32 NumPlayers);

	void Register(EDTSimPhase Phase, IDTSimSystem* System);
	void Unregister(IDTSimSystem* System);

	/** Runs exactly Steps fixed steps now, independent of frame time (tests, bots, deterministic replays). Ignores the pause request. */
	void StepNow(int32 Steps);

	/**
	 * One frame of the fixed-step clock (what Tick runs): adds DeltaTime and runs the steps due, checking the pause request
	 * between steps (07_Technical R-TEC-14). A request made during a step (from OnStepEnd) stops the loop after that step;
	 * the steps still due are dropped, so sim time does not pass while paused. While the request stands no time is added
	 * and no step runs. Returns the number of steps run.
	 */
	int32 AdvanceFrame(float DeltaTime);

	/** Pause at the next step boundary (an open hand in solo, offers.md R-OFF-11): held until ClearPauseRequest. */
	void RequestPause() { bPauseRequested = true; }
	void ClearPauseRequest() { bPauseRequested = false; }
	bool IsPauseRequested() const { return bPauseRequested; }

	/** Broadcast after every step (both AdvanceFrame and StepNow), before the pause check: systems that queue a pause bind here. */
	FSimpleMulticastDelegate OnStepEnd;

	FDTRunState& GetRun() { return Run; }
	FDTWorldView& GetView() { return View; }
	const FDTFixedStep& GetClock() const { return Clock; }
	int64 GetStepIndex() const { return StepIndex; }

private:
	void RunOneStep();

	FDTFixedStep Clock;
	FDTRunState Run;
	FDTWorldView View;
	int64 StepIndex = 0;
	bool bPauseRequested = false;
	TArray<IDTSimSystem*> Systems[static_cast<int32>(EDTSimPhase::Count)];
};
