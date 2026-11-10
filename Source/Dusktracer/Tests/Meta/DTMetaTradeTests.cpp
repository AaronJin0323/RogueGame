// The bank and a run whose boss and hero fall together (meta.md T-MET-46, boss.md R-BOS-13, D-041): what the recorder
// banks is the run the screen and run_end name. In a game world with the game mode, which closes the session as a launch does.
// Run headless: Tools\RunTests.ps1 -Filter Dusktracer.Meta.BossTrade
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
#include "Meta/DTDiscovery.h"
#include "Meta/DTMetaProfile.h"
#include "Meta/DTRunRecorder.h"
#include "Misc/FileHelper.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"
#include "TimerManager.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTMetaTradeTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	enum class EOrder : uint8
	{
		Trade,    // both last hits dealt on one step
		SameStep, // the hero down in the step whose kills take the hull
		HeroOnly  // the hero down, the hull standing
	};

	struct FResult
	{
		bool bRan = false;
		bool bModeVictory = false;
		FDTRunTally Tally;
		FDTRunAward Award;
		FString Jsonl;
	};

	static FResult Play(FAutomationTestBase& Test, EOrder Order)
	{
		FResult Out;
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTMetaTradeWorld")));
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		// A world made this way never begins play for its actors (no game state), so the test does it for the game mode.
		ADTGameMode* Mode = World->SpawnActor<ADTGameMode>();
		if (Mode && !Mode->HasActorBegunPlay())
		{
			Mode->DispatchBeginPlay();
		}
		UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
		UDTBossSubsystem* Boss = World->GetSubsystem<UDTBossSubsystem>();
		UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
		UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
		UDTRunRecorder* Recorder = World->GetSubsystem<UDTRunRecorder>();
		if (Test.TestNotNull(TEXT("game mode"), Mode) && Test.TestNotNull(TEXT("sim"), Sim) && Test.TestNotNull(TEXT("boss"), Boss)
			&& Test.TestNotNull(TEXT("enemies"), Enemies) && Test.TestNotNull(TEXT("telemetry"), Telemetry) && Test.TestNotNull(TEXT("recorder"), Recorder)
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
			if (Order == EOrder::Trade)
			{
				KillHull();
				Sim->StepNow(1);
				HitHero();
				Sim->StepNow(1);
			}
			else
			{
				if (Order == EOrder::SameStep)
				{
					KillHull();
				}
				HitHero();
				Sim->StepNow(1);
			}
			Sim->StepNow(2);
			World->GetTimerManager().Tick(1.0f / static_cast<float>(DTConst::SIM_FIXED_HZ)); // the next tick closes the session, which banks
			Test.TestFalse(TEXT("the run's end closed the session"), Telemetry->IsSessionActive());
			Telemetry->EndSession(TEXT("test_left_open"));

			Out.bRan = true;
			Out.bModeVictory = Mode->IsVictory();
			Out.Tally = Recorder->GetTally();
			Out.Award = Recorder->GetAward();
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

	/** The end run_end names, as the bank reads a close reason (R-MET-05). */
	static EDTRunEnd EndOf(const FString& Jsonl)
	{
		const FString Field = TEXT("\"kind\":\"run_end\",\"owner\":-1,\"reason\":\"");
		const int32 At = Jsonl.Find(Field);
		FString Reason = At != INDEX_NONE ? Jsonl.Mid(At + Field.Len()) : FString();
		int32 Quote = INDEX_NONE;
		if (Reason.FindChar(TEXT('"'), Quote))
		{
			Reason.LeftInline(Quote);
		}
		return UDTRunRecorder::EndFromReason(Reason);
	}

	/** What a save gains from this run: the bank's own two steps on an empty profile (FDTMetaSession::Bank). */
	static FDTMetaProfile Banked(const FResult& R)
	{
		FDTMetaProfile Profile;
		DTMeta::ApplyRun(Profile, R.Tally, EndOf(R.Jsonl));
		DTDiscovery::Apply(R.Tally, Profile);
		return Profile;
	}

	static int64 BossRoleKills(const FDTRunTally& Tally)
	{
		const int64* Found = Tally.Counters.Find(FName(TEXT("Kills.Role.Boss")));
		return Found ? *Found : 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetBossTrade, "Dusktracer.Meta.BossTrade", DTMetaTradeTest::Flags) // T-MET-46
bool FDTMetBossTrade::RunTest(const FString& Parameters)
{
	using namespace DTMetaTradeTest;
	const FName WinId(TEXT("Win"));

	// Both shared-step orders: the run closes as a victory, and everything the boss pays is paid on that victory.
	const TPair<EOrder, const TCHAR*> Wins[] = { { EOrder::Trade, TEXT("trade") }, { EOrder::SameStep, TEXT("same step") } };
	for (const TPair<EOrder, const TCHAR*>& Case : Wins)
	{
		const FResult R = Play(*this, Case.Key);
		if (!R.bRan)
		{
			continue;
		}
		const FString Tag = FString(Case.Value) + TEXT(": ");
		TestTrue(Tag + TEXT("the screen's flag reads a win"), R.bModeVictory);
		TestTrue(Tag + TEXT("run_end closes as a victory"), EndOf(R.Jsonl) == EDTRunEnd::Victory);
		TestTrue(Tag + TEXT("banked"), R.Award.bBanked);
		TestTrue(Tag + TEXT("run_award says victory with the boss"), R.Jsonl.Contains(TEXT("\"kind\":\"run_award\",\"owner\":-1,\"end\":\"victory\""))
			&& R.Jsonl.Contains(TEXT("\"boss\":true")));
		TestTrue(Tag + TEXT("the tally holds the boss kill"), R.Tally.bBossKilled);
		TestEqual(Tag + TEXT("Kills.Role.Boss"), BossRoleKills(R.Tally), 1LL);
		FDTRunTally NoBoss = R.Tally;
		NoBoss.bBossKilled = false;
		TestEqual(Tag + TEXT("the award holds the boss bonus"), R.Award.Retained - DTMeta::ComputeAward(NoBoss), static_cast<int64>(DTConst::META_CURRENCY_BOSS_KILL));
		const FDTMetaProfile Profile = Banked(R);
		TestEqual(Tag + TEXT("one win on the save"), Profile.Wins, 1);
		TestEqual(Tag + TEXT("no death on the save"), Profile.DeathsSinceWin, 0);
		TestTrue(Tag + TEXT("the Win discovery"), Profile.Discoveries.Contains(WinId));
	}

	// A plain death with the hull standing pays none of them.
	const FResult Death = Play(*this, EOrder::HeroOnly);
	if (Death.bRan)
	{
		TestFalse(TEXT("death: the screen's flag reads no win"), Death.bModeVictory);
		TestTrue(TEXT("death: run_end closes as a death"), EndOf(Death.Jsonl) == EDTRunEnd::Death);
		TestTrue(TEXT("death: banked"), Death.Award.bBanked);
		TestTrue(TEXT("death: run_award says death without the boss"), Death.Jsonl.Contains(TEXT("\"kind\":\"run_award\",\"owner\":-1,\"end\":\"death\""))
			&& Death.Jsonl.Contains(TEXT("\"boss\":false")));
		TestFalse(TEXT("death: no boss kill in the tally"), Death.Tally.bBossKilled);
		TestEqual(TEXT("death: no Kills.Role.Boss"), BossRoleKills(Death.Tally), 0LL);
		FDTRunTally WithBoss = Death.Tally;
		WithBoss.bBossKilled = true;
		TestEqual(TEXT("death: the award holds no boss bonus"), DTMeta::ComputeAward(WithBoss) - Death.Award.Retained, static_cast<int64>(DTConst::META_CURRENCY_BOSS_KILL));
		const FDTMetaProfile Profile = Banked(Death);
		TestEqual(TEXT("death: no win on the save"), Profile.Wins, 0);
		TestEqual(TEXT("death: one death on the save"), Profile.DeathsSinceWin, 1);
		TestFalse(TEXT("death: no Win discovery"), Profile.Discoveries.Contains(WinId));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
