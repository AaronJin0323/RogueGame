#include "Player/DTHeroSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "Player/DTHero.h"

bool UDTHeroSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTHeroSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	InputPhase.Latch = &Latch;
	InputPhase.Hero = &Hero;
}

void UDTHeroSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSystem = Sim.Get())
	{
		SimSystem->Unregister(this);
		SimSystem->Unregister(&InputPhase);
	}
	InputPhase.Latch = nullptr;
	InputPhase.Hero = nullptr;
	Pawns.Reset();
	Super::Deinitialize();
}

void UDTHeroSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (!Sim.IsValid())
	{
		Sim = InWorld.GetSubsystem<UDTSimSubsystem>();
	}
	UDTSimSubsystem* SimSystem = Sim.Get();
	if (!SimSystem)
	{
		UE_LOG(LogDT, Error, TEXT("UDTHeroSubsystem: no UDTSimSubsystem in this world; the hero will not step"));
		return;
	}
	SimSystem->Register(EDTSimPhase::Input, &InputPhase);
	SimSystem->Register(EDTSimPhase::Hero, this);
	Hero.Reset(SimSystem->GetRun().GetNumPlayers());
}

void UDTHeroSubsystem::SimStep(const FDTSimContext& Context)
{
	if (Context.StepIndex == 0)
	{
		bRunOverBroadcast = false;
	}
	Hero.Step(Context);
	for (const FDTHeroEvent& Event : Hero.GetStepEvents())
	{
		OnHeroEvent.Broadcast(Event);
		if (Event.Kind == EDTHeroEventKind::Intercession)
		{
			OnIntercession.Broadcast(Event.OwnerId, Event.Path);
		}
	}
	if (Hero.IsRunOver() && !bRunOverBroadcast)
	{
		bRunOverBroadcast = true;
		UE_LOG(LogDT, Log, TEXT("Hero: every player is down at step %lld; run over (Death)"), Hero.GetRunEndStep());
		OnAllHeroesDown.Broadcast(Hero.GetRunEndStep());
	}
}

void UDTHeroSubsystem::SimRender(float Alpha)
{
	RenderAlpha = Alpha;
	for (int32 Index = Pawns.Num() - 1; Index >= 0; --Index)
	{
		ADTHero* Pawn = Pawns[Index].Get();
		if (!Pawn)
		{
			Pawns.RemoveAtSwap(Index);
			continue;
		}
		if (const FDTHeroState* State = GetState(Pawn->GetPlayerIndex()))
		{
			Pawn->DrawFromSim(*State, Alpha);
		}
	}
}

void UDTHeroSubsystem::RegisterPawn(ADTHero* Pawn)
{
	if (Pawn)
	{
		Pawns.AddUnique(Pawn);
	}
}

void UDTHeroSubsystem::UnregisterPawn(ADTHero* Pawn)
{
	Pawns.Remove(Pawn);
}
