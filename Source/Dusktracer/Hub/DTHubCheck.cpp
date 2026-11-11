#include "Hub/DTHubCheck.h"

#if !UE_BUILD_SHIPPING
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/DTConstantsGen.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Hub/DTHubSubsystem.h"
#include "Hub/DTHubTab.h"
#include "InputKeyEventArgs.h"
#include "Meta/DTMetaSubsystem.h"
#include "Meta/DTTree.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferSubsystem.h"
#include "Settings/DTMenuSubsystem.h"
#include "UnrealClient.h"

namespace DTHubCheckLocal
{
	// Wall seconds: how long a key is held and left, how long a shot's frame settles, when a step gives up.
	constexpr double KeyS = 0.15;
	constexpr double ShotSettleS = 0.5;
	constexpr double ShotPollS = 0.25;
	constexpr double GiveUpS = 60.0;
	// Begin's key is held this long before the proof reads that no run started: many frames, and a map reload would have landed.
	constexpr double BeginHeldS = 1.0;
	// Sim steps a run plays before the proof presses anything in it.
	constexpr int64 ReadyStep = 60;
	const TCHAR* const ScatterNode = TEXT("Trunk.ScatterCannon");
	const TCHAR* const StormNode = TEXT("Branch.Storm.Blessing4");
	const TCHAR* const StormNext = TEXT("Branch.Storm.Blessing5");
	const TCHAR* const SealedNode = TEXT("Branch.Shade.Blessing4");
	const TCHAR* const FirstNode = TEXT("Trunk.Reroll");
	const TCHAR* const StormCard = TEXT("Blessing.Storm.Tempest.Amp1");

	/** What Tools/EarnedCheck.ps1 buys by key on the planted save, in catalog order: a Blessing4 on two Paths is TwoPaths6's door, and the last three are the new branches. */
	struct FDoorNode
	{
		const TCHAR* Node;
		const TCHAR* Card;
	};
	const FDoorNode DoorNodes[] = {
		{ StormNode, StormCard },
		{ TEXT("Branch.Forge.Blessing4"), TEXT("Blessing.Forge.Furnace.Amp1") }, // a fresh save holds Hammer (meta.md R-MET-25, D-064)
		{ SealedNode, TEXT("Blessing.Shade.Hound.Amp1") },
		{ TEXT("Branch.Radiance.Blessing4"), TEXT("Blessing.Radiance.Lance.Amp1") },
		{ TEXT("Branch.Quicksilver.Blessing4"), TEXT("Blessing.Quicksilver.Vector.Amp1") },
	};

	/** One step of the script: runs each tick until it says it is done. */
	using FStep = TFunction<bool(UDTHubSubsystem*)>;

	struct FCheck
	{
		TArray<FStep> Steps;
		int32 Index = 0;
		double StepAt = 0.0;
		int32 Fails = 0;
		bool bBuilt = false;
		bool bDone = false;
		int64 ShotSize = -1;
		double ShotPollAt = 0.0;
		bool bShotAsked = false;
		int32 WalkLeft = INDEX_NONE;
		int32 WalkBad = 0;
	};
	static FCheck Check;

	static double Elapsed()
	{
		return FPlatformTime::Seconds() - Check.StepAt;
	}

	static APlayerController* Controller(UDTHubSubsystem* Hub)
	{
		return Hub->GetWorld() ? Hub->GetWorld()->GetFirstPlayerController() : nullptr;
	}

	static void Send(UDTHubSubsystem* Hub, const FKey& Key, bool bDown)
	{
		if (APlayerController* PC = Controller(Hub))
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
		}
	}

	static void Expect(const FString& What, bool bGood)
	{
		Check.Fails += bGood ? 0 : 1;
		UE_LOG(LogDT, Log, TEXT("VIGILCHECK %s: %s"), *What, bGood ? TEXT("ok") : TEXT("WRONG"));
	}

	static void Press(const FKey& Key)
	{
		Check.Steps.Add([Key](UDTHubSubsystem* Hub) { Send(Hub, Key, true); return true; });
		Check.Steps.Add([Key](UDTHubSubsystem* Hub)
		{
			if (Elapsed() < KeyS)
			{
				return false;
			}
			Send(Hub, Key, false);
			return true;
		});
		Check.Steps.Add([](UDTHubSubsystem*) { return Elapsed() >= KeyS; });
	}

	static void Wait(double Seconds)
	{
		Check.Steps.Add([Seconds](UDTHubSubsystem*) { return Elapsed() >= Seconds; });
	}

	static void Until(TFunction<bool(UDTHubSubsystem*)> Condition)
	{
		Check.Steps.Add(MoveTemp(Condition));
	}

	static void Is(const FString& What, TFunction<bool(UDTHubSubsystem*)> Condition)
	{
		Check.Steps.Add([What, Condition = MoveTemp(Condition)](UDTHubSubsystem* Hub)
		{
			Expect(What, Condition(Hub));
			return true;
		});
	}

	/** Writes <-DTHubShots>/<Name>.png; passes with no directory given. */
	static void Shot(const FString& Name)
	{
		Check.Steps.Add([Name](UDTHubSubsystem*)
		{
			FString Dir;
			if (!FParse::Value(FCommandLine::Get(), TEXT("DTHubShots="), Dir) || Dir.IsEmpty())
			{
				return true;
			}
			if (Elapsed() < ShotSettleS)
			{
				return false;
			}
			const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Dir, Name + TEXT(".png")));
			if (!Check.bShotAsked)
			{
				Check.bShotAsked = true;
				IFileManager::Get().Delete(*Path, false, true, true);
				FScreenshotRequest::RequestScreenshot(Path, /*bInShowUI*/ true, /*bAddFilenameSuffix*/ false);
				Check.ShotSize = -1;
				Check.ShotPollAt = FPlatformTime::Seconds() + ShotPollS;
				return false;
			}
			if (FPlatformTime::Seconds() < Check.ShotPollAt)
			{
				return false;
			}
			const int64 Size = IFileManager::Get().FileSize(*Path);
			if (Size > 0 && Size == Check.ShotSize)
			{
				Check.bShotAsked = false;
				return true;
			}
			Check.ShotSize = Size;
			Check.ShotPollAt = FPlatformTime::Seconds() + ShotPollS;
			return false;
		});
	}

	/** Sets the balance by the debug command MetaCheck uses: no hub file holds a session, and a run of a second earns nothing to spend. */
	static void Fund(int64 Amount)
	{
		Check.Steps.Add([Amount](UDTHubSubsystem* Hub)
		{
			GEngine->Exec(Hub->GetWorld(), *FString::Printf(TEXT("DT.Meta.Currency %lld"), Amount));
			return true;
		});
	}

	/** Walks focus through the whole page, a row a frame, on the real font: every frame's overflow counts (R-VIG-16). */
	static void Walk(const FString& What)
	{
		Check.Steps.Add([What](UDTHubSubsystem* Hub)
		{
			if (Check.WalkLeft == INDEX_NONE)
			{
				Check.WalkLeft = Hub->GetState().NumRows();
				Check.WalkBad = 0;
				return false; // the frame after this tick is the first one read
			}
			if (Hub->GetOverflow() > 0)
			{
				++Check.WalkBad;
				UE_LOG(LogDT, Log, TEXT("VIGILCHECK %s: %d past their room on row %d"), *What, Hub->GetOverflow(), Hub->GetState().Focus);
			}
			if (Check.WalkLeft-- == 0)
			{
				Check.WalkLeft = INDEX_NONE;
				Expect(FString::Printf(TEXT("%s fits at the largest text, every row"), *What), Check.WalkBad == 0);
				return true;
			}
			Hub->Command(EDTMenuCommand::Down);
			return false;
		});
	}

	static bool IsPage(UDTHubSubsystem* Hub, EDTHubPage Page)
	{
		return Hub->IsActive() && Hub->GetState().Page == Page;
	}

	static bool OnRow(UDTHubSubsystem* Hub, EDTHubRow Row)
	{
		return IsPage(Hub, EDTHubPage::Root) && Hub->GetState().RootRow() == Row;
	}

	static UDTMenuSubsystem* Menu(UDTHubSubsystem* Hub)
	{
		return Hub->GetWorld() ? Hub->GetWorld()->GetSubsystem<UDTMenuSubsystem>() : nullptr;
	}

	static bool RunReady(UDTHubSubsystem* Hub)
	{
		UWorld* World = Hub->GetWorld();
		const UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
		return World && !Hub->IsActive() && World->URL.HasOption(TEXT("DTSeed")) && Sim && Sim->GetStepIndex() >= ReadyStep;
	}

	static const FDTMetaProfile& Profile(UDTHubSubsystem* Hub)
	{
		return *UDTMetaSubsystem::ReadProfile(Hub);
	}

	static int32 Rank(UDTHubSubsystem* Hub, const TCHAR* Node)
	{
		return Profile(Hub).TreeRanks.FindRef(FName(Node));
	}

	/** The page's row of a catalog node: its nodes in catalog order, with headings between. */
	static int32 TreeRow(UDTHubSubsystem* Hub, const TCHAR* Node)
	{
		const IDTHubTab* Tab = Hub->GetTree();
		int32 Left = DTTree::FindNode(FName(Node));
		for (int32 Row = 0; Tab && Row < Tab->NumRows(); ++Row)
		{
			if (!Tab->IsHeading(Row) && Left-- == 0)
			{
				return Row;
			}
		}
		return INDEX_NONE;
	}

	static bool OnNode(UDTHubSubsystem* Hub, const TCHAR* Node)
	{
		return IsPage(Hub, EDTHubPage::Tree) && Hub->GetState().Focus == TreeRow(Hub, Node);
	}

	static EDTPath RunPatron(UDTHubSubsystem* Hub)
	{
		UDTSimSubsystem* Sim = Hub->GetWorld() ? Hub->GetWorld()->GetSubsystem<UDTSimSubsystem>() : nullptr;
		return Sim && Sim->GetRun().GetNumPlayers() > 0 ? Sim->GetRun().GetPlayer(0).Offer.PatronPath : EDTPath::None;
	}

	/** Boot, each page, Shrine to a run, abandon, back. Keyboard here and there, pad on most of the backing out. */
	static void BuildFirst()
	{
		Is(TEXT("boot lands in the hub with no run"), [](UDTHubSubsystem* Hub)
		{
			UDTSimSubsystem* Sim = Hub->GetWorld()->GetSubsystem<UDTSimSubsystem>();
			return IsPage(Hub, EDTHubPage::Root) && Sim->GetRun().GetNumPlayers() == 0 && Sim->GetStepIndex() == 0 && !Controller(Hub)->GetPawn();
		});
		Is(TEXT("an empty profile chooses nobody"), [](UDTHubSubsystem* Hub) { return Hub->GetState().Selected == EDTPath::None && OnRow(Hub, EDTHubRow::Patron); });
		Shot(TEXT("root"));
		Press(EKeys::Enter);
		Is(TEXT("Accept opens the Shrine"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Patron) && Hub->GetState().Focus == 0; });
		Press(EKeys::Gamepad_DPad_Down);
		Press(EKeys::Down);
		Press(EKeys::S);
		Is(TEXT("focus reaches a sealed Keeper"), [](UDTHubSubsystem* Hub) { return !Hub->GetState().Patrons[Hub->GetState().Focus].bOpen; });
		Shot(TEXT("shrine"));
		Press(EKeys::Enter);
		Is(TEXT("Accept on a sealed Keeper chooses nobody"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Patron) && Hub->GetState().Selected == EDTPath::None; });
		Press(EKeys::Gamepad_FaceButton_Right);
		Is(TEXT("pad B leaves the Shrine"), [](UDTHubSubsystem* Hub) { return OnRow(Hub, EDTHubRow::Patron); });
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Is(TEXT("Contracts opens on the Tree's first node, past its heading"), [](UDTHubSubsystem* Hub) { return OnNode(Hub, TEXT("Trunk.Reroll")) && Hub->GetTree()->IsHeading(0); });
		Shot(TEXT("contracts"));
		Press(EKeys::Enter);
		Is(TEXT("Accept with an empty balance buys nothing"), [](UDTHubSubsystem* Hub) { return Profile(Hub).TreeRanks.IsEmpty() && IsPage(Hub, EDTHubPage::Tree); });
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Gamepad_FaceButton_Bottom);
		Is(TEXT("pad A opens the Codex on its first read row, at the top of the window, inside the reach"), [](UDTHubSubsystem* Hub)
		{
			const FDTHubState& State = Hub->GetState();
			const TArray<FDTCodexRow>& Book = Hub->GetCodex();
			return IsPage(Hub, EDTHubPage::Codex) && Book.IsValidIndex(State.Focus) && Book[State.Focus].bRead && State.Scroll == State.Focus
				&& !Book.ContainsByPredicate([](const FDTCodexRow& Row) { return Row.Layer > DTConst::NARR_PREWIN_LAYER_MAX; });
		});
		Shot(TEXT("codex"));
		Press(EKeys::Gamepad_FaceButton_Right);
		Press(EKeys::Gamepad_LeftStick_Down);
		Press(EKeys::SpaceBar);
		Is(TEXT("the left stick and Space reach the Ledger-stone"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Stats); });
		Shot(TEXT("ledger"));
		Press(EKeys::Gamepad_FaceButton_Right);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Is(TEXT("Settings opens the existing screen"), [](UDTHubSubsystem* Hub) { return Menu(Hub)->GetState().Screen == EDTMenuScreen::Settings; });
		Shot(TEXT("settings"));
		Press(EKeys::Gamepad_FaceButton_Right);
		Is(TEXT("B from Settings lands on its row, one level only"), [](UDTHubSubsystem* Hub) { return !Menu(Hub)->IsOpen() && OnRow(Hub, EDTHubRow::Settings); });
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Is(TEXT("Begin with no Patron opens the Shrine"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Patron); });
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Is(TEXT("the pick lands on Begin"), [](UDTHubSubsystem* Hub) { return Hub->GetState().Selected == EDTPath::Forge && OnRow(Hub, EDTHubRow::Begin); });
		Shot(TEXT("root_chosen"));
		// R-VIG-07: the key that chose Begin comes up first, so it never reaches the run (review #80).
		Check.Steps.Add([](UDTHubSubsystem* Hub) { Send(Hub, EKeys::SpaceBar, true); return true; });
		Wait(BeginHeldS);
		Is(TEXT("Begin waits while its key is held: the hub stands and no run has started"), [](UDTHubSubsystem* Hub)
		{
			return Hub->IsActive() && OnRow(Hub, EDTHubRow::Begin) && !Hub->GetWorld()->URL.HasOption(TEXT("DTSeed"));
		});
		Check.Steps.Add([](UDTHubSubsystem* Hub) { Send(Hub, EKeys::SpaceBar, false); return true; });
		Until(RunReady);
		Is(TEXT("the run plays on the chosen Patron with no hand left"), [](UDTHubSubsystem* Hub)
		{
			const UDTOfferSubsystem* Offers = Hub->GetWorld()->GetSubsystem<UDTOfferSubsystem>();
			return RunPatron(Hub) == EDTPath::Forge && Offers && !Offers->IsAnyHandOpen() && Controller(Hub)->GetPawn();
		});
		Press(EKeys::Escape);
		Is(TEXT("Esc pauses the run"), [](UDTHubSubsystem* Hub) { return Menu(Hub)->GetState().Screen == EDTMenuScreen::Pause; });
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Until([](UDTHubSubsystem* Hub) { return Menu(Hub)->GetState().Screen == EDTMenuScreen::RunOver; });
		Is(TEXT("the run-over screen offers the way back first"), [](UDTHubSubsystem* Hub)
		{
			const FDTMenuState& State = Menu(Hub)->GetState();
			return State.NumRows() == static_cast<int32>(EDTRunOverRow::Num) + 1 && State.Focus == 0;
		});
		Shot(TEXT("runover"));
		Wait(DTConst::SETTINGS_RUN_OVER_GUARD_S + KeyS);
		Press(EKeys::Enter);
		Until([](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Root); });
		Is(TEXT("back in the hub with a banked run and the Patron remembered"), [](UDTHubSubsystem* Hub)
		{
			const FDTMetaProfile* Profile = UDTMetaSubsystem::ReadProfile(Hub);
			return Profile && Profile->Runs == 1 && Profile->LastPatron == TEXT("Forge") && Hub->GetState().Selected == EDTPath::Forge
				&& !Hub->GetWorld()->URL.HasOption(TEXT("DTSeed")) && OnRow(Hub, EDTHubRow::Begin);
		});

		// Contracts by key events, both devices: two purchases at exact money, and every refusal spends nothing.
		Fund(DTConst::TREE_COST_WEAPON + DTConst::TREE_COST_BLESSING4);
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Press(EKeys::Gamepad_DPad_Down);
		Press(EKeys::Down);
		Press(EKeys::S);
		Is(TEXT("focus reaches the Scatter Cannon's node"), [](UDTHubSubsystem* Hub) { return OnNode(Hub, ScatterNode); });
		Press(EKeys::Enter);
		Is(TEXT("Enter buys it: rank, unlock and price"), [](UDTHubSubsystem* Hub)
		{
			return Rank(Hub, ScatterNode) == 1 && Profile(Hub).Unlocked.Contains(TEXT("Weapon.ScatterCannon")) && Profile(Hub).Currency == DTConst::TREE_COST_BLESSING4;
		});
		Shot(TEXT("contracts_bought"));
		Press(EKeys::SpaceBar);
		Is(TEXT("a second Accept buys nothing"), [](UDTHubSubsystem* Hub) { return Rank(Hub, ScatterNode) == 1 && Profile(Hub).Currency == DTConst::TREE_COST_BLESSING4; });
		Press(EKeys::Right);
		Is(TEXT("a page right lands on a node, past a heading"), [](UDTHubSubsystem* Hub) { return OnNode(Hub, SealedNode); });
		Shot(TEXT("contracts_sealed"));
		Press(EKeys::Enter);
		Is(TEXT("Accept on a sealed Keeper's node buys nothing"), [](UDTHubSubsystem* Hub) { return Rank(Hub, SealedNode) == 0 && Profile(Hub).Currency == DTConst::TREE_COST_BLESSING4; });
		Press(EKeys::Gamepad_DPad_Left);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Is(TEXT("focus steps over a heading to the next group"), [](UDTHubSubsystem* Hub) { return OnNode(Hub, StormNode); });
		Press(EKeys::Down);
		Shot(TEXT("contracts_locked"));
		Press(EKeys::Gamepad_FaceButton_Bottom);
		Is(TEXT("Accept on a node with its prerequisite missing buys nothing"), [](UDTHubSubsystem* Hub) { return OnNode(Hub, StormNext) && Rank(Hub, StormNext) == 0 && Profile(Hub).Currency == DTConst::TREE_COST_BLESSING4; });
		Press(EKeys::Up);
		Press(EKeys::Gamepad_FaceButton_Bottom);
		Is(TEXT("pad A buys at exact money"), [](UDTHubSubsystem* Hub) { return Rank(Hub, StormNode) == 1 && Profile(Hub).Currency == 0; });
		Press(EKeys::Gamepad_FaceButton_Right);
		Is(TEXT("pad B leaves Contracts on its row"), [](UDTHubSubsystem* Hub) { return OnRow(Hub, EDTHubRow::Tree); });
	}

	/** A second launch on the same file: the Patron survived the process, and the run's Restart still does what it did. */
	static void BuildReboot()
	{
		Is(TEXT("a new process opens on the saved Patron"), [](UDTHubSubsystem* Hub) { return Hub->GetState().Selected == EDTPath::Forge && OnRow(Hub, EDTHubRow::Begin); });
		Is(TEXT("a new process holds both purchases and what they opened"), [](UDTHubSubsystem* Hub)
		{
			const TSet<FName>& Unlocked = Profile(Hub).Unlocked;
			return Rank(Hub, ScatterNode) == 1 && Rank(Hub, StormNode) == 1 && Rank(Hub, StormNext) == 0 && Unlocked.Contains(TEXT("Weapon.ScatterCannon"))
				&& Unlocked.Contains(TEXT("Blessing.Storm.Tempest.Amp1")) && !Unlocked.Contains(TEXT("Weapon.RailLance"));
		});
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Is(TEXT("the Ledger-stone shows the run"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Stats) && UDTMetaSubsystem::ReadProfile(Hub)->Runs == 1; });
		Shot(TEXT("ledger_after"));
		Press(EKeys::Gamepad_FaceButton_Right);
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Is(TEXT("the page reads the bought node Complete"), [](UDTHubSubsystem* Hub)
		{
			return IsPage(Hub, EDTHubPage::Tree) && Hub->GetTree()->RowValue(TreeRow(Hub, ScatterNode)).EqualTo(FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.State.Maxed")));
		});
		Shot(TEXT("contracts_after"));
		Press(EKeys::Gamepad_FaceButton_Right);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Until(RunReady);
		Is(TEXT("the second run keeps the Patron"), [](UDTHubSubsystem* Hub) { return RunPatron(Hub) == EDTPath::Forge; });
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Press(EKeys::Up);
		Is(TEXT("Restart asks as it always did"), [](UDTHubSubsystem* Hub) { return Menu(Hub)->GetState().Pending == EDTMenuAction::Restart; });
		Press(EKeys::Enter);
		Until([](UDTHubSubsystem* Hub) { return RunReady(Hub) && !Hub->GetWorld()->URL.HasOption(TEXT("DTPatron")); });
		Is(TEXT("Restart reloads into a run, not the hub"), [](UDTHubSubsystem* Hub) { return RunPatron(Hub) != EDTPath::None; });
	}

	/** The same save, -DTPoolGate: a run begun from the hub carries what was bought, and only that. */
	static void BuildGated()
	{
		Press(EKeys::Enter);
		Until(RunReady);
		Is(TEXT("the run reads the gate"), [](UDTHubSubsystem*) { return UDTMetaSubsystem::GatesPool(); });
		Is(TEXT("the bought weapon is in this run's pool"), [](UDTHubSubsystem* Hub) { return Hub->GetWorld()->GetSubsystem<UDTWeaponSubsystem>()->IsInPool(TEXT("Weapon.ScatterCannon")); });
		Is(TEXT("nodes not bought stay shut"), [](UDTHubSubsystem* Hub)
		{
			const UDTWeaponSubsystem* Weapons = Hub->GetWorld()->GetSubsystem<UDTWeaponSubsystem>();
			return !Weapons->IsInPool(TEXT("Weapon.RailLance")) && !Weapons->IsInPool(TEXT("Mod.Fork")) && !Weapons->IsInPool(TEXT("Mod.ScatterShell"));
		});
		Is(TEXT("the run's grant holds the bought branch and not the next"), [](UDTHubSubsystem* Hub)
		{
			const FDTPoolGrant Grant = UDTMetaSubsystem::GetRunGrant(Hub);
			return Grant.bGated && Grant.Unlocked.Contains(TEXT("Blessing.Storm.Tempest.Amp1")) && !Grant.Unlocked.Contains(TEXT("Blessing.Storm.Downpour.Amp1"));
		});
	}

	/** The same save at C.SETTINGS.TEXT_MAX_PCT: shots of every page, and every row of each measured on the real font. */
	static void BuildLarge()
	{
		Is(TEXT("the launch reads the largest text"), [](UDTHubSubsystem* Hub)
		{
			return Hub->GetState().Window == DTHubLayout::ListWindow(static_cast<float>(DTConst::SETTINGS_TEXT_MAX_PCT) / DTConst::SETTINGS_TEXT_MIN_PCT);
		});
		Walk(TEXT("the root"));
		Shot(TEXT("root_large"));
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Shot(TEXT("shrine_large"));
		Walk(TEXT("the Shrine"));
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Shot(TEXT("contracts_large"));
		Walk(TEXT("Contracts"));
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Shot(TEXT("codex_large"));
		Walk(TEXT("the Codex"));
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Shot(TEXT("ledger_large"));
		Walk(TEXT("the Ledger-stone"));
		// The save above has read little: a Recollection's text, a bestiary line and a sealed Keeper's terms were never drawn, so
		// never measured (review #71, #81). The same pages again as a profile that has earned every row sees them.
		Press(EKeys::Escape);
		Check.Steps.Add([](UDTHubSubsystem* Hub) { Hub->ShowEverythingForCheck(); return true; });
		Is(TEXT("every row of the book is read, with its text, and every Keeper open"), [](UDTHubSubsystem* Hub)
		{
			const TArray<FDTCodexRow>& Book = Hub->GetCodex();
			const int32 Bodies = Book.FilterByPredicate([](const FDTCodexRow& Row) { return Row.bRead && !Row.Body.IsEmpty(); }).Num();
			return Book.Num() > 0 && !Book.ContainsByPredicate([](const FDTCodexRow& Row) { return !Row.bRead; }) && Bodies == Book.Num() - 1
				&& !Hub->GetState().Patrons.ContainsByPredicate([](const FDTHubPatron& Patron) { return !Patron.bOpen; });
		});
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Is(TEXT("the Shrine again"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Patron); });
		Walk(TEXT("the Shrine, every Keeper's terms"));
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Is(TEXT("the Codex again, from its first row"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Codex) && Hub->GetState().Focus == 0; });
		Walk(TEXT("the Codex, every row read"));
		// The root's four lines about the save (R-VIG-21): no launch of this check is stuck, has a bad save or a lost run, so each
		// is put on the page and every root row walked under it. A stuck line names the save itself; a set-aside one the longest
		// name a first bad save gets.
		Press(EKeys::Escape);
		for (const EDTMetaNotice Kind : { EDTMetaNotice::SaveUnreadable, EDTMetaNotice::SaveNotMoved, EDTMetaNotice::KeptAside, EDTMetaNotice::RunLost })
		{
			const bool bStuck = Kind == EDTMetaNotice::SaveUnreadable || Kind == EDTMetaNotice::SaveNotMoved;
			Check.Steps.Add([Kind, bStuck](UDTHubSubsystem* Hub) { Hub->ShowNoticeForCheck(Kind, bStuck ? TEXT("DTMeta.sav") : TEXT("DTMeta.sav.bad.10")); return true; });
			Is(TEXT("the root holds a line about the save"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Root) && !Hub->GetNotice().IsEmpty(); });
			Walk(Kind == EDTMetaNotice::SaveUnreadable ? TEXT("the root, a save that cannot be opened")
				: Kind == EDTMetaNotice::SaveNotMoved ? TEXT("the root, a bad save that could not be set aside")
				: Kind == EDTMetaNotice::KeptAside ? TEXT("the root, a save set aside") : TEXT("the root, a run not saved"));
		}
		Check.Steps.Add([](UDTHubSubsystem* Hub) { Hub->ShowNoticeForCheck(EDTMetaNotice::None, FString()); return true; });
	}

	/** A person's way into a run on an empty profile, for Tools/SeedCheck.ps1: the Shrine, the first Keeper, Begin. */
	static void BuildBegin()
	{
		Press(EKeys::Enter);
		Is(TEXT("Accept opens the Shrine"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Patron); });
		Press(EKeys::Enter);
		Is(TEXT("the pick lands on Begin"), [](UDTHubSubsystem* Hub) { return OnRow(Hub, EDTHubRow::Begin); });
		Press(EKeys::SpaceBar);
		Until(RunReady);
	}

	/** From the root's Begin row, where a saved Patron opens it, onto Contracts' first node. */
	static void OpenContracts()
	{
		for (int32 Row = static_cast<int32>(EDTHubRow::Tree); Row < static_cast<int32>(EDTHubRow::Begin); ++Row)
		{
			Press(EKeys::Up);
		}
		Press(EKeys::Enter);
		Is(TEXT("Contracts opens on its first node"), [](UDTHubSubsystem* Hub) { return OnNode(Hub, FirstNode); });
	}

	/** Down passes over a heading, so a press is one node of the catalog. */
	static void StepToNode(const TCHAR* From, const TCHAR* To)
	{
		for (int32 Left = DTTree::FindNode(FName(To)) - DTTree::FindNode(FName(From)); Left > 0; --Left)
		{
			Press(EKeys::Down);
		}
		Is(FString::Printf(TEXT("focus reaches %s"), To), [To](UDTHubSubsystem* Hub) { return OnNode(Hub, To); });
	}

	/** Tools/EarnedCheck.ps1 (meta.md T-MET-30), on the save a gated run left: no balance is set here, so a real key spends what that run banked. */
	static void BuildEarned()
	{
		static int64 Banked = 0;
		Is(TEXT("the save holds a banked run, its Ichor and a Discovery, and nothing bought"), [](UDTHubSubsystem* Hub)
		{
			const FDTMetaProfile& Read = Profile(Hub);
			Banked = Read.Currency;
			return Read.Runs == 1 && Banked > 0 && Read.Discoveries.Num() > 0 && Read.TreeRanks.IsEmpty();
		});
		Is(TEXT("the bank can afford the node"), [](UDTHubSubsystem* Hub) { return DTTree::GetNode(Profile(Hub), StormNode).State == EDTTreeState::Open; });
		OpenContracts();
		StepToNode(FirstNode, StormNode);
		Shot(TEXT("earned_open"));
		Press(EKeys::Enter);
		Is(TEXT("a real key spends what the run banked: rank, cards and price"), [](UDTHubSubsystem* Hub)
		{
			return Rank(Hub, StormNode) == 1 && Profile(Hub).Unlocked.Contains(StormCard) && Profile(Hub).Currency == Banked - DTConst::TREE_COST_BLESSING4;
		});
		Shot(TEXT("earned_bought"));
		Press(EKeys::Gamepad_FaceButton_Right);
		Is(TEXT("pad B leaves Contracts"), [](UDTHubSubsystem* Hub) { return OnRow(Hub, EDTHubRow::Tree); });
	}

	/** A new process on that save: the purchase outlived the one that made it. */
	static void BuildEarnedRead()
	{
		Is(TEXT("a new process holds the rank and the cards, and only those"), [](UDTHubSubsystem* Hub)
		{
			return Rank(Hub, StormNode) == 1 && Profile(Hub).TreeRanks.Num() == 1 && Profile(Hub).Unlocked.Contains(StormCard);
		});
		OpenContracts();
		Is(TEXT("the page reads the node Complete"), [](UDTHubSubsystem* Hub)
		{
			return Hub->GetTree()->RowValue(TreeRow(Hub, StormNode)).EqualTo(FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.State.Maxed")));
		});
		Shot(TEXT("earned_after"));
	}

	/** On a planted save: a real win's doors and a set balance. Every node is bought by key at exact money; the two Paths are TwoPaths6's door, the rest the new branches. */
	static void BuildDoors()
	{
		Is(TEXT("the three Keepers are met, and the duos are not earned"), [](UDTHubSubsystem* Hub)
		{
			const TSet<FName>& Unlocked = Profile(Hub).Unlocked;
			return Unlocked.Contains(TEXT("Keeper.Shade")) && Unlocked.Contains(TEXT("Keeper.Radiance")) && Unlocked.Contains(TEXT("Keeper.Quicksilver")) && !Unlocked.Contains(TEXT("Reaction.Duos"));
		});
		Fund(UE_ARRAY_COUNT(DoorNodes) * DTConst::TREE_COST_BLESSING4);
		OpenContracts();
		const TCHAR* From = FirstNode;
		for (const FDoorNode& Door : DoorNodes)
		{
			StepToNode(From, Door.Node);
			Press(EKeys::Enter);
			Is(FString::Printf(TEXT("a key buys %s"), Door.Node), [Door](UDTHubSubsystem* Hub) { return Rank(Hub, Door.Node) == 1 && Profile(Hub).Unlocked.Contains(Door.Card); });
			From = Door.Node;
		}
		Is(TEXT("exact money leaves nothing"), [](UDTHubSubsystem* Hub) { return Profile(Hub).Currency == 0; });
		Shot(TEXT("doors_bought"));
		Press(EKeys::Escape);
	}

	/** Each Keeper a door opened, straight after Contracts: chosen at the Shrine by keys, a gated run begun from the hub, abandoned, and back. */
	static void BuildKeepers()
	{
		EDTPath Last = EDTPath::Storm; // the Patron of the run that earned the save
		EDTHubRow FromRow = EDTHubRow::Tree; // where Back from Contracts lands; a run comes back to Begin
		for (const EDTPath Path : { EDTPath::Shade, EDTPath::Radiance, EDTPath::Quicksilver })
		{
			const FString Name = DTOffers::PathName(Path);
			for (int32 Row = static_cast<int32>(FromRow); Row > 0; --Row)
			{
				Press(EKeys::Up);
			}
			FromRow = EDTHubRow::Begin;
			Press(EKeys::Enter);
			Is(FString::Printf(TEXT("the Shrine opens on %s"), DTOffers::PathName(Last)), [Last](UDTHubSubsystem* Hub)
			{
				return IsPage(Hub, EDTHubPage::Patron) && Hub->GetState().Patrons[Hub->GetState().Focus].Path == Last;
			});
			for (int32 Row = static_cast<int32>(Last); Row < static_cast<int32>(Path); ++Row)
			{
				Press(EKeys::Down);
			}
			Press(EKeys::Enter);
			Is(FString::Printf(TEXT("%s is open at the Shrine and the pick lands on Begin"), *Name), [Path](UDTHubSubsystem* Hub) { return Hub->GetState().Selected == Path && OnRow(Hub, EDTHubRow::Begin); });
			Press(EKeys::SpaceBar);
			Until(RunReady);
			Is(FString::Printf(TEXT("a gated run plays on %s with no hand left"), *Name), [Path](UDTHubSubsystem* Hub)
			{
				const UDTOfferSubsystem* Offers = Hub->GetWorld()->GetSubsystem<UDTOfferSubsystem>();
				return UDTMetaSubsystem::GatesPool() && RunPatron(Hub) == Path && Offers && !Offers->IsAnyHandOpen();
			});
			Press(EKeys::Escape);
			Press(EKeys::Down);
			Press(EKeys::Down);
			Press(EKeys::Down);
			Press(EKeys::Enter);
			Press(EKeys::Up);
			Press(EKeys::Enter);
			Until([](UDTHubSubsystem* Hub) { return Menu(Hub)->GetState().Screen == EDTMenuScreen::RunOver; });
			Wait(DTConst::SETTINGS_RUN_OVER_GUARD_S + KeyS);
			Press(EKeys::Enter);
			Until([](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Root); });
			Is(FString::Printf(TEXT("back in the hub with %s remembered"), *Name), [Name](UDTHubSubsystem* Hub) { return Profile(Hub).LastPatron == FName(*Name) && OnRow(Hub, EDTHubRow::Begin); });
			Last = Path;
		}

		// A real Restart names nobody: the hand this save reads there leads with the Keeper last played (keepers.md R-KEE-23), under auto-pick its first card.
		Press(EKeys::Enter);
		Until(RunReady);
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Until([](UDTHubSubsystem* Hub) { return RunReady(Hub) && !Hub->GetWorld()->URL.HasOption(TEXT("DTPatron")); });
		Is(TEXT("a Restart's hand on this save leads with the Keeper last played"), [](UDTHubSubsystem* Hub) { return UDTMetaSubsystem::GatesPool() && RunPatron(Hub) == EDTPath::Quicksilver; });
	}

	static void Finish(const TCHAR* Why)
	{
		UE_LOG(LogDT, Log, TEXT("VIGILCHECK: %s (%d wrong%s%s)"), Check.Fails == 0 && !Why[0] ? TEXT("PASS") : TEXT("FAIL"), Check.Fails, Why[0] ? TEXT(", ") : TEXT(""), Why);
		Check.bDone = true;
		FPlatformMisc::RequestExit(false, TEXT("DT vigil check over"));
	}
}

void DTHubCheck::Tick(UDTHubSubsystem* Hub)
{
	using namespace DTHubCheckLocal;
	UWorld* World = Hub->GetWorld();
	FString Script;
	if (Check.bDone || !World || !World->HasBegunPlay() || !FParse::Value(FCommandLine::Get(), TEXT("DTHubCheck="), Script) || !Controller(Hub))
	{
		return;
	}
	if (!Check.bBuilt)
	{
		Check.bBuilt = true;
		if (Script == TEXT("Reboot"))
		{
			BuildReboot();
		}
		else if (Script == TEXT("Gated"))
		{
			BuildGated();
		}
		else if (Script == TEXT("Large"))
		{
			BuildLarge();
		}
		else if (Script == TEXT("Begin"))
		{
			BuildBegin();
		}
		else if (Script == TEXT("Earned"))
		{
			BuildEarned();
		}
		else if (Script == TEXT("EarnedRead"))
		{
			BuildEarnedRead();
		}
		else if (Script == TEXT("Doors"))
		{
			BuildDoors();
			BuildKeepers();
		}
		else
		{
			BuildFirst();
		}
		Check.StepAt = FPlatformTime::Seconds();
	}
	if (Elapsed() > GiveUpS)
	{
		Finish(*FString::Printf(TEXT("step %d never ended"), Check.Index));
		return;
	}
	// Steps stand in a static array, and a step may be running when the map changes: the next tick comes from the new world's hub.
	if (Check.Steps[Check.Index](Hub))
	{
		++Check.Index;
		Check.StepAt = FPlatformTime::Seconds();
		if (Check.Index == Check.Steps.Num())
		{
			Finish(TEXT(""));
		}
	}
}
#endif
