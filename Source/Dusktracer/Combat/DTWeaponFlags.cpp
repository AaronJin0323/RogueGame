#include "Combat/DTWeaponFlags.h"

namespace DTFlagParam
{
	const FName Chance(TEXT("chance"));
	const FName Children(TEXT("children"));
	const FName ChildDmg(TEXT("child_dmg"));
	const FName FanDeg(TEXT("fan_deg"));
	const FName Gen(TEXT("gen"));
	const FName Pattern(TEXT("pattern"));
	const FName On(TEXT("on"));
	const FName Trigger(TEXT("trigger"));
	const FName N(TEXT("n"));
	const FName EveryNVolley(TEXT("every_n_volley"));
	const FName Jumps(TEXT("jumps"));
	const FName RangeUu(TEXT("range_uu"));
	const FName Dmg(TEXT("dmg"));
	const FName DelaySeconds(TEXT("delay_s"));
	const FName Count(TEXT("count"));
	const FName RadiusScale(TEXT("radius_scale"));
	const FName RadiusUu(TEXT("radius_uu"));
	const FName DurationS(TEXT("duration_s"));
	const FName Dps(TEXT("dps"));
	const FName MaxLive(TEXT("max_live"));
	const FName Status(TEXT("status"));
	const FName Leave(TEXT("leave"));
	const FName DistanceUu(TEXT("distance_uu"));
	const FName Target(TEXT("target"));
	const FName Rounds(TEXT("rounds"));
	const FName ArcDeg(TEXT("arc_deg"));
	const FName DmgMult(TEXT("dmg_mult"));
	const FName DmgRatio(TEXT("dmg_ratio"));
	const FName Bounces(TEXT("bounces"));
	const FName SeekRangeUu(TEXT("seek_range_uu"));
	const FName FreezeStacks(TEXT("freeze_stacks"));
	const FName FreezeDurationScale(TEXT("freeze_duration_scale"));
	const FName ChillStacksOnPull(TEXT("chill_stacks_on_pull"));
	const FName BurnStacksMax(TEXT("burn_stacks_max"));
	const FName StillMoveUu(TEXT("still_move_uu"));
	const FName ChargeS(TEXT("charge_s"));
	const FName Mult(TEXT("mult"));
	const FName ChargesMax(TEXT("charges_max"));
	const FName ChargedPierce(TEXT("charged_pierce"));
	const FName CrushDmg(TEXT("crush_dmg"));
	const FName PulledVuln(TEXT("pulled_vuln"));
	const FName PulledVulnSeconds(TEXT("pulled_vuln_s"));
	const FName DmgVsShocked(TEXT("dmg_vs_shocked"));
	const FName ReturnSameTarget(TEXT("return_same_target"));
	const FName ForkOnKill(TEXT("fork_on_kill"));
	const FName Retarget(TEXT("retarget"));
	const FName BurningExplodeOnDeathUu(TEXT("burning_explode_on_death_uu"));
	const FName ExplodeDmg(TEXT("explode_dmg"));
	const FName SpreadBurnOnKillUu(TEXT("spread_burn_on_kill_uu"));
	const FName SpreadBurnIntervalS(TEXT("spread_burn_interval_s"));
	const FName ShatterOnFrozenKillUu(TEXT("shatter_on_frozen_kill_uu"));
	const FName ShatterUu(TEXT("shatter_uu"));
	const FName ShatterDmg(TEXT("shatter_dmg"));
	const FName ShatterStatus(TEXT("shatter_status"));
	const FName Shatter(TEXT("shatter"));
	const FName ClusterFreezeMin(TEXT("cluster_freeze_min"));
	const FName FrozenBlocksEnemies(TEXT("frozen_blocks_enemies"));
	const FName FreezeUntilHit(TEXT("freeze_until_hit"));
	const FName HeldMax(TEXT("held_max"));
	const FName FrozenHitChillsNeighboursUu(TEXT("frozen_hit_chills_neighbours_uu"));
	const FName ChargedPullUu(TEXT("charged_pull_uu"));
	const FName ChargedAuraUu(TEXT("charged_aura_uu"));
	const FName AuraStatus(TEXT("aura_status"));
	const FName AuraIntervalS(TEXT("aura_interval_s"));
	const FName ChargedFreezeRingUu(TEXT("charged_freeze_ring_uu"));
	const FName Around(TEXT("around"));
}

EDTParamOp FDTFlagParams::DefaultOp(FName Name)
{
	const FString S = Name.ToString();
	if (S.EndsWith(TEXT("_s")) || S.EndsWith(TEXT("_uu")) || S.Contains(TEXT("range")))
	{
		return EDTParamOp::Max;
	}
	if (S.Contains(TEXT("mult")) || S.Contains(TEXT("dmg")) || S.EndsWith(TEXT("scale")) || S.EndsWith(TEXT("_ratio")))
	{
		return EDTParamOp::Mul;
	}
	return EDTParamOp::Add;
}

void FDTFlagParams::Set(FName Name, float Value)
{
	Set(Name, Value, DefaultOp(Name));
}

void FDTFlagParams::Set(FName Name, float Value, EDTParamOp Op)
{
	for (FNum& Num : Nums)
	{
		if (Num.Name == Name)
		{
			switch (Op)
			{
			case EDTParamOp::Add: Num.Value += Value; break;
			case EDTParamOp::Max: Num.Value = FMath::Max(Num.Value, Value); break;
			case EDTParamOp::Mul: Num.Value *= Value; break;
			}
			return;
		}
	}
	Nums.Add(FNum{ Name, Value, Op });
}

void FDTFlagParams::Replace(FName Name, float Value)
{
	for (FNum& Num : Nums)
	{
		if (Num.Name == Name)
		{
			Num.Value = Value;
			return;
		}
	}
	Nums.Add(FNum{ Name, Value, DefaultOp(Name) });
}

void FDTFlagParams::SetWord(FName Name, FName Value)
{
	for (TPair<FName, FName>& Word : Words)
	{
		if (Word.Key == Name)
		{
			Word.Value = Value;
			return;
		}
	}
	Words.Emplace(Name, Value);
}

float FDTFlagParams::Get(FName Name, float Default) const
{
	for (const FNum& Num : Nums)
	{
		if (Num.Name == Name)
		{
			return Num.Value;
		}
	}
	return Default;
}

bool FDTFlagParams::Has(FName Name) const
{
	return Nums.ContainsByPredicate([Name](const FNum& Num) { return Num.Name == Name; });
}

FName FDTFlagParams::GetWord(FName Name) const
{
	for (const TPair<FName, FName>& Word : Words)
	{
		if (Word.Key == Name)
		{
			return Word.Value;
		}
	}
	return NAME_None;
}

bool FDTFlagParams::WordHas(FName Name, const TCHAR* Token) const
{
	const FName Word = GetWord(Name);
	if (Word.IsNone())
	{
		return false;
	}
	TArray<FString> Parts;
	Word.ToString().ParseIntoArray(Parts, TEXT("|"));
	return Parts.Contains(FString(Token));
}

void FDTFlagParams::Merge(const FDTFlagParams& Other)
{
	for (const FNum& Num : Other.Nums)
	{
		Set(Num.Name, Num.Value, Num.Op);
	}
	for (const TPair<FName, FName>& Word : Other.Words)
	{
		SetWord(Word.Key, Word.Value);
	}
}
