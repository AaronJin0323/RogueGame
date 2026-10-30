// Per-player run state and owner-tagged events (07_Technical R-TEC-01).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRngStreams.h"
#include "Math/RandomStream.h"

enum class EDTGameEventKind : uint8
{
	Hit,
	Kill,
	Pickup
};

/** Every hit, kill and pickup names the player it belongs to. */
struct FDTGameEvent
{
	EDTGameEventKind Kind = EDTGameEventKind::Hit;
	int32 OwnerId = INDEX_NONE;
	int32 Amount = 0;
};

/** Optional observer of every event offered to a run (telemetry). Called after routing, for routed and rejected events. */
class IDTRunEventSink
{
public:
	virtual ~IDTRunEventSink() = default;
	virtual void OnRunEvent(const FDTGameEvent& Event, bool bRouted) = 0;
};

/** A Keeper's Path: the key Keepers, archetypes and blessings are authored by (Keeper.<Path>). S1 uses Storm, Forge, Tide. */
enum class EDTPath : uint8
{
	Storm,
	Forge,
	Tide,
	Shade,
	Radiance,
	Quicksilver,
	Count,
	None = 0xFF
};

/** Offer card kinds (offers.md Data). */
enum class EDTCardKind : uint8
{
	Weapon,
	WeaponUpgrade,
	Mod,
	Blessing,
	Keeper,
	Reforging,
	Reserve
};

/** Card rarity, rolled per card after the draw (offers.md R-OFF-05). */
enum class EDTRarity : uint8
{
	Common,
	Rare,
	Epic
};

/** What opened a hand (offers.md Data). */
enum class EDTHandKind : uint8
{
	Level,
	Chest,
	Keeper,
	Reforging,
	Patron
};

/** One card of a hand (offers.md Data). Ids are the tag spelling (Weapon.*, Mod.*, Blessing.*, Keeper.*). */
struct FDTOfferCard
{
	EDTCardKind Kind = EDTCardKind::Reserve;
	FName ContentId;
	/** The weapon a modifier or upgrade binds to; NAME_None if unbound. */
	FName TargetWeapon;
	EDTRarity Rarity = EDTRarity::Common;
	/** R-OFF-02 weight before sampling. */
	float Weight = 0.0f;
	bool bGuaranteed = false;
	bool bLocked = false;
};

/** One hand of cards (offers.md Data). */
struct FDTOfferHand
{
	/** Per player, monotonic. */
	int32 HandId = INDEX_NONE;
	EDTHandKind HandKind = EDTHandKind::Level;
	int32 Level = 0;
	TArray<FDTOfferCard> Cards;
	bool bGuaranteeFilled = false;
	/** The hand this one rerolled; INDEX_NONE if none. */
	int32 RerollOf = INDEX_NONE;
};

/** A player's offer state (offers.md Data, R-TEC-01). Behaviour lives in the offer system. */
struct FDTPlayerOfferState
{
	int32 RerollsLeft = DTConst::OFFER_REROLLS;
	int32 BanishLeft = DTConst::OFFER_BANISH;
	int32 LockLeft = DTConst::OFFER_LOCK;
	/** Banished content ids. */
	TSet<FName> Banished;
	/** The locked card, if any. */
	TOptional<FDTOfferCard> Locked;
	/** Hands waiting to be shown, oldest first. */
	TArray<FDTOfferHand> PendingHands;
	EDTPath PatronPath = EDTPath::None;
	EDTPath SecondPath = EDTPath::None;
	/** This player's own Offer stream (seeded by FDTRunState::Start from the run seed and the player index). */
	FRandomStream Rng;
};

/** A player's Keepers (keepers.md Data, R-KEE-19). Arrays are indexed by EDTPath. */
struct FDTKeeperRunState
{
	EDTPath Primary = EDTPath::None;
	EDTPath Second = EDTPath::None;
	/** Owned blessing ids (Blessing.<Path>.<Archetype>.<Column>), in pick order. */
	TArray<FName> Owned;
	int32 Favor[static_cast<int32>(EDTPath::Count)] = {};
	/** Unlocked tier, 0 (none) to 3; never lost (R-KEE-07). */
	int32 Tier[static_cast<int32>(EDTPath::Count)] = {};
	/** Leading archetype id (Archetype.<Path>.<Name>) per Path; NAME_None if no Key is owned. */
	FName Leading[static_cast<int32>(EDTPath::Count)];
	bool bCommitDone = false;
};

/**
 * One owned weapon (weapons.md Data). Behaviour lives in the weapon system (Combat/DTWeaponSim.h); this is only its
 * per-player state. Ids are the tag spelling (Weapon.*, Mod.*).
 */
struct FDTWeaponInstance
{
	/** A socketed modifier (modifiers.md R-MOD-04, R-MOD-08): dormant = kept but inert while the weapon cannot express it. */
	struct FSocket
	{
		FName ModId;
		bool bDormant = false;
	};

	/** Weapon.<Name>. */
	FName Id;
	/** Row index in DTCat::Weapons. */
	int32 Def = INDEX_NONE;
	int32 Level = 1;
	/** Acquisition order within the player (load ties, R-WPN-03). */
	int32 AcquiredOrder = 0;
	/** Seconds until the next volley; <= 0 = ready. */
	float Cooldown = 0.0f;
	TArray<FSocket> Sockets;
	/** Active pattern conversion as a DTCat::EFlag value (ConvertMelee / ConvertOrbit); 0xFF = none (R-WPN-10). */
	uint8 Conversion = 0xFF;
	/** Volleys fired (every-Nth triggers). */
	int32 ShotCounter = 0;
	/** FDTTelemetrySources id of Weapon.<Name>. */
	uint16 SourceId = 0;
	/** Row in DTCat::Reforges this weapon evolved by (reforging.md R-RFG-03); INDEX_NONE = not evolved. */
	int32 Evolve = INDEX_NONE;
};

/** Everything one player owns during a run. Systems add their fields here, never to a global. */
struct FDTPlayerRunState
{
	int32 PlayerIndex = INDEX_NONE;
	// Progression (progression.md Data): XP is within the current level; XPToNext comes from the curve.
	int32 Level = 1;
	int32 XP = 0;
	int32 XPToNext = 0;
	/** Cumulative XP, for telemetry. */
	int64 XPTotal = 0;
	int64 DamageDealt = 0;
	int32 Kills = 0;
	int32 Pickups = 0;
	FDTPlayerOfferState Offer;
	FDTKeeperRunState Keepers;
	/** Owned weapons in slot order (at most C.WEAPON.SLOTS); the slot index is FDTHit::WeaponSlot. */
	TArray<FDTWeaponInstance> Weapons;
	/** Path rows of DTCat::Reforges taken (reforging.md R-RFG-03). */
	TArray<int32> PathReforges;
};

/** The run: its seed-derived streams and one state per player. There is no single-player shortcut. */
class DUSKTRACER_API FDTRunState
{
public:
	void Start(int32 RunSeed, int32 NumPlayers);

	int32 GetNumPlayers() const { return Players.Num(); }
	bool IsValidPlayer(int32 PlayerIndex) const { return Players.IsValidIndex(PlayerIndex); }
	FDTPlayerRunState& GetPlayer(int32 PlayerIndex) { return Players[PlayerIndex]; }
	const FDTPlayerRunState& GetPlayer(int32 PlayerIndex) const { return Players[PlayerIndex]; }
	FDTRngStreams& GetRng() { return Rng; }

	/** Routes an event to its owner. Returns false (and changes nothing) when the owner is not a player of this run. */
	bool ApplyEvent(const FDTGameEvent& Event);

	/** Installs (or clears with nullptr) the observer that sees every ApplyEvent. Survives Start(); not owned. */
	void SetEventSink(IDTRunEventSink* InSink) { Sink = InSink; }
	IDTRunEventSink* GetEventSink() const { return Sink; }

private:
	FDTRngStreams Rng;
	TArray<FDTPlayerRunState> Players;
	IDTRunEventSink* Sink = nullptr;
};
