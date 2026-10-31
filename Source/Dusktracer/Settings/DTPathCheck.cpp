#include "Settings/DTPathCheck.h"

#if !UE_BUILD_SHIPPING
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Hub/DTHubSubsystem.h"
#include "InputKeyEventArgs.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferSubsystem.h"
#include "Settings/DTMenuSubsystem.h"
#include "UnrealClient.h"

namespace DTPathCheckLocal
{
	// Wall seconds: how long a key is held and left, how long a shot's frame settles, when a step gives up, when a run played out does.
	constexpr double KeyS = 0.15;
	constexpr double ShotSettleS = 0.5;
	constexpr double ShotPollS = 0.25;
	constexpr double GiveUpS = 60.0;
	constexpr double PlayGiveUpS = 780.0;
	// How long past its guard a hand must wait for a key on a person's launch before the proof presses one.
	constexpr double HandWaitS = 1.0;
	// Sim steps a run plays before the proof presses anything in it, and the second run before it is abandoned: it earns something, so the bank's sum reads more than a count.
	constexpr int64 ReadyStep = 60;
	constexpr int64 SecondRunStep = 600;

	/** One step of the script: runs each tick until it says it is done. */
	using FStep = TFunction<bool(UDTMenuSubsystem*)>;

	struct FCheck
	{
		TArray<FStep> Steps;
		int32 Index = 0;
		double StepAt = 0.0;
		double PlayAt = 0.0;
		double HandKeyAt = 0.0;
		bool bHandKey = false;
		int32 HandsPicked = 0;
		int32 Fails = 0;
		EDTPath Patron = EDTPath::Forge;
		FString Culture;
		bool bBuilt = false;
		bool bDone = false;
		int64 ShotSize = -1;
		double ShotPollAt = 0.0;
		bool bShotAsked = false;
	};
	static FCheck Check;

	static double Elapsed()
	{
		return FPlatformTime::Seconds() - Check.StepAt;
	}

	static APlayerController* Controller(UDTMenuSubsystem* Menu)
	{
		return Menu->GetWorld() ? Menu->GetWorld()->GetFirstPlayerController() : nullptr;
	}

	static UDTHubSubsystem* Hub(UDTMenuSubsystem* Menu)
	{
		return Menu->GetWorld()->GetSubsystem<UDTHubSubsystem>();
	}

	static UDTOfferSubsystem* Offers(UDTMenuSubsystem* Menu)
	{
		return Menu->GetWorld()->GetSubsystem<UDTOfferSubsystem>();
	}

	static UDTSimSubsystem* Sim(UDTMenuSubsystem* Menu)
	{
		return Menu->GetWorld()->GetSubsystem<UDTSimSubsystem>();
	}

	static void Send(UDTMenuSubsystem* Menu, const FKey& Key, bool bDown)
	{
		if (APlayerController* PC = Controller(Menu))
		{
			PC->InputKey(FInputKeyEventArgs::CreateSimulated(Key, bDown ? IE_Pressed : IE_Released, bDown ? 1.0f : 0.0f));
		}
	}

	static void Expect(const FString& What, bool bGood)
	{
		Check.Fails += bGood ? 0 : 1;
		UE_LOG(LogDT, Log, TEXT("PATHCHECK %s: %s"), *What, bGood ? TEXT("ok") : TEXT("WRONG"));
	}

	static void Press(const FKey& Key)
	{
		Check.Steps.Add([Key](UDTMenuSubsystem* Menu) { Send(Menu, Key, true); return true; });
		Check.Steps.Add([Key](UDTMenuSubsystem* Menu)
		{
			if (Elapsed() < KeyS)
			{
				return false;
			}
			Send(Menu, Key, false);
			return true;
		});
		Check.Steps.Add([](UDTMenuSubsystem*) { return Elapsed() >= KeyS; });
	}

	static void Wait(double Seconds)
	{
		Check.Steps.Add([Seconds](UDTMenuSubsystem*) { return Elapsed() >= Seconds; });
	}

	static void Until(FStep Condition)
	{
		Check.Steps.Add(MoveTemp(Condition));
	}

	static void Is(const FString& What, FStep Condition)
	{
		Check.Steps.Add([What, Condition = MoveTemp(Condition)](UDTMenuSubsystem* Menu)
		{
			Expect(What, Condition(Menu));
			return true;
		});
	}

	/** Writes <-DTPathShots>/<Name>.png; passes with no directory given. */
	static void Shot(const FString& Name)
	{
		Check.Steps.Add([Name](UDTMenuSubsystem*)
		{
			FString Dir;
			if (!FParse::Value(FCommandLine::Get(), TEXT("DTPathShots="), Dir) || Dir.IsEmpty())
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

	/** What each table shows in this culture: the shown string against its source, the keys that show nothing. */
	static void ReadText()
	{
		Check.Steps.Add([](UDTMenuSubsystem*)
		{
			FStringTableRegistry::Get().EnumerateStringTables([](const FName& Id, const FStringTableConstRef& Table)
			{
				int32 Keys = 0;
				int32 Localized = 0;
				int32 Missing = 0;
				Table->EnumerateKeysAndSourceStrings([&Keys, &Localized, &Missing, &Id](const FTextKey& Key, const FString& Source)
				{
					const FString Shown = FText::FromStringTable(Id, Key.ToString()).ToString();
					++Keys;
					Localized += Shown.Equals(Source, ESearchCase::CaseSensitive) ? 0 : 1;
					Missing += Shown.IsEmpty() || Shown.Contains(TEXT("MISSING STRING TABLE")) ? 1 : 0;
					return true;
				});
				UE_LOG(LogDT, Log, TEXT("PATHCHECK table %s keys=%d localized=%d missing=%d"), *Id.ToString(), Keys, Localized, Missing);
				return true;
			});
			return true;
		});
	}

	static bool IsPage(UDTMenuSubsystem* Menu, EDTHubPage Page)
	{
		const UDTHubSubsystem* Found = Hub(Menu);
		return Found && Found->IsActive() && Found->GetState().Page == Page;
	}

	static bool OnRow(UDTMenuSubsystem* Menu, EDTHubRow Row)
	{
		return IsPage(Menu, EDTHubPage::Root) && Hub(Menu)->GetState().RootRow() == Row;
	}

	static bool RunReady(UDTMenuSubsystem* Menu)
	{
		const UWorld* World = Menu->GetWorld();
		const UDTHubSubsystem* Found = Hub(Menu);
		const UDTSimSubsystem* Steps = Sim(Menu);
		return Found && !Found->IsActive() && World->URL.HasOption(TEXT("DTSeed")) && Steps && Steps->GetStepIndex() >= ReadyStep;
	}

	static EDTPath RunPatron(UDTMenuSubsystem* Menu)
	{
		UDTSimSubsystem* Steps = Sim(Menu);
		return Steps && Steps->GetRun().GetNumPlayers() > 0 ? Steps->GetRun().GetPlayer(0).Offer.PatronPath : EDTPath::None;
	}

	static bool OnRunOver(UDTMenuSubsystem* Menu)
	{
		return Menu->GetState().Screen == EDTMenuScreen::RunOver;
	}

	/** The way back from a closed run: the row leads, the guard passes, Accept lands in the hub. */
	static void ReturnToHub(const FString& What)
	{
		Is(What, [](UDTMenuSubsystem* Menu)
		{
			const FDTMenuState& State = Menu->GetState();
			return State.bHubRow && State.Focus == 0 && State.RunOverRowAt(0) == EDTRunOverRow::ReturnToHub;
		});
		Wait(DTConst::SETTINGS_RUN_OVER_GUARD_S + KeyS);
		Press(EKeys::Enter);
		Until([](UDTMenuSubsystem* Menu) { return IsPage(Menu, EDTHubPage::Root); });
	}

	/** A person who never moves: every hand takes the focused card by a real key until the run ends. */
	static void PlayOut()
	{
		Check.Steps.Add([](UDTMenuSubsystem* Menu)
		{
			const double Now = FPlatformTime::Seconds();
			Check.PlayAt = Check.PlayAt > 0.0 ? Check.PlayAt : Now;
			if (Now - Check.PlayAt > PlayGiveUpS)
			{
				return true; // the step after this one reads a run that never ended
			}
			Check.StepAt = Now; // this one step may last the whole run
			if (Check.bHandKey)
			{
				if (Now - Check.HandKeyAt >= KeyS)
				{
					Send(Menu, EKeys::Enter, false);
					Check.bHandKey = false;
					Check.HandKeyAt = Now;
				}
				return false;
			}
			if (OnRunOver(Menu))
			{
				return true;
			}
			const UDTOfferSubsystem* Hands = Offers(Menu);
			if (Hands && Hands->IsAnyHandOpen() && !Hands->IsGuarded(0) && Now - Check.HandKeyAt >= KeyS)
			{
				Send(Menu, EKeys::Enter, true);
				Check.bHandKey = true;
				Check.HandKeyAt = Now;
				++Check.HandsPicked;
			}
			return false;
		});
	}

	static void Verdict(const TCHAR* Why)
	{
		UE_LOG(LogDT, Log, TEXT("PATHCHECK: %s (%d wrong, %d hand(s) picked by key%s%s)"), Check.Fails == 0 && !Why[0] ? TEXT("PASS") : TEXT("FAIL"), Check.Fails, Check.HandsPicked, Why[0] ? TEXT(", ") : TEXT(""), Why);
	}

	/** From a root row: Quit to desktop, the way a person leaves. The verdict comes first, because the key ends the process. */
	static void QuitFrom(int32 RowsDown)
	{
		for (int32 Row = 0; Row < RowsDown; ++Row)
		{
			Press(EKeys::Down);
		}
		Is(TEXT("focus reaches Quit to desktop"), [](UDTMenuSubsystem* Menu) { return OnRow(Menu, EDTHubRow::Quit); });
		Press(EKeys::Enter);
		Is(TEXT("Quit to desktop asks"), [](UDTMenuSubsystem* Menu) { return Menu->GetState().Screen == EDTMenuScreen::Confirm; });
		Press(EKeys::Up);
		Check.Steps.Add([](UDTMenuSubsystem*) { Verdict(TEXT("")); return true; });
		Press(EKeys::Enter);
	}

	/** From the root, a page by real keys and back onto the next row: the text and the shot of each page, in this package. */
	static void VisitPage(EDTHubPage Page, const FString& Name)
	{
		Press(EKeys::Enter);
		Is(FString::Printf(TEXT("the %s page opens"), *Name), [Page](UDTMenuSubsystem* Menu) { return IsPage(Menu, Page); });
		if (Page == EDTHubPage::Codex)
		{
			// The book opens on Layer 0; the Keeper rows at Layer 1 are readable from the start, so the shot sits on one.
			Until([](UDTMenuSubsystem* Menu)
			{
				UDTHubSubsystem* Found = Hub(Menu);
				const FDTHubState& State = Found->GetState();
				if (Found->GetCodex().IsValidIndex(State.Focus) && Found->GetCodex()[State.Focus].bRead)
				{
					return true;
				}
				Found->Command(EDTMenuCommand::Down);
				return false;
			});
		}
		Shot(Name);
		Press(EKeys::Escape);
		Press(EKeys::Down);
	}

	static void BuildWalk()
	{
		ReadText();
		Is(TEXT("boot lands in the hub with no run and no pawn"), [](UDTMenuSubsystem* Menu)
		{
			return IsPage(Menu, EDTHubPage::Root) && Sim(Menu)->GetRun().GetNumPlayers() == 0 && Sim(Menu)->GetStepIndex() == 0 && !Controller(Menu)->GetPawn();
		});
		Is(TEXT("a fresh save chooses nobody"), [](UDTMenuSubsystem* Menu) { return Hub(Menu)->GetState().Selected == EDTPath::None && OnRow(Menu, EDTHubRow::Patron); });
		Shot(TEXT("root_fresh"));
		Press(EKeys::Down);
		VisitPage(EDTHubPage::Tree, TEXT("contracts"));
		VisitPage(EDTHubPage::Codex, TEXT("codex"));
		VisitPage(EDTHubPage::Stats, TEXT("ledger_fresh"));
		Press(EKeys::Enter);
		Is(TEXT("Settings opens"), [](UDTMenuSubsystem* Menu) { return Menu->GetState().Screen == EDTMenuScreen::Settings; });
		Shot(TEXT("settings"));
		Press(EKeys::Escape);
		Is(TEXT("Back from Settings lands on its row"), [](UDTMenuSubsystem* Menu) { return !Menu->IsOpen() && OnRow(Menu, EDTHubRow::Settings); });
		for (int32 Row = static_cast<int32>(EDTHubRow::Settings); Row > 0; --Row)
		{
			Press(EKeys::Up);
		}
		Press(EKeys::Enter);
		Is(TEXT("Accept opens the Shrine"), [](UDTMenuSubsystem* Menu) { return IsPage(Menu, EDTHubPage::Patron); });
		Shot(TEXT("shrine"));
		for (int32 Row = DTOffers::AllKeepers().IndexOfByKey(Check.Patron); Row > 0; --Row)
		{
			Press(EKeys::Down);
		}
		Press(EKeys::Enter);
		Is(TEXT("the pick lands on Begin"), [](UDTMenuSubsystem* Menu) { return Hub(Menu)->GetState().Selected == Check.Patron && OnRow(Menu, EDTHubRow::Begin); });
		Shot(TEXT("root_chosen"));
		Press(EKeys::Enter);
		Until(RunReady);
		Is(TEXT("the run plays on the chosen Patron with a pawn and no hand"), [](UDTMenuSubsystem* Menu)
		{
			return RunPatron(Menu) == Check.Patron && Controller(Menu)->GetPawn() && !Offers(Menu)->IsAnyHandOpen();
		});
		Shot(TEXT("run"));
		Until([](UDTMenuSubsystem* Menu)
		{
			Check.StepAt = FPlatformTime::Seconds() - KeyS; // waits on the sim, not on a page
			return OnRunOver(Menu) || (Offers(Menu)->IsAnyHandOpen() && !Offers(Menu)->IsGuarded(0));
		});
		Wait(HandWaitS);
		Is(TEXT("a hand waits for a person's key"), [](UDTMenuSubsystem* Menu) { return Offers(Menu)->IsAnyHandOpen() && !UDTOfferSubsystem::IsAutoPick(); });
		Shot(TEXT("hand"));
		PlayOut();
		Is(TEXT("the first run ends on a death"), [](UDTMenuSubsystem* Menu) { return OnRunOver(Menu) && Menu->GetState().RunOver == EDTRunOverKind::Death; });
		Shot(TEXT("runover_death"));
		ReturnToHub(TEXT("the run-over screen offers the way back first"));
		Is(TEXT("back in the hub on the Patron, on Begin, no seed on the map"), [](UDTMenuSubsystem* Menu)
		{
			return Hub(Menu)->GetState().Selected == Check.Patron && OnRow(Menu, EDTHubRow::Begin) && !Menu->GetWorld()->URL.HasOption(TEXT("DTSeed"));
		});
		Shot(TEXT("root_banked"));
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Is(TEXT("the Ledger-stone opens"), [](UDTMenuSubsystem* Menu) { return IsPage(Menu, EDTHubPage::Stats); });
		Shot(TEXT("ledger_banked"));
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Until(RunReady);
		Is(TEXT("the second run keeps the Patron"), [](UDTMenuSubsystem* Menu) { return RunPatron(Menu) == Check.Patron; });
		Until([](UDTMenuSubsystem* Menu) { return Sim(Menu)->GetStepIndex() >= SecondRunStep || Offers(Menu)->IsAnyHandOpen(); });
		Press(EKeys::Escape);
		Is(TEXT("Esc pauses the second run"), [](UDTMenuSubsystem* Menu) { return Menu->GetState().Screen == EDTMenuScreen::Pause; });
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Until(OnRunOver);
		Is(TEXT("the second run ends abandoned"), [](UDTMenuSubsystem* Menu) { return Menu->GetState().RunOver == EDTRunOverKind::Abandoned; });
		Shot(TEXT("runover_abandoned"));
		ReturnToHub(TEXT("the way back leads again"));
		Is(TEXT("back in the hub a second time"), [](UDTMenuSubsystem* Menu) { return OnRow(Menu, EDTHubRow::Begin) && !Menu->GetWorld()->URL.HasOption(TEXT("DTSeed")); });
		Shot(TEXT("root_end"));
		QuitFrom(1);
	}

	static void BuildReboot()
	{
		ReadText();
		Is(TEXT("a new process opens on the saved Patron, on Begin, with no run"), [](UDTMenuSubsystem* Menu)
		{
			return Hub(Menu)->GetState().Selected != EDTPath::None && OnRow(Menu, EDTHubRow::Begin) && Sim(Menu)->GetRun().GetNumPlayers() == 0;
		});
		Shot(TEXT("root_reboot"));
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Is(TEXT("the Ledger-stone opens"), [](UDTMenuSubsystem* Menu) { return IsPage(Menu, EDTHubPage::Stats); });
		Shot(TEXT("ledger_reboot"));
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Enter);
		Is(TEXT("Settings opens"), [](UDTMenuSubsystem* Menu)
		{
			Check.Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
			return Menu->GetState().Screen == EDTMenuScreen::Settings;
		});
		Press(EKeys::Right);
		Is(TEXT("Right switches the language live"), [](UDTMenuSubsystem*) { return FInternationalization::Get().GetCurrentCulture()->GetName() != Check.Culture; });
		Press(EKeys::Escape);
		Shot(TEXT("root_switched"));
		QuitFrom(2);
	}
}

void DTPathCheck::Tick(UDTMenuSubsystem* Menu)
{
	using namespace DTPathCheckLocal;
	UWorld* World = Menu->GetWorld();
	FString Script;
	if (Check.bDone || !World || !World->HasBegunPlay() || !FParse::Value(FCommandLine::Get(), TEXT("DTPathCheck="), Script) || !Controller(Menu))
	{
		return;
	}
	if (!Check.bBuilt)
	{
		Check.bBuilt = true;
		FString Patron;
		FParse::Value(FCommandLine::Get(), TEXT("DTPathPatron="), Patron);
		Check.Patron = Patron.IsEmpty() ? EDTPath::Forge : DTHubData::ParsePath(FName(*Patron));
		Script == TEXT("Reboot") ? BuildReboot() : BuildWalk();
		Check.StepAt = FPlatformTime::Seconds();
	}
	if (Elapsed() > GiveUpS)
	{
		Verdict(*FString::Printf(TEXT("step %d never ended"), Check.Index));
		Check.bDone = true;
		FPlatformMisc::RequestExit(false, TEXT("DT path check gave up"));
		return;
	}
	// Steps stand in a static array, and a step may be running when the map changes: the next tick comes from the new world's menu.
	if (Check.Steps[Check.Index](Menu))
	{
		++Check.Index;
		Check.StepAt = FPlatformTime::Seconds();
		Check.bDone = Check.Index == Check.Steps.Num();
	}
}
#endif
