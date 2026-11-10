// The Tree (02_Systems/meta.md R-MET-20..24): node states, purchase, and what a run receives. Plain C++ on a profile and a
// session, so tests and a screen call it the same way. A screen reads UDTMetaSubsystem::FindSession, then ListNodes, CanBuy, Buy.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTCatalogGen.h"
#include "Meta/DTMetaProfile.h"
#include "Offers/DTOfferGenerator.h"

struct FDTMetaSession;

/**
 * Why a rank can or cannot be bought now. NoSave: a session with no file, or one stuck on an unreadable file, buys nothing.
 * Busy: another launch held the save's lock past the wait (R-MET-11); nothing was spent and the same ask can be made again.
 */
enum class EDTTreeBuy : uint8 { Ok, UnknownNode, NoSave, Maxed, Requires, Short, Busy };

/** What a tile shows. A missing prerequisite wins over a short balance. */
enum class EDTTreeState : uint8 { Open, Short, Locked, Maxed };

/** One node as a screen reads it. */
struct FDTTreeNode
{
	FName Id;
	/** Row in DTCat::TreeNodes. */
	int32 Row = INDEX_NONE;
	/** The branch's Path; EDTPath::None on the Trunk. */
	EDTPath Path = EDTPath::None;
	bool bPool = false;
	int32 Rank = 0;
	int32 MaxRank = 0;
	/** The next rank's price. */
	int32 Cost = 0;
	EDTTreeState State = EDTTreeState::Locked;
	/** What R-MET-21's `requires` still lacks: node ids, or Keeper.<Path> ids. */
	TArray<FName> Missing;
};

namespace DTTree
{
	/** Row in DTCat::TreeNodes, INDEX_NONE for an unknown id. */
	DUSKTRACER_API int32 FindNode(FName Id);
	/** A `;` list cell as ids. */
	DUSKTRACER_API TArray<FName> SplitIds(const TCHAR* Cell);

	/** Every node in catalog order. */
	DUSKTRACER_API TArray<FDTTreeNode> ListNodes(const FDTMetaProfile& Profile);
	/** One node; Row stays INDEX_NONE for an unknown id. */
	DUSKTRACER_API FDTTreeNode GetNode(const FDTMetaProfile& Profile, FName Id);
	DUSKTRACER_API EDTTreeBuy CanBuy(const FDTMetaSession& Session, FName Id);
	/** Spends, adds the rank, writes the unlocks and autosaves (R-MET-13), as one change under the save's lock (R-MET-11). A failed write keeps the rank in memory, as a bank does. */
	DUSKTRACER_API EDTTreeBuy Buy(FDTMetaSession& Session, FName Id);

	/** R-MET-22, R-MET-24. With bGated false the grant is empty, so no reader sees the profile. */
	DUSKTRACER_API FDTPoolGrant MakeGrant(const FDTMetaProfile& Profile, bool bGated);

	/** ST_Tree rows of a node; Desc takes the Keeper's name and the tool count. */
	DUSKTRACER_API FText NodeName(int32 Row);
	DUSKTRACER_API FText NodeDesc(int32 Row);
}
