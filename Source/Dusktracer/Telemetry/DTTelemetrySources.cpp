#include "Telemetry/DTTelemetrySources.h"
#include "Misc/ScopeRWLock.h"

namespace DTTelemetrySourcesLocal
{
	struct FTable
	{
		FRWLock Lock;
		TArray<FString> Names = { TEXT("Unknown"), TEXT("Stress") };
	};

	static FTable& Table()
	{
		static FTable Instance; // thread-safe static init
		return Instance;
	}

	/** Prefix + Name, after dropping any of the accepted prefixes Name already carries. */
	static FString Prefixed(const TCHAR* Prefix, FName Id, std::initializer_list<const TCHAR*> Accepted)
	{
		FString Name = Id.ToString();
		for (const TCHAR* Head : Accepted)
		{
			if (Name.StartsWith(Head, ESearchCase::CaseSensitive))
			{
				Name.RightChopInline(FCString::Strlen(Head));
				break;
			}
		}
		return FString(Prefix) + Name;
	}
}

uint16 FDTTelemetrySources::FindOrAdd(const FString& Name)
{
	DTTelemetrySourcesLocal::FTable& T = DTTelemetrySourcesLocal::Table();
	{
		FReadScopeLock Read(T.Lock);
		const int32 Found = T.Names.IndexOfByKey(Name);
		if (Found != INDEX_NONE)
		{
			return static_cast<uint16>(Found);
		}
	}
	FWriteScopeLock Write(T.Lock);
	const int32 Found = T.Names.IndexOfByKey(Name); // another thread may have added it in between
	if (Found != INDEX_NONE)
	{
		return static_cast<uint16>(Found);
	}
	check(T.Names.Num() < MAX_uint16);
	return static_cast<uint16>(T.Names.Add(Name));
}

uint16 FDTTelemetrySources::RegisterWeapon(FName WeaponId)
{
	return FindOrAdd(DTTelemetrySourcesLocal::Prefixed(TEXT("Weapon."), WeaponId, { TEXT("Weapon.") }));
}

uint16 FDTTelemetrySources::RegisterModifier(FName ModifierId)
{
	return FindOrAdd(DTTelemetrySourcesLocal::Prefixed(TEXT("Modifier."), ModifierId, { TEXT("Modifier."), TEXT("Mod.") }));
}

uint16 FDTTelemetrySources::RegisterKeeper(EDTPath Path)
{
	if (Path == EDTPath::None || Path == EDTPath::Count)
	{
		return Unknown;
	}
	return FindOrAdd(FString(TEXT("Keeper.")) + PathName(Path));
}

FString FDTTelemetrySources::GetName(uint16 Id)
{
	DTTelemetrySourcesLocal::FTable& T = DTTelemetrySourcesLocal::Table();
	FReadScopeLock Read(T.Lock);
	return T.Names.IsValidIndex(Id) ? T.Names[Id] : T.Names[Unknown];
}

int32 FDTTelemetrySources::Num()
{
	DTTelemetrySourcesLocal::FTable& T = DTTelemetrySourcesLocal::Table();
	FReadScopeLock Read(T.Lock);
	return T.Names.Num();
}

const TCHAR* FDTTelemetrySources::PathName(EDTPath Path)
{
	switch (Path)
	{
	case EDTPath::Storm: return TEXT("Storm");
	case EDTPath::Forge: return TEXT("Forge");
	case EDTPath::Tide: return TEXT("Tide");
	case EDTPath::Shade: return TEXT("Shade");
	case EDTPath::Radiance: return TEXT("Radiance");
	case EDTPath::Quicksilver: return TEXT("Quicksilver");
	default: return TEXT("None");
	}
}
