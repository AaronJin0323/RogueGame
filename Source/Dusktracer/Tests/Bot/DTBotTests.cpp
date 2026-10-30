// The bot (Docs/02_Systems/bot.md): it drives player 0 only through LatchInput and the offer API, deterministically.
// The long bot-run checks (T-DIR-01 no-pick death band, T-OFF-15 decision budget) live in Tools/BotRuns.ps1 -Check.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Bot; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Bot/DTBotBrain.h"
#include "Bot/DTBotPerception.h"
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
#include "Enemies/DTEnemySubsystem.h"
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
		bool bManualAim = false;
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

	static FRunResult Play(int32 Seed, EDTBotPolicy Policy, int32 Version = 1)
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
			Config.Version = Version;
			Config.Skill = 0.6f; // exercises the noise stream and manual aim as well (v1), the profile (v2)
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
				Result.bManualAim = Heroes->GetAimMode(0) == EDTAimMode::Manual;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotDeterminism, "Dusktracer.Bot.Determinism", DTBotTest::Flags) // T-BOT-02
bool FDTBotDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTBotTest;
	const FRunResult A = Play(21, EDTBotPolicy::Steered);
	const FRunResult B = Play(21, EDTBotPolicy::Steered);
	const FRunResult C = Play(22, EDTBotPolicy::Steered);
	const FRunResult V2A = Play(21, EDTBotPolicy::Steered, 2);
	const FRunResult V2B = Play(21, EDTBotPolicy::Steered, 2);
	const FRunResult V2C = Play(22, EDTBotPolicy::Steered, 2);
	TestTrue(TEXT("v2 played"), V2A.Steps > 0 && V2A.Picks >= 1);
	TestEqual(TEXT("v2, same seed: same input stream"), V2A.InputHash, V2B.InputHash);
	TestEqual(TEXT("v2, same seed: same hero position"), V2A.Pos, V2B.Pos);
	TestEqual(TEXT("v2, same seed: identical telemetry CSV"), V2A.Csv, V2B.Csv);
	TestTrue(TEXT("v2, same seed: identical telemetry JSONL except wall fields"), StripWall(V2A.Jsonl) == StripWall(V2B.Jsonl));
	TestNotEqual(TEXT("v2, another seed gives another input stream"), V2A.InputHash, V2C.InputHash);
	TestNotEqual(TEXT("v2 is not v1 on the same seed"), V2A.InputHash, A.InputHash);
	TestTrue(TEXT("v1 below skill 1 stays in Manual aim"), A.bManualAim);
	TestFalse(TEXT("T-BOT-08: v2 below skill 1 stays in Auto aim"), V2A.bManualAim);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotProbeScript, "Dusktracer.Bot.ProbeScript", DTBotTest::Flags)
bool FDTBotProbeScript::RunTest(const FString& Parameters)
{
	const FString Text = TEXT("patron Storm\nsecond Tide\nbless Blessing.Storm.Tempest.Key\nweapon Weapon.PulseRifle 6 Mod.A Mod.B\nfield 120\nhp_scale 26.8\nwarmup_s 62\nwindow_s 12\ncast_at_s 62\n");
	FDTProbeScript Script;
	FString Error;
	TestTrue(TEXT("parses"), Script.Parse(Text, Error));
	TestTrue(TEXT("keepers"), Script.Patron == EDTPath::Storm && Script.Second == EDTPath::Tide);
	TestTrue(TEXT("weapon"), Script.Weapons.Num() == 1 && Script.Weapons[0].Level == 6 && Script.Weapons[0].Mods.Num() == 2);
	TestEqual(TEXT("field"), Script.Field, 120);
	TestEqual(TEXT("hp"), Script.HpScale, 26.8f, 0.001f);
	TestEqual(TEXT("cast"), Script.CastAtS, 62.0f, 0.001f);
	FDTProbeScript Dashes;
	TestTrue(TEXT("cast dashes"), Dashes.Parse(TEXT("patron Forge\ncast_dash_s 1\n"), Error) && FMath::IsNearlyEqual(Dashes.CastDashS, 1.0f));
	FDTProbeScript Again;
	TestTrue(TEXT("the build round-trips"), Again.Parse(Script.ToText(), Error) && Again.ToText() == Script.ToText());
	TestFalse(TEXT("an unknown key is refused"), Script.Parse(TEXT("patron Storm\nnonsense 1\n"), Error));
	TestFalse(TEXT("no patron is refused"), Script.Parse(TEXT("field 3\n"), Error));
	FDTProbeScript None;
	TestTrue(TEXT("no cast by default"), None.Parse(TEXT("patron Tide\n"), Error) && None.CastAtS < 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotProbeRouteCrowd, "Dusktracer.Bot.ProbeRouteFollowsCrowd", DTBotTest::Flags)
bool FDTBotProbeRouteCrowd::RunTest(const FString& Parameters)
{
	FDTHeroState Hero;
	Hero.Pos = FVector2f(400.0f, 0.0f);
	Hero.DashCharges = 2;
	FDTBotConfig Config;
	Config.bProbeRoute = true;
	Config.CastStep = 600;
	Config.CastDashSteps = 60;
	FDTBotBrain Brain;
	Brain.Reset(Config);
	FDTBotWorld World;
	World.Hero = &Hero;
	World.StepSeconds = 1.0f / DTConst::SIM_FIXED_HZ;
	World.StepIndex = DTConst::SIM_FIXED_HZ + 1; // a walking leg
	TArray<FVector2f> Far = { FVector2f(400.0f, -1500.0f) };
	World.EnemyPos = Far;
	TestTrue(TEXT("a walking leg with nothing in reach heads for the crowd"), FVector2f::DotProduct(Brain.Decide(World).Move, FVector2f(0.0f, -1.0f)) > 0.9f);
	TArray<FVector2f> Near = { FVector2f(400.0f, -200.0f) };
	World.EnemyPos = Near;
	TestTrue(TEXT("a crowd in reach leaves the orbit as it is"), FVector2f::DotProduct(Brain.Decide(World).Move, FVector2f(0.0f, 1.0f)) > 0.9f);
	World.StepIndex = 660;
	TestTrue(TEXT("during a cast with cast dashes the route dashes"), Brain.Decide(World).bDashPress);
	World.StepIndex = 420;
	TestTrue(TEXT("the plain schedule still dashes"), World.StepIndex % (6 * DTConst::SIM_FIXED_HZ) != 0 || Brain.Decide(World).bDashPress);
	World.StepIndex = 480 + 60;
	TestFalse(TEXT("no cast dash before the cast"), Brain.Decide(World).bDashPress);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotProbeScene, "Dusktracer.Bot.ProbeScene", DTBotTest::Flags)
bool FDTBotProbeScene::RunTest(const FString& Parameters)
{
	// The scene the probes stand on: the build and the HP-scaled field are in place, no hand opens, the hero cannot die,
	// and the cast is the script's: one press in the cast arm, none in the other, same seed.
	for (const bool bCast : { true, false })
	{
		FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTBotProbeWorld")));
		Context.SetCurrentWorld(World);
		World->InitializeActorsForPlay(FURL());
		World->BeginPlay();
		UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
		UDTBotSubsystem* Bot = World->GetSubsystem<UDTBotSubsystem>();
		UDTTelemetrySubsystem* Telemetry = World->GetSubsystem<UDTTelemetrySubsystem>();
		const UDTHeroSubsystem* Heroes = World->GetSubsystem<UDTHeroSubsystem>();
		const UDTEnemySubsystem* Enemies = World->GetSubsystem<UDTEnemySubsystem>();
		if (TestTrue(TEXT("systems"), Sim && Bot && Telemetry && Heroes && Enemies))
		{
			Sim->StartRun(7, 1);
			Telemetry->StartSession(7, 1);
			FDTProbeScript Script;
			FString Error;
			const FString Text = FString::Printf(TEXT("patron Storm\nsecond Tide\nbless Blessing.Storm.Tempest.Key\nbless Blessing.Tide.Frost.Key\nweapon Weapon.PulseRifle 3\nfield 30\nhp_scale 5\nwarmup_s %f\nwindow_s 2\n%s"),
				19.0f, bCast ? TEXT("cast_at_s 19\n") : TEXT(""));
			TestTrue(TEXT("script"), Script.Parse(Text, Error));
			Bot->StartProbe(Script, 7);
			const int32 MaxSteps = 25 * DTConst::SIM_FIXED_HZ;
			while (Sim->GetStepIndex() < MaxSteps && !Bot->IsRunFinished())
			{
				if (Bot->RunSteps(MaxSteps) == 0)
				{
					break;
				}
			}
			TestTrue(TEXT("the run ends with the window"), Bot->IsRunFinished() && !Bot->IsVictory());
			const FDTPlayerRunState& Player = Sim->GetRun().GetPlayer(0);
			TestTrue(TEXT("Patron and second"), Player.Keepers.Primary == EDTPath::Storm && Player.Keepers.Second == EDTPath::Tide && Player.Keepers.bCommitDone);
			TestEqual(TEXT("blessings"), Player.Keepers.Owned.Num(), 2);
			TestTrue(TEXT("weapon at its level"), Player.Weapons.Num() == 1 && Player.Weapons[0].Level == 3);
			TestEqual(TEXT("no hand answered after the Patron"), Bot->GetBrain().GetStats().Picks, 1);
			TestEqual(TEXT("the field stands"), Enemies->GetLiveCount(), 30);
			TestTrue(TEXT("the field has the script's HP scale"), Enemies->GetPool().GetHpScales().Num() > 0 && FMath::IsNearlyEqual(Enemies->GetPool().GetHpScales()[0], 5.0f));
			TestEqual(TEXT("casts"), Heroes->GetState(0)->Intercessions, bCast ? 1 : 0);
			TestFalse(TEXT("the hero is standing"), Heroes->GetState(0)->bDown);
			TestTrue(TEXT("the route stays in the crowd"), Heroes->GetState(0)->Pos.Size() < 800.0f);
			TestTrue(TEXT("the route dashes"), Heroes->GetState(0)->Dashes > 3);
			Telemetry->EndSession(TEXT("test"));
			IFileManager::Get().Delete(*Telemetry->GetWriter().GetCsvPath(), false, true, true);
			IFileManager::Get().Delete(*Telemetry->GetWriter().GetJsonlPath(), false, true, true);
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
	}
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

// ---- Bot v2 (bot.md) ----------------------------------------------------------------------------------------------------

namespace DTBotTest
{
	static FDTOfferCard MakeCard(EDTCardKind Kind, const TCHAR* Id, const TCHAR* Target = nullptr)
	{
		FDTOfferCard C;
		C.Kind = Kind;
		C.ContentId = FName(Id);
		C.TargetWeapon = Target ? FName(Target) : NAME_None;
		return C;
	}

	static FDTBotConfig V2Config(EDTPath Path, const TCHAR* Archetype, EDTPath Second, float Skill, int32 Seed = 5)
	{
		FDTBotConfig Config;
		Config.Policy = EDTBotPolicy::Steered;
		Config.Version = 2;
		Config.Path = Path;
		Config.Archetype = FName(Archetype);
		Config.SecondPath = Second;
		Config.Skill = Skill;
		Config.Seed = Seed;
		return Config;
	}

	/** Steps the brain with one enemy this far from the hero until its range samples stand (R-BOT-09). */
	static void SampleRange(FDTBotBrain& Brain, float Range)
	{
		static FDTHeroState Hero;
		Hero.Hp = Hero.MaxHp = 100.0f;
		const FVector2f Pos[] = { FVector2f(Range, 0.0f) };
		const FVector2f Vel[] = { FVector2f::ZeroVector };
		const float Radius[] = { 0.0f };
		FDTBotWorld World;
		World.Hero = &Hero;
		World.EnemyPos = Pos;
		World.EnemyVel = Vel;
		World.EnemyRadius = Radius;
		const int32 Steps = 30 * DTConst::SIM_FIXED_HZ;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			World.StepIndex = Step;
			Brain.Decide(World);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotVersion, "Dusktracer.Bot.Version", DTBotTest::Flags) // T-BOT-01
bool FDTBotVersion::RunTest(const FString& Parameters)
{
	FDTBotConfig Config;
	TestEqual(TEXT("the default is v1"), Config.Version, 1);
	TestEqual(TEXT("2 is known"), FDTBotConfig::KnownVersion(2), 2);
	TestEqual(TEXT("an unknown version is the default"), FDTBotConfig::KnownVersion(7), 1);
	TestTrue(TEXT("a record names its version"), Config.Describe().EndsWith(TEXT("version=1")));
	FDTBotBrain Brain;
	Config.Skill = 0.3f;
	Brain.Reset(Config);
	TestTrue(TEXT("v1 never reads the skill profile"), Brain.GetPerception().IsPassThrough() && Brain.GetProfile().HorizonS == FDTBotSkillProfile().HorizonS);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotSkillProfileTest, "Dusktracer.Bot.SkillProfile", DTBotTest::Flags) // T-BOT-03
bool FDTBotSkillProfileTest::RunTest(const FString& Parameters)
{
	FDTBotSkillProfile Last = FDTBotSkillProfile::FromSkill(0.0f);
	for (int32 Tenth = 1; Tenth <= 10; ++Tenth)
	{
		const FDTBotSkillProfile P = FDTBotSkillProfile::FromSkill(static_cast<float>(Tenth) / 10.0f);
		TestTrue(TEXT("reaction and hold shrink"), P.ReactionS < Last.ReactionS && P.HoldS < Last.HoldS);
		TestTrue(TEXT("fewer cues missed"), P.CueMissP < Last.CueMissP);
		TestTrue(TEXT("more attention, range and look-ahead"), P.AttentionRounds > Last.AttentionRounds && P.ThreatRangeUU > Last.ThreatRangeUU && P.HorizonS > Last.HorizonS);
		TestTrue(TEXT("less stick error, an earlier dash, more dash plans"), P.StickErrDeg < Last.StickErrDeg && P.ImminentS > Last.ImminentS && P.DashPlanP > Last.DashPlanP);
		TestTrue(TEXT("casts sooner, less taste, more awareness"), P.CastCrowd <= Last.CastCrowd && P.TasteScore < Last.TasteScore && P.FitAwareness > Last.FitAwareness);
		Last = P;
	}
	const FDTBotSkillProfile Top = FDTBotSkillProfile::FromSkill(1.0f);
	const FDTBotSkillProfile Defaults;
	TestTrue(TEXT("skill 1 is v1's heuristics exactly"), Top.HorizonS == Defaults.HorizonS && Top.ImminentS == Defaults.ImminentS && Top.ThreatRangeUU == Defaults.ThreatRangeUU
		&& Top.CastCrowd == Defaults.CastCrowd && Top.AttentionRounds == MAX_int32 && Top.ReactionS == 0.0f);

	// R-BOT-03: on the same worlds v2 at skill 1.0 presses what v1 presses.
	FDTBotConfig V1;
	V1.Policy = EDTBotPolicy::Steered;
	V1.Seed = 9;
	FDTBotConfig V2 = V1;
	V2.Version = 2;
	FDTBotBrain A;
	FDTBotBrain B;
	A.Reset(V1);
	B.Reset(V2);
	FDTHeroState Hero;
	Hero.Hp = Hero.MaxHp = 100.0f;
	Hero.DashCharges = 2;
	TArray<FVector2f> EnemyPos;
	TArray<FVector2f> EnemyVel;
	TArray<float> EnemyRadius;
	FRandomStream Rng(77);
	for (int32 Index = 0; Index < 40; ++Index)
	{
		EnemyPos.Add(FVector2f(Rng.FRandRange(-1200.0f, 1200.0f), Rng.FRandRange(-1200.0f, 1200.0f)));
		EnemyVel.Add(FVector2f(Rng.FRandRange(-300.0f, 300.0f), Rng.FRandRange(-300.0f, 300.0f)));
		EnemyRadius.Add(30.0f);
	}
	FDTBotWorld World;
	World.Hero = &Hero;
	World.EnemyPos = EnemyPos;
	World.EnemyVel = EnemyVel;
	World.EnemyRadius = EnemyRadius;
	bool bSame = true;
	for (int32 Step = 0; Step < 240; ++Step)
	{
		World.StepIndex = Step;
		for (int32 Index = 0; Index < EnemyPos.Num(); ++Index)
		{
			EnemyPos[Index] += EnemyVel[Index] * World.StepSeconds;
		}
		const FDTHeroInput InA = A.Decide(World);
		const FDTHeroInput InB = B.Decide(World);
		bSame &= InA.Move == InB.Move && InA.AimStick == InB.AimStick && InA.bDashPress == InB.bDashPress && InA.bIntercessionPress == InB.bIntercessionPress;
		Hero.Pos += InA.Move * 2.0f;
	}
	TestTrue(TEXT("v2 at skill 1.0 gives v1's inputs"), bSame);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotReaction, "Dusktracer.Bot.Reaction", DTBotTest::Flags) // T-BOT-04
bool FDTBotReaction::RunTest(const FString& Parameters)
{
	FDTHeroState Hero;
	const TArray<FVector2f> RoundPos = { FVector2f(500.0f, 0.0f) };
	const TArray<FVector2f> RoundVel = { FVector2f(-600.0f, 0.0f) };
	const TArray<float> RoundRadius = { 12.0f };
	for (const float Skill : { 1.0f, 0.3f })
	{
		const FDTBotSkillProfile Profile = FDTBotSkillProfile::FromSkill(Skill);
		const int32 Delay = FMath::RoundToInt32(Profile.ReactionS * DTConst::SIM_FIXED_HZ);
		FDTBotPerception Perception;
		Perception.Reset(Profile, 3);
		FDTBotWorld World;
		World.Hero = &Hero;
		World.StepSeconds = 1.0f / DTConst::SIM_FIXED_HZ;
		for (int32 Step = 0; Step < 40; ++Step)
		{
			World.StepIndex = Step;
			Perception.Push(World);
			Perception.Perceive(World);
		}
		// The round appears, then flies on; the bot sees it only once its frame is a reaction old.
		for (int32 Step = 0; Step <= Delay; ++Step)
		{
			const TArray<FVector2f> Moved = { RoundPos[0] + RoundVel[0] * World.StepSeconds * static_cast<float>(Step) };
			World.RoundPos = Moved;
			World.RoundVel = RoundVel;
			World.RoundRadius = RoundRadius;
			Perception.Push(World);
			const FDTBotWorld Seen = Perception.Perceive(World);
			TestEqual(TEXT("seen only after the reaction"), Seen.RoundPos.Num(), Step >= Delay ? 1 : 0);
			if (Step >= Delay)
			{
				TestTrue(TEXT("carried forward to where it is now"), FVector2f::Distance(Seen.RoundPos[0], Moved[0]) < 1.0f);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotCueMiss, "Dusktracer.Bot.CueMiss", DTBotTest::Flags) // T-BOT-05
bool FDTBotCueMiss::RunTest(const FString& Parameters)
{
	const FDTBotSkillProfile Profile = FDTBotSkillProfile::FromSkill(0.3f);
	FDTBotPerception Perception;
	Perception.Reset(Profile, 3);
	FDTHeroState Hero;
	FDTBotWorld World;
	World.Hero = &Hero;
	World.StepSeconds = 1.0f / DTConst::SIM_FIXED_HZ;
	constexpr int32 NumCues = 2000;
	for (int32 Index = 0; Index < NumCues; ++Index)
	{
		FDTTelegraph Tele;
		Tele.Origin = FVector2f(static_cast<float>(Index) * 400.0f, 0.0f);
		const TArray<FDTTelegraph> One = { Tele };
		World.Telegraphs = One;
		Perception.Push(World);
		Perception.Perceive(World);
	}
	const int32 Judged = Perception.GetCuesJudged();
	const float Share = static_cast<float>(Perception.GetCuesMissed()) / static_cast<float>(Judged);
	TestTrue(TEXT("every cue that reached the bot was judged"), Judged > NumCues * 0.9f);
	TestTrue(*FString::Printf(TEXT("missed share %.3f near %.3f"), Share, Profile.CueMissP), FMath::Abs(Share - Profile.CueMissP) < 0.04f);

	// One cue over many steps: one judgement, and its verdict holds to the end.
	FDTBotPerception Held;
	Held.Reset(Profile, 11);
	for (int32 Pass = 0; Pass < 40; ++Pass)
	{
		FDTTelegraph Tele;
		Tele.Origin = FVector2f(100.0f, 0.0f);
		Tele.Progress = static_cast<float>(Pass) / 40.0f;
		const TArray<FDTTelegraph> One = { Tele };
		World.Telegraphs = One;
		Held.Push(World);
		Held.Perceive(World);
	}
	TestEqual(TEXT("a cue is judged once"), Held.GetCuesJudged(), 1);

	// The ring is on screen for the whole phase.
	FDTBotPerception Ring;
	Ring.Reset(FDTBotSkillProfile::FromSkill(0.0f), 4);
	for (int32 Pass = 0; Pass < 300; ++Pass)
	{
		FDTTelegraph Tele;
		Tele.Shape = EDTTelegraphShape::HazardRing;
		Tele.Origin = FVector2f(static_cast<float>(Pass) * 400.0f, 0.0f);
		const TArray<FDTTelegraph> One = { Tele };
		World.Telegraphs = One;
		Ring.Push(World);
		Ring.Perceive(World);
	}
	TestTrue(TEXT("rings were judged"), Ring.GetCuesJudged() > 100);
	TestEqual(TEXT("the hazard ring is never missed"), Ring.GetCuesMissed(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotAttention, "Dusktracer.Bot.Attention", DTBotTest::Flags) // T-BOT-06
bool FDTBotAttention::RunTest(const FString& Parameters)
{
	const FDTBotSkillProfile Profile = FDTBotSkillProfile::FromSkill(0.0f);
	FDTBotPerception Perception;
	Perception.Reset(Profile, 3);
	FDTHeroState Hero;
	TArray<FVector2f> Pos;
	TArray<FVector2f> Vel;
	TArray<float> Radius;
	for (int32 Index = 0; Index < 50; ++Index)
	{
		Pos.Add(FVector2f(100.0f + static_cast<float>(Index) * 10.0f, 0.0f));
		Vel.Add(FVector2f::ZeroVector);
		Radius.Add(10.0f);
	}
	FDTBotWorld World;
	World.Hero = &Hero;
	World.EnemyPos = Pos;
	World.EnemyVel = Vel;
	World.EnemyRadius = Radius;
	World.RoundPos = Pos;
	World.RoundVel = Vel;
	World.RoundRadius = Radius;
	Perception.Push(World);
	const FDTBotWorld Seen = Perception.Perceive(World);
	TestEqual(TEXT("the nearest rounds only"), Seen.RoundPos.Num(), Profile.AttentionRounds);
	bool bNearest = true;
	for (const FVector2f& P : Seen.RoundPos)
	{
		bNearest &= P.X < 100.0f + static_cast<float>(Profile.AttentionRounds) * 10.0f;
	}
	TestTrue(TEXT("the kept rounds are the nearest"), bNearest);
	TestEqual(TEXT("every body is seen"), Seen.EnemyPos.Num(), Pos.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotStickError, "Dusktracer.Bot.StickError", DTBotTest::Flags) // T-BOT-07
bool FDTBotStickError::RunTest(const FString& Parameters)
{
	// A far chest straight ahead: the only thing moving the direction is the stick error.
	auto Angles = [](float Skill)
	{
		FDTBotBrain Brain;
		Brain.Reset(DTBotTest::V2Config(EDTPath::Storm, TEXT("Tempest"), EDTPath::None, Skill));
		FDTHeroState Hero;
		Hero.Hp = Hero.MaxHp = 100.0f;
		const FVector2f Chest[] = { FVector2f(3000.0f, 0.0f) };
		FDTBotWorld World;
		World.Hero = &Hero;
		World.Chests = Chest;
		TArray<float> Out;
		const int32 Hold = 1 + FMath::RoundToInt32(Brain.GetProfile().HoldS * DTConst::SIM_FIXED_HZ);
		for (int32 Step = 0; Step < 60000; ++Step)
		{
			World.StepIndex = Step;
			const FDTHeroInput In = Brain.Decide(World);
			if (Step % Hold == 0)
			{
				Out.Add(FMath::RadiansToDegrees(FMath::Atan2(In.Move.Y, In.Move.X)));
			}
		}
		return Out;
	};
	const TArray<float> Top = Angles(1.0f);
	bool bStraight = true;
	for (const float A : Top)
	{
		bStraight &= FMath::IsNearlyZero(A, 0.001f);
	}
	TestTrue(TEXT("no error at skill 1"), bStraight);

	const TArray<float> Low = Angles(0.3f);
	double Sum = 0.0;
	double Sum2 = 0.0;
	double Steps = 0.0;
	for (int32 Index = 0; Index < Low.Num(); ++Index)
	{
		Sum += Low[Index];
		Sum2 += Low[Index] * Low[Index];
		if (Index > 0)
		{
			Steps += FMath::Abs(Low[Index] - Low[Index - 1]);
		}
	}
	const double Mean = Sum / Low.Num();
	const double Spread = FMath::Sqrt(Sum2 / Low.Num() - Mean * Mean);
	const double MeanStep = Steps / (Low.Num() - 1);
	const float Want = FDTBotSkillProfile::FromSkill(0.3f).StickErrDeg;
	TestTrue(*FString::Printf(TEXT("spread %.1f near the profile's %.1f"), Spread, Want), FMath::Abs(Spread - Want) < Want * 0.25);
	TestTrue(*FString::Printf(TEXT("a drift, not a jitter: step %.1f, spread %.1f"), MeanStep, Spread), MeanStep < Spread * 0.75); // a fresh angle per decision steps about 1.13 spreads
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotAimAuto, "Dusktracer.Bot.AimAuto", DTBotTest::Flags) // T-BOT-08
bool FDTBotAimAuto::RunTest(const FString& Parameters)
{
	for (const float Skill : { 1.0f, 0.6f, 0.3f })
	{
		FDTBotBrain Brain;
		Brain.Reset(DTBotTest::V2Config(EDTPath::Storm, TEXT("Tempest"), EDTPath::None, Skill));
		FDTHeroState Hero;
		Hero.Hp = Hero.MaxHp = 100.0f;
		const FVector2f Pos[] = { FVector2f(300.0f, 40.0f) };
		FDTBotWorld World;
		World.Hero = &Hero;
		World.EnemyPos = Pos;
		for (int32 Step = 0; Step < 120; ++Step)
		{
			World.StepIndex = Step;
			TestTrue(TEXT("the aim stick stays neutral"), Brain.Decide(World).AimStick.IsNearlyZero());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotCardReach, "Dusktracer.Bot.CardReach", DTBotTest::Flags) // T-BOT-09
bool FDTBotCardReach::RunTest(const FString& Parameters)
{
	using namespace DTBotTest;
	FDTPlayerRunState Player;
	FDTWeaponInstance Rifle;
	Rifle.Id = FName(TEXT("Weapon.PulseRifle"));
	Player.Weapons.Add(Rifle);
	FDTWeaponInstance Blade;
	Blade.Id = FName(TEXT("Weapon.ArcBlade"));
	Blade.Level = 1;
	FDTOfferHand Hand;
	Hand.Cards = { MakeCard(EDTCardKind::Weapon, TEXT("Weapon.ArcBlade")), MakeCard(EDTCardKind::Weapon, TEXT("Weapon.OrbitalDrones")) };

	TestEqual(TEXT("the blade's reach is its radius"), FDTBotBrain::WeaponReach(FName(TEXT("Weapon.ArcBlade")), Player), 220.0f);
	TestEqual(TEXT("the rifle's reach is its range"), FDTBotBrain::WeaponReach(FName(TEXT("Weapon.PulseRifle")), Player), 900.0f);

	FDTBotBrain Far;
	Far.Reset(V2Config(EDTPath::Tide, TEXT("Stillness"), EDTPath::Storm, 1.0f));
	SampleRange(Far, 400.0f);
	TestEqual(TEXT("kiting past its reach, the seed 6 hand goes to the drones"), Far.ChooseCard(Hand, Player), 1);

	FDTOfferHand Upgrades;
	Upgrades.Cards = { MakeCard(EDTCardKind::WeaponUpgrade, TEXT("Weapon.ArcBlade"), TEXT("Weapon.ArcBlade")), MakeCard(EDTCardKind::WeaponUpgrade, TEXT("Weapon.PulseRifle"), TEXT("Weapon.PulseRifle")) };
	FDTPlayerRunState Both = Player;
	Both.Weapons.Add(Blade);
	TestEqual(TEXT("an upgrade of a weapon out of reach loses to a usable one"), Far.ChooseCard(Upgrades, Both), 1);
	const FDTOfferCard Mod = MakeCard(EDTCardKind::Mod, TEXT("Mod.SplitCore"), TEXT("Weapon.ArcBlade"));
	const FDTOfferCard ModRifle = MakeCard(EDTCardKind::Mod, TEXT("Mod.SplitCore"), TEXT("Weapon.PulseRifle"));
	TestTrue(TEXT("a modifier on it loses half as much"), Far.ScoreCard(Mod, Upgrades, Both) < Far.ScoreCard(ModRifle, Upgrades, Both));

	FDTBotBrain Close;
	Close.Reset(V2Config(EDTPath::Tide, TEXT("Stillness"), EDTPath::Storm, 1.0f));
	SampleRange(Close, 150.0f);
	TestTrue(TEXT("in reach the blade keeps its full score"), Close.ScoreCard(Hand.Cards[0], Hand, Player) > 70.0f);

	FDTBotBrain Weak;
	Weak.Reset(V2Config(EDTPath::Tide, TEXT("Stillness"), EDTPath::Storm, 0.0f));
	SampleRange(Weak, 400.0f);
	TestTrue(TEXT("skill 0 sees no reach fit"), FMath::Abs(Weak.ScoreCard(Hand.Cards[0], Hand, Player) - Weak.CardTaste(Hand.Cards[0].ContentId) - 80.0f) < 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotCardSecond, "Dusktracer.Bot.CardSecond", DTBotTest::Flags) // T-BOT-10
bool FDTBotCardSecond::RunTest(const FString& Parameters)
{
	using namespace DTBotTest;
	FDTBotBrain Brain;
	Brain.Reset(V2Config(EDTPath::Forge, TEXT("Hammer"), EDTPath::Storm, 1.0f));
	FDTPlayerRunState Player;
	Player.Keepers.Primary = EDTPath::Forge;
	Player.Keepers.Second = EDTPath::Storm;
	FDTOfferHand Hand;
	Hand.Cards = { MakeCard(EDTCardKind::WeaponUpgrade, TEXT("Weapon.PulseRifle"), TEXT("Weapon.PulseRifle")), MakeCard(EDTCardKind::Weapon, TEXT("Weapon.RingEmitter")),
		MakeCard(EDTCardKind::Blessing, TEXT("Blessing.Storm.Tempest.Key")) };
	TestEqual(TEXT("an unblessed second Keeper beats an upgrade and a new weapon"), Brain.ChooseCard(Hand, Player), 2);
	Hand.Cards.Add(MakeCard(EDTCardKind::Blessing, TEXT("Blessing.Forge.Hammer.Key")));
	TestEqual(TEXT("the archetype's own Key still comes first"), Brain.ChooseCard(Hand, Player), 3);

	Player.Keepers.Owned.Add(FName(TEXT("Blessing.Storm.Tempest.Key")));
	FDTOfferHand Next;
	Next.Cards = { MakeCard(EDTCardKind::WeaponUpgrade, TEXT("Weapon.PulseRifle"), TEXT("Weapon.PulseRifle")), MakeCard(EDTCardKind::Blessing, TEXT("Blessing.Storm.Tempest.Amp1")) };
	TestEqual(TEXT("after the first blessing it still beats an upgrade"), Brain.ChooseCard(Next, Player), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotCardTaste, "Dusktracer.Bot.CardTaste", DTBotTest::Flags) // T-BOT-11
bool FDTBotCardTaste::RunTest(const FString& Parameters)
{
	using namespace DTBotTest;
	const FName Rifle(TEXT("Weapon.PulseRifle"));
	FDTBotBrain Top;
	Top.Reset(V2Config(EDTPath::Storm, TEXT("Tempest"), EDTPath::None, 1.0f));
	TestEqual(TEXT("no taste at skill 1"), Top.CardTaste(Rifle), 0.0f);

	FDTBotBrain Low;
	Low.Reset(V2Config(EDTPath::Storm, TEXT("Tempest"), EDTPath::None, 0.3f, 5));
	TestEqual(TEXT("the same card reads the same across a run"), Low.CardTaste(Rifle), Low.CardTaste(Rifle));
	TestTrue(TEXT("inside the profile"), FMath::Abs(Low.CardTaste(Rifle)) <= Low.GetProfile().TasteScore);
	int32 Differs = 0;
	for (int32 Seed = 6; Seed < 16; ++Seed)
	{
		FDTBotBrain Other;
		Other.Reset(V2Config(EDTPath::Storm, TEXT("Tempest"), EDTPath::None, 0.3f, Seed));
		Differs += Other.CardTaste(Rifle) != Low.CardTaste(Rifle) ? 1 : 0;
	}
	TestTrue(TEXT("another seed tastes another way"), Differs > 5);
	TestNotEqual(TEXT("another card tastes another way"), Low.CardTaste(Rifle), Low.CardTaste(FName(TEXT("Weapon.RingEmitter"))));

	FDTPlayerRunState Player;
	FDTOfferHand Patron;
	Patron.HandKind = EDTHandKind::Patron;
	Patron.Cards = { MakeCard(EDTCardKind::Keeper, TEXT("Keeper.Tide")), MakeCard(EDTCardKind::Keeper, TEXT("Keeper.Storm")) };
	bool bArm = true;
	for (int32 Seed = 1; Seed < 40; ++Seed)
	{
		FDTBotBrain Other;
		Other.Reset(V2Config(EDTPath::Storm, TEXT("Tempest"), EDTPath::None, 0.0f, Seed));
		bArm &= Other.ChooseCard(Patron, Player) == 1;
	}
	TestTrue(TEXT("a Keeper hand takes no taste: the arm stays the arm"), bArm);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
