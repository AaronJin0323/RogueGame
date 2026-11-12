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

	/** Hits of a source that claim a weapon slot: a Keeper hit claims none, so no proc feeds on it (T-RFG-22, R-KEE-13). */
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
	FDTOfferCard EvolveCard;
	EvolveCard.Kind = EDTCardKind::Reforging;
	EvolveCard.ContentId = DTReforge::RowId(DTReforge::FindEvolve(P.Weapons[0].Def));
	EvolveCard.TargetWeapon = P.Weapons[0].Id;
	const TArray<FDTDormantSocket> Named = DTOffers::DormantAfterPick(P, EvolveCard);
	TestEqual(TEXT("the card names the socket before the pick (R-MOD-08)"), Named.Num(), 1);
	TestFalse(TEXT("and reading it silences nothing"), P.Weapons[0].Sockets[0].bDormant);
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
	const float Rest = DTReforge::Param(Row, TEXT("orbit_r_uu"));
	const float Contact = DTReforge::Param(Row, TEXT("contact_r_uu"));
	const int32 Blades = FMath::RoundToInt(DTReforge::Param(Row, TEXT("blades")));
	const int32 TickSteps = FMath::RoundToInt(DTReforge::Param(Row, TEXT("contact_tick_s")) / Dt);
	// A probe for the enemy's own radius, which the contact test counts; it sits far off and is never reached.
	const int32 Probe = H.AddEnemy(FVector2f(5000.0f, 0.0f));
	const float Own = H.Enemies.GetRadii()[Probe];
	// A full ring on the rest orbit, every 10 degrees, so each blade touches some wherever the turn has carried it.
	TArray<int32> Ring;
	for (int32 Index = 0; Index < 36; ++Index)
	{
		Ring.Add(H.AddEnemy(FVector2f(Rest, 0.0f).GetRotated(10.0f * static_cast<float>(Index))));
	}
	// Two rings off the orbit, every 3 degrees: one a unit inside a blade's reach (its contact plus the enemy's radius), one two units outside.
	TArray<int32> Inside;
	TArray<int32> Outside;
	for (int32 Index = 0; Index < 120; ++Index)
	{
		Inside.Add(H.AddEnemy(FVector2f(Rest + Contact + Own - 1.0f, 0.0f).GetRotated(3.0f * static_cast<float>(Index))));
		Outside.Add(H.AddEnemy(FVector2f(Rest + Contact + Own + 2.0f, 0.0f).GetRotated(3.0f * static_cast<float>(Index))));
	}
	const auto RingAngles = [&](int32 From)
	{
		TArray<float> Angles;
		for (int32 Index = From; Index < H.HitLog.Num(); ++Index)
		{
			const FVector2f At = H.HitLog[Index].Position;
			if (FMath::Abs(At.Size() - Rest) < 1.0f)
			{
				Angles.Add(FMath::RadiansToDegrees(FMath::Atan2(At.Y, At.X)));
			}
		}
		return Angles;
	};
	// The first tick: step until the blades strike (the weapon composes on its first step).
	int32 Before = 0;
	for (int32 Step = 0; Step <= TickSteps + 2 && H.HitLog.Num() == 0; ++Step)
	{
		Before = H.HitLog.Num();
		H.StepOnce();
	}
	TArray<float> First = RingAngles(Before);
	TestTrue(TEXT("the first tick strikes the ring"), First.Num() > 0);
	// Every blade strikes on the tick, not one in turn: the ring's hits of that one step fall in every quarter the blades stand in.
	TSet<int32> Quarters;
	for (const float Angle : First)
	{
		Quarters.Add(FMath::FloorToInt(FMath::Fmod(Angle - First[0] + 405.0f, 360.0f) / 90.0f) % 4);
	}
	TestEqual(TEXT("every blade strikes on the same tick"), Quarters.Num(), Blades);
	// And none strikes again before the next tick.
	const int32 AfterFirst = H.HitLog.Num();
	for (int32 Step = 1; Step < TickSteps; ++Step)
	{
		H.StepOnce();
	}
	TestEqual(TEXT("no strike between ticks"), H.HitLog.Num(), AfterFirst);
	H.StepOnce();
	H.StepOnce();
	TestTrue(TEXT("the next tick strikes"), H.HitLog.Num() > AfterFirst);
	// The reach counts the enemy's own radius: over a few turns the inner off-orbit ring is struck, the outer never.
	RunSteps(H, 600);
	int32 InsideHits = 0;
	for (const int32 Enemy : Inside)
	{
		InsideHits += HitsOn(H, H.Enemies.GetSerials()[Enemy]);
	}
	int32 OutsideHits = 0;
	for (const int32 Enemy : Outside)
	{
		OutsideHits += HitsOn(H, H.Enemies.GetSerials()[Enemy]);
	}
	TestTrue(TEXT("an enemy within contact_r_uu plus its own radius is struck"), InsideHits > 0);
	TestEqual(TEXT("one just outside is not"), OutsideHits, 0);
	TestEqual(TEXT("the probe out of reach is not"), HitsOn(H, H.Enemies.GetSerials()[Probe]), 0);
	TestEqual(TEXT("no bolts"), H.Bullets.Num(), 0);
	TestEqual(TEXT("the blade count"), H.Weapons.GetLastSpec(0, 0).Count, Blades);

	// The blades turn at the row's speed, and the plain drones still at their constant.
	{
		const float Turn = DTReforge::Param(Row, TEXT("orbit_deg_s"));
		const float From = H.Weapons.GetDroneAngle(0, 0);
		H.StepOnce();
		TestNearlyEqual(TEXT("the blades turn at orbit_deg_s"), FMath::Fmod(H.Weapons.GetDroneAngle(0, 0) - From + 360.0f, 360.0f), Turn * Dt, 0.01f);
		FDTHarness Plain;
		Plain.Weapons.Give(Plain.Run, 0, FName(TEXT("Weapon.OrbitalDrones")), 8);
		Plain.StepOnce();
		const float PlainFrom = Plain.Weapons.GetDroneAngle(0, 0);
		Plain.StepOnce();
		TestNearlyEqual(TEXT("the plain drones turn at their constant"), FMath::Fmod(Plain.Weapons.GetDroneAngle(0, 0) - PlainFrom + 360.0f, 360.0f),
			static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_DEG_S) * Dt, 0.01f);
		TestEqual(TEXT("and orbit at their constant"), Plain.Weapons.GetBladeRadius(0, 0), static_cast<float>(DTConst::WEAPON_DRONE_ORBIT_RADIUS_UU));
	}
	// The orbit rests at orbit_r_uu, widens with speed, and stops widening at speed_cap times the reference speed.
	{
		FDTHarness Moving;
		GiveEvolved(Moving, TEXT("Weapon.OrbitalDrones"));
		Moving.StepOnce();
		TestNearlyEqual(TEXT("the orbit rests at orbit_r_uu"), Moving.Weapons.GetBladeRadius(0, 0), Rest, 0.01f);
		const float Reference = DTReforge::Param(Row, TEXT("speed_ref_uu_s"));
		const float Ratio = DTReforge::Param(Row, TEXT("speed_ratio"));
		const float Cap = DTReforge::Param(Row, TEXT("speed_cap"));
		const auto MoveAt = [&](float SpeedUuS)
		{
			for (int32 Step = 0; Step < 2; ++Step)
			{
				Moving.View.HeroPositions[0] += FVector2f(SpeedUuS * Dt, 0.0f);
				Moving.StepOnce();
			}
			return Moving.Weapons.GetBladeRadius(0, 0);
		};
		TestNearlyEqual(TEXT("the orbit widens with speed"), MoveAt(Reference), Rest * (1.0f + Ratio), 0.5f);
		TestNearlyEqual(TEXT("up to speed_cap times the reference"), MoveAt(Cap * Reference), Rest * (1.0f + Ratio * Cap), 0.5f);
		TestNearlyEqual(TEXT("and no further"), MoveAt(3.0f * Cap * Reference), Rest * (1.0f + Ratio * Cap), 0.5f);
	}
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

	// D-072: a missile deals the row's damage, launched at the row's cooldown times cooldown_mult.
	const DTCat::FWeaponLevelRow& L1 = *DTWeapons::GetLevelRow(Spec.Weapon, 1);
	TestNearlyEqual(TEXT("a missile deals dmg_ratio of the row's damage"), Spec.Damage, L1.Damage * DTReforge::Param(Row, TEXT("dmg_ratio")), 0.001f);
	TestNearlyEqual(TEXT("dmg_ratio is the row's own damage"), Spec.Damage, L1.Damage, 0.001f);
	TestNearlyEqual(TEXT("launched at the row's cooldown times cooldown_mult"), Spec.Cooldown, L1.Cooldown_s * DTReforge::Param(Row, TEXT("cooldown_mult")), 0.001f);
	bool bAllFull = true;
	for (const FDTHit& Hit : H.HitLog)
	{
		bAllFull &= FMath::IsNearlyEqual(Hit.Damage, Spec.Damage, 0.001f);
	}
	TestTrue(TEXT("every strike is the missile's whole damage"), bAllFull);

	// A lone target is struck once per missile: with nobody else to come after it, a round never returns to it.
	{
		FDTHarness Lone;
		GiveEvolved(Lone, TEXT("Weapon.SeekerSwarm"), 1);
		const int32 Boss = Lone.AddEnemy(FVector2f(300.0f, 0.0f));
		Lone.StepOnce();
		Lone.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
		const int32 Missiles = Lone.Bullets.Num();
		RunSteps(Lone, FMath::RoundToInt(DTReforge::Param(Row, TEXT("life_s")) / Dt) + 2);
		const int32 Struck = HitsOn(Lone, Lone.Enemies.GetSerials()[Boss]);
		TestTrue(TEXT("a lone target is struck"), Struck > 0);
		TestTrue(TEXT("once per missile at most"), Struck <= Missiles);
	}

	// The re-strike window: one homing round among three enemies. Under the row's rehit_after (more than two others) it strikes each once;
	// with a window of two it returns to the first once two others came after it.
	const auto MostOnOne = [&](int32 Window)
	{
		FDTHarness R;
		const int32 Targets[3] = { R.AddEnemy(FVector2f(150.0f, 0.0f)), R.AddEnemy(FVector2f(-75.0f, 130.0f)), R.AddEnemy(FVector2f(-75.0f, -130.0f)) };
		FDTBulletSpawn Round;
		Round.Position = FVector2f::ZeroVector;
		Round.Velocity = FVector2f(600.0f, 0.0f);
		Round.Radius = 9.0f;
		Round.Damage = 1.0f;
		Round.Life = 6.0f;
		Round.OwnerId = 0;
		Round.Pierce = 200;
		Round.Motion.TurnDegS = 720.0f;
		Round.Motion.SeekUu = 900.0f;
		Round.Motion.Speed = 600.0f;
		Round.Motion.Pierce = 200;
		Round.Motion.bRetarget = true;
		Round.Motion.RehitAfter = Window;
		R.Bullets.Spawn(Round);
		R.StepMany(FMath::RoundToInt(6.0f / Dt));
		int32 Most = 0;
		for (const int32 Enemy : Targets)
		{
			Most = FMath::Max(Most, HitsOn(R, R.Enemies.GetSerials()[Enemy]));
		}
		return Most;
	};
	const int32 RowWindow = FMath::RoundToInt(DTReforge::Param(Row, TEXT("rehit_after")));
	TestTrue(TEXT("the row's rehit_after exceeds the two others here"), RowWindow > 2);
	TestEqual(TEXT("with fewer than rehit_after others a round never strikes an enemy again"), MostOnOne(RowWindow), 1);
	TestTrue(TEXT("once rehit_after others came after it, it may"), MostOnOne(2) > 1);
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
	// D-074: the volley fires its rounds as the plain weapon does.
	TestEqual(TEXT("the volley fires its rounds"), H.Bullets.Num(), FMath::Max(1, H.Weapons.GetLastSpec(0, 0).Count));
	const FVector2f Before = H.Enemies.GetPositions()[Rim];
	bool bPushed = false;
	for (int32 Step = 0; Step < FMath::RoundToInt((DTReforge::Param(Row, TEXT("grow_s")) + 0.5f) / Dt); ++Step)
	{
		H.StepOnce();
		bPushed = bPushed || H.View.Displacements.ContainsByPredicate([&H, Rim](const FDTDisplacement& D) { return D.EnemySerial == H.Enemies.GetSerials()[Rim] && D.Delta.X > 0.0f; });
	}
	TestEqual(TEXT("still one ring"), H.Weapons.GetLiveFields(), 1);
	TestTrue(TEXT("the disc strikes the rim once the ring reaches it"), HitsOn(H, H.Enemies.GetSerials()[Rim]) > 0);
	// One round's damage, per enemy: the Count and Cooldown of the weapon do not enter it (D-020).
	// D-074: a hit is either a ring's strike, dps x Damage x tick_s, or one of the volley's rounds, Damage.
	const float Round = H.Weapons.GetLastSpec(0, 0).Damage;
	const float Struck = DTReforge::Param(Row, TEXT("dps")) * Round * DTReforge::Param(Row, TEXT("tick_s"));
	for (const FDTHit& Hit : H.HitLog)
	{
		if (Hit.EnemySerial == H.Enemies.GetSerials()[Rim] || Hit.EnemySerial == H.Enemies.GetSerials()[Core])
		{
			TestTrue(FString::Printf(TEXT("a hit is a tick's strike (%.2f) or a round (%.2f): %.2f"), Struck, Round, Hit.Damage),
				FMath::IsNearlyEqual(Hit.Damage, Struck, 0.01f) || FMath::IsNearlyEqual(Hit.Damage, Round, 0.01f));
		}
	}
	TestTrue(TEXT("and pushes it out through a displacement"), bPushed);
	TestEqual(TEXT("positions are not written by the weapon"), H.Enemies.GetPositions()[Rim], Before);
	// D-070: the whole disc strikes, so what stands near the centre is struck from the first ticks, more often than the rim.
	return true;
}

namespace DTReforgeTest
{
	/** As the Enemies phase applies them (FDTEnemyPool::ApplyDisplacements): stale serials and the boss are left; then cleared. */
	static void ApplyMoves(FDTHarness& H)
	{
		for (const FDTDisplacement& D : H.View.Displacements)
		{
			if (H.Enemies.GetSerials().IsValidIndex(D.EnemyIndex) && H.Enemies.GetSerials()[D.EnemyIndex] == D.EnemySerial &&
				H.Enemies.GetRoles()[D.EnemyIndex] != EDTEnemyRole::Boss)
			{
				H.Enemies.SetPosition(D.EnemyIndex, H.Enemies.GetPositions()[D.EnemyIndex] + D.Delta);
			}
		}
		H.View.Displacements.Reset();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgePulseWallDisc, "Dusktracer.Reforging.PulseWallDisc", DTWeaponTestHarness::TestFlags) // T-RFG-12, D-070
bool FDTReforgePulseWallDisc::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	{
		// Every enemy inside the ring is struck each tick for one round's tick and pushed out once a tick by push_uu_s x tick_s;
		// the boss is struck and stays; nothing outside the radius is touched.
		FDTHarness H;
		const int32 Row = GiveEvolved(H, TEXT("Weapon.RingEmitter"));
		const float Radius = DTReforge::Param(Row, TEXT("radius_uu"));
		const float TickS = DTReforge::Param(Row, TEXT("tick_s"));
		const float Kick = DTReforge::Param(Row, TEXT("push_uu_s")) * TickS;
		const int32 NearIndex = H.AddEnemy(FVector2f(Radius / 3.0f, 0.0f));
		const int32 BossIndex = H.Enemies.Spawn(EDTEnemyRole::Boss, FVector2f(0.0f, -Radius / 2.0f));
		const int32 FarIndex = H.AddEnemy(FVector2f(-(Radius + 200.0f), 0.0f));
		const uint32 Near = H.Enemies.GetSerials()[NearIndex];
		const uint32 Boss = H.Enemies.GetSerials()[BossIndex];
		const uint32 Far = H.Enemies.GetSerials()[FarIndex];
		const FVector2f BossAt = H.Enemies.GetPositions()[BossIndex];
		H.StepOnce();
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f; // one ring only
		ApplyMoves(H);
		const int32 Steps = FMath::RoundToInt((DTReforge::Param(Row, TEXT("grow_s")) + 1.0f) / Dt);
		int32 NearMoves = 0;
		bool bKickSize = true;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			H.StepOnce();
			for (const FDTDisplacement& D : H.View.Displacements)
			{
				if (D.EnemySerial == Near)
				{
					++NearMoves;
					bKickSize = bKickSize && FMath::IsNearlyEqual(D.Delta.Size(), Kick, 0.01f) && D.Delta.X > 0.0f;
				}
			}
			ApplyMoves(H);
		}
		// D-074: the volley's rounds hit too, for Damage; a ring's strike is dps x Damage x tick_s, and only it pushes.
		const float Round = H.Weapons.GetLastSpec(0, 0).Damage;
		const float Tick = DTReforge::Param(Row, TEXT("dps")) * Round * TickS;
		int32 NearHits = 0, NearRounds = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			if (Hit.EnemySerial == Near || Hit.EnemySerial == Boss)
			{
				const bool bStrike = FMath::IsNearlyEqual(Hit.Damage, Tick, 0.01f);
				const bool bRound = FMath::IsNearlyEqual(Hit.Damage, Round, 0.01f);
				TestTrue(FString::Printf(TEXT("a hit is a strike (%.2f) or a round (%.2f): %.2f"), Tick, Round, Hit.Damage), bStrike || bRound);
				NearHits += (Hit.EnemySerial == Near && bStrike) ? 1 : 0;
				NearRounds += (Hit.EnemySerial == Near && bRound) ? 1 : 0;
			}
		}
		AddInfo(FString::Printf(TEXT("near: %d strikes, %d round hits, %d pushes, ends at %.1f uu"), NearHits, NearRounds, NearMoves, H.Enemies.GetPositions()[NearIndex].Size()));
		TestTrue(TEXT("what stands inside is struck each tick"), NearHits >= 3);
		TestEqual(TEXT("and pushed once a strike, no more"), NearMoves, NearHits);
		TestTrue(TEXT("each push is push_uu_s x tick_s, outward"), bKickSize);
		TestTrue(TEXT("the boss is struck"), HitsOn(H, Boss) > 0);
		TestEqual(TEXT("and stays where it stood"), H.Enemies.GetPositions()[BossIndex], BossAt);
		TestEqual(TEXT("nothing outside the radius is touched"), HitsOn(H, Far), 0);
		TestEqual(TEXT("one ring live"), H.Weapons.GetLiveFields(), 1);
	}
	{
		// D-074: the port's capabilities stay, so every modifier that fits the Ring Emitter stays live on the evolved weapon.
		FDTHarness H;
		int32 Slot = INDEX_NONE;
		H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.RingEmitter")), 8, &Slot);
		TestEqual(TEXT("Pierce sockets on the plain weapon"), H.Weapons.Socket(H.Run, 0, Slot, TEXT("Mod.Pierce")), EDTWeaponResult::Ok);
		TestEqual(TEXT("Conduit sockets on the plain weapon"), H.Weapons.Socket(H.Run, 0, Slot, TEXT("Mod.Conduit")), EDTWeaponResult::Ok);
		FDTWeaponInstance& Owned = H.Run.GetPlayer(0).Weapons[Slot];
		Owned.Evolve = DTReforge::FindEvolve(Owned.Def);
		H.StepOnce();
		const FDTWeaponInstance& Now = H.Run.GetPlayer(0).Weapons[Slot];
		for (const FDTWeaponInstance::FSocket& Socket : Now.Sockets)
		{
			TestFalse(FString::Printf(TEXT("%s stays live on Pulse Wall"), *Socket.ModId.ToString()), Socket.bDormant);
		}
	}
	return true;
}
namespace DTReforgeTest
{
	/** An evolved Ring Emitter with these modifiers socketed on the plain weapon first (the sockets the evolution keeps). */
	static int32 GiveWallWith(FDTHarness& H, std::initializer_list<const TCHAR*> Mods)
	{
		int32 Slot = INDEX_NONE;
		H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.RingEmitter")), 8, &Slot);
		for (const TCHAR* Mod : Mods)
		{
			H.Weapons.Socket(H.Run, 0, Slot, FName(Mod));
		}
		FDTWeaponInstance& Owned = H.Run.GetPlayer(0).Weapons[Slot];
		Owned.Evolve = DTReforge::FindEvolve(Owned.Def);
		return Owned.Evolve;
	}

	/** Steps, recording any displacement of Serial toward the ring's centre (the hero at the origin): a pull, not the ring's push. */
	static bool StepAndSeePull(FDTHarness& H, int32 Steps, uint32 Serial, int32* OutMostFields = nullptr)
	{
		bool bPulled = false;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			H.StepOnce();
			if (OutMostFields)
			{
				*OutMostFields = FMath::Max(*OutMostFields, H.Weapons.GetLiveFields()); // the ring and any modifier's zones
			}
			for (const FDTDisplacement& D : H.View.Displacements)
			{
				const int32 Index = H.Enemies.GetSerials().IndexOfByKey(D.EnemySerial);
				bPulled = bPulled || (D.EnemySerial == Serial && Index != INDEX_NONE && FVector2f::DotProduct(D.Delta, H.Enemies.GetPositions()[Index]) < 0.0f);
			}
			H.View.Displacements.Reset();
		}
		return bPulled;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgePulseWallStrike, "Dusktracer.Reforging.PulseWallStrike", DTWeaponTestHarness::TestFlags) // T-RFG-12, D-074
bool FDTReforgePulseWallStrike::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const int32 Steps = FMath::RoundToInt(1.5f / Dt);
	{
		// A ring's strike places nothing and pulls nothing: an enemy between two of the volley's round lines is struck by the ring only.
		FDTHarness H;
		GiveWallWith(H, { TEXT("Mod.SplitCore"), TEXT("Mod.LingeringField"), TEXT("Mod.Backwash") });
		H.StepOnce();
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f; // one volley
		H.Bullets.Reset(); // its rounds are cleared, so only the ring meets the enemy
		const int32 Index = H.AddEnemy(FVector2f(150.0f, 0.0f));
		const uint32 Serial = H.Enemies.GetSerials()[Index];
		const int32 Before = H.HitLog.Num();
		const bool bPulled = StepAndSeePull(H, Steps, Serial);
		int32 Ring = 0;
		for (int32 Hit = Before; Hit < H.HitLog.Num(); ++Hit)
		{
			Ring += H.HitLog[Hit].EnemySerial == Serial ? 1 : 0;
		}
		TestTrue(TEXT("the ring strikes it"), Ring > 0);
		TestFalse(TEXT("and Backwash does not pull it on a ring's strike"), bPulled);
		TestEqual(TEXT("Lingering Field places nothing on a ring's strike: only the ring lives"), H.Weapons.GetLiveFields(), 1);
		TestEqual(TEXT("Split Core fans nothing from a ring's strike: no round hits it"), HitsOn(H, Serial, DTWeapons::ModifierSource(DTWeapons::FindModifier(TEXT("Mod.SplitCore")))), 0);
	}
	{
		// The same volley's rounds do what those modifiers do on the plain weapon: an enemy on a round line is struck by a round,
		// which fans Split Core's children, leaves Lingering Field's zone and pulls it back toward the hero.
		FDTHarness H;
		GiveWallWith(H, { TEXT("Mod.SplitCore"), TEXT("Mod.LingeringField"), TEXT("Mod.Backwash") });
		H.StepOnce();
		H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f; // one volley
		TestTrue(TEXT("the volley fires rounds"), H.Bullets.Num() > 0);
		if (H.Bullets.Num() == 0)
		{
			return true;
		}
		// The enemy stands on one round's line, ahead of it.
		const FVector2f Line = (H.Bullets.GetPositions()[0] - H.Bullets.GetPrevPositions()[0]).GetSafeNormal();
		const int32 Index = H.AddEnemy(H.Bullets.GetPositions()[0] + Line * 200.0f);
		const uint32 Serial = H.Enemies.GetSerials()[Index];
		int32 MostFields = 0;
		const bool bPulled = StepAndSeePull(H, Steps, Serial, &MostFields);
		TestTrue(TEXT("it is struck"), HitsOn(H, Serial) > 0);
		// The ring's strikes neither pull nor place (the scene above), so the pull and the zone are the round's.
		TestTrue(TEXT("Backwash pulls it on a round's hit"), bPulled);
		TestTrue(TEXT("Lingering Field leaves a zone where the round struck: the ring and a zone live together"), MostFields >= 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgePulseWallThrottle, "Dusktracer.Reforging.PulseWallThrottle", DTWeaponTestHarness::TestFlags) // T-RFG-12, D-074 item 5
bool FDTReforgePulseWallThrottle::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	// A crowd stands inside the disc for several ring lives; with a Keeper's Chain composed every ring strike starts jumps, and a
	// jump spends one of the weapon's emissions. The weapon's rounds must not be throttled for it.
	const auto Scene = [](bool bChain, int64& OutSpawned, int64& OutThrottled, int64& OutRounds)
	{
		FDTHarness H;
		GiveEvolved(H, TEXT("Weapon.RingEmitter"));
		if (bChain)
		{
			FDTWeaponFlags Flags;
			FDTFlagEntry& Chain = Flags.Add(DTCat::EFlag::Chain, DTWeapons::KeeperSource(EDTPath::Storm), EDTFlagTier::Key);
			Chain.Params.Set(DTFlagParam::Jumps, 2.0f);
			Chain.Params.Set(DTFlagParam::RangeUu, 350.0f);
			Chain.Params.Set(DTFlagParam::Dmg, 0.6f);
			H.Weapons.AddFlagSource(0, TEXT("Test.ArcGrid"), Flags);
		}
		for (int32 Ring = 0; Ring < 3; ++Ring)
		{
			for (int32 Index = 0; Index < 16; ++Index)
			{
				const float Angle = 2.0f * PI * (static_cast<float>(Index) + 0.5f * Ring) / 16.0f;
				H.AddEnemy(FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (150.0f + 150.0f * Ring));
			}
		}
		H.StepMany(FMath::RoundToInt(3.0f * (0.6f + 3.0f) / Dt)); // three ring lives
		OutSpawned = H.Weapons.GetCounters(0, 0).Spawned;
		OutThrottled = H.Weapons.GetCounters(0, 0).Throttled;
		OutRounds = H.Bullets.GetCounters().Spawned; // the weapon's rounds: a chain's jump is no bullet
	};
	int64 SpawnedPlain = 0, ThrottledPlain = 0, RoundsPlain = 0, SpawnedChain = 0, ThrottledChain = 0, RoundsChain = 0;
	Scene(false, SpawnedPlain, ThrottledPlain, RoundsPlain);
	Scene(true, SpawnedChain, ThrottledChain, RoundsChain);
	AddInfo(FString::Printf(TEXT("without the chain: %lld emissions, %lld throttled, %lld rounds; with it: %lld emissions, %lld throttled, %lld rounds"),
		SpawnedPlain, ThrottledPlain, RoundsPlain, SpawnedChain, ThrottledChain, RoundsChain));
	TestEqual(TEXT("the weapon fires as many rounds with the disc's chains as without them"), RoundsChain, RoundsPlain);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgePulseWallGap, "Dusktracer.Reforging.PulseWallGap", DTWeaponTestHarness::TestFlags) // T-RFG-12
bool FDTReforgePulseWallGap::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const int32 Row = GiveEvolved(H, TEXT("Weapon.RingEmitter"));
	H.AddEnemy(FVector2f(DTReforge::Param(Row, TEXT("radius_uu")) - 20.0f, 0.0f));
	const float Life = DTReforge::Param(Row, TEXT("grow_s")) + DTReforge::Param(Row, TEXT("hold_s"));
	const int32 Steps = FMath::RoundToInt(5.0f * Life / Dt);
	int32 Dark = 0, Stretch = 0, Longest = 0;
	for (int32 Step = 0; Step < Steps; ++Step)
	{
		H.StepOnce();
		const bool bNone = H.Weapons.GetLiveFields() == 0;
		Dark += bNone ? 1 : 0;
		Stretch = bNone ? Stretch + 1 : 0;
		Longest = FMath::Max(Longest, Stretch);
	}
	// D-074: the weapon's cooldown is its own; the first volley after a ring closes opens the next, so no stretch without a ring
	// outlasts one cooldown.
	const int32 CooldownSteps = FMath::CeilToInt(H.Weapons.GetLastSpec(0, 0).Cooldown / Dt);
	TestTrue(FString::Printf(TEXT("a ring follows a ring within one cooldown: longest stretch with none %d steps, cooldown %d steps (%d of %d steps with none)"),
		Longest, CooldownSteps, Dark, Steps), Longest <= CooldownSteps + 1);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReforgeCollectionCooldown, "Dusktracer.Reforging.CollectionCooldown", DTWeaponTestHarness::TestFlags) // T-RFG-19
bool FDTReforgeCollectionCooldown::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const int32 Row = DTReforge::FindPath(EDTPath::Shade);
	const uint16 Shade = DTWeapons::KeeperSource(EDTPath::Shade);
	const int32 Max = FMath::RoundToInt(DTReforge::Param(Row, TEXT("shades_max")));
	// A full orbit over one enemy for three seconds, with or without a Marked kill on every step.
	const auto Scene = [Row, Shade, Max](bool bKills)
	{
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, FName(TEXT("Weapon.PulseRifle")), 1);
		H.Run.GetPlayer(0).PathReforges.Add(Row);
		H.StepOnce();
		uint32 Serial = 900000;
		const auto MarkedKill = [&H, &Serial]()
		{
			FDTKill Dead;
			Dead.EnemySerial = ++Serial;
			Dead.OwnerId = 0;
			Dead.Position = FVector2f(5000.0f, 0.0f);
			H.Weapons.ApplyMark(0, Dead.EnemySerial, H.Step);
			H.PendingKills.Add(Dead);
		};
		for (int32 Index = 0; Index < Max; ++Index)
		{
			MarkedKill();
			H.StepOnce();
		}
		H.AddEnemy(FVector2f(350.0f, 0.0f));
		H.HitLog.Reset();
		const int32 Steps = FMath::RoundToInt(3.0f / Dt);
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			if (bKills)
			{
				MarkedKill();
			}
			H.StepOnce();
		}
		return H.CountHitsFrom(Shade);
	};
	const int32 Steady = Scene(false);
	TestTrue(TEXT("a full orbit fires"), Steady > 0);
	TestEqual(TEXT("replacing the oldest buys no volley and costs none: the seats keep the weapon's cooldown"), Scene(true), Steady);
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
	// D-070 ruling 8: one outer arc, no inner band edge (the whole disc strikes).
	TestEqual(TEXT("and that one arc only"), Seen.FilterByPredicate([](const FDTWeaponVisual& V) { return V.Kind == FDTWeaponVisual::EKind::Arc && V.ArcDeg >= 360.0f; }).Num(), 1);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTBotCardCarry, "Dusktracer.Bot.CardCarry", DTWeaponTestHarness::TestFlags) // T-BOT-18
bool FDTBotCardCarry::RunTest(const FString& Parameters)
{
	using namespace DTReforgeTest;
	using namespace DTWeaponTestHarness;
	const FName Rifle(TEXT("Weapon.PulseRifle"));
	const FName Blade(TEXT("Weapon.ArcBlade"));
	const FName Drones(TEXT("Weapon.OrbitalDrones"));
	FDTHarness H;
	Commit(H, TEXT("Weapon.PulseRifle"), 1);
	FDTPlayerRunState& Player = H.Run.GetPlayer(0);
	const auto Make = [](EDTCardKind Kind, FName Id, FName Target)
	{
		FDTOfferCard Card;
		Card.Kind = Kind;
		Card.ContentId = Id;
		Card.TargetWeapon = Target;
		return Card;
	};
	const FDTOfferCard Key = Make(EDTCardKind::Blessing, FName(TEXT("Blessing.Storm.Tempest.Key")), NAME_None);
	const FDTOfferCard Reserve = Make(EDTCardKind::Reserve, FName(TEXT("Reserve.Xp")), NAME_None);
	const FDTOfferCard RifleUp = Make(EDTCardKind::WeaponUpgrade, Rifle, Rifle);
	const FDTOfferCard BladeUp = Make(EDTCardKind::WeaponUpgrade, Blade, Blade);
	const FDTOfferCard DronesNew = Make(EDTCardKind::Weapon, Drones, NAME_None);
	H.Weapons.Give(H.Run, 0, Blade, 1);
	FDTOfferHand Hand;
	Hand.HandKind = EDTHandKind::Level;
	Hand.Cards = { Key, RifleUp, BladeUp };

	FDTBotConfig Config;
	Config.Policy = EDTBotPolicy::Steered;
	Config.Version = 2;
	Config.Path = EDTPath::Storm;
	Config.Archetype = FName(TEXT("Tempest"));
	FDTBotBrain Brain;
	Brain.Reset(Config);
	const float PlainKey = Brain.ScoreCard(Key, Hand, Player);
	const float PlainRifle = Brain.ScoreCard(RifleUp, Hand, Player);
	const float PlainBlade = Brain.ScoreCard(BladeUp, Hand, Player);
	TestEqual(TEXT("the arm off: the Key"), Brain.ChooseCard(Hand, Player), 0);

	Config.CarryWeapon = Blade;
	Brain.Reset(Config);
	TestEqual(TEXT("the arm leaves other kinds' scores alone"), Brain.ScoreCard(Key, Hand, Player), PlainKey);
	TestEqual(TEXT("level tie with the Rifle: the carried upgrade"), Brain.ChooseCard(Hand, Player), 2);
	FDTOfferHand Other = Hand;
	Other.Cards = { RifleUp, Reserve };
	TestEqual(TEXT("the Rifle's upgrade stays under Reserve"), Brain.ChooseCard(Other, Player), 1);
	Other.Cards = { DronesNew, Reserve };
	TestEqual(TEXT("a new weapon reads as usual"), Brain.ChooseCard(Other, Player), 0);

	Player.Level = DTConst::OFFER_REFORGING_LEVEL - 3;
	TestEqual(TEXT("no grant before its level"), FDTBotBrain::CarryGiftLevel(Player, Blade), 0);
	Player.Level = DTConst::OFFER_REFORGING_LEVEL - 2;
	TestEqual(TEXT("the grant reaches the evolution level"), FDTBotBrain::CarryGiftLevel(Player, Blade), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL);
	Player.Weapons[0].Level = DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL;
	TestEqual(TEXT("the grant clears every other weapon"), FDTBotBrain::CarryGiftLevel(Player, Blade), DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL + 1);
	Player.Weapons[1].Level = DTConst::OFFER_REFORGE_EVOLVE_MIN_LEVEL;
	TestEqual(TEXT("at the level but behind the Rifle on the tie: still the upgrade"), Brain.ChooseCard(Hand, Player), 2);
	Player.Weapons[0].Level = 1;
	TestEqual(TEXT("primary at the level: the Key again"), Brain.ChooseCard(Hand, Player), 0);
	TestEqual(TEXT("nothing left to grant"), FDTBotBrain::CarryGiftLevel(Player, Blade), 0);
	Player.Weapons[1].Level = 1;

	FDTOfferHand Forge;
	Forge.HandKind = EDTHandKind::Reforging;
	Forge.Cards = { Make(EDTCardKind::Reforging, FName(TEXT("Transform.Storm")), NAME_None), Make(EDTCardKind::Reforging, FName(TEXT("Transform.Tide")), NAME_None),
		Make(EDTCardKind::Reforging, FName(TEXT("Evolve.ArcBlade")), Blade) };
	TestEqual(TEXT("the evolution, whatever the seed's card"), Brain.ChooseCard(Forge, Player), 2);
	Config.bCarryHold = true;
	Brain.Reset(Config);
	TestEqual(TEXT("the control takes the Path card"), Brain.ChooseCard(Forge, Player), 0);
	TestTrue(TEXT("config names the control"), Config.Describe().EndsWith(TEXT("carry=Weapon.ArcBlade carry_hold=1")));

	Player.PathReforges.Add(0);
	TestEqual(TEXT("a Reforging pick ends the arm: the Rifle"), Brain.ScoreCard(RifleUp, Hand, Player), PlainRifle);
	TestEqual(TEXT("a Reforging pick ends the arm: the carried weapon"), Brain.ScoreCard(BladeUp, Hand, Player), PlainBlade);
	TestEqual(TEXT("a Reforging pick ends the grant"), FDTBotBrain::CarryGiftLevel(Player, Blade), 0);
	return true;
}

#endif
