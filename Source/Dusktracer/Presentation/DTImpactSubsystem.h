// The reader of the hit and kill tap for the eye (02_Systems/impact_art.md R-IMP-03, R-IMP-11): fed once per sim step after ResolveHits,
// holds the live effects and the density tier, hands the effects to its renderer once per rendered frame. Writes nothing to the sim.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Presentation/DTImpactArt.h"
#include "DTImpactSubsystem.generated.h"

class ADTImpactRenderer;
class UDTEnemySubsystem;

/**
 * Registered with UDTSimSubsystem in EDTSimPhase::Director, after ResolveHits (R-IMP-02). Each step it reads the pool's step list
 * (FDTEnemyPool::GetStepImpacts), the pool's kill rate and FDTWorldView::HeroHurt; it copies what it needs and writes no field of the pool, of
 * the view or of the run (04_Presentation R-PRE-11). The density tier advances on every step, drawing or not.
 */
UCLASS()
class DUSKTRACER_API UDTImpactSubsystem : public UWorldSubsystem, public IDTSimSystem
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

	// ---- the density reader (R-IMP-11; D-056 ruling 5): the one reader of the tier, for the eye and for the ear -----------------
	// Read-only and cheap. Valid from the end of a step's Director phase until the next step's; PerKill (rate 0, step -1) before a run's first step.

	/** R-IMP-11: the tier of the last step, DTImpactArt::NextTier folded over the pool's kill rate. */
	EDTDensityTier GetDensityTier() const { return Tier; }
	/** R-IMP-11: the kill rate (FDTEnemyPool::GetKillRate, kills per second) the tier of GetDensityStep() was judged on. */
	float GetKillsPerSecond() const { return KillsPerSecond; }
	/** R-IMP-11: the sim step that tier was judged on. */
	int64 GetDensityStep() const { return DensityStep; }

	// ---- draw and stats ------------------------------------------------------------------------------------------------------

	/** dt.Impact.Draw: while 0 no effect is raised or drawn (live ones end); the tier still advances. Process-wide, 1 at start. */
	static int32& DrawSetting();
	static bool IsDrawing() { return DrawSetting() != 0; }

	const FDTImpactEffects& GetEffects() const { return Effects; }
	ADTImpactRenderer* GetRenderer() const { return Renderer; }
	/** Counted hits and kills over the last C.SIM.FIXED_HZ steps (one second of sim time). */
	int32 GetHitsLastSecond() const { return HitsInWindow; }
	int32 GetKillsLastSecond() const { return KillsInWindow; }
	/** One line for DT.Impact.Stats and the tool runs that read it from the log. */
	FString DescribeStats() const;

	/** Test hook (T-IMP-08, T-IMP-11): the steps after this are judged on Rate instead of the pool's kill rate; a negative Rate returns to the pool's. */
	void SetRateOverride(float Rate) { RateOverride = Rate; }

private:
	void EnsureRenderer();
	void ResetRun();

	FDTImpactEffects Effects;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TWeakObjectPtr<UDTEnemySubsystem> Enemies;

	UPROPERTY(Transient)
	TObjectPtr<ADTImpactRenderer> Renderer;

	EDTDensityTier Tier = EDTDensityTier::PerKill;
	float KillsPerSecond = 0.0f;
	int64 DensityStep = -1;
	int64 LastStep = -1;
	float RateOverride = -1.0f;
	/** For DT.Impact.Stats: the highest rate since the run started and the last step a hero lost health. */
	float PeakKillsPerSecond = 0.0f;
	int64 LastHurtStep = -1;

	/** Hero positions at the end of the last two steps: a ring is drawn at the hero's interpolated place. */
	TArray<FVector2f> HeroPrev;
	TArray<FVector2f> HeroNow;
	TArray<FVector2f> HeroDrawn;

	/** Per-step counted hits and kills, one second of steps (a ring buffer), and their sums. */
	TArray<int32> HitWindow;
	TArray<int32> KillWindow;
	int32 WindowCursor = 0;
	int32 HitsInWindow = 0;
	int32 KillsInWindow = 0;
	bool bWasDrawing = true;
};
