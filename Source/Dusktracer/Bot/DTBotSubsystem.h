// Bot v1 in a game world: drives player 0 through UDTHeroSubsystem::LatchInput (the pawn's path, combat_core.md R-CMB-12)
// and answers offer hands through UDTOfferSubsystem (offers.md). Off by default; a tool, never a player-facing feature.
//
// Switches (command line, mirrored to cvars at world begin play; the cvars also work from the console or -ExecCmds):
//   -DTBot=<None|Random|Steered>        dt.Bot.Policy      (Off = no bot; the default)
//   -DTBotPath=<Storm|Forge|Tide>       dt.Bot.Path        Steered: Patron Path
//   -DTBotArchetype=<Tempest|...>       dt.Bot.Archetype   Steered: preferred archetype of that Path
//   -DTBotSecond=<Path>                 dt.Bot.Second      Steered: second Keeper (default FDTBotConfig::DefaultSecond)
//   -DTBotSkill=<0..1>                  dt.Bot.Skill       1 = no reaction delay, no movement or aim noise (default 1)
//   -DTBotFast[=<steps>]                dt.Bot.Fast        steps run per frame through UDTSimSubsystem::StepNow (0 = real time)
//   -DTBotMaxSeconds=<s>                dt.Bot.MaxSeconds  ends the telemetry session ("bot_timeout") at this sim time
//   -DTBotQuit                          dt.Bot.Quit        quits the process once the run is over and telemetry is written
//   -DTBotOut=<file.csv>                                   writes the bot record (below) at run end
// Bot record (Tools/BotRuns.ps1 reads it): hero events are counted from UDTHeroSubsystem::OnHeroEvent per sim minute,
// because the telemetry CSV does not receive hero events yet (damage_taken_per_min, dashes_per_min stay 0 on main):
//   <file.csv>      minute,t_end_s,dmg_share,dashes,hits,level,hp,alive   (dmg_share = sum of Amount / MaxHp at the hit)
//   <file.csv>.txt  key=value: outcome end_s level_630 steps dash_presses intercession_presses picks declines input_hash
// While active: dt.Offer.AutoPick is forced to 0 (the bot answers the hands), and player 0's pawn stops latching input.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Bot/DTBotBrain.h"
#include "DTBotSubsystem.generated.h"

class UDTSimSubsystem;
class UDTHeroSubsystem;
class UDTOfferSubsystem;

UCLASS()
class DUSKTRACER_API UDTBotSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** Starts driving player 0 with Config (resets the brain and the input log). */
	void Activate(const FDTBotConfig& Config);
	void Deactivate();
	bool IsActive() const { return bActive; }

	/** Opens queued hands and answers every open hand of player 0. Returns the number of hands answered. */
	int32 ServiceOffers();
	/** Runs up to MaxSteps fixed steps now (accelerated time), answering hands between steps; stops when the run ends. */
	int32 RunSteps(int32 MaxSteps);

	/** True once the hero is down, the boss is defeated or the sim-time cap is reached. */
	bool IsRunFinished() const { return bDead || bVictory || bTimedOut; }
	bool IsVictory() const { return bVictory; }

	/** Every latched input folded into one hash (determinism test); InputCount = inputs latched. */
	uint32 GetInputHash() const { return InputHash; }
	int64 GetInputCount() const { return InputCount; }
	const FDTBotBrain& GetBrain() const { return Brain; }
	int32 GetStepsPerFrame() const { return StepsPerFrame; }
	void SetStepsPerFrame(int32 Steps) { StepsPerFrame = FMath::Max(0, Steps); }

	/** Reads -DTBot* and dt.Bot.* into a config (seed from -DTSeed). Policy Off when no bot is requested. */
	static FDTBotConfig ReadConfig();

private:
	void HandleStepEnd();
	void LatchNext();
	void MutePawn();
	void HandleBossDefeated(int64 StepIndex);
	void HandleHeroEvent(const FDTHeroEvent& Event);
	void WriteRecord() const;

	struct FMinute
	{
		double DamageShare = 0.0;
		int32 Dashes = 0;
		int32 Hits = 0;
		int32 Level = 0;
		float Hp = 0.0f;
		int32 Alive = 0;
		double EndSeconds = 0.0;
	};
	TArray<FMinute> Minutes;
	int32 Level630 = 0;
	double EndSeconds = 0.0;
	FString OutPath;
	FDelegateHandle HeroEventHandle;

	FDTBotBrain Brain;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	TWeakObjectPtr<UDTHeroSubsystem> Heroes;
	TWeakObjectPtr<UDTOfferSubsystem> Offers;
	FDelegateHandle StepEndHandle;
	FDelegateHandle BossHandle;
	int32 StepsPerFrame = 0;
	float MaxSeconds = 0.0f;
	int32 FramesSinceEnd = 0;
	uint32 InputHash = 0;
	int64 InputCount = 0;
	bool bActive = false;
	bool bQuit = false;
	bool bPawnMuted = false;
	bool bDead = false;
	bool bVictory = false;
	bool bTimedOut = false;
	bool bExitRequested = false;
};
