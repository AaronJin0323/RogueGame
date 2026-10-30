#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"

bool UDTSimSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTSimSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (Run.GetNumPlayers() == 0)
	{
		StartRun(0, 1);
	}
}

TStatId UDTSimSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDTSimSubsystem, STATGROUP_Tickables);
}

void UDTSimSubsystem::StartRun(int32 RunSeed, int32 NumPlayers)
{
	Run.Start(RunSeed, NumPlayers);
	View = FDTWorldView();
	View.HeroPositions.Init(FVector2f::ZeroVector, Run.GetNumPlayers());
	View.HeroAimDirs.Init(FVector2f(1.0f, 0.0f), Run.GetNumPlayers());
	View.HeroIntangible.Init(false, Run.GetNumPlayers());
	View.HeroHurt.Init(false, Run.GetNumPlayers());
	StepIndex = 0;
	UE_LOG(LogDT, Log, TEXT("Run start seed=%d players=%d"), RunSeed, Run.GetNumPlayers());
}

void UDTSimSubsystem::Register(EDTSimPhase Phase, IDTSimSystem* System)
{
	check(Phase < EDTSimPhase::Count && System);
	Systems[static_cast<int32>(Phase)].AddUnique(System);
}

void UDTSimSubsystem::Unregister(IDTSimSystem* System)
{
	for (TArray<IDTSimSystem*>& PhaseSystems : Systems)
	{
		PhaseSystems.Remove(System);
	}
}

void UDTSimSubsystem::RunOneStep()
{
	FDTSimContext Context;
	Context.StepSeconds = static_cast<float>(Clock.GetStepSeconds());
	Context.StepIndex = StepIndex;
	Context.Run = &Run;
	Context.View = &View;
	for (const TArray<IDTSimSystem*>& PhaseSystems : Systems)
	{
		for (IDTSimSystem* System : PhaseSystems)
		{
			System->SimStep(Context);
		}
	}
	++StepIndex;
}

void UDTSimSubsystem::StepNow(int32 Steps)
{
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		RunOneStep();
		OnStepEnd.Broadcast();
	}
}

int32 UDTSimSubsystem::AdvanceFrame(float DeltaTime)
{
	if (bPauseRequested)
	{
		return 0; // paused at a step boundary: the accumulator holds, no phase runs (R-TEC-14)
	}
	const int32 Due = Clock.Advance(DeltaTime);
	int32 Ran = 0;
	while (Ran < Due)
	{
		RunOneStep();
		OnStepEnd.Broadcast();
		++Ran;
		if (bPauseRequested)
		{
			break; // the steps still due are dropped: sim time does not pass while paused
		}
	}
	return Ran;
}

void UDTSimSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	AdvanceFrame(DeltaTime);
	const float Alpha = Clock.GetAlpha();
	for (const TArray<IDTSimSystem*>& PhaseSystems : Systems)
	{
		for (IDTSimSystem* System : PhaseSystems)
		{
			System->SimRender(Alpha);
		}
	}
}
