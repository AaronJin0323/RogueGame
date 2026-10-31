#include "Meta/DTTreeTab.h"
#include "Core/DTCatalogGen.h"
#include "Hub/DTHubScreen.h"
#include "Meta/DTMetaSession.h"
#include "Meta/DTMetaSubsystem.h"
#include "Meta/DTTree.h"
#include "Offers/DTOfferGenerator.h"

namespace DTTreeTabLocal
{
	static FText Tree(const TCHAR* Key)
	{
		return FText::FromStringTable(TEXT("ST_Tree"), Key);
	}

	static FText World(const TCHAR* Key)
	{
		return FText::FromStringTable(TEXT("ST_World"), Key);
	}

	/** Tree.Requires over each node still missing. */
	static FText Requires(const FDTTreeNode& Node)
	{
		TArray<FText> Items;
		for (const FName& Need : Node.Missing)
		{
			const int32 Row = DTTree::FindNode(Need);
			if (Row != INDEX_NONE)
			{
				Items.Add(FText::Format(Tree(TEXT("Tree.RequiresItem")), DTTree::NodeName(Row), FText::AsNumber(1)));
			}
		}
		return FText::Format(Tree(TEXT("Tree.Requires")), FText::Join(FText::FromString(TEXT(" · ")), Items));
	}
}

FDTTreeTab::FDTTreeTab(FDTMetaSession& InSession)
	: Session(InSession)
{
	for (int32 Node = 0; Node < DTCat::NumTreeNodes; ++Node)
	{
		const EDTPath Path = DTCat::TreeNodes[Node].Path;
		if (Rows.IsEmpty() || Rows.Last().Path != Path)
		{
			Rows.Add({ INDEX_NONE, Path, Node });
		}
		Rows.Add({ Node, Path, Rows.Last().Branch });
	}
}

FDTTreeNode FDTTreeTab::NodeAt(int32 Row) const
{
	return DTTree::GetNode(Session.Profile, FName(DTCat::TreeNodes[Rows[Row].Node].Id));
}

bool FDTTreeTab::IsHeading(int32 Row) const
{
	return Rows.IsValidIndex(Row) && Rows[Row].Node == INDEX_NONE;
}

bool FDTTreeTab::IsSealed(int32 Row) const
{
	const FRow& Item = Rows[Row];
	return Item.Path != EDTPath::None
		&& DTTree::GetNode(Session.Profile, FName(DTCat::TreeNodes[Item.Branch].Id)).Missing.Contains(DTOffers::KeeperId(Item.Path));
}

bool FDTTreeTab::IsDimmed(int32 Row) const
{
	return Rows.IsValidIndex(Row) && !IsHeading(Row) && NodeAt(Row).State == EDTTreeState::Locked;
}

FText FDTTreeTab::RowLabel(int32 Row) const
{
	using namespace DTTreeTabLocal;
	if (!Rows.IsValidIndex(Row))
	{
		return FText::GetEmpty();
	}
	if (!IsHeading(Row))
	{
		return DTTree::NodeName(Rows[Row].Node);
	}
	return Rows[Row].Path == EDTPath::None ? Tree(TEXT("Tree.Branch.Trunk")) : FText::Format(Tree(TEXT("Tree.Branch.Keeper")), DTHubText::KeeperName(Rows[Row].Path));
}

FText FDTTreeTab::RowValue(int32 Row) const
{
	using namespace DTTreeTabLocal;
	if (!Rows.IsValidIndex(Row) || IsHeading(Row))
	{
		return FText::GetEmpty();
	}
	const FDTTreeNode Node = NodeAt(Row);
	if (Node.State == EDTTreeState::Maxed)
	{
		return Tree(TEXT("Tree.State.Maxed"));
	}
	const FText Cost = FText::Format(Tree(TEXT("Tree.Cost")), FText::AsNumber(Node.Cost));
	if (Node.State == EDTTreeState::Open)
	{
		return Cost;
	}
	const FText State = IsSealed(Row) ? World(TEXT("Hub.Shrine.Locked")) : Tree(Node.State == EDTTreeState::Locked ? TEXT("Tree.State.Locked") : TEXT("Tree.State.Short"));
	return FText::Format(Tree(TEXT("Tree.Row")), State, Cost);
}

FText FDTTreeTab::Reason(int32 Row) const
{
	using namespace DTTreeTabLocal;
	const FDTTreeNode Node = NodeAt(Row);
	if (Row == Signed)
	{
		return FText::Format(Tree(TEXT("Tree.Purchased")), DTTree::NodeName(Rows[Row].Node));
	}
	switch (DTTree::CanBuy(Session, Node.Id))
	{
	case EDTTreeBuy::NoSave: return Tree(TEXT("Tree.State.ReadOnly"));
	case EDTTreeBuy::Requires: return IsSealed(Row) ? World(TEXT("Hub.Shrine.LockedHint")) : Requires(Node);
	default: return FText::GetEmpty();
	}
}

FText FDTTreeTab::RowDetail(int32 Row) const
{
	if (!Rows.IsValidIndex(Row) || IsHeading(Row))
	{
		return FText::GetEmpty();
	}
	const FText Desc = DTTree::NodeDesc(Rows[Row].Node);
	const FText Why = Reason(Row);
	return Why.IsEmpty() ? Desc : FText::AsCultureInvariant(Desc.ToString() + TEXT("\n") + Why.ToString());
}

FText FDTTreeTab::Header() const
{
	return FText::Format(DTTreeTabLocal::Tree(TEXT("Tree.Balance")), FText::AsNumber(Session.Profile.Currency));
}

void FDTTreeTab::Accept(int32 Row)
{
	if (Rows.IsValidIndex(Row) && !IsHeading(Row) && DTTree::Buy(Session, FName(DTCat::TreeNodes[Rows[Row].Node].Id)) == EDTTreeBuy::Ok)
	{
		Signed = Row;
	}
}

void DTTreeTab::Register()
{
	DTHubTabs::SetTreeFactory([](UWorld* World) -> TSharedPtr<IDTHubTab>
	{
		FDTMetaSession* Session = UDTMetaSubsystem::FindSession(World);
		return Session ? MakeShared<FDTTreeTab>(*Session) : TSharedPtr<IDTHubTab>();
	});
}
