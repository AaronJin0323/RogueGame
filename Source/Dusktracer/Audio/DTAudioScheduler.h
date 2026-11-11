// The audio scheduler (02_Systems/audio.md R-AUD-05 to R-AUD-10, R-AUD-20): requests in, voices to start out. Engine-free
// (no UObject, no world) so tests drive it frame by frame. Ported from RogueHero's FRHAudioScheduler and changed as the spec says.
#pragma once

#include "CoreMinimal.h"
#include "Audio/DTAudioTypes.h"

/**
 * Time: a run sound's request carries its sim step, and its interval and its voice's length count steps; an interface sound
 * runs on real seconds (the world is paused under it). A frame (Frame()) resolves what was asked since the last one:
 *  - a batch of one id in one step (one frame for the interface) is one voice, louder by 1 + agg_gain x log2(count), capped at
 *    C.AUD.AGGREGATE_MAX (R-AUD-06);
 *  - a batch inside the id's interval merges into the voice that sounds: it is counted, never carried;
 *  - a row with grid waits for the next step that is a multiple of C.AUD.GRID_STEPS (R-AUD-09);
 *  - caps (R-AUD-07): the id's own voices; C.AUD.VOICES_MAX in all, of which C.AUD.VOICES_DANGER and C.AUD.VOICES_UI are kept
 *    (a player voice never takes a kept slot); C.AUD.PLAYER_STARTS_PER_S_MAX player starts in any second of sim time;
 *    C.AUD.STARTS_PER_FRAME_MAX starts a frame, danger first: an interface or player batch past it waits one frame, then drops;
 *  - danger (R-AUD-05) never waits and never varies; when no slot is free it takes the oldest player voice's slot (the engine
 *    voice is left to end, the slot is counted as stolen); while a danger voice sounds the duck goes to C.AUD.DUCK_DB within
 *    C.AUD.DUCK_ATTACK_MS and comes back over C.AUD.DUCK_RELEASE_MS, linear in dB, on real time;
 *  - variation (R-AUD-10): variants in turn (round robin, so never the same file twice running); a `key` row draws a degree of
 *    C.AUD.KEY_SEMIS_MAX's key and a level within C.AUD.LEVEL_JITTER_DB from the scheduler's own stream; `fixed` never varies.
 * Seams left for later families (no behaviour yet): `ladder` pitch plays as written; the density layers (R-AUD-08), fire
 * density (R-AUD-07's shots per second) and the heartbeat (R-AUD-12) add their rules in Frame's resolve step.
 */
class DUSKTRACER_API FDTAudioScheduler
{
public:
	/** The rows this scheduler serves (the generated table, or a test's own) and its stream's seed. */
	void Configure(TConstArrayView<DTCat::FSoundRow> InRows, uint64 Seed);
	/** A new run: forgets voices, waits, starts and the duck, reseeds the stream; the counts stay. */
	void Reset(uint64 Seed);

	/** A run sound: Count events of Sound at sim step Step (an observer calls it at OnStepEnd). */
	void Request(int32 Sound, int32 Count, int64 Step);
	/** An interface sound: Count events now, on real time. */
	void RequestUi(int32 Sound, int32 Count);

	/**
	 * Resolves the requests and waits against this frame: Now in real seconds, Step the sim step current now. Appends the
	 * voices to start, danger first, to Out.
	 */
	void Frame(double Now, int64 Step, TArray<FDTAudioVoice>& Out);

	/** The Player group's duck gain now, in dB (0 when none), as of the last Frame (R-AUD-05). */
	float GetDuckDb() const { return DuckDb; }
	/** The duck gain at real time T, from the curve the last Frame set: a pure function, for the engine side and the tests. */
	float DuckDbAt(double T) const;

	const FDTAudioCounts& GetCounts(int32 Sound) const { return Counts[Sound]; }
	int32 NumSounds() const { return Rows.Num(); }
	int32 NumAlive() const { return Alive.Num(); }
	int32 NumAlive(DTCat::ESoundSide Side) const;
	int32 NumWaiting() const { return Waiting.Num(); }
	/** Slots danger took from player voices (R-AUD-05). */
	int64 GetStolen() const { return Stolen; }
	int32 GetStartsThisFrame() const { return StartsThisFrame; }
	int32 GetPlayerStartsLastSecond() const { return PlayerStarts.Num(); }

private:
	struct FBatch
	{
		int32 Sound = INDEX_NONE;
		int32 Count = 0;
		int64 Step = 0;
		double Time = 0.0;
		/** The step a grid row may start on. */
		int64 Due = 0;
		/** It already waited a frame for the per-frame cap. */
		bool bWaited = false;
	};
	struct FLive
	{
		int32 Sound = INDEX_NONE;
		DTCat::ESoundSide Side = DTCat::ESoundSide::Player;
		bool bRealTime = false;
		int64 EndStep = 0;
		double EndTime = 0.0;
		int64 StartOrder = 0;
	};

	bool IsUi(int32 Sound) const { return Rows[Sound].Side == DTCat::ESoundSide::UI; }
	bool IsDanger(int32 Sound) const { return Rows[Sound].Side == DTCat::ESoundSide::Danger; }
	int64 IntervalSteps(int32 Sound) const;
	int64 LengthSteps(int32 Sound, float Pitch) const;
	void Expire(double Now, int64 Step);
	/** Resolves one batch: true when it is settled (started, merged or dropped), false when it waits. */
	bool Resolve(FBatch& Batch, double Now, int64 Step, TArray<FDTAudioVoice>& Out);
	bool FreeSlotFor(DTCat::ESoundSide Side) const;
	void Start(const FBatch& Batch, double Now, int64 Step, TArray<FDTAudioVoice>& Out);
	void UpdateDuck(double Now);
	float NextUnit();

	TArray<DTCat::FSoundRow> Rows;
	TArray<FDTAudioCounts> Counts;
	TArray<int64> LastStartStep;
	TArray<double> LastStartTime;
	TArray<int32> LastVariant;
	TArray<FBatch> Pending;
	TArray<FBatch> Waiting;
	TArray<FLive> Alive;
	/** Sim steps of the player starts of the last second (R-AUD-07). */
	TArray<int64> PlayerStarts;
	int64 StartOrder = 0;
	int64 Stolen = 0;
	int32 StartsThisFrame = 0;
	uint64 Stream = 0;
	/** Duck curve: it holds at C.AUD.DUCK_DB until DuckHoldUntil, having started from DuckFrom dB at DuckStart. */
	double DuckStart = -1.0e9;
	double DuckHoldUntil = -1.0e9;
	float DuckFrom = 0.0f;
	float DuckDb = 0.0f;
};
