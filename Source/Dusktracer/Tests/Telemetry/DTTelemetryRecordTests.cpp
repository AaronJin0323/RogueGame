// S1 telemetry record tests (02_Systems/telemetry.md §Data; schemas in each owning spec's §Telemetry): every writer's
// serialized line, the S1 CSV columns, aggregation per step / interval, run_end extras and source registration.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Telemetry; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Async/ParallelFor.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyPool.h"
#include "Telemetry/DTTelemetryCore.h"
#include "Telemetry/DTTelemetryRecords.h"
#include "Telemetry/DTTelemetrySources.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTTelRecordTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float TelRecIntervalSeconds = 1.0f;

	static float StepSeconds() { return 1.0f / DTConst::SIM_FIXED_HZ; }
	static int32 StepsPerInterval() { return FMath::RoundToInt32(TelRecIntervalSeconds / StepSeconds()); }

	static FDTTelemetryConfig MakeConfig(int32 Players)
	{
		FDTTelemetryConfig Config;
		Config.Seed = 5;
		Config.NumPlayers = Players;
		Config.StepSeconds = StepSeconds();
		Config.IntervalSeconds = TelRecIntervalSeconds;
		Config.Build = TEXT("test");
		Config.Culture = TEXT("en");
		Config.WallStart = TEXT("2026-01-01T00:00:00.000Z");
		return Config;
	}

	/** The head every record starts with (telemetry.md §Data): t, step, kind, owner. */
	static FString Head(const TCHAR* Kind, int32 Owner, int64 Step = 0)
	{
		return FString::Printf(TEXT("{\"t\":%.4f,\"step\":%lld,\"kind\":\"%s\",\"owner\":%d"), static_cast<double>(Step) * StepSeconds(),
			static_cast<long long>(Step), Kind, Owner);
	}

	static TArray<FString> Lines(const FString& Text)
	{
		TArray<FString> Out;
		Text.ParseIntoArrayLines(Out, true);
		return Out;
	}

	/** Lines of one kind. */
	static TArray<FString> OfKind(const FString& Jsonl, const TCHAR* Kind)
	{
		const FString Needle = FString::Printf(TEXT("\"kind\":\"%s\""), Kind);
		TArray<FString> Out;
		for (const FString& Line : Lines(Jsonl))
		{
			if (Line.Contains(Needle))
			{
				Out.Add(Line);
			}
		}
		return Out;
	}

	static FString Cell(const FString& Row, const TCHAR* Column)
	{
		TArray<FString> Header;
		FDTTelemetryCore::GetCsvHeader().ParseIntoArray(Header, TEXT(","), false);
		TArray<FString> Cells;
		Row.ParseIntoArray(Cells, TEXT(","), false);
		const int32 Index = Header.IndexOfByKey(FString(Column));
		return Cells.IsValidIndex(Index) ? Cells[Index] : FString(TEXT("<missing>"));
	}

	/** The rate the core writes for Count events over Steps steps. */
	static FString Rate(double Count, int32 Steps, bool bShare = false)
	{
		const double PerUnit = Count * DTConst::TEL_RATE_UNIT_S / (static_cast<double>(Steps) * StepSeconds());
		return bShare ? FString::Printf(TEXT("%.4f"), PerUnit) : FString::Printf(TEXT("%.3f"), PerUnit);
	}

	/** Runs Steps empty steps with the given heroes. */
	static void Steps(FDTTelemetryCore& Core, int64 First, int32 Count, TConstArrayView<FVector2f> Heroes = TConstArrayView<FVector2f>())
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FDTTelemetrySample Sample;
			Sample.Heroes = Heroes;
			Core.EndStep(First + Index, Sample);
		}
	}

	static void Finish(FDTTelemetryCore& Core, FString& Csv, FString& Jsonl)
	{
		Core.End(TEXT("test"));
		Core.TakeOutput(Csv, Jsonl);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRecOffers, "Dusktracer.Telemetry.Records.Offers", DTTelRecordTest::Flags) // offers.md §Telemetry
bool FDTTelRecOffers::RunTest(const FString& Parameters)
{
	using namespace DTTelRecordTest;
	FDTTelemetryCore Core;
	Core.Start(MakeConfig(1));

	FDTPlayerOfferState Tools;
	Tools.RerollsLeft = 2;
	Tools.BanishLeft = 1;
	Tools.LockLeft = 0;

	FDTOfferHand Patron;
	Patron.HandId = 0;
	Patron.HandKind = EDTHandKind::Patron;
	FDTOfferCard Keeper;
	Keeper.Kind = EDTCardKind::Keeper;
	Keeper.ContentId = TEXT("Keeper.Storm");
	Patron.Cards = { Keeper };
	Core.RecordOffer(0, Patron, Tools);
	Core.RecordOfferPick(0, 0, 0, Keeper, 4.0);

	FDTOfferHand Hand;
	Hand.HandId = 3;
	Hand.HandKind = EDTHandKind::Level;
	Hand.Level = 4;
	Hand.bGuaranteeFilled = true;
	FDTOfferCard Weapon;
	Weapon.Kind = EDTCardKind::Weapon;
	Weapon.ContentId = TEXT("Weapon.Lance");
	Weapon.Weight = 2.5f;
	Weapon.bGuaranteed = true;
	FDTOfferCard Mod;
	Mod.Kind = EDTCardKind::Mod;
	Mod.ContentId = TEXT("Mod.Split");
	Mod.TargetWeapon = TEXT("Weapon.Lance");
	Mod.Rarity = EDTRarity::Rare;
	Mod.Weight = 1.25f;
	Mod.bLocked = true;
	FDTOfferCard Reserve;
	Reserve.Kind = EDTCardKind::Reserve;
	Reserve.ContentId = TEXT("Reserve.Mend");
	Hand.Cards = { Weapon, Mod, Reserve };
	Core.RecordOffer(0, Hand, Tools);
	Core.RecordOfferTool(0, 3, EDTTelOfferTool::Banish, 1, TEXT("Mod.Split"));
	Core.RecordOffer(0, Hand, Tools); // the banish refill shows the hand again: same hand, not a new one
	Core.RecordOfferTool(0, 3, EDTTelOfferTool::Reroll, INDEX_NONE, NAME_None);
	FDTOfferHand Rerolled = Hand;
	Rerolled.HandId = 4;
	Rerolled.RerollOf = 3;
	Core.RecordOffer(0, Rerolled, Tools);
	Core.RecordOfferPick(0, 4, 2, Reserve, 1.5);
	Core.RecordOffer(7, Hand, Tools); // not a player: no record

	FString Csv;
	FString Jsonl;
	Finish(Core, Csv, Jsonl);

	const FString Offer = Head(TEXT("offer"), 0) + TEXT(",\"hand_id\":3,\"hand_kind\":\"Level\",\"level\":4,\"reroll_of\":-1,\"guarantee_filled\":true,"
		"\"rerolls_left\":2,\"banish_left\":1,\"lock_left\":0,\"cards\":["
		"{\"slot\":0,\"kind\":\"Weapon\",\"id\":\"Weapon.Lance\",\"target\":null,\"rarity\":\"Common\",\"weight\":2.5000,\"guaranteed\":true,\"locked\":false,\"reserve\":false},"
		"{\"slot\":1,\"kind\":\"Mod\",\"id\":\"Mod.Split\",\"target\":\"Weapon.Lance\",\"rarity\":\"Rare\",\"weight\":1.2500,\"guaranteed\":false,\"locked\":true,\"reserve\":false},"
		"{\"slot\":2,\"kind\":\"Reserve\",\"id\":\"Reserve.Mend\",\"target\":null,\"rarity\":\"Common\",\"weight\":0.0000,\"guaranteed\":false,\"locked\":false,\"reserve\":true}]}");
	const TArray<FString> Offers = OfKind(Jsonl, TEXT("offer"));
	TestEqual(TEXT("every hand shown is logged, refills and rerolls included"), Offers.Num(), 4);
	TestTrue(TEXT("offer line"), Offers.IsValidIndex(1) && Offers[1] == Offer);
	TestTrue(TEXT("reroll names the hand it rerolled"), Offers.IsValidIndex(3) && Offers[3].Contains(TEXT("\"hand_id\":4,\"hand_kind\":\"Level\",\"level\":4,\"reroll_of\":3,")));

	const TArray<FString> Picks = OfKind(Jsonl, TEXT("offer_pick"));
	TestTrue(TEXT("offer_pick line"), Picks.IsValidIndex(1) && Picks[1] == Head(TEXT("offer_pick"), 0)
		+ TEXT(",\"hand_id\":4,\"slot\":2,\"card_kind\":\"Reserve\",\"id\":\"Reserve.Mend\",\"target\":null,\"rarity\":\"Common\",\"wall_decide_s\":1.5000}"));
	for (const FString& Pick : Picks)
	{
		// The key the record head owns is written once, so a JSON reader keeps both the record kind and the card kind.
		TestEqual(TEXT("offer_pick repeats no key"), Pick.Replace(TEXT("\"kind\":"), TEXT("")).Len(), Pick.Len() - FString(TEXT("\"kind\":")).Len());
	}
	const TArray<FString> Tool = OfKind(Jsonl, TEXT("offer_tool"));
	TestTrue(TEXT("offer_tool banish line"), Tool.IsValidIndex(0) && Tool[0] == Head(TEXT("offer_tool"), 0)
		+ TEXT(",\"hand_id\":3,\"tool\":\"banish\",\"slot\":1,\"id\":\"Mod.Split\"}"));
	TestTrue(TEXT("offer_tool reroll line"), Tool.IsValidIndex(1) && Tool[1] == Head(TEXT("offer_tool"), 0)
		+ TEXT(",\"hand_id\":3,\"tool\":\"reroll\",\"slot\":-1,\"id\":null}"));

	// run_end per player (offers.md): the Patron pick is no decision; cards and Reserve cards count every shown card.
	TestTrue(TEXT("run_end offer fields"), Jsonl.Contains(TEXT("\"per_player\":{\"0\":{\"decisions\":1,\"hands_by_kind\":{\"Level\":1,\"Chest\":0,"
		"\"Keeper\":0,\"Reforging\":0,\"Patron\":1},\"reserve_cards\":3,\"cards_offered\":10,\"wall_paused_s\":5.5000,")));
	TestTrue(TEXT("an owner that is not a player is counted, not logged"), Jsonl.Contains(TEXT("\"unowned_events\":1,")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRecKeepers, "Dusktracer.Telemetry.Records.Keepers", DTTelRecordTest::Flags) // keepers.md §Telemetry
bool FDTTelRecKeepers::RunTest(const FString& Parameters)
{
	using namespace DTTelRecordTest;
	FDTTelemetryCore Core;
	Core.Start(MakeConfig(2));

	FDTTelKeeperPick Pick;
	Pick.Blessing = TEXT("Blessing.Storm.Arc.Key");
	Pick.Path = EDTPath::Storm;
	Pick.Archetype = TEXT("Archetype.Storm.Arc");
	Pick.Role = EDTTelBlessingRole::Key;
	Pick.Favor = 1;
	Pick.Tier = 0;
	Pick.Leading = TEXT("Archetype.Storm.Arc");
	Core.RecordKeeperPick(1, Pick);
	Core.RecordKeeperCommit(0, EDTTelKeeperRole::Second, EDTPath::Tide, TEXT("Archetype.Storm.Arc"));
	FDTTelIntercession Cast;
	Cast.Base = TEXT("Intercession.Storm");
	Cast.Variant = TEXT("Archetype.Storm.Arc");
	Cast.EnemiesHit = 12;
	Cast.Damage = 340.5f;
	Core.RecordIntercession(0, Cast);
	Core.RecordStatusApplied(0, EDTStatus::Shock);
	Core.RecordStatusApplied(0, EDTStatus::Shock, 2);
	Core.RecordStatusApplied(0, EDTStatus::Freeze);
	Steps(Core, 0, StepsPerInterval());

	FString Csv;
	FString Jsonl;
	Finish(Core, Csv, Jsonl);
	TestTrue(TEXT("keeper_pick line"), Jsonl.Contains(Head(TEXT("keeper_pick"), 1) + TEXT(",\"blessing\":\"Blessing.Storm.Arc.Key\",\"path\":\"Storm\","
		"\"archetype\":\"Archetype.Storm.Arc\",\"role\":\"Key\",\"favor\":1,\"tier\":0,\"leading\":\"Archetype.Storm.Arc\"}\n")));
	TestTrue(TEXT("keeper_commit line"), Jsonl.Contains(Head(TEXT("keeper_commit"), 0)
		+ TEXT(",\"role\":\"Second\",\"path\":\"Tide\",\"primary_leading\":\"Archetype.Storm.Arc\"}\n")));
	TestTrue(TEXT("intercession line (no layer before the commitment)"), Jsonl.Contains(Head(TEXT("intercession"), 0)
		+ TEXT(",\"base\":\"Intercession.Storm\",\"variant\":\"Archetype.Storm.Arc\",\"layer\":null,\"enemies_hit\":12,\"damage\":340.5000}\n")));
	const TArray<FString> Status = OfKind(Jsonl, TEXT("status"));
	TestEqual(TEXT("one status record per player with procs per interval"), Status.Num(), 1);
	TestTrue(TEXT("status line at the interval end"), Status.IsValidIndex(0)
		&& Status[0] == Head(TEXT("status"), 0, StepsPerInterval()) + TEXT(",\"applied\":{\"Shock\":3,\"Freeze\":1}}"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRecCombat, "Dusktracer.Telemetry.Records.Combat", DTTelRecordTest::Flags) // combat_core.md §Telemetry, CSV S1 columns
bool FDTTelRecCombat::RunTest(const FString& Parameters)
{
	using namespace DTTelRecordTest;
	FDTTelemetryCore Core;
	Core.Start(MakeConfig(2));
	const TArray<FVector2f> Heroes = { FVector2f(0.0f, 0.0f), FVector2f(5000.0f, 0.0f) };
	const float Ring = static_cast<float>(DTConst::DIR_THREAT_RING_MAX_UU);

	Core.SetInputDevice(0, EDTTelInputDevice::Pad);
	Core.RecordDash(1, FVector2f(0.0f, -1.0f), 1);
	Core.RecordDash(1, FVector2f(0.6f, 0.8f), 0);
	FDTHeroHit Hit;
	Hit.PlayerId = 0;
	Hit.Damage = 10.0f;
	Hit.Kind = EDTHeroHitKind::Bullet;
	Hit.SourceRole = EDTEnemyRole::Turret;
	Hit.AttackId = TEXT("Burst");
	Core.RecordHeroDamaged(0, Hit, 10.0f, 90.0f, 100.0f);
	FDTHeroHit Contact;
	Core.RecordHeroDamaged(0, Contact, 5.0f, 85.0f, 100.0f);
	Core.RecordHeroDown(0);
	Core.RecordIntercessionDenied(1);
	Core.RecordIntercessionDenied(1);
	// Threat actions: two within P0's ring, one within P1's (at the edge), one in nobody's.
	Core.RecordThreatAction(FVector2f(500.0f, 0.0f), EDTEnemyRole::Rusher, TEXT("Charge"));
	Core.RecordThreatAction(FVector2f(0.0f, -Ring), EDTEnemyRole::Turret, TEXT("Burst"));
	Core.RecordThreatAction(FVector2f(5000.0f + Ring, 0.0f), EDTEnemyRole::Boss, TEXT("Ring"));
	Core.RecordThreatAction(FVector2f(2500.0f, 0.0f), EDTEnemyRole::Rusher, TEXT("Charge"));
	Core.RecordDash(7, FVector2f(1.0f, 0.0f), 0); // not a player
	Steps(Core, 0, StepsPerInterval(), Heroes);

	FString Csv;
	FString Jsonl;
	Finish(Core, Csv, Jsonl);
	TestTrue(TEXT("dash line"), Jsonl.Contains(Head(TEXT("dash"), 1) + TEXT(",\"dir_x\":0.6000,\"dir_y\":0.8000,\"charges_after\":0}\n")));
	TestTrue(TEXT("hero_damaged line"), Jsonl.Contains(Head(TEXT("hero_damaged"), 0)
		+ TEXT(",\"amount\":10.0000,\"hp_after\":90.0000,\"max_hp\":100.0000,\"hit_kind\":\"Bullet\",\"role\":\"Turret\",\"attack\":\"Burst\"}\n")));
	TestTrue(TEXT("contact hit has no attack id"), Jsonl.Contains(TEXT(",\"hit_kind\":\"Contact\",\"role\":\"Swarm\",\"attack\":null}\n")));
	TestTrue(TEXT("hero_down line"), Jsonl.Contains(Head(TEXT("hero_down"), 0) + TEXT("}\n")));
	TestTrue(TEXT("intercession_denied per interval"), Jsonl.Contains(Head(TEXT("intercession_denied"), 1, StepsPerInterval())
		+ TEXT(",\"interval\":0,\"count\":2}\n")));
	TestEqual(TEXT("a dash for a non-player is unowned"), Core.GetUnownedEvents(), static_cast<int64>(1));
	TestEqual(TEXT("run totals for per-block producers"), Core.GetRunDashes(1), 2);
	TestEqual(TEXT("run damage share"), Core.GetRunDamageTakenShare(0), 0.15, 1e-6);

	const TArray<FString> Rows = Lines(Csv);
	TestEqual(TEXT("header and one row per player"), Rows.Num(), 3);
	if (Rows.Num() == 3)
	{
		const int32 N = StepsPerInterval();
		TestEqual(TEXT("damage_taken_per_min is a share of max HP"), Cell(Rows[1], TEXT("damage_taken_per_min")), Rate(0.15, N, true));
		TestEqual(TEXT("P1 took nothing"), Cell(Rows[2], TEXT("damage_taken_per_min")), Rate(0.0, N, true));
		TestEqual(TEXT("P1 dashes"), Cell(Rows[2], TEXT("dashes_per_min")), Rate(2.0, N));
		TestEqual(TEXT("P0 no dashes"), Cell(Rows[1], TEXT("dashes_per_min")), Rate(0.0, N));
		TestEqual(TEXT("P0 threat actions within its ring"), Cell(Rows[1], TEXT("threat_actions_per_min")), Rate(2.0, N));
		TestEqual(TEXT("P1 threat actions within its ring"), Cell(Rows[2], TEXT("threat_actions_per_min")), Rate(1.0, N));
		TestEqual(TEXT("P0 input device"), Cell(Rows[1], TEXT("input_device")), FString(TEXT("pad")));
		TestEqual(TEXT("P1 never reported a device"), Cell(Rows[2], TEXT("input_device")), FString(TEXT("unknown")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRecProgression, "Dusktracer.Telemetry.Records.Progression", DTTelRecordTest::Flags) // progression.md §Telemetry
bool FDTTelRecProgression::RunTest(const FString& Parameters)
{
	using namespace DTTelRecordTest;
	FDTTelemetryCore Core;
	Core.Start(MakeConfig(2));
	Core.RecordGemsSpawned(10);
	Core.RecordXp(0, 1, 2);
	Core.RecordXp(0, 3, 4); // same step: one record (R-TEL-04)
	Core.RecordXp(1, 1, 1);
	Core.RecordGemsMerged(3);
	Core.RecordGemsMerged(2);
	Core.RecordLevelUp(0, 2, 6, 1);
	Core.SetGemsLive(4);
	const TArray<int32> Levels = { 2, 1 };
	const TArray<int64> Xp = { 6, 1 };
	FDTTelemetrySample Sample;
	Sample.Levels = Levels;
	Sample.XpTotals = Xp;
	Core.EndStep(0, Sample);
	Core.EndStep(1, Sample); // no pickups: no xp record

	FString Csv;
	FString Jsonl;
	Finish(Core, Csv, Jsonl);
	const TArray<FString> XpLines = OfKind(Jsonl, TEXT("xp"));
	TestEqual(TEXT("one xp record per player per step with pickups"), XpLines.Num(), 2);
	TestTrue(TEXT("xp line sums the step"), XpLines.IsValidIndex(0) && XpLines[0] == Head(TEXT("xp"), 0) + TEXT(",\"gems\":4,\"xp\":6}"));
	TestTrue(TEXT("level_up line"), Jsonl.Contains(Head(TEXT("level_up"), 0) + TEXT(",\"level\":2,\"xp_total\":6,\"pending_hands\":1}\n")));
	const TArray<FString> Merged = OfKind(Jsonl, TEXT("gems_merged"));
	TestTrue(TEXT("one gems_merged record (owner -1) per step"), Merged.Num() == 1 && Merged[0] == Head(TEXT("gems_merged"), -1) + TEXT(",\"count\":5}"));
	TestTrue(TEXT("run_end progression per player"), Jsonl.Contains(TEXT("\"wall_paused_s\":0.0000,\"level\":2,\"xp_total\":6},\"1\":{")));
	TestTrue(TEXT("run_end gems of the run"), Jsonl.Contains(TEXT(",\"gems_spawned\":10,\"gems_collected\":5,\"gems_left\":4}\n")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRecWeapons, "Dusktracer.Telemetry.Records.Weapons", DTTelRecordTest::Flags) // weapons.md, modifiers.md §Telemetry
bool FDTTelRecWeapons::RunTest(const FString& Parameters)
{
	using namespace DTTelRecordTest;
	const uint16 Lance = FDTTelemetrySources::RegisterWeapon(TEXT("TestRecLance"));
	const uint16 Orbit = FDTTelemetrySources::RegisterWeapon(TEXT("Weapon.TestRecOrbit"));

	FDTRunState Run;
	FDTWorldView View;
	FDTEnemyPool Pool;
	FDTTelemetryCore Core;
	Run.Start(3, 1);
	Run.SetEventSink(&Core);
	Core.Start(MakeConfig(1));
	Core.RecordWeaponAcquired(0, Orbit);
	Core.RecordWeaponAcquired(0, Lance);
	const FName ReforgeDormant[] = { FName(TEXT("Mod.TestRecA")), FName(TEXT("Mod.TestRecB")) };
	Core.RecordReforge(0, true, FName(TEXT("Evolve.TestRec")), FName(TEXT("Weapon.TestRec")), 7, ReforgeDormant);
	Core.RecordWeaponLevel(0, Lance, 3);
	Core.SetWeaponState(0, Lance, 3, 2);
	Core.RecordWeaponEmissions(0, Lance, 10, 2);
	Core.RecordWeaponEmissions(0, Lance, 5, 0);
	Core.RecordModifierSocketed(0, TEXT("Weapon.TestRecLance"), TEXT("Mod.Split"));
	Core.RecordModifierDormant(0, TEXT("Weapon.TestRecLance"), TEXT("Mod.Split"), true);

	// Two Lance hits on a live enemy arrive by source (R-TEL-05) and land in the weapon record.
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f::ZeroVector);
	Core.OnEnemiesSpawned(0, false, Pool.Num());
	FDTHit Hit;
	Hit.EnemyIndex = 0;
	Hit.OwnerId = 0;
	Hit.Damage = 1.0f;
	Hit.SourceId = Lance;
	View.Hits = { Hit, Hit };
	FDTSimContext Context;
	Context.StepSeconds = StepSeconds();
	Context.Run = &Run;
	Context.View = &View;
	Core.BeginResolve(0, View.Hits, Pool.GetSerials());
	Pool.ResolveHits(Context);
	Core.EndResolve(Pool.Num());
	Steps(Core, 0, 2 * StepsPerInterval());

	FString Csv;
	FString Jsonl;
	Finish(Core, Csv, Jsonl);
	TestTrue(TEXT("reforge line"), Jsonl.Contains(Head(TEXT("reforge"), 0) + TEXT(",\"reforge_kind\":\"Evolve\",\"id\":\"Evolve.TestRec\",\"target\":\"Weapon.TestRec\",\"weapon_level\":7,\"dormant\":\"Mod.TestRecA;Mod.TestRecB\"}\n")));
	TestTrue(TEXT("weapon_acquired line"), Jsonl.Contains(Head(TEXT("weapon_acquired"), 0) + TEXT(",\"source\":\"Weapon.TestRecLance\"}\n")));
	TestTrue(TEXT("weapon_level line"), Jsonl.Contains(Head(TEXT("weapon_level"), 0) + TEXT(",\"source\":\"Weapon.TestRecLance\",\"level\":3}\n")));
	TestTrue(TEXT("modifier_socketed line"), Jsonl.Contains(Head(TEXT("modifier_socketed"), 0)
		+ TEXT(",\"weapon\":\"Weapon.TestRecLance\",\"modifier\":\"Mod.Split\"}\n")));
	TestTrue(TEXT("modifier_dormant line"), Jsonl.Contains(Head(TEXT("modifier_dormant"), 0)
		+ TEXT(",\"weapon\":\"Weapon.TestRecLance\",\"modifier\":\"Mod.Split\",\"dormant\":true}\n")));

	const TArray<FString> Weapons = OfKind(Jsonl, TEXT("weapon"));
	TestEqual(TEXT("one weapon record per owned weapon per interval"), Weapons.Num(), 4);
	if (Weapons.Num() == 4)
	{
		const int32 N = StepsPerInterval();
		TestEqual(TEXT("first interval, Lance (by source name)"), Weapons[0], Head(TEXT("weapon"), 0, N)
			+ TEXT(",\"source\":\"Weapon.TestRecLance\",\"level\":3,\"load_rank\":2,\"spawned\":15,\"throttled\":2,\"hits\":2,\"damage\":2}"));
		TestEqual(TEXT("first interval, Orbit idle"), Weapons[1], Head(TEXT("weapon"), 0, N)
			+ TEXT(",\"source\":\"Weapon.TestRecOrbit\",\"level\":1,\"load_rank\":0,\"spawned\":0,\"throttled\":0,\"hits\":0,\"damage\":0}"));
		TestEqual(TEXT("second interval: level kept, counts reset"), Weapons[2], Head(TEXT("weapon"), 0, 2 * N)
			+ TEXT(",\"source\":\"Weapon.TestRecLance\",\"level\":3,\"load_rank\":2,\"spawned\":0,\"throttled\":0,\"hits\":0,\"damage\":0}"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRecDirector, "Dusktracer.Telemetry.Records.Director", DTTelRecordTest::Flags) // director.md R-DIR-15
bool FDTTelRecDirector::RunTest(const FString& Parameters)
{
	using namespace DTTelRecordTest;
	FDTTelemetryCore Core;
	Core.Start(MakeConfig(2));
	FDTTelDirBlock Block;
	Block.Block = 3;
	Block.Phase = TEXT("Threat");
	Block.RowStartSeconds = 120.0f;
	Block.StartSeconds = 120.0f;
	Block.EndSeconds = 150.5f;
	Block.EndedBy = TEXT("cleared");
	Block.SpawnedByRole[static_cast<int32>(EDTEnemyRole::Swarm)] = 40;
	Block.SpawnedByRole[static_cast<int32>(EDTEnemyRole::Rusher)] = 6;
	Block.SkippedAtCap = 2;
	Block.DropIns = 3;
	Block.ThreatActions = 9;
	Block.Kills = 44;
	Block.KillHorizonP50UU = 612.5f;
	Block.AlivePeak = 31;
	Block.DamageTakenShare = { 0.25f, 0.0f };
	Block.Dashes = { 4, 1 };
	Core.RecordDirBlock(Block);
	FDTTelDirBlock Breath;
	Breath.Phase = TEXT("Breath");
	Breath.EndedBy = TEXT("duration");
	Core.RecordDirBlock(Breath);

	FString Csv;
	FString Jsonl;
	Finish(Core, Csv, Jsonl);
	TestTrue(TEXT("dir_block line"), Jsonl.Contains(Head(TEXT("dir_block"), -1) + TEXT(",\"block\":3,\"phase\":\"Threat\",\"row_start_s\":120.0000,"
		"\"start_s\":120.0000,\"end_s\":150.5000,\"ended_by\":\"cleared\",\"spawned_by_role\":{\"Swarm\":40,\"Rusher\":6},\"skipped_at_cap\":2,"
		"\"dropins\":3,\"threat_actions\":9,\"kills\":44,\"kill_horizon_p50_uu\":612.5000,\"alive_peak\":31,"
		"\"dmg_taken\":{\"0\":0.2500,\"1\":0.0000},\"dashes\":{\"0\":4,\"1\":1}}\n")));
	TestTrue(TEXT("a block without kills has no horizon"), Jsonl.Contains(TEXT("\"spawned_by_role\":{},\"skipped_at_cap\":0,\"dropins\":0,"
		"\"threat_actions\":0,\"kills\":0,\"kill_horizon_p50_uu\":null,\"alive_peak\":0,\"dmg_taken\":{},\"dashes\":{}}\n")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelRecBoss, "Dusktracer.Telemetry.Records.Boss", DTTelRecordTest::Flags) // boss.md §Telemetry
bool FDTTelRecBoss::RunTest(const FString& Parameters)
{
	using namespace DTTelRecordTest;
	FDTTelemetryCore Core;
	Core.Start(MakeConfig(1));
	Core.RecordBossStart(TEXT("I"), 0, 1);
	Core.RecordBossCalibrated(850.25f, 120000.0f, false);
	Core.RecordBossPhase(2, 41.5f, 0.66f, false);
	FDTTelBossEnd End;
	End.Outcome = TEXT("win");
	End.FightTimeSeconds = 160.0f;
	End.HpLeftFrac = 0.0f;
	End.Phase = 3;
	FDTTelBossPlayer Player;
	Player.DamageTakenByAttack.Add(TEXT("Spiral"), 0.3f);
	Player.DamageTakenByAttack.Add(TEXT("Charge"), 0.125f);
	Player.DamageTakenByAttack.Add(NAME_None, 0.05f);
	Player.Dashes = 21;
	Player.HitsPerRing = 0.5f;
	End.PerPlayer = { Player };
	Core.RecordBossEnd(End);

	FString Csv;
	FString Jsonl;
	Finish(Core, Csv, Jsonl);
	TestTrue(TEXT("boss_start line"), Jsonl.Contains(Head(TEXT("boss_start"), -1) + TEXT(",\"form\":\"I\",\"wins\":0,\"players\":1}\n")));
	TestTrue(TEXT("boss_calibrated line"), Jsonl.Contains(Head(TEXT("boss_calibrated"), -1)
		+ TEXT(",\"window_dps\":850.2500,\"effective_max\":120000.0000,\"clamped\":false}\n")));
	TestTrue(TEXT("boss_phase line"), Jsonl.Contains(Head(TEXT("boss_phase"), -1)
		+ FString::Printf(TEXT(",\"phase\":2,\"fight_time_s\":41.5000,\"hp_frac\":%.4f,\"by_time\":false}\n"), 0.66f)));
	TestTrue(TEXT("boss_end line (attacks by name, contact named)"), Jsonl.Contains(Head(TEXT("boss_end"), -1)
		+ TEXT(",\"outcome\":\"win\",\"fight_time_s\":160.0000,\"hp_left_frac\":0.0000,\"phase\":3,\"per_player\":{\"0\":{\"damage_taken_by_attack\":"
			"{\"Charge\":0.1250,\"Contact\":0.0500,\"Spiral\":0.3000},\"dashes\":21,\"hits_per_ring\":0.5000}}}\n")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelSourceRegistration, "Dusktracer.Telemetry.SourceRegistration", DTTelRecordTest::Flags) // R-TEL-05
bool FDTTelSourceRegistration::RunTest(const FString& Parameters)
{
	const uint16 Weapon = FDTTelemetrySources::RegisterWeapon(TEXT("TestRegLance"));
	TestEqual(TEXT("Weapon.<Name>"), FDTTelemetrySources::GetName(Weapon), FString(TEXT("Weapon.TestRegLance")));
	TestEqual(TEXT("the content id spelling gives the same source"), FDTTelemetrySources::RegisterWeapon(TEXT("Weapon.TestRegLance")), Weapon);
	const uint16 Mod = FDTTelemetrySources::RegisterModifier(TEXT("Mod.TestRegSplit"));
	TestEqual(TEXT("Modifier.<Name> from a Mod.* id"), FDTTelemetrySources::GetName(Mod), FString(TEXT("Modifier.TestRegSplit")));
	TestEqual(TEXT("bare and Modifier.* spellings agree"), FDTTelemetrySources::RegisterModifier(TEXT("TestRegSplit")), Mod);
	TestEqual(TEXT("Modifier.* spelling"), FDTTelemetrySources::RegisterModifier(TEXT("Modifier.TestRegSplit")), Mod);
	TestEqual(TEXT("Keeper.<Path>"), FDTTelemetrySources::GetName(FDTTelemetrySources::RegisterKeeper(EDTPath::Forge)), FString(TEXT("Keeper.Forge")));
	TestEqual(TEXT("no Path is Unknown"), FDTTelemetrySources::RegisterKeeper(EDTPath::None), FDTTelemetrySources::Unknown);
	TestEqual(TEXT("the same source as FindOrAdd"), FDTTelemetrySources::FindOrAdd(TEXT("Keeper.Forge")), FDTTelemetrySources::RegisterKeeper(EDTPath::Forge));

	// Any package, any thread: concurrent registration of the same names gives one id per name.
	constexpr int32 Names = 8;
	constexpr int32 Calls = 256;
	TArray<uint16> Ids;
	Ids.Init(MAX_uint16, Calls);
	ParallelFor(Calls, [&Ids](int32 Index)
	{
		Ids[Index] = FDTTelemetrySources::RegisterWeapon(FName(*FString::Printf(TEXT("TestRegPar%d"), Index % Names)));
	});
	bool bStable = true;
	for (int32 Index = 0; Index < Calls; ++Index)
	{
		bStable &= Ids[Index] == Ids[Index % Names];
		bStable &= FDTTelemetrySources::GetName(Ids[Index]) == FString::Printf(TEXT("Weapon.TestRegPar%d"), Index % Names);
	}
	TestTrue(TEXT("concurrent registration is stable"), bStable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTelStaleSerial, "Dusktracer.Telemetry.StaleSerial", DTTelRecordTest::Flags) // R-TEL-03 with R-ENM-23
bool FDTTelStaleSerial::RunTest(const FString& Parameters)
{
	using namespace DTTelRecordTest;
	FDTRunState Run;
	FDTWorldView View;
	FDTEnemyPool Pool;
	FDTTelemetryCore Core;
	Run.Start(2, 1);
	Run.SetEventSink(&Core);
	Core.Start(MakeConfig(1));
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f::ZeroVector);
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f, 0.0f));
	Core.OnEnemiesSpawned(0, false, Pool.Num());
	const uint16 Keeper = FDTTelemetrySources::RegisterKeeper(EDTPath::Tide);

	// A hit made for an enemy that has since died (its serial no longer matches slot 0) is ignored by the pool.
	FDTHit Stale;
	Stale.EnemyIndex = 0;
	Stale.EnemySerial = Pool.GetSerials()[0] + 1000u;
	Stale.OwnerId = 0;
	Stale.Damage = static_cast<float>(DTConst::ENM_SWARM_HP) * 2.0f;
	Stale.SourceId = FDTTelemetrySources::Stress;
	FDTHit Live = Stale;
	Live.EnemyIndex = 1;
	Live.EnemySerial = Pool.GetSerials()[1];
	Live.SourceId = Keeper;
	View.Hits = { Stale, Live };
	FDTSimContext Context;
	Context.StepSeconds = StepSeconds();
	Context.StepIndex = 4;
	Context.Run = &Run;
	Context.View = &View;
	Core.BeginResolve(4, View.Hits, Pool.GetSerials());
	Pool.ResolveHits(Context);
	Core.EndResolve(Pool.Num());

	TestEqual(TEXT("no desync from a stale hit"), Core.GetMirrorDesyncs(), static_cast<int64>(0));
	const FDTTelemetrySourceTotals* ByKeeper = Core.GetRunSources(0).Find(Keeper);
	TestTrue(TEXT("the kill goes to the live hit's source"), ByKeeper && ByKeeper->Kills == 1);
	TestNull(TEXT("the stale hit credits nothing"), Core.GetRunSources(0).Find(FDTTelemetrySources::Stress));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
