// Day-1 instrumentation core (02_Systems/telemetry.md, 11_Quality §3). Plain C++: tests drive it without a world.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"

/** What a session is about; written into run_start and every CSV row. */
struct FDTTelemetryConfig
{
	int32 Seed = 0;
	int32 NumPlayers = 1;
	float StepSeconds = 1.0f / DTConst::SIM_FIXED_HZ;
	float IntervalSeconds = DTConst::TEL_INTERVAL_S;
	FString Build = TEXT("unknown");
	FString Culture = TEXT("unknown");
	FString InputDevice = TEXT("unknown");
	/** Wall-clock start (ISO 8601); JSONL run_start only, never in the CSV (R-TEL-07). */
	FString WallStart;
};

/** World state at the end of one step, read by the subsystem from the bullet and enemy systems and the view. */
struct FDTTelemetrySample
{
	int32 LiveBullets = 0;
	int32 LiveEnemies = 0;
	int64 RecycledBulletsTotal = 0; // monotonic pool counter
	int64 RefusedSpawnsTotal = 0;   // monotonic pool counter
	TConstArrayView<FVector2f> Heroes;  // by player
	TConstArrayView<FVector2f> Enemies; // dense enemy positions
	TConstArrayView<int32> Levels;      // by player; missing entries read as level one
};

/** Damage, hits and kills one player dealt with one source. */
struct FDTTelemetrySourceTotals
{
	int64 Damage = 0;
	int32 Hits = 0;
	int32 Kills = 0;
};

/** One player's numbers for the interval in progress (also what DT.Stats prints). */
struct FDTTelemetryRow
{
	int32 Interval = 0;
	double RunTimeSeconds = 0.0;
	double ElapsedSeconds = 0.0;
	int32 Player = 0;
	int32 Kills = 0;
	int32 Hits = 0;
	int64 DamageDealt = 0;
	int64 DamageTaken = 0;
	int32 Dashes = 0;
	int32 ThreatActions = 0;
	int32 Level = 1;
	int32 LiveBullets = 0;
	int32 LiveEnemies = 0;
	int64 RecycledBullets = 0;
	int64 RefusedSpawns = 0;
	bool bHasTtk = false;
	float SwarmTtkSeconds = 0.0f;
	bool bHasNearest = false;
	float NearestEnemyUU = 0.0f;
};

/**
 * Turns owner-tagged events and per-step samples into a per-interval CSV (one row per player) and a JSONL event log.
 * The clock is the sim step count, never wall time (R-TEL-01). Output accumulates in memory; the owner drains it with
 * TakeOutput() and writes it off the game thread.
 *
 * Kill attribution and swarm TTK come from a mirror of the enemy pool's slots (spawn step per slot): the enemy system
 * reports spawns, then brackets ResolveHits with BeginResolve/EndResolve; the events that arrive in between are matched
 * to the hit list in order (the pool emits one Hit per live target, then one Kill on death, R-ENM-05/06) and the mirror
 * is compacted the way the pool compacts (R-ENM-07).
 */
class DUSKTRACER_API FDTTelemetryCore : public IDTRunEventSink
{
public:
	/** The pinned CSV header (R-TEL-06). */
	static const FString& GetCsvHeader();

	void Start(const FDTTelemetryConfig& InConfig);
	/** Writes the final partial interval (if any step ran since the last row) and run_end; the session stops. */
	void End(const FString& Reason);
	bool IsActive() const { return bActive; }
	const FDTTelemetryConfig& GetConfig() const { return Config; }

	// IDTRunEventSink
	virtual void OnRunEvent(const FDTGameEvent& Event, bool bRouted) override;

	/** Enemies phase, after spawn requests are applied: new slots are stamped with StepIndex; bCleared empties the mirror first. */
	void OnEnemiesSpawned(int64 StepIndex, bool bCleared, int32 PoolNum);
	/** ResolveHits phase, before the pool applies Hits. */
	void BeginResolve(int64 StepIndex, TConstArrayView<FDTHit> Hits);
	/** ResolveHits phase, after the pool applied and compacted; PoolNum checks the mirror. */
	void EndResolve(int32 PoolNum);
	/** Director phase: per-step samples, the per-step hit records, and the interval rollover on sim time. */
	void EndStep(int64 StepIndex, const FDTTelemetrySample& Sample);

	bool HasOutput() const { return !CsvOut.IsEmpty() || !JsonlOut.IsEmpty(); }
	/** Moves the buffered CSV rows (header first, once) and JSONL lines out. */
	void TakeOutput(FString& OutCsv, FString& OutJsonl);

	/** Current-interval numbers for Player (DT.Stats). */
	FDTTelemetryRow ComputeRow(int32 Player) const;
	int32 GetNumPlayers() const { return Players.Num(); }
	int32 GetIntervalIndex() const { return IntervalIndex; }
	int64 GetStepsDone() const { return StepsDone; }
	/** Run totals by source for Player (damage-by-source attribution, R-TEL-05). */
	const TMap<uint16, FDTTelemetrySourceTotals>& GetRunSources(int32 Player) const { return Players[Player].RunBySource; }
	int64 GetMirrorDesyncs() const { return MirrorDesyncs; }

private:
	struct FPlayerInterval
	{
		int32 Kills = 0;
		int32 Hits = 0;
		int64 DamageDealt = 0;
		int64 DamageTaken = 0;
		int32 Dashes = 0;
		int32 ThreatActions = 0;
		int32 Level = 1;
		TArray<float> TtkSamples;
		TArray<float> NearestSquaredSamples;
		TMap<uint16, FDTTelemetrySourceTotals> BySource;
		TMap<uint16, FDTTelemetrySourceTotals> RunBySource;
		// This step's hits (one JSONL record per player per step, R-TEL-04).
		int32 StepHits = 0;
		int64 StepDamage = 0;
		TMap<uint16, int64> StepDamageBySource;
	};

	struct FPendingEvent
	{
		FDTGameEvent Event;
		bool bRouted = false;
	};

	void ApplyHit(int32 Owner, bool bRouted, uint16 Source, int32 Amount);
	void ApplyKill(int32 Owner, bool bRouted, uint16 Source, float TtkSeconds);
	void WriteStepHits();
	void WriteInterval();
	void ResetInterval();
	void ResyncMirror(int32 PoolNum);
	double TimeOfCurrentStep() const;
	FString JsonHead(const TCHAR* Kind, int32 Owner) const;

	FDTTelemetryConfig Config;
	bool bActive = false;
	bool bHeaderWritten = false;
	TArray<FPlayerInterval> Players;

	int64 StepsDone = 0;          // steps ended since Start
	int64 CurrentStep = 0;        // sim step index of the step in progress
	int64 IntervalStartStep = 0;  // StepsDone at the start of the interval
	int64 IntervalSteps = 1;
	int32 IntervalIndex = 0;

	// Last world sample and the counter values at the interval start (per-interval deltas).
	int32 LastLiveBullets = 0;
	int32 LastLiveEnemies = 0;
	int64 LastRecycledTotal = 0;
	int64 LastRefusedTotal = 0;
	int64 RecycledAtIntervalStart = 0;
	int64 RefusedAtIntervalStart = 0;
	bool bHaveCounterBase = false;

	// Enemy slot mirror: spawn step per slot, and which slots died in the resolve in progress.
	TArray<int64> MirrorSpawnStep;
	TArray<uint8> MirrorDead;
	TArray<FDTHit> ResolveHits;
	TArray<FPendingEvent> Pending;
	bool bInResolve = false;
	int64 MirrorDesyncs = 0;
	int64 UnownedEvents = 0;

	FString CsvOut;
	FString JsonlOut;
};
