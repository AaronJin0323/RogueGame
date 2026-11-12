// The Warn family's observer (02_Systems/audio.md R-AUD-02, R-AUD-05; Validation/S4_audio_wave1.md §4.3, §7.1): each telegraph that is new
// this step raises its warning once, by shape. Read at step end from the public world view, at most C.AUD.SCAN_MAX entries.
#pragma once

#include "CoreMinimal.h"
#include "Audio/DTAudioObserver.h"
#include "Audio/DTSoundsGen.h"
#include "Core/DTSimTypes.h"

namespace DTWarnEvents
{
	/** The warning of a telegraph's shape: band; arc; ring (the boss's ring, spiral and hazard shapes too); aim line. */
	DUSKTRACER_API DTCat::ESoundId SoundOf(EDTTelegraphShape Shape);

	/**
	 * How "new" is known: the view carries no id, so a telegraph of this step is the one of the last step when that one has the same shape,
	 * a Progress no greater, and an origin within MatchUU; each of the last step's telegraphs is matched at most once. Anything left
	 * unmatched is new. So a telegraph that stands for many steps (its Progress only grows, its origin stays or moves a little) is new once,
	 * and a tell that starts again where the last one ended (its Progress drops) is new again.
	 */
	inline constexpr float MatchUU = 150.0f; // an origin moves at most this far in one step: a draining elite walks a few uu, a turret none
	DUSKTRACER_API void NewThisStep(TConstArrayView<FDTTelegraph> Was, TConstArrayView<FDTTelegraph> Now, TFunctionRef<void(DTCat::ESoundId Sound, int32 Count)> Raise);
}

class DUSKTRACER_API FDTObserveWarn : public IDTAudioFamily
{
public:
	virtual void OnStep(const UWorld& World, int64 Step, IDTAudioRequests& Out) override;
	virtual void Reset() override { Last.Reset(); }

private:
	TArray<FDTTelegraph> Last;
	TArray<FDTTelegraph> Scan;
};
