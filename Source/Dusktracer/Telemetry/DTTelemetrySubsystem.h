// The world's telemetry session: sim-clocked CSV + JSONL under Saved/Playtests (02_Systems/telemetry.md, 11_Quality §3).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetryWriter.h"
#include "DTTelemetrySubsystem.generated.h"

/**
 * Game/PIE worlds only. Registers for EDTSimPhase::Director (the end of each step, R-TEL-01) and observes every run
 * event through FDTRunState's sink (R-TEL-02). A session runs from StartSession (the game mode, with the run seed) to
 * EndSession (end of play, or world teardown); output is flushed off the game thread at every interval and at the end.
 */
UCLASS()
class DUSKTRACER_API UDTTelemetrySubsystem : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// IDTSimSystem (EDTSimPhase::Director)
	virtual void SimStep(const FDTSimContext& Context) override;

	/** Starts a session for a run that was just started with RunSeed; opens Saved/Playtests/<seed>_<time>.csv/.jsonl. */
	void StartSession(int32 RunSeed, int32 NumPlayers);
	/** Writes the last partial interval and run_end{Reason}, then waits for the files. No-op without a session. */
	void EndSession(const FString& Reason);
	/** Queues everything buffered so far for writing (never blocks). */
	void Flush();
	bool IsSessionActive() const { return Core.IsActive(); }

	// Enemy-system hooks (kill attribution and swarm TTK; see FDTTelemetryCore).
	void OnEnemiesSpawned(int64 StepIndex, bool bCleared, int32 PoolNum) { Core.OnEnemiesSpawned(StepIndex, bCleared, PoolNum); }
	void BeginResolve(int64 StepIndex, TConstArrayView<FDTHit> Hits) { Core.BeginResolve(StepIndex, Hits); }
	void EndResolve(int32 PoolNum) { Core.EndResolve(PoolNum); }

	const FDTTelemetryCore& GetCore() const { return Core; }
	const FDTTelemetryWriter& GetWriter() const { return Writer; }

	/** Build hash baked at compile time (DT_BUILD_HASH), else "unknown". */
	static FString GetBuildHash();

private:
	FDTTelemetryCore Core;
	FDTTelemetryWriter Writer;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TArray<int32> LevelScratch;
};
