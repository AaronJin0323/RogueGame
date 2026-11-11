#include "Hub/DTHubSubsystem.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Game/DTGameMode.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Hub/DTHubCheck.h"
#include "Hub/DTHubFlow.h"
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
	Keys = FDTHubKeys(); // what is held now came from the screen before
	const FDTMetaProfile& Read = *Profile();
	const UDTWeaponSubsystem* Weapons = GetWorld() ? GetWorld()->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	State.Open(DTHubData::Patrons(Read, [Weapons](FName Id) { return Weapons && Weapons->IsInPool(Id); }), DTHubData::ParsePath(Read.LastPatron));
	Codex = DTHubData::BuildCodex(DTHubData::ReadCodexTable(), Read);
	State.CodexRows = Codex.Num();
	State.CodexFirst = FMath::Max(0, Codex.IndexOfByPredicate([](const FDTCodexRow& Row) { return Row.bRead; }));
	Tree = DTHubTabs::MakeTree(GetWorld());
	// What this launch has to say of its save (R-VIG-21): a save it is stuck on, a bad file set aside, or a run the launch before
	// could not write. The name is a file's, never a path.
	FString NoticeFile;
	const EDTMetaNotice Kind = UDTMetaSubsystem::ReadNotice(this, &NoticeFile);
	Notice = DTHubText::Notice(Kind, NoticeFile);
	if (Kind != EDTMetaNotice::None)
	{
		const TCHAR* const Said = Kind == EDTMetaNotice::SaveUnreadable ? TEXT("SaveUnreadable")
			: Kind == EDTMetaNotice::SaveNotMoved ? TEXT("SaveNotMoved")
			: Kind == EDTMetaNotice::KeptAside ? TEXT("SaveAside") : TEXT("RunLost");
		UE_LOG(LogDT, Log, TEXT("Hub: notice %s %s"), Said, *NoticeFile);
	}
	if (UDTMenuSubsystem* Menu = GetWorld() ? GetWorld()->GetSubsystem<UDTMenuSubsystem>() : nullptr)
	{
		Menu->SetHubHost();
	}
	UE_LOG(LogDT, Log, TEXT("Hub: open, balance %lld, runs %d, last patron '%s'"), Read.Currency, Read.Runs, *Read.LastPatron.ToString());
}

#if !UE_BUILD_SHIPPING
void UDTHubSubsystem::ShowEverythingForCheck()
{
	// The hub as a profile that has earned every row sees it: no save is read or written, only what this hub draws changes.
	const TArray<FDTCodexSource> Sources = DTHubData::ReadCodexTable();
	Codex = DTHubData::BuildCodex(Sources, DTHubData::EverythingRead(Sources));
	State.CodexRows = Codex.Num();
	State.CodexFirst = 0;
	for (FDTHubPatron& Patron : State.Patrons)
	{
		Patron.bOpen = true;
	}
}

void UDTHubSubsystem::ShowNoticeForCheck(EDTMetaNotice Kind, const FString& AsideFile)
{
	Notice = DTHubText::Notice(Kind, AsideFile);
}
#endif

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
	// -DTSeed names a launch's first run, and a launch holds as many runs as a person plays (R-VIG-07).
	static bool bFirstRun = true;
	UDTSimSubsystem* const Sim = GetWorld() ? GetWorld()->GetSubsystem<UDTSimSubsystem>() : nullptr;
	const int32 Seed = DTHubFlow::BeginSeed(Sim ? Sim->GetLaunchSeed() : FDTRunSeed::ForThisLaunch(), bFirstRun, FPlatformTime::Cycles());
	bFirstRun = false;
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

void UDTHubSubsystem::SyncPage()
{
	State.Window = DTHubLayout::ListWindow(DTSettings::TextScale(), State.Page != EDTHubPage::Stats); // the Ledger-stone has no detail pane
	State.TreeRows = Tree.IsValid() ? Tree->NumRows() : 0;
	State.TreeHeadings.Reset();
	for (int32 Row = 0; Row < State.TreeRows; ++Row)
	{
		if (Tree->IsHeading(Row))
		{
			State.TreeHeadings.Add(Row);
		}
	}
}

void UDTHubSubsystem::Command(EDTMenuCommand Command)
{
	if (bActive && !bBeginPending)
	{
		SyncPage();
		const EDTHubPage From = State.Page;
		const FDTHubOutcome Outcome = State.Apply(Command);
		if (State.Page == EDTHubPage::Tree && From != EDTHubPage::Tree && Tree.IsValid())
		{
			Tree->Opened();
		}
		HandleOutcome(Outcome);
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
	// A stick a little off its axis holds two direction keys down: only the larger axis is a press (review #114).
	const float StickX = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	const float StickY = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
	const EDTMenuCommand Pressed = Keys.Poll([PC, StickX, StickY](const FKey& Key) { return PC->IsInputKeyDown(Key) && DTMenuInput::StickKeyCounts(Key, StickX, StickY); }, bAbsorb);
	if (Pressed != EDTMenuCommand::None)
	{
		Command(Pressed);
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
		SyncPage();
		FDTHubView View;
		View.State = &State;
		View.Codex = &Codex;
		View.Profile = Profile();
		View.Tree = Tree.Get();
		View.Notice = Notice;
		Overflow = DTHubScreen::Draw(Canvas, View, DTSettings::Live());
	}
}
