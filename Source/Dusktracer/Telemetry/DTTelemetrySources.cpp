#include "Telemetry/DTTelemetrySources.h"

TArray<FString>& FDTTelemetrySources::Names()
{
	static TArray<FString> Table = { TEXT("Unknown"), TEXT("Stress") };
	return Table;
}

uint16 FDTTelemetrySources::FindOrAdd(const FString& Name)
{
	TArray<FString>& Table = Names();
	const int32 Found = Table.IndexOfByKey(Name);
	if (Found != INDEX_NONE)
	{
		return static_cast<uint16>(Found);
	}
	check(Table.Num() < MAX_uint16);
	return static_cast<uint16>(Table.Add(Name));
}

const FString& FDTTelemetrySources::GetName(uint16 Id)
{
	const TArray<FString>& Table = Names();
	return Table.IsValidIndex(Id) ? Table[Id] : Table[Unknown];
}

int32 FDTTelemetrySources::Num()
{
	return Names().Num();
}
