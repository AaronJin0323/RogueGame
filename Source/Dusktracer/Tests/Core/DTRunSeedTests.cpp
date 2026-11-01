// The run seed rule (07_Technical R-TEC-16). What the launchers really do is proven live by Tools/SeedCheck.ps1 (T-TEC-16, gate step `seed`).

#include "Misc/AutomationTest.h"
#include "Core/DTRunSeed.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTRunSeedTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr uint32 AnyEntropy = 0x5EED5EEDu;

	static FString Line(const TCHAR* Value)
	{
		return FString::Printf(TEXT("-game -DTSeed=%s -culture=en"), Value);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTRunSeedExplicit, "Dusktracer.Tec.RunSeed.Explicit", DTRunSeedTest::Flags) // T-TEC-16
bool FDTRunSeedExplicit::RunTest(const FString& Parameters)
{
	using namespace DTRunSeedTest;
	struct FRow { const TCHAR* Text; int32 Seed; };
	const FRow Rows[] = {
		{ TEXT("0"), 0 }, { TEXT("1"), 1 }, { TEXT("9107"), 9107 }, { TEXT("2147483646"), 2147483646 },
		{ TEXT("2147483647"), MAX_int32 }, { TEXT("-1"), -1 }, { TEXT("-2147483648"), MIN_int32 },
		{ TEXT("+5"), 5 }, { TEXT("007"), 7 }, { TEXT("\"9107\""), 9107 },
	};
	for (const FRow& Row : Rows)
	{
		for (const bool bToolRun : { false, true })
		{
			const FDTRunSeed Got = FDTRunSeed::Resolve(*Line(Row.Text), bToolRun, AnyEntropy);
			TestEqual(FString::Printf(TEXT("-DTSeed=%s is exact (tool %d)"), Row.Text, bToolRun), Got.Seed, Row.Seed);
			TestTrue(FString::Printf(TEXT("-DTSeed=%s is explicit (tool %d)"), Row.Text, bToolRun), Got.Source == EDTSeedSource::Explicit);
		}
	}
	TestEqual(TEXT("a value at the end of the line is read"), FDTRunSeed::Resolve(TEXT("-game -DTSeed=9107"), false, AnyEntropy).Seed, 9107);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTRunSeedRejected, "Dusktracer.Tec.RunSeed.Rejected", DTRunSeedTest::Flags) // T-TEC-16
bool FDTRunSeedRejected::RunTest(const FString& Parameters)
{
	using namespace DTRunSeedTest;
	// 3276732767 is the old launcher's %RANDOM%%RANDOM% at its highest, the first row just past the top of the range.
	// The last row is a value FParse::Value ends at a separator by default, which would run the launch on the 12 in front of it.
	const TCHAR* const Rows[] = {
		TEXT("2147483648"), TEXT("3276732767"), TEXT("99999999999999999999"), TEXT("-2147483649"),
		TEXT("abc"), TEXT("12abc"), TEXT("1.5"), TEXT("0x10"), TEXT("-"), TEXT("+"), TEXT("%RANDOM%%RANDOM%"),
		TEXT("12,34"), TEXT("12)"), TEXT("12,"), TEXT(",12"), TEXT("\"12,34\""),
	};
	for (const TCHAR* const Row : Rows)
	{
		for (const bool bToolRun : { false, true })
		{
			const FDTRunSeed Got = FDTRunSeed::Resolve(*Line(Row), bToolRun, AnyEntropy);
			TestTrue(FString::Printf(TEXT("-DTSeed=%s is rejected (tool %d)"), Row, bToolRun), Got.Source == EDTSeedSource::Rejected);
			TestEqual(FString::Printf(TEXT("-DTSeed=%s leaves no seed (tool %d)"), Row, bToolRun), Got.Seed, 0);
		}
	}
	TestTrue(TEXT("no value at the end of the line is rejected"), FDTRunSeed::Resolve(TEXT("-game -DTSeed="), false, AnyEntropy).Source == EDTSeedSource::Rejected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTRunSeedDefault, "Dusktracer.Tec.RunSeed.Default", DTRunSeedTest::Flags) // T-TEC-16
bool FDTRunSeedDefault::RunTest(const FString& Parameters)
{
	using namespace DTRunSeedTest;
	const TCHAR* const Bare = TEXT("-game -culture=en");
	const FDTRunSeed Tool = FDTRunSeed::Resolve(Bare, /*bToolRun*/ true, AnyEntropy);
	TestEqual(TEXT("a tool with no seed stays on 0"), Tool.Seed, 0);
	TestTrue(TEXT("and says so"), Tool.Source == EDTSeedSource::Tool);

	const FDTRunSeed Human = FDTRunSeed::Resolve(Bare, /*bToolRun*/ false, AnyEntropy);
	TestEqual(TEXT("a person with no seed gets the draw"), Human.Seed, FDTRunSeed::FreshFromEntropy(AnyEntropy));
	TestTrue(TEXT("and it is fresh"), Human.Source == EDTSeedSource::Fresh);
	TestNotEqual(TEXT("another entropy, another seed"), FDTRunSeed::Resolve(Bare, false, AnyEntropy + 1).Seed, Human.Seed);

	// Tools/WinCheck.ps1 names its map this way: the option is the map's, for ADTGameMode::ParseSeed, and no -DTSeed.
	const TCHAR* const MapOption = TEXT("/Game/DT/Maps/L_Arena_Test?DTSeed=9100?DTPatron=Storm -game");
	TestTrue(TEXT("a map's option is no -DTSeed for a tool"), FDTRunSeed::Resolve(MapOption, true, AnyEntropy).Source == EDTSeedSource::Tool);
	TestTrue(TEXT("nor for a person"), FDTRunSeed::Resolve(MapOption, false, AnyEntropy).Source == EDTSeedSource::Fresh);

	TestTrue(TEXT("this test is a tool run"), FDTRunSeed::IsToolRun());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTRunSeedFreshRange, "Dusktracer.Tec.RunSeed.FreshRange", DTRunSeedTest::Flags) // T-TEC-16
bool FDTRunSeedFreshRange::RunTest(const FString& Parameters)
{
	const uint32 Top = static_cast<uint32>(FDTRunSeed::FreshMax - FDTRunSeed::FreshMin);
	const uint32 Edges[] = { 0u, 1u, Top - 1u, Top, Top + 1u, Top + 2u, static_cast<uint32>(MAX_int32), 0x80000000u, MAX_uint32 - 1u, MAX_uint32 };
	for (const uint32 Entropy : Edges)
	{
		const int32 Seed = FDTRunSeed::FreshFromEntropy(Entropy);
		TestTrue(FString::Printf(TEXT("entropy %u gives a positive seed under the saturation value, not %d"), Entropy, Seed),
			Seed >= FDTRunSeed::FreshMin && Seed <= FDTRunSeed::FreshMax);
	}
	TestEqual(TEXT("the first seed is reached"), FDTRunSeed::FreshFromEntropy(0u), FDTRunSeed::FreshMin);
	TestEqual(TEXT("the last seed is reached"), FDTRunSeed::FreshFromEntropy(Top), FDTRunSeed::FreshMax);
	TestTrue(TEXT("0 is out of the fresh range"), FDTRunSeed::FreshMin > 0);
	TestTrue(TEXT("the saturation value is out of the fresh range"), FDTRunSeed::FreshMax < MAX_int32);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
