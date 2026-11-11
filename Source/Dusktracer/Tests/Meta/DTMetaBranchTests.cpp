// Branches of the save's rules that the first tests stopped short of (02_Systems/meta.md R-MET-09, R-MET-11, R-MET-12; review #23, #108, #126).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Meta; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSaveEnvelope.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Meta/DTMetaProfile.h"
#include "Meta/DTMetaSession.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTMetaBranchTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	// The payload ends with the two fates and the count of FateSeen, four bytes each, low byte first (DTMeta::Serialize).
	constexpr int32 FieldBytes = 4;
	constexpr int32 TideFromEnd = 3 * FieldBytes;
	constexpr int32 ShadeFromEnd = 2 * FieldBytes;

	/** Every file a test path could leave behind is gone before it starts. */
	static FString TempSave(const TCHAR* Name)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("MetaBranchTests")));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, Name);
		TArray<FString> Old;
		IFileManager::Get().FindFiles(Old, *(Path + TEXT("*")), true, false);
		for (const FString& File : Old)
		{
			IFileManager::Get().Delete(*FPaths::Combine(Dir, File), false, true, true);
		}
		return Path;
	}

	static bool WriteFile(const FString& Path, const TArray<uint8>& Bytes)
	{
		return FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	static TArray<uint8> ReadFile(const FString& Path)
	{
		TArray<uint8> Bytes;
		FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent);
		return Bytes;
	}

	/** A payload of an empty profile with one fate field set to Value. */
	static TArray<uint8> WithFate(int32 FromEnd, int32 Value)
	{
		TArray<uint8> Payload = DTMeta::Serialize(FDTMetaProfile());
		for (int32 Byte = 0; Byte < FieldBytes; ++Byte)
		{
			Payload[Payload.Num() - FromEnd + Byte] = static_cast<uint8>(static_cast<uint32>(Value) >> (8 * Byte));
		}
		return Payload;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetFatesInRange, "Dusktracer.Meta.FatesInRange", DTMetaBranchTest::Flags) // T-MET-09, "fates in range"
bool FDTMetFatesInRange::RunTest(const FString& Parameters)
{
	using namespace DTMetaBranchTest;
	FDTMetaProfile Read;
	// The offsets are the payload's: each fate's own value has to come back from where this test writes.
	TestTrue(TEXT("Returned reads back"), DTMeta::Deserialize(WithFate(TideFromEnd, static_cast<int32>(EDTFateTide::Returned)), Read) && Read.FateTide == EDTFateTide::Returned && Read.FateShade == EDTFateShade::Bound);
	TestTrue(TEXT("OffLedger reads back"), DTMeta::Deserialize(WithFate(ShadeFromEnd, static_cast<int32>(EDTFateShade::OffLedger)), Read) && Read.FateShade == EDTFateShade::OffLedger && Read.FateTide == EDTFateTide::Bound);
	const int32 Past[] = { static_cast<int32>(EDTFateTide::Returned) + 1, -1, MAX_int32 };
	for (const int32 Value : Past)
	{
		TestFalse(FString::Printf(TEXT("a Tide fate of %d does not read"), Value), DTMeta::Deserialize(WithFate(TideFromEnd, Value), Read));
		TestFalse(FString::Printf(TEXT("a Shade fate of %d does not read"), Value), DTMeta::Deserialize(WithFate(ShadeFromEnd, Value), Read));
	}

	// On a file: a good checksum does not save it (R-MET-09), and the file is kept whole (R-MET-12).
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	const FString Path = TempSave(TEXT("Fate.sav"));
	const TArray<uint8> Bytes = FDTSaveEnvelope::Wrap(WithFate(TideFromEnd, static_cast<int32>(EDTFateTide::Returned) + 1));
	TestTrue(TEXT("the file lands"), WriteFile(Path, Bytes));
	FDTMetaSession Session;
	TestEqual(TEXT("a fate out of range is a bad file"), Session.Open(Path), EDTMetaOpen::KeptAside);
	TestTrue(TEXT("kept whole"), ReadFile(Session.AsidePath) == Bytes);
	TestTrue(TEXT("the session starts empty"), DTMeta::IsEmpty(Session.Profile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetAtomicReplace, "Dusktracer.Meta.AtomicReplace", DTMetaBranchTest::Flags) // T-MET-11, past the directory guard
bool FDTMetAtomicReplace::RunTest(const FString& Parameters)
{
	using namespace DTMetaBranchTest;
	const FString Path = TempSave(TEXT("Replace.sav"));
	const FString Temp = Path + TEXT(".tmp");
	const TArray<uint8> Old = { 1, 1, 1, 1 };
	const TArray<uint8> New = { 2, 2, 2, 2, 2, 2 };
	TestTrue(TEXT("a first write"), DTMeta::WriteAtomic(Path, Old));
	TestFalse(TEXT("leaves no temp"), IFileManager::Get().FileExists(*Temp));

#if PLATFORM_WINDOWS
	// The write itself fails here, not the guard before it: the temp is written and flushed, and the replace is refused because a
	// reader holds the old file. That is the step R-MET-11 is about.
	{
		TUniquePtr<IFileHandle> Hold(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Path));
		TestTrue(TEXT("the old file is held"), Hold.IsValid());
		TestFalse(TEXT("a refused replace fails the write"), DTMeta::WriteAtomic(Path, New));
		TestTrue(TEXT("the temp was written in full first"), ReadFile(Temp) == New);
	}
	TestTrue(TEXT("the old file is whole"), ReadFile(Path) == Old);

	// The same through a session: the failure is told, the change stays in memory, and the next write carries it over the leftover temp.
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	const FString SavePath = TempSave(TEXT("ReplaceSession.sav"));
	FDTMetaSession Session;
	Session.Open(SavePath);
	Session.Profile.Currency = 1;
	TestTrue(TEXT("a session's first write"), Session.Write());
	const TArray<uint8> Before = ReadFile(SavePath);
	Session.Profile.Currency = 2;
	{
		TUniquePtr<IFileHandle> Hold(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*SavePath));
		TestFalse(TEXT("the session's write fails"), Session.Write());
	}
	TestTrue(TEXT("and says a change is not on disk"), Session.bUnsaved);
	TestTrue(TEXT("the file is the old one, byte for byte"), ReadFile(SavePath) == Before);
	TestTrue(TEXT("a temp is left beside it"), IFileManager::Get().FileExists(*(SavePath + TEXT(".tmp"))));
	FDTMetaSession Reader;
	TestTrue(TEXT("which a reader ignores"), Reader.Open(SavePath) == EDTMetaOpen::Read && Reader.Profile.Currency == 1);
	TestTrue(TEXT("the next write lands"), Session.Write());
	TestFalse(TEXT("nothing is left beside the file"), IFileManager::Get().FileExists(*(SavePath + TEXT(".tmp"))));
	TestTrue(TEXT("with the change that waited"), Reader.Open(SavePath) == EDTMetaOpen::Read && Reader.Profile.Currency == 2 && !Session.bUnsaved);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetBadFileBranches, "Dusktracer.Meta.BadFileBranches", DTMetaBranchTest::Flags) // T-MET-12: a failed migration, a file that cannot be read
bool FDTMetBadFileBranches::RunTest(const FString& Parameters)
{
	using namespace DTMetaBranchTest;
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	const int32 Old = DTConst::SAVE_VERSION;
	FDTMetaProfile Saved;
	Saved.Currency = 40;

	// A migration that says it failed: the file goes aside whole, and the step's half-done payload is not what the session holds.
	const FString Failed = TempSave(TEXT("FailedStep.sav"));
	const TArray<uint8> FailedBytes = FDTSaveEnvelope::Wrap(DTMeta::Serialize(Saved), Old);
	TestTrue(TEXT("an older file lands"), WriteFile(Failed, FailedBytes));
	FDTMetaSession Fails;
	Fails.Version = Old + 1;
	bool bRan = false;
	Fails.Envelope.Migrations.Add(Old, [&bRan](TArray<uint8>& Payload)
	{
		bRan = true;
		FDTMetaProfile Half;
		Half.Currency = 999;
		Payload = DTMeta::Serialize(Half);
		return false;
	});
	TestEqual(TEXT("a failed migration keeps the file"), Fails.Open(Failed), EDTMetaOpen::KeptAside);
	TestTrue(TEXT("the step ran"), bRan);
	TestTrue(TEXT("kept whole"), ReadFile(Fails.AsidePath) == FailedBytes);
	TestTrue(TEXT("the session starts empty, not on what the step left"), DTMeta::IsEmpty(Fails.Profile));
	TestFalse(TEXT("the file has moved"), IFileManager::Get().FileExists(*Failed));

#if PLATFORM_WINDOWS
	// A file that cannot be read at all: the session is stuck, writes nothing, sets nothing aside, and the file stays as it is.
	const FString Locked = TempSave(TEXT("Unreadable.sav"));
	const TArray<uint8> Good = FDTSaveEnvelope::Wrap(DTMeta::Serialize(Saved));
	TestTrue(TEXT("a good file lands"), WriteFile(Locked, Good));
	{
		TUniquePtr<IFileHandle> Hold(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Locked, /*bAppend*/ true, /*bAllowRead*/ false));
		TestTrue(TEXT("held with no reader allowed"), Hold.IsValid());
		FDTMetaSession Session;
		TestEqual(TEXT("a file that cannot be read sticks the session"), Session.Open(Locked), EDTMetaOpen::Stuck);
		TestTrue(TEXT("on an empty profile"), DTMeta::IsEmpty(Session.Profile));
		TestTrue(TEXT("with nothing set aside"), Session.AsidePath.IsEmpty() && !IFileManager::Get().FileExists(*(Locked + TEXT(".bad.1"))));
		TestEqual(TEXT("and the hub's line says it cannot be opened (vigil.md R-VIG-21)"), Session.Notice(), EDTMetaNotice::SaveUnreadable);
		Session.Profile.Currency = 7;
		TestFalse(TEXT("and it writes nothing"), Session.Write());
	}
	TestTrue(TEXT("the file is untouched"), ReadFile(Locked) == Good);
	FDTMetaSession Later;
	TestTrue(TEXT("and reads again once it can be"), Later.Open(Locked) == EDTMetaOpen::Read && Later.Profile.Currency == 40);
	TestEqual(TEXT("with nothing to say"), Later.Notice(), EDTMetaNotice::None);
#endif
	return true;
}

#endif
