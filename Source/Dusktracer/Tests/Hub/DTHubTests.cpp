// The hub (02_Systems/vigil.md). T-VIG-17 is Tools/VigilCheck.ps1: real launches, real keys, shots of every page.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Hub; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Hub/DTHubData.h"
#include "Hub/DTHubFlow.h"
#include "Hub/DTHubScreen.h"
#include "Hub/DTHubState.h"
#include "Hub/DTHubTab.h"
#include "HAL/FileManager.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Meta/DTMetaSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"
#include "Settings/DTMenuScreen.h"

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
	TestTrue(TEXT("focus on it shows the hint"), DTHubText::Detail(View).EqualTo(FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine.LockedHint"))));

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

	// Every row, read: a title and a body from the tables, in this culture, never a missing key.
	FDTMetaProfile Everything;
	Everything.Wins = DTConst::BOSS_FORM_III_WINS + 1;
	Everything.LifetimeStats.Add(TEXT("BossKills"), 1);
	for (const FDTCodexSource& Source : Sources)
	{
		FString Tail;
		if (Source.Unlock.Split(TEXT("Discovery."), nullptr, &Tail))
		{
			Everything.Discoveries.Add(FName(*Tail));
		}
		if (Source.Unlock.Split(TEXT("Kill."), nullptr, &Tail))
		{
			Everything.LifetimeStats.Add(FName(*(TEXT("Kills.Role.") + Tail)), 1);
		}
	}
	const TArray<FDTCodexRow> Whole = DTHubData::BuildCodex(Sources, Everything);
	TestEqual(TEXT("all but the format row"), Whole.Num(), Sources.Num() - 1);
	for (const FDTCodexRow& Row : Whole)
	{
		TestTrue(FString::Printf(TEXT("%s is read"), *Row.Key), Row.bRead);
		TestFalse(FString::Printf(TEXT("%s has a title"), *Row.Key), Row.Title.ToString().Contains(TEXT("<MISSING")) || Row.Title.IsEmpty());
		TestFalse(FString::Printf(TEXT("%s has its text"), *Row.Key), Row.Body.ToString().Contains(TEXT("<MISSING")));
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
	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		TestEqual(TEXT("the value"), Rows[Index].Value.ToString(), FString(Want[Index]));
		TestFalse(TEXT("the label"), Rows[Index].Label.ToString().Contains(TEXT("<MISSING")));
	}
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
	TestFalse(TEXT("no factory, no tab"), DTHubTabs::MakeTree(nullptr).IsValid());
	FDTHubState State = Open();
	Focus(State, EDTHubRow::Tree);
	State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("the empty slot is a page"), State.Page, EDTHubPage::Tree);
	TestEqual(TEXT("Accept does nothing on it"), State.Apply(EDTMenuCommand::Accept).Action, EDTHubAction::None);

	TSharedPtr<FFakeTab> Fake = MakeShared<FFakeTab>();
	DTHubTabs::SetTreeFactory([Fake](UWorld*) { return Fake; });
	const TSharedPtr<IDTHubTab> Tab = DTHubTabs::MakeTree(nullptr);
	DTHubTabs::SetTreeFactory(nullptr);
	TestTrue(TEXT("the factory builds the tab"), Tab == Fake);
	State.TreeRows = Tab->NumRows();
	State.Apply(EDTMenuCommand::Down);
	const FDTHubOutcome Outcome = State.Apply(EDTMenuCommand::Accept);
	TestEqual(TEXT("Accept reaches the page"), Outcome.Action, EDTHubAction::TreeAccept);
	TestEqual(TEXT("with the row"), Outcome.Row, 1);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTVigLayout, "Dusktracer.Hub.Layout", DTHubTest::Flags) // T-VIG-16
bool FDTVigLayout::RunTest(const FString& Parameters)
{
	const float Largest = static_cast<float>(DTConst::SETTINGS_TEXT_MAX_PCT) / DTConst::SETTINGS_TEXT_MIN_PCT;
	TestTrue(TEXT("the panel stays in a 1080-line canvas"), DTHubLayout::PanelHeight() <= 1080.0f);
	TestTrue(TEXT("every root row fits at the largest text"), DTHubLayout::ListWindow(Largest) >= static_cast<int32>(EDTHubRow::Num));
	const int32 Keepers = DTOffers::AllKeepers().Num();
	TestTrue(TEXT("every Keeper fits"), DTHubLayout::ListWindow(Largest) >= Keepers);
	TestTrue(TEXT("the Ledger-stone fits, having no detail pane"), DTHubLayout::ListWindow(Largest, false) >= DTHubData::Stats(FDTMetaProfile()).Num());
	TestTrue(TEXT("a smaller text fits more rows"), DTHubLayout::ListWindow(1.0f) > DTHubLayout::ListWindow(Largest));
	return true;
}

#endif
