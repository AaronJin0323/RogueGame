// What the bot sees (Docs/02_Systems/bot.md). FDTBotWorld is the truth at a step end; FDTBotSkillProfile and FDTBotPerception
// are Bot v2's skill knob: lower skill sees that truth later and less of it (R-BOT-04..06). Plain C++, no engine state.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSimTypes.h"
#include "Math/RandomStream.h"
#include "Player/DTHeroTypes.h"

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

/**
 * Bot v2 at one skill (R-BOT-04..11). The defaults are skill 1.0, which is also Bot v1's heuristics (tool parameters, never
 * game numbers): v1 reads these fields and nothing below them changes it.
 */
struct DUSKTRACER_API FDTBotSkillProfile
{
	/** R-BOT-04: how old the world the bot decides on is; how long an input is held. */
	float ReactionS = 0.0f;
	float HoldS = 0.0f;
	/** R-BOT-04: clearance added to every collision radius; a player who reacts late leaves more room. */
	float MarginUU = 18.0f;
	/** R-BOT-14: how far inside the clearance a body stops counting once the path is leaving it; a blocked wall move is no move. 0 at skill 1.0, v1's reading. */
	float LeaveUU = 0.0f;
	/** R-BOT-05: share of telegraphs missed. */
	float CueMissP = 0.0f;
	/** R-BOT-06: enemy rounds tracked, how far threats are considered, look-ahead for moving threats. */
	int32 AttentionRounds = MAX_int32;
	float ThreatRangeUU = 1400.0f;
	float HorizonS = 0.6f;
	/** R-BOT-07: spread of the drifting stick error; chance a dash is aimed; a threat this close in time asks for a dash; Intercession at this many enemies near. */
	float StickErrDeg = 0.0f;
	float DashPlanP = 1.0f;
	float ImminentS = 0.22f;
	int32 CastCrowd = 12;
	/** R-BOT-11: the largest per-card score offset; the weight of the fit terms. */
	float TasteScore = 0.0f;
	float FitAwareness = 1.0f;

	/** Clamps Skill to 0..1; at 1 exactly the defaults. */
	static FDTBotSkillProfile FromSkill(float Skill);
};

/** The world through a profile: delayed and carried forward (R-BOT-04), rounds attended (R-BOT-06), cues judged once (R-BOT-05). */
class DUSKTRACER_API FDTBotPerception
{
public:
	void Reset(const FDTBotSkillProfile& InProfile, int32 Seed);
	/** Records this step's world. Call once per step, in step order. */
	void Push(const FDTBotWorld& Now);
	/** The world to decide on. Its views stay valid until the next call. Hero, gems and chests are Now's. */
	FDTBotWorld Perceive(const FDTBotWorld& Now);
	/** True when the profile changes nothing: Perceive returns Now itself. */
	bool IsPassThrough() const { return bPassThrough; }
	/** Telegraphs judged so far and how many of them were missed (T-BOT-05). */
	int32 GetCuesJudged() const { return CuesJudged; }
	int32 GetCuesMissed() const { return CuesMissed; }

private:
	struct FFrame
	{
		TArray<FVector2f> EnemyPos;
		TArray<FVector2f> EnemyVel;
		TArray<float> EnemyRadius;
		TArray<FVector2f> RoundPos;
		TArray<FVector2f> RoundVel;
		TArray<float> RoundRadius;
		TArray<FDTTelegraph> Telegraphs;
	};
	struct FCue
	{
		EDTTelegraphShape Shape = EDTTelegraphShape::Band;
		FVector2f Origin = FVector2f::ZeroVector;
		FVector2f Dir = FVector2f::ZeroVector;
		float Progress = 0.0f;
		bool bSeen = true;
	};

	void JudgeCues(TConstArrayView<FDTTelegraph> Telegraphs);

	FDTBotSkillProfile Profile;
	FRandomStream CueRng;
	bool bPassThrough = true;
	int32 DelaySteps = 0;
	TArray<FFrame> Frames;
	int32 Head = INDEX_NONE;
	int32 Count = 0;
	TArray<FCue> Cues;
	TArray<FCue> CuesNext;
	TArray<bool> CueTaken;
	TArray<TPair<float, int32>> RoundOrder;
	FFrame Out;
	int32 CuesJudged = 0;
	int32 CuesMissed = 0;
};
