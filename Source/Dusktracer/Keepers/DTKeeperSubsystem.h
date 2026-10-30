// The Keeper layer in a game world (02_Systems/keepers.md): owns FDTKeeperSim and wires it to the merged seams.
//   offers  - OnBlessingTaken / OnKeeperChosen: rebuild the player's Keeper flag sources (R-KEE-03, R-KEE-07); right after
//             the Patron pick the player is given Weapon.PulseRifle at level one (D-015)
//   hero    - OnIntercession: queue the cast; it is composed and run in EDTSimPhase::Effects (R-KEE-14)
//   sim     - EDTSimPhase::Bullets (after the pool): capture the step's player hits; ::Effects: enchantment, frozen_vuln,
//             Intercession; ::Pickups: status telemetry of the step's applications
//   render  - Keeper trails behind player rounds through extra ADTBulletRenderer instances (public API only): the primary's
//             Pal.Keeper.<Path> on the trail body, the second's on the tail tip; the round's core is never touched (R-PRE-01)
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/DTBulletRenderer.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Keepers/DTKeeperSim.h"
#include "DTKeeperSubsystem.generated.h"

class UDTKeeperSubsystem;

/** Forwards one phase to the Keeper subsystem (a system registers for one phase per object). */
class FDTKeeperPhaseAdapter : public IDTSimSystem
{
public:
	UDTKeeperSubsystem* Owner = nullptr;
	EDTSimPhase Phase = EDTSimPhase::Bullets;
	virtual void SimStep(const FDTSimContext& Context) override;
};

UCLASS()
class DUSKTRACER_API UDTKeeperSubsystem : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	// IDTSimSystem (EDTSimPhase::Effects)
	virtual void SimStep(const FDTSimContext& Context) override;
	virtual void SimRender(float Alpha) override;

	/** EDTSimPhase::Bullets and ::Pickups through the adapters. */
	void StepPhase(EDTSimPhase Phase, const FDTSimContext& Context);

	/** Rebuilds OwnerId's Keeper flag sources now (offers, debug). */
	void Rebuild(int32 OwnerId);
	/** Grants a blessing through the run state (as an offer pick does) and rebuilds; false for an unknown or owned id. */
	bool Bless(int32 OwnerId, FName BlessingId);
	/** Queues an Intercession cast for OwnerId (the hero's activation calls this; DT.Intercede fills the meter first). */
	void Intercede(int32 OwnerId) { Sim.QueueIntercession(OwnerId); }

	FDTKeeperSim& GetSim() { return Sim; }
	const FDTKeeperSim& GetSim() const { return Sim; }
	/** Number of trail renderers spawned so far (lazily, one per colour and part). */
	int32 GetTrailRendererCount() const;

private:
	FDTRunState* GetRun() const;
	void HandleBlessingTaken(int32 OwnerId, FName BlessingId);
	void HandleKeeperChosen(int32 OwnerId, EDTPath Path, bool bPatron);
	void HandleIntercession(int32 OwnerId, EDTPath Path);
	void RecordStatuses(const FDTSimContext& Context);
	ADTBulletRenderer* EnsureTrailRenderer(int32 Bucket);

	FDTKeeperSim Sim;
	FDTKeeperPhaseAdapter CapturePhase;
	FDTKeeperPhaseAdapter PickupsPhase;
	TWeakObjectPtr<UDTSimSubsystem> SimSubsystem;
	FDelegateHandle BlessingHandle;
	FDelegateHandle KeeperHandle;
	FDelegateHandle IntercessionHandle;
	TArray<TArray<FTransform>> TrailTransforms;

	/** Bucket = Path * 2 + part (0 trail body, 1 tail tip); spawned on first use. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<ADTBulletRenderer>> TrailRenderers;
};
