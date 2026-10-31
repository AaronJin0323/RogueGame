// The S2 Keeper mechanics of the weapon system (keepers.md §9): Mark, Blind, hunters, Debt, crit reactions, novas, dash volleys,
// Momentum. Every parameter comes from expression.csv; a hit made here carries no weapon slot unless a rule says it does, so no
// proc feeds on it (R-KEE-13's Keeper hits). Reads the flag sources directly, so they count before the next compose.
#include "Combat/DTReforge.h"
#include "Combat/DTWeaponSim.h"
#include "Core/DTConstantsGen.h"
#include "Enemies/DTEnemyPool.h"

namespace DTWeaponSimKeepersLocal
{
	static const FName ParamMarkS(TEXT("mark_s"));
	static const FName ParamMarkedVuln(TEXT("marked_vuln"));
	static const FName ParamExecuteBelow(TEXT("execute_below"));
	static const FName ParamSource(TEXT("source"));
	static const FName ParamLifeS(TEXT("life_s"));
	static const FName ParamAnyKill(TEXT("any_kill_chance"));
	static const FName ParamExplodeUu(TEXT("explode_uu"));
	static const FName ParamKillExtends(TEXT("kill_extends_s"));
	static const FName ParamLastingEvery(TEXT("lasting_every"));
	static const FName ParamLastingDmg(TEXT("lasting_dmg"));
	static const FName ParamLastingMax(TEXT("lasting_max"));
	static const FName ParamDebtMax(TEXT("debt_max"));
	static const FName ParamPaydayMult(TEXT("payday_mult"));
	static const FName ParamPaydayPierce(TEXT("payday_pierce"));
	static const FName ParamPaydayExecute(TEXT("payday_execute_below"));
	static const FName ParamEliteDebt(TEXT("elite_debt"));
	static const FName ParamCritPulseUu(TEXT("crit_pulse_uu"));
	static const FName ParamCritPulseDmg(TEXT("crit_pulse_dmg"));
	static const FName ParamCritStatus(TEXT("crit_status"));
	static const FName ParamFocalStack(TEXT("focal_stack"));
	static const FName ParamFocalMax(TEXT("focal_max"));
	static const FName ParamPulseEvery(TEXT("pulse_every"));
	static const FName ParamPulseUu(TEXT("pulse_uu"));
	static const FName ParamPulseDmg(TEXT("pulse_dmg"));
	static const FName ParamBlindSpreadUu(TEXT("blind_spread_uu"));
	static const FName ParamRingUu(TEXT("ring_uu"));
	static const FName ParamRingMult(TEXT("ring_mult"));
	static const FName ParamSunShare(TEXT("sun_share"));
	static const FName ParamSunIntervalS(TEXT("sun_interval_s"));
	static const FName ParamKillRefundS(TEXT("kill_refund_s"));
	static const FName ParamRefundMax(TEXT("refund_max"));
	static const FName ParamFreeEvery(TEXT("free_every"));
	static const FName ParamTailwindRate(TEXT("tailwind_rate"));
	static const FName ParamTailwindS(TEXT("tailwind_s"));
	static const FName ParamPerStack(TEXT("per_stack"));
	static const FName ParamStacksMax(TEXT("stacks_max"));
	static const FName ParamPer100Uu(TEXT("per_100uu"));
	static const FName ParamDashStacks(TEXT("dash_stacks"));
	static const FName ParamHitLoss(TEXT("hit_loss"));
	static const FName ParamDecayPerS(TEXT("decay_per_s"));
	static const FName ParamMaxPierce(TEXT("max_pierce"));
	static const FName ParamMaxEchoDelay(TEXT("max_echo_delay_s"));
	static const FName ParamMaxEchoDmg(TEXT("max_echo_dmg"));
	static const FName ParamWaveEveryUu(TEXT("wave_every_uu"));
	static const FName ParamWaveUu(TEXT("wave_uu"));
	static const FName ParamWaveDmg(TEXT("wave_dmg"));
	static const FName ParamKillFreeRound(TEXT("kill_free_round"));

	constexpr float UuPerMetre = 100.0f;
	/** A payday round turns at will (the source: "hunts every enemy"). */
	constexpr float PaydayTurnDegS = 720.0f;
	/** A payday round outlives the frame it crosses. */
	constexpr float PaydayLifeScale = 2.0f;

	static FVector2f Rotate(const FVector2f& V, float Deg)
	{
		float S = 0.0f;
		float C = 1.0f;
		FMath::SinCos(&S, &C, FMath::DegreesToRadians(Deg));
		return FVector2f(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}

	static FVector2f HeroOf(const FDTWorldView& View, int32 OwnerId)
	{
		return View.HeroPositions.IsValidIndex(OwnerId) ? View.HeroPositions[OwnerId] : FVector2f::ZeroVector;
	}

	static FVector2f SafeDir(const FVector2f& V, const FVector2f& Fallback)
	{
		const float Len = V.Size();
		return Len > KINDA_SMALL_NUMBER ? V / Len : Fallback;
	}

	static int64 StepsOf(float Seconds)
	{
		return FMath::RoundToInt64(Seconds * static_cast<float>(DTConst::SIM_FIXED_HZ));
	}

	static void PurgeUntil(TMap<uint32, int64>& Map, int64 Step)
	{
		for (TMap<uint32, int64>::TIterator It = Map.CreateIterator(); It; ++It)
		{
			if (It.Value() < Step)
			{
				It.RemoveCurrent();
			}
		}
	}
}

const FDTFlagEntry* FDTWeaponSim::KeeperEntry(int32 OwnerId, DTCat::EFlag Flag) const
{
	if (Players.IsValidIndex(OwnerId))
	{
		for (const FFlagSource& Source : Players[OwnerId].Sources)
		{
			for (const FDTFlagEntry& Entry : Source.Flags.Entries)
			{
				if (Entry.Flag == Flag && Entry.Params.GetWord(DTFlagParam::On).IsNone())
				{
					return &Entry;
				}
			}
		}
	}
	return nullptr;
}

// ---- Mark and Blind ------------------------------------------------------------------------------------------------------

void FDTWeaponSim::ApplyMark(int32 OwnerId, uint32 Serial, int64 StepIndex)
{
	using namespace DTWeaponSimKeepersLocal;
	if (OwnerId < 0 || Serial == 0)
	{
		return;
	}
	float Seconds = DTConst::STATUS_MARK_S;
	if (const FDTFlagEntry* Seek = KeeperEntry(OwnerId, DTCat::EFlag::Seek))
	{
		Seconds += Seek->Params.Get(ParamMarkS);
	}
	EnsurePlayer(OwnerId).MarkedUntilStep.Add(Serial, StepIndex + StepsOf(Seconds));
}

bool FDTWeaponSim::IsMarked(int32 OwnerId, uint32 Serial) const
{
	const int64* Until = Players.IsValidIndex(OwnerId) ? Players[OwnerId].MarkedUntilStep.Find(Serial) : nullptr;
	return Until && *Until >= LastStep;
}

bool FDTWeaponSim::IsBlinded(int32 OwnerId, uint32 Serial) const
{
	const int64* Until = Players.IsValidIndex(OwnerId) ? Players[OwnerId].BlindedUntilStep.Find(Serial) : nullptr;
	return Until && *Until >= LastStep;
}

void FDTWeaponSim::AbsorbStatuses(const FDTSimContext& Context)
{
	using namespace DTWeaponSimKeepersLocal;
	if (!Context.View)
	{
		return;
	}
	for (const FDTStatusApply& Apply : Context.View->StatusApplies)
	{
		if (Apply.OwnerId < 0)
		{
			continue;
		}
		if (Apply.Status == EDTStatus::Mark)
		{
			ApplyMark(Apply.OwnerId, Apply.Handle.Serial, Context.StepIndex);
		}
		else if (Apply.Status == EDTStatus::Blind)
		{
			EnsurePlayer(Apply.OwnerId).BlindedUntilStep.Add(Apply.Handle.Serial, Context.StepIndex + StepsOf(DTConst::STATUS_BLIND_S));
		}
	}
}

void FDTWeaponSim::MarkCircle(const FDTSimContext& Context, int32 OwnerId, const FVector2f& Center, float Radius, uint16 SourceId)
{
	FDTWorldView& View = *Context.View;
	TArray<int32> Local;
	EnemiesInCircle(View, Center, Radius, Local);
	for (const int32 Enemy : Local)
	{
		const uint32 Serial = View.EnemySerials[Enemy];
		ApplyMark(OwnerId, Serial, Context.StepIndex);
		FDTStatusApply& Apply = View.StatusApplies.AddDefaulted_GetRef();
		Apply.Handle = FDTEnemyHandle{ Enemy, Serial };
		Apply.Status = EDTStatus::Mark;
		Apply.OwnerId = OwnerId;
		Apply.SourceId = SourceId;
	}
}

void FDTWeaponSim::KeeperHit(const FDTWorldView& View, int32 Enemy, int32 OwnerId, float Damage, uint16 SourceId, FDTWorldView& Out) const
{
	FDTHit& Hit = Out.Hits.AddDefaulted_GetRef();
	Hit.EnemyIndex = Enemy;
	Hit.EnemySerial = View.EnemySerials[Enemy];
	Hit.OwnerId = OwnerId;
	Hit.Damage = Damage;
	Hit.Position = View.EnemyPositions[Enemy];
	Hit.SourceId = SourceId;
}

bool FDTWeaponSim::CanExecute(int32 Enemy, float Below) const
{
	if (!Enemies || Below <= 0.0f || Enemy < 0 || Enemy >= Enemies->Num())
	{
		return false;
	}
	const float Max = Enemies->GetMaxHp()[Enemy];
	const float Hp = Enemies->GetHp()[Enemy];
	return Enemies->GetRoles()[Enemy] != EDTEnemyRole::Boss && Hp > 0.0f && Max > 0.0f && Hp <= Max * DuoExecuteBelow(Enemy, Below);
}

int32 FDTWeaponSim::ExecuteMarked(const FDTSimContext& Context, int32 OwnerId, float Below, uint16 SourceId)
{
	FDTWorldView& View = *Context.View;
	const int32 Num = FMath::Min(View.EnemySerials.Num(), View.EnemyPositions.Num());
	int32 Count = 0;
	for (int32 Enemy = 0; Enemy < Num; ++Enemy)
	{
		if (IsMarked(OwnerId, View.EnemySerials[Enemy]) && CanExecute(Enemy, Below))
		{
			KeeperHit(View, Enemy, OwnerId, Enemies->GetHp()[Enemy], SourceId, View);
			++Count;
		}
	}
	return Count;
}

void FDTWeaponSim::ReactSeek(const FDTSimContext& Context, const FDTResolvedFlag& Flag, const FImpact& Impact, int32 HitEnemy)
{
	using namespace DTWeaponSimKeepersLocal;
	if (HitEnemy == INDEX_NONE || !IsMarked(Impact.OwnerId, Impact.EnemySerial))
	{
		return;
	}
	FDTWorldView& View = *Context.View;
	const FDTFlagParams& Params = Flag.Entry.Params;
	const bool bClassDamage = Flag.IsFallback() && (Flag.Resolution.Cap == DTCat::ECap::Melee || Flag.Resolution.Cap == DTCat::ECap::Beam);
	const float Share = Params.Get(ParamMarkedVuln) + (bClassDamage ? DTConst::KEEPER_SEEK_FALLBACK_DMG : 0.0f);
	if (Share > 0.0f)
	{
		KeeperHit(View, HitEnemy, Impact.OwnerId, Impact.HitDamage * Share, Flag.Entry.SourceId, View);
	}
	if (CanExecute(HitEnemy, Params.Get(ParamExecuteBelow)))
	{
		KeeperHit(View, HitEnemy, Impact.OwnerId, Enemies->GetHp()[HitEnemy], Flag.Entry.SourceId, View);
	}
}

uint32 FDTWeaponSim::NearestMarked(int32 OwnerId, const FVector2f& From, float Range, const FDTWorldView& View) const
{
	if (!Players.IsValidIndex(OwnerId) || Players[OwnerId].MarkedUntilStep.Num() == 0)
	{
		return 0;
	}
	TArray<int32> Local;
	EnemiesInCircle(View, From, Range, Local);
	uint32 Best = 0;
	float BestDist = TNumericLimits<float>::Max();
	for (const int32 Enemy : Local)
	{
		const float D = FVector2f::DistSquared(From, View.EnemyPositions[Enemy]);
		if (D < BestDist && IsMarked(OwnerId, View.EnemySerials[Enemy]))
		{
			BestDist = D;
			Best = View.EnemySerials[Enemy];
		}
	}
	return Best;
}

// ---- hunters ---------------------------------------------------------------------------------------------------------------

int32 FDTWeaponSim::GetLiveHunters(int32 OwnerId) const
{
	return Hunters.FilterByPredicate([OwnerId](const FHunter& H) { return H.OwnerId == OwnerId && !H.bShade; }).Num();
}

int32 FDTWeaponSim::GetLiveShades(int32 OwnerId) const
{
	return Hunters.FilterByPredicate([OwnerId](const FHunter& H) { return H.OwnerId == OwnerId && H.bShade; }).Num();
}

FDTReforgeRead FDTWeaponSim::GetReforgeRead(int32 OwnerId) const
{
	FDTReforgeRead Out = Players.IsValidIndex(OwnerId) ? Players[OwnerId].Read : FDTReforgeRead();
	Out.Shades = Players.IsValidIndex(OwnerId) ? Players[OwnerId].ShadesRaised : 0;
	return Out;
}

void FDTWeaponSim::RaiseShade(int32 OwnerId, FPlayerRuntime& Player)
{
	const int32 Row = Player.CollectionRow;
	float SeatTimer = 0.0f;
	if (GetLiveShades(OwnerId) >= FMath::RoundToInt(DTReforge::Param(Row, TEXT("shades_max"))))
	{
		int32 Oldest = INDEX_NONE;
		for (int32 Index = 0; Index < Hunters.Num(); ++Index)
		{
			if (Hunters[Index].OwnerId == OwnerId && Hunters[Index].bShade && (Oldest == INDEX_NONE || Hunters[Index].ShadeNumber < Hunters[Oldest].ShadeNumber))
			{
				Oldest = Index;
			}
		}
		// R-RFG-19: the seat keeps its cooldown, so a kill at the cap neither buys a volley nor delays one.
		SeatTimer = Hunters[Oldest].Timer;
		Hunters.RemoveAtSwap(Oldest);
	}
	FHunter& Shade = Hunters.AddDefaulted_GetRef();
	Shade.OwnerId = OwnerId;
	Shade.DmgRatio = DTReforge::Param(Row, TEXT("dmg"));
	Shade.Timer = SeatTimer;
	Shade.bShade = true;
	Shade.ShadeNumber = Player.ShadesRaised++;
}

void FDTWeaponSim::RaiseHunters(int32 OwnerId, int32 Count, const FVector2f& At, float LifeS, float DmgRatio)
{
	const FDTFlagEntry* Entry = KeeperEntry(OwnerId, DTCat::EFlag::Hunter);
	const int32 Max = Entry ? FMath::RoundToInt(Entry->Params.Get(DTFlagParam::MaxLive)) : Count;
	for (int32 Index = 0; Index < Count && GetLiveHunters(OwnerId) < Max; ++Index)
	{
		FHunter& Hunter = Hunters.AddDefaulted_GetRef();
		Hunter.OwnerId = OwnerId;
		Hunter.Position = At;
		Hunter.Left = LifeS;
		Hunter.DmgRatio = DmgRatio;
	}
}

void FDTWeaponSim::StepHunters(const FDTSimContext& Context, FDTBulletPool* Bullets)
{
	using namespace DTWeaponSimKeepersLocal;
	FDTWorldView& View = *Context.View;
	const float Frame = static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU);
	const float Walk = static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S) * Context.StepSeconds;
	for (FPlayerRuntime& Player : Players)
	{
		Player.ShadeOrbitDeg = FMath::Fmod(Player.ShadeOrbitDeg + static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_DEG_S) * Context.StepSeconds, 360.0f);
	}
	for (int32 Index = 0; Index < Hunters.Num();)
	{
		FHunter& Hunter = Hunters[Index];
		const FDTFlagEntry* Entry = KeeperEntry(Hunter.OwnerId, DTCat::EFlag::Hunter);
		const uint16 Source = Entry ? Entry->SourceId : DTWeapons::KeeperSource(EDTPath::Shade);
		if (!Hunter.bLasting && !Hunter.bShade)
		{
			Hunter.Left -= Context.StepSeconds;
			if (Hunter.Left <= 0.0f)
			{
				const float Blast = Entry ? Entry->Params.Get(ParamExplodeUu) : 0.0f;
				if (Blast > 0.0f)
				{
					HitCircle(Context, Hunter.OwnerId, INDEX_NONE, Source, Hunter.Position, Blast,
						StrongestDamage(Hunter.OwnerId) * Entry->Params.Get(DTFlagParam::ExplodeDmg), false, nullptr);
				}
				Hunters.RemoveAtSwap(Index);
				continue;
			}
		}
		++Index;
		const FPlayerRuntime& Player = Players[Hunter.OwnerId];
		int32 Slot = INDEX_NONE;
		for (int32 Candidate = 0; Candidate < Player.Weapons.Num(); ++Candidate)
		{
			Slot = Slot == INDEX_NONE || Player.Weapons[Candidate].Spec.LoadRank < Player.Weapons[Slot].Spec.LoadRank ? Candidate : Slot;
		}
		if (Hunter.bShade)
		{
			// R-RFG-19: the first weapon, from a seat on the orbit; a replaced Shade's seat goes to the one that replaces it.
			Slot = 0;
			const int32 Seats = FMath::Max(1, FMath::RoundToInt(DTReforge::Param(Player.CollectionRow, TEXT("shades_max"))));
			Hunter.Position = HeroOf(View, Hunter.OwnerId) + Rotate(FVector2f(static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_RADIUS_UU), 0.0f),
				Player.ShadeOrbitDeg + 360.0f * static_cast<float>(Hunter.ShadeNumber % Seats) / static_cast<float>(Seats));
		}
		FWeaponRuntime* Runtime = FindRuntime(Hunter.OwnerId, Slot);
		int32 Target = INDEX_NONE;
		if (!Runtime || !FindNearest(View, Hunter.Position, Frame, 0, Target))
		{
			continue;
		}
		const FDTWeaponSpec& Spec = Runtime->Spec;
		const FVector2f To = View.EnemyPositions[Target] - Hunter.Position;
		const float Dist = To.Size();
		const FVector2f Dir = SafeDir(To, FVector2f(1.0f, 0.0f));
		if (Dist > Spec.Range && !Hunter.bShade)
		{
			Hunter.Position += Dir * Walk;
		}
		Hunter.Timer -= Context.StepSeconds;
		if (Hunter.Timer > 0.0f || Dist > Spec.Range)
		{
			continue;
		}
		Hunter.Timer = Spec.Cooldown;
		FShot Shot = MakeShot(Hunter.OwnerId, Slot, *Runtime, Source, Hunter.Position);
		Shot.Damage *= Hunter.DmgRatio;
		switch (Spec.Pattern)
		{
		case EDTWeaponPattern::MeleeArc:
			EmitArc(Context, *Runtime, Shot, Dir, Spec.ArcDeg, Spec.Range, false);
			break;
		case EDTWeaponPattern::Mine:
			PlaceMine(Context, *Runtime, Hunter.OwnerId, Slot, View.EnemyPositions[Target], Shot.Damage, Source, false);
			break;
		case EDTWeaponPattern::Lob:
			LaunchShell(*Runtime, Shot, View.EnemyPositions[Target]);
			break;
		default:
		{
			// The pattern's shape from the hunter: a cone, a ring, or lanes along the aim.
			const int32 Fan = FMath::Max(1, Spec.Count);
			const bool bRing = Spec.Pattern == EDTWeaponPattern::Radial || Spec.Pattern == EDTWeaponPattern::Spiral;
			const float Cone = Spec.Pattern == EDTWeaponPattern::Cone && Fan > 1 ? static_cast<float>(DTConst::WEAPON_CONE_DEG) : 0.0f;
			const FVector2f Side(-Dir.Y, Dir.X);
			for (int32 Round = 0; Round < Fan; ++Round)
			{
				const float Part = Fan > 1 ? static_cast<float>(Round) / static_cast<float>(Fan - 1) : 0.0f;
				const float Angle = bRing ? 360.0f * static_cast<float>(Round) / static_cast<float>(Fan) : -0.5f * Cone + Cone * Part;
				const float Offset = bRing || Cone > 0.0f ? 0.0f : (static_cast<float>(Round) - 0.5f * static_cast<float>(Fan - 1)) * 2.0f * Spec.Radius;
				SpawnRound(Context, Bullets, *Runtime, Shot, Hunter.Position + Side * Offset, Rotate(Dir, Angle));
			}
			break;
		}
		}
	}
}

// ---- Debt ------------------------------------------------------------------------------------------------------------------

FDTKeeperMeterRead FDTWeaponSim::GetKeeperMeters(int32 OwnerId) const
{
	using namespace DTWeaponSimKeepersLocal;
	FDTKeeperMeterRead Out;
	if (!Players.IsValidIndex(OwnerId))
	{
		return Out;
	}
	const FPlayerRuntime& Player = Players[OwnerId];
	if (const FDTFlagEntry* Debt = KeeperEntry(OwnerId, DTCat::EFlag::Debt))
	{
		Out.bDebt = true;
		Out.Debt = Player.Debt;
		Out.DebtMax = FMath::RoundToInt(Debt->Params.Get(ParamDebtMax));
	}
	Out.bPayday = IsPaydayReady(OwnerId);
	if (const FDTFlagEntry* Speed = KeeperEntry(OwnerId, DTCat::EFlag::SpeedScale))
	{
		Out.bMomentum = true;
		Out.Momentum = Player.Momentum;
		Out.MomentumMax = Player.SlipstreamRow != INDEX_NONE ? 0.0f : Speed->Params.Get(ParamStacksMax); // R-RFG-21: no cap
	}
	// The player's word for a hunter and a Collection Shade is the same (ST_Keywords Flag.Hunter).
	const FDTFlagEntry* Hunter = KeeperEntry(OwnerId, DTCat::EFlag::Hunter);
	Out.Shades = GetLiveHunters(OwnerId) + GetLiveShades(OwnerId);
	Out.bShades = Hunter != nullptr || Player.CollectionRow != INDEX_NONE || Out.Shades > 0;
	if (Hunter)
	{
		Out.ShadesMax += FMath::RoundToInt(Hunter->Params.Get(DTFlagParam::MaxLive)) + FMath::RoundToInt(Hunter->Params.Get(ParamLastingMax));
	}
	if (Player.CollectionRow != INDEX_NONE)
	{
		Out.ShadesMax += FMath::RoundToInt(DTReforge::Param(Player.CollectionRow, TEXT("shades_max")));
	}
	return Out;
}

int32 FDTWeaponSim::GetDebt(int32 OwnerId) const
{
	return Players.IsValidIndex(OwnerId) ? Players[OwnerId].Debt : 0;
}

bool FDTWeaponSim::IsPaydayReady(int32 OwnerId) const
{
	return Players.IsValidIndex(OwnerId) && Players[OwnerId].PaydayMult > 0.0f;
}

void FDTWeaponSim::AddDebt(int32 OwnerId, int32 Debt)
{
	using namespace DTWeaponSimKeepersLocal;
	const FDTFlagEntry* Entry = KeeperEntry(OwnerId, DTCat::EFlag::Debt);
	if (!Entry)
	{
		return;
	}
	FPlayerRuntime& Player = EnsurePlayer(OwnerId);
	Player.Debt += Debt;
	const int32 Max = FMath::RoundToInt(Entry->Params.Get(ParamDebtMax));
	if (Max > 0 && Player.Debt >= Max)
	{
		Player.Debt = 0;
		ForcePayday(OwnerId, Entry->Params.Get(ParamPaydayMult), Entry->Params.Get(ParamPaydayExecute));
	}
}

void FDTWeaponSim::ForcePayday(int32 OwnerId, float Mult, float ExecuteBelow)
{
	FPlayerRuntime& Player = EnsurePlayer(OwnerId);
	Player.PaydayMult = Mult;
	Player.PaydayExecute = ExecuteBelow;
}

void FDTWeaponSim::PayNow(int32 OwnerId)
{
	FPlayerRuntime& Player = EnsurePlayer(OwnerId);
	Player.bPaydayNow = Player.PaydayMult > 0.0f;
}

void FDTWeaponSim::FirePaydayNow(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, FPlayerRuntime& Player)
{
	int32 Slot = INDEX_NONE;
	for (int32 Candidate = 0; Candidate < Player.Weapons.Num(); ++Candidate)
	{
		Slot = Slot == INDEX_NONE || Player.Weapons[Candidate].Spec.LoadRank < Player.Weapons[Slot].Spec.LoadRank ? Candidate : Slot;
	}
	FirePayday(Context, Bullets, OwnerId, Slot, Player);
}

bool FDTWeaponSim::FirePayday(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot, FPlayerRuntime& Player)
{
	using namespace DTWeaponSimKeepersLocal;
	FDTWorldView& View = *Context.View;
	FWeaponRuntime* Runtime = FindRuntime(OwnerId, Slot);
	const FVector2f Hero = HeroOf(View, OwnerId);
	const float Frame = static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU);
	TArray<int32> Local;
	EnemiesInCircle(View, Hero, Frame, Local);
	if (!Runtime || Local.Num() == 0)
	{
		return false; // the next volley with a screen to hunt
	}
	const FDTFlagEntry* Entry = KeeperEntry(OwnerId, DTCat::EFlag::Debt);
	const uint16 Source = Entry ? Entry->SourceId : DTWeapons::KeeperSource(EDTPath::Shade);
	FShot Shot = MakeShot(OwnerId, Slot, *Runtime, Source, Hero);
	Shot.Damage *= Player.PaydayMult;
	Shot.Pierce += Entry ? FMath::RoundToInt(Entry->Params.Get(ParamPaydayPierce)) : 0;
	if (Shot.Speed <= 0.0f)
	{
		Shot.Speed = static_cast<float>(DTConst::WEAPON_CHILD_SPEED_UU_S);
		Shot.Radius = static_cast<float>(DTConst::WEAPON_CHILD_RADIUS_UU);
	}
	Shot.Life = PaydayLifeScale * Frame / Shot.Speed;
	Shot.Motion.TurnDegS = PaydayTurnDegS;
	Shot.Motion.SeekUu = Frame;
	for (const int32 Enemy : Local)
	{
		FShot Round = Shot;
		Round.Motion.TargetSerial = View.EnemySerials[Enemy];
		SpawnRound(Context, Bullets, *Runtime, Round, Hero, SafeDir(View.EnemyPositions[Enemy] - Hero, FVector2f(1.0f, 0.0f)));
	}
	ExecuteMarked(Context, OwnerId, Player.PaydayExecute, Source);
	Player.PaydayMult = 0.0f;
	Player.PaydayExecute = 0.0f;
	Player.bPaydayNow = false;
	return true;
}

// ---- kills -------------------------------------------------------------------------------------------------------------------

void FDTWeaponSim::ReactKeeperKills(const FDTSimContext& Context, FDTBulletPool* Bullets)
{
	using namespace DTWeaponSimKeepersLocal;
	FDTWorldView& View = *Context.View;
	FRandomStream& Rng = Context.Run->GetRng().Get(EDTRngStream::Combat);
	for (const FDTKill& Kill : View.Kills)
	{
		if (!Players.IsValidIndex(Kill.OwnerId))
		{
			continue;
		}
		FPlayerRuntime& Player = Players[Kill.OwnerId];
		// Pyre Debt: a Cremation ember's kill counts as a Marked kill (reactions.md).
		const bool bPyre = (Player.DuoReactions & (1u << static_cast<int32>(DTCat::EReaction::ForgeShade))) != 0 &&
			Kill.SourceId == DTReactions::Source(static_cast<int32>(DTCat::EReaction::ForgeShade));
		const bool bMarked = IsMarked(Kill.OwnerId, Kill.EnemySerial) || bPyre;
		const bool bWeapon = Kill.WeaponSlot != INDEX_NONE;
		if (Player.CollectionRow != INDEX_NONE && bMarked)
		{
			RaiseShade(Kill.OwnerId, Player);
		}
		if (const FDTFlagEntry* Debt = KeeperEntry(Kill.OwnerId, DTCat::EFlag::Debt))
		{
			if (bMarked)
			{
				AddDebt(Kill.OwnerId, 1 + (Kill.bElite ? FMath::RoundToInt(Debt->Params.Get(ParamEliteDebt)) : 0));
			}
		}
		if (const FDTFlagEntry* Hunter = KeeperEntry(Kill.OwnerId, DTCat::EFlag::Hunter))
		{
			const FDTFlagParams& Params = Hunter->Params;
			const float AnyKill = Params.Get(ParamAnyKill);
			const bool bRoll = AnyKill > 0.0f && Rng.FRand() < AnyKill;
			if (bRoll || (bMarked && Params.WordHas(ParamSource, TEXT("MarkedKill"))) || (Kill.bElite && Params.WordHas(ParamSource, TEXT("EliteKill"))))
			{
				RaiseHunters(Kill.OwnerId, 1, Kill.Position, Params.Get(ParamLifeS), Params.Get(DTFlagParam::Dmg));
			}
			const float Extends = Params.Get(ParamKillExtends);
			if (Extends > 0.0f && bWeapon && Kill.SourceId == Hunter->SourceId)
			{
				for (FHunter& Living : Hunters)
				{
					Living.Left += Living.OwnerId == Kill.OwnerId ? Extends : 0.0f;
				}
			}
			const int32 Every = FMath::RoundToInt(Params.Get(ParamLastingEvery));
			if (Every > 0 && ++Player.LastingKills % Every == 0 && Player.LastingLive < FMath::RoundToInt(Params.Get(ParamLastingMax)))
			{
				FHunter& Lasting = Hunters.AddDefaulted_GetRef();
				Lasting.OwnerId = Kill.OwnerId;
				Lasting.Position = Kill.Position;
				Lasting.DmgRatio = Params.Get(ParamLastingDmg);
				Lasting.bLasting = true;
				++Player.LastingLive;
			}
		}
		if (const FDTFlagEntry* Prism = KeeperEntry(Kill.OwnerId, DTCat::EFlag::CritSplit))
		{
			const float Spread = Prism->Params.Get(ParamBlindSpreadUu);
			if (Spread > 0.0f && IsBlinded(Kill.OwnerId, Kill.EnemySerial))
			{
				FDTFlagParams Blind;
				Blind.SetWord(DTFlagParam::Status, FName(TEXT("Status.Blind")));
				TArray<int32> Local;
				EnemiesInCircle(View, Kill.Position, Spread, Local);
				for (const int32 Enemy : Local)
				{
					ApplyStatus(Context, Blind, FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] }, 0.0f, Kill.OwnerId, Prism->SourceId);
				}
			}
		}
		if (const FDTFlagEntry* Dash = KeeperEntry(Kill.OwnerId, DTCat::EFlag::VolleyOnDash))
		{
			if (bWeapon && Kill.SourceId == Dash->SourceId)
			{
				const float Cap = Dash->Params.Get(ParamRefundMax) * static_cast<float>(DTConst::DASH_COOLDOWN_S);
				const float Refund = FMath::Min(Dash->Params.Get(ParamKillRefundS), FMath::Max(0.0f, Cap - Player.DashRefundUsed));
				Player.DashRefund += Refund;
				Player.DashRefundUsed += Refund;
			}
		}
		if (const FDTFlagEntry* Ricochet = KeeperEntry(Kill.OwnerId, DTCat::EFlag::Ricochet))
		{
			FWeaponRuntime* Runtime = FindRuntime(Kill.OwnerId, Kill.WeaponSlot);
			int32 Next = INDEX_NONE;
			if (Runtime && Kill.SourceId == Ricochet->SourceId && Ricochet->Params.Get(ParamKillFreeRound) > 0.0f &&
				FindNearest(View, Kill.Position, Ricochet->Params.Get(DTFlagParam::SeekRangeUu), Kill.EnemySerial, Next))
			{
				FShot Free = MakeShot(Kill.OwnerId, Kill.WeaponSlot, *Runtime, Ricochet->SourceId, Kill.Position);
				Free.Pierce = 1;
				Free.Bounces = 0;
				SpawnRound(Context, Bullets, *Runtime, Free, Kill.Position, SafeDir(View.EnemyPositions[Next] - Kill.Position, FVector2f(1.0f, 0.0f)));
			}
		}
		Player.MarkedUntilStep.Remove(Kill.EnemySerial);
		Player.BlindedUntilStep.Remove(Kill.EnemySerial);
	}
}

// ---- crits and novas ---------------------------------------------------------------------------------------------------------

bool FDTWeaponSim::RollShot(const FDTSimContext& Context, const FDTWeaponSpec& Spec, const FShot& Shot, float& InOutDamage) const
{
	if (Shot.bForceCrit)
	{
		InOutDamage *= Spec.CritMult;
		return true;
	}
	if (Shot.CritBonus <= 0.0f)
	{
		return RollCrit(Context, Spec, InOutDamage);
	}
	FDTWeaponSpec Charged = Spec;
	Charged.CritChance = FMath::Min(Spec.CritChance + Shot.CritBonus, static_cast<float>(DTConst::MOD_CRIT_CHANCE_CAP));
	return RollCrit(Context, Charged, InOutDamage);
}

void FDTWeaponSim::ReactToCrit(const FDTSimContext& Context, FWeaponRuntime& Runtime, const FImpact& Impact, int32 HitEnemy)
{
	using namespace DTWeaponSimKeepersLocal;
	FDTWorldView& View = *Context.View;
	FPlayerRuntime& Player = Players[Impact.OwnerId];
	const FVector2f At = Impact.EnemySerial != 0 ? Impact.EnemyPosition : Impact.Position;
	for (const FDTResolvedFlag& Flag : Runtime.Spec.Flags)
	{
		const FDTFlagParams& Params = Flag.Entry.Params;
		const float Pulse = Params.Get(ParamCritPulseUu);
		if (Pulse > 0.0f)
		{
			HitCircle(Context, Impact.OwnerId, INDEX_NONE, Flag.Entry.SourceId, At, Pulse, Impact.HitDamage * Params.Get(ParamCritPulseDmg), false, nullptr);
		}
		const FName Status = Params.GetWord(ParamCritStatus);
		if (!Status.IsNone() && HitEnemy != INDEX_NONE)
		{
			FDTFlagParams Apply;
			Apply.SetWord(DTFlagParam::Status, Status);
			ApplyStatus(Context, Apply, FDTEnemyHandle{ HitEnemy, Impact.EnemySerial }, Impact.HitDamage, Impact.OwnerId, Flag.Entry.SourceId);
		}
		const float Focal = Params.Get(ParamFocalStack);
		if (Focal > 0.0f && HitEnemy != INDEX_NONE)
		{
			const int32 Max = FMath::RoundToInt(Params.Get(ParamFocalMax));
			Player.FocalStacks = Player.LastCritSerial == Impact.EnemySerial ? FMath::Min(Player.FocalStacks + 1, Max) : 0;
			Player.LastCritSerial = Impact.EnemySerial;
			if (Player.FocalStacks > 0)
			{
				KeeperHit(View, HitEnemy, Impact.OwnerId, Impact.HitDamage * Focal * static_cast<float>(Player.FocalStacks), Flag.Entry.SourceId, View);
			}
		}
		const int32 Every = FMath::RoundToInt(Params.Get(ParamPulseEvery));
		if (Every > 0 && ++Player.CritCount % Every == 0)
		{
			HitCircle(Context, Impact.OwnerId, INDEX_NONE, Flag.Entry.SourceId, At, Params.Get(ParamPulseUu), Impact.HitDamage * Params.Get(ParamPulseDmg), false, nullptr);
		}
	}
}

void FDTWeaponSim::NoteNova(const FDTHit& Hit, const FDTWeaponSpec& Spec, float Overkill)
{
	if (const FDTResolvedFlag* Nova = Spec.FindFlag(DTCat::EFlag::Nova))
	{
		NovaOverkills.Add(FOverkill{ Hit.EnemySerial, Hit.OwnerId, Hit.WeaponSlot, Nova->Entry.SourceId, Overkill });
	}
}

void FDTWeaponSim::StepNovas(const FDTSimContext& Context)
{
	using namespace DTWeaponSimKeepersLocal;
	FDTWorldView& View = *Context.View;
	for (const FOverkill& Over : NovaOverkills)
	{
		const FDTKill* Kill = View.Kills.FindByPredicate([&Over](const FDTKill& K) { return K.EnemySerial == Over.Serial; });
		const FDTFlagEntry* Nova = KeeperEntry(Over.OwnerId, DTCat::EFlag::Nova);
		if (!Kill || !Nova)
		{
			continue;
		}
		const FDTFlagParams& Params = Nova->Params;
		const float Ring = Params.Get(ParamRingUu);
		const bool bInRing = Ring > 0.0f && FVector2f::Distance(Kill->Position, HeroOf(View, Over.OwnerId)) <= Ring;
		const float Damage = Over.Damage * Params.Get(DTFlagParam::Dmg, 1.0f) * (bInRing ? Params.Get(ParamRingMult, 1.0f) : 1.0f);
		HitCircle(Context, Over.OwnerId, INDEX_NONE, Over.SourceId, Kill->Position, Params.Get(DTFlagParam::RadiusUu), Damage, false, nullptr);
	}
	NovaOverkills.Reset();
	// Halo Ring's sun: what was stored goes to the nearest elite or boss, once per interval.
	for (int32 Owner = 0; Owner < Players.Num(); ++Owner)
	{
		FPlayerRuntime& Player = Players[Owner];
		Player.SunReadyIn -= Context.StepSeconds;
		const FDTFlagEntry* Nova = Player.SunStored > 0.0f ? KeeperEntry(Owner, DTCat::EFlag::Nova) : nullptr;
		if (!Nova || Player.SunReadyIn > 0.0f)
		{
			continue;
		}
		const FVector2f Hero = HeroOf(View, Owner);
		int32 Best = INDEX_NONE;
		float BestDist = FMath::Square(static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU));
		for (const int32 Enemy : PriorityTargets(View))
		{
			const float D = FVector2f::DistSquared(Hero, View.EnemyPositions[Enemy]);
			if (D <= BestDist)
			{
				BestDist = D;
				Best = Enemy;
			}
		}
		if (Best != INDEX_NONE)
		{
			KeeperHit(View, Best, Owner, Player.SunStored, Nova->SourceId, View);
			Player.SunStored = 0.0f;
			Player.SunReadyIn = Nova->Params.Get(ParamSunIntervalS);
		}
	}
}

void FDTWeaponSim::AccrueSun(const FDTWorldView& View)
{
	using namespace DTWeaponSimKeepersLocal;
	bool bAny = false;
	for (int32 Owner = 0; Owner < Players.Num(); ++Owner)
	{
		const FDTFlagEntry* Nova = KeeperEntry(Owner, DTCat::EFlag::Nova);
		bAny |= Nova && Nova->Params.Get(ParamSunShare) > 0.0f;
	}
	if (!bAny || !Enemies)
	{
		return;
	}
	const TConstArrayView<uint8> Flags = Enemies->GetFlags();
	const TConstArrayView<EDTEnemyRole> Roles = Enemies->GetRoles();
	for (const FDTHit& Hit : View.Hits)
	{
		if (Hit.WeaponSlot == INDEX_NONE || !Players.IsValidIndex(Hit.OwnerId) || !Flags.IsValidIndex(Hit.EnemyIndex))
		{
			continue;
		}
		const FDTFlagEntry* Nova = KeeperEntry(Hit.OwnerId, DTCat::EFlag::Nova);
		const float Share = Nova ? Nova->Params.Get(ParamSunShare) : 0.0f;
		if (Share > 0.0f && ((Flags[Hit.EnemyIndex] & DTEnemyFlags::Elite) != 0 || Roles[Hit.EnemyIndex] == EDTEnemyRole::Boss))
		{
			Players[Hit.OwnerId].SunStored += Hit.Damage * Share;
		}
	}
}

// ---- hero motion -----------------------------------------------------------------------------------------------------------

float FDTWeaponSim::GetMomentum(int32 OwnerId) const
{
	return Players.IsValidIndex(OwnerId) ? Players[OwnerId].Momentum : 0.0f;
}

void FDTWeaponSim::GrantMomentum(int32 OwnerId, float Stacks, bool bHold)
{
	FPlayerRuntime& Player = EnsurePlayer(OwnerId);
	Player.Momentum = FMath::Max(Player.Momentum, Stacks);
	Player.bMomentumHold = bHold;
}

float FDTWeaponSim::TakeDashRefund(int32 OwnerId)
{
	if (!Players.IsValidIndex(OwnerId))
	{
		return 0.0f;
	}
	const float Out = Players[OwnerId].DashRefund;
	Players[OwnerId].DashRefund = 0.0f;
	return Out;
}

int32 FDTWeaponSim::TakeDashRefills(int32 OwnerId)
{
	if (!Players.IsValidIndex(OwnerId))
	{
		return 0;
	}
	const int32 Out = Players[OwnerId].DashRefills;
	Players[OwnerId].DashRefills = 0;
	return Out;
}

float FDTWeaponSim::HpOf(int32 Enemy) const
{
	return Enemies && Enemy >= 0 && Enemy < Enemies->Num() ? Enemies->GetHp()[Enemy] : 0.0f;
}

float FDTWeaponSim::BestCritMult(int32 OwnerId) const
{
	float Best = 1.0f;
	if (Players.IsValidIndex(OwnerId))
	{
		for (const FWeaponRuntime& Weapon : Players[OwnerId].Weapons)
		{
			Best = FMath::Max(Best, Weapon.Spec.CritMult);
		}
	}
	return Best;
}

void FDTWeaponSim::AddFlash(int32 OwnerId, const FVector2f& Position, const FVector2f& Dir, float Reach, float ArcDeg, float Seconds)
{
	FArcFlash& Flash = Arcs.AddDefaulted_GetRef();
	Flash.OwnerId = OwnerId;
	Flash.Position = Position;
	Flash.Dir = Dir;
	Flash.Reach = Reach;
	Flash.ArcDeg = ArcDeg;
	Flash.TimeLeft = Seconds;
}

void FDTWeaponSim::StepKeeperHero(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, FPlayerRuntime& Player, FDTPlayerRunState& RunPlayer)
{
	using namespace DTWeaponSimKeepersLocal;
	const FDTWorldView& ConstView = *Context.View;
	const FVector2f Hero = HeroOf(ConstView, OwnerId);
	const float Moved = Player.bHasHeroPos ? FVector2f::Distance(Hero, Player.LastHeroPos) : 0.0f;
	const FVector2f Line = SafeDir(Hero - Player.LastHeroPos, ConstView.HeroAimDirs.IsValidIndex(OwnerId) ? ConstView.HeroAimDirs[OwnerId] : FVector2f(1.0f, 0.0f));
	Player.LastHeroPos = Hero;
	Player.bHasHeroPos = true;
	const bool bIntangible = ConstView.HeroIntangible.IsValidIndex(OwnerId) && ConstView.HeroIntangible[OwnerId];
	const bool bDashStart = bIntangible && !Player.bDashWas;
	const bool bDashLand = !bIntangible && Player.bDashWas;
	Player.bDashWas = bIntangible;
	const bool bHurt = ConstView.HeroHurt.IsValidIndex(OwnerId) && ConstView.HeroHurt[OwnerId];

	if (const FDTFlagEntry* Speed = KeeperEntry(OwnerId, DTCat::EFlag::SpeedScale))
	{
		const FDTFlagParams& Params = Speed->Params;
		const float Cap = Params.Get(ParamStacksMax);
		float Stacks = Player.Momentum + Moved / UuPerMetre * Params.Get(ParamPer100Uu);
		Stacks += bDashStart ? Params.Get(ParamDashStacks) : 0.0f;
		Stacks -= bHurt ? Params.Get(ParamHitLoss) : 0.0f;
		Stacks -= Moved <= KINDA_SMALL_NUMBER && !Player.bMomentumHold ? Params.Get(ParamDecayPerS) * Context.StepSeconds : 0.0f;
		if (Player.SlipstreamRow != INDEX_NONE)
		{
			// R-RFG-21: no cap; a leak on what is held, so the level stands where speed balances it.
			Stacks -= Stacks * DTReforge::Param(Player.SlipstreamRow, TEXT("leak_per_s")) * Context.StepSeconds;
			Player.Momentum = FMath::Max(Stacks, 0.0f);
		}
		else
		{
			Player.Momentum = FMath::Clamp(Stacks, 0.0f, Cap);
		}
		const float Mult = 1.0f + Player.Momentum * Params.Get(ParamPerStack);
		for (FWeaponRuntime& Weapon : Player.Weapons)
		{
			Weapon.Spec.Damage *= Mult;
			Weapon.Spec.Cooldown /= Mult;
			Weapon.Spec.Pierce += Cap > 0.0f && Player.Momentum >= Cap ? FMath::RoundToInt(Params.Get(ParamMaxPierce)) : 0;
		}
		const float Every = Params.Get(ParamWaveEveryUu);
		if (Every > 0.0f)
		{
			Player.WaveDistance += Moved;
			while (Player.WaveDistance >= Every)
			{
				Player.WaveDistance -= Every;
				HitCircle(Context, OwnerId, INDEX_NONE, Speed->SourceId, Hero, Params.Get(ParamWaveUu), StrongestDamage(OwnerId) * Params.Get(ParamWaveDmg), false, nullptr);
			}
		}
	}

	Player.TailwindLeft -= Context.StepSeconds;
	const FDTFlagEntry* Dash = KeeperEntry(OwnerId, DTCat::EFlag::VolleyOnDash);
	if (Dash && bDashStart)
	{
		++Player.DashesDone;
		Player.DashRefundUsed = 0.0f;
		Player.TailwindLeft = Dash->Params.Get(ParamTailwindS);
		const int32 Every = FMath::RoundToInt(Dash->Params.Get(ParamFreeEvery));
		Player.DashRefills += Every > 0 && Player.DashesDone % Every == 0 ? 1 : 0;
	}
	if (Dash && Player.TailwindLeft > 0.0f)
	{
		const float Rate = FMath::Max(1.0f, Dash->Params.Get(ParamTailwindRate, 1.0f));
		for (FWeaponRuntime& Weapon : Player.Weapons)
		{
			Weapon.Spec.Cooldown /= Rate;
		}
	}
	if (Dash && (bDashStart || bDashLand))
	{
		VolleyDir = bDashStart ? Line : Player.LastDashLine;
		Player.LastDashLine = Line;
		VolleyMult = Dash->Params.Get(DTFlagParam::DmgMult, 1.0f);
		for (int32 Slot = 0; Slot < Player.Weapons.Num(); ++Slot)
		{
			FWeaponRuntime& Runtime = Player.Weapons[Slot];
			// Melee sweeps at the landing (flag_compat.csv); everything else fires at the start.
			if ((Runtime.Spec.Pattern == EDTWeaponPattern::MeleeArc) == bDashLand)
			{
				bVolleyForced = true;
				FireVolley(Context, Bullets, RunPlayer, Slot, Runtime, Dash->SourceId, true);
				bVolleyForced = false;
			}
		}
		VolleyMult = 1.0f;
	}
}

void FDTWeaponSim::StepKeeperState(const FDTSimContext& Context)
{
	using namespace DTWeaponSimKeepersLocal;
	for (FPlayerRuntime& Player : Players)
	{
		PurgeUntil(Player.MarkedUntilStep, Context.StepIndex);
		PurgeUntil(Player.BlindedUntilStep, Context.StepIndex);
	}
}
