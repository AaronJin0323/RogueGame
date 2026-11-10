// One change to the save at a time (02_Systems/meta.md R-MET-11, T-MET-49, T-MET-50, D-042): a second session's change forced
// inside the first one's reread-to-write window, which two game processes reach only by timing (Tools/TwoWindowsCheck.ps1 -Race),
// and what a session does while the lock is somebody else's.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Meta.Save; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSaveEnvelope.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Meta/DTMetaProfile.h"
#include "Meta/DTMetaSession.h"
#include "Meta/DTTree.h"
#include "Misc/FileHelper.h"
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
	AddExpectedError(TEXT("Tree:"), EAutomationExpectedErrorFlags::Contains, 0);

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
		TestTrue(TEXT("the bank inside the window found the lock taken and says so"), B.bDeferred && B.Waiting == 1 && Second.bUnsaved);
		TestFalse(TEXT("the bank that held the lock wrote"), A.bDeferred || First.bUnsaved);
		TestEqual(TEXT("the run inside the window is in its session's balance"), B.Balance, B.Retained);
		// The second window's next save point: whatever its first bank could not write must be in the file by now.
		const FDTRunAward C = Second.Bank(Minutes(1), EDTRunEnd::Left);
		FDTMetaSession Reader;
		TestEqual(TEXT("the file reads"), Reader.Open(Path), EDTMetaOpen::Read);
		TestEqual(TEXT("the file holds all three runs"), Reader.Profile.Runs, 3);
		TestEqual(TEXT("and all three awards"), Reader.Profile.Currency, A.Retained + B.Retained + C.Retained);
		TestEqual(TEXT("and both deaths"), Reader.Profile.DeathsSinceWin, 2);
		TestEqual(TEXT("the last bank reads the whole balance"), C.Balance, A.Retained + B.Retained + C.Retained);
		TestFalse(TEXT("nothing is left waiting"), C.bDeferred || Second.bUnsaved || Second.Pending.Num() > 0);
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
		TestEqual(TEXT("the purchase inside the window is refused as busy"), Result, EDTTreeBuy::Busy);
		TestFalse(TEXT("and leaves nothing waiting"), Second.bUnsaved);
		FDTMetaSession Reader;
		TestEqual(TEXT("the file reads"), Reader.Open(Path), EDTMetaOpen::Read);
		const int32 Rank = Reader.Profile.TreeRanks.FindRef(LockNode);
		TestEqual(TEXT("the bank is in the file"), Reader.Profile.Runs, 1);
		TestEqual(TEXT("a purchase that said bought is in the file, and one that is not there said so"), Rank, Result == EDTTreeBuy::Ok ? 1 : 0);
		TestEqual(TEXT("the price was taken once for a rank, and not at all for none"), Reader.Profile.Currency, Price + A.Retained - Rank * Price);
		TestEqual(TEXT("the buyer's own balance is what it was told"), Second.Profile.Currency, Result == EDTTreeBuy::Ok ? 0 : Price);
		TestEqual(TEXT("asked again with the lock free, it buys on top of the bank"), DTTree::Buy(Second, LockNode), EDTTreeBuy::Ok);
		TestEqual(TEXT("the file reads again"), Reader.Open(Path), EDTMetaOpen::Read);
		TestTrue(TEXT("the run, its award and the rank"), Reader.Profile.Runs == 1 && Reader.Profile.Currency == A.Retained && Reader.Profile.TreeRanks.FindRef(LockNode) == 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetSaveLock, "Dusktracer.Meta.SaveLock", DTMetaLockTest::Flags) // T-MET-50
bool FDTMetSaveLock::RunTest(const FString& Parameters)
{
	using namespace DTMetaLockTest;
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("Tree:"), EAutomationExpectedErrorFlags::Contains, 0);
	const int64 Price = DTCat::TreeNodes[DTTree::FindNode(LockNode)].Cost;

	// The lock itself: one holder, and nothing left behind that a later launch would have to clean up.
	{
		const FString Path = TempSave(TEXT("Lock.sav"));
		{
			const FDTMetaLock Holder(Path);
			TestTrue(TEXT("a free lock is had"), Holder.IsHeld());
			const double Start = FPlatformTime::Seconds();
			const FDTMetaLock Late(Path);
			const double WaitedMs = (FPlatformTime::Seconds() - Start) * 1000.0;
			TestFalse(TEXT("a held lock is not had"), Late.IsHeld());
			TestTrue(FString::Printf(TEXT("the wait is C.SAVE.LOCK_WAIT_MS and no more than a player would notice (%.0f ms)"), WaitedMs),
				WaitedMs >= DTConst::SAVE_LOCK_WAIT_MS * 0.9 && WaitedMs < 1000.0);
			const FDTMetaLock None(Path, 0);
			TestFalse(TEXT("nor with no wait"), None.IsHeld());
		}
		const FDTMetaLock Again(Path, 0);
		TestTrue(TEXT("let go, it is free at once"), Again.IsHeld());
	}
#if PLATFORM_WINDOWS
	TestFalse(TEXT("no lock file is left"), IFileManager::Get().FileExists(*FDTMetaLock::PathFor(TempSave(TEXT("Lock.sav")))));
#endif
	{
		// A lock file nobody holds, as a crash could leave where the file outlives its handle: it blocks nothing.
		const FString Path = TempSave(TEXT("Stale.sav"));
		TestTrue(TEXT("a stale lock file lands"), FFileHelper::SaveArrayToFile(TArray<uint8>({ 1 }), *FDTMetaLock::PathFor(Path)));
		FDTMetaSession Session;
		Session.Open(Path);
		const FDTRunAward Award = Session.Bank(Minutes(1), EDTRunEnd::Left);
		TestFalse(TEXT("a bank goes through a stale lock file"), Award.bDeferred || Session.bUnsaved);
		FDTMetaSession Reader;
		TestTrue(TEXT("and is in the file"), Reader.Open(Path) == EDTMetaOpen::Read && Reader.Profile.Runs == 1);
	}

	// The lock is another launch's for longer than the wait.
	{
		const FString Path = TempSave(TEXT("Held.sav"));
		FDTMetaSession Session;
		Session.Open(Path);
		Session.Profile.Currency = Price;
		TestTrue(TEXT("the price is planted"), Session.Write());
		const TArray<uint8> Planted = Session.Seen;
		FDTRunAward Award;
		{
			const FDTMetaLock Other(Path);
			TestTrue(TEXT("another launch holds the lock"), Other.IsHeld());
			TestEqual(TEXT("a purchase is refused as busy"), DTTree::Buy(Session, LockNode), EDTTreeBuy::Busy);
			TestTrue(TEXT("and spends nothing"), Session.Profile.Currency == Price && Session.Profile.TreeRanks.FindRef(LockNode) == 0 && !Session.bUnsaved);
			Award = Session.Bank(Minutes(2), EDTRunEnd::Death);
			TestTrue(TEXT("a bank is kept in memory and says so"), Award.bBanked && Award.bDeferred && Award.Waiting == 1 && Session.bUnsaved);
			TestEqual(TEXT("the balance the screen reads has the run"), Award.Balance, Price + Award.Retained);
			TestFalse(TEXT("a save point with the lock still held writes nothing"), Session.Flush());
			TestFalse(TEXT("nor does a plant"), Session.Write());
			TArray<uint8> OnDisk;
			TestTrue(TEXT("the file is as it was"), FFileHelper::LoadFileToArray(OnDisk, *Path) && OnDisk == Planted);
			TestEqual(TEXT("the run still waits"), Session.Pending.Num(), 1);

			// What the holder was doing all along: it writes five runs of its own onto the planted balance.
			FDTMetaProfile Theirs;
			Theirs.Currency = Price + 1000;
			Theirs.Runs = 5;
			TestTrue(TEXT("the holder writes"), DTMeta::WriteAtomic(Path, FDTSaveEnvelope::Wrap(DTMeta::Serialize(Theirs))));
		}
		TestTrue(TEXT("the next save point, the lock free, writes"), Session.Flush());
		TestFalse(TEXT("nothing waits"), Session.bUnsaved || Session.Pending.Num() > 0);
		FDTMetaSession Reader;
		TestEqual(TEXT("the file reads"), Reader.Open(Path), EDTMetaOpen::Read);
		TestEqual(TEXT("the kept run landed on the holder's five"), Reader.Profile.Runs, 6);
		TestEqual(TEXT("with its award on the holder's balance"), Reader.Profile.Currency, Price + 1000 + Award.Retained);
		TestEqual(TEXT("and its death"), Reader.Profile.DeathsSinceWin, 1);
	}

	// A change a failed write left in memory is made again on what another launch wrote meanwhile, not written over it.
	{
		const FString Path = TempSave(TEXT("Carry.sav"));
		const FString Blocker = Path + TEXT(".tmp");
		FDTMetaSession Session;
		Session.Open(Path);
		Session.Profile.Currency = Price;
		TestTrue(TEXT("the price is planted"), Session.Write());
		FDTMetaSession Other;
		TestEqual(TEXT("a second window reads it"), Other.Open(Path), EDTMetaOpen::Read);

		IFileManager::Get().MakeDirectory(*Blocker, true);
		const FDTRunAward Run = Session.Bank(Minutes(1), EDTRunEnd::Left);
		TestEqual(TEXT("a purchase through a blocked write is in memory"), DTTree::Buy(Session, LockNode), EDTTreeBuy::Ok);
		TestTrue(TEXT("both wait"), Session.bUnsaved && Session.Pending.Num() == 2 && !Run.bDeferred);
		IFileManager::Get().DeleteDirectory(*Blocker, false, true);

		TestEqual(TEXT("the second window buys the node with the same Ichor"), DTTree::Buy(Other, LockNode), EDTTreeBuy::Ok);
		const FDTRunAward Theirs = Other.Bank(Minutes(3), EDTRunEnd::Left);
		TestTrue(TEXT("the next save point writes"), Session.Flush());
		FDTMetaSession Reader;
		TestEqual(TEXT("the file reads"), Reader.Open(Path), EDTMetaOpen::Read);
		TestEqual(TEXT("both windows' runs stand"), Reader.Profile.Runs, 2);
		TestEqual(TEXT("the node was sold once"), Reader.Profile.TreeRanks.FindRef(LockNode), 1);
		TestEqual(TEXT("for one price: the purchase that could not be made again was dropped, not charged"), Reader.Profile.Currency, Run.Retained + Theirs.Retained);
		TestTrue(TEXT("and this window reads what the file holds"), DTMeta::Same(Session.Profile, Reader.Profile));
	}
	return true;
}

#endif
