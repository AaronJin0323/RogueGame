#include "Combat/DTWeaponCatalog.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTWeaponCatalogLocal
{
	/** Modifier parameter names that differ from the flag vocabulary they feed (modifiers.csv -> expression.csv). */
	struct FParamAlias { const TCHAR* ModName; const TCHAR* FlagName; };
	static const FParamAlias ParamAliases[] =
	{
		{ TEXT("child_damage_ratio"), TEXT("child_dmg") },
		{ TEXT("dps_ratio"), TEXT("dps") },
		{ TEXT("pull_uu"), TEXT("distance_uu") },
	};

	/** Where a flag-carrying modifier's flag fires (modifiers.md table "Expresses"); words, not numbers. */
	struct FModWords { const TCHAR* Mod; const TCHAR* On; const TCHAR* Pattern; const TCHAR* Target; const TCHAR* Status; };
	static const FModWords ModWords[] =
	{
		{ TEXT("Mod.SplitCore"), TEXT("Hit"), TEXT("Fan"), nullptr, nullptr },
		{ TEXT("Mod.ScatterShell"), TEXT("Expiry"), TEXT("Ring"), nullptr, nullptr },
		{ TEXT("Mod.LingeringField"), TEXT("Impact"), nullptr, nullptr, nullptr },
		{ TEXT("Mod.Backwash"), TEXT("Hit"), nullptr, TEXT("Origin"), nullptr },
		{ TEXT("Mod.Conduit"), TEXT("Hit"), nullptr, nullptr, TEXT("Patron") },
		{ TEXT("Mod.Ricochet"), TEXT("Spent"), nullptr, nullptr, nullptr },
	};

	static bool IdMatches(const TCHAR* CatalogId, FName Id, const TCHAR* Prefix)
	{
		const FString Wanted = Id.ToString();
		if (Wanted.Equals(CatalogId, ESearchCase::IgnoreCase))
		{
			return true;
		}
		return (FString(Prefix) + Wanted).Equals(CatalogId, ESearchCase::IgnoreCase);
	}
}

int32 DTWeapons::NumWeapons()
{
	return UE_ARRAY_COUNT(DTCat::Weapons);
}

int32 DTWeapons::NumModifiers()
{
	return UE_ARRAY_COUNT(DTCat::Modifiers);
}

int32 DTWeapons::FindWeapon(FName Id)
{
	for (int32 Index = 0; Index < NumWeapons(); ++Index)
	{
		if (DTWeaponCatalogLocal::IdMatches(DTCat::Weapons[Index].Id, Id, TEXT("Weapon.")))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

int32 DTWeapons::FindModifier(FName Id)
{
	for (int32 Index = 0; Index < NumModifiers(); ++Index)
	{
		if (DTWeaponCatalogLocal::IdMatches(DTCat::Modifiers[Index].Id, Id, TEXT("Mod.")))
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

FName DTWeapons::WeaponId(int32 Weapon)
{
	return (Weapon >= 0 && Weapon < NumWeapons()) ? FName(DTCat::Weapons[Weapon].Id) : NAME_None;
}

FName DTWeapons::ModifierId(int32 Mod)
{
	return (Mod >= 0 && Mod < NumModifiers()) ? FName(DTCat::Modifiers[Mod].Id) : NAME_None;
}

FString DTWeapons::ShortName(const TCHAR* Id)
{
	FString S(Id);
	int32 Dot = INDEX_NONE;
	return S.FindChar(TEXT('.'), Dot) ? S.Mid(Dot + 1) : S;
}

EDTWeaponPattern DTWeapons::GetPattern(int32 Weapon)
{
	static const TCHAR* Names[] = { TEXT("Aimed"), TEXT("Radial"), TEXT("Orbiter"), TEXT("MeleeArc"), TEXT("Mine"), TEXT("Cone"), TEXT("Line"), TEXT("Lob"), TEXT("Homing"), TEXT("Spiral") };
	if (Weapon >= 0 && Weapon < NumWeapons())
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			if (FCString::Strcmp(Names[Index], DTCat::Weapons[Weapon].Pattern) == 0)
			{
				return static_cast<EDTWeaponPattern>(Index);
			}
		}
	}
	return EDTWeaponPattern::Unknown;
}

EDTWeaponTarget DTWeapons::GetTarget(int32 Weapon)
{
	static const TCHAR* Names[] = { TEXT("Nearest"), TEXT("NearestToDrone"), TEXT("DensestCell"), TEXT("None"), TEXT("DensestLine"), TEXT("RandomAlive") };
	if (Weapon >= 0 && Weapon < NumWeapons())
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			if (FCString::Strcmp(Names[Index], DTCat::Weapons[Weapon].Target) == 0)
			{
				return static_cast<EDTWeaponTarget>(Index);
			}
		}
	}
	return EDTWeaponTarget::Unknown;
}

int32 DTWeapons::GetMaxLevel(int32 Weapon)
{
	return (Weapon >= 0 && Weapon < NumWeapons()) ? FMath::Min(DTCat::Weapons[Weapon].NumLevels, DTConst::WEAPON_LEVEL_MAX) : 0;
}

const DTCat::FWeaponLevelRow* DTWeapons::GetLevelRow(int32 Weapon, int32 Level)
{
	const int32 Max = GetMaxLevel(Weapon);
	if (Max <= 0)
	{
		return nullptr;
	}
	const int32 Clamped = FMath::Clamp(Level, 1, Max);
	return &DTCat::WeaponLevels[DTCat::Weapons[Weapon].FirstLevel + Clamped - 1];
}

FDTCapList DTWeapons::DeclaredCaps(int32 Weapon)
{
	FDTCapList List;
	if (Weapon >= 0 && Weapon < NumWeapons())
	{
		const DTCat::FWeaponRow& Row = DTCat::Weapons[Weapon];
		for (int32 Index = 0; Index < Row.NumCaps; ++Index)
		{
			List.Add(Row.Caps[Index]);
		}
	}
	return List;
}

FDTCapList DTWeapons::EffectiveCaps(int32 Weapon, DTCat::EFlag Conversion, bool bConvertsEveryRound)
{
	const FDTCapList Declared = DeclaredCaps(Weapon);
	DTCat::ECap Added = DTCat::ECap::Count;
	if (Conversion == DTCat::EFlag::ConvertMelee)
	{
		Added = DTCat::ECap::Melee;
	}
	else if (Conversion == DTCat::EFlag::ConvertOrbit)
	{
		Added = DTCat::ECap::Orbit;
	}
	if (Added == DTCat::ECap::Count || Declared.Has(Added))
	{
		return Declared;
	}
	FDTCapList Out;
	if (bConvertsEveryRound)
	{
		// The conversion becomes the carrier: it leads the walk and the original carrier (Projectile) is gone.
		Out.Add(Added);
		for (const DTCat::ECap Cap : Declared.Caps)
		{
			if (Cap != DTCat::ECap::Projectile)
			{
				Out.Add(Cap);
			}
		}
	}
	else
	{
		Out = Declared;
		Out.Add(Added);
	}
	return Out;
}

FDTFlagResolution DTWeapons::ResolveFlag(const FDTCapList& Caps, DTCat::EFlag Flag)
{
	FDTFlagResolution Out;
	if (Flag == DTCat::EFlag::None || Flag >= DTCat::EFlag::Count)
	{
		return Out;
	}
	const DTCat::FFlagCompatRow& Row = DTCat::FlagCompat[static_cast<int32>(Flag)];
	for (const DTCat::ECap Cap : Caps.Caps)
	{
		const DTCat::ECompat Cell = Row.Cells[static_cast<int32>(Cap)];
		if (Cell != DTCat::ECompat::None)
		{
			Out.Compat = Cell;
			Out.Cap = Cap;
			Out.Fallback = Row.Fallback[static_cast<int32>(Cap)];
			return Out;
		}
	}
	return Out;
}

DTCat::EFlag DTWeapons::ModifierFlag(int32 Mod)
{
	return (Mod >= 0 && Mod < NumModifiers()) ? DTCat::Modifiers[Mod].Flag : DTCat::EFlag::None;
}

float DTWeapons::ModifierParam(int32 Mod, const TCHAR* Name)
{
	if (Mod >= 0 && Mod < NumModifiers())
	{
		const DTCat::FModifierRow& Row = DTCat::Modifiers[Mod];
		for (int32 Index = 0; Index < Row.NumParams; ++Index)
		{
			if (FCString::Strcmp(Row.Params[Index].Name, Name) == 0)
			{
				return Row.Params[Index].Value;
			}
		}
	}
	return 0.0f;
}

bool DTWeapons::ModifierFlagEntry(int32 Mod, uint16 SourceId, FDTFlagEntry& OutEntry)
{
	const DTCat::EFlag Flag = ModifierFlag(Mod);
	if (Flag == DTCat::EFlag::None)
	{
		return false;
	}
	const DTCat::FModifierRow& Row = DTCat::Modifiers[Mod];
	OutEntry = FDTFlagEntry();
	OutEntry.Flag = Flag;
	OutEntry.SourceId = SourceId;
	OutEntry.Tier = EDTFlagTier::Modifier;
	for (int32 Index = 0; Index < Row.NumParams; ++Index)
	{
		const TCHAR* Name = Row.Params[Index].Name;
		for (const DTWeaponCatalogLocal::FParamAlias& Alias : DTWeaponCatalogLocal::ParamAliases)
		{
			if (FCString::Strcmp(Alias.ModName, Name) == 0)
			{
				Name = Alias.FlagName;
				break;
			}
		}
		OutEntry.Params.Set(FName(Name), Row.Params[Index].Value);
	}
	for (const DTWeaponCatalogLocal::FModWords& Words : DTWeaponCatalogLocal::ModWords)
	{
		if (FCString::Strcmp(Words.Mod, Row.Id) == 0)
		{
			if (Words.On) { OutEntry.Params.SetWord(DTFlagParam::On, FName(Words.On)); }
			if (Words.Pattern) { OutEntry.Params.SetWord(DTFlagParam::Pattern, FName(Words.Pattern)); }
			if (Words.Target) { OutEntry.Params.SetWord(DTFlagParam::Target, FName(Words.Target)); }
			if (Words.Status) { OutEntry.Params.SetWord(DTFlagParam::Status, FName(Words.Status)); }
		}
	}
	return true;
}

uint16 DTWeapons::WeaponSource(int32 Weapon)
{
	return (Weapon >= 0 && Weapon < NumWeapons()) ? FDTTelemetrySources::FindOrAdd(DTCat::Weapons[Weapon].Id) : FDTTelemetrySources::Unknown;
}

uint16 DTWeapons::ModifierSource(int32 Mod)
{
	if (Mod < 0 || Mod >= NumModifiers())
	{
		return FDTTelemetrySources::Unknown;
	}
	// Mod.<Name> catalog id -> Modifier.<Name> telemetry source (telemetry.md R-TEL-05).
	return FDTTelemetrySources::FindOrAdd(TEXT("Modifier.") + ShortName(DTCat::Modifiers[Mod].Id));
}

uint16 DTWeapons::KeeperSource(EDTPath Path)
{
	static const TCHAR* Names[] = { TEXT("Keeper.Storm"), TEXT("Keeper.Forge"), TEXT("Keeper.Tide"), TEXT("Keeper.Shade"), TEXT("Keeper.Radiance"), TEXT("Keeper.Quicksilver") };
	const int32 Index = static_cast<int32>(Path);
	return (Index >= 0 && Index < UE_ARRAY_COUNT(Names)) ? FDTTelemetrySources::FindOrAdd(Names[Index]) : FDTTelemetrySources::Unknown;
}

bool DTModCompat::IsCompatible(const FDTCapList& Caps, int32 Mod)
{
	if (Mod < 0 || Mod >= DTWeapons::NumModifiers())
	{
		return false;
	}
	const DTCat::FModifierRow& Row = DTCat::Modifiers[Mod];
	if (Row.RequiredCaps != 0 && (Caps.Mask & Row.RequiredCaps) == 0)
	{
		return false;
	}
	if ((Caps.Mask & Row.BlockedCaps) != 0)
	{
		return false;
	}
	if (Row.Flag != DTCat::EFlag::None && DTWeapons::ResolveFlag(Caps, Row.Flag).Compat != DTCat::ECompat::Native)
	{
		return false;
	}
	return true;
}

bool DTModCompat::IsCompatible(int32 Weapon, int32 Mod)
{
	return Weapon >= 0 && Weapon < DTWeapons::NumWeapons() && IsCompatible(DTWeapons::DeclaredCaps(Weapon), Mod);
}

bool DTModCompat::IsCompatible(FName WeaponId, FName ModId)
{
	return IsCompatible(DTWeapons::FindWeapon(WeaponId), DTWeapons::FindModifier(ModId));
}
