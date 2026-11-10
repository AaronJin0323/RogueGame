// One outcome for a run whose boss and hero fall together (boss.md R-BOS-13, T-BOS-13, D-041), in a real world with the
// game mode, so the sim, the game mode the screen reads, boss_end, dir_block and run_end are all read from one run.
// Run headless: Tools\RunTests.ps1 -Filter Dusktracer.Boss.RunEnd
#include "Misc/AutomationTest.h"
#include "Boss/DTBossSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DTGameMode.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTRunEndTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	/** Which of the two last hit points goes first. A hit on the hero is applied the step after it is dealt (R-CMB-08). */
	enum class EOrder : uint8
	{
		Trade,       // both last hits are dealt on one step: the hull dies in its ResolveHits, the hero's hit waits for the next
		SameStep,    // the hero goes down in the Hero phase of the step whose ResolveHits kills the hull
		HeroOnly,    // the hero goes down, the hull stands
		HeroFirst    // the hero goes down; the hull dies a step later, to fire still in the air
	};

	/** What one run read, everywhere a run's outcome is read. */
	struct FResult
	{
		bool bRan = false;
		bool bModeOver = false;
		bool bModeVictory = false;
		bool bAllDown = false;
		float HeroHp = 0.0f;
		FString BossOutcome;
		FString Jsonl;
	};

	static int32 Count(const FString& Text, const TCHAR* Needle)
	{
		int32 Found = 0;
		for (int32 At = Text.Find(Needle); At != INDEX_NONE; At = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
		{
			++Found;
		}
		return Found;
	}

	static FResult Play(FAutomationTestBase& Test, EOrder Order)
	{
		FResult Out;
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTRunEndTestWorld")));
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		// The game mode as a launch has it: it starts the run and its telemetry session and closes the session by the run's end.
		// A world made this way never begins play for its actors (no game state), so the test does it for this one.
		ADTGameMode* Mode = World->SpawnActor<ADTGameMode>();
		if (Mode && !Mode->HasActorBegunPlay())
		{
			Mode->DispatchBeginPlay();
		}
		UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
		UDTBossSubsystem* Boss = World->GetSubsystem<UDTBossSubsystem>();
		UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
		UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
		UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
		if (Test.TestNotNull(TEXT("game mode"), Mode) && Test.TestNotNull(TEXT("sim"), Sim) && Test.TestNotNull(TEXT("boss"), Boss)
			&& Test.TestNotNull(TEXT("enemies"), Enemies) && Test.TestNotNull(TEXT("heroes"), Heroes) && Test.TestNotNull(TEXT("telemetry"), Telemetry)
			&& Test.TestTrue(TEXT("the game mode opened a session"), Telemetry->IsSessionActive()))
		{
			Boss->StartFight();
			Sim->StepNow(3);
			Test.TestTrue(TEXT("the hull stands"), Boss->GetCore().HasHull());

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
			switch (Order)
			{
			case EOrder::Trade:
				KillHull();
				Sim->StepNow(1);
				HitHero();
				Sim->StepNow(1);
				break;
			case EOrder::SameStep:
				KillHull();
				HitHero();
				Sim->StepNow(1);
				break;
			case EOrder::HeroOnly:
				HitHero();
				Sim->StepNow(1);
				break;
			case EOrder::HeroFirst:
				HitHero();
				Sim->StepNow(1);
				KillHull();
				Sim->StepNow(1);
				break;
			}
			Sim->StepNow(2); // the sim goes on until the session closes: nothing after the end changes it
			World->GetTimerManager().Tick(1.0f / static_cast<float>(DTConst::SIM_FIXED_HZ)); // the next tick closes the session
			Test.TestFalse(TEXT("the run's end closed the session"), Telemetry->IsSessionActive());
			Telemetry->EndSession(TEXT("test_left_open"));

			Out.bRan = true;
			Out.bModeOver = Mode->IsRunOver();
			Out.bModeVictory = Mode->IsVictory();
			Out.bAllDown = Heroes->IsRunOver();
			Out.HeroHp = Heroes->GetState(0) ? Heroes->GetState(0)->Hp : 0.0f;
			Out.BossOutcome = Boss->GetCore().GetOutcome();
			const FString CsvPath = Telemetry->GetWriter().GetCsvPath();
			const FString JsonlPath = Telemetry->GetWriter().GetJsonlPath();
			FFileHelper::LoadFileToString(Out.Jsonl, *JsonlPath);
			IFileManager::Get().Delete(*CsvPath, false, true, true);
			IFileManager::Get().Delete(*JsonlPath, false, true, true);
		}
		for (AActor* Actor : FActorRange(World))
		{
			if (Actor)
			{
				Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
			}
		}
		World->DestroyWorld(true);
		GEngine->DestroyWorldContext(World);
		return Out;
	}

	/** The run reads as a win in the sim, on the game mode the HUD's banners read, and in every record that names an end. */
	static void ExpectWin(FAutomationTestBase& Test, const FResult& R, const TCHAR* What)
	{
		const FString Tag = FString(What) + TEXT(": ");
		Test.TestTrue(Tag + TEXT("the run is over"), R.bModeOver);
		Test.TestTrue(Tag + TEXT("the game mode reads a win (Hud.Victory, never Hud.RunOver)"), R.bModeVictory);
		Test.TestEqual(Tag + TEXT("the fight's outcome"), R.BossOutcome, FString(TEXT("win")));
		Test.TestEqual(Tag + TEXT("one run_end"), Count(R.Jsonl, TEXT("\"kind\":\"run_end\"")), 1);
		Test.TestTrue(Tag + TEXT("run_end says victory"), R.Jsonl.Contains(TEXT("\"kind\":\"run_end\",\"owner\":-1,\"reason\":\"victory\"")));
		Test.TestEqual(Tag + TEXT("one boss_end"), Count(R.Jsonl, TEXT("\"kind\":\"boss_end\"")), 1);
		Test.TestTrue(Tag + TEXT("boss_end says win"), R.Jsonl.Contains(TEXT("\"kind\":\"boss_end\",\"owner\":-1,\"outcome\":\"win\"")));
		Test.TestEqual(Tag + TEXT("no record says death"), Count(R.Jsonl, TEXT("\"death\"")), 0);
		Test.TestTrue(Tag + TEXT("the open block ends by the win"), R.Jsonl.Contains(TEXT("\"ended_by\":\"victory\"")));
	}

	static void ExpectDeath(FAutomationTestBase& Test, const FResult& R, const TCHAR* What)
	{
		const FString Tag = FString(What) + TEXT(": ");
		Test.TestTrue(Tag + TEXT("the run is over"), R.bModeOver);
		Test.TestFalse(Tag + TEXT("the game mode reads no win"), R.bModeVictory);
		Test.TestTrue(Tag + TEXT("every hero is down"), R.bAllDown);
		Test.TestEqual(Tag + TEXT("the fight's outcome"), R.BossOutcome, FString(TEXT("death")));
		Test.TestTrue(Tag + TEXT("run_end says death"), R.Jsonl.Contains(TEXT("\"kind\":\"run_end\",\"owner\":-1,\"reason\":\"death\"")));
		Test.TestEqual(Tag + TEXT("one boss_end"), Count(R.Jsonl, TEXT("\"kind\":\"boss_end\"")), 1);
		Test.TestTrue(Tag + TEXT("boss_end says death"), R.Jsonl.Contains(TEXT("\"kind\":\"boss_end\",\"owner\":-1,\"outcome\":\"death\"")));
		Test.TestEqual(Tag + TEXT("one hero_down"), Count(R.Jsonl, TEXT("\"kind\":\"hero_down\"")), 1);
		Test.TestTrue(Tag + TEXT("the open block ends by the death"), R.Jsonl.Contains(TEXT("\"ended_by\":\"death\"")));
		Test.TestEqual(Tag + TEXT("no record says victory or win"), Count(R.Jsonl, TEXT("\"victory\"")) + Count(R.Jsonl, TEXT("\"win\"")), 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossRunEnd, "Dusktracer.Boss.RunEnd", DTRunEndTest::Flags) // T-BOS-13
bool FDTBossRunEnd::RunTest(const FString& Parameters)
{
	using namespace DTRunEndTest;

	// Both last hits dealt on one step: the Warden's death is read before the hero's hit is applied, and the hit is dropped.
	const FResult Trade = Play(*this, EOrder::Trade);
	if (Trade.bRan)
	{
		ExpectWin(*this, Trade, TEXT("trade"));
		TestFalse(TEXT("trade: the hero stands"), Trade.bAllDown);
		TestTrue(TEXT("trade: the hit that came with the kill took no HP"), Trade.HeroHp >= static_cast<float>(DTConst::HERO_MAX_HP));
		TestEqual(TEXT("trade: no hero_down"), Count(Trade.Jsonl, TEXT("\"kind\":\"hero_down\"")), 0);
	}

	// The hero falls in the step whose kills take the Warden: the step's kills are read before the death ends the run.
	const FResult Same = Play(*this, EOrder::SameStep);
	if (Same.bRan)
	{
		ExpectWin(*this, Same, TEXT("same step"));
		TestTrue(TEXT("same step: the hero did fall"), Same.bAllDown);
		TestEqual(TEXT("same step: one hero_down"), Count(Same.Jsonl, TEXT("\"kind\":\"hero_down\"")), 1);
	}

	// The controls: a death with the hull standing, and a hull that falls a step after the run ended, are deaths.
	const FResult Death = Play(*this, EOrder::HeroOnly);
	if (Death.bRan)
	{
		ExpectDeath(*this, Death, TEXT("hull standing"));
	}
	const FResult Late = Play(*this, EOrder::HeroFirst);
	if (Late.bRan)
	{
		ExpectDeath(*this, Late, TEXT("hull falls a step late"));
	}
	return true;
}

namespace DTRunEndTwoTest
{
	/** Two heroes. The game mode starts one (ADTGameMode::BeginPlay), so these runs are started by the test and read where the game mode reads: the two delegates it binds. */
	enum class EScene : uint8
	{
		SameStepLast,  // one hero down earlier; the other falls in the step whose kills take the hull
		SameStepBoth,  // both fall in the step whose kills take the hull
		SameStepOne,   // one falls in that step, the other stands
		Trade,         // the hull's last hit and both heroes' last hits are dealt on one step
		DeathInTurn,   // one down, then the other, the hull standing
		HeroesFirst    // both down; the hull dies a step later
	};

	struct FResult
	{
		bool bRan = false;
		bool bVictory = false;
		int32 AllDownCalls = 0;
		bool bOverAfterFirst = false;
		bool bFightOnAfterFirst = false;
		bool bDown[2] = { false, false };
		float Hp[2] = { 0.0f, 0.0f };
		FString BossOutcome;
		FString Jsonl;
	};

	static FResult Play(FAutomationTestBase& Test, EScene Scene)
	{
		FResult Out;
		constexpr int32 Seed = 4242;
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTRunEndTwoWorld")));
		Context.SetCurrentWorld(World);
		UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
		if (Sim)
		{
			Sim->StartRun(Seed, 2); // before play begins: the hero system takes its count there
		}
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		UDTBossSubsystem* Boss = World->GetSubsystem<UDTBossSubsystem>();
		UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
		UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
		UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
		if (Test.TestNotNull(TEXT("sim"), Sim) && Test.TestNotNull(TEXT("boss"), Boss) && Test.TestNotNull(TEXT("enemies"), Enemies) && Test.TestNotNull(TEXT("heroes"), Heroes)
			&& Test.TestNotNull(TEXT("telemetry"), Telemetry) && Test.TestEqual(TEXT("two heroes"), Heroes->GetNumHeroes(), 2))
		{
			Telemetry->StartSession(Seed, 2);
			// What ADTGameMode binds: a death closes the session on the next tick, a win is the boss system's own close.
			Heroes->OnAllHeroesDown.AddLambda([&Out, World, Telemetry](int64)
			{
				++Out.AllDownCalls;
				World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(Telemetry, [Telemetry]() { Telemetry->EndSession(TEXT("death")); }));
			});
			Boss->OnBossDefeated.AddLambda([&Out](int64) { Out.bVictory = true; });
			Boss->StartFight();
			Sim->StepNow(3);
			Test.TestTrue(TEXT("the hull stands"), Boss->GetCore().HasHull());

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
			const auto HitHero = [Sim](int32 Player)
			{
				FDTHeroHit Hit;
				Hit.PlayerId = Player;
				Hit.Damage = static_cast<float>(DTConst::HERO_MAX_HP) * 10.0f;
				Sim->GetView().HeroHits.Add(Hit);
			};
			const auto FirstFalls = [&Out, Sim, Heroes, Boss, &HitHero]()
			{
				HitHero(0);
				Sim->StepNow(2);
				Out.bOverAfterFirst = Heroes->IsRunOver() || Out.AllDownCalls > 0 || Out.bVictory;
				Out.bFightOnAfterFirst = Heroes->IsDown(0) && !Heroes->IsDown(1) && Boss->GetCore().HasHull() && Boss->GetCore().GetOutcome().IsEmpty();
			};
			switch (Scene)
			{
			case EScene::SameStepLast:
				FirstFalls();
				KillHull();
				HitHero(1);
				Sim->StepNow(1);
				break;
			case EScene::SameStepBoth:
				KillHull();
				HitHero(0);
				HitHero(1);
				Sim->StepNow(1);
				break;
			case EScene::SameStepOne:
				KillHull();
				HitHero(1);
				Sim->StepNow(1);
				break;
			case EScene::Trade:
				KillHull();
				Sim->StepNow(1);
				HitHero(0);
				HitHero(1);
				Sim->StepNow(1);
				break;
			case EScene::DeathInTurn:
				FirstFalls();
				HitHero(1);
				Sim->StepNow(1);
				break;
			case EScene::HeroesFirst:
				HitHero(0);
				HitHero(1);
				Sim->StepNow(1);
				KillHull();
				Sim->StepNow(1);
				break;
			}
			Sim->StepNow(2);
			World->GetTimerManager().Tick(1.0f / static_cast<float>(DTConst::SIM_FIXED_HZ));
			Test.TestFalse(TEXT("the run's end closed the session"), Telemetry->IsSessionActive());
			Telemetry->EndSession(TEXT("test_left_open"));

			Out.bRan = true;
			for (int32 Player = 0; Player < 2; ++Player)
			{
				Out.bDown[Player] = Heroes->IsDown(Player);
				Out.Hp[Player] = Heroes->GetState(Player) ? Heroes->GetState(Player)->Hp : 0.0f;
			}
			Out.BossOutcome = Boss->GetCore().GetOutcome();
			const FString CsvPath = Telemetry->GetWriter().GetCsvPath();
			const FString JsonlPath = Telemetry->GetWriter().GetJsonlPath();
			FFileHelper::LoadFileToString(Out.Jsonl, *JsonlPath);
			IFileManager::Get().Delete(*CsvPath, false, true, true);
			IFileManager::Get().Delete(*JsonlPath, false, true, true);
		}
		for (AActor* Actor : FActorRange(World))
		{
			if (Actor)
			{
				Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
			}
		}
		World->DestroyWorld(true);
		GEngine->DestroyWorldContext(World);
		return Out;
	}

	static void ExpectWin(FAutomationTestBase& Test, const FResult& R, const TCHAR* What, int32 HeroesDown)
	{
		using DTRunEndTest::Count;
		const FString Tag = FString(What) + TEXT(": ");
		Test.TestTrue(Tag + TEXT("the win is announced"), R.bVictory);
		Test.TestEqual(Tag + TEXT("no death is announced"), R.AllDownCalls, 0);
		Test.TestEqual(Tag + TEXT("the fight's outcome"), R.BossOutcome, FString(TEXT("win")));
		Test.TestEqual(Tag + TEXT("one run_end"), Count(R.Jsonl, TEXT("\"kind\":\"run_end\"")), 1);
		Test.TestTrue(Tag + TEXT("run_end says victory"), R.Jsonl.Contains(TEXT("\"kind\":\"run_end\",\"owner\":-1,\"reason\":\"victory\"")));
		Test.TestEqual(Tag + TEXT("one boss_end"), Count(R.Jsonl, TEXT("\"kind\":\"boss_end\"")), 1);
		Test.TestTrue(Tag + TEXT("boss_end says win"), R.Jsonl.Contains(TEXT("\"kind\":\"boss_end\",\"owner\":-1,\"outcome\":\"win\"")));
		Test.TestEqual(Tag + TEXT("no record says death"), Count(R.Jsonl, TEXT("\"death\"")), 0);
		Test.TestEqual(Tag + TEXT("heroes down"), (R.bDown[0] ? 1 : 0) + (R.bDown[1] ? 1 : 0), HeroesDown);
		Test.TestEqual(Tag + TEXT("hero_down records"), Count(R.Jsonl, TEXT("\"kind\":\"hero_down\"")), HeroesDown);
	}

	static void ExpectDeath(FAutomationTestBase& Test, const FResult& R, const TCHAR* What)
	{
		using DTRunEndTest::Count;
		const FString Tag = FString(What) + TEXT(": ");
		Test.TestFalse(Tag + TEXT("no win is announced"), R.bVictory);
		Test.TestEqual(Tag + TEXT("the death is announced once"), R.AllDownCalls, 1);
		Test.TestTrue(Tag + TEXT("both heroes are down"), R.bDown[0] && R.bDown[1]);
		Test.TestEqual(Tag + TEXT("the fight's outcome"), R.BossOutcome, FString(TEXT("death")));
		Test.TestTrue(Tag + TEXT("run_end says death"), R.Jsonl.Contains(TEXT("\"kind\":\"run_end\",\"owner\":-1,\"reason\":\"death\"")));
		Test.TestEqual(Tag + TEXT("one boss_end"), Count(R.Jsonl, TEXT("\"kind\":\"boss_end\"")), 1);
		Test.TestTrue(Tag + TEXT("boss_end says death"), R.Jsonl.Contains(TEXT("\"kind\":\"boss_end\",\"owner\":-1,\"outcome\":\"death\"")));
		Test.TestEqual(Tag + TEXT("two hero_down"), Count(R.Jsonl, TEXT("\"kind\":\"hero_down\"")), 2);
		Test.TestEqual(Tag + TEXT("no record says victory or win"), Count(R.Jsonl, TEXT("\"victory\"")) + Count(R.Jsonl, TEXT("\"win\"")), 0);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBossRunEndTwoHeroes, "Dusktracer.Boss.RunEndTwoHeroes", DTRunEndTest::Flags) // T-BOS-15
bool FDTBossRunEndTwoHeroes::RunTest(const FString& Parameters)
{
	using namespace DTRunEndTwoTest;

	// "Every hero down" with two (R-BOS-13): the last hero standing falls in the step whose kills take the Warden.
	const FResult Last = Play(*this, EScene::SameStepLast);
	if (Last.bRan)
	{
		TestFalse(TEXT("last hero: one hero down ends nothing"), Last.bOverAfterFirst);
		TestTrue(TEXT("last hero: the fight goes on with one standing"), Last.bFightOnAfterFirst);
		ExpectWin(*this, Last, TEXT("last hero, same step"), 2);
	}
	const FResult Both = Play(*this, EScene::SameStepBoth);
	if (Both.bRan)
	{
		ExpectWin(*this, Both, TEXT("both heroes, same step"), 2);
	}
	const FResult One = Play(*this, EScene::SameStepOne);
	if (One.bRan)
	{
		ExpectWin(*this, One, TEXT("one hero, same step"), 1);
		TestTrue(TEXT("one hero, same step: the other stands"), !One.bDown[0] && One.bDown[1]);
	}
	// The hull's last hit and both heroes' last hits dealt on one step: a won run takes no hit, for either hero.
	const FResult Trade = Play(*this, EScene::Trade);
	if (Trade.bRan)
	{
		ExpectWin(*this, Trade, TEXT("trade"), 0);
		TestTrue(TEXT("trade: neither hit took HP"), Trade.Hp[0] >= static_cast<float>(DTConst::HERO_MAX_HP) && Trade.Hp[1] >= static_cast<float>(DTConst::HERO_MAX_HP));
	}
	// The controls: both down in turn with the hull standing, and a hull that falls a step after the second hero.
	const FResult Turn = Play(*this, EScene::DeathInTurn);
	if (Turn.bRan)
	{
		TestFalse(TEXT("in turn: one hero down ends nothing"), Turn.bOverAfterFirst);
		TestTrue(TEXT("in turn: the fight goes on with one standing"), Turn.bFightOnAfterFirst);
		ExpectDeath(*this, Turn, TEXT("in turn, hull standing"));
	}
	const FResult Late = Play(*this, EScene::HeroesFirst);
	if (Late.bRan)
	{
		ExpectDeath(*this, Late, TEXT("hull falls a step late"));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
