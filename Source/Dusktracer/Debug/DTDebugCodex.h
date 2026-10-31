// Layer audit of the story tables (03_Narrative R-NAR-06): reads the Layer and Unlock metadata the string tables carry.
#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

namespace DTDebugCodex
{
	/** One story row as a player could meet it. */
	struct FStoryRow
	{
		FName Table;
		FString Key;
		FString Unlock;
		FString Text;
		int32 Layer = 0;
	};

	/** Every row of the story tables, sorted by layer, then table, then key. */
	DUSKTRACER_API TArray<FStoryRow> ReadStoryRows();

	/** True when the unlock takes a win (Unlock column, 03_Narrative R-NAR-03). */
	DUSKTRACER_API bool NeedsWin(const FString& Unlock);

	/** The deepest layer among rows that need no win; -1 for no rows. */
	DUSKTRACER_API int32 MaxLayerBeforeFirstWin(const TArray<FStoryRow>& Rows);
}

#endif
