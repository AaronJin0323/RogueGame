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
};

namespace DTHubTabs
{
	/** Registers what builds the Tree tab, once at module start. With none the tab is an empty page. */
	DUSKTRACER_API void SetTreeFactory(TFunction<TSharedPtr<IDTHubTab>(UWorld*)> Factory);
	DUSKTRACER_API TSharedPtr<IDTHubTab> MakeTree(UWorld* World);
}
