#include "Meta/DTRunRecorder.h"
#include "Boss/DTBossSubsystem.h"
#include "Core/DTRunState.h"
#include "Engine/World.h"
#include "Meta/DTMetaSubsystem.h"
#include "Offers/DTOfferGenerator.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"

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
	}
	bFrozen = bEndPending;
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
	}
	const EDTRunEnd End = EndFromReason(Reason);
	FDTMetaSession* Session = UDTMetaSubsystem::FindSession(this);
	FDTMetaSession NoSave; // a world with no game instance (tests): the award is still the run's
	Award = (Session ? *Session : NoSave).Bank(Tally, End);
	if (Award.bBanked)
	{
		if (UDTTelemetrySubsystem* TelemetrySystem = Telemetry.Get())
		{
			TelemetrySystem->Records().RecordRunAward(EndName(End), Award.Retained, Tally.Steps, Tally.Kills, Tally.Elites, Tally.bBossKilled, Tally.Heat);
		}
	}
}
