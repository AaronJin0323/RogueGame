// The Fire family's observer (02_Systems/audio.md R-AUD-02, R-AUD-07; Validation/S4_audio_wave1.md §4.3, §7.1): each weapon of the
// local player sounds as it fires, by the growth of its spawned counter, and above C.AUD.FIRE_DENSE_SHOTS_PER_S firings a second at most
// C.AUD.FIRE_DENSE_VOICES_PER_S times a second.
#pragma once

#include "CoreMinimal.h"
#include "Audio/DTAudioObserver.h"
#include "Audio/DTSoundsGen.h"
#include "Core/DTConstantsGen.h"

/** What the local player's weapons show at one step's end: all the fire observer compares. */
struct FDTFireView
{
	int64 Step = 0;
	/** Per weapon slot: the weapon's spawned counter (it only grows within a run) and its row in DTCat::Weapons (INDEX_NONE: no weapon). */
	int64 Spawned[DTConst::WEAPON_SLOTS] = {};
	int32 Weapon[DTConst::WEAPON_SLOTS] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
};
static_assert(DTConst::WEAPON_SLOTS == 4, "FDTFireView's Weapon initialiser names C.WEAPON.SLOTS slots");

namespace DTFireEvents
{
	/** The fire sound of a weapon id, by name: Weapon.<Name> sounds as Snd.Fire.<Name>; INDEX_NONE when the catalog has no such row. */
	DUSKTRACER_API int32 SoundOfId(const TCHAR* WeaponId);
	/** The fire sound of a row of DTCat::Weapons (INDEX_NONE for none). A Reforging form keeps its base weapon's row, so it sounds as it. */
	DUSKTRACER_API int32 SoundOf(int32 WeaponRow);
	/** The firings between two step views: per slot whose counter grew, its sound and the growth. A counter that went down, or a slot
	 *  that held another weapon, raises nothing; a slot that held none last step compares with what it showed there. */
	DUSKTRACER_API void Diff(const FDTFireView& Was, const FDTFireView& Now, TFunctionRef<void(int32 Slot, int32 Sound, int32 Count)> Raise);
}

/**
 * R-AUD-07's last clause for one weapon, on sim time (engine-free). A weapon whose firings of the last second (C.SIM.FIXED_HZ steps,
 * this one included) come faster than C.AUD.FIRE_DENSE_SHOTS_PER_S a second, read from the first of them to this one, is dense: it
 * sounds only when at least MinGapSteps() have passed since it last sounded, so in any second it sounds at most
 * C.AUD.FIRE_DENSE_VOICES_PER_S times, evenly spread. A weapon below the rate sounds every time. A firing is one step on which the
 * weapon's counter grew, whatever it spawned.
 */
class DUSKTRACER_API FDTFireDensity
{
public:
	/** A firing at Step (steps only grow until Reset): true when it sounds, false when it is withheld. */
	bool Admit(int64 Step);
	void Reset();
	/** ceil(C.SIM.FIXED_HZ / C.AUD.FIRE_DENSE_VOICES_PER_S): the least steps between two voices of a dense weapon. */
	static int64 MinGapSteps();

private:
	/** The firings of the last second, oldest first: at most one a step, so it never holds more than C.SIM.FIXED_HZ. */
	TArray<int64> Recent;
	int64 LastSounded = TNumericLimits<int64>::Lowest();
	bool bSounded = false;
};

class DUSKTRACER_API FDTObserveFire : public IDTAudioFamily
{
public:
	virtual void OnStep(const UWorld& World, int64 Step, IDTAudioRequests& Out) override;
	virtual void Reset() override;
	/** One step's view: the run's baseline on its first step, then each slot's firing, sounded or withheld by its density. */
	void Observe(const FDTFireView& Now, IDTAudioRequests& Out);

private:
	FDTFireView Last;
	bool bPrimed = false;
	FDTFireDensity Density[DTConst::WEAPON_SLOTS];
};
