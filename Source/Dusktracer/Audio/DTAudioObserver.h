// Turns what the game shows into sound requests (02_Systems/audio.md R-AUD-02): bound to UDTSimSubsystem::OnStepEnd for the run
// side, polled once a frame for the interface. It only reads: public delegates, counters that only grow, state getters. It never
// calls a take-once queue and never walks the bullet or enemy arrays. Each family is one file under Audio/Observe/, so a family
// adds a file and edits none.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSimSubsystem.h"

class UWorld;

/** Where a family's requests go (the scheduler, through UDTAudioSubsystem; a recording in the tests). */
class IDTAudioRequests
{
public:
	virtual ~IDTAudioRequests() = default;
	/** A run sound at a sim step. bNoDuck: this danger voice does not lower the Player group (the heartbeat under a held duck). */
	virtual void Run(int32 Sound, int32 Count, int64 Step, bool bNoDuck = false) = 0;
	/** An interface sound now. */
	virtual void Ui(int32 Sound, int32 Count) = 0;
	/** The Player group held at least this deep for as long as it is asked (0 = no hold): the low-health hold of dt.Audio.LowHpHoldDb. */
	virtual void HeldDuck(float Db) {}
};

/** One family's observer. */
class IDTAudioFamily
{
public:
	virtual ~IDTAudioFamily() = default;
	/** After every sim step, on both step paths (R-AUD-02). */
	virtual void OnStep(const UWorld& World, int64 Step, IDTAudioRequests& Out) {}
	/** Once a frame, paused or not. */
	virtual void OnFrame(const UWorld& World, IDTAudioRequests& Out) {}
	/** A new run in the same world. */
	virtual void Reset() {}
	/** The observer bound the world's sim: a family that listens to a public delegate binds it here, and lets it go in End. */
	virtual void Begin(UWorld& World) {}
	virtual void End() {}
};

class DUSKTRACER_API FDTAudioObserver
{
public:
	FDTAudioObserver();
	~FDTAudioObserver();
	FDTAudioObserver(const FDTAudioObserver&) = delete;
	FDTAudioObserver& operator=(const FDTAudioObserver&) = delete;

	/** Where requests go (set once, before the first frame). */
	void SetOutput(IDTAudioRequests& Out) { Requests = &Out; }
	/** Binds OnStepEnd of InSim: the run families observe from then on. */
	void Bind(UDTSimSubsystem& InSim);
	void Unbind();
	/** The frame poll. */
	void Frame(const UWorld& World);
	void Reset();
	/** Called at every step end before any family observes (the subsystem follows a new run there, so no request of it is lost). */
	TFunction<void()> BeforeStep;
	/** Seconds spent in OnStep since the last call (the observe part of the timer, R-AUD-21). */
	double TakeStepSeconds();

private:
	void HandleStepEnd();

	TArray<TUniquePtr<IDTAudioFamily>> Families;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	IDTAudioRequests* Requests = nullptr;
	FDelegateHandle StepEndHandle;
	uint64 StepCycles = 0;
};
