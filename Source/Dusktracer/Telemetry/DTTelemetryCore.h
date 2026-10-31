// Day-1 instrumentation core (02_Systems/telemetry.md, 11_Quality §3). Plain C++: tests drive it without a world.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Telemetry/DTTelemetryRecords.h"

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
	TConstArrayView<int64> XpTotals;    // by player (FDTPlayerRunState::XPTotal); run_end xp_total
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
	/** Applied damage as a share of max HP (each hit / max HP at that hit). */
	double DamageTakenShare = 0.0;
	int32 Dashes = 0;
	int32 ThreatActions = 0;
	int32 Level = 1;
	EDTTelInputDevice InputDevice = EDTTelInputDevice::Unknown;
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
	/**
	 * Writes the final partial interval (if any step ran since the last row) and run_end; the session stops. run_end
	 * carries per_player{owner: {decisions hands_by_kind{} reserve_cards cards_offered wall_paused_s level xp_total}} and
	 * the run's gems_spawned gems_collected gems_left (offers.md, progression.md).
	 */
	void End(const FString& Reason);
	bool IsActive() const { return bActive; }
	const FDTTelemetryConfig& GetConfig() const { return Config; }

	// IDTRunEventSink
	virtual void OnRunEvent(const FDTGameEvent& Event, bool bRouted) override;

	/** Enemies phase, after spawn requests are applied: new slots are stamped with StepIndex; bCleared empties the mirror first. */
	void OnEnemiesSpawned(int64 StepIndex, bool bCleared, int32 PoolNum);
	/**
	 * ResolveHits phase, before the pool applies Hits. Serials: the pool's serial per slot (FDTWorldView::EnemySerials);
	 * when given, a hit whose non-zero serial no longer matches its slot is skipped, as the pool skips it (R-ENM-23).
	 */
	void BeginResolve(int64 StepIndex, TConstArrayView<FDTHit> Hits, TConstArrayView<uint32> Serials = TConstArrayView<uint32>());
	/** ResolveHits phase, after the pool applied and compacted; PoolNum checks the mirror. */
	void EndResolve(int32 PoolNum);
	/** Director phase: per-step samples, the per-step hit and kill records, and the interval rollover on sim time. */
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
	int64 GetUnownedEvents() const { return UnownedEvents; }

	// ---- S1 record writers (02_Systems/telemetry.md §Data; schemas in each owning spec's §Telemetry) -------------------
	// Every writer is a no-op without a session. Owner is the player (R-TEC-01); a writer given an owner that is not a
	// player of the run writes nothing and counts in run_end.unowned_events (R-TEL-02). Records that can occur more than
	// once per step are aggregated per step per player (R-TEL-04) or per interval, as marked.

	// offers.md
	/** offer: every hand shown, and again after every reroll or banish refill (all cards and weights). */
	void RecordOffer(int32 Owner, const FDTOfferHand& Hand, const FDTPlayerOfferState& Tools);
	/** offer_pick; wall_decide_s is wall time in the hand (excluded from determinism, R-TEL-08). */
	void RecordOfferPick(int32 Owner, int32 HandId, int32 Slot, const FDTOfferCard& Card, double WallDecideSeconds);
	/** offer_tool; Slot INDEX_NONE and ContentId NAME_None for a reroll (written as -1 / null). */
	void RecordOfferTool(int32 Owner, int32 HandId, EDTTelOfferTool Tool, int32 Slot, FName ContentId);

	// keepers.md
	void RecordKeeperPick(int32 Owner, const FDTTelKeeperPick& Pick);
	void RecordKeeperCommit(int32 Owner, EDTTelKeeperRole Role, EDTPath Path, FName PrimaryLeading);
	/** Status procs; aggregated into one status{applied{}} record per player at each interval end. */
	void RecordStatusApplied(int32 Owner, EDTStatus Status, int32 Count = 1);
	/** A step's fires and damage of a pair; bFirst writes reaction_first (reactions.md). */
	void RecordReaction(int32 Owner, FName Pair, int32 Fires, float Damage, bool bFirst);
	void RecordIntercession(int32 Owner, const FDTTelIntercession& Cast);

	// combat_core.md
	/** dash record (one per dash) and the CSV column dashes_per_min. */
	void RecordDash(int32 Owner, FVector2f Dir, int32 ChargesAfter);
	/** hero_damaged per applied hit (Amount after the hero's own reductions) and the CSV column damage_taken_per_min. */
	void RecordHeroDamaged(int32 Owner, const FDTHeroHit& Hit, float Amount, float HpAfter, float MaxHp);
	void RecordHeroDown(int32 Owner);
	/** Counted per interval; one intercession_denied{interval, count} record per player at the interval end when above zero. */
	void RecordIntercessionDenied(int32 Owner);
	/** The device that last produced gameplay input (CSV input_device at the interval end). */
	void SetInputDevice(int32 Owner, EDTTelInputDevice Device);

	// enemies.md / director.md / boss.md
	/** A threat action started at Origin (ThreatAction{Role, AttackId}); counts for every player whose hero is within
	 *  C.DIR.THREAT_RING_MAX_UU at the end of the step (CSV threat_actions_per_min). No JSONL line. */
	void RecordThreatAction(FVector2f Origin, EDTEnemyRole Role, FName AttackId);
	void RecordDirBlock(const FDTTelDirBlock& Block);
	/** elites.md R-ELT-13; owner is the killer. */
	void RecordEliteDeath(const FDTTelElite& Elite);
	void RecordBossStart(FName Form, int32 Wins, int32 NumPlayers);
	void RecordBossPhase(int32 Phase, float FightTimeSeconds, float HpFrac, bool bByTime);
	void RecordBossCalibrated(float WindowDps, float EffectiveMax, bool bClamped);
	void RecordBossEnd(const FDTTelBossEnd& End);

	// progression.md
	/** Aggregated: one xp{gems, xp} record per player per step with pickups. */
	void RecordXp(int32 Owner, int32 Gems, int32 Xp);
	void RecordLevelUp(int32 Owner, int32 Level, int64 XpTotal, int32 PendingHands);
	/** Aggregated: one gems_merged{count} record (owner -1) per step in which the cap merged gems. */
	void RecordGemsMerged(int32 Count);
	/** run_end gems_spawned (no record). */
	void RecordGemsSpawned(int32 Count);
	/** Gems lying in the arena now; the last value is run_end gems_left (no record). */
	void SetGemsLive(int32 Count);

	// weapons.md
	void RecordWeaponAcquired(int32 Owner, uint16 Source);
	/** reforging.md Telemetry: a Reforging pick. Target and WeaponLevel are for an evolution; Dormant lists what it put to sleep. */
	void RecordReforge(int32 Owner, bool bEvolve, FName Id, FName Target, int32 WeaponLevel, TConstArrayView<FName> Dormant);
	void RecordWeaponLevel(int32 Owner, uint16 Source, int32 Level);
	/** The weapon's current level and load rank; read by the per-interval weapon record (no record of its own). */
	void SetWeaponState(int32 Owner, uint16 Source, int32 Level, int32 LoadRank);
	/** Emissions this step (spawned and throttled, R-WPN-09); summed into the per-interval weapon record. */
	void RecordWeaponEmissions(int32 Owner, uint16 Source, int32 Spawned, int32 Throttled);

	// modifiers.md
	void RecordModifierSocketed(int32 Owner, FName Weapon, FName Modifier);
	void RecordModifierDormant(int32 Owner, FName Weapon, FName Modifier, bool bDormant);

	/** Run totals by player, for producers that report per block (director dir_block: diff two snapshots). */
	double GetRunDamageTakenShare(int32 Player) const { return Players.IsValidIndex(Player) ? Players[Player].RunDamageTakenShare : 0.0; }
	int32 GetRunDashes(int32 Player) const { return Players.IsValidIndex(Player) ? Players[Player].RunDashes : 0; }

private:
	struct FWeaponInterval
	{
		int32 Level = 1;
		int32 LoadRank = 0;
		int32 Spawned = 0;
		int32 Throttled = 0;
	};

	struct FReactionInterval
	{
		int32 Fires = 0;
		double Damage = 0.0;
	};

	struct FPlayerInterval
	{
		int32 Kills = 0;
		int32 Hits = 0;
		int64 DamageDealt = 0;
		double DamageTakenShare = 0.0;
		int32 Dashes = 0;
		int32 ThreatActions = 0;
		int32 IntercessionDenied = 0;
		int32 StatusApplied[static_cast<int32>(EDTStatus::Count)] = {};
		TMap<FName, FReactionInterval> Reactions;
		int32 Level = 1;
		int64 XpTotal = 0;
		EDTTelInputDevice InputDevice = EDTTelInputDevice::Unknown;
		bool bHasInputDevice = false;
		/** Owned weapons by source: level and load rank persist, emissions reset per interval. */
		TMap<uint16, FWeaponInterval> Weapons;
		// Run totals (run_end and per-block producers).
		double RunDamageTakenShare = 0.0;
		int32 RunDashes = 0;
		int32 Decisions = 0;
		int32 HandsByKind[static_cast<int32>(EDTHandKind::Patron) + 1] = {};
		int32 ReserveCards = 0;
		int32 CardsOffered = 0;
		double WallPausedSeconds = 0.0;
		TMap<int32, EDTHandKind> HandKinds; // hand id -> kind, for decisions (Patron hands do not count, offers.md R-OFF-08)
		// This step's xp pickups (one xp record per player per step).
		int32 StepGems = 0;
		int32 StepXp = 0;
		TArray<float> TtkSamples;
		TArray<float> NearestSquaredSamples;
		TMap<uint16, FDTTelemetrySourceTotals> BySource;
		TMap<uint16, FDTTelemetrySourceTotals> RunBySource;
		// This step's hits and kills (at most one JSONL record of each per player per step, R-TEL-04).
		int32 StepHits = 0;
		int64 StepDamage = 0;
		TMap<uint16, int64> StepDamageBySource;
		int32 StepKills = 0;
		TMap<uint16, int64> StepKillsBySource;
		int32 StepTtkCount = 0;
		double StepTtkSum = 0.0;
		float StepTtkMin = 0.0f;
		float StepTtkMax = 0.0f;
	};

	struct FPendingEvent
	{
		FDTGameEvent Event;
		bool bRouted = false;
	};

	void ApplyHit(int32 Owner, bool bRouted, uint16 Source, int32 Amount);
	void ApplyKill(int32 Owner, bool bRouted, uint16 Source, float TtkSeconds);
	/** Writes this step's hit and kill records (at most one each per player) and clears the step accumulators. */
	void WriteStepRecords();
	void WriteInterval();
	void ResetInterval();
	void ResyncMirror(int32 PoolNum);
	double TimeOfCurrentStep() const;
	FString JsonHead(const TCHAR* Kind, int32 Owner) const;
	/** The player's interval, or nullptr (counted as unowned) when the session is off or Owner is not a player. */
	FPlayerInterval* RecordOwner(int32 Owner);
	/** Per-interval S1 records of one player (status, weapon, intercession_denied), after its CSV row. */
	void WriteIntervalRecords(int32 Player);
	/** run_end's per-player and run fields (offers.md, progression.md). */
	FString RunEndExtras() const;

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
	/** Serials of the pool's slots when the resolve began (stale hits are skipped like the pool does, R-ENM-23). */
	TArray<uint32> ResolveSerials;

	// S1 run-level state.
	TArray<FVector2f> StepThreatOrigins; // threat actions started this step, matched to heroes at EndStep
	int32 StepGemsMerged = 0;
	int64 GemsSpawned = 0;
	int64 GemsCollected = 0;
	int32 GemsLive = 0;

	FString CsvOut;
	FString JsonlOut;
};
