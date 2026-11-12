#include "Audio/Observe/DTObserveDanger.h"
#include "Boss/DTBossSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/World.h"
#include "Game/DTHudLayout.h"
#include "HAL/IConsoleManager.h"

namespace DTObserveDangerLocal
{
#if !UE_BUILD_SHIPPING
	static TAutoConsoleVariable<float> CVarLowHpHoldDb(
		TEXT("dt.Audio.LowHpHoldDb"),
		static_cast<float>(DTConst::AUD_LOWHP_HOLD_DB),
		TEXT("Audio, the heartbeat's form (not in Shipping, where C.AUD.LOWHP_HOLD_DB holds): below 0 (the default, the key) = the player's side is held ")
		TEXT("this many dB down for as long as health is low, the heartbeat riding on it without its own duck (form b); 0 = each beat ducks the player's ")
		TEXT("side as any danger voice does (form a)."));
#endif
	constexpr int32 LocalOwner = 0; // the solo player, whose HP block the HUD draws
}

namespace DTDangerEvents
{
	int64 PulseIndex(int64 Step)
	{
		// The HUD's clock (DTHud::LowHp, from ADTHUD): sim seconds, and a pulse that starts full at every whole period.
		const double SimSeconds = static_cast<double>(Step) / DTConst::SIM_FIXED_HZ;
		return static_cast<int64>(FMath::FloorToDouble(SimSeconds / static_cast<double>(DTConst::HUD_LOW_HP_PULSE_S)));
	}

	void Diff(const FDTDangerView& Was, const FDTDangerView& Now, TFunctionRef<void(DTCat::ESoundId Sound, int32 Count)> Raise)
	{
		using DTCat::ESoundId;
		if (Now.EnemyRounds > Was.EnemyRounds)
		{
			Raise(ESoundId::Snd_Danger_EnemyFire, static_cast<int32>(FMath::Min<int64>(Now.EnemyRounds - Was.EnemyRounds, MAX_int32)));
		}
		// The hero's public events. A step that holds a down sounds as the fall alone: the hurt and the fall started in one frame read over
		// C.AUD.LIMIT_DBFS and five dB above either alone (the Sound session's decision, 2026-11-13). Hurts with no down keep their count.
		if (Now.Down > 0)
		{
			Raise(ESoundId::Snd_Danger_HeroDown, Now.Down);
		}
		else if (Now.Hurt > 0)
		{
			Raise(ESoundId::Snd_Danger_HeroHurt, Now.Hurt);
		}
		if (Now.bBossActive && Was.bBossActive && Now.BossPhase != Was.BossPhase)
		{
			Raise(ESoundId::Snd_Danger_BossPhase, 1);
		}
		// R-AUD-12: one beat as each of the HP bar's pulses begins, while the HUD shows low HP; a step that does not run beats nothing.
		if (Now.bLowHp && PulseIndex(Now.Step) != PulseIndex(Was.Step))
		{
			Raise(ESoundId::Snd_Danger_Heartbeat, 1);
		}
	}

	float LowHpHoldDb()
	{
#if !UE_BUILD_SHIPPING
		return FMath::Min(0.0f, DTObserveDangerLocal::CVarLowHpHoldDb.GetValueOnGameThread());
#else
		return FMath::Min(0.0f, static_cast<float>(DTConst::AUD_LOWHP_HOLD_DB)); // form b, the game's (R-AUD-12): in Shipping the variable does not exist
#endif
	}
}

void FDTObserveDanger::Begin(UWorld& World)
{
	End();
	if (UDTHeroSubsystem* Hero = World.GetSubsystem<UDTHeroSubsystem>())
	{
		Heroes = Hero;
		HeroHandle = Hero->OnHeroEvent.AddRaw(this, &FDTObserveDanger::HandleHeroEvent);
	}
}

void FDTObserveDanger::End()
{
	if (UDTHeroSubsystem* Hero = Heroes.Get())
	{
		Hero->OnHeroEvent.Remove(HeroHandle);
	}
	HeroHandle.Reset();
	Heroes.Reset();
}

void FDTObserveDanger::HandleHeroEvent(const FDTHeroEvent& Event)
{
	if (Event.OwnerId != DTObserveDangerLocal::LocalOwner)
	{
		return;
	}
	PendingHurt += Event.Kind == EDTHeroEventKind::HeroDamaged ? 1 : 0;
	PendingDown += Event.Kind == EDTHeroEventKind::HeroDown ? 1 : 0;
}

void FDTObserveDanger::Reset()
{
	Last = FDTDangerView();
	bPrimed = false;
	PendingHurt = 0;
	PendingDown = 0;
}

void FDTObserveDanger::OnStep(const UWorld& World, int64 Step, IDTAudioRequests& Out)
{
	FDTDangerView Now;
	Now.Step = Step;
	if (const UDTEnemySubsystem* Enemies = World.GetSubsystem<UDTEnemySubsystem>())
	{
		Now.EnemyRounds = Enemies->GetPool().GetRounds().GetCounters().Spawned;
	}
	if (const UDTBossSubsystem* Boss = World.GetSubsystem<UDTBossSubsystem>())
	{
		Now.bBossActive = Boss->GetCore().IsActive();
		Now.BossPhase = Boss->GetCore().GetPhase();
	}
	if (const UDTHeroSubsystem* Hero = World.GetSubsystem<UDTHeroSubsystem>())
	{
		if (const FDTHeroState* State = Hero->GetState(DTObserveDangerLocal::LocalOwner))
		{
			// The HUD's own condition (R-HUD-05), on the HUD's own clock: ear and eye agree on when and how often.
			Now.bLowHp = !State->bDown && DTHud::LowHp(State->Hp, State->MaxHp, static_cast<double>(Step) / DTConst::SIM_FIXED_HZ, true).bLow;
		}
	}
	Now.Hurt = PendingHurt;
	Now.Down = PendingDown;
	PendingHurt = 0;
	PendingDown = 0;
	Observe(Now, Out, DTDangerEvents::LowHpHoldDb());
}

void FDTObserveDanger::Observe(const FDTDangerView& Now, IDTAudioRequests& Out, float HoldDb)
{
	if (!bPrimed)
	{
		// The first step of a run is the baseline: rounds already counted and a phase already set are not this step's news.
		Last = Now;
		Last.Hurt = Last.Down = 0;
		Last.Step = Now.Step - 1;
		bPrimed = true;
	}
	const bool bHeld = Now.bLowHp && HoldDb < 0.0f;
	DTDangerEvents::Diff(Last, Now, [&](DTCat::ESoundId Sound, int32 Count)
	{
		Out.Run(static_cast<int32>(Sound), Count, Now.Step, bHeld && Sound == DTCat::ESoundId::Snd_Danger_Heartbeat);
	});
	Out.HeldDuck(bHeld ? HoldDb : 0.0f);
	Last = Now; // a counter that went down (a new pool) is the new baseline: Diff raised nothing for it
}
