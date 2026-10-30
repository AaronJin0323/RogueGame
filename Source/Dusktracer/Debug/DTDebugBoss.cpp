// Boss debug console commands (R-TEC-12: compiled out of Shipping; 02_Systems/boss.md Data).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Boss/DTBossSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace DTDebugBoss
{
	static UDTBossSubsystem* Find(UWorld* World)
	{
		UDTBossSubsystem* Boss = World ? World->GetSubsystem<UDTBossSubsystem>() : nullptr;
		if (!Boss)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Boss: no boss system in this world (Game/PIE worlds only)"));
		}
		return Boss;
	}

	static void Start(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTBossSubsystem* Boss = Find(World))
		{
			if (Args.Num() > 0)
			{
				UE_LOG(LogDT, Log, TEXT("DT.Boss: the form comes from wins (R-BOS-01); '%s' ignored, S1 plays form I"), *Args[0]);
			}
			Boss->StartFight();
			UE_LOG(LogDT, Log, TEXT("DT.Boss: the Warden enters at the next step"));
		}
	}

	static void Phase(const TArray<FString>& Args, UWorld* World)
	{
		UDTBossSubsystem* Boss = Args.Num() > 0 ? Find(World) : nullptr;
		if (!Boss || !Boss->GetCore().HasHull())
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Boss.Phase <n>: needs a running fight (DT.Boss)"));
			return;
		}
		Boss->ForcePhase(FCString::Atoi(*Args[0]));
		UE_LOG(LogDT, Log, TEXT("DT.Boss.Phase: now phase %d"), Boss->GetCore().GetPhase());
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		const UDTBossSubsystem* Boss = Find(World);
		if (!Boss)
		{
			return;
		}
		const FDTBossCore& Core = Boss->GetCore();
		const FDTBossCounters& C = Core.GetCounters();
		UE_LOG(LogDT, Log, TEXT("DT.Boss.Stats active=%d form=%s phase=%d/%d fight=%.1f s shift=%.2f hp_frac=%.3f dealt=%.0f effective_max=%.0f reserve=%.0f calibrated=%d hazard=%s%.0f outcome=%s"),
			Core.IsActive() ? 1 : 0, FDTBossCore::FormName(Core.GetForm()), Core.GetPhase(), FDTBossCore::NumPhases(), Core.GetFightTime(),
			Core.GetShiftLeft(), Core.GetHpFrac(), Core.GetDamageDealt(), Core.GetEffectiveMax(), Core.GetReserveHp(), Core.IsCalibrated() ? 1 : 0,
			Core.IsHazardOn() ? TEXT("r=") : TEXT("off "), Core.GetHazardRadius(), Core.GetOutcome().IsEmpty() ? TEXT("-") : *Core.GetOutcome());
		FString Attacks;
		TArray<FName> Keys;
		C.AttacksStarted.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		for (const FName& Key : Keys)
		{
			Attacks += FString::Printf(TEXT(" %s=%d"), *Key.ToString(), C.AttacksStarted[Key]);
		}
		UE_LOG(LogDT, Log, TEXT("  attacks:%s rings=%d rounds=%d refused=%d spiral=%d (skipped %d, min lane %.1f deg) hazard_hits=%d adds=%d form_fallbacks=%d"),
			*Attacks, C.RingsFired, C.RoundsFired, C.RoundsRefused, C.SpiralEmissions, C.SpiralSkipped, C.SpiralMinLaneDeg, C.HazardHits,
			C.AddsQueued, C.FormFallbacks);
		if (Core.GetRingShots().Num() > 0)
		{
			const FDTBossRingShot& Last = Core.GetRingShots().Last();
			UE_LOG(LogDT, Log, TEXT("  last ring %s: %d/%d rounds, lane %.1f deg (gap %.1f deg, chord %.0f uu, need %.0f), target bearing %.1f, cross offset %.1f"),
				*Last.AttackId.ToString(), Last.Fired, Last.Count, Last.LaneCentreDeg, Last.LaneGapDeg, Last.LaneChordUU,
				FDTBossCore::LaneChordRequired(), Last.TargetBearingDeg, Last.CrossOffsetDeg);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs StartCmd(TEXT("DT.Boss"),
		TEXT("DT.Boss [Form]: start the Warden fight now (the form follows wins, R-BOS-01; S1 plays form I)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
	static FAutoConsoleCommandWithWorldAndArgs PhaseCmd(TEXT("DT.Boss.Phase"),
		TEXT("DT.Boss.Phase <n>: jump the running fight to phase n (1-based; phases only advance)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Phase));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Boss.Stats"),
		TEXT("DT.Boss.Stats: log the fight's phase, effective HP, calibration, hazard and attack counts"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
}

#endif // !UE_BUILD_SHIPPING
