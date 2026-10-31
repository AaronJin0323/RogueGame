#include "Meta/DTDiscovery.h"
#include "Core/DTConstantsGen.h"

int64 DTDiscovery::Value(const FDTRunTally& Tally, const TCHAR* Counter)
{
	const FString Key(Counter);
	if (Key == TEXT("Seconds"))
	{
		return Tally.Steps / DTConst::SIM_FIXED_HZ;
	}
	if (Key == TEXT("Kills"))
	{
		return Tally.Kills;
	}
	if (Key == TEXT("Elites"))
	{
		return Tally.Elites;
	}
	if (Key == TEXT("BossKills"))
	{
		return Tally.bBossKilled ? 1 : 0;
	}
	const int64* Found = Tally.Counters.Find(FName(Counter));
	return Found ? *Found : 0;
}

bool DTDiscovery::IsMet(const DTCat::FDiscoveryRow& Row, const FDTRunTally& Tally)
{
	return Value(Tally, Row.Counter) >= Row.Threshold;
}

TArray<FName> DTDiscovery::MetBy(const FDTRunTally& Tally)
{
	TArray<FName> Met;
	for (const DTCat::FDiscoveryRow& Row : DTCat::Discoveries)
	{
		if (IsMet(Row, Tally))
		{
			Met.Add(FName(Row.Id));
		}
	}
	return Met;
}

FName DTDiscovery::UnlockKey(const TCHAR* Id)
{
	return FName(FString::Printf(TEXT("Discovery.%s"), Id));
}

void DTDiscovery::Apply(const FDTRunTally& Tally, FDTMetaProfile& Profile)
{
	for (const DTCat::FDiscoveryRow& Row : DTCat::Discoveries)
	{
		if (!IsMet(Row, Tally))
		{
			continue;
		}
		Profile.Discoveries.Add(FName(Row.Id));
		Profile.Unlocked.Add(UnlockKey(Row.Id));
		for (const TCHAR* Target : Row.Targets)
		{
			if (Target[0] != TEXT('\0'))
			{
				Profile.Unlocked.Add(FName(Target));
			}
		}
	}
}
