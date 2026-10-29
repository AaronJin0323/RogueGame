// Enemy debug console commands (R-TEC-12: compiled out of Shipping).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Enemies/DTEnemySubsystem.h"

namespace DTDebugEnemies
{
	static UDTEnemySubsystem* Find(UWorld* World)
	{
		UDTEnemySubsystem* Enemies = World ? World->GetSubsystem<UDTEnemySubsystem>() : nullptr;
		if (!Enemies)
		{
			UE_LOG(LogDT, Warning, TEXT("DT enemies: no enemy subsystem in this world (Game/PIE worlds only)"));
		}
		return Enemies;
	}

	static int32 ParseCount(const TArray<FString>& Args, int32 Default)
	{
		return Args.Num() > 0 ? FCString::Atoi(*Args[0]) : Default;
	}

	static void Spawn(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			const int32 Count = ParseCount(Args, 1);
			Enemies->RequestSpawn(Count);
			UE_LOG(LogDT, Log, TEXT("DT.Spawn %d queued (live %d, cap %d)"), Count, Enemies->GetLiveCount(), DTConst::ENEMIES_CAP);
		}
	}

	static void Keep(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			Enemies->SetKeepPopulation(ParseCount(Args, DTConst::ENEMIES_CAP));
			UE_LOG(LogDT, Log, TEXT("DT.Spawn.Keep %d"), Enemies->GetKeepPopulation());
		}
	}

	static void Clear(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			Enemies->RequestClear();
			UE_LOG(LogDT, Log, TEXT("DT.Spawn.Clear queued"));
		}
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			UE_LOG(LogDT, Log, TEXT("DT.Enemies.Stats live=%d cap=%d keep=%d kills=%lld kills_per_s=%.1f refused=%lld"),
				Enemies->GetLiveCount(), DTConst::ENEMIES_CAP, Enemies->GetKeepPopulation(), Enemies->GetTotalKills(),
				Enemies->GetKillsPerSecond(), Enemies->GetRefusedSpawns());
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs SpawnCmd(TEXT("DT.Spawn"),
		TEXT("DT.Spawn <N>: spawn N swarm enemies on a ring around hero 0 (capped at C.ENEMIES.CAP)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Spawn));
	static FAutoConsoleCommandWithWorldAndArgs KeepCmd(TEXT("DT.Spawn.Keep"),
		TEXT("DT.Spawn.Keep <N>: keep the swarm population topped up to N (0 = off; default the cap)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Keep));
	static FAutoConsoleCommandWithWorldAndArgs ClearCmd(TEXT("DT.Spawn.Clear"),
		TEXT("DT.Spawn.Clear: remove every enemy and stop DT.Spawn.Keep"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Clear));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Enemies.Stats"),
		TEXT("DT.Enemies.Stats: log live count, kills, kills per second and refused spawns"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
}

#endif // !UE_BUILD_SHIPPING
