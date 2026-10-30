#include "Offers/DTOfferStyle.h"

#include "Combat/DTReforge.h"
#include "Combat/DTWeaponCatalog.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Internationalization/Text.h"
#include "Offers/DTOfferGenerator.h"

namespace DTOfferStyleLocal
{
	const TCHAR* const ColumnNames[] = { TEXT("Key"), TEXT("Amp1"), TEXT("Amp2"), TEXT("Trait") };
	const TCHAR* const UiTable = TEXT("ST_UI");
	const TCHAR* const KeepersTable = TEXT("ST_Keepers");
	const TCHAR* const WeaponsTable = TEXT("ST_Weapons");
	const TCHAR* const ModifiersTable = TEXT("ST_Modifiers");
	const TCHAR* const KeywordsTable = TEXT("ST_Keywords");
	const TCHAR* const ReforgingTable = TEXT("ST_Reforging");
	const TCHAR* const ReserveNames[] = { TEXT("Restore"), TEXT("Hone"), TEXT("SpareTerms"), TEXT("Salvage"), TEXT("Mend") };
	static_assert(UE_ARRAY_COUNT(ReserveNames) == static_cast<int32>(EDTReserveKind::Count), "one text id per Reserve kind");
	constexpr double MetresPerUu = 0.01;
	constexpr double Percent = 100.0;

	/** "Storm.Tempest" for Archetype.Storm.Tempest. */
	FString ArchetypeKey(int32 Row)
	{
		FString Key(DTCat::Archetypes[Row].Id);
		Key.RemoveFromStart(TEXT("Archetype."));
		return Key;
	}

	FText Number(double Value)
	{
		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 2;
		Options.MinimumFractionalDigits = 0;
		Options.UseGrouping = false;
		return FText::AsNumber(Value, &Options);
	}

	FText FromTable(const FString& Table, const FString& Key)
	{
		return FText::FromStringTable(FName(*Table), Key);
	}
}

// ---- style ------------------------------------------------------------------------------------------------------------

FDTCardStyle DTOfferStyle::ForKind(EDTCardKind Kind)
{
	FDTCardStyle Style;
	switch (Kind)
	{
	case EDTCardKind::Weapon:
		Style = { EDTCardBorder::TechDouble, EDTCardIcon::Crosshair, EDTCardFamily::Tech, TEXT("Offer.Kind.Weapon") };
		break;
	case EDTCardKind::WeaponUpgrade:
		Style = { EDTCardBorder::TechChevron, EDTCardIcon::UpArrow, EDTCardFamily::Tech, TEXT("Offer.Kind.WeaponUpgrade") };
		break;
	case EDTCardKind::Mod:
		Style = { EDTCardBorder::TechSocket, EDTCardIcon::Hex, EDTCardFamily::Tech, TEXT("Offer.Kind.Mod") };
		break;
	case EDTCardKind::Blessing:
		Style = { EDTCardBorder::StoneHeavy, EDTCardIcon::Diamond, EDTCardFamily::Stone, TEXT("Offer.Kind.Blessing") };
		break;
	case EDTCardKind::Keeper:
		Style = { EDTCardBorder::StoneCrown, EDTCardIcon::Crown, EDTCardFamily::Stone, TEXT("Offer.Kind.Keeper") };
		break;
	case EDTCardKind::Reforging:
		Style = { EDTCardBorder::StoneInner, EDTCardIcon::Anvil, EDTCardFamily::Stone, TEXT("Offer.Kind.Reforging") };
		break;
	case EDTCardKind::Reserve:
		Style = { EDTCardBorder::PlainDashed, EDTCardIcon::Cross, EDTCardFamily::Plain, TEXT("Offer.Kind.Reserve") };
		break;
	}
	return Style;
}

FColor DTOfferStyle::FamilyToken(EDTCardFamily Family)
{
	switch (Family)
	{
	case EDTCardFamily::Tech: return DTPal::Card_Tech;
	case EDTCardFamily::Stone: return DTPal::Card_Stone;
	default: return DTPal::Card_Plain;
	}
}

int32 DTOfferStyle::RarityPips(EDTRarity Rarity)
{
	return static_cast<int32>(Rarity) + 1;
}

FColor DTOfferStyle::RarityToken(EDTRarity Rarity)
{
	switch (Rarity)
	{
	case EDTRarity::Epic: return DTPal::Rarity_Epic;
	case EDTRarity::Rare: return DTPal::Rarity_Rare;
	default: return DTPal::Rarity_Common;
	}
}

const TCHAR* DTOfferStyle::RarityLabelKey(EDTRarity Rarity)
{
	switch (Rarity)
	{
	case EDTRarity::Epic: return TEXT("Offer.Rarity.Epic");
	case EDTRarity::Rare: return TEXT("Offer.Rarity.Rare");
	default: return TEXT("Offer.Rarity.Common");
	}
}

const TCHAR* DTOfferStyle::HandTitleKey(EDTHandKind Kind)
{
	switch (Kind)
	{
	case EDTHandKind::Chest: return TEXT("Offer.Hand.Chest");
	case EDTHandKind::Keeper: return TEXT("Offer.Hand.Keeper");
	case EDTHandKind::Reforging: return TEXT("Offer.Hand.Reforging");
	case EDTHandKind::Patron: return TEXT("Offer.Hand.Patron");
	default: return TEXT("Offer.Hand.Level");
	}
}

// ---- text -------------------------------------------------------------------------------------------------------------

static bool DTOfferTextKey(const FDTOfferCard& Card, const TCHAR* Suffix, FString& OutTable, FString& OutKey)
{
	using namespace DTOfferStyleLocal;
	const bool bName = FCString::Strcmp(Suffix, TEXT("Name")) == 0;
	switch (Card.Kind)
	{
	case EDTCardKind::Weapon:
	case EDTCardKind::WeaponUpgrade:
	{
		const int32 Weapon = DTWeapons::FindWeapon(Card.ContentId);
		if (Weapon == INDEX_NONE)
		{
			return false;
		}
		OutTable = WeaponsTable;
		OutKey = DTWeapons::ShortName(DTCat::Weapons[Weapon].Id) + TEXT(".") + Suffix;
		return true;
	}
	case EDTCardKind::Mod:
	{
		const int32 Mod = DTWeapons::FindModifier(Card.ContentId);
		if (Mod == INDEX_NONE)
		{
			return false;
		}
		OutTable = ModifiersTable;
		OutKey = DTWeapons::ShortName(DTCat::Modifiers[Mod].Id) + TEXT(".") + Suffix;
		return true;
	}
	case EDTCardKind::Blessing:
	{
		const FDTBlessingInfo Info = DTOffers::FindBlessing(Card.ContentId);
		if (!Info.IsValid())
		{
			return false;
		}
		OutTable = KeepersTable;
		OutKey = FString::Printf(TEXT("%s.%s.%s"), *ArchetypeKey(Info.Archetype), ColumnNames[Info.Column], Suffix);
		return true;
	}
	case EDTCardKind::Keeper:
	{
		const EDTPath Path = DTOffers::ParseKeeper(Card.ContentId);
		if (Path == EDTPath::None)
		{
			return false;
		}
		OutTable = KeepersTable;
		OutKey = FString::Printf(TEXT("%s.%s"), DTOffers::PathName(Path), Suffix);
		return true;
	}
	case EDTCardKind::Reserve:
	{
		const EDTReserveKind Reserve = DTOffers::ParseReserve(Card.ContentId);
		if (Reserve == EDTReserveKind::Count)
		{
			return false;
		}
		OutTable = UiTable;
		OutKey = FString::Printf(TEXT("Offer.Reserve.%s.%s"), ReserveNames[static_cast<int32>(Reserve)], bName ? TEXT("Name") : TEXT("Rules"));
		return true;
	}
	case EDTCardKind::Reforging:
		OutTable = ReforgingTable;
		OutKey = FString::Printf(TEXT("%s.%s"), *Card.ContentId.ToString(), Suffix);
		return true;
	default:
		return false;
	}
}

bool DTOfferText::NameKey(const FDTOfferCard& Card, FString& OutTable, FString& OutKey)
{
	return DTOfferTextKey(Card, TEXT("Name"), OutTable, OutKey);
}

bool DTOfferText::DescKey(const FDTOfferCard& Card, FString& OutTable, FString& OutKey)
{
	return DTOfferTextKey(Card, TEXT("Desc"), OutTable, OutKey);
}

FText DTOfferText::Ui(const TCHAR* Key)
{
	return FText::FromStringTable(FName(DTOfferStyleLocal::UiTable), Key);
}

FText DTOfferText::Name(const FDTOfferCard& Card)
{
	FString Table;
	FString Key;
	if (!NameKey(Card, Table, Key))
	{
		return FText::AsCultureInvariant(Card.ContentId.ToString());
	}
	return DTOfferStyleLocal::FromTable(Table, Key);
}

FText DTOfferText::Desc(const FDTOfferCard& Card)
{
	using namespace DTOfferStyleLocal;
	FString Table;
	FString Key;
	if (!DescKey(Card, Table, Key))
	{
		return FText::GetEmpty();
	}
	const FText Pattern = FromTable(Table, Key);
	FFormatNamedArguments Args;
	if (Card.Kind == EDTCardKind::Blessing)
	{
		// Every numeric parameter of the effect spec: raw, as a percentage and, for uu distances, in metres.
		const FDTBlessingInfo Info = DTOffers::FindBlessing(Card.ContentId);
		for (const TPair<FString, double>& Param : DTOffers::SpecParams(DTCat::Archetypes[Info.Archetype].Effects[Info.Column]))
		{
			Args.Add(Param.Key, Number(Param.Value));
			Args.Add(Param.Key + TEXT("_pct"), Number(FMath::RoundToDouble(Param.Value * Percent)));
			Args.Add(Param.Key + TEXT("_m"), Number(Param.Value * MetresPerUu));
		}
	}
	else if (Card.Kind == EDTCardKind::Reserve)
	{
		switch (DTOffers::ParseReserve(Card.ContentId))
		{
		case EDTReserveKind::Restore: Args.Add(TEXT("Percent"), Number(FMath::RoundToDouble(DTConst::OFFER_RESERVE_RESTORE_HP_RATIO * Percent))); break;
		case EDTReserveKind::Hone: Args.Add(TEXT("Percent"), Number(FMath::RoundToDouble(DTConst::OFFER_RESERVE_HONE_DAMAGE_RATIO * Percent))); break;
		case EDTReserveKind::SpareTerms: Args.Add(TEXT("Count"), Number(DTConst::OFFER_RESERVE_SPARE_REROLLS)); break;
		case EDTReserveKind::Salvage: Args.Add(TEXT("Percent"), Number(FMath::RoundToDouble(DTConst::OFFER_RESERVE_SALVAGE_RADIUS_RATIO * Percent))); break;
		case EDTReserveKind::Mend: Args.Add(TEXT("Hp"), Number(DTConst::OFFER_RESERVE_MEND_MAX_HP)); break;
		default: break;
		}
	}
	return Args.Num() > 0 ? FText::Format(Pattern, Args) : Pattern;
}

FText DTOfferText::Line(const FDTOfferCard& Card, const FDTPlayerRunState& Player)
{
	using namespace DTOfferStyleLocal;
	switch (Card.Kind)
	{
	case EDTCardKind::Weapon:
		return Ui(TEXT("Offer.Line.NewWeapon"));
	case EDTCardKind::WeaponUpgrade:
	{
		const FDTWeaponInstance* Owned = Player.Weapons.FindByPredicate([&Card](const FDTWeaponInstance& W) { return W.Id == Card.TargetWeapon; });
		const int32 From = Owned ? Owned->Level : 1;
		const int32 Max = Owned ? DTWeapons::GetMaxLevel(Owned->Def) : From;
		FFormatNamedArguments Args;
		Args.Add(TEXT("From"), FText::AsNumber(From));
		Args.Add(TEXT("To"), FText::AsNumber(FMath::Min(From + DTOffers::UpgradeLevels(Card.Rarity), Max)));
		return FText::Format(Ui(TEXT("Offer.Line.Upgrade")), Args);
	}
	case EDTCardKind::Mod:
	{
		FDTOfferCard Target;
		Target.Kind = EDTCardKind::Weapon;
		Target.ContentId = Card.TargetWeapon;
		FFormatNamedArguments Args;
		Args.Add(TEXT("Weapon"), Name(Target));
		return FText::Format(Ui(TEXT("Offer.Line.Target")), Args);
	}
	case EDTCardKind::Blessing:
	{
		const FDTBlessingInfo Info = DTOffers::FindBlessing(Card.ContentId);
		if (!Info.IsValid())
		{
			return FText::GetEmpty();
		}
		FDTOfferCard KeeperCard;
		KeeperCard.Kind = EDTCardKind::Keeper;
		KeeperCard.ContentId = DTOffers::KeeperId(Info.Path);
		FFormatNamedArguments Args;
		Args.Add(TEXT("Keeper"), Name(KeeperCard));
		Args.Add(TEXT("Archetype"), FromTable(KeepersTable, ArchetypeKey(Info.Archetype) + TEXT(".Name")));
		return FText::Format(Ui(TEXT("Offer.Line.Archetype")), Args);
	}
	case EDTCardKind::Reforging:
	{
		FFormatNamedArguments Args;
		if (Card.TargetWeapon.IsNone())
		{
			const int32 Row = DTReforge::FindRow(Card.ContentId);
			FDTOfferCard KeeperCard;
			KeeperCard.Kind = EDTCardKind::Keeper;
			KeeperCard.ContentId = Row != INDEX_NONE ? FName(DTCat::Reforges[Row].Source) : NAME_None;
			Args.Add(TEXT("Keeper"), Name(KeeperCard));
			return FText::Format(Ui(TEXT("Offer.Line.PathReforge")), Args);
		}
		FDTOfferCard Target;
		Target.Kind = EDTCardKind::Weapon;
		Target.ContentId = Card.TargetWeapon;
		Args.Add(TEXT("Weapon"), Name(Target));
		return FText::Format(Ui(TEXT("Offer.Line.Evolves")), Args);
	}
	default:
		return FText::GetEmpty();
	}
}

TArray<FString> DTOfferText::Keywords(const FDTOfferCard& Card)
{
	static const FName KwColumn(TEXT("Kw"));
	TArray<FString> Out;
	FString Table;
	FString Key;
	if (!DescKey(Card, Table, Key))
	{
		return Out;
	}
	const FStringTableConstPtr Source = FStringTableRegistry::Get().FindStringTable(FName(*Table));
	if (Source.IsValid())
	{
		Source->GetMetaData(FTextKey(Key), KwColumn).ParseIntoArray(Out, TEXT(";"));
	}
	if (Out.Num() > DTConst::OFFER_KEYWORD_NOTES_MAX)
	{
		Out.SetNum(DTConst::OFFER_KEYWORD_NOTES_MAX);
	}
	return Out;
}

bool DTOfferText::IsNoLineStart(TCHAR Ch)
{
	// CJK comma and stop, fullwidth stops, marks, closers and tilde, ideographic closers, ellipsis, dashes, middle dots,
	// closing curly quotes; then the ASCII forms.
	static const TCHAR Closing[] = TEXT("、。，．；：！？）］｝％～")
		TEXT("】》〉」』〕〗…—–·・’”,.;:!?)]}%");
	return Ch != 0 && FCString::Strchr(Closing, Ch) != nullptr;
}

bool DTOfferText::IsNoLineEnd(TCHAR Ch)
{
	// Fullwidth and ideographic openers, opening curly quotes; then the ASCII forms.
	static const TCHAR Opening[] = TEXT("（［｛【《〈「『〔〖‘“([{");
	return Ch != 0 && FCString::Strchr(Opening, Ch) != nullptr;
}

TArray<FString> DTOfferText::WrapLines(const FString& Text, TFunctionRef<float(const FString&)> Measure, float MaxWidth)
{
	constexpr TCHAR FirstWide = 0x2E80; // the CJK blocks start here; each wide character is a break opportunity
	const auto CanBreakBefore = [](const FString& Line, int32 Index)
	{
		const TCHAR Prev = Line[Index - 1];
		const TCHAR Next = Line[Index];
		// Spaces at a break are dropped, so what must not start a line is the first character after them.
		int32 First = Index;
		while (First < Line.Len() && Line[First] == TEXT(' '))
		{
			++First;
		}
		if (First == Line.Len() || IsNoLineStart(Line[First]) || IsNoLineEnd(Prev))
		{
			return false;
		}
		return Prev == TEXT(' ') || Next == TEXT(' ') || Prev >= FirstWide || Next >= FirstWide;
	};

	TArray<FString> Lines;
	const auto AddLine = [&Lines](const FString& Line)
	{
		const FString Trimmed = Line.TrimEnd();
		if (!Trimmed.IsEmpty())
		{
			Lines.Add(Trimmed);
		}
	};
	TArray<FString> Paragraphs;
	Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
	for (const FString& Paragraph : Paragraphs)
	{
		const int32 Len = Paragraph.Len();
		int32 Start = 0;
		while (Start < Len)
		{
			// The longest run from Start that fits; a line always takes at least one character.
			int32 End = Start + 1;
			while (End < Len && Measure(Paragraph.Mid(Start, End + 1 - Start)) <= MaxWidth)
			{
				++End;
			}
			if (End >= Len)
			{
				AddLine(Paragraph.Mid(Start));
				break;
			}
			int32 Break = End;
			while (Break > Start && !CanBreakBefore(Paragraph, Break))
			{
				--Break;
			}
			if (Break == Start)
			{
				// No opportunity in the run: a hard break, still clear of a closing mark when it can be.
				Break = End;
				while (Break > Start + 1 && IsNoLineStart(Paragraph[Break]))
				{
					--Break;
				}
			}
			AddLine(Paragraph.Mid(Start, Break - Start));
			Start = Break;
			while (Start < Len && Paragraph[Start] == TEXT(' '))
			{
				++Start;
			}
		}
	}
	return Lines;
}

FText DTOfferText::KeywordName(const FString& Keyword)
{
	return DTOfferStyleLocal::FromTable(DTOfferStyleLocal::KeywordsTable, Keyword + TEXT(".Name"));
}

FText DTOfferText::KeywordDesc(const FString& Keyword)
{
	return DTOfferStyleLocal::FromTable(DTOfferStyleLocal::KeywordsTable, Keyword + TEXT(".Desc"));
}

TArray<TPair<FString, FString>> DTOfferText::AllKeys()
{
	using namespace DTOfferStyleLocal;
	TArray<TPair<FString, FString>> Keys;
	auto AddCard = [&Keys](const FDTOfferCard& Card)
	{
		FString Table;
		FString Key;
		if (NameKey(Card, Table, Key))
		{
			Keys.AddUnique(TPair<FString, FString>(Table, Key));
		}
		if (DescKey(Card, Table, Key))
		{
			Keys.AddUnique(TPair<FString, FString>(Table, Key));
		}
		for (const FString& Keyword : Keywords(Card))
		{
			Keys.AddUnique(TPair<FString, FString>(KeywordsTable, Keyword + TEXT(".Name")));
			Keys.AddUnique(TPair<FString, FString>(KeywordsTable, Keyword + TEXT(".Desc")));
		}
	};
	FDTOfferCard Card;
	for (int32 Weapon = 0; Weapon < DTCat::NumWeapons; ++Weapon)
	{
		if (DTWeapons::GetMaxLevel(Weapon) > 0)
		{
			Card.Kind = EDTCardKind::Weapon;
			Card.ContentId = DTWeapons::WeaponId(Weapon);
			AddCard(Card);
		}
	}
	for (int32 Mod = 0; Mod < DTCat::NumModifiers; ++Mod)
	{
		Card.Kind = EDTCardKind::Mod;
		Card.ContentId = DTWeapons::ModifierId(Mod);
		AddCard(Card);
	}
	for (int32 Row = 0; Row < DTCat::NumAllArchetypes; ++Row)
	{
		Keys.AddUnique(TPair<FString, FString>(KeepersTable, ArchetypeKey(Row) + TEXT(".Name")));
		for (int32 Column = 0; Column < UE_ARRAY_COUNT(ColumnNames); ++Column)
		{
			Card.Kind = EDTCardKind::Blessing;
			Card.ContentId = FName(DTCat::Archetypes[Row].BlessingIds[Column]);
			AddCard(Card);
		}
	}
	for (EDTPath Path : DTOffers::AllKeepers())
	{
		Card.Kind = EDTCardKind::Keeper;
		Card.ContentId = DTOffers::KeeperId(Path);
		AddCard(Card);
	}
	for (int32 Reserve = 0; Reserve < static_cast<int32>(EDTReserveKind::Count); ++Reserve)
	{
		AddCard(DTOffers::MakeReserve(static_cast<EDTReserveKind>(Reserve)));
	}
	for (int32 Kind = 0; Kind < DTOfferStyle::NumCardKinds; ++Kind)
	{
		Keys.AddUnique(TPair<FString, FString>(UiTable, DTOfferStyle::ForKind(static_cast<EDTCardKind>(Kind)).LabelKey));
	}
	for (int32 Rarity = 0; Rarity < DTOfferStyle::NumRarities; ++Rarity)
	{
		Keys.AddUnique(TPair<FString, FString>(UiTable, DTOfferStyle::RarityLabelKey(static_cast<EDTRarity>(Rarity))));
	}
	for (int32 Kind = 0; Kind < DTOfferStyle::NumHandKinds; ++Kind)
	{
		Keys.AddUnique(TPair<FString, FString>(UiTable, DTOfferStyle::HandTitleKey(static_cast<EDTHandKind>(Kind))));
	}
	for (const TCHAR* Key : { TEXT("Offer.Line.Evolves"), TEXT("Offer.Line.PathReforge"), TEXT("Offer.Line.NewWeapon"), TEXT("Offer.Line.Upgrade"), TEXT("Offer.Line.Target"), TEXT("Offer.Line.Archetype"),
		TEXT("Offer.Tool.Reroll"), TEXT("Offer.Tool.Banish"), TEXT("Offer.Tool.Lock"), TEXT("Offer.Tool.Button"),
		TEXT("Offer.Marker.Guaranteed"), TEXT("Offer.Marker.Held"), TEXT("Offer.Hint.Pick"), TEXT("Offer.Hint.Guard"), TEXT("Offer.Hint.Focus") })
	{
		Keys.AddUnique(TPair<FString, FString>(UiTable, Key));
	}
	return Keys;
}
