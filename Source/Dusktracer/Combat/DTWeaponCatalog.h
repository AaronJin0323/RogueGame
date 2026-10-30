// Catalog access for weapons and modifiers, capability walks and the compatibility function
// (weapons.md R-WPN-01/02/04/08/10, modifiers.md R-MOD-01/02). Pure functions over the generated DTCat tables (D-014).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTCatalogGen.h"
#include "Combat/DTWeaponFlags.h"

/** Fire pattern (weapons.csv `pattern`). */
enum class EDTWeaponPattern : uint8
{
	Aimed,
	Radial,
	Orbiter,
	MeleeArc,
	Mine,
	Cone,
	Line,
	Lob,
	Homing,
	Spiral,
	Unknown
};

/** Targeting rule (weapons.csv `target`, R-WPN-12). */
enum class EDTWeaponTarget : uint8
{
	Nearest,
	NearestToDrone,
	DensestCell,
	None,
	DensestLine,
	RandomAlive,
	Unknown
};

/** An ordered capability list: the walk order of R-WPN-08 (most specific first). */
struct FDTCapList
{
	TArray<DTCat::ECap, TInlineAllocator<DTCat::MaxWeaponCaps + 1>> Caps;
	uint32 Mask = 0;

	void Add(DTCat::ECap Cap)
	{
		if ((Mask & DTCat::CapBit(Cap)) == 0)
		{
			Caps.Add(Cap);
			Mask |= DTCat::CapBit(Cap);
		}
	}
	bool Has(DTCat::ECap Cap) const { return (Mask & DTCat::CapBit(Cap)) != 0; }
	bool operator==(const FDTCapList& Other) const { return Mask == Other.Mask && Caps == Other.Caps; }
};

/** How a flag is expressed on a cap list (R-WPN-08). */
struct FDTFlagResolution
{
	DTCat::ECompat Compat = DTCat::ECompat::None;
	/** The cap that decided; Count when none did. */
	DTCat::ECap Cap = DTCat::ECap::Count;
	/** flag_compat.csv fallback text when Compat is Fallback. */
	const TCHAR* Fallback = TEXT("");
};

namespace DTWeapons
{
	DUSKTRACER_API int32 NumWeapons();
	DUSKTRACER_API int32 NumModifiers();
	/** Row index in DTCat::Weapons / DTCat::Modifiers, or INDEX_NONE. Accepts the full id or the bare name. */
	DUSKTRACER_API int32 FindWeapon(FName Id);
	DUSKTRACER_API int32 FindModifier(FName Id);
	DUSKTRACER_API FName WeaponId(int32 Weapon);
	DUSKTRACER_API FName ModifierId(int32 Mod);
	/** "PulseRifle" for Weapon.PulseRifle, "SplitCore" for Mod.SplitCore. */
	DUSKTRACER_API FString ShortName(const TCHAR* Id);

	DUSKTRACER_API EDTWeaponPattern GetPattern(int32 Weapon);
	DUSKTRACER_API EDTWeaponTarget GetTarget(int32 Weapon);
	DUSKTRACER_API int32 GetMaxLevel(int32 Weapon);
	/** The level row (1-based level, clamped to the weapon's track), or nullptr for a weapon without level rows. */
	DUSKTRACER_API const DTCat::FWeaponLevelRow* GetLevelRow(int32 Weapon, int32 Level);

	/** The weapon's declared caps in catalog order (R-WPN-02). */
	DUSKTRACER_API FDTCapList DeclaredCaps(int32 Weapon);
	/**
	 * Effective caps under a conversion (R-WPN-10): a full conversion leads with the conversion cap and drops the
	 * carrier cap (Projectile); a partial one appends it. Conversion EFlag::None returns the declared caps.
	 */
	DUSKTRACER_API FDTCapList EffectiveCaps(int32 Weapon, DTCat::EFlag Conversion, bool bConvertsEveryRound);

	/** Walks Caps in order through flag_compat.csv; the first cell that is not `none` decides (R-WPN-08). */
	DUSKTRACER_API FDTFlagResolution ResolveFlag(const FDTCapList& Caps, DTCat::EFlag Flag);

	/** The flag a modifier grants (EFlag::None for none). */
	DUSKTRACER_API DTCat::EFlag ModifierFlag(int32 Mod);
	/** A modifier row's parameter by its modifiers.csv name (0 when absent). */
	DUSKTRACER_API float ModifierParam(int32 Mod, const TCHAR* Name);
	/**
	 * The flag entry a flag-carrying modifier feeds into its weapon's accumulator (R-MOD-05): its parameters in the
	 * flag vocabulary plus the words that place it (on, pattern, target). False for modifiers that grant no flag.
	 */
	DUSKTRACER_API bool ModifierFlagEntry(int32 Mod, uint16 SourceId, FDTFlagEntry& OutEntry);

	/** Telemetry source ids, registered on first use: Weapon.<Name>, Modifier.<Name>, Keeper.<Path>. */
	DUSKTRACER_API uint16 WeaponSource(int32 Weapon);
	DUSKTRACER_API uint16 ModifierSource(int32 Mod);
	DUSKTRACER_API uint16 KeeperSource(EDTPath Path);

	/** S1 rule (R-WPN-05): no weapon contributes a card to the L16 Reforging hand. Always empty in S1. */
	DUSKTRACER_API TArray<FName> ReforgeCards(int32 Weapon);
}

namespace DTModCompat
{
	/**
	 * THE compatibility function (modifiers.md R-MOD-01, R-MOD-02), used by offers, sockets and composition alike:
	 * Caps contains one of the modifier's required caps (none required = any), none of its blocked caps, and, when the
	 * modifier carries a flag, that flag resolves `native` on Caps.
	 */
	DUSKTRACER_API bool IsCompatible(const FDTCapList& Caps, int32 Mod);
	/** Against the weapon's declared caps (offers, the matrix). */
	DUSKTRACER_API bool IsCompatible(int32 Weapon, int32 Mod);
	/** By ids (Weapon.* / Mod.*); false for an unknown id. */
	DUSKTRACER_API bool IsCompatible(FName WeaponId, FName ModId);
}
