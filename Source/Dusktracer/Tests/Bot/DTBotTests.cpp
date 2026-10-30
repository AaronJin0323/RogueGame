// Bot v1 (S1 tool): it drives player 0 only through LatchInput and the offer API, deterministically.
// The long bot-run checks (T-DIR-01 no-pick death band, T-OFF-15 decision budget) live in Tools/BotRuns.ps1 -Check.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Bot; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Bot/DTBotBrain.h"
#include "Bot/DTBotSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTBotTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	/** Long enough for the Patron pick, the first spawns, hits, dashes and a level-up hand. */
	constexpr int32 RunSeconds = 75;

	struct FRunResult
	{
		uint32 InputHash = 0;
		int64 Inputs = 0;
		int64 Steps = 0;
		FVector2f Pos = FVector2f::ZeroVector;
		float Hp = 0.0f;
		int32 Dashes = 0;
		int32 Picks = 0;
		int32 Level = 0;
		int32 Weapons = 0;
		FString Csv;
		FString Jsonl;
	};

	/** Removes every "wall" / "wall_*" field (R-TEL-08: the only fields allowed to differ between identical runs). */
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

	static FRunResult Play(int32 Seed, EDTBotPolicy Policy)
	{
		FRunResult Result;
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTBotTestWorld")));
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();

		UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
		UDTBotSubsystem* Bot = World->GetSubsystem<UDTBotSubsystem>();
		UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
		UDTDirector* Director = World->GetSubsystem<UDTDirector>();
		const UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
		if (Sim && Bot && Telemetry && Director && Heroes)
		{
			Sim->StartRun(Seed, 1);
			Telemetry->StartSession(Seed, 1);
			Director->StartRun();
			FDTBotConfig Config;
			Config.Policy = Policy;
			Config.Path = EDTPath::Storm;
			Config.Archetype = FName(TEXT("Tempest"));
			Config.Skill = 0.6f; // exercises the noise stream and manual aim as well
			Config.Seed = Seed;
			Bot->Activate(Config);
			const int32 Steps = RunSeconds * DTConst::SIM_FIXED_HZ;
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
			Result.Inputs = Bot->GetInputCount();
			Result.Steps = Sim->GetStepIndex();
			Result.Picks = Bot->GetBrain().GetStats().Picks;
			if (const FDTHeroState* State = Heroes->GetState(0))
			{
				Result.Pos = State->Pos;
				Result.Hp = State->Hp;
				Result.Dashes = State->Dashes;
			}
			Result.Level = Sim->GetRun().GetPlayer(0).Level;
			Result.Weapons = Sim->GetRun().GetPlayer(0).Weapons.Num();
			Bot->Deactivate();
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
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotDeterminism, "Dusktracer.Bot.Determinism", DTBotTest::Flags)
bool FDTBotDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTBotTest;
	const FRunResult A = Play(21, EDTBotPolicy::Steered);
	const FRunResult B = Play(21, EDTBotPolicy::Steered);
	const FRunResult C = Play(22, EDTBotPolicy::Steered);

	TestTrue(TEXT("the bot latched one input per step (plus the first)"), A.Inputs >= A.Steps && A.Steps > 0);
	TestTrue(TEXT("the Patron hand was answered through the offer API"), A.Picks >= 1);
	TestTrue(TEXT("the hero owns a weapon after the Patron pick (D-015)"), A.Weapons >= 1);
	TestTrue(TEXT("the bot moved the hero"), !A.Pos.IsNearlyZero());
	TestTrue(TEXT("telemetry was written"), !A.Csv.IsEmpty() && !A.Jsonl.IsEmpty());

	TestEqual(TEXT("same seed: same number of steps"), A.Steps, B.Steps);
	TestEqual(TEXT("same seed: same input stream"), A.InputHash, B.InputHash);
	TestEqual(TEXT("same seed: same hero position"), A.Pos, B.Pos);
	TestEqual(TEXT("same seed: same HP"), A.Hp, B.Hp);
	TestEqual(TEXT("same seed: same dashes"), A.Dashes, B.Dashes);
	TestEqual(TEXT("same seed: same level"), A.Level, B.Level);
	TestEqual(TEXT("same seed: identical telemetry CSV"), A.Csv, B.Csv);
	TestTrue(TEXT("same seed: identical telemetry JSONL except wall fields"), StripWall(A.Jsonl) == StripWall(B.Jsonl));
	TestFalse(TEXT("the strip leaves no wall field"), StripWall(A.Jsonl).Contains(TEXT("\"wall")));
	TestNotEqual(TEXT("another seed gives another input stream"), A.InputHash, C.InputHash);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotCards, "Dusktracer.Bot.Cards", DTBotTest::Flags)
bool FDTBotCards::RunTest(const FString& Parameters)
{
	FDTPlayerRunState Player;
	auto Card = [](EDTCardKind Kind, const TCHAR* Id)
	{
		FDTOfferCard C;
		C.Kind = Kind;
		C.ContentId = FName(Id);
		return C;
	};
	FDTOfferHand Patron;
	Patron.HandKind = EDTHandKind::Patron;
	Patron.Cards = { Card(EDTCardKind::Keeper, TEXT("Keeper.Storm")), Card(EDTCardKind::Keeper, TEXT("Keeper.Forge")), Card(EDTCardKind::Keeper, TEXT("Keeper.Tide")) };
	FDTOfferHand Level;
	Level.HandKind = EDTHandKind::Level;
	Level.Cards = { Card(EDTCardKind::Reserve, TEXT("Reserve.Hone")), Card(EDTCardKind::Blessing, TEXT("Blessing.Forge.Slam.Key")),
		Card(EDTCardKind::Blessing, TEXT("Blessing.Forge.Hammer.Key")), Card(EDTCardKind::Mod, TEXT("Mod.SplitCore")) };
	FDTOfferHand Keeper;
	Keeper.HandKind = EDTHandKind::Keeper;
	Keeper.Cards = { Card(EDTCardKind::Keeper, TEXT("Keeper.Tide")), Card(EDTCardKind::Keeper, TEXT("Keeper.Storm")) };

	FDTBotBrain Brain;
	FDTBotConfig Config;
	Config.Policy = EDTBotPolicy::None;
	Brain.Reset(Config);
	TestEqual(TEXT("None picks the Patron (slot 0)"), Brain.ChooseCard(Patron, Player), 0);
	TestEqual(TEXT("None declines a level hand"), Brain.ChooseCard(Level, Player), static_cast<int32>(INDEX_NONE));

	Config.Policy = EDTBotPolicy::Steered;
	Config.Path = EDTPath::Forge;
	Config.Archetype = FName(TEXT("Hammer"));
	Brain.Reset(Config);
	TestEqual(TEXT("Steered takes its Keeper as Patron"), Brain.ChooseCard(Patron, Player), 1);
	TestEqual(TEXT("Steered takes its archetype's Key"), Brain.ChooseCard(Level, Player), 2);
	TestEqual(TEXT("Steered takes the fixed second Keeper (Forge -> Storm)"), Brain.ChooseCard(Keeper, Player), 1);

	Config.Policy = EDTBotPolicy::Random;
	Config.Seed = 5;
	Brain.Reset(Config);
	TArray<int32> First;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		First.Add(Brain.ChooseCard(Level, Player));
	}
	Brain.Reset(Config);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		TestEqual(TEXT("Random is seeded"), Brain.ChooseCard(Level, Player), First[Index]);
	}
	return true;
}

// The ring is a skill check for a hero who can see it. Standing far outside, past the look-ahead, every move lands in the
// hazard: the bot must still walk in, even with an enemy on the inside pushing its kite outward (S1_boss_after_fields.md).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotHazardRingInward, "Dusktracer.Bot.HazardRingInward", DTBotTest::Flags)
bool FDTBotHazardRingInward::RunTest(const FString& Parameters)
{
	FDTTelegraph Ring;
	Ring.Shape = EDTTelegraphShape::HazardRing;
	Ring.Length = 2800.0f;
	Ring.Progress = 1.0f;
	FDTHeroState Hero;
	Hero.Pos = FVector2f(2500.0f, 2500.0f); // 3535 uu out, clear of the wall band, which would push it in anyway
	Hero.Hp = Hero.MaxHp = 100.0f;
	const FVector2f EnemyPos[] = { FVector2f(1800.0f, 1800.0f) };
	const FVector2f EnemyVel[] = { FVector2f::ZeroVector };
	const float EnemyRadius[] = { 340.0f };
	FDTBotWorld World;
	World.Hero = &Hero;
	World.Telegraphs = MakeArrayView(&Ring, 1);
	World.EnemyPos = EnemyPos;
	World.EnemyVel = EnemyVel;
	World.EnemyRadius = EnemyRadius;
	FDTBotConfig Config;
	Config.Policy = EDTBotPolicy::None;
	FDTBotBrain Brain;
	Brain.Reset(Config);
	TestTrue(TEXT("danger grows with the distance outside the ring"), Brain.DangerAt(World, Hero.Pos, 0.0f) > Brain.DangerAt(World, Hero.Pos * 0.9f, 0.0f));
	const FDTHeroInput Input = Brain.Decide(World);
	TestTrue(TEXT("a hero far outside the ring walks in"), FVector2f::DotProduct(Input.Move, Hero.Pos.GetSafeNormal()) < -0.5f);
	return true;
}

// R-DIR-16 clamps a chest to the arena, so it can lie against a wall. The wall band must not stop the bot fetching it, or
// a run misses its chest hand and the magnet with it (S1_boss_after_fields.md, T-OFF-15).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotHazardRingThroughRounds, "Dusktracer.Bot.HazardRingThroughRounds", DTBotTest::Flags)
bool FDTBotHazardRingThroughRounds::RunTest(const FString& Parameters)
{
	// Rounds in the way cost one hit; the live ring costs one every interval until it is crossed. A hero outside it goes in
	// through rounds that would stop a walk anywhere else (S1_probes.md, the three ring deaths).
	FDTBotConfig Config;
	Config.Policy = EDTBotPolicy::Steered;
	Config.Seed = 5;
	FDTBotBrain Brain;
	Brain.Reset(Config);

	FDTHeroState Hero;
	Hero.Pos = FVector2f(-2300.0f, -2300.0f); // 3253 uu out, 453 past the ring, clear of the walls
	Hero.Hp = 60.0f;
	Hero.DashCharges = 0;
	const FVector2f In = (-Hero.Pos).GetSafeNormal();
	const FVector2f Across(-In.Y, In.X);

	FDTTelegraph Ring;
	Ring.Shape = EDTTelegraphShape::HazardRing;
	Ring.Origin = FVector2f::ZeroVector;
	Ring.Length = 2800.0f;
	Ring.Progress = 1.0f;

	// A curtain of rounds across the way in, drifting outward.
	TArray<FVector2f> RoundPos;
	TArray<FVector2f> RoundVel;
	TArray<float> RoundRadius;
	for (int32 Row = 0; Row < 3; ++Row)
	{
		for (int32 Col = -5; Col <= 5; ++Col)
		{
			RoundPos.Add(Hero.Pos + In * (160.0f + 120.0f * Row) + Across * (80.0f * Col));
			RoundVel.Add(-In * 200.0f);
			RoundRadius.Add(12.0f);
		}
	}
	FDTBotWorld World;
	World.Hero = &Hero;
	World.StepSeconds = 1.0f / DTConst::SIM_FIXED_HZ;
	World.StepIndex = 0;
	TArray<FDTTelegraph> Telegraphs = { Ring };
	World.Telegraphs = Telegraphs;
	World.RoundPos = RoundPos;
	World.RoundVel = RoundVel;
	World.RoundRadius = RoundRadius;
	const FDTHeroInput Input = Brain.Decide(World);
	const float Dot = FVector2f::DotProduct(Input.Move, In);
	TestTrue(*FString::Printf(TEXT("the bot walks in through the rounds (dot %.2f)"), Dot), Dot > 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotChestAtWall, "Dusktracer.Bot.ChestAtWall", DTBotTest::Flags)
bool FDTBotChestAtWall::RunTest(const FString& Parameters)
{
	FDTHeroState Hero;
	Hero.Pos = FVector2f(3350.0f, 0.0f);
	Hero.Hp = Hero.MaxHp = 100.0f;
	const FVector2f Chest[] = { FVector2f(3900.0f, 0.0f) };
	FDTBotWorld World;
	World.Hero = &Hero;
	World.Chests = Chest;
	FDTBotConfig Config;
	Config.Policy = EDTBotPolicy::None;
	FDTBotBrain Brain;
	Brain.Reset(Config);
	TestTrue(TEXT("walks through the wall band to a chest"), Brain.Decide(World).Move.X > 0.5f);

	// Only the wall gives way: a chest behind a landing telegraph still waits.
	FDTTelegraph Band;
	Band.Shape = EDTTelegraphShape::Band;
	Band.Origin = FVector2f(3500.0f, -500.0f);
	Band.Dir = FVector2f(0.0f, 1.0f);
	Band.Length = 1000.0f;
	Band.Width = 300.0f;
	Band.Progress = 1.0f;
	World.Telegraphs = MakeArrayView(&Band, 1);
	Brain.Reset(Config);
	TestTrue(TEXT("not through a telegraph"), Brain.Decide(World).Move.X <= 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotDodge, "Dusktracer.Bot.Dodge", DTBotTest::Flags)
bool FDTBotDodge::RunTest(const FString& Parameters)
{
	// Footprints.
	FDTTelegraph Band;
	Band.Shape = EDTTelegraphShape::Band;
	Band.Origin = FVector2f(0.0f, 0.0f);
	Band.Dir = FVector2f(1.0f, 0.0f);
	Band.Length = 1000.0f;
	Band.Width = 100.0f;
	TestTrue(TEXT("band: inside"), FDTBotBrain::InTelegraph(Band, FVector2f(500.0f, 20.0f), 0.0f));
	TestFalse(TEXT("band: beside"), FDTBotBrain::InTelegraph(Band, FVector2f(500.0f, 200.0f), 0.0f));
	FDTTelegraph Gap;
	Gap.Shape = EDTTelegraphShape::RingGap;
	Gap.Dir = FVector2f(0.0f, 1.0f);
	Gap.Length = 2000.0f;
	Gap.Width = 40.0f;
	TestFalse(TEXT("ring gap: the lane is safe"), FDTBotBrain::InTelegraph(Gap, FVector2f(0.0f, 800.0f), 40.0f));
	TestTrue(TEXT("ring gap: outside the lane is hit"), FDTBotBrain::InTelegraph(Gap, FVector2f(800.0f, 0.0f), 40.0f));
	FDTTelegraph Hazard;
	Hazard.Shape = EDTTelegraphShape::HazardRing;
	Hazard.Length = 1500.0f;
	TestTrue(TEXT("hazard ring: outside is hit"), FDTBotBrain::InTelegraph(Hazard, FVector2f(1600.0f, 0.0f), 0.0f));
	TestFalse(TEXT("hazard ring: inside is safe"), FDTBotBrain::InTelegraph(Hazard, FVector2f(100.0f, 0.0f), 0.0f));

	// Standing in a landing band: the bot steps out sideways; with a round about to hit it dashes.
	FDTHeroState Hero;
	Hero.Pos = FVector2f(500.0f, 0.0f);
	Hero.Hp = Hero.MaxHp = 100.0f;
	Hero.DashCharges = 1;
	FDTBotConfig Config;
	Config.Policy = EDTBotPolicy::Steered;
	FDTBotBrain Brain;
	Brain.Reset(Config);
	Band.Progress = 0.5f;
	const TArray<FDTTelegraph> Teles = { Band };
	FDTBotWorld World;
	World.Hero = &Hero;
	World.Telegraphs = Teles;
	const FDTHeroInput Out = Brain.Decide(World);
	TestTrue(TEXT("steps out of the band (across it)"), FMath::Abs(Out.Move.Y) > 0.6f);

	Brain.Reset(Config);
	const TArray<FVector2f> RoundPos = { FVector2f(500.0f, 120.0f) };
	const TArray<FVector2f> RoundVel = { FVector2f(0.0f, -900.0f) };
	const TArray<float> RoundRadius = { 12.0f };
	FDTBotWorld Shot;
	Shot.Hero = &Hero;
	Shot.RoundPos = RoundPos;
	Shot.RoundVel = RoundVel;
	Shot.RoundRadius = RoundRadius;
	FDTHeroState Still = Hero;
	Shot.Hero = &Still;
	const FDTHeroInput Dodge = Brain.Decide(Shot);
	TestTrue(TEXT("a round about to hit: the bot moves or dashes off its line"), Dodge.bDashPress || FMath::Abs(Dodge.Move.X) > 0.5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
