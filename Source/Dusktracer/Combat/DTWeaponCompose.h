// Weapon composition: level row -> modifiers -> flags -> hero stats -> load factor (weapons.md R-WPN-03, R-WPN-09,
// R-WPN-10; modifiers.md R-MOD-04, R-MOD-05, R-MOD-08). Pure functions: no world, no randomness.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Combat/DTWeaponCatalog.h"
#include "Combat/DTWeaponFlags.h"

/**
 * The hero-stat inputs of composition (the FDTStatSet slice weapons read). Set by the hero/stat owner through
 * UDTWeaponSubsystem::SetHeroStats; identity by default (S1 has no crit source: modifiers.md Open).
 */
struct FDTWeaponHeroStats
{
	float DamageMult = 1.0f;
	float CooldownMult = 1.0f;
	float CritChance = 0.0f;
	/** Multiplier of a critical emission's damage; 1 = crits change nothing. */
	float CritMult = 1.0f;
};

/** A composed flag entry and how the weapon expresses it (R-WPN-08). */
struct FDTResolvedFlag
{
	FDTFlagEntry Entry;
	FDTFlagResolution Resolution;
	bool IsNative() const { return Resolution.Compat == DTCat::ECompat::Native; }
	bool IsFallback() const { return Resolution.Compat == DTCat::ECompat::Fallback; }
};

/** A weapon's composed spec for one step (weapons.md Data). Damage is final per emission before crit. */
struct FDTWeaponSpec
{
	int32 Weapon = INDEX_NONE;
	EDTWeaponPattern Pattern = EDTWeaponPattern::Unknown;
	EDTWeaponTarget Target = EDTWeaponTarget::Unknown;
	int32 Level = 0;

	float Damage = 0.0f;
	int32 Count = 0;
	float Cooldown = 0.0f;
	/** Round radius; melee reach for MeleeArc. */
	float Radius = 0.0f;
	float Speed = 0.0f;
	/** Targeting range (melee: its reach). */
	float Range = 0.0f;
	/** Round life = range / speed (R-WPN-06). */
	float Life = 0.0f;
	int32 Pierce = 1;
	float AreaRadius = 0.0f;
	/** Sweep width of a MeleeArc. */
	float ArcDeg = 0.0f;
	float CritChance = 0.0f;
	float CritMult = 1.0f;
	int32 LoadRank = 0;
	float LoadFactor = 1.0f;
	FDTCapList EffectiveCaps;

	// Modifier behaviours that are not flags (R-MOD-03): 0 when not socketed or dormant.
	uint16 MirrorSource = 0;
	float MirrorDamageRatio = 0.0f;
	uint16 ForkSource = 0;
	int32 ForkRounds = 0;
	float ForkAngleDeg = 0.0f;
	uint16 ChainTriggerSource = 0;
	float ChainTriggerIntervalS = 0.0f;

	// The active pattern conversion (R-WPN-10), when it is native (it reshapes the rounds).
	DTCat::EFlag Conversion = DTCat::EFlag::None;
	int32 ConvertRounds = 0;
	bool bConvertsEveryRound = false;
	float ConvertArcDeg = 0.0f;
	float ConvertReach = 0.0f;
	float ConvertDamageFactor = 0.0f;

	/** Every flag entry the weapon carries after stacking and caps, with its resolution. */
	TArray<FDTResolvedFlag> Flags;

	const FDTResolvedFlag* FindFlag(DTCat::EFlag Flag) const
	{
		return Flags.FindByPredicate([Flag](const FDTResolvedFlag& F) { return F.Entry.Flag == Flag; });
	}
	bool HasFlag(DTCat::EFlag Flag) const { return FindFlag(Flag) != nullptr; }
};

/** What happened to sockets while composing (telemetry modifier_dormant). */
struct FDTDormancyChange
{
	FName ModId;
	bool bDormant = false;
};

namespace DTWeaponCompose
{
	/** C.LOAD.FACTOR_<Rank+1> for a zero-based rank (R-WPN-03). */
	DUSKTRACER_API float LoadFactor(int32 Rank);
	/** Zero-based load rank per slot: level descending, ties by acquisition order (R-WPN-03). */
	DUSKTRACER_API TArray<int32> LoadRanks(TConstArrayView<FDTWeaponInstance> Weapons);

	/** Stacks entries of the same flag, source and `on` word (R-MOD-05) and applies the per-flag caps. */
	DUSKTRACER_API TArray<FDTFlagEntry> StackEntries(TConstArrayView<FDTFlagEntry> Entries);

	/**
	 * Composes one weapon (R-WPN-09). PlayerFlags are the player's flag-source entries (Keepers); socketed modifiers
	 * are re-checked against the effective caps and marked dormant or awake on Instance (R-MOD-08), with each change
	 * appended to OutChanges when given.
	 */
	DUSKTRACER_API FDTWeaponSpec Compose(FDTWeaponInstance& Instance, TConstArrayView<FDTFlagEntry> PlayerFlags,
		const FDTWeaponHeroStats& Stats, int32 LoadRank, TArray<FDTDormancyChange>* OutChanges = nullptr);
}
