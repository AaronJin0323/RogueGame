// The five-card offer generator (02_Systems/offers.md). Pure functions of (player run state, catalogs, stream): no world,
// no UObject, so tests drive every rule headless. The world flow (pause, screen, telemetry, weapon calls) is
// UDTOfferSubsystem (Offers/DTOfferSubsystem.h).
//
// Reads, never re-derives: modifier legality is DTModCompat::IsCompatible (Combat/DTWeaponCatalog.h) on the caps the
// caller passes per slot; second-Keeper legality is the IncompatFlags column of DTCat::Archetypes (keepers.md R-KEE-16)
// against the flags a blessing's effect spec names; the run pool is the catalogs' `starter` column plus unlocks (D-013).
#pragma once

#include "CoreMinimal.h"
#include "Combat/DTWeaponCatalog.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTRunState.h"

class FDTWeaponSim;
enum class EDTWeaponResult : uint8;

/** Reserve card kinds (R-OFF-12), in fill order. Ids are Reserve.<Name>. */
enum class EDTReserveKind : uint8
{
	Restore,
	Hone,
	SpareTerms,
	Salvage,
	Mend,
	Count
};

/** What the generator reads besides the player state. */
struct FDTOfferPool
{
	/** Non-starter ids put in the run pool (DT.Unlock until meta, D-013). */
	TSet<FName> Unlocked;
	/** The caps each owned weapon slot expresses now (UDTWeaponSubsystem composes them); a missing slot uses its declared caps. */
	TArray<FDTCapList> SlotCaps;
};

/** A blessing's catalog address (keepers.md Data: Blessing.<Path>.<Archetype>.<Key|Amp1|Amp2|Trait>). */
struct FDTBlessingInfo
{
	/** Row in DTCat::Archetypes, INDEX_NONE for an unknown id. */
	int32 Archetype = INDEX_NONE;
	/** 0 Key, 1 Amp1, 2 Amp2, 3 Trait. */
	int32 Column = 0;
	EDTPath Path = EDTPath::None;
	/** FlagBit mask of every Flag.* the effect spec names. */
	uint32 Flags = 0;
	bool IsValid() const { return Archetype != INDEX_NONE; }
};

/** What a pick asks of systems outside the run state (the caller performs it: UDTWeaponSubsystem, hero, keepers). */
struct FDTPickOutcome
{
	EDTCardKind Kind = EDTCardKind::Reserve;
	/** Weapon.* to give (Weapon), raise (WeaponUpgrade) or socket into (Mod). */
	FName Weapon;
	FName Mod;
	/** WeaponUpgrade: the level to raise to (capped at the weapon's top level). */
	int32 TargetLevel = 0;
	/** Blessing taken (already appended to FDTKeeperRunState::Owned). */
	FName Blessing;
	/** Keeper card: the Path committed and whether it was the Patron (run start) or the second Keeper (L8). */
	EDTPath Keeper = EDTPath::None;
	bool bPatron = false;
	EDTReserveKind Reserve = EDTReserveKind::Count;
	bool bValid = false;
};

namespace DTOffers
{
	// --- ids -------------------------------------------------------------------------------------------------------
	DUSKTRACER_API FName ReserveId(EDTReserveKind Kind);
	/** EDTReserveKind::Count when Id is not a Reserve id. */
	DUSKTRACER_API EDTReserveKind ParseReserve(FName Id);
	DUSKTRACER_API FName KeeperId(EDTPath Path);
	/** EDTPath::None when Id is not Keeper.<Path>. */
	DUSKTRACER_API EDTPath ParseKeeper(FName Id);
	DUSKTRACER_API const TCHAR* PathName(EDTPath Path);

	// --- catalog reads -------------------------------------------------------------------------------------------------
	/** The Keepers of the run (every Path with an expression.csv row), in Path order. */
	DUSKTRACER_API TArray<EDTPath> RunKeepers();
	DUSKTRACER_API FDTBlessingInfo FindBlessing(FName Id);
	/** Flag.* tokens named in an effect spec, as a FlagBit mask. */
	DUSKTRACER_API uint32 SpecFlags(const TCHAR* Spec);
	/** Numeric parameters of an effect spec ("+x" read as x), for card text arguments. Later flags overwrite. */
	DUSKTRACER_API TArray<TPair<FString, double>> SpecParams(const TCHAR* Spec);
	/** R-OFF-07: in the run pool = a starter row within the C.POOL.* cap (catalog order) or an unlocked id. */
	DUSKTRACER_API bool IsWeaponInPool(int32 Weapon, const FDTOfferPool& Pool);
	DUSKTRACER_API bool IsModInPool(int32 Mod, const FDTOfferPool& Pool);
	/** Archetype keys are the starter blessings (within C.POOL.STARTER_BLESSINGS, catalog order). */
	DUSKTRACER_API bool IsStarterKey(int32 Archetype);

	// --- Keeper state (keepers.md R-KEE-06, R-KEE-07; state only) ------------------------------------------------------
	/** Owned cards of one archetype row. */
	DUSKTRACER_API int32 OwnedInArchetype(const FDTKeeperRunState& Keepers, int32 Archetype);
	/** R-KEE-06: most owned cards among archetypes whose Key is owned; tie to the most recently taken card; none = INDEX_NONE. */
	DUSKTRACER_API int32 LeadingArchetype(const FDTKeeperRunState& Keepers, EDTPath Path);
	/** R-KEE-07: the tier Favor unlocks (0..3). */
	DUSKTRACER_API int32 TierForFavor(int32 Favor);
	/** Appends a blessing and updates Favor, Tier (never lost) and Leading for its Path. False for an unknown or owned id. */
	DUSKTRACER_API bool AddBlessing(FDTKeeperRunState& Keepers, FName BlessingId);

	// --- legality and weight (R-OFF-02, R-OFF-03) ----------------------------------------------------------------------
	DUSKTRACER_API bool IsCommitted(const FDTPlayerRunState& Player);
	/** Card identity order (R-OFF-04): kind, content id, target weapon. */
	DUSKTRACER_API bool IdentityLess(const FDTOfferCard& A, const FDTOfferCard& B);
	DUSKTRACER_API bool SameIdentity(const FDTOfferCard& A, const FDTOfferCard& B);
	/** Every legal weapon, upgrade, modifier and blessing card for Player, sorted by identity, Weight filled. */
	DUSKTRACER_API TArray<FDTOfferCard> Candidates(const FDTPlayerRunState& Player, const FDTOfferPool& Pool);
	DUSKTRACER_API bool IsLegal(const FDTPlayerRunState& Player, const FDTOfferPool& Pool, const FDTOfferCard& Card);
	/** R-OFF-02 weight of a content card (weapon, upgrade, modifier, blessing) for Player. */
	DUSKTRACER_API float Weight(const FDTPlayerRunState& Player, const FDTOfferCard& Card);
	/** Keeper card weight: base x favor factor (R-OFF-08). */
	DUSKTRACER_API float KeeperWeight(const FDTPlayerRunState& Player, EDTPath Path);

	// --- hands ------------------------------------------------------------------------------------------------------
	/** The hand a request becomes: a Level hand at C.OFFER.SECOND_KEEPER_LEVEL is the Keeper hand, at C.OFFER.REFORGING_LEVEL the Reforging hand. */
	DUSKTRACER_API EDTHandKind ResolveKind(const FDTPlayerRunState& Player, EDTHandKind Requested, int32 Level);
	DUSKTRACER_API int32 HandSize(EDTHandKind Kind);
	/**
	 * Builds one hand (HandId and RerollOf are the caller's). Carried: locked cards that keep their place (the lock from
	 * the previous hand for a Level hand, the locked cards of the hand being rerolled); a carried card that is no longer
	 * legal is dropped and its index added to OutDropped. Draws only from Rng.
	 */
	DUSKTRACER_API FDTOfferHand BuildHand(const FDTPlayerRunState& Player, const FDTOfferPool& Pool, EDTHandKind Kind, int32 Level,
		FRandomStream& Rng, TConstArrayView<FDTOfferCard> Carried = TConstArrayView<FDTOfferCard>(), TArray<int32>* OutDropped = nullptr);
	/** R-OFF-12: the Reserve card of Kind (no Keeper, Favor or archetype). */
	DUSKTRACER_API FDTOfferCard MakeReserve(EDTReserveKind Kind);
	/** Replaces Hand.Cards[Slot] by a fresh legal draw not already in the hand (Reserve when none), rarity rolled. */
	DUSKTRACER_API void RefillSlot(const FDTPlayerRunState& Player, const FDTOfferPool& Pool, FDTOfferHand& Hand, int32 Slot, FRandomStream& Rng);
	/** R-OFF-05 roll (always two draws, so the stream use does not depend on the result). */
	DUSKTRACER_API EDTRarity RollRarity(FRandomStream& Rng);

	// --- tools (R-OFF-10) ---------------------------------------------------------------------------------------------
	DUSKTRACER_API bool HandAllowsTools(EDTHandKind Kind);
	/** Reserve, Keeper and Reforging cards refuse banish and lock. */
	DUSKTRACER_API bool CardAllowsBanishLock(const FDTOfferCard& Card);

	// --- picks ------------------------------------------------------------------------------------------------------
	/** Levels a WeaponUpgrade of this rarity adds (R-OFF-05). */
	DUSKTRACER_API int32 UpgradeLevels(EDTRarity Rarity);
	/**
	 * Applies the run-state part of a pick: blessings (Owned, Favor, Tier, Leading), Keeper commits (Patron / second),
	 * Reserve SpareTerms, the lock it consumes. Weapon, hero and Keeper-flag work is returned for the caller.
	 */
	DUSKTRACER_API FDTPickOutcome ApplyPickState(FDTPlayerRunState& Player, EDTHandKind HandKind, const FDTOfferCard& Card);
	/** Headless weapon part of a pick (tests, bots): Give / LevelUp / Socket on a bare FDTWeaponSim. */
	DUSKTRACER_API EDTWeaponResult ApplyToSim(FDTWeaponSim& Weapons, FDTRunState& Run, int32 OwnerId, const FDTPickOutcome& Outcome);
}
