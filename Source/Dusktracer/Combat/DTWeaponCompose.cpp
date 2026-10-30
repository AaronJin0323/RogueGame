#include "Combat/DTWeaponCompose.h"

float DTWeaponCompose::LoadFactor(int32 Rank)
{
	switch (Rank)
	{
	case 0: return DTConst::LOAD_FACTOR_1;
	case 1: return DTConst::LOAD_FACTOR_2;
	case 2: return DTConst::LOAD_FACTOR_3;
	default: return DTConst::LOAD_FACTOR_4;
	}
}

TArray<int32> DTWeaponCompose::LoadRanks(TConstArrayView<FDTWeaponInstance> Weapons)
{
	TArray<int32> Order;
	Order.Reserve(Weapons.Num());
	for (int32 Slot = 0; Slot < Weapons.Num(); ++Slot)
	{
		Order.Add(Slot);
	}
	Order.StableSort([&Weapons](int32 A, int32 B)
	{
		if (Weapons[A].Level != Weapons[B].Level)
		{
			return Weapons[A].Level > Weapons[B].Level;
		}
		return Weapons[A].AcquiredOrder < Weapons[B].AcquiredOrder;
	});
	TArray<int32> Ranks;
	Ranks.SetNumZeroed(Weapons.Num());
	for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
	{
		Ranks[Order[Rank]] = Rank;
	}
	return Ranks;
}

TArray<FDTFlagEntry> DTWeaponCompose::StackEntries(TConstArrayView<FDTFlagEntry> Entries)
{
	TArray<FDTFlagEntry> Out;
	for (const FDTFlagEntry& Entry : Entries)
	{
		if (Entry.Flag == DTCat::EFlag::None || Entry.Flag >= DTCat::EFlag::Count)
		{
			continue;
		}
		const FName On = Entry.Params.GetWord(DTFlagParam::On);
		FDTFlagEntry* Same = Out.FindByPredicate([&Entry, On](const FDTFlagEntry& E)
		{
			return E.Flag == Entry.Flag && E.SourceId == Entry.SourceId && E.Params.GetWord(DTFlagParam::On) == On;
		});
		if (Same)
		{
			Same->Params.Merge(Entry.Params);
			Same->Tier = FMath::Max(Same->Tier, Entry.Tier);
		}
		else
		{
			Out.Add(Entry);
		}
	}
	// Per-flag caps after every source is summed (R-MOD-05).
	for (FDTFlagEntry& Entry : Out)
	{
		if (Entry.Params.Has(DTFlagParam::Chance))
		{
			const float Chance = FMath::Clamp(Entry.Params.Get(DTFlagParam::Chance), 0.0f, 1.0f);
			Entry.Params.Replace(DTFlagParam::Chance, Chance);
		}
		if (Entry.Flag == DTCat::EFlag::Split && Entry.Params.Has(DTFlagParam::Gen))
		{
			const float Gen = FMath::Min(Entry.Params.Get(DTFlagParam::Gen), static_cast<float>(DTConst::MOD_SPLIT_GENERATIONS_MAX));
			Entry.Params.Replace(DTFlagParam::Gen, Gen);
		}
	}
	return Out;
}

FDTWeaponSpec DTWeaponCompose::Compose(FDTWeaponInstance& Instance, TConstArrayView<FDTFlagEntry> PlayerFlags,
	const FDTWeaponHeroStats& Stats, int32 LoadRank, TArray<FDTDormancyChange>* OutChanges)
{
	using namespace DTCat;
	FDTWeaponSpec Spec;
	const int32 Weapon = Instance.Def;
	Spec.Weapon = Weapon;
	Spec.Pattern = DTWeapons::GetPattern(Weapon);
	Spec.Target = DTWeapons::GetTarget(Weapon);
	const FWeaponLevelRow* Row = DTWeapons::GetLevelRow(Weapon, Instance.Level);
	if (!Row)
	{
		return Spec;
	}

	// 1. Level row (R-WPN-04).
	Spec.Level = Row->Level;
	Spec.Damage = Row->Damage;
	Spec.Count = Row->Count;
	Spec.Cooldown = Row->Cooldown_s;
	Spec.Radius = Row->Radius_uu;
	Spec.Speed = Row->Speed_uu_s;
	Spec.Range = Row->Range_uu;
	Spec.Pierce = FMath::Max(1, Row->Pierce);
	Spec.AreaRadius = Row->AreaR_uu;
	if (Spec.Pattern == EDTWeaponPattern::MeleeArc)
	{
		Spec.ArcDeg = static_cast<float>(DTConst::WEAPON_ARC_DEG);
		if (Spec.Range <= 0.0f)
		{
			Spec.Range = Spec.Radius; // a sweep reaches as far as it targets
		}
	}

	// Player flags first: the conversion decides the effective caps every other check uses (R-WPN-02, R-WPN-10).
	TArray<FDTFlagEntry> Entries = StackEntries(PlayerFlags);
	const FDTCapList Declared = DTWeapons::DeclaredCaps(Weapon);
	int32 ConversionIndex = INDEX_NONE;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const EFlag Flag = Entries[Index].Flag;
		if ((Flag == EFlag::ConvertMelee || Flag == EFlag::ConvertOrbit) &&
			(ConversionIndex == INDEX_NONE || Entries[Index].Tier > Entries[ConversionIndex].Tier))
		{
			ConversionIndex = Index;
		}
	}
	EFlag NativeConversion = EFlag::None;
	if (ConversionIndex != INDEX_NONE)
	{
		const FDTFlagEntry& Conv = Entries[ConversionIndex];
		Instance.Conversion = static_cast<uint8>(Conv.Flag);
		if (DTWeapons::ResolveFlag(Declared, Conv.Flag).Compat == ECompat::Native && Conv.Flag == EFlag::ConvertMelee)
		{
			NativeConversion = Conv.Flag;
			const int32 Rounds = FMath::RoundToInt(Conv.Params.Get(DTFlagParam::Rounds));
			Spec.ConvertRounds = Rounds > 0 ? FMath::Min(Rounds, Spec.Count) : Spec.Count;
			Spec.bConvertsEveryRound = Spec.ConvertRounds >= Spec.Count;
			Spec.ConvertArcDeg = Conv.Params.Get(DTFlagParam::ArcDeg, static_cast<float>(DTConst::WEAPON_ARC_DEG));
			Spec.ConvertReach = Conv.Params.Get(DTFlagParam::RadiusUu, Spec.Range) * (1.0f + Conv.Params.Get(DTFlagParam::RadiusScale));
			Spec.ConvertDamageFactor = DTConst::WEAPON_CONVERT_MELEE_DAMAGE_RATIO * Conv.Params.Get(DTFlagParam::DmgMult, 1.0f);
		}
		// Conversions never stack: only the winner stays (R-WPN-10).
		const FDTFlagEntry Winner = Conv;
		Entries.RemoveAll([](const FDTFlagEntry& E) { return E.Flag == EFlag::ConvertMelee || E.Flag == EFlag::ConvertOrbit; });
		Entries.Add(Winner);
	}
	else
	{
		Instance.Conversion = 0xFF;
	}
	Spec.Conversion = NativeConversion;
	Spec.EffectiveCaps = DTWeapons::EffectiveCaps(Weapon, NativeConversion, Spec.bConvertsEveryRound);

	// 2. Modifiers: re-checked on the effective caps; an incompatible one is dormant, never removed (R-MOD-08).
	TArray<FDTFlagEntry> ModEntries;
	for (FDTWeaponInstance::FSocket& Socket : Instance.Sockets)
	{
		const int32 Mod = DTWeapons::FindModifier(Socket.ModId);
		const bool bDormant = !DTModCompat::IsCompatible(Spec.EffectiveCaps, Mod);
		if (bDormant != Socket.bDormant)
		{
			Socket.bDormant = bDormant;
			if (OutChanges)
			{
				OutChanges->Add(FDTDormancyChange{ Socket.ModId, bDormant });
			}
		}
		if (bDormant)
		{
			continue;
		}
		const uint16 Source = DTWeapons::ModifierSource(Mod);
		const FString Name = DTWeapons::ShortName(DTCat::Modifiers[Mod].Id);
		if (Name == TEXT("Pierce"))
		{
			Spec.Pierce += FMath::RoundToInt(DTWeapons::ModifierParam(Mod, TEXT("pierce_add")));
		}
		else if (Name == TEXT("Mirror"))
		{
			Spec.MirrorSource = Source;
			Spec.MirrorDamageRatio = DTWeapons::ModifierParam(Mod, TEXT("copy_damage_ratio"));
		}
		else if (Name == TEXT("Fork"))
		{
			Spec.ForkSource = Source;
			Spec.ForkRounds += FMath::RoundToInt(DTWeapons::ModifierParam(Mod, TEXT("extra_rounds")));
			Spec.ForkAngleDeg = FMath::Max(Spec.ForkAngleDeg, DTWeapons::ModifierParam(Mod, TEXT("angle_deg")));
		}
		else if (Name == TEXT("ChainTrigger"))
		{
			Spec.ChainTriggerSource = Source;
			Spec.ChainTriggerIntervalS = FMath::Max(Spec.ChainTriggerIntervalS, DTWeapons::ModifierParam(Mod, TEXT("min_interval_s")));
		}
		FDTFlagEntry ModEntry;
		if (DTWeapons::ModifierFlagEntry(Mod, Source, ModEntry))
		{
			ModEntries.Add(ModEntry);
		}
	}

	// 3. Flags: modifiers and Keepers feed one accumulator; each entry resolves on the effective caps (R-WPN-08).
	Entries.Append(ModEntries);
	Entries = StackEntries(Entries);
	for (const FDTFlagEntry& Entry : Entries)
	{
		FDTResolvedFlag& Resolved = Spec.Flags.AddDefaulted_GetRef();
		Resolved.Entry = Entry;
		Resolved.Resolution = DTWeapons::ResolveFlag(Spec.EffectiveCaps, Entry.Flag);
		// Capability fallbacks that reshape the composed numbers.
		if (Entry.Flag == EFlag::ConvertMelee && Resolved.IsFallback())
		{
			const float Scale = 1.0f + Entry.Params.Get(DTFlagParam::RadiusScale);
			switch (Resolved.Resolution.Cap)
			{
			case ECap::Melee: // +arc size
				Spec.ArcDeg = FMath::Max(Spec.ArcDeg, Entry.Params.Get(DTFlagParam::ArcDeg));
				Spec.Radius *= Scale;
				Spec.Range *= Scale;
				break;
			case ECap::Mine: // +blast area
				Spec.AreaRadius *= Scale;
				break;
			case ECap::Orbit: // +drone contact area
				Spec.Radius *= Scale;
				break;
			default:
				break;
			}
		}
	}

	// 4. Hero stats; crit chance clamps at the cap whatever its sources (R-MOD-05).
	Spec.Damage *= Stats.DamageMult;
	Spec.Cooldown *= Stats.CooldownMult;
	Spec.CritChance = FMath::Clamp(Stats.CritChance, 0.0f, DTConst::MOD_CRIT_CHANCE_CAP);
	Spec.CritMult = Stats.CritMult;

	// 5. Load factor by rank (R-WPN-03).
	Spec.LoadRank = LoadRank;
	Spec.LoadFactor = LoadFactor(LoadRank);
	Spec.Damage *= Spec.LoadFactor;

	Spec.Life = Spec.Speed > 0.0f ? Spec.Range / Spec.Speed : 0.0f;
	return Spec;
}
