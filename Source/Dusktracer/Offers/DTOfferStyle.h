// Card identity (offers.md R-OFF-13, 04_Presentation R-PRE-08): every card kind has its own border, icon and label, and
// every rarity its own marker, each channel alone enough to tell them apart. Text is FText from the string tables.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"

/** Border channel: tech kinds are machined (double rule, chevrons, socket notches), Keeper kinds are stone (heavy rule, crown band, inner frame), Reserve is plain stock (dashed). */
enum class EDTCardBorder : uint8
{
	TechDouble,   // Weapon
	TechChevron,  // WeaponUpgrade
	TechSocket,   // Mod
	StoneHeavy,   // Blessing
	StoneCrown,   // Keeper
	StoneInner,   // Reforging
	PlainDashed   // Reserve
};

/** Icon channel: a line glyph drawn on the card. */
enum class EDTCardIcon : uint8
{
	Crosshair,  // Weapon
	UpArrow,    // WeaponUpgrade
	Hex,        // Mod
	Diamond,    // Blessing
	Crown,      // Keeper
	Anvil,      // Reforging
	Cross       // Reserve
};

/** Card family material (the colour token of border and icon). */
enum class EDTCardFamily : uint8
{
	Tech,
	Stone,
	Plain
};

struct FDTCardStyle
{
	EDTCardBorder Border = EDTCardBorder::PlainDashed;
	EDTCardIcon Icon = EDTCardIcon::Cross;
	EDTCardFamily Family = EDTCardFamily::Plain;
	/** ST_UI key of the kind label (label channel). */
	const TCHAR* LabelKey = TEXT("");
};

namespace DTOfferStyle
{
	inline constexpr int32 NumCardKinds = static_cast<int32>(EDTCardKind::Reserve) + 1;
	inline constexpr int32 NumRarities = static_cast<int32>(EDTRarity::Epic) + 1;
	inline constexpr int32 NumHandKinds = static_cast<int32>(EDTHandKind::Patron) + 1;

	DUSKTRACER_API FDTCardStyle ForKind(EDTCardKind Kind);
	/** Palette token of a card family (Pal.Card.*). */
	DUSKTRACER_API FColor FamilyToken(EDTCardFamily Family);
	/** Rarity marker: pip count (1..3) and token (Pal.Rarity.*) and its ST_UI label key. */
	DUSKTRACER_API int32 RarityPips(EDTRarity Rarity);
	DUSKTRACER_API FColor RarityToken(EDTRarity Rarity);
	DUSKTRACER_API const TCHAR* RarityLabelKey(EDTRarity Rarity);
	/** ST_UI title key per hand kind (ST_UI.Offer.Hand.<HandKind>). */
	DUSKTRACER_API const TCHAR* HandTitleKey(EDTHandKind Kind);
}

/** Card text from the string tables: name, the binding line (target, levels, Keeper), and the rules line. */
namespace DTOfferText
{
	/** String table id and key of a card's name / rules line (empty table for a kind without text, e.g. S2 Reforging). */
	DUSKTRACER_API bool NameKey(const FDTOfferCard& Card, FString& OutTable, FString& OutKey);
	DUSKTRACER_API bool DescKey(const FDTOfferCard& Card, FString& OutTable, FString& OutKey);
	DUSKTRACER_API FText Name(const FDTOfferCard& Card);
	/** Rules text with its format arguments filled from the catalog (expression.csv parameters, C.OFFER.RESERVE_*). */
	DUSKTRACER_API FText Desc(const FDTOfferCard& Card);
	/** "Level 2 -> 3", "Socket into Pulse Rifle", "Keeper · Archetype"; empty when the kind has none. */
	DUSKTRACER_API FText Line(const FDTOfferCard& Card, const FDTPlayerRunState& Player);
	DUSKTRACER_API FText Ui(const TCHAR* Key);
	/** Every string-table key the offer screen can read (T-OFF-13 text coverage). */
	DUSKTRACER_API TArray<TPair<FString, FString>> AllKeys();
}
