#include "Keepers/DTKeeperSim.h"
#include "Combat/DTWeaponSim.h"
#include "Core/DTConstantsGen.h"
#include "Enemies/DTEnemyPool.h"

namespace DTKeeperSimLocal
{
	static const FName ParamFrozenVuln(TEXT("frozen_vuln"));
	static const FName ParamBurnStacksMax(TEXT("burn_stacks_max"));
	static const FName ParamBurnTickRate(TEXT("burn_tick_rate"));
	static const FName ParamFreezeStacks(TEXT("freeze_stacks"));
	static const FName ParamFreezeDurationScale(TEXT("freeze_duration_scale"));
	static const FName ParamChildDmg(TEXT("child_dmg"));
	static const FName ParamChildren(TEXT("children"));
	static const FName ParamChance(TEXT("chance"));
	static const FName ParamGen(TEXT("gen"));
	static const FName ParamJumps(TEXT("jumps"));
	static const FName ParamRangeUu(TEXT("range_uu"));
	static const FName ParamDmg(TEXT("dmg"));
	static const FName ParamDmgRatio(TEXT("dmg_ratio"));
	static const FName ParamDmgMult(TEXT("dmg_mult"));
	static const FName ParamKnockbackUu(TEXT("knockback_uu"));
	static const FName ParamRadiusUu(TEXT("radius_uu"));
	static const FName ParamDurationS(TEXT("duration_s"));
	static const FName ParamNetIntervalS(TEXT("net_interval_s"));
	static const FName ParamLinksPerEnemy(TEXT("links_per_enemy"));
	static const FName ParamIntervalS(TEXT("interval_s"));
	static const FName ParamStacks(TEXT("stacks"));
	static const FName ParamPoolRadiusUu(TEXT("pool_radius_uu"));
	static const FName ParamStillMoveUu(TEXT("still_move_uu"));
	static const FName ParamChargeS(TEXT("charge_s"));
	static const FName ParamBurnToCap(TEXT("burn_to_cap"));
	static const FName ParamGrantCharge(TEXT("grant_charge"));
	static const FName ParamDashCooldownReset(TEXT("dash_cooldown_reset"));
	static const FName WordPattern(TEXT("pattern"));
	static const FName WordFan(TEXT("Fan"));
	static const FName WordOn(TEXT("on"));
	static const FName WordPer(TEXT("per"));
	static const FName WordTarget(TEXT("target"));
	static const FName WordTrigger(TEXT("trigger"));
	static const FName WordStatus(TEXT("status"));
	static const FName WordSinglePoint(TEXT("SinglePoint"));
	static const FName WordIntercessionHit(TEXT("IntercessionHit"));
	static const FName WordCast(TEXT("Cast"));

	static FDTSpecClause MakeClause(DTCat::EFlag Flag)
	{
		FDTSpecClause Clause;
		Clause.Flag = Flag;
		return Clause;
	}

	static void AddNum(FDTSpecClause& Clause, FName Name, float Value)
	{
		Clause.Nums.Add(FDTSpecNum{ Name, Value, false });
	}

	static bool HasTrigger(const FDTSpecClause& Clause, const TCHAR* Token)
	{
		TArray<FString> Parts;
		Clause.GetWord(WordTrigger).ToString().ParseIntoArray(Parts, TEXT("|"));
		return Parts.Contains(FString(Token));
	}

	static const FDTSpecClause* FindClause(const FDTEffectSpec& Spec, DTCat::EFlag Flag)
	{
		return Spec.Clauses.FindByPredicate([Flag](const FDTSpecClause& C) { return C.Flag == Flag; });
	}

	static int32 NumEnemies(const FDTWorldView& View)
	{
		return FMath::Min3(View.EnemyPositions.Num(), View.EnemySerials.Num(), View.EnemyRadii.Num());
	}

	static FVector2f HeroOf(const FDTWorldView& View, int32 OwnerId)
	{
		return View.HeroPositions.IsValidIndex(OwnerId) ? View.HeroPositions[OwnerId] : FVector2f::ZeroVector;
	}

	static FVector2f AimOf(const FDTWorldView& View, int32 OwnerId)
	{
		const FVector2f Aim = View.HeroAimDirs.IsValidIndex(OwnerId) ? View.HeroAimDirs[OwnerId] : FVector2f(1.0f, 0.0f);
		return Aim.IsNearlyZero() ? FVector2f(1.0f, 0.0f) : Aim.GetSafeNormal();
	}

	static bool IsPath(EDTPath Path)
	{
		return Path != EDTPath::None && Path < EDTPath::Count;
	}
}

// ---- players and flags -------------------------------------------------------------------------------------------------------

FDTKeeperSim::FPlayer& FDTKeeperSim::EnsurePlayer(int32 OwnerId)
{
	if (Players.Num() <= OwnerId)
	{
		Players.SetNum(OwnerId + 1);
	}
	return Players[OwnerId];
}

const FDTKeeperCounters& FDTKeeperSim::GetCounters(int32 OwnerId) const
{
	static const FDTKeeperCounters None;
	return Players.IsValidIndex(OwnerId) ? Players[OwnerId].Counters : None;
}

const FDTWeaponFlags& FDTKeeperSim::GetBuiltFlags(int32 OwnerId, EDTPath Path) const
{
	static const FDTWeaponFlags Empty;
	return Players.IsValidIndex(OwnerId) && DTKeeperSimLocal::IsPath(Path) ? Players[OwnerId].Built[static_cast<int32>(Path)] : Empty;
}

void FDTKeeperSim::RebuildFlags(const FDTRunState& Run, int32 OwnerId, FDTWeaponSim& Weapons)
{
	if (!Run.IsValidPlayer(OwnerId))
	{
		return;
	}
	FPlayer& Player = EnsurePlayer(OwnerId);
	const FDTKeeperRunState& Keepers = Run.GetPlayer(OwnerId).Keepers;
	for (int32 P = 0; P < static_cast<int32>(EDTPath::Count); ++P)
	{
		const EDTPath Path = static_cast<EDTPath>(P);
		Player.Built[P] = DTKeepers::BuildKeeperFlags(Keepers, Path, Player.Overlay[P]);
		// One source per Keeper, replaced on every rebuild: a blessing applies once to every weapon, owned or later.
		if (Player.Built[P].Entries.Num() > 0)
		{
			Weapons.AddFlagSource(OwnerId, DTKeepers::SourceTag(Path), Player.Built[P]);
		}
		else
		{
			Weapons.RemoveFlagSource(OwnerId, DTKeepers::SourceTag(Path));
		}
	}
	++Player.Counters.Rebuilds;
}

void FDTKeeperSim::QueueIntercession(int32 OwnerId)
{
	if (OwnerId >= 0)
	{
		EnsurePlayer(OwnerId).bQueued = true;
	}
}

bool FDTKeeperSim::IsCasting(int32 OwnerId) const
{
	return Players.IsValidIndex(OwnerId) && Players[OwnerId].Cast.bActive;
}

const FDTIntercessionPlan& FDTKeeperSim::GetActivePlan(int32 OwnerId) const
{
	static const FDTIntercessionPlan None;
	return IsCasting(OwnerId) ? Players[OwnerId].Cast.Plan : None;
}

void FDTKeeperSim::ResetRun()
{
	Captured.Reset();
	Finished.Reset();
	DashResets.Reset();
	for (FPlayer& Player : Players)
	{
		Player.Cast = FCast();
		Player.bQueued = false;
		for (TArray<FDTSpecClause>& Overlay : Player.Overlay)
		{
			Overlay.Reset();
		}
		Player.Counters = FDTKeeperCounters();
	}
	LastStep = -1;
}

// ---- capture and enchantment ---------------------------------------------------------------------------------------------------

void FDTKeeperSim::CaptureHits(const FDTSimContext& Context, const FDTEnemyPool* Enemies)
{
	if (!Context.Run || !Context.View)
	{
		return;
	}
	if (Context.StepIndex <= LastStep)
	{
		ResetRun(); // the clock went back: a new run started
	}
	Captured.Reset();
	const FDTWorldView& View = *Context.View;
	for (const FDTHit& Hit : View.Hits)
	{
		// Player hits only: a weapon (or a flag reaction on a weapon) made it. Status ticks, Keeper-only hits and stress
		// fire carry no slot, so procs never feed on themselves; a field's tick is not a hit (R-KEE-16).
		if (Hit.WeaponSlot == INDEX_NONE || Hit.bFieldTick || !Context.Run->IsValidPlayer(Hit.OwnerId))
		{
			continue;
		}
		uint32 Serial = Hit.EnemySerial;
		if (Serial == 0 && View.EnemySerials.IsValidIndex(Hit.EnemyIndex))
		{
			Serial = View.EnemySerials[Hit.EnemyIndex];
		}
		if (Serial == 0)
		{
			continue;
		}
		FCapturedHit& Out = Captured.AddDefaulted_GetRef();
		Out.OwnerId = Hit.OwnerId;
		Out.EnemySerial = Serial;
		Out.Damage = Hit.Damage;
		Out.Position = Hit.Position;
		if (Enemies)
		{
			const FDTEnemyHandle Handle{ Hit.EnemyIndex, Serial };
			Out.bFrozen = Enemies->IsHandleValid(Handle) && Enemies->GetAux(Hit.EnemyIndex).IsFrozen();
		}
	}
}

void FDTKeeperSim::BuildSerialIndex(const FDTWorldView& View)
{
	SerialToIndex.Reset();
	const int32 Num = DTKeeperSimLocal::NumEnemies(View);
	for (int32 Enemy = 0; Enemy < Num; ++Enemy)
	{
		SerialToIndex.Add(View.EnemySerials[Enemy], Enemy);
	}
}

int32 FDTKeeperSim::IndexOfSerial(uint32 Serial) const
{
	const int32* Found = SerialToIndex.Find(Serial);
	return Found ? *Found : INDEX_NONE;
}

void FDTKeeperSim::AppendStatus(FDTWorldView& View, FPlayer& Player, EDTStatus Status, int32 Enemy, uint32 Serial, float HitDamage,
	int32 OwnerId, EDTPath Path, float Stacks)
{
	using namespace DTKeeperSimLocal;
	FDTStatusApply& Apply = View.StatusApplies.AddDefaulted_GetRef();
	Apply.Handle = FDTEnemyHandle{ Enemy, Serial };
	Apply.Status = Status;
	Apply.OwnerId = OwnerId;
	Apply.SourceId = DTKeepers::KeeperSource(Path);
	// Riders travel with every application of this owner (R-KEE-11, R-KEE-12); enemies.md owns what they do.
	float StackBonus = 0.0f;
	float BurnRate = 0.0f;
	float FreezeAt = 0.0f;
	float FreezeScale = 0.0f;
	for (const FDTWeaponFlags& Built : Player.Built)
	{
		for (const FDTFlagEntry& Entry : Built.Entries)
		{
			StackBonus = FMath::Max(StackBonus, Entry.Params.Get(ParamBurnStacksMax));
			BurnRate = FMath::Max(BurnRate, Entry.Params.Get(ParamBurnTickRate));
			if (Entry.Flag == DTCat::EFlag::StatusApply)
			{
				const float At = Entry.Params.Get(ParamFreezeStacks);
				FreezeAt = At > 0.0f && (FreezeAt <= 0.0f || At < FreezeAt) ? At : FreezeAt;
				FreezeScale = FMath::Max(FreezeScale, Entry.Params.Get(ParamFreezeDurationScale));
			}
		}
	}
	switch (Status)
	{
	case EDTStatus::Burn:
		Apply.Magnitude = DTConst::STATUS_BURN_DPS_RATIO * HitDamage * (1.0f + BurnRate);
		Apply.StackBonus = FMath::RoundToInt(StackBonus);
		break;
	case EDTStatus::Chill:
		Apply.Magnitude = Stacks;
		Apply.FreezeAt = FMath::RoundToInt(FreezeAt);
		Apply.FreezeScale = 1.0f + FreezeScale;
		break;
	case EDTStatus::Freeze:
		Apply.FreezeScale = Stacks;
		break;
	default:
		break;
	}
}

void FDTKeeperSim::Enchant(const FDTSimContext& Context, FPlayer& Player, const FCapturedHit& Hit, int32 Enemy)
{
	const FDTKeeperRunState& Keepers = Context.Run->GetPlayer(Hit.OwnerId).Keepers;
	FRandomStream& Rng = Context.Run->GetRng().Get(EDTRngStream::Combat);
	++Player.Counters.HitsRolled;
	EDTStatus Status;
	// R-KEE-09: primary from the first blessing; both rolls always draw, so the stream use never depends on the result.
	const bool bPrimaryHit = Rng.FRand() < DTConst::ENCHANT_STATUS_CHANCE_PRIMARY;
	if (bPrimaryHit && DTKeepers::EnchantStatus(Keepers.Primary, Status))
	{
		AppendStatus(*Context.View, Player, Status, Enemy, Hit.EnemySerial, Hit.Damage, Hit.OwnerId, Keepers.Primary);
		++Player.Counters.PrimaryProcs[static_cast<int32>(Status)];
	}
	const bool bCommitted = Keepers.bCommitDone && DTKeeperSimLocal::IsPath(Keepers.Second);
	if (!bCommitted)
	{
		return;
	}
	const bool bSecondHit = Rng.FRand() < DTConst::ENCHANT_STATUS_CHANCE_SECOND;
	if (bSecondHit && DTKeepers::EnchantStatus(Keepers.Second, Status))
	{
		AppendStatus(*Context.View, Player, Status, Enemy, Hit.EnemySerial, Hit.Damage, Hit.OwnerId, Keepers.Second);
		++Player.Counters.SecondProcs[static_cast<int32>(Status)];
	}
}

void FDTKeeperSim::StepEffects(const FDTSimContext& Context, FDTWeaponSim* Weapons)
{
	using namespace DTKeeperSimLocal;
	if (!Context.Run || !Context.View)
	{
		return;
	}
	if (Context.StepIndex < LastStep)
	{
		ResetRun();
	}
	FDTRunState& Run = *Context.Run;
	FDTWorldView& View = *Context.View;
	if (Run.GetNumPlayers() > 0)
	{
		EnsurePlayer(Run.GetNumPlayers() - 1);
	}
	BuildSerialIndex(View);

	// Who is enchanted this step (once per player, not per hit).
	TArray<bool> Enchanted;
	TArray<float> FrozenVuln;
	TArray<EDTPath> VulnPath;
	Enchanted.Init(false, Run.GetNumPlayers());
	FrozenVuln.Init(0.0f, Run.GetNumPlayers());
	VulnPath.Init(EDTPath::None, Run.GetNumPlayers());
	for (int32 Owner = 0; Owner < Run.GetNumPlayers(); ++Owner)
	{
		const FDTKeeperRunState& Keepers = Run.GetPlayer(Owner).Keepers;
		Enchanted[Owner] = IsPath(Keepers.Primary) && DTKeepers::OwnedOfPath(Keepers, Keepers.Primary) > 0;
		for (int32 P = 0; P < static_cast<int32>(EDTPath::Count); ++P)
		{
			for (const FDTFlagEntry& Entry : Players[Owner].Built[P].Entries)
			{
				const float Vuln = Entry.Flag == DTCat::EFlag::StatusApply ? Entry.Params.Get(ParamFrozenVuln) : 0.0f;
				if (Vuln > FrozenVuln[Owner])
				{
					FrozenVuln[Owner] = Vuln;
					VulnPath[Owner] = static_cast<EDTPath>(P);
				}
			}
		}
	}

	for (const FCapturedHit& Hit : Captured)
	{
		const int32 Enemy = IndexOfSerial(Hit.EnemySerial);
		if (Enemy == INDEX_NONE || !Run.IsValidPlayer(Hit.OwnerId))
		{
			continue; // died this step
		}
		FPlayer& Player = Players[Hit.OwnerId];
		if (Enchanted[Hit.OwnerId])
		{
			Enchant(Context, Player, Hit, Enemy);
		}
		if (Player.Cast.bActive && Player.Cast.Plan.Base == EDTPath::Storm)
		{
			ApplyLayerStatus(Context, Hit.OwnerId, Player, Enemy, Hit.Damage); // a Storm cast's hits are its rounds', never its own
		}
		// Frost: a Frozen enemy takes frozen_vuln more from every hit; the extra is Keeper damage (R-KEE-13).
		if (Hit.bFrozen && FrozenVuln[Hit.OwnerId] > 0.0f)
		{
			FDTHit& Out = View.Hits.AddDefaulted_GetRef();
			Out.EnemyIndex = Enemy;
			Out.EnemySerial = Hit.EnemySerial;
			Out.OwnerId = Hit.OwnerId;
			Out.Damage = Hit.Damage * FrozenVuln[Hit.OwnerId];
			Out.Position = View.EnemyPositions[Enemy];
			Out.SourceId = DTKeepers::KeeperSource(VulnPath[Hit.OwnerId]);
			++Player.Counters.FrozenVulnHits;
		}
	}
	Captured.Reset();

	for (int32 Owner = 0; Owner < Run.GetNumPlayers(); ++Owner)
	{
		FPlayer& Player = Players[Owner];
		if (Player.bQueued)
		{
			Player.bQueued = false;
			if (Player.Cast.bActive)
			{
				EndCast(Context, Owner, Player, Weapons);
			}
			StartCast(Context, Owner, Player, Weapons);
		}
		if (Player.Cast.bActive)
		{
			StepCast(Context, Owner, Player, Weapons);
		}
	}
	LastStep = Context.StepIndex;
}

// ---- Intercession --------------------------------------------------------------------------------------------------------------

float FDTKeeperSim::StrongestDamage(const FDTSimContext& Context, int32 OwnerId, const FDTWeaponSim* Weapons) const
{
	float Best = 0.0f;
	if (!Weapons || !Context.Run->IsValidPlayer(OwnerId))
	{
		return Best;
	}
	for (int32 Slot = 0; Slot < Context.Run->GetPlayer(OwnerId).Weapons.Num(); ++Slot)
	{
		Best = FMath::Max(Best, Weapons->GetLastSpec(OwnerId, Slot).Damage);
	}
	return Best;
}

void FDTKeeperSim::StartCast(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, FDTWeaponSim* Weapons)
{
	using namespace DTKeeperSimLocal;
	const FDTKeeperRunState& Keepers = Context.Run->GetPlayer(OwnerId).Keepers;
	FCast& Cast = Player.Cast;
	Cast = FCast();
	Cast.Plan = DTKeepers::ComposeIntercession(Keepers);
	if (!IsPath(Cast.Plan.Base))
	{
		return; // no Patron yet: nothing to cast
	}
	Cast.bActive = true;
	++Player.Counters.Casts;
	for (TArray<FDTSpecClause>& Overlay : Player.Overlay)
	{
		Overlay.Reset();
	}
	const FDTIntercessionPlan& Plan = Cast.Plan;
	const int32 Base = static_cast<int32>(Plan.Base);

	// Base (R-KEE-14 table), shaped by the variant.
	switch (Plan.Base)
	{
	case EDTPath::Storm:
	{
		// Every round fired Splits into C.INTERCESSION.STORM_SPLIT_CHILDREN and each child Chains once.
		FDTSpecClause Split = MakeClause(DTCat::EFlag::Split);
		AddNum(Split, ParamChildren, static_cast<float>(DTConst::INTERCESSION_STORM_SPLIT_CHILDREN));
		AddNum(Split, ParamChance, 1.0f);
		AddNum(Split, ParamGen, 1.0f);
		AddNum(Split, ParamChildDmg, DTKeepers::PathParam(EDTPath::Storm, DTCat::EFlag::Split, ParamChildDmg, 1.0f));
		Split.Words.Emplace(WordPattern, WordFan);
		FDTSpecClause Chain = MakeClause(DTCat::EFlag::Chain);
		AddNum(Chain, ParamJumps, 1.0f);
		AddNum(Chain, ParamRangeUu, DTKeepers::PathParam(EDTPath::Storm, DTCat::EFlag::Chain, ParamRangeUu, 0.0f));
		AddNum(Chain, ParamDmg, DTKeepers::PathParam(EDTPath::Storm, DTCat::EFlag::Chain, ParamDmg, 1.0f));
		Player.Overlay[Base].Add(Split);
		Player.Overlay[Base].Add(Chain);
		Player.Overlay[Base].Append(Plan.VariantSpec.Clauses); // Tempest's fan, Downpour's echoes, Arc Grid's net
		Cast.TimeLeft = DTConst::INTERCESSION_STORM_DURATION_S;
		break;
	}
	case EDTPath::Forge:
		// Rings in StepCast; Hammer's variant turns cleaves into full circles for the window.
		for (const FDTSpecClause& Clause : Plan.VariantSpec.Clauses)
		{
			if (Clause.Flag == DTCat::EFlag::ConvertMelee)
			{
				FDTSpecClause Cleave = Clause;
				Cleave.RemoveWord(WordPer);
				Player.Overlay[Base].Add(Cleave);
			}
		}
		Cast.TimeLeft = DTConst::INTERCESSION_FORGE_DURATION_S;
		break;
	case EDTPath::Tide:
		TidePull(Context, OwnerId, Player);
		Cast.TimeLeft = DTConst::INTERCESSION_TIDE_FREEZE_S;
		if (const FDTSpecClause* Still = FindClause(Plan.VariantSpec, DTCat::EFlag::Stillness))
		{
			// Stillness: moving inside the pool counts as standing still and every volley is charged.
			FDTSpecClause Pool = *Still;
			Pool.Nums.Reset();
			AddNum(Pool, ParamStillMoveUu, Still->GetNum(ParamPoolRadiusUu));
			AddNum(Pool, ParamChargeS, Context.StepSeconds);
			Player.Overlay[Base].Add(Pool);
		}
		if (const FDTSpecClause* Frost = FindClause(Plan.VariantSpec, DTCat::EFlag::StatusApply))
		{
			if (Frost->HasNum(ParamFrozenVuln))
			{
				FDTSpecClause Vuln = MakeClause(DTCat::EFlag::StatusApply);
				AddNum(Vuln, ParamFrozenVuln, Frost->GetNum(ParamFrozenVuln));
				Player.Overlay[Base].Add(Vuln);
			}
		}
		break;
	default:
		break;
	}
	for (const FDTSpecClause& Clause : Plan.VariantSpec.Clauses)
	{
		if (Clause.HasNum(ParamDurationS))
		{
			Cast.TimeLeft = Clause.GetNum(ParamDurationS); // Furnace, Frost, Stillness name their window
		}
	}

	// Layer of the second Keeper (from the commitment): its hits carry the second's source.
	if (Plan.Layer != INDEX_NONE && IsPath(Plan.LayerPath))
	{
		const int32 Second = static_cast<int32>(Plan.LayerPath);
		for (const FDTSpecClause& Clause : Plan.LayerSpec.Clauses)
		{
			const FName On = Clause.GetWord(WordOn);
			if (Clause.Flag == DTCat::EFlag::StatusApply && On == WordIntercessionHit)
			{
				continue; // rides on the cast's own hits (ApplyLayerStatus)
			}
			if (Clause.Flag == DTCat::EFlag::ConvertMelee && Clause.HasNum(ParamIntervalS))
			{
				continue; // Hammer layer: a full cleave every interval_s (StepCast)
			}
			if (Clause.Flag == DTCat::EFlag::Slam)
			{
				if (HasTrigger(Clause, TEXT("CastStart")))
				{
					HitCircle(Context, OwnerId, Player, HeroOf(*Context.View, OwnerId), Clause.GetNum(ParamRadiusUu),
						StrongestDamage(Context, OwnerId, Weapons) * Clause.GetNum(ParamDmgRatio, 1.0f), Plan.LayerPath, 0.0f, false, 0);
				}
				continue; // CastEnd in EndCast
			}
			if (Clause.Flag == DTCat::EFlag::Stillness && (On == WordCast || Clause.GetNum(ParamGrantCharge) > 0.0f))
			{
				// Grant a charge: one charge interval of standing still anywhere, then the overlay goes (StepCast).
				FDTSpecClause Grant = MakeClause(DTCat::EFlag::Stillness);
				AddNum(Grant, ParamStillMoveUu, TNumericLimits<float>::Max());
				AddNum(Grant, ParamChargeS, Context.StepSeconds);
				Player.Overlay[Second].Add(Grant);
				Cast.LayerChargeSteps = 2; // the anchor step, then the charging step (FDTWeaponSim::StepStillness)
				continue;
			}
			Player.Overlay[Second].Add(Clause);
		}
	}
	if (Weapons)
	{
		RebuildFlags(*Context.Run, OwnerId, *Weapons);
	}
}

void FDTKeeperSim::StepCast(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, FDTWeaponSim* Weapons)
{
	using namespace DTKeeperSimLocal;
	FCast& Cast = Player.Cast;
	const FDTIntercessionPlan& Plan = Cast.Plan;
	const FDTWorldView& View = *Context.View;
	const float Dt = Context.StepSeconds;
	const FVector2f Hero = HeroOf(View, OwnerId);
	const bool bIntangible = View.HeroIntangible.IsValidIndex(OwnerId) && View.HeroIntangible[OwnerId];
	const bool bDashLanding = Cast.bWasIntangible && !bIntangible;
	Cast.bWasIntangible = bIntangible;

	if (Plan.Base == EDTPath::Forge)
	{
		// A fire Slam ring every interval, knocking back and Burning (R-KEE-14 table).
		const float Damage = StrongestDamage(Context, OwnerId, Weapons) *
			DTKeepers::PathParam(EDTPath::Forge, DTCat::EFlag::Slam, ParamDmgRatio, 1.0f);
		float Knockback = DTKeepers::PathParam(EDTPath::Forge, DTCat::EFlag::ConvertMelee, ParamKnockbackUu, 0.0f);
		int32 BurnApplications = 1;
		const FDTSpecClause* SlamVariant = nullptr;
		for (const FDTSpecClause& Clause : Plan.VariantSpec.Clauses)
		{
			Knockback = Clause.HasNum(ParamKnockbackUu) ? Clause.GetNum(ParamKnockbackUu) : Knockback;
			if (Clause.GetNum(ParamBurnToCap) > 0.0f)
			{
				BurnApplications = DTConst::STATUS_BURN_STACKS_MAX; // Furnace: every ring burns to the cap
			}
			SlamVariant = Clause.Flag == DTCat::EFlag::Slam ? &Clause : SlamVariant;
		}
		Cast.RingTimer -= Dt;
		if (Cast.RingTimer <= 0.0f)
		{
			Cast.RingTimer += DTConst::INTERCESSION_FORGE_RING_INTERVAL_S;
			HitCircle(Context, OwnerId, Player, Hero, DTConst::INTERCESSION_FORGE_RING_RADIUS_UU, Damage, EDTPath::Forge, Knockback, true, BurnApplications);
		}
		if (SlamVariant && bDashLanding && HasTrigger(*SlamVariant, TEXT("DashLanding")))
		{
			HitCircle(Context, OwnerId, Player, Hero, SlamVariant->GetNum(ParamRadiusUu, DTConst::INTERCESSION_FORGE_RING_RADIUS_UU),
				Damage, EDTPath::Forge, Knockback, true, BurnApplications);
			if (SlamVariant->GetNum(ParamDashCooldownReset) > 0.0f)
			{
				DashResets.Add(OwnerId);
			}
		}
	}
	if (Plan.Base == EDTPath::Storm)
	{
		// Arc Grid: every net_interval_s each enemy on screen takes links_per_enemy chain links.
		if (const FDTSpecClause* Net = FindClause(Plan.VariantSpec, DTCat::EFlag::Chain))
		{
			const float Interval = Net->GetNum(ParamNetIntervalS);
			if (Interval > 0.0f)
			{
				Cast.NetTimer -= Dt;
				if (Cast.NetTimer <= 0.0f)
				{
					Cast.NetTimer += Interval;
					const float Link = StrongestDamage(Context, OwnerId, Weapons) * DTKeepers::PathParam(EDTPath::Storm, DTCat::EFlag::Chain, ParamDmg, 1.0f);
					const int32 Links = FMath::Max(1, FMath::RoundToInt(Net->GetNum(ParamLinksPerEnemy, 1.0f)));
					HitCircle(Context, OwnerId, Player, Hero, DTConst::ARENA_CAMERA_FRAME_RADIUS_UU, Link * static_cast<float>(Links),
						EDTPath::Storm, 0.0f, false, 0);
				}
			}
		}
	}
	// Hammer layer: a full cleave around the hero every interval_s, as the second Keeper.
	if (Plan.Layer != INDEX_NONE)
	{
		for (const FDTSpecClause& Clause : Plan.LayerSpec.Clauses)
		{
			if (Clause.Flag != DTCat::EFlag::ConvertMelee || !Clause.HasNum(ParamIntervalS))
			{
				continue;
			}
			Cast.LayerTimer -= Dt;
			if (Cast.LayerTimer <= 0.0f)
			{
				Cast.LayerTimer += Clause.GetNum(ParamIntervalS);
				HitCircle(Context, OwnerId, Player, Hero, Clause.GetNum(ParamRadiusUu),
					StrongestDamage(Context, OwnerId, Weapons) * Clause.GetNum(ParamDmgMult, 1.0f), Plan.LayerPath, 0.0f, false, 0);
			}
		}
	}
	if (Cast.LayerChargeSteps > 0 && --Cast.LayerChargeSteps == 0 && IsPath(Plan.LayerPath))
	{
		Player.Overlay[static_cast<int32>(Plan.LayerPath)].RemoveAll([](const FDTSpecClause& C)
		{
			return C.Flag == DTCat::EFlag::Stillness && C.GetNum(ParamStillMoveUu) == TNumericLimits<float>::Max();
		});
		if (Weapons)
		{
			RebuildFlags(*Context.Run, OwnerId, *Weapons);
		}
	}
	Cast.TimeLeft -= Dt;
	if (Cast.TimeLeft <= 0.0f)
	{
		EndCast(Context, OwnerId, Player, Weapons);
	}
}

void FDTKeeperSim::EndCast(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, FDTWeaponSim* Weapons)
{
	using namespace DTKeeperSimLocal;
	FCast& Cast = Player.Cast;
	const FDTIntercessionPlan& Plan = Cast.Plan;
	if (Plan.Layer != INDEX_NONE)
	{
		for (const FDTSpecClause& Clause : Plan.LayerSpec.Clauses)
		{
			if (Clause.Flag == DTCat::EFlag::Slam && HasTrigger(Clause, TEXT("CastEnd")))
			{
				HitCircle(Context, OwnerId, Player, HeroOf(*Context.View, OwnerId), Clause.GetNum(ParamRadiusUu),
					StrongestDamage(Context, OwnerId, Weapons) * Clause.GetNum(ParamDmgRatio, 1.0f), Plan.LayerPath, 0.0f, false, 0);
			}
		}
	}
	FDTKeeperCastRecord& Record = Finished.AddDefaulted_GetRef();
	Record.OwnerId = OwnerId;
	Record.Record.Base = DTKeepers::SourceTag(Plan.Base);
	Record.Record.Variant = Plan.Variant != INDEX_NONE ? FName(DTCat::Archetypes[Plan.Variant].Id) : NAME_None;
	Record.Record.Layer = Plan.Layer != INDEX_NONE ? FName(DTCat::Archetypes[Plan.Layer].Id) : NAME_None;
	Record.Record.EnemiesHit = Cast.EnemiesHit.Num();
	Record.Record.Damage = static_cast<float>(Cast.Damage);
	Cast = FCast();
	for (TArray<FDTSpecClause>& Overlay : Player.Overlay)
	{
		Overlay.Reset();
	}
	if (Weapons)
	{
		RebuildFlags(*Context.Run, OwnerId, *Weapons);
	}
}

int32 FDTKeeperSim::HitCircle(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, const FVector2f& Center, float Radius,
	float Damage, EDTPath Path, float KnockbackUu, bool bBurn, int32 BurnApplications)
{
	FDTWorldView& View = *Context.View;
	const int32 Num = DTKeeperSimLocal::NumEnemies(View);
	if (Radius <= 0.0f || Num == 0 || View.EnemyHash.Num() == 0)
	{
		return 0;
	}
	TArray<int32> Local;
	View.EnemyHash.QueryCircle(Center, Radius, Local);
	Local.Sort();
	int32 Count = 0;
	for (const int32 Enemy : Local)
	{
		if (Enemy < 0 || Enemy >= Num)
		{
			continue;
		}
		const FVector2f Away = View.EnemyPositions[Enemy] - Center;
		if (Away.Size() > Radius + View.EnemyRadii[Enemy])
		{
			continue;
		}
		const uint32 Serial = View.EnemySerials[Enemy];
		if (Damage > 0.0f)
		{
			FDTHit& Out = View.Hits.AddDefaulted_GetRef();
			Out.EnemyIndex = Enemy;
			Out.EnemySerial = Serial;
			Out.OwnerId = OwnerId;
			Out.Damage = Damage;
			Out.Position = View.EnemyPositions[Enemy];
			Out.SourceId = DTKeepers::KeeperSource(Path); // R-KEE-13: Intercession damage is Keeper damage
			Player.Cast.Damage += Damage;
		}
		Player.Cast.EnemiesHit.Add(Serial);
		if (KnockbackUu > 0.0f)
		{
			FDTDisplacement& Move = View.Displacements.AddDefaulted_GetRef();
			Move.EnemyIndex = Enemy;
			Move.EnemySerial = Serial;
			Move.Delta = (Away.IsNearlyZero() ? DTKeeperSimLocal::AimOf(View, OwnerId) : Away.GetSafeNormal()) * KnockbackUu;
			Move.OwnerId = OwnerId;
		}
		for (int32 Burn = 0; bBurn && Burn < BurnApplications; ++Burn)
		{
			AppendStatus(View, Player, EDTStatus::Burn, Enemy, Serial, Damage, OwnerId, Path);
		}
		ApplyLayerStatus(Context, OwnerId, Player, Enemy, Damage);
		++Count;
	}
	return Count;
}

void FDTKeeperSim::ApplyLayerStatus(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, int32 Enemy, float HitDamage)
{
	using namespace DTKeeperSimLocal;
	const FDTIntercessionPlan& Plan = Player.Cast.Plan;
	if (Plan.Layer == INDEX_NONE || !IsPath(Plan.LayerPath))
	{
		return;
	}
	for (const FDTSpecClause& Clause : Plan.LayerSpec.Clauses)
	{
		EDTStatus Status;
		if (Clause.Flag == DTCat::EFlag::StatusApply && Clause.GetWord(WordOn) == WordIntercessionHit &&
			DTKeepers::ParseStatus(Clause.GetWord(WordStatus), Status))
		{
			AppendStatus(*Context.View, Player, Status, Enemy, Context.View->EnemySerials[Enemy], HitDamage, OwnerId, Plan.LayerPath,
				Clause.GetNum(ParamStacks, 1.0f));
		}
	}
}

void FDTKeeperSim::TidePull(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player)
{
	using namespace DTKeeperSimLocal;
	FDTWorldView& View = *Context.View;
	const int32 Num = NumEnemies(View);
	if (Num == 0 || View.EnemyHash.Num() == 0)
	{
		return;
	}
	const FDTIntercessionPlan& Plan = Player.Cast.Plan;
	const FVector2f Hero = HeroOf(View, OwnerId);
	const FVector2f Dir = AimOf(View, OwnerId);
	const FVector2f Side(-Dir.Y, Dir.X);
	const float Radius = DTConst::INTERCESSION_TIDE_RADIUS_UU;
	// The line starts one pull radius ahead of the hero (Tide's own Pull reach, expression.csv).
	const float Gap = DTKeepers::PathParam(EDTPath::Tide, DTCat::EFlag::Pull, ParamRadiusUu, 0.0f);
	const FDTSpecClause* PullVariant = FindClause(Plan.VariantSpec, DTCat::EFlag::Pull);
	const bool bSinglePoint = PullVariant && PullVariant->GetWord(WordTarget) == WordSinglePoint;
	// D-019: the base line, or a variant naming Flag.Pull, moves the crowd. Frost and Stillness Freeze it where it stands.
	const bool bMoves = Plan.VariantSpec.Clauses.IsEmpty() || PullVariant != nullptr;
	float FreezeSeconds = DTConst::INTERCESSION_TIDE_FREEZE_S;
	if (const FDTSpecClause* Freeze = FindClause(Plan.VariantSpec, DTCat::EFlag::StatusApply))
	{
		FreezeSeconds = Freeze->GetNum(ParamDurationS, FreezeSeconds); // Frost
	}

	TArray<int32> Local;
	View.EnemyHash.QueryCircle(Hero, Radius, Local);
	TArray<TPair<float, int32>> Order;
	float Widest = 0.0f;
	for (const int32 Enemy : Local)
	{
		if (Enemy >= 0 && Enemy < Num && FVector2f::Distance(View.EnemyPositions[Enemy], Hero) <= Radius)
		{
			Order.Emplace(FVector2f::DistSquared(View.EnemyPositions[Enemy], Hero), Enemy);
			Widest = FMath::Max(Widest, View.EnemyRadii[Enemy]);
		}
	}
	Order.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value; });

	// Ranks of up to the Tide radius in length, alternating sides of the aim line, nearest enemies first.
	float Along = Gap;
	int32 Rank = 0;
	for (const TPair<float, int32>& Item : Order)
	{
		const int32 Enemy = Item.Value;
		const uint32 Serial = View.EnemySerials[Enemy];
		if (bMoves)
		{
			const float R = View.EnemyRadii[Enemy];
			FVector2f Target = Hero + Dir * Gap;
			if (!bSinglePoint)
			{
				if (Along + R > Gap + Radius)
				{
					Along = Gap;
					++Rank;
				}
				const float Sign = (Rank % 2 == 1) ? 1.0f : -1.0f;
				const float Offset = Sign * static_cast<float>((Rank + 1) / 2) * 2.0f * Widest;
				Target = Hero + Dir * (Along + R) + Side * Offset;
				Along += 2.0f * R;
			}
			FDTDisplacement& Move = View.Displacements.AddDefaulted_GetRef();
			Move.EnemyIndex = Enemy;
			Move.EnemySerial = Serial;
			Move.Delta = Target - View.EnemyPositions[Enemy];
			Move.OwnerId = OwnerId;
		}
		AppendStatus(View, Player, EDTStatus::Freeze, Enemy, Serial, 0.0f, OwnerId, EDTPath::Tide,
			FreezeSeconds / FMath::Max(DTConst::STATUS_FREEZE_S, KINDA_SMALL_NUMBER));
		Player.Cast.EnemiesHit.Add(Serial);
	}
}
