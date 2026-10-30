// Keeper content (02_Systems/keepers.md): parses the effect specs of 09_Catalog/expression.csv (generated as
// DTCat::Archetypes, D-014), validates them at load (R-KEE-02, R-KEE-20), turns a player's Keeper state into one weapon
// flag source per Keeper (R-KEE-03, R-KEE-07) and composes the Intercession of a cast (R-KEE-14). Pure functions: no
// world, no randomness, so every rule is testable headless.
//
// Effect spec grammar (keepers.md Data): clauses `Flag.<Name>{param=value;...}` joined by " + ". A value is a number,
// `true`/`false` (1/0), or a word (`Fan`, `Status.Shock`, `DashLanding|EveryNMelee`). A number written `+x` / `-x` is
// relative: it adds to the same parameter of the Keeper's earlier cards; a plain number replaces it.
#pragma once

#include "CoreMinimal.h"
#include "Combat/DTWeaponFlags.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"

/** One numeric parameter of a clause. */
struct FDTSpecNum
{
	FName Name;
	float Value = 0.0f;
	/** Written "+x" or "-x": adds to the earlier value instead of replacing it. */
	bool bRelative = false;
};

/** One `Flag.<Name>{...}` clause. */
struct DUSKTRACER_API FDTSpecClause
{
	DTCat::EFlag Flag = DTCat::EFlag::None;
	TArray<FDTSpecNum> Nums;
	TArray<TPair<FName, FName>> Words;

	bool HasNum(FName Name) const;
	float GetNum(FName Name, float Default = 0.0f) const;
	FName GetWord(FName Name) const;
	void RemoveWord(FName Name);
};

/** A parsed effect spec. */
struct FDTEffectSpec
{
	TArray<FDTSpecClause> Clauses;
	bool HasFlag(DTCat::EFlag Flag) const { return Clauses.ContainsByPredicate([Flag](const FDTSpecClause& C) { return C.Flag == Flag; }); }
};

/** Which card of an archetype a blessing is (R-KEE-01): Key, Amp1/Amp2 (Amp) or Trait. */
enum class EDTBlessingRole : uint8
{
	Key,
	Amp,
	Trait
};

/** What a cast does, read at cast time (R-KEE-14): the primary's base, the primary's leading variant, the second's layer. */
struct FDTIntercessionPlan
{
	EDTPath Base = EDTPath::None;
	/** Row in DTCat::Archetypes of the primary's leading archetype; INDEX_NONE = base only. */
	int32 Variant = INDEX_NONE;
	/** Row of the second Keeper's leading archetype; INDEX_NONE = no layer (before the commitment or no Key owned). */
	int32 Layer = INDEX_NONE;
	EDTPath LayerPath = EDTPath::None;
	FDTEffectSpec VariantSpec;
	FDTEffectSpec LayerSpec;
	/** Layer clauses whose flag the variant also shapes: their shape words were dropped (the variant's shape wins). */
	int32 ConflictsResolved = 0;
};

namespace DTKeepers
{
	// --- names -----------------------------------------------------------------------------------------------------
	/** Every Keeper's flag source tag (Keeper.<Path>): one per Keeper and player, replaced on every rebuild. */
	DUSKTRACER_API FName SourceTag(EDTPath Path);
	/** Telemetry source Keeper.<Path> (R-KEE-13). */
	DUSKTRACER_API uint16 KeeperSource(EDTPath Path);
	/** Pal.Keeper.<Path> (R-KEE-09, R-PRE-01). */
	DUSKTRACER_API FColor TrailToken(EDTPath Path);
	/** The enchantment status of a Path (EnchantStatus column, first status), false when the Path has no row. */
	DUSKTRACER_API bool EnchantStatus(EDTPath Path, EDTStatus& OutStatus);
	/** EnchantStatus parsed from the catalog now (EnchantStatus caches it). */
	DUSKTRACER_API bool EnchantStatusUncached(EDTPath Path, EDTStatus& OutStatus);
	DUSKTRACER_API bool ParseStatus(FName Word, EDTStatus& OutStatus);
	DUSKTRACER_API int32 FindArchetype(FName ArchetypeId);

	// --- parsing and validation ------------------------------------------------------------------------------------------
	/** Parses Text; false (with a reason) for an unknown flag, a clause that is not Flag.*, a bad value or bad braces. */
	DUSKTRACER_API bool ParseSpec(const TCHAR* Text, FDTEffectSpec& Out, FString* OutError = nullptr);
	/**
	 * R-KEE-02: a Key carries exactly one behaviour flag, its archetype's key flag (Flag.StatusApply riders are a status,
	 * not a behaviour, unless StatusApply is the key flag); an Amp or Trait changes or adds a Flag.* parameter. A
	 * blessing whose whole effect is a stat delta (damage, rate, crit, armor, HP) is rejected.
	 */
	DUSKTRACER_API bool ValidateBlessing(const TCHAR* Text, EDTBlessingRole Role, DTCat::EFlag KeyFlag, FString* OutError = nullptr);
	/**
	 * R-KEE-01, R-KEE-02, R-KEE-20 over a set of rows: C.KEEPER.ARCHETYPES_PER_KEEPER rows per Keeper of the run, ids
	 * Blessing.<Path>.<Archetype>.<column>, unique key flags, every spec parsing, every IncompatFlags pair symmetric.
	 */
	DUSKTRACER_API bool ValidateCatalog(TConstArrayView<DTCat::FArchetypeRow> Rows, TArray<FString>& OutErrors);
	/** The load result of the generated catalog (validated once): false for a blessing rejected at load (never granted flags). */
	DUSKTRACER_API bool IsBlessingAccepted(int32 Archetype, int32 Column);
	/** Errors of the generated catalog's load validation (empty when it is clean). */
	DUSKTRACER_API const TArray<FString>& CatalogErrors();

	// --- flags -----------------------------------------------------------------------------------------------------------
	/**
	 * One Keeper's weapon flags for a player (R-KEE-03, R-KEE-07): every owned blessing of Path in pick order, then the tier
	 * lines Tier1..Tier[Path] of the Path's current leading archetype, then Overlay (an active Intercession). Clauses of the
	 * same flag and `on` word fold into one entry: relative numbers add, plain numbers and words replace. A clause with
	 * every_n_volley and its own pattern is a separate per-volley entry (on=Volley). Empty when nothing applies.
	 */
	DUSKTRACER_API FDTWeaponFlags BuildKeeperFlags(const FDTKeeperRunState& Keepers, EDTPath Path,
		TConstArrayView<FDTSpecClause> Overlay = TConstArrayView<FDTSpecClause>());
	/** Folds Clause into Flags (the merge rule of BuildKeeperFlags), as SourceId at Tier. */
	DUSKTRACER_API void FoldClause(FDTWeaponFlags& Flags, const FDTSpecClause& Clause, uint16 SourceId, EDTFlagTier Tier);
	/** Owned blessings of Path. */
	DUSKTRACER_API int32 OwnedOfPath(const FDTKeeperRunState& Keepers, EDTPath Path);

	/**
	 * A parameter of Path's own content, from the first archetype row (catalog order) whose Key names Flag with a plain
	 * value of Param; Default when none does. Used where a base names a mechanic but not its number (keepers.md §6).
	 */
	DUSKTRACER_API float PathParam(EDTPath Path, DTCat::EFlag Flag, FName Param, float Default);

	// --- Intercession ----------------------------------------------------------------------------------------------------
	/** R-KEE-14: base = primary; variant = primary's leading archetype; layer = second's leading archetype from the commitment. */
	DUSKTRACER_API FDTIntercessionPlan ComposeIntercession(const FDTKeeperRunState& Keepers);
	/** Shape words a variant owns when a layer names the same flag (R-KEE-14 conflict rule). */
	DUSKTRACER_API bool IsShapeWord(FName Name);
}
