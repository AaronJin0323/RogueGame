// Hero producers reach telemetry in a real world (combat_core.md §Telemetry; telemetry.md CSV damage_taken_per_min,
// dashes_per_min, input_device): the telemetry subsystem binds UDTHeroSubsystem::OnHeroEvent at begin play.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Telemetry.Hero; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Enemies/DTEnemyTypes.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
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

#endif
