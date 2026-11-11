// Two marks a session's logs carry for the readings of D-045 and D-054 (telemetry.md R-TEL-10, R-TEL-08; T-TEL-14, T-TEL-15):
// hero_damaged.arrival tells a drop-in's hit from any other, and run_start.bot_fast tells a bot's fast run from a person's.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Telemetry.Mark; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Bot/DTBotSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTEnemyTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTTelMarkTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr int32 DTTelMarkFastSteps = 600; // what Tools/BotRuns.ps1 passes as -DTBotFast, not a tuned number

	static TArray<FString> OfKind(const FString& Jsonl, const TCHAR* Kind)
	{
		TArray<FString> Lines;
		Jsonl.ParseIntoArrayLines(Lines);
		const FString Needle = FString::Printf(TEXT("\"kind\":\"%s\""), Kind);
		return Lines.FilterByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle); });
	}

	/** A game world with a run and a session; the hero stands where the sim put it. */
	struct FScene
	{
		UWorld* World = nullptr;
		UDTSimSubsystem* Sim = nullptr;
		UDTTelemetrySubsystem* Telemetry = nullptr;
		UDTEnemySubsystem* Enemies = nullptr;
		UDTHeroSubsystem* Heroes = nullptr;
		UDTBotSubsystem* Bot = nullptr;

		explicit FScene(const TCHAR* Name)
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), Name));
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
			Sim = World->GetSubsystem<UDTSimSubsystem>();
			Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
			Enemies = World->GetSubsystem<UDTEnemySubsystem>();
			Heroes = World->GetSubsystem<UDTHeroSubsystem>();
			Bot = World->GetSubsystem<UDTBotSubsystem>();
		}

		bool Valid() const { return Sim && Telemetry && Enemies && Heroes && Bot; }

		FString End()
		{
			FString Jsonl;
			Telemetry->EndSession(TEXT("test"));
			const FString CsvPath = Telemetry->GetWriter().GetCsvPath();
			const FString JsonlPath = Telemetry->GetWriter().GetJsonlPath();
			FFileHelper::LoadFileToString(Jsonl, *JsonlPath);
			IFileManager::Get().Delete(*CsvPath, false, true, true);
			IFileManager::Get().Delete(*JsonlPath, false, true, true);
			return Jsonl;
		}

		void Close()
		{
			for (AActor* Actor : FActorRange(World))
			{
				if (Actor)
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}
	};

	/** The first hero_damaged of a session in which one enemy of Role (an arrival or not) stands on the hero. */
	static FString FirstHitBy(EDTEnemyRole Role, bool bArrival, const TCHAR* Name, int32 Seed)
	{
		FScene Scene(Name);
		FString Jsonl;
		if (Scene.Valid())
		{
			Scene.Sim->StartRun(Seed, 1);
			Scene.Telemetry->StartSession(Seed, 1);
			Scene.Sim->StepNow(1);
			FDTEnemySpawn Touch;
			Touch.Role = Role;
			Touch.bArrival = bArrival;
			Touch.Position = Scene.Sim->GetView().HeroPositions[0];
			Scene.Enemies->QueueSpawn(Touch);
			for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ && Scene.Heroes->GetState(0)->HitsTaken == 0; ++Step)
			{
				Scene.Sim->StepNow(1);
			}
			Jsonl = Scene.End();
		}
		Scene.Close();
		const TArray<FString> Damaged = OfKind(Jsonl, TEXT("hero_damaged"));
		return Damaged.Num() > 0 ? Damaged[0] : FString();
	}

	/** run_start of a session started with the bot as given (Activate: on or off; Steps: its steps per frame). */
	static FString RunStartWith(bool bActivate, int32 Steps, const TCHAR* Name, int32 Seed)
	{
		FScene Scene(Name);
		FString Jsonl;
		if (Scene.Valid())
		{
			Scene.Sim->StartRun(Seed, 1);
			if (bActivate)
			{
				FDTBotConfig Config;
				Config.Policy = EDTBotPolicy::Random;
				Config.Seed = Seed;
				Scene.Bot->Activate(Config);
			}
			Scene.Bot->SetStepsPerFrame(Steps);
			Scene.Telemetry->StartSession(Seed, 1);
			Scene.Bot->Deactivate();
			Jsonl = Scene.End();
		}
		Scene.Close();
		const TArray<FString> Start = OfKind(Jsonl, TEXT("run_start"));
		return Start.Num() > 0 ? Start[0] : FString();
	}
}

// T-TEL-14: hero_damaged.arrival is true for a hit by a director drop-in (DTEnemyFlags::Arrival, by contact or by the band of
// one killed in its wind-up) and false for any other enemy's; the field is on every hero_damaged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelArrivalMark, "Dusktracer.Telemetry.Mark.Arrival", DTTelMarkTest::Flags) // T-TEL-14
bool FDTTelArrivalMark::RunTest(const FString& Parameters)
{
	using namespace DTTelMarkTest;
	const FString Arrival = FirstHitBy(EDTEnemyRole::Rusher, true, TEXT("DTTelMarkArrival"), 61);
	const FString Walker = FirstHitBy(EDTEnemyRole::Rusher, false, TEXT("DTTelMarkWalker"), 62);
	TestTrue(TEXT("an arrival's contact is marked"), Arrival.Contains(TEXT("\"arrival\":true")));
	TestTrue(TEXT("its kind stays the contact's"), Arrival.Contains(TEXT("\"hit_kind\":\"Contact\"")));
	TestTrue(TEXT("a Rusher that walked in is not"), Walker.Contains(TEXT("\"arrival\":false")));

	// The band of an arrival killed in its wind-up resolves as a hazard carrying the mark (enemies.md R-ENM-13).
	FDTRunState Run;
	Run.Start(63, 1);
	FDTWorldView View;
	View.HeroPositions = { FVector2f(500.0f, 0.0f) };
	View.HeroIntangible.Init(false, 1);
	FDTSimContext Context;
	Context.StepSeconds = 1.0f / static_cast<float>(DTConst::SIM_FIXED_HZ);
	Context.Run = &Run;
	Context.View = &View;
	FDTEnemyPool Pool;
	FDTEnemySpawn Spec;
	Spec.Role = EDTEnemyRole::Rusher;
	Spec.bArrival = true;
	Pool.Spawn(Spec, nullptr, true);
	int32 Hazards = 0;
	int32 Marked = 0;
	for (int32 Step = 0; Step < 2 * DTConst::SIM_FIXED_HZ; ++Step)
	{
		if (Step == 1 && Pool.Num() > 0)
		{
			FDTHit Kill;
			Kill.EnemyIndex = 0;
			Kill.EnemySerial = Pool.GetSerials()[0];
			Kill.OwnerId = 0;
			Kill.Damage = 1.0e6f;
			Kill.Position = Pool.GetPositions()[0];
			View.Hits.Add(Kill);
		}
		Context.StepIndex = Step;
		Pool.StepEnemies(Context);
		Pool.ResolveHits(Context);
		for (const FDTHeroHit& Hit : View.HeroHits)
		{
			Hazards += Hit.Kind == EDTHeroHitKind::Charge ? 1 : 0;
			Marked += Hit.Kind == EDTHeroHitKind::Charge && Hit.bArrival ? 1 : 0;
		}
	}
	TestEqual(TEXT("the killed arrival's band lands once"), Hazards, 1);
	TestEqual(TEXT("and carries the mark"), Marked, 1);
	return true;
}

// T-TEL-15: run_start.bot_fast is the steps per frame of an active bot's fast run, and 0 for a run with no active bot.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelBotFastMark, "Dusktracer.Telemetry.Mark.BotFast", DTTelMarkTest::Flags) // T-TEL-15
bool FDTTelBotFastMark::RunTest(const FString& Parameters)
{
	using namespace DTTelMarkTest;
	const FString Fast = RunStartWith(true, DTTelMarkFastSteps, TEXT("DTTelMarkFast"), 71);
	const FString RealTime = RunStartWith(true, 0, TEXT("DTTelMarkRealTime"), 72);
	const FString Person = RunStartWith(false, DTTelMarkFastSteps, TEXT("DTTelMarkPerson"), 73);
	TestTrue(TEXT("a bot's fast run names its steps per frame"), Fast.Contains(FString::Printf(TEXT("\"bot_fast\":%d,"), DTTelMarkFastSteps)));
	TestTrue(TEXT("a bot at real speed reads 0"), RealTime.Contains(TEXT("\"bot_fast\":0,")));
	TestTrue(TEXT("a run with no active bot reads 0"), Person.Contains(TEXT("\"bot_fast\":0,")));

	// The plain core writes what its config says, and a person's default.
	FDTTelemetryCore Core;
	FDTTelemetryConfig Config;
	Core.Start(Config);
	FString Csv;
	FString Jsonl;
	Core.TakeOutput(Csv, Jsonl);
	TestTrue(TEXT("the default config is a person's run"), Jsonl.Contains(TEXT("\"bot_fast\":0,")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
