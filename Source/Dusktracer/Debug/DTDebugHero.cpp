// Hero debug console commands (R-TEC-12: compiled out of Shipping; 02_Systems/combat_core.md).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Player/DTHero.h"
#include "Player/DTHeroSubsystem.h"

namespace DTDebugHero
{
	static UDTHeroSubsystem* Find(UWorld* World)
	{
		UDTHeroSubsystem* Hero = World ? World->GetSubsystem<UDTHeroSubsystem>() : nullptr;
		if (!Hero)
		{
			UE_LOG(LogDT, Warning, TEXT("DT hero: no hero subsystem in this world (Game/PIE worlds only)"));
		}
		return Hero;
	}

	static void God(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTHeroSubsystem* Hero = Find(World))
		{
			const bool bOn = Args.Num() > 0 ? FCString::Atoi(*Args[0]) != 0 : !Hero->GetNoDamage();
			Hero->SetNoDamage(bOn);
			UE_LOG(LogDT, Log, TEXT("DT.God %d (every hero hit is discarded while on)"), bOn ? 1 : 0);
		}
	}

	static void AimMode(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTHeroSubsystem* Hero = Find(World))
		{
			const int32 Player = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
			EDTAimMode Mode = Hero->GetAimMode(Player) == EDTAimMode::Auto ? EDTAimMode::Manual : EDTAimMode::Auto;
			if (Args.Num() > 0)
			{
				Mode = Args[0].Equals(TEXT("Manual"), ESearchCase::IgnoreCase) ? EDTAimMode::Manual : EDTAimMode::Auto;
			}
			Hero->SetAimMode(Player, Mode);
			UE_LOG(LogDT, Log, TEXT("DT.Hero.AimMode player=%d %s"), Player, Mode == EDTAimMode::Manual ? TEXT("Manual") : TEXT("Auto"));
		}
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTHeroSubsystem* Hero = Find(World))
		{
			for (int32 Player = 0; Player < Hero->GetNumHeroes(); ++Player)
			{
				const FDTHeroState& S = *Hero->GetState(Player);
				UE_LOG(LogDT, Log, TEXT("DT.Hero.Stats player=%d pos=(%.0f,%.0f) hp=%.1f/%.0f down=%d charges=%d refill=%.2f dashing=%.2f iframes=%.2f meter=%.2f aim=(%.2f,%.2f) mode=%s dashes=%d hits=%d intercessions=%d god=%d run_over=%d"),
					Player, S.Pos.X, S.Pos.Y, S.Hp, S.MaxHp, S.bDown ? 1 : 0, S.DashCharges, S.DashRefillLeft, S.DashLeft,
					S.HurtIFramesLeft, S.Intercession01, S.AimDir.X, S.AimDir.Y,
					Hero->GetAimMode(Player) == EDTAimMode::Manual ? TEXT("Manual") : TEXT("Auto"), S.Dashes, S.HitsTaken,
					S.Intercessions, Hero->GetNoDamage() ? 1 : 0, Hero->IsRunOver() ? 1 : 0);
			}
		}
	}

	/**
	 * DT.Hero.Walk: player 0 walks to a point on latched input, as a person's stick would take it, and stands there. A staged frame's
	 * hero position (Tools/Scenes, the edge goldens of Docs/Validation/S4_p0.md): the sim is driven, never written, so the arena clamp,
	 * the camera and every rule apply as in play.
	 */
	class FWalker
	{
	public:
		TWeakObjectPtr<UWorld> World;
		FVector2f Target = FVector2f::ZeroVector;

		void Latch()
		{
			UWorld* W = World.Get();
			UDTHeroSubsystem* Hero = W ? W->GetSubsystem<UDTHeroSubsystem>() : nullptr;
			const FDTHeroState* State = Hero ? Hero->GetState(0) : nullptr;
			if (!State)
			{
				return;
			}
			// The pawn latches its own empty input every frame and the newest axis wins (as the bot's mute, UDTBotSubsystem).
			for (TActorIterator<ADTHero> It(W); It; ++It)
			{
				It->SetActorTickEnabled(false);
			}
			// Within one step's travel the hero stands: a full-speed step would carry it past the point and back.
			const FVector2f To = Target - State->Pos;
			const float StepTravel = static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S) / static_cast<float>(DTConst::SIM_FIXED_HZ);
			FDTHeroInput Input;
			Input.Move = To.Size() > StepTravel ? To.GetSafeNormal() : FVector2f::ZeroVector;
			Hero->LatchInput(0, Input);
		}
	};

	static TMap<TWeakObjectPtr<UWorld>, TUniquePtr<FWalker>>& Walkers()
	{
		static TMap<TWeakObjectPtr<UWorld>, TUniquePtr<FWalker>> Map;
		return Map;
	}

	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		TUniquePtr<FWalker> Walker;
		if (Walkers().RemoveAndCopyValue(World, Walker))
		{
			if (UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>())
			{
				Sim->OnStepEnd.RemoveAll(Walker.Get());
			}
		}
	}

	static void Walk(const TArray<FString>& Args, UWorld* World)
	{
		UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
		if (!Sim || !Find(World) || Args.Num() < 2)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Hero.Walk <x> <y>: game worlds only"));
			return;
		}
		static bool bCleanupBound = false;
		if (!bCleanupBound)
		{
			FWorldDelegates::OnWorldCleanup.AddStatic(&OnWorldCleanup);
			bCleanupBound = true;
		}
		TUniquePtr<FWalker>& Walker = Walkers().FindOrAdd(World);
		if (!Walker)
		{
			Walker = MakeUnique<FWalker>();
			Walker->World = World;
			Sim->OnStepEnd.AddRaw(Walker.Get(), &FWalker::Latch);
		}
		Walker->Target = FVector2f(FCString::Atof(*Args[0]), FCString::Atof(*Args[1]));
		UE_LOG(LogDT, Log, TEXT("DT.Hero.Walk to (%.0f,%.0f)"), Walker->Target.X, Walker->Target.Y);
	}

	static FAutoConsoleCommandWithWorldAndArgs GodCmd(TEXT("DT.God"),
		TEXT("DT.God [0|1]: heroes take no damage (toggles without an argument)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&God));
	static FAutoConsoleCommandWithWorldAndArgs AimModeCmd(TEXT("DT.Hero.AimMode"),
		TEXT("DT.Hero.AimMode [Auto|Manual] [player]: set a player's aim mode (toggles without an argument; default player 0)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AimMode));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Hero.Stats"),
		TEXT("DT.Hero.Stats: log every hero's position, HP, dash, i-frames, Intercession meter and aim"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
	static FAutoConsoleCommandWithWorldAndArgs WalkCmd(TEXT("DT.Hero.Walk"),
		TEXT("DT.Hero.Walk <x> <y>: player 0 walks to the point on latched input and stands there (staged frames)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Walk));
}

#endif // !UE_BUILD_SHIPPING
