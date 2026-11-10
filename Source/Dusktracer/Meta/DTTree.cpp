#include "Meta/DTTree.h"
#include "Dusktracer.h"
#include "Internationalization/Text.h"
#include "Meta/DTMetaSession.h"

int32 DTTree::FindNode(FName Id)
{
	for (int32 Row = 0; Row < DTCat::NumTreeNodes; ++Row)
	{
		if (Id == FName(DTCat::TreeNodes[Row].Id))
		{
			return Row;
		}
	}
	return INDEX_NONE;
}

TArray<FName> DTTree::SplitIds(const TCHAR* Cell)
{
	TArray<FString> Parts;
	FString(Cell).ParseIntoArray(Parts, TEXT(";"));
	TArray<FName> Out;
	for (const FString& Part : Parts)
	{
		Out.Add(FName(*Part));
	}
	return Out;
}

FDTTreeNode DTTree::GetNode(const FDTMetaProfile& Profile, FName Id)
{
	FDTTreeNode Node;
	Node.Id = Id;
	const int32 Row = FindNode(Id);
	if (Row == INDEX_NONE)
	{
		return Node;
	}
	const DTCat::FTreeNodeRow& Data = DTCat::TreeNodes[Row];
	Node.Row = Row;
	Node.Path = Data.Path;
	Node.bPool = Data.Class == DTCat::ETreeClass::Pool;
	Node.Rank = Profile.TreeRanks.FindRef(Id);
	Node.MaxRank = Data.Ranks;
	Node.Cost = Data.Cost;
	for (const FName& Need : SplitIds(Data.Requires))
	{
		// A node needs one rank; anything else is an id of the Unlocked set (R-MET-21).
		const bool bMet = FindNode(Need) != INDEX_NONE ? Profile.TreeRanks.FindRef(Need) >= 1 : Profile.Unlocked.Contains(Need);
		if (!bMet)
		{
			Node.Missing.Add(Need);
		}
	}
	if (Node.Rank >= Node.MaxRank)
	{
		Node.State = EDTTreeState::Maxed;
	}
	else if (Node.Missing.Num() > 0)
	{
		Node.State = EDTTreeState::Locked;
	}
	else
	{
		Node.State = Profile.Currency < Node.Cost ? EDTTreeState::Short : EDTTreeState::Open;
	}
	return Node;
}

TArray<FDTTreeNode> DTTree::ListNodes(const FDTMetaProfile& Profile)
{
	TArray<FDTTreeNode> Out;
	Out.Reserve(DTCat::NumTreeNodes);
	for (const DTCat::FTreeNodeRow& Data : DTCat::TreeNodes)
	{
		Out.Add(GetNode(Profile, FName(Data.Id)));
	}
	return Out;
}

EDTTreeBuy DTTree::CanBuy(const FDTMetaSession& Session, FName Id)
{
	const FDTTreeNode Node = GetNode(Session.Profile, Id);
	if (Node.Row == INDEX_NONE)
	{
		return EDTTreeBuy::UnknownNode;
	}
	if (Session.Path.IsEmpty() || !Session.bWritable)
	{
		return EDTTreeBuy::NoSave;
	}
	switch (Node.State)
	{
	case EDTTreeState::Maxed: return EDTTreeBuy::Maxed;
	case EDTTreeState::Locked: return EDTTreeBuy::Requires;
	case EDTTreeState::Short: return EDTTreeBuy::Short;
	default: return EDTTreeBuy::Ok;
	}
}

EDTTreeBuy DTTree::Buy(FDTMetaSession& Session, FName Id)
{
	const EDTTreeBuy Before = CanBuy(Session, Id);
	if (Before == EDTTreeBuy::UnknownNode || Before == EDTTreeBuy::NoSave)
	{
		return Before;
	}
	// The purchase as an edit: a second window on this save may have spent or banked since, so the checks and the price are
	// taken on the file's profile, under the lock. Kept after a failed write, it is checked again before it is made again.
	const TSharedRef<EDTTreeBuy> Why = MakeShared<EDTTreeBuy>(Before);
	const FDTMetaEdit Purchase = [Id, Why](FDTMetaSession& Held)
	{
		*Why = CanBuy(Held, Id);
		if (*Why != EDTTreeBuy::Ok)
		{
			return false;
		}
		const DTCat::FTreeNodeRow& Data = DTCat::TreeNodes[FindNode(Id)];
		Held.Profile.Currency -= Data.Cost;
		Held.Profile.TreeRanks.FindOrAdd(Id) += 1;
		for (const FName& Item : SplitIds(Data.Unlocks))
		{
			Held.Profile.Unlocked.Add(Item);
		}
		return true;
	};
	switch (Session.Change(Purchase, /*bKeepIfBusy*/ false))
	{
	case EDTMetaChange::Busy:
		UE_LOG(LogDT, Warning, TEXT("Tree: %s not bought, the save is locked by another launch; nothing was spent"), *Id.ToString());
		return EDTTreeBuy::Busy;
	case EDTMetaChange::Refused:
		return *Why;
	default:
		UE_LOG(LogDT, Log, TEXT("Tree: bought %s, balance %lld"), *Id.ToString(), Session.Profile.Currency);
		return EDTTreeBuy::Ok;
	}
}

FDTPoolGrant DTTree::MakeGrant(const FDTMetaProfile& Profile, bool bGated)
{
	FDTPoolGrant Grant;
	if (!bGated)
	{
		return Grant;
	}
	Grant.bGated = true;
	Grant.Unlocked = Profile.Unlocked;
	Grant.LastPatron = Profile.LastPatron;
	for (const DTCat::FTreeNodeRow& Data : DTCat::TreeNodes)
	{
		const int32 Amount = FMath::Clamp(Profile.TreeRanks.FindRef(FName(Data.Id)), 0, Data.Ranks) * Data.GrantPerRank;
		switch (Data.Grant)
		{
		case DTCat::ETreeGrant::Reroll: Grant.Rerolls += Amount; break;
		case DTCat::ETreeGrant::Banish: Grant.Banishes += Amount; break;
		case DTCat::ETreeGrant::Lock: Grant.Locks += Amount; break;
		default: break;
		}
	}
	return Grant;
}

FText DTTree::NodeName(int32 Row)
{
	return FText::FromStringTable(TEXT("ST_Tree"), FString(DTCat::TreeNodes[Row].TextKey) + TEXT(".Name"));
}

FText DTTree::NodeDesc(int32 Row)
{
	const DTCat::FTreeNodeRow& Data = DTCat::TreeNodes[Row];
	const FText Keeper = Data.Path == EDTPath::None ? FText::GetEmpty()
		: FText::FromStringTable(TEXT("ST_Keepers"), FString(DTOffers::PathName(Data.Path)) + TEXT(".Name"));
	return FText::Format(FText::FromStringTable(TEXT("ST_Tree"), FString(Data.TextKey) + TEXT(".Desc")), Keeper, FText::AsNumber(Data.GrantPerRank));
}
