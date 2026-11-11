// The hub (02_Systems/vigil.md). T-VIG-17 is Tools/VigilCheck.ps1: real launches, real keys, shots of every page.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Hub; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunSeed.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Hub/DTHubData.h"
#include "Hub/DTHubFlow.h"
#include "Hub/DTHubScreen.h"
#include "Hub/DTHubState.h"
#include "Hub/DTHubTab.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Meta/DTMetaSession.h"
#include "Meta/DTMetaSubsystem.h"
#include "Meta/DTTree.h"
#include "Meta/DTTreeTab.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"
#include "Settings/DTMenuScreen.h"
#include "Templates/UniquePtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTHubTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static TArray<FDTHubPatron> Patrons(const FDTMetaProfile& Profile = FDTMetaProfile())
	{
		return DTHubData::Patrons(Profile, [](FName) { return false; });
	}

	static FDTHubState Open(EDTPath Last = EDTPath::None)
	{
		FDTHubState State;
		State.Window = 4;
		State.CodexRows = 10;
		State.Open(Patrons(), Last);
		return State;
	}

	static void Focus(FDTHubState& State, EDTHubRow Row)
	{
		State.Page = EDTHubPage::Root;
		State.Focus = static_cast<int32>(Row);
	}

	static const FDTCodexRow* Find(const TArray<FDTCodexRow>& Book, const TCHAR* Key)
	{
		return Book.FindByPredicate([Key](const FDTCodexRow& Row) { return Row.Key == Key; });
	}

	static FString TempSave(const TCHAR* Name)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("HubTests")));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, Name);
		IFileManager::Get().Delete(*Path, false, true, true);
		return Path;
	}

	static int32 TreeRow(const FDTTreeTab& Tab, const TCHAR* Node)
	{
		int32 Left = DTTree::FindNode(FName(Node));
		for (int32 Row = 0; Row < Tab.NumRows(); ++Row)
		{
			if (!Tab.IsHeading(Row) && Left-- == 0)
			{
				return Row;
			}
		}
		return INDEX_NONE;
	}

	static FString TreeText(const TCHAR* Key)
	{
		return FText::FromStringTable(TEXT("ST_Tree"), Key).ToString();
	}

	static FString CostText(int32 Cost)
	{
		return FText::Format(FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.Cost")), FText::AsNumber(Cost)).ToString();
	}

	/** A state beside the price. */
	static FString StateCost(const FText& State, int32 Cost)
	{
		return FText::Format(FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.Row")), State, FText::FromString(CostText(Cost))).ToString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigBoot, "Dusktracer.Hub.Boot", DTHubTest::Flags) // T-VIG-01
bool FDTVigBoot::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a person's launch opens the hub"), DTHubFlow::Wants(false, false, false));
	TestFalse(TEXT("a map with a seed is a run"), DTHubFlow::Wants(false, false, true));
	TestTrue(TEXT("a proof can force the hub"), DTHubFlow::Wants(true, true, false));
	TestFalse(TEXT("a forced proof still plays its run"), DTHubFlow::Wants(true, true, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigTools, "Dusktracer.Hub.ToolRuns", DTHubTest::Flags) // T-VIG-02
bool FDTVigTools::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("a tool run skips the hub"), DTHubFlow::Wants(true, false, false));
	TestTrue(TEXT("this very run is a tool run"), UDTMetaSubsystem::IsToolRun());
	TestFalse(TEXT("so this process has no hub"), DTHubFlow::Enabled());
	FDTMenuState Menu;
	Menu.OpenRunOver(EDTRunOverKind::Death);
	TestEqual(TEXT("with no hub the run-over rows are as they were"), Menu.NumRows(), static_cast<int32>(EDTRunOverRow::Num));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigRoot, "Dusktracer.Hub.RootRows", DTHubTest::Flags) // T-VIG-03
bool FDTVigRoot::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	FDTHubState State = Open();
	TestEqual(TEXT("the root's rows"), State.NumRows(), static_cast<int32>(EDTHubRow::Num));
	TestEqual(TEXT("no Patron: focus on the Shrine"), State.RootRow(), EDTHubRow::Patron);
	const EDTHubPage Pages[] = { EDTHubPage::Patron, EDTHubPage::Tree, EDTHubPage::Codex, EDTHubPage::Stats };
	const EDTHubRow Rows[] = { EDTHubRow::Patron, EDTHubRow::Tree, EDTHubRow::Codex, EDTHubRow::Stats };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Pages); ++Index)
	{
		Focus(State, Rows[Index]);
		State.Apply(EDTMenuCommand::Accept);
		TestEqual(TEXT("the row opens its page"), State.Page, Pages[Index]);
	}
	Focus(State, EDTHubRow::Settings);
	TestEqual(TEXT("Settings is an action"), State.Apply(EDTMenuCommand::Accept).Action, EDTHubAction::Settings);
	Focus(State, EDTHubRow::Quit);
	TestEqual(TEXT("Quit is an action"), State.Apply(EDTMenuCommand::Accept).Action, EDTHubAction::Quit);

	// The rows in order, each under its own name (R-VIG-03): a row that opens the Codex and reads "Contracts" fails here.
	struct FRootLabel
	{
		EDTHubRow Row;
		const TCHAR* Table;
		const TCHAR* Key;
		EDTHubPage Opens;
	};
	const FRootLabel Labels[] = {
		{ EDTHubRow::Patron, TEXT("ST_World"), TEXT("Hub.Shrine"), EDTHubPage::Patron }, { EDTHubRow::Tree, TEXT("ST_World"), TEXT("Hub.Contracts"), EDTHubPage::Tree },
		{ EDTHubRow::Codex, TEXT("ST_World"), TEXT("Hub.Codex"), EDTHubPage::Codex }, { EDTHubRow::Stats, TEXT("ST_World"), TEXT("Hub.LedgerStone"), EDTHubPage::Stats },
		{ EDTHubRow::Settings, TEXT("ST_World"), TEXT("Hub.Settings"), EDTHubPage::Root }, { EDTHubRow::Begin, TEXT("ST_World"), TEXT("Hub.Begin"), EDTHubPage::Root },
		{ EDTHubRow::Quit, TEXT("ST_UI"), TEXT("Pause.Quit"), EDTHubPage::Root } };
	TestEqual(TEXT("a label for every root row"), static_cast<int32>(UE_ARRAY_COUNT(Labels)), static_cast<int32>(EDTHubRow::Num));
	TSet<FString> Shown;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
	{
		const FRootLabel& Want = Labels[Index];
		const FString Label = DTHubText::RootLabel(Want.Row).ToString();
		TestEqual(FString::Printf(TEXT("screen order: row %d"), Index), static_cast<int32>(Want.Row), Index);
		TestEqual(FString::Printf(TEXT("row %d reads its own row of the table"), Index), Label, FText::FromStringTable(FName(Want.Table), Want.Key).ToString());
		TestFalse(FString::Printf(TEXT("row %d resolves"), Index), Label.IsEmpty() || Label.Contains(TEXT("<MISSING")));
		Shown.Add(Label);
		if (Want.Opens != EDTHubPage::Root)
		{
			TestEqual(FString::Printf(TEXT("the page row %d opens is titled as the row"), Index), DTHubText::PageTitle(Want.Opens).ToString(), Label);
		}
	}
	TestEqual(TEXT("no two rows read alike"), Shown.Num(), static_cast<int32>(EDTHubRow::Num));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigBack, "Dusktracer.Hub.Navigation", DTHubTest::Flags) // T-VIG-04
bool FDTVigBack::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	FDTHubState State = Open();
	const int32 Rows = State.NumRows();
	State.Apply(EDTMenuCommand::Up);
	TestEqual(TEXT("focus wraps up"), State.Focus, Rows - 1);
	State.Apply(EDTMenuCommand::Down);
	TestEqual(TEXT("focus wraps down"), State.Focus, 0);
	const EDTHubRow Each[] = { EDTHubRow::Patron, EDTHubRow::Tree, EDTHubRow::Codex, EDTHubRow::Stats };
	for (const EDTHubRow Row : Each)
	{
		Focus(State, Row);
		State.Apply(EDTMenuCommand::Accept);
		State.Apply(EDTMenuCommand::Back);
		TestEqual(TEXT("Back lands on the root"), State.Page, EDTHubPage::Root);
		TestEqual(TEXT("on the row that opened the page"), State.RootRow(), Row);
	}
	{
		// Begin with no Patron opens the Shrine, and Back from there lands on Begin, the row that opened it.
		FDTHubState Bare = Open();
		Bare.Selected = EDTPath::None;
		Focus(Bare, EDTHubRow::Begin);
		Bare.Apply(EDTMenuCommand::Accept);
		TestEqual(TEXT("Begin with no Patron opens the Shrine"), Bare.Page, EDTHubPage::Patron);
		Bare.Apply(EDTMenuCommand::Back);
		TestEqual(TEXT("and Back lands on Begin"), Bare.RootRow(), EDTHubRow::Begin);
		Focus(Bare, EDTHubRow::Patron);
		Bare.Apply(EDTMenuCommand::Accept);
		Bare.Apply(EDTMenuCommand::Back);
		TestEqual(TEXT("the Shrine's own row still gets it back"), Bare.RootRow(), EDTHubRow::Patron);
	}
	{
		// The key reader: a key held as the hub opens is no press, and one poll gives one command.
		TSet<FKey> Held = { EKeys::Enter };
		const auto IsDown = [&Held](const FKey& Key) { return Held.Contains(Key); };
		FDTHubKeys Keys;
		TestEqual(TEXT("a key held on arrival is not a press"), Keys.Poll(IsDown, false), EDTMenuCommand::None);
		TestEqual(TEXT("nor while it stays down"), Keys.Poll(IsDown, false), EDTMenuCommand::None);
		Held.Reset();
		Keys.Poll(IsDown, false);
		Held = { EKeys::Enter };
		TestEqual(TEXT("up and down again is one"), Keys.Poll(IsDown, false), EDTMenuCommand::Accept);
		Held.Reset();
		Keys.Poll(IsDown, false);
		int32 Accepts = 0;
		for (const FKey& Key : DTMenuInput::Keys())
		{
			if (DTMenuInput::ForKey(Key) == EDTMenuCommand::Accept)
			{
				Held.Add(Key);
				++Accepts;
			}
		}
		TestTrue(TEXT("two keys accept"), Accepts >= 2);
		TestEqual(TEXT("two keys landing together give one command"), Keys.Poll(IsDown, false), EDTMenuCommand::Accept);
		TestEqual(TEXT("and the other is not kept for the next poll"), Keys.Poll(IsDown, false), EDTMenuCommand::None);
		Held.Reset();
		Keys.Poll(IsDown, false);
		Held = { EKeys::Enter };
		TestEqual(TEXT("a press under a menu is the menu's"), Keys.Poll(IsDown, true), EDTMenuCommand::None);
		TestEqual(TEXT("and stays so while held"), Keys.Poll(IsDown, false), EDTMenuCommand::None);
	}
	{
		// A stick pushed up, 15 degrees off its axis, holds its sideways key down as well: only the larger axis is a press.
		const float OffX = 0.26f;
		const float OffY = 0.97f;
		TestTrue(TEXT("up counts on a push that is mostly up"), DTMenuInput::StickKeyCounts(EKeys::Gamepad_LeftStick_Up, OffX, OffY));
		TestFalse(TEXT("its sideways key does not"), DTMenuInput::StickKeyCounts(EKeys::Gamepad_LeftStick_Right, OffX, OffY));
		TestFalse(TEXT("nor to the left"), DTMenuInput::StickKeyCounts(EKeys::Gamepad_LeftStick_Left, -OffX, -OffY));
		TestTrue(TEXT("down counts on a push that is mostly down"), DTMenuInput::StickKeyCounts(EKeys::Gamepad_LeftStick_Down, -OffX, -OffY));
		TestTrue(TEXT("sideways counts on a push that is mostly sideways"), DTMenuInput::StickKeyCounts(EKeys::Gamepad_LeftStick_Left, -OffY, OffX));
		TestFalse(TEXT("and its vertical key does not"), DTMenuInput::StickKeyCounts(EKeys::Gamepad_LeftStick_Up, -OffY, OffX));
		TestTrue(TEXT("a stick key with no axis reading is a press"), DTMenuInput::StickKeyCounts(EKeys::Gamepad_LeftStick_Left, 0.0f, 0.0f) && DTMenuInput::StickKeyCounts(EKeys::Gamepad_LeftStick_Down, 0.0f, 0.0f));
		TestTrue(TEXT("the D-pad and the keyboard never wait on the stick"), DTMenuInput::StickKeyCounts(EKeys::Gamepad_DPad_Left, OffX, OffY) && DTMenuInput::StickKeyCounts(EKeys::A, OffX, OffY));

		// Through the reader, as the hub polls it: that push on a long page moves one row and never a window.
		FDTHubState Page = Open();
		Page.CodexRows = 30;
		Focus(Page, EDTHubRow::Codex);
		Page.Apply(EDTMenuCommand::Accept);
		const TSet<FKey> Held = { EKeys::Gamepad_LeftStick_Right, EKeys::Gamepad_LeftStick_Down };
		const auto IsDown = [&Held, OffX, OffY](const FKey& Key) { return Held.Contains(Key) && DTMenuInput::StickKeyCounts(Key, OffX, -OffY); };
		FDTHubKeys Stick;
		Stick.Poll([](const FKey&) { return false; }, false);
		const EDTMenuCommand First = Stick.Poll(IsDown, false);
		TestEqual(TEXT("the push is Down"), First, EDTMenuCommand::Down);
		Page.Apply(First);
		const EDTMenuCommand Second = Stick.Poll(IsDown, false);
		TestEqual(TEXT("and nothing more while it is held"), Second, EDTMenuCommand::None);
		TestEqual(TEXT("so focus moved one row"), Page.Focus, 1);
	}
	const FDTHubOutcome AtRoot = State.Apply(EDTMenuCommand::Back);
	TestEqual(TEXT("Back at the root does nothing but move"), AtRoot.Action, EDTHubAction::None);
	TestEqual(TEXT("to Quit"), State.RootRow(), EDTHubRow::Quit);

	// Every menu key of both devices gives a command the hub answers, with Back on Esc and pad B.
	TestEqual(TEXT("pad B backs out"), DTMenuInput::ForKey(EKeys::Gamepad_FaceButton_Right), EDTMenuCommand::Back);
	TestEqual(TEXT("pad A accepts"), DTMenuInput::ForKey(EKeys::Gamepad_FaceButton_Bottom), EDTMenuCommand::Accept);

	// A long page scrolls with the focus and pages sideways, stopping at its ends.
	Focus(State, EDTHubRow::Codex);
	State.Apply(EDTMenuCommand::Accept);
	for (int32 Step = 0; Step < 6; ++Step)
	{
		State.Apply(EDTMenuCommand::Down);
	}
	TestEqual(TEXT("focus stays in the window"), State.Scroll, State.Focus - State.Window + 1);
	State.Apply(EDTMenuCommand::Right);
	TestEqual(TEXT("a page right stops at the end"), State.Focus, State.CodexRows - 1);
	State.Apply(EDTMenuCommand::Down);
	TestEqual(TEXT("focus wraps to the top"), State.Focus, 0);
	TestEqual(TEXT("the window follows"), State.Scroll, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigShrine, "Dusktracer.Hub.Shrine", DTHubTest::Flags) // T-VIG-05
bool FDTVigShrine::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	FDTHubState State = Open();
	TestEqual(TEXT("every Keeper is listed"), State.Patrons.Num(), DTOffers::AllKeepers().Num());
	for (const FDTHubPatron& Patron : State.Patrons)
	{
		TestEqual(TEXT("a run's Keepers are open and the others sealed"), Patron.bOpen, DTOffers::RunKeepers().Contains(Patron.Path));
	}
	const int32 Sealed = State.Patrons.IndexOfByPredicate([](const FDTHubPatron& Patron) { return !Patron.bOpen; });
	if (!TestTrue(TEXT("a Keeper is sealed"), Sealed != INDEX_NONE))
	{
		return false;
	}
	Focus(State, EDTHubRow::Patron);
	State.Apply(EDTMenuCommand::Accept);
	State.Focus = Sealed;
	State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("a sealed Keeper is not chosen"), State.Selected, EDTPath::None);
	TestEqual(TEXT("and the page stays"), State.Page, EDTHubPage::Patron);

	FDTHubView View;
	TArray<FDTCodexRow> None;
	FDTMetaProfile Profile;
	View.State = &State;
	View.Codex = &None;
	View.Profile = &Profile;
	const FString Hint = DTHubText::Detail(View).ToString();
	TestTrue(TEXT("focus on it shows the hint"), DTHubText::Detail(View).EqualTo(FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine.LockedHint"))));

	// An open Keeper's detail is its own terms, <Path>.Desc (R-VIG-05): not the hint, not its name, not another Keeper's.
	TSet<FString> Terms;
	int32 OpenRows = 0;
	for (int32 Row = 0; Row < State.Patrons.Num(); ++Row)
	{
		if (!State.Patrons[Row].bOpen)
		{
			continue;
		}
		++OpenRows;
		State.Focus = Row;
		const FString Path = DTOffers::PathName(State.Patrons[Row].Path);
		const FString Detail = DTHubText::Detail(View).ToString();
		TestEqual(FString::Printf(TEXT("%s open: the detail is its Desc row"), *Path), Detail, FText::FromStringTable(TEXT("ST_Keepers"), Path + TEXT(".Desc")).ToString());
		TestFalse(FString::Printf(TEXT("%s: the row resolves"), *Path), Detail.IsEmpty() || Detail.Contains(TEXT("<MISSING")));
		TestTrue(FString::Printf(TEXT("%s: it is neither the sealed hint nor the Keeper's name"), *Path), Detail != Hint && Detail != DTHubText::KeeperName(State.Patrons[Row].Path).ToString());
		Terms.Add(Detail);
	}
	TestTrue(TEXT("a run's Keepers are open"), OpenRows > 0);
	TestEqual(TEXT("each open Keeper shows its own terms"), Terms.Num(), OpenRows);
	State.Focus = Sealed;

	FDTMetaProfile Opened;
	Opened.Unlocked.Add(DTOffers::KeeperId(State.Patrons[Sealed].Path));
	TestTrue(TEXT("a saved unlock opens it"), Patrons(Opened)[Sealed].bOpen);
	const FName Id = DTOffers::KeeperId(State.Patrons[Sealed].Path);
	TestTrue(TEXT("so does this launch's pool"), DTHubData::Patrons(FDTMetaProfile(), [Id](FName Asked) { return Asked == Id; })[Sealed].bOpen);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigLast, "Dusktracer.Hub.LastPatron", DTHubTest::Flags) // T-VIG-06
bool FDTVigLast::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	TestEqual(TEXT("a saved name reads back"), DTHubData::ParsePath(TEXT("Forge")), EDTPath::Forge);
	TestEqual(TEXT("an empty name is none"), DTHubData::ParsePath(NAME_None), EDTPath::None);
	TestEqual(TEXT("an unknown name is none"), DTHubData::ParsePath(TEXT("Nobody")), EDTPath::None);
	FDTHubState State = Open(EDTPath::Forge);
	TestEqual(TEXT("the last Patron is chosen"), State.Selected, EDTPath::Forge);
	TestEqual(TEXT("focus on Begin"), State.RootRow(), EDTHubRow::Begin);
	Focus(State, EDTHubRow::Patron);
	State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("the Shrine opens on it"), State.Patrons[State.Focus].Path, EDTPath::Forge);
	FDTHubState Sealed = Open(EDTPath::Shade);
	TestEqual(TEXT("a Patron since sealed is not chosen"), Sealed.Selected, EDTPath::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigBegin, "Dusktracer.Hub.Begin", DTHubTest::Flags) // T-VIG-07
bool FDTVigBegin::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	FDTHubState State = Open();
	Focus(State, EDTHubRow::Begin);
	TestEqual(TEXT("no Patron: no run"), State.Apply(EDTMenuCommand::Accept).Action, EDTHubAction::None);
	TestEqual(TEXT("the Shrine opens instead"), State.Page, EDTHubPage::Patron);
	State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("the pick lands on Begin"), State.RootRow(), EDTHubRow::Begin);
	TestEqual(TEXT("then Begin starts"), State.Apply(EDTMenuCommand::Accept).Action, EDTHubAction::Begin);

	FDTMetaProfile Profile;
	TestEqual(TEXT("the map options"), DTHubData::RunOptions(7, EDTPath::Tide, Profile), FString(TEXT("DTSeed=7?DTPatron=Tide")));
	Profile.Unlocked.Add(DTOffers::KeeperId(EDTPath::Shade));
	Profile.Unlocked.Add(TEXT("Branch.Storm.Blessing4"));
	const FString Options = DTHubData::RunOptions(7, EDTPath::Shade, Profile);
	TestTrue(TEXT("a saved Keeper unlock travels, and no other key"), Options.EndsWith(TEXT("?DTPatron=Shade?DTUnlock=Keeper.Shade")));

	const FDTRunSeed Named = FDTRunSeed::Resolve(TEXT("-DTSeed=9107"), /*bToolRun*/ false, 1);
	TestEqual(TEXT("-DTSeed names the launch's first run"), DTHubFlow::BeginSeed(Named, true, 1), 9107);
	TestNotEqual(TEXT("and only that one"), DTHubFlow::BeginSeed(Named, false, 1), 9107);
	TestEqual(TEXT("0 is a seed when named"), DTHubFlow::BeginSeed(FDTRunSeed::Resolve(TEXT("-DTSeed=0"), false, 1), true, 1), 0);
	const FDTRunSeed Person = FDTRunSeed::Resolve(TEXT(""), false, 77);
	TestEqual(TEXT("a person's launch runs on its fresh seed"), DTHubFlow::BeginSeed(Person, true, 1), Person.Seed);
	TestEqual(TEXT("each hub world holds its own"), DTHubFlow::BeginSeed(Person, false, 1), Person.Seed);
	TestTrue(TEXT("a tool that forced the hub draws"), DTHubFlow::BeginSeed(FDTRunSeed::Resolve(TEXT(""), true, 1), true, 1) >= FDTRunSeed::FreshMin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigCodex, "Dusktracer.Hub.CodexRows", DTHubTest::Flags) // T-VIG-08
bool FDTVigCodex::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	const TArray<FDTCodexSource> Sources = DTHubData::ReadCodexTable();
	if (!TestTrue(TEXT("the table is read with its metadata"), Sources.Num() > 0))
	{
		return false;
	}
	FDTMetaProfile Fresh;
	const TArray<FDTCodexRow> Book = DTHubData::BuildCodex(Sources, Fresh);
	TestNull(TEXT("the format row is no entry"), Find(Book, TEXT("Codex.EntryTitle")));
	TestNull(TEXT("Layer 3 is not listed before the first win"), Find(Book, TEXT("Codex.Root.Title")));
	TestNull(TEXT("nor the Form III entry"), Find(Book, TEXT("Codex.Warden.FormIII")));
	const FDTCodexRow* Locked = Find(Book, TEXT("Codex.Recollection.Velocity"));
	if (TestNotNull(TEXT("a locked row is listed"), Locked))
	{
		TestFalse(TEXT("unread"), Locked->bRead);
		TestTrue(TEXT("its text is not there"), Locked->Body.IsEmpty());
		TestTrue(TEXT("nor its title"), Locked->Title.EqualTo(FText::FromStringTable(TEXT("ST_UI"), TEXT("Codex.Locked"))));
	}
	const FDTCodexRow* Keeper = Find(Book, TEXT("Codex.Keeper.Storm"));
	if (TestNotNull(TEXT("a Start row is listed"), Keeper))
	{
		TestTrue(TEXT("and readable"), Keeper->bRead && !Keeper->Body.IsEmpty());
	}
	for (int32 Index = 1; Index < Book.Num(); ++Index)
	{
		TestTrue(TEXT("the book sorts by layer"), Book[Index - 1].Layer <= Book[Index].Layer);
	}

	// The first view moves the window and nothing else: the sort is the book's, a locked row stays bare, the reach stays the reach.
	const int32 First = Book.IndexOfByPredicate([](const FDTCodexRow& Row) { return Row.bRead; });
	TestTrue(TEXT("a fresh book's first read row is under locked ones"), First > 0);
	FDTHubState State = Open();
	State.CodexRows = Book.Num();
	State.CodexFirst = First;
	Focus(State, EDTHubRow::Codex);
	State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("the page opens on it"), State.Focus, First);
	TestEqual(TEXT("at the top of the window"), State.Scroll, First);
	TestTrue(TEXT("with its text in the detail pane"), Book.IsValidIndex(State.Focus) && !Book[State.Focus].Body.IsEmpty());
	State.Apply(EDTMenuCommand::Up);
	TestEqual(TEXT("the locked rows above are still there"), State.Focus, First - 1);
	State.Apply(EDTMenuCommand::Back);
	State.CodexFirst = Book.Num() - 1;
	Focus(State, EDTHubRow::Codex);
	State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("a first read row at the end leaves the window full"), State.Scroll, Book.Num() - State.Window);
	TestEqual(TEXT("and in it"), State.Focus, Book.Num() - 1);
	FDTCodexSource Deep;
	Deep.Key = TEXT("Codex.Test.Deep");
	Deep.Layer = DTConst::NARR_PREWIN_LAYER_MAX + 1;
	Deep.Unlock = TEXT("Start");
	TestNull(TEXT("a read row past the reach is still not listed"), Find(DTHubData::BuildCodex({ Deep }, Fresh), TEXT("Codex.Test.Deep")));

	// Every row, read: a title and a body from the tables, in this culture, never a missing key.
	const FDTMetaProfile Everything = DTHubData::EverythingRead(Sources);
	TestTrue(TEXT("the profile that reads everything is one a save could hold"), DTMeta::IsValid(Everything));
	const TArray<FDTCodexRow> Whole = DTHubData::BuildCodex(Sources, Everything);
	TestEqual(TEXT("all but the format row"), Whole.Num(), Sources.Num() - 1);
	for (const FDTCodexRow& Row : Whole)
	{
		TestTrue(FString::Printf(TEXT("%s is read"), *Row.Key), Row.bRead);
		TestFalse(FString::Printf(TEXT("%s has a title"), *Row.Key), Row.Title.ToString().Contains(TEXT("<MISSING")) || Row.Title.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has its text"), *Row.Key), Row.Body.ToString().Contains(TEXT("<MISSING")));
		TestFalse(FString::Printf(TEXT("%s: the title is not the entry's own text"), *Row.Key), Row.Title.ToString() == Row.Body.ToString());
	}
	// The title comes from the row's subject (R-VIG-08): a Keeper's name, an enemy's name, the Discovery's name.
	const auto TitleIs = [this, &Whole](const TCHAR* Key, const TCHAR* Table, const TCHAR* TitleKey)
	{
		const FDTCodexRow* Row = DTHubTest::Find(Whole, Key);
		if (TestNotNull(FString::Printf(TEXT("%s is listed"), Key), Row))
		{
			TestEqual(FString::Printf(TEXT("%s is titled by %s"), Key, TitleKey), Row->Title.ToString(), FText::FromStringTable(FName(Table), TitleKey).ToString());
		}
	};
	TitleIs(TEXT("Codex.Keeper.Storm"), TEXT("ST_Keepers"), TEXT("Storm.Name"));
	TitleIs(TEXT("Codex.Keeper.Quicksilver"), TEXT("ST_Keepers"), TEXT("Quicksilver.Name"));
	TitleIs(TEXT("Codex.Enemy.Tank"), TEXT("ST_Enemies"), TEXT("Enemy.Tank.Name"));
	TitleIs(TEXT("Codex.Enemy.Boss"), TEXT("ST_Enemies"), TEXT("Enemy.Boss.Name"));
	TitleIs(TEXT("Codex.Warden.FormIII"), TEXT("ST_Enemies"), TEXT("Enemy.Boss.Name"));
	TitleIs(TEXT("Codex.Recollection.Velocity"), TEXT("ST_Discoveries"), TEXT("Discovery.Dashes100.Name"));
	TitleIs(TEXT("Codex.Recollection.Win"), TEXT("ST_Discoveries"), TEXT("Discovery.Win.Name"));
	for (const FDTCodexSource& Source : Sources)
	{
		FString Tail;
		const FDTCodexRow* Row = Find(Whole, *Source.Key);
		if (Row && Source.Key.StartsWith(TEXT("Codex.Recollection.")) && Source.Unlock.Split(TEXT("Discovery."), nullptr, &Tail))
		{
			TestEqual(FString::Printf(TEXT("%s is titled by its Discovery"), *Source.Key), Row->Title.ToString(),
				FText::FromStringTable(TEXT("ST_Discoveries"), FString::Printf(TEXT("Discovery.%s.Name"), *Tail)).ToString());
		}
	}
	const FDTCodexRow* Root = Find(Whole, TEXT("Codex.Root.Title"));
	if (TestNotNull(TEXT("the ROOT row after a win"), Root))
	{
		TestTrue(TEXT("a title and no body"), Root->Body.IsEmpty() && Root->Title.ToString().Contains(TEXT("ROOT")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigUnlock, "Dusktracer.Hub.Unlock", DTHubTest::Flags) // T-VIG-09
bool FDTVigUnlock::RunTest(const FString& Parameters)
{
	FDTMetaProfile P;
	TestTrue(TEXT("Start"), DTHubData::IsUnlocked(TEXT("Start"), P));
	TestFalse(TEXT("Win before one"), DTHubData::IsUnlocked(TEXT("Win"), P));
	TestFalse(TEXT("an unknown cell"), DTHubData::IsUnlocked(TEXT("Moon.Phase"), P));
	TestFalse(TEXT("a Discovery not earned"), DTHubData::IsUnlocked(TEXT("Discovery.Win"), P));
	TestFalse(TEXT("a role not killed"), DTHubData::IsUnlocked(TEXT("Kill.Tank"), P));
	TestFalse(TEXT("a boss not killed"), DTHubData::IsUnlocked(TEXT("Kill.Boss"), P));
	P.Wins = 1;
	TestTrue(TEXT("Win"), DTHubData::IsUnlocked(TEXT("Win"), P));
	P.Wins = DTConst::BOSS_FORM_III_WINS;
	TestFalse(TEXT("the wins that open Form III are not its kill"), DTHubData::IsUnlocked(TEXT("Wins3.FormIII"), P));
	P.Wins = DTConst::BOSS_FORM_III_WINS + 1;
	TestTrue(TEXT("one more is"), DTHubData::IsUnlocked(TEXT("Wins3.FormIII"), P));
	P.Discoveries.Add(TEXT("Win"));
	TestTrue(TEXT("a bare Discovery id"), DTHubData::IsUnlocked(TEXT("Discovery.Win"), P));
	P.Discoveries.Add(TEXT("Discovery.Dashes100"));
	TestTrue(TEXT("a full Discovery key"), DTHubData::IsUnlocked(TEXT("Discovery.Dashes100"), P));
	P.LifetimeStats.Add(TEXT("Kills.Role.Tank"), 1);
	TestTrue(TEXT("a role's counter"), DTHubData::IsUnlocked(TEXT("Kill.Tank"), P));
	P.LifetimeStats.Add(TEXT("BossKills"), 1);
	TestTrue(TEXT("a boss kill"), DTHubData::IsUnlocked(TEXT("Kill.Boss"), P));
	FDTMetaProfile NoWin;
	TestEqual(TEXT("reach before the first win"), DTHubData::LayerReach(NoWin), DTConst::NARR_PREWIN_LAYER_MAX);
	NoWin.Wins = 1;
	TestEqual(TEXT("reach after it"), DTHubData::LayerReach(NoWin), DTConst::NARR_LAYER_MAX);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigStats, "Dusktracer.Hub.Stats", DTHubTest::Flags) // T-VIG-10
bool FDTVigStats::RunTest(const FString& Parameters)
{
	FDTMetaProfile P;
	P.Currency = 123;
	P.Runs = 4;
	P.Wins = 2;
	P.DeathsSinceWin = 1;
	P.LifetimeStats.Add(TEXT("CurrencyEarned"), 456);
	P.LifetimeStats.Add(TEXT("Steps"), DTConst::SIM_FIXED_HZ * 3725);
	P.LifetimeStats.Add(TEXT("Kills"), 789);
	P.LifetimeStats.Add(TEXT("Elites"), 11);
	P.LifetimeStats.Add(TEXT("BossKills"), 3);
	const TArray<FDTHubStat> Rows = DTHubData::Stats(P);
	const TCHAR* const Want[] = { TEXT("123"), TEXT("456"), TEXT("4"), TEXT("2"), TEXT("1"), TEXT("1:02:05"), TEXT("789"), TEXT("11"), TEXT("3") };
	if (!TestEqual(TEXT("the rows"), Rows.Num(), static_cast<int32>(UE_ARRAY_COUNT(Want))))
	{
		return false;
	}
	// The label beside each value (R-VIG-10): the balance row reading "Runs" fails here.
	const TCHAR* const WantLabel[] = { TEXT("Hub.Stats.Balance"), TEXT("Hub.Stats.Earned"), TEXT("Hub.Stats.Runs"), TEXT("Hub.Stats.Wins"), TEXT("Hub.Stats.Repairs"),
		TEXT("RunSummary.Time"), TEXT("Hub.Stats.Kills"), TEXT("Hub.Stats.Elites"), TEXT("Hub.Stats.Bosses") };
	TSet<FString> Shown;
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		const FString Label = Rows[Index].Label.ToString();
		TestEqual(TEXT("the value"), Rows[Index].Value.ToString(), FString(Want[Index]));
		TestFalse(TEXT("the label"), Label.IsEmpty() || Label.Contains(TEXT("<MISSING")));
		TestEqual(FString::Printf(TEXT("row %d: the label of its value"), Index), Label, FText::FromStringTable(TEXT("ST_UI"), WantLabel[Index]).ToString());
		Shown.Add(Label);
	}
	TestEqual(TEXT("no two rows read alike"), Shown.Num(), Rows.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigSettings, "Dusktracer.Hub.Settings", DTHubTest::Flags) // T-VIG-11
bool FDTVigSettings::RunTest(const FString& Parameters)
{
	FDTUserSettings S;
	FDTMenuState Menu;
	Menu.bHubHost = true;
	Menu.Open();
	Menu.Screen = EDTMenuScreen::Settings;
	TestTrue(TEXT("Back from Settings closes"), Menu.Apply(EDTMenuCommand::Back, S).bClosed && Menu.Screen == EDTMenuScreen::None);
	Menu.Open();
	Menu.Screen = EDTMenuScreen::Controls;
	Menu.Apply(EDTMenuCommand::Back, S);
	TestEqual(TEXT("Back from Controls is Settings"), Menu.Screen, EDTMenuScreen::Settings);
	Menu.OpenQuitConfirm();
	TestEqual(TEXT("the Quit row asks"), Menu.Screen, EDTMenuScreen::Confirm);
	TestTrue(TEXT("with no line about a run: the hub has none"), DTMenuText::ConfirmBodyKey(Menu) == nullptr);
	{
		FDTMenuState Paused;
		Paused.OpenQuitConfirm();
		TestEqual(TEXT("a pause's confirmation keeps the line"), FString(DTMenuText::ConfirmBodyKey(Paused)), FString(TEXT("Pause.ConfirmBody")));
	}
	TestEqual(TEXT("on Cancel"), Menu.Focus, static_cast<int32>(EDTConfirmRow::Cancel));
	TestTrue(TEXT("Cancel closes"), Menu.Apply(EDTMenuCommand::Accept, S).bClosed);
	Menu.OpenQuitConfirm();
	TestTrue(TEXT("Back closes"), Menu.Apply(EDTMenuCommand::Back, S).bClosed);
	Menu.OpenQuitConfirm();
	Menu.Apply(EDTMenuCommand::Up, S);
	TestEqual(TEXT("the confirm row quits"), Menu.Apply(EDTMenuCommand::Accept, S).Action, EDTMenuAction::Quit);
	return true;
}

namespace DTHubTest
{
	struct FFakeTab : IDTHubTab
	{
		int32 Accepted = INDEX_NONE;
		virtual int32 NumRows() const override { return 2; }
		virtual FText RowLabel(int32) const override { return FText::GetEmpty(); }
		virtual FText RowValue(int32) const override { return FText::GetEmpty(); }
		virtual FText RowDetail(int32 Row) const override { return FText::AsNumber(Row); }
		virtual FText Header() const override { return FText::GetEmpty(); }
		virtual void Accept(int32 Row) override { Accepted = Row; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigTree, "Dusktracer.Hub.TreeSlot", DTHubTest::Flags) // T-VIG-12
bool FDTVigTree::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	TestFalse(TEXT("the module's page needs a session"), DTHubTabs::MakeTree(nullptr).IsValid());
	DTHubTabs::SetTreeFactory(nullptr);
	TestFalse(TEXT("no factory, no tab"), DTHubTabs::MakeTree(nullptr).IsValid());
	FDTHubState State = Open();
	Focus(State, EDTHubRow::Tree);
	State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("the empty slot is a page"), State.Page, EDTHubPage::Tree);
	TestEqual(TEXT("Accept does nothing on it"), State.Apply(EDTMenuCommand::Accept).Action, EDTHubAction::None);

	TSharedPtr<FFakeTab> Fake = MakeShared<FFakeTab>();
	DTHubTabs::SetTreeFactory([Fake](UWorld*) { return Fake; });
	const TSharedPtr<IDTHubTab> Tab = DTHubTabs::MakeTree(nullptr);
	DTTreeTab::Register();
	TestTrue(TEXT("the factory builds the tab"), Tab == Fake);
	State.TreeRows = Tab->NumRows();
	State.Apply(EDTMenuCommand::Down);
	const FDTHubOutcome Outcome = State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("Accept reaches the page"), Outcome.Action, EDTHubAction::TreeAccept);
	TestEqual(TEXT("with the row"), Outcome.Row, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigTreeRows, "Dusktracer.Hub.TreeRows", DTHubTest::Flags) // T-VIG-18
bool FDTVigTreeRows::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	FDTMetaSession Session;
	Session.Open(TempSave(TEXT("rows.sav")));
	FDTTreeTab Tab(Session);
	TestEqual(TEXT("every node, a heading for the Trunk and one for each branch"), Tab.NumRows(), DTCat::NumTreeNodes + 1 + DTOffers::AllKeepers().Num());
	TestEqual(TEXT("the Trunk's heading leads"), Tab.RowLabel(0).ToString(), TreeText(TEXT("Tree.Branch.Trunk")));
	TestTrue(TEXT("a heading is one"), Tab.IsHeading(0));
	TestTrue(TEXT("and holds no value or detail"), Tab.RowValue(0).IsEmpty() && Tab.RowDetail(0).IsEmpty());
	TestFalse(TEXT("nor is it grey"), Tab.IsDimmed(0));
	const int32 Storm = TreeRow(Tab, TEXT("Branch.Storm.Blessing4"));
	TestTrue(TEXT("a branch has its heading"), Tab.IsHeading(Storm - 1));
	TestTrue(TEXT("naming the Keeper"), Tab.RowLabel(Storm - 1).ToString().Contains(DTHubText::KeeperName(EDTPath::Storm).ToString()));

	const int32 Reroll = TreeRow(Tab, TEXT("Trunk.Reroll"));
	const FText NotEnough = FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.State.Short"));
	TestTrue(TEXT("the header is the balance"), Tab.Header().EqualTo(FText::Format(FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.Balance")), FText::AsNumber(0))));
	TestEqual(TEXT("short: the state and the price"), Tab.RowValue(Reroll).ToString(), StateCost(NotEnough, DTConst::TREE_COST_TOOL));
	TestFalse(TEXT("and not grey: more runs open it"), Tab.IsDimmed(Reroll));
	Session.Profile.Currency = DTConst::TREE_COST_TOOL;
	TestEqual(TEXT("open: the price"), Tab.RowValue(Reroll).ToString(), CostText(DTConst::TREE_COST_TOOL));
	TestEqual(TEXT("the detail is the node's line"), Tab.RowDetail(Reroll).ToString(), DTTree::NodeDesc(DTTree::FindNode(TEXT("Trunk.Reroll"))).ToString());

	const int32 Next = TreeRow(Tab, TEXT("Branch.Storm.Blessing5"));
	const FText Locked = FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.State.Locked"));
	TestEqual(TEXT("locked: the state and the price"), Tab.RowValue(Next).ToString(), StateCost(Locked, DTConst::TREE_COST_BLESSING5));
	TestTrue(TEXT("grey"), Tab.IsDimmed(Next));
	TestTrue(TEXT("the detail names what it needs"), Tab.RowDetail(Next).ToString().Contains(DTTree::NodeName(DTTree::FindNode(TEXT("Branch.Storm.Blessing4"))).ToString()));

	const FText Sealed = FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine.Locked"));
	const FString Hint = FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine.LockedHint")).ToString();
	for (const TCHAR* Node : { TEXT("Branch.Shade.Blessing4"), TEXT("Branch.Shade.Blessing5"), TEXT("Branch.Shade.Reforge") })
	{
		const int32 Row = TreeRow(Tab, Node);
		const int32 Cost = DTCat::TreeNodes[DTTree::FindNode(Node)].Cost;
		TestEqual(TEXT("a sealed Keeper's branch says so, all the way down"), Tab.RowValue(Row).ToString(), StateCost(Sealed, Cost));
		TestTrue(TEXT("grey"), Tab.IsDimmed(Row));
	}
	TestTrue(TEXT("with the Shrine's hint"), Tab.RowDetail(TreeRow(Tab, TEXT("Branch.Shade.Blessing4"))).ToString().Contains(Hint));
	Session.Profile.Currency = 0;
	Session.Profile.Unlocked.Add(DTOffers::KeeperId(EDTPath::Shade));
	TestEqual(TEXT("a met Keeper's branch is open"), Tab.RowValue(TreeRow(Tab, TEXT("Branch.Shade.Blessing4"))).ToString(), StateCost(NotEnough, DTConst::TREE_COST_BLESSING4));
	Session.Profile.Unlocked.Remove(DTOffers::KeeperId(EDTPath::Shade));

	Session.Profile.TreeRanks.Add(TEXT("Trunk.Reroll"), 1);
	TestEqual(TEXT("bought: Complete, no price"), Tab.RowValue(Reroll).ToString(), TreeText(TEXT("Tree.State.Maxed")));
	TestFalse(TEXT("and not grey"), Tab.IsDimmed(Reroll));

	// Every row in both cultures, in every state a row can read: no key, no brace, no raw id reaches the screen.
	const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	for (const TCHAR* Name : { TEXT("en"), TEXT("zh-Hans") })
	{
		FInternationalization::Get().SetCurrentCulture(Name);
		for (int32 Row = 0; Row < Tab.NumRows(); ++Row)
		{
			for (const FText& Line : { Tab.RowLabel(Row), Tab.RowValue(Row), Tab.RowDetail(Row) })
			{
				const FString Text = Line.ToString();
				TestFalse(FString::Printf(TEXT("%s row %d reads clean: %s"), Name, Row, *Text), Text.Contains(TEXT("Tree.")) || Text.Contains(TEXT("Hub.")) || Text.Contains(TEXT("Keeper.")) || Text.Contains(TEXT("{")) || Text.Contains(TEXT("<MISSING")));
			}
		}
		TestFalse(TEXT("the footer resolves"), TreeText(TEXT("Tree.Hint")).Contains(TEXT("<MISSING")));
	}
	FInternationalization::Get().SetCurrentCulture(Culture);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigTreeBuy, "Dusktracer.Hub.TreeBuy", DTHubTest::Flags) // T-VIG-19
bool FDTVigTreeBuy::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	FDTMetaSession Session;
	Session.Open(TempSave(TEXT("buy.sav")));
	FDTTreeTab Tab(Session);
	const int32 Scatter = TreeRow(Tab, TEXT("Trunk.ScatterCannon"));
	const int32 Rail = TreeRow(Tab, TEXT("Trunk.RailLance"));
	const int32 Next = TreeRow(Tab, TEXT("Branch.Storm.Blessing5"));
	const int32 Sealed = TreeRow(Tab, TEXT("Branch.Shade.Blessing4"));

	Tab.Accept(Scatter);
	TestTrue(TEXT("short: nothing bought"), Session.Profile.TreeRanks.IsEmpty());
	Session.Profile.Currency = DTConst::TREE_COST_WEAPON;
	Tab.Accept(0);
	Tab.Accept(Next);
	Tab.Accept(Sealed);
	Tab.Accept(INDEX_NONE);
	Tab.Accept(Tab.NumRows());
	TestTrue(TEXT("a heading, a locked row, a sealed row and a row off the page buy nothing"), Session.Profile.TreeRanks.IsEmpty());
	TestEqual(TEXT("and spend nothing"), Session.Profile.Currency, static_cast<int64>(DTConst::TREE_COST_WEAPON));

	Tab.Accept(Scatter);
	TestEqual(TEXT("exact money buys"), Session.Profile.TreeRanks.FindRef(TEXT("Trunk.ScatterCannon")), 1);
	TestEqual(TEXT("and leaves nothing"), Session.Profile.Currency, 0LL);
	TestTrue(TEXT("the weapon joins Unlocked"), Session.Profile.Unlocked.Contains(TEXT("Weapon.ScatterCannon")));
	TestTrue(TEXT("the row says it was signed"), Tab.RowDetail(Scatter).ToString().Contains(FText::Format(FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.Purchased")), DTTree::NodeName(DTTree::FindNode(TEXT("Trunk.ScatterCannon")))).ToString()));
	TestFalse(TEXT("only that row"), Tab.RowDetail(Rail).ToString().Contains(TEXT("\n")));
	Tab.Opened();
	TestFalse(TEXT("left and opened again, the page says nothing was just signed"), Tab.RowDetail(Scatter).ToString().Contains(TEXT("\n")));

	// Read back from the file Accept wrote, before anything else touches it.
	FDTMetaSession Reader;
	TestEqual(TEXT("a reload reads the file"), Reader.Open(Session.Path), EDTMetaOpen::Read);
	TestEqual(TEXT("the rank is in it"), Reader.Profile.TreeRanks.FindRef(TEXT("Trunk.ScatterCannon")), 1);
	TestEqual(TEXT("and the balance"), Reader.Profile.Currency, 0LL);
	TestTrue(TEXT("and the unlock"), Reader.Profile.Unlocked.Contains(TEXT("Weapon.ScatterCannon")));
	FDTTreeTab Again(Reader);
	TestEqual(TEXT("a new page reads it Complete"), Again.RowValue(Scatter).ToString(), TreeText(TEXT("Tree.State.Maxed")));
	TestFalse(TEXT("with no signed line"), Again.RowDetail(Scatter).ToString().Contains(TEXT("\n")));

	Session.Profile.Currency = DTConst::TREE_COST_WEAPON;
	Tab.Accept(Scatter);
	TestEqual(TEXT("a bought row buys nothing"), Session.Profile.Currency, static_cast<int64>(DTConst::TREE_COST_WEAPON));

	// The save's lock is another launch's (meta.md R-MET-11): the press is refused, and the row says so in each culture.
	{
		AddExpectedError(TEXT("is locked by another launch"), EAutomationExpectedErrorFlags::Contains, 0);
		const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
		{
			const FDTMetaLock Other(Session.Path);
			TestTrue(TEXT("another launch holds the lock"), Other.IsHeld());
			Tab.Accept(Rail);
			TestTrue(TEXT("a busy save sells nothing"), Session.Profile.TreeRanks.FindRef(TEXT("Trunk.RailLance")) == 0 && Session.Profile.Currency == DTConst::TREE_COST_WEAPON);
			TArray<FString> Lines;
			for (const TCHAR* Name : { TEXT("en"), TEXT("zh-Hans") })
			{
				FInternationalization::Get().SetCurrentCulture(Name);
				Lines.Add(TreeText(TEXT("Tree.State.Busy")));
				TestFalse(FString::Printf(TEXT("%s: the busy line resolves"), Name), Lines.Last().IsEmpty() || Lines.Last().Contains(TEXT("Tree.")));
				TestTrue(FString::Printf(TEXT("%s: the row says the save is in use"), Name), Tab.RowDetail(Rail).ToString().EndsWith(TEXT("\n") + Lines.Last()));
			}
			FInternationalization::Get().SetCurrentCulture(Culture);
			// The editor process does not swap the game's text with the culture, so the two languages are read where they are written.
			FString Csv;
			FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DT/Text/ST_Tree.csv")), FFileHelper::EHashOptions::None);
			TestTrue(TEXT("the busy row carries both languages"), Csv.Contains(TEXT("Tree.State.Busy,The save is in use by another game. Press again.,存档正被另一个游戏使用，请再按一次。,")));
			TestFalse(TEXT("only that row"), Tab.RowDetail(Scatter).ToString().Contains(TreeText(TEXT("Tree.State.Busy"))));
			TestEqual(TEXT("its state is as it was: open, at its price"), Tab.RowValue(Rail).ToString(), FText::Format(FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.Cost")), FText::AsNumber(DTConst::TREE_COST_WEAPON)).ToString());
		}
		Tab.Accept(Rail);
		TestEqual(TEXT("pressed again with the lock free, it buys"), Session.Profile.TreeRanks.FindRef(TEXT("Trunk.RailLance")), 1);
		TestFalse(TEXT("and the busy line is gone"), Tab.RowDetail(Rail).ToString().Contains(TreeText(TEXT("Tree.State.Busy"))));
	}

	// With no writable file every row says so, and the page still backs out: reading it is all it does.
	FDTMetaSession None;
	None.Open(FString());
	None.Profile.Currency = 1000000;
	FDTTreeTab Page(None);
	Page.Accept(Scatter);
	TestTrue(TEXT("no file, no sale"), None.Profile.TreeRanks.IsEmpty());
	TestEqual(TEXT("nothing spent"), None.Profile.Currency, 1000000LL);
	const FString ReadOnly = TreeText(TEXT("Tree.State.ReadOnly"));
	TestTrue(TEXT("the row says why"), Page.RowDetail(Scatter).ToString().Contains(ReadOnly));
	FDTMetaSession Stuck;
	Stuck.Open(TempSave(TEXT("stuck.sav")));
	Stuck.bWritable = false;
	FDTTreeTab Held(Stuck);
	TestTrue(TEXT("a stuck session says the same"), Held.RowDetail(Scatter).ToString().Contains(ReadOnly));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigTreeFocus, "Dusktracer.Hub.TreeFocus", DTHubTest::Flags) // T-VIG-20
bool FDTVigTreeFocus::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	FDTHubState State = Open();
	State.TreeRows = 15;
	State.TreeHeadings = { 0, 7, 11 };
	State.Window = 4;
	Focus(State, EDTHubRow::Tree);
	State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("the page opens past its first heading"), State.Focus, 1);
	State.Apply(EDTMenuCommand::Up);
	TestEqual(TEXT("up wraps past it to the last row"), State.Focus, 14);
	State.Apply(EDTMenuCommand::Down);
	TestEqual(TEXT("down wraps past it"), State.Focus, 1);
	TestEqual(TEXT("and the heading is back in view"), State.Scroll, 0);
	State.Focus = 6;
	State.Apply(EDTMenuCommand::Down);
	TestEqual(TEXT("focus steps over a heading"), State.Focus, 8);
	State.Apply(EDTMenuCommand::Up);
	TestEqual(TEXT("both ways"), State.Focus, 6);
	State.Focus = 3;
	State.Apply(EDTMenuCommand::Right);
	TestEqual(TEXT("a page right that lands on a heading goes on past it"), State.Focus, 8);
	State.Apply(EDTMenuCommand::Right);
	State.Apply(EDTMenuCommand::Right);
	TestEqual(TEXT("and stops at the end"), State.Focus, 14);
	State.Focus = 4;
	State.Apply(EDTMenuCommand::Left);
	TestEqual(TEXT("a page left that lands on the first heading turns back from the end"), State.Focus, 1);
	State.Focus = 9;
	State.Scroll = 9;
	State.Apply(EDTMenuCommand::Up);
	TestEqual(TEXT("up to a group's first row"), State.Focus, 8);
	TestEqual(TEXT("brings its heading into the window"), State.Scroll, 7);
	// Not only a group's first row (R-VIG-20): any row reached by scrolling back, when the heading and the row fit one window.
	State.Focus = 10;
	State.Scroll = 10;
	State.Apply(EDTMenuCommand::Up);
	TestEqual(TEXT("up to a group's second row"), State.Focus, 9);
	TestEqual(TEXT("brings its heading in too"), State.Scroll, 7);
	State.Focus = 14;
	State.Scroll = 11;
	State.Apply(EDTMenuCommand::Left);
	TestEqual(TEXT("a page left into the middle of a group"), State.Focus, 10);
	TestEqual(TEXT("brings that group's heading in"), State.Scroll, 7);
	State.Focus = 12;
	State.Scroll = 11;
	State.Apply(EDTMenuCommand::Down);
	State.Apply(EDTMenuCommand::Down);
	TestEqual(TEXT("walking down a group"), State.Focus, 14);
	TestEqual(TEXT("keeps its heading in the window while it fits"), State.Scroll, 11);
	State.Focus = 6;
	State.Scroll = 6;
	State.Apply(EDTMenuCommand::Up);
	TestEqual(TEXT("a row too far under its heading for one window"), State.Focus, 5);
	TestEqual(TEXT("scrolls by the row alone"), State.Scroll, 5);
	State.Focus = 9;
	State.Scroll = 9;
	State.Apply(EDTMenuCommand::Up);
	const FDTHubOutcome Outcome = State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("Accept names the row"), Outcome.Row, 8);
	State.Apply(EDTMenuCommand::Back);
	TestEqual(TEXT("Back lands on the row that opened the page"), State.RootRow(), EDTHubRow::Tree);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigRunEnd, "Dusktracer.Hub.RunEnd", DTHubTest::Flags) // T-VIG-13
bool FDTVigRunEnd::RunTest(const FString& Parameters)
{
	FDTUserSettings S;
	FDTMenuState Menu;
	Menu.OpenRunOver(EDTRunOverKind::Abandoned);
	Menu.bHubRow = true;
	TestEqual(TEXT("one more row"), Menu.NumRows(), static_cast<int32>(EDTRunOverRow::Num) + 1);
	TestEqual(TEXT("the way back is first"), FString(DTMenuText::PlainRowKey(Menu, 0)), FString(TEXT("RunSummary.ReturnTo")));
	TestEqual(TEXT("Restart is next"), FString(DTMenuText::PlainRowKey(Menu, 1)), FString(TEXT("Pause.Restart")));
	TestEqual(TEXT("Quit last"), FString(DTMenuText::PlainRowKey(Menu, 2)), FString(TEXT("Pause.Quit")));
	TestEqual(TEXT("Accept goes back"), Menu.Apply(EDTMenuCommand::Accept, S).Action, EDTMenuAction::ReturnToHub);
	Menu.Apply(EDTMenuCommand::Down, S);
	TestEqual(TEXT("Restart still restarts"), Menu.Apply(EDTMenuCommand::Accept, S).Action, EDTMenuAction::Restart);
	Menu.Apply(EDTMenuCommand::Down, S);
	TestEqual(TEXT("Quit still quits"), Menu.Apply(EDTMenuCommand::Accept, S).Action, EDTMenuAction::Quit);
	Menu.Apply(EDTMenuCommand::Back, S);
	TestEqual(TEXT("Back still does nothing"), Menu.Screen, EDTMenuScreen::RunOver);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigReadOnly, "Dusktracer.Hub.ReadOnly", DTHubTest::Flags) // T-VIG-14
bool FDTVigReadOnly::RunTest(const FString& Parameters)
{
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer/Hub")));
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *Dir, TEXT("*.*"));
	TestTrue(TEXT("found the hub"), Files.Num() > 5);
	for (const FString& File : Files)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, File));
		TestFalse(FString::Printf(TEXT("%s holds no writable session"), *File), Text.Contains(TEXT("FindSession")) || Text.Contains(TEXT("FDTMetaSession")) || Text.Contains(TEXT("->Write(")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigText, "Dusktracer.Hub.TextKeys", DTHubTest::Flags) // T-VIG-15
bool FDTVigText::RunTest(const FString& Parameters)
{
	const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(TEXT("ST_UI"));
	if (!TestTrue(TEXT("ST_UI"), Table.IsValid()))
	{
		return false;
	}
	for (const FString& Key : DTHubText::AllKeys())
	{
		TestTrue(FString::Printf(TEXT("%s is in ST_UI"), *Key), Table->FindEntry(Key).IsValid());
	}
	for (int32 Row = 0; Row < static_cast<int32>(EDTHubRow::Num); ++Row)
	{
		TestFalse(TEXT("a root label"), DTHubText::RootLabel(static_cast<EDTHubRow>(Row)).ToString().Contains(TEXT("<MISSING")));
	}
	for (const EDTPath Path : DTOffers::AllKeepers())
	{
		TestFalse(TEXT("a Keeper's name"), DTHubText::KeeperName(Path).ToString().Contains(TEXT("<MISSING")));
	}
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer/Hub")));
	FString Text;
	TestTrue(TEXT("the screen"), FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, TEXT("DTHubScreen.cpp"))));
	TestTrue(TEXT("colour comes from the tokens"), Text.Contains(TEXT("DTPal::UI_")));
	TestFalse(TEXT("no literal colour"), Text.Contains(TEXT("FColor(")) || Text.Contains(TEXT("FLinearColor(")) || Text.Contains(TEXT("FromHex")));
	TestFalse(TEXT("no combat token"), Text.Contains(TEXT("DTPal::Enemy")) || Text.Contains(TEXT("DTPal::Telegraph")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigNotice, "Dusktracer.Hub.Notice", DTHubTest::Flags) // T-VIG-21
bool FDTVigNotice::RunTest(const FString& Parameters)
{
	using namespace DTHubTest;
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	TestTrue(TEXT("nothing to say, no line"), DTHubText::Notice(EDTMetaNotice::None, TEXT("DTMeta.sav.bad.1")).IsEmpty());
	const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	FInternationalization::Get().SetCurrentCulture(TEXT("en"));
	const FText Aside = DTHubText::Notice(EDTMetaNotice::KeptAside, TEXT("DTMeta.sav.bad.1"));
	const FText Lost = DTHubText::Notice(EDTMetaNotice::RunLost, FString());
	const FText Unreadable = DTHubText::Notice(EDTMetaNotice::SaveUnreadable, TEXT("DTMeta.sav"));
	const FText NotMoved = DTHubText::Notice(EDTMetaNotice::SaveNotMoved, TEXT("DTMeta.sav"));
	TestEqual(TEXT("a save set aside names the file it is kept as"), Aside.ToString(), FString(TEXT("The save could not be read. It is kept as DTMeta.sav.bad.1, and a new one starts empty.")));
	TestEqual(TEXT("a run the last launch could not write"), Lost.ToString(), FString(TEXT("The last run could not be saved. Nothing from it is in this save.")));
	TestEqual(TEXT("a save that cannot be opened names the save"), Unreadable.ToString(),
		FString(TEXT("The save DTMeta.sav could not be opened. This session starts empty and saves nothing; the file is left as it is.")));
	TestEqual(TEXT("a bad save that could not be set aside names the save"), NotMoved.ToString(),
		FString(TEXT("The save DTMeta.sav could not be read and could not be set aside. This session starts empty and saves nothing; the file is left as it is.")));
	FInternationalization::Get().SetCurrentCulture(Culture);
	TestTrue(TEXT("both keys are the hub's"), DTHubText::AllKeys().Contains(TEXT("Hub.Notice.SaveAside")) && DTHubText::AllKeys().Contains(TEXT("Hub.Notice.RunLost")));
	TestTrue(TEXT("both stuck keys are the hub's"), DTHubText::AllKeys().Contains(TEXT("Hub.Notice.SaveUnreadable")) && DTHubText::AllKeys().Contains(TEXT("Hub.Notice.SaveNotMoved")));
	// The editor process does not swap the game's text with the culture, so the two languages are read where they are written.
	FString Csv;
	FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DT/Text/ST_UI.csv")), FFileHelper::EHashOptions::None);
	TestTrue(TEXT("the set-aside row carries both languages"), Csv.Contains(TEXT("Hub.Notice.SaveAside,\"The save could not be read. It is kept as {0}, and a new one starts empty.\",存档无法读取，已保留为 {0}。现在用的是一份空白的新存档。,")));
	TestTrue(TEXT("the lost-run row carries both languages"), Csv.Contains(TEXT("Hub.Notice.RunLost,The last run could not be saved. Nothing from it is in this save.,上一局没能保存。这份存档里没有那一局的记录。,")));
	TestTrue(TEXT("the unreadable row carries both languages"), Csv.Contains(TEXT("Hub.Notice.SaveUnreadable,The save {0} could not be opened. This session starts empty and saves nothing; the file is left as it is.,存档 {0} 打不开。这次用的是一份空白存档，不会保存任何内容；原文件保持不动。,")));
	TestTrue(TEXT("the not-moved row carries both languages"), Csv.Contains(TEXT("Hub.Notice.SaveNotMoved,The save {0} could not be read and could not be set aside. This session starts empty and saves nothing; the file is left as it is.,存档 {0} 无法读取，也没能另行保留。这次用的是一份空白存档，不会保存任何内容；原文件保持不动。,")));

	// Where it is drawn: the root's detail, on every row, and only there. What Begin lacks comes first on Begin.
	const TArray<FDTCodexRow> Book;
	const FDTMetaProfile Profile;
	FDTHubState State = Open();
	FDTHubView View;
	View.State = &State;
	View.Codex = &Book;
	View.Profile = &Profile;
	for (int32 Row = 0; Row < static_cast<int32>(EDTHubRow::Num); ++Row)
	{
		Focus(State, static_cast<EDTHubRow>(Row));
		View.Notice = FText::GetEmpty();
		const FString Bare = DTHubText::Detail(View).ToString();
		View.Notice = Lost;
		const FString Said = DTHubText::Detail(View).ToString();
		if (static_cast<EDTHubRow>(Row) == EDTHubRow::Begin)
		{
			TestTrue(TEXT("Begin with no Patron says what it lacks, with or without a notice"), !Bare.IsEmpty() && Said == Bare);
		}
		else
		{
			TestTrue(FString::Printf(TEXT("root row %d: nothing without a notice, the notice with one"), Row), Bare.IsEmpty() && Said == Lost.ToString());
		}
	}
	FDTHubState Chosen = Open(EDTPath::Storm);
	Chosen.Selected = EDTPath::Storm;
	Focus(Chosen, EDTHubRow::Begin);
	View.State = &Chosen;
	TestEqual(TEXT("Begin with a Patron chosen shows the notice"), DTHubText::Detail(View).ToString(), Lost.ToString());
	Focus(Chosen, EDTHubRow::Stats);
	Chosen.Apply(EDTMenuCommand::Accept);
	TestTrue(TEXT("no other page draws it"), Chosen.Page == EDTHubPage::Stats && DTHubText::Detail(View).IsEmpty());

	// The session decides which line: a bad save set aside comes before a run the last launch lost, and a launch says each once.
	const FString Path = TempSave(TEXT("Notice.sav"));
	IFileManager::Get().Delete(*DTMeta::LostPath(Path), false, true, true);
	IFileManager::Get().Delete(*(Path + TEXT(".bad.1")), false, true, true);
	FDTMetaSession Plain;
	Plain.Open(Path);
	TestEqual(TEXT("a fresh save has nothing to say"), Plain.Notice(), EDTMetaNotice::None);
	TestTrue(TEXT("a marker is left"), FFileHelper::SaveStringToFile(TEXT("1"), *DTMeta::LostPath(Path)));
	FDTMetaSession AfterLoss;
	AfterLoss.Open(Path);
	TestEqual(TEXT("the launch after a lost run says so"), AfterLoss.Notice(), EDTMetaNotice::RunLost);
	TestTrue(TEXT("a marker and a bad save"), FFileHelper::SaveStringToFile(TEXT("1"), *DTMeta::LostPath(Path)) && FFileHelper::SaveStringToFile(TEXT("not a save"), *Path));
	FDTMetaSession Both;
	TestEqual(TEXT("the bad save is set aside"), Both.Open(Path), EDTMetaOpen::KeptAside);
	TestTrue(TEXT("both hold, and the save set aside is the line"), Both.LostRuns == 1 && Both.Notice() == EDTMetaNotice::KeptAside);
	TestEqual(TEXT("the line names the kept file"), DTHubText::Notice(Both.Notice(), FPaths::GetCleanFilename(Both.AsidePath)).ToString().Contains(TEXT("Notice.sav.bad.1")), true);
	FDTMetaSession Next;
	Next.Open(Path);
	TestEqual(TEXT("the launch after that says neither"), Next.Notice(), EDTMetaNotice::None);

#if PLATFORM_WINDOWS
	// A stuck session's line comes before the rest, and a save still stuck says so again (the seams of T-MET-12).
	IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
	const FString Locked = TempSave(TEXT("NoticeUnreadable.sav"));
	IFileManager::Get().Delete(*DTMeta::LostPath(Locked), false, true, true);
	IFileManager::Get().Delete(*(Locked + TEXT(".bad.1")), false, true, true);
	FDTMetaSession Writer;
	Writer.Open(Locked);
	TestTrue(TEXT("a good save lands"), Writer.Write() && FFileHelper::SaveStringToFile(TEXT("1"), *DTMeta::LostPath(Locked)));
	{
		TUniquePtr<IFileHandle> Hold(Platform.OpenWrite(*Locked, /*bAppend*/ true, /*bAllowRead*/ false));
		TestTrue(TEXT("held with no reader allowed"), Hold.IsValid());
		FDTMetaSession Stuck;
		TestEqual(TEXT("a file that cannot be read sticks the session"), Stuck.Open(Locked), EDTMetaOpen::Stuck);
		TestTrue(TEXT("a marker and a file it cannot read: the stuck line, nothing set aside, the marker read"),
			Stuck.Notice() == EDTMetaNotice::SaveUnreadable && Stuck.AsidePath.IsEmpty() && Stuck.LostRuns == 1);
		TestTrue(TEXT("the line names the save"), DTHubText::Notice(Stuck.Notice(), FPaths::GetCleanFilename(Stuck.Path)).ToString().Contains(TEXT("NoticeUnreadable.sav")));
		FDTMetaSession Again;
		Again.Open(Locked);
		TestEqual(TEXT("the launch after, still stuck, says so again"), Again.Notice(), EDTMetaNotice::SaveUnreadable);
	}
	FDTMetaSession Freed;
	TestTrue(TEXT("once it can be read, the launch after says neither"), Freed.Open(Locked) == EDTMetaOpen::Read && Freed.Notice() == EDTMetaNotice::None);

	const FString Held = TempSave(TEXT("NoticeHeld.sav"));
	IFileManager::Get().Delete(*DTMeta::LostPath(Held), false, true, true);
	IFileManager::Get().Delete(*(Held + TEXT(".bad.1")), false, true, true);
	TestTrue(TEXT("a marker and a bad save"), FFileHelper::SaveStringToFile(TEXT("1"), *DTMeta::LostPath(Held)) && FFileHelper::SaveStringToFile(TEXT("not a save"), *Held));
	{
		TUniquePtr<IFileHandle> Hold(Platform.OpenRead(*Held));
		TestTrue(TEXT("held open"), Hold.IsValid());
		FDTMetaSession Stuck;
		TestEqual(TEXT("held open it cannot move"), Stuck.Open(Held), EDTMetaOpen::Stuck);
		TestTrue(TEXT("a marker and a bad file it cannot move: the stuck line, nothing set aside, the marker read"),
			Stuck.Notice() == EDTMetaNotice::SaveNotMoved && Stuck.AsidePath.IsEmpty() && Stuck.LostRuns == 1 && !IFileManager::Get().FileExists(*(Held + TEXT(".bad.1"))));
		TestTrue(TEXT("the line names the save"), DTHubText::Notice(Stuck.Notice(), FPaths::GetCleanFilename(Stuck.Path)).ToString().Contains(TEXT("NoticeHeld.sav")));
		FDTMetaSession Again;
		Again.Open(Held);
		TestEqual(TEXT("the launch after, still stuck, says so again"), Again.Notice(), EDTMetaNotice::SaveNotMoved);
	}
	FDTMetaSession Moved;
	TestTrue(TEXT("once it can move, the launch after sets it aside"), Moved.Open(Held) == EDTMetaOpen::KeptAside && Moved.Notice() == EDTMetaNotice::KeptAside);
	FDTMetaSession Quiet;
	Quiet.Open(Held);
	TestEqual(TEXT("and the launch after that says neither"), Quiet.Notice(), EDTMetaNotice::None);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigLayout, "Dusktracer.Hub.Layout", DTHubTest::Flags) // T-VIG-16
bool FDTVigLayout::RunTest(const FString& Parameters)
{
	const float Largest = static_cast<float>(DTConst::SETTINGS_TEXT_MAX_PCT) / DTConst::SETTINGS_TEXT_MIN_PCT;
	TestTrue(TEXT("the panel stays in a 1080-line canvas"), DTHubLayout::PanelHeight() <= 1080.0f);
	TestTrue(TEXT("every root row fits at the largest text"), DTHubLayout::ListWindow(Largest) >= static_cast<int32>(EDTHubRow::Num));
	const int32 Keepers = DTOffers::AllKeepers().Num();
	TestTrue(TEXT("every Keeper fits"), DTHubLayout::ListWindow(Largest) >= Keepers);
	TestTrue(TEXT("the Ledger-stone fits, having no detail pane"), DTHubLayout::ListWindow(Largest, false) >= DTHubData::Stats(FDTMetaProfile()).Num());
	TMap<EDTPath, int32> Group;
	for (int32 Row = 0; Row < DTCat::NumTreeNodes; ++Row)
	{
		++Group.FindOrAdd(DTCat::TreeNodes[Row].Path);
	}
	for (const TPair<EDTPath, int32>& Each : Group)
	{
		TestTrue(TEXT("each group of Contracts fits one window with its heading"), DTHubLayout::ListWindow(Largest) >= Each.Value + 1);
	}
	TestTrue(TEXT("a smaller text fits more rows"), DTHubLayout::ListWindow(1.0f) > DTHubLayout::ListWindow(Largest));
	return true;
}

#endif
