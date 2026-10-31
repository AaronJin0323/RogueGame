// Counts what a run earned, on the sim clock, and banks it when the run's telemetry session closes (02_Systems/meta.md).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Meta/DTMetaProfile.h"
#include "Subsystems/WorldSubsystem.h"
#include "DTRunRecorder.generated.h"

class UDTBossSubsystem;
class UDTHeroSubsystem;
class UDTTelemetrySubsystem;

/**
 * Game/PIE worlds only. Registers for EDTSimPhase::Director and only reads: nothing in the sim reads it back (R-MET-03,
 * R-MET-16). The end of a run freezes the tally at the end of that step; the close of the telemetry session, which is
 * once per run, banks it (R-MET-04). That close never comes from inside a step.
 */
UCLASS()
class DUSKTRACER_API UDTRunRecorder : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// IDTSimSystem (EDTSimPhase::Director)
	virtual void SimStep(const FDTSimContext& Context) override;

	const FDTRunTally& GetTally() const { return Tally; }
	const FDTRunAward& GetAward() const { return Award; }

	/** Run-end systems add their per-run counts here (R-MET-19). */
	void AddCounter(FName Key, int64 Amount) { Tally.Counters.FindOrAdd(Key) += Amount; }

	/** A telemetry close reason as the run's end (R-MET-05). */
	static EDTRunEnd EndFromReason(const FString& Reason);
	static const TCHAR* EndName(EDTRunEnd End);

private:
	void HandleRunEnd(int64 StepIndex);
	void HandleSessionEnding(const FString& Reason);

	FDTRunTally Tally;
	FDTRunAward Award;
	bool bEndPending = false;
	bool bFrozen = false;
	bool bClosed = false;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TWeakObjectPtr<UDTTelemetrySubsystem> Telemetry;
	TWeakObjectPtr<UDTHeroSubsystem> Heroes;
	TWeakObjectPtr<UDTBossSubsystem> Boss;
	FDelegateHandle SessionHandle;
	FDelegateHandle AllDownHandle;
	FDelegateHandle BossHandle;
};
