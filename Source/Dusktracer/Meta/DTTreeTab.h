// The Tree as a page of the hub (02_Systems/vigil.md R-VIG-18..R-VIG-20). The hub draws rows and calls Accept; this page says what
// they read and makes the purchase, because no hub file holds a session (R-VIG-14). Plain C++ on a session, so tests drive it on temp files.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Hub/DTHubTab.h"

struct FDTMetaSession;
struct FDTTreeNode;

class DUSKTRACER_API FDTTreeTab final : public IDTHubTab
{
public:
	explicit FDTTreeTab(FDTMetaSession& InSession);

	virtual int32 NumRows() const override { return Rows.Num(); }
	virtual FText RowLabel(int32 Row) const override;
	virtual FText RowValue(int32 Row) const override;
	virtual FText RowDetail(int32 Row) const override;
	virtual FText Header() const override;
	virtual void Accept(int32 Row) override;
	virtual void Opened() override { Signed = INDEX_NONE; }
	virtual bool IsHeading(int32 Row) const override;
	virtual bool IsDimmed(int32 Row) const override;

private:
	/** A heading has no node. Branch: the catalog row of the group's first node. */
	struct FRow
	{
		int32 Node = INDEX_NONE;
		EDTPath Path = EDTPath::None;
		int32 Branch = INDEX_NONE;
	};

	FDTTreeNode NodeAt(int32 Row) const;
	/** The branch of a Keeper the profile has not met (R-MET-21): its rows say Sealed, as the Shrine does. */
	bool IsSealed(int32 Row) const;
	/** What stands in the way, or what just happened; empty when the state and the price say it all. */
	FText Reason(int32 Row) const;

	FDTMetaSession& Session;
	TArray<FRow> Rows;
	/** The row bought last on this visit of the page, for its line. */
	int32 Signed = INDEX_NONE;
};

namespace DTTreeTab
{
	/** Gives the hub this page: once at module start, and again by a test that swapped the factory. */
	DUSKTRACER_API void Register();
}
