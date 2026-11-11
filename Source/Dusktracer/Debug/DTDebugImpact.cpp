// Impact art debug commands (R-TEC-12: compiled out of Shipping; 02_Systems/impact_art.md). Nothing here writes sim state.
// DT.Impact.Stats prints the live effects, the density tier and its rate, hits and kills over the last second and the effects a cap refused;
// "hold" prints it once more when the sim is held for a shot, so a tool run reads what its held frame holds.
// dt.Impact.Draw 0 raises and draws no effect (the tier still advances, R-IMP-11); outside Shipping it starts from the environment variable
// DT_IMPACT_DRAW when that is set, so a script that cannot pass a console variable can still run both ways.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Presentation/DTImpactSubsystem.h"

namespace DTDebugImpact
{
	/** dt.Impact.Draw's start: DT_IMPACT_DRAW when set (0 or 1), else 1. */
	static int32& StartValue()
	{
		int32& Draw = UDTImpactSubsystem::DrawSetting();
		const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("DT_IMPACT_DRAW"));
		if (!Env.IsEmpty())
		{
			Draw = FCString::Atoi(*Env) != 0 ? 1 : 0;
		}
		return Draw;
	}

	static FAutoConsoleVariableRef CVarDraw(TEXT("dt.Impact.Draw"), StartValue(),
		TEXT("1 (default): impact effects are raised and drawn. 0: none is raised or drawn and live ones end; the density tier still advances. Starts from DT_IMPACT_DRAW when set."));

	static void Print(UWorld* World)
	{
		const UDTImpactSubsystem* Impact = World ? World->GetSubsystem<UDTImpactSubsystem>() : nullptr;
		if (!Impact)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Impact.Stats: game worlds only"));
			return;
		}
		UE_LOG(LogDT, Log, TEXT("%s"), *Impact->DescribeStats());
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		Print(World);
		if (Args.Num() == 0 || !Args[0].Equals(TEXT("hold"), ESearchCase::IgnoreCase) || !World)
		{
			return;
		}
		// Once more on the held step of a shot (-DTShot holds the sim): the line then names what the shot's frame holds.
		TWeakObjectPtr<UWorld> Weak(World);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
		{
			UWorld* Live = Weak.Get();
			const UDTSimSubsystem* Sim = Live ? Live->GetSubsystem<UDTSimSubsystem>() : nullptr;
			if (!Sim)
			{
				return false;
			}
			if (!Sim->IsHeld())
			{
				return true;
			}
			if (const UDTImpactSubsystem* Held = Live->GetSubsystem<UDTImpactSubsystem>())
			{
				UE_LOG(LogDT, Log, TEXT("DT.Impact.Stats held: %s"), *Held->DescribeStats());
			}
			return false;
		}));
	}

	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Impact.Stats"),
		TEXT("DT.Impact.Stats [hold]: live sparks, flashes, pieces and hero rings, the density tier and its rate, hits and kills over the last second, effects refused by a cap since run start; hold prints it again on the held step of a shot"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
}

#endif // !UE_BUILD_SHIPPING
