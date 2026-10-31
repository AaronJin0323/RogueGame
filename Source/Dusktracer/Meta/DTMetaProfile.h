// The meta save's data, its canonical bytes, its validator and the run award (02_Systems/meta.md). Plain C++: no engine state.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"

/** Source §5.1: both irreversible, per profile. */
enum class EDTFateTide : uint8 { Bound, Returned };
enum class EDTFateShade : uint8 { Bound, OffLedger };

/** How a banked run ended (R-MET-05). Left: a restart or quit from a run still going, or a tool's own end. */
enum class EDTRunEnd : uint8 { Victory, Death, Left };

/** One profile, in payload order (meta.md §Data). Later systems fill their fields; a new field raises C.SAVE.VERSION. */
struct DUSKTRACER_API FDTMetaProfile
{
	int64 Currency = 0;
	int32 Runs = 0;
	int32 Wins = 0;
	int32 DeathsSinceWin = 0;
	FName LastPatron;
	TMap<FName, int32> TreeRanks;
	TSet<FName> Unlocked;
	TSet<FName> Discoveries;
	TMap<FName, int64> LifetimeStats;
	int32 Act = DTConst::META_ACT_START;
	TSet<FName> StoryFlags;
	int32 RecollectionsRead = 0;
	TMap<FName, int32> ArenaWins;
	TMap<FName, int32> ArenaHeat;
	EDTFateTide FateTide = EDTFateTide::Bound;
	EDTFateShade FateShade = EDTFateShade::Bound;
	TSet<FName> FateSeen;
};

/** What one run earned, counted on the sim clock (R-MET-03). */
struct FDTRunTally
{
	int64 Steps = 0;
	int64 Kills = 0;
	int64 Elites = 0;
	bool bBossKilled = false;
	int32 Heat = 0;
	FName Patron;
	/** Per-run counters for run-end systems, added to LifetimeStats by name (R-MET-19); a `Best.` key keeps its highest value (R-MET-44). */
	TMap<FName, int64> Counters;
};

/** What a bank produced. Retained is the run's; Balance is the profile's after it. */
struct FDTRunAward
{
	bool bBanked = false;
	int64 Retained = 0;
	int64 Balance = 0;
	/** The Discoveries this bank added, sorted by key (R-MET-44). */
	TArray<FName> Discovered;
};

namespace DTMeta
{
	/** The canonical payload (R-MET-08): one profile always gives the same bytes. */
	DUSKTRACER_API TArray<uint8> Serialize(const FDTMetaProfile& Profile);
	/** False on anything but exactly one well-formed profile. */
	DUSKTRACER_API bool Deserialize(const TArray<uint8>& Payload, FDTMetaProfile& Out);
	/** R-MET-09. */
	DUSKTRACER_API bool IsValid(const FDTMetaProfile& Profile);
	/** True for a profile with nothing in it (R-MET-16). */
	DUSKTRACER_API bool IsEmpty(const FDTMetaProfile& Profile);
	DUSKTRACER_API bool Same(const FDTMetaProfile& A, const FDTMetaProfile& B);

	/** R-MET-02. */
	DUSKTRACER_API int64 ComputeAward(const FDTRunTally& Tally);
	/** R-MET-05; returns the award. */
	DUSKTRACER_API int64 ApplyRun(FDTMetaProfile& Profile, const FDTRunTally& Tally, EDTRunEnd End);
}
