#include "Settings/DTMenuSubsystem.h"
#include "Dusktracer.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHero.h"
#include "Settings/DTMenuScreen.h"
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

void UDTMenuSubsystem::RequestPause()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (IsOpen() || Down.Contains(EKeys::Escape) || Down.Contains(EKeys::Gamepad_Special_Right))
	{
		return; // the Back press that just closed the screen is still down
	}
	State.Open();
	OpenedWall = FPlatformTime::Seconds();
	Down.Reset();
	if (PC)
	{
		for (const FKey& Key : DTMenuInput::Keys())
		{
			if (PC->IsInputKeyDown(Key))
			{
				Down.Add(Key);
			}
		}
	}
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

void UDTMenuSubsystem::HandleOutcome(const FDTMenuOutcome& Outcome)
{
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
	TArray<FKey> Stale;
	for (const FKey& Key : Down)
	{
		if (!PC->IsInputKeyDown(Key))
		{
			Stale.Add(Key);
		}
	}
	for (const FKey& Key : Stale)
	{
		Down.Remove(Key);
	}
	if (!IsOpen())
	{
		return;
	}
	for (const FKey& Key : DTMenuInput::Keys())
	{
		if (PC->IsInputKeyDown(Key) && !Down.Contains(Key))
		{
			Down.Add(Key);
			Command(DTMenuInput::ForKey(Key));
			if (!IsOpen())
			{
				return;
			}
		}
	}
}

void UDTMenuSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	RunLanguageSwitch(); // queued by last frame's press (R-SET-06)
	PollKeys();
#if !UE_BUILD_SHIPPING
	TickDevShot();
#endif
}

void UDTMenuSubsystem::Draw(UCanvas* Canvas)
{
	if (IsOpen())
	{
		DTMenuScreen::Draw(Canvas, State, DTSettings::Live());
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
