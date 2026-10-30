// Owns the gem and chest pool, steps it in EDTSimPhase::Pickups, credits XP and raises level-ups (02_Systems/progression.md).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Progression/DTGemPool.h"
#include "Progression/DTPickupRenderer.h"
#include "Progression/DTProgression.h"
#include "DTGemSubsystem.generated.h"

/** A player reached Level (after its hand was queued). Broadcast in step order from the Pickups phase. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDTOnLevelUp, int32 /*OwnerId*/, int32 /*Level*/);

/**
 * Registers for EDTSimPhase::Pickups (after Effects, before Director). The director drops gems and chests through
 * Spawn / SpawnChest (director.md R-DIR-12, R-DIR-16); chests and Keeper effects pull gems through MagnetAll. A gem is
 * owned here from spawn to collection; drops per kill are the director's, not this system's.
 */
UCLASS()
class DUSKTRACER_API UDTGemSubsystem : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// IDTSimSystem (EDTSimPhase::Pickups)
	virtual void SimStep(const FDTSimContext& Context) override;
	virtual void SimRender(float Alpha) override;

	/** R-PRG-07: adds a gem (or merges it at the cap). Callable from any phase; it is stepped from the next Pickups phase. */
	EDTGemSpawnResult Spawn(const FDTGemSpawn& Gem);
	/** R-PRG-09: a chest lying at Position until a hero touches it. Returns its index. */
	int32 SpawnChest(const FVector2f& Position);
	/** R-PRG-09: binds every live free gem to OwnerId. Returns the number bound. */
	int32 MagnetAll(int32 OwnerId);

	const FDTGemPool& GetPool() const { return Pool; }

	/** Per-player XP-gain and pickup-radius multipliers (identity until FDTStatSet writes them). */
	void SetStats(int32 OwnerId, const FDTProgressionStats& Stats);
	FDTProgressionStats GetStats(int32 OwnerId) const { return Stats.IsValidIndex(OwnerId) ? Stats[OwnerId] : FDTProgressionStats(); }

	/** Debug and tools (DT.Xp, DT.Level): adds XP to one player, or to every player when OwnerId is INDEX_NONE. */
	int32 GrantXP(int32 OwnerId, int64 Amount);

	/** Sim time (s) of the player's last level-up, negative if none (HUD flash). */
	double GetLastLevelUpTime(int32 OwnerId) const { return LastLevelUpTime.IsValidIndex(OwnerId) ? LastLevelUpTime[OwnerId] : -1.0; }
	/** Sim time (s) of the last stepped Pickups phase. */
	double GetSimTime() const { return SimTime; }

	FDTOnLevelUp OnLevelUp;

private:
	/** Empties the floor when the sim's step index was rewound (a new run). bStepping: called from SimStep. */
	void CheckRunRestart(int64 StepIndex, bool bStepping);
	void SyncPlayers(int32 NumPlayers);
	void RaiseLevels();
	void EnsureRenderer();

	FDTGemPool Pool;
	FDTPickupStepResult StepResult;
	TArray<FDTLevelGain> LevelScratch;
	TArray<int64> CreditScratch;
	TArray<int64> StepXp;
	TArray<int32> StepGems;
	TArray<bool> PresentScratch;
	TArray<float> RadiusScratch;
	TArray<float> XpGainScratch;
	TArray<FDTProgressionStats> Stats;
	TArray<double> LastLevelUpTime;
	TWeakObjectPtr<UDTSimSubsystem> Sim;

	UPROPERTY(Transient)
	TObjectPtr<ADTPickupRenderer> Renderer;

	TArray<FTransform> GemTransforms;
	TArray<FTransform> ChestTransforms;
	int64 LastStepped = -1;
	double SimTime = 0.0;
};
