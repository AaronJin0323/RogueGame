// What the hub reads from the profile and the tables (02_Systems/vigil.md R-VIG-05, R-VIG-08..10). Read only: nothing here writes a profile.
#pragma once

#include "CoreMinimal.h"
#include "Hub/DTHubState.h"
#include "Meta/DTMetaProfile.h"

/** One row of ST_Codex as the table gives it. */
struct FDTCodexSource
{
	FString Key;
	int32 Layer = 0;
	FString Unlock;
};

/** One line of the book. Body is empty when the row is not read yet or is a title only. */
struct FDTCodexRow
{
	FString Key;
	int32 Layer = 0;
	bool bRead = false;
	FText Title;
	FText Body;
};

struct FDTHubStat
{
	FText Label;
	FText Value;
};

namespace DTHubData
{
	/** R-VIG-09: does this profile meet an Unlock cell? An unknown cell does not. */
	DUSKTRACER_API bool IsUnlocked(const FString& Unlock, const FDTMetaProfile& Profile);
	/** R-VIG-08: the deepest layer listed for this profile. */
	DUSKTRACER_API int32 LayerReach(const FDTMetaProfile& Profile);
	/** The rows of ST_Codex with their metadata; empty when the table is not registered. */
	DUSKTRACER_API TArray<FDTCodexSource> ReadCodexTable();
	/** The book: listed rows only, by layer, then key. A row's text reaches the output only when it is read. */
	DUSKTRACER_API TArray<FDTCodexRow> BuildCodex(const TArray<FDTCodexSource>& Sources, const FDTMetaProfile& Profile);
	/** A profile that reads every one of these rows: the wins, kills and Discoveries their Unlock cells ask for. For tests and proofs. */
	DUSKTRACER_API FDTMetaProfile EverythingRead(const TArray<FDTCodexSource>& Sources);
	/** The Keepers in Path order; InPool: this launch's own pool says the id is in it (R-VIG-05). */
	DUSKTRACER_API TArray<FDTHubPatron> Patrons(const FDTMetaProfile& Profile, TFunctionRef<bool(FName)> InPool);
	/** The Path of a save's LastPatron; None for an empty or unknown name. */
	DUSKTRACER_API EDTPath ParsePath(FName Name);
	DUSKTRACER_API TArray<FDTHubStat> Stats(const FDTMetaProfile& Profile);
	/** The map options of a run started from the hub (R-VIG-07). */
	DUSKTRACER_API FString RunOptions(int32 Seed, EDTPath Patron, const FDTMetaProfile& Profile);
	/** H:MM:SS of sim steps. */
	DUSKTRACER_API FText FormatSteps(int64 Steps);
}
