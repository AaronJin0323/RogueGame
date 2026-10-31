// Duo reactions and duo blessings (reactions.md). Numbers come from reactions.csv. A reaction's hit carries Keeper.Duo.<Pair> and
// no weapon slot, so no proc feeds on it and no reaction starts from it (R-REA-03). Nothing here draws from a stream (R-REA-06), and
// with Reaction.Duos out of the pool nothing here runs (R-REA-02).
#include "Combat/DTReforge.h"
#include "Combat/DTWeaponSim.h"
#include "Core/DTConstantsGen.h"
#include "Enemies/DTEnemyPool.h"
#include "Keepers/DTKeeperSpec.h"

namespace DTWeaponSimDuosLocal
{
	using DTCat::EReaction;

	constexpr uint8 BurnBit = 1 << static_cast<int32>(EDTStatus::Burn);
	constexpr uint8 ChillBit = 1 << static_cast<int32>(EDTStatus::Chill);
	constexpr uint8 FreezeBit = 1 << static_cast<int32>(EDTStatus::Freeze);
	constexpr uint8 ShockBit = 1 << static_cast<int32>(EDTStatus::Shock);
	/** How much of a reaction's reach its inner ring takes (the second Keeper's hue, R-REA-11). */
	constexpr float InnerRing = 0.6f;
	/** A cue's rings have some size even when the reaction has no reach of its own. */
	constexpr float MinCueReach = 60.0f;
	/** An ember turns at will (the source: "seeks"), as a payday round does. */
	constexpr float EmberTurnDegS = 720.0f;

	static int32 RowOf(EReaction Pair) { return static_cast<int32>(Pair); }

	static uint32 Bit(EReaction Pair) { return 1u << RowOf(Pair); }

	static int64 StepsOf(float Seconds) { return FMath::RoundToInt64(Seconds * static_cast<float>(DTConst::SIM_FIXED_HZ)); }

	static EDTPath PathOfSource(uint16 Source)
	{
		for (int32 Path = 0; Path < static_cast<int32>(EDTPath::Count); ++Path)
		{
			if (Source == DTWeapons::KeeperSource(static_cast<EDTPath>(Path)))
			{
				return static_cast<EDTPath>(Path);
			}
		}
		return EDTPath::None;
	}

	static FVector2f HeroOf(const FDTWorldView& View, int32 OwnerId)
	{
		return View.HeroPositions.IsValidIndex(OwnerId) ? View.HeroPositions[OwnerId] : FVector2f::ZeroVector;
	}

	static FVector2f Rotate(const FVector2f& V, float Deg)
	{
		float S = 0.0f;
		float C = 1.0f;
		FMath::SinCos(&S, &C, FMath::DegreesToRadians(Deg));
		return FVector2f(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}
}

// ---- state -----------------------------------------------------------------------------------------------------------------

void FDTWeaponSim::SetReactions(int32 OwnerId, uint32 Live, uint32 Duos)
{
	if (OwnerId >= 0)
	{
		FPlayerRuntime& Player = EnsurePlayer(OwnerId);
		Player.LiveReactions = Live;
		Player.DuoReactions = Duos;
	}
}

uint32 FDTWeaponSim::LiveReactionsOf(int32 OwnerId) const
{
	return Players.IsValidIndex(OwnerId) && Unlocked.Contains(DTReactions::UnlockId()) ? Players[OwnerId].LiveReactions : 0u;
}

bool FDTWeaponSim::HasDuo(const FPlayerRuntime& Player, DTCat::EReaction Pair) const
{
	return (Player.DuoReactions & DTWeaponSimDuosLocal::Bit(Pair)) != 0;
}

const FDTReactionCounters& FDTWeaponSim::GetReactionCounters(int32 OwnerId, int32 Row) const
{
	static const FDTReactionCounters None;
	return Players.IsValidIndex(OwnerId) && Players[OwnerId].ReactionCounters.IsValidIndex(Row) ? Players[OwnerId].ReactionCounters[Row] : None;
}

TArray<FDTReactionEvent> FDTWeaponSim::TakeReactionEvents()
{
	return MoveTemp(ReactionEvents);
}

int32 FDTWeaponSim::GetToast(int32 OwnerId, int64 StepIndex) const
{
	return Players.IsValidIndex(OwnerId) && StepIndex < Players[OwnerId].ToastUntilStep ? Players[OwnerId].ToastRow : INDEX_NONE;
}

float FDTWeaponSim::GetWildfireLeft(int32 OwnerId) const
{
	return Players.IsValidIndex(OwnerId) ? FMath::Max(0.0f, Players[OwnerId].WildfireLeft) : 0.0f;
}

FDTDuoHero FDTWeaponSim::GetDuoHero(int32 OwnerId) const
{
	using namespace DTWeaponSimDuosLocal;
	FDTDuoHero Hero;
	if (Players.IsValidIndex(OwnerId) && LiveReactionsOf(OwnerId) != 0)
	{
		const FPlayerRuntime& Player = Players[OwnerId];
		Hero.MoveSpeedMul = Player.DuoSpeedMul;
		Hero.DashCharges = Player.DuoChargeLeft > 0.0f ? FMath::RoundToInt32(DTReactions::DuoParam(RowOf(EReaction::ShadeQuicksilver), TEXT("charges"))) : 0;
	}
	if (Players.IsValidIndex(OwnerId) && Players[OwnerId].SlipstreamRow != INDEX_NONE)
	{
		Hero.DashCharges += FMath::RoundToInt32(DTReforge::Param(Players[OwnerId].SlipstreamRow, TEXT("dash_charges"))); // R-RFG-21
	}
	return Hero;
}

void FDTWeaponSim::ApplyBlind(int32 OwnerId, uint32 Serial, int64 StepIndex, float Seconds)
{
	using namespace DTWeaponSimDuosLocal;
	if (OwnerId >= 0)
	{
		int64& Until = EnsurePlayer(OwnerId).BlindedUntilStep.FindOrAdd(Serial, 0);
		Until = FMath::Max(Until, StepIndex + StepsOf(Seconds));
	}
}

bool FDTWeaponSim::ClaimReaction(FPlayerRuntime& Player, int32 Row, uint32 Serial, int64 StepIndex)
{
	using namespace DTWeaponSimDuosLocal;
	const uint64 Key = (static_cast<uint64>(Row) << 32) | Serial;
	int64& Ready = Player.ReactionReadyStep.FindOrAdd(Key, 0);
	if (StepIndex < Ready)
	{
		return false;
	}
	Ready = StepIndex + StepsOf(DTConst::REACTION_ENEMY_COOLDOWN_S);
	return true;
}

void FDTWeaponSim::NoteReaction(const FDTSimContext& Context, FPlayerRuntime& Player, int32 OwnerId, int32 Row, const FVector2f& At, float Reach)
{
	using namespace DTWeaponSimDuosLocal;
	if (Player.ReactionCounters.Num() != DTCat::NumReactions)
	{
		Player.ReactionCounters.SetNum(DTCat::NumReactions);
	}
	++Player.ReactionCounters[Row].Fires;
	const bool bFirst = (Player.ReactionsFired & (1u << Row)) == 0;
	if (bFirst)
	{
		Player.ReactionsFired |= 1u << Row;
		Player.ToastRow = Row;
		Player.ToastUntilStep = Context.StepIndex + 1 + StepsOf(DTConst::HUD_BANNER_S);
	}
	FDTReactionEvent* Event = ReactionEvents.FindByPredicate([OwnerId, Row](const FDTReactionEvent& E) { return E.OwnerId == OwnerId && E.Row == Row; });
	if (!Event)
	{
		Event = &ReactionEvents.AddDefaulted_GetRef();
		Event->OwnerId = OwnerId;
		Event->Row = Row;
	}
	++Event->Fires;
	Event->bFirst |= bFirst;

	// R-REA-11: two rings, one per Keeper's hue, under the cap; past it the effect still ran.
	if (Context.StepIndex - CueWindowStart >= DTConst::SIM_FIXED_HZ)
	{
		CueWindowStart = Context.StepIndex;
		CueWindow = 0;
	}
	if (CueWindow >= DTConst::REACTION_FX_PER_S_MAX)
	{
		return;
	}
	++CueWindow;
	const DTCat::FReactionRow& R = DTCat::Reactions[Row];
	const float Outer = FMath::Max(Reach, MinCueReach);
	for (int32 Ring = 0; Ring < 2; ++Ring)
	{
		FArcFlash& Flash = Arcs.AddDefaulted_GetRef();
		Flash.OwnerId = OwnerId;
		Flash.Position = At;
		Flash.Reach = Ring == 0 ? Outer : Outer * InnerRing;
		Flash.ArcDeg = 360.0f;
		Flash.TimeLeft = DTConst::REACTION_CUE_S;
		Flash.Tint = Ring == 0 ? R.PathA : R.PathB;
	}
}

void FDTWeaponSim::NoteReactionDamage(FPlayerRuntime& Player, int32 OwnerId, int32 Row, float Damage)
{
	if (Player.ReactionCounters.Num() != DTCat::NumReactions)
	{
		Player.ReactionCounters.SetNum(DTCat::NumReactions);
	}
	Player.ReactionCounters[Row].Damage += Damage;
	FDTReactionEvent* Event = ReactionEvents.FindByPredicate([OwnerId, Row](const FDTReactionEvent& E) { return E.OwnerId == OwnerId && E.Row == Row; });
	if (!Event)
	{
		Event = &ReactionEvents.AddDefaulted_GetRef();
		Event->OwnerId = OwnerId;
		Event->Row = Row;
	}
	Event->Damage += Damage;
}

void FDTWeaponSim::DuoHit(const FDTSimContext& Context, int32 OwnerId, int32 Row, int32 Enemy, float Damage, bool bCrit)
{
	FDTWorldView& View = *Context.View;
	KeeperHit(View, Enemy, OwnerId, Damage, DTReactions::Source(Row), View);
	View.Hits.Last().bCrit = bCrit;
	NoteReactionDamage(Players[OwnerId], OwnerId, Row, Damage);
}

int32 FDTWeaponSim::DuoCircle(const FDTSimContext& Context, int32 OwnerId, int32 Row, const FVector2f& Center, float Radius, float Damage, uint32 Skip, TArray<int32>* OutHit)
{
	const FDTWorldView& View = *Context.View;
	TArray<int32> Local;
	EnemiesInCircle(View, Center, Radius + MaxEnemyRadius, Local);
	int32 Count = 0;
	for (const int32 Enemy : Local)
	{
		const float Reach = Radius + View.EnemyRadii[Enemy];
		if (View.EnemySerials[Enemy] == Skip || FVector2f::DistSquared(Center, View.EnemyPositions[Enemy]) > Reach * Reach)
		{
			continue;
		}
		DuoHit(Context, OwnerId, Row, Enemy, Damage);
		if (OutHit)
		{
			OutHit->Add(Enemy);
		}
		++Count;
	}
	return Count;
}

void FDTWeaponSim::DuoStatus(const FDTSimContext& Context, int32 OwnerId, int32 Row, int32 Enemy, EDTStatus Status, float HitDamage)
{
	const FDTWorldView& View = *Context.View;
	FDTFlagParams Params;
	switch (Status)
	{
	case EDTStatus::Burn: Params.SetWord(DTFlagParam::Status, FName(TEXT("Status.Burn"))); break;
	case EDTStatus::Chill: Params.SetWord(DTFlagParam::Status, FName(TEXT("Status.Chill"))); break;
	case EDTStatus::Freeze: Params.SetWord(DTFlagParam::Status, FName(TEXT("Status.Freeze"))); break;
	case EDTStatus::Shock: Params.SetWord(DTFlagParam::Status, FName(TEXT("Status.Shock"))); break;
	default: Params.SetWord(DTFlagParam::Status, FName(TEXT("Status.Mark"))); break;
	}
	ApplyStatus(Context, Params, FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] }, HitDamage, OwnerId, DTReactions::Source(Row));
}

void FDTWeaponSim::DuoStun(const FDTSimContext& Context, int32 OwnerId, int32 Row, int32 Enemy, float Seconds)
{
	FDTWorldView& View = *Context.View;
	FDTStatusApply& Apply = View.StatusApplies.AddDefaulted_GetRef();
	Apply.Handle = FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] };
	Apply.Status = EDTStatus::Freeze;
	Apply.OwnerId = OwnerId;
	Apply.SourceId = DTReactions::Source(Row);
	Apply.FreezeScale = Seconds / DTConst::STATUS_FREEZE_S;
}

void FDTWeaponSim::DuoBlind(const FDTSimContext& Context, int32 OwnerId, int32 Row, int32 Enemy, float Seconds)
{
	// keepers.md R-KEE-25, with a length of its own (R-REA-07).
	DuoStun(Context, OwnerId, Row, Enemy, Seconds);
	ApplyBlind(OwnerId, Context.View->EnemySerials[Enemy], Context.StepIndex, Seconds);
}

void FDTWeaponSim::DuoConsume(const FDTSimContext& Context, int32 OwnerId, int32 Row, int32 Enemy, EDTStatus Status)
{
	FDTWorldView& View = *Context.View;
	FDTStatusApply& Apply = View.StatusApplies.AddDefaulted_GetRef();
	Apply.Handle = FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] };
	Apply.Status = Status;
	Apply.OwnerId = OwnerId;
	Apply.SourceId = DTReactions::Source(Row);
	Apply.bConsume = true;
}

void FDTWeaponSim::NearestEnemies(const FDTWorldView& View, const FVector2f& From, float Range, int32 Count, uint32 Skip, TFunctionRef<bool(int32)> Filter, TArray<int32>& Out) const
{
	Out.Reset();
	TArray<int32> Local;
	EnemiesInCircle(View, From, Range, Local);
	TArray<TPair<float, int32>> Order;
	for (const int32 Enemy : Local)
	{
		if (View.EnemySerials[Enemy] != Skip && Filter(Enemy))
		{
			Order.Emplace(FVector2f::DistSquared(From, View.EnemyPositions[Enemy]), Enemy);
		}
	}
	Order.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value; });
	for (int32 Index = 0; Index < Order.Num() && (Count < 0 || Index < Count); ++Index)
	{
		Out.Add(Order[Index].Value);
	}
}

float FDTWeaponSim::BonusSpeed(int32 OwnerId) const
{
	static const FName ParamPerStack(TEXT("per_stack"));
	const FDTFlagEntry* Speed = KeeperEntry(OwnerId, DTCat::EFlag::SpeedScale);
	return Speed ? Players[OwnerId].Momentum * Speed->Params.Get(ParamPerStack) : 0.0f;
}

float FDTWeaponSim::DuoExecuteBelow(int32 Enemy, float Below) const
{
	using namespace DTWeaponSimDuosLocal;
	if (Below <= 0.0f || !Enemies || Enemy < 0 || Enemy >= Enemies->Num() || (Enemies->GetStatusMask(Enemy) & BurnBit) == 0)
	{
		return Below;
	}
	for (int32 Owner = 0; Owner < Players.Num(); ++Owner)
	{
		if ((LiveReactionsOf(Owner) & Bit(EReaction::ForgeShade)) != 0)
		{
			return Below + DTReactions::Param(RowOf(EReaction::ForgeShade), TEXT("execute_add"));
		}
	}
	return Below;
}

// ---- capture ---------------------------------------------------------------------------------------------------------------

void FDTWeaponSim::CaptureDuoHits(const FDTWorldView& View)
{
	using namespace DTWeaponSimDuosLocal;
	if (!Enemies || !Unlocked.Contains(DTReactions::UnlockId()))
	{
		return;
	}
	for (const FDTHit& Hit : View.Hits)
	{
		if (LiveReactionsOf(Hit.OwnerId) == 0 || !View.EnemySerials.IsValidIndex(Hit.EnemyIndex))
		{
			continue;
		}
		const uint32 Serial = Hit.EnemySerial != 0 ? Hit.EnemySerial : View.EnemySerials[Hit.EnemyIndex];
		FDuoHit& Out = DuoHits.AddDefaulted_GetRef();
		Out.OwnerId = Hit.OwnerId;
		Out.Serial = Serial;
		Out.Path = PathOfSource(Hit.SourceId);
		Out.Damage = Hit.Damage;
		Out.Mask = StatusMaskOf(Hit.EnemyIndex, Serial);
		Out.bCrit = Hit.bCrit;
		Out.Slot = Hit.WeaponSlot;
		// A reaction's own hit starts nothing (R-REA-03).
		for (int32 Row = 0; Row < DTCat::NumReactions; ++Row)
		{
			if (Hit.SourceId == DTReactions::Source(Row))
			{
				DuoHits.Pop(EAllowShrinking::No);
				break;
			}
		}
	}
}

// ---- deaths ----------------------------------------------------------------------------------------------------------------

void FDTWeaponSim::ReactDuoKills(const FDTSimContext& Context, FDTBulletPool* Bullets)
{
	using namespace DTWeaponSimDuosLocal;
	if (!Enemies || !Unlocked.Contains(DTReactions::UnlockId()))
	{
		return;
	}
	const FDTWorldView& View = *Context.View;
	const TArray<FDTKill>& Kills = View.Kills;
	const TConstArrayView<FDTEnemyDeath> Deaths = Enemies->GetStepDeaths();
	for (int32 Index = 0; Index < Kills.Num(); ++Index)
	{
		const FDTKill& Kill = Kills[Index];
		const uint32 Live = LiveReactionsOf(Kill.OwnerId);
		if (Live == 0 || !Deaths.IsValidIndex(Index) || Deaths[Index].Serial != Kill.EnemySerial)
		{
			continue;
		}
		FPlayerRuntime& Player = Players[Kill.OwnerId];
		const uint8 Mask = Deaths[Index].StatusMask;
		const bool bMarked = IsMarked(Kill.OwnerId, Kill.EnemySerial);
		if ((Live & Bit(EReaction::StormShade)) != 0 && bMarked && (Mask & ShockBit) != 0)
		{
			const int32 Row = RowOf(EReaction::StormShade);
			const bool bSummons = HasDuo(Player, EReaction::StormShade);
			const int32 Owner = Kill.OwnerId;
			TArray<int32> Targets;
			const auto Writ = [this, &View, Owner](int32 Enemy) { return IsMarked(Owner, View.EnemySerials[Enemy]); };
			const int32 Bolts = FMath::RoundToInt32(DTReactions::Param(Row, TEXT("bolts")));
			const float Range = DTReactions::Param(Row, TEXT("range_uu"));
			NearestEnemies(View, Kill.Position, Range, Bolts, Kill.EnemySerial, Writ, Targets);
			if (bSummons && Targets.Num() < Bolts)
			{
				NearestEnemies(View, Kill.Position, Range, Bolts, Kill.EnemySerial, [](int32) { return true; }, Targets);
			}
			NoteReaction(Context, Player, Owner, Row, Kill.Position, Range);
			const float Damage = StrongestDamage(Owner) * DTReactions::Param(Row, TEXT("dmg"));
			for (const int32 Enemy : Targets)
			{
				DuoHit(Context, Owner, Row, Enemy, Damage);
				if (bSummons)
				{
					DuoStatus(Context, Owner, Row, Enemy, EDTStatus::Mark, 0.0f);
				}
			}
		}
		if ((Live & Bit(EReaction::ForgeShade)) != 0 && bMarked && (Mask & BurnBit) != 0)
		{
			const int32 Row = RowOf(EReaction::ForgeShade);
			NoteReaction(Context, Player, Kill.OwnerId, Row, Kill.Position, 0.0f);
			TArray<int32> Targets;
			NearestEnemies(View, Kill.Position, DTReactions::Param(Row, TEXT("range_uu")), 1, Kill.EnemySerial, [](int32) { return true; }, Targets);
			FWeaponRuntime* Runtime = FindRuntime(Kill.OwnerId, Kill.WeaponSlot != INDEX_NONE ? Kill.WeaponSlot : 0);
			if (Targets.Num() > 0 && Runtime)
			{
				// A homing round of the shared child kind (R-REA-07).
				FShot Ember = MakeShot(Kill.OwnerId, Kill.WeaponSlot != INDEX_NONE ? Kill.WeaponSlot : 0, *Runtime, DTReactions::Source(Row), Kill.Position);
				Ember.Damage = StrongestDamage(Kill.OwnerId) * DTReactions::Param(Row, TEXT("ember_dmg"));
				Ember.Pierce = 1;
				Ember.Bounces = 0;
				Ember.bTrack = true;
				Ember.DuoRow = static_cast<int8>(Row);
				Ember.Speed = static_cast<float>(DTConst::WEAPON_CHILD_SPEED_UU_S);
				Ember.Radius = static_cast<float>(DTConst::WEAPON_CHILD_RADIUS_UU);
				Ember.Life = static_cast<float>(DTConst::WEAPON_CHILD_RANGE_UU) / Ember.Speed;
				const int32 Target = Targets[0];
				const FVector2f To = View.EnemyPositions[Target] - Kill.Position;
				Ember.Motion.TurnDegS = EmberTurnDegS;
				Ember.Motion.SeekUu = Ember.Speed * Ember.Life;
				Ember.Motion.TargetSerial = View.EnemySerials[Target];
				SpawnRound(Context, Bullets, *Runtime, Ember, Kill.Position, To.GetSafeNormal());
			}
		}
		if ((Live & Bit(EReaction::ShadeRadiance)) != 0 && bMarked && IsBlinded(Kill.OwnerId, Kill.EnemySerial))
		{
			const int32 Row = RowOf(EReaction::ShadeRadiance);
			MarkCircle(Context, Kill.OwnerId, Kill.Position, DTReactions::Param(Row, TEXT("mark_uu")), DTReactions::Source(Row));
		}
	}
}

// ---- hits ------------------------------------------------------------------------------------------------------------------

void FDTWeaponSim::FireSteam(const FDTSimContext& Context, FPlayerRuntime& Player, int32 OwnerId, int32 Enemy, float Damage)
{
	using namespace DTWeaponSimDuosLocal;
	const int32 Row = RowOf(EReaction::ForgeTide);
	const FDTWorldView& View = *Context.View;
	const uint32 Serial = View.EnemySerials[Enemy];
	if (!ClaimReaction(Player, Row, Serial, Context.StepIndex))
	{
		return;
	}
	const FVector2f At = View.EnemyPositions[Enemy];
	const float Radius = DTReactions::Param(Row, TEXT("radius_uu"));
	NoteReaction(Context, Player, OwnerId, Row, At, Radius);
	DuoConsume(Context, OwnerId, Row, Enemy, EDTStatus::Burn);
	DuoConsume(Context, OwnerId, Row, Enemy, EDTStatus::Chill);
	DuoConsume(Context, OwnerId, Row, Enemy, EDTStatus::Freeze);
	const float Blast = Damage * DTReactions::Param(Row, TEXT("dmg"));
	TArray<int32> Struck;
	DuoCircle(Context, OwnerId, Row, At, Radius, Blast, 0, &Struck);
	const bool bCooker = HasDuo(Player, EReaction::ForgeTide);
	const float Stagger = DTReactions::Param(Row, TEXT("stagger_s"));
	for (const int32 Other : Struck)
	{
		if (Other != Enemy)
		{
			DuoStun(Context, OwnerId, Row, Other, Stagger);
			if (bCooker)
			{
				DuoStatus(Context, OwnerId, Row, Other, EDTStatus::Chill, Blast);
				DuoStatus(Context, OwnerId, Row, Other, EDTStatus::Burn, Blast);
			}
		}
	}
}

void FDTWeaponSim::FireShards(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Enemy, float Damage, int8 Slot)
{
	using namespace DTWeaponSimDuosLocal;
	const int32 Row = RowOf(EReaction::TideRadiance);
	const FDTWorldView& View = *Context.View;
	const int32 Use = Slot != INDEX_NONE ? Slot : 0;
	FWeaponRuntime* Runtime = FindRuntime(OwnerId, Use);
	if (!Runtime)
	{
		return;
	}
	const FVector2f At = View.EnemyPositions[Enemy];
	FShot Shard = MakeShot(OwnerId, Use, *Runtime, DTReactions::Source(Row), At);
	Shard.Damage = Damage * DTReactions::Param(Row, TEXT("dmg"));
	Shard.Pierce = FMath::RoundToInt32(DTReactions::Param(Row, TEXT("pierce")));
	Shard.Bounces = 0;
	Shard.bTrack = true;
	Shard.DuoRow = static_cast<int8>(Row);
	Shard.Speed = static_cast<float>(DTConst::WEAPON_CHILD_SPEED_UU_S);
	Shard.Radius = static_cast<float>(DTConst::WEAPON_CHILD_RADIUS_UU);
	Shard.Life = static_cast<float>(DTConst::WEAPON_CHILD_RANGE_UU) / Shard.Speed;
	const int32 Shards = FMath::RoundToInt32(DTReactions::Param(Row, TEXT("shards")));
	const float Fan = DTReactions::Param(Row, TEXT("fan_deg"));
	const FVector2f Forward = (At - HeroOf(View, OwnerId)).GetSafeNormal();
	const float Clear = View.EnemyRadii[Enemy] + Shard.Radius;
	for (int32 Index = 0; Index < Shards; ++Index)
	{
		const float Deg = Shards > 1 ? -0.5f * Fan + Fan * static_cast<float>(Index) / static_cast<float>(Shards - 1) : 0.0f;
		const FVector2f Dir = Rotate(Forward, Deg);
		SpawnRound(Context, Bullets, *Runtime, Shard, At + Dir * Clear, Dir);
	}
}

void FDTWeaponSim::ReactDuoRound(const FDTSimContext& Context, const FImpact& Impact, int32 HitEnemy)
{
	using namespace DTWeaponSimDuosLocal;
	if (HitEnemy == INDEX_NONE || !Players.IsValidIndex(Impact.OwnerId))
	{
		return;
	}
	FPlayerRuntime& Player = Players[Impact.OwnerId];
	NoteReactionDamage(Player, Impact.OwnerId, Impact.DuoRow, Impact.HitDamage);
	if (Impact.DuoRow == RowOf(EReaction::ForgeShade))
	{
		DuoStatus(Context, Impact.OwnerId, Impact.DuoRow, HitEnemy, EDTStatus::Burn, Impact.HitDamage);
	}
	else if (Impact.DuoRow == RowOf(EReaction::TideRadiance) && HasDuo(Player, EReaction::TideRadiance))
	{
		DuoStatus(Context, Impact.OwnerId, Impact.DuoRow, HitEnemy, EDTStatus::Chill, Impact.HitDamage);
		if (Impact.bCrit)
		{
			DuoBlind(Context, Impact.OwnerId, Impact.DuoRow, HitEnemy, DTConst::STATUS_BLIND_S);
		}
	}
}

int32 FDTWeaponSim::DuoBounces(const FDTSimContext& Context, const FImpact& Impact, int32 HitEnemy, bool bHasBounce)
{
	using namespace DTWeaponSimDuosLocal;
	const uint32 Live = LiveReactionsOf(Impact.OwnerId);
	if (Live == 0 || HitEnemy == INDEX_NONE)
	{
		return 0;
	}
	FPlayerRuntime& Player = Players[Impact.OwnerId];
	const FDTWorldView& View = *Context.View;
	if ((Live & Bit(EReaction::StormQuicksilver)) != 0 && (Impact.StatusMask & ShockBit) != 0)
	{
		const int32 Row = RowOf(EReaction::StormQuicksilver);
		if (!ClaimReaction(Player, Row, Impact.EnemySerial, Context.StepIndex))
		{
			return 0;
		}
		NoteReaction(Context, Player, Impact.OwnerId, Row, Impact.EnemyPosition, DTReactions::Param(Row, TEXT("arc_range_uu")));
		TArray<int32> Targets;
		NearestEnemies(View, Impact.EnemyPosition, DTReactions::Param(Row, TEXT("arc_range_uu")), 1, Impact.EnemySerial, [](int32) { return true; }, Targets);
		for (const int32 Enemy : Targets)
		{
			DuoHit(Context, Impact.OwnerId, Row, Enemy, Impact.HitDamage * DTReactions::Param(Row, TEXT("arc_dmg")));
		}
		return FMath::RoundToInt32(DTReactions::Param(Row, TEXT("bounces")));
	}
	if ((Live & Bit(EReaction::TideQuicksilver)) != 0 && bHasBounce)
	{
		const int32 Row = RowOf(EReaction::TideQuicksilver);
		const uint8 Works = HasDuo(Player, EReaction::TideQuicksilver) ? (FreezeBit | ChillBit) : FreezeBit;
		if ((Impact.StatusMask & Works) != 0 && ClaimReaction(Player, Row, Impact.EnemySerial, Context.StepIndex))
		{
			NoteReaction(Context, Player, Impact.OwnerId, Row, Impact.EnemyPosition, 0.0f);
			return FMath::RoundToInt32(DTReactions::Param(Row, TEXT("bounce_free")));
		}
	}
	return 0;
}

void FDTWeaponSim::ReactDuoHit(const FDTSimContext& Context, FDTBulletPool* Bullets, FPlayerRuntime& Player, const FDuoHit& Hit, int32 Enemy)
{
	using namespace DTWeaponSimDuosLocal;
	FDTWorldView& View = *Context.View;
	const uint32 Live = LiveReactionsOf(Hit.OwnerId);
	const FVector2f At = View.EnemyPositions[Enemy];
	const int32 Owner = Hit.OwnerId;
	const bool bPulled = Player.StepPulled.Contains(Hit.Serial);
	const uint8 Mask = Hit.Mask | (bPulled ? ChillBit : 0);
	const bool bMarked = IsMarked(Owner, Hit.Serial);
	const bool bBlinded = IsBlinded(Owner, Hit.Serial);

	if ((Live & Bit(EReaction::StormForge)) != 0 && Hit.Path == EDTPath::Storm && (Mask & BurnBit) != 0)
	{
		const int32 Row = RowOf(EReaction::StormForge);
		if (ClaimReaction(Player, Row, Hit.Serial, Context.StepIndex))
		{
			const float Radius = DTReactions::Param(Row, TEXT("radius_uu"));
			const float Burst = Hit.Damage * DTReactions::Param(Row, TEXT("dmg"));
			NoteReaction(Context, Player, Owner, Row, At, Radius);
			TArray<int32> Struck;
			DuoCircle(Context, Owner, Row, At, Radius, Burst, 0, &Struck);
			for (const int32 Other : Struck)
			{
				DuoStatus(Context, Owner, Row, Other, EDTStatus::Burn, Burst);
			}
			if (HasDuo(Player, EReaction::StormForge))
			{
				FDTFlagParams Ground;
				Ground.Replace(DTFlagParam::RadiusUu, Radius);
				Ground.Replace(DTFlagParam::DurationS, DTReactions::DuoParam(Row, TEXT("field_s")));
				Ground.Replace(DTFlagParam::Dps, DTReactions::DuoParam(Row, TEXT("field_dps")));
				Ground.SetWord(DTFlagParam::Status, FName(TEXT("Status.Burn")));
				SpawnField(Owner, 0, DTReactions::Source(Row), At, Ground, Burst);
			}
		}
	}
	if ((Live & Bit(EReaction::StormTide)) != 0 && Hit.Path == EDTPath::Storm && (Mask & (ChillBit | FreezeBit)) != 0)
	{
		const int32 Row = RowOf(EReaction::StormTide);
		if (ClaimReaction(Player, Row, Hit.Serial, Context.StepIndex))
		{
			const bool bSurge = HasDuo(Player, EReaction::StormTide);
			const float Radius = DTReactions::Param(Row, TEXT("radius_uu"));
			NoteReaction(Context, Player, Owner, Row, At, Radius);
			TArray<int32> Targets;
			const auto Cold = [this, &View](int32 Other) { return (StatusMaskOf(Other, View.EnemySerials[Other]) & (ChillBit | FreezeBit)) != 0; };
			NearestEnemies(View, At, Radius, bSurge ? -1 : FMath::RoundToInt32(DTReactions::Param(Row, TEXT("max"))), Hit.Serial, Cold, Targets);
			const int32 Every = FMath::RoundToInt32(DTReactions::DuoParam(Row, TEXT("every_n")));
			for (const int32 Other : Targets)
			{
				DuoHit(Context, Owner, Row, Other, Hit.Damage * DTReactions::Param(Row, TEXT("dmg")));
				if (bSurge && Every > 0 && ++Player.ConductedJumps % Every == 0)
				{
					DuoStatus(Context, Owner, Row, Other, EDTStatus::Freeze, 0.0f);
				}
			}
		}
	}
	if ((Live & Bit(EReaction::StormRadiance)) != 0)
	{
		const int32 Row = RowOf(EReaction::StormRadiance);
		const bool bArc = HasDuo(Player, EReaction::StormRadiance);
		if (bArc && bBlinded && Hit.Path == EDTPath::Storm)
		{
			DuoHit(Context, Owner, Row, Enemy, Hit.Damage * DTReactions::DuoParam(Row, TEXT("arc_vuln")));
		}
		if (Hit.bCrit && (Mask & ShockBit) != 0 && ClaimReaction(Player, Row, Hit.Serial, Context.StepIndex))
		{
			const float Range = DTReactions::Param(Row, TEXT("range_uu"));
			NoteReaction(Context, Player, Owner, Row, At, Range);
			TArray<int32> Targets;
			NearestEnemies(View, At, Range, FMath::RoundToInt32(DTReactions::Param(Row, TEXT("targets"))), Hit.Serial, [](int32) { return true; }, Targets);
			const float Blind = bArc ? DTReactions::DuoParam(Row, TEXT("blind_s")) : DTReactions::Param(Row, TEXT("blind_s"));
			for (const int32 Other : Targets)
			{
				DuoHit(Context, Owner, Row, Other, Hit.Damage * DTReactions::Param(Row, TEXT("dmg")));
				DuoBlind(Context, Owner, Row, Other, Blind);
			}
		}
	}
	if ((Live & Bit(EReaction::ForgeTide)) != 0 && (Mask & BurnBit) != 0 && (Mask & (ChillBit | FreezeBit)) != 0)
	{
		FireSteam(Context, Player, Owner, Enemy, Hit.Damage);
	}
	if ((Live & Bit(EReaction::ForgeRadiance)) != 0 && Hit.bCrit && (Mask & BurnBit) != 0)
	{
		const int32 Row = RowOf(EReaction::ForgeRadiance);
		float Owed = 0.0f;
		for (const FDTBurnStack& Stack : Enemies->GetAux(Enemy).Burn)
		{
			Owed += Stack.Left > 0.0f ? Stack.Left * Stack.Dps : 0.0f;
		}
		if (Owed > 0.0f && ClaimReaction(Player, Row, Hit.Serial, Context.StepIndex))
		{
			const bool bSunspot = HasDuo(Player, EReaction::ForgeRadiance);
			NoteReaction(Context, Player, Owner, Row, At, 0.0f);
			DuoConsume(Context, Owner, Row, Enemy, EDTStatus::Burn);
			DuoHit(Context, Owner, Row, Enemy, Owed * DTReactions::Param(Row, TEXT("mult")) * (bSunspot ? BestCritMult(Owner) : 1.0f), bSunspot);
		}
	}
	if ((Live & Bit(EReaction::TideShade)) != 0 && bMarked && (Mask & FreezeBit) != 0)
	{
		const int32 Row = RowOf(EReaction::TideShade);
		if (CanExecute(Enemy, DTReactions::Param(Row, TEXT("execute_below"))) && ClaimReaction(Player, Row, Hit.Serial, Context.StepIndex))
		{
			const float Shatter = DTReactions::Param(Row, TEXT("shatter_uu"));
			NoteReaction(Context, Player, Owner, Row, At, Shatter);
			DuoHit(Context, Owner, Row, Enemy, Enemies->GetHp()[Enemy]);
			DuoCircle(Context, Owner, Row, At, Shatter, StrongestDamage(Owner) * DTReactions::Param(Row, TEXT("shatter_dmg")), Hit.Serial, nullptr);
			if (HasDuo(Player, EReaction::TideShade))
			{
				TArray<int32> Local;
				EnemiesInCircle(View, At, DTReactions::DuoParam(Row, TEXT("freeze_uu")), Local);
				for (const int32 Other : Local)
				{
					DuoStatus(Context, Owner, Row, Other, EDTStatus::Freeze, 0.0f);
				}
			}
		}
	}
	if ((Live & Bit(EReaction::TideRadiance)) != 0 && Hit.bCrit && (Mask & FreezeBit) != 0)
	{
		const int32 Row = RowOf(EReaction::TideRadiance);
		if (ClaimReaction(Player, Row, Hit.Serial, Context.StepIndex))
		{
			NoteReaction(Context, Player, Owner, Row, At, 0.0f);
			FireShards(Context, Bullets, Owner, Enemy, Hit.Damage, Hit.Slot);
		}
	}
	if ((Live & Bit(EReaction::ShadeRadiance)) != 0 && bBlinded)
	{
		const int32 Row = RowOf(EReaction::ShadeRadiance);
		const float Vuln = bMarked ? DTReactions::Param(Row, TEXT("vuln")) : (HasDuo(Player, EReaction::ShadeRadiance) ? DTReactions::DuoParam(Row, TEXT("vuln")) : 0.0f);
		if (Vuln > 0.0f)
		{
			DuoHit(Context, Owner, Row, Enemy, Hit.Damage * Vuln);
			if (ClaimReaction(Player, Row, Hit.Serial, Context.StepIndex))
			{
				NoteReaction(Context, Player, Owner, Row, At, 0.0f);
			}
		}
	}
	if ((Live & Bit(EReaction::RadianceQuicksilver)) != 0 && Hit.bCrit)
	{
		const int32 Row = RowOf(EReaction::RadianceQuicksilver);
		if (Hit.Path == EDTPath::Quicksilver && ClaimReaction(Player, Row, Hit.Serial, Context.StepIndex))
		{
			NoteReaction(Context, Player, Owner, Row, At, 0.0f);
			DuoBlind(Context, Owner, Row, Enemy, DTReactions::Param(Row, TEXT("blind_s")));
		}
		if (HasDuo(Player, EReaction::RadianceQuicksilver) && Player.DuoRefundReadyIn <= 0.0f)
		{
			Player.DuoRefundReadyIn = DTConst::REACTION_ENEMY_COOLDOWN_S;
			Player.DashRefund += DTConst::DASH_COOLDOWN_S * DTReactions::DuoParam(Row, TEXT("refund"));
		}
	}
	if ((Live & Bit(EReaction::ForgeQuicksilver)) != 0 && Player.WildfireLeft > 0.0f && Hit.Slot != INDEX_NONE)
	{
		DuoStatus(Context, Owner, RowOf(EReaction::ForgeQuicksilver), Enemy, EDTStatus::Burn, Hit.Damage);
	}
}

// ---- the dash ----------------------------------------------------------------------------------------------------------------

void FDTWeaponSim::StepDuoDash(const FDTSimContext& Context, FPlayerRuntime& Player, int32 OwnerId)
{
	using namespace DTWeaponSimDuosLocal;
	FDTWorldView& View = *Context.View;
	const uint32 Live = LiveReactionsOf(OwnerId);
	const bool bDash = View.HeroIntangible.IsValidIndex(OwnerId) && View.HeroIntangible[OwnerId];
	const bool bDashStart = bDash && !Player.bDuoDashWas;
	Player.bDuoDashWas = bDash;
	const FVector2f Hero = HeroOf(View, OwnerId);

	// Wildfire: the owner's field under a dash, bonus speed, or any dash with Afterburner.
	if ((Live & Bit(EReaction::ForgeQuicksilver)) != 0)
	{
		const int32 Row = RowOf(EReaction::ForgeQuicksilver);
		const bool bBurner = HasDuo(Player, EReaction::ForgeQuicksilver);
		bool bStart = bDashStart && bBurner;
		if (bDash && !bStart)
		{
			bStart = Fields.ContainsByPredicate([OwnerId, &Hero](const FField& Field)
			{
				return Field.OwnerId == OwnerId && FVector2f::DistSquared(Field.Position, Hero) <= Field.Radius * Field.Radius;
			});
		}
		bStart |= BonusSpeed(OwnerId) >= DTReactions::Param(Row, TEXT("speed_over"));
		if (bStart)
		{
			if (Player.WildfireLeft <= 0.0f)
			{
				NoteReaction(Context, Player, OwnerId, Row, Hero, 0.0f);
			}
			Player.WildfireLeft = DTReactions::Param(Row, TEXT("burn_s"));
		}
		float Speed = 1.0f;
		if (bBurner)
		{
			TArray<int32> Local;
			const auto Burning = [this, &View](int32 Enemy) { return (StatusMaskOf(Enemy, View.EnemySerials[Enemy]) & BurnBit) != 0; };
			NearestEnemies(View, Hero, DTReactions::DuoParam(Row, TEXT("near_uu")), 1, 0, Burning, Local);
			Speed += Local.Num() > 0 ? DTReactions::DuoParam(Row, TEXT("speed")) : 0.0f;
		}
		Player.DuoSpeedMul = Speed;
	}

	// Lightning Step: fields along the dash.
	if ((Live & Bit(EReaction::StormQuicksilver)) != 0 && HasDuo(Player, EReaction::StormQuicksilver))
	{
		const int32 Row = RowOf(EReaction::StormQuicksilver);
		const float Spacing = DTReactions::DuoParam(Row, TEXT("trail_uu"));
		Player.TrailDistance = bDashStart ? Spacing : Player.TrailDistance + (bDash ? FVector2f::Distance(Hero, Player.TrailLast) : 0.0f);
		Player.TrailLast = Hero;
		if (bDash && Player.TrailDistance >= Spacing)
		{
			Player.TrailDistance = 0.0f;
			FDTFlagParams Trail;
			Trail.Replace(DTFlagParam::RadiusUu, 0.5f * Spacing);
			Trail.Replace(DTFlagParam::DurationS, DTReactions::DuoParam(Row, TEXT("trail_s")));
			Trail.SetWord(DTFlagParam::Status, FName(TEXT("Status.Shock")));
			SpawnField(OwnerId, 0, DTReactions::Source(Row), Hero, Trail, 0.0f);
		}
	}

	if (!bDash || (Live & (Bit(EReaction::StormQuicksilver) | Bit(EReaction::TideQuicksilver) | Bit(EReaction::ShadeQuicksilver))) == 0)
	{
		return;
	}
	const float Hull = static_cast<float>(DTConst::HERO_RADIUS_UU);
	TArray<int32> Local;
	EnemiesInCircle(View, Hero, Hull + MaxEnemyRadius, Local);
	for (const int32 Enemy : Local)
	{
		const float Reach = Hull + View.EnemyRadii[Enemy];
		const FVector2f At = View.EnemyPositions[Enemy];
		if (FVector2f::DistSquared(Hero, At) > Reach * Reach)
		{
			continue;
		}
		const uint32 Serial = View.EnemySerials[Enemy];
		const uint8 Mask = StatusMaskOf(Enemy, Serial);
		if ((Live & Bit(EReaction::StormQuicksilver)) != 0 && (Mask & ShockBit) != 0)
		{
			const int32 Row = RowOf(EReaction::StormQuicksilver);
			if (ClaimReaction(Player, Row, Serial, Context.StepIndex))
			{
				const float Radius = DTReactions::Param(Row, TEXT("radius_uu"));
				NoteReaction(Context, Player, OwnerId, Row, At, Radius);
				DuoConsume(Context, OwnerId, Row, Enemy, EDTStatus::Shock);
				const float Burst = StrongestDamage(OwnerId) * DTReactions::Param(Row, TEXT("dmg"));
				TArray<int32> Struck;
				DuoCircle(Context, OwnerId, Row, At, Radius, Burst, 0, &Struck);
				if (HasDuo(Player, EReaction::StormQuicksilver))
				{
					TArray<int32> Targets;
					NearestEnemies(View, At, DTReactions::Param(Row, TEXT("arc_range_uu")), 1, Serial, [&Struck](int32 Other) { return !Struck.Contains(Other); }, Targets);
					for (const int32 Other : Targets)
					{
						DuoHit(Context, OwnerId, Row, Other, Burst * DTReactions::Param(Row, TEXT("arc_dmg")));
					}
				}
			}
		}
		if ((Live & Bit(EReaction::TideQuicksilver)) != 0 && (Mask & ChillBit) != 0)
		{
			const int32 Row = RowOf(EReaction::TideQuicksilver);
			if (ClaimReaction(Player, Row, Serial, Context.StepIndex))
			{
				NoteReaction(Context, Player, OwnerId, Row, At, 0.0f);
				DuoStatus(Context, OwnerId, Row, Enemy, EDTStatus::Freeze, 0.0f);
			}
		}
		if ((Live & Bit(EReaction::ShadeQuicksilver)) != 0 && IsMarked(OwnerId, Serial))
		{
			const int32 Row = RowOf(EReaction::ShadeQuicksilver);
			const bool bFast = HasDuo(Player, EReaction::ShadeQuicksilver);
			const float Below = bFast ? DTReactions::DuoParam(Row, TEXT("below")) : DTReactions::Param(Row, TEXT("below"));
			if (CanExecute(Enemy, Below) && ClaimReaction(Player, Row, Serial, Context.StepIndex))
			{
				NoteReaction(Context, Player, OwnerId, Row, At, 0.0f);
				DuoHit(Context, OwnerId, Row, Enemy, Enemies->GetHp()[Enemy]);
				Player.DashRefund += DTConst::DASH_COOLDOWN_S * DTReactions::Param(Row, TEXT("refund"));
				Player.DuoChargeLeft = bFast ? DTReactions::DuoParam(Row, TEXT("charge_s")) : Player.DuoChargeLeft;
			}
		}
	}
}

void FDTWeaponSim::StepDuoCrit(FPlayerRuntime& Player, int32 OwnerId)
{
	using namespace DTWeaponSimDuosLocal;
	if ((LiveReactionsOf(OwnerId) & Bit(EReaction::RadianceQuicksilver)) == 0)
	{
		return;
	}
	const int32 Row = RowOf(EReaction::RadianceQuicksilver);
	const float Scale = HasDuo(Player, EReaction::RadianceQuicksilver) ? DTReactions::DuoParam(Row, TEXT("crit_scale"), 1.0f) : 1.0f;
	const float Add = 10.0f * BonusSpeed(OwnerId) * DTReactions::Param(Row, TEXT("crit_per_10pct")) * Scale;
	for (FWeaponRuntime& Weapon : Player.Weapons)
	{
		Weapon.Spec.CritChance = FMath::Min(Weapon.Spec.CritChance + Add, static_cast<float>(DTConst::MOD_CRIT_CHANCE_CAP));
	}
}

// ---- the step -----------------------------------------------------------------------------------------------------------------

void FDTWeaponSim::StepDuos(const FDTSimContext& Context, FDTBulletPool* Bullets)
{
	using namespace DTWeaponSimDuosLocal;
	if (!Enemies || !Unlocked.Contains(DTReactions::UnlockId()))
	{
		return;
	}
	const TArray<FDuoHit> Batch = MoveTemp(DuoHits);
	DuoHits.Reset();
	for (const FDuoHit& Hit : Batch)
	{
		const int32 Enemy = IndexOfSerial(Hit.Serial);
		if (Enemy != INDEX_NONE && Players.IsValidIndex(Hit.OwnerId))
		{
			ReactDuoHit(Context, Bullets, Players[Hit.OwnerId], Hit, Enemy);
		}
	}
	for (int32 Owner = 0; Owner < Players.Num() && Owner < Context.Run->GetNumPlayers(); ++Owner)
	{
		FPlayerRuntime& Player = Players[Owner];
		if (LiveReactionsOf(Owner) == 0)
		{
			continue;
		}
		Player.WildfireLeft -= Context.StepSeconds;
		Player.DuoChargeLeft -= Context.StepSeconds;
		Player.DuoRefundReadyIn -= Context.StepSeconds;
		StepDuoDash(Context, Player, Owner);
		// A Pull landing on a Burning enemy is Steam with no hit (R-REA-04).
		if ((LiveReactionsOf(Owner) & Bit(EReaction::ForgeTide)) != 0)
		{
			for (const uint32 Serial : Player.StepPulled)
			{
				const int32 Enemy = IndexOfSerial(Serial);
				if (Enemy != INDEX_NONE && (StatusMaskOf(Enemy, Serial) & BurnBit) != 0)
				{
					FireSteam(Context, Player, Owner, Enemy, StrongestDamage(Owner));
				}
			}
		}
		Player.StepPulled.Reset();
		if (Context.StepIndex % DTConst::SIM_FIXED_HZ == 0)
		{
			const int64 Step = Context.StepIndex;
			for (TMap<uint64, int64>::TIterator It = Player.ReactionReadyStep.CreateIterator(); It; ++It)
			{
				if (It.Value() <= Step)
				{
					It.RemoveCurrent();
				}
			}
		}
	}
}
