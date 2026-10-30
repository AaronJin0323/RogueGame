#include "Combat/DTReforge.h"

#include "Combat/DTWeaponCompose.h"

namespace DTReforgeLocal
{
	struct FParam
	{
		FString Name;
		float Value = 0.0f;
	};

	/** The `effect` cells parsed once; the generator has already checked name=number. */
	static const TArray<TArray<FParam>>& Parsed()
	{
		static const TArray<TArray<FParam>> Rows = []
		{
			TArray<TArray<FParam>> Out;
			for (int32 Row = 0; Row < DTCat::NumReforges; ++Row)
			{
				TArray<FParam>& Params = Out.AddDefaulted_GetRef();
				TArray<FString> Pairs;
				FString(DTCat::Reforges[Row].Effect).ParseIntoArray(Pairs, TEXT(";"));
				for (const FString& Pair : Pairs)
				{
					FString Name;
					FString Value;
					if (Pair.Split(TEXT("="), &Name, &Value))
					{
						Params.Add(FParam{ Name, FCString::Atof(*Value) });
					}
				}
			}
			return Out;
		}();
		return Rows;
	}

	static const TCHAR* const PathIds[] = { TEXT("Keeper.Storm"), TEXT("Keeper.Forge"), TEXT("Keeper.Tide"), TEXT("Keeper.Shade"), TEXT("Keeper.Radiance"), TEXT("Keeper.Quicksilver") };
}

int32 DTReforge::FindRow(FName Id)
{
	const FString Text = Id.ToString();
	for (int32 Row = 0; Row < DTCat::NumReforges; ++Row)
	{
		if (Text == DTCat::Reforges[Row].Id)
		{
			return Row;
		}
	}
	return INDEX_NONE;
}

int32 DTReforge::FindEvolve(int32 Weapon)
{
	return Weapon >= 0 && Weapon < DTWeapons::NumWeapons() ? FindRow(FName(DTCat::Weapons[Weapon].Evolution)) : INDEX_NONE;
}

int32 DTReforge::FindPath(EDTPath Path)
{
	const int32 Index = static_cast<int32>(Path);
	if (Index < 0 || Index >= UE_ARRAY_COUNT(DTReforgeLocal::PathIds))
	{
		return INDEX_NONE;
	}
	for (int32 Row = 0; Row < DTCat::NumReforges; ++Row)
	{
		if (!DTCat::Reforges[Row].bEvolve && FCString::Strcmp(DTCat::Reforges[Row].Source, DTReforgeLocal::PathIds[Index]) == 0)
		{
			return Row;
		}
	}
	return INDEX_NONE;
}

FName DTReforge::RowId(int32 Row)
{
	return Row >= 0 && Row < DTCat::NumReforges ? FName(DTCat::Reforges[Row].Id) : NAME_None;
}

float DTReforge::Param(int32 Row, const TCHAR* Name)
{
	if (Row >= 0 && Row < DTCat::NumReforges)
	{
		for (const DTReforgeLocal::FParam& Entry : DTReforgeLocal::Parsed()[Row])
		{
			if (Entry.Name == Name)
			{
				return Entry.Value;
			}
		}
	}
	return 0.0f;
}

EDTEvolve DTReforge::Evolution(int32 Row)
{
	if (Row < 0 || Row >= DTCat::NumReforges)
	{
		return EDTEvolve::None;
	}
	static const TCHAR* const Names[] = { TEXT(""), TEXT("OrbitLaunch"), TEXT("EmbedBlast"), TEXT("ContactBlades"), TEXT("SweepBeam"), TEXT("CarpetLine"),
		TEXT("MoveSpin"), TEXT("Persist"), TEXT("PulseWall"), TEXT("Vortex"), TEXT("Merge") };
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		if (FCString::Strcmp(DTCat::Reforges[Row].Behaviour, Names[Index]) == 0)
		{
			return static_cast<EDTEvolve>(Index);
		}
	}
	return EDTEvolve::None;
}

EDTPathReforge DTReforge::PathReforge(int32 Row)
{
	if (Row < 0 || Row >= DTCat::NumReforges)
	{
		return EDTPathReforge::None;
	}
	static const TCHAR* const Names[] = { TEXT(""), TEXT("Overload"), TEXT("Kiln"), TEXT("Glacier") };
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		if (FCString::Strcmp(DTCat::Reforges[Row].Behaviour, Names[Index]) == 0)
		{
			return static_cast<EDTPathReforge>(Index);
		}
	}
	return EDTPathReforge::None;
}

FDTCapList DTReforge::Caps(int32 Row)
{
	FDTCapList Out;
	if (Row >= 0 && Row < DTCat::NumReforges)
	{
		for (int32 Index = 0; Index < DTCat::Reforges[Row].NumCaps; ++Index)
		{
			Out.Add(DTCat::Reforges[Row].Caps[Index]);
		}
	}
	return Out;
}

FDTCapList DTReforge::DeclaredCaps(const FDTWeaponInstance& Weapon)
{
	return Weapon.Evolve != INDEX_NONE ? Caps(Weapon.Evolve) : DTWeapons::DeclaredCaps(Weapon.Def);
}

int32 DTReforge::PrimarySlot(TConstArrayView<FDTWeaponInstance> Weapons)
{
	const TArray<int32> Ranks = DTWeaponCompose::LoadRanks(Weapons);
	return Ranks.IndexOfByKey(0);
}

void DTReforge::ApplyEvolution(int32 Row, const DTCat::FWeaponLevelRow& Level, FDTWeaponSpec& Spec)
{
}

int32 DTReforge::OwnedPathRow(const FDTPlayerRunState& Player, EDTPathReforge PathKind)
{
	for (const int32 Row : Player.PathReforges)
	{
		if (PathReforge(Row) == PathKind)
		{
			return Row;
		}
	}
	return INDEX_NONE;
}
