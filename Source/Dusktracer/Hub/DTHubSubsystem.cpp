#include "Hub/DTHubSubsystem.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Dusktracer.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Game/DTGameMode.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Hub/DTHubCheck.h"
#include "Hub/DTHubScreen.h"
#include "Hub/DTHubTab.h"
#include "Kismet/GameplayStatics.h"
#include "Meta/DTMetaSubsystem.h"
#include "Settings/DTMenuSubsystem.h"
#include "Settings/DTUserSettings.h"
#include "UObject/Package.h"

bool UDTHubSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

TStatId UDTHubSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDTHubSubsystem, STATGROUP_Tickables);
}

const FDTMetaProfile* UDTHubSubsystem::Profile() const
{
	const FDTMetaProfile* Found = UDTMetaSubsystem::ReadProfile(this);
	return Found ? Found : &Empty;
}

void UDTHubSubsystem::Enter()
{
	bActive = true;
	const FDTMetaProfile& Read = *Profile();
	const UDTWeaponSubsystem* Weapons = GetWorld() ? GetWorld()->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	State.Open(DTHubData::Patrons(Read, [Weapons](FName Id) { return Weapons && Weapons->IsInPool(Id); }), DTHubData::ParsePath(Read.LastPatron));
	Codex = DTHubData::BuildCodex(DTHubData::ReadCodexTable(), Read);
	State.CodexRows = Codex.Num();
	Tree = DTHubTabs::MakeTree(GetWorld());
	if (UDTMenuSubsystem* Menu = GetWorld() ? GetWorld()->GetSubsystem<UDTMenuSubsystem>() : nullptr)
	{
		Menu->SetHubHost();
	}
	UE_LOG(LogDT, Log, TEXT("Hub: open, balance %lld, runs %d, last patron '%s'"), Read.Currency, Read.Runs, *Read.LastPatron.ToString());
}

bool UDTHubSubsystem::IsAcceptHeld() const
{
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

void UDTHubSubsystem::BeginRun()
{
	const int32 CommandSeed = ADTGameMode::GetCommandLineSeed();
	const int32 Seed = CommandSeed != 0 ? CommandSeed : ADTGameMode::NextSeed(0, FPlatformTime::Cycles());
	PendingOptions = DTHubData::RunOptions(Seed, State.Selected, *Profile());
	bBeginPending = true; // as a restart: the key that chose it comes up first, so it never reaches the run (settings.md R-SET-23)
}

void UDTHubSubsystem::FinishBegin()
{
	bBeginPending = false;
	UE_LOG(LogDT, Log, TEXT("Hub: begin %s"), *PendingOptions);
	if (BeginOverride)
	{
		BeginOverride(PendingOptions);
		return;
	}
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())), true, PendingOptions);
	}
}

void UDTHubSubsystem::HandleOutcome(const FDTHubOutcome& Outcome)
{
	UDTMenuSubsystem* Menu = GetWorld() ? GetWorld()->GetSubsystem<UDTMenuSubsystem>() : nullptr;
	switch (Outcome.Action)
	{
	case EDTHubAction::Settings:
		if (Menu)
		{
			Menu->OpenAt(EDTMenuScreen::Settings);
		}
		break;
	case EDTHubAction::Quit:
		if (Menu)
		{
			Menu->OpenQuitConfirm();
		}
		break;
	case EDTHubAction::Begin: BeginRun(); break;
	case EDTHubAction::TreeAccept:
		if (Tree.IsValid())
		{
			Tree->Accept(Outcome.Row);
		}
		break;
	default: break;
	}
}

void UDTHubSubsystem::Command(EDTMenuCommand Command)
{
	if (bActive && !bBeginPending)
	{
		State.Window = DTHubLayout::ListWindow(DTSettings::TextScale(), State.Page != EDTHubPage::Stats);
		State.TreeRows = Tree.IsValid() ? Tree->NumRows() : 0;
		HandleOutcome(State.Apply(Command));
	}
}

void UDTHubSubsystem::PollKeys()
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const UDTMenuSubsystem* Menu = GetWorld() ? GetWorld()->GetSubsystem<UDTMenuSubsystem>() : nullptr;
	if (!PC)
	{
		return;
	}
	// While a menu runs the keys are its own, and a press that closed it stays down: the frame after it still counts as open.
	const bool bMenuNow = Menu && Menu->IsOpen();
	const bool bAbsorb = bMenuNow || bMenuLastFrame;
	bMenuLastFrame = bMenuNow;
	for (const FKey& Key : DTMenuInput::Keys())
	{
		if (!PC->IsInputKeyDown(Key))
		{
			Down.Remove(Key);
		}
		else if (!Down.Contains(Key))
		{
			Down.Add(Key);
			if (!bAbsorb)
			{
				Command(DTMenuInput::ForKey(Key));
			}
		}
	}
}

void UDTHubSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (bActive)
	{
		if (bBeginPending && !IsAcceptHeld())
		{
			FinishBegin();
		}
		PollKeys();
	}
#if !UE_BUILD_SHIPPING
	DTHubCheck::Tick(this);
#endif
}

void UDTHubSubsystem::Draw(UCanvas* Canvas)
{
	if (bActive)
	{
		State.Window = DTHubLayout::ListWindow(DTSettings::TextScale(), State.Page != EDTHubPage::Stats); // the Ledger-stone has no detail pane
		State.TreeRows = Tree.IsValid() ? Tree->NumRows() : 0;
		FDTHubView View;
		View.State = &State;
		View.Codex = &Codex;
		View.Profile = Profile();
		View.Tree = Tree.Get();
		DTHubScreen::Draw(Canvas, View, DTSettings::Live());
	}
}
