// DT.Bullets <N> / DT.Bullets.Stats: bullet stress emitters for the stress arena (02_Systems/bullets.md). Not in Shipping (R-TEC-12).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Combat/DTBulletPool.h"
#include "Combat/DTBulletSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Telemetry/DTTelemetrySources.h"
#include "UObject/WeakObjectPtr.h"

namespace DTDebugBullets
{
	/** A rotating ring at the origin firing outward (owner 0) at the rate that keeps about Target bullets alive. */
	class FStressEmitter : public IDTSimSystem
	{
	public:
		TWeakObjectPtr<UDTBulletSubsystem> Bullets;
		int32 Target = 0;

		virtual void SimStep(const FDTSimContext& Context) override
		{
			UDTBulletSubsystem* Subsystem = Bullets.Get();
			if (!Subsystem || Target <= 0)
			{
				return;
			}
			FDTBulletPool& Pool = Subsystem->GetPool();
			const float Life = DTConst::BULLETS_STRESS_LIFE_S;
			Carry += static_cast<float>(Target) / Life * Context.StepSeconds;
			int32 Count = FMath::FloorToInt32(Carry);
			Carry -= static_cast<float>(Count);
			Count = FMath::Min(Count, FMath::Max(0, Target - Pool.Num()));

			FDTBulletSpawn Spawn;
			Spawn.Radius = static_cast<float>(DTConst::BULLETS_STRESS_RADIUS_UU);
			Spawn.Damage = DTConst::BULLETS_STRESS_DAMAGE;
			Spawn.Life = Life;
			Spawn.OwnerId = 0;
			Spawn.Pierce = DTConst::BULLETS_STRESS_PIERCE;
			Spawn.SourceId = FDTTelemetrySources::Stress;
			const float Speed = static_cast<float>(DTConst::BULLETS_STRESS_SPEED_UU_S);
			const float TwoPi = static_cast<float>(UE_TWO_PI);
			for (int32 Shot = 0; Shot < Count; ++Shot)
			{
				const float Angle = Phase + TwoPi * static_cast<float>(Shot) / static_cast<float>(Count);
				Spawn.Velocity = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Speed;
				Pool.Spawn(Spawn);
			}
			Phase = FMath::Fmod(Phase + FMath::DegreesToRadians(static_cast<float>(DTConst::BULLETS_STRESS_SPIN_DEG_S)) * Context.StepSeconds, TwoPi);
		}

	private:
		float Carry = 0.0f;
		float Phase = 0.0f;
	};

	static TMap<TWeakObjectPtr<UWorld>, TUniquePtr<FStressEmitter>>& Emitters()
	{
		static TMap<TWeakObjectPtr<UWorld>, TUniquePtr<FStressEmitter>> Map;
		return Map;
	}

	static void Stop(UWorld* World)
	{
		TUniquePtr<FStressEmitter> Emitter;
		if (!Emitters().RemoveAndCopyValue(World, Emitter))
		{
			return;
		}
		if (UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>())
		{
			Sim->Unregister(Emitter.Get());
		}
	}

	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		Stop(World);
	}

	static void LogStats(UWorld* World)
	{
		const UDTBulletSubsystem* Subsystem = World ? World->GetSubsystem<UDTBulletSubsystem>() : nullptr;
		if (!Subsystem)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Bullets: no bullet subsystem in this world (Game/PIE only)"));
			return;
		}
		const TUniquePtr<FStressEmitter>* Emitter = Emitters().Find(World);
		const FDTBulletCounters& C = Subsystem->GetPool().GetCounters();
		UE_LOG(LogDT, Log, TEXT("DT.Bullets live=%d/%d target=%d spawned=%lld recycled=%lld expired=%lld out=%lld spent=%lld hits=%lld"),
			Subsystem->GetPool().Num(), Subsystem->GetPool().GetCapacity(), Emitter ? (*Emitter)->Target : 0,
			C.Spawned, C.Recycled, C.Expired, C.OutOfBounds, C.Spent, C.Hits);
	}

	static void SetTarget(const TArray<FString>& Args, UWorld* World)
	{
		UDTBulletSubsystem* Subsystem = World ? World->GetSubsystem<UDTBulletSubsystem>() : nullptr;
		UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
		if (!Subsystem || !Sim)
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Bullets: no bullet/sim subsystem in this world (Game/PIE only)"));
			return;
		}
		const int32 Target = Args.Num() > 0 ? FMath::Clamp(FCString::Atoi(*Args[0]), 0, Subsystem->GetPool().GetCapacity()) : 0;
		if (Target == 0)
		{
			Stop(World);
			UE_LOG(LogDT, Log, TEXT("DT.Bullets: stress stopped"));
			LogStats(World);
			return;
		}

		static bool bCleanupBound = false;
		if (!bCleanupBound)
		{
			FWorldDelegates::OnWorldCleanup.AddStatic(&OnWorldCleanup);
			bCleanupBound = true;
		}
		TUniquePtr<FStressEmitter>& Emitter = Emitters().FindOrAdd(World);
		if (!Emitter)
		{
			Emitter = MakeUnique<FStressEmitter>();
			Emitter->Bullets = Subsystem;
			Sim->Register(EDTSimPhase::Weapons, Emitter.Get());
		}
		Emitter->Target = Target;
		UE_LOG(LogDT, Log, TEXT("DT.Bullets: stress target %d"), Target);
	}

	static FAutoConsoleCommandWithWorldAndArgs BulletsCommand(
		TEXT("DT.Bullets"),
		TEXT("DT.Bullets <N>: keep about N player bullets alive with stress emitters at the origin (capped at C.BULLETS.CAP); 0 stops."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetTarget));

	static FAutoConsoleCommandWithWorld StatsCommand(
		TEXT("DT.Bullets.Stats"),
		TEXT("Logs the live bullet count and the spawn/recycle/expire/hit counters."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&LogStats));
}

#endif // !UE_BUILD_SHIPPING
