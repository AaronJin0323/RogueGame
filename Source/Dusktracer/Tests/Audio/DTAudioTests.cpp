// The audio runtime (02_Systems/audio.md T-AUD-01, 02, 03, 05, 06, 07, 09, 10, 11, 12, 19, 20, 21).
// Run headless: Tools\RunTests.ps1 -Filter Dusktracer.Audio

#include "Misc/AutomationTest.h"
#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Audio/DTAudioScheduler.h"
#include "Audio/DTAudioSubsystem.h"
#include "Audio/DTAudioTypes.h"
#include "Audio/DTStingerQueue.h"
#include "Audio/Observe/DTObserveDanger.h"
#include "Audio/Observe/DTObserveFire.h"
#include "Audio/Observe/DTObserveInterface.h"
#include "Audio/Observe/DTObserveWarn.h"
#include "Bot/DTBotSubsystem.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/DTHudLayout.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Offers/DTOfferSubsystem.h"
#include "Internationalization/Regex.h"
#include "Audio.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundEffectSubmix.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundWave.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"
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
		bool bGrid, int32 LengthMs, DTCat::ESoundNote Note = DTCat::ESoundNote::None)
	{
		const ESoundClass Class = Side == ESoundSide::Danger ? ESoundClass::Danger : (Side == ESoundSide::UI ? ESoundClass::UI : ESoundClass::Player);
		const DTCat::ESoundNote Written = Pitch == ESoundPitch::Key && Note == DTCat::ESoundNote::None ? DTCat::ESoundNote::D : Note;
		return DTCat::FSoundRow{ Id, TEXT("/Game/DT/Audio/SFX/SW_Test"), Side, Class, Variants, false, Volume, Pitch, Written, IntervalMs, Voices, Agg, bGrid, LengthMs, true };
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
		/** Asked of the four warnings, and of enemy fire. */
		int64 WarnAsked = 0;
		int64 FireAsked = 0;
		/** The nine Warn and Danger ids' counts, one line each. */
		TArray<FString> DangerCounts;
		/** Asked of the ten weapons' fire sounds (the local player's), and their counts, one line each. */
		int64 PlayerFireAsked = 0;
		bool bFireSettled = true;
		TArray<FString> FireCounts;
		bool bObserving = false;
		bool bHasAudio = false;
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
	static FRunResult Play(int32 Seed, bool bArmed = false)
	{
		FRunResult Result;
		FTestWorld Test;
		UDTSimSubsystem* Sim = Test.World->GetSubsystem<UDTSimSubsystem>();
		UDTBotSubsystem* Bot = Test.World->GetSubsystem<UDTBotSubsystem>();
		UDTTelemetrySubsystem* Telemetry = Test.World->GetSubsystem<UDTTelemetrySubsystem>();
		UDTDirector* Director = Test.World->GetSubsystem<UDTDirector>();
		const UDTHeroSubsystem* Heroes = Test.World->GetSubsystem<UDTHeroSubsystem>();
		UDTAudioSubsystem* Audio = Test.World->GetSubsystem<UDTAudioSubsystem>();
		Result.bHasAudio = Audio != nullptr;
		if (Sim && Bot && Telemetry && Director && Heroes)
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
			if (UDTEnemySubsystem* Enemies = bArmed ? Test.World->GetSubsystem<UDTEnemySubsystem>() : nullptr)
			{
				// The opening flock neither shoots nor tells: two turrets and two rushers at the start, in both arms alike, bring telegraphs
				// and enemy fire into the first seconds.
				for (const FVector2f& At : { FVector2f(700.0f, 0.0f), FVector2f(-700.0f, 300.0f) })
				{
					Enemies->GetPool().Spawn(EDTEnemyRole::Turret, At);
					Enemies->GetPool().Spawn(EDTEnemyRole::Rusher, At * 0.8f + FVector2f(0.0f, 200.0f));
				}
			}
			const int32 Steps = BotSeconds * DTConst::SIM_FIXED_HZ;
			if (Audio)
			{
				Audio->Tick(0.0f); // binds the observer
			}
			while (Sim->GetStepIndex() < Steps && !Bot->IsRunFinished())
			{
				// Armed, a frame follows every step, as in a person's run at the sim's rate: the counts are a real run's.
				const int32 Chunk = FMath::Min<int32>(bArmed ? 1 : DTConst::SIM_FIXED_HZ, Steps - static_cast<int32>(Sim->GetStepIndex()));
				if (Bot->RunSteps(Chunk) == 0)
				{
					break;
				}
				if (Audio)
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
			for (int32 Sound = 0; Audio && Sound < Audio->GetScheduler().NumSounds(); ++Sound)
			{
				Result.AudioAsked += Audio->GetScheduler().GetCounts(Sound).Asked;
			}
			if (Audio)
			{
				for (const DTCat::ESoundId Warn : { DTCat::ESoundId::Snd_Warn_Line, DTCat::ESoundId::Snd_Warn_Band, DTCat::ESoundId::Snd_Warn_Ring, DTCat::ESoundId::Snd_Warn_Arc })
				{
					Result.WarnAsked += Audio->GetScheduler().GetCounts(static_cast<int32>(Warn)).Asked;
				}
				Result.FireAsked = Audio->GetScheduler().GetCounts(static_cast<int32>(DTCat::ESoundId::Snd_Danger_EnemyFire)).Asked;
				for (int32 Sound = static_cast<int32>(DTCat::ESoundId::Snd_Warn_Line); Sound <= static_cast<int32>(DTCat::ESoundId::Snd_Danger_BossPhase); ++Sound)
				{
					const FDTAudioCounts& C = Audio->GetScheduler().GetCounts(Sound);
					Result.DangerCounts.Add(FString::Printf(TEXT("%s asked %lld played %lld merged %lld dropped %lld"), DTCat::Sounds[Sound].Id, C.Asked, C.Played, C.Merged, C.Dropped));
				}
				for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
				{
					const int32 Sound = DTFireEvents::SoundOf(Row);
					if (Sound == INDEX_NONE)
					{
						continue;
					}
					const FDTAudioCounts& C = Audio->GetScheduler().GetCounts(Sound);
					Result.PlayerFireAsked += C.Asked;
					// What still waits at the run's end (a grid row's last batch) is not settled yet; only a count past the sum is wrong.
					Result.bFireSettled &= C.Played + C.Merged + C.Dropped <= C.Asked;
					Result.FireCounts.Add(FString::Printf(TEXT("%s asked %lld played %lld merged %lld dropped %lld"), DTCat::Sounds[Sound].Id, C.Asked, C.Played, C.Merged, C.Dropped));
				}
			}
			Result.bObserving = Audio && Audio->IsObserving();
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
	const FRunResult On = Play(31, true);
	Enabled.Set(0);
	const FRunResult Off = Play(31, true);
	TestTrue(TEXT("the run with audio played"), On.Steps > 0 && !On.Csv.IsEmpty());
	AddInfo(FString::Printf(TEXT("audio on: observing %d, asked %lld (warnings %lld, enemy fire %lld), decisions %d"), On.bObserving, On.AudioAsked, On.WarnAsked, On.FireAsked, On.Decisions));
	TestTrue(TEXT("with audio the world has the runtime, observing and asked for sounds (the picks)"), On.bHasAudio && On.bObserving && On.AudioAsked > 0);
	TestTrue(TEXT("the danger observers were on: the run's telegraphs asked for warnings and its enemies' rounds for enemy fire"), On.WarnAsked > 0 && On.FireAsked > 0);
	for (const FString& Line : On.DangerCounts)
	{
		AddInfo(FString::Printf(TEXT("danger count: %s"), *Line));
	}
	TestTrue(FString::Printf(TEXT("the fire observer was on: the bot's weapons asked for their fire sounds (%lld)"), On.PlayerFireAsked), On.PlayerFireAsked > 0);
	TestTrue(TEXT("the fire counts never pass what was asked"), On.bFireSettled);
	for (const FString& Line : On.FireCounts)
	{
		AddInfo(FString::Printf(TEXT("fire count: %s"), *Line));
	}
	TestFalse(TEXT("dt.Audio.Enabled 0 at world creation: no audio runtime at all"), Off.bHasAudio);
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
	V.SettingsKey = 3;
	Expect(TEXT("settings opens deeper, and opening it is no Step"), Step(V), One(ESoundId::Snd_UI_Open));
	V.SettingsKey = 4;
	Expect(TEXT("a settings value moves on the settings screen: one Step"), Step(V), One(ESoundId::Snd_UI_Step));
	Expect(TEXT("the value held raises nothing"), Step(V), TArray<ESoundId>());
	{
		FDTUserSettings A;
		FDTUserSettings B = A;
		B.VolumePct -= DTConst::SETTINGS_VOLUME_STEP_PCT;
		FDTUserSettings C = A;
		C.Bindings.Set(EDTBindSlot::Dash, EDTBindDevice::Kbm, EKeys::LeftShift);
		TestTrue(TEXT("a volume step changes the settings key"), DTInterfaceEvents::SettingsKey(A) != DTInterfaceEvents::SettingsKey(B));
		TestEqual(TEXT("a binding (the controls screen) does not"), DTInterfaceEvents::SettingsKey(A), DTInterfaceEvents::SettingsKey(C));
	}
	V.Screen = EDTMenuScreen::Controls;
	V.SettingsKey = 5;
	Expect(TEXT("controls, as deep, opens; a value moved off the settings screen is no Step"), Step(V), One(ESoundId::Snd_UI_Open));
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

	// The Warn family (R-AUD-02): each telegraph new this step raises its warning once, by shape; several of one shape are one request.
	{
		using FAsks = TArray<TPair<ESoundId, int32>>;
		FAsks Asked;
		const auto Warn = [&](TConstArrayView<FDTTelegraph> Before, TConstArrayView<FDTTelegraph> After)
		{
			Asked.Reset();
			DTWarnEvents::NewThisStep(Before, After, [&](ESoundId Sound, int32 Count) { Asked.Emplace(Sound, Count); });
			return Asked;
		};
		const auto Tell = [](EDTTelegraphShape Shape, FVector2f Origin, float Progress)
		{
			FDTTelegraph T;
			T.Shape = Shape;
			T.Origin = Origin;
			T.Progress = Progress;
			return T;
		};
		TestTrue(TEXT("band warns as a band"), DTWarnEvents::SoundOf(EDTTelegraphShape::Band) == ESoundId::Snd_Warn_Band);
		TestTrue(TEXT("arc warns as an arc"), DTWarnEvents::SoundOf(EDTTelegraphShape::Arc) == ESoundId::Snd_Warn_Arc);
		TestTrue(TEXT("aim line warns as a line"), DTWarnEvents::SoundOf(EDTTelegraphShape::AimLine) == ESoundId::Snd_Warn_Line);
		for (const EDTTelegraphShape Shape : { EDTTelegraphShape::Ring, EDTTelegraphShape::RingGap, EDTTelegraphShape::SpiralLane, EDTTelegraphShape::HazardRing })
		{
			TestTrue(FString::Printf(TEXT("shape %d warns as a ring"), static_cast<int32>(Shape)), DTWarnEvents::SoundOf(Shape) == ESoundId::Snd_Warn_Ring);
		}
		// A standing telegraph: a hundred steps of one tell, its Progress growing and its origin drifting a little, raise exactly one warning.
		TArray<FDTTelegraph> TWas;
		int32 WarnRaised = 0;
		for (int32 Index = 0; Index < 100; ++Index)
		{
			const TArray<FDTTelegraph> TNow = { Tell(EDTTelegraphShape::Band, FVector2f(500.0f + Index * 2.0f, 0.0f), Index / 100.0f) };
			for (const TPair<ESoundId, int32>& Ask : Warn(TWas, TNow))
			{
				WarnRaised += Ask.Value;
				TestTrue(TEXT("the standing band warns as a band"), Ask.Key == ESoundId::Snd_Warn_Band);
			}
			TWas = TNow;
		}
		TestEqual(TEXT("a standing telegraph raises exactly one warning"), WarnRaised, 1);
		TestTrue(TEXT("the same telegraphs again raise nothing"), Warn(TWas, TWas).Num() == 0);
		const TArray<FDTTelegraph> Rings = { TWas[0], Tell(EDTTelegraphShape::Ring, FVector2f(0.0f, 900.0f), 0.0f), Tell(EDTTelegraphShape::Ring, FVector2f(0.0f, -900.0f), 0.0f),
			Tell(EDTTelegraphShape::Arc, FVector2f(-900.0f, 0.0f), 0.0f) };
		TestTrue(TEXT("two new rings in one step are one request carrying two; the arc is its own"),
			Warn(TWas, Rings) == FAsks({ { ESoundId::Snd_Warn_Ring, 2 }, { ESoundId::Snd_Warn_Arc, 1 } }));
		TArray<FDTTelegraph> Again = Rings;
		Again[0].Progress = 0.0f;
		TestTrue(TEXT("a tell that starts again (its Progress drops) is new again"), Warn(Rings, Again) == FAsks({ { ESoundId::Snd_Warn_Band, 1 } }));
		TArray<FDTTelegraph> Moved = Rings;
		Moved[1].Origin += FVector2f(DTWarnEvents::MatchUU * 2.0f, 0.0f);
		TestTrue(TEXT("a ring far from every ring of the last step is new"), Warn(Rings, Moved) == FAsks({ { ESoundId::Snd_Warn_Ring, 1 } }));
		TestTrue(TEXT("a telegraph gone raises nothing"), Warn(Rings, TArray<FDTTelegraph>()).Num() == 0);

		// The view's order is not stable (a kill's swap-remove, an orphan at the end): the nearest fit wins, not the first.
		const FVector2f P0(1000.0f, 0.0f);
		const FVector2f P60 = P0 + FVector2f(60.0f, 0.0f);
		{
			const TArray<FDTTelegraph> Before = { Tell(EDTTelegraphShape::Band, P0, 0.10f), Tell(EDTTelegraphShape::Band, P60, 0.50f) };
			const TArray<FDTTelegraph> After = { Tell(EDTTelegraphShape::Band, P60, 0.52f), Tell(EDTTelegraphShape::Band, P0, 0.12f) };
			TestTrue(TEXT("two bands 60 uu apart trade places in the list, both going on: no warning"), Warn(Before, After).Num() == 0);
		}
		{
			const TArray<FDTTelegraph> Before = { Tell(EDTTelegraphShape::Band, P0, 0.10f), Tell(EDTTelegraphShape::Band, P60, 0.30f),
				Tell(EDTTelegraphShape::Band, P0 + FVector2f(0.0f, 90.0f), 0.60f) };
			const TArray<FDTTelegraph> After = { Tell(EDTTelegraphShape::Band, P60, 0.32f), Tell(EDTTelegraphShape::Band, P0 + FVector2f(0.0f, 90.0f), 0.62f),
				Tell(EDTTelegraphShape::Band, P0, 0.12f) };
			TestTrue(TEXT("three bands inside the match distance, the first moved to the end going on (an orphan): no warning"), Warn(Before, After).Num() == 0);
		}
		{
			TArray<FDTTelegraph> Before;
			TArray<FDTTelegraph> After;
			for (int32 Line = 0; Line < 5; ++Line)
			{
				Before.Add(Tell(EDTTelegraphShape::AimLine, P0, 0.40f));
				After.Add(Tell(EDTTelegraphShape::AimLine, P0, 0.42f));
			}
			After.Insert(Tell(EDTTelegraphShape::AimLine, P0 + FVector2f(0.0f, 100.0f), 0.0f), 2);
			TestTrue(TEXT("a fan's five lines from one origin stay five; a sixth 100 uu away at 0 is exactly one new"), Warn(Before, After) == FAsks({ { ESoundId::Snd_Warn_Line, 1 } }));
		}
		{
			const TArray<FDTTelegraph> Before = { Tell(EDTTelegraphShape::Ring, P0, 0.2f), Tell(EDTTelegraphShape::Ring, P0, 0.8f) };
			const TArray<FDTTelegraph> After = { Tell(EDTTelegraphShape::Ring, P0, 0.0f), Tell(EDTTelegraphShape::Ring, P0, 0.22f) };
			TestTrue(TEXT("two rings from one origin, the 0.8 gone and a new one at 0: exactly one new"), Warn(Before, After) == FAsks({ { ESoundId::Snd_Warn_Ring, 1 } }));
		}
	}

	// The Danger family: enemy fire by the spawned counter's growth, the hero's events, the boss's phase (the heartbeat: T-AUD-12).
	{
		using FAsks = TArray<TPair<ESoundId, int32>>;
		FAsks Asked;
		FDTDangerView DWas;
		const auto Danger = [&](const FDTDangerView& DNow)
		{
			Asked.Reset();
			DTDangerEvents::Diff(DWas, DNow, [&](ESoundId Sound, int32 Count) { Asked.Emplace(Sound, Count); });
			DWas = DNow;
			return Asked;
		};
		FDTDangerView D;
		D.Step = 10;
		TestTrue(TEXT("nothing happening raises nothing"), Danger(D).Num() == 0);
		D.Step = 11;
		D.EnemyRounds = 7;
		TestTrue(TEXT("seven rounds spawned in a step: one enemy-fire request carrying seven"), Danger(D) == FAsks({ { ESoundId::Snd_Danger_EnemyFire, 7 } }));
		D.Step = 12;
		TestTrue(TEXT("an unchanged step raises none"), Danger(D).Num() == 0);
		D.Step = 13;
		D.EnemyRounds = 0;
		TestTrue(TEXT("a counter that starts again raises nothing"), Danger(D).Num() == 0);
		D.Step = 14;
		D.Hurt = 1;
		TestTrue(TEXT("the hero hurt"), Danger(D) == FAsks({ { ESoundId::Snd_Danger_HeroHurt, 1 } }));
		D.Step = 15;
		D.Hurt = 0;
		TestTrue(TEXT("the hurt is of its step only"), Danger(D).Num() == 0);
		D.Step = 16;
		D.Hurt = 1;
		D.Down = 1;
		TestTrue(TEXT("the blow that downs sounds as the fall alone"), Danger(D) == FAsks({ { ESoundId::Snd_Danger_HeroDown, 1 } }));
		D.Step = 17;
		D.Hurt = 2;
		D.Down = 0;
		TestTrue(TEXT("two hurts and no down: one hurt request carrying two"), Danger(D) == FAsks({ { ESoundId::Snd_Danger_HeroHurt, 2 } }));
		D.Step = 17;
		D.Hurt = D.Down = 0;
		D.bBossActive = true;
		D.BossPhase = 1;
		TestTrue(TEXT("the boss arriving at its first phase is no phase change"), Danger(D).Num() == 0);
		D.Step = 18;
		D.BossPhase = 2;
		TestTrue(TEXT("a change of phase"), Danger(D) == FAsks({ { ESoundId::Snd_Danger_BossPhase, 1 } }));
		D.Step = 19;
		TestTrue(TEXT("the phase held raises nothing"), Danger(D).Num() == 0);
		D.Step = 20;
		D.bBossActive = false;
		D.BossPhase = 3;
		TestTrue(TEXT("the boss gone raises nothing"), Danger(D).Num() == 0);
		D.Step = 21;
		D.bLowHp = true;
		TestTrue(TEXT("low health inside one pulse raises nothing"), Danger(D).Num() == 0);
	}

	// The Fire family: each weapon slot of the local player by the growth of its spawned counter, one request carrying the growth.
	{
		struct FFireAsks : public IDTAudioRequests
		{
			TArray<TPair<int32, int32>> Runs;
			TArray<TPair<int32, int32>> Merges;
			virtual void Run(int32 Sound, int32 Count, int64 AtStep, bool bNoDuck) override { Runs.Emplace(Sound, Count); }
			virtual void Ui(int32 Sound, int32 Count) override {}
			virtual void Merged(int32 Sound, int32 Count) override { Merges.Emplace(Sound, Count); }
		};
		using FPairs = TArray<TPair<int32, int32>>;
		const int32 Rifle = DTFireEvents::SoundOfId(TEXT("Weapon.PulseRifle"));
		const int32 Ring = DTFireEvents::SoundOfId(TEXT("Weapon.RingEmitter"));
		TestEqual(TEXT("Weapon.PulseRifle sounds as Snd.Fire.PulseRifle"), Rifle, static_cast<int32>(ESoundId::Snd_Fire_PulseRifle));
		TestEqual(TEXT("Weapon.RingEmitter sounds as Snd.Fire.RingEmitter"), Ring, static_cast<int32>(ESoundId::Snd_Fire_RingEmitter));
		int32 RifleRow = INDEX_NONE;
		int32 RingRow = INDEX_NONE;
		for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
		{
			RifleRow = FCString::Strcmp(DTCat::Weapons[Row].Id, TEXT("Weapon.PulseRifle")) == 0 ? Row : RifleRow;
			RingRow = FCString::Strcmp(DTCat::Weapons[Row].Id, TEXT("Weapon.RingEmitter")) == 0 ? Row : RingRow;
		}
		FDTObserveFire Fire;
		FFireAsks Asks;
		// Steps a hundred apart: no weapon here fires faster than the density's rate, so every firing sounds.
		const auto Observe = [&](int64 AtStep, int64 RifleSpawned, int64 RingSpawned)
		{
			FDTFireView FV;
			FV.Step = AtStep;
			FV.Weapon[0] = RifleRow;
			FV.Spawned[0] = RifleSpawned;
			FV.Weapon[1] = RingRow;
			FV.Spawned[1] = RingSpawned;
			Asks.Runs.Reset(); // the merges add up over the whole sequence
			Fire.Observe(FV, Asks);
			return Asks.Runs;
		};
		TestTrue(TEXT("the run's first step is the baseline: rounds already counted raise nothing"), Observe(100, 5, 0).Num() == 0);
		TestTrue(TEXT("the rifle fired two rounds: one request carrying two"), Observe(200, 7, 0) == FPairs({ { Rifle, 2 } }));
		TestTrue(TEXT("an unchanged step raises none"), Observe(300, 7, 0).Num() == 0);
		TestTrue(TEXT("both fire in one step: one request each, the ring's carrying its twelve"), Observe(400, 9, 12) == FPairs({ { Rifle, 2 }, { Ring, 12 } }));
		TestTrue(TEXT("a counter that went down is a new baseline"), Observe(500, 1, 12).Num() == 0);
		TestTrue(TEXT("and grows from there"), Observe(600, 3, 12) == FPairs({ { Rifle, 2 } }));
		TestTrue(TEXT("nothing was withheld at this pace"), Asks.Merges.Num() == 0);
		{
			// The observer and its density together: a rifle firing every third step sounds at most C.AUD.FIRE_DENSE_VOICES_PER_S times in any
			// second, and every firing it withholds is passed on as merged, carrying its rounds.
			FDTObserveFire Dense;
			FFireAsks DenseAsks;
			TArray<int64> Sounded;
			int64 Rounds = 0;
			for (int64 AtStep = 0; AtStep <= 5 * DTConst::SIM_FIXED_HZ; ++AtStep)
			{
				FDTFireView FV;
				FV.Step = AtStep;
				FV.Weapon[0] = RifleRow;
				FV.Spawned[0] = 2 * (AtStep / 3 + 1);
				const int32 RunsBefore = DenseAsks.Runs.Num();
				Dense.Observe(FV, DenseAsks);
				if (DenseAsks.Runs.Num() > RunsBefore)
				{
					Sounded.Add(AtStep);
				}
			}
			for (const TPair<int32, int32>& Asked : DenseAsks.Runs)
			{
				Rounds += Asked.Value;
			}
			for (const TPair<int32, int32>& Asked : DenseAsks.Merges)
			{
				Rounds += Asked.Value;
			}
			int32 Most = 0;
			for (const int64 From : Sounded)
			{
				Most = FMath::Max(Most, Sounded.FilterByPredicate([From](int64 S) { return S >= From && S < From + DTConst::SIM_FIXED_HZ; }).Num());
			}
			TestTrue(FString::Printf(TEXT("20 firings a second sound at most %d times in any second (most %d)"), DTConst::AUD_FIRE_DENSE_VOICES_PER_S, Most),
				Most <= DTConst::AUD_FIRE_DENSE_VOICES_PER_S && Sounded.Num() > 0);
			TestTrue(TEXT("the rest are withheld and passed on as merged"), DenseAsks.Merges.Num() > 0);
			TestEqual(TEXT("sounded and withheld together carry every round after the baseline"), Rounds, 2 * (5 * DTConst::SIM_FIXED_HZ / 3 + 1) - 2ll);
		}
		Fire.Reset();
		TestTrue(TEXT("a new run: its first step is the baseline again"), Observe(0, 50, 0).Num() == 0);
		TestTrue(TEXT("then each firing once"), Observe(100, 51, 0) == FPairs({ { Rifle, 1 } }));
		{
			// A slot that held no weapon last step (a weapon just given) sounds its first firing; one that held another weapon is a baseline.
			FDTFireView Before;
			FDTFireView After;
			After.Weapon[2] = RingRow;
			After.Spawned[2] = 8;
			FPairs Got;
			DTFireEvents::Diff(Before, After, [&](int32 Slot, int32 Sound, int32 Count) { Got.Emplace(Sound, Count); });
			TestTrue(TEXT("a weapon just given sounds its first firing"), Got == FPairs({ { Ring, 8 } }));
			Got.Reset();
			Before.Weapon[2] = RifleRow;
			DTFireEvents::Diff(Before, After, [&](int32 Slot, int32 Sound, int32 Count) { Got.Emplace(Sound, Count); });
			TestTrue(TEXT("a slot that held another weapon raises nothing"), Got.Num() == 0);
		}
		// Every weapon id the game's table holds resolves to its fire sound, by name, and a Reforging form keeps its base weapon's row.
		int32 Resolved = 0;
		for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
		{
			const int32 Sound = DTFireEvents::SoundOf(Row);
			const FString Want = FString(TEXT("Snd.Fire.")) + FString(DTCat::Weapons[Row].Id).RightChop(7);
			if (TestTrue(*FString::Printf(TEXT("%s resolves to a sound id"), DTCat::Weapons[Row].Id), Sound != INDEX_NONE))
			{
				TestEqual(*FString::Printf(TEXT("%s sounds as %s"), DTCat::Weapons[Row].Id, *Want), FString(DTCat::Sounds[Sound].Id), Want);
				++Resolved;
			}
		}
		TestEqual(TEXT("the ten weapons of the game's table resolve"), Resolved, 10);
		TestEqual(TEXT("the game's table holds ten weapons"), DTCat::NumWeapons, 10);
		for (int32 Row = 0; Row < DTCat::NumReforges; ++Row)
		{
			if (DTCat::Reforges[Row].bEvolve)
			{
				TestTrue(*FString::Printf(TEXT("%s sounds as its base weapon %s"), DTCat::Reforges[Row].Id, DTCat::Reforges[Row].Source),
					DTFireEvents::SoundOfId(DTCat::Reforges[Row].Source) != INDEX_NONE);
			}
		}
		TestEqual(TEXT("an id that is not a weapon resolves to none"), DTFireEvents::SoundOfId(TEXT("Snd.Fire.PulseRifle")), static_cast<int32>(INDEX_NONE));
		TestEqual(TEXT("a row outside the table resolves to none"), DTFireEvents::SoundOf(DTCat::NumWeapons), static_cast<int32>(INDEX_NONE));
	}

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
	// The predicates on staged inputs.
	TestTrue(TEXT("-unattended is a tool process"), UDTAudioSubsystem::IsToolProcess(true, TEXT("Default")));
	TestTrue(TEXT("a hidden desktop with no -unattended is a tool process (a packaged check)"), UDTAudioSubsystem::IsToolProcess(false, TEXT("DTHidden4242")));
	TestFalse(TEXT("a person on the default desktop is not"), UDTAudioSubsystem::IsToolProcess(false, TEXT("Default")));
	TestFalse(TEXT("no desktop name (not Windows) is not, by that signal"), UDTAudioSubsystem::IsToolProcess(false, FString()));
	TestTrue(TEXT("a tool process is silent"), UDTAudioSubsystem::IsSilent(true, false));
	TestTrue(TEXT("automation alone is silent (lock 1) and not a tool process (lock 2 leaves the volume)"), UDTAudioSubsystem::IsSilent(false, true));
	TestFalse(TEXT("a person's launch is neither"), UDTAudioSubsystem::IsSilent(false, false));

	// This process: the tests always run under automation, and a tool run (RunTests.ps1) is also a tool process.
	const bool bTool = UDTAudioSubsystem::IsToolProcessNow();
	TestTrue(TEXT("under automation the process is silent"), UDTAudioSubsystem::IsSilentNow());
	AddInfo(FString::Printf(TEXT("desktop '%s', tool process %d"), *UDTAudioSubsystem::ProcessDesktopName(), bTool));
	const float Was = FApp::GetVolumeMultiplier();
	FApp::SetVolumeMultiplier(0.8f); // what the settings subsystem sets at its own start
	{
		DTAudioTest::FTestWorld Test;
		UDTAudioSubsystem* Audio = Test.World->GetSubsystem<UDTAudioSubsystem>();
		if (!TestNotNull(TEXT("a game world has the audio runtime"), Audio))
		{
			return false;
		}
		if (bTool)
		{
			TestEqual(TEXT("lock 2: zero straight after Initialize, before any tick"), FApp::GetVolumeMultiplier(), 0.0f);
			FApp::SetVolumeMultiplier(0.6f);
			Audio->Tick(0.0f);
			TestEqual(TEXT("lock 2: zero again after a tick that follows a write"), FApp::GetVolumeMultiplier(), 0.0f);
		}
		else
		{
			TestEqual(TEXT("automation in a person's editor leaves the volume alone"), FApp::GetVolumeMultiplier(), 0.8f);
		}
		TestFalse(TEXT("lock 1: no engine sink"), Audio->HasEngineSink());
		Audio->RequestUi(static_cast<int32>(DTCat::ESoundId::Snd_UI_Open), 1);
		Audio->RunFrame(1.0);
		const FDTAudioCounts& C = Audio->GetScheduler().GetCounts(static_cast<int32>(DTCat::ESoundId::Snd_UI_Open));
		TestTrue(TEXT("voices are scheduled with no engine sink to take them"), C.Asked == 1 && C.Played == 1);
		TestFalse(TEXT("and still no engine sink"), Audio->HasEngineSink());
	}
	if (!bTool)
	{
		FApp::SetVolumeMultiplier(Was); // a tool process never gives the volume back
	}
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

	// Danger first, at the frame cap: player batches of several ids are asked before a warning in one frame. The warning starts
	// first, the cap's other starts go to the first players, the rest wait one frame and are dropped when that frame is full too.
	{
		TArray<DTCat::FSoundRow> CapRows;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			CapRows.Add(Row(TEXT("Snd.Test.P"), ESoundSide::Player, 1, 0.5f, ESoundPitch::Fixed, 0, 8, 0.0f, false, 50));
		}
		for (int32 Index = 0; Index < DTConst::AUD_STARTS_PER_FRAME_MAX; ++Index)
		{
			CapRows.Add(Row(TEXT("Snd.Test.D"), ESoundSide::Danger, 1, 0.8f, ESoundPitch::Fixed, 0, 8, 0.0f, false, 50));
		}
		const int32 FirstDanger = 6;
		FDTAudioScheduler C;
		C.Configure(CapRows, 3);
		for (int32 Player = 0; Player < 6; ++Player)
		{
			C.Request(Player, 1, 100);
		}
		C.Request(FirstDanger, 1, 100);
		TArray<FDTAudioVoice> Frame1;
		C.Frame(SecondsOf(100), 100, Frame1);
		TestTrue(TEXT("the warning starts first though asked last"), Frame1.Num() > 0 && Frame1[0].Sound == FirstDanger);
		TestEqual(TEXT("the cap holds, the warning counted in it"), Frame1.Num(), DTConst::AUD_STARTS_PER_FRAME_MAX);
		TestEqual(TEXT("the players past the cap wait"), C.NumWaiting(), 6 - (DTConst::AUD_STARTS_PER_FRAME_MAX - 1));
		for (int32 Danger = FirstDanger; Danger < CapRows.Num(); ++Danger)
		{
			C.Request(Danger, 1, 101);
		}
		TArray<FDTAudioVoice> Frame2;
		C.Frame(SecondsOf(101), 101, Frame2);
		TestTrue(TEXT("the next frame's warnings take its cap"), Frame2.Num() == DTConst::AUD_STARTS_PER_FRAME_MAX && Algo::AllOf(Frame2, [&](const FDTAudioVoice& V) { return V.Sound >= FirstDanger; }));
		int64 Dropped = 0;
		for (int32 Player = 0; Player < 6; ++Player)
		{
			Dropped += C.GetCounts(Player).Dropped;
		}
		TestEqual(TEXT("the waiting players dropped after their one frame"), Dropped, static_cast<int64>(6 - (DTConst::AUD_STARTS_PER_FRAME_MAX - 1)));
		TestEqual(TEXT("nothing waits"), C.NumWaiting(), 0);
	}

	// The duck: down to C.AUD.DUCK_DB within C.AUD.DUCK_ATTACK_MS, held while it sounds, back over C.AUD.DUCK_RELEASE_MS.
	const double Attack = DTConst::AUD_DUCK_ATTACK_MS / 1000.0;
	const double Release = DTConst::AUD_DUCK_RELEASE_MS / 1000.0;
	const double End = T0 + Rows[1].LengthMs / 1000.0;
	TestTrue(TEXT("halfway down at half the attack"), FMath::IsNearlyEqual(S.DuckDbAt(T0 + Attack / 2), DTConst::AUD_DUCK_DB / 2.0f, 0.01f));
	TestTrue(TEXT("down at the attack's end"), FMath::IsNearlyEqual(S.DuckDbAt(T0 + Attack), static_cast<float>(DTConst::AUD_DUCK_DB), 0.01f));
	TestTrue(TEXT("held while the warning sounds"), FMath::IsNearlyEqual(S.DuckDbAt(End - 0.01), static_cast<float>(DTConst::AUD_DUCK_DB), 0.01f));
	TestTrue(TEXT("halfway back at half the release"), FMath::IsNearlyEqual(S.DuckDbAt(End + Release / 2), DTConst::AUD_DUCK_DB / 2.0f, 0.01f));
	TestTrue(TEXT("back at zero after the release"), FMath::IsNearlyEqual(S.DuckDbAt(End + Release + 0.01), 0.0f, 0.001f));

	// The duck sent to the engine (R-AUD-05): a send only on frames where the curve moved; down within the attack, nothing while it holds,
	// back over the release, then nothing.
	{
		const TArray<DTCat::FSoundRow> DuckRows = { Row(TEXT("Snd.Test.Danger"), ESoundSide::Danger, 1, 0.8f, ESoundPitch::Fixed, 0, 4, 0.0f, false, 500) };
		FDTAudioScheduler D;
		D.Configure(DuckRows, 1);
		FDTDuckSender Sender;
		FDTRecordingSink Sink;
		TArray<FDTAudioVoice> Frame;
		const double Start = 10.0;
		const double Dt = 1.0 / DTConst::SIM_FIXED_HZ;
		TArray<double> SendTimes;
		for (int32 Index = 0; Index < 120; ++Index)
		{
			const double Now = Start + Index * Dt;
			if (Index == 0)
			{
				D.Request(0, 1, 600);
			}
			Frame.Reset();
			D.Frame(Now, 600 + Index, Frame);
			if (Sender.Next(D.GetDuckDb()))
			{
				Sink.SetPlayerDuckDb(Sender.Sent);
				SendTimes.Add(Now);
			}
		}
		const double HoldEnd = Start + DuckRows[0].LengthMs / 1000.0;
		const double ReleaseEnd = HoldEnd + DTConst::AUD_DUCK_RELEASE_MS / 1000.0;
		TestTrue(TEXT("the duck is sent"), Sink.DuckSends.Num() > 0);
		// A world starts at 0 dB whatever the engine kept: a fresh sender sends the curve's value on its first frame, 0 here.
		TestTrue(TEXT("a fresh sender sends 0 dB first, though the curve has not moved"), Sink.DuckSends.Num() > 0 && Sink.DuckSends[0] == 0.0f && SendTimes[0] == Start);
		{
			FDTDuckSender Fresh;
			TestTrue(TEXT("a fresh sender sends on its first frame"), Fresh.Next(0.0f));
			TestFalse(TEXT("and not again while nothing moves"), Fresh.Next(0.0f));
		}
		const int32 FloorAt = Sink.DuckSends.IndexOfByPredicate([](float Db) { return FMath::IsNearlyEqual(Db, static_cast<float>(DTConst::AUD_DUCK_DB), 0.01f); });
		TestTrue(TEXT("down to C.AUD.DUCK_DB within C.AUD.DUCK_ATTACK_MS (one frame's grain)"), FloorAt != INDEX_NONE && SendTimes[FloorAt] <= Start + DTConst::AUD_DUCK_ATTACK_MS / 1000.0 + Dt);
		TestTrue(TEXT("nothing sent while it holds"), !SendTimes.ContainsByPredicate([&](double T) { return T > SendTimes[FloorAt] && T < HoldEnd; }));
		TestTrue(TEXT("back to zero at the release's end"), FMath::IsNearlyEqual(Sink.DuckSends.Last(), 0.0f, 0.001f) && SendTimes.Last() <= ReleaseEnd + Dt);
		TestTrue(TEXT("the release is sent frame by frame"), SendTimes.FilterByPredicate([&](double T) { return T > HoldEnd; }).Num() >= FMath::FloorToInt((ReleaseEnd - HoldEnd) / Dt) - 1);
		TestTrue(TEXT("every send follows the curve"), Algo::AllOf(Sink.DuckSends, [](float Db) { return Db <= 0.0f && Db >= DTConst::AUD_DUCK_DB - 0.001f; }));
	}

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
	TestEqual(TEXT("the trace reaches the player voice limit"), WorstPlayer, PlayerCap);
	TestEqual(TEXT("the trace reaches the player starts in a second"), WorstSlidingSecond, DTConst::AUD_PLAYER_STARTS_PER_S_MAX);
	TestEqual(TEXT("the trace reaches the starts in a frame"), WorstFrame, DTConst::AUD_STARTS_PER_FRAME_MAX);
	TestTrue(TEXT("and drops what the caps refuse"), S.GetCounts(Hit).Dropped > 0);
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
	// A minor pentatonic as pitch classes above A, written here and not read from the scheduler's own table.
	const TSet<int32> Scale = { 0, 3, 5, 7, 10 };
	const TPair<DTCat::ESoundNote, int32> Notes[] = { { DTCat::ESoundNote::A, 0 }, { DTCat::ESoundNote::C, 3 }, { DTCat::ESoundNote::D, 5 },
		{ DTCat::ESoundNote::E, 7 }, { DTCat::ESoundNote::G, 10 } };
	for (const TPair<DTCat::ESoundNote, int32>& Written : Notes)
	{
		const TArray<DTCat::FSoundRow> Rows = {
			Row(TEXT("Snd.Test.Key"), ESoundSide::Player, 3, 0.5f, ESoundPitch::Key, 0, 64, 0.0f, false, 10, Written.Key),
			Row(TEXT("Snd.Test.Fixed"), ESoundSide::UI, 2, 0.6f, ESoundPitch::Fixed, 0, 64, 0.0f, false, 10),
			Row(TEXT("Snd.Test.Warn"), ESoundSide::Danger, 1, 0.8f, ESoundPitch::Key, 0, 64, 0.0f, false, 10, Written.Key),
		};
		const auto Run = [&](uint64 Seed)
		{
			FDTAudioScheduler S;
			S.Configure(Rows, Seed);
			TArray<FDTAudioVoice> All;
			for (int64 Step = 1; Step <= 60; ++Step)
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
		TSet<int32> Offsets;
		int32 Last = INDEX_NONE;
		for (const FDTAudioVoice& Voice : A)
		{
			if (Voice.Sound == 0)
			{
				TestTrue(TEXT("no variant twice running"), Voice.Variant != Last);
				Last = Voice.Variant;
				const int32 Offset = FMath::RoundToInt(12.0f * FMath::Log2(Voice.Pitch));
				TestTrue(TEXT("the pitch is a whole number of semitones"), FMath::IsNearlyEqual(Voice.Pitch, FMath::Pow(2.0f, Offset / 12.0f), 1.0e-5f));
				const int32 Class = ((Written.Value + Offset) % 12 + 12) % 12;
				TestTrue(FString::Printf(TEXT("note %d: offset %+d lands in A minor pentatonic"), Written.Value, Offset), Scale.Contains(Class));
				TestTrue(TEXT("within C.AUD.KEY_SEMIS_MAX"), FMath::Abs(Offset) <= DTConst::AUD_KEY_SEMIS_MAX);
				const float Db = 20.0f * FMath::LogX(10.0f, Voice.Volume / 0.5f);
				TestTrue(TEXT("the level within C.AUD.LEVEL_JITTER_DB"), FMath::Abs(Db) <= DTConst::AUD_LEVEL_JITTER_DB + 1.0e-4f);
				Offsets.Add(Offset);
			}
			else
			{
				TestEqual(TEXT("a fixed row and a warning never vary in pitch"), Voice.Pitch, 1.0f);
				TestEqual(TEXT("nor in level"), Voice.Volume, Rows[Voice.Sound].Volume);
			}
		}
		TestTrue(TEXT("degrees are drawn, not one"), Offsets.Num() >= 3);
		const TArray<FDTAudioVoice> B = Run(42);
		bool bSame = A.Num() == B.Num();
		for (int32 Index = 0; bSame && Index < A.Num(); ++Index)
		{
			bSame = A[Index].Sound == B[Index].Sound && A[Index].Variant == B[Index].Variant && A[Index].Pitch == B[Index].Pitch && A[Index].Volume == B[Index].Volume;
		}
		TestTrue(TEXT("one seed, one voice log"), bSame);
	}
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
	int64 Dropped = Q.GetDropped();
	Q.Push(2, EDTStinger::Pick, 10.0);
	TestFalse(TEXT("an entry past its expiry is dropped"), Q.Pop(10.0 + Expiry + 0.01, E));
	TestTrue(TEXT("and counted"), Q.Num() == 0 && Q.GetDropped() == Dropped + 1);
	Dropped = Q.GetDropped();

	Q.Reset();
	Q.Push(3, EDTStinger::Pick, 20.0);
	Q.Push(3, EDTStinger::Tier, 20.0);
	TestFalse(TEXT("a pick behind its Path's tier is dropped"), Q.Push(3, EDTStinger::Pick, 20.0));
	TestTrue(TEXT("the tier dropped the pending pick of its Path, and both drops are counted"), Q.Num() == 1 && Q.GetDropped() == Dropped + 2);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestNewRun, "Dusktracer.Audio.NewRun", DTAudioTest::Flags) // T-AUD-02
bool FDTAudTestNewRun::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	const TArray<DTCat::FSoundRow> Rows = {
		Row(TEXT("Snd.Test.Run"), ESoundSide::Player, 1, 0.5f, ESoundPitch::Fixed, 500, 4, 0.0f, true, 1000),
		Row(TEXT("Snd.Test.Ui"), ESoundSide::UI, 1, 0.6f, ESoundPitch::Fixed, 500, 4, 0.0f, false, 1000),
		Row(TEXT("Snd.Test.Ui2"), ESoundSide::UI, 1, 0.6f, ESoundPitch::Fixed, 0, 4, 0.0f, false, 1000),
	};
	FDTAudioScheduler S;
	S.Configure(Rows, 1);
	TArray<FDTAudioVoice> Out;
	S.Request(0, 1, 30);
	S.RequestUi(1, 1);
	S.Frame(1.0, 30, Out);
	S.Request(0, 1, 31);   // a grid batch that waits for step 33
	TArray<FDTAudioVoice> Next;
	S.Frame(1.01, 31, Next);
	TestTrue(TEXT("a run voice and an interface voice sound"), S.NumAlive(ESoundSide::Player) == 1 && S.NumAlive(ESoundSide::UI) == 1 && S.NumWaiting() == 1);
	S.RequestUi(2, 1);     // asked before the new run, resolved after it
	S.ResetRun(2);
	TestEqual(TEXT("the run's voices are gone"), S.NumAlive(ESoundSide::Player), 0);
	TestEqual(TEXT("the interface voice still sounds"), S.NumAlive(ESoundSide::UI), 1);
	TestEqual(TEXT("the run's waiting batch is dropped, and counted"), S.GetCounts(0).Dropped, int64(1));
	Out.Reset();
	S.RequestUi(1, 1);     // inside the interface interval of its voice
	S.Request(0, 1, 1);    // the new run's first step: no interval left over from the old run
	S.Frame(1.1, 3, Out);
	TestTrue(TEXT("the interface interval still holds across runs"), S.GetCounts(1).Merged == 1);
	TestTrue(TEXT("the interface request asked before the new run still plays"), Out.ContainsByPredicate([](const FDTAudioVoice& V) { return V.Sound == 2; }));
	TestTrue(TEXT("the new run starts free of the old run's interval"), Out.ContainsByPredicate([](const FDTAudioVoice& V) { return V.Sound == 0; }));
	return true;
}

namespace DTAudioTest
{
	static const TCHAR* ClassAssetOf(DTCat::ESoundClass Class)
	{
		switch (Class)
		{
		case DTCat::ESoundClass::Player: return TEXT("SC_Player");
		case DTCat::ESoundClass::Danger: return TEXT("SC_Danger");
		case DTCat::ESoundClass::UI:     return TEXT("SC_UI");
		default:                         return TEXT("SC_Music");
		}
	}

	static USoundWave* LoadWave(const DTCat::FSoundRow& Row, int32 Variant, FString& OutName)
	{
		const FString Package = Variant == 0 ? FString(Row.Asset) : FString::Printf(TEXT("%s_%d"), Row.Asset, Variant);
		OutName = FPackageName::GetShortName(Package);
		return LoadObject<USoundWave>(nullptr, *FString::Printf(TEXT("%s.%s"), *Package, *OutName), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	static USoundClass* LoadClass(const TCHAR* Name)
	{
		return LoadObject<USoundClass>(nullptr, *FString::Printf(TEXT("/Game/DT/Audio/Mix/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	/** The SHA-1 of a PCM block, lower-case hex, as Tools/audio/audio_build.py writes pcm_sha1. */
	static FString PcmSha1(const uint8* Data, int32 Size)
	{
		uint8 Hash[20];
		FSHA1::HashBuffer(Data, Size, Hash);
		return BytesToHex(Hash, 20).ToLower();
	}

	/** The manifest's lines by file name: column name to cell. */
	static TMap<FString, TMap<FString, FString>> ReadManifest()
	{
		TMap<FString, TMap<FString, FString>> Out;
		TArray<FString> Lines;
		FFileHelper::LoadFileToStringArray(Lines, *FPaths::Combine(FPaths::ProjectDir(), TEXT("Tools/audio/sound_manifest.csv")));
		if (Lines.Num() == 0)
		{
			return Out;
		}
		TArray<FString> Header;
		Lines[0].ParseIntoArray(Header, TEXT(","), false);
		for (int32 Index = 1; Index < Lines.Num(); ++Index)
		{
			TArray<FString> Cells;
			Lines[Index].ParseIntoArray(Cells, TEXT(","), false);
			if (Cells.Num() != Header.Num())
			{
				continue;
			}
			TMap<FString, FString>& Line = Out.Add(Cells[0]);
			for (int32 Cell = 0; Cell < Cells.Num(); ++Cell)
			{
				Line.Add(Header[Cell], Cells[Cell]);
			}
		}
		return Out;
	}
}

#if WITH_EDITORONLY_DATA
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestAssets, "Dusktracer.Audio.Assets", DTAudioTest::Flags) // T-AUD-19
bool FDTAudTestAssets::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	const TMap<FString, TMap<FString, FString>> Manifest = ReadManifest();
	TestTrue(TEXT("the manifest is read"), Manifest.Num() >= DTCat::SoundCount);
	int32 Checked = 0;
	for (const DTCat::FSoundRow& Row : DTAudio::Catalog())
	{
		if (!Row.bPlayable)
		{
			continue; // R-AUD-22: a row the owner did not accept has no asset to check
		}
		for (int32 Variant = 0; Variant < Row.Variants; ++Variant)
		{
			FString Name;
			USoundWave* Wave = LoadWave(Row, Variant, Name);
			const TMap<FString, FString>* Line = Manifest.Find(Name);
			if (!TestNotNull(*FString::Printf(TEXT("%s exists"), *Name), Wave) || !TestNotNull(*FString::Printf(TEXT("%s has a manifest line"), *Name), Line))
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("%s: one channel"), *Name), Wave->NumChannels, 1);
			TestEqual(*FString::Printf(TEXT("%s: the generator's rate"), *Name), static_cast<int32>(Wave->GetImportedSampleRate()), DTConst::AUD_SAMPLE_RATE_HZ);
			TestEqual(*FString::Printf(TEXT("%s: loops as the catalog says"), *Name), static_cast<bool>(Wave->bLooping), Row.bLoop);
			const FSharedBuffer Raw = Wave->RawData.GetPayload().Get();
			FWaveModInfo Info;
			if (!TestTrue(*FString::Printf(TEXT("%s holds a WAV"), *Name), Raw.GetSize() > 0 && Info.ReadWaveInfo(static_cast<const uint8*>(Raw.GetData()), static_cast<int32>(Raw.GetSize()))))
			{
				continue;
			}
			TestEqual(*FString::Printf(TEXT("%s: 16-bit"), *Name), static_cast<int32>(*Info.pBitsPerSample), 16);
			TestEqual(*FString::Printf(TEXT("%s: frames equal the manifest's"), *Name), static_cast<int32>(Info.SampleDataSize / 2), FCString::Atoi(*(*Line)[TEXT("frames")]));
			TestEqual(*FString::Printf(TEXT("%s: the PCM hashes to the manifest's"), *Name), PcmSha1(Info.SampleDataStart, Info.SampleDataSize), (*Line)[TEXT("pcm_sha1")]);
			if (++Checked == 1)
			{
				// The comparison can fail: a copy with one sample changed reports a mismatch.
				TArray<uint8> Copy(Info.SampleDataStart, Info.SampleDataSize);
				Copy[Copy.Num() / 2] ^= 1;
				TestNotEqual(TEXT("one changed sample is a mismatch"), PcmSha1(Copy.GetData(), Copy.Num()), (*Line)[TEXT("pcm_sha1")]);
			}
		}
	}
	TestTrue(TEXT("every playable variant was checked"), Checked > 0);
	return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestClasses, "Dusktracer.Audio.Classes", DTAudioTest::Flags) // T-AUD-04
bool FDTAudTestClasses::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	// R-AUD-04: SC_Master over SC_SFX, SC_UI and SC_Music; SC_SFX over SC_Player and SC_Danger.
	const TPair<const TCHAR*, const TCHAR*> Tree[] = { { TEXT("SC_Master"), nullptr }, { TEXT("SC_SFX"), TEXT("SC_Master") }, { TEXT("SC_UI"), TEXT("SC_Master") },
		{ TEXT("SC_Music"), TEXT("SC_Master") }, { TEXT("SC_Player"), TEXT("SC_SFX") }, { TEXT("SC_Danger"), TEXT("SC_SFX") } };
	for (const TPair<const TCHAR*, const TCHAR*>& Node : Tree)
	{
		USoundClass* Class = LoadClass(Node.Key);
		if (!TestNotNull(*FString::Printf(TEXT("%s exists"), Node.Key), Class))
		{
			continue;
		}
		const USoundClass* Parent = Node.Value ? LoadClass(Node.Value) : nullptr;
		TestTrue(*FString::Printf(TEXT("%s sits under %s"), Node.Key, Node.Value ? Node.Value : TEXT("nothing")), Class->ParentClass == Parent);
		if (Parent)
		{
			TestTrue(*FString::Printf(TEXT("%s lists %s"), Node.Value, Node.Key), Parent->ChildClasses.Contains(Class));
		}
		TestEqual(*FString::Printf(TEXT("%s: volume 1"), Node.Key), Class->Properties.Volume, 1.0f);
		TestEqual(*FString::Printf(TEXT("%s: pitch 1"), Node.Key), Class->Properties.Pitch, 1.0f);
		TestEqual(*FString::Printf(TEXT("%s: the paused flag only on SC_UI"), Node.Key), static_cast<bool>(Class->Properties.bIsUISound), FCString::Strcmp(Node.Key, TEXT("SC_UI")) == 0);
	}
	TestTrue(TEXT("the duck's mix exists"), LoadObject<USoundMix>(nullptr, TEXT("/Game/DT/Audio/Mix/SMX_Duck.SMX_Duck"), nullptr, LOAD_NoWarn | LOAD_Quiet) != nullptr);
	// R-AUD-04: one submix for the whole game, holding one dynamics processor set as a limiter at C.AUD.LIMIT_DBFS; every class routes to it.
	// Read by reflection, so this module needs no AudioMixer header: USubmixEffectDynamicsProcessorPreset's Settings, its type and threshold.
	USoundSubmix* Game = LoadObject<USoundSubmix>(nullptr, TEXT("/Game/DT/Audio/Mix/SMX_Game.SMX_Game"), nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (TestNotNull(TEXT("the game's submix SMX_Game exists"), Game))
	{
		TestEqual(TEXT("SMX_Game holds one effect"), Game->SubmixEffectChain.Num(), 1);
		const USoundEffectSubmixPreset* Preset = Game->SubmixEffectChain.Num() > 0 ? Game->SubmixEffectChain[0].Get() : nullptr;
		if (TestNotNull(TEXT("its effect is set"), Preset))
		{
			TestEqual(TEXT("its effect is SMX_GameLimiter"), Preset->GetName(), FString(TEXT("SMX_GameLimiter")));
			TestEqual(TEXT("a dynamics processor"), Preset->GetClass()->GetName(), FString(TEXT("SubmixEffectDynamicsProcessorPreset")));
			const FStructProperty* SettingsProp = FindFProperty<FStructProperty>(Preset->GetClass(), TEXT("Settings"));
			const FEnumProperty* TypeProp = SettingsProp ? FindFProperty<FEnumProperty>(SettingsProp->Struct, TEXT("DynamicsProcessorType")) : nullptr;
			const FNumericProperty* ThresholdProp = SettingsProp ? FindFProperty<FNumericProperty>(SettingsProp->Struct, TEXT("ThresholdDb")) : nullptr;
			const FNumericProperty* KneeProp = SettingsProp ? FindFProperty<FNumericProperty>(SettingsProp->Struct, TEXT("KneeBandwidthDb")) : nullptr;
			if (TestTrue(TEXT("its settings are read"), SettingsProp && TypeProp && ThresholdProp && KneeProp))
			{
				const void* Settings = SettingsProp->ContainerPtrToValuePtr<void>(Preset);
				const int64 Type = TypeProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(TypeProp->ContainerPtrToValuePtr<void>(Settings));
				TestEqual(TEXT("set as a limiter"), TypeProp->GetEnum()->GetNameStringByValue(Type), FString(TEXT("Limiter")));
				TestEqual(TEXT("at C.AUD.LIMIT_DBFS"), ThresholdProp->GetFloatingPointPropertyValue(ThresholdProp->ContainerPtrToValuePtr<void>(Settings)), static_cast<double>(DTConst::AUD_LIMIT_DBFS));
				TestEqual(TEXT("a hard knee: nothing under the line is touched"), KneeProp->GetFloatingPointPropertyValue(KneeProp->ContainerPtrToValuePtr<void>(Settings)), 0.0);
			}
		}
		for (const TPair<const TCHAR*, const TCHAR*>& Node : Tree)
		{
			const USoundClass* Class = LoadClass(Node.Key);
			TestTrue(*FString::Printf(TEXT("%s routes to SMX_Game"), Node.Key), Class && Class->Properties.DefaultSubmix == Game && !Class->Properties.bApplyEffects);
		}
	}
	int32 Waves = 0;
	for (const DTCat::FSoundRow& Row : DTAudio::Catalog())
	{
		for (int32 Variant = 0; Row.bPlayable && Variant < Row.Variants; ++Variant)
		{
			FString Name;
			USoundWave* Wave = LoadWave(Row, Variant, Name);
			if (!TestNotNull(*FString::Printf(TEXT("%s exists"), *Name), Wave))
			{
				continue;
			}
			++Waves;
			TestTrue(*FString::Printf(TEXT("%s sits in %s"), *Name, ClassAssetOf(Row.Class)), Wave->SoundClassObject && Wave->SoundClassObject->GetName() == ClassAssetOf(Row.Class));
			TestTrue(*FString::Printf(TEXT("%s loads inline: a first play never waits"), *Name), Wave->LoadingBehavior == ESoundWaveLoadingBehavior::ForceInline);
		}
	}
	TestTrue(TEXT("the playable waves were read"), Waves > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestPlayable, "Dusktracer.Audio.Playable", DTAudioTest::Flags) // T-AUD-19
bool FDTAudTestPlayable::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	using DTCat::ESoundId;
	// A person's launch would play them: each interface moment raised, through a recording sink in place of the engine sink, gets its asset,
	// for every playable interface id.
	DTAudioTest::FTestWorld Test;
	UDTAudioSubsystem* Audio = Test.World->GetSubsystem<UDTAudioSubsystem>();
	if (!TestNotNull(TEXT("the audio runtime"), Audio))
	{
		return false;
	}
	TestFalse(TEXT("T-AUD-03 holds: the test process has no engine sink"), Audio->HasEngineSink());
	TSharedPtr<FDTRecordingSink> Sink = MakeShared<FDTRecordingSink>();
	Audio->SetSinkForTest(Sink);
	FDTInterfaceView Was;
	TArray<FDTInterfaceView> Moments;
	FDTInterfaceView V;
	V.Screen = EDTMenuScreen::Pause; Moments.Add(V);                   // Open
	V.MenuFocus = 1; Moments.Add(V);                                    // Focus
	V.Screen = EDTMenuScreen::Settings; V.MenuFocus = 0; Moments.Add(V); // Open
	V.SettingsKey = 9; Moments.Add(V);                                  // Step
	V.Screen = EDTMenuScreen::Controls; Moments.Add(V);                 // Open
	V.MenuStatus = EDTMenuStatus::Reserved; Moments.Add(V);             // Refuse
	V.Screen = EDTMenuScreen::None; V.MenuStatus = EDTMenuStatus::None; Moments.Add(V); // Back
	V.HandId = 1; Moments.Add(V);                                       // HandOpen
	V.Rerolls = 1; V.HandId = 2; V.RerollOf = 1; Moments.Add(V);        // Reroll
	V.Banishes = 1; Moments.Add(V);                                     // Banish
	V.Locks = 1; Moments.Add(V);                                        // Lock
	V.Decisions = 1; V.HandId = INDEX_NONE; Moments.Add(V);             // Pick
	double Now = 100.0;
	for (const FDTInterfaceView& Moment : Moments)
	{
		DTInterfaceEvents::Diff(Was, Moment, [&](ESoundId Sound, int32 Count) { Audio->RequestUi(static_cast<int32>(Sound), Count); });
		Was = Moment;
		Now += 1.0; // a second apart: every interval passes, no frame cap is reached
		Audio->RunFrame(Now);
	}
	TSet<int32> Heard;
	for (int32 Index = 0; Index < Sink->Voices.Num(); ++Index)
	{
		const int32 Sound = Sink->Voices[Index].Sound;
		TestNotNull(*FString::Printf(TEXT("%s is given its asset"), DTCat::Sounds[Sound].Id), Sink->Sounds[Index]);
		Heard.Add(Sound);
	}
	// Only the interface rows are judged here: a run family's rows are never raised by an interface moment, and its own wiring job proves them.
	int32 Judged = 0;
	for (int32 Sound = 0; Sound < DTCat::SoundCount; ++Sound)
	{
		if (DTCat::Sounds[Sound].bPlayable && DTCat::Sounds[Sound].Side == DTCat::ESoundSide::UI)
		{
			++Judged;
			TestTrue(*FString::Printf(TEXT("%s was raised and started"), DTCat::Sounds[Sound].Id), Heard.Contains(Sound));
			TestEqual(*FString::Printf(TEXT("%s: missing 0"), DTCat::Sounds[Sound].Id), Audio->GetMissing(Sound), int64(0));
		}
	}
	TestTrue(TEXT("the playable interface rows were judged"), Judged > 0);
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
		virtual void Run(int32, int32, int64, bool) override {}
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

namespace DTAudioTest
{
	/** Requests straight into a scheduler, as the subsystem's own; bPlain drops the hold and the no-duck flag (the runtime before form b). */
	struct FToScheduler : public IDTAudioRequests
	{
		FDTAudioScheduler& S;
		bool bPlain = false;
		TArray<int64> Beats;
		explicit FToScheduler(FDTAudioScheduler& In, bool bInPlain = false) : S(In), bPlain(bInPlain) {}
		virtual void Run(int32 Sound, int32 Count, int64 Step, bool bNoDuck) override
		{
			S.Request(Sound, Count, Step, bPlain ? false : bNoDuck);
			if (Sound == static_cast<int32>(DTCat::ESoundId::Snd_Danger_Heartbeat))
			{
				Beats.Add(Step);
			}
		}
		virtual void Ui(int32 Sound, int32 Count) override { S.RequestUi(Sound, Count); }
		virtual void Merged(int32 Sound, int32 Count) override { S.CountMerged(Sound, Count); }
		virtual void HeldDuck(float Db) override
		{
			if (!bPlain)
			{
				S.SetHeldDuckDb(Db);
			}
		}
	};

	static float LowHpValue() { return 100.0f * DTConst::HUD_LOW_HP_FRAC * 0.5f; }

	/** The hero's HP at a step of the staged run: full, low, full, low, down. MaxHp is a hundred. */
	static float StagedHp(int64 Step)
	{
		return Step < 300 ? 100.0f : Step < 1200 ? LowHpValue() : Step < 1500 ? 100.0f : Step < 1800 ? LowHpValue() : 0.0f;
	}

	/** The view of a staged step: the observer's own reading of the HUD's condition, on the HUD's clock. */
	static FDTDangerView StagedView(int64 Step, float Hp)
	{
		FDTDangerView V;
		V.Step = Step;
		V.bLowHp = DTHud::LowHp(Hp, 100.0f, SecondsOf(Step), true).bLow;
		return V;
	}

	struct FStagedFrame
	{
		double Now = 0.0;
		int64 Step = 0;
		bool bPaused = false;
		float DuckDb = 0.0f;
		TArray<FDTAudioVoice> Voices;
	};

	static void NoExtra(FDTAudioScheduler&, int64) {}

	/** The staged run through the real table, a frame a step; PauseAt holds the sim for PauseFrames frames of real time. */
	static TArray<FStagedFrame> StagedRun(float HoldDb, bool bPlain, int64 Steps, int64 PauseAt, int32 PauseFrames, TArray<int64>* OutBeats,
		TFunctionRef<void(FDTAudioScheduler&, int64)> Extra)
	{
		FDTAudioScheduler S;
		S.Configure(DTAudio::Catalog(), 5);
		FDTObserveDanger Obs;
		FToScheduler Out(S, bPlain);
		TArray<FStagedFrame> Frames;
		double Now = 50.0;
		const double Dt = 1.0 / DTConst::SIM_FIXED_HZ;
		for (int64 Step = 0; Step < Steps; ++Step)
		{
			Obs.Observe(StagedView(Step, StagedHp(Step)), Out, HoldDb);
			Extra(S, Step);
			FStagedFrame& F = Frames.AddDefaulted_GetRef();
			F.Now = Now;
			F.Step = Step;
			S.Frame(Now, Step, F.Voices);
			F.DuckDb = S.GetDuckDb();
			Now += Dt;
			for (int32 Hold = 0; Step == PauseAt && Hold < PauseFrames; ++Hold)
			{
				FStagedFrame& P = Frames.AddDefaulted_GetRef();
				P.Now = Now;
				P.Step = Step;
				P.bPaused = true;
				S.Frame(Now, Step, P.Voices); // the sim stopped: frames go on, no step ends
				P.DuckDb = S.GetDuckDb();
				Now += Dt;
			}
		}
		if (OutBeats)
		{
			*OutBeats = Out.Beats;
		}
		return Frames;
	}

	static bool IsBeat(const FDTAudioVoice& V) { return V.Sound == static_cast<int32>(DTCat::ESoundId::Snd_Danger_Heartbeat); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestHeartbeat, "Dusktracer.Audio.Heartbeat", DTAudioTest::Flags) // T-AUD-12
bool FDTAudTestHeartbeat::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	// R-AUD-12: the heartbeat runs exactly while the HUD's low-HP state does, one beat a pulse of the HP bar, on the HUD's clock and phase.
	constexpr int64 Steps = 2000;
	constexpr int64 PauseAt = 700;
	constexpr int32 PauseFrames = 300;
	// The game's form is b (R-AUD-12): the hold the observer reads by default is C.AUD.LOWHP_HOLD_DB, and the staged run plays it.
	const float GameHold = DTDangerEvents::LowHpHoldDb();
	TestEqual(TEXT("the game's heartbeat is form b: the hold read by default is C.AUD.LOWHP_HOLD_DB"), GameHold, static_cast<float>(DTConst::AUD_LOWHP_HOLD_DB));
	TestTrue(TEXT("C.AUD.LOWHP_HOLD_DB is a depth (below zero)"), DTConst::AUD_LOWHP_HOLD_DB < 0);
	TArray<int64> Asked;
	const TArray<FStagedFrame> Frames = StagedRun(GameHold, false, Steps, PauseAt, PauseFrames, &Asked, NoExtra);
	TArray<int64> Played;
	int32 PausedBeats = 0;
	for (const FStagedFrame& F : Frames)
	{
		for (const FDTAudioVoice& V : F.Voices)
		{
			if (IsBeat(V))
			{
				Played.Add(V.Step);
				PausedBeats += F.bPaused ? 1 : 0;
			}
		}
	}
	TestTrue(TEXT("every beat asked for starts, on its step"), Played == Asked && Played.Num() > 0);
	TestEqual(TEXT("the sim stopped five seconds: no beat while it stands"), PausedBeats, 0);
	const double Period = DTConst::HUD_LOW_HP_PULSE_S * DTConst::SIM_FIXED_HZ; // steps a pulse
	const double PulseS = DTConst::HUD_LOW_HP_PULSE_S;
	int32 Wraps = 0;
	for (int64 Step = 1; Step < Steps; ++Step)
	{
		const FDTHudLowHp Hud = DTHud::LowHp(StagedHp(Step), 100.0f, SecondsOf(Step), true);
		// The HUD's pulse starts again (full) where its phase wraps: its own arithmetic, not the observer's.
		const bool bWrap = FMath::Fmod(SecondsOf(Step), PulseS) < FMath::Fmod(SecondsOf(Step - 1), PulseS);
		const bool bBeat = Asked.Contains(Step);
		Wraps += Hud.bLow && bWrap ? 1 : 0;
		if (bBeat)
		{
			TestTrue(FString::Printf(TEXT("step %lld: a beat only while the HUD shows low HP"), Step), Hud.bLow);
			// The pulse's top falls between two steps; the beat is on the first step after it, the fill within a step's fall of full.
			const float StepFall = 1.0f - DTHud::LowHp(StagedHp(Step), 100.0f, 1.0 / DTConst::SIM_FIXED_HZ, true).FillAlpha;
			TestTrue(FString::Printf(TEXT("step %lld: a beat where the bar's fill starts full again (fill %.4f)"), Step, Hud.FillAlpha), bWrap && Hud.FillAlpha >= 1.0f - StepFall - 1.0e-4f);
		}
		else if (Hud.bLow && bWrap)
		{
			AddError(FString::Printf(TEXT("step %lld: the HP bar's pulse starts again with no beat"), Step));
		}
	}
	TestEqual(TEXT("one beat for each of the HUD's pulses while low"), Asked.Num(), Wraps);
	double Gaps = 0.0;
	int32 NumGaps = 0;
	for (int32 Index = 1; Index < Asked.Num(); ++Index)
	{
		const int64 Gap = Asked[Index] - Asked[Index - 1];
		if (Gap < 2 * Period)
		{
			TestTrue(FString::Printf(TEXT("a gap of %lld steps is the pulse's period, to the step"), Gap), FMath::Abs(Gap - Period) <= 1.0);
			Gaps += Gap;
			++NumGaps;
		}
	}
	TestTrue(TEXT("the beats keep the pulse's period on average"), NumGaps > 10 && FMath::IsNearlyEqual(Gaps / FMath::Max(1, NumGaps), Period, 0.1));
	TestFalse(TEXT("none above the line"), Asked.ContainsByPredicate([](int64 S) { return S < 300 || (S >= 1200 && S < 1500); }));
	TestFalse(TEXT("none once down"), Asked.ContainsByPredicate([](int64 S) { return S >= 1800; }));
	{
		// Form a, at a hold of 0, beats on the same steps: the form changes the duck, never when the heart beats.
		TArray<int64> AskedA;
		StagedRun(0.0f, false, Steps, PauseAt, PauseFrames, &AskedA, NoExtra);
		TestTrue(TEXT("form a (hold 0) beats on the very steps form b does"), AskedA == Asked);
	}

	// A new run resets it: the observer's baseline is the new run's first step, so the beat is on the new run's clock from its first pulse.
	{
		FDTAudioScheduler S;
		S.Configure(DTAudio::Catalog(), 5);
		FDTObserveDanger Obs;
		FToScheduler Out(S);
		for (int64 Step = 1000; Step < 1030; ++Step)
		{
			Obs.Observe(StagedView(Step, LowHpValue()), Out, GameHold);
		}
		Out.Beats.Reset();
		Obs.Reset();
		S.ResetRun(6);
		for (int64 Step = 0; Step < 3 * Period; ++Step)
		{
			Obs.Observe(StagedView(Step, LowHpValue()), Out, GameHold);
		}
		TestTrue(TEXT("the new run's beats fall on its own pulses: its first step, then a period apart"),
			Out.Beats.Num() == 3 && Out.Beats[0] == 0 && FMath::Abs(Out.Beats[1] - Period) <= 1.0 && FMath::Abs(Out.Beats[2] - 2 * Period) <= 1.0);
		TestEqual(TEXT("the new run's first pulse starts full"), DTHud::LowHp(LowHpValue(), 100.0f, 0.0, true).FillAlpha, 1.0f);
	}

	// Form a's dip in numbers (reached at a hold of 0, no longer the game's): the duck the beats give the Player group over one beat in a
	// steady low stretch, printed beside form b's level hold.
	{
		const DTCat::FSoundRow& Beat = DTCat::Sounds[static_cast<int32>(DTCat::ESoundId::Snd_Danger_Heartbeat)];
		FDTAudioScheduler S;
		S.Configure(DTAudio::Catalog(), 5);
		TArray<FDTAudioVoice> Out;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			const int64 Step = FMath::RoundToInt(Index * Period);
			S.Request(static_cast<int32>(DTCat::ESoundId::Snd_Danger_Heartbeat), 1, Step);
			Out.Reset();
			S.Frame(10.0 + Index * PulseS, Step, Out);
		}
		const double From = 10.0 + 3 * PulseS; // the fourth beat: the curve is at rest when it starts
		int32 MsBelow = 0;
		double SumDb = 0.0;
		double SumGain = 0.0;
		float Depth = 0.0f;
		const int32 Ms = FMath::RoundToInt(PulseS * 1000.0);
		for (int32 M = 0; M < Ms; ++M)
		{
			const float Db = S.DuckDbAt(From + M / 1000.0 + 0.0005);
			MsBelow += Db < -0.01f ? 1 : 0;
			SumDb += Db;
			SumGain += FMath::Pow(10.0, Db / 20.0);
			Depth = FMath::Min(Depth, Db);
		}
		const double AvgDb = SumDb / Ms;
		const double AvgGainDb = 20.0 * FMath::LogX(10.0, SumGain / Ms);
		AddInfo(FString::Printf(TEXT("heartbeat form a: depth %.2f dB, %d ms of every %d below full (voice %d ms, release %d ms), average %.2f dB (by level), %.2f dB (by gain)"),
			Depth, MsBelow, Ms, Beat.LengthMs, DTConst::AUD_DUCK_RELEASE_MS, AvgDb, AvgGainDb));
		TestTrue(TEXT("form a dips to C.AUD.DUCK_DB"), FMath::IsNearlyEqual(Depth, static_cast<float>(DTConst::AUD_DUCK_DB), 0.01f));
		TestTrue(TEXT("below full for the voice and the release, to the ms"), FMath::Abs(MsBelow - (Beat.LengthMs + DTConst::AUD_DUCK_RELEASE_MS)) <= 1);
		AddInfo(FString::Printf(TEXT("heartbeat form b (the game's): the Player group held level at %.2f dB for as long as health is low, no dip at a beat"), GameHold));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestLowHpHold, "Dusktracer.Audio.LowHpHold", DTAudioTest::Flags) // T-AUD-12, form b
bool FDTAudTestLowHpHold::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	constexpr int64 Steps = 2000;
	// The game's hold, the key (R-AUD-12): what the observer reads by default.
	constexpr float Hold = static_cast<float>(DTConst::AUD_LOWHP_HOLD_DB);
	static_assert(Hold < 0.0f, "C.AUD.LOWHP_HOLD_DB is a depth below zero (form b)");
	constexpr int64 WarnAt = 600;
	const int32 Ring = static_cast<int32>(DTCat::ESoundId::Snd_Warn_Ring);
	const auto Warn = [Ring](FDTAudioScheduler& S, int64 Step)
	{
		if (Step == WarnAt)
		{
			S.Request(Ring, 1, Step);
		}
	};
	TestEqual(TEXT("the default the observer reads is the key"), DTDangerEvents::LowHpHoldDb(), Hold);
	const TArray<FStagedFrame> B = StagedRun(DTDangerEvents::LowHpHoldDb(), false, Steps, INDEX_NONE, 0, nullptr, Warn);
	const auto At = [&](int64 Step) { return B[Step].DuckDb; };
	TestEqual(TEXT("full while health is full"), At(299), 0.0f);
	const double Attack = DTConst::AUD_DUCK_ATTACK_MS / 1000.0;
	const int64 AttackSteps = FMath::CeilToInt(Attack * FMath::Abs(Hold / DTConst::AUD_DUCK_DB) * DTConst::SIM_FIXED_HZ) + 1;
	const int64 ReleaseSteps = FMath::CeilToInt(DTConst::AUD_DUCK_RELEASE_MS / 1000.0 * DTConst::SIM_FIXED_HZ) + 1;
	const int64 WarnEnd = WarnAt + FMath::CeilToInt((DTCat::Sounds[Ring].LengthMs + DTConst::AUD_DUCK_RELEASE_MS) / 1000.0 * DTConst::SIM_FIXED_HZ) + 2;
	TestTrue(TEXT("health turns low: down to the hold within the duck's attack"), FMath::IsNearlyEqual(At(300 + AttackSteps), Hold, 0.01f));
	// It goes down once, then stays level across the beats with no send while it holds.
	FDTDuckSender Sender;
	TArray<int64> Sends;
	for (const FStagedFrame& F : B)
	{
		if (Sender.Next(F.DuckDb))
		{
			Sends.Add(F.Step);
		}
	}
	TestFalse(TEXT("no send while it holds, beats and all"), Sends.ContainsByPredicate([&](int64 S) { return (S > 300 + AttackSteps && S < WarnAt) || (S > WarnEnd && S < 1200); }));
	TestTrue(TEXT("level across the beats"), Algo::AllOf(B, [&](const FStagedFrame& F) { return F.Step <= 300 + AttackSteps || F.Step >= WarnAt || F.DuckDb == Hold; }));
	int32 Beats = 0;
	for (const FStagedFrame& F : B)
	{
		Beats += F.Step > 300 && F.Step < 1200 && F.Voices.ContainsByPredicate(IsBeat) ? 1 : 0;
	}
	TestTrue(TEXT("the beats still sound"), Beats > 10);
	// A warning during it takes it to C.AUD.DUCK_DB and back to the hold.
	float WarnDepth = 0.0f;
	for (int64 Step = WarnAt; Step < WarnEnd; ++Step)
	{
		WarnDepth = FMath::Min(WarnDepth, At(Step));
	}
	TestTrue(TEXT("a warning takes it to C.AUD.DUCK_DB"), FMath::IsNearlyEqual(WarnDepth, static_cast<float>(DTConst::AUD_DUCK_DB), 0.01f));
	TestTrue(TEXT("and back to the hold, not to full"), FMath::IsNearlyEqual(At(WarnEnd), Hold, 0.01f));
	// Recovery: back to zero by the release.
	TestTrue(TEXT("health recovers: back to full within the release"), At(1200 + ReleaseSteps) == 0.0f && At(1201) < 0.0f);
	TestTrue(TEXT("down once more for the second low stretch"), FMath::IsNearlyEqual(At(1500 + AttackSteps), Hold, 0.01f));
	TestTrue(TEXT("down: the hold ends"), At(1800 + ReleaseSteps) == 0.0f);
	// The heartbeat's own voice adds no duck: across the beats before the warning the curve never goes past the hold.
	float Lowest = 0.0f;
	for (int64 Step = 300 + AttackSteps; Step < WarnAt; ++Step)
	{
		Lowest = FMath::Min(Lowest, At(Step));
	}
	AddInfo(FString::Printf(TEXT("heartbeat form b at %.1f dB: %.2f dB through every beat, held for as long as health is low (the average over a beat is the same)"), Hold, Lowest));
	TestEqual(TEXT("the heartbeat does not duck past the hold"), Lowest, Hold);

	// A new depth asked while the hold stands (a console trial while low): deeper by the attack's slope from where the curve is, in that
	// frame's update; shallower at once.
	{
		FDTAudioScheduler S;
		S.Configure(DTAudio::Catalog(), 5);
		FDTObserveDanger Obs;
		FToScheduler Out(S);
		TArray<FDTAudioVoice> Voices;
		const double Dt = 1.0 / DTConst::SIM_FIXED_HZ;
		TArray<float> Curve;
		float Mid = 0.0f;
		for (int64 Step = 0; Step < 200; ++Step)
		{
			const float Depth = Step < 60 ? -4.0f : Step < 120 ? -6.0f : -2.0f;
			Obs.Observe(StagedView(Step, LowHpValue()), Out, Depth);
			Voices.Reset();
			S.Frame(20.0 + Step * Dt, Step, Voices);
			Curve.Add(S.GetDuckDb());
			if (Step == 60)
			{
				Mid = S.DuckDbAt(20.0 + Step * Dt + 0.004); // four ms into the attack
			}
		}
		const float Slope = static_cast<float>(DTConst::AUD_DUCK_DB) / (DTConst::AUD_DUCK_ATTACK_MS / 1000.0f); // dB a second
		TestEqual(TEXT("held at -4 first"), Curve[59], -4.0f);
		TestEqual(TEXT("-6 asked while low: the attack starts in that frame from where the curve is"), Curve[60], -4.0f);
		TestTrue(TEXT("and goes on by the attack's slope, a frame later"), FMath::IsNearlyEqual(Curve[61], FMath::Max(-6.0f, -4.0f + Slope * static_cast<float>(Dt)), 0.01f));
		TestTrue(FString::Printf(TEXT("four ms in, the attack's slope: %.3f dB"), Mid), FMath::IsNearlyEqual(Mid, -4.0f + Slope * 0.004f, 0.01f));
		TestEqual(TEXT("held at -6"), Curve[119], -6.0f);
		TestTrue(TEXT("never past -6 on the way"), Algo::AllOf(Curve, [](float Db) { return Db >= -6.0f - 0.001f; }));
		TestEqual(TEXT("-2 asked while low: up at once, in that frame"), Curve[120], -2.0f);
		TestEqual(TEXT("and held there"), Curve[199], -2.0f);
	}

	// Form a is still reached at a hold of zero, and nothing differs from it, sample for sample: the same voices and the same curve as the
	// runtime before form b.
	{
		const TArray<FStagedFrame> A = StagedRun(0.0f, false, Steps, 700, 120, nullptr, Warn);
		const TArray<FStagedFrame> Plain = StagedRun(0.0f, true, Steps, 700, 120, nullptr, Warn);
		bool bSame = A.Num() == Plain.Num();
		for (int32 Index = 0; bSame && Index < A.Num(); ++Index)
		{
			bSame = A[Index].DuckDb == Plain[Index].DuckDb && A[Index].Voices.Num() == Plain[Index].Voices.Num();
			for (int32 V = 0; bSame && V < A[Index].Voices.Num(); ++V)
			{
				const FDTAudioVoice& X = A[Index].Voices[V];
				const FDTAudioVoice& Y = Plain[Index].Voices[V];
				bSame = X.Sound == Y.Sound && X.Step == Y.Step && X.Variant == Y.Variant && X.Volume == Y.Volume && X.Pitch == Y.Pitch && X.DuckDb == Y.DuckDb && X.Count == Y.Count;
			}
		}
		TestTrue(TEXT("hold 0: frame for frame, the curve and every voice equal form a's"), bSame && A.Num() > Steps);
	}

	// The console variable: there outside Shipping, its default the key, 0 selecting form a, never above zero.
	IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("dt.Audio.LowHpHoldDb"));
	if (TestNotNull(TEXT("dt.Audio.LowHpHoldDb exists outside Shipping"), Var))
	{
		const float Was = Var->GetFloat();
		TestEqual(TEXT("its default is C.AUD.LOWHP_HOLD_DB (form b)"), Var->GetFloat(), Hold);
		Var->Set(0.0f, ECVF_SetByCode);
		TestEqual(TEXT("0 selects form a"), DTDangerEvents::LowHpHoldDb(), 0.0f);
		Var->Set(-6.0f, ECVF_SetByCode);
		TestEqual(TEXT("another depth is read"), DTDangerEvents::LowHpHoldDb(), -6.0f);
		Var->Set(3.0f, ECVF_SetByCode);
		TestEqual(TEXT("never above zero"), DTDangerEvents::LowHpHoldDb(), 0.0f);
		Var->Set(Was, ECVF_SetByCode);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestFireDensity, "Dusktracer.Audio.FireDensity", DTAudioTest::Flags) // T-AUD-07, T-AUD-20
bool FDTAudTestFireDensity::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	// R-AUD-07's last clause, on sim time: above C.AUD.FIRE_DENSE_SHOTS_PER_S firings a second a weapon sounds at most
	// C.AUD.FIRE_DENSE_VOICES_PER_S times in any second, evenly spread; below it, every time.
	const int64 Second = DTConst::SIM_FIXED_HZ;
	const int64 Gap = FDTFireDensity::MinGapSteps();
	TestEqual(TEXT("the least gap is a second over the voices, rounded up"), Gap, (Second + DTConst::AUD_FIRE_DENSE_VOICES_PER_S - 1) / DTConst::AUD_FIRE_DENSE_VOICES_PER_S);
	TestTrue(TEXT("the keys leave room to thin: the voices are fewer than the dense rate"), DTConst::AUD_FIRE_DENSE_VOICES_PER_S <= DTConst::AUD_FIRE_DENSE_SHOTS_PER_S);
	constexpr int64 Seconds = 30;
	for (int32 Rate = 14; Rate <= 22; ++Rate)
	{
		FDTFireDensity D;
		TArray<int64> Fired;
		TArray<int64> Sounded;
		// Firings at Rate a second on whole steps: the k-th on step floor(k * FIXED_HZ / Rate), as a weapon's cooldown falls on steps.
		for (int64 K = 0; K * Second / Rate < Seconds * Second; ++K)
		{
			const int64 AtStep = K * Second / Rate;
			if (Fired.Num() > 0 && Fired.Last() == AtStep)
			{
				continue;
			}
			Fired.Add(AtStep);
			if (D.Admit(AtStep))
			{
				Sounded.Add(AtStep);
			}
		}
		int32 Most = 0;
		for (int64 From = 0; From < Seconds * Second; ++From)
		{
			int32 In = 0;
			for (const int64 S : Sounded)
			{
				In += S >= From && S < From + Second ? 1 : 0;
			}
			Most = FMath::Max(Most, In);
		}
		int64 MaxFireGap = 0;
		for (int32 Index = 1; Index < Fired.Num(); ++Index)
		{
			MaxFireGap = FMath::Max(MaxFireGap, Fired[Index] - Fired[Index - 1]);
		}
		bool bEven = Sounded.Num() > 1;
		for (int32 Index = 1; Index < Sounded.Num(); ++Index)
		{
			const int64 Between = Sounded[Index] - Sounded[Index - 1];
			bEven &= Between >= Gap && Between < Gap + MaxFireGap;
		}
		TestTrue(FString::Printf(TEXT("%d firings a second: at most %d voices in any second of sim time (most %d)"), Rate, DTConst::AUD_FIRE_DENSE_VOICES_PER_S, Most),
			Most <= DTConst::AUD_FIRE_DENSE_VOICES_PER_S);
		TestTrue(FString::Printf(TEXT("%d firings a second: evenly spread, each voice the least gap to one firing's gap after the last"), Rate), bEven);
		TestTrue(FString::Printf(TEXT("%d firings a second: it still sounds, about every %lld steps (%d voices in %lld s)"), Rate, Gap, Sounded.Num(), Seconds),
			Sounded.Num() >= Seconds * Second / (Gap + MaxFireGap));
	}
	{
		// Twice a second, and at the dense rate itself: every firing sounds.
		for (const int32 Rate : { 2, DTConst::AUD_FIRE_DENSE_SHOTS_PER_S - 1 })
		{
			FDTFireDensity D;
			int32 Fired = 0;
			int32 Sounded = 0;
			for (int64 AtStep = 0; AtStep < Seconds * Second; AtStep += Second / Rate)
			{
				++Fired;
				Sounded += D.Admit(AtStep) ? 1 : 0;
			}
			TestEqual(*FString::Printf(TEXT("%d firings a second sound every time"), Rate), Sounded, Fired);
		}
	}
	{
		// A reset (a new run) forgets the rate: the first firing after it sounds.
		FDTFireDensity D;
		D.Admit(0);
		D.Admit(3);
		D.Reset();
		TestTrue(TEXT("after a reset the first firing sounds"), D.Admit(4));
	}
	{
		// Through the scheduler on the real table: what the density withholds is counted as merged, so asked = played + merged + dropped
		// (T-AUD-20), and the rifle's played voices stay under the density's count.
		FDTAudioScheduler S;
		S.Configure(DTAudio::Catalog(), 9);
		FToScheduler Out(S);
		FDTObserveFire Fire;
		int32 RifleRow = INDEX_NONE;
		for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
		{
			RifleRow = FCString::Strcmp(DTCat::Weapons[Row].Id, TEXT("Weapon.PulseRifle")) == 0 ? Row : RifleRow;
		}
		const int32 Rifle = DTFireEvents::SoundOf(RifleRow);
		TArray<FDTAudioVoice> Voices;
		double Now = 10.0;
		for (int64 AtStep = 0; AtStep < 10 * Second; ++AtStep)
		{
			FDTFireView V;
			V.Step = AtStep;
			V.Weapon[0] = RifleRow;
			V.Spawned[0] = 2 * (AtStep / 3 + 1); // twenty firings a second
			Fire.Observe(V, Out);
			Voices.Reset();
			S.Frame(Now, AtStep, Voices);
			Now += 1.0 / Second;
		}
		for (int32 Flush = 0; Flush < 4; ++Flush)
		{
			Voices.Reset();
			S.Frame(Now, 10 * Second + DTConst::AUD_GRID_STEPS * (Flush + 1), Voices); // a grid row's last request starts or settles
			Now += 1.0 / Second;
		}
		const FDTAudioCounts& C = S.GetCounts(Rifle);
		AddInfo(FString::Printf(TEXT("the rifle at 20 firings a second for 10 s: asked %lld, played %lld, merged %lld, dropped %lld"), C.Asked, C.Played, C.Merged, C.Dropped));
		const int64 Rounds = 2 * ((10 * Second - 1) / 3 + 1) - 2; // the counter at the last step, less the baseline's two
		TestEqual(TEXT("asked is every round after the baseline, sounded or withheld"), C.Asked, Rounds);
		TestTrue(TEXT("asked = played + merged + dropped"), Settled(S, Rifle) && S.NumWaiting() == 0);
		TestTrue(TEXT("played at most the density's voices a second, and some"), C.Played > 0 && C.Played <= 10 * DTConst::AUD_FIRE_DENSE_VOICES_PER_S);
		TestTrue(TEXT("the withheld rounds are merged"), C.Merged > 0);
	}
	return true;
}

#if WITH_EDITORONLY_DATA
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestDangerPlayable, "Dusktracer.Audio.DangerPlayable", DTAudioTest::Flags) // T-AUD-19
bool FDTAudTestDangerPlayable::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	using DTCat::ESoundId;
	// Each danger moment raised by its observer's own rule, through a recording sink in place of the engine sink, gets its asset: all nine ids.
	DTAudioTest::FTestWorld Test;
	UDTAudioSubsystem* Audio = Test.World->GetSubsystem<UDTAudioSubsystem>();
	if (!TestNotNull(TEXT("the audio runtime"), Audio))
	{
		return false;
	}
	TSharedPtr<FDTRecordingSink> Sink = MakeShared<FDTRecordingSink>();
	Audio->SetSinkForTest(Sink);
	const auto Ask = [&](ESoundId Sound, int32 Count) { Audio->RequestRun(static_cast<int32>(Sound), Count, 0); };
	double Now = 100.0;
	const auto Frame = [&]() { Now += 5.0; Audio->RunFrame(Now); }; // five seconds apart: every voice ends, every interval passes
	Frame(); // the runtime meets the world's run first, so no request below is a run's leftover
	for (const EDTTelegraphShape Shape : { EDTTelegraphShape::AimLine, EDTTelegraphShape::Band, EDTTelegraphShape::Ring, EDTTelegraphShape::Arc })
	{
		FDTTelegraph T;
		T.Shape = Shape;
		const TArray<FDTTelegraph> None;
		const TArray<FDTTelegraph> One = { T };
		DTWarnEvents::NewThisStep(None, One, Ask);
		Frame();
	}
	FDTDangerView Base;
	Base.Step = 1;
	FDTDangerView Fire = Base;
	Fire.EnemyRounds = 3;
	FDTDangerView Hurt = Base;
	Hurt.Hurt = 1;
	FDTDangerView Down = Base;
	Down.Down = 1;
	FDTDangerView LowA = Base;
	LowA.Step = 47;
	LowA.bLowHp = true;
	FDTDangerView LowB = LowA;
	LowB.Step = 49;
	FDTDangerView BossA = Base;
	BossA.bBossActive = true;
	BossA.BossPhase = 1;
	FDTDangerView BossB = BossA;
	BossB.BossPhase = 2;
	const TPair<FDTDangerView, FDTDangerView> Moments[] = { { Base, Fire }, { Base, Hurt }, { Base, Down }, { LowA, LowB }, { BossA, BossB } };
	for (const TPair<FDTDangerView, FDTDangerView>& Moment : Moments)
	{
		DTDangerEvents::Diff(Moment.Key, Moment.Value, Ask);
		Frame();
	}
	TSet<int32> Heard;
	for (int32 Index = 0; Index < Sink->Voices.Num(); ++Index)
	{
		const int32 Sound = Sink->Voices[Index].Sound;
		TestNotNull(*FString::Printf(TEXT("%s is given its asset"), DTCat::Sounds[Sound].Id), Sink->Sounds[Index]);
		Heard.Add(Sound);
	}
	int32 Judged = 0;
	for (const ESoundId Id : { ESoundId::Snd_Warn_Line, ESoundId::Snd_Warn_Band, ESoundId::Snd_Warn_Ring, ESoundId::Snd_Warn_Arc, ESoundId::Snd_Danger_EnemyFire,
		ESoundId::Snd_Danger_HeroHurt, ESoundId::Snd_Danger_HeroDown, ESoundId::Snd_Danger_Heartbeat, ESoundId::Snd_Danger_BossPhase })
	{
		const int32 Sound = static_cast<int32>(Id);
		++Judged;
		TestTrue(*FString::Printf(TEXT("%s is playable"), DTCat::Sounds[Sound].Id), DTCat::Sounds[Sound].bPlayable);
		TestTrue(*FString::Printf(TEXT("%s was raised and started"), DTCat::Sounds[Sound].Id), Heard.Contains(Sound));
		TestEqual(*FString::Printf(TEXT("%s: missing 0"), DTCat::Sounds[Sound].Id), Audio->GetMissing(Sound), int64(0));
	}
	TestEqual(TEXT("the nine danger ids were judged"), Judged, 9);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTAudTestFirePlayable, "Dusktracer.Audio.FirePlayable", DTAudioTest::Flags) // T-AUD-19
bool FDTAudTestFirePlayable::RunTest(const FString& Parameters)
{
	using namespace DTAudioTest;
	// Each weapon's firing, raised by the fire family's own rule through a recording sink in place of the engine sink, gets its asset:
	// the playable fire rows are judged, and all ten weapons must be among them.
	DTAudioTest::FTestWorld Test;
	UDTAudioSubsystem* Audio = Test.World->GetSubsystem<UDTAudioSubsystem>();
	if (!TestNotNull(TEXT("the audio runtime"), Audio))
	{
		return false;
	}
	TSharedPtr<FDTRecordingSink> Sink = MakeShared<FDTRecordingSink>();
	Audio->SetSinkForTest(Sink);
	double Now = 100.0;
	const auto Frame = [&]() { Now += 5.0; Audio->RunFrame(Now); }; // five seconds apart: every voice ends, every interval passes
	Frame(); // the runtime meets the world's run first, so no request below is a run's leftover
	for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
	{
		FDTFireView Was;
		Was.Weapon[0] = Row;
		Was.Spawned[0] = 4;
		FDTFireView Fired = Was;
		Fired.Step = 1;
		Fired.Spawned[0] = 7;
		DTFireEvents::Diff(Was, Fired, [&](int32 Slot, int32 Sound, int32 Count) { Audio->RequestRun(Sound, Count, 0); });
		Frame();
	}
	TSet<int32> Heard;
	for (int32 Index = 0; Index < Sink->Voices.Num(); ++Index)
	{
		const int32 Sound = Sink->Voices[Index].Sound;
		TestNotNull(*FString::Printf(TEXT("%s is given its asset"), DTCat::Sounds[Sound].Id), Sink->Sounds[Index]);
		Heard.Add(Sound);
	}
	int32 Judged = 0;
	for (int32 Sound = 0; Sound < DTCat::SoundCount; ++Sound)
	{
		if (DTCat::Sounds[Sound].bPlayable && FString(DTCat::Sounds[Sound].Id).StartsWith(TEXT("Snd.Fire."), ESearchCase::CaseSensitive))
		{
			++Judged;
			TestTrue(*FString::Printf(TEXT("%s was raised and started"), DTCat::Sounds[Sound].Id), Heard.Contains(Sound));
			TestEqual(*FString::Printf(TEXT("%s: missing 0"), DTCat::Sounds[Sound].Id), Audio->GetMissing(Sound), int64(0));
		}
	}
	for (int32 Row = 0; Row < DTCat::NumWeapons; ++Row)
	{
		const int32 Sound = DTFireEvents::SoundOf(Row);
		TestTrue(*FString::Printf(TEXT("%s's fire sound is playable"), DTCat::Weapons[Row].Id), Sound != INDEX_NONE && DTCat::Sounds[Sound].bPlayable);
	}
	TestEqual(TEXT("the ten weapons' fire rows were judged"), Judged, 10);
	return true;
}
#endif

#endif // WITH_DEV_AUTOMATION_TESTS

