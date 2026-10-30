#include "Bot/DTBotPerception.h"
#include "Core/DTConstantsGen.h"

// Skill 0 of the profile. Tool parameters of the measuring instrument, not game tuning (bot.md Numbers): no gameplay value is
// read from here. A change is recorded in Docs/Validation/S2_bot_v2.md.
namespace DTBotSkillTuning
{
	constexpr float MaxReactionS = 0.45f;
	constexpr float MaxHoldS = 0.18f;
	constexpr float MaxCueMissP = 0.5f;
	/** Rounds tracked at skill 0 and just below skill 1 (at 1 there is no limit). */
	constexpr int32 MinAttentionRounds = 10;
	constexpr int32 MaxAttentionRounds = 80;
	constexpr float MinThreatRangeUU = 800.0f;
	constexpr float MinHorizonS = 0.3f;
	constexpr float MaxStickErrDeg = 30.0f;
	constexpr float MinImminentS = 0.12f;
	constexpr int32 MaxCastCrowd = 24;
	constexpr float MaxTasteScore = 30.0f;
	constexpr int32 CueSalt = 0x0C0E;
	/** A telegraph within this of a known one, with its direction and no earlier progress, is that cue. */
	constexpr float CueMatchUU = 150.0f;
	constexpr float CueMatchDirCos = 0.7f;
}

FDTBotSkillProfile FDTBotSkillProfile::FromSkill(float Skill)
{
	using namespace DTBotSkillTuning;
	FDTBotSkillProfile Top;
	const float S = FMath::Clamp(Skill, 0.0f, 1.0f);
	if (S >= 1.0f)
	{
		return Top;
	}
	const float D = 1.0f - S;
	FDTBotSkillProfile P;
	P.ReactionS = D * MaxReactionS;
	P.HoldS = D * MaxHoldS;
	P.CueMissP = D * MaxCueMissP;
	P.AttentionRounds = FMath::RoundToInt32(FMath::Lerp(static_cast<float>(MinAttentionRounds), static_cast<float>(MaxAttentionRounds), S));
	P.ThreatRangeUU = FMath::Lerp(MinThreatRangeUU, Top.ThreatRangeUU, S);
	P.HorizonS = FMath::Lerp(MinHorizonS, Top.HorizonS, S);
	P.StickErrDeg = D * MaxStickErrDeg;
	P.DashPlanP = S;
	P.ImminentS = FMath::Lerp(MinImminentS, Top.ImminentS, S);
	P.CastCrowd = FMath::RoundToInt32(FMath::Lerp(static_cast<float>(MaxCastCrowd), static_cast<float>(Top.CastCrowd), S));
	P.TasteScore = D * MaxTasteScore;
	P.FitAwareness = S;
	return P;
}

void FDTBotPerception::Reset(const FDTBotSkillProfile& InProfile, int32 Seed)
{
	Profile = InProfile;
	CueRng.Initialize(Seed ^ DTBotSkillTuning::CueSalt);
	DelaySteps = FMath::RoundToInt32(Profile.ReactionS * static_cast<float>(DTConst::SIM_FIXED_HZ));
	bPassThrough = DelaySteps == 0 && Profile.CueMissP <= 0.0f && Profile.AttentionRounds == MAX_int32;
	Frames.Reset();
	Frames.SetNum(bPassThrough ? 0 : DelaySteps + 1);
	Head = INDEX_NONE;
	Count = 0;
	Cues.Reset();
	CuesJudged = CuesMissed = 0;
}

void FDTBotPerception::Push(const FDTBotWorld& Now)
{
	if (bPassThrough)
	{
		return;
	}
	Head = (Head + 1) % Frames.Num();
	Count = FMath::Min(Count + 1, Frames.Num());
	FFrame& F = Frames[Head];
	F.EnemyPos = Now.EnemyPos;
	F.EnemyVel = Now.EnemyVel;
	F.EnemyRadius = Now.EnemyRadius;
	F.RoundPos = Now.RoundPos;
	F.RoundVel = Now.RoundVel;
	F.RoundRadius = Now.RoundRadius;
	F.Telegraphs = Now.Telegraphs;
}

void FDTBotPerception::JudgeCues(TConstArrayView<FDTTelegraph> Telegraphs)
{
	using namespace DTBotSkillTuning;
	CuesNext.Reset();
	CueTaken.Reset();
	CueTaken.SetNumZeroed(Cues.Num());
	for (const FDTTelegraph& Tele : Telegraphs)
	{
		FCue Cue;
		Cue.Shape = Tele.Shape;
		Cue.Origin = Tele.Origin;
		Cue.Dir = Tele.Dir;
		Cue.Progress = Tele.Progress;
		bool bKnown = false;
		for (int32 Index = 0; Index < Cues.Num() && !bKnown; ++Index)
		{
			const FCue& Old = Cues[Index];
			if (!CueTaken[Index] && Old.Shape == Tele.Shape && Old.Progress <= Tele.Progress && FVector2f::DistSquared(Old.Origin, Tele.Origin) <= CueMatchUU * CueMatchUU
				&& FVector2f::DotProduct(Old.Dir, Tele.Dir) >= CueMatchDirCos)
			{
				CueTaken[Index] = true;
				Cue.bSeen = Old.bSeen;
				bKnown = true;
			}
		}
		if (!bKnown)
		{
			++CuesJudged;
			Cue.bSeen = Tele.Shape == EDTTelegraphShape::HazardRing || CueRng.FRand() >= Profile.CueMissP;
			CuesMissed += Cue.bSeen ? 0 : 1;
		}
		CuesNext.Add(Cue);
		if (Cue.bSeen)
		{
			Out.Telegraphs.Add(Tele);
		}
	}
	Swap(Cues, CuesNext);
}

FDTBotWorld FDTBotPerception::Perceive(const FDTBotWorld& Now)
{
	if (bPassThrough || Count == 0)
	{
		return Now;
	}
	const int32 Age = FMath::Min(Count - 1, DelaySteps);
	const FFrame& F = Frames[(Head - Age + Frames.Num()) % Frames.Num()];
	const float Carry = static_cast<float>(Age) * Now.StepSeconds;
	const FVector2f HeroPos = Now.Hero->Pos;

	// Bodies: every one, carried along its velocity to now.
	Out.EnemyPos.Reset();
	for (int32 Index = 0; Index < F.EnemyPos.Num(); ++Index)
	{
		Out.EnemyPos.Add(F.EnemyPos[Index] + (F.EnemyVel.IsValidIndex(Index) ? F.EnemyVel[Index] * Carry : FVector2f::ZeroVector));
	}

	// Rounds: carried the same way; the nearest AttentionRounds are tracked.
	RoundOrder.Reset();
	for (int32 Index = 0; Index < F.RoundPos.Num(); ++Index)
	{
		const FVector2f P = F.RoundPos[Index] + (F.RoundVel.IsValidIndex(Index) ? F.RoundVel[Index] * Carry : FVector2f::ZeroVector);
		RoundOrder.Emplace(FVector2f::DistSquared(P, HeroPos), Index);
	}
	if (RoundOrder.Num() > Profile.AttentionRounds)
	{
		RoundOrder.Sort(); // distance, then index: deterministic
		RoundOrder.SetNum(Profile.AttentionRounds, EAllowShrinking::No);
	}
	Out.RoundPos.Reset();
	Out.RoundVel.Reset();
	Out.RoundRadius.Reset();
	for (const TPair<float, int32>& Entry : RoundOrder)
	{
		const int32 Index = Entry.Value;
		const FVector2f Vel = F.RoundVel.IsValidIndex(Index) ? F.RoundVel[Index] : FVector2f::ZeroVector;
		Out.RoundPos.Add(F.RoundPos[Index] + Vel * Carry);
		Out.RoundVel.Add(Vel);
		Out.RoundRadius.Add(F.RoundRadius.IsValidIndex(Index) ? F.RoundRadius[Index] : 0.0f);
	}

	Out.Telegraphs.Reset();
	JudgeCues(F.Telegraphs);

	FDTBotWorld Seen = Now;
	Seen.EnemyPos = Out.EnemyPos;
	Seen.EnemyVel = F.EnemyVel;
	Seen.EnemyRadius = F.EnemyRadius;
	Seen.RoundPos = Out.RoundPos;
	Seen.RoundVel = Out.RoundVel;
	Seen.RoundRadius = Out.RoundRadius;
	Seen.Telegraphs = Out.Telegraphs;
	return Seen;
}
