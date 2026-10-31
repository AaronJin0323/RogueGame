#include "Meta/DTMetaSubsystem.h"
#include "Dusktracer.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Meta/DTDiscovery.h"
#include "Meta/DTRunRecorder.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/CString.h"
#include "Misc/Parse.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#endif

namespace DTMetaSubsystemLocal
{
	/** -DTMetaFile=<path>, empty when absent. */
	static FString PathOverride()
	{
		FString Path;
		FParse::Value(FCommandLine::Get(), TEXT("DTMetaFile="), Path);
		return Path;
	}

#if !UE_BUILD_SHIPPING
	static FAutoConsoleCommandWithWorldAndArgs CmdWins(TEXT("DT.Meta.Wins"), TEXT("Set this session's wins and write the file (Tools/MetaCheck.ps1)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			FDTMetaSession* Session = UDTMetaSubsystem::FindSession(World);
			if (Session && Args.Num() == 1)
			{
				Session->Profile.Wins = FCString::Atoi(*Args[0]);
				Session->Profile.Runs = FMath::Max(Session->Profile.Runs, Session->Profile.Wins);
				Session->Write();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs CmdPlant(TEXT("DT.Meta.Plant"), TEXT("Add to a counter of this run's tally (Tools/MetaCheck.ps1): DT.Meta.Plant <key> <amount>."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UDTRunRecorder* Recorder = World ? World->GetSubsystem<UDTRunRecorder>() : nullptr;
			if (Recorder && Args.Num() == 2)
			{
				Recorder->AddCounter(FName(*Args[0]), FCString::Atoi64(*Args[1]));
			}
		}));
#endif
}

bool UDTMetaSubsystem::IsToolRun()
{
	const TCHAR* const Command = FCommandLine::Get();
	return FApp::IsUnattended() || GIsAutomationTesting || FParse::Param(Command, TEXT("DTNoSave")) || FCString::Stristr(Command, TEXT("-DTBot")) != nullptr;
}

FString UDTMetaSubsystem::ActivePath()
{
	const FString Override = DTMetaSubsystemLocal::PathOverride();
	if (!DTMeta::UsesFile(IsToolRun(), !Override.IsEmpty()))
	{
		return FString();
	}
	return Override.IsEmpty() ? DTMeta::DefaultPath() : Override;
}

void UDTMetaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Session.OnBank.AddStatic(&DTDiscovery::Apply);
	const FString Path = ActivePath();
	const EDTMetaOpen Opened = Session.Open(Path);
	UE_LOG(LogDT, Log, TEXT("Meta: opened (%d) %s, balance %lld, wins %d, discoveries %d"), static_cast<int32>(Opened), Path.IsEmpty() ? TEXT("no save") : *Path, Session.Profile.Currency, Session.Profile.Wins, Session.Profile.Discoveries.Num());
}

FDTMetaSession* UDTMetaSubsystem::FindSession(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
	UDTMetaSubsystem* Meta = Instance ? Instance->GetSubsystem<UDTMetaSubsystem>() : nullptr;
	return Meta ? &Meta->Session : nullptr;
}

int32 UDTMetaSubsystem::GetWins(const UObject* WorldContext)
{
	const FDTMetaSession* Found = FindSession(WorldContext);
	return Found ? Found->Profile.Wins : 0;
}
