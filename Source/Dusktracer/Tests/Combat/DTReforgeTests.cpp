// L16 Reforging tests (02_Systems/reforging.md T-RFG-01..18).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Reforging; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Bot/DTBotBrain.h"
#include "Combat/DTReforge.h"
#include "Combat/DTWeaponCatalog.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferStyle.h"

namespace DTReforgeTest
{
	using namespace DTWeaponTestHarness;

	/** Weapon at level 8, evolved. Returns its row. */
	static int32 GiveEvolved(FDTHarness& H, const TCHAR* Weapon, int32 Level = 8)
	{
		int32 Slot = INDEX_NONE;
		H.Weapons.Give(H.Run, 0, FName(Weapon), Level, &Slot);
		FDTWeaponInstance& Owned = H.Run.GetPlayer(0).Weapons[Slot];
		Owned.Evolve = DTReforge::FindEvolve(Owned.Def);
		return Owned.Evolve;
	}

	static FDTWeaponFlags StatusFlag(EDTPath Path, const TCHAR* Status)
	{
		FDTWeaponFlags Flags;
		FDTFlagEntry& Entry = Flags.Add(DTCat::EFlag::StatusApply, DTWeapons::KeeperSource(Path), EDTFlagTier::Key);
		Entry.Params.SetWord(DTFlagParam::Status, FName(Status));
		Entry.Params.Set(DTFlagParam::Chance, 1.0f);
		return Flags;
	}

	/** The harness has no Enemies phase: statuses land here, as they do at the next step in the sim. */
	static void RunSteps(FDTHarness& H, int32 Steps)
	{
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			H.StepOnce();
			for (const FDTStatusApply& Apply : H.View.StatusApplies)
			{
				H.Enemies.ApplyStatus(Apply);
			}
			H.View.StatusApplies.Reset();
		}
	}

	static void FreezeEnemy(FDTHarness& H, int32 Enemy)
	{
		FDTStatusApply Apply;
		Apply.Handle = H.Enemies.GetHandle(Enemy);
		Apply.Status = EDTStatus::Freeze;
		Apply.OwnerId = 0;
		H.Enemies.ApplyStatus(Apply);
	}

	static int32 HitsOn(const FDTHarness& H, uint32 Serial, uint16 Source = 0)
	{
		int32 Count = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			Count += Hit.EnemySerial == Serial && (Source == 0 || Hit.SourceId == Source) ? 1 : 0;
		}
		return Count;
	}

	/** Hits of a source that claim a weapon slot: a Keeper hit claims none, so no proc feeds on it (keepers.md §1, R-KEE-13). */
	static int32 SlottedHits(const FDTHarness& H, uint16 Source)
	{
		int32 Count = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			Count += Hit.SourceId == Source && Hit.WeaponSlot != INDEX_NONE ? 1 : 0;
		}
		return Count;
	}

	/** A committed player: Storm Patron, Forge second, with Weapon at Level. */
	static void Commit(FDTHarness& H, const TCHAR* Weapon, int32 Level)
	{
		FDTPlayerRunState& P = H.Run.GetPlayer(0);
		P.Offer.PatronPath = P.Keepers.Primary = EDTPath::Storm;
		P.Offer.SecondPath = P.Keepers.Second = EDTPath::Forge;
		P.Keepers.bCommitDone = true;
		H.Weapons.Give(H.Run, 0, FName(Weapon), Level);
	}

	static FDTOfferHand ReforgeHand(FDTHarness& H)
	{
		FRandomStream Rng(5);
		return DTOffers::BuildHand(H.Run.GetPlayer(0), FDTOfferPool(), EDTHandKind::Reforging, DTConst::OFFER_REFORGING_LEVEL, Rng, {}, nullptr);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeCatalog, "Dusktracer.Reforging.Catalog", DTWeaponTestHarness::TestFlags) // T-RFG-01
bool FDTReforgeCatalog::RunTest(const FString& Parameters)
{
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		const int32 Row = DTReforge::FindEvolve(Weapon);
		TestTrue(FString::Printf(TEXT("%s has an Evolve row"), DTCat::Weapons[Weapon].Id), Row != INDEX_NONE);
		const FDTCapList Caps = DTReforge::Caps(Row);
		TestTrue(TEXT("an evolution declares caps"), Caps.Caps.Num() > 0);
		for (int32 Flag = 0; Flag < static_cast<int32>(DTCat::EFlag::Count); ++Flag)
		{
			TestTrue(FString::Printf(TEXT("%s resolves %s"), DTCat::Reforges[Row].Id, DTCat::FlagIds[Flag]),
				DTWeapons::ResolveFlag(Caps, static_cast<DTCat::EFlag>(Flag)).Compat != DTCat::ECompat::None);
		}
	}
	for (const EDTPath Path : { EDTPath::Storm, EDTPath::Forge, EDTPath::Tide, EDTPath::Shade, EDTPath::Radiance, EDTPath::Quicksilver })
	{
		TestTrue(FString::Printf(TEXT("Keeper %d has a Path row"), static_cast<int32>(Path)), DTReforge::FindPath(Path) != INDEX_NONE);
	}
	for (int32 Row = 0; Row < DTCat::NumReforges; ++Row)
	{
		const FName Id = DTReforge::RowId(Row);
		TestTrue(FString::Printf(TEXT("%s behaviour is known"), DTCat::Reforges[Row].Id),
			DTReforge::Evolution(Row) != EDTEvolve::None || DTReforge::PathReforge(Row) != EDTPathReforge::None);
		FDTOfferCard Card;
		Card.Kind = EDTCardKind::Reforging;
		Card.ContentId = Id;
		FString Table;
		FString Key;
		TestTrue(TEXT("a name key"), DTOfferText::NameKey(Card, Table, Key));
		TestFalse(FString::Printf(TEXT("%s has a name"), DTCat::Reforges[Row].Id), FText::FromStringTable(FName(Table), Key).IsEmpty());
		TestTrue(TEXT("a rules key"), DTOfferText::DescKey(Card, Table, Key));
		TestFalse(FString::Printf(TEXT("%s has a rules line"), DTCat::Reforges[Row].Id), DTOfferText::Desc(Card).IsEmpty());
		for (const FString& Keyword : DTOfferText::Keywords(Card))
		{
			TestFalse(FString::Printf(TEXT("%s: %s resolves"), DTCat::Reforges[Row].Id, *Keyword), DTOfferText::KeywordName(Keyword).IsEmpty());
		}
		TestTrue(FString::Printf(TEXT("%s (Path) has keyword notes"), DTCat::Reforges[Row].Id), DTCat::Reforges[Row].bEvolve || DTOfferText::Keywords(Card).Num() > 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeHand, "Dusktracer.Reforging.Hand", DTWeaponTestHarness::TestFlags) // T-RFG-02
bool FDTReforgeHand::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	{
		FDTHarness H;
		Commit(H, TEXT("Weapon.PulseRifle"), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL);
		const FDTOfferHand Hand = ReforgeHand(H);
		TestEqual(TEXT("card count"), Hand.Cards.Num(), DTConst::OFFER_REFORGING_CARDS);
		if (Hand.Cards.Num() == 3)
		{
			TestEqual(TEXT("slot one: the Patron's row"), Hand.Cards[0].ContentId, FName(TEXT("Transform.Storm")));
			TestEqual(TEXT("slot two: the second's row"), Hand.Cards[1].ContentId, FName(TEXT("Transform.Forge")));
			TestEqual(TEXT("slot three: the evolution"), Hand.Cards[2].ContentId, FName(TEXT("Evolve.PulseRifle")));
			TestEqual(TEXT("it binds the weapon"), Hand.Cards[2].TargetWeapon, FName(TEXT("Weapon.PulseRifle")));
			for (const FDTOfferCard& Card : Hand.Cards)
			{
				TestEqual(TEXT("a Reforging card"), Card.Kind, EDTCardKind::Reforging);
				TestEqual(TEXT("Epic"), Card.Rarity, EDTRarity::Epic);
				TestFalse(TEXT("no tools"), DTOffers::CardAllowsBanishLock(Card));
			}
		}
	}
	{
		FDTHarness H;
		Commit(H, TEXT("Weapon.PulseRifle"), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL - 1);
		const FDTOfferHand Hand = ReforgeHand(H);
		TestEqual(TEXT("under the level: still the count"), Hand.Cards.Num(), DTConst::OFFER_REFORGING_CARDS);
		TestFalse(TEXT("under the level: no evolution"), Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Evolve.PulseRifle")); }));
	}
	{
		FDTHarness H;
		Commit(H, TEXT("Weapon.PulseRifle"), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL);
		H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.ArcBlade")), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL + 1);
		const FDTOfferHand Hand = ReforgeHand(H);
		TestEqual(TEXT("the primary is rank one, not the first slot"), DTReforge::PrimarySlot(H.Run.GetPlayer(0).Weapons), 1);
		TestTrue(TEXT("its evolution is offered"), Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Evolve.ArcBlade")); }));
	}
	{
		FDTHarness H;
		Commit(H, TEXT("Weapon.PulseRifle"), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL);
		FDTPlayerRunState& P = H.Run.GetPlayer(0);
		P.PathReforges.Add(DTReforge::FindPath(EDTPath::Storm));
		P.Weapons[0].Evolve = DTReforge::FindEvolve(P.Weapons[0].Def);
		const FDTOfferHand Hand = ReforgeHand(H);
		TestFalse(TEXT("a taken row leaves"), Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Transform.Storm")); }));
		TestFalse(TEXT("an evolved weapon does not evolve again"), Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Evolve.PulseRifle")); }));
		TestEqual(TEXT("filler keeps the count"), Hand.Cards.Num(), DTConst::OFFER_REFORGING_CARDS);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgePick, "Dusktracer.Reforging.Pick", DTWeaponTestHarness::TestFlags) // T-RFG-03
bool FDTReforgePick::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	Commit(H, TEXT("Weapon.PulseRifle"), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL);
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.ArcBlade")), 1);
	FDTPlayerRunState& P = H.Run.GetPlayer(0);
	const FDTOfferHand Hand = ReforgeHand(H);
	if (Hand.Cards.Num() != 3)
	{
		AddError(TEXT("no hand"));
		return false;
	}
	const int32 Favor = P.Keepers.Favor[static_cast<int32>(EDTPath::Storm)];
	const FDTPickOutcome Path = DTOffers::ApplyPickState(P, Hand.HandKind, Hand.Cards[0]);
	TestTrue(TEXT("a Path pick is valid"), Path.bValid);
	TestEqual(TEXT("no Favor"), P.Keepers.Favor[static_cast<int32>(EDTPath::Storm)], Favor);
	TestEqual(TEXT("the row is owned"), DTReforge::OwnedPathRow(P, EDTPathReforge::Overload), DTReforge::FindPath(EDTPath::Storm));
	const FDTPickOutcome Evolve = DTOffers::ApplyPickState(P, Hand.HandKind, Hand.Cards[2]);
	TestTrue(TEXT("an Evolve pick is valid"), Evolve.bValid);
	TestEqual(TEXT("only that weapon evolves"), P.Weapons[0].Evolve, DTReforge::FindEvolve(P.Weapons[0].Def));
	TestEqual(TEXT("the other stays"), P.Weapons[1].Evolve, static_cast<int32>(INDEX_NONE));
	TestEqual(TEXT("upgrades still apply"), H.Weapons.LevelUp(H.Run, 0, 0), EDTWeaponResult::Ok);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeCaps, "Dusktracer.Reforging.Caps", DTWeaponTestHarness::TestFlags) // T-RFG-04
bool FDTReforgeCaps::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.OrbitalDrones")), 8);
	FDTPlayerRunState& P = H.Run.GetPlayer(0);
	int32 Mod = INDEX_NONE;
	const FDTCapList Evolved = DTReforge::Caps(DTReforge::FindEvolve(P.Weapons[0].Def));
	for (int32 Index = 0; Index < DTWeapons::NumModifiers() && Mod == INDEX_NONE; ++Index)
	{
		if (DTModCompat::IsCompatible(P.Weapons[0].Def, Index) && !DTModCompat::IsCompatible(Evolved, Index))
		{
			Mod = Index;
		}
	}
	if (Mod == INDEX_NONE)
	{
		AddError(TEXT("no modifier the Halo loses"));
		return false;
	}
	TestEqual(TEXT("socketed"), H.Weapons.Socket(H.Run, 0, 0, DTWeapons::ModifierId(Mod)), EDTWeaponResult::Ok);
	P.Weapons[0].Evolve = DTReforge::FindEvolve(P.Weapons[0].Def);
	H.StepOnce();
	const FDTWeaponSpec Spec = H.Weapons.GetLastSpec(0, 0);
	TestTrue(TEXT("the row's caps"), Spec.EffectiveCaps == Evolved);
	TestEqual(TEXT("still socketed"), P.Weapons[0].Sockets.Num(), 1);
	TestTrue(TEXT("dormant"), P.Weapons[0].Sockets[0].bDormant);
	TestEqual(TEXT("declared caps follow the row"), DTReforge::DeclaredCaps(P.Weapons[0]).Mask, Evolved.Mask);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeRailstorm, "Dusktracer.Reforging.Railstorm", DTWeaponTestHarness::TestFlags) // T-RFG-05
bool FDTReforgeRailstorm::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.PulseRifle"));
	H.AddEnemy(FVector2f(300.0f, 0.0f));
	const float Launch = DTReforge::Param(Row, TEXT("launch_s"));
	RunSteps(H, FMath::RoundToInt(10.0f * Launch / Dt));
	const FDTWeaponSpec& Spec = H.Weapons.GetLastSpec(0, 0);
	const DTCat::FWeaponLevelRow& L8 = *DTWeapons::GetLevelRow(Spec.Weapon, 8);
	TestTrue(TEXT("a boomerang"), Spec.bBoomerang);
	TestEqual(TEXT("the row's pierce"), Spec.Pierce, FMath::RoundToInt(DTReforge::Param(Row, TEXT("pierce"))));
	TestNearlyEqual(TEXT("one launch per launch_s"), H.Weapons.GetCounters(0, 0).Spawned, 10.0, 1.1);
	TestNearlyEqual(TEXT("the stated damage"), Spec.Damage, L8.Damage * L8.Count * DTReforge::Param(Row, TEXT("dmg_ratio")) * DTConst::LOAD_FACTOR_1, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeFlechette, "Dusktracer.Reforging.Flechette", DTWeaponTestHarness::TestFlags) // T-RFG-06
bool FDTReforgeFlechette::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.ScatterCannon"));
	const int32 Enemy = H.AddEnemy(FVector2f(120.0f, 0.0f));
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 0.0f;
	H.StepOnce();
	const FDTWeaponSpec& Spec = H.Weapons.GetLastSpec(0, 0);
	const DTCat::FWeaponLevelRow& L8 = *DTWeapons::GetLevelRow(Spec.Weapon, 8);
	TestEqual(TEXT("the pellet count"), Spec.Count, FMath::RoundToInt(DTReforge::Param(Row, TEXT("pellets"))));
	TestNearlyEqual(TEXT("the row's total"), Spec.Damage * Spec.Count, L8.Damage * L8.Count * DTConst::LOAD_FACTOR_1, 0.1f);
	// Long cooldown: read only the first volley's hits and the strikes that follow.
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
	RunSteps(H, FMath::RoundToInt(0.3f / Dt));
	const int32 Direct = HitsOn(H, H.Enemies.GetSerials()[Enemy]);
	TestTrue(TEXT("pellets struck"), Direct > 0);
	RunSteps(H, FMath::RoundToInt(DTReforge::Param(Row, TEXT("embed_s")) / Dt) + 2);
	TestTrue(TEXT("embedded strikes land later"), HitsOn(H, H.Enemies.GetSerials()[Enemy]) > Direct);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeBladeHalo, "Dusktracer.Reforging.BladeHalo", DTWeaponTestHarness::TestFlags) // T-RFG-07
bool FDTReforgeBladeHalo::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.OrbitalDrones"));
	const float Orbit = static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_RADIUS_UU);
	const int32 Near = H.AddEnemy(FVector2f(Orbit, 0.0f));
	const int32 Far = H.AddEnemy(FVector2f(Orbit + 400.0f, 0.0f));
	RunSteps(H, 300);
	TestEqual(TEXT("no bolts"), H.Bullets.Num(), 0);
	TestTrue(TEXT("a blade strikes what it touches"), HitsOn(H, H.Enemies.GetSerials()[Near]) > 0);
	TestEqual(TEXT("and nothing out of reach"), HitsOn(H, H.Enemies.GetSerials()[Far]), 0);
	TestEqual(TEXT("the blade count"), H.Weapons.GetLastSpec(0, 0).Count, FMath::RoundToInt(DTReforge::Param(Row, TEXT("blades"))));
	const float Still = H.Weapons.GetBladeRadius(0, 0);
	const FVector2f Speed(DTReforge::Param(Row, TEXT("speed_ref_uu_s")) * Dt, 0.0f);
	H.View.HeroPositions[0] += Speed;
	H.StepOnce();
	H.View.HeroPositions[0] += Speed;
	H.StepOnce();
	TestNearlyEqual(TEXT("the orbit widens with speed"), H.Weapons.GetBladeRadius(0, 0), Still * (1.0f + DTReforge::Param(Row, TEXT("speed_ratio"))), 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeSweepBeam, "Dusktracer.Reforging.SweepBeam", DTWeaponTestHarness::TestFlags) // T-RFG-08
bool FDTReforgeSweepBeam::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.RailLance"));
	const float Half = 0.5f * DTReforge::Param(Row, TEXT("sweep_deg"));
	const FVector2f Base(1.0f, 0.0f);
	const int32 Centre = H.AddEnemy(Base * 500.0f);
	const FVector2f Edge = Base.GetRotated(Half - 5.0f) * 500.0f;
	const int32 Side = H.AddEnemy(Edge);
	const int32 Behind = H.AddEnemy(-Base * 500.0f);
	H.StepOnce();
	TestTrue(TEXT("the beam starts on one side, not all at once"), HitsOn(H, H.Enemies.GetSerials()[Centre]) == 0);
	RunSteps(H, FMath::RoundToInt(DTReforge::Param(Row, TEXT("beam_s")) / Dt));
	TestTrue(TEXT("the sweep reaches its middle"), HitsOn(H, H.Enemies.GetSerials()[Centre]) > 0);
	TestTrue(TEXT("and its edge"), HitsOn(H, H.Enemies.GetSerials()[Side]) > 0);
	TestEqual(TEXT("not behind it"), HitsOn(H, H.Enemies.GetSerials()[Behind]), 0);
	TestEqual(TEXT("no rounds"), H.Bullets.Num(), 0);
	TestTrue(TEXT("the cooldown holds the beam"), H.Weapons.GetLastSpec(0, 0).Cooldown >= DTReforge::Param(Row, TEXT("beam_s")) - KINDA_SMALL_NUMBER);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeCarpet, "Dusktracer.Reforging.Carpet", DTWeaponTestHarness::TestFlags) // T-RFG-09
bool FDTReforgeCarpet::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.FlakMortar"), 1);
	const float Walk = DTReforge::Param(Row, TEXT("walk_uu"));
	const int32 Blasts = FMath::RoundToInt(DTReforge::Param(Row, TEXT("blasts")));
	for (int32 Index = 0; Index < 8; ++Index)
	{
		H.AddEnemy(FVector2f(300.0f, 0.0f)); // the densest cell, so the shell goes here
	}
	for (int32 Index = -6; Index <= 6; ++Index)
	{
		H.AddEnemy(FVector2f(300.0f + Walk, 80.0f * Index)); // a screen across the line's end, one enemy a cell
	}
	H.StepOnce();
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
	RunSteps(H, FMath::RoundToInt((1.0f + DTReforge::Param(Row, TEXT("step_s")) * Blasts) / Dt));
	float Furthest = 0.0f;
	for (const FDTHit& Hit : H.HitLog)
	{
		Furthest = FMath::Max(Furthest, Hit.Position.X);
	}
	TestTrue(TEXT("the line reaches its end"), Furthest > 300.0f + 0.9f * Walk);
	const int32 Settled = H.HitLog.Num();
	RunSteps(H, 120);
	TestEqual(TEXT("a strike does not walk"), H.HitLog.Num(), Settled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeCyclone, "Dusktracer.Reforging.Cyclone", DTWeaponTestHarness::TestFlags) // T-RFG-10
bool FDTReforgeCyclone::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.ArcBlade"));
	const int32 Front = H.AddEnemy(FVector2f(100.0f, 0.0f));
	const int32 Back = H.AddEnemy(FVector2f(-100.0f, 30.0f));
	RunSteps(H, 120);
	TestEqual(TEXT("at rest: nothing"), H.HitLog.Num(), 0);
	const float Speed = 2.0f * DTReforge::Param(Row, TEXT("move_min_uu_s"));
	for (int32 Step = 0; Step < static_cast<int32>(1.0f / Dt); ++Step)
	{
		H.View.HeroPositions[0].Y += Speed * Dt;
		H.StepOnce();
	}
	TestNearlyEqual(TEXT("the stated rate"), static_cast<float>(HitsOn(H, H.Enemies.GetSerials()[Front])), DTReforge::Param(Row, TEXT("hits_s")), 1.1f);
	TestTrue(TEXT("all round"), HitsOn(H, H.Enemies.GetSerials()[Back]) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeMurmuration, "Dusktracer.Reforging.Murmuration", DTWeaponTestHarness::TestFlags) // T-RFG-11
bool FDTReforgeMurmuration::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.SeekerSwarm"), 1);
	const int32 A = H.AddEnemy(FVector2f(200.0f, 0.0f));
	const int32 B = H.AddEnemy(FVector2f(200.0f, 300.0f));
	const int32 C = H.AddEnemy(FVector2f(-200.0f, 300.0f));
	H.StepOnce();
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
	const int32 Launched = H.Bullets.Num();
	TestTrue(TEXT("missiles left"), Launched > 0);
	RunSteps(H, FMath::RoundToInt(3.0f / Dt));
	TestEqual(TEXT("still alive past a plain missile's life"), H.Bullets.Num(), Launched);
	const FDTWeaponSpec& Spec = H.Weapons.GetLastSpec(0, 0);
	TestNearlyEqual(TEXT("the row's life"), Spec.Life, DTReforge::Param(Row, TEXT("life_s")), 0.01f);
	const int32 Total = HitsOn(H, H.Enemies.GetSerials()[A]) + HitsOn(H, H.Enemies.GetSerials()[B]) + HitsOn(H, H.Enemies.GetSerials()[C]);
	TestTrue(TEXT("a missile moved on"), Total > Launched);
	TestTrue(TEXT("once each at most"), Total <= Launched * 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgePulseWall, "Dusktracer.Reforging.PulseWall", DTWeaponTestHarness::TestFlags) // T-RFG-12
bool FDTReforgePulseWall::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.RingEmitter"));
	const float Radius = DTReforge::Param(Row, TEXT("radius_uu"));
	const int32 Rim = H.AddEnemy(FVector2f(Radius - 20.0f, 0.0f));
	const int32 Core = H.AddEnemy(FVector2f(0.0f, 60.0f));
	H.StepOnce();
	TestEqual(TEXT("one ring"), H.Weapons.GetLiveFields(), 1);
	TestEqual(TEXT("no rounds"), H.Bullets.Num(), 0);
	const FVector2f Before = H.Enemies.GetPositions()[Rim];
	bool bPushed = false;
	for (int32 Step = 0; Step < FMath::RoundToInt((DTReforge::Param(Row, TEXT("grow_s")) + 0.5f) / Dt); ++Step)
	{
		H.StepOnce();
		bPushed = bPushed || H.View.Displacements.ContainsByPredicate([&H, Rim](const FDTDisplacement& D) { return D.EnemySerial == H.Enemies.GetSerials()[Rim] && D.Delta.X > 0.0f; });
	}
	TestEqual(TEXT("still one ring"), H.Weapons.GetLiveFields(), 1);
	TestTrue(TEXT("the band hurts the rim"), HitsOn(H, H.Enemies.GetSerials()[Rim]) > 0);
	TestTrue(TEXT("and pushes it out through a displacement"), bPushed);
	TestEqual(TEXT("positions are not written by the weapon"), H.Enemies.GetPositions()[Rim], Before);
	TestEqual(TEXT("what the ring passed is left alone"), HitsOn(H, H.Enemies.GetSerials()[Core]), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeVortex, "Dusktracer.Reforging.Vortex", DTWeaponTestHarness::TestFlags) // T-RFG-13
bool FDTReforgeVortex::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	GiveEvolved(H, TEXT("Weapon.SpiralProjector"), 1);
	H.StepOnce();
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
	TestTrue(TEXT("a round left"), H.Bullets.Num() > 0);
	if (H.Bullets.Num() > 0)
	{
		const FVector2f V0 = (H.Bullets.GetPositions()[0] - H.Bullets.GetPrevPositions()[0]);
		H.StepOnce();
		const FVector2f V1 = (H.Bullets.GetPositions()[0] - H.Bullets.GetPrevPositions()[0]);
		TestTrue(TEXT("it curls"), FMath::Abs(V0.GetSafeNormal().Dot(V1.GetSafeNormal())) < 1.0f - 1.0e-6f);
	}
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
	RunSteps(H, FMath::RoundToInt(3.0f / Dt));
	TestEqual(TEXT("an expiry opened one zone, now closed or open, never two"), H.Weapons.GetLiveFields() <= 1, true);
	TestTrue(TEXT("a zone opened"), H.Weapons.GetFieldsOpened() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeSingularity, "Dusktracer.Reforging.Singularity", DTWeaponTestHarness::TestFlags) // T-RFG-14
bool FDTReforgeSingularity::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.GravityMine"), 8);
	H.AddEnemy(FVector2f(300.0f, 0.0f));
	H.AddEnemy(FVector2f(-300.0f, 0.0f));
	int32 MostLive = 0;
	for (int32 Step = 0; Step < 300; ++Step)
	{
		H.StepOnce();
		MostLive = FMath::Max(MostLive, H.Weapons.GetLiveMines(0, 0));
	}
	TestEqual(TEXT("one live mine"), MostLive, 1);
	const FDTWeaponSpec& Spec = H.Weapons.GetLastSpec(0, 0);
	const DTCat::FWeaponLevelRow& L8 = *DTWeapons::GetLevelRow(Spec.Weapon, 8);
	TestNearlyEqual(TEXT("the larger blast"), Spec.AreaRadius, L8.AreaR_uu * DTReforge::Param(Row, TEXT("blast_ratio")), 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeOverload, "Dusktracer.Reforging.Overload", DTWeaponTestHarness::TestFlags) // T-RFG-15
bool FDTReforgeOverload::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const int32 Row = DTReforge::FindPath(EDTPath::Storm);
	const uint16 Storm = DTWeapons::KeeperSource(EDTPath::Storm);
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
	H.Weapons.AddFlagSource(0, TEXT("Test.Shock"), StatusFlag(EDTPath::Storm, TEXT("Status.Shock")));
	H.Run.GetPlayer(0).PathReforges.Add(Row);
	H.AddEnemy(FVector2f(200.0f, 0.0f));
	const int32 Neighbour = H.AddEnemy(FVector2f(1200.0f, 100.0f));
	const int32 Near = H.AddEnemy(FVector2f(200.0f, 200.0f));
	// Only the closest is shot; a neighbour in range takes bolts, the far one nothing.
	RunSteps(H, 120);
	TestTrue(TEXT("a discharge reached its neighbour"), HitsOn(H, H.Enemies.GetSerials()[Near], Storm) > 0);
	TestEqual(TEXT("not past its range"), HitsOn(H, H.Enemies.GetSerials()[Neighbour], Storm), 0);
	const float Cooldown = DTReforge::Param(Row, TEXT("cooldown_s"));
	const int32 PerSecond = HitsOn(H, H.Enemies.GetSerials()[Near], Storm);
	TestTrue(TEXT("the cooldown holds"), static_cast<float>(PerSecond) <= 2.0f * 2.0f / Cooldown + 1.0f);
	TestEqual(TEXT("a discharge carries no weapon slot"), SlottedHits(H, Storm), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeKiln, "Dusktracer.Reforging.Kiln", DTWeaponTestHarness::TestFlags) // T-RFG-16
bool FDTReforgeKiln::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const int32 Row = DTReforge::FindPath(EDTPath::Forge);
	const uint16 Forge = DTWeapons::KeeperSource(EDTPath::Forge);
	for (const bool bBurn : { true, false })
	{
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
		if (bBurn)
		{
			H.Weapons.AddFlagSource(0, TEXT("Test.Burn"), StatusFlag(EDTPath::Forge, TEXT("Status.Burn")));
		}
		H.Run.GetPlayer(0).PathReforges.Add(Row);
		const int32 Near = H.AddEnemy(FVector2f(280.0f, 2000.0f), ToughHp); // first: removing the other leaves its index alone
		H.AddEnemy(FVector2f(200.0f, 0.0f), 40.0f);
		RunSteps(H, 60);
		TestEqual(FString::Printf(TEXT("a husk only from a Burning death (%d)"), bBurn), H.Weapons.GetLiveHusks() > 0, bBurn);
		H.Enemies.SetPosition(Near, FVector2f(200.0f, 40.0f));
		RunSteps(H, FMath::RoundToInt(1.0f / Dt));
		if (bBurn)
		{
			TestTrue(TEXT("a husk strikes"), HitsOn(H, H.Enemies.GetSerials()[Near], Forge) > 0);
			TestEqual(TEXT("a husk's cleave carries no weapon slot"), SlottedHits(H, Forge), 0);
			RunSteps(H, FMath::RoundToInt(DTReforge::Param(Row, TEXT("husk_s")) / Dt));
			TestEqual(TEXT("and ends"), H.Weapons.GetLiveHusks(), 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeGlacier, "Dusktracer.Reforging.Glacier", DTWeaponTestHarness::TestFlags) // T-RFG-17
bool FDTReforgeGlacier::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const int32 Row = DTReforge::FindPath(EDTPath::Tide);
	FDTHarness H;
	H.Run.GetPlayer(0).PathReforges.Add(Row);
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f; // no weapon strikes until asked
	const int32 Pillar = H.AddEnemy(FVector2f(300.0f, 0.0f));
	const int32 Walker = H.AddEnemy(FVector2f(305.0f, 0.0f));
	FreezeEnemy(H, Pillar);
	RunSteps(H, 2);
	TestEqual(TEXT("a pillar"), H.Weapons.GetLivePillars(), 1);
	FDTEnemyRoundSpawn Round;
	Round.Position = FVector2f(300.0f, 0.0f);
	Round.Velocity = FVector2f::ZeroVector;
	Round.Radius = 5.0f;
	Round.Life = 10.0f;
	H.Enemies.GetRounds().Spawn(Round);
	RunSteps(H, 2);
	TestEqual(TEXT("an enemy round stops at a pillar"), H.Enemies.GetRounds().Num(), 0);
	TestTrue(TEXT("a body is pushed out"), H.View.Displacements.Num() > 0 || FVector2f::Distance(H.Enemies.GetPositions()[Walker], H.Enemies.GetPositions()[Pillar]) > 5.0f);
	RunSteps(H, FMath::RoundToInt(DTConst::STATUS_FREEZE_S / Dt) + 2);
	if (DTReforge::Param(Row, TEXT("pillar_s")) > DTConst::STATUS_FREEZE_S + 0.1f)
	{
		TestTrue(TEXT("Freeze holds past its length"), H.Enemies.GetAux(Pillar).IsFrozen());
	}
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 0.0f;
	const uint16 Tide = DTWeapons::KeeperSource(EDTPath::Tide);
	RunSteps(H, 30);
	TestEqual(TEXT("a strike shatters it"), H.Weapons.GetLivePillars(), 0);
	TestTrue(TEXT("the shatter is the Keeper's"), H.CountHitsFrom(Tide) > 0);
	TestEqual(TEXT("a shatter carries no weapon slot"), SlottedHits(H, Tide), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeGlacierNeighbour, "Dusktracer.Reforging.GlacierNeighbour", DTWeaponTestHarness::TestFlags) // T-RFG-17
bool FDTReforgeGlacierNeighbour::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	// A shatter is a Keeper hit (keepers.md §1): it must not read as the owner's weapon striking the next pillar, or one hit takes a whole wall.
	const int32 Row = DTReforge::FindPath(EDTPath::Tide);
	FDTHarness H;
	H.Run.GetPlayer(0).PathReforges.Add(Row);
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
	const int32 Struck = H.AddEnemy(FVector2f(300.0f, 0.0f));
	const int32 Near = H.AddEnemy(FVector2f(300.0f, 80.0f));
	FreezeEnemy(H, Struck);
	FreezeEnemy(H, Near);
	RunSteps(H, 2);
	TestEqual(TEXT("two pillars"), H.Weapons.GetLivePillars(), 2);
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 0.0f; // the nearest is struck first
	int32 Steps = 0;
	while (H.Weapons.GetLivePillars() == 2 && Steps++ < 60)
	{
		RunSteps(H, 1);
	}
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
	RunSteps(H, 6);
	TestEqual(TEXT("a shatter leaves the next pillar standing"), H.Weapons.GetLivePillars(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeCollection, "Dusktracer.Reforging.Collection", DTWeaponTestHarness::TestFlags) // T-RFG-19
bool FDTReforgeCollection::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const int32 Row = DTReforge::FindPath(EDTPath::Shade);
	const uint16 Shade = DTWeapons::KeeperSource(EDTPath::Shade);
	uint32 NextSerial = 900000;
	const auto Kill = [&NextSerial](FDTHarness& H, const FVector2f& At, bool bMarked)
	{
		FDTKill Dead;
		Dead.EnemySerial = ++NextSerial;
		Dead.OwnerId = 0;
		Dead.Position = At;
		if (bMarked)
		{
			H.Weapons.ApplyMark(0, Dead.EnemySerial, H.Step);
		}
		H.PendingKills.Add(Dead);
		H.StepOnce();
	};
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.ArcBlade")), 8);
	H.Run.GetPlayer(0).PathReforges.Add(Row);
	H.StepOnce();
	Kill(H, FVector2f(5000.0f, 0.0f), false);
	TestEqual(TEXT("a plain death raises nothing"), H.Weapons.GetLiveShades(0), 0);
	Kill(H, FVector2f(5000.0f, 0.0f), true);
	TestEqual(TEXT("a Marked death raises a Shade"), H.Weapons.GetLiveShades(0), 1);
	TestEqual(TEXT("a Shade is not a hunter"), H.Weapons.GetLiveHunters(0), 0);
	const int32 Max = FMath::RoundToInt(DTReforge::Param(Row, TEXT("shades_max")));
	for (int32 Index = 0; Index < Max + 3; ++Index)
	{
		Kill(H, FVector2f(5000.0f, 0.0f), true);
	}
	TestEqual(TEXT("the cap holds, the oldest replaced"), H.Weapons.GetLiveShades(0), Max);

	// One Shade: it circles the hero and fires the first weapon, not the highest-load one, at the stated share.
	FDTHarness S;
	S.Weapons.Give(S.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
	S.Weapons.Give(S.Run, 0, FName(TEXT("Weapon.ArcBlade")), 8);
	S.Run.GetPlayer(0).PathReforges.Add(Row);
	S.StepOnce();
	Kill(S, FVector2f(5000.0f, 0.0f), true);
	S.AddEnemy(FVector2f(350.0f, 0.0f));
	S.HitLog.Reset();
	S.StepMany(FMath::RoundToInt(3.0f / Dt));
	TestTrue(TEXT("it fires"), S.CountHitsFrom(Shade) > 0);
	const FDTWeaponSpec& First = S.Weapons.GetLastSpec(0, 0);
	float Largest = 0.0f;
	for (const FDTHit& Hit : S.HitLog)
	{
		Largest = Hit.SourceId == Shade ? FMath::Max(Largest, Hit.Damage) : Largest;
	}
	TestTrue(TEXT("the stated share of the first weapon"), Largest <= First.Damage * First.CritMult * DTReforge::Param(Row, TEXT("dmg")) + 0.01f);
	TArray<FDTWeaponVisual> Seen;
	S.Weapons.GatherVisuals(S.Run, S.View, Seen);
	const FDTWeaponVisual* Core = Seen.FindByPredicate([](const FDTWeaponVisual& V) { return V.Kind == FDTWeaponVisual::EKind::Drone; });
	TestTrue(TEXT("it stays on the orbit"), Core && FMath::IsNearlyEqual(Core->Position.Size(), static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_RADIUS_UU), 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeZenith, "Dusktracer.Reforging.Zenith", DTWeaponTestHarness::TestFlags) // T-RFG-20
bool FDTReforgeZenith::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const int32 Row = DTReforge::FindPath(EDTPath::Radiance);
	const uint16 Radiance = DTWeapons::KeeperSource(EDTPath::Radiance);
	const int32 Lances = FMath::RoundToInt(DTReforge::Param(Row, TEXT("lances")));
	const int32 Every = FMath::RoundToInt(DTReforge::Param(Row, TEXT("every_n")));
	const auto Scene = [Row](bool bOwned, const TCHAR* Weapon, int32 Steps, FDTWeaponCounters& Out)
	{
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, FName(Weapon), 8);
		if (bOwned)
		{
			H.Run.GetPlayer(0).PathReforges.Add(Row);
		}
		for (int32 Index = 0; Index < 5; ++Index)
		{
			H.AddEnemy(FVector2f(200.0f, -200.0f + 100.0f * Index));
		}
		H.StepMany(Steps);
		Out = H.Weapons.GetCounters(0, 0);
		return H.CountHitsFrom(DTWeapons::KeeperSource(EDTPath::Radiance));
	};
	FDTWeaponCounters Plain;
	FDTWeaponCounters Owned;
	const int32 Steps = FMath::RoundToInt(6.0f / Dt);
	TestEqual(TEXT("not owned: no charged shot, no lance"), Scene(false, TEXT("Weapon.PulseRifle"), Steps, Plain), 0);
	TestTrue(TEXT("owned: a build with no Charge card still charges and splits"), Scene(true, TEXT("Weapon.PulseRifle"), Steps, Owned) > 0);
	TestTrue(TEXT("lances are extra rounds"), Owned.Spawned >= Plain.Spawned + Lances);
	FDTWeaponCounters Arc;
	FDTWeaponCounters ArcPlain;
	Scene(false, TEXT("Weapon.ArcBlade"), Steps, ArcPlain);
	TestTrue(TEXT("a melee weapon charges too"), Scene(true, TEXT("Weapon.ArcBlade"), Steps, Arc) > 0);
	TestTrue(TEXT("and fires lances"), Arc.Spawned >= ArcPlain.Spawned + Lances);

	// A lance neither splits nor charges again; an owner of Flag.Charge keeps its own every_n.
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 8);
	H.Run.GetPlayer(0).PathReforges.Add(Row);
	FDTWeaponFlags Own;
	FDTFlagEntry& Entry = Own.Add(DTCat::EFlag::Charge, Radiance, EDTFlagTier::Key);
	Entry.Params.Set(DTFlagParam::EveryN, static_cast<float>(Every + 1));
	Entry.Params.Set(DTFlagParam::Mult, 2.0f);
	H.Weapons.AddFlagSource(0, TEXT("Test.Charge"), Own);
	H.AddEnemy(FVector2f(200.0f, 0.0f));
	H.StepMany(Steps);
	TestEqual(TEXT("a lance never splits"), H.Weapons.GetMaxGeneration(), 1);
	TestEqual(TEXT("the card's Charge wins over the row's"), H.Weapons.GetLastSpec(0, 0).FindFlag(DTCat::EFlag::Charge)->Entry.Params.Get(DTFlagParam::EveryN), static_cast<float>(Every + 1));

	// Overkill bursts in light: a weak enemy's death hits its neighbour.
	FDTHarness N;
	N.Weapons.Give(N.Run, 0, FName(TEXT("Weapon.PulseRifle")), 8);
	N.Run.GetPlayer(0).PathReforges.Add(Row);
	const int32 Next = N.AddEnemy(FVector2f(200.0f, 0.5f * DTReforge::Param(Row, TEXT("nova_uu"))), ToughHp); // first: removing the other leaves its index alone
	N.AddEnemy(FVector2f(100.0f, 0.0f), 1.0f); // the nearer: it is shot first
	RunSteps(N, FMath::RoundToInt(1.0f / Dt));
	TestTrue(TEXT("an overkill bursts"), HitsOn(N, N.Enemies.GetSerials()[Next], Radiance) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeSlipstream, "Dusktracer.Reforging.Slipstream", DTWeaponTestHarness::TestFlags) // T-RFG-21
bool FDTReforgeSlipstream::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const int32 Row = DTReforge::FindPath(EDTPath::Quicksilver);
	const auto Dash = [](FDTHarness& H)
	{
		H.View.HeroIntangible[0] = true;
		H.StepOnce();
		H.View.HeroIntangible[0] = false;
		H.StepOnce();
	};
	const auto Scene = [Row, &Dash](bool bOwned)
	{
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
		if (bOwned)
		{
			H.Run.GetPlayer(0).PathReforges.Add(Row);
		}
		H.AddEnemy(FVector2f(100.0f, 0.0f));
		H.StepOnce();
		Dash(H);
		H.StepMany(60);
		return H.CountHitsFrom(DTWeapons::KeeperSource(EDTPath::Quicksilver));
	};
	TestEqual(TEXT("not owned: a dash fires nothing"), Scene(false), 0);
	TestTrue(TEXT("owned: a dash fires every weapon"), Scene(true) > 0);

	FDTHarness H;
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
	H.Run.GetPlayer(0).PathReforges.Add(Row);
	H.StepOnce();
	H.StepOnce();
	TestEqual(TEXT("a dash charge more"), H.Weapons.GetDuoHero(0).DashCharges, FMath::RoundToInt(DTReforge::Param(Row, TEXT("dash_charges"))));
	const float Base = H.Weapons.GetLastSpec(0, 0).Damage;
	float Previous = 0.0f;
	float Peak = 0.0f;
	const float Stride = static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S) * Dt;
	for (int32 Step = 0; Step < FMath::RoundToInt(10.0f / Dt); ++Step)
	{
		H.View.HeroPositions[0].X += Stride;
		H.StepOnce();
		Peak = H.Weapons.GetMomentum(0);
		if (Step == FMath::RoundToInt(1.0f / Dt))
		{
			Previous = Peak;
		}
	}
	TestTrue(TEXT("power rises with distance"), Peak > Previous && Previous > 0.0f);
	TestTrue(TEXT("past any card's cap"), Peak > 30.0f);
	TestTrue(TEXT("it reaches the weapons"), H.Weapons.GetLastSpec(0, 0).Damage > Base);
	const float Slow = Peak;
	FDTHarness Q;
	Q.Weapons.Give(Q.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
	Q.Run.GetPlayer(0).PathReforges.Add(Row);
	Q.StepOnce();
	for (int32 Step = 0; Step < FMath::RoundToInt(30.0f / Dt); ++Step)
	{
		Q.View.HeroPositions[0].X += 2.0f * Stride;
		Q.StepOnce();
	}
	TestTrue(TEXT("faster goes higher, and the leak bounds it"), Q.Weapons.GetMomentum(0) > Slow &&
		Q.Weapons.GetMomentum(0) < 2.0f * static_cast<float>(DTConst::HERO_MOVE_SPEED_UU_S) / 100.0f * DTReforge::Param(Row, TEXT("per_100uu")) / DTReforge::Param(Row, TEXT("leak_per_s")) + 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgePulseWallLook, "Dusktracer.Reforging.PulseWallLook", DTWeaponTestHarness::TestFlags) // T-RFG-12
bool FDTReforgePulseWallLook::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.RingEmitter"));
	H.AddEnemy(FVector2f(300.0f, 0.0f));
	H.StepMany(FMath::RoundToInt((DTReforge::Param(Row, TEXT("grow_s")) + 0.2f) / Dt));
	TArray<FDTWeaponVisual> Seen;
	H.Weapons.GatherVisuals(H.Run, H.View, Seen);
	const float Radius = DTReforge::Param(Row, TEXT("radius_uu"));
	TestFalse(TEXT("the wall is no filled disc"), Seen.ContainsByPredicate([](const FDTWeaponVisual& V) { return V.Kind == FDTWeaponVisual::EKind::Field; }));
	TestTrue(TEXT("it draws its outer edge"), Seen.ContainsByPredicate([Radius](const FDTWeaponVisual& V)
	{
		return V.Kind == FDTWeaponVisual::EKind::Arc && V.ArcDeg >= 360.0f && FMath::IsNearlyEqual(V.Radius, Radius, 1.0f);
	}));
	const float Inner = Radius - DTReforge::Param(Row, TEXT("band_uu"));
	TestTrue(TEXT("and the inner edge of what hurts"), Seen.ContainsByPredicate([Inner](const FDTWeaponVisual& V)
	{
		return V.Kind == FDTWeaponVisual::EKind::Arc && V.ArcDeg >= 360.0f && FMath::IsNearlyEqual(V.Radius, Inner, 1.0f);
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeCycloneStillness, "Dusktracer.Reforging.CycloneStillness", DTWeaponTestHarness::TestFlags) // T-RFG-10
bool FDTReforgeCycloneStillness::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	// The dead-pairing lead: what a charge gathered at rest does to a weapon that fires only in motion.
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.ArcBlade"));
	FDTWeaponFlags Flags;
	FDTFlagEntry& Entry = Flags.Add(DTCat::EFlag::Stillness, DTWeapons::KeeperSource(EDTPath::Tide), EDTFlagTier::Key);
	Entry.Params.Set(DTFlagParam::StillMoveUu, 20.0f);
	Entry.Params.Set(DTFlagParam::ChargeS, 0.5f);
	Entry.Params.Set(DTFlagParam::Mult, 3.0f);
	H.Weapons.AddFlagSource(0, TEXT("Test.Still"), Flags);
	const int32 Enemy = H.AddEnemy(FVector2f(100.0f, 0.0f));
	RunSteps(H, FMath::RoundToInt(1.0f / Dt));
	TestEqual(TEXT("at rest it holds fire"), H.HitLog.Num(), 0);
	const FDTWeaponSpec& Spec = H.Weapons.GetLastSpec(0, 0);
	const float Stride = 2.0f * DTReforge::Param(Row, TEXT("move_min_uu_s")) * Dt;
	H.View.HeroPositions[0].Y += Stride;
	H.StepOnce();
	H.View.HeroPositions[0].Y += Stride;
	H.StepOnce();
	float Largest = 0.0f;
	for (const FDTHit& Hit : H.HitLog)
	{
		Largest = Hit.EnemySerial == H.Enemies.GetSerials()[Enemy] ? FMath::Max(Largest, Hit.Damage) : Largest;
	}
	TestTrue(TEXT("the charge is not lost: the first spin carries it"), Largest >= Spec.Damage * 2.9f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeDeterminism, "Dusktracer.Reforging.Determinism", DTWeaponTestHarness::TestFlags) // T-RFG-18
bool FDTReforgeDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		TArray<float> Runs[2];
		for (int32 Run = 0; Run < 2; ++Run)
		{
			FDTHarness H(13);
			GiveEvolved(H, DTCat::Weapons[Weapon].Id);
			H.Run.GetPlayer(0).PathReforges.Add(DTReforge::FindPath(EDTPath::Storm));
			for (int32 Index = 0; Index < 12; ++Index)
			{
				H.AddEnemy(FVector2f(150.0f + 60.0f * Index, 120.0f * (Index % 4) - 200.0f));
			}
			RunSteps(H, 200);
			for (const FDTHit& Hit : H.HitLog)
			{
				Runs[Run].Add(Hit.Damage);
			}
			TestTrue(FString::Printf(TEXT("%s evolved does something"), DTCat::Weapons[Weapon].Id), H.HitLog.Num() > 0 || DTCat::Weapons[Weapon].Id == FString(TEXT("Weapon.ArcBlade")));
		}
		TestTrue(FString::Printf(TEXT("%s same hits"), DTCat::Weapons[Weapon].Id), Runs[0] == Runs[1]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotCardReforge, "Dusktracer.Bot.CardReforge", DTWeaponTestHarness::TestFlags) // T-BOT-15
bool FDTBotCardReforge::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	Commit(H, TEXT("Weapon.PulseRifle"), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL);
	const FDTOfferHand Hand = ReforgeHand(H);
	TSet<FName> Taken;
	for (int32 Seed = 0; Seed < 3; ++Seed)
	{
		const int32 Slot = FDTBotBrain::ReforgePick(Hand, Seed);
		if (Hand.Cards.IsValidIndex(Slot))
		{
			Taken.Add(Hand.Cards[Slot].ContentId);
		}
	}
	TestEqual(TEXT("three seeds, three cards"), Taken.Num(), 3);
	FDTOfferHand Filler = Hand;
	if (Filler.Cards.Num() == 3)
	{
		Filler.Cards[2].Kind = EDTCardKind::Blessing;
		for (int32 Seed = 0; Seed < 3; ++Seed)
		{
			const int32 Slot = FDTBotBrain::ReforgePick(Filler, Seed);
			TestTrue(TEXT("a Reforging card beats filler, and a missing card falls to the next"), Filler.Cards.IsValidIndex(Slot) && Filler.Cards[Slot].Kind == EDTCardKind::Reforging);
		}
		for (FDTOfferCard& Card : Filler.Cards)
		{
			Card.Kind = EDTCardKind::Blessing;
		}
		TestEqual(TEXT("no Reforging card: no opinion"), FDTBotBrain::ReforgePick(Filler, 0), static_cast<int32>(INDEX_NONE));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotCardPrimaryFirst, "Dusktracer.Bot.CardPrimaryFirst", DTWeaponTestHarness::TestFlags) // T-BOT-16
bool FDTBotCardPrimaryFirst::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	Commit(H, TEXT("Weapon.PulseRifle"), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL - 1);
	H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.ArcBlade")), 1);
	FDTOfferCard Upgrade;
	Upgrade.Kind = EDTCardKind::WeaponUpgrade;
	Upgrade.ContentId = FName(TEXT("Weapon.PulseRifle"));
	Upgrade.TargetWeapon = Upgrade.ContentId;
	FDTOfferCard Side = Upgrade;
	Side.ContentId = FName(TEXT("Weapon.ArcBlade"));
	Side.TargetWeapon = Side.ContentId;
	FDTOfferCard Key;
	Key.Kind = EDTCardKind::Blessing;
	Key.ContentId = FName(TEXT("Blessing.Storm.Tempest.Key"));
	FDTOfferHand Hand;
	Hand.HandKind = EDTHandKind::Level;
	Hand.Cards = { Key, Side, Upgrade };
	FDTBotConfig Config;
	Config.Policy = EDTBotPolicy::Steered;
	Config.Version = 2;
	Config.Path = EDTPath::Storm;
	Config.Archetype = FName(TEXT("Tempest"));
	FDTBotBrain Brain;
	Brain.Reset(Config);
	TestEqual(TEXT("the arm off: the Key"), Brain.ChooseCard(Hand, H.Run.GetPlayer(0)), 0);
	Config.bPrimaryFirst = true;
	Brain.Reset(Config);
	TestEqual(TEXT("the arm on: the primary's upgrade"), Brain.ChooseCard(Hand, H.Run.GetPlayer(0)), 2);
	H.Run.GetPlayer(0).Weapons[0].Level = DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL;
	TestEqual(TEXT("at the level it stops mattering"), Brain.ChooseCard(Hand, H.Run.GetPlayer(0)), 0);
	return true;
}

#endif
