// Draws the hub on the HUD canvas (02_Systems/vigil.md R-VIG-15): grey box, tokens, tables, labels only.
#pragma once

#include "CoreMinimal.h"
#include "Hub/DTHubData.h"

class IDTHubTab;
class UCanvas;
struct FDTUserSettings;

/** What one frame of the hub shows; the subsystem owns all of it. */
struct FDTHubView
{
	const FDTHubState* State = nullptr;
	const TArray<FDTCodexRow>* Codex = nullptr;
	const FDTMetaProfile* Profile = nullptr;
	const IDTHubTab* Tree = nullptr;
};

namespace DTHubText
{
	/** Every literal ST_UI key the hub draws (T-VIG-15). */
	DUSKTRACER_API TArray<FString> AllKeys();
	DUSKTRACER_API FText PageTitle(EDTHubPage Page);
	DUSKTRACER_API FText RootLabel(EDTHubRow Row);
	/** The honorific of a Path. */
	DUSKTRACER_API FText KeeperName(EDTPath Path);
	/** The text under the rows: a Keeper's terms, a sealed hint, a book entry. */
	DUSKTRACER_API FText Detail(const FDTHubView& View);
}

namespace DTHubScreen
{
	DUSKTRACER_API void Draw(UCanvas* Canvas, const FDTHubView& View, const FDTUserSettings& Settings);
}
