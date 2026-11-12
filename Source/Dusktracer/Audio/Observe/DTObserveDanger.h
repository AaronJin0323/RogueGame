// The Danger family's observer (02_Systems/audio.md R-AUD-02, R-AUD-05, R-AUD-12; Validation/S4_audio_wave1.md §4.3, §7.1): enemy fire,
// the hero hurt and down, the boss's phase, and the low-health heartbeat in step with the HP bar's pulse (hud.md R-HUD-05).
#pragma once

#include "CoreMinimal.h"
#include "Audio/DTAudioObserver.h"
#include "Audio/DTSoundsGen.h"
#include "Player/DTHeroSubsystem.h"

/** What the danger side shows at one step's end: all the observer compares. */
struct FDTDangerView
{
	int64 Step = 0;
	/** The enemy rounds' spawned counter (it only grows within a run). */
	int64 EnemyRounds = 0;
	/** The local player's hero events of this step (OnHeroEvent): damage taken and down. */
	int32 Hurt = 0;
	int32 Down = 0;
	bool bBossActive = false;
	int32 BossPhase = 0;
	/** The HUD's low-HP state of the local player's HP block (DTHud::LowHp): at or under C.HUD.LOW_HP_FRAC and alive. */
	bool bLowHp = false;
};

namespace DTDangerEvents
{
	/** The index of the HP bar's pulse that a step falls in: the HUD's own clock (sim seconds over C.HUD.LOW_HP_PULSE_S). */
	DUSKTRACER_API int64 PulseIndex(int64 Step);
	/** The danger moments between two step views, each one request with its count; the caller says whether the heartbeat ducks. */
	DUSKTRACER_API void Diff(const FDTDangerView& Was, const FDTDangerView& Now, TFunctionRef<void(DTCat::ESoundId Sound, int32 Count)> Raise);
	/** The low-health hold (R-AUD-12, form b): C.AUD.LOWHP_HOLD_DB in Shipping; elsewhere dt.Audio.LowHpHoldDb, whose default is the key
	 *  and whose 0 selects form a (each beat ducks). Never above zero. */
	DUSKTRACER_API float LowHpHoldDb();
}

class DUSKTRACER_API FDTObserveDanger : public IDTAudioFamily
{
public:
	virtual ~FDTObserveDanger() override { End(); }
	virtual void OnStep(const UWorld& World, int64 Step, IDTAudioRequests& Out) override;
	virtual void Reset() override;
	virtual void Begin(UWorld& World) override;
	virtual void End() override;
	/** One step's view after the world is read: the run's baseline on its first step, then the moments, the held duck (HoldDb below zero: form b). */
	void Observe(const FDTDangerView& Now, IDTAudioRequests& Out, float HoldDb);

private:
	void HandleHeroEvent(const FDTHeroEvent& Event);

	FDTDangerView Last;
	bool bPrimed = false;
	int32 PendingHurt = 0;
	int32 PendingDown = 0;
	TWeakObjectPtr<UDTHeroSubsystem> Heroes;
	FDelegateHandle HeroHandle;
};
