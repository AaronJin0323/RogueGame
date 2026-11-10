// One change to the save at a time (02_Systems/meta.md R-MET-11, T-MET-49, D-042): a second session's change forced inside
// the first one's reread-to-write window, which two game processes reach only by timing (Tools/TwoWindowsCheck.ps1 -Race).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Meta.SaveWindow; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "HAL/FileManager.h"
#include "Meta/DTMetaProfile.h"
#include "Meta/DTMetaSession.h"
#include "Meta/DTTree.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTMetaLockTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	const FName LockNode(TEXT("Trunk.Reroll"));

	static FString TempSave(const TCHAR* Name)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("MetaLockTests")));
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

	static FDTRunTally Minutes(int32 Count)
	{
		FDTRunTally T;
		T.Steps = static_cast<int64>(Count) * 60 * DTConst::SIM_FIXED_HZ;
		return T;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetSaveWindow, "Dusktracer.Meta.SaveWindow", DTMetaLockTest::Flags) // T-MET-49
bool FDTMetSaveWindow::RunTest(const FString& Parameters)
{
	using namespace DTMetaLockTest;
	// The bank hook runs after the first session has reread the file and before it writes: the inside of its window.
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);

	// A bank inside a bank.
	{
		const FString Path = TempSave(TEXT("BankInBank.sav"));
		FDTMetaSession First;
		FDTMetaSession Second;
		TestEqual(TEXT("one window opens a fresh save"), First.Open(Path), EDTMetaOpen::Fresh);
		TestEqual(TEXT("so does the other"), Second.Open(Path), EDTMetaOpen::Fresh);
		FDTRunAward B;
		const FDelegateHandle Inside = First.OnBank.AddLambda([&Second, &B](const FDTRunTally&, FDTMetaProfile&)
		{
			B = Second.Bank(Minutes(2), EDTRunEnd::Death);
		});
		const FDTRunAward A = First.Bank(Minutes(3), EDTRunEnd::Death);
		First.OnBank.Remove(Inside);
		TestTrue(TEXT("both banks ran"), A.bBanked && B.bBanked && A.Retained > 0 && B.Retained > 0);
		// The second window's next save point: whatever its first bank could not write must be in the file by now.
		const FDTRunAward C = Second.Bank(Minutes(1), EDTRunEnd::Left);
		FDTMetaSession Reader;
		TestEqual(TEXT("the file reads"), Reader.Open(Path), EDTMetaOpen::Read);
		TestEqual(TEXT("the file holds all three runs"), Reader.Profile.Runs, 3);
		TestEqual(TEXT("and all three awards"), Reader.Profile.Currency, A.Retained + B.Retained + C.Retained);
		TestEqual(TEXT("and both deaths"), Reader.Profile.DeathsSinceWin, 2);
		TestEqual(TEXT("the last bank reads the whole balance"), C.Balance, A.Retained + B.Retained + C.Retained);
	}

	// A purchase inside a bank: it is in the file with its price taken, or it was refused and nothing was spent.
	{
		const FString Path = TempSave(TEXT("BuyInBank.sav"));
		const int64 Price = DTCat::TreeNodes[DTTree::FindNode(LockNode)].Cost;
		FDTMetaSession Second;
		Second.Open(Path);
		Second.Profile.Currency = Price;
		TestTrue(TEXT("the price is planted"), Second.Write());
		FDTMetaSession First;
		TestEqual(TEXT("the other window reads it"), First.Open(Path), EDTMetaOpen::Read);
		EDTTreeBuy Result = EDTTreeBuy::UnknownNode;
		const FDelegateHandle Inside = First.OnBank.AddLambda([&Second, &Result](const FDTRunTally&, FDTMetaProfile&)
		{
			Result = DTTree::Buy(Second, LockNode);
		});
		const FDTRunAward A = First.Bank(Minutes(3), EDTRunEnd::Left);
		First.OnBank.Remove(Inside);
		FDTMetaSession Reader;
		TestEqual(TEXT("the file reads"), Reader.Open(Path), EDTMetaOpen::Read);
		const int32 Rank = Reader.Profile.TreeRanks.FindRef(LockNode);
		TestEqual(TEXT("the bank is in the file"), Reader.Profile.Runs, 1);
		TestEqual(TEXT("a purchase that said bought is in the file, and one that is not there said so"), Rank, Result == EDTTreeBuy::Ok ? 1 : 0);
		TestEqual(TEXT("the price was taken once for a rank, and not at all for none"), Reader.Profile.Currency, Price + A.Retained - Rank * Price);
		TestEqual(TEXT("the buyer's own balance is what it was told"), Second.Profile.Currency, Result == EDTTreeBuy::Ok ? 0 : Price);
	}
	return true;
}

#endif
