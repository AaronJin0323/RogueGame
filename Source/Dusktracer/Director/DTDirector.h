// The run director in the world: pacing, threats, drop-ins and drops (02_Systems/director.md).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Director/DTDirectorCore.h"
#include "DTDirector.generated.h"

class ADTTelegraphRenderer;
class UDTEnemySubsystem;
class UDTGemSubsystem;
class UDTHeroSubsystem;

/** The run reached C.RUN.LENGTH_TO_BOSS (R-DIR-13): normal spawning has stopped; the boss package binds here. */
DECLARE_MULTICAST_DELEGATE_OneParam(FDTOnBossTime, float /*RunSeconds*/);

/**
 * Registers for EDTSimPhase::Director (the last phase) in Game/PIE worlds and stays idle until StartRun (ADTGameMode at
 * run start), so test worlds and tools that never start a run see no director spawns. Each step it reads the enemy pool
 * (alive by role, threat actions), the heroes and FDTWorldView::Kills, and drives FDTDirectorCore, which queues spawns
 * (UDTEnemySubsystem::QueueSpawn), drops gems and chests (UDTGemSubsystem), publishes DropInMarkers and bThreatBlock,
 * and writes one dir_block per ended block (telemetry). Drop-in markers are drawn red with the telegraph renderer.
 *
 * Off switches: cvar dt.Director.Enabled (default 1) and the command-line flag -DTNoDirector (sets the cvar to 0 at run
 * start; Tools/Smoke.ps1 passes it so the smoke goldens stay deterministic). Turning the cvar on later starts the
 * timeline from zero at that step.
 */
UCLASS()
class DUSKTRACER_API UDTDirector : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// IDTSimSystem (EDTSimPhase::Director)
	virtual void SimStep(const FDTSimContext& Context) override;
	virtual void SimRender(float Alpha) override;

	/** Called when the run starts (after UDTSimSubsystem::StartRun): the timeline starts at the next step. */
	void StartRun();
	/** True once StartRun was called and the director is enabled. */
	bool IsActive() const;

	FDTDirectorCore& GetCore() { return Core; }
	const FDTDirectorCore& GetCore() const { return Core; }

	/** R-DIR-13: broadcast once per run, from the Director phase of the step the run reaches boss time. */
	FDTOnBossTime OnBossTime;

	/** The cvar dt.Director.Enabled. */
	static bool IsEnabledByCVar();

private:
	class FSink : public IDTDirectorSink
	{
	public:
		UDTDirector* Owner = nullptr;
		virtual void QueueSpawn(const FDTEnemySpawn& Spawn) override;
		virtual void SpawnGem(const FDTGemSpawn& Gem) override;
		virtual void SpawnChest(const FVector2f& Position) override;
		virtual void BossTime(float RunSeconds) override;
		virtual void BlockEnded(FDTTelDirBlock& Block) override;
	};

	void EnsureRenderer();

	FDTDirectorCore Core;
	FSink Sink;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TWeakObjectPtr<UDTEnemySubsystem> Enemies;
	TWeakObjectPtr<UDTGemSubsystem> Gems;
	TWeakObjectPtr<UDTHeroSubsystem> Heroes;

	UPROPERTY(Transient)
	TObjectPtr<ADTTelegraphRenderer> MarkerRenderer;

	TArray<bool> HeroAliveScratch;
	TArray<FVector2f> ThreatScratch;
	TArray<FDTTelegraph> MarkerScratch;
	/** Run totals at the end of the last block, by player: dir_block reports the difference. */
	TArray<double> LastDamageShare;
	TArray<int32> LastDashes;
	bool bRunStarted = false;
	bool bWasActive = false;
};
