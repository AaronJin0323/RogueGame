#include "Core/DTRunSeed.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Guid.h"
#include "Misc/Parse.h"

bool FDTRunSeed::ParseValue(const FString& Text, int32& OutSeed)
{
	const bool bNegative = Text.StartsWith(TEXT("-"));
	const int32 First = bNegative || Text.StartsWith(TEXT("+")) ? 1 : 0;
	if (First == Text.Len())
	{
		return false;
	}
	const int64 Limit = bNegative ? -static_cast<int64>(MIN_int32) : static_cast<int64>(MAX_int32);
	int64 Value = 0;
	for (int32 Index = First; Index < Text.Len(); ++Index)
	{
		const TCHAR Digit = Text[Index];
		if (Digit < TEXT('0') || Digit > TEXT('9'))
		{
			return false;
		}
		Value = Value * 10 + (Digit - TEXT('0'));
		if (Value > Limit)
		{
			return false;
		}
	}
	OutSeed = static_cast<int32>(bNegative ? -Value : Value);
	return true;
}

int32 FDTRunSeed::FreshFromEntropy(uint32 Entropy)
{
	return FreshMin + static_cast<int32>(Entropy % static_cast<uint32>(FreshMax - FreshMin + 1));
}

FDTRunSeed FDTRunSeed::Resolve(const TCHAR* CommandLine, bool bToolRun, uint32 Entropy)
{
	FDTRunSeed Result;
	FString Text;
	// Only whitespace ends the value: by default ',' and ')' end it too, which would run -DTSeed=12,34 on 12.
	// The switch alone: a map on the command line can carry ?DTSeed=, and that seed is the map's (ADTGameMode::ParseSeed).
	if (FParse::Value(CommandLine, TEXT("-DTSeed="), Text, /*bShouldStopOnSeparator*/ false))
	{
		Result.Source = ParseValue(Text, Result.Seed) ? EDTSeedSource::Explicit : EDTSeedSource::Rejected;
	}
	else if (!bToolRun)
	{
		Result.Seed = FreshFromEntropy(Entropy);
		Result.Source = EDTSeedSource::Fresh;
	}
	return Result;
}

bool FDTRunSeed::IsToolRun()
{
	return FApp::IsUnattended();
}

FDTRunSeed FDTRunSeed::ForThisLaunch()
{
	return Resolve(FCommandLine::Get(), IsToolRun(), GetTypeHash(FGuid::NewGuid()));
}
