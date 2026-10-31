// The S2 perf proof scene (Tools/Perf.ps1 -Scene S2, Docs/Validation/S2_perf.md). Compiled out of Shipping (R-TEC-12).
#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Combat/DTBulletPool.h"

namespace DTPerfScene
{
	/**
	 * One round of the scene's filler: it collides like any round but belongs to no player and deals nothing, so the bullet cap
	 * is met without the filler killing the crowd or starting a reaction. What fires in the scene, the build fired.
	 */
	DUSKTRACER_API FDTBulletSpawn FillerRound(float Angle);
}

#endif
