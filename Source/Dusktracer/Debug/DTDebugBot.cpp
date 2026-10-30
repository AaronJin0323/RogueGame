// Bot v1 debug console commands (R-TEC-12: compiled out of Shipping).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Bot/DTBotSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace DTDebugBot
{
	static UDTBotSubsystem* Find(UWorld* World)
	{
		UDTBotSubsystem* Bot = World ? World->GetSubsystem<UDTBotSubsystem>() : nullptr;
		if (!Bot)
		{
			UE_LOG(LogDT, Warning, TEXT("DT bot: no bot subsystem in this world (Game/PIE worlds only)"));
		}
		return Bot;
	}

	static void Start(const TArray<FString>& Args, UWorld* World)
	{
		UDTBotSubsystem* Bot = Find(World);
		if (!Bot)
		{
			return;
		}
		if (Args.Num() > 0 && Args[0].Equals(TEXT("Off"), ESearchCase::IgnoreCase))
		{
			Bot->Deactivate();
			UE_LOG(LogDT, Log, TEXT("DT.Bot: off (pawn input stays muted until the next run)"));
			return;
		}
		FDTBotConfig Config = UDTBotSubsystem::ReadConfig();
		Config.Policy = Args.Num() > 0 ? FDTBotConfig::ParsePolicy(Args[0]) : EDTBotPolicy::Steered;
		if (Config.Policy == EDTBotPolicy::Off)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Bot: unknown policy '%s' (None, Random, Steered, Off)"), *Args[0]);
			return;
		}
		if (Args.Num() > 1)
		{
			Config.Path = FDTBotConfig::ParsePath(Args[1]);
		}
		if (Args.Num() > 2)
		{
			Config.Archetype = FName(*Args[2]);
		}
		Bot->Activate(Config);
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		if (const UDTBotSubsystem* Bot = Find(World))
		{
			const FDTBotStats& S = Bot->GetBrain().GetStats();
			UE_LOG(LogDT, Log, TEXT("DT.Bot.Stats active=%d %s decisions=%lld dash_presses=%d intercession_presses=%d picks=%d declines=%d inputs=%lld hash=%08x finished=%d"),
				Bot->IsActive() ? 1 : 0, *Bot->GetBrain().GetConfig().Describe(), S.Decisions, S.DashPresses, S.IntercessionPresses,
				S.Picks, S.Declines, Bot->GetInputCount(), Bot->GetInputHash(), Bot->IsRunFinished() ? 1 : 0);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs StartCmd(TEXT("DT.Bot"),
		TEXT("DT.Bot [None|Random|Steered|Off] [Path] [Archetype]: drive player 0 with Bot v1 from now (default Steered)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Bot.Stats"),
		TEXT("DT.Bot.Stats: log the bot's config, decisions, presses, picks and input hash"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
}

#endif // !UE_BUILD_SHIPPING
