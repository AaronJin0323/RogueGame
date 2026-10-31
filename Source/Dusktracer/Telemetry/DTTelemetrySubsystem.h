// The world's telemetry session: sim-clocked CSV + JSONL under Saved/Playtests (02_Systems/telemetry.md, 11_Quality §3).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetryWriter.h"
#include "DTTelemetrySubsystem.generated.h"

/** A session is closing: its records are still open. Once per session. */
DECLARE_MULTICAST_DELEGATE_OneParam(FDTOnSessionEnding, const FString& /*Reason*/);

/**
 * Game/PIE worlds only. Registers for EDTSimPhase::Director (the end of each step, R-TEL-01) and observes every run
 * event through FDTRunState's sink (R-TEL-02). A session runs from StartSession (the game mode, with the run seed) to
 * EndSession (end of play, or world teardown); output is flushed off the game thread at every interval and at the end.
 * Hero events (combat_core.md: dash, hero_damaged, hero_down, intercession_denied) reach it through the hero subsystem's
 * OnHeroEvent, bound at begin play; the hero's last input device is read at every step end (CSV input_device).
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

	/** Bound by the run recorder (meta.md R-MET-04): every way a run ends comes through EndSession. */
	FDTOnSessionEnding OnSessionEnding;

	// Enemy-system hooks (kill attribution and swarm TTK; see FDTTelemetryCore).
	void OnEnemiesSpawned(int64 StepIndex, bool bCleared, int32 PoolNum) { Core.OnEnemiesSpawned(StepIndex, bCleared, PoolNum); }
	/** Also passes the published enemy serials (FDTWorldView::EnemySerials) so stale hits are skipped like the pool does. */
	void BeginResolve(int64 StepIndex, TConstArrayView<FDTHit> Hits);
	void EndResolve(int32 PoolNum) { Core.EndResolve(PoolNum); }

	const FDTTelemetryCore& GetCore() const { return Core; }

	/**
	 * The record API every S1 producer calls (FDTTelemetryCore::Record*, Set*; see DTTelemetryRecords.h). Every call is a
	 * no-op without a session, so producers never check. Game thread (the sim step or a paused hand).
	 *   if (UDTTelemetrySubsystem* T = UDTTelemetrySubsystem::Find(this)) { T->Records().RecordDash(Owner, Dir, Charges); }
	 */
	FDTTelemetryCore& Records() { return Core; }

	/** The telemetry subsystem of WorldContext's world, or nullptr (editor worlds, tests without a world). */
	static UDTTelemetrySubsystem* Find(const UObject* WorldContext);
	const FDTTelemetryWriter& GetWriter() const { return Writer; }

	/** Build hash baked at compile time (DT_BUILD_HASH), else "unknown". */
	static FString GetBuildHash();

private:
	/** Forwards one hero event to the record API (no-op without a session). */
	void HandleHeroEvent(const FDTHeroEvent& Event);

	FDTTelemetryCore Core;
	FDTTelemetryWriter Writer;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TWeakObjectPtr<UDTHeroSubsystem> Heroes;
	FDelegateHandle HeroEventHandle;
	TArray<int32> LevelScratch;
	TArray<int64> XpScratch;
};
