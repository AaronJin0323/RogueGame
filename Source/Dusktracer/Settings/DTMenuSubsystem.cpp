#include "Settings/DTMenuSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Dusktracer.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Game/DTGameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Hub/DTHubFlow.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Meta/DTRunRecorder.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHero.h"
#include "Telemetry/DTTelemetrySubsystem.h"
#include "UObject/Package.h"
#include "Settings/DTMenuScreen.h"
#include "Settings/DTPathCheck.h"
#include "Settings/DTRestartCheck.h"
#include "Settings/DTSettingsSubsystem.h"

#if !UE_BUILD_SHIPPING
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace DTMenuSubsystemDev
{
	// World timers stop while the menu holds the pause, so -DTShot is finished from here (Tools/Smoke.ps1).
	constexpr double ShotHoldSeconds = 3.0;

	static UDTMenuSubsystem* FindMenu(UWorld* World)
	{
		return World ? World->GetSubsystem<UDTMenuSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorldAndArgs CmdSettings(TEXT("DT.UI.Settings"), TEXT("Open the settings screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UDTMenuSubsystem* Menu = FindMenu(World))
			{
				Menu->OpenAt(EDTMenuScreen::Settings);
			}
		}));
	static FAutoConsoleCommandWithWorldAndArgs CmdControls(TEXT("DT.UI.Controls"), TEXT("Open the controls screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UDTMenuSubsystem* Menu = FindMenu(World))
			{
				Menu->OpenAt(EDTMenuScreen::Controls);
			}
		}));
	static FAutoConsoleCommandWithWorldAndArgs CmdShot(TEXT("DT.UI.Shot"), TEXT("Finish -DTShot from the menu tick with no menu open, for a shot of an open hand."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UDTMenuSubsystem* Menu = FindMenu(World))
			{
				Menu->ForceDevShot();
			}
		}));
	static FAutoConsoleCommandWithWorldAndArgs CmdPause(TEXT("DT.UI.Pause"), TEXT("Open the pause screen."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UDTMenuSubsystem* Menu = FindMenu(World))
			{
				Menu->RequestPause();
			}
		}));
	static FAutoConsoleCommandWithWorldAndArgs CmdRunOver(TEXT("DT.UI.RunOver"), TEXT("Open the run-over screen: 0 death, 1 victory, 2 abandoned."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UDTMenuSubsystem* Menu = FindMenu(World))
			{
				Menu->OpenRunOver(static_cast<EDTRunOverKind>(Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0));
			}
		}));
	static FAutoConsoleCommandWithWorldAndArgs CmdFocus(TEXT("DT.UI.Key"), TEXT("Send one menu command: Up Down Left Right Accept Back."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDTMenuSubsystem* Menu = FindMenu(World);
			if (!Menu || Args.Num() == 0)
			{
				return;
			}
			const TCHAR* const Names[] = { TEXT("None"), TEXT("Up"), TEXT("Down"), TEXT("Left"), TEXT("Right"), TEXT("Accept"), TEXT("Back") };
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
			{
				if (Args[0] == Names[Index])
				{
					Menu->Command(static_cast<EDTMenuCommand>(Index));
				}
			}
		}));
	static FAutoConsoleCommandWithWorldAndArgs CmdSet(TEXT("DT.UI.Set"), TEXT("Set values for a shot, unsaved: text <pct> | outline <0|1>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld*)
		{
			if (Args.Num() == 2 && Args[0] == TEXT("text"))
			{
				DTSettings::Live().TextPct = FCString::Atoi(*Args[1]);
			}
			else if (Args.Num() == 2 && Args[0] == TEXT("outline"))
			{
				DTSettings::Live().bDangerOutline = Args[1] == TEXT("1");
			}
		}));
}
#endif

bool UDTMenuSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UDTMenuSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDTMenuSubsystem, STATGROUP_Tickables);
}

void UDTMenuSubsystem::SetPaused(bool bWanted)
{
	if (bWanted == bPausedByMenu)
	{
		return;
	}
	bPausedByMenu = bWanted;
	if (PauseOverride)
	{
		PauseOverride(bWanted);
		return;
	}
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!GameMode || !PC)
	{
		return;
	}
	if (bWanted)
	{
		// Each pauser holds its own lock: an open hand's stays when this one goes (R-SET-16).
		GameMode->SetPause(PC, FCanUnpause::CreateUObject(this, &UDTMenuSubsystem::CanUnpause));
	}
	else
	{
		GameMode->ClearPause();
	}
	UE_LOG(LogDT, Log, TEXT("Menu: %s, world paused=%d"), bWanted ? TEXT("open") : TEXT("closed"), World->IsPaused() ? 1 : 0);
}

void UDTMenuSubsystem::OpenAt(EDTMenuScreen Screen)
{
	if (!IsOpen())
	{
		RequestPause();
	}
	State.Screen = IsOpen() ? Screen : EDTMenuScreen::None;
	State.Focus = 0;
}

void UDTMenuSubsystem::SnapshotKeys()
{
	Down.Reset();
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC)
	{
		const float StickX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
		const float StickY = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
		for (const FKey& Key : DTMenuInput::Keys())
		{
			if (PC->IsInputKeyDown(Key) && DTMenuInput::StickKeyCounts(Key, StickX, StickY))
			{
				Down.Add(Key);
			}
		}
	}
}

bool UDTMenuSubsystem::IsAcceptHeld() const
{
	if (HeldOverride)
	{
		return HeldOverride();
	}
	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PC)
	{
		for (const FKey& Key : DTMenuInput::Keys())
		{
			if (DTMenuInput::ForKey(Key) == EDTMenuCommand::Accept && PC->IsInputKeyDown(Key))
			{
				return true;
			}
		}
	}
	return false;
}

void UDTMenuSubsystem::RequestPause()
{
	if (IsOpen() || Down.Contains(EKeys::Escape) || Down.Contains(EKeys::Gamepad_Special_Right))
	{
		return; // the Back press that just closed the screen is still down
	}
	State.Open();
	OpenedWall = FPlatformTime::Seconds();
	SnapshotKeys();
	if (UDTOfferSubsystem* Offers = GetWorld() ? GetWorld()->GetSubsystem<UDTOfferSubsystem>() : nullptr)
	{
		Offers->HoldInput();
	}
	SetPaused(true);
}

void UDTMenuSubsystem::Close()
{
	if (IsOpen())
	{
		State.Close();
	}
	if (UDTOfferSubsystem* Offers = GetWorld() ? GetWorld()->GetSubsystem<UDTOfferSubsystem>() : nullptr)
	{
		Offers->ResumeAfterMenu(FPlatformTime::Seconds() - OpenedWall);
	}
	SetPaused(false);
}

void UDTMenuSubsystem::OpenQuitConfirm()
{
	if (!IsOpen())
	{
		RequestPause();
	}
	if (IsOpen())
	{
		State.OpenQuitConfirm();
	}
}

void UDTMenuSubsystem::OpenRunOver(EDTRunOverKind Kind)
{
	State.OpenRunOver(Kind);
	State.bHubRow = DTHubFlow::Enabled();
	RunOverGuardLeft = DTConst::SETTINGS_RUN_OVER_GUARD_S;
	SnapshotKeys();
	if (UDTOfferSubsystem* Offers = GetWorld() ? GetWorld()->GetSubsystem<UDTOfferSubsystem>() : nullptr)
	{
		Offers->HoldInput(); // for good: a hand under this screen has no run left to feed
	}
	UE_LOG(LogDT, Log, TEXT("Menu: run over (%d)"), static_cast<int32>(Kind));
}

void UDTMenuSubsystem::WatchRunOver()
{
	const ADTGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADTGameMode>() : nullptr;
	if (Mode && Mode->IsRunOver() && State.Screen != EDTMenuScreen::RunOver)
	{
		// Death and victory close the session on a next-tick world timer, and a paused world never fires one: a pause or a hand
		// that lands in the run's last frame would hold the run open until the player leaves, which banks it as Left (meta.md R-MET-04).
		// The step is over by now, so its records are in; a session the timer already closed ignores this.
		if (GetWorld()->IsPaused())
		{
			CloseRun(Mode->IsVictory() ? TEXT("victory") : TEXT("death"));
		}
		OpenRunOver(Mode->IsVictory() ? EDTRunOverKind::Victory : EDTRunOverKind::Death);
	}
}

void UDTMenuSubsystem::CloseRun(const TCHAR* Reason)
{
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		Telemetry->EndSession(Reason); // a no-op on a run that already closed with its own reason
	}
}

void UDTMenuSubsystem::AbandonRun()
{
	CloseRun(TEXT("abandoned"));
	if (ADTGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADTGameMode>() : nullptr)
	{
		Mode->AbandonRun();
	}
	OpenRunOver(EDTRunOverKind::Abandoned); // the world stays paused: this menu still holds the pause
}

void UDTMenuSubsystem::RestartRun(TOptional<int32> ForcedSeed)
{
	if (bLeaving)
	{
		return;
	}
	CloseRun(TEXT("restart"));
	PendingSeed = ForcedSeed.IsSet() ? ForcedSeed.GetValue() : ADTGameMode::NextSeed(ADTGameMode::GetRunSeed(GetWorld()).Seed, FPlatformTime::Cycles());
	bRestartPending = true;
	bLeaving = true;
}

void UDTMenuSubsystem::ReturnToHub()
{
	if (bLeaving)
	{
		return;
	}
	CloseRun(TEXT("restart")); // a run already closed keeps its own reason
	bReturnPending = true;
	bRestartPending = true;
	bLeaving = true;
}

void UDTMenuSubsystem::FinishRestart()
{
	bRestartPending = false;
	if (bReturnPending)
	{
		UE_LOG(LogDT, Log, TEXT("Menu: return to the hub"));
		if (ReturnOverride)
		{
			ReturnOverride();
		}
		else if (UWorld* World = GetWorld())
		{
			UGameplayStatics::OpenLevel(World, FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())));
		}
		return;
	}
	UE_LOG(LogDT, Log, TEXT("Menu: restart on seed %d"), PendingSeed);
	if (RestartOverride)
	{
		RestartOverride(PendingSeed);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		// A whole map reload is the reset: every subsystem and actor of the run goes with it (R-SET-24).
		const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
		UGameplayStatics::OpenLevel(World, FName(*Map), true, FString::Printf(TEXT("DTSeed=%d"), PendingSeed));
	}
}

void UDTMenuSubsystem::QuitGame()
{
	CloseRun(TEXT("quit"));
	bLeaving = true;
	if (QuitOverride)
	{
		QuitOverride();
		return;
	}
	FPlatformMisc::RequestExit(false, TEXT("DT quit to desktop"));
}

void UDTMenuSubsystem::HandleOutcome(const FDTMenuOutcome& Outcome)
{
	switch (Outcome.Action)
	{
	case EDTMenuAction::Restart: RestartRun(); break;
	case EDTMenuAction::Abandon: AbandonRun(); break;
	case EDTMenuAction::Quit: QuitGame(); break;
	case EDTMenuAction::ReturnToHub: ReturnToHub(); break;
	default: break;
	}
	if (Outcome.bClosed)
	{
		Close();
	}
	if (Outcome.bBindingsChanged)
	{
		RefreshHeroBindings();
	}
	if (Outcome.bSettingsChanged)
	{
		const FDTUserSettings& Settings = DTSettings::Live();
		UDTSettingsSubsystem::ApplyVolume(Settings);
		if (static_cast<EDTSettingRow>(State.Focus) == EDTSettingRow::Graphics || static_cast<EDTSettingRow>(State.Focus) == EDTSettingRow::Window)
		{
			UDTSettingsSubsystem::ApplyDisplay(Settings);
		}
	}
	if (Outcome.bSettingsChanged || Outcome.bBindingsChanged)
	{
		UDTSettingsSubsystem::Persist();
	}
}

void UDTMenuSubsystem::Command(EDTMenuCommand Command)
{
	if (bLeaving || (State.Screen == EDTMenuScreen::RunOver && RunOverGuardLeft > 0.0f))
	{
		return;
	}
	HandleOutcome(State.Apply(Command, DTSettings::Live()));
}

void UDTMenuSubsystem::CaptureKey(const FKey& Key)
{
	HandleOutcome(State.Capture(Key, DTSettings::Live()));
}

void UDTMenuSubsystem::RunLanguageSwitch()
{
	if (State.bLanguagePending)
	{
		UDTSettingsSubsystem::ApplyCulture(DTSettings::Live());
		State.bLanguagePending = false;
	}
}

void UDTMenuSubsystem::RefreshHeroBindings()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (ADTHero* Hero = PC ? Cast<ADTHero>(PC->GetPawn()) : nullptr)
	{
		Hero->RefreshBindings();
	}
}

void UDTMenuSubsystem::PollKeys()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}
	if (State.bCapturing)
	{
		// Every button of both devices, so a wrong device gets its own message. Keys down when capture began wait for release.
		TArray<FKey> All;
		EKeys::GetAllKeys(All);
		for (const FKey& Key : All)
		{
			if (!Key.IsDigital() || !Key.IsBindableToActions() || Key.IsTouch() || Key.IsGesture())
			{
				continue;
			}
			const bool bDown = PC->IsInputKeyDown(Key);
			if (bDown && !Down.Contains(Key))
			{
				Down.Add(Key);
				CaptureKey(Key);
				return;
			}
			if (!bDown)
			{
				Down.Remove(Key);
			}
		}
		return;
	}
	// A stick a little off its axis holds two direction keys down: only the larger axis is a press, as in the hub (review #114),
	// so a push down a list with a sideways slip does not change the value of the row it lands on.
	const float StickX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const float StickY = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	const auto IsDown = [PC, StickX, StickY](const FKey& Key) { return PC->IsInputKeyDown(Key) && DTMenuInput::StickKeyCounts(Key, StickX, StickY); };
	if (!IsOpen())
	{
		const TSet<FKey> Held = Down;
		for (const FKey& Key : Held)
		{
			if (!IsDown(Key))
			{
				Down.Remove(Key);
			}
		}
		return;
	}
	for (const EDTMenuCommand Pressed : DTMenuInput::Presses(Down, IsDown))
	{
		Command(Pressed);
		if (!IsOpen())
		{
			return;
		}
	}
}

void UDTMenuSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RunLanguageSwitch(); // queued by last frame's press (R-SET-06)
	RunOverGuardLeft = FMath::Max(0.0f, RunOverGuardLeft - DeltaTime);
	WatchRunOver();
	if (bRestartPending && !IsAcceptHeld())
	{
		FinishRestart();
	}
	PollKeys();
#if !UE_BUILD_SHIPPING
	TickDevShot();
	DTRestartCheck::Tick(this);
	DTPathCheck::Tick(this);
#endif
}

void UDTMenuSubsystem::Draw(UCanvas* Canvas)
{
	if (IsOpen())
	{
		// The bank runs when the run's session closes, a tick after the screen opens for a death or a win (meta.md R-MET-06).
		const UDTRunRecorder* Recorder = GetWorld() ? GetWorld()->GetSubsystem<UDTRunRecorder>() : nullptr;
		const bool bSummary = State.Screen == EDTMenuScreen::RunOver && Recorder && Recorder->GetAward().bBanked;
		FText Line;
		FText Toast;
		if (bSummary)
		{
			// A stuck save says nothing was kept; a busy one says the run waits; else the award and what the run found (meta.md R-MET-06).
			const FDTRunAward& Award = Recorder->GetAward();
			DTMenuText::RunOverLines(Award.bNotSaved, Award.bDeferred, Award.Retained, Award.Balance, Award.Discovered, Line, Toast);
		}
		DTMenuScreen::Draw(Canvas, State, DTSettings::Live(), Line, Toast);
	}
}

#if !UE_BUILD_SHIPPING
void UDTMenuSubsystem::TickDevShot()
{
	if (!IsOpen() && !bDevShotAsked && !bDevShotForced)
	{
		DevShotAt = -1.0;
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (DevShotAt < 0.0)
	{
		FString Path;
		if (!FParse::Value(FCommandLine::Get(), TEXT("DTShot="), Path) || Path.IsEmpty())
		{
			return;
		}
		DevShotPath = FPaths::ConvertRelativePathToFull(Path);
		DevShotAt = Now + DTMenuSubsystemDev::ShotHoldSeconds;
	}
	if (Now < DevShotAt)
	{
		return;
	}
	if (!bDevShotAsked)
	{
		bDevShotAsked = true;
		FScreenshotRequest::RequestScreenshot(DevShotPath, /*bInShowUI*/ true, /*bAddFilenameSuffix*/ false);
		DevShotLastSize = -1;
		DevShotAt = Now + 0.5;
		return;
	}
	const int64 Size = IFileManager::Get().FileSize(*DevShotPath);
	if (Size > 0 && Size == DevShotLastSize)
	{
		FPlatformMisc::RequestExit(false, TEXT("DT menu screenshot written"));
		return;
	}
	DevShotLastSize = Size;
	DevShotAt = Now + 0.5;
}
#endif
