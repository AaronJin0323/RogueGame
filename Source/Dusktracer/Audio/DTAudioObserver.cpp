#include "Audio/DTAudioObserver.h"
#include "Audio/Observe/DTObserveDanger.h"
#include "Audio/Observe/DTObserveInterface.h"
#include "Audio/Observe/DTObserveWarn.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"

FDTAudioObserver::FDTAudioObserver()
{
	// Wave 1's families, one file each (Validation/S4_audio_wave1.md §4.3). A family adds its line here.
	Families.Add(MakeUnique<FDTObserveInterface>());
	Families.Add(MakeUnique<FDTObserveWarn>());
	Families.Add(MakeUnique<FDTObserveDanger>());
}

FDTAudioObserver::~FDTAudioObserver()
{
	Unbind();
}

void FDTAudioObserver::Bind(UDTSimSubsystem& InSim)
{
	Unbind();
	Sim = &InSim;
	StepEndHandle = InSim.OnStepEnd.AddRaw(this, &FDTAudioObserver::HandleStepEnd);
	if (UWorld* World = InSim.GetWorld())
	{
		for (const TUniquePtr<IDTAudioFamily>& Family : Families)
		{
			Family->Begin(*World);
		}
	}
}

void FDTAudioObserver::Unbind()
{
	if (UDTSimSubsystem* Bound = Sim.Get())
	{
		Bound->OnStepEnd.Remove(StepEndHandle);
	}
	if (Sim.IsValid() || StepEndHandle.IsValid())
	{
		for (const TUniquePtr<IDTAudioFamily>& Family : Families)
		{
			Family->End();
		}
	}
	StepEndHandle.Reset();
	Sim.Reset();
}

void FDTAudioObserver::HandleStepEnd()
{
	const UDTSimSubsystem* Bound = Sim.Get();
	const UWorld* World = Bound ? Bound->GetWorld() : nullptr;
	if (!World || !Requests)
	{
		return;
	}
	const uint64 Start = FPlatformTime::Cycles64();
	if (BeforeStep)
	{
		BeforeStep();
	}
	for (const TUniquePtr<IDTAudioFamily>& Family : Families)
	{
		Family->OnStep(*World, Bound->GetStepIndex(), *Requests);
	}
	StepCycles += FPlatformTime::Cycles64() - Start;
}

void FDTAudioObserver::Frame(const UWorld& World)
{
	if (!Requests)
	{
		return;
	}
	for (const TUniquePtr<IDTAudioFamily>& Family : Families)
	{
		Family->OnFrame(World, *Requests);
	}
}

void FDTAudioObserver::Reset()
{
	for (const TUniquePtr<IDTAudioFamily>& Family : Families)
	{
		Family->Reset();
	}
}

double FDTAudioObserver::TakeStepSeconds()
{
	const double Seconds = FPlatformTime::ToSeconds64(StepCycles);
	StepCycles = 0;
	return Seconds;
}
