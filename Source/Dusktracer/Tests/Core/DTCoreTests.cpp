// Day-0 invariant tests (07_Technical §2).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Tec; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTFixedStep.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Core/DTSaveEnvelope.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTCoreTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static TArray<int32> Draw(FRandomStream& Stream, int32 Count)
	{
		TArray<int32> Out;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Out.Add(Stream.RandHelper(1 << 30));
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTecRunStatePerPlayer, "Dusktracer.Tec.RunState.PerPlayer", DTCoreTest::Flags) // T-TEC-01
bool FDTTecRunStatePerPlayer::RunTest(const FString& Parameters)
{
	FDTRunState Run;
	Run.Start(1234, 2);
	TestEqual(TEXT("two players"), Run.GetNumPlayers(), 2);

	TestTrue(TEXT("kill routes"), Run.ApplyEvent({ EDTGameEventKind::Kill, 1, 0 }));
	TestTrue(TEXT("pickup routes"), Run.ApplyEvent({ EDTGameEventKind::Pickup, 0, 7 }));
	TestTrue(TEXT("hit routes"), Run.ApplyEvent({ EDTGameEventKind::Hit, 1, 40 }));

	TestEqual(TEXT("P0 kills untouched by P1's kill"), Run.GetPlayer(0).Kills, 0);
	TestEqual(TEXT("P1 kill counted"), Run.GetPlayer(1).Kills, 1);
	TestEqual(TEXT("P0 xp"), Run.GetPlayer(0).Xp, static_cast<int64>(7));
	TestEqual(TEXT("P1 xp untouched"), Run.GetPlayer(1).Xp, static_cast<int64>(0));
	TestEqual(TEXT("P1 damage"), Run.GetPlayer(1).DamageDealt, static_cast<int64>(40));

	TestFalse(TEXT("unowned event rejected"), Run.ApplyEvent({ EDTGameEventKind::Kill, INDEX_NONE, 0 }));
	TestFalse(TEXT("out-of-range owner rejected"), Run.ApplyEvent({ EDTGameEventKind::Kill, 2, 0 }));
	TestEqual(TEXT("rejected events change nothing"), Run.GetPlayer(0).Kills + Run.GetPlayer(1).Kills, 1);

	FDTRunState Solo;
	Solo.Start(1234, 1);
	TestEqual(TEXT("solo is one player, same path"), Solo.GetNumPlayers(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTecRngStreams, "Dusktracer.Tec.Rng.Streams", DTCoreTest::Flags) // T-TEC-02
bool FDTTecRngStreams::RunTest(const FString& Parameters)
{
	using namespace DTCoreTest;
	FDTRngStreams A(42);
	FDTRngStreams B(42);
	TestEqual(TEXT("same seed, same Offer sequence"), Draw(A.Get(EDTRngStream::Offer), 16), Draw(B.Get(EDTRngStream::Offer), 16));

	// Independence: draining Spawn on one copy must not move its Combat stream.
	FDTRngStreams C(42);
	FDTRngStreams D(42);
	Draw(C.Get(EDTRngStream::Spawn), 1000);
	TestEqual(TEXT("Combat unaffected by Spawn draws"), Draw(C.Get(EDTRngStream::Combat), 16), Draw(D.Get(EDTRngStream::Combat), 16));

	FDTRngStreams E(43);
	FDTRngStreams F(42);
	TestNotEqual(TEXT("different seeds differ"), Draw(E.Get(EDTRngStream::Spawn), 16), Draw(F.Get(EDTRngStream::Spawn), 16));

	TSet<int32> Seeds;
	for (int32 Index = 0; Index < static_cast<int32>(EDTRngStream::Count); ++Index)
	{
		Seeds.Add(FDTRngStreams::DeriveSeed(42, static_cast<EDTRngStream>(Index)));
	}
	TestEqual(TEXT("every stream has its own seed"), Seeds.Num(), static_cast<int32>(EDTRngStream::Count));

	// Pinned value: the derivation must not change silently (saves, dailies and bug reports depend on it).
	// Expected values computed independently (Python splitmix64), not from this code.
	TestEqual(TEXT("derivation pinned (42, Offer)"), FDTRngStreams::DeriveSeed(42, EDTRngStream::Offer), 1859874986);
	TestEqual(TEXT("derivation pinned (-7, Weather)"), FDTRngStreams::DeriveSeed(-7, EDTRngStream::Weather), 1136073414);
	FDTRngStreams G;
	G.Reset(42);
	TestEqual(TEXT("Reset equals construct"), Draw(G.Get(EDTRngStream::Boss), 8), Draw(FDTRngStreams(42).Get(EDTRngStream::Boss), 8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTecFixedStepAccumulator, "Dusktracer.Tec.FixedStep.Accumulator", DTCoreTest::Flags) // T-TEC-03
bool FDTTecFixedStepAccumulator::RunTest(const FString& Parameters)
{
	const double Step = 1.0 / DTConst::SIM_FIXED_HZ;

	FDTFixedStep Exact;
	int32 Steps = 0;
	for (int32 Frame = 0; Frame < DTConst::SIM_FIXED_HZ; ++Frame)
	{
		Steps += Exact.Advance(Step);
	}
	TestEqual(TEXT("one second of exact frames = SIM_FIXED_HZ steps"), Steps, DTConst::SIM_FIXED_HZ);
	TestTrue(TEXT("no drift after exact frames"), Exact.GetAccumulator() < Step * 1e-3);

	FDTFixedStep Slow;
	TestEqual(TEXT("half a step: no step yet"), Slow.Advance(Step * 0.5), 0);
	TestTrue(TEXT("alpha is the carried half"), FMath::IsNearlyEqual(Slow.GetAlpha(), 0.5f, 1e-4f));
	TestEqual(TEXT("second half completes a step"), Slow.Advance(Step * 0.5), 1);

	FDTFixedStep Fast;
	TestEqual(TEXT("1.5 steps of time = 1 step"), Fast.Advance(Step * 1.5), 1);
	TestEqual(TEXT("carried half + 1.5 = 2 steps"), Fast.Advance(Step * 1.5), 2);

	FDTFixedStep Hitch;
	TestEqual(TEXT("a hitch is capped at MAX_SUBSTEPS"), Hitch.Advance(Step * 10.25), DTConst::SIM_MAX_SUBSTEPS);
	TestEqual(TEXT("excess steps are dropped and counted"), Hitch.GetDroppedSteps(), static_cast<int64>(10 - DTConst::SIM_MAX_SUBSTEPS));
	TestTrue(TEXT("only the fraction is carried"), Hitch.GetAccumulator() < Step);

	FDTFixedStep Bad;
	TestEqual(TEXT("negative time = no step"), Bad.Advance(-1.0), 0);
	TestEqual(TEXT("NaN = no step"), Bad.Advance(std::numeric_limits<double>::quiet_NaN()), 0);
	TestEqual(TEXT("bad input leaves nothing carried"), Bad.GetAccumulator(), 0.0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTecSaveEnvelope, "Dusktracer.Tec.Save.Envelope", DTCoreTest::Flags) // T-TEC-04
bool FDTTecSaveEnvelope::RunTest(const FString& Parameters)
{
	const TArray<uint8> Payload = { 1, 2, 3, 4, 5 };
	FDTSaveEnvelope Env;
	TArray<uint8> Out;
	int32 Loaded = 0;

	TArray<uint8> Bytes = FDTSaveEnvelope::Wrap(Payload);
	TestEqual(TEXT("round trip ok"), Env.Unwrap(Bytes, Out, Loaded), EDTSaveLoadResult::Ok);
	TestEqual(TEXT("round trip payload"), Out, Payload);
	TestEqual(TEXT("written at the current version"), Loaded, DTConst::SAVE_VERSION);

	TArray<uint8> Flipped = Bytes;
	Flipped.Last() ^= 0xFF;
	TestEqual(TEXT("flipped payload byte is corrupt"), Env.Unwrap(Flipped, Out, Loaded), EDTSaveLoadResult::Corrupt);
	TArray<uint8> Truncated = Bytes;
	Truncated.Pop();
	TestEqual(TEXT("truncated file is corrupt"), Env.Unwrap(Truncated, Out, Loaded), EDTSaveLoadResult::Corrupt);
	TestEqual(TEXT("garbage has a bad header"), Env.Unwrap(TArray<uint8>{ 'X', 'Y' }, Out, Loaded), EDTSaveLoadResult::BadHeader);
	TestTrue(TEXT("no payload on failure"), Out.IsEmpty());

	const int32 Current = DTConst::SAVE_VERSION;
	TestEqual(TEXT("newer save is rejected"),
		Env.Unwrap(FDTSaveEnvelope::Wrap(Payload, Current + 1), Out, Loaded, Current), EDTSaveLoadResult::TooNew);

	// Older save, two migration steps: v1 -> v2 appends 6, v2 -> v3 appends 7.
	FDTSaveEnvelope Migrating;
	Migrating.Migrations.Add(1, [](TArray<uint8>& P) { P.Add(6); return true; });
	Migrating.Migrations.Add(2, [](TArray<uint8>& P) { P.Add(7); return true; });
	TestEqual(TEXT("older save migrates"), Migrating.Unwrap(FDTSaveEnvelope::Wrap(Payload, 1), Out, Loaded, 3), EDTSaveLoadResult::Ok);
	TestEqual(TEXT("migrations ran in order"), Out, TArray<uint8>{ 1, 2, 3, 4, 5, 6, 7 });
	TestEqual(TEXT("loaded version reported"), Loaded, 1);

	FDTSaveEnvelope Gap;
	Gap.Migrations.Add(1, [](TArray<uint8>& P) { return true; });
	TestEqual(TEXT("missing step fails"), Gap.Unwrap(FDTSaveEnvelope::Wrap(Payload, 1), Out, Loaded, 3), EDTSaveLoadResult::MigrationMissing);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
