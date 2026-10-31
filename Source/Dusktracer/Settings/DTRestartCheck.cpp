#include "Settings/DTRestartCheck.h"

#if !UE_BUILD_SHIPPING
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHeroSim.h"
#include "Player/DTHeroSubsystem.h"
#include "Settings/DTMenuSubsystem.h"

namespace DTRestartCheckLocal
{
	// Wall seconds: how long a held key is watched, how long a phase may take before the check gives up.
	constexpr double SettleS = 0.5;
	constexpr double GiveUpS = 60.0;
	// Sim steps a run plays before the first press, so its opening hand is behind it.
	constexpr int64 ReadyStep = 30;

	enum class EPhase : uint8 { Control, ControlSeen, PauseIt, HoldThroughResume, WatchHeld, Fresh, FreshSeen, OpenRestart, ConfirmHeld, WatchHold, Reloaded, ReloadedSeen, Done };

	struct FCheck
	{
		EPhase Phase = EPhase::Control;
		double PhaseAt = 0.0;
		int32 Dashes = 0;
		int32 Base = 0;
		int32 Fails = 0;
		TWeakObjectPtr<UDTHeroSubsystem> Bound;
		FDelegateHandle Handle;
		TWeakObjectPtr<UWorld> OldWorld;
	};
	static FCheck Check;

	static void Enter(EPhase Phase)
	{
		Check.Phase = Phase;
		Check.PhaseAt = FPlatformTime::Seconds();
	}

	static double Elapsed()
	{
		return FPlatformTime::Seconds() - Check.PhaseAt;
	}

	static void Key(APlayerController* PC, const FKey& Pressed, bool bDown)
	{
		PC->InputKey(FInputKeyEventArgs::CreateSimulated(Pressed, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
	}

	static void Expect(const TCHAR* What, bool bGood)
	{
		Check.Fails += bGood ? 0 : 1;
		UE_LOG(LogDT, Log, TEXT("INPUTCHECK %s: %s (dashes %d, base %d)"), What, bGood ? TEXT("ok") : TEXT("WRONG"), Check.Dashes, Check.Base);
	}

	static void Finish(const TCHAR* Why)
	{
		UE_LOG(LogDT, Log, TEXT("INPUTCHECK: %s (%d wrong%s%s)"), Check.Fails == 0 && !Why[0] ? TEXT("PASS") : TEXT("FAIL"), Check.Fails, Why[0] ? TEXT(", ") : TEXT(""), Why);
		Enter(EPhase::Done);
		FPlatformMisc::RequestExit(false, TEXT("DT input check over"));
	}

	static bool Ready(UWorld* World, UDTHeroSubsystem* Heroes)
	{
		const UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
		const UDTOfferSubsystem* Offers = World->GetSubsystem<UDTOfferSubsystem>();
		const FDTHeroState* State = Heroes->GetState(0);
		return Sim && State && Sim->GetStepIndex() >= ReadyStep && !World->IsPaused() && !(Offers && Offers->IsAnyHandOpen())
			&& State->DashCharges >= FDTHeroSim::ComputeMaxCharges(State->Stats);
	}

	static void TickInputCheck(UDTMenuSubsystem* Menu)
	{
		UWorld* World = Menu->GetWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		UDTHeroSubsystem* Heroes = World ? World->GetSubsystem<UDTHeroSubsystem>() : nullptr;
		if (!PC || !Heroes || !World->HasBegunPlay() || !PC->GetPawn() || Check.Phase == EPhase::Done)
		{
			return;
		}
		if (Check.PhaseAt == 0.0)
		{
			Enter(EPhase::Control);
		}
		if (Check.Bound.Get() != Heroes)
		{
			Check.Bound = Heroes;
			Check.Handle = Heroes->OnHeroEvent.AddLambda([](const FDTHeroEvent& Event)
			{
				Check.Dashes += Event.Kind == EDTHeroEventKind::Dash ? 1 : 0;
			});
		}
		if (Elapsed() > GiveUpS)
		{
			Finish(TEXT("a phase never ended"));
			return;
		}
		switch (Check.Phase)
		{
		case EPhase::Control:
			if (Ready(World, Heroes))
			{
				Check.Base = Check.Dashes;
				Key(PC, EKeys::SpaceBar, true);
				Enter(EPhase::ControlSeen);
			}
			break;
		case EPhase::ControlSeen:
			if (Elapsed() >= SettleS)
			{
				Expect(TEXT("a press in play dashes"), Check.Dashes == Check.Base + 1);
				Key(PC, EKeys::SpaceBar, false);
				Enter(EPhase::PauseIt);
			}
			break;
		case EPhase::PauseIt:
			if (Elapsed() >= SettleS && Ready(World, Heroes))
			{
				Check.Base = Check.Dashes;
				Menu->RequestPause();
				Enter(EPhase::HoldThroughResume);
			}
			break;
		case EPhase::HoldThroughResume:
			if (Elapsed() >= SettleS)
			{
				Key(PC, EKeys::SpaceBar, true); // Space is a second Accept: focus is on Resume, so this press closes the menu and stays down
				Enter(EPhase::WatchHeld);
			}
			break;
		case EPhase::WatchHeld:
			if (Menu->IsOpen())
			{
				Check.PhaseAt = FPlatformTime::Seconds();
			}
			else if (Elapsed() >= SettleS)
			{
				Expect(TEXT("Space held through Resume dashes nothing"), Check.Dashes == Check.Base);
				Key(PC, EKeys::SpaceBar, false);
				Enter(EPhase::Fresh);
			}
			break;
		case EPhase::Fresh:
			if (Elapsed() >= SettleS && Ready(World, Heroes))
			{
				Check.Base = Check.Dashes;
				Key(PC, EKeys::SpaceBar, true);
				Enter(EPhase::FreshSeen);
			}
			break;
		case EPhase::FreshSeen:
			if (Elapsed() >= SettleS)
			{
				Expect(TEXT("a fresh press after the pause dashes"), Check.Dashes == Check.Base + 1);
				Key(PC, EKeys::SpaceBar, false);
				Enter(EPhase::OpenRestart);
			}
			break;
		case EPhase::OpenRestart:
			if (Elapsed() >= SettleS && Ready(World, Heroes))
			{
				Menu->RequestPause();
				Menu->Command(EDTMenuCommand::Down);
				Menu->Command(EDTMenuCommand::Down);
				Menu->Command(EDTMenuCommand::Accept);
				Menu->Command(EDTMenuCommand::Up); // off Cancel, onto the confirm row
				Enter(EPhase::ConfirmHeld);
			}
			break;
		case EPhase::ConfirmHeld:
			if (Elapsed() >= SettleS)
			{
				Check.OldWorld = World;
				Key(PC, EKeys::SpaceBar, true);
				Enter(EPhase::WatchHold);
			}
			break;
		case EPhase::WatchHold:
			if (Elapsed() >= 2.0 * SettleS)
			{
				Expect(TEXT("a held Accept holds the reload"), Check.OldWorld.Get() == World);
				Key(PC, EKeys::SpaceBar, false);
				Enter(EPhase::Reloaded);
			}
			break;
		case EPhase::Reloaded:
			if (World != Check.OldWorld.Get() && Ready(World, Heroes))
			{
				Expect(TEXT("the map reloaded with a seed"), World->URL.HasOption(TEXT("DTSeed")));
				Check.Base = Check.Dashes;
				Key(PC, EKeys::SpaceBar, true);
				Enter(EPhase::ReloadedSeen);
			}
			break;
		case EPhase::ReloadedSeen:
			if (Elapsed() >= SettleS)
			{
				Expect(TEXT("keys work after the reload"), Check.Dashes == Check.Base + 1);
				Key(PC, EKeys::SpaceBar, false);
				Finish(TEXT(""));
			}
			break;
		default:
			break;
		}
	}
}

void DTRestartCheck::Tick(UDTMenuSubsystem* Menu)
{
	UWorld* World = Menu->GetWorld();
	if (!World || !World->HasBegunPlay())
	{
		return;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("DTInputCheck")))
	{
		DTRestartCheckLocal::TickInputCheck(Menu);
		return;
	}
	int64 AtStep = -1;
	int32 Seed = 0;
	const UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	if (Sim && !World->URL.HasOption(TEXT("DTSeed")) && FParse::Value(FCommandLine::Get(), TEXT("DTRestartAtStep="), AtStep)
		&& FParse::Value(FCommandLine::Get(), TEXT("DTRestartSeed="), Seed) && Sim->GetStepIndex() >= AtStep)
	{
		UE_LOG(LogDT, Log, TEXT("RestartCheck: restart on seed %d at step %lld"), Seed, Sim->GetStepIndex());
		Menu->RestartRun(TOptional<int32>(Seed));
	}
}
#endif
