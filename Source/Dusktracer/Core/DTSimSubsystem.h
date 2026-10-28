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

	/** Runs exactly Steps fixed steps now, independent of frame time (tests, bots, deterministic replays). */
	void StepNow(int32 Steps);

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
	TArray<IDTSimSystem*> Systems[static_cast<int32>(EDTSimPhase::Count)];
};
