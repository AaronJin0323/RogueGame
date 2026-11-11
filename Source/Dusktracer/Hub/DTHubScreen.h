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
	/** The root's line about the save, empty when there is none (R-VIG-21). */
	FText Notice;
};

namespace DTHubText
{
	/** Every literal ST_UI key the hub draws (T-VIG-15). */
	DUSKTRACER_API TArray<FString> AllKeys();
	DUSKTRACER_API FText PageTitle(EDTHubPage Page);
	DUSKTRACER_API FText RootLabel(EDTHubRow Row);
	/** The honorific of a Path. */
	DUSKTRACER_API FText KeeperName(EDTPath Path);
	/** What the root says of the save (R-VIG-21): Hub.Notice.SaveAside with the file's name, Hub.Notice.RunLost, or nothing. */
	DUSKTRACER_API FText Notice(EDTMetaNotice Kind, const FString& AsideFile);
	/** The text under the rows: a Keeper's terms, a sealed hint, a book entry; on the root, what Begin lacks, else the notice. */
	DUSKTRACER_API FText Detail(const FDTHubView& View);
}

namespace DTHubScreen
{
	/** Returns how many strings of this frame ran past their room (R-VIG-16; Tools/VigilCheck.ps1 reads it on the real font). */
	DUSKTRACER_API int32 Draw(UCanvas* Canvas, const FDTHubView& View, const FDTUserSettings& Settings);
}
