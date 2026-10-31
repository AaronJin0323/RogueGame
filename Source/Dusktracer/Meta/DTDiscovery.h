// The Discoveries (02_Systems/meta.md R-MET-40..R-MET-45): rows of Docs/09_Catalog/discoveries.csv read against one run's tally. Plain C++.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTCatalogGen.h"
#include "Meta/DTMetaProfile.h"

namespace DTDiscovery
{
	/** A row's counter in this tally: the tally's own fields by name, else FDTRunTally::Counters, else 0. */
	DUSKTRACER_API int64 Value(const FDTRunTally& Tally, const TCHAR* Counter);
	DUSKTRACER_API bool IsMet(const DTCat::FDiscoveryRow& Row, const FDTRunTally& Tally);
	/** Every row this tally meets, in catalog order. Reads no profile. */
	DUSKTRACER_API TArray<FName> MetBy(const FDTRunTally& Tally);
	/** The `Unlocked` key of a Discovery: the Unlock column of its story rows (03_Narrative R-NAR-03). */
	DUSKTRACER_API FName UnlockKey(const TCHAR* Id);
	/** The FDTMetaSession::OnBank hook: earned rows go into Discoveries, their keys into Unlocked. A repeat changes nothing. */
	DUSKTRACER_API void Apply(const FDTRunTally& Tally, FDTMetaProfile& Profile);
}
