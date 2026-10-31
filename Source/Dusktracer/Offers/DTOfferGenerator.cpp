#include "Offers/DTOfferGenerator.h"
#include "Combat/DTReforge.h"
#include "Keepers/DTReactions.h"
#include "Combat/DTWeaponSim.h"
#include "Core/DTConstantsGen.h"

namespace DTOfferGenLocal
{
	constexpr int32 NumColumns = 4; // Key, Amp1, Amp2, Trait (keepers.md Data)
	const TCHAR* const ReserveNames[] = { TEXT("Reserve.Restore"), TEXT("Reserve.Hone"), TEXT("Reserve.SpareTerms"), TEXT("Reserve.Salvage"), TEXT("Reserve.Mend") };
	const TCHAR* const PathNames[] = { TEXT("Storm"), TEXT("Forge"), TEXT("Tide"), TEXT("Shade"), TEXT("Radiance"), TEXT("Quicksilver") };
	static_assert(UE_ARRAY_COUNT(ReserveNames) == static_cast<int32>(EDTReserveKind::Count), "one id per Reserve kind");
	static_assert(UE_ARRAY_COUNT(PathNames) == static_cast<int32>(EDTPath::Count), "one name per Path");

	int32 PathIndex(EDTPath Path) { return static_cast<int32>(Path); }
	bool ValidPath(EDTPath Path) { return Path != EDTPath::None && Path < EDTPath::Count; }

	/** Paths of every archetype the player owns a card of (fit and amplification, R-OFF-02). */
	uint32 OwnedPathMask(const FDTKeeperRunState& Keepers)
	{
		uint32 Mask = 0;
		for (const FName& Id : Keepers.Owned)
		{
			const FDTBlessingInfo Info = DTOffers::FindBlessing(Id);
			if (Info.IsValid())
			{
				Mask |= DTCat::PathBit(Info.Path);
			}
		}
		return Mask;
	}

	bool OwnsWeapon(const FDTPlayerRunState& Player, FName Id, int32* OutSlot = nullptr)
	{
		for (int32 Slot = 0; Slot < Player.Weapons.Num(); ++Slot)
		{
			if (Player.Weapons[Slot].Id == Id)
			{
				if (OutSlot)
				{
					*OutSlot = Slot;
				}
				return true;
			}
		}
		return false;
	}

	int32 WeightedPick(const TArray<FDTOfferCard>& Cards, FRandomStream& Rng)
	{
		if (Cards.Num() == 0)
		{
			return INDEX_NONE;
		}
		double Total = 0.0;
		for (const FDTOfferCard& Card : Cards)
		{
			Total += FMath::Max(0.0f, Card.Weight);
		}
		const double Roll = static_cast<double>(Rng.FRand()) * Total;
		if (Total <= 0.0)
		{
			return 0;
		}
		double Acc = 0.0;
		for (int32 Index = 0; Index < Cards.Num(); ++Index)
		{
			Acc += FMath::Max(0.0f, Cards[Index].Weight);
			if (Roll < Acc)
			{
				return Index;
			}
		}
		return Cards.Num() - 1;
	}

	/** Draws one card from the subset of Pool matching Filter (weighted), removes it from Pool and returns it. */
	template <typename FilterT>
	bool DrawFrom(TArray<FDTOfferCard>& Pool, FRandomStream& Rng, FilterT&& Filter, FDTOfferCard& Out)
	{
		TArray<FDTOfferCard> Subset;
		TArray<int32> Map;
		for (int32 Index = 0; Index < Pool.Num(); ++Index)
		{
			if (Filter(Pool[Index]))
			{
				Subset.Add(Pool[Index]);
				Map.Add(Index);
			}
		}
		const int32 Picked = WeightedPick(Subset, Rng);
		if (Picked == INDEX_NONE)
		{
			return false;
		}
		Out = Pool[Map[Picked]];
		Pool.RemoveAt(Map[Picked]);
		return true;
	}

	void RemoveIdentity(TArray<FDTOfferCard>& Pool, const FDTOfferCard& Card)
	{
		Pool.RemoveAll([&Card](const FDTOfferCard& Other) { return DTOffers::SameIdentity(Other, Card); });
	}

	bool InHand(const TArray<FDTOfferCard>& Cards, const FDTOfferCard& Card)
	{
		return Cards.ContainsByPredicate([&Card](const FDTOfferCard& Other) { return DTOffers::SameIdentity(Other, Card); });
	}

	/** Owned-archetype blessing (R-OFF-06): its archetype holds at least one owned card. */
	bool IsOwnedArchetypeBlessing(const FDTPlayerRunState& Player, const FDTOfferCard& Card)
	{
		if (Card.Kind != EDTCardKind::Blessing)
		{
			return false;
		}
		const FDTBlessingInfo Info = DTOffers::FindBlessing(Card.ContentId);
		return Info.IsValid() && DTOffers::OwnedInArchetype(Player.Keepers, Info.Archetype) > 0;
	}

	/** Kind rules of R-OFF-05 / R-OFF-09 / R-OFF-14 on top of the roll. */
	/** A duo card is at least Rare (reactions.md R-REA-08). */
	EDTRarity DuoRarity(const FDTOfferCard& Card, EDTRarity Rarity)
	{
		return Rarity == EDTRarity::Common && DTReactions::FindDuo(Card.ContentId) != INDEX_NONE ? EDTRarity::Rare : Rarity;
	}

	EDTRarity FinalRarity(EDTCardKind Kind, EDTHandKind HandKind, EDTRarity Rolled)
	{
		EDTRarity Rarity = Kind == EDTCardKind::WeaponUpgrade ? Rolled : EDTRarity::Common;
		if (Kind == EDTCardKind::Reserve || Kind == EDTCardKind::Keeper)
		{
			return EDTRarity::Common;
		}
		if (HandKind == EDTHandKind::Chest && Rarity == EDTRarity::Common)
		{
			Rarity = EDTRarity::Rare;
		}
		return Rarity;
	}

	void FillReserve(TArray<FDTOfferCard>& Cards, int32 Size)
	{
		for (int32 Kind = 0; Kind < static_cast<int32>(EDTReserveKind::Count) && Cards.Num() < Size; ++Kind)
		{
			const FDTOfferCard Reserve = DTOffers::MakeReserve(static_cast<EDTReserveKind>(Kind));
			if (!InHand(Cards, Reserve))
			{
				Cards.Add(Reserve);
			}
		}
	}
}

// ---- ids ------------------------------------------------------------------------------------------------------------

FName DTOffers::ReserveId(EDTReserveKind Kind)
{
	return Kind < EDTReserveKind::Count ? FName(DTOfferGenLocal::ReserveNames[static_cast<int32>(Kind)]) : NAME_None;
}

EDTReserveKind DTOffers::ParseReserve(FName Id)
{
	for (int32 Kind = 0; Kind < static_cast<int32>(EDTReserveKind::Count); ++Kind)
	{
		if (Id == FName(DTOfferGenLocal::ReserveNames[Kind]))
		{
			return static_cast<EDTReserveKind>(Kind);
		}
	}
	return EDTReserveKind::Count;
}

const TCHAR* DTOffers::PathName(EDTPath Path)
{
	return DTOfferGenLocal::ValidPath(Path) ? DTOfferGenLocal::PathNames[static_cast<int32>(Path)] : TEXT("None");
}

FName DTOffers::KeeperId(EDTPath Path)
{
	return DTOfferGenLocal::ValidPath(Path) ? FName(*FString::Printf(TEXT("Keeper.%s"), PathName(Path))) : NAME_None;
}

EDTPath DTOffers::ParseKeeper(FName Id)
{
	for (int32 Path = 0; Path < static_cast<int32>(EDTPath::Count); ++Path)
	{
		if (Id == KeeperId(static_cast<EDTPath>(Path)))
		{
			return static_cast<EDTPath>(Path);
		}
	}
	return EDTPath::None;
}

// ---- catalog reads --------------------------------------------------------------------------------------------------

namespace DTOfferGenLocal
{
	static TArray<EDTPath> PathsOf(int32 MaxStage)
	{
		uint32 Mask = 0;
		for (const DTCat::FArchetypeRow& Row : DTCat::Archetypes)
		{
			Mask |= Row.Stage <= MaxStage ? DTCat::PathBit(Row.Path) : 0u;
		}
		TArray<EDTPath> Out;
		for (int32 Path = 0; Path < static_cast<int32>(EDTPath::Count); ++Path)
		{
			if (Mask & DTCat::PathBit(static_cast<EDTPath>(Path)))
			{
				Out.Add(static_cast<EDTPath>(Path));
			}
		}
		return Out;
	}
}

TArray<EDTPath> DTOffers::RunKeepers()
{
	return DTOfferGenLocal::PathsOf(1);
}

TArray<EDTPath> DTOffers::AllKeepers()
{
	return DTOfferGenLocal::PathsOf(TNumericLimits<int32>::Max());
}

TArray<EDTPath> DTOffers::RunKeepers(const FDTOfferPool& Pool)
{
	// R-KEE-23: an unlocked Keeper always has a seat, so it leads, in Path order; the S1 Keepers follow.
	TArray<EDTPath> Out;
	for (const EDTPath Path : AllKeepers())
	{
		if (Pool.Unlocked.Contains(KeeperId(Path)))
		{
			Out.Add(Path);
		}
	}
	for (const EDTPath Path : RunKeepers())
	{
		Out.AddUnique(Path);
	}
	return Out;
}

FDTBlessingInfo DTOffers::FindBlessing(FName Id)
{
	FDTBlessingInfo Info;
	if (Id.IsNone())
	{
		return Info;
	}
	for (int32 Row = 0; Row < DTCat::NumAllArchetypes; ++Row)
	{
		for (int32 Column = 0; Column < DTOfferGenLocal::NumColumns; ++Column)
		{
			if (Id == FName(DTCat::Archetypes[Row].BlessingIds[Column]))
			{
				Info.Archetype = Row;
				Info.Column = Column;
				Info.Path = DTCat::Archetypes[Row].Path;
				Info.Flags = SpecFlags(DTCat::Archetypes[Row].Effects[Column]);
				return Info;
			}
		}
	}
	return Info;
}

uint32 DTOffers::SpecFlags(const TCHAR* Spec)
{
	uint32 Mask = 0;
	const FString Text(Spec);
	for (int32 Flag = 0; Flag < DTCat::NumFlags; ++Flag)
	{
		// A token is the flag id followed by its parameter block: "Flag.Split{" never matches "Flag.SplitX{".
		if (Text.Contains(FString(DTCat::FlagIds[Flag]) + TEXT("{"), ESearchCase::CaseSensitive))
		{
			Mask |= DTCat::FlagBit(static_cast<DTCat::EFlag>(Flag));
		}
	}
	return Mask;
}

TArray<TPair<FString, double>> DTOffers::SpecParams(const TCHAR* Spec)
{
	TArray<TPair<FString, double>> Out;
	const FString Text(Spec);
	int32 Open = INDEX_NONE;
	int32 From = 0;
	while ((Open = Text.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, From)) != INDEX_NONE)
	{
		const int32 Close = Text.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
		if (Close == INDEX_NONE)
		{
			break;
		}
		TArray<FString> Pairs;
		Text.Mid(Open + 1, Close - Open - 1).ParseIntoArray(Pairs, TEXT(";"));
		for (const FString& Pair : Pairs)
		{
			FString Key;
			FString Value;
			if (!Pair.Split(TEXT("="), &Key, &Value))
			{
				continue;
			}
			Value.TrimStartAndEndInline();
			Value.RemoveFromStart(TEXT("+"));
			if (!Value.IsNumeric())
			{
				continue;
			}
			Key.TrimStartAndEndInline();
			const double Number = FCString::Atod(*Value);
			TPair<FString, double>* Existing = Out.FindByPredicate([&Key](const TPair<FString, double>& P) { return P.Key == Key; });
			if (Existing)
			{
				Existing->Value = Number;
			}
			else
			{
				Out.Emplace(Key, Number);
			}
		}
		From = Close + 1;
	}
	return Out;
}

bool DTOffers::IsWeaponInPool(int32 Weapon, const FDTOfferPool& Pool)
{
	if (Weapon < 0 || Weapon >= DTCat::NumWeapons)
	{
		return false;
	}
	if (Pool.Unlocked.Contains(DTWeapons::WeaponId(Weapon)))
	{
		return true;
	}
	if (!DTCat::Weapons[Weapon].bStarter)
	{
		return false;
	}
	int32 Rank = 0;
	for (int32 Row = 0; Row < Weapon; ++Row)
	{
		Rank += DTCat::Weapons[Row].bStarter ? 1 : 0;
	}
	return Rank < DTConst::POOL_STARTER_WEAPONS;
}

bool DTOffers::IsModInPool(int32 Mod, const FDTOfferPool& Pool)
{
	if (Mod < 0 || Mod >= DTCat::NumModifiers)
	{
		return false;
	}
	if (Pool.Unlocked.Contains(DTWeapons::ModifierId(Mod)))
	{
		return true;
	}
	if (!DTCat::Modifiers[Mod].bStarter)
	{
		return false;
	}
	int32 Rank = 0;
	for (int32 Row = 0; Row < Mod; ++Row)
	{
		Rank += DTCat::Modifiers[Row].bStarter ? 1 : 0;
	}
	return Rank < DTConst::POOL_STARTER_MODIFIERS;
}

bool DTOffers::IsStarterKey(int32 Archetype)
{
	return Archetype >= 0 && Archetype < DTCat::NumAllArchetypes && Archetype < DTConst::POOL_STARTER_BLESSINGS;
}

// ---- Keeper state -----------------------------------------------------------------------------------------------------

int32 DTOffers::OwnedInArchetype(const FDTKeeperRunState& Keepers, int32 Archetype)
{
	if (Archetype < 0 || Archetype >= DTCat::NumAllArchetypes)
	{
		return 0;
	}
	int32 Count = 0;
	for (int32 Column = 0; Column < DTOfferGenLocal::NumColumns; ++Column)
	{
		Count += Keepers.Owned.Contains(FName(DTCat::Archetypes[Archetype].BlessingIds[Column])) ? 1 : 0;
	}
	return Count;
}

int32 DTOffers::LeadingArchetype(const FDTKeeperRunState& Keepers, EDTPath Path)
{
	int32 Best = INDEX_NONE;
	int32 BestCount = 0;
	int32 BestLatest = INDEX_NONE;
	for (int32 Row = 0; Row < DTCat::NumAllArchetypes; ++Row)
	{
		const DTCat::FArchetypeRow& Archetype = DTCat::Archetypes[Row];
		if (Archetype.Path != Path || !Keepers.Owned.Contains(FName(Archetype.BlessingIds[0])))
		{
			continue;
		}
		const int32 Count = OwnedInArchetype(Keepers, Row);
		int32 Latest = INDEX_NONE;
		for (int32 Column = 0; Column < DTOfferGenLocal::NumColumns; ++Column)
		{
			Latest = FMath::Max(Latest, Keepers.Owned.Find(FName(Archetype.BlessingIds[Column])));
		}
		if (Count > BestCount || (Count == BestCount && Latest > BestLatest))
		{
			Best = Row;
			BestCount = Count;
			BestLatest = Latest;
		}
	}
	return Best;
}

int32 DTOffers::TierForFavor(int32 Favor)
{
	return Favor >= DTConst::TIER_FAVOR_T3 ? 3 : (Favor >= DTConst::TIER_FAVOR_T2 ? 2 : (Favor >= DTConst::TIER_FAVOR_T1 ? 1 : 0));
}

bool DTOffers::AddBlessing(FDTKeeperRunState& Keepers, FName BlessingId)
{
	const FDTBlessingInfo Info = FindBlessing(BlessingId);
	const int32 Duo = DTReactions::FindDuo(BlessingId);
	if ((!Info.IsValid() && Duo == INDEX_NONE) || Keepers.Owned.Contains(BlessingId))
	{
		return false;
	}
	Keepers.Owned.Add(BlessingId);
	const auto Refavor = [&Keepers](EDTPath Path)
	{
		const int32 P = DTOfferGenLocal::PathIndex(Path);
		int32 Owned = 0;
		for (const FName& Id : Keepers.Owned)
		{
			Owned += FindBlessing(Id).Path == Path || DTReactions::DuoCountsFor(DTReactions::FindDuo(Id), Path) ? 1 : 0; // reactions.md R-REA-09
		}
		Keepers.Favor[P] = Owned * DTConst::TIER_FAVOR_PER_BLESSING;
		Keepers.Tier[P] = FMath::Max(Keepers.Tier[P], TierForFavor(Keepers.Favor[P]));
	};
	if (Duo != INDEX_NONE)
	{
		Refavor(DTCat::Reactions[Duo].PathA);
		Refavor(DTCat::Reactions[Duo].PathB);
		return true;
	}
	Refavor(Info.Path);
	const int32 P = DTOfferGenLocal::PathIndex(Info.Path);
	const int32 Leading = LeadingArchetype(Keepers, Info.Path);
	Keepers.Leading[P] = Leading == INDEX_NONE ? NAME_None : FName(DTCat::Archetypes[Leading].Id);
	return true;
}

// ---- legality and weight ----------------------------------------------------------------------------------------------

bool DTOffers::IsCommitted(const FDTPlayerRunState& Player)
{
	return DTOfferGenLocal::ValidPath(Player.Offer.SecondPath);
}

bool DTOffers::IdentityLess(const FDTOfferCard& A, const FDTOfferCard& B)
{
	if (A.Kind != B.Kind)
	{
		return A.Kind < B.Kind;
	}
	const int32 ByContent = A.ContentId.Compare(B.ContentId);
	if (ByContent != 0)
	{
		return ByContent < 0;
	}
	return A.TargetWeapon.Compare(B.TargetWeapon) < 0;
}

bool DTOffers::SameIdentity(const FDTOfferCard& A, const FDTOfferCard& B)
{
	return A.Kind == B.Kind && A.ContentId == B.ContentId && A.TargetWeapon == B.TargetWeapon;
}

float DTOffers::KeeperWeight(const FDTPlayerRunState& Player, EDTPath Path)
{
	const int32 Favor = DTOfferGenLocal::ValidPath(Path) ? Player.Keepers.Favor[DTOfferGenLocal::PathIndex(Path)] : 0;
	return DTConst::OFFER_WEIGHT_BASE * (1.0f + DTConst::OFFER_WEIGHT_FAVOR * static_cast<float>(Favor));
}

float DTOffers::Weight(const FDTPlayerRunState& Player, const FDTOfferCard& Card)
{
	using namespace DTOfferGenLocal;
	float W = DTConst::OFFER_WEIGHT_BASE;
	const uint32 OwnedPaths = OwnedPathMask(Player.Keepers);
	switch (Card.Kind)
	{
	case EDTCardKind::Weapon:
	{
		const int32 Weapon = DTWeapons::FindWeapon(Card.ContentId);
		if (Weapon != INDEX_NONE && (DTCat::Weapons[Weapon].Fits & OwnedPaths) != 0)
		{
			W *= DTConst::OFFER_WEIGHT_WEAPON_FIT;
		}
		break;
	}
	case EDTCardKind::WeaponUpgrade:
	case EDTCardKind::Mod:
	{
		int32 Slot = INDEX_NONE;
		if (OwnsWeapon(Player, Card.TargetWeapon, &Slot))
		{
			W *= 1.0f + DTConst::OFFER_WEIGHT_WEAPON_LEVEL * static_cast<float>(Player.Weapons[Slot].Level);
		}
		if (Card.Kind == EDTCardKind::Mod)
		{
			const int32 Mod = DTWeapons::FindModifier(Card.ContentId);
			const bool bAmplifies = Mod != INDEX_NONE && (DTCat::Modifiers[Mod].Fits & OwnedPaths) != 0;
			W *= bAmplifies ? DTConst::OFFER_WEIGHT_MOD_AMPLIFIES : DTConst::OFFER_WEIGHT_MOD_NO_AMPLIFY;
		}
		break;
	}
	case EDTCardKind::Blessing:
	{
		const FDTBlessingInfo Info = FindBlessing(Card.ContentId);
		if (!Info.IsValid())
		{
			W *= DTReactions::FindDuo(Card.ContentId) != INDEX_NONE ? DTConst::OFFER_WEIGHT_DUO : 1.0f; // reactions.md R-REA-08
			break;
		}
		W *= 1.0f + DTConst::OFFER_WEIGHT_FAVOR * static_cast<float>(Player.Keepers.Favor[PathIndex(Info.Path)]);
		if (IsCommitted(Player) && (Info.Path == Player.Offer.PatronPath || Info.Path == Player.Offer.SecondPath))
		{
			W *= DTConst::OFFER_WEIGHT_COMMITTED;
		}
		W *= 1.0f + DTConst::OFFER_WEIGHT_MOMENTUM * static_cast<float>(OwnedInArchetype(Player.Keepers, Info.Archetype));
		for (int32 Row = 0; Row < DTCat::NumAllArchetypes; ++Row)
		{
			if (Row != Info.Archetype && DTCat::Archetypes[Row].Path == Info.Path
				&& OwnedInArchetype(Player.Keepers, Row) >= DTConst::OFFER_MOMENTUM_DAMP_AFTER)
			{
				W *= DTConst::OFFER_WEIGHT_OTHER_ARCHETYPE;
				break;
			}
		}
		break;
	}
	default:
		break;
	}
	return W;
}

TArray<FDTOfferCard> DTOffers::Candidates(const FDTPlayerRunState& Player, const FDTOfferPool& Pool)
{
	using namespace DTOfferGenLocal;
	TArray<FDTOfferCard> Out;
	const TSet<FName>& Banished = Player.Offer.Banished;
	auto Add = [&Out, &Player](EDTCardKind Kind, FName Id, FName Target)
	{
		FDTOfferCard& Card = Out.AddDefaulted_GetRef();
		Card.Kind = Kind;
		Card.ContentId = Id;
		Card.TargetWeapon = Target;
		Card.Weight = Weight(Player, Card);
	};

	// (a) new weapons, (b) upgrades; a banished weapon id removes both (R-OFF-10).
	const bool bFreeSlot = Player.Weapons.Num() < DTConst::WEAPON_SLOTS;
	for (int32 Weapon = 0; Weapon < DTCat::NumWeapons; ++Weapon)
	{
		const FName Id = DTWeapons::WeaponId(Weapon);
		if (Banished.Contains(Id) || DTWeapons::GetMaxLevel(Weapon) <= 0)
		{
			continue;
		}
		int32 Slot = INDEX_NONE;
		if (OwnsWeapon(Player, Id, &Slot))
		{
			if (Player.Weapons[Slot].Level < DTWeapons::GetMaxLevel(Weapon))
			{
				Add(EDTCardKind::WeaponUpgrade, Id, Id);
			}
		}
		else if (bFreeSlot && IsWeaponInPool(Weapon, Pool))
		{
			Add(EDTCardKind::Weapon, Id, NAME_None);
		}
	}

	// (c) modifiers bound to an owned weapon with a free socket, not carried yet, compatible on its caps now.
	for (int32 Mod = 0; Mod < DTCat::NumModifiers; ++Mod)
	{
		const FName Id = DTWeapons::ModifierId(Mod);
		if (Banished.Contains(Id) || !IsModInPool(Mod, Pool))
		{
			continue;
		}
		for (int32 Slot = 0; Slot < Player.Weapons.Num(); ++Slot)
		{
			const FDTWeaponInstance& Owned = Player.Weapons[Slot];
			if (Owned.Sockets.Num() >= DTConst::MOD_SOCKETS_PER_WEAPON
				|| Owned.Sockets.ContainsByPredicate([&Id](const FDTWeaponInstance::FSocket& S) { return S.ModId == Id; }))
			{
				continue;
			}
			const FDTCapList Caps = Pool.SlotCaps.IsValidIndex(Slot) && Pool.SlotCaps[Slot].Mask != 0
				? Pool.SlotCaps[Slot] : DTReforge::DeclaredCaps(Owned);
			if (DTModCompat::IsCompatible(Caps, Mod))
			{
				Add(EDTCardKind::Mod, Id, Owned.Id);
			}
		}
	}

	// (d)-(g) blessings: Patron, and the second Keeper after the L8 hand; keys unlock the rest; IncompatFlags read.
	const EDTPath Patron = Player.Offer.PatronPath;
	const EDTPath Second = IsCommitted(Player) ? Player.Offer.SecondPath : EDTPath::None;
	uint32 Excluded = 0;
	if (ValidPath(Patron))
	{
		const int32 Leading = LeadingArchetype(Player.Keepers, Patron);
		Excluded = Leading == INDEX_NONE ? 0u : DTCat::Archetypes[Leading].IncompatFlags;
	}
	for (int32 Row = 0; Row < DTCat::NumAllArchetypes; ++Row)
	{
		const DTCat::FArchetypeRow& Archetype = DTCat::Archetypes[Row];
		const bool bPatron = ValidPath(Patron) && Archetype.Path == Patron;
		const bool bSecond = ValidPath(Second) && Archetype.Path == Second;
		if (!bPatron && !bSecond)
		{
			continue;
		}
		const bool bKeyOwned = Player.Keepers.Owned.Contains(FName(Archetype.BlessingIds[0]));
		const int32 OwnedHere = OwnedInArchetype(Player.Keepers, Row);
		for (int32 Column = 0; Column < NumColumns; ++Column)
		{
			const FName Id(Archetype.BlessingIds[Column]);
			if (Player.Keepers.Owned.Contains(Id) || Banished.Contains(Id))
			{
				continue;
			}
			if (Column == 0 && !IsStarterKey(Row))
			{
				continue;
			}
			if (Column > 0 && !bKeyOwned)
			{
				continue; // (d): never an amplifier or trait without its key
			}
			if (Column == 3 && OwnedHere < DTConst::KEEPER_TRAIT_PREREQ_CARDS)
			{
				continue; // keepers.md R-KEE-04
			}
			if (bSecond && (SpecFlags(Archetype.Effects[Column]) & Excluded) != 0)
			{
				continue; // (e)
			}
			Add(EDTCardKind::Blessing, Id, NAME_None);
		}
	}

	// (h) the committed pair's duo card (reactions.md R-REA-08).
	const int32 Duo = ValidPath(Second) && Pool.Unlocked.Contains(DTReactions::UnlockId()) ? DTReactions::FindPair(Patron, Second) : INDEX_NONE;
	if (Duo != INDEX_NONE)
	{
		const FName Id(DTCat::Reactions[Duo].BlessingId);
		if (!Player.Keepers.Owned.Contains(Id) && !Banished.Contains(Id))
		{
			Add(EDTCardKind::Blessing, Id, NAME_None);
		}
	}

	Out.Sort([](const FDTOfferCard& A, const FDTOfferCard& B) { return IdentityLess(A, B); });
	return Out;
}

bool DTOffers::IsLegal(const FDTPlayerRunState& Player, const FDTOfferPool& Pool, const FDTOfferCard& Card)
{
	return DTOfferGenLocal::InHand(Candidates(Player, Pool), Card);
}

// ---- hands ----------------------------------------------------------------------------------------------------------

EDTHandKind DTOffers::ResolveKind(const FDTPlayerRunState& Player, EDTHandKind Requested, int32 Level)
{
	if (Requested != EDTHandKind::Level)
	{
		return Requested;
	}
	if (Level == DTConst::OFFER_SECOND_KEEPER_LEVEL && DTOfferGenLocal::ValidPath(Player.Offer.PatronPath) && !IsCommitted(Player))
	{
		return EDTHandKind::Keeper;
	}
	if (Level == DTConst::OFFER_REFORGING_LEVEL)
	{
		return EDTHandKind::Reforging;
	}
	return EDTHandKind::Level;
}

int32 DTOffers::HandSize(EDTHandKind Kind)
{
	switch (Kind)
	{
	case EDTHandKind::Chest: return DTConst::OFFER_CHEST_CARDS;
	case EDTHandKind::Reforging: return DTConst::OFFER_REFORGING_CARDS;
	case EDTHandKind::Keeper:
	case EDTHandKind::Patron: return DTConst::OFFER_KEEPER_CHOICES;
	default: return DTConst::OFFER_CARDS;
	}
}

FDTOfferCard DTOffers::MakeReserve(EDTReserveKind Kind)
{
	FDTOfferCard Card;
	Card.Kind = EDTCardKind::Reserve;
	Card.ContentId = ReserveId(Kind);
	Card.Weight = 0.0f;
	return Card;
}

EDTRarity DTOffers::RollRarity(FRandomStream& Rng)
{
	const float Epic = Rng.FRand();
	const float Rare = Rng.FRand();
	if (Epic < DTConst::OFFER_RARITY_EPIC_P)
	{
		return EDTRarity::Epic;
	}
	return Rare < DTConst::OFFER_RARITY_RARE_P ? EDTRarity::Rare : EDTRarity::Common;
}

FDTOfferHand DTOffers::BuildHand(const FDTPlayerRunState& Player, const FDTOfferPool& Pool, EDTHandKind Kind, int32 Level,
	FRandomStream& Rng, TConstArrayView<FDTOfferCard> Carried, TArray<int32>* OutDropped)
{
	using namespace DTOfferGenLocal;
	FDTOfferHand Hand;
	Hand.HandKind = Kind;
	Hand.Level = Level;
	const int32 Size = HandSize(Kind);

	// --- Keeper hands (R-OFF-08): no tools, no Reserve, Common.
	if (Kind == EDTHandKind::Patron || Kind == EDTHandKind::Keeper)
	{
		TArray<FDTOfferCard> Keepers;
		for (EDTPath Path : RunKeepers(Pool))
		{
			if (Kind == EDTHandKind::Keeper && Path == Player.Offer.PatronPath)
			{
				continue;
			}
			FDTOfferCard& Card = Keepers.AddDefaulted_GetRef();
			Card.Kind = EDTCardKind::Keeper;
			Card.ContentId = KeeperId(Path);
			Card.Weight = Kind == EDTHandKind::Patron ? DTConst::OFFER_WEIGHT_BASE : KeeperWeight(Player, Path);
		}
		if (Kind == EDTHandKind::Patron)
		{
			// Path order, unweighted: no Favor exists yet.
			for (int32 Index = 0; Index < Keepers.Num() && Hand.Cards.Num() < Size; ++Index)
			{
				Hand.Cards.Add(Keepers[Index]);
			}
		}
		else
		{
			// R-KEE-23: an unlocked Keeper keeps its seat at the commitment too.
			for (int32 Index = 0; Index < Keepers.Num() && Hand.Cards.Num() < Size;)
			{
				if (Pool.Unlocked.Contains(Keepers[Index].ContentId))
				{
					Hand.Cards.Add(Keepers[Index]);
					Keepers.RemoveAt(Index);
				}
				else
				{
					++Index;
				}
			}
			FDTOfferCard Drawn;
			while (Hand.Cards.Num() < Size && DrawFrom(Keepers, Rng, [](const FDTOfferCard&) { return true; }, Drawn))
			{
				Hand.Cards.Add(Drawn);
			}
		}
		Hand.bGuaranteeFilled = false;
		return Hand;
	}

	TArray<FDTOfferCard> Pool2 = Candidates(Player, Pool);

	// --- Reforging (R-OFF-09): sources, then the highest-weight legal blessings of the chosen Keepers, Epic; then Reserve.
	if (Kind == EDTHandKind::Reforging)
	{
		// R-RFG-02: Patron, second Keeper, primary weapon; a source with no eligible card yields its slot.
		const auto AddSource = [&Hand, Size](int32 Row, FName Target)
		{
			if (Row != INDEX_NONE && Hand.Cards.Num() < Size)
			{
				FDTOfferCard& Card = Hand.Cards.AddDefaulted_GetRef();
				Card.Kind = EDTCardKind::Reforging;
				Card.ContentId = DTReforge::RowId(Row);
				Card.TargetWeapon = Target;
				Card.Rarity = EDTRarity::Epic;
			}
		};
		for (const EDTPath Path : { Player.Offer.PatronPath, Player.Offer.SecondPath })
		{
			const int32 Row = DTReforge::FindPath(Path);
			if (!Player.PathReforges.Contains(Row))
			{
				AddSource(Row, NAME_None);
			}
		}
		const int32 Primary = DTReforge::PrimarySlot(Player.Weapons);
		if (Primary != INDEX_NONE && Player.Weapons[Primary].Evolve == INDEX_NONE && Player.Weapons[Primary].Level >= DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL)
		{
			AddSource(DTReforge::FindEvolve(Player.Weapons[Primary].Def), Player.Weapons[Primary].Id);
		}
		TArray<FDTOfferCard> Blessings = Pool2.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing; });
		Blessings.StableSort([](const FDTOfferCard& A, const FDTOfferCard& B)
		{
			return A.Weight != B.Weight ? A.Weight > B.Weight : IdentityLess(A, B);
		});
		for (FDTOfferCard& Card : Blessings)
		{
			if (Hand.Cards.Num() >= Size)
			{
				break;
			}
			Card.Rarity = EDTRarity::Epic;
			Hand.Cards.Add(Card);
		}
		FillReserve(Hand.Cards, Size);
		return Hand;
	}

	// --- Level and Chest hands.
	TArray<FDTOfferCard> Kept;
	for (int32 Index = 0; Index < Carried.Num(); ++Index)
	{
		const FDTOfferCard* Fresh = Pool2.FindByPredicate([&Carried, Index](const FDTOfferCard& C) { return SameIdentity(C, Carried[Index]); });
		if (!Fresh || InHand(Kept, *Fresh))
		{
			if (OutDropped)
			{
				OutDropped->Add(Index);
			}
			continue;
		}
		FDTOfferCard Card = *Fresh;
		Card.bLocked = true;
		Card.Rarity = Carried[Index].Rarity;
		Kept.Add(Card);
		RemoveIdentity(Pool2, Card);
	}

	auto IsGuaranteedKind = [](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing || C.Kind == EDTCardKind::WeaponUpgrade; };
	bool bNeedRarity[16] = {};
	auto Push = [&Hand, &bNeedRarity](const FDTOfferCard& Card, bool bRoll)
	{
		if (Hand.Cards.Num() < UE_ARRAY_COUNT(bNeedRarity))
		{
			bNeedRarity[Hand.Cards.Num()] = bRoll;
		}
		Hand.Cards.Add(Card);
	};

	if (Kind == EDTHandKind::Level)
	{
		// R-OFF-06: the first card comes from the guaranteed set; a carried guaranteed-kind card counts toward it.
		const int32 CarriedGuarantee = Kept.IndexOfByPredicate(IsGuaranteedKind);
		const float Roll = Rng.FRand();
		if (CarriedGuarantee != INDEX_NONE)
		{
			FDTOfferCard Card = Kept[CarriedGuarantee];
			Card.bGuaranteed = true;
			Kept.RemoveAt(CarriedGuarantee);
			Push(Card, false);
			Hand.bGuaranteeFilled = true;
		}
		else
		{
			const bool bAnyBlessing = Pool2.ContainsByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing; });
			const bool bAnyUpgrade = Pool2.ContainsByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::WeaponUpgrade; });
			FDTOfferCard Card;
			bool bDrawn = false;
			if (bAnyBlessing || bAnyUpgrade)
			{
				const bool bBlessing = bAnyBlessing && (Roll < DTConst::OFFER_GUARANTEE_BLESSING_P || !bAnyUpgrade);
				const EDTCardKind Wanted = bBlessing ? EDTCardKind::Blessing : EDTCardKind::WeaponUpgrade;
				bDrawn = DrawFrom(Pool2, Rng, [Wanted](const FDTOfferCard& C) { return C.Kind == Wanted; }, Card);
				Card.bGuaranteed = bDrawn;
				Hand.bGuaranteeFilled = bDrawn;
			}
			else
			{
				bDrawn = DrawFrom(Pool2, Rng, [](const FDTOfferCard&) { return true; }, Card);
				Hand.bGuaranteeFilled = false;
			}
			if (bDrawn)
			{
				Push(Card, true);
			}
		}
	}
	for (const FDTOfferCard& Card : Kept)
	{
		if (Hand.Cards.Num() < Size)
		{
			Push(Card, false);
		}
	}

	if (Kind == EDTHandKind::Level && IsCommitted(Player))
	{
		// R-OFF-06 / R-OFF-11 (d): the committed-blessing floor, with one owned-archetype card when legal.
		auto CountBlessings = [&Hand]() { return Hand.Cards.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing; }).Num(); };
		auto HasOwnedArchetype = [&Hand, &Player]()
		{
			return Hand.Cards.ContainsByPredicate([&Player](const FDTOfferCard& C) { return IsOwnedArchetypeBlessing(Player, C); });
		};
		while (Hand.Cards.Num() < Size && (CountBlessings() < DTConst::OFFER_MIN_COMMITTED_BLESSINGS || !HasOwnedArchetype()))
		{
			FDTOfferCard Card;
			const bool bOwnedFirst = !HasOwnedArchetype()
				&& Pool2.ContainsByPredicate([&Player](const FDTOfferCard& C) { return IsOwnedArchetypeBlessing(Player, C); });
			bool bDrawn = false;
			if (bOwnedFirst)
			{
				bDrawn = DrawFrom(Pool2, Rng, [&Player](const FDTOfferCard& C) { return IsOwnedArchetypeBlessing(Player, C); }, Card);
			}
			else if (CountBlessings() < DTConst::OFFER_MIN_COMMITTED_BLESSINGS)
			{
				bDrawn = DrawFrom(Pool2, Rng, [](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing; }, Card);
			}
			if (!bDrawn)
			{
				break;
			}
			Push(Card, true);
		}
	}

	if (Kind == EDTHandKind::Level && Level >= DTConst::OFFER_FORCE_WEAPON_LEVEL && Player.Weapons.Num() < DTConst::OFFER_FORCE_WEAPON_BELOW
		&& !Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Weapon; }) && Hand.Cards.Num() < Size)
	{
		// R-OFF-11 (b): below the weapon floor the hand holds a new weapon whenever one is legal.
		FDTOfferCard Card;
		if (DrawFrom(Pool2, Rng, [](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Weapon; }, Card))
		{
			Push(Card, true);
		}
	}

	FDTOfferCard Drawn;
	while (Hand.Cards.Num() < Size && DrawFrom(Pool2, Rng, [](const FDTOfferCard&) { return true; }, Drawn))
	{
		Push(Drawn, true);
	}

	// R-OFF-05 / R-OFF-14: rarity per drawn card from the same stream; carried cards keep theirs.
	for (int32 Slot = 0; Slot < Hand.Cards.Num(); ++Slot)
	{
		FDTOfferCard& Card = Hand.Cards[Slot];
		const bool bRoll = Slot < UE_ARRAY_COUNT(bNeedRarity) ? bNeedRarity[Slot] : true;
		const EDTRarity Rolled = bRoll ? RollRarity(Rng) : Card.Rarity;
		Card.Rarity = DuoRarity(Card, FinalRarity(Card.Kind, Kind, Rolled));
	}

	FillReserve(Hand.Cards, Size);
	return Hand;
}

void DTOffers::RefillSlot(const FDTPlayerRunState& Player, const FDTOfferPool& Pool, FDTOfferHand& Hand, int32 Slot, FRandomStream& Rng)
{
	using namespace DTOfferGenLocal;
	if (!Hand.Cards.IsValidIndex(Slot))
	{
		return;
	}
	TArray<FDTOfferCard> Pool2 = Candidates(Player, Pool);
	for (int32 Other = 0; Other < Hand.Cards.Num(); ++Other)
	{
		if (Other != Slot)
		{
			RemoveIdentity(Pool2, Hand.Cards[Other]);
		}
	}
	RemoveIdentity(Pool2, Hand.Cards[Slot]);
	FDTOfferCard Card;
	if (DrawFrom(Pool2, Rng, [](const FDTOfferCard&) { return true; }, Card))
	{
		Card.Rarity = DuoRarity(Card, FinalRarity(Card.Kind, Hand.HandKind, RollRarity(Rng)));
		Hand.Cards[Slot] = Card;
		return;
	}
	TArray<FDTOfferCard> Others = Hand.Cards;
	Others.RemoveAt(Slot);
	for (int32 Kind = 0; Kind < static_cast<int32>(EDTReserveKind::Count); ++Kind)
	{
		const FDTOfferCard Reserve = MakeReserve(static_cast<EDTReserveKind>(Kind));
		if (!InHand(Others, Reserve))
		{
			Hand.Cards[Slot] = Reserve;
			return;
		}
	}
}

// ---- tools and picks ----------------------------------------------------------------------------------------------

bool DTOffers::HandAllowsTools(EDTHandKind Kind)
{
	return Kind == EDTHandKind::Level || Kind == EDTHandKind::Chest;
}

bool DTOffers::CardAllowsBanishLock(const FDTOfferCard& Card)
{
	return Card.Kind == EDTCardKind::Weapon || Card.Kind == EDTCardKind::WeaponUpgrade || Card.Kind == EDTCardKind::Mod
		|| Card.Kind == EDTCardKind::Blessing;
}

int32 DTOffers::UpgradeLevels(EDTRarity Rarity)
{
	switch (Rarity)
	{
	case EDTRarity::Epic: return DTConst::OFFER_EPIC_UPGRADE_LEVELS;
	case EDTRarity::Rare: return DTConst::OFFER_RARE_UPGRADE_LEVELS;
	default: return 1;
	}
}

FDTPickOutcome DTOffers::ApplyPickState(FDTPlayerRunState& Player, EDTHandKind HandKind, const FDTOfferCard& Card)
{
	using namespace DTOfferGenLocal;
	FDTPickOutcome Out;
	Out.Kind = Card.Kind;
	if (Player.Offer.Locked.IsSet() && SameIdentity(Player.Offer.Locked.GetValue(), Card))
	{
		Player.Offer.Locked.Reset(); // the held card is taken: nothing left to carry
	}
	switch (Card.Kind)
	{
	case EDTCardKind::Weapon:
		Out.Weapon = Card.ContentId;
		Out.bValid = true;
		break;
	case EDTCardKind::WeaponUpgrade:
	{
		int32 Slot = INDEX_NONE;
		if (OwnsWeapon(Player, Card.TargetWeapon, &Slot))
		{
			const FDTWeaponInstance& Owned = Player.Weapons[Slot];
			Out.Weapon = Owned.Id;
			Out.TargetLevel = FMath::Min(Owned.Level + UpgradeLevels(Card.Rarity), DTWeapons::GetMaxLevel(Owned.Def));
			Out.bValid = true;
		}
		break;
	}
	case EDTCardKind::Mod:
		Out.Weapon = Card.TargetWeapon;
		Out.Mod = Card.ContentId;
		Out.bValid = true;
		break;
	case EDTCardKind::Blessing:
		Out.bValid = AddBlessing(Player.Keepers, Card.ContentId);
		Out.Blessing = Card.ContentId;
		break;
	case EDTCardKind::Keeper:
	{
		const EDTPath Path = ParseKeeper(Card.ContentId);
		if (!ValidPath(Path))
		{
			break;
		}
		Out.Keeper = Path;
		Out.bPatron = HandKind == EDTHandKind::Patron;
		if (Out.bPatron)
		{
			Player.Offer.PatronPath = Path;
			Player.Keepers.Primary = Path;
		}
		else
		{
			Player.Offer.SecondPath = Path;
			Player.Keepers.Second = Path;
			Player.Keepers.bCommitDone = true;
		}
		Out.bValid = true;
		break;
	}
	case EDTCardKind::Reserve:
		Out.Reserve = ParseReserve(Card.ContentId);
		if (Out.Reserve == EDTReserveKind::SpareTerms)
		{
			Player.Offer.RerollsLeft += DTConst::OFFER_RESERVE_SPARE_REROLLS;
		}
		Out.bValid = Out.Reserve != EDTReserveKind::Count;
		break;
	case EDTCardKind::Reforging:
	{
		const int32 Row = DTReforge::FindRow(Card.ContentId);
		Out.ReforgeRow = Row;
		if (Row == INDEX_NONE)
		{
			break;
		}
		if (DTCat::Reforges[Row].bEvolve)
		{
			int32 Slot = INDEX_NONE;
			if (OwnsWeapon(Player, Card.TargetWeapon, &Slot))
			{
				Player.Weapons[Slot].Evolve = Row;
				Out.Weapon = Card.TargetWeapon;
				Out.bValid = true;
			}
		}
		else
		{
			Player.PathReforges.AddUnique(Row);
			Out.bValid = true;
		}
		break;
	}
	}
	return Out;
}

EDTWeaponResult DTOffers::ApplyToSim(FDTWeaponSim& Weapons, FDTRunState& Run, int32 OwnerId, const FDTPickOutcome& Outcome)
{
	switch (Outcome.Kind)
	{
	case EDTCardKind::Weapon:
		return Weapons.Give(Run, OwnerId, Outcome.Weapon, 1);
	case EDTCardKind::WeaponUpgrade:
		return Weapons.LevelUp(Run, OwnerId, FDTWeaponSim::FindSlot(Run, OwnerId, Outcome.Weapon), Outcome.TargetLevel);
	case EDTCardKind::Mod:
		return Weapons.Socket(Run, OwnerId, FDTWeaponSim::FindSlot(Run, OwnerId, Outcome.Weapon), Outcome.Mod);
	default:
		return EDTWeaponResult::Ok;
	}
}
