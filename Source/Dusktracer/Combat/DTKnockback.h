// The one knockback path (keepers.md ConvertMelee, R-KEE-14 Forge base; D-053 item 3).
// A strike that knocks back queues one FDTDisplacement per enemy it hits, straight away from the strike's centre, and the enemy
// pool applies the queue at the next Enemies phase (FDTEnemyPool::ApplyDisplacements: a stale serial is ignored, the boss hull
// never moves, R-BOS-03, and every push is clamped to the arena). Forge's Intercession ring and a cleave under Hard Blow both
// call this; neither writes a displacement of its own.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSimTypes.h"

namespace DTKnockback
{
	/**
	 * Queues a push of Enemy (at View's index, with View's serial) by DistanceUu away from Center. An enemy standing on the
	 * centre is pushed along Fallback (the owner's aim). Nothing is queued for a distance of zero or less.
	 */
	DUSKTRACER_API void Push(FDTWorldView& View, int32 OwnerId, int32 Enemy, const FVector2f& Center, const FVector2f& Fallback, float DistanceUu);
}
