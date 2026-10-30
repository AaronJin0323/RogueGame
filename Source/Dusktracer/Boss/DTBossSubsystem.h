// The Warden in the world (02_Systems/boss.md): starts the fight on the director's boss time, steps FDTBossCore in the
// Enemies phase right after the enemy pool, writes boss telemetry, ends the run on the boss's death and draws its bar.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Boss/DTBossCore.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "DTBossSubsystem.generated.h"

class UCanvas;
class UDTDirector;
class UDTEnemySubsystem;
class UDTHeroSubsystem;

/** The Warden died (one Kill to KillerOwner): the run ends with reason victory. Broadcast from inside the step. */
DECLARE_MULTICAST_DELEGATE_OneParam(FDTOnBossDefeated, int64 /*StepIndex*/);

/**
 * Registers for EDTSimPhase::Enemies after UDTEnemySubsystem (07_Technical §4: "... enemy fire and contact append
 * HeroHits; boss"), so its hazard hits survive the pool's HeroHits reset and its telegraphs append to the pool's.
 * Binds UDTDirector::OnBossTime (R-DIR-13) and UDTHeroSubsystem::OnAllHeroesDown (boss_end outcome death).
 *
 * On the boss's death it logs "Run end reason=victory step=<n>", broadcasts OnBossDefeated and ends the telemetry
 * session ("victory") on the next tick, mirroring ADTGameMode's death path. ADTGameMode marks its run over from
 * OnBossDefeated; ADTHUD calls DrawBossBar.
 */
UCLASS()
class DUSKTRACER_API UDTBossSubsystem : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// IDTSimSystem (EDTSimPhase::Enemies)
	virtual void SimStep(const FDTSimContext& Context) override;

	/** Starts the fight at the next Enemies phase (director boss time, DT.Boss). Ignored while a fight is running. */
	void StartFight();
	/** Debug (DT.Boss.Phase): advances to Phase (1-based), never back. */
	void ForcePhase(int32 Phase);

	/** HUD hook (ADTHUD::DrawHUD): the boss name and its effective HP bar while the fight runs (boss.md Text). */
	void DrawBossBar(UCanvas* Canvas) const;

	const FDTBossCore& GetCore() const { return Core; }
	bool IsFightPending() const { return bStartRequested; }

	FDTOnBossDefeated OnBossDefeated;

	/** Cumulative wins for the form choice (R-BOS-01); the meta save owns them (S3), 0 until then. */
	static int32 GetWins() { return 0; }

private:
	class FSink : public IDTBossSink
	{
	public:
		UDTBossSubsystem* Owner = nullptr;
		virtual void QueueSpawn(const FDTEnemySpawn& Spawn) override;
		virtual void BossStart(FName Form, int32 Wins) override;
		virtual void BossPhase(int32 Phase, float FightTime, float HpFrac, bool bByTime) override;
		virtual void BossCalibrated(float WindowDps, float EffectiveMax, bool bClamped) override;
		virtual void ThreatAction(const FVector2f& Origin, FName AttackId) override;
		virtual void BossEnd(FDTTelBossEnd& End) override;
		virtual void BossDefeated(int64 StepIndex, int32 KillerOwner) override;
	};

	void HandleBossTime(float RunSeconds);
	void HandleAllHeroesDown(int64 StepIndex);
	void EndTelemetrySession();

	FDTBossCore Core;
	FSink Sink;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TWeakObjectPtr<UDTEnemySubsystem> Enemies;
	TWeakObjectPtr<UDTDirector> Director;
	TWeakObjectPtr<UDTHeroSubsystem> Heroes;
	FDelegateHandle BossTimeHandle;
	FDelegateHandle AllDownHandle;
	/** Per-player dashes when the fight started (boss_end dashes). */
	TArray<int32> DashesAtStart;
	int64 LastStep = -1;
	bool bStartRequested = false;
};
