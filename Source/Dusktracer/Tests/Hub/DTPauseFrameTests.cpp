// A run that ends on the frame a pause lands (meta.md T-MET-47, R-MET-04, settings.md R-SET-21; review #1 and #8 of
// Validation/S3_review_fixes.md), and D-041's shared step under such a pause (boss.md T-BOS-14).
// Played as a launch plays it: a game instance, the game mode made by the world, a player controller, the engine's own
// pause (AGameModeBase::SetPause) and whole frames through UWorld::Tick, so the timer manager and every tickable run, or do
// not run, as LevelTick.cpp decides. The pause is asked for where a key press is: in the frame, before the tickables.
// It reads the menu (Settings/) and the bank (Meta/) of one run, so it lives under Tests/Hub, the test folder both reader rules leave
// open to both (T-SET-02, T-MET-15).
// Run headless: Tools\RunTests.ps1 -Filter Dusktracer.Meta.PauseFrame
#include "Misc/AutomationTest.h"
#include "Boss/DTBossSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DTGameMode.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Meta/DTMetaSession.h"
#include "Meta/DTMetaSubsystem.h"
#include "Meta/DTRunRecorder.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHeroSubsystem.h"
#include "Settings/DTMenuScreen.h"
#include "Settings/DTMenuSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTPauseFrameTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	/** A frame a little longer than a step: one step a frame over the few frames a scene plays. */
	constexpr float FrameS = 1.01f / static_cast<float>(DTConst::SIM_FIXED_HZ);

	/** How the run ends. The last two are D-041's orders (boss.md R-BOS-13). */
	enum class EEnd : uint8
	{
		Death,    // the hero falls outside a fight
		Win,      // the hull falls; the win is read on the next step
		SameStep, // the hero falls in the step whose kills take the hull
		Trade     // both last hits dealt on one step; the win is read before the hero's hit is applied
	};

	/** What pauses the world on the frame of the run's last step. */
	enum class EPause : uint8
	{
		None, // the control: the next frame's timer closes the run
		Key,  // IA_Pause: ADTHero::OnPauseStarted calls UDTMenuSubsystem::RequestPause while the frame's actors tick
		Hand  // a level-up queued inside the last step: the offer system pauses for the hand
	};

	/** Pickups phase: queues a level-up hand on one step, as progression does. */
	struct FLevelUpAt : IDTSimSystem
	{
		int64 QueueAt = -1;
		virtual void SimStep(const FDTSimContext& Context) override
		{
			if (Context.StepIndex == QueueAt)
			{
				FDTOfferHand Request;
				Request.HandKind = EDTHandKind::Level;
				Request.Level = 2;
				Context.Run->GetPlayer(0).Offer.PendingHands.Add(Request);
			}
		}
	};

	struct FResult
	{
		bool bRan = false;
		/** After the frame of the last step: the run is over and the world is paused. */
		bool bOverOnFrame = false;
		bool bPausedOnFrame = false;
		bool bInputRan = false;
		/** Three frames later, with nobody having left the run. */
		bool bClosed = false;
		bool bModeVictory = false;
		bool bRunOverScreen = false;
		EDTRunOverKind Kind = EDTRunOverKind::Abandoned;
		FDTRunAward Award;
		FText Summary;
		FString Jsonl;
		bool bSaveRead = false;
		FDTMetaProfile Saved;
	};

	/** One frame of the world. Input runs where a key press does: after the frame has read whether it is paused, before the tickables. */
	static bool Frame(UWorld* World, TFunction<void()> Input = nullptr)
	{
		bool bInputRan = false;
		FDelegateHandle Handle;
		if (Input)
		{
			Handle = FWorldDelegates::OnWorldPreActorTick.AddLambda([World, &Input, &bInputRan](UWorld* Ticked, ELevelTick, float)
			{
				if (Ticked == World && !bInputRan)
				{
					bInputRan = true;
					Input();
				}
			});
		}
		++GFrameCounter; // the engine loop's: a timer manager ticks once a frame number
		World->Tick(LEVELTICK_All, FrameS);
		FWorldDelegates::OnWorldPreActorTick.Remove(Handle);
		return bInputRan;
	}

	static FResult Play(FAutomationTestBase& Test, EEnd End, EPause Pause)
	{
		FResult Out;
		// A person's game: no hand picks itself (the tests run -unattended).
		IConsoleVariable* AutoPick = IConsoleManager::Get().FindConsoleVariable(TEXT("dt.Offer.AutoPick"));
		const int32 AutoPickWas = AutoPick ? AutoPick->GetInt() : -1;
		if (AutoPick)
		{
			AutoPick->Set(0, ECVF_SetByCode);
		}
		const FString SavePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("PauseFrame.sav"));
		IFileManager::Get().Delete(*SavePath, false, true, true);

		FLevelUpAt LevelUp; // outlives the world's sim
		UGameInstance* Instance = NewObject<UGameInstance>(GEngine);
		Instance->AddToRoot();
		Instance->InitializeStandalone();
		UWorld* World = Instance->GetWorld();
		World->GetWorldSettings()->DefaultGameMode = ADTGameMode::StaticClass();
		World->SetGameMode(FURL());
		World->InitializeActorsForPlay(FURL());
		ADTGameMode* Mode = World->GetAuthGameMode<ADTGameMode>();
		// A launch spawns the local player's controller before play begins (UEngine::LoadMap).
		APlayerController* PC = Mode ? World->SpawnActor<APlayerController>(Mode->PlayerControllerClass) : nullptr;
		World->BeginPlay();

		UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
		UDTBossSubsystem* Boss = World->GetSubsystem<UDTBossSubsystem>();
		UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
		UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
		UDTOfferSubsystem* Offers = World->GetSubsystem<UDTOfferSubsystem>();
		UDTMenuSubsystem* Menu = World->GetSubsystem<UDTMenuSubsystem>();
		UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
		UDTRunRecorder* Recorder = World->GetSubsystem<UDTRunRecorder>();
		FDTMetaSession* Session = UDTMetaSubsystem::FindSession(World);
		if (Test.TestNotNull(TEXT("the world made the game mode"), Mode) && Test.TestNotNull(TEXT("a player controller"), PC) && Test.TestNotNull(TEXT("its player state"), PC->PlayerState.Get())
			&& Test.TestNotNull(TEXT("sim"), Sim) && Test.TestNotNull(TEXT("boss"), Boss) && Test.TestNotNull(TEXT("enemies"), Enemies) && Test.TestNotNull(TEXT("heroes"), Heroes)
			&& Test.TestNotNull(TEXT("offers"), Offers) && Test.TestNotNull(TEXT("menu"), Menu) && Test.TestNotNull(TEXT("telemetry"), Telemetry) && Test.TestNotNull(TEXT("recorder"), Recorder)
			&& Test.TestNotNull(TEXT("the game instance's save session"), Session) && Test.TestTrue(TEXT("the game mode opened a session"), Telemetry->IsSessionActive()))
		{
			Session->Open(SavePath); // a real file, so the bank is read back from disk
			Sim->Register(EDTSimPhase::Pickups, &LevelUp);

			// The run-start hand holds the world paused until a card is picked.
			Frame(World);
			Test.TestTrue(TEXT("the Patron's hand pauses the world"), World->IsPaused() && Offers->GetOpenHand(0) != nullptr);
			Test.TestEqual(TEXT("no step under the hand"), Sim->GetStepIndex(), static_cast<int64>(0));
			Offers->Pick(0, 0);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Frame(World);
			}
			Test.TestFalse(TEXT("the pick lets the world go"), World->IsPaused());
			Test.TestTrue(TEXT("frames step the sim"), Sim->GetStepIndex() >= 2);

			if (End != EEnd::Death)
			{
				Boss->StartFight();
				for (int32 Index = 0; Index < 4; ++Index)
				{
					Frame(World);
				}
				Test.TestTrue(TEXT("the hull stands"), Boss->GetCore().HasHull());
			}
			const auto KillHull = [Sim, Enemies, &Test]()
			{
				const FDTEnemyPool& Pool = Enemies->GetPool();
				int32 Index = INDEX_NONE;
				for (int32 At = 0; At < Pool.Num(); ++At)
				{
					Index = Pool.GetRoles()[At] == EDTEnemyRole::Boss ? At : Index;
				}
				if (Test.TestTrue(TEXT("a hull to kill"), Index != INDEX_NONE))
				{
					FDTHit Hit;
					Hit.EnemyIndex = Index;
					Hit.EnemySerial = Pool.GetSerials()[Index];
					Hit.OwnerId = 0;
					Hit.Damage = Pool.GetHp()[Index] * 2.0f;
					Sim->GetView().Hits.Add(Hit);
				}
			};
			const auto HitHero = [Sim]()
			{
				FDTHeroHit Hit;
				Hit.PlayerId = 0;
				Hit.Damage = static_cast<float>(DTConst::HERO_MAX_HP) * 10.0f;
				Sim->GetView().HeroHits.Add(Hit);
			};
			switch (End)
			{
			case EEnd::Death:
				HitHero();
				break;
			case EEnd::Win:
				KillHull();
				Frame(World);
				Test.TestFalse(TEXT("the kill's own step does not end the run"), Mode->IsRunOver());
				break;
			case EEnd::SameStep:
				KillHull();
				HitHero();
				break;
			case EEnd::Trade:
				KillHull();
				Frame(World);
				Test.TestFalse(TEXT("the kill's own step does not end the run"), Mode->IsRunOver());
				HitHero();
				break;
			}
			Test.TestFalse(TEXT("not paused before the last frame"), World->IsPaused());

			// The frame of the run's last step, with the pause landing in it.
			if (Pause == EPause::Hand)
			{
				LevelUp.QueueAt = Sim->GetStepIndex();
			}
			Out.bInputRan = Frame(World, Pause == EPause::Key ? TFunction<void()>([Menu]() { Menu->RequestPause(); }) : TFunction<void()>());
			Out.bOverOnFrame = Mode->IsRunOver();
			if (Pause == EPause::Hand && !World->IsPaused())
			{
				Frame(World); // the offer system ticks before the sim: it opens the hand the frame after the step that queued it
			}
			Out.bPausedOnFrame = World->IsPaused();
			// Nobody leaves: whatever closes the run has to do it without the player.
			for (int32 Index = 0; Index < 3; ++Index)
			{
				Frame(World);
			}
			Out.bClosed = !Telemetry->IsSessionActive();
			Out.bModeVictory = Mode->IsVictory();
			Out.bRunOverScreen = Menu->GetState().Screen == EDTMenuScreen::RunOver;
			Out.Kind = Menu->GetState().RunOver;
			Out.Award = Recorder->GetAward();
			// What UDTMenuSubsystem::Draw puts under the rows.
			Out.Summary = Out.bRunOverScreen && Out.Award.bBanked ? DTMenuText::RunSummary(Out.Award.Retained, Out.Award.Balance) : FText::GetEmpty();
			Telemetry->EndSession(TEXT("quit")); // a run still open here closes as the player's leaving would close it
			Out.bRan = true;

			const FString CsvPath = Telemetry->GetWriter().GetCsvPath();
			const FString JsonlPath = Telemetry->GetWriter().GetJsonlPath();
			FFileHelper::LoadFileToString(Out.Jsonl, *JsonlPath);
			IFileManager::Get().Delete(*CsvPath, false, true, true);
			IFileManager::Get().Delete(*JsonlPath, false, true, true);
			FDTMetaSession Read;
			Out.bSaveRead = Read.Open(SavePath) == EDTMetaOpen::Read;
			Out.Saved = Read.Profile;
			Sim->Unregister(&LevelUp);
		}
		World->EndPlay(EEndPlayReason::LevelTransition);
		Instance->Shutdown();
		World->DestroyWorld(true);
		GEngine->DestroyWorldContext(World);
		Instance->RemoveFromRoot();
		IFileManager::Get().Delete(*SavePath, false, true, true);
		if (AutoPick)
		{
			AutoPick->Set(AutoPickWas, ECVF_SetByCode);
		}
		return Out;
	}

	/** The run closed by itself with its true end, on the screen, in the records, and in the save file. */
	static void Expect(FAutomationTestBase& Test, const FResult& R, const TCHAR* What, bool bVictory, bool bPaused)
	{
		if (!R.bRan)
		{
			return;
		}
		const FString Tag = FString(What) + TEXT(": ");
		const TCHAR* const Reason = bVictory ? TEXT("victory") : TEXT("death");
		Test.TestTrue(Tag + TEXT("the run ended on that frame"), R.bOverOnFrame);
		Test.TestEqual(Tag + TEXT("the world is paused by the frame's end"), R.bPausedOnFrame, bPaused);
		Test.TestTrue(Tag + TEXT("the run closed with nobody leaving"), R.bClosed);
		Test.TestEqual(Tag + TEXT("the game mode's win"), R.bModeVictory, bVictory);
		Test.TestTrue(Tag + TEXT("the run-over screen is open"), R.bRunOverScreen);
		Test.TestTrue(Tag + TEXT("with the run's own title"), R.Kind == (bVictory ? EDTRunOverKind::Victory : EDTRunOverKind::Death));
		Test.TestTrue(Tag + TEXT("the run is banked"), R.Award.bBanked);
		Test.TestFalse(Tag + TEXT("the summary line is there"), R.Summary.IsEmpty());
		Test.TestTrue(Tag + TEXT("run_end names the end"), R.Jsonl.Contains(FString::Printf(TEXT("\"kind\":\"run_end\",\"owner\":-1,\"reason\":\"%s\""), Reason)));
		Test.TestTrue(Tag + TEXT("run_award names the end"), R.Jsonl.Contains(FString::Printf(TEXT("\"kind\":\"run_award\",\"owner\":-1,\"end\":\"%s\""), Reason)));
		Test.TestFalse(Tag + TEXT("nothing closed it as a left run"), R.Jsonl.Contains(TEXT("\"reason\":\"quit\"")) || R.Jsonl.Contains(TEXT("\"end\":\"left\"")));
		Test.TestTrue(Tag + TEXT("the save file reads"), R.bSaveRead);
		Test.TestEqual(Tag + TEXT("one run on the save"), R.Saved.Runs, 1);
		Test.TestEqual(Tag + TEXT("wins on the save"), R.Saved.Wins, bVictory ? 1 : 0);
		Test.TestEqual(Tag + TEXT("deaths since a win on the save"), R.Saved.DeathsSinceWin, bVictory ? 0 : 1);
		Test.TestEqual(Tag + TEXT("the save holds the award"), R.Saved.Currency, R.Award.Retained);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetPauseFrame, "Dusktracer.Meta.PauseFrame", DTPauseFrameTest::Flags) // T-MET-47
bool FDTMetPauseFrame::RunTest(const FString& Parameters)
{
	using namespace DTPauseFrameTest;
	// The controls: with no pause the next frame's timer closes the run, so the frames here do fire timers.
	Expect(*this, Play(*this, EEnd::Death, EPause::None), TEXT("death, no pause"), false, false);
	Expect(*this, Play(*this, EEnd::Win, EPause::None), TEXT("win, no pause"), true, false);
	// Pause pressed on the frame of the last step.
	const FResult DeathKey = Play(*this, EEnd::Death, EPause::Key);
	TestTrue(TEXT("death, Pause: the press ran inside the frame"), DeathKey.bInputRan);
	Expect(*this, DeathKey, TEXT("death, Pause on the frame"), false, true);
	const FResult WinKey = Play(*this, EEnd::Win, EPause::Key);
	TestTrue(TEXT("win, Pause: the press ran inside the frame"), WinKey.bInputRan);
	Expect(*this, WinKey, TEXT("win, Pause on the frame"), true, true);
	// A level-up in the last step: the hand's pause.
	Expect(*this, Play(*this, EEnd::Death, EPause::Hand), TEXT("death, a hand in the step"), false, true);
	Expect(*this, Play(*this, EEnd::Win, EPause::Hand), TEXT("win, a hand in the step"), true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossSharedStepPaused, "Dusktracer.Boss.RunEndPaused", DTPauseFrameTest::Flags) // T-BOS-14
bool FDTBossSharedStepPaused::RunTest(const FString& Parameters)
{
	using namespace DTPauseFrameTest;
	// D-041's two orders with Pause, or a hand, landing on the frame of the shared step: one outcome, a win, banked as one.
	const TPair<EEnd, const TCHAR*> Orders[] = { { EEnd::SameStep, TEXT("same step") }, { EEnd::Trade, TEXT("trade") } };
	const TPair<EPause, const TCHAR*> Pauses[] = { { EPause::None, TEXT("no pause") }, { EPause::Key, TEXT("Pause on the frame") }, { EPause::Hand, TEXT("a hand in the step") } };
	for (const TPair<EEnd, const TCHAR*>& Order : Orders)
	{
		for (const TPair<EPause, const TCHAR*>& Pause : Pauses)
		{
			const FString What = FString::Printf(TEXT("%s, %s"), Order.Value, Pause.Value);
			const FResult R = Play(*this, Order.Key, Pause.Key);
			Expect(*this, R, *What, true, Pause.Key != EPause::None);
			TestEqual(What + TEXT(": no record says death"), R.Jsonl.Contains(TEXT("\"death\"")), false);
			TestTrue(What + TEXT(": boss_end says win"), R.Jsonl.Contains(TEXT("\"kind\":\"boss_end\",\"owner\":-1,\"outcome\":\"win\"")));
			TestTrue(What + TEXT(": the award holds the boss"), R.Jsonl.Contains(TEXT("\"boss\":true")));
		}
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
