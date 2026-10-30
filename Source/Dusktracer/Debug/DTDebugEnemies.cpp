// Enemy debug console commands (R-TEC-12: compiled out of Shipping). Requests apply at the start of the next Enemies phase.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
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

	static int32 ParseCount(const TArray<FString>& Args, int32 Index, int32 Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atoi(*Args[Index]) : Default;
	}

	static const TCHAR* RoleName(EDTEnemyRole Role)
	{
		switch (Role)
		{
		case EDTEnemyRole::Swarm: return TEXT("Swarm");
		case EDTEnemyRole::Tank: return TEXT("Tank");
		case EDTEnemyRole::Rusher: return TEXT("Rusher");
		case EDTEnemyRole::Turret: return TEXT("Turret");
		case EDTEnemyRole::Boss: return TEXT("Boss");
		default: return TEXT("?");
		}
	}

	/** Accepts the role key with or without the tag root: "Rusher", "Enemy.Rusher" (case-insensitive). */
	static bool ParseRole(const FString& Text, EDTEnemyRole& Out)
	{
		FString Key = Text;
		Key.RemoveFromStart(TEXT("Enemy."), ESearchCase::IgnoreCase);
		for (int32 Role = 0; Role < static_cast<int32>(EDTEnemyRole::Count); ++Role)
		{
			if (Key.Equals(RoleName(static_cast<EDTEnemyRole>(Role)), ESearchCase::IgnoreCase))
			{
				Out = static_cast<EDTEnemyRole>(Role);
				return true;
			}
		}
		return false;
	}

	static bool ParseStatus(const FString& Text, EDTStatus& Out)
	{
		static const TCHAR* Names[] = { TEXT("Shock"), TEXT("Burn"), TEXT("Chill"), TEXT("Freeze") };
		static_assert(UE_ARRAY_COUNT(Names) == static_cast<int32>(EDTStatus::Count), "one name per status");
		FString Key = Text;
		Key.RemoveFromStart(TEXT("Status."), ESearchCase::IgnoreCase);
		for (int32 Kind = 0; Kind < static_cast<int32>(EDTStatus::Count); ++Kind)
		{
			if (Key.Equals(Names[Kind], ESearchCase::IgnoreCase))
			{
				Out = static_cast<EDTStatus>(Kind);
				return true;
			}
		}
		return false;
	}

	static bool HasFlag(const TArray<FString>& Args, int32 From, const TCHAR* Flag)
	{
		for (int32 Index = From; Index < Args.Num(); ++Index)
		{
			if (Args[Index].Equals(Flag, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	static void QueueRole(UDTEnemySubsystem* Enemies, EDTEnemyRole Role, int32 Count, bool bElite, bool bArrival)
	{
		if (Role == EDTEnemyRole::Boss)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Spawn: the boss hull is spawned by the boss system (DT.Boss), not here"));
			return;
		}
		Enemies->RequestSpawnRole(Role, Count, bElite, bArrival);
		UE_LOG(LogDT, Log, TEXT("DT.Spawn %s x%d%s%s queued (live %d, cap %d)"), RoleName(Role), Count, bElite ? TEXT(" elite") : TEXT(""),
			bArrival ? TEXT(" arrival") : TEXT(""), Enemies->GetLiveCount(), DTConst::ENEMIES_CAP);
	}

	/** DT.Spawn <N> [Role] [elite] [arrival] */
	static void Spawn(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			const int32 Count = ParseCount(Args, 0, 1);
			EDTEnemyRole Role = EDTEnemyRole::Swarm;
			if (Args.IsValidIndex(1) && ParseRole(Args[1], Role))
			{
				QueueRole(Enemies, Role, Count, HasFlag(Args, 2, TEXT("elite")), HasFlag(Args, 2, TEXT("arrival")));
				return;
			}
			Enemies->RequestSpawn(Count);
			UE_LOG(LogDT, Log, TEXT("DT.Spawn %d queued (live %d, cap %d)"), Count, Enemies->GetLiveCount(), DTConst::ENEMIES_CAP);
		}
	}

	/** DT.Spawn.Role <Role> [N] [elite] [arrival] */
	static void SpawnRole(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			EDTEnemyRole Role = EDTEnemyRole::Swarm;
			if (!Args.IsValidIndex(0) || !ParseRole(Args[0], Role))
			{
				UE_LOG(LogDT, Warning, TEXT("DT.Spawn.Role <Swarm|Tank|Rusher|Turret> [N] [elite] [arrival]"));
				return;
			}
			QueueRole(Enemies, Role, ParseCount(Args, 1, 1), HasFlag(Args, 1, TEXT("elite")), HasFlag(Args, 1, TEXT("arrival")));
		}
	}

	static void Keep(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			Enemies->SetKeepPopulation(ParseCount(Args, 0, DTConst::ENEMIES_CAP));
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
			const FDTEnemyPool& Pool = Enemies->GetPool();
			const FDTEnemyCounters& C = Pool.GetCounters();
			UE_LOG(LogDT, Log, TEXT("DT.Enemies.Stats live=%d (swarm=%d tank=%d rusher=%d turret=%d boss=%d) cap=%d keep=%d kills=%lld kills_per_s=%.1f refused=%lld stale_hits=%lld"),
				Enemies->GetLiveCount(), Pool.CountRole(EDTEnemyRole::Swarm), Pool.CountRole(EDTEnemyRole::Tank), Pool.CountRole(EDTEnemyRole::Rusher),
				Pool.CountRole(EDTEnemyRole::Turret), Pool.CountRole(EDTEnemyRole::Boss), DTConst::ENEMIES_CAP, Enemies->GetKeepPopulation(),
				Enemies->GetTotalKills(), Enemies->GetKillsPerSecond(), Enemies->GetRefusedSpawns(), C.StaleHitsIgnored);
			UE_LOG(LogDT, Log, TEXT("DT.Enemies.Stats statuses shock=%lld burn=%lld chill=%lld freeze=%lld ignored=%lld; threat actions rusher=%lld turret=%lld"),
				C.StatusesApplied[0], C.StatusesApplied[1], C.StatusesApplied[2], C.StatusesApplied[3], C.StatusesIgnored,
				C.ThreatActions[static_cast<int32>(EDTEnemyRole::Rusher)], C.ThreatActions[static_cast<int32>(EDTEnemyRole::Turret)]);
		}
	}

	/** DT.EnemyFire [0|1]: no argument toggles. */
	static void Fire(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			const bool bOn = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) != 0 : !Enemies->IsEnemyFireEnabled();
			Enemies->SetEnemyFireEnabled(bOn);
			UE_LOG(LogDT, Log, TEXT("DT.EnemyFire %s"), bOn ? TEXT("on") : TEXT("off"));
		}
	}

	static void FireStats(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			const FDTEnemyBulletPool& Rounds = Enemies->GetPool().GetRounds();
			const FDTEnemyRoundCounters& C = Rounds.GetCounters();
			UE_LOG(LogDT, Log, TEXT("DT.EnemyFire.Stats fire=%s live=%d cap=%d spawned=%lld refused=%lld hero_hits=%lld passed_through=%lld expired=%lld out=%lld token_waits=%lld"),
				Enemies->IsEnemyFireEnabled() ? TEXT("on") : TEXT("off"), Rounds.Num(), Rounds.GetCapacity(), C.Spawned, C.Refused, C.HeroHits,
				C.PassedThrough, C.Expired, C.OutOfBounds, Enemies->GetPool().GetCounters().TokenWaits);
		}
	}

	/** DT.EnemyFire.Ring [N] [radius_uu] [speed_ratio]: captures of enemy rounds without an attack in frame; they drift outward, away from a hero at the origin. */
	static void FireRing(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			const int32 Count = ParseCount(Args, 0, 24);
			const float Radius = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 600.0f;
			const float Speed = (Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : 0.1f) * static_cast<float>(DTConst::ENEMY_FIRE_SHOT_SPEED_UU_S);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const float Angle = UE_TWO_PI * Index / FMath::Max(1, Count);
				const FVector2f Dir(FMath::Cos(Angle), FMath::Sin(Angle));
				FDTEnemyRoundSpawn Spawn;
				Spawn.Position = Dir * Radius;
				Spawn.Velocity = Dir * Speed;
				Spawn.Radius = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
				Spawn.Damage = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_DAMAGE);
				Spawn.Life = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_LIFE_S);
				Spawn.TargetPlayer = 0;
				Spawn.AttackId = FName(TEXT("Turret.Fire"));
				Enemies->GetPool().GetRounds().Spawn(Spawn);
			}
			UE_LOG(LogDT, Log, TEXT("DT.EnemyFire.Ring %d rounds at %.0f uu"), Count, Radius);
		}
	}

	/** DT.Status <Shock|Burn|Chill|Freeze> [N] [Magnitude] [delay_s]: N = 0 or absent applies to every enemy. */
	static void Status(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTEnemySubsystem* Enemies = Find(World))
		{
			EDTStatus Which = EDTStatus::Shock;
			if (!Args.IsValidIndex(0) || !ParseStatus(Args[0], Which))
			{
				UE_LOG(LogDT, Warning, TEXT("DT.Status <Shock|Burn|Chill|Freeze> [N] [Magnitude]"));
				return;
			}
			const int32 Count = ParseCount(Args, 1, 0);
			// Default magnitude: one Chill stack; a Burn of the swarm's HP per second (debug only, not a tuned value).
			const float Default = Which == EDTStatus::Burn ? static_cast<float>(DTConst::ENM_SWARM_HP) : 1.0f;
			const float Magnitude = Args.IsValidIndex(2) ? FCString::Atof(*Args[2]) : Default;
			const float Delay = Args.IsValidIndex(3) ? FCString::Atof(*Args[3]) : 0.0f;
			if (Delay > 0.0f)
			{
				// Captures and scripted runs: apply after Delay wall seconds (-ExecCmds run before anything has spawned).
				TWeakObjectPtr<UDTEnemySubsystem> Weak(Enemies);
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak, Which, Count, Magnitude](float)
				{
					if (UDTEnemySubsystem* Later = Weak.Get())
					{
						Later->RequestStatus(Which, Count, Magnitude);
					}
					return false;
				}), Delay);
			}
			else
			{
				Enemies->RequestStatus(Which, Count, Magnitude);
			}
			UE_LOG(LogDT, Log, TEXT("DT.Status %s x%d magnitude %.2f queued"), *Args[0], Count, Magnitude);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs SpawnCmd(TEXT("DT.Spawn"),
		TEXT("DT.Spawn <N> [Role] [elite] [arrival]: spawn N enemies (default swarm) on a ring around hero 0 (capped at C.ENEMIES.CAP)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Spawn));
	static FAutoConsoleCommandWithWorldAndArgs SpawnRoleCmd(TEXT("DT.Spawn.Role"),
		TEXT("DT.Spawn.Role <Swarm|Tank|Rusher|Turret> [N] [elite] [arrival]: spawn N enemies of a role on a ring around hero 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SpawnRole));
	static FAutoConsoleCommandWithWorldAndArgs KeepCmd(TEXT("DT.Spawn.Keep"),
		TEXT("DT.Spawn.Keep <N>: keep the swarm population topped up to N (0 = off; default the cap)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Keep));
	static FAutoConsoleCommandWithWorldAndArgs ClearCmd(TEXT("DT.Spawn.Clear"),
		TEXT("DT.Spawn.Clear: remove every enemy and enemy round and stop DT.Spawn.Keep"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Clear));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Enemies.Stats"),
		TEXT("DT.Enemies.Stats: log live count by role, kills, kills per second, refused spawns, stale hits, statuses and threat actions"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
	static FAutoConsoleCommandWithWorldAndArgs FireCmd(TEXT("DT.EnemyFire"),
		TEXT("DT.EnemyFire [0|1]: turn enemy fire off or on (no argument toggles); rounds in flight keep flying"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Fire));
	static FAutoConsoleCommandWithWorldAndArgs FireStatsCmd(TEXT("DT.EnemyFire.Stats"),
		TEXT("DT.EnemyFire.Stats: log live rounds, cap, spawned, refused, hero hits, passed through, expired, token waits"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FireStats));
	static FAutoConsoleCommandWithWorldAndArgs FireRingCmd(TEXT("DT.EnemyFire.Ring"),
		TEXT("DT.EnemyFire.Ring [N] [radius_uu] [speed_ratio]: spawn a ring of enemy rounds flying outward (captures of round readability)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&FireRing));
	static FAutoConsoleCommandWithWorldAndArgs StatusCmd(TEXT("DT.Status"),
		TEXT("DT.Status <Shock|Burn|Chill|Freeze> [N] [Magnitude] [delay_s]: apply a status to the first N enemies (0 = all) as player 0"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Status));
}

#endif // !UE_BUILD_SHIPPING
