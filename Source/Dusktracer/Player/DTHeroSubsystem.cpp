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
	// R-BOS-13 (D-041): while the Warden's hull stands the boss system ends the run, after the step's kills are read.
	const bool bRunWon = Context.View && Context.View->bRunWon;
	if (!bRunWon && !bHoldRunEnd)
	{
		AnnounceAllDown();
	}
}

void UDTHeroSubsystem::Heal(int32 OwnerId, float Amount, const TCHAR* Source)
{
	const FDTHeroState* State = GetState(OwnerId);
	if (!State || State->bDown)
	{
		return;
	}
	FDTHeroEvent Event;
	Event.Kind = EDTHeroEventKind::HeroHealed;
	Event.OwnerId = OwnerId;
	Event.Amount = Hero.Heal(OwnerId, Amount);
	Event.HpAfter = State->Hp;
	Event.MaxHp = State->MaxHp;
	Event.HealSource = FName(Source);
	OnHeroEvent.Broadcast(Event);
}

void UDTHeroSubsystem::AnnounceAllDown()
{
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

const UDTInputSet* UDTHeroSubsystem::GetInputSet(int32 OwnerId) const
{
	for (const TWeakObjectPtr<ADTHero>& Entry : Pawns)
	{
		const ADTHero* const Pawn = Entry.Get();
		if (Pawn && Pawn->GetPlayerIndex() == OwnerId)
		{
			return Pawn->GetInputSet();
		}
	}
	return nullptr;
}
