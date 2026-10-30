// The director's pacing state machine (02_Systems/director.md R-DIR-03..16). Plain C++ so tests run whole runs headless.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyTypes.h"
#include "Progression/DTGemPool.h"
#include "Telemetry/DTTelemetryRecords.h"

/** The block the director is in (R-DIR-04). Breath is never a timeline row; it follows each beat. */
enum class EDTDirBlock : uint8
{
	Idle,    // before StartRun
	Harvest,
	Threat,
	Breath,
	Boss
};

/** Where the director's requests go. UDTDirector forwards them to the enemy, gem and telemetry systems; tests record them. */
class IDTDirectorSink
{
public:
	virtual ~IDTDirectorSink() = default;
	/** R-DIR-03: applied at the start of the next Enemies phase. */
	virtual void QueueSpawn(const FDTEnemySpawn& Spawn) = 0;
	/** R-DIR-12. */
	virtual void SpawnGem(const FDTGemSpawn& Gem) = 0;
	/** R-DIR-16. */
	virtual void SpawnChest(const FVector2f& Position) = 0;
	/** R-DIR-13: once per run, at the boss row's start. */
	virtual void BossTime(float RunSeconds) = 0;
	/** R-DIR-15: one per ended block; the sink fills the per-player numbers (DamageTakenShare, Dashes) before writing. */
	virtual void BlockEnded(FDTTelDirBlock& Block) = 0;
};

/** What the director reads each step besides the world view. */
struct FDTDirectorStepIn
{
	float StepSeconds = 0.0f;
	int64 StepIndex = 0;
	/** EDTRngStream::Director (R-DIR-14). */
	FRandomStream* Rng = nullptr;
	/** Hero positions, kills; DropInMarkers and bThreatBlock are written. */
	FDTWorldView* View = nullptr;
	/** By player; a missing entry counts as alive (R-DIR-05: spawns pick a living hero). */
	TConstArrayView<bool> HeroAlive;
	/** Live enemies by role after this step's ResolveHits (spawns queued this step are added by the director). */
	int32 AliveByRole[static_cast<int32>(EDTEnemyRole::Count)] = {};
	int32 AliveTotal = 0;
	/** Origins of threat actions started this step (FDTEnemyPool::GetStepThreatActions). */
	TConstArrayView<FVector2f> ThreatOrigins;
};

/** Run totals for DT.Dir.Stats and tests. */
struct FDTDirectorTotals
{
	int32 Spawned[static_cast<int32>(EDTEnemyRole::Count)] = {};
	int32 EventSpawned = 0;
	int32 SkippedAtCap = 0;
	int32 DropInsPublished = 0;
	int32 DropInsLanded = 0;
	int32 DropInsCancelled = 0;
	int32 Beats = 0;
	int32 BeatsCleared = 0;
	int32 BeatsSkipped = 0;
	int32 Chests = 0;
	int32 Gems = 0;
	int32 Kills = 0;
	int32 ThreatActions = 0;
	int32 Blocks = 0;
	bool bBossRequested = false;
};

/**
 * One run of the arena timeline. Step once per fixed step in EDTSimPhase::Director; its clock is the steps it was given
 * (plus debug jumps), so a paused sim advances nothing (R-DIR-03). Every draw comes from the Director stream (R-DIR-14).
 */
class DUSKTRACER_API FDTDirectorCore
{
public:
	/** One live drop-in: a published marker and its spawn (R-DIR-07). */
	struct FDropIn
	{
		FDTDropInMarker Marker;
		FDTEnemySpawn Spawn;
	};

	FDTDirectorCore();

	/** Starts a run on Rows (sorted by start; default: DTCat::TimelineArena1). Clears every block, marker and total. */
	void StartRun(TConstArrayView<DTCat::FTimelineRow> InRows);
	void StartRun();
	/** Back to Idle: no block, nothing spawns. */
	void Stop();
	bool IsRunning() const { return Block != EDTDirBlock::Idle; }

	/** One fixed step (see the step order in DTDirectorCore.cpp). */
	void Step(const FDTDirectorStepIn& In, IDTDirectorSink& Sink);

	// Debug (DT.Dir.*): never used by the run itself.
	/** Moves the clock by Seconds (>= 0); rows passed are applied at the next step, beats whose window passed are skipped. */
	void Skip(float Seconds);
	/** Forces a block at the next step: Harvest ends a beat or Breath, Threat starts the next Threat row now, Breath ends the beat. */
	void ForceBlock(EDTDirBlock Wanted);

	// State (tests, stats, render).
	EDTDirBlock GetBlock() const { return Block; }
	double GetTime() const { return Time; }
	float GetCredit() const { return Credit; }
	float GetDropInProgress() const { return DropInProgress; }
	bool IsDropInWaiting() const { return bDropInDue; }
	int32 GetHarvestRow() const { return HarvestRow; }
	int32 GetBeatRow() const { return BeatRow; }
	int32 GetBeatId() const { return BeatId; }
	int32 GetEventAlive() const { return EventAlive; }
	TConstArrayView<FDropIn> GetDropIns() const { return DropIns; }
	const FDTDirectorTotals& GetTotals() const { return Totals; }
	const FDTTelDirBlock& GetOpenBlock() const { return Open; }
	float GetBossSeconds() const { return BossSeconds; }

	/** R-DIR-10: the scales stamped on a spawn at RunSeconds. */
	static float HpScaleAt(double RunSeconds);
	static float DmgScaleAt(double RunSeconds);
	/** R-DIR-12: XP of one kill. */
	static int32 GemValue(EDTEnemyRole Role, bool bElite);
	/** R-DIR-05: the alive share cap of a role (1 = none). */
	static float ShareCap(EDTEnemyRole Role);
	/** R-DIR-05: true when Role may not be drawn with these alive counts. */
	static bool IsAtShareCap(EDTEnemyRole Role, int32 RoleAlive, int32 TotalAlive);
	/** Inside the arena with room for Radius (R-DIR-05 "moved inside the arena"). */
	static FVector2f ClampToArena(const FVector2f& Position, float Radius);
	static const TCHAR* BlockName(EDTDirBlock InBlock);

private:
	void OpenBlock(EDTDirBlock State, float RowStart);
	void CloseBlock(const TCHAR* EndedBy, IDTDirectorSink& Sink);
	void StartBeat(int32 Row, const FDTDirectorStepIn& In, IDTDirectorSink& Sink);
	void EndBeat(bool bCleared, const FDTDirectorStepIn& In, IDTDirectorSink& Sink);
	void SpawnEvent(const DTCat::FTimelineRow& Row, const FDTDirectorStepIn& In, IDTDirectorSink& Sink);
	void RegularSpawn(const DTCat::FTimelineRow& Row, const FDTDirectorStepIn& In, IDTDirectorSink& Sink);
	void StepDropIns(const FDTDirectorStepIn& In, IDTDirectorSink& Sink);
	void CancelDropIns();
	void Emit(FDTEnemySpawn Spawn, IDTDirectorSink& Sink);

	int32 PickHero(const FDTDirectorStepIn& In) const;
	bool HasLivingHero(const FDTDirectorStepIn& In) const;
	FVector2f HeroAt(const FDTDirectorStepIn& In, int32 Hero) const;
	FVector2f OnRing(const FVector2f& Center, float MinR, float MaxR, FRandomStream& Rng) const;
	/** Weighted draw over Weights (Swarm Tank Rusher Turret); roles at their share cap excluded when bShareCaps. */
	bool DrawRole(const float* Weights, bool bShareCaps, FRandomStream& Rng, EDTEnemyRole& OutRole) const;
	FDTEnemySpawn MakeSpawn(EDTEnemyRole Role, const FVector2f& Position) const;
	void ApplyForced(EDTDirBlock Wanted, const FDTDirectorStepIn& In, IDTDirectorSink& Sink);
	/** The next Threat row not yet started (pending first, then the rows ahead); INDEX_NONE if none. */
	int32 TakeNextThreatRow();

	TConstArrayView<DTCat::FTimelineRow> Rows;
	EDTDirBlock Block = EDTDirBlock::Idle;
	double Time = 0.0;
	float BossSeconds = 0.0f;
	int32 NextRow = 0;
	int32 HarvestRow = INDEX_NONE;
	/** The Harvest row the open Harvest block was opened for; INDEX_NONE before the first row. */
	int32 OpenRow = INDEX_NONE;
	TArray<int32> PendingThreats;
	/** Threat rows started ahead of their time (DT.Dir.Block Threat); skipped when the clock reaches them. */
	TSet<int32> StartedEarly;

	// Beat (R-DIR-04, R-DIR-11, R-DIR-16).
	int32 BeatRow = INDEX_NONE;
	int32 BeatId = INDEX_NONE;
	double BeatEnd = 0.0;
	double BreathEnd = 0.0;
	int32 EventAlive = 0;
	int32 EventQueued = 0;
	FVector2f LastEventDeath = FVector2f::ZeroVector;

	// Regular spawns (R-DIR-05).
	float Credit = 0.0f;

	// Drop-ins (R-DIR-07, R-DIR-08).
	TArray<FDropIn> DropIns;
	float DropInProgress = 0.0f;
	bool bDropInDue = false;
	/** Run time of the last landing; unset before the first. */
	TOptional<double> LastLandTime;

	// Forced blocks (debug), applied at the next step.
	TOptional<EDTDirBlock> Forced;

	// Per-step scratch: alive counts including this step's queued spawns.
	int32 Alive[static_cast<int32>(EDTEnemyRole::Count)] = {};
	int32 AliveTotal = 0;
	int64 StepIndex = 0;
	float StepSeconds = 0.0f;

	FDTTelDirBlock Open;
	TArray<float> KillDistances;
	FDTDirectorTotals Totals;
};
