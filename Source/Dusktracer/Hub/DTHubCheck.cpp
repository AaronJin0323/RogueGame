#include "Hub/DTHubCheck.h"

#if !UE_BUILD_SHIPPING
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "Core/DTConstantsGen.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Hub/DTHubSubsystem.h"
#include "InputKeyEventArgs.h"
#include "Meta/DTMetaSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
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
	// Sim steps a run plays before the proof presses anything in it.
	constexpr int64 ReadyStep = 60;

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
		Is(TEXT("the Tree slot opens empty"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Tree) && Hub->GetState().NumRows() == 0; });
		Shot(TEXT("contracts"));
		Press(EKeys::Escape);
		Press(EKeys::Down);
		Press(EKeys::Gamepad_FaceButton_Bottom);
		Is(TEXT("pad A opens the Codex"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Codex) && Hub->GetCodex().Num() > 0; });
		// The book opens on Layer 0; the six Keeper rows at Layer 1 are readable from the start, so the shot sits on one.
		Until([](UDTHubSubsystem* Hub)
		{
			const FDTHubState& State = Hub->GetState();
			if (State.Page == EDTHubPage::Codex && Hub->GetCodex().IsValidIndex(State.Focus) && Hub->GetCodex()[State.Focus].bRead)
			{
				return true;
			}
			Hub->Command(EDTMenuCommand::Down);
			return false;
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
		Press(EKeys::SpaceBar);
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
	}

	/** A second launch on the same file: the Patron survived the process, and the run's Restart still does what it did. */
	static void BuildReboot()
	{
		Is(TEXT("a new process opens on the saved Patron"), [](UDTHubSubsystem* Hub) { return Hub->GetState().Selected == EDTPath::Forge && OnRow(Hub, EDTHubRow::Begin); });
		Press(EKeys::Up);
		Press(EKeys::Up);
		Press(EKeys::Enter);
		Is(TEXT("the Ledger-stone shows the run"), [](UDTHubSubsystem* Hub) { return IsPage(Hub, EDTHubPage::Stats) && UDTMetaSubsystem::ReadProfile(Hub)->Runs == 1; });
		Shot(TEXT("ledger_after"));
		Press(EKeys::Gamepad_FaceButton_Right);
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
		Script == TEXT("Reboot") ? BuildReboot() : BuildFirst();
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
