// Director debug console commands (R-TEC-12: compiled out of Shipping; 02_Systems/director.md Data).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Dusktracer.h"
#include "Director/DTDirector.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace DTDebugDirector
{
	static UDTDirector* Find(UWorld* World)
	{
		UDTDirector* Director = World ? World->GetSubsystem<UDTDirector>() : nullptr;
		if (!Director)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Dir: no director in this world (Game/PIE worlds only)"));
			return nullptr;
		}
		if (!Director->IsActive() || !Director->GetCore().IsRunning())
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Dir: the director is not running (run not started, or dt.Director.Enabled 0)"));
			return nullptr;
		}
		return Director;
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		UDTDirector* Director = Find(World);
		if (!Director)
		{
			return;
		}
		const FDTDirectorCore& Core = Director->GetCore();
		const FDTDirectorTotals& T = Core.GetTotals();
		UE_LOG(LogDT, Log, TEXT("DT.Dir.Stats t=%.1f s block=%s harvest_row=%d beat_row=%d beat=%d event_alive=%d credit=%.2f dropin_progress=%.2f waiting=%d markers=%d"),
			Core.GetTime(), FDTDirectorCore::BlockName(Core.GetBlock()), Core.GetHarvestRow(), Core.GetBeatRow(), Core.GetBeatId(),
			Core.GetEventAlive(), Core.GetCredit(), Core.GetDropInProgress(), Core.IsDropInWaiting() ? 1 : 0, Core.GetDropIns().Num());
		UE_LOG(LogDT, Log, TEXT("  run: spawned swarm=%d tank=%d rusher=%d turret=%d events=%d skipped=%d dropins=%d/%d landed (%d cancelled) beats=%d (%d cleared, %d skipped) chests=%d gems=%d kills=%d threats=%d blocks=%d boss=%d"),
			T.Spawned[0], T.Spawned[1], T.Spawned[2], T.Spawned[3], T.EventSpawned, T.SkippedAtCap, T.DropInsPublished, T.DropInsLanded,
			T.DropInsCancelled, T.Beats, T.BeatsCleared, T.BeatsSkipped, T.Chests, T.Gems, T.Kills, T.ThreatActions, T.Blocks, T.bBossRequested ? 1 : 0);
		if (const UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>())
		{
			const FDTEnemyPool& Pool = Enemies->GetPool();
			const int32 Total = FMath::Max(1, Pool.Num());
			UE_LOG(LogDT, Log, TEXT("  alive=%d shares swarm=%.2f tank=%.2f (cap %.2f) rusher=%.2f (cap %.2f) turret=%.2f (cap %.2f)"), Pool.Num(),
				Pool.CountRole(EDTEnemyRole::Swarm) / static_cast<float>(Total),
				Pool.CountRole(EDTEnemyRole::Tank) / static_cast<float>(Total), FDTDirectorCore::ShareCap(EDTEnemyRole::Tank),
				Pool.CountRole(EDTEnemyRole::Rusher) / static_cast<float>(Total), FDTDirectorCore::ShareCap(EDTEnemyRole::Rusher),
				Pool.CountRole(EDTEnemyRole::Turret) / static_cast<float>(Total), FDTDirectorCore::ShareCap(EDTEnemyRole::Turret));
		}
	}

	static void Skip(const TArray<FString>& Args, UWorld* World)
	{
		UDTDirector* Director = Args.Num() > 0 ? Find(World) : nullptr;
		if (!Director)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Dir.Skip <seconds>"));
			return;
		}
		const float Seconds = FCString::Atof(*Args[0]);
		Director->GetCore().Skip(Seconds);
		UE_LOG(LogDT, Log, TEXT("DT.Dir.Skip %.1f s: run clock now %.1f s"), Seconds, Director->GetCore().GetTime());
	}

	static void Time(const TArray<FString>& Args, UWorld* World)
	{
		UDTDirector* Director = Args.Num() > 0 ? Find(World) : nullptr;
		if (!Director)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Dir.Time <seconds>"));
			return;
		}
		const double Target = FCString::Atod(*Args[0]);
		const double Now = Director->GetCore().GetTime();
		if (Target < Now)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Dir.Time: the clock only moves forward (now %.1f s)"), Now);
			return;
		}
		Director->GetCore().Skip(static_cast<float>(Target - Now));
		UE_LOG(LogDT, Log, TEXT("DT.Dir.Time: run clock now %.1f s"), Director->GetCore().GetTime());
	}

	static void Block(const TArray<FString>& Args, UWorld* World)
	{
		UDTDirector* Director = Args.Num() > 0 ? Find(World) : nullptr;
		if (!Director)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Dir.Block <Harvest|Threat|Breath>"));
			return;
		}
		EDTDirBlock Wanted = EDTDirBlock::Harvest;
		if (Args[0].Equals(TEXT("Harvest"), ESearchCase::IgnoreCase)) { Wanted = EDTDirBlock::Harvest; }
		else if (Args[0].Equals(TEXT("Threat"), ESearchCase::IgnoreCase)) { Wanted = EDTDirBlock::Threat; }
		else if (Args[0].Equals(TEXT("Breath"), ESearchCase::IgnoreCase)) { Wanted = EDTDirBlock::Breath; }
		else
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Dir.Block <Harvest|Threat|Breath>"));
			return;
		}
		Director->GetCore().ForceBlock(Wanted);
		UE_LOG(LogDT, Log, TEXT("DT.Dir.Block %s at the next step"), FDTDirectorCore::BlockName(Wanted));
	}

	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Dir.Stats"),
		TEXT("DT.Dir.Stats: log the director's block, credit, drop-in state, live markers, run totals and alive shares"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
	static FAutoConsoleCommandWithWorldAndArgs SkipCmd(TEXT("DT.Dir.Skip"),
		TEXT("DT.Dir.Skip <seconds>: move the director's run clock forward (rows apply at the next step; passed beats are skipped)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Skip));
	static FAutoConsoleCommandWithWorldAndArgs TimeCmd(TEXT("DT.Dir.Time"),
		TEXT("DT.Dir.Time <seconds>: jump the director's run clock to an absolute time (forward only)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Time));
	static FAutoConsoleCommandWithWorldAndArgs BlockCmd(TEXT("DT.Dir.Block"),
		TEXT("DT.Dir.Block <Harvest|Threat|Breath>: force a block at the next step (Threat starts the next Threat row now)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Block));
}

#endif // !UE_BUILD_SHIPPING
