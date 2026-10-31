// Whether a world is the hub (02_Systems/vigil.md R-VIG-01, R-VIG-02). No save or settings type here: the game mode reads this.
#pragma once

#include "CoreMinimal.h"

struct FURL;

namespace DTHubFlow
{
	/** The pure rule: a person's launch, or a proof that forced the hub, unless the map came with a seed. */
	DUSKTRACER_API bool Wants(bool bToolRun, bool bForced, bool bSeedOption);
	/** This process uses the hub: not a tool run, or -DTHub. Does not look at a map. */
	DUSKTRACER_API bool Enabled();
	/** This map load opens the hub, not a run. */
	DUSKTRACER_API bool WantsHub(const FURL& Url);
}
