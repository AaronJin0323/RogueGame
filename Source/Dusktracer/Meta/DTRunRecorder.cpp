#include "Meta/DTRunRecorder.h"
#include "Boss/DTBossSubsystem.h"
#include "Core/DTRunState.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Meta/DTDiscovery.h"
#include "Meta/DTMetaSubsystem.h"
#include "Offers/DTOfferGenerator.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"

namespace DTRunRecorderLocal
{
	static const TCHAR* const RoleNames[] = { TEXT("Swarm"), TEXT("Tank"), TEXT("Rusher"), TEXT("Turret"), TEXT("Boss") };
	static_assert(static_cast<int32>(UE_ARRAY_COUNT(RoleNames)) == static_cast<int32>(EDTEnemyRole::Count), "one name per EDTEnemyRole");
	static const TCHAR* const WeaponPrefix = TEXT("Weapon.");
}

bool UDTRunRecorder::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTRunRecorder::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	Telemetry = Collection.InitializeDependency<UDTTelemetrySubsystem>();
}

void UDTRunRecorder::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Register(EDTSimPhase::Director, this);
	}
	if (UDTTelemetrySubsystem* TelemetrySystem = Telemetry.Get())
	{
		SessionHandle = TelemetrySystem->OnSessionEnding.AddUObject(this, &UDTRunRecorder::HandleSessionEnding);
	}
	if (UDTHeroSubsystem* HeroSystem = InWorld.GetSubsystem<UDTHeroSubsystem>())
	{
		Heroes = HeroSystem;
		AllDownHandle = HeroSystem->OnAllHeroesDown.AddUObject(this, &UDTRunRecorder::HandleRunEnd);
	}
	if (UDTBossSubsystem* BossSystem = InWorld.GetSubsystem<UDTBossSubsystem>())
	{
		Boss = BossSystem;
		BossHandle = BossSystem->OnBossDefeated.AddUObject(this, &UDTRunRecorder::HandleRunEnd);
	}
	Enemies = InWorld.GetSubsystem<UDTEnemySubsystem>();
}

void UDTRunRecorder::Deinitialize()
{
	if (UDTTelemetrySubsystem* TelemetrySystem = Telemetry.Get())
	{
		TelemetrySystem->OnSessionEnding.Remove(SessionHandle);
	}
	if (UDTHeroSubsystem* HeroSystem = Heroes.Get())
	{
		HeroSystem->OnAllHeroesDown.Remove(AllDownHandle);
	}
	if (UDTBossSubsystem* BossSystem = Boss.Get())
	{
		BossSystem->OnBossDefeated.Remove(BossHandle);
	}
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
	}
	Super::Deinitialize();
}

void UDTRunRecorder::HandleRunEnd(int64 StepIndex)
{
	bEndPending = true; // both fire inside a step: this step's kills still count
}

void UDTRunRecorder::SimStep(const FDTSimContext& Context)
{
	if (bFrozen || bClosed || !Context.View)
	{
		return;
	}
	++Tally.Steps;
	for (const FDTKill& Kill : Context.View->Kills)
	{
		++Tally.Kills;
		Tally.Elites += Kill.bElite ? 1 : 0;
		Tally.bBossKilled |= Kill.Role == EDTEnemyRole::Boss;
		if (Context.Run)
		{
			CountKill(Kill, *Context.Run);
		}
	}
	if (bEndPending && Context.Run)
	{
		TakeCounters(*Context.Run);
	}
	bFrozen = bEndPending;
}

void UDTRunRecorder::CountKill(const FDTKill& Kill, const FDTRunState& Run)
{
	++KillsByRole[static_cast<int32>(Kill.Role)];
	if (!Run.IsValidPlayer(Kill.OwnerId))
	{
		return;
	}
	const TArray<FDTWeaponInstance>& Weapons = Run.GetPlayer(Kill.OwnerId).Weapons;
	if (Weapons.IsValidIndex(Kill.WeaponSlot)) // a Keeper's own kill names no weapon
	{
		const FDTWeaponInstance& Weapon = Weapons[Kill.WeaponSlot];
		++KillsByWeapon.FindOrAdd(Weapon.Id);
		BestEliteKillMods = Kill.bElite ? FMath::Max<int64>(BestEliteKillMods, Weapon.Sockets.Num()) : BestEliteKillMods;
	}
}

void UDTRunRecorder::KeepBest(const TCHAR* Key, int64 Value)
{
	int64& Best = Tally.Counters.FindOrAdd(FName(Key));
	Best = FMath::Max(Best, Value);
}

void UDTRunRecorder::TakeCounters(const FDTRunState& Run)
{
	using namespace DTRunRecorderLocal;
	if (bCountersTaken)
	{
		return;
	}
	bCountersTaken = true;
	for (int32 Player = 0; Player < Run.GetNumPlayers(); ++Player)
	{
		const FDTPlayerRunState& State = Run.GetPlayer(Player);
		KeepBest(TEXT("Best.Level"), State.Level);
		for (const FDTWeaponInstance& Weapon : State.Weapons)
		{
			KeepBest(TEXT("Best.WeaponLevel"), Weapon.Level);
		}
		int32 AtTier2 = 0;
		for (const int32 Tier : State.Keepers.Tier)
		{
			KeepBest(TEXT("Best.Tier"), Tier);
			AtTier2 += Tier >= 2 ? 1 : 0;
		}
		KeepBest(TEXT("Best.PathsAtTier2"), AtTier2);
		if (const UDTHeroSubsystem* HeroSystem = Heroes.Get())
		{
			if (const FDTHeroState* Hero = HeroSystem->GetState(Player))
			{
				AddCounter(FName(TEXT("Dashes")), Hero->Dashes);
			}
		}
	}
	KeepBest(TEXT("Best.EliteKillMods"), BestEliteKillMods);
	if (const UDTEnemySubsystem* EnemySystem = Enemies.Get())
	{
		const FDTEnemyCounters& Counters = EnemySystem->GetPool().GetCounters();
		AddCounter(FName(TEXT("Status.Freeze")), Counters.FreezesBegun);
		KeepBest(TEXT("Best.Crit"), FMath::FloorToInt64(Counters.BestCrit));
	}
	for (int32 Role = 0; Role < static_cast<int32>(EDTEnemyRole::Count); ++Role)
	{
		if (KillsByRole[Role] > 0)
		{
			AddCounter(FName(FString::Printf(TEXT("Kills.Role.%s"), RoleNames[Role])), KillsByRole[Role]);
		}
	}
	for (const TPair<FName, int64>& Pair : KillsByWeapon)
	{
		AddCounter(FName(TEXT("Kills.Weapon.") + Pair.Key.ToString().RightChop(FCString::Strlen(WeaponPrefix))), Pair.Value);
	}
}

EDTRunEnd UDTRunRecorder::EndFromReason(const FString& Reason)
{
	if (Reason == TEXT("victory"))
	{
		return EDTRunEnd::Victory;
	}
	return Reason == TEXT("death") || Reason == TEXT("abandoned") ? EDTRunEnd::Death : EDTRunEnd::Left;
}

const TCHAR* UDTRunRecorder::EndName(EDTRunEnd End)
{
	return End == EDTRunEnd::Victory ? TEXT("victory") : (End == EDTRunEnd::Death ? TEXT("death") : TEXT("left"));
}

void UDTRunRecorder::LogDiscoveries() const
{
	TArray<FString> Counters;
	for (const TPair<FName, int64>& Counter : Tally.Counters)
	{
		Counters.Add(FString::Printf(TEXT("%s=%lld"), *Counter.Key.ToString(), Counter.Value));
	}
	Counters.Sort();
	const TArray<FName> Met = DTDiscovery::MetBy(Tally);
	const FString MetList = FString::JoinBy(Met, TEXT(" "), [](const FName& Id) { return Id.ToString(); });
	const FString NewList = FString::JoinBy(Award.Discovered, TEXT(" "), [](const FName& Id) { return Id.ToString(); });
	UE_LOG(LogDT, Log, TEXT("Meta: counters %s"), *FString::Join(Counters, TEXT(" ")));
	UE_LOG(LogDT, Log, TEXT("Meta: discoveries met %d of %d [%s], new [%s]"), Met.Num(), DTCat::NumDiscoveries, *MetList, *NewList);
}

void UDTRunRecorder::HandleSessionEnding(const FString& Reason)
{
	if (bClosed)
	{
		return;
	}
	bClosed = true;
	UDTSimSubsystem* SimSystem = Sim.Get();
	if (SimSystem && SimSystem->GetRun().GetNumPlayers() > 0)
	{
		const EDTPath Patron = SimSystem->GetRun().GetPlayer(0).Offer.PatronPath;
		Tally.Patron = Patron == EDTPath::None ? FName() : FName(DTOffers::PathName(Patron));
		if (Tally.Steps > 0)
		{
			TakeCounters(SimSystem->GetRun()); // a run closed with no end of its own: a restart, a quit, a tool
		}
	}
	const EDTRunEnd End = EndFromReason(Reason);
	FDTMetaSession* Session = UDTMetaSubsystem::FindSession(this);
	FDTMetaSession NoSave; // a world with no game instance (tests): the award is still the run's
	Award = (Session ? *Session : NoSave).Bank(Tally, End);
	if (Award.bBanked)
	{
		LogDiscoveries();
		if (UDTTelemetrySubsystem* TelemetrySystem = Telemetry.Get())
		{
			TelemetrySystem->Records().RecordRunAward(EndName(End), Award.Retained, Tally.Steps, Tally.Kills, Tally.Elites, Tally.bBossKilled, Tally.Heat);
		}
	}
}
