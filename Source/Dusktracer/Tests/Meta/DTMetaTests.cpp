// The meta save, the award and the run-end autosave (02_Systems/meta.md T-MET-01..19, T-MET-40..43; T-MET-06 is in Tests/Settings, the menu's text being a settings header).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Meta; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Boss/DTBossCore.h"
#include "Bot/DTBotSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSaveEnvelope.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Enemies/DTEnemyPool.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Internationalization/Regex.h"
#include "Meta/DTDiscovery.h"
#include "Meta/DTMetaProfile.h"
#include "Meta/DTMetaSession.h"
#include "Meta/DTMetaSubsystem.h"
#include "Meta/DTRunRecorder.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"
#include "Templates/UniquePtr.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTMetaTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr int32 BotSeconds = 45;

	/** Every file a test path could leave behind is gone before it starts. */
	static FString TempSave(const TCHAR* Name)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("MetaTests")));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, Name);
		TArray<FString> Old;
		IFileManager::Get().FindFiles(Old, *(Path + TEXT("*")), true, false);
		for (const FString& File : Old)
		{
			IFileManager::Get().Delete(*FPaths::Combine(Dir, File), false, true, true);
		}
		IFileManager::Get().DeleteDirectory(*(Path + TEXT(".tmp")), false, true);
		return Path;
	}

	static FDTRunTally Tally(int64 Steps, int64 Kills = 0, int64 Elites = 0, bool bBoss = false, int32 Heat = 0)
	{
		FDTRunTally T;
		T.Steps = Steps;
		T.Kills = Kills;
		T.Elites = Elites;
		T.bBossKilled = bBoss;
		T.Heat = Heat;
		return T;
	}

	static int64 StepsForMinutes(int32 Minutes)
	{
		return static_cast<int64>(Minutes) * 60 * DTConst::SIM_FIXED_HZ;
	}

	/** One change per field, so a field dropped from the payload shows (T-MET-18). */
	static TArray<TFunction<void(FDTMetaProfile&)>> FieldEdits()
	{
		const FName Key(TEXT("Some.Key"));
		return {
			[](FDTMetaProfile& P) { P.Currency = 123456789012LL; },
			[](FDTMetaProfile& P) { P.Runs = 9; },
			[](FDTMetaProfile& P) { P.Runs = 9; P.Wins = 2; },
			[](FDTMetaProfile& P) { P.Runs = 9; P.DeathsSinceWin = 3; },
			[](FDTMetaProfile& P) { P.LastPatron = FName(TEXT("Tide")); },
			[Key](FDTMetaProfile& P) { P.TreeRanks.Add(Key, 2); },
			[Key](FDTMetaProfile& P) { P.Unlocked.Add(Key); },
			[Key](FDTMetaProfile& P) { P.Discoveries.Add(Key); },
			[Key](FDTMetaProfile& P) { P.LifetimeStats.Add(Key, 5000000000LL); },
			[](FDTMetaProfile& P) { P.Act = DTConst::META_ACT_MAX; },
			[Key](FDTMetaProfile& P) { P.StoryFlags.Add(Key); },
			[](FDTMetaProfile& P) { P.RecollectionsRead = 4; },
			[Key](FDTMetaProfile& P) { P.ArenaWins.Add(Key, 1); },
			[Key](FDTMetaProfile& P) { P.ArenaHeat.Add(Key, 3); },
			[](FDTMetaProfile& P) { P.FateTide = EDTFateTide::Returned; },
			[](FDTMetaProfile& P) { P.FateShade = EDTFateShade::OffLedger; },
			[Key](FDTMetaProfile& P) { P.FateSeen.Add(Key); } };
	}

	static FDTMetaProfile FullProfile()
	{
		FDTMetaProfile P;
		for (const TFunction<void(FDTMetaProfile&)>& Edit : FieldEdits())
		{
			Edit(P);
		}
		return P;
	}

	static bool WriteFile(const FString& Path, const TArray<uint8>& Bytes)
	{
		return FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	static TArray<uint8> ReadFile(const FString& Path)
	{
		TArray<uint8> Bytes;
		FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent);
		return Bytes;
	}

	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTMetaTestWorld")));
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
		~FTestWorld()
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

	struct FRunResult
	{
		uint32 InputHash = 0;
		int64 Steps = 0;
		FVector2f Pos = FVector2f::ZeroVector;
		float Hp = 0.0f;
		int32 Dashes = 0;
		int32 Level = 0;
		FDTRunTally Tally;
		FDTRunAward Award;
		FString Csv;
		FString Jsonl;
	};

	static FRunResult Play(int32 Seed, bool bRecorderOnClock)
	{
		FRunResult Result;
		FTestWorld Test;
		UDTSimSubsystem* Sim = Test.World->GetSubsystem<UDTSimSubsystem>();
		UDTBotSubsystem* Bot = Test.World->GetSubsystem<UDTBotSubsystem>();
		UDTTelemetrySubsystem* Telemetry = Test.World->GetSubsystem<UDTTelemetrySubsystem>();
		UDTDirector* Director = Test.World->GetSubsystem<UDTDirector>();
		UDTRunRecorder* Recorder = Test.World->GetSubsystem<UDTRunRecorder>();
		const UDTHeroSubsystem* Heroes = Test.World->GetSubsystem<UDTHeroSubsystem>();
		if (Sim && Bot && Telemetry && Director && Recorder && Heroes)
		{
			if (!bRecorderOnClock)
			{
				Sim->Unregister(Recorder);
			}
			Sim->StartRun(Seed, 1);
			Telemetry->StartSession(Seed, 1);
			Director->StartRun();
			FDTBotConfig Config;
			Config.Policy = EDTBotPolicy::Steered;
			Config.Path = EDTPath::Storm;
			Config.Archetype = FName(TEXT("Tempest"));
			Config.Version = 2;
			Config.Skill = 0.6f;
			Config.Seed = Seed;
			Bot->Activate(Config);
			const int32 Steps = BotSeconds * DTConst::SIM_FIXED_HZ;
			while (Sim->GetStepIndex() < Steps && !Bot->IsRunFinished())
			{
				if (Bot->RunSteps(Steps - static_cast<int32>(Sim->GetStepIndex())) == 0)
				{
					break;
				}
			}
			Telemetry->EndSession(TEXT("test"));
			const FString CsvPath = Telemetry->GetWriter().GetCsvPath();
			const FString JsonlPath = Telemetry->GetWriter().GetJsonlPath();
			FFileHelper::LoadFileToString(Result.Csv, *CsvPath);
			FFileHelper::LoadFileToString(Result.Jsonl, *JsonlPath);
			IFileManager::Get().Delete(*CsvPath, false, true, true);
			IFileManager::Get().Delete(*JsonlPath, false, true, true);
			Result.InputHash = Bot->GetInputHash();
			Result.Steps = Sim->GetStepIndex();
			Result.Tally = Recorder->GetTally();
			Result.Award = Recorder->GetAward();
			if (const FDTHeroState* State = Heroes->GetState(0))
			{
				Result.Pos = State->Pos;
				Result.Hp = State->Hp;
				Result.Dashes = State->Dashes;
			}
			Result.Level = Sim->GetRun().GetPlayer(0).Level;
			Bot->Deactivate();
		}
		return Result;
	}

	/** Every line but run_award, wall fields gone (R-TEL-08). */
	static FString Comparable(const FString& Jsonl)
	{
		TArray<FString> Lines;
		Jsonl.ParseIntoArrayLines(Lines);
		FString Kept;
		const FRegexPattern Wall(TEXT(",?\"wall[a-z_]*\":(\"([^\"\\\\]|\\\\.)*\"|[-+0-9.eE]+)"));
		for (const FString& Line : Lines)
		{
			if (Line.Contains(TEXT("\"kind\":\"run_award\"")))
			{
				continue;
			}
			FRegexMatcher Matcher(Wall, Line);
			int32 Last = 0;
			while (Matcher.FindNext())
			{
				Kept += Line.Mid(Last, Matcher.GetMatchBeginning() - Last);
				Last = Matcher.GetMatchEnding();
			}
			Kept += Line.Mid(Last) + TEXT("\n");
		}
		return Kept;
	}

	static int32 CountOf(const FString& Text, const TCHAR* Needle)
	{
		int32 Count = 0;
		for (int32 At = Text.Find(Needle); At != INDEX_NONE; At = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + 1))
		{
			++Count;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetNeutralKeys, "Dusktracer.Meta.NeutralKeys", DTMetaTest::Flags) // T-MET-01
bool FDTMetNeutralKeys::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer/Meta")));
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.*"), true, false);
	TestTrue(TEXT("found the source"), Files.Num() >= 8);
	for (const FString& File : Files)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *File);
		for (const TCHAR* Name : { TEXT("Ichor"), TEXT("Nightblood"), TEXT("Vigil"), TEXT("Contract"), TEXT("Keep "), TEXT("God") })
		{
			TestFalse(FString::Printf(TEXT("%s holds no %s"), *FPaths::GetCleanFilename(File), Name), Text.Contains(Name, ESearchCase::CaseSensitive));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetAward, "Dusktracer.Meta.Award", DTMetaTest::Flags) // T-MET-02
bool FDTMetAward::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	TestEqual(TEXT("nothing earns nothing"), DTMeta::ComputeAward(FDTRunTally()), 0LL);
	TestEqual(TEXT("minutes"), DTMeta::ComputeAward(Tally(StepsForMinutes(6))), static_cast<int64>(DTConst::META_CURRENCY_PER_MINUTE * 6));
	TestEqual(TEXT("half a minute counts"), DTMeta::ComputeAward(Tally(StepsForMinutes(1) / 2)), static_cast<int64>(DTConst::META_CURRENCY_PER_MINUTE / 2));
	TestEqual(TEXT("elites"), DTMeta::ComputeAward(Tally(0, 0, 3)), static_cast<int64>(DTConst::META_CURRENCY_PER_ELITE * 3));
	TestEqual(TEXT("the boss"), DTMeta::ComputeAward(Tally(0, 0, 0, true)), static_cast<int64>(DTConst::META_CURRENCY_BOSS_KILL));
	const int64 Thousand = 1000;
	const int64 KillPay = FMath::FloorToInt64(static_cast<double>(DTConst::META_CURRENCY_PER_KILL) * Thousand);
	TestEqual(TEXT("kills"), DTMeta::ComputeAward(Tally(0, Thousand)), KillPay);
	const double OneKill = static_cast<double>(DTConst::META_CURRENCY_PER_KILL);
	TestEqual(TEXT("the total is floored"), DTMeta::ComputeAward(Tally(0, 1)), FMath::FloorToInt64(OneKill));

	const int64 All = DTConst::META_CURRENCY_PER_MINUTE * 10 + KillPay + DTConst::META_CURRENCY_PER_ELITE * 2 + DTConst::META_CURRENCY_BOSS_KILL;
	TestEqual(TEXT("all together"), DTMeta::ComputeAward(Tally(StepsForMinutes(10), Thousand, 2, true)), All);
	const int32 Heat = 2;
	const double HeatScale = 1.0 + static_cast<double>(DTConst::META_CURRENCY_HEAT_BONUS) * Heat;
	TestEqual(TEXT("Heat scales the total"), DTMeta::ComputeAward(Tally(StepsForMinutes(10), Thousand, 2, true, Heat)), FMath::FloorToInt64(static_cast<double>(All) * HeatScale));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetTally, "Dusktracer.Meta.Tally", DTMetaTest::Flags) // T-MET-03
bool FDTMetTally::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FRunResult Run = Play(73, true);
	TestTrue(TEXT("the bot killed"), Run.Tally.Kills > 0);
	TestEqual(TEXT("steps count the run"), Run.Tally.Steps, Run.Steps);
	int64 Logged = 0;
	FRegexMatcher Matcher(FRegexPattern(TEXT("\"kind\":\"kill\".*?\"kills\":(\\d+)")), Run.Jsonl);
	while (Matcher.FindNext())
	{
		Logged += FCString::Atoi64(*Matcher.GetCaptureGroup(1));
	}
	TestEqual(TEXT("the kills are telemetry's"), Run.Tally.Kills, Logged);
	TestTrue(TEXT("a played run is banked"), Run.Award.bBanked);
	TestEqual(TEXT("the award is the formula"), Run.Award.Retained, DTMeta::ComputeAward(Run.Tally));

	// The end freezes the count at the end of its step: that step's kills count, the next step's do not.
	FTestWorld Test;
	UDTRunRecorder* Recorder = Test.World->GetSubsystem<UDTRunRecorder>();
	UDTHeroSubsystem* Heroes = Test.World->GetSubsystem<UDTHeroSubsystem>();
	if (!TestNotNull(TEXT("recorder"), Recorder) || !TestNotNull(TEXT("heroes"), Heroes))
	{
		return false;
	}
	FDTWorldView View;
	FDTKill& Kill = View.Kills.AddDefaulted_GetRef();
	Kill.Role = EDTEnemyRole::Boss;
	Kill.bElite = true;
	FDTSimContext Context;
	Context.View = &View;
	Recorder->SimStep(Context);
	TestTrue(TEXT("the boss kill is seen"), Recorder->GetTally().bBossKilled);
	Heroes->OnAllHeroesDown.Broadcast(1);
	Recorder->SimStep(Context);
	Recorder->SimStep(Context);
	TestEqual(TEXT("steps stop at the end"), Recorder->GetTally().Steps, 2LL);
	TestEqual(TEXT("kills stop at the end"), Recorder->GetTally().Kills, 2LL);
	TestEqual(TEXT("elites stop at the end"), Recorder->GetTally().Elites, 2LL);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetBankOnce, "Dusktracer.Meta.BankOnce", DTMetaTest::Flags) // T-MET-04
bool FDTMetBankOnce::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	{
		FTestWorld Test;
		UDTSimSubsystem* Sim = Test.World->GetSubsystem<UDTSimSubsystem>();
		UDTTelemetrySubsystem* Telemetry = Test.World->GetSubsystem<UDTTelemetrySubsystem>();
		UDTRunRecorder* Recorder = Test.World->GetSubsystem<UDTRunRecorder>();
		if (!TestNotNull(TEXT("sim"), Sim) || !TestNotNull(TEXT("telemetry"), Telemetry) || !TestNotNull(TEXT("recorder"), Recorder))
		{
			return false;
		}
		Sim->StartRun(74, 1);
		Telemetry->StartSession(74, 1);
		Sim->StepNow(DTConst::SIM_FIXED_HZ);
		Telemetry->EndSession(TEXT("death"));
		const FDTRunAward First = Recorder->GetAward();
		Telemetry->EndSession(TEXT("restart"));
		const FString Path = Telemetry->GetWriter().GetJsonlPath();
		FString Jsonl;
		FFileHelper::LoadFileToString(Jsonl, *Path);
		TestTrue(TEXT("banked"), First.bBanked);
		TestEqual(TEXT("one award record"), CountOf(Jsonl, TEXT("\"kind\":\"run_award\"")), 1);
		TestTrue(TEXT("it reads the first end"), Jsonl.Contains(TEXT("\"end\":\"death\"")));
		TestTrue(TEXT("before run_end"), Jsonl.Find(TEXT("\"run_award\"")) < Jsonl.Find(TEXT("\"run_end\"")));
		IFileManager::Get().Delete(*Path, false, true, true);
		IFileManager::Get().Delete(*Telemetry->GetWriter().GetCsvPath(), false, true, true);
	}
	const FString Save = TempSave(TEXT("ZeroStep.sav"));
	FDTMetaSession Session;
	Session.Open(Save);
	const FDTRunAward None = Session.Bank(FDTRunTally(), EDTRunEnd::Left);
	TestFalse(TEXT("a zero-step run banks nothing"), None.bBanked);
	TestTrue(TEXT("the profile stays empty"), DTMeta::IsEmpty(Session.Profile));
	TestFalse(TEXT("and writes no file"), IFileManager::Get().FileExists(*Save));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetBankCounts, "Dusktracer.Meta.BankCounts", DTMetaTest::Flags) // T-MET-05
bool FDTMetBankCounts::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	FDTMetaProfile P;
	FDTRunTally Run = Tally(StepsForMinutes(2), 100, 1);
	Run.Patron = FName(TEXT("Forge"));
	Run.Counters.Add(TEXT("Kills.Weapon.Scatter"), 40);
	const int64 Award = DTMeta::ApplyRun(P, Run, EDTRunEnd::Death);
	TestEqual(TEXT("the award is added"), P.Currency, Award);
	TestEqual(TEXT("a death is a run"), P.Runs, 1);
	TestEqual(TEXT("and a repair"), P.DeathsSinceWin, 1);
	TestEqual(TEXT("no win"), P.Wins, 0);
	TestEqual(TEXT("the Patron stays"), P.LastPatron, FName(TEXT("Forge")));

	DTMeta::ApplyRun(P, Tally(StepsForMinutes(1)), EDTRunEnd::Left);
	TestEqual(TEXT("leaving is a run"), P.Runs, 2);
	TestEqual(TEXT("leaving moves no repair"), P.DeathsSinceWin, 1);
	TestEqual(TEXT("an empty Patron keeps the last"), P.LastPatron, FName(TEXT("Forge")));

	DTMeta::ApplyRun(P, Tally(StepsForMinutes(1), 0, 0, true), EDTRunEnd::Victory);
	TestEqual(TEXT("a win"), P.Wins, 1);
	TestEqual(TEXT("a win clears the repairs"), P.DeathsSinceWin, 0);
	TestEqual(TEXT("kills add"), P.LifetimeStats.FindRef(TEXT("Kills")), 100LL);
	TestEqual(TEXT("elites add"), P.LifetimeStats.FindRef(TEXT("Elites")), 1LL);
	TestEqual(TEXT("boss kills add"), P.LifetimeStats.FindRef(TEXT("BossKills")), 1LL);
	TestEqual(TEXT("steps add"), P.LifetimeStats.FindRef(TEXT("Steps")), StepsForMinutes(4));
	TestEqual(TEXT("earned matches the balance"), P.LifetimeStats.FindRef(TEXT("CurrencyEarned")), P.Currency);
	TestEqual(TEXT("counters add by name"), P.LifetimeStats.FindRef(TEXT("Kills.Weapon.Scatter")), 40LL);

	TestTrue(TEXT("Abandon run is a death"), UDTRunRecorder::EndFromReason(TEXT("abandoned")) == EDTRunEnd::Death);
	TestTrue(TEXT("a win"), UDTRunRecorder::EndFromReason(TEXT("victory")) == EDTRunEnd::Victory);
	for (const TCHAR* Reason : { TEXT("restart"), TEXT("quit"), TEXT("bot_timeout"), TEXT("world_end") })
	{
		TestTrue(Reason, UDTRunRecorder::EndFromReason(Reason) == EDTRunEnd::Left);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetFileHeader, "Dusktracer.Meta.FileHeader", DTMetaTest::Flags) // T-MET-07
bool FDTMetFileHeader::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FString Path = TempSave(TEXT("Header.sav"));
	FDTMetaSession Session;
	Session.Open(Path);
	Session.Profile.Currency = 5;
	TestTrue(TEXT("written"), Session.Write());
	const TArray<uint8> Bytes = ReadFile(Path);
	TArray<uint8> Payload;
	int32 Loaded = INDEX_NONE;
	TestEqual(TEXT("the envelope opens it"), FDTSaveEnvelope().Unwrap(Bytes, Payload, Loaded), EDTSaveLoadResult::Ok);
	TestEqual(TEXT("at the current version"), Loaded, DTConst::SAVE_VERSION);
	TestTrue(TEXT("the payload is canonical"), Payload == DTMeta::Serialize(Session.Profile));
	TestTrue(TEXT("the default slot"), DTMeta::DefaultPath().EndsWith(TEXT("Saved/SaveGames/DTMeta.sav")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetCanonical, "Dusktracer.Meta.Canonical", DTMetaTest::Flags) // T-MET-08
bool FDTMetCanonical::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FDTMetaProfile Full = FullProfile();
	FDTMetaProfile Read;
	TestTrue(TEXT("reads back"), DTMeta::Deserialize(DTMeta::Serialize(Full), Read));
	TestTrue(TEXT("every field round-trips"), DTMeta::Same(Read, Full));

	FDTMetaProfile Ab;
	FDTMetaProfile Ba;
	Ab.Unlocked = { FName(TEXT("Alpha")), FName(TEXT("Beta")), FName(TEXT("Gamma")) };
	Ba.Unlocked = { FName(TEXT("Gamma")), FName(TEXT("Beta")), FName(TEXT("Alpha")) };
	Ab.TreeRanks.Add(TEXT("One"), 1);
	Ab.TreeRanks.Add(TEXT("Two"), 2);
	Ba.TreeRanks.Add(TEXT("Two"), 2);
	Ba.TreeRanks.Add(TEXT("One"), 1);
	TestTrue(TEXT("insertion order leaves the bytes alone"), DTMeta::Serialize(Ab) == DTMeta::Serialize(Ba));

	const TArray<uint8> Good = DTMeta::Serialize(Full);
	TArray<uint8> Short = Good;
	Short.Pop();
	TestFalse(TEXT("truncated"), DTMeta::Deserialize(Short, Read));
	TArray<uint8> Long = Good;
	Long.Add(0);
	TestFalse(TEXT("padded"), DTMeta::Deserialize(Long, Read));
	TestFalse(TEXT("nothing"), DTMeta::Deserialize(TArray<uint8>(), Read));

	FDTMetaProfile EmptyKey;
	EmptyKey.Unlocked.Add(NAME_None);
	TestFalse(TEXT("an empty key"), DTMeta::Deserialize(DTMeta::Serialize(EmptyKey), Read));
	TArray<uint8> HugeCount = DTMeta::Serialize(FDTMetaProfile());
	const int32 TreeCountAt = sizeof(int64) + 3 * sizeof(int32) + sizeof(int32); // balance, three counts, an empty Patron
	HugeCount[TreeCountAt] = 0xFF;
	HugeCount[TreeCountAt + 1] = 0xFF;
	HugeCount[TreeCountAt + 2] = 0xFF;
	HugeCount[TreeCountAt + 3] = 0x7F;
	TestFalse(TEXT("a count larger than the file"), DTMeta::Deserialize(HugeCount, Read));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetValidator, "Dusktracer.Meta.Validator", DTMetaTest::Flags) // T-MET-09
bool FDTMetValidator::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	TestTrue(TEXT("empty is valid"), DTMeta::IsValid(FDTMetaProfile()));
	TestTrue(TEXT("full is valid"), DTMeta::IsValid(FullProfile()));
	const FName Key(TEXT("Key"));
	const TArray<TFunction<void(FDTMetaProfile&)>> Breaks = {
		[](FDTMetaProfile& P) { P.Currency = -1; },
		[](FDTMetaProfile& P) { P.Runs = -1; },
		[](FDTMetaProfile& P) { P.Runs = 1; P.Wins = 2; },
		[](FDTMetaProfile& P) { P.Wins = -1; },
		[](FDTMetaProfile& P) { P.Runs = 1; P.DeathsSinceWin = 2; },
		[](FDTMetaProfile& P) { P.Act = DTConst::META_ACT_START - 1; },
		[](FDTMetaProfile& P) { P.Act = DTConst::META_ACT_MAX + 1; },
		[](FDTMetaProfile& P) { P.RecollectionsRead = -1; },
		[Key](FDTMetaProfile& P) { P.TreeRanks.Add(Key, -1); },
		[Key](FDTMetaProfile& P) { P.LifetimeStats.Add(Key, -1); },
		[Key](FDTMetaProfile& P) { P.ArenaWins.Add(Key, -1); },
		[Key](FDTMetaProfile& P) { P.ArenaHeat.Add(Key, -1); } };
	for (int32 Index = 0; Index < Breaks.Num(); ++Index)
	{
		FDTMetaProfile P;
		Breaks[Index](P);
		TestFalse(FString::Printf(TEXT("break %d"), Index), DTMeta::IsValid(P));
	}

	const FString Path = TempSave(TEXT("Invalid.sav"));
	FDTMetaProfile Broken;
	Broken.Currency = -5;
	const TArray<uint8> Bytes = FDTSaveEnvelope::Wrap(DTMeta::Serialize(Broken));
	TestTrue(TEXT("the file is on disk"), WriteFile(Path, Bytes));
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	FDTMetaSession Session;
	TestEqual(TEXT("a good checksum does not save it"), Session.Open(Path), EDTMetaOpen::KeptAside);
	TestTrue(TEXT("kept whole"), ReadFile(Session.AsidePath) == Bytes);
	TestTrue(TEXT("the session is empty"), DTMeta::IsEmpty(Session.Profile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetMigration, "Dusktracer.Meta.Migration", DTMetaTest::Flags) // T-MET-10
bool FDTMetMigration::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const int32 Old = DTConst::SAVE_VERSION;
	const int32 Next = Old + 1;
	const FString Path = TempSave(TEXT("Migrate.sav"));
	FDTMetaProfile Saved;
	Saved.Currency = 40;
	Saved.TreeRanks.Add(TEXT("Trunk.FromALaterBuild"), 3);
	TestTrue(TEXT("an older file"), WriteFile(Path, FDTSaveEnvelope::Wrap(DTMeta::Serialize(Saved), Old)));

	FDTMetaSession Session;
	Session.Version = Next;
	Session.Envelope.Migrations.Add(Old, [](TArray<uint8>& Payload)
	{
		FDTMetaProfile Step;
		if (!DTMeta::Deserialize(Payload, Step))
		{
			return false;
		}
		Step.Currency += 2;
		Payload = DTMeta::Serialize(Step);
		return true;
	});
	TestEqual(TEXT("it upgrades"), Session.Open(Path), EDTMetaOpen::Read);
	TestEqual(TEXT("the step ran"), Session.Profile.Currency, 42LL);
	TestEqual(TEXT("an unknown key stays"), Session.Profile.TreeRanks.FindRef(TEXT("Trunk.FromALaterBuild")), 3);
	TestTrue(TEXT("written up"), Session.Write());
	int32 Loaded = INDEX_NONE;
	TArray<uint8> Payload;
	FDTSaveEnvelope().Unwrap(ReadFile(Path), Payload, Loaded, Next);
	TestEqual(TEXT("at the new version"), Loaded, Next);

	// The validator runs after the step: a step that breaks the profile keeps the file.
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	const FString BadStep = TempSave(TEXT("BadStep.sav"));
	WriteFile(BadStep, FDTSaveEnvelope::Wrap(DTMeta::Serialize(Saved), Old));
	FDTMetaSession Breaks;
	Breaks.Version = Next;
	Breaks.Envelope.Migrations.Add(Old, [](TArray<uint8>& Payload2)
	{
		FDTMetaProfile Step;
		DTMeta::Deserialize(Payload2, Step);
		Step.Currency = -1;
		Payload2 = DTMeta::Serialize(Step);
		return true;
	});
	TestEqual(TEXT("checked after the step"), Breaks.Open(BadStep), EDTMetaOpen::KeptAside);

	const FString Gap = TempSave(TEXT("Gap.sav"));
	const TArray<uint8> GapBytes = FDTSaveEnvelope::Wrap(DTMeta::Serialize(Saved), Old);
	WriteFile(Gap, GapBytes);
	FDTMetaSession Missing;
	Missing.Version = Next;
	TestEqual(TEXT("a missing step keeps the file"), Missing.Open(Gap), EDTMetaOpen::KeptAside);
	TestTrue(TEXT("whole"), ReadFile(Missing.AsidePath) == GapBytes);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetAtomic, "Dusktracer.Meta.AtomicWrite", DTMetaTest::Flags) // T-MET-11
bool FDTMetAtomic::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FString Path = TempSave(TEXT("Atomic.sav"));
	const FString Temp = Path + TEXT(".tmp");
	FDTMetaSession Session;
	Session.Open(Path);
	Session.Profile.Currency = 1;
	TestTrue(TEXT("first write"), Session.Write());
	const TArray<uint8> Before = ReadFile(Path);

	IFileManager::Get().MakeDirectory(*Temp, true);
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	Session.Profile.Currency = 2;
	TestFalse(TEXT("a blocked write fails"), Session.Write());
	IFileManager::Get().DeleteDirectory(*Temp, false, true);
	TestTrue(TEXT("the old file is whole"), ReadFile(Path) == Before);

	TestTrue(TEXT("a stale temp lands on disk"), WriteFile(Temp, { 1, 2, 3 }));
	FDTMetaSession Reader;
	TestEqual(TEXT("a stale temp is ignored"), Reader.Open(Path), EDTMetaOpen::Read);
	TestEqual(TEXT("the file's value"), Reader.Profile.Currency, 1LL);
	TestTrue(TEXT("and replaced"), Session.Write());
	TestFalse(TEXT("nothing is left beside the file"), IFileManager::Get().FileExists(*Temp));
	TestTrue(TEXT("the new value stands"), Reader.Open(Path) == EDTMetaOpen::Read && Reader.Profile.Currency == 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetBadFile, "Dusktracer.Meta.BadFile", DTMetaTest::Flags) // T-MET-12
bool FDTMetBadFile::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	FDTMetaProfile Saved;
	Saved.Currency = 900;
	const TArray<uint8> Good = FDTSaveEnvelope::Wrap(DTMeta::Serialize(Saved));
	TArray<uint8> Flipped = Good;
	Flipped.Last() ^= 0x01;
	TArray<uint8> Truncated = Good;
	Truncated.SetNum(Good.Num() / 2);
	TArray<uint8> BadMagic = Good;
	BadMagic[0] = 0;
	const TArray<TArray<uint8>> Cases = { Flipped, Truncated, BadMagic, FDTSaveEnvelope::Wrap(DTMeta::Serialize(Saved), DTConst::SAVE_VERSION + 1), {}, { 7, 7, 7 } };

	const FString Path = TempSave(TEXT("Bad.sav"));
	TArray<FString> Kept;
	for (int32 Index = 0; Index < Cases.Num(); ++Index)
	{
		TestTrue(TEXT("the bad file lands"), WriteFile(Path, Cases[Index]));
		FDTMetaSession Session;
		TestEqual(FString::Printf(TEXT("case %d goes aside"), Index), Session.Open(Path), EDTMetaOpen::KeptAside);
		TestTrue(TEXT("a new name each time"), !Kept.Contains(Session.AsidePath) && Session.AsidePath.EndsWith(FString::Printf(TEXT(".bad.%d"), Index + 1)));
		Kept.Add(Session.AsidePath);
		TestFalse(TEXT("the file has moved"), IFileManager::Get().FileExists(*Path));
		TestTrue(TEXT("the session starts empty"), DTMeta::IsEmpty(Session.Profile));
		for (int32 Earlier = 0; Earlier <= Index; ++Earlier)
		{
			TestTrue(TEXT("every copy stays byte for byte"), ReadFile(Kept[Earlier]) == Cases[Earlier]);
		}
		Session.Profile.Currency = 1;
		TestTrue(TEXT("the next save makes a fresh file"), Session.Write());
		FDTMetaSession Reader;
		TestEqual(TEXT("which reads"), Reader.Open(Path), EDTMetaOpen::Read);
		IFileManager::Get().Delete(*Path, false, true, true);
	}

#if PLATFORM_WINDOWS
	// A file that cannot be moved sticks the session: it writes nothing, and the file stays.
	const TArray<uint8> Junk = { 9, 9, 9 };
	const FString Held = TempSave(TEXT("Held.sav"));
	TestTrue(TEXT("the held file lands"), WriteFile(Held, Junk));
	{
		TUniquePtr<IFileHandle> Hold(FPlatformFileManager::Get().GetPlatformFile().OpenRead(*Held));
		FDTMetaSession Session;
		TestEqual(TEXT("held open it cannot move"), Session.Open(Held), EDTMetaOpen::Stuck);
		TestFalse(TEXT("a stuck session writes nothing"), Session.Write());
		TestEqual(TEXT("not even at a bank"), Session.Bank(Tally(StepsForMinutes(1)), EDTRunEnd::Left).Retained, static_cast<int64>(DTConst::META_CURRENCY_PER_MINUTE));
	}
	TestTrue(TEXT("the file is untouched"), ReadFile(Held) == Junk);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetAutosave, "Dusktracer.Meta.Autosave", DTMetaTest::Flags) // T-MET-13
bool FDTMetAutosave::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FString Path = TempSave(TEXT("Autosave.sav"));
	FDTMetaSession Session;
	TestEqual(TEXT("no file yet"), Session.Open(Path), EDTMetaOpen::Fresh);
	const FDTRunAward Award = Session.Bank(Tally(StepsForMinutes(3), 0, 1), EDTRunEnd::Death);
	TestTrue(TEXT("the file exists when the bank returns"), IFileManager::Get().FileExists(*Path));
	TestEqual(TEXT("the balance is the award"), Award.Balance, Award.Retained);
	FDTMetaSession Reader;
	TestEqual(TEXT("a reload reads it"), Reader.Open(Path), EDTMetaOpen::Read);
	TestTrue(TEXT("the same profile"), DTMeta::Same(Reader.Profile, Session.Profile));

	const FString Temp = Path + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*Temp, true);
	AddExpectedError(TEXT("Meta:"), EAutomationExpectedErrorFlags::Contains, 0);
	const FDTRunAward Second = Session.Bank(Tally(StepsForMinutes(1)), EDTRunEnd::Left);
	IFileManager::Get().DeleteDirectory(*Temp, false, true);
	const FDTRunAward Third = Session.Bank(Tally(StepsForMinutes(1)), EDTRunEnd::Left);
	TestEqual(TEXT("a failed write loses nothing"), Third.Balance, Award.Retained + Second.Retained + Third.Retained);
	TestEqual(TEXT("a reload sees both runs"), Reader.Open(Path), EDTMetaOpen::Read);
	TestEqual(TEXT("the file has all three"), Reader.Profile.Runs, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetToolRuns, "Dusktracer.Meta.ToolRuns", DTMetaTest::Flags) // T-MET-14
bool FDTMetToolRuns::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	TestTrue(TEXT("a player's process uses the file"), DTMeta::UsesFile(false, false));
	TestFalse(TEXT("a tool run does not"), DTMeta::UsesFile(true, false));
	TestTrue(TEXT("a tool run naming a file does"), DTMeta::UsesFile(true, true));
	TestTrue(TEXT("this very process is a tool run"), UDTMetaSubsystem::IsToolRun());
	TestTrue(TEXT("this very run has no active file"), UDTMetaSubsystem::ActivePath().IsEmpty());

	FDTMetaSession Session;
	TestEqual(TEXT("no path, no save"), Session.Open(FString()), EDTMetaOpen::NoSave);
	const FDTRunAward Award = Session.Bank(Tally(StepsForMinutes(2), 0, 0, true), EDTRunEnd::Victory);
	TestTrue(TEXT("the award is still computed"), Award.bBanked && Award.Retained > 0);
	TestEqual(TEXT("and shown"), Award.Balance, Award.Retained);
	TestTrue(TEXT("the profile stays empty, so wins stay at zero across a reload"), DTMeta::IsEmpty(Session.Profile));
	TestFalse(TEXT("nothing to write"), Session.Write());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetReaders, "Dusktracer.Meta.Readers", DTMetaTest::Flags) // T-MET-15
bool FDTMetReaders::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer")));
	const TArray<FString> Allowed = { TEXT("Boss/DTBossSubsystem.cpp"), TEXT("Settings/DTMenuSubsystem.cpp") };
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.cpp"), true, false, false);
	TestTrue(TEXT("found the source"), Files.Num() > 100);
	int32 Readers = 0;
	for (const FString& File : Files)
	{
		FString Rel = File;
		FPaths::MakePathRelativeTo(Rel, *(Root + TEXT("/")));
		if (Rel.StartsWith(TEXT("Meta/")) || Rel.StartsWith(TEXT("Tests/Meta/")))
		{
			continue;
		}
		FString Text;
		FFileHelper::LoadFileToString(Text, *File);
		if (Text.Contains(TEXT("#include \"Meta/")))
		{
			++Readers;
			TestTrue(FString::Printf(TEXT("%s may read the meta save"), *Rel), Allowed.Contains(Rel));
		}
	}
	TestEqual(TEXT("each listed reader exists"), Readers, Allowed.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetSimUntouched, "Dusktracer.Meta.SimUntouched", DTMetaTest::Flags) // T-MET-16
bool FDTMetSimUntouched::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FRunResult Off = Play(75, false);
	const FRunResult On = Play(75, true);
	TestTrue(TEXT("the run played"), Off.Steps > 0 && !Off.Csv.IsEmpty());
	TestEqual(TEXT("off the clock it counts nothing"), Off.Tally.Steps, 0LL);
	TestEqual(TEXT("on the clock it counts"), On.Tally.Steps, On.Steps);
	TestEqual(TEXT("same steps"), On.Steps, Off.Steps);
	TestEqual(TEXT("same input stream"), On.InputHash, Off.InputHash);
	TestEqual(TEXT("same hero position"), On.Pos, Off.Pos);
	TestEqual(TEXT("same HP"), On.Hp, Off.Hp);
	TestTrue(TEXT("identical CSV"), On.Csv == Off.Csv);
	TestEqual(TEXT("one added record"), CountOf(On.Jsonl, TEXT("\"kind\":\"run_award\"")), 1);
	TestTrue(TEXT("identical JSONL but for it"), Comparable(On.Jsonl) == Comparable(Off.Jsonl));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetWinsReachForm, "Dusktracer.Meta.WinsReachForm", DTMetaTest::Flags) // T-MET-17
bool FDTMetWinsReachForm::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	TestEqual(TEXT("no context reads zero"), UDTMetaSubsystem::GetWins(nullptr), 0);
	{
		FTestWorld Test;
		TestEqual(TEXT("a world with no game instance reads zero"), UDTMetaSubsystem::GetWins(Test.World), 0);
		TestNull(TEXT("and has no session"), UDTMetaSubsystem::FindSession(Test.World));
	}
	TestTrue(TEXT("zero wins is form I"), FDTBossCore::FormForWins(FDTMetaProfile().Wins) == EDTBossForm::I);
	FDTMetaProfile Won;
	Won.Wins = DTConst::BOSS_FORM_II_WINS;
	TestTrue(TEXT("a saved win picks form II"), FDTBossCore::FormForWins(Won.Wins) == EDTBossForm::II);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetModelFields, "Dusktracer.Meta.ModelFields", DTMetaTest::Flags) // T-MET-18
bool FDTMetModelFields::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	TestTrue(TEXT("a default profile is empty"), DTMeta::IsEmpty(FDTMetaProfile()));
	const TArray<TFunction<void(FDTMetaProfile&)>> Edits = FieldEdits();
	for (int32 Index = 0; Index < Edits.Num(); ++Index)
	{
		FDTMetaProfile P;
		Edits[Index](P);
		TestFalse(FString::Printf(TEXT("field %d is in the bytes"), Index), DTMeta::IsEmpty(P));
		TestTrue(FString::Printf(TEXT("field %d is valid"), Index), DTMeta::IsValid(P));
		FDTMetaProfile Read;
		TestTrue(FString::Printf(TEXT("field %d reads"), Index), DTMeta::Deserialize(DTMeta::Serialize(P), Read) && DTMeta::Same(Read, P));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetBankHook, "Dusktracer.Meta.BankHook", DTMetaTest::Flags) // T-MET-19
bool FDTMetBankHook::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FString Path = TempSave(TEXT("Hook.sav"));
	const FName Found(TEXT("Discovery.Test"));
	int32 Calls = 0;
	int64 SeenBalance = 0;
	const auto Hook = [&Calls, &SeenBalance, Found](const FDTRunTally&, FDTMetaProfile& Profile)
	{
		++Calls;
		SeenBalance = Profile.Currency;
		Profile.Discoveries.Add(Found);
	};
	FDTMetaSession Session;
	Session.OnBank.AddLambda(Hook);
	Session.Open(Path);
	const FDTRunAward Award = Session.Bank(Tally(StepsForMinutes(2)), EDTRunEnd::Left);
	TestEqual(TEXT("the hook ran once"), Calls, 1);
	TestEqual(TEXT("after the award"), SeenBalance, Award.Retained);
	FDTMetaSession Reader;
	Reader.Open(Path);
	TestTrue(TEXT("what it added is in the file"), Reader.Profile.Discoveries.Contains(Found));

	FDTMetaSession NoFile;
	NoFile.OnBank.AddLambda(Hook);
	NoFile.Open(FString());
	NoFile.Bank(Tally(StepsForMinutes(2)), EDTRunEnd::Left);
	TestEqual(TEXT("a session with no file calls no hook"), Calls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetDiscoveryCatalog, "Dusktracer.Meta.DiscoveryCatalog", DTMetaTest::Flags) // T-MET-40
bool FDTMetDiscoveryCatalog::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("fifteen rows"), DTCat::NumDiscoveries, 15);
	TSet<FString> Ids;
	FDTRunTally Empty;
	for (const DTCat::FDiscoveryRow& Row : DTCat::Discoveries)
	{
		Ids.Add(Row.Id);
		TestTrue(FString::Printf(TEXT("%s needs a positive count"), Row.Id), Row.Threshold > 0);
		TestFalse(FString::Printf(TEXT("%s is not met by nothing"), Row.Id), DTDiscovery::IsMet(Row, Empty));
		for (const TCHAR* Target : Row.Targets)
		{
			const FString Key(Target);
			TestTrue(FString::Printf(TEXT("%s target %s in tag spelling"), Row.Id, Target), Key.IsEmpty() || Key.StartsWith(TEXT("Weapon.")) || Key.StartsWith(TEXT("Mod.")));
		}
		const FString Name = FText::FromStringTable(TEXT("ST_Discoveries"), FString::Printf(TEXT("Discovery.%s.Name"), Row.Id)).ToString();
		TestFalse(FString::Printf(TEXT("%s has its name"), Row.Id), Name.IsEmpty() || Name.Contains(TEXT("MISSING")));
	}
	TestEqual(TEXT("no id twice"), Ids.Num(), DTCat::NumDiscoveries);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetDiscoveryPlanted, "Dusktracer.Meta.DiscoveryPlanted", DTMetaTest::Flags) // T-MET-41
bool FDTMetDiscoveryPlanted::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	for (const DTCat::FDiscoveryRow& Row : DTCat::Discoveries)
	{
		const FString Counter(Row.Counter);
		const auto At = [&Counter, &Row](int64 Value)
		{
			FDTRunTally T = Tally(1);
			if (Counter == TEXT("Seconds"))
			{
				T.Steps = Value * DTConst::SIM_FIXED_HZ;
			}
			else if (Counter == TEXT("Kills"))
			{
				T.Kills = Value;
			}
			else if (Counter == TEXT("Elites"))
			{
				T.Elites = Value;
			}
			else if (Counter == TEXT("BossKills"))
			{
				T.bBossKilled = Value > 0;
			}
			else
			{
				T.Counters.Add(FName(Row.Counter), Value);
			}
			return T;
		};
		FDTMetaProfile Below;
		DTDiscovery::Apply(At(Row.Threshold - 1), Below);
		TestEqual(FString::Printf(TEXT("%s one short earns nothing"), Row.Id), Below.Discoveries.Num(), 0);
		TestEqual(FString::Printf(TEXT("%s one short unlocks nothing"), Row.Id), Below.Unlocked.Num(), 0);

		FDTMetaProfile Met;
		DTDiscovery::Apply(At(Row.Threshold), Met);
		TestEqual(FString::Printf(TEXT("%s earns itself alone"), Row.Id), Met.Discoveries.Num(), 1);
		TestTrue(FString::Printf(TEXT("%s is in Discoveries"), Row.Id), Met.Discoveries.Contains(FName(Row.Id)));
		TestTrue(FString::Printf(TEXT("%s opens its Unlock key"), Row.Id), Met.Unlocked.Contains(FName(FString::Printf(TEXT("Discovery.%s"), Row.Id))));
		int32 Keys = 1;
		for (const TCHAR* Target : Row.Targets)
		{
			if (Target[0] != TEXT('\0'))
			{
				++Keys;
				TestTrue(FString::Printf(TEXT("%s opens %s"), Row.Id, Target), Met.Unlocked.Contains(FName(Target)));
			}
		}
		TestEqual(FString::Printf(TEXT("%s writes only its keys"), Row.Id), Met.Unlocked.Num(), Keys);
		const TArray<uint8> Once = DTMeta::Serialize(Met);
		DTDiscovery::Apply(At(Row.Threshold), Met);
		TestTrue(FString::Printf(TEXT("%s twice is once"), Row.Id), Once == DTMeta::Serialize(Met));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetDiscoveryBank, "Dusktracer.Meta.DiscoveryBank", DTMetaTest::Flags) // T-MET-42
bool FDTMetDiscoveryBank::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FString Path = TempSave(TEXT("Discovery.sav"));
	const FName Survive(TEXT("Survive10"));
	const FName Best(TEXT("Best.Level"));
	const FName Dashes(TEXT("Dashes"));
	FDTRunTally Long = Tally(StepsForMinutes(10), 2);
	Long.Counters.Add(Best, 12);
	Long.Counters.Add(Dashes, 7);

	FDTMetaSession Session;
	Session.OnBank.AddStatic(&DTDiscovery::Apply);
	Session.Open(Path);
	const FDTRunAward First = Session.Bank(Long, EDTRunEnd::Death);
	TestTrue(TEXT("a new discovery shows, with what the level brought"), First.Discovered.Contains(Survive) && First.Discovered.Contains(FName(TEXT("ReachL10"))));

	FDTMetaSession Reader;
	Reader.OnBank.AddStatic(&DTDiscovery::Apply);
	Reader.Open(Path);
	TestTrue(TEXT("it lands in the same save"), Reader.Profile.Discoveries.Contains(Survive));
	TestTrue(TEXT("its Unlock key and targets too"), Reader.Profile.Unlocked.Contains(FName(TEXT("Discovery.Survive10"))) && Reader.Profile.Unlocked.Contains(FName(TEXT("Weapon.SeekerSwarm"))));

	FDTRunTally Shorter = Long;
	Shorter.Counters[Best] = 3;
	const FDTRunAward Second = Reader.Bank(Shorter, EDTRunEnd::Death);
	TestEqual(TEXT("a repeat shows nothing"), Second.Discovered.Num(), 0);
	TestEqual(TEXT("a Best key keeps its highest"), Reader.Profile.LifetimeStats[Best], 12LL);
	TestEqual(TEXT("a plain counter adds up"), Reader.Profile.LifetimeStats[Dashes], 14LL);

	FDTMetaSession NoFile;
	NoFile.OnBank.AddStatic(&DTDiscovery::Apply);
	NoFile.Open(FString());
	const FDTRunAward Tool = NoFile.Bank(Long, EDTRunEnd::Death);
	TestEqual(TEXT("a tool run earns nothing"), Tool.Discovered.Num(), 0);
	TestEqual(TEXT("and keeps nothing"), NoFile.Profile.Discoveries.Num(), 0);
	TestEqual(TEXT("but the tally still reads"), DTDiscovery::MetBy(Long).Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTMetRunCounters, "Dusktracer.Meta.RunCounters", DTMetaTest::Flags) // T-MET-43
bool FDTMetRunCounters::RunTest(const FString& Parameters)
{
	using namespace DTMetaTest;
	const FRunResult Run = Play(76, true);
	const auto Counter = [&Run](const TCHAR* Key)
	{
		const int64* Found = Run.Tally.Counters.Find(FName(Key));
		return Found ? *Found : 0LL;
	};
	int64 ByRole = 0;
	int64 ByWeapon = 0;
	for (const TPair<FName, int64>& Pair : Run.Tally.Counters)
	{
		const FString Key = Pair.Key.ToString();
		ByRole += Key.StartsWith(TEXT("Kills.Role.")) ? Pair.Value : 0;
		ByWeapon += Key.StartsWith(TEXT("Kills.Weapon.")) ? Pair.Value : 0;
	}
	TestTrue(TEXT("the run killed"), Run.Tally.Kills > 0);
	TestEqual(TEXT("roles add up to the kills"), ByRole, Run.Tally.Kills);
	TestTrue(TEXT("weapons name kills, and none twice"), ByWeapon > 0 && ByWeapon <= Run.Tally.Kills);
	TestEqual(TEXT("dashes are the hero's"), Counter(TEXT("Dashes")), static_cast<int64>(Run.Dashes));
	TestEqual(TEXT("level is the player's"), Counter(TEXT("Best.Level")), static_cast<int64>(Run.Level));
	TestTrue(TEXT("a weapon has a level"), Counter(TEXT("Best.WeaponLevel")) >= 1);

	FDTRunState State;
	State.Start(1, 1);
	FDTEnemyPool Pool;
	const int32 Enemy = Pool.Spawn(EDTEnemyRole::Tank, FVector2f::ZeroVector);
	FDTHit Hit;
	Hit.EnemyIndex = Enemy;
	Hit.OwnerId = 0;
	Hit.Damage = 2.0f;
	Hit.bPierce = true;
	FDTHit Crit = Hit;
	Crit.Damage = 3.0f;
	Crit.bCrit = true;
	Hit.Damage = 9.0f;
	const FDTHit Hits[] = { Hit, Crit };
	Pool.ApplyHits(Hits, State);
	TestEqual(TEXT("only a critical hit is a crit's best"), Pool.GetCounters().BestCrit, 3.0f);

	FDTStatusApply Freeze;
	Freeze.Handle = Pool.GetHandle(Enemy);
	Freeze.Status = EDTStatus::Freeze;
	Freeze.OwnerId = 0;
	Freeze.FreezeScale = 1.0f;
	Pool.ApplyStatus(Freeze);
	Pool.ApplyStatus(Freeze);
	TestEqual(TEXT("a freeze counts, its refresh does not"), Pool.GetCounters().FreezesBegun, 1LL);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
