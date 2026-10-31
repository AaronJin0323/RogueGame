// A win on a real gated save, proved on one launch (meta.md T-MET-29, Tools/WinCheck.ps1): the toast on the run-over screen, the keys in the
// file on disk, the way back and the Codex open on what the run earned, the bestiary included. -DTWinCheck=<dir> starts it and shots land there.
// A bot plays; every line after the run reads the same objects a person's screen does. R-TEC-12: compiled out of Shipping.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Dusktracer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/DTGameMode.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Hub/DTHubSubsystem.h"
#include "Meta/DTMetaSession.h"
#include "Meta/DTMetaSubsystem.h"
#include "Meta/DTRunRecorder.h"
#include "Misc/CommandLine.h"
#include "Misc/DelayedAutoRegister.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"
#include "Settings/DTMenuScreen.h"
#include "Settings/DTMenuSubsystem.h"
#include "UnrealClient.h"

namespace DTDebugWinCheck
{
	// Wall seconds: how long a frame settles before a shot, how often the file is polled, when the wait for a run or a step gives up.
	constexpr double ShotSettleS = 0.5;
	constexpr double ShotPollS = 0.25;
	constexpr double RunGiveUpS = 500.0;
	constexpr double StepGiveUpS = 60.0;
	/** Commands a step may send to reach a Codex row or a root row: more than any page holds. */
	constexpr int32 PageGuard = 512;

	enum class EStage : uint8 { Run, ShotRunOver, File, Back, Hub, Codex, ShotCodex };

	struct FCheck
	{
		FString Dir;
		EStage Stage = EStage::Run;
		double StageAt = 0.0;
		int32 Fails = 0;
		bool bShotAsked = false;
		int64 ShotSize = -1;
		double ShotPollAt = 0.0;
		bool bFinished = false;
		TSet<FName> Earned;
	};
	static FCheck Check;

	static UWorld* GameWorld()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::Game && Context.World())
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static void Expect(const FString& What, bool bGood)
	{
		Check.Fails += bGood ? 0 : 1;
		UE_LOG(LogDT, Log, TEXT("WINCHECK %s: %s"), *What, bGood ? TEXT("ok") : TEXT("WRONG"));
	}

	static void Next(EStage Stage)
	{
		Check.Stage = Stage;
		Check.StageAt = FPlatformTime::Seconds();
	}

	static void Finish(const TCHAR* Why)
	{
		UE_LOG(LogDT, Log, TEXT("WINCHECK: %s (%d wrong%s%s)"), Check.Fails == 0 && !Why[0] ? TEXT("PASS") : TEXT("FAIL"), Check.Fails, Why[0] ? TEXT(", ") : TEXT(""), Why);
		Check.bFinished = true;
		FPlatformMisc::RequestExit(false, TEXT("DT win check over"));
	}

	/** One tick of a shot: true once <Dir>/<Name>.png stands whole. */
	static bool Shot(const TCHAR* Name)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Check.StageAt < ShotSettleS)
		{
			return false;
		}
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Check.Dir, FString(Name) + TEXT(".png")));
		if (!Check.bShotAsked)
		{
			Check.bShotAsked = true;
			IFileManager::Get().Delete(*Path, false, true, true);
			FScreenshotRequest::RequestScreenshot(Path, /*bInShowUI*/ true, /*bAddFilenameSuffix*/ false);
			Check.ShotSize = -1;
			Check.ShotPollAt = Now + ShotPollS;
			return false;
		}
		if (Now < Check.ShotPollAt)
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
		Check.ShotPollAt = Now + ShotPollS;
		return false;
	}

	/** The run is over and banked: the toast is what the screen draws, from the award the screen reads. */
	static bool ReadRunOver(UWorld* World)
	{
		const UDTMenuSubsystem* Menu = World->GetSubsystem<UDTMenuSubsystem>();
		const UDTRunRecorder* Recorder = World->GetSubsystem<UDTRunRecorder>();
		if (!Menu || !Recorder || Menu->GetState().Screen != EDTMenuScreen::RunOver || !Recorder->GetAward().bBanked)
		{
			return false;
		}
		const ADTGameMode* Mode = World->GetAuthGameMode<ADTGameMode>();
		const FDTRunAward& Award = Recorder->GetAward();
		Expect(TEXT("the pool was gated"), UDTMetaSubsystem::GatesPool());
		Expect(TEXT("the run is a win"), Mode && Mode->IsVictory());
		Expect(TEXT("the bank names Win"), Award.Discovered.Contains(FName(TEXT("Win"))));
		Check.Earned = TSet<FName>(Award.Discovered);
		const FString Toast = DTMenuText::DiscoveryToast(Award.Discovered).ToString();
		const FString WinName = FText::FromStringTable(TEXT("ST_Discoveries"), TEXT("Discovery.Win.Name")).ToString();
		UE_LOG(LogDT, Log, TEXT("WINCHECK toast: %s"), *Toast);
		Expect(TEXT("the toast names the Discovery"), !WinName.IsEmpty() && Toast.Contains(WinName));
		Expect(TEXT("the screen's first row is the way back"), Menu->GetState().Focus == 0 && Menu->GetState().bHubRow);
		return true;
	}

	/** The file on disk, read by a second session: what a new process would open. */
	static void ReadFile()
	{
		FDTMetaSession Reader;
		const EDTMetaOpen Opened = Reader.Open(UDTMetaSubsystem::ActivePath());
		Expect(TEXT("the file reads back"), Opened == EDTMetaOpen::Read);
		Expect(TEXT("it holds the win"), Reader.Profile.Wins == 1 && Reader.Profile.Runs == 1);
		for (const DTCat::FDiscoveryRow& Row : DTCat::Discoveries)
		{
			if (!Check.Earned.Contains(FName(Row.Id)))
			{
				continue;
			}
			Expect(FString::Printf(TEXT("%s is in the file"), Row.Id), Reader.Profile.Discoveries.Contains(FName(Row.Id)) && Reader.Profile.Unlocked.Contains(FName(FString::Printf(TEXT("Discovery.%s"), Row.Id))));
			for (const TCHAR* Target : Row.Targets)
			{
				if (Target[0] != TEXT('\0'))
				{
					Expect(FString::Printf(TEXT("%s wrote %s"), Row.Id, Target), Reader.Profile.Unlocked.Contains(FName(Target)));
				}
			}
		}
		TArray<FString> Keys;
		for (const FName& Key : Reader.Profile.Unlocked)
		{
			Keys.Add(Key.ToString());
		}
		Keys.Sort();
		UE_LOG(LogDT, Log, TEXT("WINCHECK unlocked: %s"), *FString::Join(Keys, TEXT(" ")));
	}

	/** The hub's own rows, after the reload: the Shrine and the book read the saved profile (vigil.md R-VIG-05, R-VIG-09). */
	static void ReadHub(UDTHubSubsystem* Hub)
	{
		const FDTMetaProfile* Profile = UDTMetaSubsystem::ReadProfile(Hub);
		const TArray<EDTPath> Seated = DTOffers::RunKeepers();
		for (const FDTHubPatron& Patron : Hub->GetState().Patrons)
		{
			const bool bMet = Seated.Contains(Patron.Path) || (Profile && Profile->Unlocked.Contains(DTOffers::KeeperId(Patron.Path)));
			Expect(FString::Printf(TEXT("the Shrine %s %s"), bMet ? TEXT("opens") : TEXT("seals"), DTOffers::PathName(Patron.Path)), Patron.bOpen == bMet);
		}
	}

	static const FDTCodexRow* FindRow(UDTHubSubsystem* Hub, const FString& Key)
	{
		return Hub->GetCodex().FindByPredicate([&Key](const FDTCodexRow& Row) { return Row.Key == Key; });
	}

	static void ReadBook(UDTHubSubsystem* Hub)
	{
		// vigil.md R-VIG-09: each bestiary row against the kills the save holds; the two roles the game does not have stay locked (R-MET-27).
		for (const TCHAR* Role : { TEXT("Swarm"), TEXT("Tank"), TEXT("Rusher"), TEXT("Turret"), TEXT("Boss") })
		{
			const FDTCodexRow* Row = FindRow(Hub, FString::Printf(TEXT("Codex.Enemy.%s"), Role));
			Expect(FString::Printf(TEXT("the bestiary opens %s"), Role), Row && Row->bRead && !Row->Body.IsEmpty());
		}
		for (const TCHAR* Role : { TEXT("Bruiser"), TEXT("Support") })
		{
			const FDTCodexRow* Row = FindRow(Hub, FString::Printf(TEXT("Codex.Enemy.%s"), Role));
			Expect(FString::Printf(TEXT("the bestiary keeps %s shut"), Role), Row && !Row->bRead && Row->Body.IsEmpty());
		}
		for (const DTCat::FDiscoveryRow& Row : DTCat::Discoveries)
		{
			const FDTCodexRow* Line = FindRow(Hub, Row.Recollection);
			Expect(FString::Printf(TEXT("%s reads as earned"), Row.Recollection), Line && Line->bRead == Check.Earned.Contains(FName(Row.Id)));
		}
	}

	static bool Tick(float)
	{
		if (Check.bFinished)
		{
			return false;
		}
		UWorld* World = GameWorld();
		if (!World || !World->HasBegunPlay())
		{
			return true;
		}
		if (FPlatformTime::Seconds() - Check.StageAt > (Check.Stage == EStage::Run ? RunGiveUpS : StepGiveUpS))
		{
			Finish(*FString::Printf(TEXT("stage %d never ended"), static_cast<int32>(Check.Stage)));
			return false;
		}
		UDTHubSubsystem* Hub = World->GetSubsystem<UDTHubSubsystem>();
		UDTMenuSubsystem* Menu = World->GetSubsystem<UDTMenuSubsystem>();
		switch (Check.Stage)
		{
		case EStage::Run:
			if (ReadRunOver(World))
			{
				Next(EStage::ShotRunOver);
			}
			break;
		case EStage::ShotRunOver:
			if (Shot(TEXT("runover")))
			{
				Next(EStage::File);
			}
			break;
		case EStage::File:
			ReadFile();
			Next(EStage::Back);
			break;
		case EStage::Back:
			if (Menu && FPlatformTime::Seconds() - Check.StageAt > DTConst::SETTINGS_RUN_OVER_GUARD_S + ShotSettleS)
			{
				Menu->Command(EDTMenuCommand::Accept); // R-VIG-13: the first row, past the guard
				Next(EStage::Hub);
			}
			break;
		case EStage::Hub:
			if (Hub && Hub->IsActive() && Hub->GetState().Page == EDTHubPage::Root)
			{
				ReadHub(Hub);
				for (int32 Guard = 0; Hub->GetState().RootRow() != EDTHubRow::Codex && Guard < PageGuard; ++Guard)
				{
					Hub->Command(EDTMenuCommand::Down);
				}
				Hub->Command(EDTMenuCommand::Accept);
				Expect(TEXT("the Codex opens"), Hub->GetState().Page == EDTHubPage::Codex && Hub->GetCodex().Num() > 0);
				Next(EStage::Codex);
			}
			break;
		case EStage::Codex:
			ReadBook(Hub);
			for (int32 Guard = 0; Guard < PageGuard; ++Guard)
			{
				const int32 Focus = Hub->GetState().Focus;
				if (Hub->GetCodex().IsValidIndex(Focus) && Hub->GetCodex()[Focus].Key == TEXT("Codex.Recollection.Win"))
				{
					break;
				}
				Hub->Command(EDTMenuCommand::Down);
			}
			Next(EStage::ShotCodex);
			break;
		case EStage::ShotCodex:
			if (Shot(TEXT("codex")))
			{
				Finish(TEXT(""));
			}
			break;
		}
		return !Check.bFinished;
	}

	static FDelayedAutoRegisterHelper Register(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
		if (FParse::Value(FCommandLine::Get(), TEXT("DTWinCheck="), Check.Dir) && !Check.Dir.IsEmpty())
		{
			Check.StageAt = FPlatformTime::Seconds();
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
		}
	});
}

#endif
