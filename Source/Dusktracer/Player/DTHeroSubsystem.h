// Owns every player's hero and steps it in the fixed-step sim (02_Systems/combat_core.md R-CMB-01).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Player/DTHeroSim.h"
#include "Player/DTHeroTypes.h"
#include "DTHeroSubsystem.generated.h"

class ADTHero;

/** A player's Intercession fired this step (R-CMB-11). What it does is keepers.md; bind here. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDTOnIntercession, int32 /*OwnerId*/, EDTPath /*Path*/);
/** Every hero event, in step order (telemetry, cues). */
DECLARE_MULTICAST_DELEGATE_OneParam(FDTOnHeroEvent, const FDTHeroEvent& /*Event*/);
/** Every player's hero is down: the run ends with reason Death (R-CMB-10). */
DECLARE_MULTICAST_DELEGATE_OneParam(FDTOnAllHeroesDown, int64 /*StepIndex*/);

/**
 * Registers an input adapter for EDTSimPhase::Input (latched FDTHeroInput -> step) and itself for EDTSimPhase::Hero.
 * Pawns, the bot and tests feed input only through LatchInput (R-CMB-12). After each Hero phase the step's events are
 * broadcast (OnHeroEvent, OnIntercession, OnAllHeroesDown); in SimRender every registered ADTHero is drawn at the
 * interpolation alpha.
 */
UCLASS()
class DUSKTRACER_API UDTHeroSubsystem : public UWorldSubsystem, public IDTSimSystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// IDTSimSystem (EDTSimPhase::Hero)
	virtual void SimStep(const FDTSimContext& Context) override;
	virtual void SimRender(float Alpha) override;

	/** The only way input reaches the hero: folds one frame sample into the player's pending input. */
	void LatchInput(int32 OwnerId, const FDTHeroInput& Frame) { Latch.Latch(OwnerId, Frame); }

	void RegisterPawn(ADTHero* Pawn);
	void UnregisterPawn(ADTHero* Pawn);

	const FDTHeroSim& GetHero() const { return Hero; }
	int32 GetNumHeroes() const { return Hero.Num(); }
	/** Null when OwnerId is not a hero of this run. */
	const FDTHeroState* GetState(int32 OwnerId) const { return Hero.IsValidPlayer(OwnerId) ? &Hero.GetState(OwnerId) : nullptr; }
	bool IsDown(int32 OwnerId) const { return Hero.IsValidPlayer(OwnerId) && Hero.GetState(OwnerId).bDown; }
	bool IsRunOver() const { return Hero.IsRunOver(); }

	EDTAimMode GetAimMode(int32 OwnerId) const { return Hero.GetAimMode(OwnerId); }
	void SetAimMode(int32 OwnerId, EDTAimMode Mode) { Hero.SetAimMode(OwnerId, Mode); }
	void SetStats(int32 OwnerId, const FDTHeroStats& Stats) { Hero.SetStats(OwnerId, Stats); }
	void SetNoDamage(bool bInNoDamage) { Hero.SetNoDamage(bInNoDamage); }
	bool GetNoDamage() const { return Hero.GetNoDamage(); }

	/** The latest interpolation alpha the sim rendered with. */
	float GetRenderAlpha() const { return RenderAlpha; }

	FDTOnIntercession OnIntercession;
	FDTOnHeroEvent OnHeroEvent;
	FDTOnAllHeroesDown OnAllHeroesDown;

private:
	FDTHeroSim Hero;
	FDTHeroInputLatch Latch;
	FDTHeroInputPhase InputPhase;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TArray<TWeakObjectPtr<ADTHero>> Pawns;
	float RenderAlpha = 0.0f;
	bool bRunOverBroadcast = false;
};
