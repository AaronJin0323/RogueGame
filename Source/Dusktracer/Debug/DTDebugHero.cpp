// Hero debug console commands (R-TEC-12: compiled out of Shipping; 02_Systems/combat_core.md).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Dusktracer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
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

	static FAutoConsoleCommandWithWorldAndArgs GodCmd(TEXT("DT.God"),
		TEXT("DT.God [0|1]: heroes take no damage (toggles without an argument)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&God));
	static FAutoConsoleCommandWithWorldAndArgs AimModeCmd(TEXT("DT.Hero.AimMode"),
		TEXT("DT.Hero.AimMode [Auto|Manual] [player]: set a player's aim mode (toggles without an argument; default player 0)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&AimMode));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Hero.Stats"),
		TEXT("DT.Hero.Stats: log every hero's position, HP, dash, i-frames, Intercession meter and aim"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
}

#endif // !UE_BUILD_SHIPPING
