// Hero producers reach telemetry in a real world (combat_core.md §Telemetry; telemetry.md CSV damage_taken_per_min,
// dashes_per_min, input_device): the telemetry subsystem binds UDTHeroSubsystem::OnHeroEvent at begin play.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Telemetry.Hero; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Combat/DTWeaponCatalog.h"
#include "Combat/DTWeaponFlags.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Enemies/DTEnemyBulletPool.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTEnemyTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTTelHeroTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static int32 CountKind(const FString& Jsonl, const TCHAR* Kind)
	{
		const FString Needle = FString::Printf(TEXT("\"kind\":\"%s\""), Kind);
		int32 Count = 0;
		for (int32 At = Jsonl.Find(Needle); At != INDEX_NONE; At = Jsonl.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
		{
			++Count;
		}
		return Count;
	}

	/** The value of Column in the first data row of the session CSV ("" when missing). */
	static FString CsvValue(const FString& Csv, const TCHAR* Column)
	{
		TArray<FString> Lines;
		Csv.ParseIntoArrayLines(Lines);
		if (Lines.Num() < 2)
		{
			return FString();
		}
		TArray<FString> Header;
		TArray<FString> Row;
		Lines[0].ParseIntoArray(Header, TEXT(","), false);
		Lines[1].ParseIntoArray(Row, TEXT(","), false);
		const int32 Index = Header.IndexOfByKey(FString(Column));
		return Row.IsValidIndex(Index) ? Row[Index] : FString();
	}

	/** Lines of one kind. */
	static TArray<FString> OfKind(const FString& Jsonl, const TCHAR* Kind)
	{
		TArray<FString> Lines;
		Jsonl.ParseIntoArrayLines(Lines);
		const FString Needle = FString::Printf(TEXT("\"kind\":\"%s\""), Kind);
		return Lines.FilterByPredicate([&Needle](const FString& Line) { return Line.Contains(Needle); });
	}

	/** The number after "Field": in Line (0 when missing). */
	static double Number(const FString& Line, const TCHAR* Field)
	{
		const FString Needle = FString::Printf(TEXT("\"%s\":"), Field);
		const int32 At = Line.Find(Needle);
		return At != INDEX_NONE ? FCString::Atod(*Line + At + Needle.Len()) : 0.0;
	}

	/** One frame of this length is a hitch: thirty steps come due at once (Dusktracer.Telemetry.RunEnd). */
	constexpr float DTTelRunEndHitchSeconds = 0.5f;

	/** A game world for one test; Close tears it down. */
	struct FWorld
	{
		UWorld* World = nullptr;

		explicit FWorld(const TCHAR* Name)
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), Name));
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
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

	/** Ends the session and moves its JSONL into OutJsonl (the files are removed). */
	static void TakeSession(UDTTelemetrySubsystem& Telemetry, const TCHAR* Reason, FString& OutJsonl)
	{
		Telemetry.EndSession(Reason);
		const FString CsvPath = Telemetry.GetWriter().GetCsvPath();
		const FString JsonlPath = Telemetry.GetWriter().GetJsonlPath();
		FFileHelper::LoadFileToString(OutJsonl, *JsonlPath);
		IFileManager::Get().Delete(*CsvPath, false, true, true);
		IFileManager::Get().Delete(*JsonlPath, false, true, true);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelHeroRecords, "Dusktracer.Telemetry.HeroRecords", DTTelHeroTest::Flags) // combat_core.md §Telemetry
bool FDTTelHeroRecords::RunTest(const FString& Parameters)
{
	using namespace DTTelHeroTest;
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTTelHeroWorld")));
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();

	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
	UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
	UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
	FString Csv;
	FString Jsonl;
	if (TestNotNull(TEXT("sim"), Sim) && TestNotNull(TEXT("telemetry"), Telemetry) && TestNotNull(TEXT("heroes"), Heroes)
		&& TestNotNull(TEXT("enemies"), Enemies))
	{
		Sim->StartRun(31, 1);
		Telemetry->StartSession(31, 1);
		Sim->GetRun().GetPlayer(0).Keepers.Primary = EDTPath::Storm;
		Sim->StepNow(1);

		// A dash, and an Intercession press with an empty meter (denied).
		FDTHeroInput Dash;
		Dash.Move = FVector2f(1.0f, 0.0f);
		Dash.bDashPress = true;
		Heroes->LatchInput(0, Dash);
		Sim->StepNow(1);
		FDTHeroInput Press;
		Press.bIntercessionPress = true;
		Heroes->LatchInput(0, Press);
		Sim->StepNow(DTConst::SIM_FIXED_HZ);

		// Contact damage, then a lethal contact: hero_damaged and hero_down.
		FDTEnemySpawn Touch;
		Touch.Position = Sim->GetView().HeroPositions[0];
		Enemies->QueueSpawn(Touch);
		Sim->StepNow(DTConst::SIM_FIXED_HZ / 2);
		FDTEnemySpawn Lethal = Touch;
		Lethal.Position = Sim->GetView().HeroPositions[0];
		Lethal.DmgScale = static_cast<float>(DTConst::HERO_MAX_HP) * 10.0f;
		Enemies->QueueSpawn(Lethal);
		Sim->StepNow(DTConst::SIM_FIXED_HZ);
		TestTrue(TEXT("the hero went down"), Heroes->IsDown(0));

		// Close the first interval so the CSV row carries the per-minute columns.
		const int32 IntervalSteps = FMath::RoundToInt32(DTConst::TEL_INTERVAL_S * DTConst::SIM_FIXED_HZ);
		Sim->StepNow(FMath::Max(0, IntervalSteps - static_cast<int32>(Sim->GetStepIndex()) + 1));
		Telemetry->EndSession(TEXT("test"));
		const FString CsvPath = Telemetry->GetWriter().GetCsvPath();
		const FString JsonlPath = Telemetry->GetWriter().GetJsonlPath();
		FFileHelper::LoadFileToString(Csv, *CsvPath);
		FFileHelper::LoadFileToString(Jsonl, *JsonlPath);
		IFileManager::Get().Delete(*CsvPath, false, true, true);
		IFileManager::Get().Delete(*JsonlPath, false, true, true);
	}

	TestEqual(TEXT("one dash record"), CountKind(Jsonl, TEXT("dash")), 1);
	TestTrue(TEXT("hero_damaged records"), CountKind(Jsonl, TEXT("hero_damaged")) >= 2);
	TestEqual(TEXT("one hero_down record"), CountKind(Jsonl, TEXT("hero_down")), 1);
	TestEqual(TEXT("one intercession_denied record"), CountKind(Jsonl, TEXT("intercession_denied")), 1);
	TestTrue(TEXT("CSV damage_taken_per_min is not zero"), FCString::Atod(*CsvValue(Csv, TEXT("damage_taken_per_min"))) > 0.0);
	TestTrue(TEXT("CSV dashes_per_min is not zero"), FCString::Atod(*CsvValue(Csv, TEXT("dashes_per_min"))) > 0.0);
	TestEqual(TEXT("CSV input_device is the hero's device"), CsvValue(Csv, TEXT("input_device")), FString(TEXT("kbm")));

	for (AActor* Actor : FActorRange(World))
	{
		if (Actor)
		{
			Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
		}
	}
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRunEnd, "Dusktracer.Telemetry.RunEnd", DTTelHeroTest::Flags) // R-DIR-15, R-TEL-08, R-TEL-10 in a real world
bool FDTTelRunEnd::RunTest(const FString& Parameters)
{
	using namespace DTTelHeroTest;
	FWorld Scene(TEXT("DTTelRunEndWorld"));
	UWorld* World = Scene.World;
	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
	UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
	UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
	UDTDirector* Director = World->GetSubsystem<UDTDirector>();
	FString Jsonl;
	FVector2f EnemyAt = FVector2f::ZeroVector;
	if (TestNotNull(TEXT("sim"), Sim) && TestNotNull(TEXT("telemetry"), Telemetry) && TestNotNull(TEXT("heroes"), Heroes)
		&& TestNotNull(TEXT("enemies"), Enemies) && TestNotNull(TEXT("director"), Director))
	{
		Sim->StartRun(41, 1);
		// A hitch before the session: steps dropped then are not this run's.
		Sim->AdvanceFrame(DTTelRunEndHitchSeconds);
		Telemetry->StartSession(41, 1);
		Director->StartRun();
		Sim->StepNow(DTConst::SIM_FIXED_HZ); // inside the first Harvest block
		// Two hitches in the run: each frame runs C.SIM.MAX_SUBSTEPS steps and drops the rest (R-TEC-03).
		Sim->AdvanceFrame(DTTelRunEndHitchSeconds);
		Sim->AdvanceFrame(DTTelRunEndHitchSeconds);

		// A lethal touch off to one side of the hero: the record must tell the two positions apart.
		const FVector2f Hero = Sim->GetView().HeroPositions[0];
		EnemyAt = Hero + FVector2f(0.5f * static_cast<float>(DTConst::HERO_RADIUS_UU), 0.0f);
		FDTEnemySpawn Lethal;
		Lethal.Position = EnemyAt;
		Lethal.DmgScale = static_cast<float>(DTConst::HERO_MAX_HP) * 10.0f;
		Enemies->QueueSpawn(Lethal);
		for (int32 Step = 0; Step < DTConst::SIM_FIXED_HZ && !Heroes->IsDown(0); ++Step)
		{
			Sim->StepNow(1);
		}
		TestTrue(TEXT("the hero went down"), Heroes->IsDown(0));
		Sim->StepNow(DTConst::SIM_FIXED_HZ); // the sim goes on until the session closes: none of that is the block's
		TakeSession(*Telemetry, TEXT("death"), Jsonl);
	}

	const TArray<FString> Blocks = OfKind(Jsonl, TEXT("dir_block"));
	const TArray<FString> Down = OfKind(Jsonl, TEXT("hero_down"));
	const TArray<FString> Damaged = OfKind(Jsonl, TEXT("hero_damaged"));
	TestEqual(TEXT("the block a death falls in is written, once"), Blocks.Num(), 1);
	TestEqual(TEXT("one hero_down"), Down.Num(), 1);
	TestEqual(TEXT("the fatal hit"), Damaged.Num(), 1);
	if (Blocks.Num() == 1 && Down.Num() == 1 && Damaged.Num() == 1)
	{
		TestTrue(TEXT("ended by the death"), Blocks[0].Contains(TEXT("\"ended_by\":\"death\"")));
		TestEqual(TEXT("it ends at the fatal hit"), Number(Blocks[0], TEXT("end_s")), Number(Down[0], TEXT("t")), 1e-3);
		const double Share = Number(Damaged[0], TEXT("amount")) / Number(Damaged[0], TEXT("max_hp"));
		const FString Taken = TEXT("\"dmg_taken\":{\"0\":");
		const int32 At = Blocks[0].Find(Taken);
		TestTrue(TEXT("dmg_taken holds the fatal hit"), At != INDEX_NONE && FMath::IsNearlyEqual(FCString::Atod(*Blocks[0] + At + Taken.Len()), Share, 1e-3));
		TestEqual(TEXT("src_x is the enemy's"), Number(Damaged[0], TEXT("src_x")), static_cast<double>(FMath::RoundToInt32(EnemyAt.X)), 200.0);
		TestTrue(TEXT("the source is not the hero"), Number(Damaged[0], TEXT("src_x")) != Number(Damaged[0], TEXT("hero_x")));
	}

	// A slow PC's run can be told from a normal one: run_end counts the steps the clock dropped and the wall time (R-TEL-08).
	const TArray<FString> End = OfKind(Jsonl, TEXT("run_end"));
	if (TestEqual(TEXT("one run_end"), End.Num(), 1))
	{
		const int32 HitchSteps = FMath::RoundToInt32(DTTelRunEndHitchSeconds * DTConst::SIM_FIXED_HZ);
		TestTrue(TEXT("run_end has wall_dropped_steps"), End[0].Contains(TEXT("\"wall_dropped_steps\":")));
		TestEqual(TEXT("wall_dropped_steps counts the run's dropped steps, not the ones before it"),
			Number(End[0], TEXT("wall_dropped_steps")), static_cast<double>(2 * (HitchSteps - DTConst::SIM_MAX_SUBSTEPS)));
		TestTrue(TEXT("run_end has wall_duration_s"), End[0].Contains(TEXT("\"wall_duration_s\":")));
		TestTrue(TEXT("wall_duration_s is the wall time of this session"),
			Number(End[0], TEXT("wall_duration_s")) >= 0.0 && Number(End[0], TEXT("wall_duration_s")) < 300.0);
	}

	const TArray<FString> Start = OfKind(Jsonl, TEXT("run_start"));
	if (TestEqual(TEXT("one run_start"), Start.Num(), 1))
	{
		TestFalse(TEXT("run_start claims no input device"), Start[0].Contains(TEXT("input_device")));
		TestTrue(TEXT("this world names its build configuration"), Start[0].Contains(FString::Printf(TEXT("\"config\":\"%s\""), LexToString(FApp::GetBuildConfiguration()))));
		TestTrue(TEXT("a test is never a person"), Start[0].Contains(TEXT("\"driver\":\"tool\"")));
		const FString Field = TEXT("\"wall\":\"");
		const int32 At = Start[0].Find(Field);
		FDateTime Wall;
		TestTrue(TEXT("wall parses"), At != INDEX_NONE && FDateTime::ParseIso8601(*Start[0].Mid(At + Field.Len()).LeftChop(2), Wall));
		TestTrue(TEXT("wall is UTC, as its Z says"), FMath::Abs((FDateTime::UtcNow() - Wall).GetTotalMinutes()) < 5.0);
	}
	Scene.Close();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelSessionEndClose, "Dusktracer.Telemetry.SessionEndClose", DTTelHeroTest::Flags) // R-DIR-15 in a real world
bool FDTTelSessionEndClose::RunTest(const FString& Parameters)
{
	using namespace DTTelHeroTest;
	FWorld Scene(TEXT("DTTelSessionEndWorld"));
	UWorld* World = Scene.World;
	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
	UDTDirector* Director = World->GetSubsystem<UDTDirector>();
	FString Jsonl;
	if (TestNotNull(TEXT("sim"), Sim) && TestNotNull(TEXT("telemetry"), Telemetry) && TestNotNull(TEXT("director"), Director))
	{
		Sim->StartRun(53, 1);
		Telemetry->StartSession(53, 1);
		Director->StartRun();
		Sim->StepNow(DTConst::SIM_FIXED_HZ); // inside the first Harvest block
		TakeSession(*Telemetry, TEXT("quit"), Jsonl);
	}

	const TArray<FString> Blocks = OfKind(Jsonl, TEXT("dir_block"));
	if (TestEqual(TEXT("the open block is written, once"), Blocks.Num(), 1))
	{
		TestTrue(TEXT("ended by the session's reason"), Blocks[0].Contains(TEXT("\"ended_by\":\"quit\"")));
		TestEqual(TEXT("at the quit"), Number(Blocks[0], TEXT("end_s")), 1.0, 2.0 / DTConst::SIM_FIXED_HZ);
	}
	const int32 Block = Jsonl.Find(TEXT("\"kind\":\"dir_block\""));
	const int32 End = Jsonl.Find(TEXT("\"kind\":\"run_end\""));
	TestTrue(TEXT("before run_end"), Block != INDEX_NONE && End != INDEX_NONE && Block < End);
	Scene.Close();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRoundSource, "Dusktracer.Telemetry.RoundSource", DTTelHeroTest::Flags) // T-TEL-10 in a real world
bool FDTTelRoundSource::RunTest(const FString& Parameters)
{
	using namespace DTTelHeroTest;
	FWorld Scene(TEXT("DTTelRoundSourceWorld"));
	UWorld* World = Scene.World;
	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
	UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
	UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
	FString Jsonl;
	const float Speed = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_SPEED_UU_S);
	const float Range = 3.0f * static_cast<float>(DTConst::HERO_RADIUS_UU + DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
	FVector2f FiredFrom = FVector2f::ZeroVector;
	if (TestNotNull(TEXT("sim"), Sim) && TestNotNull(TEXT("telemetry"), Telemetry) && TestNotNull(TEXT("heroes"), Heroes) && TestNotNull(TEXT("enemies"), Enemies))
	{
		Sim->StartRun(47, 1);
		Telemetry->StartSession(47, 1);
		Sim->StepNow(1);

		// A round fired from well outside the touch: the record must say where from, not where it landed.
		FiredFrom = Sim->GetView().HeroPositions[0] + FVector2f(Range, 0.0f);
		FDTEnemyRoundSpawn Round;
		Round.Position = FiredFrom;
		Round.Velocity = FVector2f(-Speed, 0.0f);
		Round.Radius = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_RADIUS_UU);
		Round.Damage = static_cast<float>(DTConst::ENEMY_FIRE_SHOT_DAMAGE);
		Round.Life = DTConst::ENEMY_FIRE_SHOT_LIFE_S;
		Round.TargetPlayer = 0;
		Round.AttackId = FName(TEXT("Turret.Fire"));
		Enemies->GetPool().GetRounds().Spawn(Round);
		for (int32 Step = 0; Step < 2 * DTConst::SIM_FIXED_HZ && Heroes->GetState(0)->HitsTaken == 0; ++Step)
		{
			Sim->StepNow(1);
		}
		TakeSession(*Telemetry, TEXT("test"), Jsonl);
	}

	const TArray<FString> Damaged = OfKind(Jsonl, TEXT("hero_damaged"));
	if (TestEqual(TEXT("the round's hit"), Damaged.Num(), 1))
	{
		const double Step = 1.0 / DTConst::SIM_FIXED_HZ;
		TestTrue(TEXT("hit_kind Bullet"), Damaged[0].Contains(TEXT("\"hit_kind\":\"Bullet\"")));
		TestEqual(TEXT("src_x is the firing point"), Number(Damaged[0], TEXT("src_x")), static_cast<double>(FMath::RoundToInt32(FiredFrom.X)));
		TestEqual(TEXT("src_y is the firing point"), Number(Damaged[0], TEXT("src_y")), static_cast<double>(FMath::RoundToInt32(FiredFrom.Y)));
		TestEqual(TEXT("vel_x is the round's"), Number(Damaged[0], TEXT("vel_x")), static_cast<double>(FMath::RoundToInt32(-Speed)));
		TestEqual(TEXT("vel_y is the round's"), Number(Damaged[0], TEXT("vel_y")), 0.0);
		const double Reach = DTConst::HERO_RADIUS_UU + DTConst::ENEMY_FIRE_SHOT_RADIUS_UU;
		TestEqual(TEXT("flight_s is the time it flew"), Number(Damaged[0], TEXT("flight_s")), (Range - Reach) / Speed, 1.5 * Step);
	}
	Scene.Close();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelLoadoutRecords, "Dusktracer.Telemetry.LoadoutRecords", DTTelHeroTest::Flags) // T-TEL-12
bool FDTTelLoadoutRecords::RunTest(const FString& Parameters)
{
	using namespace DTTelHeroTest;
	FWorld Scene(TEXT("DTTelLoadoutWorld"));
	UWorld* World = Scene.World;
	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
	UDTWeaponSubsystem* Weapons = World->GetSubsystem<UDTWeaponSubsystem>();
	UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
	FString Jsonl;
	const FName Rifle(TEXT("Weapon.PulseRifle"));
	if (TestNotNull(TEXT("sim"), Sim) && TestNotNull(TEXT("telemetry"), Telemetry) && TestNotNull(TEXT("weapons"), Weapons) && TestNotNull(TEXT("enemies"), Enemies))
	{
		Sim->StartRun(43, 1);
		Telemetry->StartSession(43, 1);
		TestEqual(TEXT("given"), Weapons->Give(0, Rifle, 1), EDTWeaponResult::Ok);
		TestEqual(TEXT("levelled"), Weapons->LevelUp(0, Rifle, 2), EDTWeaponResult::Ok);
		TestEqual(TEXT("socketed"), Weapons->Socket(0, Rifle, TEXT("Mod.Pierce")), EDTWeaponResult::Ok);
		TestEqual(TEXT("a refused gift writes nothing"), Weapons->Give(0, Rifle, 1), EDTWeaponResult::AlreadyOwned);

		// Something to shoot at, for the emissions.
		FDTEnemySpawn Target;
		Target.Position = FVector2f(300.0f, 0.0f);
		Enemies->QueueSpawn(Target);
		Sim->StepNow(DTConst::SIM_FIXED_HZ);

		// A full conversion puts the Pierce to sleep (R-MOD-08); its end wakes it.
		FDTWeaponFlags Hammer;
		Hammer.Add(DTCat::EFlag::ConvertMelee, DTWeapons::KeeperSource(EDTPath::Forge), EDTFlagTier::Key).Params.Set(DTFlagParam::Rounds, 2.0f);
		Weapons->AddFlagSource(0, TEXT("Test.Hammer"), Hammer);
		Sim->StepNow(1);
		Weapons->RemoveFlagSource(0, TEXT("Test.Hammer"));
		Sim->StepNow(1);
		TakeSession(*Telemetry, TEXT("test"), Jsonl);
	}

	TestEqual(TEXT("weapon_acquired"), CountKind(Jsonl, TEXT("weapon_acquired")), 1);
	TestTrue(TEXT("weapon_acquired names the source"), Jsonl.Contains(TEXT("\"kind\":\"weapon_acquired\",\"owner\":0,\"source\":\"Weapon.PulseRifle\"}")));
	TestTrue(TEXT("weapon_level carries the level"), Jsonl.Contains(TEXT("\"kind\":\"weapon_level\",\"owner\":0,\"source\":\"Weapon.PulseRifle\",\"level\":2}")));
	TestEqual(TEXT("weapon_level"), CountKind(Jsonl, TEXT("weapon_level")), 1);
	TestTrue(TEXT("modifier_socketed"), Jsonl.Contains(TEXT("\"kind\":\"modifier_socketed\",\"owner\":0,\"weapon\":\"Weapon.PulseRifle\",\"modifier\":\"Mod.Pierce\"}")));
	const TArray<FString> Dormant = OfKind(Jsonl, TEXT("modifier_dormant"));
	if (TestEqual(TEXT("asleep and awake"), Dormant.Num(), 2))
	{
		TestTrue(TEXT("first dormant"), Dormant[0].Contains(TEXT("\"modifier\":\"Mod.Pierce\",\"dormant\":true}")));
		TestTrue(TEXT("then awake"), Dormant[1].Contains(TEXT("\"modifier\":\"Mod.Pierce\",\"dormant\":false}")));
	}
	const TArray<FString> Interval = OfKind(Jsonl, TEXT("weapon"));
	if (TestEqual(TEXT("the owned weapon's interval record"), Interval.Num(), 1))
	{
		TestEqual(TEXT("its level"), Number(Interval[0], TEXT("level")), 2.0);
		TestEqual(TEXT("its load rank"), Number(Interval[0], TEXT("load_rank")), 0.0);
		TestTrue(TEXT("its emissions"), Number(Interval[0], TEXT("spawned")) > 0.0);
	}
	Scene.Close();
	return true;
}

#endif
