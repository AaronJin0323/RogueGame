// Owns the enemy pool and its enemy fire, steps them inside the fixed-step sim and draws them (02_Systems/enemies.md).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyBulletRenderer.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemyRenderer.h"
#include "Enemies/DTEnemyTypes.h"
#include "Enemies/DTTelegraphRenderer.h"
#include "DTEnemySubsystem.generated.h"

class UDTEnemySubsystem;
class FDTTelemetryCore;

/** Forwards EDTSimPhase::ResolveHits to the enemy subsystem (a system registers for one phase per object). */
class FDTEnemyResolveHitsPhase : public IDTSimSystem
{
public:
	UDTEnemySubsystem* Owner = nullptr;
	virtual void SimStep(const FDTSimContext& Context) override;
};

/**
 * Registers itself for EDTSimPhase::Enemies (spawns, statuses, roles, movement, publish, enemy fire) and an adapter for
 * EDTSimPhase::ResolveHits (damage, deaths, Kills, compaction, republish). Spawn requests from outside the step are
 * queued and applied at the start of the next Enemies phase, so indices never change between phases.
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

	/**
	 * Director spawn API (director.md R-DIR-07, R-DIR-10): queues one enemy, applied in order at the start of the next
	 * Enemies phase and marked Director. Role, position (clamped to the arena), HpScale and DmgScale (kept for life),
	 * bElite (C.ROLE.ELITE_HP_MULT), bArrival (drop-in: Rusher in WindUp, Turret in its tell when it gets a token) and
	 * BeatId (copied into FDTKill). A Turret draws its fire pattern from the Spawn stream. At C.ENEMIES.CAP the spawn
	 * is refused and counted (GetRefusedSpawns), never queued past the step.
	 */
	void QueueSpawn(const FDTEnemySpawn& InSpawn);

	/** Debug: queues Count swarm enemies on a ring of C.ENM.SPAWN_RING_RADIUS_UU around hero 0 (cap respected at spawn). */
	void RequestSpawn(int32 Count);
	/** Debug: as RequestSpawn for any role, optionally elite or arriving (not Director: drops nothing). */
	void RequestSpawnRole(EDTEnemyRole Role, int32 Count, bool bElite = false, bool bArrival = false);
	/** Keeps the swarm population topped up to Count every step (0 turns it off). */
	void SetKeepPopulation(int32 Count);
	/** Removes every enemy and enemy round at the start of the next step and stops any keep target. */
	void RequestClear();
	/** Debug (DT.Status): applies Status to the first Count enemies (all when Count <= 0) at the next Enemies phase. */
	void RequestStatus(EDTStatus Status, int32 Count, float Magnitude);
	/** Debug (DT.EnemyFire): while off no ranged unit starts a tell. */
	void SetEnemyFireEnabled(bool bEnabled) { Pool.SetFireEnabled(bEnabled); }
	bool IsEnemyFireEnabled() const { return Pool.IsFireEnabled(); }

	const FDTEnemyPool& GetPool() const { return Pool; }
	FDTEnemyPool& GetPool() { return Pool; }
	int32 GetLiveCount() const { return Pool.Num(); }
	int64 GetTotalKills() const { return Pool.GetTotalKills(); }
	int64 GetRefusedSpawns() const { return Pool.GetRefusedSpawns(); }
	float GetKillsPerSecond() const { return Pool.GetKillRate().GetKillsPerSecond(); }
	int32 GetKeepPopulation() const { return KeepPopulation; }
	int32 GetPendingSpawns() const { return PendingSpawns; }
	ADTEnemyRenderer* GetRenderer() const { return Renderer; }
	ADTEnemyBulletRenderer* GetRoundRenderer() const { return RoundRenderer; }
	ADTTelegraphRenderer* GetTelegraphRenderer() const { return TelegraphRenderer; }

	/** Instance colours (04_Presentation §3, R-ENM-17, R-ENM-24): palette tokens, never literals. */
	static FLinearColor GetHullColor();
	static FLinearColor GetOpticColor();
	static FLinearColor GetRoundColor();
	static FLinearColor GetRoundCoreColor();
	static FLinearColor GetTelegraphColor();

	/** Sends every threat action Pool started in its last step to Records (ThreatAction{Role, AttackId}, threat_actions_per_min). */
	static void ForwardThreatActions(const FDTEnemyPool& InPool, FDTTelemetryCore& Records);

private:
	struct FRoleRequest
	{
		EDTEnemyRole Role = EDTEnemyRole::Swarm;
		int32 Count = 0;
		bool bElite = false;
		bool bArrival = false;
	};
	struct FStatusRequest
	{
		EDTStatus Status = EDTStatus::Shock;
		int32 Count = 0;
		float Magnitude = 0.0f;
	};

	void ApplyRequests(const FDTSimContext& Context);
	void EnsureRenderer();

	FDTEnemyPool Pool;
	FDTEnemyResolveHitsPhase ResolvePhase;
	TWeakObjectPtr<UDTSimSubsystem> Sim;

	UPROPERTY(Transient)
	TObjectPtr<ADTEnemyRenderer> Renderer;

	UPROPERTY(Transient)
	TObjectPtr<ADTEnemyBulletRenderer> RoundRenderer;

	UPROPERTY(Transient)
	TObjectPtr<ADTTelegraphRenderer> TelegraphRenderer;

	TArray<FTransform> HullTransforms;
	TArray<FTransform> OpticTransforms;
	TArray<FTransform> RoundTransforms;
	TArray<FDTEnemySpawn> QueuedSpawns;
	TArray<FRoleRequest> RoleRequests;
	TArray<FStatusRequest> StatusRequests;
	int32 PendingSpawns = 0;
	int32 KeepPopulation = 0;
	bool bClearPending = false;
};
