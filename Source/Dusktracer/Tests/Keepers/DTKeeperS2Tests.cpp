// S2 Keepers: Shade, Radiance, Quicksilver (keepers.md §9, T-KEE-23..36). One test per card and each cast part, against the same
// scene without it. A Shade or Radiance card is often tested under a Storm Patron so the Patron's own enchantment cannot fake it.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Keepers/DTKeeperHarness.h"
#include "Combat/DTWeaponCatalog.h"
#include "Core/DTCatalogGen.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"

namespace DTKeeperS2Test
{
	using namespace DTKeeperTest;
	using DTCat::EFlag;

	static const FName GravityMine(TEXT("Weapon.GravityMine"));

	constexpr int32 Second = DTConst::SIM_FIXED_HZ;
	constexpr float ScaledHp = 1000.0f;

	static uint16 Src(EDTPath Path) { return DTKeepers::KeeperSource(Path); }

	/** A Patron and the named cards of Archetype; the Patron can differ from the cards' Keeper. */
	static void Own(FDTKeeperHarness& H, EDTPath Patron, const TCHAR* Archetype, std::initializer_list<const TCHAR*> Columns)
	{
		H.Patron(Patron);
		for (const TCHAR* Column : Columns)
		{
			H.Bless(Blessing(Archetype, Column));
		}
	}

	/** A swarm enemy of ScaledHp times the swarm's health, so a share of its HP outlasts a rifle. */
	static int32 AddScaled(FDTKeeperHarness& H, const FVector2f& At, EDTEnemyRole Role = EDTEnemyRole::Swarm, bool bElite = false)
	{
		FDTEnemySpawn Spawn;
		Spawn.Role = Role;
		Spawn.Position = At;
		Spawn.HpScale = ScaledHp;
		Spawn.bElite = bElite;
		FRandomStream Rng(1);
		return H.Enemies.Spawn(Spawn, &Rng);
	}

	static uint32 SerialOf(const FDTKeeperHarness& H, int32 Enemy) { return H.Enemies.GetSerials()[Enemy]; }

	static int32 HitsOn(const FDTKeeperHarness& H, uint32 Serial, uint16 Source)
	{
		int32 Count = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			Count += Hit.EnemySerial == Serial && Hit.SourceId == Source ? 1 : 0;
		}
		return Count;
	}

	static int32 StatusesOn(const FDTKeeperHarness& H, uint32 Serial, EDTStatus Status)
	{
		int32 Count = 0;
		for (const FDTStatusApply& Apply : H.StatusLog)
		{
			Count += Apply.Handle.Serial == Serial && Apply.Status == Status ? 1 : 0;
		}
		return Count;
	}

	static bool IsAlive(const FDTKeeperHarness& H, uint32 Serial)
	{
		return H.Enemies.GetSerials().Contains(Serial);
	}

	/** Fires until the enemy with this serial is gone, at most Steps. */
	static void KillIt(FDTKeeperHarness& H, uint32 Serial, int32 Steps = 3 * Second)
	{
		for (int32 Step = 0; Step < Steps && IsAlive(H, Serial); ++Step)
		{
			H.StepOnce();
		}
		H.StepOnce();
	}

	/** One dash step: the hero jumps by Delta intangible, then lands. */
	static void Dash(FDTKeeperHarness& H, const FVector2f& Delta)
	{
		H.View.HeroPositions[0] += Delta;
		H.View.HeroIntangible[0] = true;
		H.StepOnce();
		H.View.HeroIntangible[0] = false;
		H.StepOnce();
	}

	/** Casts and steps into the window. */
	static void Cast(FDTKeeperHarness& H, int32 Steps = 2)
	{
		H.Keepers.QueueIntercession(0);
		H.StepMany(Steps);
	}

	static const FDTFlagEntry* EntryOf(const FDTKeeperHarness& H, EDTPath Path, EFlag Flag)
	{
		return H.Built(Path, Flag);
	}

	static float CastSteps(float Seconds) { return Seconds * static_cast<float>(Second); }
}

#define DT_S2_TEST(Class, Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, "Dusktracer.Keepers.S2." Name, DTWeaponTestHarness::TestFlags) \
	bool Class::RunTest(const FString& Parameters)

// ---- stage, Mark, Blind (T-KEE-23..25) ---------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2Stage, "Stage") // T-KEE-23
{
	using namespace DTKeeperS2Test;
	TestEqual(TEXT("the S2 rows exist"), DTCat::NumAllArchetypes, 18);
	TestEqual(TEXT("the default run has S1 Keepers"), DTOffers::RunKeepers().Num(), 3);
	TestEqual(TEXT("six Keepers in all"), DTOffers::AllKeepers().Num(), 6);
	for (int32 R = 0; R < DTCat::NumAllArchetypes; ++R)
	{
		for (int32 Column = 0; Column < DTConst::KEEPER_BLESSINGS_PER_ARCHETYPE; ++Column)
		{
			TestTrue(FString::Printf(TEXT("%s accepted"), DTCat::Archetypes[R].BlessingIds[Column]), DTKeepers::IsBlessingAccepted(R, Column));
		}
	}
	FDTKeeperHarness H;
	FDTOfferPool Pool;
	FRandomStream Rng(3);
	FDTOfferHand Hand = DTOffers::BuildHand(H.P(), Pool, EDTHandKind::Patron, 1, Rng);
	TestEqual(TEXT("no S2 card by default"), Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Keeper.Shade")); }), false);
	Pool.Unlocked.Add(TEXT("Keeper.Shade"));
	Hand = DTOffers::BuildHand(H.P(), Pool, EDTHandKind::Patron, 1, Rng);
	TestEqual(TEXT("an unlocked Keeper leads the Patron hand"), Hand.Cards[0].ContentId, FName(TEXT("Keeper.Shade")));
	TestEqual(TEXT("the hand keeps its size"), Hand.Cards.Num(), DTConst::OFFER_KEEPER_CHOICES);
	H.P().Offer.PatronPath = EDTPath::Storm;
	Pool.Unlocked.Add(TEXT("Keeper.Quicksilver"));
	Hand = DTOffers::BuildHand(H.P(), Pool, EDTHandKind::Keeper, 8, Rng);
	TestTrue(TEXT("unlocked Keepers keep a seat at L8"), Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Keeper.Quicksilver")); }));
	H.P().Offer.PatronPath = EDTPath::Shade;
	const TArray<FDTOfferCard> Cards = DTOffers::Candidates(H.P(), Pool);
	TestTrue(TEXT("the Keys of an S2 Patron are offered"), Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Blessing.Shade.Hound.Key")); }));
	return true;
}

DT_S2_TEST(FDTKeeperS2Mark, "Mark") // T-KEE-24
{
	using namespace DTKeeperS2Test;
	const int32 Steps = FMath::RoundToInt(DTConst::STATUS_MARK_S * Second);
	{
		FDTKeeperHarness H(7, 2);
		const int32 Enemy = H.AddEnemy(FVector2f(3000.0f, 0.0f));
		const uint32 Serial = SerialOf(H, Enemy);
		H.StepOnce();
		H.Weapons.ApplyMark(0, Serial, H.Step);
		TestTrue(TEXT("Marked for its owner"), H.Weapons.IsMarked(0, Serial));
		TestFalse(TEXT("not for another player"), H.Weapons.IsMarked(1, Serial));
		H.StepMany(Steps - 2);
		TestTrue(TEXT("still Marked before the end"), H.Weapons.IsMarked(0, Serial));
		H.StepMany(4);
		TestFalse(TEXT("gone at the end"), H.Weapons.IsMarked(0, Serial));
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Shade, TEXT("Shade.Hound"), { TEXT("Key") });
		const int32 Enemy = H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.StepMany(Second);
		TestTrue(TEXT("the Key's hits Mark"), H.Weapons.IsMarked(0, SerialOf(H, Enemy)));
		TestTrue(TEXT("carried by Keeper.Shade"), H.CountStatuses(EDTStatus::Mark, Src(EDTPath::Shade)) > 0);
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2Blind, "Blind") // T-KEE-25
{
	using namespace DTKeeperS2Test;
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Radiance.Prism"), { TEXT("Key"), TEXT("Amp1") });
		H.P().Weapons.Reset();
		H.Weapons.Give(H.Run, 0, PulseRifle, 8);
		H.AddBlock();
		bool bRemembered = false;
		for (int32 Step = 0; Step < 12 * Second; ++Step)
		{
			H.StepOnce();
			for (const uint32 Serial : H.Enemies.GetSerials())
			{
				bRemembered |= H.Weapons.IsBlinded(0, Serial);
			}
		}
		TestTrue(TEXT("Status.Blind is logged"), H.CountStatuses(EDTStatus::Blind, Src(EDTPath::Radiance)) > 0);
		bool bShort = false;
		for (const FDTStatusApply& Apply : H.StatusLog)
		{
			bShort |= Apply.Status == EDTStatus::Freeze && FMath::IsNearlyEqual(Apply.FreezeScale * DTConst::STATUS_FREEZE_S, DTConst::STATUS_BLIND_S, 0.001f);
		}
		TestTrue(TEXT("a Blind is a Freeze of its own length"), bShort);
		TestTrue(TEXT("the owner remembers it"), bRemembered);
	}
	{
		EDTStatus Status;
		TestFalse(TEXT("Quicksilver has no status"), DTKeepers::EnchantStatus(EDTPath::Quicksilver, Status));
		TestTrue(TEXT("Shade Marks"), DTKeepers::EnchantStatus(EDTPath::Shade, Status) && Status == EDTStatus::Mark);
		TestTrue(TEXT("Radiance Blinds"), DTKeepers::EnchantStatus(EDTPath::Radiance, Status) && Status == EDTStatus::Blind);
	}
	return true;
}

// ---- Hound (T-KEE-26) --------------------------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2HoundKey, "Card.Shade.Hound.Key") // T-KEE-26
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hound"), {});
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Hound"), TEXT("Key")));
		}
		const int32 Enemy = H.AddEnemy(FVector2f(600.0f, 0.0f));
		H.StepMany(4); // a round is in the air
		H.Enemies.SetPosition(Enemy, FVector2f(600.0f, 110.0f));
		H.StepMany(Second / 3);
		return H.CountHits(DTWeapons::WeaponSource(DTWeapons::FindWeapon(PulseRifle)));
	};
	TestTrue(TEXT("a seeking round bends onto it"), Scene(true) > Scene(false));
	FDTKeeperHarness H;
	Own(H, EDTPath::Shade, TEXT("Shade.Hound"), { TEXT("Key") });
	const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, 0);
	TestEqual(TEXT("the Key's turn"), Spec.HomingTurnDegS, 60.0f);
	TestEqual(TEXT("one more pierce"), Spec.Pierce, 2);
	return true;
}

DT_S2_TEST(FDTKeeperS2HoundFallbacks, "Seek") // T-KEE-26
{
	using namespace DTKeeperS2Test;
	const uint16 Shade = Src(EDTPath::Shade);
	{
		// Melee: damage against the target class.
		const auto Melee = [Shade](bool bCard)
		{
			FDTKeeperHarness H;
			H.Patron(EDTPath::Storm, 0, false);
			H.Weapons.Give(H.Run, 0, ArcBlade, 1);
			if (bCard)
			{
				H.Bless(Blessing(TEXT("Shade.Hound"), TEXT("Key")));
			}
			const int32 Enemy = H.AddEnemy(FVector2f(100.0f, 0.0f));
			H.StepOnce();
			H.Weapons.ApplyMark(0, SerialOf(H, Enemy), H.Step);
			H.StepMany(2 * Second);
			return H.Damage(Shade);
		};
		TestEqual(TEXT("no flag, no Keeper damage"), Melee(false), 0.0);
		TestTrue(TEXT("a sweep hits a Marked enemy harder"), Melee(true) > 0.0);
	}
	{
		// Orbit: drones pick the Marked enemy first.
		const auto Drones = [](bool bCard)
		{
			FDTKeeperHarness H;
			H.Patron(EDTPath::Storm, 0, false);
			H.Weapons.Give(H.Run, 0, OrbitalDrones, 1);
			if (bCard)
			{
				// The flag alone: the Key would also Mark the near enemy on its first hit.
				FDTWeaponFlags Flags;
				Flags.Add(EFlag::Seek, Src(EDTPath::Shade), EDTFlagTier::Key).Params.Set(TEXT("turn_deg_s"), 60.0f);
				H.Weapons.AddFlagSource(0, TEXT("Test.Seek"), Flags);
			}
			H.AddEnemy(FVector2f(200.0f, 0.0f));
			const int32 Far = H.AddEnemy(FVector2f(200.0f, 420.0f));
			H.StepOnce();
			H.Weapons.ApplyMark(0, SerialOf(H, Far), H.Step);
			H.StepMany(2 * Second);
			return HitsOn(H, SerialOf(H, Far), DTWeapons::WeaponSource(DTWeapons::FindWeapon(OrbitalDrones)));
		};
		TestTrue(TEXT("the flag sends drones at the Marked enemy"), Drones(true) > Drones(false));
	}
	{
		// Mine: drops on the target class.
		FDTKeeperHarness H;
		H.Patron(EDTPath::Storm, 0, false);
		H.Weapons.Give(H.Run, 0, GravityMine, 1);
		H.Bless(Blessing(TEXT("Shade.Hound"), TEXT("Key")));
		for (int32 X = 0; X < 6; ++X)
		{
			H.AddEnemy(FVector2f(300.0f + 10.0f * X, 0.0f));
		}
		const int32 Lone = H.AddEnemy(FVector2f(-300.0f, 0.0f));
		H.StepOnce();
		H.Weapons.ApplyMark(0, SerialOf(H, Lone), H.Step);
		H.StepMany(4 * Second);
		const uint16 Mine = DTWeapons::WeaponSource(DTWeapons::FindWeapon(GravityMine));
		TestTrue(TEXT("a mine lands on the lone Marked enemy over the crowd"), HitsOn(H, SerialOf(H, Lone), Mine) > 0);
	}
	{
		// Homing: + turn rate.
		FDTKeeperHarness H;
		H.Patron(EDTPath::Storm, 0, false);
		H.Weapons.Give(H.Run, 0, TEXT("Weapon.SeekerSwarm"), 1);
		const float Before = H.Weapons.ComposeSlot(H.Run, 0, 0).HomingTurnDegS;
		H.Bless(Blessing(TEXT("Shade.Hound"), TEXT("Key")));
		TestEqual(TEXT("a missile turns harder"), H.Weapons.ComposeSlot(H.Run, 0, 0).HomingTurnDegS, Before + 60.0f);
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2HoundAmp1, "Card.Shade.Hound.Amp1") // T-KEE-26
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hound"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Hound"), TEXT("Amp1")));
		}
		const int32 Enemy = H.AddEnemy(FVector2f(5000.0f, 0.0f));
		H.StepOnce();
		H.Weapons.ApplyMark(0, SerialOf(H, Enemy), H.Step);
		H.StepMany(FMath::RoundToInt((DTConst::STATUS_MARK_S + 1.0f) * Second));
		return H.Weapons.IsMarked(0, SerialOf(H, Enemy));
	};
	TestFalse(TEXT("the Mark has run out"), Scene(false));
	TestTrue(TEXT("the card lengthens it"), Scene(true));
	FDTKeeperHarness H;
	Own(H, EDTPath::Shade, TEXT("Shade.Hound"), { TEXT("Key"), TEXT("Amp1") });
	TestEqual(TEXT("and turns harder"), H.Weapons.ComposeSlot(H.Run, 0, 0).HomingTurnDegS, 90.0f);
	return true;
}

DT_S2_TEST(FDTKeeperS2HoundAmp2, "Card.Shade.Hound.Amp2") // T-KEE-26
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hound"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Hound"), TEXT("Amp2")));
		}
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.StepMany(3 * Second);
		return H.Damage(Src(EDTPath::Shade));
	};
	TestEqual(TEXT("no share without the card"), Scene(false), 0.0);
	TestTrue(TEXT("Marked enemies take the share as Keeper.Shade"), Scene(true) > 0.0);
	return true;
}

DT_S2_TEST(FDTKeeperS2HoundTrait, "Card.Shade.Hound.Trait") // T-KEE-26
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard, EDTEnemyRole Role)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hound"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Hound"), TEXT("Trait")));
		}
		const int32 Enemy = AddScaled(H, FVector2f(300.0f, 0.0f), Role);
		H.Enemies.SetHp(Enemy, H.Enemies.GetMaxHp()[Enemy] * 0.05f);
		const uint32 Serial = SerialOf(H, Enemy);
		H.StepMany(Second);
		return IsAlive(H, Serial);
	};
	TestTrue(TEXT("without the card the enemy stands"), Scene(false, EDTEnemyRole::Swarm));
	TestFalse(TEXT("the card executes it"), Scene(true, EDTEnemyRole::Swarm));
	TestTrue(TEXT("a boss is never executed"), Scene(true, EDTEnemyRole::Boss));
	return true;
}

// ---- Collectors (T-KEE-27, T-KEE-28) -----------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2HuntersKey, "Card.Shade.Hunters.Key") // T-KEE-27
{
	using namespace DTKeeperS2Test;
	const auto KillOne = [](FDTKeeperHarness& H, bool bMark, bool bElite)
	{
		const int32 Enemy = bElite ? AddScaled(H, FVector2f(300.0f, 0.0f), EDTEnemyRole::Swarm, true) : H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f);
		H.Enemies.SetHp(Enemy, 1.0f);
		const uint32 Serial = SerialOf(H, Enemy);
		H.StepOnce();
		if (bMark)
		{
			H.Weapons.ApplyMark(0, Serial, H.Step);
		}
		KillIt(H, Serial);
	};
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key") });
		KillOne(H, false, false);
		TestEqual(TEXT("a plain kill raises nothing"), H.Weapons.GetLiveHunters(0), 0);
		KillOne(H, true, false);
		TestEqual(TEXT("a Marked kill raises a hunter"), H.Weapons.GetLiveHunters(0), 1);
		KillOne(H, false, true);
		TestEqual(TEXT("an elite kill raises one"), H.Weapons.GetLiveHunters(0), 2);
		H.Weapons.RaiseHunters(0, 9, FVector2f::ZeroVector, 6.0f, 0.4f);
		TestEqual(TEXT("never above max_live"), H.Weapons.GetLiveHunters(0), 6);
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key") });
		H.StepOnce();
		H.Weapons.RaiseHunters(0, 1, FVector2f(-400.0f, 0.0f), 6.0f, 0.4f);
		const int32 Enemy = H.AddEnemy(FVector2f(-800.0f, 0.0f));
		H.StepMany(2 * Second);
		TestTrue(TEXT("a hunter fires as Keeper.Shade"), HitsOn(H, SerialOf(H, Enemy), Src(EDTPath::Shade)) > 0);
		H.StepMany(5 * Second);
		TestEqual(TEXT("and fades"), H.Weapons.GetLiveHunters(0), 0);
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2HunterCue, "HunterCue") // T-KEE-28
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key") });
	H.StepOnce();
	TArray<FDTWeaponVisual> Before;
	H.Weapons.GatherVisuals(H.Run, H.View, Before);
	H.Weapons.RaiseHunters(0, 1, FVector2f(50.0f, 0.0f), 6.0f, 0.4f);
	TArray<FDTWeaponVisual> After;
	H.Weapons.GatherVisuals(H.Run, H.View, After);
	const auto Drones = [](const TArray<FDTWeaponVisual>& V)
	{
		return V.FilterByPredicate([](const FDTWeaponVisual& X) { return X.Kind == FDTWeaponVisual::EKind::Drone; }).Num();
	};
	TestEqual(TEXT("a hunter draws as a core"), Drones(After), Drones(Before) + 1);
	return true;
}

DT_S2_TEST(FDTKeeperS2HuntersAmp1, "Card.Shade.Hunters.Amp1") // T-KEE-27
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key"), TEXT("Amp1") });
	const FDTFlagEntry* Entry = EntryOf(H, EDTPath::Shade, EFlag::Hunter);
	if (TestNotNull(TEXT("the flag"), Entry))
	{
		TestEqual(TEXT("life"), Entry->Params.Get(TEXT("life_s")), 8.0f);
		TestEqual(TEXT("damage"), Entry->Params.Get(DTFlagParam::Dmg), 0.5f);
	}
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness S;
		Own(S, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key") });
		if (bCard)
		{
			S.Bless(Blessing(TEXT("Shade.Hunters"), TEXT("Amp1")));
		}
		const uint32 Serial = SerialOf(S, S.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f));
		S.StepOnce();
		S.Weapons.ApplyMark(0, Serial, S.Step);
		KillIt(S, Serial);
		S.StepMany(7 * Second);
		return S.Weapons.GetLiveHunters(0);
	};
	TestEqual(TEXT("six seconds and it fades"), Scene(false), 0);
	TestEqual(TEXT("the card keeps it longer"), Scene(true), 1);
	return true;
}

DT_S2_TEST(FDTKeeperS2HuntersAmp2, "Card.Shade.Hunters.Amp2") // T-KEE-27
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Hunters"), TEXT("Amp2")));
		}
		H.StepOnce();
		H.P().Weapons[0].Cooldown = 1000.0f; // the hunter does the killing
		H.Weapons.RaiseHunters(0, 1, FVector2f(0.0f, 0.0f), 2.0f, 0.4f);
		for (int32 Index = 0; Index < 4; ++Index)
		{
			H.AddEnemy(FVector2f(300.0f + 60.0f * Index, 0.0f), 1.0f);
		}
		H.StepMany(3 * Second);
		return H.Weapons.GetLiveHunters(0);
	};
	TestEqual(TEXT("it fades on time"), Scene(false), 0);
	TestEqual(TEXT("a hunter's kills keep it"), Scene(true), 1);
	return true;
}

DT_S2_TEST(FDTKeeperS2HuntersTrait, "Card.Shade.Hunters.Trait") // T-KEE-27
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Hunters"), TEXT("Trait")));
		}
		for (int32 Index = 0; Index < 25; ++Index)
		{
			H.AddEnemy(FVector2f(200.0f + 8.0f * Index, 0.0f), 1.0f);
		}
		H.StepMany(25 * Second);
		return H.Weapons.GetLiveHunters(0);
	};
	TestEqual(TEXT("no lasting hunter"), Scene(false), 0);
	TestEqual(TEXT("the kills raise one that stays"), Scene(true), 1);
	return true;
}

DT_S2_TEST(FDTKeeperS2HuntersTiers, "Tier.Shade.Hunters") // T-KEE-27
{
	using namespace DTKeeperS2Test;
	const uint16 Shade = Src(EDTPath::Shade);
	{
		const auto Scene = [](int32 Tier)
		{
			FDTKeeperHarness H;
			Own(H, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key") });
			H.P().Keepers.Tier[static_cast<int32>(EDTPath::Shade)] = Tier;
			H.Keepers.RebuildFlags(H.Run, 0, H.Weapons);
			for (int32 Index = 0; Index < 60; ++Index)
			{
				H.AddEnemy(FVector2f(200.0f + 4.0f * Index, 0.0f), 1.0f);
			}
			int32 Most = 0;
			for (int32 Step = 0; Step < 30 * Second; ++Step)
			{
				H.StepOnce();
				Most = FMath::Max(Most, H.Weapons.GetLiveHunters(0));
			}
			return Most;
		};
		TestEqual(TEXT("below the tier a plain kill raises none"), Scene(1), 0);
		TestTrue(TEXT("Tier 2: any kill may"), Scene(2) > 0);
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Hunters"), { TEXT("Key") });
		H.P().Keepers.Tier[static_cast<int32>(EDTPath::Shade)] = 3;
		H.Keepers.RebuildFlags(H.Run, 0, H.Weapons);
		H.StepOnce();
		H.Weapons.RaiseHunters(0, 1, FVector2f(-500.0f, 0.0f), 0.2f, 0.4f);
		const int32 Enemy = H.AddEnemy(FVector2f(-560.0f, 0.0f));
		H.StepMany(Second);
		TestTrue(TEXT("Tier 3: a fading hunter explodes"), HitsOn(H, SerialOf(H, Enemy), Shade) > 0);
	}
	return true;
}

// ---- Ledger (T-KEE-29) -------------------------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2EconomyKey, "Card.Shade.Economy.Key") // T-KEE-29
{
	using namespace DTKeeperS2Test;
	const uint16 Shade = Src(EDTPath::Shade);
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Shade.Economy"), { TEXT("Key") });
	const uint32 Weak = SerialOf(H, H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f));
	H.StepOnce();
	H.Weapons.ApplyMark(0, Weak, H.Step);
	KillIt(H, Weak);
	TestEqual(TEXT("a Marked kill adds Debt"), H.Weapons.GetDebt(0), 1);
	const int32 Front = H.AddEnemy(FVector2f(300.0f, 0.0f));
	const int32 Back = H.AddEnemy(FVector2f(-600.0f, 0.0f));
	H.Weapons.AddDebt(0, 14);
	TestTrue(TEXT("full Debt readies a payday"), H.Weapons.IsPaydayReady(0));
	TestEqual(TEXT("and resets"), H.Weapons.GetDebt(0), 0);
	H.StepMany(Second);
	TestTrue(TEXT("the payday reaches the enemy the aim ignores"), HitsOn(H, SerialOf(H, Back), Shade) > 0);
	TestTrue(TEXT("and the near one"), HitsOn(H, SerialOf(H, Front), Shade) > 0);
	TestFalse(TEXT("it is spent"), H.Weapons.IsPaydayReady(0));
	const int32 Later = H.CountHits(Shade);
	H.StepMany(Second);
	TestEqual(TEXT("one volley only"), H.CountHits(Shade), Later);
	return true;
}

DT_S2_TEST(FDTKeeperS2EconomyVolley, "Card.Shade.Economy.Volley") // T-KEE-29: a payday replaces one volley
{
	using namespace DTKeeperS2Test;
	const uint16 Shade = Src(EDTPath::Shade);
	const uint16 Rifle = DTWeapons::WeaponSource(DTWeapons::FindWeapon(PulseRifle));
	FDTKeeperHarness H;
	Own(H, EDTPath::Shade, TEXT("Shade.Economy"), { TEXT("Key") });
	H.AddEnemy(FVector2f(300.0f, 0.0f));
	int32 Guard = 0;
	while (H.CountHits(Rifle) == 0 && Guard++ < Second * 3)
	{
		H.StepOnce(); // the rifle has just fired: its cooldown is running
	}
	const int32 Gap = FMath::CeilToInt(H.Weapons.GetLastSpec(0, 0).Cooldown * DTConst::SIM_FIXED_HZ);
	const int32 Before = H.CountHits(Rifle);
	H.Weapons.AddDebt(0, 15);
	H.StepOnce();
	TestTrue(TEXT("a payday waits for the next volley"), H.Weapons.IsPaydayReady(0));
	TestEqual(TEXT("nothing is paid between volleys"), H.CountHits(Shade), 0);
	H.StepMany(Gap + 1);
	TestFalse(TEXT("the next volley spends it"), H.Weapons.IsPaydayReady(0));
	H.StepMany(FMath::CeilToInt(300.0f / H.Weapons.GetLastSpec(0, 0).Speed * DTConst::SIM_FIXED_HZ) + 2);
	TestTrue(TEXT("its round lands as Shade"), H.CountHits(Shade) > 0);
	// The rounds already in flight land; a volley fired with the payday would add to them within its flight time.
	TestTrue(FString::Printf(TEXT("instead of its pattern (%d rifle hits then, %d now)"), Before, H.CountHits(Rifle)), H.CountHits(Rifle) <= Before + 1);
	return true;
}

DT_S2_TEST(FDTKeeperS2EconomyAmp1, "Card.Shade.Economy.Amp1") // T-KEE-29
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Economy"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Economy"), TEXT("Amp1")));
		}
		const int32 Elite = AddScaled(H, FVector2f(300.0f, 0.0f), EDTEnemyRole::Swarm, true);
		H.Enemies.SetHp(Elite, 1.0f);
		const uint32 Serial = SerialOf(H, Elite);
		H.StepOnce();
		H.Weapons.ApplyMark(0, Serial, H.Step);
		KillIt(H, Serial);
		return H.Weapons.GetDebt(0);
	};
	TestEqual(TEXT("one Debt"), Scene(false), 1);
	TestEqual(TEXT("an elite adds more"), Scene(true), 2);
	return true;
}

DT_S2_TEST(FDTKeeperS2EconomyAmp2, "Card.Shade.Economy.Amp2") // T-KEE-29
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Economy"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Economy"), TEXT("Amp2")));
		}
		for (int32 Index = 0; Index < 4; ++Index)
		{
			H.AddEnemy(FVector2f(300.0f + 40.0f * Index, 0.0f));
		}
		H.Weapons.AddDebt(0, 15);
		H.StepMany(Second);
		return H.CountHits(Src(EDTPath::Shade));
	};
	TestTrue(TEXT("payday rounds pass through"), Scene(true) > Scene(false));
	return true;
}

DT_S2_TEST(FDTKeeperS2EconomyTrait, "Card.Shade.Economy.Trait") // T-KEE-29
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Shade.Economy"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Shade.Economy"), TEXT("Trait")));
		}
		const int32 Enemy = AddScaled(H, FVector2f(900.0f, 800.0f));
		H.Enemies.SetHp(Enemy, H.Enemies.GetMaxHp()[Enemy] * 0.2f);
		const uint32 Serial = SerialOf(H, Enemy);
		H.StepOnce();
		H.Weapons.ApplyMark(0, Serial, H.Step);
		H.Weapons.AddDebt(0, 15);
		H.StepMany(Second / 2);
		return IsAlive(H, Serial);
	};
	TestTrue(TEXT("a payday alone does not finish it"), Scene(false));
	TestFalse(TEXT("the audit executes it"), Scene(true));
	return true;
}

// ---- crit and Lance (T-KEE-30, T-KEE-31) ---------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2Crit, "Crit") // T-KEE-30
{
	using namespace DTKeeperS2Test;
	const uint16 Radiance = Src(EDTPath::Radiance);
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Radiance.Prism"), { TEXT("Key") });
		const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, 0);
		TestEqual(TEXT("crit_add"), Spec.CritChance, 0.2f);
		TestEqual(TEXT("crit_mult"), Spec.CritMult, 2.0f);
		H.P().Keepers.Tier[static_cast<int32>(EDTPath::Radiance)] = 3;
		H.Keepers.RebuildFlags(H.Run, 0, H.Weapons);
		const FDTWeaponSpec Tiered = H.Weapons.ComposeSlot(H.Run, 0, 0);
		TestEqual(TEXT("tiers add"), Tiered.CritChance, 0.3f);
		TestEqual(TEXT("Tier 3 raises crit damage"), Tiered.CritMult, 2.5f);
	}
	{
		// Under the cap whatever the sum.
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Radiance.Prism"), { TEXT("Key") });
		FDTWeaponFlags Flags;
		FDTFlagEntry& Entry = Flags.Add(EFlag::CritSplit, Radiance, EDTFlagTier::Key);
		Entry.Params.Set(TEXT("crit_add"), 5.0f);
		H.Weapons.AddFlagSource(0, TEXT("Test.Crit"), Flags);
		TestEqual(TEXT("the cap holds"), H.Weapons.ComposeSlot(H.Run, 0, 0).CritChance, static_cast<float>(DTConst::MOD_CRIT_CHANCE_CAP));
	}
	{
		// Tier 2's pulse and Tier 3's status on a crit.
		const auto Scene = [](int32 Tier)
		{
			FDTKeeperHarness H;
			Own(H, EDTPath::Storm, TEXT("Radiance.Lance"), { TEXT("Key"), TEXT("Trait") });
			H.P().Keepers.Tier[static_cast<int32>(EDTPath::Radiance)] = Tier;
			H.Keepers.RebuildFlags(H.Run, 0, H.Weapons);
			H.AddEnemy(FVector2f(300.0f, 0.0f));
			const int32 Near = H.AddEnemy(FVector2f(300.0f, 60.0f));
			H.StepMany(6 * Second);
			return TPair<int32, int32>(H.CountHits(Src(EDTPath::Radiance)), StatusesOn(H, SerialOf(H, Near), EDTStatus::Blind) + StatusesOn(H, H.Enemies.GetSerials()[0], EDTStatus::Blind));
		};
		TestTrue(TEXT("Tier 2 pulses"), Scene(2).Key > Scene(1).Key);
		TestEqual(TEXT("no Blind below Tier 3"), Scene(2).Value, 0);
		TestTrue(TEXT("Tier 3 Blinds on a crit"), Scene(3).Value > 0);
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2LanceKey, "Card.Radiance.Lance.Key") // T-KEE-31
{
	using namespace DTKeeperS2Test;
	const uint16 Radiance = Src(EDTPath::Radiance);
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Radiance.Lance"), { TEXT("Key") });
	const int32 Front = H.AddEnemy(FVector2f(300.0f, 0.0f));
	const int32 Behind = H.AddEnemy(FVector2f(340.0f, 0.0f));
	H.StepMany(4 * Second);
	const uint16 Rifle = DTWeapons::WeaponSource(DTWeapons::FindWeapon(PulseRifle));
	const int32 Charged = HitsOn(H, SerialOf(H, Front), Radiance);
	const int32 Plain = HitsOn(H, SerialOf(H, Front), Rifle);
	TestTrue(TEXT("some shots are charged"), Charged > 0);
	TestTrue(TEXT("one in five"), Plain >= 3 * Charged);
	TestTrue(TEXT("a charged shot pierces"), HitsOn(H, SerialOf(H, Behind), Radiance) > 0);
	float Most = 0.0f;
	for (const FDTHit& Hit : H.HitLog)
	{
		Most = Hit.SourceId == Radiance ? FMath::Max(Most, Hit.Damage) : Most;
	}
	TestEqual(TEXT("for three times the damage"), Most, 3.0f * H.Weapons.GetLastSpec(0, 0).Damage);
	return true;
}

DT_S2_TEST(FDTKeeperS2LanceAmp1, "Card.Radiance.Lance.Amp1") // T-KEE-31
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H(11);
		Own(H, EDTPath::Storm, TEXT("Radiance.Lance"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Radiance.Lance"), TEXT("Amp1")));
		}
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.StepMany(30 * Second);
		int32 Crits = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			Crits += Hit.bCrit ? 1 : 0;
		}
		return Crits;
	};
	TestEqual(TEXT("no crit source"), Scene(false), 0);
	TestTrue(TEXT("charged shots crit sometimes"), Scene(true) > 0);
	return true;
}

DT_S2_TEST(FDTKeeperS2LanceAmp2, "Card.Radiance.Lance.Amp2") // T-KEE-31
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Radiance.Lance"), { TEXT("Key"), TEXT("Trait") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Radiance.Lance"), TEXT("Amp2")));
		}
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.StepMany(10 * Second);
		return H.CountHits(Src(EDTPath::Radiance));
	};
	TestTrue(TEXT("repeat crits on one enemy add hits"), Scene(true) > Scene(false));
	return true;
}

DT_S2_TEST(FDTKeeperS2LanceTrait, "Card.Radiance.Lance.Trait") // T-KEE-31
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Radiance.Lance"), { TEXT("Key"), TEXT("Trait") });
	H.AddEnemy(FVector2f(300.0f, 0.0f));
	H.StepMany(5 * Second);
	int32 Charged = 0;
	int32 Crits = 0;
	for (const FDTHit& Hit : H.HitLog)
	{
		if (Hit.SourceId == Src(EDTPath::Radiance))
		{
			++Charged;
			Crits += Hit.bCrit ? 1 : 0;
		}
	}
	TestTrue(TEXT("charged shots land"), Charged > 0);
	TestEqual(TEXT("and every one crits"), Crits, Charged);
	return true;
}

// ---- Prism (T-KEE-32) --------------------------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2PrismKey, "Card.Radiance.Prism.Key") // T-KEE-32
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H(5);
	Own(H, EDTPath::Storm, TEXT("Radiance.Prism"), { TEXT("Key") });
	H.AddBlock();
	H.StepMany(12 * Second);
	TestTrue(TEXT("crits fan lances as Keeper.Radiance"), H.CountHits(Src(EDTPath::Radiance)) > 0);
	TestEqual(TEXT("lances never split"), H.Weapons.GetMaxGeneration(), 1);
	return true;
}

DT_S2_TEST(FDTKeeperS2PrismAmp1, "Card.Radiance.Prism.Amp1") // T-KEE-32
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H(5);
		Own(H, EDTPath::Storm, TEXT("Radiance.Prism"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Radiance.Prism"), TEXT("Amp1")));
		}
		H.AddBlock();
		H.StepMany(12 * Second);
		return H.CountStatuses(EDTStatus::Blind, Src(EDTPath::Radiance));
	};
	TestEqual(TEXT("no Blind"), Scene(false), 0);
	TestTrue(TEXT("lances Blind"), Scene(true) > 0);
	return true;
}

DT_S2_TEST(FDTKeeperS2PrismAmp2, "Card.Radiance.Prism.Amp2") // T-KEE-32
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H(5);
		Own(H, EDTPath::Storm, TEXT("Radiance.Prism"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Radiance.Prism"), TEXT("Amp2")));
		}
		H.AddBlock();
		H.StepMany(12 * Second);
		return H.CountHits(Src(EDTPath::Radiance));
	};
	TestTrue(TEXT("every few crits a pulse adds hits"), Scene(true) > Scene(false));
	return true;
}

DT_S2_TEST(FDTKeeperS2PrismTrait, "Card.Radiance.Prism.Trait") // T-KEE-32
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Radiance.Prism"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Radiance.Prism"), TEXT("Trait")));
		}
		const int32 Enemy = H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f);
		const int32 Near = H.AddEnemy(FVector2f(1500.0f, 1400.0f));
		H.Enemies.SetPosition(Near, FVector2f(300.0f, 120.0f));
		H.StepOnce();
		FDTStatusApply Apply;
		Apply.Handle = FDTEnemyHandle{ Enemy, SerialOf(H, Enemy) };
		Apply.Status = EDTStatus::Blind;
		Apply.OwnerId = 0;
		H.View.StatusApplies.Add(Apply);
		H.Weapons.AbsorbStatuses(H.Context());
		const uint32 NearSerial = SerialOf(H, Near);
		H.StepMany(Second);
		return StatusesOn(H, NearSerial, EDTStatus::Blind);
	};
	TestEqual(TEXT("a Blinded death alone spreads nothing"), Scene(false), 0);
	TestTrue(TEXT("the card spreads it"), Scene(true) > 0);
	return true;
}

// ---- Solar (T-KEE-33) --------------------------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2SolarKey, "Card.Radiance.Solar.Key") // T-KEE-33
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Radiance.Solar"), { TEXT("Key") });
	H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f);
	const uint32 Near = SerialOf(H, H.AddEnemy(FVector2f(400.0f, 0.0f)));
	const uint32 Far = SerialOf(H, H.AddEnemy(FVector2f(300.0f, 240.0f)));
	H.StepMany(Second);
	const uint16 Radiance = Src(EDTPath::Radiance);
	TestTrue(TEXT("overkill bursts on a neighbour"), HitsOn(H, Near, Radiance) > 0);
	TestEqual(TEXT("not past the radius"), HitsOn(H, Far, Radiance), 0);
	return true;
}

DT_S2_TEST(FDTKeeperS2SolarAmp1, "Card.Radiance.Solar.Amp1") // T-KEE-33
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Radiance.Solar"), { TEXT("Key"), TEXT("Amp1") });
	H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f);
	const uint32 Far = SerialOf(H, H.AddEnemy(FVector2f(300.0f, 240.0f)));
	H.StepMany(Second);
	TestTrue(TEXT("the wider burst reaches it"), HitsOn(H, Far, Src(EDTPath::Radiance)) > 0);
	return true;
}

DT_S2_TEST(FDTKeeperS2SolarAmp2, "Card.Radiance.Solar.Amp2") // T-KEE-33
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Radiance.Solar"), { TEXT("Key"), TEXT("Amp2") });
	H.AddEnemy(FVector2f(300.0f, 0.0f));
	H.StepMany(Second);
	TestTrue(TEXT("impacts leave light"), H.Weapons.GetLiveFields() > 0);
	return true;
}

DT_S2_TEST(FDTKeeperS2SolarTrait, "Card.Radiance.Solar.Trait") // T-KEE-33
{
	using namespace DTKeeperS2Test;
	const uint16 Radiance = Src(EDTPath::Radiance);
	const auto Ring = [Radiance](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Radiance.Solar"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Radiance.Solar"), TEXT("Trait")));
		}
		H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f);
		H.AddEnemy(FVector2f(400.0f, 0.0f));
		H.StepMany(Second);
		return H.Damage(Radiance);
	};
	TestEqual(TEXT("a kill in the ring bursts double"), Ring(true), 2.0 * Ring(false));
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Radiance.Solar"), { TEXT("Key"), TEXT("Trait") });
	const int32 Elite = AddScaled(H, FVector2f(300.0f, 0.0f), EDTEnemyRole::Swarm, true);
	H.StepMany(4 * Second);
	TestTrue(TEXT("a sun charges off an elite and bursts on it"), HitsOn(H, SerialOf(H, Elite), Radiance) > 0);
	return true;
}

// ---- Quicksilver (T-KEE-34) ----------------------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2VectorKey, "Card.Quicksilver.Vector.Key") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const uint16 Quick = Src(EDTPath::Quicksilver);
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Vector"), { TEXT("Key") });
		const int32 Enemy = H.AddEnemy(FVector2f(0.0f, 700.0f));
		H.StepOnce();
		H.P().Weapons[0].Cooldown = 10.0f; // its own cadence stays out of this
		Dash(H, FVector2f(0.0f, 200.0f));
		H.StepMany(Second / 2);
		TestTrue(TEXT("a dash fires along its line as Keeper.Quicksilver"), HitsOn(H, SerialOf(H, Enemy), Quick) > 0);
		float Most = 0.0f;
		for (const FDTHit& Hit : H.HitLog)
		{
			Most = Hit.SourceId == Quick ? FMath::Max(Most, Hit.Damage) : Most;
		}
		TestEqual(TEXT("at the Key's multiplier"), Most, 1.5f * H.Weapons.GetLastSpec(0, 0).Damage);
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Vector"), { TEXT("Key") });
		H.AddEnemy(FVector2f(300.0f, 0.0f), 1.0f);
		H.StepOnce();
		H.P().Weapons[0].Cooldown = 10.0f;
		Dash(H, FVector2f(50.0f, 0.0f));
		H.StepMany(Second);
		TestEqual(TEXT("its kill refunds cooldown"), H.Weapons.TakeDashRefund(0), 0.15f);
		TestEqual(TEXT("once"), H.Weapons.TakeDashRefund(0), 0.0f);
	}
	{
		// Melee sweeps at the landing.
		FDTKeeperHarness H;
		H.Patron(EDTPath::Storm, 0, false);
		H.Weapons.Give(H.Run, 0, ArcBlade, 1);
		H.Bless(Blessing(TEXT("Quicksilver.Vector"), TEXT("Key")));
		const int32 Enemy = H.AddEnemy(FVector2f(100.0f, 0.0f));
		H.StepOnce();
		H.P().Weapons[0].Cooldown = 10.0f;
		H.View.HeroPositions[0] += FVector2f(20.0f, 0.0f);
		H.View.HeroIntangible[0] = true;
		H.StepOnce();
		TestEqual(TEXT("no sweep at the start"), HitsOn(H, SerialOf(H, Enemy), Quick), 0);
		H.View.HeroIntangible[0] = false;
		H.StepOnce();
		TestTrue(TEXT("a sweep at the landing"), HitsOn(H, SerialOf(H, Enemy), Quick) > 0);
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2VectorAmp1, "Card.Quicksilver.Vector.Amp1") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Quicksilver.Vector"), { TEXT("Key") });
	TestEqual(TEXT("no change"), H.Keepers.GetHeroRiders(0).DashCooldownMul, 1.0f);
	H.Bless(Blessing(TEXT("Quicksilver.Vector"), TEXT("Amp1")));
	TestEqual(TEXT("a shorter dash cooldown"), H.Keepers.GetHeroRiders(0).DashCooldownMul, 0.8f);
	return true;
}

DT_S2_TEST(FDTKeeperS2VectorAmp2, "Card.Quicksilver.Vector.Amp2") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Vector"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Quicksilver.Vector"), TEXT("Amp2")));
		}
		H.StepOnce();
		Dash(H, FVector2f(50.0f, 0.0f));
		return H.Weapons.ComposeSlot(H.Run, 0, 0).Cooldown / FMath::Max(H.Weapons.GetLastSpec(0, 0).Cooldown, 0.001f);
	};
	TestEqual(TEXT("no change"), Scene(false), 1.0f);
	TestEqual(TEXT("fire rate rises after a dash"), Scene(true), 1.5f);
	return true;
}

DT_S2_TEST(FDTKeeperS2VectorTrait, "Card.Quicksilver.Vector.Trait") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Quicksilver.Vector"), { TEXT("Key"), TEXT("Trait") });
	H.StepOnce();
	Dash(H, FVector2f(50.0f, 0.0f));
	Dash(H, FVector2f(50.0f, 0.0f));
	TestEqual(TEXT("no refill yet"), H.Weapons.TakeDashRefills(0), 0);
	Dash(H, FVector2f(50.0f, 0.0f));
	TestEqual(TEXT("every third dash refills"), H.Weapons.TakeDashRefills(0), 1);
	return true;
}

DT_S2_TEST(FDTKeeperS2QuicksilverTiers, "Tier.Quicksilver") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	for (const TCHAR* Arch : { TEXT("Quicksilver.Vector"), TEXT("Quicksilver.Momentum"), TEXT("Quicksilver.Rebound") })
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, Arch, { TEXT("Key") });
		H.P().Keepers.Leading[static_cast<int32>(EDTPath::Quicksilver)] = FName(*FString::Printf(TEXT("Archetype.%s"), Arch));
		H.P().Keepers.Tier[static_cast<int32>(EDTPath::Quicksilver)] = 1;
		H.Keepers.RebuildFlags(H.Run, 0, H.Weapons);
		TestEqual(FString::Printf(TEXT("%s no charge at Tier 1"), Arch), H.Keepers.GetHeroRiders(0).DashCharges, 0);
		H.P().Keepers.Tier[static_cast<int32>(EDTPath::Quicksilver)] = 2;
		H.Keepers.RebuildFlags(H.Run, 0, H.Weapons);
		TestEqual(FString::Printf(TEXT("%s Tier 2: one more dash charge"), Arch), H.Keepers.GetHeroRiders(0).DashCharges, 1);
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2MomentumKey, "Card.Quicksilver.Momentum.Key") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	Own(H, EDTPath::Storm, TEXT("Quicksilver.Momentum"), { TEXT("Key") });
	H.StepOnce();
	const float Base = H.Weapons.GetLastSpec(0, 0).Damage;
	H.View.HeroPositions[0] += FVector2f(500.0f, 0.0f);
	H.StepOnce();
	TestEqual(TEXT("moving builds it"), H.Weapons.GetMomentum(0), 5.0f);
	H.View.HeroPositions[0] += FVector2f(10.0f, 0.0f);
	H.StepOnce();
	TestEqual(TEXT("each point adds damage"), H.Weapons.GetLastSpec(0, 0).Damage, Base * (1.0f + 0.02f * H.Weapons.GetMomentum(0)), 0.01f);
	const float Before = H.Weapons.GetMomentum(0);
	Dash(H, FVector2f::ZeroVector);
	TestTrue(TEXT("a dash adds five"), H.Weapons.GetMomentum(0) > Before + 4.0f);
	H.View.HeroPositions[0] += FVector2f(5000.0f, 0.0f);
	H.StepOnce();
	TestEqual(TEXT("capped"), H.Weapons.GetMomentum(0), 30.0f);
	H.View.HeroHurt[0] = true;
	H.StepOnce();
	H.View.HeroHurt[0] = false;
	TestTrue(TEXT("a hit loses ten"), H.Weapons.GetMomentum(0) <= 20.0f);
	const float Held = H.Weapons.GetMomentum(0);
	H.StepMany(2 * Second);
	TestTrue(TEXT("still, it fades"), H.Weapons.GetMomentum(0) < Held);
	return true;
}

DT_S2_TEST(FDTKeeperS2MomentumAmp1, "Card.Quicksilver.Momentum.Amp1") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Momentum"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Quicksilver.Momentum"), TEXT("Amp1")));
		}
		H.StepOnce();
		Dash(H, FVector2f::ZeroVector);
		const float After = H.Weapons.GetMomentum(0);
		H.StepMany(Second);
		return TPair<float, float>(After, H.Weapons.GetMomentum(0));
	};
	TestTrue(TEXT("a dash gives more"), Scene(true).Key > Scene(false).Key + 4.0f);
	TestTrue(TEXT("and it fades slower"), Scene(true).Key - Scene(true).Value < Scene(false).Key - Scene(false).Value);
	return true;
}

DT_S2_TEST(FDTKeeperS2MomentumAmp2, "Card.Quicksilver.Momentum.Amp2") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Momentum"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Quicksilver.Momentum"), TEXT("Amp2")));
		}
		H.AddEnemy(FVector2f(1200.0f, 150.0f));
		H.StepOnce();
		H.View.HeroPositions[0] += FVector2f(1100.0f, 0.0f);
		H.StepMany(2);
		return H.CountHits(Src(EDTPath::Quicksilver));
	};
	TestEqual(TEXT("no wave"), Scene(false), 0);
	TestTrue(TEXT("distance releases one"), Scene(true) > 0);
	return true;
}

DT_S2_TEST(FDTKeeperS2MomentumTrait, "Card.Quicksilver.Momentum.Trait") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard, bool bFull)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Momentum"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Quicksilver.Momentum"), TEXT("Trait")));
		}
		H.StepOnce();
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Weapons.GrantMomentum(0, bFull ? 30.0f : 10.0f, true);
		H.StepMany(2 * Second);
		return TPair<int32, int32>(H.CountHits(Src(EDTPath::Quicksilver)), H.Weapons.GetLastSpec(0, 0).Pierce);
	};
	TestEqual(TEXT("no strike without the card"), Scene(false, true).Key, 0);
	TestEqual(TEXT("no strike short of the cap"), Scene(true, false).Key, 0);
	TestTrue(TEXT("at the cap each impact echoes"), Scene(true, true).Key > 0);
	TestEqual(TEXT("and pierces one more"), Scene(true, true).Value, Scene(false, true).Value + 1);
	return true;
}

DT_S2_TEST(FDTKeeperS2ReboundKey, "Card.Quicksilver.Rebound.Key") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const uint16 Quick = Src(EDTPath::Quicksilver);
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Rebound"), { TEXT("Key") });
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		const int32 Other = H.AddEnemy(FVector2f(300.0f, 200.0f));
		H.StepMany(2 * Second);
		TestTrue(TEXT("a round bounces on"), H.Enemies.GetSerials().Num() > 0 && HitsOn(H, SerialOf(H, Other), Quick) > 0);
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Rebound"), { TEXT("Key") });
		const int32 Alone = H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.StepMany(2 * Second);
		TestTrue(TEXT("alone, it strikes the same enemy"), HitsOn(H, SerialOf(H, Alone), Quick) > 0);
	}
	{
		// Fallbacks: Melee fires faster, a Mine strikes again (no weapon resolves Beam first: Projectile comes first on the Rail).
		FDTKeeperHarness H;
		H.Patron(EDTPath::Storm, 0, false);
		H.Weapons.Give(H.Run, 0, ArcBlade, 1);
		const float Melee = H.Weapons.ComposeSlot(H.Run, 0, 0).Cooldown;
		H.Bless(Blessing(TEXT("Quicksilver.Rebound"), TEXT("Key")));
		TestEqual(TEXT("a sweep comes faster"), H.Weapons.ComposeSlot(H.Run, 0, 0).Cooldown, Melee / (1.0f + DTConst::KEEPER_RICOCHET_FALLBACK_RATE), 0.001f);
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Storm, 0, false);
		H.Weapons.Give(H.Run, 0, GravityMine, 1);
		H.Bless(Blessing(TEXT("Quicksilver.Rebound"), TEXT("Key")));
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.AddEnemy(FVector2f(300.0f, 450.0f));
		H.StepMany(5 * Second);
		TestTrue(TEXT("a mine hops to the next enemy"), H.CountHits(Quick) > 0);
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2ReboundAmp1, "Card.Quicksilver.Rebound.Amp1") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Rebound"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Quicksilver.Rebound"), TEXT("Amp1")));
		}
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.AddEnemy(FVector2f(300.0f, 200.0f));
		H.StepMany(2 * Second);
		return H.CountHits(Src(EDTPath::Quicksilver));
	};
	TestTrue(TEXT("one more bounce"), Scene(true) > Scene(false));
	return true;
}

DT_S2_TEST(FDTKeeperS2ReboundAmp2, "Card.Quicksilver.Rebound.Amp2") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Rebound"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Quicksilver.Rebound"), TEXT("Amp2")));
		}
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.AddEnemy(FVector2f(300.0f, 200.0f));
		H.StepMany(2 * Second);
		return H.Damage(Src(EDTPath::Quicksilver));
	};
	TestTrue(TEXT("bounces hit harder"), Scene(true) > Scene(false));
	return true;
}

DT_S2_TEST(FDTKeeperS2ReboundTrait, "Card.Quicksilver.Rebound.Trait") // T-KEE-34
{
	using namespace DTKeeperS2Test;
	const auto Scene = [](bool bCard)
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Storm, TEXT("Quicksilver.Rebound"), { TEXT("Key") });
		if (bCard)
		{
			H.Bless(Blessing(TEXT("Quicksilver.Rebound"), TEXT("Trait")));
		}
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.AddEnemy(FVector2f(300.0f, 150.0f), 1.0f);
		H.AddEnemy(FVector2f(300.0f, 300.0f));
		H.StepMany(3 * Second);
		return H.CountHits(Src(EDTPath::Quicksilver));
	};
	TestTrue(TEXT("a bounce's kill fires one more round"), Scene(true) > Scene(false));
	return true;
}

// ---- casts (T-KEE-35) ------------------------------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2CastClosing, "Cast.Shade") // T-KEE-35
{
	using namespace DTKeeperS2Test;
	const uint16 Shade = Src(EDTPath::Shade);
	FDTKeeperHarness H;
	H.Patron(EDTPath::Shade);
	const int32 Weak = AddScaled(H, FVector2f(1200.0f, 900.0f));
	H.Enemies.SetHp(Weak, H.Enemies.GetMaxHp()[Weak] * 0.2f);
	const int32 Strong = AddScaled(H, FVector2f(-1200.0f, 900.0f));
	const uint32 WeakSerial = SerialOf(H, Weak);
	const uint32 StrongSerial = SerialOf(H, Strong);
	H.StepOnce();
	Cast(H);
	TestTrue(TEXT("a cast is running"), H.Keepers.IsCasting(0));
	TestTrue(TEXT("every enemy on screen is Marked"), H.Weapons.IsMarked(0, WeakSerial) && H.Weapons.IsMarked(0, StrongSerial));
	TestEqual(TEXT("rounds Seek hard"), H.Weapons.ComposeSlot(H.Run, 0, 0).HomingTurnDegS, static_cast<float>(DTConst::INTERCESSION_SHADE_TURN_DEG_S));
	H.Weapons.SetAimManual(0, true);
	H.P().Weapons[0].Cooldown = 1000.0f;
	H.StepMany(FMath::RoundToInt(CastSteps(DTConst::INTERCESSION_SHADE_DURATION_S)));
	TestFalse(TEXT("the cast ends"), H.Keepers.IsCasting(0));
	TestFalse(TEXT("the weak are executed"), IsAlive(H, WeakSerial));
	TestTrue(TEXT("as Keeper.Shade"), HitsOn(H, WeakSerial, Shade) > 0);
	TestTrue(TEXT("the strong stand"), IsAlive(H, StrongSerial));
	return true;
}

DT_S2_TEST(FDTKeeperS2CastShadeVariants, "Cast.Shade.Variants") // T-KEE-35
{
	using namespace DTKeeperS2Test;
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Shade, TEXT("Shade.Hound"), { TEXT("Key") });
		const int32 Pierce = H.Weapons.ComposeSlot(H.Run, 0, 0).Pierce;
		Cast(H);
		const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, 0);
		TestEqual(TEXT("Hound turns at will"), Spec.HomingTurnDegS, 720.0f);
		TestEqual(TEXT("and pierces two more"), Spec.Pierce, Pierce + 2);
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Shade, TEXT("Shade.Hunters"), { TEXT("Key") });
		Cast(H);
		TestEqual(TEXT("Collectors raise six"), H.Weapons.GetLiveHunters(0), 6);
		H.StepMany(FMath::RoundToInt(CastSteps(8.0f)));
		TestEqual(TEXT("with double life"), H.Weapons.GetLiveHunters(0), 6);
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Shade, TEXT("Shade.Economy"), { TEXT("Key") });
		Cast(H, 1);
		TestTrue(TEXT("Ledger pays out now"), H.Weapons.IsPaydayReady(0));
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2CastSunLance, "Cast.Radiance") // T-KEE-35
{
	using namespace DTKeeperS2Test;
	const uint16 Radiance = Src(EDTPath::Radiance);
	const auto Scene = [this, Radiance](const TCHAR* Arch, int32& OutLine, int32& OutSide)
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Radiance, 0, false);
		H.Weapons.Give(H.Run, 0, PulseRifle, 1);
		H.P().Weapons[0].Cooldown = 1000.0f;
		if (Arch)
		{
			H.Bless(Blessing(Arch, TEXT("Key")));
		}
		const int32 Line = H.AddEnemy(FVector2f(800.0f, 0.0f));
		const int32 Side = H.AddEnemy(FVector2f(690.0f, 400.0f)); // 30 degrees off the beam
		H.StepOnce();
		Cast(H);
		TestEqual(TEXT("the charge lands nothing"), HitsOn(H, SerialOf(H, Line), Radiance), 0);
		H.StepMany(FMath::RoundToInt(CastSteps(DTConst::INTERCESSION_RADIANCE_CHARGE_S + 3.0f * DTConst::INTERCESSION_RADIANCE_BEAM_S)));
		OutLine = HitsOn(H, SerialOf(H, Line), Radiance);
		OutSide = HitsOn(H, SerialOf(H, Side), Radiance);
		bool bAllCrit = true;
		for (const FDTHit& Hit : H.HitLog)
		{
			bAllCrit &= Hit.SourceId != Radiance || Hit.bCrit;
		}
		TestTrue(TEXT("a beam always crits"), bAllCrit);
	};
	int32 BaseLine = 0;
	int32 BaseSide = 0;
	Scene(nullptr, BaseLine, BaseSide);
	const int32 Ticks = FMath::RoundToInt(DTConst::INTERCESSION_RADIANCE_BEAM_S / DTConst::INTERCESSION_RADIANCE_BEAM_TICK_S);
	TestTrue(TEXT("the beam ticks through its length"), FMath::Abs(BaseLine - Ticks) <= 1);
	TestEqual(TEXT("and stays on its line"), BaseSide, 0);
	int32 LanceLine = 0;
	int32 LanceSide = 0;
	Scene(TEXT("Radiance.Lance"), LanceLine, LanceSide);
	TestTrue(TEXT("Lance runs three beams"), LanceLine + LanceSide >= 3 * Ticks - 3);
	int32 PrismLine = 0;
	int32 PrismSide = 0;
	Scene(TEXT("Radiance.Prism"), PrismLine, PrismSide);
	TestTrue(TEXT("Prism's fan reaches off the line"), PrismSide > 0);
	return true;
}

DT_S2_TEST(FDTKeeperS2CastSunLanceSolar, "Cast.Radiance.Solar") // T-KEE-35
{
	using namespace DTKeeperS2Test;
	const uint16 Radiance = Src(EDTPath::Radiance);
	const auto Scene = [Radiance](bool bSolar)
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Radiance, 0, false);
		H.Weapons.Give(H.Run, 0, PulseRifle, 1);
		H.P().Weapons[0].Cooldown = 1000.0f;
		if (bSolar)
		{
			H.Bless(Blessing(TEXT("Radiance.Solar"), TEXT("Key")));
		}
		const int32 Near = H.AddEnemy(FVector2f(800.0f, 180.0f));
		H.StepOnce();
		Cast(H);
		H.StepMany(FMath::RoundToInt(CastSteps(DTConst::INTERCESSION_RADIANCE_CHARGE_S + DTConst::INTERCESSION_RADIANCE_BEAM_S)));
		return HitsOn(H, SerialOf(H, Near), Radiance);
	};
	TestEqual(TEXT("off the beam nothing lands"), Scene(false), 0);
	TestTrue(TEXT("Solar novas along it"), Scene(true) > 0);
	return true;
}

DT_S2_TEST(FDTKeeperS2CastQuicksilver, "Cast.Quicksilver") // T-KEE-35
{
	using namespace DTKeeperS2Test;
	const uint16 Quick = Src(EDTPath::Quicksilver);
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Quicksilver);
		const int32 Enemy = H.AddEnemy(FVector2f(0.0f, 500.0f));
		H.StepOnce();
		H.P().Weapons[0].Cooldown = 1000.0f;
		Dash(H, FVector2f(0.0f, 10.0f));
		H.StepMany(Second);
		TestEqual(TEXT("before the cast a dash fires nothing"), HitsOn(H, SerialOf(H, Enemy), Quick), 0);
		Cast(H);
		Dash(H, FVector2f(0.0f, 10.0f));
		H.StepMany(Second);
		TestTrue(TEXT("during it a dash fires"), HitsOn(H, SerialOf(H, Enemy), Quick) > 0);
		H.StepMany(FMath::RoundToInt(CastSteps(DTConst::INTERCESSION_QUICKSILVER_DURATION_S)));
		TestFalse(TEXT("the cast ends"), H.Keepers.IsCasting(0));
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Quicksilver, TEXT("Quicksilver.Vector"), { TEXT("Key") });
		Cast(H);
		TestEqual(TEXT("Vector's dash cooldown"), H.Keepers.GetHeroRiders(0).DashCooldownMul, 0.2f / static_cast<float>(DTConst::DASH_COOLDOWN_S), 0.001f);
		H.StepMany(FMath::RoundToInt(CastSteps(DTConst::INTERCESSION_QUICKSILVER_DURATION_S)));
		TestEqual(TEXT("it ends with the cast"), H.Keepers.GetHeroRiders(0).DashCooldownMul, 1.0f);
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Quicksilver, TEXT("Quicksilver.Momentum"), { TEXT("Key") });
		Cast(H);
		H.StepMany(Second);
		TestEqual(TEXT("Momentum jumps to the cap and holds"), H.Weapons.GetMomentum(0), 30.0f);
	}
	{
		FDTKeeperHarness H;
		Own(H, EDTPath::Quicksilver, TEXT("Quicksilver.Rebound"), { TEXT("Key") });
		Cast(H);
		const FDTFlagEntry* Entry = EntryOf(H, EDTPath::Quicksilver, EFlag::Ricochet);
		TestTrue(TEXT("Rebound: three bounces"), Entry && Entry->Params.Get(DTFlagParam::Bounces) == 3.0f);
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2CastLayers, "Cast.Layers") // T-KEE-35
{
	using namespace DTKeeperS2Test;
	struct FLayer { EDTPath Path; const TCHAR* Arch; EFlag Flag; };
	const FLayer Layers[] = {
		{ EDTPath::Shade, TEXT("Shade.Hound"), EFlag::Seek }, { EDTPath::Shade, TEXT("Shade.Hunters"), EFlag::Hunter }, { EDTPath::Shade, TEXT("Shade.Economy"), EFlag::Debt },
		{ EDTPath::Radiance, TEXT("Radiance.Lance"), EFlag::Charge }, { EDTPath::Radiance, TEXT("Radiance.Prism"), EFlag::CritSplit }, { EDTPath::Radiance, TEXT("Radiance.Solar"), EFlag::Nova },
		{ EDTPath::Quicksilver, TEXT("Quicksilver.Vector"), EFlag::VolleyOnDash }, { EDTPath::Quicksilver, TEXT("Quicksilver.Momentum"), EFlag::SpeedScale }, { EDTPath::Quicksilver, TEXT("Quicksilver.Rebound"), EFlag::Ricochet } };
	for (const FLayer& Layer : Layers)
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Forge);
		H.Commit(Layer.Path);
		H.Bless(Blessing(Layer.Arch, TEXT("Key")));
		const FString Name(Layer.Arch);
		Cast(H);
		const FDTIntercessionPlan& Plan = H.Keepers.GetActivePlan(0);
		TestEqual(*(Name + TEXT(": the second's layer")), Plan.LayerPath, Layer.Path);
		TestTrue(*(Name + TEXT(": its row")), Plan.Layer == Row(Layer.Arch));
		TestTrue(*(Name + TEXT(": the flag runs for the cast")), H.Keepers.GetBuiltFlags(0, Layer.Path).Has(Layer.Flag));
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Forge);
		H.Commit(EDTPath::Shade);
		H.Bless(Blessing(TEXT("Shade.Hunters"), TEXT("Key")));
		Cast(H);
		TestEqual(TEXT("Collectors as a layer raise two"), H.Weapons.GetLiveHunters(0), 2);
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Forge);
		H.Commit(EDTPath::Shade);
		H.Bless(Blessing(TEXT("Shade.Economy"), TEXT("Key")));
		Cast(H);
		TestTrue(TEXT("Ledger as a layer fills the Debt"), H.Weapons.IsPaydayReady(0));
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Forge);
		H.Commit(EDTPath::Quicksilver);
		H.Bless(Blessing(TEXT("Quicksilver.Momentum"), TEXT("Key")));
		Cast(H);
		TestTrue(TEXT("Momentum as a layer grants stacks"), H.Weapons.GetMomentum(0) >= 14.0f);
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Forge);
		H.Commit(EDTPath::Shade);
		H.Bless(Blessing(TEXT("Shade.Hound"), TEXT("Key")));
		const int32 Enemy = H.AddEnemy(FVector2f(100.0f, 0.0f));
		Cast(H, 4);
		TestTrue(TEXT("Hound as a layer Marks what the cast hits"), H.Weapons.IsMarked(0, SerialOf(H, Enemy)));
	}
	return true;
}

// ---- cues, tiers, text (T-KEE-36) ----------------------------------------------------------------------------------------------

DT_S2_TEST(FDTKeeperS2Cue, "Cue") // T-KEE-36
{
	using namespace DTKeeperS2Test;
	FDTKeeperHarness H;
	H.Patron(EDTPath::Shade);
	const int32 Enemy = H.AddEnemy(FVector2f(3000.0f, 0.0f));
	H.StepOnce();
	H.Weapons.ApplyMark(0, SerialOf(H, Enemy), H.Step);
	TArray<FDTWeaponVisual> Out;
	H.Weapons.GatherVisuals(H.Run, H.View, Out);
	TestTrue(TEXT("a Marked enemy shows its cue"), Out.ContainsByPredicate([](const FDTWeaponVisual& V) { return V.Kind == FDTWeaponVisual::EKind::Marked; }));
	return true;
}

DT_S2_TEST(FDTKeeperS2TierLines, "TierLines") // T-KEE-36
{
	using namespace DTKeeperS2Test;
	for (int32 R = DTCat::NumArchetypes; R < DTCat::NumAllArchetypes; ++R)
	{
		const DTCat::FArchetypeRow& Arch = DTCat::Archetypes[R];
		const int32 P = static_cast<int32>(Arch.Path);
		FDTKeeperRunState Keepers;
		Keepers.Owned.Add(FName(Arch.BlessingIds[0]));
		Keepers.Leading[P] = FName(Arch.Id);
		for (int32 Tier = 1; Tier <= 3; ++Tier)
		{
			Keepers.Tier[P] = Tier - 1;
			const FDTWeaponFlags Below = DTKeepers::BuildKeeperFlags(Keepers, Arch.Path);
			Keepers.Tier[P] = Tier;
			const FDTWeaponFlags At = DTKeepers::BuildKeeperFlags(Keepers, Arch.Path);
			bool bChanged = At.Entries.Num() != Below.Entries.Num();
			for (int32 E = 0; !bChanged && E < At.Entries.Num(); ++E)
			{
				bChanged = At.Entries[E].Params.Nums.Num() != Below.Entries[E].Params.Nums.Num();
				for (int32 N = 0; !bChanged && N < At.Entries[E].Params.Nums.Num(); ++N)
				{
					bChanged = At.Entries[E].Params.Nums[N].Value != Below.Entries[E].Params.Nums[N].Value;
				}
			}
			TestTrue(FString::Printf(TEXT("%s Tier %d changes a flag"), Arch.Id, Tier), bChanged);
		}
	}
	return true;
}

DT_S2_TEST(FDTKeeperS2Text, "Text") // T-KEE-36
{
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("DT/Text/ST_Keepers.csv");
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		AddError(TEXT("no ST_Keepers.csv"));
		return false;
	}
	const auto ParseLine = [](const FString& Line)
	{
		TArray<FString> Fields;
		FString Field;
		bool bQuoted = false;
		for (int32 I = 0; I < Line.Len(); ++I)
		{
			const TCHAR C = Line[I];
			if (bQuoted && C == TEXT('"') && I + 1 < Line.Len() && Line[I + 1] == TEXT('"'))
			{
				Field.AppendChar(C);
				++I;
			}
			else if (C == TEXT('"'))
			{
				bQuoted = !bQuoted;
			}
			else if (C == TEXT(',') && !bQuoted)
			{
				Fields.Add(Field);
				Field.Reset();
			}
			else
			{
				Field.AppendChar(C);
			}
		}
		Fields.Add(Field);
		return Fields;
	};
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	TMap<FString, TPair<FString, FString>> ByKey;
	for (const FString& Line : Lines)
	{
		const TArray<FString> F = ParseLine(Line);
		if (F.Num() >= 3)
		{
			ByKey.Add(F[0], TPair<FString, FString>(F[1], F[2]));
		}
	}
	static const TCHAR* const Columns[] = { TEXT("Key"), TEXT("Amp1"), TEXT("Amp2"), TEXT("Trait") };
	const auto Args = [](const FString& S)
	{
		TArray<FString> Out;
		int32 Open = INDEX_NONE;
		int32 From = 0;
		while ((Open = S.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, From)) != INDEX_NONE)
		{
			const int32 Close = S.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open);
			Out.Add(S.Mid(Open + 1, Close - Open - 1));
			From = Close + 1;
		}
		return Out;
	};
	for (int32 R = DTCat::NumArchetypes; R < DTCat::NumAllArchetypes; ++R)
	{
		const DTCat::FArchetypeRow& Row = DTCat::Archetypes[R];
		const FString Prefix = FString(Row.TextPrefix).Mid(FString(TEXT("ST_Keepers.")).Len());
		TestTrue(*(Prefix + TEXT(".Name")), ByKey.Contains(Prefix + TEXT(".Name")));
		const auto CheckArgs = [this, &Args](const FString& Key, const FString& En, const TCHAR* Spec)
		{
			TSet<FString> Known;
			for (const TPair<FString, double>& Param : DTOffers::SpecParams(Spec))
			{
				Known.Add(Param.Key);
				Known.Add(Param.Key + TEXT("_pct"));
				Known.Add(Param.Key + TEXT("_m"));
			}
			for (const FString& Arg : Args(En))
			{
				TestTrue(FString::Printf(TEXT("%s names {%s} of its spec"), *Key, *Arg), Known.Contains(Arg));
			}
		};
		for (int32 Column = 0; Column < 4; ++Column)
		{
			const FString Base = Prefix + TEXT(".") + Columns[Column];
			const TPair<FString, FString>* Name = ByKey.Find(Base + TEXT(".Name"));
			const TPair<FString, FString>* Desc = ByKey.Find(Base + TEXT(".Desc"));
			if (TestNotNull(*(Base + TEXT(".Name")), Name) && TestNotNull(*(Base + TEXT(".Desc")), Desc))
			{
				TestTrue(*(Base + TEXT(" name <= 18 / 6")), Name->Key.Len() <= 18 && Name->Value.Len() <= 6);
				TestTrue(*(Base + TEXT(" rules <= 100 en")), Desc->Key.Len() <= 100);
				CheckArgs(Base, Desc->Key, Row.Effects[Column]);
			}
		}
		for (int32 Tier = 1; Tier <= 3; ++Tier)
		{
			const FString Key = FString::Printf(TEXT("%s.Tier%d"), *Prefix, Tier);
			const TPair<FString, FString>* Line = ByKey.Find(Key);
			if (TestNotNull(*Key, Line))
			{
				TArray<FString> Words;
				Line->Key.ParseIntoArrayWS(Words);
				TestTrue(*(Key + TEXT(" within the tier limit")), Words.Num() <= DTConst::TIER_LINE_WORDS_EN_MAX);
				CheckArgs(Key, Line->Key, Row.Effects[3 + Tier]);
			}
		}
		const TPair<FString, FString>* Ult = ByKey.Find(Prefix + TEXT(".Ult.Desc"));
		const TPair<FString, FString>* Layer = ByKey.Find(Prefix + TEXT(".Layer.Desc"));
		if (TestNotNull(*(Prefix + TEXT(".Ult.Desc")), Ult) && TestNotNull(*(Prefix + TEXT(".Layer.Desc")), Layer))
		{
			CheckArgs(Prefix + TEXT(".Ult"), Ult->Key, Row.IntercessionVariant);
			CheckArgs(Prefix + TEXT(".Layer"), Layer->Key, Row.SecondKeeperLayer);
		}
	}
	return true;
}

#undef DT_S2_TEST

#endif // WITH_DEV_AUTOMATION_TESTS
