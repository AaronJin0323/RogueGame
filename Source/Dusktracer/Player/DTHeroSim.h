// The hero simulation: plain data stepped in EDTSimPhase::Hero (02_Systems/combat_core.md R-CMB-01..12).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSimTypes.h"
#include "Player/DTHeroTypes.h"

/**
 * Holds what the pawn (or the bot, or a test) has latched since the last Input phase, one entry per player. Axes take
 * the newest frame; presses accumulate until exactly one step consumes them (R-CMB-02).
 */
class DUSKTRACER_API FDTHeroInputLatch
{
public:
	/** Folds one frame sample into the player's pending input. Invalid ids grow the latch (players join by index). */
	void Latch(int32 OwnerId, const FDTHeroInput& Frame);
	/** The pending input for one step: presses are cleared from the latch, axes stay (held sticks keep moving). */
	FDTHeroInput Consume(int32 OwnerId);
	void Reset() { Pending.Reset(); }

private:
	TArray<FDTHeroInput> Pending;
};

/**
 * Every player's hero state, stepped once per fixed step. Deterministic: no engine state, no randomness (R-CMB-12).
 * Reads last step's HeroHits and the enemy view; writes HeroPositions, HeroAimDirs, HeroIntangible; clears HeroHits.
 */
class DUSKTRACER_API FDTHeroSim
{
public:
	/** Fresh run: NumPlayers heroes at the origin with full HP, base charges and an empty meter. Aim modes persist. */
	void Reset(int32 NumPlayers);

	/** The input the next Step uses for this player (Input phase). */
	void SetStepInput(int32 OwnerId, const FDTHeroInput& Input);

	/** EDTSimPhase::Hero. */
	void Step(const FDTSimContext& Context);

	int32 Num() const { return States.Num(); }
	bool IsValidPlayer(int32 OwnerId) const { return States.IsValidIndex(OwnerId); }
	const FDTHeroState& GetState(int32 OwnerId) const { return States[OwnerId]; }
	const TArray<FDTHeroState>& GetStates() const { return States; }
	/** The input the last (or next) Step used for this player; e.g. its Device for telemetry `input_device`. */
	const FDTHeroInput& GetStepInput(int32 OwnerId) const { return StepInputs[OwnerId]; }

	/** Events of the last Step, in order. */
	const TArray<FDTHeroEvent>& GetStepEvents() const { return StepEvents; }

	/** True from the step every hero is down (R-CMB-10). */
	bool IsRunOver() const { return bRunOver; }
	int64 GetRunEndStep() const { return RunEndStep; }

	EDTAimMode GetAimMode(int32 OwnerId) const;
	void SetAimMode(int32 OwnerId, EDTAimMode Mode);

	/** Written by the stat system; MaxHp and charges follow on the next step. */
	void SetStats(int32 OwnerId, const FDTHeroStats& Stats);

	/** Reserve.Restore (offers.md): adds Amount HP, never above MaxHp; a down hero stays down. */
	void Heal(int32 OwnerId, float Amount);
	/** Slam's dash_cooldown_reset (keepers.md): every dash charge back, the refill timer cleared. */
	void RefillDash(int32 OwnerId);

	/** Debug (DT.God): every hit is discarded. */
	void SetNoDamage(bool bInNoDamage) { bNoDamage = bInNoDamage; }
	bool GetNoDamage() const { return bNoDamage; }

	/** Base max HP plus the stat bonus. */
	static float ComputeMaxHp(const FDTHeroStats& Stats);
	static int32 ComputeMaxCharges(const FDTHeroStats& Stats);
	/** Clamps to unit length, then applies the radial dead zone C.HERO.STICK_DEADZONE (rescaled so the edge is zero). */
	static FVector2f ShapeStick(const FVector2f& Raw);
	/** Index into Positions of the nearest point to From (ties: lower index); INDEX_NONE when there is none. */
	static int32 FindNearest(const FVector2f& From, TConstArrayView<FVector2f> Positions, const FDTSpatialHash& Hash);

private:
	void StepPlayer(int32 OwnerId, const FDTSimContext& Context, const FDTHeroHit* Hit);
	void Emit(FDTHeroEvent Event, int32 OwnerId, int64 StepIndex);

	TArray<FDTHeroState> States;
	TArray<FDTHeroInput> StepInputs;
	TArray<EDTAimMode> AimModes;
	TArray<FDTHeroEvent> StepEvents;
	TArray<int32> HitScratch;
	TArray<int32> QueryScratch;
	int64 RunEndStep = INDEX_NONE;
	bool bRunOver = false;
	bool bNoDamage = false;
};

/** EDTSimPhase::Input: resets the heroes at step 0 of a run and hands each player's latched input to the step. */
class DUSKTRACER_API FDTHeroInputPhase : public IDTSimSystem
{
public:
	FDTHeroInputLatch* Latch = nullptr;
	FDTHeroSim* Hero = nullptr;
	virtual void SimStep(const FDTSimContext& Context) override;
};

/** EDTSimPhase::Hero: steps the heroes. */
class DUSKTRACER_API FDTHeroStepPhase : public IDTSimSystem
{
public:
	FDTHeroSim* Hero = nullptr;
	virtual void SimStep(const FDTSimContext& Context) override;
};
