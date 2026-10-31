#include "Combat/DTWeaponSim.h"
#include "Combat/DTBulletPool.h"
#include "Core/DTConstantsGen.h"
#include "Enemies/DTEnemyPool.h"
#include "Telemetry/DTTelemetrySources.h"

// A cleave with apply_modifier_on_hit also bounces a modifier's Ricochet off each enemy it strikes (Split is dormant under a conversion).
// Which reactions an impact runs (R-WPN-08 native expressions; fallbacks run at activation, OnActivation):
//   RoundHit   (a pool round hit an enemy):  Split on=Hit, Field, Pull, StatusApply, Chain, Echo, Ricochet when spent
//   ArcHit     (one enemy inside a sweep):    StatusApply (on=Cleave for converted cleaves), Chain, Backwash pull
//   ArcSweep   (one per sweep, at mid reach): Field, Pull, Echo
//   MineHit    (one enemy in a detonation):   StatusApply, Chain
//   Detonation (one per mine):                Split (children burst), Field, Pull, Echo (fallback: repeats in place)
//   Expiry     (a tracked round's life ran out): Split on=Expiry (Mod.ScatterShell), Mod.SpreadWave's wave
// Field ticks, echo strikes, chain jumps, slams and mine pull ticks are hits but never impacts (no reaction chains).
// Kills (StepEffects, ReactToKills): arc forks, burning explosions, Burn spread. Crush, pulled_vuln and explosion hits carry
// no weapon slot: they are Keeper-only and never feed the enchantment or other reactions.

namespace DTWeaponSimLocal
{
	static FVector2f Rotate(const FVector2f& V, float Deg)
	{
		float S = 0.0f;
		float C = 1.0f;
		FMath::SinCos(&S, &C, FMath::DegreesToRadians(Deg));
		return FVector2f(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}

	static FVector2f SafeDir(const FVector2f& V, const FVector2f& Fallback)
	{
		const float Len = V.Size();
		return Len > KINDA_SMALL_NUMBER ? V / Len : Fallback;
	}

	static FVector2f HeroOf(const FDTWorldView& View, int32 OwnerId)
	{
		return View.HeroPositions.IsValidIndex(OwnerId) ? View.HeroPositions[OwnerId] : FVector2f::ZeroVector;
	}

	/** Frost's held control as the Keeper flags state it: the sim (StepHeldControl) and the cues read the same words. */
	struct FHeldControl
	{
		bool bBlocks = false;
		int32 HeldMax = 0;
		uint16 Source = 0;
	};

	static FHeldControl ReadHeldControl(const FDTWeaponSpec& Spec)
	{
		FHeldControl Out;
		for (const FDTResolvedFlag& Flag : Spec.Flags)
		{
			const FDTFlagParams& Params = Flag.Entry.Params;
			if (Flag.Entry.Flag == DTCat::EFlag::StatusApply && (Params.Has(DTFlagParam::FrozenBlocksEnemies) || Params.Has(DTFlagParam::FreezeUntilHit)))
			{
				Out.bBlocks |= Params.Get(DTFlagParam::FrozenBlocksEnemies) > 0.0f;
				Out.HeldMax = Params.Get(DTFlagParam::FreezeUntilHit) > 0.0f ? FMath::RoundToInt(Params.Get(DTFlagParam::HeldMax)) : Out.HeldMax;
				Out.Source = Flag.Entry.SourceId;
			}
		}
		return Out;
	}

	static int32 NumEnemies(const FDTWorldView& View)
	{
		return FMath::Min3(View.EnemyPositions.Num(), View.EnemyRadii.Num(), View.EnemySerials.Num());
	}

	static bool ParseStatus(const FString& Word, EDTStatus& Out)
	{
		static const TCHAR* Names[] = { TEXT("Status.Shock"), TEXT("Status.Burn"), TEXT("Status.Chill"), TEXT("Status.Freeze"), TEXT("Status.Mark"), TEXT("Status.Blind") };
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			if (Word.StartsWith(Names[Index]))
			{
				Out = static_cast<EDTStatus>(Index);
				return true;
			}
		}
		return false;
	}

	/** The status a Patron applies: the first status of its archetypes' enchantment (keepers.md R-KEE-09). */
	static bool PatronStatus(EDTPath Path, EDTStatus& Out)
	{
		for (const DTCat::FArchetypeRow& Row : DTCat::Archetypes)
		{
			if (Row.Path == Path)
			{
				return ParseStatus(Row.EnchantStatus, Out);
			}
		}
		return false;
	}

	/** The weapon's own rounds' motion (R-BUL-09). An orbit adds its hold to the flight; a return doubles the flight. */
	static void ApplyMotion(const FDTWeaponSpec& Spec, float& InOutLife, FDTBulletMotion& Out)
	{
		Out.TurnDegS = Spec.HomingTurnDegS;
		Out.SeekUu = Spec.HomingSeekUu;
		Out.OrbitLeft = Spec.OrbitHoldS;
		Out.OrbitRadius = Spec.OrbitRadiusUu;
		Out.OrbitDegS = static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_DEG_S);
		Out.ReleaseRange = Spec.Range * Spec.OrbitRangeRatio;
		Out.bBoomerang = Spec.bBoomerang;
		Out.ReturnAt = InOutLife;
		InOutLife = InOutLife * (Spec.bBoomerang ? 2.0f : 1.0f) + Spec.OrbitHoldS;
	}

	static bool NeedsLedger(const FDTWeaponSpec& Spec)
	{
		if (Spec.WaveSource != 0)
		{
			return true;
		}
		for (const FDTResolvedFlag& Flag : Spec.Flags)
		{
			if (Flag.Entry.Flag == DTCat::EFlag::Split || Flag.Entry.Flag == DTCat::EFlag::Ricochet ||
				(Flag.Entry.Flag == DTCat::EFlag::Pull && Flag.Entry.Params.GetWord(DTFlagParam::Target) == FName(TEXT("Origin"))))
			{
				return true;
			}
		}
		return false;
	}

	static const FDTWeaponSpec EmptySpec;
	static const FDTWeaponCounters EmptyCounters;
	static const FName WordHit(TEXT("Hit"));
	static const FName WordExpiry(TEXT("Expiry"));
	static const FName WordVolley(TEXT("Volley"));
	static const FName WordCleave(TEXT("Cleave"));
	static const FName WordPulled(TEXT("Pulled"));
	static const FName WordChainHit(TEXT("ChainHit"));
	static const FName WordRing(TEXT("Ring"));
	static const FName WordOrigin(TEXT("Origin"));
	static const FName WordField(TEXT("Field"));
	static const FName WordPatron(TEXT("Patron"));
	static const FName WordHitOnBurning(TEXT("HitOnBurning"));
	static const FName WordEliteOrBoss(TEXT("EliteOrBoss"));
	static const FName WordBurn(TEXT("Status.Burn"));
	static const FName WordFreeze(TEXT("Status.Freeze"));
	static const FName WordShock(TEXT("Status.Shock"));
	static const FName WordChargedVolley(TEXT("ChargedVolley"));
	static const FName WordHero(TEXT("Hero"));
	constexpr uint8 BurnBit = 1 << static_cast<int32>(EDTStatus::Burn);
	constexpr uint8 FreezeBit = 1 << static_cast<int32>(EDTStatus::Freeze);
	constexpr uint8 ShockBit = 1 << static_cast<int32>(EDTStatus::Shock);
}

// ------------------------------------------------------------------------------------------------------------------
// Inventory

EDTWeaponResult FDTWeaponSim::Give(FDTRunState& Run, int32 OwnerId, FName WeaponId, int32 Level, int32* OutSlot)
{
	if (!Run.IsValidPlayer(OwnerId))
	{
		return EDTWeaponResult::BadPlayer;
	}
	const int32 Weapon = DTWeapons::FindWeapon(WeaponId);
	if (Weapon == INDEX_NONE || DTWeapons::GetMaxLevel(Weapon) <= 0)
	{
		return EDTWeaponResult::UnknownId;
	}
	FDTPlayerRunState& Player = Run.GetPlayer(OwnerId);
	if (FindSlot(Run, OwnerId, DTWeapons::WeaponId(Weapon)) != INDEX_NONE)
	{
		return EDTWeaponResult::AlreadyOwned;
	}
	if (Player.Weapons.Num() >= DTConst::WEAPON_SLOTS)
	{
		return EDTWeaponResult::NoSlot;
	}
	int32 Order = 0;
	for (const FDTWeaponInstance& Owned : Player.Weapons)
	{
		Order = FMath::Max(Order, Owned.AcquiredOrder + 1);
	}
	FDTWeaponInstance& Instance = Player.Weapons.AddDefaulted_GetRef();
	Instance.Id = DTWeapons::WeaponId(Weapon);
	Instance.Def = Weapon;
	Instance.Level = FMath::Clamp(Level, 1, DTWeapons::GetMaxLevel(Weapon));
	Instance.AcquiredOrder = Order;
	Instance.SourceId = DTWeapons::WeaponSource(Weapon);
	if (OutSlot)
	{
		*OutSlot = Player.Weapons.Num() - 1;
	}
	SyncPlayers(Run);
	return EDTWeaponResult::Ok;
}

EDTWeaponResult FDTWeaponSim::LevelUp(FDTRunState& Run, int32 OwnerId, int32 Slot, int32 Level)
{
	if (!Run.IsValidPlayer(OwnerId))
	{
		return EDTWeaponResult::BadPlayer;
	}
	FDTPlayerRunState& Player = Run.GetPlayer(OwnerId);
	if (!Player.Weapons.IsValidIndex(Slot))
	{
		return EDTWeaponResult::NoWeapon;
	}
	FDTWeaponInstance& Instance = Player.Weapons[Slot];
	const int32 Max = DTWeapons::GetMaxLevel(Instance.Def);
	const int32 Wanted = Level > 0 ? Level : Instance.Level + 1;
	if (Wanted > Max || Wanted < 1)
	{
		return EDTWeaponResult::MaxLevel;
	}
	Instance.Level = Wanted;
	return EDTWeaponResult::Ok;
}

EDTWeaponResult FDTWeaponSim::Socket(FDTRunState& Run, int32 OwnerId, int32 Slot, FName ModId)
{
	if (!Run.IsValidPlayer(OwnerId))
	{
		return EDTWeaponResult::BadPlayer;
	}
	FDTPlayerRunState& Player = Run.GetPlayer(OwnerId);
	if (!Player.Weapons.IsValidIndex(Slot))
	{
		return EDTWeaponResult::NoWeapon;
	}
	const int32 Mod = DTWeapons::FindModifier(ModId);
	if (Mod == INDEX_NONE)
	{
		return EDTWeaponResult::UnknownId;
	}
	FDTWeaponInstance& Instance = Player.Weapons[Slot];
	const FName Id = DTWeapons::ModifierId(Mod);
	if (Instance.Sockets.ContainsByPredicate([Id](const FDTWeaponInstance::FSocket& S) { return S.ModId == Id; }))
	{
		return EDTWeaponResult::Duplicate;
	}
	if (Instance.Sockets.Num() >= DTConst::MOD_SOCKETS_PER_WEAPON)
	{
		return EDTWeaponResult::SocketsFull;
	}
	const FDTWeaponSpec Spec = ComposeSlot(Run, OwnerId, Slot);
	if (!DTModCompat::IsCompatible(Spec.EffectiveCaps, Mod))
	{
		return EDTWeaponResult::Incompatible;
	}
	FDTWeaponInstance::FSocket& Added = Instance.Sockets.AddDefaulted_GetRef();
	Added.ModId = Id;
	return EDTWeaponResult::Ok;
}

int32 FDTWeaponSim::FindSlot(const FDTRunState& Run, int32 OwnerId, FName WeaponId)
{
	if (!Run.IsValidPlayer(OwnerId))
	{
		return INDEX_NONE;
	}
	const int32 Weapon = DTWeapons::FindWeapon(WeaponId);
	const TArray<FDTWeaponInstance>& Weapons = Run.GetPlayer(OwnerId).Weapons;
	for (int32 Slot = 0; Slot < Weapons.Num(); ++Slot)
	{
		if (Weapons[Slot].Def == Weapon)
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

// ------------------------------------------------------------------------------------------------------------------
// Flag sources, inputs, composition

FDTWeaponSim::FPlayerRuntime& FDTWeaponSim::EnsurePlayer(int32 OwnerId)
{
	if (Players.Num() <= OwnerId)
	{
		Players.SetNum(OwnerId + 1);
	}
	return Players[OwnerId];
}

FDTWeaponSim::FWeaponRuntime* FDTWeaponSim::FindRuntime(int32 OwnerId, int32 Slot)
{
	return (Players.IsValidIndex(OwnerId) && Players[OwnerId].Weapons.IsValidIndex(Slot)) ? &Players[OwnerId].Weapons[Slot] : nullptr;
}

void FDTWeaponSim::SyncPlayers(FDTRunState& Run)
{
	if (Run.GetNumPlayers() > 0)
	{
		EnsurePlayer(Run.GetNumPlayers() - 1);
	}
	for (int32 Owner = 0; Owner < Run.GetNumPlayers(); ++Owner)
	{
		Players[Owner].Weapons.SetNum(Run.GetPlayer(Owner).Weapons.Num());
	}
}

void FDTWeaponSim::AddFlagSource(int32 OwnerId, FName SourceTag, const FDTWeaponFlags& Flags)
{
	if (OwnerId < 0)
	{
		return;
	}
	FPlayerRuntime& Player = EnsurePlayer(OwnerId);
	for (FFlagSource& Source : Player.Sources)
	{
		if (Source.Tag == SourceTag)
		{
			Source.Flags = Flags;
			return;
		}
	}
	Player.Sources.Add(FFlagSource{ SourceTag, Flags });
}

void FDTWeaponSim::RemoveFlagSource(int32 OwnerId, FName SourceTag)
{
	if (Players.IsValidIndex(OwnerId))
	{
		Players[OwnerId].Sources.RemoveAll([SourceTag](const FFlagSource& S) { return S.Tag == SourceTag; });
	}
}

void FDTWeaponSim::ClearFlagSources(int32 OwnerId)
{
	if (Players.IsValidIndex(OwnerId))
	{
		Players[OwnerId].Sources.Reset();
	}
}

TArray<FDTFlagEntry> FDTWeaponSim::GetPlayerFlags(int32 OwnerId) const
{
	TArray<FDTFlagEntry> Out;
	if (Players.IsValidIndex(OwnerId))
	{
		for (const FFlagSource& Source : Players[OwnerId].Sources)
		{
			Out.Append(Source.Flags.Entries);
		}
	}
	return Out;
}

void FDTWeaponSim::SetHeroStats(int32 OwnerId, const FDTWeaponHeroStats& Stats)
{
	if (OwnerId >= 0)
	{
		EnsurePlayer(OwnerId).Stats = Stats;
	}
}

void FDTWeaponSim::SetAimManual(int32 OwnerId, bool bManual)
{
	if (OwnerId >= 0)
	{
		EnsurePlayer(OwnerId).bAimManual = bManual;
	}
}

FDTWeaponSpec FDTWeaponSim::ComposeSlot(FDTRunState& Run, int32 OwnerId, int32 Slot) const
{
	if (!Run.IsValidPlayer(OwnerId) || !Run.GetPlayer(OwnerId).Weapons.IsValidIndex(Slot))
	{
		return FDTWeaponSpec();
	}
	const TArray<FDTWeaponInstance>& Weapons = Run.GetPlayer(OwnerId).Weapons;
	const TArray<int32> Ranks = DTWeaponCompose::LoadRanks(Weapons);
	FDTWeaponInstance Copy = Weapons[Slot];
	const FDTWeaponHeroStats Stats = Players.IsValidIndex(OwnerId) ? Players[OwnerId].Stats : FDTWeaponHeroStats();
	return DTWeaponCompose::Compose(Copy, GetPlayerFlags(OwnerId), Stats, Ranks[Slot]);
}

const FDTWeaponCounters& FDTWeaponSim::GetCounters(int32 OwnerId, int32 Slot) const
{
	return (Players.IsValidIndex(OwnerId) && Players[OwnerId].Weapons.IsValidIndex(Slot)) ? Players[OwnerId].Weapons[Slot].Counters : DTWeaponSimLocal::EmptyCounters;
}

const FDTWeaponSpec& FDTWeaponSim::GetLastSpec(int32 OwnerId, int32 Slot) const
{
	return (Players.IsValidIndex(OwnerId) && Players[OwnerId].Weapons.IsValidIndex(Slot)) ? Players[OwnerId].Weapons[Slot].Spec : DTWeaponSimLocal::EmptySpec;
}

int32 FDTWeaponSim::GetLiveMines(int32 OwnerId, int32 Slot) const
{
	return (Players.IsValidIndex(OwnerId) && Players[OwnerId].Weapons.IsValidIndex(Slot)) ? Players[OwnerId].Weapons[Slot].Mines.Num() : 0;
}

void FDTWeaponSim::ResetTransient()
{
	for (FPlayerRuntime& Player : Players)
	{
		Player.Weapons.Reset();
		Player.StillTime = 0.0f;
		Player.bHasAnchor = false;
		Player.bWasIntangible = false;
		Player.PulledUntilStep.Reset();
		Player.SpreadReadyIn = 0.0f;
		Player.HeldFrozen.Reset();
		Player.FrozenStruck.Reset();
		Player.AuraReadyIn = 0.0f;
		Player.bSlamBoosted = false;
		Player.SkyStrikes = 0;
		Player.MarkedUntilStep.Reset();
		Player.BlindedUntilStep.Reset();
		Player.Debt = 0;
		Player.PaydayMult = 0.0f;
		Player.PaydayExecute = 0.0f;
		Player.LastingKills = 0;
		Player.LastingLive = 0;
		Player.LastCritSerial = 0;
		Player.FocalStacks = 0;
		Player.CritCount = 0;
		Player.SunStored = 0.0f;
		Player.SunReadyIn = 0.0f;
		Player.Momentum = 0.0f;
		Player.bMomentumHold = false;
		Player.WaveDistance = 0.0f;
		Player.bHasHeroPos = false;
		Player.bDashWas = false;
		Player.TailwindLeft = 0.0f;
		DuoHits.Reset();
		ReactionEvents.Reset();
		Player.ReactionsFired = 0;
		Player.ReactionCounters.Reset();
		Player.ReactionReadyStep.Reset();
		Player.StepPulled.Reset();
		Player.ToastRow = INDEX_NONE;
		Player.ToastUntilStep = 0;
		Player.WildfireLeft = 0.0f;
		Player.DuoChargeLeft = 0.0f;
		Player.DuoRefundReadyIn = 0.0f;
		Player.TrailDistance = 0.0f;
		Player.bDuoDashWas = false;
		Player.ConductedJumps = 0;
		Player.DuoSpeedMul = 1.0f;
		Player.DashesDone = 0;
		Player.DashRefund = 0.0f;
		Player.DashRefundUsed = 0.0f;
		Player.DashRefills = 0;
	}
	Hunters.Reset();
	NovaOverkills.Reset();
	Overkills.Reset();
	Rounds.Reset();
	Fields.Reset();
	Echoes.Reset();
	Arcs.Reset();
	Impacts.Reset();
	PendingStatuses.Reset();
	ArcMarks.Reset();
	StepCleaved.Reset();
	StepSlammed.Reset();
	StepClustered.Reset();
	WeaponsHitsEnd = 0;
	MaxGeneration = 0;
	LastStep = -1;
}

// ------------------------------------------------------------------------------------------------------------------
// Emission primitives

bool FDTWeaponSim::TryEmit(FWeaponRuntime& Runtime)
{
	if (Runtime.EmitRing.Num() == 0)
	{
		Runtime.EmitRing.SetNumZeroed(1);
	}
	if (Runtime.EmitWindow >= DTConst::WEAPON_SPAWN_RATE_MAX_PER_S)
	{
		++Runtime.Counters.Throttled;
		return false;
	}
	++Runtime.EmitWindow;
	++Runtime.EmitRing[Runtime.EmitCursor];
	++Runtime.Counters.Spawned;
	return true;
}

bool FDTWeaponSim::RollCrit(const FDTSimContext& Context, const FDTWeaponSpec& Spec, float& InOutDamage) const
{
	if (Spec.CritChance <= 0.0f || !Context.Run)
	{
		return false;
	}
	if (Context.Run->GetRng().Get(EDTRngStream::Combat).FRand() < Spec.CritChance)
	{
		InOutDamage *= Spec.CritMult;
		return true;
	}
	return false;
}

FDTWeaponSim::FShot FDTWeaponSim::MakeShot(int32 OwnerId, int32 Slot, const FWeaponRuntime& Runtime, uint16 SourceId, const FVector2f& Origin) const
{
	const FDTWeaponSpec& Spec = Runtime.Spec;
	FShot Shot;
	Shot.OwnerId = OwnerId;
	Shot.Slot = Slot;
	Shot.SourceId = SourceId;
	Shot.Origin = Origin;
	Shot.Damage = Spec.Damage;
	Shot.Pierce = Spec.Pierce;
	Shot.Speed = Spec.Speed;
	Shot.Radius = Spec.Radius;
	Shot.Life = Spec.Life;
	Shot.bTrack = DTWeaponSimLocal::NeedsLedger(Spec);
	for (const FDTResolvedFlag& Flag : Spec.Flags)
	{
		if (Flag.Entry.Flag == DTCat::EFlag::Ricochet && Flag.IsNative())
		{
			Shot.Bounces += FMath::RoundToInt(Flag.Entry.Params.Get(DTFlagParam::Bounces)); // a modifier and a Keeper add up
		}
	}
	return Shot;
}

void FDTWeaponSim::SpawnRound(const FDTSimContext& Context, FDTBulletPool* Bullets, FWeaponRuntime& Runtime, const FShot& Shot,
	const FVector2f& Position, const FVector2f& Dir)
{
	if (!TryEmit(Runtime))
	{
		return;
	}
	float Damage = Shot.Damage;
	const bool bCrit = RollShot(Context, Runtime.Spec, Shot, Damage);
	FDTBulletSpawn Spawn;
	Spawn.Position = Position;
	Spawn.Velocity = Dir * Shot.Speed;
	Spawn.Radius = Shot.Radius;
	Spawn.Damage = Damage;
	Spawn.Life = Shot.Life;
	Spawn.OwnerId = Shot.OwnerId;
	Spawn.Pierce = FMath::Max(1, Shot.Pierce);
	Spawn.SourceId = Shot.SourceId;
	Spawn.WeaponSlot = static_cast<int8>(Shot.Slot);
	Spawn.bCrit = bCrit;
	const bool bMoving = Shot.Motion.IsActive();
	const uint32 RoundId = bMoving && Shot.bTrack ? ++NextRoundId : 0;
	if (bMoving)
	{
		Spawn.Motion = Shot.Motion;
		Spawn.Motion.OrbitAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
		Spawn.Motion.RoundId = RoundId;
	}
	if (Bullets)
	{
		Bullets->Spawn(Spawn);
	}
	MaxGeneration = FMath::Max(MaxGeneration, Shot.Generation);
	if (Shot.bTrack)
	{
		FRound& Round = Rounds.AddDefaulted_GetRef();
		Round.OwnerId = Shot.OwnerId;
		Round.Slot = static_cast<int8>(Shot.Slot);
		Round.SourceId = Shot.SourceId;
		Round.Position = Spawn.Position;
		Round.Velocity = Spawn.Velocity;
		Round.Origin = Shot.Origin;
		Round.LifeLeft = Spawn.Life;
		Round.Radius = Spawn.Radius;
		Round.BaseDamage = Shot.Damage;
		Round.PierceLeft = Spawn.Pierce;
		Round.Generation = Shot.Generation;
		Round.BouncesLeft = Shot.Bounces;
		Round.bCharged = Shot.bCharged;
		Round.bEmber = Shot.bEmber;
		Round.bLance = Shot.bLance;
		Round.DuoRow = Shot.DuoRow;
		Round.RoundId = RoundId;
		Round.Pierce = Spawn.Pierce;
		Round.bBoomerang = Shot.Motion.bBoomerang;
	}
}

int32 FDTWeaponSim::EmitArc(const FDTSimContext& Context, FWeaponRuntime& Runtime, const FShot& Shot, const FVector2f& Dir,
	float ArcDeg, float Reach, bool bCleave, bool bImpacts)
{
	if (!TryEmit(Runtime))
	{
		return 0;
	}
	FDTWorldView& View = *Context.View;
	float Damage = Shot.Damage;
	const bool bCrit = RollShot(Context, Runtime.Spec, Shot, Damage);
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(0.5f * ArcDeg));
	const bool bFull = ArcDeg >= 360.0f;
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	Candidates.Reset();
	if (Num > 0 && View.EnemyHash.Num() > 0)
	{
		View.EnemyHash.QueryCircle(Shot.Origin, Reach + MaxEnemyRadius, Candidates);
	}
	int32 Hit = 0;
	const FDTResolvedFlag* Melee = Runtime.Spec.FindFlag(DTCat::EFlag::ConvertMelee);
	const bool bMarkKills = Melee && Melee->Entry.Params.Has(DTFlagParam::BurningExplodeOnDeathUu);
	for (const int32 Enemy : Candidates)
	{
		if (Enemy < 0 || Enemy >= Num)
		{
			continue;
		}
		const FVector2f ToEnemy = View.EnemyPositions[Enemy] - Shot.Origin;
		const float Dist = ToEnemy.Size();
		if (Dist > Reach + View.EnemyRadii[Enemy])
		{
			continue;
		}
		if (!bFull && Dist > KINDA_SMALL_NUMBER && FVector2f::DotProduct(ToEnemy / Dist, Dir) < CosHalf)
		{
			continue;
		}
		FDTHit& Out = View.Hits.AddDefaulted_GetRef();
		Out.EnemyIndex = Enemy;
		Out.EnemySerial = View.EnemySerials[Enemy];
		Out.OwnerId = Shot.OwnerId;
		Out.Damage = Damage;
		// The contact point: the rim of the enemy facing the sweep's origin (R-ENM-10 reads it for the Tank's plate).
		Out.Position = Dist > KINDA_SMALL_NUMBER ? View.EnemyPositions[Enemy] - ToEnemy / Dist * View.EnemyRadii[Enemy] : View.EnemyPositions[Enemy];
		Out.SourceId = Shot.SourceId;
		Out.WeaponSlot = static_cast<int8>(Shot.Slot);
		Out.bCrit = bCrit;
		++Hit;
		if (!bImpacts)
		{
			continue; // a wave strikes and starts no reaction
		}

		FImpact& Impact = Impacts.AddDefaulted_GetRef();
		Impact.Kind = EImpact::ArcHit;
		Impact.OwnerId = Shot.OwnerId;
		Impact.Slot = static_cast<int8>(Shot.Slot);
		Impact.SourceId = Shot.SourceId;
		Impact.Position = Out.Position;
		Impact.Origin = Shot.Origin;
		Impact.Dir = Dir;
		Impact.BaseDamage = Shot.Damage;
		Impact.HitDamage = Damage;
		Impact.EnemySerial = Out.EnemySerial;
		Impact.EnemyRadius = View.EnemyRadii[Enemy];
		Impact.EnemyPosition = View.EnemyPositions[Enemy];
		Impact.bCleave = bCleave;
		Impact.bCharged = Shot.bCharged;
		Impact.bCrit = bCrit;
		Impact.StatusMask = StatusMaskOf(Enemy, Out.EnemySerial);
		if (bMarkKills)
		{
			StepCleaved.Add(Out.EnemySerial);
		}
		++Runtime.Counters.Impacts;
	}
	if (bImpacts)
	{
		FImpact& Sweep = Impacts.AddDefaulted_GetRef();
		Sweep.Kind = EImpact::ArcSweep;
		Sweep.OwnerId = Shot.OwnerId;
		Sweep.Slot = static_cast<int8>(Shot.Slot);
		Sweep.SourceId = Shot.SourceId;
		Sweep.Origin = Shot.Origin;
		Sweep.Dir = Dir;
		Sweep.Position = Shot.Origin + Dir * (0.5f * Reach); // mid reach: where a sweep's field or strike lands
		Sweep.Area = 0.5f * Reach;
		Sweep.BaseDamage = Shot.Damage;
		Sweep.HitDamage = Damage;
		Sweep.bCleave = bCleave;
		Sweep.bCharged = Shot.bCharged;
	}

	FArcFlash& Flash = Arcs.AddDefaulted_GetRef();
	Flash.OwnerId = Shot.OwnerId;
	Flash.Position = Shot.Origin;
	Flash.Dir = Dir;
	Flash.Reach = Reach;
	Flash.ArcDeg = ArcDeg;
	Flash.TimeLeft = 0.5f * Runtime.Spec.Cooldown; // visible for half the weapon's cadence
	return Hit;
}

int32 FDTWeaponSim::HitCircle(const FDTSimContext& Context, int32 OwnerId, int32 Slot, uint16 SourceId, const FVector2f& Center,
	float Radius, float Damage, bool bRollCrit, const FDTWeaponSpec* Spec, TArray<FDTEnemyHandle>* OutHit)
{
	FDTWorldView& View = *Context.View;
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	if (Num == 0 || View.EnemyHash.Num() == 0 || Damage <= 0.0f)
	{
		return 0;
	}
	TArray<int32> Local;
	View.EnemyHash.QueryCircle(Center, Radius + MaxEnemyRadius, Local);
	int32 Hit = 0;
	for (const int32 Enemy : Local)
	{
		if (Enemy < 0 || Enemy >= Num)
		{
			continue;
		}
		const float Reach = Radius + View.EnemyRadii[Enemy];
		if (FVector2f::DistSquared(Center, View.EnemyPositions[Enemy]) > Reach * Reach)
		{
			continue;
		}
		float Final = Damage;
		const bool bCrit = bRollCrit && Spec && RollCrit(Context, *Spec, Final);
		FDTHit& Out = View.Hits.AddDefaulted_GetRef();
		Out.EnemyIndex = Enemy;
		Out.EnemySerial = View.EnemySerials[Enemy];
		Out.OwnerId = OwnerId;
		Out.Damage = Final;
		Out.Position = View.EnemyPositions[Enemy];
		Out.SourceId = SourceId;
		Out.WeaponSlot = static_cast<int8>(Slot);
		Out.bCrit = bCrit;
		if (OutHit)
		{
			OutHit->Add(FDTEnemyHandle{ Enemy, Out.EnemySerial });
		}
		++Hit;
	}
	return Hit;
}

bool FDTWeaponSim::FindNearest(const FDTWorldView& View, const FVector2f& From, float Range, uint32 ExcludeSerial, int32& OutEnemy) const
{
	OutEnemy = INDEX_NONE;
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	if (Num == 0 || View.EnemyHash.Num() == 0 || Range <= 0.0f)
	{
		return false;
	}
	TArray<int32> Local;
	View.EnemyHash.QueryCircle(From, Range, Local);
	float Best = Range * Range;
	for (const int32 Enemy : Local)
	{
		if (Enemy < 0 || Enemy >= Num || (ExcludeSerial != 0 && View.EnemySerials[Enemy] == ExcludeSerial))
		{
			continue;
		}
		const float D = FVector2f::DistSquared(From, View.EnemyPositions[Enemy]);
		// Within range, nearest first; ties on the lowest enemy index (R-WPN-12).
		const bool bBetter = OutEnemy == INDEX_NONE ? D <= Best : (D < Best || (D == Best && Enemy < OutEnemy));
		if (bBetter)
		{
			Best = D;
			OutEnemy = Enemy;
		}
	}
	return OutEnemy != INDEX_NONE;
}

void FDTWeaponSim::FindDensestCells(const FDTWorldView& View, const FVector2f& Hero, float Range, int32 Count, TArray<FVector2f>& Out) const
{
	Out.Reset();
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	if (Num == 0 || View.EnemyHash.Num() == 0 || Count <= 0)
	{
		return;
	}
	const float Cell = View.EnemyHash.GetCellSize();
	const float Half = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU);
	const int32 PerSide = View.EnemyHash.GetCellsPerSide();
	struct FCellCount { int32 Key = 0; int32 Count = 0; int32 Lowest = 0; };
	TArray<FCellCount> Cells;
	TArray<int32> Local;
	View.EnemyHash.QueryCircle(Hero, Range, Local);
	for (const int32 Enemy : Local)
	{
		if (Enemy < 0 || Enemy >= Num || FVector2f::DistSquared(Hero, View.EnemyPositions[Enemy]) > Range * Range)
		{
			continue;
		}
		const FVector2f P = View.EnemyPositions[Enemy];
		const int32 X = FMath::Clamp(FMath::FloorToInt((P.X + Half) / Cell), 0, PerSide - 1);
		const int32 Y = FMath::Clamp(FMath::FloorToInt((P.Y + Half) / Cell), 0, PerSide - 1);
		const int32 Key = Y * PerSide + X;
		FCellCount* Found = Cells.FindByPredicate([Key](const FCellCount& C) { return C.Key == Key; });
		if (!Found)
		{
			Found = &Cells.AddDefaulted_GetRef();
			Found->Key = Key;
			Found->Lowest = Enemy;
		}
		++Found->Count;
		Found->Lowest = FMath::Min(Found->Lowest, Enemy);
	}
	// Most enemies first; ties on the lowest enemy index (R-WPN-12).
	Cells.Sort([](const FCellCount& A, const FCellCount& B) { return A.Count != B.Count ? A.Count > B.Count : A.Lowest < B.Lowest; });
	for (int32 Index = 0; Index < Cells.Num() && Out.Num() < Count; ++Index)
	{
		const int32 X = Cells[Index].Key % PerSide;
		const int32 Y = Cells[Index].Key / PerSide;
		Out.Add(FVector2f((X + 0.5f) * Cell - Half, (Y + 0.5f) * Cell - Half));
	}
}

void FDTWeaponSim::EnemiesInRange(const FDTWorldView& View, const FVector2f& From, float Range, TArray<int32>& Out) const
{
	EnemiesInCircle(View, From, Range, Out);
}

bool FDTWeaponSim::FindDensestLine(const FDTWorldView& View, const FVector2f& Hero, float Range, FVector2f& OutDir) const
{
	struct FSector { int32 Count = 0; int32 Lowest = 0; FVector2f Sum = FVector2f::ZeroVector; };
	TArray<FSector, TInlineAllocator<128>> Sectors;
	Sectors.SetNum(DTConst::WEAPON_LINE_SECTORS);
	TArray<int32> Local;
	EnemiesInRange(View, Hero, Range, Local);
	const float Width = 360.0f / static_cast<float>(DTConst::WEAPON_LINE_SECTORS);
	for (const int32 Enemy : Local)
	{
		const FVector2f Dir = DTWeaponSimLocal::SafeDir(View.EnemyPositions[Enemy] - Hero, FVector2f(1.0f, 0.0f));
		const float Deg = FMath::Fmod(FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) + 360.0f, 360.0f);
		FSector& Sector = Sectors[FMath::Clamp(FMath::FloorToInt(Deg / Width), 0, DTConst::WEAPON_LINE_SECTORS - 1)];
		Sector.Lowest = Sector.Count == 0 ? Enemy : FMath::Min(Sector.Lowest, Enemy);
		++Sector.Count;
		Sector.Sum += Dir;
	}
	const FSector* Best = nullptr;
	for (const FSector& Sector : Sectors)
	{
		// Most enemies first; ties on the lowest enemy index (R-WPN-12).
		if (Sector.Count > 0 && (!Best || Sector.Count > Best->Count || (Sector.Count == Best->Count && Sector.Lowest < Best->Lowest)))
		{
			Best = &Sector;
		}
	}
	if (!Best)
	{
		return false;
	}
	OutDir = DTWeaponSimLocal::SafeDir(Best->Sum, FVector2f(1.0f, 0.0f));
	return true;
}

// ------------------------------------------------------------------------------------------------------------------
// Firing

bool FDTWeaponSim::FireVolley(const FDTSimContext& Context, FDTBulletPool* Bullets, FDTPlayerRunState& Player, int32 Slot,
	FWeaponRuntime& Runtime, uint16 SourceOverride, bool bFree)
{
	using namespace DTWeaponSimLocal;
	const FDTWeaponSpec& Spec = Runtime.Spec;
	FDTWorldView& View = *Context.View;
	const int32 Owner = Player.PlayerIndex;
	FDTWeaponInstance& Instance = Player.Weapons[Slot];
	const FVector2f Hero = HeroOf(View, Owner);
	const bool bManual = Players.IsValidIndex(Owner) && Players[Owner].bAimManual;
	const FVector2f AimDir = SafeDir(View.HeroAimDirs.IsValidIndex(Owner) ? View.HeroAimDirs[Owner] : FVector2f(1.0f, 0.0f), FVector2f(1.0f, 0.0f));

	if (Spec.Pattern == EDTWeaponPattern::Orbiter)
	{
		StepDrones(Context, Bullets, Player, Slot, Runtime, true);
		return true;
	}

	// Targeting (R-WPN-12): a targeted weapon with nothing in range does not fire.
	FVector2f Dir = FVector2f(1.0f, 0.0f);
	TArray<FVector2f> MineCells;
	TArray<int32> Seekers;
	int32 Target = INDEX_NONE;
	switch (Spec.Pattern)
	{
	case EDTWeaponPattern::Aimed:
	case EDTWeaponPattern::Cone:
	case EDTWeaponPattern::MeleeArc:
		if (bVolleyForced)
		{
			Dir = VolleyDir; // a dash volley fires along the dash (keepers.md R-KEE-34)
		}
		else if (Spec.Target == EDTWeaponTarget::Nearest && bManual)
		{
			Dir = AimDir;
		}
		else if (FindNearest(View, Hero, Spec.Range, 0, Target))
		{
			Dir = SafeDir(View.EnemyPositions[Target] - Hero, AimDir);
		}
		else
		{
			return false;
		}
		break;
	case EDTWeaponPattern::Radial:
	case EDTWeaponPattern::Spiral:
		Dir = FindNearest(View, Hero, Spec.Range, 0, Target) ? SafeDir(View.EnemyPositions[Target] - Hero, AimDir) : AimDir; // only a cleave reads it
		break;
	case EDTWeaponPattern::Line:
		if (bVolleyForced)
		{
			Dir = VolleyDir;
		}
		else if (!FindDensestLine(View, Hero, Spec.Range, Dir))
		{
			return false;
		}
		break;
	case EDTWeaponPattern::Homing:
		EnemiesInRange(View, Hero, Spec.Range, Seekers);
		if (Seekers.Num() == 0)
		{
			return false;
		}
		break;
	case EDTWeaponPattern::Lob:
	case EDTWeaponPattern::Mine:
		FindDensestCells(View, Hero, Spec.Range, Spec.Count + Spec.ForkRounds, MineCells);
		if (const FDTResolvedFlag* Seek = Spec.FindFlag(DTCat::EFlag::Seek))
		{
			// Fallback: shells and mines take the Marked enemy first (keepers.md R-KEE-26).
			const uint32 Marked = Seek->IsFallback() ? NearestMarked(Owner, Hero, Spec.Range, View) : 0;
			if (Marked != 0)
			{
				MineCells.Insert(View.EnemyPositions[IndexOfSerialIn(View, Marked)], 0);
			}
		}
		if (MineCells.Num() == 0)
		{
			return false;
		}
		break;
	default:
		return false;
	}

	FShot Shot = MakeShot(Owner, Slot, Runtime, SourceOverride != 0 ? SourceOverride : Instance.SourceId, Hero);
	ApplyMotion(Spec, Shot.Life, Shot.Motion);
	Shot.Damage *= bVolleyForced ? VolleyMult : 1.0f;
	if (Spec.bSeekMarked)
	{
		Shot.Motion.TargetSerial = NearestMarked(Owner, Hero, Spec.HomingSeekUu, View);
	}
	// Reshaping by a Keeper flag carries Keeper.<Path>; by a modifier or the weapon itself, the weapon's source (R-WPN-07, D-015).
	auto ReshapeSource = [](uint16 FlagSource, uint16 Current)
	{
		for (int32 Path = 0; Path < static_cast<int32>(EDTPath::Count); ++Path)
		{
			if (FlagSource != 0 && FlagSource == DTWeapons::KeeperSource(static_cast<EDTPath>(Path)))
			{
				return FlagSource;
			}
		}
		return Current;
	};
	if (!bFree)
	{
		ChargeShot(Spec, Instance.ShotCounter + 1, Shot);
	}
	// Stillness: a charge reshapes the weapon's own next volley.
	if (!bFree && Runtime.StillCharges > 0)
	{
		if (const FDTResolvedFlag* Still = Spec.FindFlag(DTCat::EFlag::Stillness))
		{
			SpendCharge(Context, Owner, Runtime, *Still, Shot);
			Shot.SourceId = ReshapeSource(Still->Entry.SourceId, Shot.SourceId);
		}
	}

	bool bMelee = false;
	int32 Converted = 0;
	if (Spec.Conversion == DTCat::EFlag::ConvertMelee && Spec.Pattern != EDTWeaponPattern::Mine && Spec.Pattern != EDTWeaponPattern::MeleeArc)
	{
		// R-WPN-10: the converted rounds fold into ONE arc of their summed damage x the conversion ratio.
		Converted = Spec.ConvertRounds;
		FShot Cleave = Shot;
		if (const FDTResolvedFlag* Conversion = Spec.FindFlag(DTCat::EFlag::ConvertMelee))
		{
			Cleave.SourceId = ReshapeSource(Conversion->Entry.SourceId, Cleave.SourceId); // a Keeper cleave is the Keeper's
		}
		Cleave.Damage = Shot.Damage * static_cast<float>(Converted) * Spec.ConvertDamageFactor;
		EmitArc(Context, Runtime, Cleave, Dir, Spec.ConvertArcDeg, Spec.ConvertReach, true);
		if (Spec.MirrorSource != 0)
		{
			FShot Copy = Cleave;
			Copy.SourceId = Spec.MirrorSource;
			Copy.Damage = Cleave.Damage * Spec.MirrorDamageRatio;
			EmitArc(Context, Runtime, Copy, -Dir, Spec.ConvertArcDeg, Spec.ConvertReach, true);
		}
		bMelee = true;
	}

	const auto EmitForks = [&](int32 Remaining)
	{
		if (Spec.ForkSource == 0 || Remaining <= 0)
		{
			return;
		}
		FShot Fork = Shot;
		Fork.SourceId = Spec.ForkSource;
		for (int32 Extra = 1; Extra <= Spec.ForkRounds; ++Extra)
		{
			const float Sign = (Extra % 2 == 1) ? 1.0f : -1.0f;
			const float Angle = Sign * Spec.ForkAngleDeg * static_cast<float>((Extra + 1) / 2);
			SpawnRound(Context, Bullets, Runtime, Fork, Hero, Rotate(Dir, Angle));
		}
	};

	switch (Spec.Pattern)
	{
	case EDTWeaponPattern::Aimed:
	case EDTWeaponPattern::Line:
	{
		const int32 Remaining = Spec.Count - Converted;
		const FVector2f Side(-Dir.Y, Dir.X);
		for (int32 Index = 0; Index < Remaining; ++Index)
		{
			// Parallel lanes one round-diameter apart, centred on the aim line.
			const float Offset = (static_cast<float>(Index) - 0.5f * static_cast<float>(Remaining - 1)) * 2.0f * Spec.Radius;
			SpawnRound(Context, Bullets, Runtime, Shot, Hero + Side * Offset, Dir);
			if (Spec.MirrorSource != 0)
			{
				FShot Copy = Shot;
				Copy.SourceId = Spec.MirrorSource;
				Copy.Damage = Shot.Damage * Spec.MirrorDamageRatio;
				SpawnRound(Context, Bullets, Runtime, Copy, Hero + Side * Offset, -Dir);
			}
		}
		EmitForks(Remaining);
		break;
	}
	case EDTWeaponPattern::Cone:
	{
		// Pellets evenly across the cone (R-WPN-14): spread is geometry, never a roll.
		const int32 Remaining = Spec.Count - Converted;
		const float Cone = static_cast<float>(DTConst::WEAPON_CONE_DEG);
		for (int32 Index = 0; Index < Remaining; ++Index)
		{
			const float Angle = Remaining > 1 ? -0.5f * Cone + Cone * static_cast<float>(Index) / static_cast<float>(Remaining - 1) : 0.0f;
			SpawnRound(Context, Bullets, Runtime, Shot, Hero, Rotate(Dir, Angle));
			if (Spec.MirrorSource != 0)
			{
				FShot Copy = Shot;
				Copy.SourceId = Spec.MirrorSource;
				Copy.Damage = Shot.Damage * Spec.MirrorDamageRatio;
				SpawnRound(Context, Bullets, Runtime, Copy, Hero, Rotate(-Dir, Angle));
			}
		}
		EmitForks(Remaining);
		break;
	}
	case EDTWeaponPattern::Homing:
	{
		// Each missile draws its own target from the living in range (R-WPN-17) and leaves on a fan around it.
		FRandomStream& Rng = Context.Run->GetRng().Get(EDTRngStream::Combat);
		const int32 Regular = Spec.Count - Converted;
		const int32 Missiles = Regular + (Spec.ForkSource != 0 && Regular > 0 ? Spec.ForkRounds : 0);
		const float Cone = static_cast<float>(DTConst::WEAPON_CONE_DEG);
		for (int32 Index = 0; Index < Missiles; ++Index)
		{
			const int32 Enemy = Seekers[Rng.RandRange(0, Seekers.Num() - 1)];
			FShot Missile = Shot;
			Missile.SourceId = Index < Regular ? Shot.SourceId : Spec.ForkSource;
			Missile.Motion.TargetSerial = View.EnemySerials[Enemy];
			const FVector2f ToEnemy = SafeDir(View.EnemyPositions[Enemy] - Hero, AimDir);
			const float Angle = Missiles > 1 ? -0.5f * Cone + Cone * static_cast<float>(Index) / static_cast<float>(Missiles - 1) : 0.0f;
			SpawnRound(Context, Bullets, Runtime, Missile, Hero, Rotate(ToEnemy, Angle));
		}
		break;
	}
	case EDTWeaponPattern::Spiral:
	{
		// Count arms; the stream turns on by the spin times the time since the last round (R-WPN-18).
		Runtime.SpiralDeg = FMath::Fmod(Runtime.SpiralDeg + static_cast<float>(DTConst::WEAPON_SPIRAL_DEG_S) * Spec.Cooldown, 360.0f);
		const FVector2f Base(1.0f, 0.0f);
		for (int32 Index = Converted; Index < Spec.Count; ++Index)
		{
			const float Angle = Runtime.SpiralDeg + 360.0f * static_cast<float>(Index) / static_cast<float>(FMath::Max(1, Spec.Count));
			SpawnRound(Context, Bullets, Runtime, Shot, Hero, Rotate(Base, Angle));
		}
		break;
	}
	case EDTWeaponPattern::Lob:
	{
		// Shells take the densest cells, and repeat them when the crowd is thin (R-WPN-16).
		const int32 Remaining = Spec.Count - Converted;
		for (int32 Index = 0; Index < Remaining; ++Index)
		{
			const FVector2f Cell = MineCells[Index % MineCells.Num()];
			LaunchShell(Runtime, Shot, Cell);
			if (Spec.MirrorSource != 0)
			{
				FShot Copy = Shot;
				Copy.SourceId = Spec.MirrorSource;
				Copy.Damage = Shot.Damage * Spec.MirrorDamageRatio;
				LaunchShell(Runtime, Copy, Hero * 2.0f - Cell);
			}
		}
		if (Spec.ForkSource != 0 && Remaining > 0)
		{
			FShot Fork = Shot;
			Fork.SourceId = Spec.ForkSource;
			for (int32 Extra = 1; Extra <= Spec.ForkRounds; ++Extra)
			{
				const float Sign = (Extra % 2 == 1) ? 1.0f : -1.0f;
				LaunchShell(Runtime, Fork, Hero + Rotate(MineCells[0] - Hero, Sign * Spec.ForkAngleDeg * static_cast<float>((Extra + 1) / 2)));
			}
		}
		break;
	}
	case EDTWeaponPattern::Radial:
	{
		const FVector2f Base(1.0f, 0.0f);
		for (int32 Index = Converted; Index < Spec.Count; ++Index)
		{
			const float Angle = 360.0f * static_cast<float>(Index) / static_cast<float>(FMath::Max(1, Spec.Count));
			SpawnRound(Context, Bullets, Runtime, Shot, Hero, Rotate(Base, Angle));
		}
		break;
	}
	case EDTWeaponPattern::MeleeArc:
	{
		for (int32 Sweep = 0; Sweep < Spec.Count; ++Sweep)
		{
			// Every second sweep faces the opposite way (R-WPN-06).
			const FVector2f SweepDir = (Sweep % 2 == 0) ? Dir : -Dir;
			EmitArc(Context, Runtime, Shot, SweepDir, Spec.ArcDeg, Spec.Range, false);
			if (Spec.MirrorSource != 0)
			{
				FShot Copy = Shot;
				Copy.SourceId = Spec.MirrorSource;
				Copy.Damage = Shot.Damage * Spec.MirrorDamageRatio;
				EmitArc(Context, Runtime, Copy, -SweepDir, Spec.ArcDeg, Spec.Range, false);
			}
		}
		bMelee = true;
		break;
	}
	case EDTWeaponPattern::Mine:
	{
		for (int32 Index = 0; Index < Spec.Count && Index < MineCells.Num(); ++Index)
		{
			PlaceMine(Context, Runtime, Owner, Slot, MineCells[Index], Shot.Damage, Shot.SourceId, Shot.bCharged);
		}
		for (int32 Extra = 0; Spec.ForkSource != 0 && Extra < Spec.ForkRounds; ++Extra)
		{
			// Extra mines take the next densest cells, or repeat the densest when the crowd is thin.
			const int32 Cell = Spec.Count + Extra < MineCells.Num() ? Spec.Count + Extra : Extra % MineCells.Num();
			PlaceMine(Context, Runtime, Owner, Slot, MineCells[Cell], Shot.Damage, Spec.ForkSource, Shot.bCharged);
		}
		break;
	}
	default:
		break;
	}

	if (!bFree)
	{
		++Instance.ShotCounter;
		OnActivation(Context, Bullets, Owner, Slot, Runtime, bMelee);
	}
	return true;
}

void FDTWeaponSim::StepDrones(const FDTSimContext& Context, FDTBulletPool* Bullets, FDTPlayerRunState& Player, int32 Slot,
	FWeaponRuntime& Runtime, bool bFreeVolley)
{
	using namespace DTWeaponSimLocal;
	const FDTWeaponSpec& Spec = Runtime.Spec;
	FDTWorldView& View = *Context.View;
	const int32 Owner = Player.PlayerIndex;
	FDTWeaponInstance& Instance = Player.Weapons[Slot];
	const FVector2f Hero = HeroOf(View, Owner);
	int32 Drones = FMath::Max(1, Spec.Count);
	if (const FDTResolvedFlag* Orbit = Spec.FindFlag(DTCat::EFlag::ConvertOrbit))
	{
		Drones += Orbit->IsFallback() && Orbit->Resolution.Cap == DTCat::ECap::Orbit ? 1 : 0; // "+1 drone"
	}
	if (!bFreeVolley)
	{
		Runtime.DroneAngleDeg = FMath::Fmod(Runtime.DroneAngleDeg + static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_DEG_S) * Context.StepSeconds, 360.0f);
		Runtime.DroneTimer -= Context.StepSeconds;
	}
	auto DronePos = [&](int32 Drone)
	{
		const float Angle = Runtime.DroneAngleDeg + 360.0f * static_cast<float>(Drone) / static_cast<float>(Drones);
		return Hero + Rotate(FVector2f(static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_RADIUS_UU), 0.0f), Angle);
	};
	auto FireBolt = [&](int32 Drone, int32 Enemy, uint16 Source, bool bCharge)
	{
		const FVector2f From = DronePos(Drone);
		const FVector2f Dir = SafeDir(View.EnemyPositions[Enemy] - From, FVector2f(1.0f, 0.0f));
		FShot Shot = MakeShot(Owner, Slot, Runtime, Source, From);
		if (bCharge)
		{
			ChargeShot(Spec, Instance.ShotCounter + 1, Shot);
		}
		if (bCharge && Runtime.StillCharges > 0)
		{
			if (const FDTResolvedFlag* Still = Spec.FindFlag(DTCat::EFlag::Stillness))
			{
				SpendCharge(Context, Owner, Runtime, *Still, Shot);
			}
		}
		SpawnRound(Context, Bullets, Runtime, Shot, From, Dir);
		if (Spec.ForkSource != 0)
		{
			FShot Fork = Shot;
			Fork.SourceId = Spec.ForkSource;
			for (int32 Extra = 1; Extra <= Spec.ForkRounds; ++Extra)
			{
				const float Sign = (Extra % 2 == 1) ? 1.0f : -1.0f;
				SpawnRound(Context, Bullets, Runtime, Fork, From, Rotate(Dir, Sign * Spec.ForkAngleDeg * static_cast<float>((Extra + 1) / 2)));
			}
		}
	};

	if (bFreeVolley)
	{
		// A free volley (Mod.ChainTrigger): every drone fires once at its nearest enemy.
		const uint16 Source = Spec.ChainTriggerSource != 0 ? Spec.ChainTriggerSource : Instance.SourceId;
		for (int32 Drone = 0; Drone < Drones; ++Drone)
		{
			int32 Enemy = INDEX_NONE;
			if (FindNearest(View, DronePos(Drone), Spec.Range, 0, Enemy))
			{
				FireBolt(Drone, Enemy, Source, false);
			}
		}
		return;
	}
	if (Runtime.DroneTimer > 0.0f)
	{
		return;
	}
	// One drone at a time, every cooldown / count, at the enemy nearest that drone (R-WPN-06).
	for (int32 Attempt = 0; Attempt < Drones; ++Attempt)
	{
		const int32 Drone = (Runtime.NextDrone + Attempt) % Drones;
		int32 Enemy = INDEX_NONE;
		const FDTResolvedFlag* Seek = Spec.FindFlag(DTCat::EFlag::Seek);
		const uint32 Marked = Seek ? NearestMarked(Owner, DronePos(Drone), Spec.Range, View) : 0;
		if (Marked != 0 || FindNearest(View, DronePos(Drone), Spec.Range, 0, Enemy))
		{
			Enemy = Marked != 0 ? IndexOfSerialIn(View, Marked) : Enemy;
			FireBolt(Drone, Enemy, Instance.SourceId, true);
			Runtime.NextDrone = (Drone + 1) % Drones;
			Runtime.DroneTimer = FMath::Max(Runtime.DroneTimer, 0.0f) + Spec.Cooldown / static_cast<float>(Drones);
			++Instance.ShotCounter;
			OnActivation(Context, Bullets, Owner, Slot, Runtime, false);
			return;
		}
	}
	Runtime.DroneTimer = 0.0f; // nothing in range: stay ready (R-WPN-12)
}

void FDTWeaponSim::PlaceMine(const FDTSimContext& Context, FWeaponRuntime& Runtime, int32 OwnerId, int32 Slot, const FVector2f& At,
	float Damage, uint16 SourceId, bool bCharged)
{
	if (!TryEmit(Runtime))
	{
		return;
	}
	if (Runtime.Mines.Num() >= DTConst::WEAPON_MINE_LIVE_MAX)
	{
		// One more than the live cap detonates the oldest at once (R-WPN-13).
		const FMine Oldest = Runtime.Mines[0];
		Runtime.Mines.RemoveAt(0);
		Detonate(Context, OwnerId, Slot, Runtime, Oldest);
	}
	FMine& Mine = Runtime.Mines.AddDefaulted_GetRef();
	Mine.Position = At;
	Mine.Origin = At;
	Mine.Damage = Damage;
	Mine.SourceId = SourceId;
	Mine.bCharged = bCharged;
}

void FDTWeaponSim::StepMines(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime)
{
	FDTWorldView& View = *Context.View;
	const float Arm = DTConst::WEAPON_MINE_ARM_S;
	const float Tick = DTConst::WEAPON_MINE_PULL_TICK_S;
	const float PullRadius = Runtime.Spec.AreaRadius * DTConst::WEAPON_MINE_PULL_RADIUS_RATIO;
	const float PullStep = static_cast<float>(DTConst::WEAPON_MINE_PULL_SPEED_UU_S) * Tick;
	int32 Index = 0;
	while (Index < Runtime.Mines.Num())
	{
		FMine& Mine = Runtime.Mines[Index];
		Mine.Age += Context.StepSeconds;
		while (Mine.TicksDone < DTConst::WEAPON_MINE_PULL_TICKS &&
			Mine.Age + KINDA_SMALL_NUMBER >= Arm + Tick * static_cast<float>(Mine.TicksDone + 1))
		{
			// A pull tick: displacement requests toward the mine (never a position write) and a small hit.
			const int32 Num = DTWeaponSimLocal::NumEnemies(View);
			TArray<int32> Local;
			if (Num > 0 && View.EnemyHash.Num() > 0)
			{
				View.EnemyHash.QueryCircle(Mine.Position, PullRadius, Local);
			}
			for (const int32 Enemy : Local)
			{
				if (Enemy < 0 || Enemy >= Num)
				{
					continue;
				}
				const FVector2f ToMine = Mine.Position - View.EnemyPositions[Enemy];
				const float Dist = ToMine.Size();
				if (Dist > PullRadius)
				{
					continue;
				}
				if (Dist > KINDA_SMALL_NUMBER)
				{
					FDTDisplacement& Move = View.Displacements.AddDefaulted_GetRef();
					Move.EnemyIndex = Enemy;
					Move.EnemySerial = View.EnemySerials[Enemy];
					Move.Delta = ToMine / Dist * FMath::Min(PullStep, Dist);
					Move.OwnerId = OwnerId;
				}
				float Damage = Mine.Damage * DTConst::WEAPON_MINE_PULL_TICK_DAMAGE_RATIO;
				const bool bCrit = RollCrit(Context, Runtime.Spec, Damage);
				FDTHit& Hit = View.Hits.AddDefaulted_GetRef();
				Hit.EnemyIndex = Enemy;
				Hit.EnemySerial = View.EnemySerials[Enemy];
				Hit.OwnerId = OwnerId;
				Hit.Damage = Damage;
				Hit.Position = View.EnemyPositions[Enemy];
				Hit.SourceId = Mine.SourceId;
				Hit.WeaponSlot = static_cast<int8>(Slot);
				Hit.bCrit = bCrit;
			}
			++Mine.TicksDone;
		}
		if (Mine.TicksDone >= DTConst::WEAPON_MINE_PULL_TICKS)
		{
			const FMine Done = Mine;
			Runtime.Mines.RemoveAt(Index);
			Detonate(Context, OwnerId, Slot, Runtime, Done);
			continue;
		}
		++Index;
	}
}

void FDTWeaponSim::LaunchShell(FWeaponRuntime& Runtime, const FShot& Shot, const FVector2f& Target)
{
	if (!TryEmit(Runtime))
	{
		return;
	}
	FShell& Shell = Runtime.Shells.AddDefaulted_GetRef();
	Shell.Origin = Shot.Origin;
	Shell.Target = Target;
	Shell.Flight = Shot.Speed > 0.0f ? FVector2f::Distance(Shot.Origin, Target) / Shot.Speed : 0.0f;
	Shell.Left = Shell.Flight;
	Shell.Damage = Shot.Damage;
	Shell.SourceId = Shot.SourceId;
	Shell.bCharged = Shot.bCharged;
}

void FDTWeaponSim::StepShells(const FDTSimContext& Context, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime)
{
	for (int32 Index = 0; Index < Runtime.Shells.Num();)
	{
		FShell& Shell = Runtime.Shells[Index];
		Shell.Left -= Context.StepSeconds;
		if (Shell.Left > 0.0f)
		{
			++Index;
			continue;
		}
		// A landing is a detonation: blast, reactions and sources run as a mine's do (R-WPN-16).
		FMine Blast;
		Blast.Position = Shell.Target;
		Blast.Origin = Shell.Origin;
		Blast.Damage = Shell.Damage;
		Blast.SourceId = Shell.SourceId;
		Blast.bCharged = Shell.bCharged;
		Runtime.Shells.RemoveAt(Index);
		Detonate(Context, OwnerId, Slot, Runtime, Blast);
	}
}

void FDTWeaponSim::Detonate(const FDTSimContext& Context, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime, const FMine& Mine)
{
	FDTWorldView& View = *Context.View;
	TArray<FDTEnemyHandle> Hit;
	HitCircle(Context, OwnerId, Slot, Mine.SourceId, Mine.Position, Runtime.Spec.AreaRadius, Mine.Damage, true, &Runtime.Spec, &Hit);
	for (int32 Index = 0; Index < Hit.Num(); ++Index)
	{
		const FDTHit& Out = View.Hits[View.Hits.Num() - Hit.Num() + Index];
		FImpact& Impact = Impacts.AddDefaulted_GetRef();
		Impact.Kind = EImpact::MineHit;
		Impact.OwnerId = OwnerId;
		Impact.Slot = static_cast<int8>(Slot);
		Impact.SourceId = Mine.SourceId;
		Impact.Position = Out.Position;
		Impact.Origin = Mine.Origin;
		Impact.Dir = DTWeaponSimLocal::SafeDir(Out.Position - Mine.Position, FVector2f(1.0f, 0.0f));
		Impact.BaseDamage = Mine.Damage;
		Impact.HitDamage = Out.Damage;
		Impact.EnemySerial = Out.EnemySerial;
		Impact.EnemyRadius = View.EnemyRadii[Out.EnemyIndex];
		Impact.EnemyPosition = Out.Position;
		Impact.StatusMask = StatusMaskOf(Out.EnemyIndex, Out.EnemySerial);
		Impact.bCharged = Mine.bCharged;
		++Runtime.Counters.Impacts;
	}
	FImpact& Blast = Impacts.AddDefaulted_GetRef();
	Blast.Kind = EImpact::Detonation;
	Blast.OwnerId = OwnerId;
	Blast.Slot = static_cast<int8>(Slot);
	Blast.SourceId = Mine.SourceId;
	Blast.Position = Mine.Position;
	Blast.Origin = Mine.Position;
	Blast.Area = Runtime.Spec.AreaRadius;
	Blast.BaseDamage = Mine.Damage;
	Blast.HitDamage = Mine.Damage;
	Blast.bCharged = Mine.bCharged;
}

// ------------------------------------------------------------------------------------------------------------------
// Activation-time expressions, stillness, slam

void FDTWeaponSim::ChargeShot(const FDTWeaponSpec& Spec, int32 ShotNumber, FShot& Shot) const
{
	const FDTResolvedFlag* Charge = Spec.FindFlag(DTCat::EFlag::Charge);
	if (!Charge)
	{
		return;
	}
	static const FName ParamPierceAdd(TEXT("pierce_add"));
	static const FName ParamChargedCrit(TEXT("charged_crit"));
	static const FName ParamAlwaysCrit(TEXT("always_crit"));
	const FDTFlagParams& Params = Charge->Entry.Params;
	const int32 Every = FMath::RoundToInt(Params.Get(DTFlagParam::EveryN));
	if (Every > 0 && ShotNumber % Every == 0)
	{
		Shot.Damage *= Params.Get(DTFlagParam::Mult, 1.0f);
		Shot.Pierce += FMath::RoundToInt(Params.Get(ParamPierceAdd));
		Shot.CritBonus = Params.Get(ParamChargedCrit);
		Shot.bForceCrit = Params.Get(ParamAlwaysCrit) > 0.0f;
		Shot.SourceId = Charge->Entry.SourceId; // reshaping by a Keeper flag is the Keeper's (R-WPN-07)
	}
}

int32 FDTWeaponSim::IndexOfSerialIn(const FDTWorldView& View, uint32 Serial) const
{
	return View.EnemySerials.Find(Serial);
}

void FDTWeaponSim::OnActivation(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot,
	FWeaponRuntime& Runtime, bool bMelee)
{
	using namespace DTWeaponSimLocal;
	if (!Context.Run->IsValidPlayer(OwnerId) || !Context.Run->GetPlayer(OwnerId).Weapons.IsValidIndex(Slot))
	{
		return;
	}
	const int32 Shots = Context.Run->GetPlayer(OwnerId).Weapons[Slot].ShotCounter;
	const FVector2f Hero = HeroOf(*Context.View, OwnerId);
	if (bMelee)
	{
		++Runtime.MeleeActivations;
	}
	for (const FDTResolvedFlag& Flag : Runtime.Spec.Flags)
	{
		const FDTFlagParams& Params = Flag.Entry.Params;
		const bool bOnVolley = Params.GetWord(DTFlagParam::On) == WordVolley;
		if (Flag.Entry.Flag == DTCat::EFlag::Split && (Flag.IsFallback() || bOnVolley))
		{
			// Fallback on Orbit / Melee: every Nth activation a ring of children from the hero (R-WPN-08). N is the
			// flag's every_n_volley, else the reciprocal of its chance (the same expected rate as a per-hit roll).
			const float Chance = Params.Has(DTFlagParam::Chance) ? Params.Get(DTFlagParam::Chance) : 1.0f;
			const int32 N = Params.Has(DTFlagParam::EveryNVolley) ? FMath::Max(1, FMath::RoundToInt(Params.Get(DTFlagParam::EveryNVolley)))
				: FMath::Max(1, FMath::RoundToInt(1.0f / FMath::Max(Chance, KINDA_SMALL_NUMBER)));
			if (Shots % N == 0)
			{
				FImpact Ring;
				Ring.OwnerId = OwnerId;
				Ring.Slot = static_cast<int8>(Slot);
				Ring.Position = Hero;
				Ring.BaseDamage = Runtime.Spec.Damage;
				EmitChildren(Context, Bullets, Runtime, Ring, Flag.Entry, true, Hero);
			}
		}
		else if (Flag.Entry.Flag == DTCat::EFlag::Slam && bMelee &&
			(Params.WordHas(DTFlagParam::Trigger, TEXT("EveryNMelee")) || Params.WordHas(DTFlagParam::Trigger, TEXT("EveryNCleave"))))
		{
			const int32 N = FMath::Max(1, FMath::RoundToInt(Params.Get(DTFlagParam::N, 1.0f)));
			if (Runtime.MeleeActivations % N == 0)
			{
				EmitSlam(Context, Bullets, OwnerId, Slot, Runtime, Flag);
			}
		}
	}
}

void FDTWeaponSim::StepStillness(const FDTSimContext& Context, int32 OwnerId, FPlayerRuntime& Player)
{
	const FDTResolvedFlag* Still = nullptr;
	for (const FWeaponRuntime& Weapon : Player.Weapons)
	{
		if ((Still = Weapon.Spec.FindFlag(DTCat::EFlag::Stillness)) != nullptr)
		{
			break;
		}
	}
	if (!Still)
	{
		Player.bHasAnchor = false;
		Player.StillTime = 0.0f;
		return;
	}
	const FDTWorldView& View = *Context.View;
	const FVector2f Pos = DTWeaponSimLocal::HeroOf(View, OwnerId);
	const bool bDashing = View.HeroIntangible.IsValidIndex(OwnerId) && View.HeroIntangible[OwnerId];
	if (!Player.bHasAnchor || bDashing)
	{
		// A dash keeps the charge but is not standing still (keepers.md: dashing keeps the charge).
		Player.StillAnchor = Pos;
		Player.bHasAnchor = true;
		Player.StillTime = 0.0f;
		return;
	}
	const FDTFlagParams& Params = Still->Entry.Params;
	if (FVector2f::Distance(Pos, Player.StillAnchor) > Params.Get(DTFlagParam::StillMoveUu))
	{
		// Walking spends the charge.
		for (FWeaponRuntime& Weapon : Player.Weapons)
		{
			Weapon.StillCharges = 0;
		}
		Player.StillAnchor = Pos;
		Player.StillTime = 0.0f;
		return;
	}
	Player.StillTime += Context.StepSeconds;
	const float ChargeS = Params.Get(DTFlagParam::ChargeS);
	if (ChargeS > 0.0f && Player.StillTime + KINDA_SMALL_NUMBER >= ChargeS)
	{
		Player.StillTime = 0.0f;
		const int32 Max = FMath::Max(1, FMath::RoundToInt(Params.Get(DTFlagParam::ChargesMax, 1.0f)));
		for (FWeaponRuntime& Weapon : Player.Weapons)
		{
			if (Weapon.Spec.HasFlag(DTCat::EFlag::Stillness))
			{
				Weapon.StillCharges = FMath::Min(Weapon.StillCharges + 1, Max);
			}
		}
	}
}

void FDTWeaponSim::SpendCharge(const FDTSimContext& Context, int32 OwnerId, FWeaponRuntime& Runtime, const FDTResolvedFlag& Still, FShot& Shot)
{
	const FDTFlagParams& Params = Still.Entry.Params;
	Shot.Damage *= Params.Get(DTFlagParam::Mult, 1.0f);
	Shot.Pierce += FMath::RoundToInt(Params.Get(DTFlagParam::ChargedPierce));
	Shot.bCharged = true;
	Shot.bTrack = true; // the impact must know it was charged
	--Runtime.StillCharges;
	const float Ring = Params.Get(DTFlagParam::ChargedFreezeRingUu);
	if (Ring <= 0.0f)
	{
		return;
	}
	const FDTWorldView& View = *Context.View;
	TArray<int32> Local;
	EnemiesInCircle(View, DTWeaponSimLocal::HeroOf(View, OwnerId), Ring, Local);
	for (const int32 Enemy : Local)
	{
		FPendingStatus& Pending = PendingStatuses.AddDefaulted_GetRef();
		Pending.EnemySerial = View.EnemySerials[Enemy];
		Pending.OwnerId = OwnerId;
		Pending.SourceId = Still.Entry.SourceId;
		Pending.Params.SetWord(DTFlagParam::Status, DTWeaponSimLocal::WordFreeze);
	}
}

void FDTWeaponSim::StepSlamTriggers(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, FPlayerRuntime& Player)
{
	const FDTWorldView& View = *Context.View;
	const bool bIntangible = View.HeroIntangible.IsValidIndex(OwnerId) && View.HeroIntangible[OwnerId];
	const bool bDashLanding = Player.bWasIntangible && !bIntangible;
	Player.bWasIntangible = bIntangible;
	const bool bHitTaken = View.HeroHurt.IsValidIndex(OwnerId) && View.HeroHurt[OwnerId];
	Player.bSlamBoosted |= bHitTaken;
	if (!bDashLanding && !bHitTaken)
	{
		return;
	}
	// Hero-triggered slams happen once per player, from the highest-load weapon that carries Slam.
	int32 BestSlot = INDEX_NONE;
	for (int32 Slot = 0; Slot < Player.Weapons.Num(); ++Slot)
	{
		const FDTResolvedFlag* Slam = Player.Weapons[Slot].Spec.FindFlag(DTCat::EFlag::Slam);
		if (!Slam)
		{
			continue;
		}
		const FDTFlagParams& Params = Slam->Entry.Params;
		const bool bFires = (bDashLanding && Params.WordHas(DTFlagParam::Trigger, TEXT("DashLanding"))) ||
			(bHitTaken && Params.WordHas(DTFlagParam::Trigger, TEXT("HitTaken")));
		if (bFires && (BestSlot == INDEX_NONE || Player.Weapons[Slot].Spec.LoadRank < Player.Weapons[BestSlot].Spec.LoadRank))
		{
			BestSlot = Slot;
		}
	}
	if (BestSlot != INDEX_NONE)
	{
		FWeaponRuntime& Runtime = Player.Weapons[BestSlot];
		EmitSlam(Context, Bullets, OwnerId, BestSlot, Runtime, *Runtime.Spec.FindFlag(DTCat::EFlag::Slam));
	}
}

void FDTWeaponSim::EmitSlam(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime, const FDTResolvedFlag& Slam)
{
	if (!TryEmit(Runtime))
	{
		return;
	}
	const FDTFlagParams& Params = Slam.Entry.Params;
	const FVector2f Hero = DTWeaponSimLocal::HeroOf(*Context.View, OwnerId);
	const float Radius = Params.Get(DTFlagParam::RadiusUu) * (1.0f + Params.Get(DTFlagParam::RadiusScale));
	float Damage = Runtime.Spec.Damage * Params.Get(DTFlagParam::DmgRatio, 1.0f);
	RollCrit(Context, Runtime.Spec, Damage);
	// after_hit_taken_mult: the first slam after a hit, which is usually the retaliation that hit set off.
	FPlayerRuntime& Player = Players[OwnerId];
	if (Player.bSlamBoosted && Params.Has(DTFlagParam::AfterHitTakenMult))
	{
		Damage *= Params.Get(DTFlagParam::AfterHitTakenMult);
		Player.bSlamBoosted = false;
	}
	TArray<FDTEnemyHandle> Hit;
	HitCircle(Context, OwnerId, Slot, Slam.Entry.SourceId, Hero, Radius, Damage, false, nullptr, &Hit);
	// detonate_burning: every Burning enemy in the ring takes what its Burn still owes, now, as the Keeper. The Burn runs on
	// (enemies.md owns its state and weapons never write it), so the detonation is that damage once more, ahead of time.
	if (Params.Get(DTFlagParam::DetonateBurning) > 0.0f && Enemies)
	{
		FDTWorldView& View = *Context.View;
		for (const FDTEnemyHandle& Handle : Hit)
		{
			if (!Enemies->IsHandleValid(Handle))
			{
				continue;
			}
			float Owed = 0.0f;
			for (const FDTBurnStack& Stack : Enemies->GetAux(Handle.Index).Burn)
			{
				Owed += Stack.Left > 0.0f ? Stack.Left * Stack.Dps : 0.0f;
			}
			if (Owed > 0.0f)
			{
				FDTHit& Out = View.Hits.AddDefaulted_GetRef();
				Out.EnemyIndex = Handle.Index;
				Out.EnemySerial = Handle.Serial;
				Out.OwnerId = OwnerId;
				Out.Damage = Owed;
				Out.Position = View.EnemyPositions[Handle.Index];
				Out.SourceId = Slam.Entry.SourceId;
			}
		}
	}
	// embers: a ring of the shared child kind; each applies ember_status on its hit (ReactToImpact).
	const int32 Embers = FMath::RoundToInt(Params.Get(DTFlagParam::Embers));
	if (Embers > 0)
	{
		FShot Ember = MakeShot(OwnerId, Slot, Runtime, Slam.Entry.SourceId, Hero);
		Ember.Damage = Damage * Params.Get(DTFlagParam::EmberDmg);
		Ember.Pierce = 1;
		Ember.bTrack = true;
		Ember.bEmber = true;
		Ember.Generation = 1;
		Ember.Bounces = 0;
		Ember.Speed = static_cast<float>(DTConst::WEAPON_CHILD_SPEED_UU_S);
		Ember.Radius = static_cast<float>(DTConst::WEAPON_CHILD_RADIUS_UU);
		Ember.Life = static_cast<float>(DTConst::WEAPON_CHILD_RANGE_UU) / Ember.Speed;
		for (int32 Index = 0; Index < Embers; ++Index)
		{
			const FVector2f Dir = DTWeaponSimLocal::Rotate(FVector2f(1.0f, 0.0f), 360.0f * static_cast<float>(Index) / static_cast<float>(Embers));
			SpawnRound(Context, Bullets, Runtime, Ember, Hero, Dir);
		}
	}
	if (Params.Has(DTFlagParam::BurningExplodeOnDeathUu))
	{
		for (const FDTEnemyHandle& Handle : Hit)
		{
			StepSlammed.Add(Handle.Serial);
		}
	}
	if (!Params.GetWord(DTFlagParam::Status).IsNone())
	{
		for (const FDTEnemyHandle& Handle : Hit)
		{
			FPendingStatus& Pending = PendingStatuses.AddDefaulted_GetRef();
			Pending.EnemySerial = Handle.Serial;
			Pending.HitDamage = Damage;
			Pending.OwnerId = OwnerId;
			Pending.SourceId = Slam.Entry.SourceId;
			Pending.Params.SetWord(DTFlagParam::Status, Params.GetWord(DTFlagParam::Status));
		}
	}
	if (Params.GetWord(DTFlagParam::Leave) == DTWeaponSimLocal::WordField)
	{
		for (const FDTResolvedFlag& Field : Runtime.Spec.Flags)
		{
			if (Field.Entry.Flag == DTCat::EFlag::Field && Field.Entry.SourceId == Slam.Entry.SourceId)
			{
				SpawnField(OwnerId, Slot, Field.Entry.SourceId, Hero, Field.Entry.Params, Damage);
				break;
			}
		}
	}
	FArcFlash& Flash = Arcs.AddDefaulted_GetRef();
	Flash.OwnerId = OwnerId;
	Flash.Position = Hero;
	Flash.Reach = Radius;
	Flash.ArcDeg = 360.0f;
	Flash.TimeLeft = 0.5f * Runtime.Spec.Cooldown;
}

// ------------------------------------------------------------------------------------------------------------------
// Deferred strikes and zones

void FDTWeaponSim::SpawnField(int32 OwnerId, int32 Slot, uint16 SourceId, const FVector2f& At, const FDTFlagParams& Params, float BaseDamage)
{
	FWeaponRuntime* Runtime = FindRuntime(OwnerId, Slot);
	if (!Runtime)
	{
		return;
	}
	const int32 MaxLive = FMath::RoundToInt(Params.Get(DTFlagParam::MaxLive));
	if (MaxLive > 0)
	{
		int32 Live = 0;
		for (const FField& Field : Fields)
		{
			Live += (Field.OwnerId == OwnerId && Field.Slot == Slot && Field.SourceId == SourceId) ? 1 : 0;
		}
		if (Live >= MaxLive)
		{
			return;
		}
	}
	if (!TryEmit(*Runtime))
	{
		return;
	}
	FField& Field = Fields.AddDefaulted_GetRef();
	Field.OwnerId = OwnerId;
	Field.Slot = static_cast<int8>(Slot);
	Field.SourceId = SourceId;
	Field.Position = At;
	Field.Radius = Params.Get(DTFlagParam::RadiusUu) * (1.0f + Params.Get(DTFlagParam::RadiusScale));
	Field.TimeLeft = Params.Get(DTFlagParam::DurationS);
	Field.DamagePerSecond = Params.Get(DTFlagParam::Dps) * BaseDamage;
	Field.BaseDamage = BaseDamage;
	Field.Params = Params;
}

void FDTWeaponSim::StepDeferred(const FDTSimContext& Context)
{
	const float Dt = Context.StepSeconds;
	for (int32 Index = 0; Index < Echoes.Num();)
	{
		FEcho& Echo = Echoes[Index];
		Echo.Delay -= Dt;
		if (Echo.Delay > 0.0f)
		{
			++Index;
			continue;
		}
		if (Echo.bScreen)
		{
			HitCircle(Context, Echo.OwnerId, Echo.Slot, Echo.SourceId, DTWeaponSimLocal::HeroOf(*Context.View, Echo.OwnerId),
				static_cast<float>(DTConst::ARENA_CAMERA_FRAME_RADIUS_UU), Echo.Damage, false, nullptr);
		}
		else
		{
			HitCircle(Context, Echo.OwnerId, Echo.Slot, Echo.SourceId, Echo.Position, Echo.Radius, Echo.Damage, false, nullptr);
		}
		if (Echo.bLeaveField)
		{
			if (FWeaponRuntime* Runtime = FindRuntime(Echo.OwnerId, Echo.Slot))
			{
				for (const FDTResolvedFlag& Field : Runtime->Spec.Flags)
				{
					if (Field.Entry.Flag == DTCat::EFlag::Field && Field.Entry.SourceId == Echo.SourceId)
					{
						SpawnField(Echo.OwnerId, Echo.Slot, Echo.SourceId, Echo.Position, Field.Entry.Params, Echo.Damage);
						break;
					}
				}
			}
		}
		Echoes.RemoveAt(Index);
	}
	// R-KEE-22: statuses land on one grid, so overlapping fields of a source are one application per enemy per pulse.
	const int64 PulseSteps = FMath::Max<int64>(1, FMath::RoundToInt64(DTConst::STATUS_BURN_TICK_S / Dt));
	const bool bStatusPulse = Context.StepIndex % PulseSteps == 0;
	TSet<uint64> Pulsed;
	TArray<int32> InField;
	for (int32 Index = 0; Index < Fields.Num();)
	{
		FField& Field = Fields[Index];
		Field.TimeLeft -= Dt;
		if (Field.TimeLeft < 0.0f)
		{
			Fields.RemoveAt(Index);
			continue;
		}
		// A field ticks every step for its damage per second (a tick is a hit, never an impact).
		HitCircle(Context, Field.OwnerId, Field.Slot, Field.SourceId, Field.Position, Field.Radius, Field.DamagePerSecond * Dt, false, nullptr);
		if (bStatusPulse && !Field.Params.GetWord(DTFlagParam::Status).IsNone())
		{
			EnemiesInCircle(*Context.View, Field.Position, Field.Radius + MaxEnemyRadius, InField);
			for (const int32 Enemy : InField)
			{
				const float Reach = Field.Radius + Context.View->EnemyRadii[Enemy]; // the field's damage reach
				if (FVector2f::DistSquared(Field.Position, Context.View->EnemyPositions[Enemy]) > Reach * Reach)
				{
					continue;
				}
				const uint32 Serial = Context.View->EnemySerials[Enemy];
				bool bAlready = false;
				Pulsed.Add((static_cast<uint64>(Field.SourceId) << 32) | Serial, &bAlready);
				if (!bAlready)
				{
					ApplyStatus(Context, Field.Params, FDTEnemyHandle{ Enemy, Serial }, Field.BaseDamage, Field.OwnerId, Field.SourceId);
				}
			}
		}
		++Index;
	}
	for (int32 Index = 0; Index < Arcs.Num();)
	{
		Arcs[Index].TimeLeft -= Dt;
		if (Arcs[Index].TimeLeft <= 0.0f)
		{
			Arcs.RemoveAt(Index);
			continue;
		}
		++Index;
	}
}

// ------------------------------------------------------------------------------------------------------------------
// The step

void FDTWeaponSim::StepWeapons(const FDTSimContext& Context, FDTBulletPool* Bullets)
{
	if (!Context.Run || !Context.View)
	{
		return;
	}
	if (Context.StepIndex <= LastStep)
	{
		ResetTransient(); // the clock went back: a new run started
	}
	LastStep = Context.StepIndex;
	LastBullets = Bullets;
	FDTRunState& Run = *Context.Run;
	FDTWorldView& View = *Context.View;
	SyncPlayers(Run);
	MaxEnemyRadius = 0.0f;
	for (const float Radius : View.EnemyRadii)
	{
		MaxEnemyRadius = FMath::Max(MaxEnemyRadius, Radius);
	}
	const int32 WindowSteps = FMath::Max(1, FMath::RoundToInt(1.0f / FMath::Max(Context.StepSeconds, KINDA_SMALL_NUMBER)));

	StepDeferred(Context);

	for (int32 Owner = 0; Owner < Run.GetNumPlayers(); ++Owner)
	{
		FDTPlayerRunState& Player = Run.GetPlayer(Owner);
		FPlayerRuntime& PlayerRt = Players[Owner];
		const TArray<int32> Ranks = DTWeaponCompose::LoadRanks(Player.Weapons);
		const TArray<FDTFlagEntry> Flags = GetPlayerFlags(Owner);
		for (int32 Slot = 0; Slot < Player.Weapons.Num(); ++Slot)
		{
			FWeaponRuntime& Runtime = PlayerRt.Weapons[Slot];
			Runtime.Spec = DTWeaponCompose::Compose(Player.Weapons[Slot], Flags, PlayerRt.Stats, Ranks[Slot]);
			// Slide the one-second spawn window (R-WPN-09).
			if (Runtime.EmitRing.Num() != WindowSteps)
			{
				Runtime.EmitRing.Init(0, WindowSteps);
				Runtime.EmitCursor = 0;
				Runtime.EmitWindow = 0;
			}
			else
			{
				Runtime.EmitCursor = (Runtime.EmitCursor + 1) % WindowSteps;
				Runtime.EmitWindow -= Runtime.EmitRing[Runtime.EmitCursor];
				Runtime.EmitRing[Runtime.EmitCursor] = 0;
			}
		}
		StepKeeperHero(Context, Bullets, Owner, PlayerRt, Player);
		StepDuoCrit(PlayerRt, Owner);
		if (PlayerRt.PaydayMult > 0.0f)
		{
			FirePayday(Context, Bullets, Owner, PlayerRt);
		}
		StepStillness(Context, Owner, PlayerRt);
		StepSlamTriggers(Context, Bullets, Owner, PlayerRt);
		for (int32 Slot = 0; Slot < Player.Weapons.Num(); ++Slot)
		{
			FWeaponRuntime& Runtime = PlayerRt.Weapons[Slot];
			FDTWeaponInstance& Instance = Player.Weapons[Slot];
			Runtime.ChainTriggerReadyIn -= Context.StepSeconds;
			Runtime.RefitReadyIn -= Context.StepSeconds;
			StepMines(Context, Bullets, Owner, Slot, Runtime);
			StepShells(Context, Owner, Slot, Runtime);
			if (Runtime.Spec.Pattern == EDTWeaponPattern::Orbiter)
			{
				StepDrones(Context, Bullets, Player, Slot, Runtime, false);
				continue;
			}
			Instance.Cooldown -= Context.StepSeconds;
			if (Instance.Cooldown > 0.0f)
			{
				continue;
			}
			if (FireVolley(Context, Bullets, Player, Slot, Runtime, 0, false))
			{
				Instance.Cooldown = FMath::Max(Instance.Cooldown + Runtime.Spec.Cooldown, 0.0f);
			}
			else
			{
				Instance.Cooldown = 0.0f; // no target: keep the cooldown ready (R-WPN-12)
			}
		}
	}
	StepHunters(Context, Bullets);
	WeaponsHitsEnd = View.Hits.Num();

	// Published each step: a released orbiting round aims at the boss first (R-MOD-11).
	View.bBossAlive = false;
	if (Enemies)
	{
		const TConstArrayView<EDTEnemyRole> Roles = Enemies->GetRoles();
		const int32 Num = FMath::Min(DTWeaponSimLocal::NumEnemies(View), Roles.Num());
		for (int32 Enemy = 0; Enemy < Num && !View.bBossAlive; ++Enemy)
		{
			if (Roles[Enemy] == EDTEnemyRole::Boss)
			{
				View.bBossAlive = true;
				View.BossPosition = View.EnemyPositions[Enemy];
			}
		}
	}
}

void FDTWeaponSim::ObserveBullets(const FDTSimContext& Context)
{
	using namespace DTWeaponSimLocal;
	if (!Context.View)
	{
		return;
	}
	FDTWorldView& View = *Context.View;
	const float Dt = Context.StepSeconds;
	const float Half = static_cast<float>(DTConst::ARENA_HALF_EXTENT_UU);

	// 1. Mirror the pool's own float ops (FDTBulletPool::Step): life, move, expiry, bounds. A moving round is read off the
	// pool's report instead: steering depends on state the mirror does not hold (R-BUL-09).
	TMap<uint32, FDTRoundReport> Reports;
	if (LastBullets)
	{
		for (const FDTRoundReport& Report : LastBullets->GetRoundReports())
		{
			Reports.Add(Report.RoundId, Report);
		}
	}
	for (int32 Index = 0; Index < Rounds.Num();)
	{
		FRound& Round = Rounds[Index];
		if (Round.RoundId != 0)
		{
			const FDTRoundReport* Report = Reports.Find(Round.RoundId);
			if (!Report)
			{
				Rounds.RemoveAtSwap(Index); // recycled at the cap, or no pool in this test
				continue;
			}
			Round.Position = Report->Position;
			Round.Velocity = Report->Velocity;
			Round.LifeLeft = Report->LifeLeft;
			Round.PierceLeft = Report->PierceLeft;
			Round.bReturning = Report->bReturning;
		}
		else
		{
			Round.LifeLeft -= Dt;
			Round.Position += Round.Velocity * Dt;
		}
		if (Round.LifeLeft <= 0.0f)
		{
			FImpact& Expiry = Impacts.AddDefaulted_GetRef();
			Expiry.Kind = EImpact::Expiry;
			Expiry.OwnerId = Round.OwnerId;
			Expiry.Slot = Round.Slot;
			Expiry.SourceId = Round.SourceId;
			Expiry.Position = Round.Position;
			Expiry.Origin = Round.Origin;
			Expiry.Dir = SafeDir(Round.Velocity, FVector2f(1.0f, 0.0f));
			Expiry.BaseDamage = Round.BaseDamage;
			Expiry.HitDamage = Round.BaseDamage;
			Expiry.Area = Round.Radius;
			Expiry.Generation = Round.Generation;
			Rounds.RemoveAtSwap(Index);
			continue;
		}
		if (FMath::Abs(Round.Position.X) > Half || FMath::Abs(Round.Position.Y) > Half)
		{
			Rounds.RemoveAtSwap(Index);
			continue;
		}
		++Index;
	}

	// 2. Every bullet hit of this step maps back to its round by owner, slot, source and exact position.
	TMultiMap<uint32, int32> ByKey;
	auto KeyOf = [](int32 Owner, int8 Slot, uint16 Source, const FVector2f& P)
	{
		uint32 Hash = GetTypeHash(Owner);
		Hash = HashCombine(Hash, GetTypeHash(Slot));
		Hash = HashCombine(Hash, GetTypeHash(Source));
		Hash = HashCombine(Hash, GetTypeHash(P.X));
		return HashCombine(Hash, GetTypeHash(P.Y));
	};
	for (int32 Index = 0; Index < Rounds.Num(); ++Index)
	{
		ByKey.Add(KeyOf(Rounds[Index].OwnerId, Rounds[Index].Slot, Rounds[Index].SourceId, Rounds[Index].Position), Index);
	}
	TArray<int32> Spent;
	const int32 NumEnemies = DTWeaponSimLocal::NumEnemies(View);
	for (int32 HitIndex = FMath::Min(WeaponsHitsEnd, View.Hits.Num()); HitIndex < View.Hits.Num(); ++HitIndex)
	{
		const FDTHit& Hit = View.Hits[HitIndex];
		FWeaponRuntime* Runtime = FindRuntime(Hit.OwnerId, Hit.WeaponSlot);
		if (!Runtime)
		{
			continue; // stress fire, enemy-side or another system's hit
		}
		FImpact& Impact = Impacts.AddDefaulted_GetRef();
		Impact.Kind = EImpact::RoundHit;
		Impact.OwnerId = Hit.OwnerId;
		Impact.Slot = Hit.WeaponSlot;
		Impact.SourceId = Hit.SourceId;
		Impact.Position = Hit.Position;
		Impact.HitDamage = Hit.Damage;
		Impact.EnemySerial = Hit.EnemySerial;
		if (Hit.EnemyIndex >= 0 && Hit.EnemyIndex < NumEnemies)
		{
			Impact.EnemyPosition = View.EnemyPositions[Hit.EnemyIndex];
			Impact.EnemyRadius = View.EnemyRadii[Hit.EnemyIndex];
			Impact.StatusMask = StatusMaskOf(Hit.EnemyIndex, Hit.EnemySerial);
		}
		Impact.bCrit = Hit.bCrit;
		const FVector2f Hero = HeroOf(View, Hit.OwnerId);
		Impact.Origin = Hero;
		Impact.Dir = SafeDir(Hit.Position - Hero, FVector2f(1.0f, 0.0f));
		Impact.BaseDamage = Hit.bCrit && Runtime->Spec.CritMult > KINDA_SMALL_NUMBER ? Hit.Damage / Runtime->Spec.CritMult : Hit.Damage;
		Impact.Area = Runtime->Spec.Radius;
		Impact.bSpent = Hit.bPierce == false;
		TArray<int32> Matches;
		ByKey.MultiFind(KeyOf(Hit.OwnerId, Hit.WeaponSlot, Hit.SourceId, Hit.Position), Matches);
		for (const int32 Match : Matches)
		{
			FRound& Round = Rounds[Match];
			if (Round.OwnerId != Hit.OwnerId || Round.Slot != Hit.WeaponSlot || Round.SourceId != Hit.SourceId ||
				Round.Position != Hit.Position || Round.PierceLeft <= 0)
			{
				continue;
			}
			Impact.Origin = Round.Origin;
			Impact.Dir = SafeDir(Round.Velocity, Impact.Dir);
			Impact.BaseDamage = Round.BaseDamage;
			Impact.Area = Round.Radius;
			Impact.Generation = Round.Generation;
			Impact.BouncesLeft = Round.BouncesLeft;
			Impact.bCharged = Round.bCharged;
			Impact.bEmber = Round.bEmber;
			Impact.bLance = Round.bLance;
			Impact.DuoRow = Round.DuoRow;
			Impact.bSpent = --Round.PierceLeft <= 0;
			if (Impact.bSpent && Round.bBoomerang && !Round.bReturning)
			{
				// The pool turned it home instead of spending it.
				Impact.bSpent = false;
				Round.bReturning = true;
				Round.PierceLeft = Round.Pierce;
			}
			if (Impact.bSpent)
			{
				Spent.Add(Match);
			}
			break;
		}
		++Runtime->Counters.Impacts;
	}
	Spent.Sort([](int32 A, int32 B) { return A > B; });
	for (const int32 Index : Spent)
	{
		Rounds.RemoveAtSwap(Index);
	}
	EstimateOverkill(View);
	AccrueSun(View);
	CaptureDuoHits(View);
}

void FDTWeaponSim::EstimateOverkill(const FDTWorldView& View)
{
	bool bAny = false;
	for (const FPlayerRuntime& Player : Players)
	{
		for (const FWeaponRuntime& Weapon : Player.Weapons)
		{
			bAny |= Weapon.Spec.SpilloverSource != 0 || Weapon.Spec.HasFlag(DTCat::EFlag::Nova);
		}
	}
	if (!bAny || !Enemies)
	{
		return;
	}
	// ResolveHits owns health and keeps no overkill, so the step's hits are replayed against it here, in its own order. The
	// Shock multiplier is read; the Tank's plate is not (R-MOD-14 calls this an estimate).
	const TConstArrayView<float> Hp = Enemies->GetHp();
	const int32 Num = FMath::Min(DTWeaponSimLocal::NumEnemies(View), Hp.Num());
	const uint8 ShockBit = DTWeaponSimLocal::ShockBit;
	TMap<int32, float> Left;
	for (const FDTHit& Hit : View.Hits)
	{
		if (Hit.EnemyIndex < 0 || Hit.EnemyIndex >= Num || (Hit.EnemySerial != 0 && View.EnemySerials[Hit.EnemyIndex] != Hit.EnemySerial))
		{
			continue;
		}
		float& Remaining = Left.FindOrAdd(Hit.EnemyIndex, Hp[Hit.EnemyIndex]);
		if (Remaining <= 0.0f)
		{
			continue; // already dead this step
		}
		const bool bShocked = (StatusMaskOf(Hit.EnemyIndex, View.EnemySerials[Hit.EnemyIndex]) & ShockBit) != 0;
		Remaining -= FMath::Max(0.0f, Hit.Damage) * (bShocked ? 1.0f + DTConst::STATUS_SHOCK_DAMAGE_TAKEN : 1.0f);
		if (Remaining > 0.0f)
		{
			continue;
		}
		const FWeaponRuntime* Runtime = Players.IsValidIndex(Hit.OwnerId) && Players[Hit.OwnerId].Weapons.IsValidIndex(Hit.WeaponSlot)
			? &Players[Hit.OwnerId].Weapons[Hit.WeaponSlot] : nullptr;
		// A jump's own overkill never jumps again: reactions do not chain.
		if (Runtime && Runtime->Spec.SpilloverSource != 0 && Hit.SourceId != Runtime->Spec.SpilloverSource)
		{
			Overkills.Add(FOverkill{ View.EnemySerials[Hit.EnemyIndex], Hit.OwnerId, Hit.WeaponSlot, Runtime->Spec.SpilloverSource, -Remaining });
		}
		if (Runtime)
		{
			FDTHit Lethal = Hit;
			Lethal.EnemySerial = View.EnemySerials[Hit.EnemyIndex];
			NoteNova(Lethal, Runtime->Spec, -Remaining);
		}
	}
}

void FDTWeaponSim::SpillOverkill(const FDTSimContext& Context)
{
	const FDTWorldView& ConstView = *Context.View;
	for (const FOverkill& Over : Overkills)
	{
		const FDTKill* Kill = ConstView.Kills.FindByPredicate([&Over](const FDTKill& K) { return K.EnemySerial == Over.Serial; });
		FWeaponRuntime* Runtime = FindRuntime(Over.OwnerId, Over.Slot);
		if (!Kill || !Runtime)
		{
			continue;
		}
		int32 Next = INDEX_NONE;
		if (!FindNearest(ConstView, Kill->Position, Runtime->Spec.SpilloverSeekUu, Over.Serial, Next) || !TryEmit(*Runtime))
		{
			continue;
		}
		FDTHit& Out = Context.View->Hits.AddDefaulted_GetRef();
		Out.EnemyIndex = Next;
		Out.EnemySerial = ConstView.EnemySerials[Next];
		Out.OwnerId = Over.OwnerId;
		Out.Damage = Over.Damage;
		Out.Position = ConstView.EnemyPositions[Next];
		Out.SourceId = Over.SourceId;
		Out.WeaponSlot = Over.Slot;
	}
	Overkills.Reset();
}

bool FDTWeaponSim::TryRefit(const FDTSimContext& Context, FDTPlayerRunState& Player, FWeaponRuntime& Source)
{
	FRandomStream& Rng = Context.Run->GetRng().Get(EDTRngStream::Combat);
	const TArray<FWeaponRuntime>& Runtimes = Players[Player.PlayerIndex].Weapons;
	for (int32 Slot = 0; Slot < Player.Weapons.Num() && Slot < Runtimes.Num(); ++Slot)
	{
		FDTWeaponInstance& Weapon = Player.Weapons[Slot];
		if (Weapon.Sockets.Num() >= DTConst::MOD_SOCKETS_PER_WEAPON)
		{
			continue;
		}
		TArray<int32, TInlineAllocator<32>> Pool;
		for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
		{
			const FName Id = DTWeapons::ModifierId(Mod);
			const bool bInPool = DTCat::Modifiers[Mod].bStarter || Unlocked.Contains(Id);
			const bool bFitted = Weapon.Sockets.ContainsByPredicate([Id](const FDTWeaponInstance::FSocket& S) { return S.ModId == Id; });
			if (bInPool && !bFitted && DTModCompat::IsCompatible(Runtimes[Slot].Spec.EffectiveCaps, Mod))
			{
				Pool.Add(Mod);
			}
		}
		if (Pool.Num() == 0)
		{
			continue;
		}
		FDTWeaponInstance::FSocket& Added = Weapon.Sockets.AddDefaulted_GetRef();
		Added.ModId = DTWeapons::ModifierId(Pool[Rng.RandRange(0, Pool.Num() - 1)]);
		return true;
	}
	return false;
}


void FDTWeaponSim::BuildSerialIndex(const FDTWorldView& View)
{
	SerialToIndex.Reset();
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	for (int32 Enemy = 0; Enemy < Num; ++Enemy)
	{
		SerialToIndex.Add(View.EnemySerials[Enemy], Enemy);
	}
}

int32 FDTWeaponSim::IndexOfSerial(uint32 Serial) const
{
	const int32* Found = SerialToIndex.Find(Serial);
	return Found ? *Found : INDEX_NONE;
}

void FDTWeaponSim::StepEffects(const FDTSimContext& Context, FDTBulletPool* Bullets)
{
	if (!Context.Run || !Context.View)
	{
		return;
	}
	FDTRunState& Run = *Context.Run;
	FDTWorldView& View = *Context.View;
	SyncPlayers(Run);
	BuildSerialIndex(View);
	bPriorityBuilt = false;
	StepClustered.Reset();
	for (FPlayerRuntime& Player : Players)
	{
		Player.SpreadReadyIn -= Context.StepSeconds;
		Player.AuraReadyIn -= Context.StepSeconds;
		for (TMap<uint32, int64>::TIterator It = Player.PulledUntilStep.CreateIterator(); It; ++It)
		{
			if (It.Value() < Context.StepIndex)
			{
				It.RemoveCurrent();
			}
		}
	}

	// Kills first: the marks were made by last Effects' arcs and the cleaves and slams of this step.
	const TArray<FArcMark> Marks = MoveTemp(ArcMarks);
	ArcMarks.Reset();
	ReactToKills(Context, Marks);
	ReactDuoKills(Context, Bullets); // before Marks of the dead are dropped
	StepCleaved.Reset();
	StepSlammed.Reset();
	StepKeeperState(Context);
	ReactKeeperKills(Context, Bullets);

	TArray<FImpact> Batch = MoveTemp(Impacts);
	Impacts.Reset();
	for (const FImpact& Impact : Batch)
	{
		ReactToImpact(Context, Bullets, Impact);
	}
	for (int32 Owner = 0; Owner < Players.Num() && Owner < Run.GetNumPlayers(); ++Owner)
	{
		StepHeldControl(Context, Owner, Players[Owner]);
	}
	StepDuos(Context, Bullets);

	// Mod.ChainTrigger: a kill by the weapon fires a free volley, at most once per its interval.
	for (const FDTKill& Kill : View.Kills)
	{
		FWeaponRuntime* Runtime = FindRuntime(Kill.OwnerId, Kill.WeaponSlot);
		if (!Runtime || Runtime->Spec.ChainTriggerSource == 0 || Runtime->ChainTriggerReadyIn > 0.0f || !Run.IsValidPlayer(Kill.OwnerId))
		{
			continue;
		}
		Runtime->ChainTriggerReadyIn = Runtime->Spec.ChainTriggerIntervalS;
		FireVolley(Context, Bullets, Run.GetPlayer(Kill.OwnerId), Kill.WeaponSlot, *Runtime, Runtime->Spec.ChainTriggerSource, true);
	}

	SpillOverkill(Context);
	StepNovas(Context);

	// Mod.Refit: a kill by a full weapon, once per its interval; an interval with no socket to fill is not spent.
	for (const FDTKill& Kill : View.Kills)
	{
		FWeaponRuntime* Runtime = FindRuntime(Kill.OwnerId, Kill.WeaponSlot);
		if (!Runtime || Runtime->Spec.RefitSource == 0 || Runtime->RefitReadyIn > 0.0f || !Run.IsValidPlayer(Kill.OwnerId))
		{
			continue;
		}
		FDTPlayerRunState& Player = Run.GetPlayer(Kill.OwnerId);
		if (Player.Weapons[Kill.WeaponSlot].Sockets.Num() >= Runtime->Spec.RefitFitted && TryRefit(Context, Player, *Runtime))
		{
			Runtime->RefitReadyIn = Runtime->Spec.RefitIntervalS;
		}
	}

	for (const FPendingStatus& Pending : PendingStatuses)
	{
		const int32 Enemy = IndexOfSerial(Pending.EnemySerial);
		if (Enemy != INDEX_NONE)
		{
			ApplyStatus(Context, Pending.Params, FDTEnemyHandle{ Enemy, Pending.EnemySerial }, Pending.HitDamage, Pending.OwnerId, Pending.SourceId);
		}
	}
	PendingStatuses.Reset();
}

// ------------------------------------------------------------------------------------------------------------------
// Reactions

void FDTWeaponSim::EnemiesInCircle(const FDTWorldView& View, const FVector2f& Center, float Radius, TArray<int32>& Out) const
{
	Out.Reset();
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	if (Num == 0 || View.EnemyHash.Num() == 0 || Radius <= 0.0f)
	{
		return;
	}
	View.EnemyHash.QueryCircle(Center, Radius, Out);
	Out.RemoveAll([&View, &Center, Radius, Num](int32 Enemy)
	{
		return Enemy < 0 || Enemy >= Num || FVector2f::DistSquared(Center, View.EnemyPositions[Enemy]) > Radius * Radius;
	});
	Out.Sort();
}

FDTFlagParams FDTWeaponSim::StatusParams(const FDTWeaponSpec& Spec, FName StatusWord) const
{
	FDTFlagParams Out;
	Out.SetWord(DTFlagParam::Status, StatusWord);
	for (const FDTResolvedFlag& Flag : Spec.Flags)
	{
		const FDTFlagParams& Params = Flag.Entry.Params;
		if (Flag.Entry.Flag == DTCat::EFlag::StatusApply && Params.Has(DTFlagParam::FreezeStacks))
		{
			Out.Replace(DTFlagParam::FreezeStacks, Params.Get(DTFlagParam::FreezeStacks));
			Out.Replace(DTFlagParam::FreezeDurationScale, Params.Get(DTFlagParam::FreezeDurationScale));
			break;
		}
	}
	return Out;
}

void FDTWeaponSim::StepHeldControl(const FDTSimContext& Context, int32 OwnerId, FPlayerRuntime& Player)
{
	using namespace DTWeaponSimLocal;
	if (!Enemies || Player.Weapons.Num() == 0)
	{
		return;
	}
	FDTWorldView& View = *Context.View;
	const FHeldControl Held = ReadHeldControl(Player.Weapons[0].Spec); // Keeper flags reach every weapon alike (R-KEE-03)
	const bool bBlocks = Held.bBlocks;
	const int32 HeldMax = Held.HeldMax;
	const uint16 Source = Held.Source;
	const int32 Num = FMath::Min(NumEnemies(View), Enemies->Num());

	// frozen_blocks_enemies: a Frozen enemy is a wall. Anything overlapping one is pushed back out to its rim, through the
	// displacement queue (weapons never write positions), so the crowd has to walk around it.
	if (bBlocks)
	{
		TArray<int32> Local;
		for (int32 Enemy = 0; Enemy < Num; ++Enemy)
		{
			if (Enemies->GetAux(Enemy).FreezeOwner != OwnerId || !Enemies->GetAux(Enemy).IsFrozen())
			{
				continue;
			}
			const FVector2f Wall = View.EnemyPositions[Enemy];
			const float Radius = View.EnemyRadii[Enemy];
			EnemiesInCircle(View, Wall, Radius + MaxEnemyRadius, Local);
			for (const int32 Other : Local)
			{
				if (Other == Enemy || Other >= Num || Enemies->GetAux(Other).IsFrozen())
				{
					continue;
				}
				const FVector2f Away = View.EnemyPositions[Other] - Wall;
				const float Dist = Away.Size();
				const float Reach = Radius + View.EnemyRadii[Other];
				if (Dist >= Reach)
				{
					continue;
				}
				FDTDisplacement& Move = View.Displacements.AddDefaulted_GetRef();
				Move.EnemyIndex = Other;
				Move.EnemySerial = View.EnemySerials[Other];
				Move.Delta = SafeDir(Away, FVector2f(1.0f, 0.0f)) * (Reach - Dist);
				Move.OwnerId = OwnerId;
			}
		}
	}

	// freeze_until_hit: enemies.md owns Freeze, so the hold is a Freeze re-applied for two steps while it would run out and
	// no weapon has struck the enemy. A strike stops the hold; it never shortens a Freeze already running.
	if (HeldMax > 0)
	{
		const float Hold = 2.0f * Context.StepSeconds;
		auto Refresh = [&](int32 Enemy, uint32 Serial)
		{
			FDTStatusApply& Apply = View.StatusApplies.AddDefaulted_GetRef();
			Apply.Handle = FDTEnemyHandle{ Enemy, Serial };
			Apply.Status = EDTStatus::Freeze;
			Apply.OwnerId = OwnerId;
			Apply.SourceId = Source;
			Apply.FreezeScale = Hold / DTConst::STATUS_FREEZE_S;
		};
		for (int32 Index = 0; Index < Player.HeldFrozen.Num();)
		{
			const uint32 Serial = Player.HeldFrozen[Index];
			const int32 Enemy = IndexOfSerial(Serial);
			if (Enemy == INDEX_NONE || Enemy >= Num || !Enemies->GetAux(Enemy).IsFrozen() || Player.FrozenStruck.Contains(Serial))
			{
				Player.HeldFrozen.RemoveAt(Index);
				continue;
			}
			Refresh(Enemy, Serial);
			++Index;
		}
		for (int32 Enemy = 0; Enemy < Num && Player.HeldFrozen.Num() < HeldMax; ++Enemy)
		{
			const FDTEnemyAux& Aux = Enemies->GetAux(Enemy);
			const uint32 Serial = View.EnemySerials[Enemy];
			if (Aux.IsFrozen() && Aux.FreezeOwner == OwnerId && Aux.FreezeLeft <= Hold && !Player.FrozenStruck.Contains(Serial) && !Player.HeldFrozen.Contains(Serial))
			{
				Player.HeldFrozen.Add(Serial);
				Refresh(Enemy, Serial);
			}
		}
	}
	for (TSet<uint32>::TIterator It = Player.FrozenStruck.CreateIterator(); It; ++It)
	{
		const int32 Enemy = IndexOfSerial(*It);
		if (Enemy == INDEX_NONE || Enemy >= Num || !Enemies->GetAux(Enemy).IsFrozen())
		{
			It.RemoveCurrent();
		}
	}

	// charged_aura_uu: while any weapon holds a charge the hero pulses the aura status.
	for (const FWeaponRuntime& Weapon : Player.Weapons)
	{
		const FDTResolvedFlag* Still = Weapon.StillCharges > 0 ? Weapon.Spec.FindFlag(DTCat::EFlag::Stillness) : nullptr;
		const float Aura = Still ? Still->Entry.Params.Get(DTFlagParam::ChargedAuraUu) : 0.0f;
		if (Aura <= 0.0f)
		{
			continue;
		}
		if (Player.AuraReadyIn <= 0.0f)
		{
			Player.AuraReadyIn = Still->Entry.Params.Get(DTFlagParam::AuraIntervalS);
			const FDTFlagParams Params = StatusParams(Weapon.Spec, Still->Entry.Params.GetWord(DTFlagParam::AuraStatus));
			TArray<int32> Local;
			EnemiesInCircle(View, HeroOf(View, OwnerId), Aura, Local);
			for (const int32 Enemy : Local)
			{
				ApplyStatus(Context, Params, FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] }, 0.0f, OwnerId, Still->Entry.SourceId);
			}
		}
		break;
	}
}

uint8 FDTWeaponSim::StatusMaskOf(int32 Enemy, uint32 Serial) const
{
	return Enemies && Enemies->IsHandleValid(FDTEnemyHandle{ Enemy, Serial }) ? Enemies->GetStatusMask(Enemy) : 0;
}

float FDTWeaponSim::BurnRate(int32 OwnerId) const
{
	float Rate = 0.0f;
	if (Players.IsValidIndex(OwnerId))
	{
		for (const FFlagSource& Source : Players[OwnerId].Sources)
		{
			for (const FDTFlagEntry& Entry : Source.Flags.Entries)
			{
				Rate = FMath::Max(Rate, Entry.Params.Get(DTFlagParam::BurnTickRate));
			}
		}
	}
	return Rate;
}

float FDTWeaponSim::StrongestDamage(int32 OwnerId) const
{
	float Best = 0.0f;
	if (Players.IsValidIndex(OwnerId))
	{
		for (const FWeaponRuntime& Weapon : Players[OwnerId].Weapons)
		{
			Best = FMath::Max(Best, Weapon.Spec.Damage);
		}
	}
	return Best;
}

const TArray<int32>& FDTWeaponSim::PriorityTargets(const FDTWorldView& View)
{
	if (bPriorityBuilt)
	{
		return Priority;
	}
	bPriorityBuilt = true;
	Priority.Reset();
	if (Enemies)
	{
		const TConstArrayView<uint8> Flags = Enemies->GetFlags();
		const TConstArrayView<EDTEnemyRole> Roles = Enemies->GetRoles();
		const int32 Num = FMath::Min3(DTWeaponSimLocal::NumEnemies(View), Flags.Num(), Roles.Num());
		for (int32 Enemy = 0; Enemy < Num; ++Enemy)
		{
			if ((Flags[Enemy] & DTEnemyFlags::Elite) != 0 || Roles[Enemy] == EDTEnemyRole::Boss)
			{
				Priority.Add(Enemy);
			}
		}
	}
	return Priority;
}

bool FDTWeaponSim::ArcJump(const FDTSimContext& Context, FWeaponRuntime& Runtime, const FDTFlagEntry& Chain, const TArray<FDTResolvedFlag>& Flags,
	int32 OwnerId, int8 Slot, int32 Enemy, float Damage, bool bMark)
{
	using namespace DTWeaponSimLocal;
	if (!TryEmit(Runtime))
	{
		return false;
	}
	FDTWorldView& View = *Context.View;
	const uint32 Serial = View.EnemySerials[Enemy];
	float Final = Damage;
	if ((StatusMaskOf(Enemy, Serial) & ShockBit) != 0)
	{
		Final *= 1.0f + Chain.Params.Get(DTFlagParam::DmgVsShocked);
	}
	FDTHit& Out = View.Hits.AddDefaulted_GetRef();
	Out.EnemyIndex = Enemy;
	Out.EnemySerial = Serial;
	Out.OwnerId = OwnerId;
	Out.Damage = Final;
	Out.Position = View.EnemyPositions[Enemy];
	Out.SourceId = Chain.SourceId;
	Out.WeaponSlot = Slot;
	if (bMark)
	{
		ArcMarks.Add(FArcMark{ Serial, OwnerId, Slot, Chain.SourceId, Damage });
	}
	for (const FDTResolvedFlag& Status : Flags)
	{
		if (Status.Entry.Flag == DTCat::EFlag::StatusApply && Status.Entry.Params.GetWord(DTFlagParam::On) == WordChainHit)
		{
			ApplyStatus(Context, Status.Entry.Params, FDTEnemyHandle{ Enemy, Serial }, Final, OwnerId, Status.Entry.SourceId);
		}
	}
	return true;
}

void FDTWeaponSim::CrushContacts(const FDTSimContext& Context, const FDTFlagEntry& Pull, const TArray<TPair<int32, FVector2f>>& Moved, float HitDamage, int32 OwnerId)
{
	FDTWorldView& View = *Context.View;
	const float Damage = HitDamage * Pull.Params.Get(DTFlagParam::CrushDmg);
	for (int32 A = 0; A < Moved.Num(); ++A)
	{
		const float RadiusA = View.EnemyRadii[Moved[A].Key];
		bool bTouching = false;
		for (int32 B = 0; B < Moved.Num() && !bTouching; ++B)
		{
			if (A != B)
			{
				const float Reach = RadiusA + View.EnemyRadii[Moved[B].Key];
				bTouching = FVector2f::DistSquared(Moved[A].Value, Moved[B].Value) <= Reach * Reach;
			}
		}
		if (bTouching)
		{
			FDTHit& Out = View.Hits.AddDefaulted_GetRef();
			Out.EnemyIndex = Moved[A].Key;
			Out.EnemySerial = View.EnemySerials[Moved[A].Key];
			Out.OwnerId = OwnerId;
			Out.Damage = Damage;
			Out.Position = View.EnemyPositions[Moved[A].Key];
			Out.SourceId = Pull.SourceId;
		}
	}
}

void FDTWeaponSim::ForkArcs(const FDTSimContext& Context, const FDTKill& Kill, const FArcMark& Mark)
{
	FWeaponRuntime* Runtime = FindRuntime(Mark.OwnerId, Mark.Slot);
	if (!Runtime)
	{
		return;
	}
	const FDTResolvedFlag* Chain = Runtime->Spec.Flags.FindByPredicate([&Mark](const FDTResolvedFlag& F)
	{
		return F.Entry.Flag == DTCat::EFlag::Chain && F.Entry.SourceId == Mark.SourceId;
	});
	if (!Chain)
	{
		return;
	}
	const FDTWorldView& View = *Context.View;
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	// kill_shocks_neighbours_uu: the arc's kill Shocks everything in reach of the death.
	const float ShockReach = Chain->Entry.Params.Get(DTFlagParam::KillShocksNeighboursUu);
	if (ShockReach > 0.0f)
	{
		FDTFlagParams Shock;
		Shock.SetWord(DTFlagParam::Status, DTWeaponSimLocal::WordShock);
		TArray<int32> Near;
		EnemiesInCircle(View, Kill.Position, ShockReach, Near);
		for (const int32 Enemy : Near)
		{
			ApplyStatus(Context, Shock, FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] }, Mark.Damage, Mark.OwnerId, Mark.SourceId);
		}
	}
	const int32 Forks = FMath::RoundToInt(Chain->Entry.Params.Get(DTFlagParam::ForkOnKill));
	const float Range = Chain->Entry.Params.Get(DTFlagParam::RangeUu);
	TArray<int32> Local;
	View.EnemyHash.QueryCircle(Kill.Position, Range, Local);
	Local.Sort();
	TArray<int32, TInlineAllocator<8>> Struck;
	for (int32 Fork = 0; Fork < Forks; ++Fork)
	{
		int32 Best = INDEX_NONE;
		float BestDist = Range * Range;
		for (const int32 Enemy : Local)
		{
			if (Enemy < 0 || Enemy >= Num || Struck.Contains(Enemy))
			{
				continue;
			}
			const float D = FVector2f::DistSquared(Kill.Position, View.EnemyPositions[Enemy]);
			if (D < BestDist)
			{
				BestDist = D;
				Best = Enemy;
			}
		}
		if (Best == INDEX_NONE || !ArcJump(Context, *Runtime, Chain->Entry, Runtime->Spec.Flags, Mark.OwnerId, Mark.Slot, Best, Mark.Damage, false))
		{
			return; // a fork never forks again
		}
		Struck.Add(Best);
	}
}

void FDTWeaponSim::ShatterAt(const FDTSimContext& Context, const FDTFlagEntry& Entry, int32 OwnerId, int32 Slot, const FVector2f& Center, float Radius)
{
	FWeaponRuntime* Runtime = FindRuntime(OwnerId, Slot != INDEX_NONE ? Slot : 0);
	if (!Runtime || !TryEmit(*Runtime))
	{
		return;
	}
	const float Damage = StrongestDamage(OwnerId) * Entry.Params.Get(DTFlagParam::ShatterDmg);
	TArray<FDTEnemyHandle> Hit;
	HitCircle(Context, OwnerId, INDEX_NONE, Entry.SourceId, Center, Radius, Damage, false, nullptr, &Hit);
	const FName Word = Entry.Params.GetWord(DTFlagParam::ShatterStatus);
	if (!Word.IsNone())
	{
		const FDTFlagParams Params = StatusParams(Runtime->Spec, Word);
		for (const FDTEnemyHandle& Handle : Hit)
		{
			ApplyStatus(Context, Params, Handle, Damage, OwnerId, Entry.SourceId);
		}
	}
	FArcFlash& Flash = Arcs.AddDefaulted_GetRef();
	Flash.OwnerId = OwnerId;
	Flash.Position = Center;
	Flash.Reach = Radius;
	Flash.ArcDeg = 360.0f;
	Flash.TimeLeft = 0.5f * Runtime->Spec.Cooldown;
}

void FDTWeaponSim::FreezeCluster(const FDTSimContext& Context, const FDTFlagEntry& Pull, const TArray<TPair<int32, FVector2f>>& Moved, const FImpact& Impact)
{
	using namespace DTWeaponSimLocal;
	const FDTWorldView& View = *Context.View;
	// Only enemies not yet Frozen count, so a held cluster cannot freeze and burst again on every impact.
	TArray<TPair<int32, FVector2f>> Open;
	for (const TPair<int32, FVector2f>& Item : Moved)
	{
		const uint32 Serial = View.EnemySerials[Item.Key];
		if ((StatusMaskOf(Item.Key, Serial) & FreezeBit) == 0 && !StepClustered.Contains(Serial))
		{
			Open.Add(Item);
		}
	}
	TArray<int32> Cluster;
	FVector2f Center = FVector2f::ZeroVector;
	for (int32 A = 0; A < Open.Num(); ++A)
	{
		for (int32 B = 0; B < Open.Num(); ++B)
		{
			const float Reach = View.EnemyRadii[Open[A].Key] + View.EnemyRadii[Open[B].Key];
			if (A != B && FVector2f::DistSquared(Open[A].Value, Open[B].Value) <= Reach * Reach)
			{
				Cluster.Add(Open[A].Key);
				Center += Open[A].Value;
				break;
			}
		}
	}
	if (Cluster.Num() == 0 || Cluster.Num() < FMath::RoundToInt(Pull.Params.Get(DTFlagParam::ClusterFreezeMin)))
	{
		return;
	}
	FDTFlagParams Freeze;
	Freeze.SetWord(DTFlagParam::Status, WordFreeze);
	for (const int32 Enemy : Cluster)
	{
		StepClustered.Add(View.EnemySerials[Enemy]);
		ApplyStatus(Context, Freeze, FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] }, Impact.HitDamage, Impact.OwnerId, Pull.SourceId);
	}
	if (Pull.Params.Get(DTFlagParam::Shatter) > 0.0f)
	{
		ShatterAt(Context, Pull, Impact.OwnerId, Impact.Slot, Center / static_cast<float>(Cluster.Num()), Pull.Params.Get(DTFlagParam::ShatterUu));
	}
}

void FDTWeaponSim::ExplodeBurning(const FDTSimContext& Context, const FDTKill& Kill, const FDTFlagEntry& Entry, float Radius)
{
	const int32 Slot = Kill.WeaponSlot != INDEX_NONE ? Kill.WeaponSlot : 0;
	FWeaponRuntime* Runtime = FindRuntime(Kill.OwnerId, Slot);
	if (!Runtime || !TryEmit(*Runtime))
	{
		return;
	}
	const float Damage = StrongestDamage(Kill.OwnerId) * Entry.Params.Get(DTFlagParam::ExplodeDmg);
	TArray<FDTEnemyHandle> Hit;
	HitCircle(Context, Kill.OwnerId, INDEX_NONE, Entry.SourceId, Kill.Position, Radius, Damage, false, nullptr, &Hit);
	FDTFlagParams Burn;
	Burn.SetWord(DTFlagParam::Status, DTWeaponSimLocal::WordBurn);
	for (const FDTEnemyHandle& Handle : Hit)
	{
		ApplyStatus(Context, Burn, Handle, Damage, Kill.OwnerId, Entry.SourceId); // the explosion Burns (keeper_identity_spec)
	}
	if (Entry.Flag == DTCat::EFlag::Field && Entry.Params.GetWord(DTFlagParam::Leave) == DTWeaponSimLocal::WordField)
	{
		SpawnField(Kill.OwnerId, Slot, Entry.SourceId, Kill.Position, Entry.Params, Damage);
	}
}

void FDTWeaponSim::SpreadBurn(const FDTSimContext& Context, const FDTKill& Kill, const FDTFlagEntry& Entry, float Radius, float BurnDps)
{
	const FDTWorldView& View = *Context.View;
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	if (Num == 0 || View.EnemyHash.Num() == 0)
	{
		return;
	}
	TArray<int32> Local;
	View.EnemyHash.QueryCircle(Kill.Position, Radius, Local);
	Local.Sort();
	FDTFlagParams Burn;
	Burn.SetWord(DTFlagParam::Status, DTWeaponSimLocal::WordBurn);
	// ApplyStatus turns it back into the same damage per second.
	const float HitDamage = BurnDps / (DTConst::STATUS_BURN_DPS_RATIO * (1.0f + BurnRate(Kill.OwnerId)));
	for (const int32 Enemy : Local)
	{
		if (Enemy >= 0 && Enemy < Num && FVector2f::DistSquared(Kill.Position, View.EnemyPositions[Enemy]) <= Radius * Radius)
		{
			ApplyStatus(Context, Burn, FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] }, HitDamage, Kill.OwnerId, Entry.SourceId);
		}
	}
}

void FDTWeaponSim::ReactToKills(const FDTSimContext& Context, TConstArrayView<FArcMark> Marks)
{
	using namespace DTWeaponSimLocal;
	const TArray<FDTKill>& Kills = Context.View->Kills;
	const TConstArrayView<FDTEnemyDeath> Deaths = Enemies ? Enemies->GetStepDeaths() : TConstArrayView<FDTEnemyDeath>();
	for (int32 Index = 0; Index < Kills.Num(); ++Index)
	{
		const FDTKill& Kill = Kills[Index];
		if (!Players.IsValidIndex(Kill.OwnerId))
		{
			continue;
		}
		for (const FArcMark& Mark : Marks)
		{
			if (Mark.Serial == Kill.EnemySerial && Mark.OwnerId == Kill.OwnerId)
			{
				ForkArcs(Context, Kill, Mark);
				break;
			}
		}
		const bool bHasDeath = Deaths.IsValidIndex(Index) && Deaths[Index].Serial == Kill.EnemySerial;
		if (!bHasDeath)
		{
			continue;
		}
		FPlayerRuntime& Player = Players[Kill.OwnerId];
		if ((Deaths[Index].StatusMask & FreezeBit) != 0)
		{
			for (const FFlagSource& Source : Player.Sources)
			{
				for (const FDTFlagEntry& Entry : Source.Flags.Entries)
				{
					const float Radius = Entry.Params.Get(DTFlagParam::ShatterOnFrozenKillUu);
					if (Radius > 0.0f)
					{
						ShatterAt(Context, Entry, Kill.OwnerId, Kill.WeaponSlot, Kill.Position, Radius);
					}
				}
			}
		}
		if ((Deaths[Index].StatusMask & BurnBit) == 0)
		{
			continue;
		}
		for (const FFlagSource& Source : Player.Sources)
		{
			for (const FDTFlagEntry& Entry : Source.Flags.Entries)
			{
				const float Explode = Entry.Params.Get(DTFlagParam::BurningExplodeOnDeathUu);
				// Hammer's explosions need a cleave's kill, Slam's a slam's, Furnace's any death (their tier lines, ST_Keepers).
				const bool bBlast = Explode > 0.0f && (Entry.Flag == DTCat::EFlag::Field ||
					(Entry.Flag == DTCat::EFlag::ConvertMelee && StepCleaved.Contains(Kill.EnemySerial)) ||
					(Entry.Flag == DTCat::EFlag::Slam && StepSlammed.Contains(Kill.EnemySerial)));
				if (bBlast)
				{
					ExplodeBurning(Context, Kill, Entry, Explode);
				}
				const float Spread = Entry.Params.Get(DTFlagParam::SpreadBurnOnKillUu);
				if (Spread > 0.0f && Player.SpreadReadyIn <= 0.0f)
				{
					Player.SpreadReadyIn = Entry.Params.Get(DTFlagParam::SpreadBurnIntervalS);
					SpreadBurn(Context, Kill, Entry, Spread, Deaths[Index].BurnDps);
				}
			}
		}
	}
}

void FDTWeaponSim::ApplyStatus(const FDTSimContext& Context, const FDTFlagParams& Params, const FDTEnemyHandle& Handle,
	float HitDamage, int32 OwnerId, uint16 SourceId)
{
	using namespace DTWeaponSimLocal;
	const float Chance = Params.Has(DTFlagParam::Chance) ? Params.Get(DTFlagParam::Chance) : 1.0f;
	if (Chance < 1.0f && Context.Run->GetRng().Get(EDTRngStream::Combat).FRand() >= Chance)
	{
		return;
	}
	EDTStatus Status = EDTStatus::Shock;
	const FName Word = Params.GetWord(DTFlagParam::Status);
	bool bOk = false;
	if (Word == WordPatron)
	{
		if (Context.Run->IsValidPlayer(OwnerId))
		{
			const FDTPlayerRunState& Player = Context.Run->GetPlayer(OwnerId);
			const EDTPath Patron = Player.Keepers.Primary != EDTPath::None ? Player.Keepers.Primary : Player.Offer.PatronPath;
			bOk = PatronStatus(Patron, Status);
		}
	}
	else if (!Word.IsNone())
	{
		bOk = ParseStatus(Word.ToString(), Status);
	}
	if (!bOk)
	{
		return; // no Patron yet, or no status named
	}
	FDTStatusApply& Apply = Context.View->StatusApplies.AddDefaulted_GetRef();
	Apply.Handle = Handle;
	Apply.Status = Status;
	Apply.OwnerId = OwnerId;
	Apply.SourceId = SourceId;
	switch (Status)
	{
	case EDTStatus::Mark:
		ApplyMark(OwnerId, Handle.Serial, Context.StepIndex); // keepers.md R-KEE-24: the owner holds it, enemies refuse the word
		break;
	case EDTStatus::Blind:
	{
		// keepers.md R-KEE-25: it reaches the enemy as a short Freeze.
		FDTStatusApply Stun = Apply;
		Stun.Status = EDTStatus::Freeze;
		Stun.FreezeScale = DTConst::STATUS_BLIND_S / DTConst::STATUS_FREEZE_S;
		Context.View->StatusApplies.Add(Stun);
		EnsurePlayer(OwnerId).BlindedUntilStep.Add(Handle.Serial, Context.StepIndex + FMath::RoundToInt64(DTConst::STATUS_BLIND_S * static_cast<float>(DTConst::SIM_FIXED_HZ)));
		break;
	}
	case EDTStatus::Burn:
		Apply.Magnitude = DTConst::STATUS_BURN_DPS_RATIO * HitDamage * (1.0f + BurnRate(OwnerId)); // keepers.md R-KEE-11
		Apply.StackBonus = FMath::RoundToInt(Params.Get(DTFlagParam::BurnStacksMax));
		break;
	case EDTStatus::Chill:
		Apply.Magnitude = FMath::Max(1.0f, Params.Get(DTFlagParam::ChillStacksOnPull)); // keepers.md R-KEE-12
		Apply.FreezeAt = FMath::RoundToInt(Params.Get(DTFlagParam::FreezeStacks));
		Apply.FreezeScale = 1.0f + Params.Get(DTFlagParam::FreezeDurationScale);
		break;
	default:
		break;
	}
}

void FDTWeaponSim::EmitChildren(const FDTSimContext& Context, FDTBulletPool* Bullets, FWeaponRuntime& Runtime, const FImpact& Impact,
	const FDTFlagEntry& Entry, bool bRing, const FVector2f& From)
{
	using namespace DTWeaponSimLocal;
	const FDTFlagParams& Params = Entry.Params;
	const int32 Children = FMath::RoundToInt(Params.Get(DTFlagParam::Children));
	if (Children <= 0)
	{
		return;
	}
	FShot Shot = MakeShot(Impact.OwnerId, Impact.Slot, Runtime, Entry.SourceId, From);
	Shot.Damage = Impact.BaseDamage * Params.Get(DTFlagParam::ChildDmg, 1.0f);
	Shot.Pierce = 1;
	Shot.bTrack = true; // children carry their generation (R-MOD-05)
	Shot.Generation = Impact.Generation + 1;
	Shot.Bounces = 0;
	Shot.bLance = Entry.Flag == DTCat::EFlag::CritSplit && !Params.GetWord(FName(TEXT("lance_status"))).IsNone();
	if (Shot.Speed <= 0.0f)
	{
		// A carrier with no round speed (arcs, mines) emits children of the shared child kind.
		Shot.Speed = static_cast<float>(DTConst::WEAPON_CHILD_SPEED_UU_S);
		Shot.Radius = static_cast<float>(DTConst::WEAPON_CHILD_RADIUS_UU);
		Shot.Life = static_cast<float>(DTConst::WEAPON_CHILD_RANGE_UU) / Shot.Speed;
	}
	const float Clear = Impact.EnemySerial != 0 ? Impact.EnemyRadius + Shot.Radius : 0.0f;
	const FVector2f Forward = SafeDir(Impact.Dir, FVector2f(1.0f, 0.0f));
	// retarget=EliteOrBoss: children go for the elites and the boss in their reach, nearest first, then the fan.
	TArray<TPair<float, int32>, TInlineAllocator<8>> Seek;
	if (!bRing && Params.GetWord(DTFlagParam::Retarget) == WordEliteOrBoss)
	{
		const FDTWorldView& View = *Context.View;
		const float Reach = Shot.Speed * Shot.Life;
		for (const int32 Enemy : PriorityTargets(View))
		{
			const float D = FVector2f::DistSquared(From, View.EnemyPositions[Enemy]);
			if (D <= Reach * Reach && View.EnemySerials[Enemy] != Impact.EnemySerial)
			{
				Seek.Emplace(D, Enemy);
			}
		}
		Seek.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key != B.Key ? A.Key < B.Key : A.Value < B.Value; });
	}
	for (int32 Child = 0; Child < Children; ++Child)
	{
		if (Seek.Num() > 0)
		{
			const FVector2f Dir = SafeDir(Context.View->EnemyPositions[Seek[Child % Seek.Num()].Value] - From, Forward);
			SpawnRound(Context, Bullets, Runtime, Shot, From + Dir * Clear, Dir);
			continue;
		}
		float Angle = 0.0f;
		if (bRing)
		{
			Angle = 360.0f * static_cast<float>(Child) / static_cast<float>(Children);
		}
		else if (Params.Has(DTFlagParam::FanDeg))
		{
			const float Fan = Params.Get(DTFlagParam::FanDeg);
			Angle = Children > 1 ? -0.5f * Fan + Fan * static_cast<float>(Child) / static_cast<float>(Children - 1) : 0.0f;
		}
		else
		{
			// A fan with no width spreads evenly over the forward half-plane.
			Angle = -90.0f + 180.0f * static_cast<float>(Child + 1) / static_cast<float>(Children + 1);
		}
		const FVector2f Dir = Rotate(Forward, Angle);
		SpawnRound(Context, Bullets, Runtime, Shot, From + Dir * Clear, Dir);
	}
}

void FDTWeaponSim::ReactToImpact(const FDTSimContext& Context, FDTBulletPool* Bullets, const FImpact& Impact)
{
	using namespace DTWeaponSimLocal;
	FWeaponRuntime* Runtime = FindRuntime(Impact.OwnerId, Impact.Slot);
	if (!Runtime)
	{
		return;
	}
	FDTWorldView& View = *Context.View;
	const FDTWeaponSpec& Spec = Runtime->Spec;
	const bool bEnemyHit = Impact.Kind == EImpact::RoundHit || Impact.Kind == EImpact::ArcHit || Impact.Kind == EImpact::MineHit;
	const bool bPlace = Impact.Kind == EImpact::RoundHit || Impact.Kind == EImpact::ArcSweep || Impact.Kind == EImpact::Detonation;
	const int32 HitEnemy = bEnemyHit ? IndexOfSerial(Impact.EnemySerial) : INDEX_NONE;
	const int32 NumEnemies = DTWeaponSimLocal::NumEnemies(View);
	FRandomStream& Rng = Context.Run->GetRng().Get(EDTRngStream::Combat);
	if (bEnemyHit && (Impact.StatusMask & FreezeBit) != 0)
	{
		Players[Impact.OwnerId].FrozenStruck.Add(Impact.EnemySerial);
	}
	if (Impact.bEmber)
	{
		const FDTResolvedFlag* Slam = Spec.FindFlag(DTCat::EFlag::Slam);
		if (Slam && HitEnemy != INDEX_NONE)
		{
			FDTFlagParams Ember;
			Ember.SetWord(DTFlagParam::Status, Slam->Entry.Params.GetWord(DTFlagParam::EmberStatus));
			ApplyStatus(Context, Ember, FDTEnemyHandle{ HitEnemy, Impact.EnemySerial }, Impact.HitDamage, Impact.OwnerId, Slam->Entry.SourceId);
		}
		return;
	}
	if (Impact.bLance)
	{
		const FDTResolvedFlag* Prism = Spec.FindFlag(DTCat::EFlag::CritSplit);
		if (Prism && HitEnemy != INDEX_NONE)
		{
			FDTFlagParams Lance;
			Lance.SetWord(DTFlagParam::Status, Prism->Entry.Params.GetWord(FName(TEXT("lance_status"))));
			ApplyStatus(Context, Lance, FDTEnemyHandle{ HitEnemy, Impact.EnemySerial }, Impact.HitDamage, Impact.OwnerId, Prism->Entry.SourceId);
		}
		return;
	}
	if (Impact.DuoRow != INDEX_NONE)
	{
		ReactDuoRound(Context, Impact, HitEnemy);
		return;
	}
	if (Impact.bCrit && bEnemyHit)
	{
		ReactToCrit(Context, *Runtime, Impact, HitEnemy);
	}
	if (Impact.Kind == EImpact::Expiry && Spec.WaveSource != 0 && Impact.Generation == 0)
	{
		// Mod.SpreadWave: the round's end is a short arc onward, from its own source; a wave starts no reaction.
		FShot Wave = MakeShot(Impact.OwnerId, Impact.Slot, *Runtime, Spec.WaveSource, Impact.Position);
		Wave.Damage = Impact.BaseDamage * Spec.WaveDamageRatio;
		EmitArc(Context, *Runtime, Wave, Impact.Dir, Spec.WaveArcDeg, Spec.WaveReachUu, false, false);
	}
	// apply_modifier_on_hit: a cleave's hit counts as a round's hit for the modifiers the weapon carries.
	const FDTResolvedFlag* Melee = Impact.Kind == EImpact::ArcHit && Impact.bCleave ? Spec.FindFlag(DTCat::EFlag::ConvertMelee) : nullptr;
	const bool bCarries = Melee && !Melee->Entry.Params.GetWord(DTFlagParam::ApplyModifierOnHit).IsNone();
	auto FromModifier = [](uint16 Source)
	{
		for (int32 Path = 0; Path < static_cast<int32>(EDTPath::Count); ++Path)
		{
			if (Source == DTWeapons::KeeperSource(static_cast<EDTPath>(Path)))
			{
				return false;
			}
		}
		return true;
	};

	// Reactions spawn rounds and hits but never recompose: the spec is fixed for the step.
	const TArray<FDTResolvedFlag>& Flags = Spec.Flags;
	bool bBounced = false; // a modifier's and a Keeper's Ricochet are one bounce
	for (const FDTResolvedFlag& Flag : Flags)
	{
		const FDTFlagParams& Params = Flag.Entry.Params;
		const FName On = Params.GetWord(DTFlagParam::On);
		switch (Flag.Entry.Flag)
		{
		case DTCat::EFlag::Seek:
			if (bEnemyHit)
			{
				ReactSeek(Context, Flag, Impact, HitEnemy);
			}
			break;
		case DTCat::EFlag::CritSplit:
			// A first-generation crit fans lances; lances never split (keepers.md R-KEE-32).
			if (Impact.bCrit && bEnemyHit && Impact.Generation == 0)
			{
				EmitChildren(Context, Bullets, *Runtime, Impact, Flag.Entry, false, Impact.EnemySerial != 0 ? Impact.EnemyPosition : Impact.Position);
			}
			break;
		case DTCat::EFlag::SpeedScale:
		{
			static const FName ParamMaxEchoDmg(TEXT("max_echo_dmg"));
			static const FName ParamMaxEchoDelay(TEXT("max_echo_delay_s"));
			static const FName ParamStacksMax(TEXT("stacks_max"));
			const float Cap = Params.Get(ParamStacksMax);
			if (Params.Get(ParamMaxEchoDmg) > 0.0f && Cap > 0.0f && Players[Impact.OwnerId].Momentum >= Cap &&
				(Impact.Kind == EImpact::RoundHit || Impact.Kind == EImpact::ArcSweep) && TryEmit(*Runtime))
			{
				FEcho& Echo = Echoes.AddDefaulted_GetRef();
				Echo.OwnerId = Impact.OwnerId;
				Echo.Slot = Impact.Slot;
				Echo.SourceId = Flag.Entry.SourceId;
				Echo.Position = Impact.Position;
				Echo.Delay = Params.Get(ParamMaxEchoDelay);
				Echo.Damage = Impact.BaseDamage * Params.Get(ParamMaxEchoDmg);
				Echo.Radius = Impact.Area;
			}
			break;
		}
		case DTCat::EFlag::Split:
		{
			if (!Flag.IsNative() || On == WordVolley)
			{
				break; // fallbacks and per-volley rings run at activation
			}
			bool bFires = false;
			if (Impact.Kind == EImpact::Detonation)
			{
				bFires = true; // children burst from the detonation
			}
			else if (Impact.Kind == EImpact::Expiry)
			{
				bFires = On == WordExpiry;
			}
			else if (Impact.Kind == EImpact::RoundHit)
			{
				bFires = On.IsNone() || On == WordHit;
			}
			if (!bFires)
			{
				break;
			}
			// Generation cap counts every split source together (R-MOD-05); a child at the limit never splits.
			const int32 Limit = Params.Has(DTFlagParam::Gen)
				? FMath::Min(FMath::RoundToInt(Params.Get(DTFlagParam::Gen)), DTConst::MOD_SPLIT_GENERATIONS_MAX)
				: DTConst::MOD_SPLIT_GENERATIONS_MAX;
			if (Impact.Generation >= Limit)
			{
				break;
			}
			const float Chance = Params.Has(DTFlagParam::Chance) ? Params.Get(DTFlagParam::Chance) : 1.0f;
			if (Chance < 1.0f && Rng.FRand() >= Chance)
			{
				break;
			}
			const bool bRing = Params.GetWord(DTFlagParam::Pattern) == WordRing;
			const FVector2f From = (Impact.Kind == EImpact::RoundHit && Impact.EnemySerial != 0) ? Impact.EnemyPosition : Impact.Position;
			EmitChildren(Context, Bullets, *Runtime, Impact, Flag.Entry, bRing, From);
			break;
		}
		case DTCat::EFlag::Field:
			if (Params.GetWord(DTFlagParam::Around) == WordHero)
			{
				break; // Stillness's aura pulses from the hero (StepHeldControl); it never lands on an impact
			}
			if (Params.GetWord(DTFlagParam::Trigger) == WordHitOnBurning)
			{
				if (Flag.IsNative() && bEnemyHit && (Impact.StatusMask & BurnBit) != 0)
				{
					SpawnField(Impact.OwnerId, Impact.Slot, Flag.Entry.SourceId, Impact.EnemyPosition, Params, Impact.BaseDamage);
				}
			}
			else if (Flag.IsNative() && bPlace)
			{
				SpawnField(Impact.OwnerId, Impact.Slot, Flag.Entry.SourceId, Impact.Position, Params, Impact.BaseDamage);
			}
			break;
		case DTCat::EFlag::Pull:
		{
			if (!Flag.IsNative() || On == WordVolley)
			{
				break;
			}
			const float Distance = Params.Get(DTFlagParam::DistanceUu);
			// pulled_vuln: a pulled enemy takes more from every hit; the extra is Keeper damage (as frozen_vuln, R-KEE-13).
			const float Vuln = Params.Get(DTFlagParam::PulledVuln);
			FPlayerRuntime& Puller = Players[Impact.OwnerId];
			// R-KEE-21: one pull per enemy per interval, whoever pulls; what a pull carries (crush, the mark, on=Pulled) rides on it.
			const int64 PullSteps = FMath::Max<int64>(1, FMath::RoundToInt64(DTConst::KEEPER_PULL_INTERVAL_S / Context.StepSeconds));
			const auto MayPull = [this, &Context, PullSteps](const FDTEnemyHandle& Handle)
			{
				return !Enemies || Enemies->TryPull(Handle, Context.StepIndex, PullSteps);
			};
			if (Vuln > 0.0f && HitEnemy != INDEX_NONE && Puller.PulledUntilStep.Contains(Impact.EnemySerial))
			{
				FDTHit& Extra = View.Hits.AddDefaulted_GetRef();
				Extra.EnemyIndex = HitEnemy;
				Extra.EnemySerial = Impact.EnemySerial;
				Extra.OwnerId = Impact.OwnerId;
				Extra.Damage = Impact.HitDamage * Vuln;
				Extra.Position = View.EnemyPositions[HitEnemy];
				Extra.SourceId = Flag.Entry.SourceId;
			}
			if (Params.GetWord(DTFlagParam::Target) == WordOrigin)
			{
				// Mod.Backwash: the hit enemy is drawn toward the round's origin.
				if ((Impact.Kind == EImpact::RoundHit || Impact.Kind == EImpact::ArcHit || Impact.Kind == EImpact::MineHit) && HitEnemy != INDEX_NONE)
				{
					const FVector2f To = Impact.Origin - View.EnemyPositions[HitEnemy];
					const float Dist = To.Size();
					if (Dist > KINDA_SMALL_NUMBER && MayPull(FDTEnemyHandle{ HitEnemy, Impact.EnemySerial }))
					{
						FDTDisplacement& Move = View.Displacements.AddDefaulted_GetRef();
						Move.EnemyIndex = HitEnemy;
						Move.EnemySerial = Impact.EnemySerial;
						Move.Delta = To / Dist * FMath::Min(Distance, Dist);
						Move.OwnerId = Impact.OwnerId;
					}
				}
				break;
			}
			if (!bPlace || NumEnemies == 0 || View.EnemyHash.Num() == 0)
			{
				break;
			}
			float Radius = Params.Get(DTFlagParam::RadiusUu);
			if (On == WordChargedVolley)
			{
				// Stillness's Pull: only a charged volley's impact, reaching charged_pull_uu.
				const FDTResolvedFlag* Still = Impact.bCharged ? Spec.FindFlag(DTCat::EFlag::Stillness) : nullptr;
				if (!Still)
				{
					break;
				}
				Radius = Still->Entry.Params.Get(DTFlagParam::ChargedPullUu);
			}
			const int64 VulnSteps = FMath::RoundToInt64(Params.Get(DTFlagParam::PulledVulnSeconds) / Context.StepSeconds);
			const bool bCrush = Params.Get(DTFlagParam::CrushDmg) > 0.0f;
			const bool bCluster = Params.Get(DTFlagParam::ClusterFreezeMin) > 0.0f;
			// chill_stacks_on_pull sits on the Pull; the status it scales is the on=Pulled StatusApply (R-KEE-12).
			TArray<TPair<FDTFlagParams, uint16>, TInlineAllocator<2>> PulledStatuses;
			for (const FDTResolvedFlag& Status : Flags)
			{
				if (Status.Entry.Flag == DTCat::EFlag::StatusApply && Status.Entry.Params.GetWord(DTFlagParam::On) == WordPulled)
				{
					FDTFlagParams& Pulled = PulledStatuses.Emplace_GetRef(Status.Entry.Params, Status.Entry.SourceId).Key;
					if (Params.Has(DTFlagParam::ChillStacksOnPull))
					{
						Pulled.Replace(DTFlagParam::ChillStacksOnPull, Params.Get(DTFlagParam::ChillStacksOnPull));
					}
				}
			}
			TArray<TPair<int32, FVector2f>> Moved;
			TArray<int32> Local;
			View.EnemyHash.QueryCircle(Impact.Position, Radius, Local);
			for (const int32 Enemy : Local)
			{
				if (Enemy < 0 || Enemy >= NumEnemies)
				{
					continue;
				}
				const FVector2f To = Impact.Position - View.EnemyPositions[Enemy];
				const float Dist = To.Size();
				if (Dist > Radius || Dist <= KINDA_SMALL_NUMBER || !MayPull(FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] }))
				{
					continue;
				}
				FDTDisplacement& Move = View.Displacements.AddDefaulted_GetRef();
				Move.EnemyIndex = Enemy;
				Move.EnemySerial = View.EnemySerials[Enemy];
				Move.Delta = To / Dist * FMath::Min(Distance, Dist);
				Move.OwnerId = Impact.OwnerId;
				if (LiveReactionsOf(Impact.OwnerId) != 0)
				{
					Puller.StepPulled.Add(Move.EnemySerial); // reactions.md R-REA-04
				}
				if (bCrush || bCluster)
				{
					Moved.Emplace(Enemy, View.EnemyPositions[Enemy] + Move.Delta);
				}
				if (Vuln > 0.0f)
				{
					Puller.PulledUntilStep.Add(Move.EnemySerial, Context.StepIndex + VulnSteps);
				}
				const uint32 PulledSerial = Move.EnemySerial;
				for (const TPair<FDTFlagParams, uint16>& Status : PulledStatuses)
				{
					ApplyStatus(Context, Status.Key, FDTEnemyHandle{ Enemy, PulledSerial }, Impact.HitDamage, Impact.OwnerId, Status.Value);
				}
			}
			if (bCrush)
			{
				CrushContacts(Context, Flag.Entry, Moved, Impact.HitDamage, Impact.OwnerId);
			}
			if (bCluster)
			{
				FreezeCluster(Context, Flag.Entry, Moved, Impact);
			}
			break;
		}
		case DTCat::EFlag::StatusApply:
		{
			// frozen_hit_chills_neighbours_uu: a hit on a Frozen enemy (even the lethal one) Chills around it.
			const float Neighbours = Params.Get(DTFlagParam::FrozenHitChillsNeighboursUu);
			if (Neighbours > 0.0f && bEnemyHit && (Impact.StatusMask & FreezeBit) != 0)
			{
				const FDTFlagParams Chill = StatusParams(Spec, Params.GetWord(DTFlagParam::Status));
				TArray<int32> Local;
				EnemiesInCircle(View, Impact.EnemyPosition, Neighbours, Local);
				for (const int32 Enemy : Local)
				{
					if (View.EnemySerials[Enemy] != Impact.EnemySerial)
					{
						ApplyStatus(Context, Chill, FDTEnemyHandle{ Enemy, View.EnemySerials[Enemy] }, Impact.HitDamage, Impact.OwnerId, Flag.Entry.SourceId);
					}
				}
			}
			if (!bEnemyHit || HitEnemy == INDEX_NONE)
			{
				break;
			}
			const bool bFires = On.IsNone() || On == WordHit || (On == WordCleave && Impact.bCleave);
			if (bFires)
			{
				ApplyStatus(Context, Params, FDTEnemyHandle{ HitEnemy, Impact.EnemySerial }, Impact.HitDamage, Impact.OwnerId, Flag.Entry.SourceId);
			}
			break;
		}
		case DTCat::EFlag::Chain:
		{
			if (!bEnemyHit || NumEnemies == 0)
			{
				break;
			}
			// The hit jumps to the nearest further enemies in range for a share of its damage (keepers.md §2).
			const int32 Jumps = FMath::RoundToInt(Params.Get(DTFlagParam::Jumps));
			const float Range = Params.Get(DTFlagParam::RangeUu);
			const float Damage = Impact.HitDamage * Params.Get(DTFlagParam::Dmg, 1.0f);
			const float ReturnChance = Params.Get(DTFlagParam::ReturnSameTarget);
			const bool bMark = Params.Get(DTFlagParam::ForkOnKill) > 0.0f || Params.Get(DTFlagParam::KillShocksNeighboursUu) > 0.0f;
			TArray<uint32, TInlineAllocator<8>> Visited;
			Visited.Add(Impact.EnemySerial);
			FVector2f From = Impact.EnemyPosition;
			int32 Previous = HitEnemy; // the enemy the arc leaves; INDEX_NONE when the struck one died
			for (int32 Jump = 0; Jump < Jumps; ++Jump)
			{
				TArray<int32> Local;
				View.EnemyHash.QueryCircle(From, Range, Local);
				int32 Best = INDEX_NONE;
				float BestDist = Range * Range;
				for (const int32 Enemy : Local)
				{
					if (Enemy < 0 || Enemy >= NumEnemies || Visited.Contains(View.EnemySerials[Enemy]))
					{
						continue;
					}
					const float D = FVector2f::DistSquared(From, View.EnemyPositions[Enemy]);
					if (D < BestDist || (D == BestDist && Best != INDEX_NONE && Enemy < Best))
					{
						BestDist = D;
						Best = Enemy;
					}
				}
				if (Best == INDEX_NONE || !ArcJump(Context, *Runtime, Flag.Entry, Flags, Impact.OwnerId, Impact.Slot, Best, Damage, bMark))
				{
					break;
				}
				Visited.Add(View.EnemySerials[Best]);
				// return_same_target: the arc may leap back onto the enemy it just left (one roll per jump).
				if (ReturnChance > 0.0f && Previous != INDEX_NONE && Rng.FRand() < ReturnChance)
				{
					ArcJump(Context, *Runtime, Flag.Entry, Flags, Impact.OwnerId, Impact.Slot, Previous, Damage, bMark);
				}
				Previous = Best;
				From = View.EnemyPositions[Best];
			}
			break;
		}
		case DTCat::EFlag::Echo:
		{
			// Continuous's fallback: a strike on every Nth stream hit (R-WPN-08).
			const bool bStream = Flag.IsFallback() && Flag.Resolution.Cap == DTCat::ECap::Continuous && Impact.Kind == EImpact::RoundHit &&
				++Runtime->StreamHits % DTConst::WEAPON_STREAM_ECHO_EVERY_N == 0;
			const bool bFires = bStream || (Flag.IsNative() && (Impact.Kind == EImpact::RoundHit || Impact.Kind == EImpact::ArcSweep)) ||
				Impact.Kind == EImpact::Detonation; // Mine: native or "the detonation repeats at the same point"
			if (!bFires)
			{
				break;
			}
			const int32 Count = FMath::Max(1, FMath::RoundToInt(Params.Get(DTFlagParam::Count, 1.0f)));
			const int32 EveryN = FMath::RoundToInt(Params.Get(DTFlagParam::EveryN));
			int32& SkyStrikes = Players[Impact.OwnerId].SkyStrikes;
			for (int32 Strike = 0; Strike < Count; ++Strike)
			{
				if (!TryEmit(*Runtime))
				{
					break;
				}
				FEcho& Echo = Echoes.AddDefaulted_GetRef();
				Echo.OwnerId = Impact.OwnerId;
				Echo.Slot = Impact.Slot;
				Echo.SourceId = Flag.Entry.SourceId;
				Echo.Position = Impact.Position;
				Echo.Delay = Params.Get(DTFlagParam::DelaySeconds) * static_cast<float>(Strike + 1);
				Echo.Damage = Impact.BaseDamage * Params.Get(DTFlagParam::Dmg, 1.0f);
				Echo.Radius = Impact.Area * (1.0f + Params.Get(DTFlagParam::RadiusScale));
				Echo.bLeaveField = Params.GetWord(DTFlagParam::Leave) == WordField;
				Echo.bScreen = EveryN > 0 && ++SkyStrikes % EveryN == 0;
			}
			break;
		}
		case DTCat::EFlag::Stillness:
			// The Key's status: a charged volley applies it to what it hits (Status.Freeze, refused by the boss).
			if (Impact.bCharged && HitEnemy != INDEX_NONE && !Params.GetWord(DTFlagParam::Status).IsNone())
			{
				ApplyStatus(Context, Params, FDTEnemyHandle{ HitEnemy, Impact.EnemySerial }, Impact.HitDamage, Impact.OwnerId, Flag.Entry.SourceId);
			}
			break;
		case DTCat::EFlag::Ricochet:
		{
			static const FName ParamReturnSame(TEXT("return_same_target"));
			static const FName ParamBounceDmg(TEXT("bounce_dmg"));
			if (Flag.IsFallback() && Impact.Kind == EImpact::Detonation)
			{
				// Lob and Mine: the blast hops once to the nearest enemy (keepers.md R-KEE-34).
				int32 Hop = INDEX_NONE;
				if (FindNearest(View, Impact.Position, Params.Get(DTFlagParam::SeekRangeUu), 0, Hop))
				{
					HitCircle(Context, Impact.OwnerId, INDEX_NONE, Flag.Entry.SourceId, View.EnemyPositions[Hop], Impact.Area, Impact.BaseDamage, false, nullptr);
				}
				break;
			}
			const bool bFromCleave = bCarries && FromModifier(Flag.Entry.SourceId);
			int32 Bounces = bFromCleave ? FMath::RoundToInt(Params.Get(DTFlagParam::Bounces)) : Impact.BouncesLeft;
			if (Flag.IsNative() && !bBounced && !bFromCleave && Impact.Kind == EImpact::RoundHit && Impact.bSpent)
			{
				Bounces += DuoBounces(Context, Impact, HitEnemy, Bounces > 0);
			}
			if (!Flag.IsNative() || bBounced || (!bFromCleave && (Impact.Kind != EImpact::RoundHit || !Impact.bSpent)) || Bounces <= 0)
			{
				break;
			}
			bBounced = true;
			int32 Next = INDEX_NONE;
			if (!FindNearest(View, Impact.Position, Params.Get(DTFlagParam::SeekRangeUu), Impact.EnemySerial, Next))
			{
				// return_same_target: with nothing in reach the bounce strikes the enemy it left (keepers.md R-KEE-34).
				if (Params.Get(ParamReturnSame) > 0.0f && HitEnemy != INDEX_NONE)
				{
					for (int32 Left = 0; Left < Bounces; ++Left)
					{
						KeeperHit(View, HitEnemy, Impact.OwnerId, Impact.BaseDamage * (1.0f + Params.Get(ParamBounceDmg)), Flag.Entry.SourceId, View);
					}
				}
				break;
			}
			FShot Bounce = MakeShot(Impact.OwnerId, Impact.Slot, *Runtime, Flag.Entry.SourceId, Impact.Position);
			Bounce.Damage = Impact.BaseDamage * (1.0f + Params.Get(ParamBounceDmg));
			Bounce.Pierce = 1;
			Bounce.bTrack = true;
			Bounce.Generation = Impact.Generation;
			Bounce.Bounces = Bounces - 1;
			const FVector2f Dir = SafeDir(View.EnemyPositions[Next] - Impact.Position, Impact.Dir);
			const FVector2f From = Impact.EnemySerial != 0 ? Impact.EnemyPosition + Dir * (Impact.EnemyRadius + Bounce.Radius) : Impact.Position;
			SpawnRound(Context, Bullets, *Runtime, Bounce, From, Dir);
			break;
		}
		default:
			break; // Hunter, Debt, Nova, Charge, VolleyOnDash run off kills, shots and hero motion (DTWeaponSimKeepers.cpp); Push, ConvertOrbit native: no source
		}
	}
}

// ------------------------------------------------------------------------------------------------------------------
// Visuals

void FDTWeaponSim::GatherVisuals(const FDTRunState& Run, const FDTWorldView& View, TArray<FDTWeaponVisual>& Out) const
{
	using namespace DTWeaponSimLocal;
	auto PathOf = [&Run](int32 Owner)
	{
		return Run.IsValidPlayer(Owner) ? Run.GetPlayer(Owner).Keepers.Primary : EDTPath::None;
	};
	for (int32 Owner = 0; Owner < Players.Num() && Owner < Run.GetNumPlayers(); ++Owner)
	{
		const FVector2f Hero = HeroOf(View, Owner);
		for (const FWeaponRuntime& Weapon : Players[Owner].Weapons)
		{
			const FDTWeaponSpec& Spec = Weapon.Spec;
			if (Spec.Pattern == EDTWeaponPattern::Orbiter)
			{
				const int32 Drones = FMath::Max(1, Spec.Count);
				for (int32 Drone = 0; Drone < Drones; ++Drone)
				{
					FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
					V.Kind = FDTWeaponVisual::EKind::Drone;
					V.Position = Hero + Rotate(FVector2f(static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_RADIUS_UU), 0.0f),
						Weapon.DroneAngleDeg + 360.0f * static_cast<float>(Drone) / static_cast<float>(Drones));
					V.Radius = 2.0f * Spec.Radius; // a drone reads as twice its bolt
				}
			}
			for (const FShell& Shell : Weapon.Shells)
			{
				FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
				V.Kind = FDTWeaponVisual::EKind::Mine;
				V.Position = FMath::Lerp(Shell.Target, Shell.Origin, Shell.Flight > 0.0f ? Shell.Left / Shell.Flight : 0.0f);
				V.Radius = Spec.Radius;
			}
			for (const FMine& Mine : Weapon.Mines)
			{
				FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
				V.Kind = FDTWeaponVisual::EKind::Mine;
				V.Position = Mine.Position;
				V.Radius = Spec.Radius;
				if (Mine.Age >= DTConst::WEAPON_MINE_ARM_S)
				{
					FDTWeaponVisual& Zone = Out.AddDefaulted_GetRef();
					Zone.Kind = FDTWeaponVisual::EKind::MineZone;
					Zone.Position = Mine.Position;
					Zone.Radius = Spec.AreaRadius * DTConst::WEAPON_MINE_PULL_RADIUS_RATIO;
					Zone.Path = PathOf(Owner);
				}
			}
		}
	}
	for (const FHunter& Hunter : Hunters)
	{
		FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
		V.Kind = FDTWeaponVisual::EKind::Drone;
		V.Position = Hunter.Position;
		V.Radius = 2.0f * static_cast<float>(DTConst::WEAPON_CHILD_RADIUS_UU);
	}
	for (const FField& Field : Fields)
	{
		FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
		V.Kind = FDTWeaponVisual::EKind::Field;
		V.Position = Field.Position;
		V.Radius = Field.Radius;
		V.Path = PathOf(Field.OwnerId);
	}
	for (const FArcFlash& Arc : Arcs)
	{
		FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
		V.Kind = FDTWeaponVisual::EKind::Arc;
		V.Position = Arc.Position;
		V.Dir = Arc.Dir;
		V.Radius = Arc.Reach;
		V.ArcDeg = Arc.ArcDeg;
		V.Path = Arc.Tint != EDTPath::None ? Arc.Tint : PathOf(Arc.OwnerId);
	}

	// Control cues (R-PRE-11). Reads only: what StepHeldControl acts on, never a position or a timer of its own.
	const int32 Num = Enemies ? FMath::Min(NumEnemies(View), Enemies->Num()) : 0;
	for (int32 Owner = 0; Owner < Players.Num() && Owner < Run.GetNumPlayers(); ++Owner)
	{
		const FPlayerRuntime& Player = Players[Owner];
		if (Player.Weapons.Num() == 0)
		{
			continue;
		}
		// The Stillness dial needs no Trait: every owner of the flag sees what its next volley holds.
		const FDTResolvedFlag* Dial = nullptr;
		int32 Charges = 0;
		for (const FWeaponRuntime& Weapon : Player.Weapons)
		{
			if (const FDTResolvedFlag* Flag = Weapon.Spec.FindFlag(DTCat::EFlag::Stillness))
			{
				Dial = Dial ? Dial : Flag;
				Charges = FMath::Max(Charges, Weapon.StillCharges);
			}
		}
		if (Dial)
		{
			const FDTFlagParams& Params = Dial->Entry.Params;
			const float ChargeS = Params.Get(DTFlagParam::ChargeS);
			const int32 MaxCharges = FMath::Max(1, FMath::RoundToInt(Params.Get(DTFlagParam::ChargesMax, 1.0f)));
			const float Gathered = Charges < MaxCharges && ChargeS > 0.0f ? FMath::Clamp(Player.StillTime / ChargeS, 0.0f, 1.0f) : 0.0f;
			if (Charges > 0 || Gathered > 0.0f)
			{
				FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
				V.Kind = FDTWeaponVisual::EKind::StillCharge;
				V.Position = HeroOf(View, Owner);
				V.Radius = static_cast<float>(DTConst::HERO_RADIUS_UU) * DTConst::PRE_CHARGE_RING_RATIO;
				V.Charges = Charges;
				V.Charge01 = Gathered;
			}
		}
		for (const FWeaponRuntime& Weapon : Player.Weapons)
		{
			const FDTResolvedFlag* Still = Weapon.StillCharges > 0 ? Weapon.Spec.FindFlag(DTCat::EFlag::Stillness) : nullptr;
			const float Aura = Still ? Still->Entry.Params.Get(DTFlagParam::ChargedAuraUu) : 0.0f;
			if (Aura > 0.0f)
			{
				const float Interval = Still->Entry.Params.Get(DTFlagParam::AuraIntervalS);
				FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
				V.Kind = FDTWeaponVisual::EKind::StillAura;
				V.Position = HeroOf(View, Owner);
				V.Radius = Aura;
				V.Pulse01 = Interval > 0.0f ? FMath::Clamp(Player.AuraReadyIn / Interval, 0.0f, 1.0f) : 0.0f;
				break;
			}
		}
	}
	// A Marked enemy stands on a plate of the Keeper's colour (keepers.md R-KEE-36).
	for (int32 Enemy = 0; Enemy < Num; ++Enemy)
	{
		for (int32 Owner = 0; Owner < Players.Num() && Owner < Run.GetNumPlayers(); ++Owner)
		{
			if (IsMarked(Owner, View.EnemySerials[Enemy]))
			{
				FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
				V.Kind = FDTWeaponVisual::EKind::Marked;
				V.Position = View.EnemyPositions[Enemy];
				V.Radius = View.EnemyRadii[Enemy];
				V.Path = EDTPath::Shade;
				break;
			}
		}
	}
	// Every Frozen enemy shows a cue: the wall and the hold where they apply, else the plain diamond.
	for (int32 Enemy = 0; Enemy < Num; ++Enemy)
	{
		const FDTEnemyAux& Aux = Enemies->GetAux(Enemy);
		if (!Aux.IsFrozen())
		{
			continue;
		}
		const auto Emit = [&Out, &View, Enemy](FDTWeaponVisual::EKind Kind)
		{
			FDTWeaponVisual& V = Out.AddDefaulted_GetRef();
			V.Kind = Kind;
			V.Position = View.EnemyPositions[Enemy];
			V.Radius = View.EnemyRadii[Enemy];
		};
		bool bCued = false;
		for (int32 Owner = 0; Owner < Players.Num() && Owner < Run.GetNumPlayers(); ++Owner)
		{
			const FPlayerRuntime& Player = Players[Owner];
			if (Player.Weapons.Num() == 0)
			{
				continue;
			}
			if (Aux.FreezeOwner == Owner && ReadHeldControl(Player.Weapons[0].Spec).bBlocks)
			{
				Emit(FDTWeaponVisual::EKind::FrozenWall);
				bCued = true;
			}
			if (Player.HeldFrozen.Contains(View.EnemySerials[Enemy]))
			{
				Emit(FDTWeaponVisual::EKind::FrozenHold);
				bCued = true;
			}
		}
		if (!bCued)
		{
			Emit(FDTWeaponVisual::EKind::FrozenPlain);
		}
	}
}
