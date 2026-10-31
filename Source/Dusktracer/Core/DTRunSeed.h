// Where a run's seed comes from (07_Technical R-TEC-16): one rule for every launch, a person's or a tool's.
#pragma once

#include "CoreMinimal.h"

/** Why a launch has the seed it has. */
enum class EDTSeedSource : uint8
{
	Tool,     // no -DTSeed on a tool run: 0, the seed the goldens and perf scenes are read on
	Fresh,    // no -DTSeed on a human launch: a new seed
	Explicit, // -DTSeed=<n>: exactly that number
	Rejected, // -DTSeed is not a whole int32: no run starts
};

/** A launch's seed and why it is that number. */
struct FDTRunSeed
{
	/** A fresh seed is never 0 (a tool's default) and never MAX_int32 (what an overflowing launcher saturated to). */
	static constexpr int32 FreshMin = 1;
	static constexpr int32 FreshMax = MAX_int32 - 1;

	/** Process exit code of a launch whose -DTSeed was rejected. */
	static constexpr uint8 RejectedExitCode = 2;

	int32 Seed = 0;
	EDTSeedSource Source = EDTSeedSource::Tool;

	/** True for a whole base-10 number that fits an int32; anything else leaves OutSeed alone. FCString::Atoi would saturate. */
	static bool ParseValue(const FString& Text, int32& OutSeed);

	/** Maps any entropy onto FreshMin..FreshMax. */
	static int32 FreshFromEntropy(uint32 Entropy);

	/** The rule, with nothing read from the process: what this command line means for this kind of run. */
	static FDTRunSeed Resolve(const TCHAR* CommandLine, bool bToolRun, uint32 Entropy);

	/** True when no person is at this process: -unattended or an automation test. */
	static bool IsToolRun();

	/** The rule on this process's command line; a human launch draws here, so ask once per run (UDTSimSubsystem::GetLaunchSeed). */
	static FDTRunSeed ForThisLaunch();
};
