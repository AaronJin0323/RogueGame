// Hero data: per-player state, the one input struct, stats and hero events (02_Systems/combat_core.md Data).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"

/** How a player's aim direction is chosen (R-CMB-04). A player option; Auto is the default. */
enum class EDTAimMode : uint8
{
	Auto,   // nearest enemy, hold when none
	Manual  // mouse cursor on the sim plane, or the right stick beyond the dead zone
};

/** The device that last produced gameplay input (telemetry `input_device`). */
enum class EDTInputDevice : uint8
{
	KBM,
	Pad
};

/**
 * Everything a player tells the hero for one step (R-CMB-02, R-CMB-12). The pawn, the bot and the tests all drive the
 * hero through this struct and nothing else. Vectors are on the sim plane (world X, world Y).
 * Presses are edges: a latched press is consumed by exactly one step (FDTHeroInputLatch).
 */
struct FDTHeroInput
{
	/** Raw move input; the sim clamps it to unit length and applies the radial dead zone. */
	FVector2f Move = FVector2f::ZeroVector;
	/** Raw right stick (Manual aim); the sim applies the radial dead zone. */
	FVector2f AimStick = FVector2f::ZeroVector;
	/** The mouse cursor projected onto the sim plane at C.SIM.PLANE_Z_UU (Manual aim). */
	FVector2f AimCursorWorld = FVector2f::ZeroVector;
	bool bAimCursorValid = false;
	bool bDashPress = false;
	bool bIntercessionPress = false;
	EDTInputDevice Device = EDTInputDevice::KBM;

	/** Folds a newer frame sample into this pending input: axes and cursor take the newest value, presses accumulate. */
	void MergeFrame(const FDTHeroInput& Frame)
	{
		Move = Frame.Move;
		AimStick = Frame.AimStick;
		AimCursorWorld = Frame.AimCursorWorld;
		bAimCursorValid = Frame.bAimCursorValid;
		Device = Frame.Device;
		bDashPress |= Frame.bDashPress;
		bIntercessionPress |= Frame.bIntercessionPress;
	}
};

/**
 * Multipliers and bonuses the hero applies from its own stats (keepers.md, modifiers.md own the sources). Identity by
 * default; the stat system (FDTStatSet) writes these when it exists.
 */
struct FDTHeroStats
{
	float MoveSpeedMul = 1.0f;
	float DashCooldownMul = 1.0f;
	float IntercessionCooldownMul = 1.0f;
	float DamageTakenMul = 1.0f;
	float MaxHpBonus = 0.0f;
	int32 DashChargesBonus = 0;
};

/** One player's hero (R-CMB-01). Plain data stepped by FDTHeroSim; times are seconds of sim time. */
struct FDTHeroState
{
	FVector2f Pos = FVector2f::ZeroVector;
	FVector2f PrevPos = FVector2f::ZeroVector;
	FVector2f Vel = FVector2f::ZeroVector;
	/** Unit; the dash direction when there is no move input. */
	FVector2f LastMoveDir = FVector2f(1.0f, 0.0f);
	/** Unit; published as FDTWorldView::HeroAimDirs. */
	FVector2f AimDir = FVector2f(1.0f, 0.0f);
	/** Unit; the direction of the dash in progress. */
	FVector2f DashDir = FVector2f(1.0f, 0.0f);
	float Hp = 0.0f;
	float MaxHp = 0.0f;
	int32 DashCharges = 0;
	/** Charge capacity at the last step; a capacity gained from stats arrives charged. */
	int32 DashChargesMax = 0;
	/** Time until the next spent charge returns; zero when every charge is in. */
	float DashRefillLeft = 0.0f;
	/** Time left of the dash in progress; zero when not dashing. */
	float DashLeft = 0.0f;
	float HurtIFramesLeft = 0.0f;
	/** Intercession meter, zero to one. */
	float Intercession01 = 0.0f;
	/** Sim time a dash / Intercession press has been pending; negative when none is pending (R-CMB-02 buffer). */
	float DashPressAge = -1.0f;
	float IntercessionPressAge = -1.0f;
	/** HeroIntangible as published by the last step (hits from that step are discarded, R-CMB-06). */
	bool bIntangible = false;
	/** Set once (R-CMB-10). */
	bool bDown = false;
	FDTHeroStats Stats;
	// Counters (debug, telemetry cross-checks).
	int32 Dashes = 0;
	int32 HitsTaken = 0;
	int32 Intercessions = 0;
};

enum class EDTHeroEventKind : uint8
{
	Dash,
	HeroDamaged,
	HeroDown,
	Intercession,
	IntercessionDenied
};

/** A hero event (combat_core.md Data), always with its OwnerId. Fields not used by a kind keep their defaults. */
struct FDTHeroEvent
{
	EDTHeroEventKind Kind = EDTHeroEventKind::Dash;
	int32 OwnerId = INDEX_NONE;
	int64 StepIndex = 0;
	/** Dash: direction. */
	FVector2f Dir = FVector2f::ZeroVector;
	/** Dash: charges left after it. */
	int32 ChargesAfter = 0;
	/** HeroDamaged. */
	float Amount = 0.0f;
	float HpAfter = 0.0f;
	float MaxHp = 0.0f;
	EDTHeroHitKind HitKind = EDTHeroHitKind::Contact;
	EDTEnemyRole SourceRole = EDTEnemyRole::Swarm;
	FName AttackId;
	/** Intercession: the player's Patron Path. */
	EDTPath Path = EDTPath::None;
};
