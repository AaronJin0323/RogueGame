// The behaviour-flag layer of a weapon (weapons.md R-WPN-08, R-WPN-09; modifiers.md R-MOD-05).
//
// THE SEAM FOR KEEPERS: a flag source (a blessing, a tier line, an Intercession) never touches weapon code. It builds
// an FDTWeaponFlags and hands it to UDTWeaponSubsystem::AddFlagSource(OwnerId, SourceTag, Flags) (or
// FDTWeaponSim::AddFlagSource); SourceTag names the source (e.g. Blessing.Storm.Tempest.Key) so re-adding replaces
// it and RemoveFlagSource drops it. Every entry applies to every weapon of that player, owned now or later
// (keepers.md R-KEE-03). The weapon system composes, caps and expresses the flags itself.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTCatalogGen.h"

/** How a numeric parameter stacks with the same parameter of another source (R-WPN-09). */
enum class EDTParamOp : uint8
{
	Add, // counts and magnitudes (and every "+x" delta of expression.csv)
	Max, // ranges and times
	Mul  // damage and scale factors
};

/** Which kind of source granted an entry; a conversion keeps the highest (R-WPN-10: capstone > key > tier > trait). */
enum class EDTFlagTier : uint8
{
	Modifier,
	Trait,
	Tier,
	Key,
	Capstone
};

/** Named parameters of one flag (names are the expression.csv / modifiers.csv vocabulary). */
struct DUSKTRACER_API FDTFlagParams
{
	struct FNum
	{
		FName Name;
		float Value = 0.0f;
		EDTParamOp Op = EDTParamOp::Add;
	};
	TArray<FNum> Nums;
	/** Word parameters (status=Status.Burn, pattern=Fan, on=Hit, trigger=A|B, leave=Field); the later source wins. */
	TArray<TPair<FName, FName>> Words;

	/** Sets or stacks Name. Op defaults to DefaultOp(Name). */
	void Set(FName Name, float Value);
	void Set(FName Name, float Value, EDTParamOp Op);
	/** Overwrites Name (a cap after stacking), keeping its op. */
	void Replace(FName Name, float Value);
	void SetWord(FName Name, FName Value);

	float Get(FName Name, float Default = 0.0f) const;
	bool Has(FName Name) const;
	FName GetWord(FName Name) const;
	/** True when the word Name contains Token as one of its '|'-separated parts. */
	bool WordHas(FName Name, const TCHAR* Token) const;

	/** Stacks every parameter of Other into this one (R-WPN-09). */
	void Merge(const FDTFlagParams& Other);

	/** Default op by name: "*_s" and "*range*" and "*_uu" without a "+" source take the largest, "*mult*"/"*scale" multiply, else add. */
	static EDTParamOp DefaultOp(FName Name);
};

/** One flag from one source. Entries with the same Flag, SourceId and `on` word are stacked into one when composed. */
struct FDTFlagEntry
{
	DTCat::EFlag Flag = DTCat::EFlag::None;
	/** Telemetry source of everything this entry adds (Keeper.<Path> / Modifier.<Name>, R-WPN-07). */
	uint16 SourceId = 0;
	EDTFlagTier Tier = EDTFlagTier::Trait;
	FDTFlagParams Params;
};

/** A set of flag entries: what a source grants, and (composed) what a weapon carries. */
struct FDTWeaponFlags
{
	TArray<FDTFlagEntry> Entries;

	FDTFlagEntry& Add(DTCat::EFlag Flag, uint16 SourceId, EDTFlagTier Tier)
	{
		FDTFlagEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Flag = Flag;
		Entry.SourceId = SourceId;
		Entry.Tier = Tier;
		return Entry;
	}
	bool Has(DTCat::EFlag Flag) const { return Entries.ContainsByPredicate([Flag](const FDTFlagEntry& E) { return E.Flag == Flag; }); }
};

/** Parameter names shared by the flag expressions (the expression.csv vocabulary). */
namespace DTFlagParam
{
	DUSKTRACER_API extern const FName Chance, Children, ChildDmg, FanDeg, Gen, Pattern, On, Trigger, N, EveryNVolley;
	DUSKTRACER_API extern const FName Jumps, RangeUu, Dmg, DelaySeconds, Count, RadiusScale, RadiusUu, DurationS, Dps;
	DUSKTRACER_API extern const FName MaxLive, Status, Leave, DistanceUu, Target, Rounds, ArcDeg, DmgMult, DmgRatio;
	DUSKTRACER_API extern const FName Bounces, SeekRangeUu, FreezeStacks, FreezeDurationScale, ChillStacksOnPull;
	DUSKTRACER_API extern const FName BurnStacksMax, StillMoveUu, ChargeS, Mult, ChargesMax, ChargedPierce;
	DUSKTRACER_API extern const FName CrushDmg, PulledVuln, PulledVulnSeconds, DmgVsShocked, ReturnSameTarget, ForkOnKill, Retarget;
	DUSKTRACER_API extern const FName BurningExplodeOnDeathUu, ExplodeDmg, SpreadBurnOnKillUu, SpreadBurnIntervalS;
	DUSKTRACER_API extern const FName ShatterOnFrozenKillUu, ShatterUu, ShatterDmg, ShatterStatus, Shatter, ClusterFreezeMin;
	DUSKTRACER_API extern const FName FrozenBlocksEnemies, FreezeUntilHit, HeldMax, FrozenHitChillsNeighboursUu;
	DUSKTRACER_API extern const FName ChargedPullUu, ChargedAuraUu, AuraStatus, AuraIntervalS, ChargedFreezeRingUu, Around;
}
