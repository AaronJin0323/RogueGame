// Owns the enemy pool, steps it inside the fixed-step sim and draws it as one instanced mesh (02_Systems/enemies.md).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemyRenderer.h"
#include "DTEnemySubsystem.generated.h"

class UDTEnemySubsystem;

/** Forwards EDTSimPhase::ResolveHits to the enemy subsystem (a system registers for one phase per object). */
class FDTEnemyResolveHitsPhase : public IDTSimSystem
{
public:
	UDTEnemySubsystem* Owner = nullptr;
	virtual void SimStep(const FDTSimContext& Context) override;
};

/**
 * Registers itself for EDTSimPhase::Enemies (move + publish) and an adapter for EDTSimPhase::ResolveHits (damage,
 * deaths, compaction). Spawn requests from outside the step are queued and applied at the start of the next
 * Enemies phase, so indices never change between phases.
 */
UCLASS()
class DUSKTRACER_API UDTEnemySubsystem : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// IDTSimSystem (EDTSimPhase::Enemies)
	virtual void SimStep(const FDTSimContext& Context) override;
	virtual void SimRender(float Alpha) override;

	/** EDTSimPhase::ResolveHits, called through the adapter. */
	void StepResolveHits(const FDTSimContext& Context);

	/** Queues Count swarm enemies on a ring of C.ENM.SPAWN_RING_RADIUS_UU around hero 0 (cap respected at spawn). */
	void RequestSpawn(int32 Count);
	/** Keeps the population topped up to Count every step (0 turns it off). */
	void SetKeepPopulation(int32 Count);
	/** Removes every enemy at the start of the next step and stops any keep target. */
	void RequestClear();

	const FDTEnemyPool& GetPool() const { return Pool; }
	int32 GetLiveCount() const { return Pool.Num(); }
	int64 GetTotalKills() const { return Pool.GetTotalKills(); }
	int64 GetRefusedSpawns() const { return Pool.GetRefusedSpawns(); }
	float GetKillsPerSecond() const { return Pool.GetKillRate().GetKillsPerSecond(); }
	int32 GetKeepPopulation() const { return KeepPopulation; }
	int32 GetPendingSpawns() const { return PendingSpawns; }

	/** Body colour written into the instances: a dark neutral tinted toward the always-lit optic (04_Presentation §3). */
	static FLinearColor GetInstanceColor();

private:
	void ApplyRequests(const FDTSimContext& Context);
	void EnsureRenderer();

	FDTEnemyPool Pool;
	FDTEnemyResolveHitsPhase ResolvePhase;
	TWeakObjectPtr<UDTSimSubsystem> Sim;

	UPROPERTY(Transient)
	TObjectPtr<ADTEnemyRenderer> Renderer;

	TArray<FTransform> RenderTransforms;
	int32 PendingSpawns = 0;
	int32 KeepPopulation = 0;
	bool bClearPending = false;
};
