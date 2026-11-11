// The audio runtime (02_Systems/audio.md T-AUD-01, 02, 03, 05, 06, 07, 09, 10, 11, 20, 21).
// Run headless: Tools\RunTests.ps1 -Filter Dusktracer.Audio

#include "Misc/AutomationTest.h"
#include "Algo/AnyOf.h"
#include "Audio/DTAudioScheduler.h"
#include "Audio/DTAudioSubsystem.h"
#include "Audio/DTAudioTypes.h"
#include "Audio/DTStingerQueue.h"
#include "Audio/Observe/DTObserveInterface.h"
#include "Bot/DTBotSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Offers/DTOfferSubsystem.h"
#include "Internationalization/Regex.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTAudioTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr int32 BotSeconds = 45;
	using DTCat::ESoundClass;
	using DTCat::ESoundPitch;
	using DTCat::ESoundSide;

	static DTCat::FSoundRow Row(const TCHAR* Id, ESoundSide Side, int32 Variants, float Volume, ESoundPitch Pitch, int32 IntervalMs, int32 Voices, float Agg,
		bool bGrid, int32 LengthMs)
	{
		const ESoundClass Class = Side == ESoundSide::Danger ? ESoundClass::Danger : (Side == ESoundSide::UI ? ESoundClass::UI : ESoundClass::Player);
		return DTCat::FSoundRow{ Id, TEXT("/Game/DT/Audio/SFX/SW_Test"), Side, Class, Variants, false, Volume, Pitch, IntervalMs, Voices, Agg, bGrid, LengthMs };
	}

	/** The staged trace's rows: ten weapons, hits, kills, a warning, an interface sound (RogueHero's table as the start, plan §3.4). */
	enum ETraceRow : int32 { Weapon0 = 0, Hit = 10, Kill, Warn, Ui, NumTraceRows };

	static TArray<DTCat::FSoundRow> TraceRows()
	{
		TArray<DTCat::FSoundRow> Rows;
		for (int32 Index = 0; Index < 10; ++Index)
		{
			Rows.Add(Row(TEXT("Snd.Test.Fire"), ESoundSide::Player, 4, 0.3f, ESoundPitch::Key, 70, 2, 0.15f, true, 150));
		}
		Rows.Add(Row(TEXT("Snd.Test.Hit"), ESoundSide::Player, 4, 0.4f, ESoundPitch::Key, 35, 4, 0.15f, true, 80));
		Rows.Add(Row(TEXT("Snd.Test.Kill"), ESoundSide::Player, 4, 0.5f, ESoundPitch::Key, 45, 5, 0.2f, false, 110));
		Rows.Add(Row(TEXT("Snd.Test.Warn"), ESoundSide::Danger, 1, 0.8f, ESoundPitch::Fixed, DTConst::AUD_WARN_MIN_INTERVAL_MS, 2, 0.25f, false, 150));
		Rows.Add(Row(TEXT("Snd.Test.Ui"), ESoundSide::UI, 1, 0.6f, ESoundPitch::Fixed, 40, 2, 0.0f, false, 60));
		return Rows;
	}

	/** One step of the density trace (plan §6.1): the caps' worst case at C.ENEMIES.CAP enemies and C.BULLETS.CAP rounds. */
	static void TraceStep(FDTAudioScheduler& S, int64 Step)
	{
		for (int32 Weapon = 0; Weapon < 10; ++Weapon)
		{
			S.Request(Weapon0 + Weapon, 2, Step);                          // ten weapons, each firing every step
		}
		S.Request(Hit, DTConst::BULLETS_CAP / DTConst::SIM_FIXED_HZ, Step);  // thousands of hits a second
		S.Request(Kill, 1 + static_cast<int32>(Step % 3 == 0), Step);      // about a hundred kills a second (of C.ENEMIES.CAP enemies)
		if (Step % 30 == 0)
		{
			S.Request(Warn, 3, Step);
		}
		if (Step % 120 == 0)
		{
			S.Request(Ui, 1, Step);
		}
	}

	static double SecondsOf(int64 Step) { return static_cast<double>(Step) / DTConst::SIM_FIXED_HZ; }

	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTAudioTestWorld")));
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

	struct FEnabledGuard
	{
		IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("dt.Audio.Enabled"));
		int32 Was = Var ? Var->GetInt() : 1;
		void Set(int32 Value) { if (Var) { Var->Set(Value, ECVF_SetByCode); } }
		~FEnabledGuard() { Set(Was); }
	};

	struct FRunResult
	{
		uint32 InputHash = 0;
		int64 Steps = 0;
		FVector2f Pos = FVector2f::ZeroVector;
		float Hp = 0.0f;
		FString Csv;
		FString Jsonl;
		int64 AudioAsked = 0;
		bool bObserving = false;
		int32 Decisions = 0;
	};

	static FString StripWall(const FString& In)
	{
		const FRegexPattern Pattern(TEXT(",?\"wall[a-z_]*\":(\"([^\"\\\\]|\\\\.)*\"|[-+0-9.eE]+)"));
		FRegexMatcher Matcher(Pattern, In);
		FString Out;
		int32 Last = 0;
		while (Matcher.FindNext())
		{
			Out += In.Mid(Last, Matcher.GetMatchBeginning() - Last);
			Last = Matcher.GetMatchEnding();
		}
		Out += In.Mid(Last);
		return Out;
	}

	/** One bot run of BotSeconds on Seed; the audio runtime runs a frame every second of sim time when it is on. */
	static FRunResult Play(int32 Seed)
	{
		FRunResult Result;
		FTestWorld Test;
		UDTSimSubsystem* Sim = Test.World->GetSubsystem<UDTSimSubsystem>();
		UDTBotSubsystem* Bot = Test.World->GetSubsystem<UDTBotSubsystem>();
		UDTTelemetrySubsystem* Telemetry = Test.World->GetSubsystem<UDTTelemetrySubsystem>();
		UDTDirector* Director = Test.World->GetSubsystem<UDTDirector>();
		const UDTHeroSubsystem* Heroes = Test.World->GetSubsystem<UDTHeroSubsystem>();
		UDTAudioSubsystem* Audio = Test.World->GetSubsystem<UDTAudioSubsystem>();
		if (Sim && Bot && Telemetry && Director && Heroes && Audio)
		{
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
			Audio->Tick(0.0f); // binds the observer when the runtime is on
			while (Sim->GetStepIndex() < Steps && !Bot->IsRunFinished())
			{
				const int32 Chunk = FMath::Min<int32>(DTConst::SIM_FIXED_HZ, Steps - static_cast<int32>(Sim->GetStepIndex()));
				if (Bot->RunSteps(Chunk) == 0)
				{
					break;
				}
				if (UDTAudioSubsystem::IsEnabled())
				{
					Audio->RunFrame(SecondsOf(Sim->GetStepIndex()));
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
			if (const FDTHeroState* State = Heroes->GetState(0))
			{
				Result.Pos = State->Pos;
				Result.Hp = State->Hp;
			}
			for (int32 Sound = 0; Sound < Audio->GetScheduler().NumSounds(); ++Sound)
			{
				Result.AudioAsked += Audio->GetScheduler().GetCounts(Sound).Asked;
			}
			Result.bObserving = Audio->IsObserving();
			if (const UDTOfferSubsystem* Offers = Test.World->GetSubsystem<UDTOfferSubsystem>())
			{
				Result.Decisions = Offers->GetStats(0) ? Offers->GetStats(0)->Decisions : -1;
			}
			Bot->Deactivate();
		}
		return Result;
	}

	static bool Settled(const FDTAudioScheduler& S, int32 Sound)
	{
		const FDTAudioCounts& C = S.GetCounts(Sound);
		return C.Asked == C.Played + C.Merged + C.Dropped;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestSimUntouched, "Dusktracer.Audio.SimUntouched", DTAudioTest::Flags) // T-AUD-01
bool FDTAudTestSimUntouched::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	FEnabledGuard Enabled;
	Enabled.Set(1);
	const FRunResult On = Play(31);
	Enabled.Set(0);
	const FRunResult Off = Play(31);
	TestTrue(TEXT("the run with audio played"), On.Steps > 0 && !On.Csv.IsEmpty());
	AddInfo(FString::Printf(TEXT("audio on: observing %d, asked %lld, decisions %d; off: observing %d, asked %lld"), On.bObserving, On.AudioAsked, On.Decisions, Off.bObserving, Off.AudioAsked));
	TestTrue(TEXT("audio observed and was asked for sounds (the picks)"), On.bObserving && On.AudioAsked > 0);
	TestTrue(TEXT("audio off observed nothing"), !Off.bObserving && Off.AudioAsked == 0);
	TestEqual(TEXT("same steps"), Off.Steps, On.Steps);
	TestEqual(TEXT("same input stream"), Off.InputHash, On.InputHash);
	TestEqual(TEXT("same hero position"), Off.Pos, On.Pos);
	TestEqual(TEXT("same HP"), Off.Hp, On.Hp);
	TestEqual(TEXT("identical telemetry CSV"), Off.Csv, On.Csv);
	TestTrue(TEXT("identical telemetry JSONL except wall fields"), StripWall(Off.Jsonl) == StripWall(On.Jsonl));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestObserver, "Dusktracer.Audio.Observer", DTAudioTest::Flags) // T-AUD-02
bool FDTAudTestObserver::RunTest(const FString& Parameters)
{
	using DTCat::ESoundId;
	TArray<ESoundId> Raised;
	FDTInterfaceView Was;
	const auto Step = [&](const FDTInterfaceView& Now) -> TArray<ESoundId>
	{
		Raised.Reset();
		DTInterfaceEvents::Diff(Was, Now, [&](ESoundId Sound, int32 Count) { for (int32 Index = 0; Index < Count; ++Index) { Raised.Add(Sound); } });
		Was = Now;
		return Raised;
	};
	const auto One = [](ESoundId Sound) { return TArray<ESoundId>{ Sound }; };
	const auto Expect = [this](const TCHAR* What, const TArray<ESoundId>& Got, const TArray<ESoundId>& Want)
	{
		TestTrue(FString::Printf(TEXT("%s (raised %d, expected %d)"), What, Got.Num(), Want.Num()), Got == Want);
	};
	FDTInterfaceView V;
	Expect(TEXT("nothing shown, nothing raised"), Step(V), TArray<ESoundId>());

	V.Screen = EDTMenuScreen::Pause;
	Expect(TEXT("the pause screen opens"), Step(V), One(ESoundId::Snd_UI_Open));
	Expect(TEXT("the same frame again raises nothing"), Step(V), TArray<ESoundId>());
	V.MenuFocus = 2;
	Expect(TEXT("the menu focus moves"), Step(V), One(ESoundId::Snd_UI_Focus));
	V.Screen = EDTMenuScreen::Settings;
	V.MenuFocus = 0;
	Expect(TEXT("settings opens deeper"), Step(V), One(ESoundId::Snd_UI_Open));
	V.Screen = EDTMenuScreen::Controls;
	Expect(TEXT("controls, as deep, opens"), Step(V), One(ESoundId::Snd_UI_Open));
	V.MenuColumn = EDTBindDevice::Pad;
	Expect(TEXT("the controls column moves the focus"), Step(V), One(ESoundId::Snd_UI_Focus));
	V.MenuStatus = EDTMenuStatus::Reserved;
	Expect(TEXT("a reserved key is refused"), Step(V), One(ESoundId::Snd_UI_Refuse));
	V.MenuStatus = EDTMenuStatus::Bound;
	Expect(TEXT("a binding taken raises nothing"), Step(V), TArray<ESoundId>());
	V.MenuStatus = EDTMenuStatus::WrongDevicePad;
	Expect(TEXT("a key of the wrong device is refused"), Step(V), One(ESoundId::Snd_UI_Refuse));
	V.Screen = EDTMenuScreen::Pause;
	Expect(TEXT("back to the pause screen"), Step(V), One(ESoundId::Snd_UI_Back));
	V.Screen = EDTMenuScreen::None;
	Expect(TEXT("closing is Back"), Step(V), One(ESoundId::Snd_UI_Back));
	V.Screen = EDTMenuScreen::RunOver;
	Expect(TEXT("the run-over screen opens"), Step(V), One(ESoundId::Snd_UI_Open));
	V.Screen = EDTMenuScreen::None;
	Step(V);

	V.bHub = true;
	Expect(TEXT("the hub opens"), Step(V), One(ESoundId::Snd_UI_Open));
	V.HubFocus = 3;
	Expect(TEXT("the hub focus moves"), Step(V), One(ESoundId::Snd_UI_Focus));
	V.HubPage = EDTHubPage::Tree;
	V.HubFocus = 0;
	Expect(TEXT("a hub page opens"), Step(V), One(ESoundId::Snd_UI_Open));
	V.HubPage = EDTHubPage::Root;
	Expect(TEXT("back to the hub root"), Step(V), One(ESoundId::Snd_UI_Back));
	V.bHub = false;
	Expect(TEXT("leaving the hub raises nothing"), Step(V), TArray<ESoundId>());

	V.HandId = 5;
	Expect(TEXT("a hand opens"), Step(V), One(ESoundId::Snd_UI_HandOpen));
	V.HandFocus = 1;
	Expect(TEXT("the hand focus moves"), Step(V), One(ESoundId::Snd_UI_Focus));
	V.HandId = 6;
	V.RerollOf = 5;
	V.HandFocus = 0;
	V.Rerolls = 1;
	Expect(TEXT("a reroll is Reroll, not a new hand"), Step(V), One(ESoundId::Snd_UI_Reroll));
	V.Banishes = 1;
	Expect(TEXT("a banish"), Step(V), One(ESoundId::Snd_UI_Banish));
	V.Locks = 1;
	Expect(TEXT("a lock"), Step(V), One(ESoundId::Snd_UI_Lock));
	V.Decisions = 1;
	V.HandId = 7;
	V.RerollOf = INDEX_NONE;
	Expect(TEXT("a pick, and the next hand opens"), Step(V), (TArray<ESoundId>{ ESoundId::Snd_UI_HandOpen, ESoundId::Snd_UI_Pick }));
	V.Decisions = 3;
	V.HandId = INDEX_NONE;
	Expect(TEXT("two picks in one frame raise two"), Step(V), (TArray<ESoundId>{ ESoundId::Snd_UI_Pick, ESoundId::Snd_UI_Pick }));

	// A world with nothing open reads as nothing shown.
	{
		DTAudioTest::FTestWorld Test;
		const FDTInterfaceView Live = DTInterfaceEvents::Read(*Test.World);
		TestTrue(TEXT("a fresh world shows no screen and no hand"), Live.Screen == EDTMenuScreen::None && Live.HandId == INDEX_NONE && !Live.bHub);
	}

	// Never a take-once queue (R-AUD-02): no file of Audio/ names one.
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer"), TEXT("Audio")), TEXT("*.*"), true, false);
	TestTrue(TEXT("the audio sources are readable"), Files.Num() >= 8);
	for (const FString& File : Files)
	{
		FString Text;
		FFileHelper::LoadFileToString(Text, *File);
		for (const TCHAR* Take : { TEXT("TakeEmissions"), TEXT("TakeReactionEvents"), TEXT("TakeStartedCasts"), TEXT("TakeFinishedCasts"), TEXT("TakeOutput") })
		{
			TestFalse(FString::Printf(TEXT("%s calls no %s"), *FPaths::GetCleanFilename(File), Take), Text.Contains(Take));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestSilent, "Dusktracer.Audio.Silent", DTAudioTest::Flags) // T-AUD-03
bool FDTAudTestSilent::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("the tests run unattended, as every tool run does"), UDTAudioSubsystem::IsSilentProcess());
	const float Was = FApp::GetVolumeMultiplier();
	{
		DTAudioTest::FTestWorld Test;
		UDTAudioSubsystem* Audio = Test.World->GetSubsystem<UDTAudioSubsystem>();
		if (!TestNotNull(TEXT("a game world has the audio runtime"), Audio))
		{
			return false;
		}
		TestFalse(TEXT("lock 1: no engine sink was made"), Audio->HasEngineSink());
		FApp::SetVolumeMultiplier(1.0f); // what the settings subsystem sets at its own start
		Audio->Tick(0.0f);
		TestEqual(TEXT("lock 2: the application volume reads zero"), FApp::GetVolumeMultiplier(), 0.0f);
		Audio->RequestUi(static_cast<int32>(DTCat::ESoundId::Snd_UI_Open), 1);
		Audio->RunFrame(1.0);
		const FDTAudioCounts& C = Audio->GetScheduler().GetCounts(static_cast<int32>(DTCat::ESoundId::Snd_UI_Open));
		TestTrue(TEXT("the scheduler still runs and counts"), C.Asked == 1 && C.Played == 1);
		TestEqual(TEXT("no voice reached the engine"), Audio->GetEngineStarts(), int64(0));
		TestEqual(TEXT("the volume holds at zero"), FApp::GetVolumeMultiplier(), 0.0f);
	}
	FApp::SetVolumeMultiplier(Was);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestDangerFirst, "Dusktracer.Audio.DangerFirst", DTAudioTest::Flags) // T-AUD-05
bool FDTAudTestDangerFirst::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	const TArray<DTCat::FSoundRow> Rows = {
		Row(TEXT("Snd.Test.Player"), ESoundSide::Player, 1, 0.5f, ESoundPitch::Fixed, 0, 64, 0.0f, false, 4000),
		Row(TEXT("Snd.Test.Danger"), ESoundSide::Danger, 1, 0.8f, ESoundPitch::Fixed, 0, 64, 0.0f, true, 1000),
	};
	FDTAudioScheduler S;
	S.Configure(Rows, 7);
	TArray<FDTAudioVoice> Out;
	int64 Step = 1;
	// Every player limit full: player voices asked for every step until they hold every slot they may take.
	for (; Step < 40; ++Step)
	{
		S.Request(0, 1, Step);
		Out.Reset();
		S.Frame(SecondsOf(Step), Step, Out);
	}
	const int32 PlayerCap = DTConst::AUD_VOICES_MAX - DTConst::AUD_VOICES_DANGER - DTConst::AUD_VOICES_UI;
	TestEqual(TEXT("player voices stop at the slots not kept"), S.NumAlive(ESoundSide::Player), PlayerCap);
	TestEqual(TEXT("the duck rests at zero"), S.GetDuckDb(), 0.0f);

	// A warning with the frame's cap already asked for by player voices: it starts in its own frame, first, off the grid.
	for (int32 Index = 0; Index < 8; ++Index)
	{
		S.Request(0, 1, Step);
	}
	S.Request(1, 1, Step);
	Out.Reset();
	const double T0 = SecondsOf(Step);
	S.Frame(T0, Step, Out);
	TestTrue(TEXT("the warning starts in the frame it is asked for, first"), Out.Num() >= 1 && Out[0].Sound == 1 && Out[0].Step == Step);
	TestTrue(TEXT("never varied"), Out.Num() >= 1 && Out[0].Pitch == 1.0f && Out[0].Volume == 0.8f);
	TestTrue(TEXT("the frame cap holds"), Out.Num() <= DTConst::AUD_STARTS_PER_FRAME_MAX);

	// The duck: down to C.AUD.DUCK_DB within C.AUD.DUCK_ATTACK_MS, held while it sounds, back over C.AUD.DUCK_RELEASE_MS.
	const double Attack = DTConst::AUD_DUCK_ATTACK_MS / 1000.0;
	const double Release = DTConst::AUD_DUCK_RELEASE_MS / 1000.0;
	const double End = T0 + Rows[1].LengthMs / 1000.0;
	TestTrue(TEXT("halfway down at half the attack"), FMath::IsNearlyEqual(S.DuckDbAt(T0 + Attack / 2), DTConst::AUD_DUCK_DB / 2.0f, 0.01f));
	TestTrue(TEXT("down at the attack's end"), FMath::IsNearlyEqual(S.DuckDbAt(T0 + Attack), static_cast<float>(DTConst::AUD_DUCK_DB), 0.01f));
	TestTrue(TEXT("held while the warning sounds"), FMath::IsNearlyEqual(S.DuckDbAt(End - 0.01), static_cast<float>(DTConst::AUD_DUCK_DB), 0.01f));
	TestTrue(TEXT("halfway back at half the release"), FMath::IsNearlyEqual(S.DuckDbAt(End + Release / 2), DTConst::AUD_DUCK_DB / 2.0f, 0.01f));
	TestTrue(TEXT("back at zero after the release"), FMath::IsNearlyEqual(S.DuckDbAt(End + Release + 0.01), 0.0f, 0.001f));

	// No slot left: danger takes the oldest player voice's slot, so no danger voice is dropped while a player voice holds one.
	for (int32 Index = 0; Index < DTConst::AUD_VOICES_MAX - DTConst::AUD_VOICES_DANGER; ++Index)
	{
		++Step;
		S.Request(1, 1, Step);
		Out.Reset();
		S.Frame(SecondsOf(Step), Step, Out);
	}
	TestTrue(TEXT("danger took player slots"), S.GetStolen() > 0);
	TestTrue(TEXT("the voice cap holds"), S.NumAlive() <= DTConst::AUD_VOICES_MAX);
	TestEqual(TEXT("no danger voice dropped"), S.GetCounts(1).Dropped, int64(0));
	TestEqual(TEXT("every warning played"), S.GetCounts(1).Played, S.GetCounts(1).Asked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestAggregate, "Dusktracer.Audio.Aggregate", DTAudioTest::Flags) // T-AUD-06
bool FDTAudTestAggregate::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	const TArray<DTCat::FSoundRow> Rows = { Row(TEXT("Snd.Test.Hit"), ESoundSide::Player, 1, 0.5f, ESoundPitch::Fixed, 100, 8, 0.25f, false, 50) };
	FDTAudioScheduler S;
	S.Configure(Rows, 1);
	TArray<FDTAudioVoice> Out;
	S.Request(0, 5, 10);
	S.Request(0, 3, 10);
	S.Frame(SecondsOf(10), 10, Out);
	TestEqual(TEXT("a batch of one step is one voice"), Out.Num(), 1);
	TestTrue(TEXT("louder by the aggregation rule, capped at C.AUD.AGGREGATE_MAX"), Out.Num() == 1 && FMath::IsNearlyEqual(Out[0].Volume, 0.5f * FMath::Min(1.0f + 0.25f * 3.0f, DTConst::AUD_AGGREGATE_MAX)));
	TestTrue(TEXT("it carries eight"), Out.Num() == 1 && Out[0].Count == 8);
	Out.Reset();
	S.Request(0, 4, 13); // 50 ms later, inside 100 ms
	S.Frame(SecondsOf(13), 13, Out);
	TestEqual(TEXT("a request inside the interval merges"), Out.Num(), 0);
	Out.Reset();
	S.Request(0, 2, 16); // 100 ms later: out of it
	S.Frame(SecondsOf(16), 16, Out);
	TestTrue(TEXT("after the interval a new voice, louder by log2(2)"), Out.Num() == 1 && FMath::IsNearlyEqual(Out[0].Volume, 0.5f * 1.25f));
	const FDTAudioCounts& C = S.GetCounts(0);
	TestEqual(TEXT("asked"), C.Asked, int64(14));
	TestEqual(TEXT("played"), C.Played, int64(2));
	TestEqual(TEXT("merged: seven into the first, four inside the interval, one into the second"), C.Merged, int64(12));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestDensity, "Dusktracer.Audio.Density", DTAudioTest::Flags) // T-AUD-07
bool FDTAudTestDensity::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	FDTAudioScheduler S;
	S.Configure(TraceRows(), 9107);
	TArray<FDTAudioVoice> Out;
	const int32 PlayerCap = DTConst::AUD_VOICES_MAX - DTConst::AUD_VOICES_DANGER - DTConst::AUD_VOICES_UI;
	int32 WorstAlive = 0, WorstPlayer = 0, WorstSecond = 0, WorstFrame = 0;
	TArray<int64> PlayerStartSteps;
	int32 WorstSlidingSecond = 0;
	for (int64 Step = 1; Step <= 20 * DTConst::SIM_FIXED_HZ; ++Step)
	{
		TraceStep(S, Step);
		Out.Reset();
		S.Frame(SecondsOf(Step), Step, Out);
		WorstAlive = FMath::Max(WorstAlive, S.NumAlive());
		WorstPlayer = FMath::Max(WorstPlayer, S.NumAlive(ESoundSide::Player));
		WorstSecond = FMath::Max(WorstSecond, S.GetPlayerStartsLastSecond());
		WorstFrame = FMath::Max(WorstFrame, Out.Num());
		for (const FDTAudioVoice& Voice : Out)
		{
			if (Voice.Sound != Warn && Voice.Sound != Ui)
			{
				PlayerStartSteps.Add(Voice.Step);
			}
		}
		PlayerStartSteps.RemoveAll([Step](int64 Started) { return Started <= Step - DTConst::SIM_FIXED_HZ; });
		WorstSlidingSecond = FMath::Max(WorstSlidingSecond, PlayerStartSteps.Num());
	}
	TestTrue(TEXT("the voice cap holds"), WorstAlive <= DTConst::AUD_VOICES_MAX);
	TestTrue(TEXT("player voices keep off the kept slots"), WorstPlayer <= PlayerCap);
	TestTrue(TEXT("player starts in any second at most the cap"), WorstSecond <= DTConst::AUD_PLAYER_STARTS_PER_S_MAX && WorstSlidingSecond <= DTConst::AUD_PLAYER_STARTS_PER_S_MAX);
	TestTrue(TEXT("starts in any frame at most the cap"), WorstFrame <= DTConst::AUD_STARTS_PER_FRAME_MAX);
	TestTrue(TEXT("the trace was dense: the caps were reached"), WorstSlidingSecond >= DTConst::AUD_PLAYER_STARTS_PER_S_MAX / 2 && S.GetCounts(Hit).Dropped > 0);
	TestEqual(TEXT("no warning dropped"), S.GetCounts(Warn).Dropped, int64(0));
	TestTrue(TEXT("every warning batch played"), S.GetCounts(Warn).Played == 20 * DTConst::SIM_FIXED_HZ / 30);
	AddInfo(FString::Printf(TEXT("worst: %d voices, %d player, %d player starts in a second, %d starts in a frame"), WorstAlive, WorstPlayer, WorstSlidingSecond, WorstFrame));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestGrid, "Dusktracer.Audio.Grid", DTAudioTest::Flags) // T-AUD-09
bool FDTAudTestGrid::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	const TArray<DTCat::FSoundRow> Rows = {
		Row(TEXT("Snd.Test.Fire"), ESoundSide::Player, 1, 0.5f, ESoundPitch::Fixed, 0, 4, 0.0f, true, 50),
		Row(TEXT("Snd.Test.Kill"), ESoundSide::Player, 1, 0.5f, ESoundPitch::Fixed, 0, 4, 0.0f, false, 50),
		Row(TEXT("Snd.Test.Warn"), ESoundSide::Danger, 1, 0.8f, ESoundPitch::Fixed, 0, 4, 0.0f, true, 50),
	};
	FDTAudioScheduler S;
	S.Configure(Rows, 1);
	const int64 Grid = DTConst::AUD_GRID_STEPS;
	const int64 Asked = 2 * Grid + 1;
	const int64 Due = 3 * Grid;
	S.Request(0, 1, Asked);
	S.Request(1, 1, Asked);
	S.Request(2, 1, Asked);
	TArray<FDTAudioVoice> Out;
	for (int64 Step = Asked; Step <= Due; ++Step)
	{
		TArray<FDTAudioVoice> Frame;
		S.Frame(SecondsOf(Step), Step, Frame);
		for (const FDTAudioVoice& Voice : Frame)
		{
			Out.Add(Voice);
			if (Voice.Sound == 0)
			{
				TestEqual(TEXT("the grid voice starts on the grid step"), Step, Due);
			}
			else
			{
				TestEqual(TEXT("a voice off the grid starts when asked"), Step, Asked);
			}
		}
	}
	const FDTAudioVoice* Fire = Out.FindByPredicate([](const FDTAudioVoice& Voice) { return Voice.Sound == 0; });
	const FDTAudioVoice* Warn = Out.FindByPredicate([](const FDTAudioVoice& Voice) { return Voice.Sound == 2; });
	TestTrue(TEXT("the grid voice is logged on the grid step"), Fire && Fire->Step == Due);
	TestTrue(TEXT("a danger voice never waits, grid or not"), Warn && Warn->Step == Asked);
	TestEqual(TEXT("three voices"), Out.Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestVariation, "Dusktracer.Audio.Variation", DTAudioTest::Flags) // T-AUD-10
bool FDTAudTestVariation::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	const TArray<DTCat::FSoundRow> Rows = {
		Row(TEXT("Snd.Test.Key"), ESoundSide::Player, 3, 0.5f, ESoundPitch::Key, 0, 64, 0.0f, false, 10),
		Row(TEXT("Snd.Test.Fixed"), ESoundSide::UI, 2, 0.6f, ESoundPitch::Fixed, 0, 64, 0.0f, false, 10),
		Row(TEXT("Snd.Test.Warn"), ESoundSide::Danger, 1, 0.8f, ESoundPitch::Key, 0, 64, 0.0f, false, 10),
	};
	const auto Run = [&](uint64 Seed)
	{
		FDTAudioScheduler S;
		S.Configure(Rows, Seed);
		TArray<FDTAudioVoice> All;
		for (int64 Step = 1; Step <= 40; ++Step)
		{
			S.Request(0, 1, Step);
			S.Request(2, 1, Step);
			if (Step % 4 == 0)
			{
				S.RequestUi(1, 1);
			}
			S.Frame(SecondsOf(Step), Step, All);
		}
		return All;
	};
	const TArray<FDTAudioVoice> A = Run(42);
	TSet<float> Pitches;
	int32 Last = INDEX_NONE;
	for (const FDTAudioVoice& Voice : A)
	{
		if (Voice.Sound == 0)
		{
			TestTrue(TEXT("no variant twice running"), Voice.Variant != Last);
			Last = Voice.Variant;
			const bool bDegree = Algo::AnyOf(DTAudio::KeyDegrees, [&](int32 Degree) { return FMath::IsNearlyEqual(Voice.Pitch, FMath::Pow(2.0f, Degree / 12.0f), 1.0e-5f); });
			TestTrue(TEXT("the pitch is a degree of the key"), bDegree);
			const float Db = 20.0f * FMath::LogX(10.0f, Voice.Volume / 0.5f);
			TestTrue(TEXT("the level within C.AUD.LEVEL_JITTER_DB"), FMath::Abs(Db) <= DTConst::AUD_LEVEL_JITTER_DB + 1.0e-4f);
			Pitches.Add(Voice.Pitch);
		}
		else
		{
			TestEqual(TEXT("a fixed row and a warning never vary in pitch"), Voice.Pitch, 1.0f);
			TestEqual(TEXT("nor in level"), Voice.Volume, Rows[Voice.Sound].Volume);
		}
	}
	TestTrue(TEXT("degrees are drawn, not one"), Pitches.Num() >= 3);
	const TArray<FDTAudioVoice> B = Run(42);
	bool bSame = A.Num() == B.Num();
	for (int32 Index = 0; bSame && Index < A.Num(); ++Index)
	{
		bSame = A[Index].Sound == B[Index].Sound && A[Index].Variant == B[Index].Variant && A[Index].Pitch == B[Index].Pitch && A[Index].Volume == B[Index].Volume;
	}
	TestTrue(TEXT("one seed, one voice log"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestStingers, "Dusktracer.Audio.Stingers", DTAudioTest::Flags) // T-AUD-11
bool FDTAudTestStingers::RunTest(const FString& Parameters)
{
	const double Gap = DTConst::AUD_STINGER_GAP_S;
	const double Expiry = DTConst::AUD_STINGER_EXPIRY_S;
	FDTStingerQueue Q;
	FDTStingerQueue::FEntry E;
	TestTrue(TEXT("a pick queues"), Q.Push(0, EDTStinger::Pick, 0.0));
	TestTrue(TEXT("another Path's pick queues"), Q.Push(1, EDTStinger::Pick, 0.01));
	TestTrue(TEXT("the oldest starts"), Q.Pop(0.02, E) && E.Path == 0);
	TestFalse(TEXT("nothing inside the gap"), Q.Pop(0.02 + Gap * 0.5, E));
	TestTrue(TEXT("the next one after the gap"), Q.Pop(0.02 + Gap, E) && E.Path == 1);

	Q.Reset();
	Q.Push(2, EDTStinger::Pick, 10.0);
	TestFalse(TEXT("an entry past its expiry is dropped"), Q.Pop(10.0 + Expiry + 0.01, E));
	TestEqual(TEXT("and counted"), Q.Num(), 0);

	Q.Reset();
	Q.Push(3, EDTStinger::Pick, 20.0);
	Q.Push(3, EDTStinger::Tier, 20.0);
	TestFalse(TEXT("a pick behind its Path's tier is dropped"), Q.Push(3, EDTStinger::Pick, 20.0));
	TestEqual(TEXT("the tier dropped the pending pick of its Path"), Q.Num(), 1);
	Q.Push(4, EDTStinger::Pick, 19.9);
	TestTrue(TEXT("the tier goes first, older picks or not"), Q.Pop(20.1, E) && E.Kind == EDTStinger::Tier);

	// Under full load: one per Path waits, more are dropped; still one per gap.
	Q.Reset();
	const int64 DroppedBefore = Q.GetDropped();
	for (int32 Path = 0; Path < FDTStingerQueue::NumPaths; ++Path)
	{
		TestTrue(TEXT("one per Path waits"), Q.Push(Path, EDTStinger::Pick, 30.0));
	}
	TestFalse(TEXT("past capacity is dropped"), Q.Push(0, EDTStinger::PassedOver, 30.0));
	TestEqual(TEXT("counted"), Q.GetDropped(), DroppedBefore + 1);
	int32 Started = 0;
	for (double T = 30.0; T < 30.0 + Expiry; T += 1.0 / 60.0)
	{
		Started += Q.Pop(T, E) ? 1 : 0;
	}
	TestTrue(TEXT("at most one per gap within the expiry"), Started <= FMath::FloorToInt(Expiry / Gap) + 1 && Started >= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestCounts, "Dusktracer.Audio.Counts", DTAudioTest::Flags) // T-AUD-20
bool FDTAudTestCounts::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	FDTAudioScheduler S;
	S.Configure(TraceRows(), 5);
	TArray<FDTAudioVoice> Out;
	int64 Step = 1;
	for (; Step <= 5 * DTConst::SIM_FIXED_HZ; ++Step)
	{
		TraceStep(S, Step);
		S.RequestUi(Ui, 3);
		S.Frame(SecondsOf(Step), Step, Out);
	}
	for (int32 Flush = 0; Flush < 2 * DTConst::AUD_GRID_STEPS && S.NumWaiting() > 0; ++Flush, ++Step)
	{
		S.Frame(SecondsOf(Step), Step, Out);
	}
	TestEqual(TEXT("nothing waits"), S.NumWaiting(), 0);
	for (int32 Sound = 0; Sound < S.NumSounds(); ++Sound)
	{
		const FDTAudioCounts& C = S.GetCounts(Sound);
		TestTrue(FString::Printf(TEXT("row %d: asked %lld = played %lld + merged %lld + dropped %lld"), Sound, C.Asked, C.Played, C.Merged, C.Dropped), Settled(S, Sound) && C.Asked > 0);
	}
	int64 Played = 0;
	for (int32 Sound = 0; Sound < S.NumSounds(); ++Sound)
	{
		Played += S.GetCounts(Sound).Played;
	}
	TestEqual(TEXT("played is the voices started"), Played, static_cast<int64>(Out.Num()));
	// A run that ends with requests waiting still adds up: they are dropped with it.
	S.Request(0, 2, Step + 1);
	S.Reset(6);
	TestTrue(TEXT("a reset drops what waited, and counts it"), Settled(S, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestCost, "Dusktracer.Audio.Cost", DTAudioTest::Flags) // T-AUD-21
bool FDTAudTestCost::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	// A blow-up check, not a reading: the plan gives observing and scheduling 0.01 ms a frame (S4_audio_wave1.md §7.3); this
	// fails at ten times that, on the density trace, on a shared machine. The reading of record is the scheduler's perf run.
	constexpr double PlannedMs = 0.01;
	constexpr double BlowUp = 10.0;
	DTAudioTest::FTestWorld Test;
	FDTAudioScheduler S;
	S.Configure(TraceRows(), 3);
	FDTObserveInterface Interface;
	struct FNull : public IDTAudioRequests
	{
		virtual void Run(int32, int32, int64) override {}
		virtual void Ui(int32, int32) override {}
	} Null;
	TArray<FDTAudioVoice> Out;
	const int32 Frames = 20 * DTConst::SIM_FIXED_HZ;
	const uint64 Start = FPlatformTime::Cycles64();
	for (int64 Step = 1; Step <= Frames; ++Step)
	{
		Interface.OnFrame(*Test.World, Null);
		TraceStep(S, Step);
		Out.Reset();
		S.Frame(SecondsOf(Step), Step, Out);
	}
	const double MsPerFrame = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - Start) / Frames;
	AddInfo(FString::Printf(TEXT("observe and schedule: %.5f ms a frame over %d frames of the density trace"), MsPerFrame, Frames));
	TestTrue(FString::Printf(TEXT("%.5f ms a frame is under ten times the planned share"), MsPerFrame), MsPerFrame < PlannedMs * BlowUp);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
