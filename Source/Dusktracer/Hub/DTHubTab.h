// The slot one hub page plugs into (02_Systems/vigil.md R-VIG-12). The hub knows rows and Accept; everything else belongs to the page.
#pragma once

#include "CoreMinimal.h"

class UWorld;

/** A page of rows on the hub. Text only: the hub draws it, moves focus and calls Accept. */
class IDTHubTab
{
public:
	virtual ~IDTHubTab() = default;
	virtual int32 NumRows() const = 0;
	virtual FText RowLabel(int32 Row) const = 0;
	/** Right-aligned value: a rank, a cost; may be empty. */
	virtual FText RowValue(int32 Row) const = 0;
	/** The focused row's detail, under the list; may be empty. */
	virtual FText RowDetail(int32 Row) const = 0;
	/** The line under the page title, such as the balance; may be empty. */
	virtual FText Header() const = 0;
	/** Accept on a row. The page writes its own data; the hub only redraws. */
	virtual void Accept(int32 Row) = 0;
	/** A heading groups the rows under it: drawn, never focused, takes no Accept (R-VIG-20). */
	virtual bool IsHeading(int32 Row) const { return false; }
	/** A row that cannot be taken yet: drawn grey, and its value says why in words (R-VIG-15). */
	virtual bool IsDimmed(int32 Row) const { return false; }
};

namespace DTHubTabs
{
	/** Registers what builds the Tree tab, once at module start (DTTreeTab::Register). With none the tab is an empty page. */
	DUSKTRACER_API void SetTreeFactory(TFunction<TSharedPtr<IDTHubTab>(UWorld*)> Factory);
	DUSKTRACER_API TSharedPtr<IDTHubTab> MakeTree(UWorld* World);
}
