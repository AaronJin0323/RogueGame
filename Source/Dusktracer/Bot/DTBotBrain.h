// Bot decisions (Docs/02_Systems/bot.md): plain C++, no engine state, so the same snapshot and seed give the same input and
// the same card choice. The bot drives player 0 only through FDTHeroInput (combat_core.md R-CMB-12) and the offer API
// (offers.md); it never touches private setters. Version 1 is the S1 bot, frozen (R-BOT-01); version 2 adds the card rules and
// the skill knob, and at skill 1.0 moves as version 1 does (R-BOT-03).
#pragma once

#include "CoreMinimal.h"
#include "Bot/DTBotPerception.h"
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
	/** 1 = the S1 bot, frozen (R-BOT-01); 2 = bot v2. */
	int32 Version = 1;
	/** 1 = the bound: no reaction delay and no error; 0 = the largest. V1 and v2 read it differently (R-BOT-08). */
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
	/** A version the bot knows; anything else is the default, 1. */
	static int32 KnownVersion(int32 Version);
	/** Storm -> Tide, Forge -> Storm, Tide -> Forge (any fixed choice; recorded in Docs/Validation/S1_bot_baseline.md). */
	static EDTPath DefaultSecond(EDTPath Primary);
	FString Describe() const;
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
	/** R-BOT-15: the Reforging card this seed reads (Patron, second Keeper, evolution, cycling); INDEX_NONE with none in the hand. */
	static int32 ReforgePick(const FDTOfferHand& Hand, int32 Seed);

	const FDTBotStats& GetStats() const { return Stats; }
	FDTBotStats& GetStats() { return Stats; }

	/** Danger of standing at P after T seconds (exposed for tests). */
	float DangerAt(const FDTBotWorld& World, const FVector2f& P, float T) const;
	/** True if P lies in the telegraph's footprint grown by Margin. */
	static bool InTelegraph(const FDTTelegraph& Tele, const FVector2f& P, float Margin);

	/** Bot v2 only (R-BOT-09): the card's reach in uu at the level Player owns it (level 1 if not owned), 0 for an unknown id. */
	static float WeaponReach(FName WeaponId, const FDTPlayerRunState& Player);
	/** Bot v2 only (R-BOT-09): the share of the recent range samples that Reach covers. */
	float ReachUse(float Reach) const;
	const FDTBotSkillProfile& GetProfile() const { return Profile; }
	const FDTBotPerception& GetPerception() const { return Perception; }
	/** Bot v2 only (R-BOT-11): the fixed score offset of this card for this run. */
	float CardTaste(FName ContentId) const;
	/** The score a Steered bot gives a card (exposed for tests). */
	float ScoreCard(const FDTOfferCard& Card, const FDTOfferHand& Hand, const FDTPlayerRunState& Player) const;

private:
	float ScoreMove(const FDTBotWorld& World, const FVector2f& Dir, float Speed) const;
	/** R-BOT-14: Dir without the part the arena wall stops; Dir itself where the profile leaves walls unread (v1, skill 1.0). */
	FVector2f OpenPart(const FVector2f& Pos, const FVector2f& Dir) const;
	float ThreatAlong(const FDTBotWorld& World, const FVector2f& Vel, float Horizon, float* OutSoonest) const;
	FVector2f ChooseDash(const FDTBotWorld& World) const;
	/** The kite, pickup and danger scoring shared by both versions: the best direction and the enemies within the cast radius. */
	void ChooseMove(const FDTBotWorld& World, float Speed, FVector2f& OutDir, int32& OutCrowd) const;
	FDTHeroInput DecideV2(const FDTBotWorld& Now);
	void SampleReach(const FDTBotWorld& Now);
	/** One step of the probe route (FDTBotConfig::bProbeRoute). */
	FDTHeroInput ProbeInput(const FDTBotWorld& World);
	float CardScore(const FDTOfferCard& Card, const FDTOfferHand& Hand, const FDTPlayerRunState& Player) const;
	float CardScoreV2(const FDTOfferCard& Card, const FDTOfferHand& Hand, const FDTPlayerRunState& Player) const;
	int32 ReactionSteps() const;

	FDTBotConfig Config;
	/** Skill 1.0 for v1, which reads only its heuristics here; the config's skill for v2. */
	FDTBotSkillProfile Profile;
	FDTBotPerception Perception;
	FRandomStream MoveRng;
	FRandomStream CardRng;
	FVector2f LastDir = FVector2f::ZeroVector;
	FDTHeroInput Held;
	int64 NextDecideStep = 0;
	int32 ImminentSteps = 0;
	float StickErr = 0.0f;
	/** Ring of nearest-enemy rim distances (R-BOT-09). */
	TArray<float> ReachSamples;
	int32 ReachNext = 0;
	FDTBotStats Stats;
};
