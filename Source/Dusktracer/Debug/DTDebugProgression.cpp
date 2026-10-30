// Progression debug console commands (R-TEC-12: compiled out of Shipping; 02_Systems/progression.md).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Progression/DTGemSubsystem.h"
#include "Progression/DTProgression.h"

namespace DTDebugProgression
{
	static UDTGemSubsystem* Find(UWorld* World)
	{
		UDTGemSubsystem* Gems = World ? World->GetSubsystem<UDTGemSubsystem>() : nullptr;
		if (!Gems || !World->GetSubsystem<UDTSimSubsystem>())
		{
			UE_LOG(LogDT, Warning, TEXT("DT progression: no gem/sim subsystem in this world (Game/PIE worlds only)"));
			return nullptr;
		}
		return Gems;
	}

	static void LogPlayers(UWorld* World)
	{
		const FDTRunState& Run = World->GetSubsystem<UDTSimSubsystem>()->GetRun();
		for (int32 Index = 0; Index < Run.GetNumPlayers(); ++Index)
		{
			const FDTPlayerRunState& P = Run.GetPlayer(Index);
			UE_LOG(LogDT, Log, TEXT("  player %d level=%d xp=%d/%d xp_total=%lld pending_hands=%d"), Index, P.Level, P.XP,
				P.XPToNext, P.XPTotal, P.Offer.PendingHands.Num());
		}
	}

	static void Xp(const TArray<FString>& Args, UWorld* World)
	{
		UDTGemSubsystem* Gems = Find(World);
		if (!Gems || Args.Num() < 1)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Xp <amount> [player]"));
			return;
		}
		const int64 Amount = FCString::Atoi64(*Args[0]);
		const int32 Player = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : INDEX_NONE;
		const int32 Gained = Gems->GrantXP(Player, Amount);
		UE_LOG(LogDT, Log, TEXT("DT.Xp %lld to %s: %d level(s) gained"), Amount, Player == INDEX_NONE ? TEXT("every player") : *FString::FromInt(Player), Gained);
		LogPlayers(World);
	}

	static void Level(const TArray<FString>& Args, UWorld* World)
	{
		UDTGemSubsystem* Gems = Find(World);
		if (!Gems || Args.Num() < 1)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Level <n> [player]"));
			return;
		}
		const int32 Target = FCString::Atoi(*Args[0]);
		const int32 Only = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : INDEX_NONE;
		FDTRunState& Run = World->GetSubsystem<UDTSimSubsystem>()->GetRun();
		FDTProgression::EnsureCurve(Run);
		for (int32 Index = 0; Index < Run.GetNumPlayers(); ++Index)
		{
			if (Only == INDEX_NONE || Only == Index)
			{
				// Through XP, so every level on the way queues its hand like a real run.
				Gems->GrantXP(Index, FDTProgression::XPToReach(Run.GetPlayer(Index), Target));
			}
		}
		UE_LOG(LogDT, Log, TEXT("DT.Level %d"), Target);
		LogPlayers(World);
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTGemSubsystem* Gems = Find(World))
		{
			const FDTGemPool& Pool = Gems->GetPool();
			int32 Bound = 0;
			for (const int32 Owner : Pool.GetBoundTo())
			{
				Bound += Owner != INDEX_NONE ? 1 : 0;
			}
			UE_LOG(LogDT, Log, TEXT("DT.Gems.Stats live=%d cap=%d bound=%d floor_xp=%lld chests=%d spawned=%lld merged=%lld collected=%lld chests_collected=%lld"),
				Pool.Num(), DTConst::PICKUP_GEM_CAP, Bound, Pool.GetFloorXP(), Pool.NumChests(), Pool.GetTotalSpawned(),
				Pool.GetTotalMerged(), Pool.GetTotalCollected(), Pool.GetTotalChestsCollected());
			LogPlayers(World);
		}
	}

	/** DT.Gems.Drop <gems> [chests] [radius]: pickups on a circle around the origin (captures: T-PRE-05 needs both in frame). */
	static void Drop(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTGemSubsystem* Gems = Find(World))
		{
			const int32 NumGems = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 1;
			const int32 NumChests = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 0;
			const float Radius = Args.Num() > 2 ? FCString::Atof(*Args[2]) : 900.0f;
			const int32 Total = FMath::Max(1, NumGems + NumChests);
			for (int32 Index = 0; Index < NumGems + NumChests; ++Index)
			{
				const float A = UE_TWO_PI * static_cast<float>(Index) / static_cast<float>(Total);
				const FVector2f At = FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius;
				if (Index < NumGems)
				{
					FDTGemSpawn Gem;
					Gem.Position = At;
					Gem.Value = DTConst::DROP_XP_SWARM;
					Gems->Spawn(Gem);
				}
				else
				{
					Gems->SpawnChest(At);
				}
			}
			UE_LOG(LogDT, Log, TEXT("DT.Gems.Drop %d gem(s), %d chest(s) at radius %.0f"), NumGems, NumChests, Radius);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs DropCmd(TEXT("DT.Gems.Drop"),
		TEXT("DT.Gems.Drop <gems> [chests] [radius]: drop pickups on a circle around the origin"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Drop));
	static FAutoConsoleCommandWithWorldAndArgs XpCmd(TEXT("DT.Xp"),
		TEXT("DT.Xp <amount> [player]: add XP to one player, or to every player when none is given (queues a hand per level)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Xp));
	static FAutoConsoleCommandWithWorldAndArgs LevelCmd(TEXT("DT.Level"),
		TEXT("DT.Level <n> [player]: add the XP that takes the player(s) to level n (no-op if already there)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Level));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Gems.Stats"),
		TEXT("DT.Gems.Stats: log live, bound and merged gems, floor XP, chests, and each player's level and pending hands"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
}

#endif // !UE_BUILD_SHIPPING
