// Bot v1 decisions (S1 tool, AGENTS queue item 5): plain C++, no engine state, so the same snapshot and seed give the same
// input and the same card choice. The bot drives player 0 only through FDTHeroInput (combat_core.md R-CMB-12) and the
// offer API (offers.md); it never touches private setters.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Math/RandomStream.h"
#include "Player/DTHeroTypes.h"

/** How the bot answers offer hands. */
enum class EDTBotPolicy : uint8
{
	Off,
	/** No-pick arm (director.md R-DIR-01): picks the run-start Patron (slot 0, not a decision, D-013) and declines every other hand. */
	None,
	/** A uniform slot from the bot's own stream. */
	Random,
	/** Prefers the archetype's blessings, takes its Keeper as Patron and a fixed second Keeper. */
	Steered
};

struct DUSKTRACER_API FDTBotConfig
{
	EDTBotPolicy Policy = EDTBotPolicy::Off;
	/** Steered: the Patron Path and archetype name ("Tempest" for Archetype.Storm.Tempest). */
	EDTPath Path = EDTPath::Storm;
	FName Archetype;
	/** Steered: the second Keeper; None = DefaultSecond(Path). */
	EDTPath SecondPath = EDTPath::None;
	/** 1 = no reaction delay and no noise; 0 = the largest delay and noise. */
	float Skill = 1.0f;
	/** Seeds the bot's own streams (never one of the run's streams, R-TEC-02). */
	int32 Seed = 0;
	/** Intercession presses: CastAuto = when the meter is full and a crowd is near; CastNever = none (a probe's no-cast arm);
	 * a step index = one press at the first decision from that step on (a probe's cast). */
	static constexpr int64 CastAuto = -1;
	static constexpr int64 CastNever = -2;
	int64 CastStep = CastAuto;
	/** A probe's fixed route instead of the danger scoring: the same steps for every build, in the crowd (ProbeInput). */
	bool bProbeRoute = false;
	/** Probe route, from CastStep for C.INTERCESSION.FORGE_DURATION_S: a dash every this many steps; 0 = the plain route's dashes. */
	int32 CastDashSteps = 0;

	static const TCHAR* PolicyName(EDTBotPolicy Policy);
	static EDTBotPolicy ParsePolicy(const FString& Text);
	static EDTPath ParsePath(const FString& Text);
	/** Storm -> Tide, Forge -> Storm, Tide -> Forge (any fixed choice; recorded in Docs/Validation/S1_bot_baseline.md). */
	static EDTPath DefaultSecond(EDTPath Primary);
	FString Describe() const;
};

/** What the bot sees at a step end: views into the sim's published data (07_Technical §4) plus the hero state. */
struct FDTBotWorld
{
	const FDTHeroState* Hero = nullptr;
	float StepSeconds = 1.0f / 60.0f;
	int64 StepIndex = 0;
	TConstArrayView<FVector2f> EnemyPos;
	TConstArrayView<FVector2f> EnemyVel;
	TConstArrayView<float> EnemyRadius;
	TConstArrayView<FVector2f> RoundPos;
	TConstArrayView<FVector2f> RoundVel;
	TConstArrayView<float> RoundRadius;
	TConstArrayView<FDTTelegraph> Telegraphs;
	TConstArrayView<FVector2f> Gems;
	TConstArrayView<FVector2f> Chests;
};

/** Counters for DT.Bot.Stats and the run summary. */
struct FDTBotStats
{
	int64 Decisions = 0;
	int32 DashPresses = 0;
	int32 IntercessionPresses = 0;
	int32 Picks = 0;
	int32 Declines = 0;
};

class DUSKTRACER_API FDTBotBrain
{
public:
	void Reset(const FDTBotConfig& InConfig);
	const FDTBotConfig& GetConfig() const { return Config; }

	/** The input for the next step. Call once per step, in step order. */
	FDTHeroInput Decide(const FDTBotWorld& World);

	/** The slot to pick in Hand, or INDEX_NONE to decline it (None policy only; the Patron hand is always picked). */
	int32 ChooseCard(const FDTOfferHand& Hand, const FDTPlayerRunState& Player);

	const FDTBotStats& GetStats() const { return Stats; }
	FDTBotStats& GetStats() { return Stats; }

	/** Danger of standing at P after T seconds (exposed for tests). */
	float DangerAt(const FDTBotWorld& World, const FVector2f& P, float T) const;
	/** True if P lies in the telegraph's footprint grown by Margin. */
	static bool InTelegraph(const FDTTelegraph& Tele, const FVector2f& P, float Margin);

private:
	float ScoreMove(const FDTBotWorld& World, const FVector2f& Dir, float Speed) const;
	float ThreatAlong(const FDTBotWorld& World, const FVector2f& Vel, float Horizon, float* OutSoonest) const;
	FVector2f ChooseDash(const FDTBotWorld& World) const;
	/** One step of the probe route (FDTBotConfig::bProbeRoute). */
	FDTHeroInput ProbeInput(const FDTBotWorld& World);
	float CardScore(const FDTOfferCard& Card, const FDTOfferHand& Hand, const FDTPlayerRunState& Player) const;
	int32 ReactionSteps() const;

	FDTBotConfig Config;
	FRandomStream MoveRng;
	FRandomStream CardRng;
	FVector2f LastDir = FVector2f::ZeroVector;
	FDTHeroInput Held;
	int64 NextDecideStep = 0;
	int32 ImminentSteps = 0;
	FDTBotStats Stats;
};
