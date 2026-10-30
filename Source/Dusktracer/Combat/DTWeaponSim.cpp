#include "Combat/DTWeaponSim.h"
#include "Combat/DTBulletPool.h"
#include "Core/DTConstantsGen.h"
#include "Telemetry/DTTelemetrySources.h"

// Which reactions an impact runs (R-WPN-08 native expressions; fallbacks run at activation, OnActivation):
//   RoundHit   (a pool round hit an enemy):  Split on=Hit, Field, Pull, StatusApply, Chain, Echo, Ricochet when spent
//   ArcHit     (one enemy inside a sweep):    StatusApply (on=Cleave for converted cleaves), Chain, Backwash pull
//   ArcSweep   (one per sweep, at mid reach): Field, Pull, Echo
//   MineHit    (one enemy in a detonation):   StatusApply, Chain
//   Detonation (one per mine):                Split (children burst), Field, Pull, Echo (fallback: repeats in place)
//   Expiry     (a tracked round's life ran out): Split on=Expiry (Mod.ScatterShell)
// Field ticks, echo strikes, chain jumps, slams and mine pull ticks are hits but never impacts (no reaction chains).

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

	static int32 NumEnemies(const FDTWorldView& View)
	{
		return FMath::Min3(View.EnemyPositions.Num(), View.EnemyRadii.Num(), View.EnemySerials.Num());
	}

	static bool ParseStatus(const FString& Word, EDTStatus& Out)
	{
		static const TCHAR* Names[] = { TEXT("Status.Shock"), TEXT("Status.Burn"), TEXT("Status.Chill"), TEXT("Status.Freeze") };
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

	static bool NeedsLedger(const FDTWeaponSpec& Spec)
	{
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
	}
	Rounds.Reset();
	Fields.Reset();
	Echoes.Reset();
	Arcs.Reset();
	Impacts.Reset();
	PendingStatuses.Reset();
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
	if (const FDTResolvedFlag* Ricochet = Spec.FindFlag(DTCat::EFlag::Ricochet))
	{
		if (Ricochet->IsNative())
		{
			Shot.Bounces = FMath::RoundToInt(Ricochet->Entry.Params.Get(DTFlagParam::Bounces));
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
	const bool bCrit = RollCrit(Context, Runtime.Spec, Damage);
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
	}
}

int32 FDTWeaponSim::EmitArc(const FDTSimContext& Context, FWeaponRuntime& Runtime, const FShot& Shot, const FVector2f& Dir,
	float ArcDeg, float Reach, bool bCleave)
{
	if (!TryEmit(Runtime))
	{
		return 0;
	}
	FDTWorldView& View = *Context.View;
	float Damage = Shot.Damage;
	const bool bCrit = RollCrit(Context, Runtime.Spec, Damage);
	const float CosHalf = FMath::Cos(FMath::DegreesToRadians(0.5f * ArcDeg));
	const bool bFull = ArcDeg >= 360.0f;
	const int32 Num = DTWeaponSimLocal::NumEnemies(View);
	Candidates.Reset();
	if (Num > 0 && View.EnemyHash.Num() > 0)
	{
		View.EnemyHash.QueryCircle(Shot.Origin, Reach + MaxEnemyRadius, Candidates);
	}
	int32 Hit = 0;
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
		Out.Position = View.EnemyPositions[Enemy];
		Out.SourceId = Shot.SourceId;
		Out.WeaponSlot = static_cast<int8>(Shot.Slot);
		Out.bCrit = bCrit;

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
		Impact.EnemyPosition = Out.Position;
		Impact.bCleave = bCleave;
		++Runtime.Counters.Impacts;
		++Hit;
	}
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
	int32 Target = INDEX_NONE;
	switch (Spec.Pattern)
	{
	case EDTWeaponPattern::Aimed:
	case EDTWeaponPattern::MeleeArc:
		if (Spec.Target == EDTWeaponTarget::Nearest && bManual)
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
		Dir = FindNearest(View, Hero, Spec.Range, 0, Target) ? SafeDir(View.EnemyPositions[Target] - Hero, AimDir) : AimDir;
		break;
	case EDTWeaponPattern::Mine:
		FindDensestCells(View, Hero, Spec.Range, Spec.Count + Spec.ForkRounds, MineCells);
		if (MineCells.Num() == 0)
		{
			return false;
		}
		break;
	default:
		return false; // S2 patterns are not fired in S1
	}

	FShot Shot = MakeShot(Owner, Slot, Runtime, SourceOverride != 0 ? SourceOverride : Instance.SourceId, Hero);
	// Stillness: a charge reshapes the weapon's own next volley (keeps the weapon source, R-WPN-07).
	if (!bFree && Runtime.StillCharges > 0)
	{
		if (const FDTResolvedFlag* Still = Spec.FindFlag(DTCat::EFlag::Stillness))
		{
			Shot.Damage *= Still->Entry.Params.Get(DTFlagParam::Mult, 1.0f);
			Shot.Pierce += FMath::RoundToInt(Still->Entry.Params.Get(DTFlagParam::ChargedPierce));
			--Runtime.StillCharges;
		}
	}

	bool bMelee = false;
	int32 Converted = 0;
	if (Spec.Conversion == DTCat::EFlag::ConvertMelee && (Spec.Pattern == EDTWeaponPattern::Aimed || Spec.Pattern == EDTWeaponPattern::Radial))
	{
		// R-WPN-10: the converted rounds fold into ONE arc of their summed damage x the conversion ratio.
		Converted = Spec.ConvertRounds;
		FShot Cleave = Shot;
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

	switch (Spec.Pattern)
	{
	case EDTWeaponPattern::Aimed:
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
		if (Spec.ForkSource != 0 && Remaining > 0)
		{
			FShot Fork = Shot;
			Fork.SourceId = Spec.ForkSource;
			for (int32 Extra = 1; Extra <= Spec.ForkRounds; ++Extra)
			{
				const float Sign = (Extra % 2 == 1) ? 1.0f : -1.0f;
				const float Angle = Sign * Spec.ForkAngleDeg * static_cast<float>((Extra + 1) / 2);
				SpawnRound(Context, Bullets, Runtime, Fork, Hero, Rotate(Dir, Angle));
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
			PlaceMine(Context, Runtime, Owner, Slot, MineCells[Index], Shot.Damage, Shot.SourceId);
		}
		for (int32 Extra = 0; Spec.ForkSource != 0 && Extra < Spec.ForkRounds; ++Extra)
		{
			// Extra mines take the next densest cells, or repeat the densest when the crowd is thin.
			const int32 Cell = Spec.Count + Extra < MineCells.Num() ? Spec.Count + Extra : Extra % MineCells.Num();
			PlaceMine(Context, Runtime, Owner, Slot, MineCells[Cell], Shot.Damage, Spec.ForkSource);
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
		if (bCharge && Runtime.StillCharges > 0)
		{
			if (const FDTResolvedFlag* Still = Spec.FindFlag(DTCat::EFlag::Stillness))
			{
				Shot.Damage *= Still->Entry.Params.Get(DTFlagParam::Mult, 1.0f);
				Shot.Pierce += FMath::RoundToInt(Still->Entry.Params.Get(DTFlagParam::ChargedPierce));
				--Runtime.StillCharges;
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
		if (FindNearest(View, DronePos(Drone), Spec.Range, 0, Enemy))
		{
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
	float Damage, uint16 SourceId)
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
	Mine.Damage = Damage;
	Mine.SourceId = SourceId;
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
		Impact.Origin = Mine.Position;
		Impact.Dir = DTWeaponSimLocal::SafeDir(Out.Position - Mine.Position, FVector2f(1.0f, 0.0f));
		Impact.BaseDamage = Mine.Damage;
		Impact.HitDamage = Out.Damage;
		Impact.EnemySerial = Out.EnemySerial;
		Impact.EnemyRadius = View.EnemyRadii[Out.EnemyIndex];
		Impact.EnemyPosition = Out.Position;
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
}

// ------------------------------------------------------------------------------------------------------------------
// Activation-time expressions, stillness, slam

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
				EmitSlam(Context, OwnerId, Slot, Runtime, Flag);
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

void FDTWeaponSim::StepSlamTriggers(const FDTSimContext& Context, int32 OwnerId, FPlayerRuntime& Player)
{
	const FDTWorldView& View = *Context.View;
	const bool bIntangible = View.HeroIntangible.IsValidIndex(OwnerId) && View.HeroIntangible[OwnerId];
	const bool bDashLanding = Player.bWasIntangible && !bIntangible;
	Player.bWasIntangible = bIntangible;
	const bool bHitTaken = View.HeroHits.ContainsByPredicate([OwnerId](const FDTHeroHit& Hit) { return Hit.PlayerId == OwnerId; });
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
		EmitSlam(Context, OwnerId, BestSlot, Runtime, *Runtime.Spec.FindFlag(DTCat::EFlag::Slam));
	}
}

void FDTWeaponSim::EmitSlam(const FDTSimContext& Context, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime, const FDTResolvedFlag& Slam)
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
	TArray<FDTEnemyHandle> Hit;
	HitCircle(Context, OwnerId, Slot, Slam.Entry.SourceId, Hero, Radius, Damage, false, nullptr, &Hit);
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
		HitCircle(Context, Echo.OwnerId, Echo.Slot, Echo.SourceId, Echo.Position, Echo.Radius, Echo.Damage, false, nullptr);
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
		StepStillness(Context, Owner, PlayerRt);
		StepSlamTriggers(Context, Owner, PlayerRt);
		for (int32 Slot = 0; Slot < Player.Weapons.Num(); ++Slot)
		{
			FWeaponRuntime& Runtime = PlayerRt.Weapons[Slot];
			FDTWeaponInstance& Instance = Player.Weapons[Slot];
			Runtime.ChainTriggerReadyIn -= Context.StepSeconds;
			StepMines(Context, Bullets, Owner, Slot, Runtime);
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
	WeaponsHitsEnd = View.Hits.Num();
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

	// 1. Mirror the pool's own float ops (FDTBulletPool::Step): life, move, expiry, bounds.
	for (int32 Index = 0; Index < Rounds.Num();)
	{
		FRound& Round = Rounds[Index];
		Round.LifeLeft -= Dt;
		Round.Position += Round.Velocity * Dt;
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
		}
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
			Impact.bSpent = --Round.PierceLeft <= 0;
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

	TArray<FImpact> Batch = MoveTemp(Impacts);
	Impacts.Reset();
	for (const FImpact& Impact : Batch)
	{
		ReactToImpact(Context, Bullets, Impact);
	}

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
	case EDTStatus::Burn:
		Apply.Magnitude = DTConst::STATUS_BURN_DPS_RATIO * HitDamage; // keepers.md R-KEE-11
		Apply.StackBonus = FMath::RoundToInt(Params.Get(DTFlagParam::BurnStacksMax));
		break;
	case EDTStatus::Chill:
		Apply.Magnitude = 1.0f + Params.Get(DTFlagParam::ChillStacksOnPull); // keepers.md R-KEE-12
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
	if (Shot.Speed <= 0.0f)
	{
		// A carrier with no round speed (arcs, mines) emits children of the shared child kind.
		Shot.Speed = static_cast<float>(DTConst::WEAPON_CHILD_SPEED_UU_S);
		Shot.Radius = static_cast<float>(DTConst::WEAPON_CHILD_RADIUS_UU);
		Shot.Life = static_cast<float>(DTConst::WEAPON_CHILD_RANGE_UU) / Shot.Speed;
	}
	const float Clear = Impact.EnemySerial != 0 ? Impact.EnemyRadius + Shot.Radius : 0.0f;
	const FVector2f Forward = SafeDir(Impact.Dir, FVector2f(1.0f, 0.0f));
	for (int32 Child = 0; Child < Children; ++Child)
	{
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

	// Reactions spawn rounds and hits but never recompose: the spec is fixed for the step.
	const TArray<FDTResolvedFlag>& Flags = Spec.Flags;
	for (const FDTResolvedFlag& Flag : Flags)
	{
		const FDTFlagParams& Params = Flag.Entry.Params;
		const FName On = Params.GetWord(DTFlagParam::On);
		switch (Flag.Entry.Flag)
		{
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
			if (Flag.IsNative() && bPlace)
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
			if (Params.GetWord(DTFlagParam::Target) == WordOrigin)
			{
				// Mod.Backwash: the hit enemy is drawn toward the round's origin.
				if ((Impact.Kind == EImpact::RoundHit || Impact.Kind == EImpact::ArcHit) && HitEnemy != INDEX_NONE)
				{
					const FVector2f To = Impact.Origin - View.EnemyPositions[HitEnemy];
					const float Dist = To.Size();
					if (Dist > KINDA_SMALL_NUMBER)
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
			const float Radius = Params.Get(DTFlagParam::RadiusUu);
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
				if (Dist > Radius || Dist <= KINDA_SMALL_NUMBER)
				{
					continue;
				}
				FDTDisplacement& Move = View.Displacements.AddDefaulted_GetRef();
				Move.EnemyIndex = Enemy;
				Move.EnemySerial = View.EnemySerials[Enemy];
				Move.Delta = To / Dist * FMath::Min(Distance, Dist);
				Move.OwnerId = Impact.OwnerId;
				for (const FDTResolvedFlag& Status : Flags)
				{
					if (Status.Entry.Flag == DTCat::EFlag::StatusApply && Status.Entry.Params.GetWord(DTFlagParam::On) == WordPulled)
					{
						ApplyStatus(Context, Status.Entry.Params, FDTEnemyHandle{ Enemy, Move.EnemySerial }, Impact.HitDamage, Impact.OwnerId, Status.Entry.SourceId);
					}
				}
			}
			break;
		}
		case DTCat::EFlag::StatusApply:
		{
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
			TArray<uint32, TInlineAllocator<8>> Visited;
			Visited.Add(Impact.EnemySerial);
			FVector2f From = Impact.EnemyPosition;
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
				if (Best == INDEX_NONE || !TryEmit(*Runtime))
				{
					break;
				}
				Visited.Add(View.EnemySerials[Best]);
				FDTHit& Out = View.Hits.AddDefaulted_GetRef();
				Out.EnemyIndex = Best;
				Out.EnemySerial = View.EnemySerials[Best];
				Out.OwnerId = Impact.OwnerId;
				Out.Damage = Damage;
				Out.Position = View.EnemyPositions[Best];
				Out.SourceId = Flag.Entry.SourceId;
				Out.WeaponSlot = Impact.Slot;
				for (const FDTResolvedFlag& Status : Flags)
				{
					if (Status.Entry.Flag == DTCat::EFlag::StatusApply && Status.Entry.Params.GetWord(DTFlagParam::On) == WordChainHit)
					{
						ApplyStatus(Context, Status.Entry.Params, FDTEnemyHandle{ Best, Out.EnemySerial }, Damage, Impact.OwnerId, Status.Entry.SourceId);
					}
				}
				From = Out.Position;
			}
			break;
		}
		case DTCat::EFlag::Echo:
		{
			const bool bFires = (Flag.IsNative() && (Impact.Kind == EImpact::RoundHit || Impact.Kind == EImpact::ArcSweep)) ||
				Impact.Kind == EImpact::Detonation; // Mine: native or "the detonation repeats at the same point"
			if (!bFires)
			{
				break;
			}
			const int32 Count = FMath::Max(1, FMath::RoundToInt(Params.Get(DTFlagParam::Count, 1.0f)));
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
			}
			break;
		}
		case DTCat::EFlag::Ricochet:
		{
			if (!Flag.IsNative() || Impact.Kind != EImpact::RoundHit || !Impact.bSpent || Impact.BouncesLeft <= 0)
			{
				break;
			}
			int32 Next = INDEX_NONE;
			if (!FindNearest(View, Impact.Position, Params.Get(DTFlagParam::SeekRangeUu), Impact.EnemySerial, Next))
			{
				break;
			}
			FShot Bounce = MakeShot(Impact.OwnerId, Impact.Slot, *Runtime, Flag.Entry.SourceId, Impact.Position);
			Bounce.Damage = Impact.BaseDamage;
			Bounce.Pierce = 1;
			Bounce.bTrack = true;
			Bounce.Generation = Impact.Generation;
			Bounce.Bounces = Impact.BouncesLeft - 1;
			const FVector2f Dir = SafeDir(View.EnemyPositions[Next] - Impact.Position, Impact.Dir);
			const FVector2f From = Impact.EnemySerial != 0 ? Impact.EnemyPosition + Dir * (Impact.EnemyRadius + Bounce.Radius) : Impact.Position;
			SpawnRound(Context, Bullets, *Runtime, Bounce, From, Dir);
			break;
		}
		default:
			break; // Seek, Hunter, Nova, Charge, VolleyOnDash, SpeedScale, Push, ConvertOrbit native: no S1 source
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
		V.Path = PathOf(Arc.OwnerId);
	}
}
