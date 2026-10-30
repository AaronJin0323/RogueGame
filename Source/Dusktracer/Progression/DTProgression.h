// XP, levels and the hands they queue (02_Systems/progression.md R-PRG-01, -02, -05, -06). Plain C++ so tests drive it.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"

/** One level gained by one player (raised in EDTSimPhase::Pickups, in level order). */
struct FDTLevelGain
{
	int32 OwnerId = INDEX_NONE;
	/** The level reached. */
	int32 Level = 0;
};

/** Per-player multipliers progression reads (R-PRG-01, R-PRG-08). Identity by default; FDTStatSet writes them when it exists. */
struct FDTProgressionStats
{
	float XpGainMul = 1.0f;
	float PickupRadiusMul = 1.0f;
};

/**
 * The XP curve and the level rules. The queue entry a level or chest adds to FDTPlayerOfferState::PendingHands is a
 * frozen FDTOfferHand with only HandKind and Level set: HandId stays INDEX_NONE and Cards stay empty until the offer
 * system fills the hand when it opens it (offers.md R-OFF-14). PendingHands is "waiting to be shown": the open hand is
 * not in it.
 */
struct DUSKTRACER_API FDTProgression
{
	/** R-PRG-02: XP needed to go from Level to Level + 1 (catalog row, then x C.XP.GROWTH per level, rounded). */
	static int32 XPToNext(int32 Level);

	/** R-PRG-06: the hand a level grants (Keeper at C.OFFER.SECOND_KEEPER_LEVEL, Reforging at C.OFFER.REFORGING_LEVEL). */
	static EDTHandKind HandKindForLevel(int32 Level);

	/** Sets XPToNext for every player that has none yet (a fresh FDTRunState starts at level 1 with zero). */
	static void EnsureCurve(FDTRunState& Run);

	/**
	 * Adds Amount XP to one player (R-PRG-05): carries the overflow, passes as many levels as it pays for and queues one
	 * hand per level in level order. Counts into XPTotal. Returns the number of levels gained; appends them to OutLevels.
	 */
	static int32 AddXP(FDTRunState& Run, int32 OwnerId, int64 Amount, TArray<FDTLevelGain>* OutLevels = nullptr);

	/**
	 * R-PRG-01: one collected gem of Value. Emits the Pickup event with the collector's OwnerId (Amount = the collector's
	 * credit, which also counts it into the collector's XPTotal), then credits Value x each player's XpGainMul to every
	 * player of the run. XpGainMul is indexed by player (missing = 1). OutCredits[p] receives each player's credit.
	 * Returns false (and changes nothing) when Collector is not a player of the run.
	 */
	static bool CreditPickup(FDTRunState& Run, int32 Collector, int32 Value, TConstArrayView<float> XpGainMul,
		TArray<FDTLevelGain>* OutLevels = nullptr, TArray<int64>* OutCredits = nullptr);

	/**
	 * Queues one hand. Level, Keeper and Reforging hands go before the first queued chest hand, chest hands at the back
	 * (offers.md R-OFF-14: level-up hands first in level order, then chest hands).
	 */
	static void QueueHand(FDTPlayerOfferState& Offer, EDTHandKind Kind, int32 Level);

	/** XP still needed by this player to reach TargetLevel (0 when already there). */
	static int64 XPToReach(const FDTPlayerRunState& Player, int32 TargetLevel);

private:
	static int32 GrantXP(FDTPlayerRunState& Player, int64 Amount, bool bCountTotal, TArray<FDTLevelGain>* OutLevels);
};
