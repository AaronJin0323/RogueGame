// Offer system tests (02_Systems/offers.md T-OFF-01..15). The generator is pure, so most tests need no world; the flow
// tests (queue order, tools through the subsystem) use a headless game world without ticking it.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Offers; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Combat/DTWeaponCatalog.h"
#include "Combat/DTWeaponSim.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferScreen.h"
#include "Offers/DTOfferStyle.h"
#include "Offers/DTOfferSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTOfferTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	const FName PulseRifle(TEXT("Weapon.PulseRifle"));
	const FName RingEmitter(TEXT("Weapon.RingEmitter"));
	const FName OrbitalDrones(TEXT("Weapon.OrbitalDrones"));
	const FName ArcBlade(TEXT("Weapon.ArcBlade"));
	const FName GravityMine(TEXT("Weapon.GravityMine"));

	/** A headless run: one run state, a weapon sim for the weapon part of picks, the generator for hands. */
	struct FSimRun
	{
		FDTRunState Run;
		FDTWeaponSim Weapons;
		FDTOfferPool Pool;

		explicit FSimRun(int32 Seed, int32 NumPlayers = 1) { Run.Start(Seed, NumPlayers); }
		FDTPlayerRunState& P(int32 Owner = 0) { return Run.GetPlayer(Owner); }

		FDTOfferHand Hand(EDTHandKind Kind, int32 Level, int32 Owner = 0, TArray<int32>* Dropped = nullptr)
		{
			FDTPlayerRunState& Player = P(Owner);
			const EDTHandKind Resolved = DTOffers::ResolveKind(Player, Kind, Level);
			TArray<FDTOfferCard> Carried;
			if (Resolved == EDTHandKind::Level && Player.Offer.Locked.IsSet())
			{
				Carried.Add(Player.Offer.Locked.GetValue());
				Player.Offer.Locked.Reset();
			}
			return DTOffers::BuildHand(Player, Pool, Resolved, Level, Player.Offer.Rng, Carried, Dropped);
		}

		FDTPickOutcome Pick(const FDTOfferHand& Hand, int32 Slot, int32 Owner = 0)
		{
			const FDTPickOutcome Outcome = DTOffers::ApplyPickState(P(Owner), Hand.HandKind, Hand.Cards[Slot]);
			DTOffers::ApplyToSim(Weapons, Run, Owner, Outcome);
			return Outcome;
		}

		void ChoosePatron(EDTPath Path, int32 Owner = 0)
		{
			const FDTOfferHand Patron = Hand(EDTHandKind::Patron, 1, Owner);
			for (int32 Slot = 0; Slot < Patron.Cards.Num(); ++Slot)
			{
				if (DTOffers::ParseKeeper(Patron.Cards[Slot].ContentId) == Path)
				{
					Pick(Patron, Slot, Owner);
				}
			}
		}
	};

	/** Steered policy: a blessing of an owned archetype, else any blessing, else an upgrade, else slot 0. */
	int32 SteeredSlot(const FDTPlayerRunState& Player, const FDTOfferHand& Hand)
	{
		int32 Best = 0;
		int32 BestScore = -1;
		for (int32 Slot = 0; Slot < Hand.Cards.Num(); ++Slot)
		{
			const FDTOfferCard& Card = Hand.Cards[Slot];
			int32 Score = 0;
			if (Card.Kind == EDTCardKind::Blessing)
			{
				const FDTBlessingInfo Info = DTOffers::FindBlessing(Card.ContentId);
				Score = 3 + (DTOffers::OwnedInArchetype(Player.Keepers, Info.Archetype) > 0 ? 2 : 0);
			}
			else if (Card.Kind == EDTCardKind::WeaponUpgrade || Card.Kind == EDTCardKind::Weapon)
			{
				Score = 2;
			}
			else if (Card.Kind == EDTCardKind::Mod || Card.Kind == EDTCardKind::Keeper)
			{
				Score = 1;
			}
			if (Score > BestScore)
			{
				Best = Slot;
				BestScore = Score;
			}
		}
		return Best;
	}

	FString Identity(const FDTOfferCard& Card)
	{
		return FString::Printf(TEXT("%d:%s:%s"), static_cast<int32>(Card.Kind), *Card.ContentId.ToString(), *Card.TargetWeapon.ToString());
	}

	FString HandString(const FDTOfferHand& Hand)
	{
		FString Out;
		for (const FDTOfferCard& Card : Hand.Cards)
		{
			Out += Identity(Card) + FString::Printf(TEXT("/%d|"), static_cast<int32>(Card.Rarity));
		}
		return Out;
	}

	/** Plays a run of level-up hands L2..MaxLevel (with chests every ChestEvery levels); Visit sees every hand before its pick. */
	template <typename VisitT>
	void PlayRun(FSimRun& Sim, int32 MaxLevel, int32 ChestEvery, bool bRandomPicks, VisitT&& Visit)
	{
		FRandomStream Choice(Sim.Run.GetPlayer(0).Offer.Rng.GetInitialSeed() ^ 0x5eed);
		for (int32 Level = 2; Level <= MaxLevel; ++Level)
		{
			Sim.P().Level = Level;
			const int32 Count = (ChestEvery > 0 && Level % ChestEvery == 0) ? 2 : 1;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const EDTHandKind Kind = Index == 0 ? EDTHandKind::Level : EDTHandKind::Chest;
				const FDTOfferHand Hand = Sim.Hand(Kind, Level);
				Visit(Hand, Sim);
				if (Hand.Cards.Num() == 0)
				{
					continue;
				}
				const int32 Slot = bRandomPicks ? Choice.RandRange(0, Hand.Cards.Num() - 1) : SteeredSlot(Sim.P(), Hand);
				// Now and then hold or banish a card, as a player would.
				if (bRandomPicks && DTOffers::HandAllowsTools(Hand.HandKind) && Choice.FRand() < 0.15f)
				{
					const int32 Other = (Slot + 1) % Hand.Cards.Num();
					FDTPlayerOfferState& Tools = Sim.P().Offer;
					if (DTOffers::CardAllowsBanishLock(Hand.Cards[Other]))
					{
						if (Choice.FRand() < 0.5f && Tools.BanishLeft > 0)
						{
							--Tools.BanishLeft;
							Tools.Banished.Add(Hand.Cards[Other].ContentId);
						}
						else if (Tools.LockLeft > 0 && !Tools.Locked.IsSet())
						{
							--Tools.LockLeft;
							FDTOfferCard Held = Hand.Cards[Other];
							Held.bLocked = true;
							Tools.Locked = Held;
						}
					}
				}
				Sim.Pick(Hand, Slot);
			}
		}
	}

	bool IsAllowedBlessingPath(const FDTPlayerRunState& Player, EDTPath Path)
	{
		return Path == Player.Offer.PatronPath || (DTOffers::IsCommitted(Player) && Path == Player.Offer.SecondPath);
	}
}

// T-OFF-01
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferHandShapeTest, "Dusktracer.Offers.HandShape", DTOfferTest::Flags)
bool FDTOfferHandShapeTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	int32 Hands = 0;
	int32 Bad = 0;
	for (int32 Seed = 1; Seed <= 120; ++Seed)
	{
		FSimRun Sim(Seed);
		Sim.ChoosePatron(static_cast<EDTPath>(Seed % 3));
		PlayRun(Sim, 24, 4, true, [&](const FDTOfferHand& Hand, FSimRun&)
		{
			TSet<FString> Ids;
			for (const FDTOfferCard& Card : Hand.Cards)
			{
				Ids.Add(Identity(Card));
			}
			const bool bDistinct = Ids.Num() == Hand.Cards.Num();
			const bool bSize = Hand.Cards.Num() == DTOffers::HandSize(Hand.HandKind)
				|| (Hand.HandKind == EDTHandKind::Keeper && Hand.Cards.Num() == FMath::Min(DTConst::OFFER_KEEPER_CHOICES, DTOffers::RunKeepers().Num() - 1));
			Bad += (bDistinct && bSize) ? 0 : 1;
			Hands += Hand.HandKind == EDTHandKind::Level ? 1 : 0;
			if (Hand.HandKind == EDTHandKind::Level && Hand.Cards.Num() != DTConst::OFFER_CARDS)
			{
				AddError(FString::Printf(TEXT("seed %d L%d: %d cards"), Seed, Hand.Level, Hand.Cards.Num()));
			}
		});
	}
	TestTrue(TEXT("many level-up hands checked"), Hands > 1000);
	TestEqual(TEXT("every hand has its card count and distinct identities"), Bad, 0);
	return true;
}

// T-OFF-02
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferWeightsTest, "Dusktracer.Offers.Weights", DTOfferTest::Flags)
bool FDTOfferWeightsTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	auto Card = [](EDTCardKind Kind, const TCHAR* Id, FName Target = NAME_None)
	{
		FDTOfferCard C;
		C.Kind = Kind;
		C.ContentId = FName(Id);
		C.TargetWeapon = Target;
		return C;
	};
	const float Base = DTConst::OFFER_WEIGHT_BASE;
	FSimRun Sim(3);
	Sim.ChoosePatron(EDTPath::Storm);
	FDTPlayerRunState& P = Sim.P();
	Sim.Weapons.Give(Sim.Run, 0, PulseRifle, 3);

	TestEqual(TEXT("base: a new weapon with nothing owned"), DTOffers::Weight(P, Card(EDTCardKind::Weapon, TEXT("Weapon.RingEmitter"))), Base);
	TestEqual(TEXT("owned: an upgrade x (1 + k x level)"), DTOffers::Weight(P, Card(EDTCardKind::WeaponUpgrade, TEXT("Weapon.PulseRifle"), PulseRifle)),
		Base * (1.0f + DTConst::OFFER_WEIGHT_WEAPON_LEVEL * 3.0f));
	TestEqual(TEXT("synergy: a modifier that amplifies nothing owned"), DTOffers::Weight(P, Card(EDTCardKind::Mod, TEXT("Mod.SplitCore"), PulseRifle)),
		Base * (1.0f + DTConst::OFFER_WEIGHT_WEAPON_LEVEL * 3.0f) * DTConst::OFFER_WEIGHT_MOD_NO_AMPLIFY);

	DTOffers::AddBlessing(P.Keepers, FName(TEXT("Blessing.Storm.Tempest.Key")));
	const float Favor1 = 1.0f + DTConst::OFFER_WEIGHT_FAVOR * DTConst::TIER_FAVOR_PER_BLESSING;
	TestEqual(TEXT("favor x momentum: an amplifier of the owned archetype"), DTOffers::Weight(P, Card(EDTCardKind::Blessing, TEXT("Blessing.Storm.Tempest.Amp1"))),
		Base * Favor1 * (1.0f + DTConst::OFFER_WEIGHT_MOMENTUM * 1.0f));
	TestEqual(TEXT("favor only: another archetype's key"), DTOffers::Weight(P, Card(EDTCardKind::Blessing, TEXT("Blessing.Storm.ArcGrid.Key"))), Base * Favor1);
	TestEqual(TEXT("synergy: a modifier that amplifies an owned archetype"), DTOffers::Weight(P, Card(EDTCardKind::Mod, TEXT("Mod.SplitCore"), PulseRifle)),
		Base * (1.0f + DTConst::OFFER_WEIGHT_WEAPON_LEVEL * 3.0f) * DTConst::OFFER_WEIGHT_MOD_AMPLIFIES);
	TestEqual(TEXT("fit: a new weapon that fits an owned archetype"), DTOffers::Weight(P, Card(EDTCardKind::Weapon, TEXT("Weapon.RingEmitter"))), Base * DTConst::OFFER_WEIGHT_WEAPON_FIT);
	TestEqual(TEXT("fit: an off-fit new weapon is unchanged"), DTOffers::Weight(P, Card(EDTCardKind::Weapon, TEXT("Weapon.ArcBlade"))), Base);

	for (int32 Index = 1; Index < DTConst::OFFER_MOMENTUM_DAMP_AFTER; ++Index)
	{
		DTOffers::AddBlessing(P.Keepers, FName(DTCat::Archetypes[0].BlessingIds[Index]));
	}
	const float FavorN = 1.0f + DTConst::OFFER_WEIGHT_FAVOR * DTConst::TIER_FAVOR_PER_BLESSING * DTConst::OFFER_MOMENTUM_DAMP_AFTER;
	TestEqual(TEXT("damp: the same Keeper's other archetypes once one holds the damp count"),
		DTOffers::Weight(P, Card(EDTCardKind::Blessing, TEXT("Blessing.Storm.ArcGrid.Key"))), Base * FavorN * DTConst::OFFER_WEIGHT_OTHER_ARCHETYPE);
	P.Offer.SecondPath = EDTPath::Forge;
	TestEqual(TEXT("committed: the chosen Keepers' blessings after L8"), DTOffers::Weight(P, Card(EDTCardKind::Blessing, TEXT("Blessing.Forge.Hammer.Key"))),
		Base * DTConst::OFFER_WEIGHT_COMMITTED);
	P.Offer.SecondPath = EDTPath::None;

	// The empirical share of the first chest card (a plain weighted draw) matches the weights.
	const TArray<FDTOfferCard> Pool = DTOffers::Candidates(P, Sim.Pool);
	double Total = 0.0;
	for (const FDTOfferCard& C : Pool)
	{
		Total += C.Weight;
	}
	TMap<FString, int32> Counts;
	constexpr int32 Draws = 6000;
	FRandomStream Rng(77);
	for (int32 Draw = 0; Draw < Draws; ++Draw)
	{
		const FDTOfferHand Hand = DTOffers::BuildHand(P, Sim.Pool, EDTHandKind::Chest, P.Level, Rng);
		Counts.FindOrAdd(Identity(Hand.Cards[0]))++;
	}
	double WorstGap = 0.0;
	for (const FDTOfferCard& C : Pool)
	{
		const double Expected = C.Weight / Total;
		const double Seen = static_cast<double>(Counts.FindRef(Identity(C))) / Draws;
		WorstGap = FMath::Max(WorstGap, FMath::Abs(Expected - Seen));
	}
	TestTrue(FString::Printf(TEXT("pick share follows the weights (worst gap %.3f)"), WorstGap), WorstGap < 0.025);
	return true;
}

// T-OFF-03
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferLegalityTest, "Dusktracer.Offers.Legality", DTOfferTest::Flags)
bool FDTOfferLegalityTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	int32 Cards = 0;
	int32 SecondBlessings = 0;
	TMap<FString, int32> Violations;
	for (int32 Seed = 1; Seed <= 150; ++Seed)
	{
		FSimRun Sim(Seed);
		if (Seed % 4 == 0)
		{
			// DT.Unlock content widens the pool; every rule still holds.
			Sim.Pool.Unlocked = { GravityMine, FName(TEXT("Mod.Fork")), FName(TEXT("Mod.Ricochet")) };
		}
		Sim.ChoosePatron(static_cast<EDTPath>((Seed / 3) % 3));
		PlayRun(Sim, 22, 3, true, [&](const FDTOfferHand& Hand, FSimRun& S)
		{
			const FDTPlayerRunState& P = S.P();
			const int32 PatronLeading = DTOffers::LeadingArchetype(P.Keepers, P.Offer.PatronPath);
			const uint32 Excluded = PatronLeading == INDEX_NONE ? 0u : DTCat::Archetypes[PatronLeading].IncompatFlags;
			for (const FDTOfferCard& Card : Hand.Cards)
			{
				++Cards;
				auto Fail = [&Violations](const TCHAR* Why) { Violations.FindOrAdd(Why)++; };
				if (P.Offer.Banished.Contains(Card.ContentId) && !Card.bLocked)
				{
					Fail(TEXT("banished id shown"));
				}
				if (Card.Kind == EDTCardKind::Weapon && P.Weapons.Num() >= DTConst::WEAPON_SLOTS)
				{
					Fail(TEXT("new weapon with full slots"));
				}
				if (Card.Kind == EDTCardKind::Mod)
				{
					const int32 Slot = FDTWeaponSim::FindSlot(S.Run, 0, Card.TargetWeapon);
					if (Slot == INDEX_NONE)
					{
						Fail(TEXT("modifier for a weapon not owned"));
						continue;
					}
					const FDTWeaponInstance& W = P.Weapons[Slot];
					if (W.Sockets.Num() >= DTConst::MOD_SOCKETS_PER_WEAPON)
					{
						Fail(TEXT("modifier for a weapon without a free socket"));
					}
					if (!DTModCompat::IsCompatible(Card.TargetWeapon, Card.ContentId))
					{
						Fail(TEXT("modifier the matrix forbids"));
					}
					if (W.Sockets.ContainsByPredicate([&Card](const FDTWeaponInstance::FSocket& Sk) { return Sk.ModId == Card.ContentId; }))
					{
						Fail(TEXT("modifier already carried"));
					}
				}
				if (Card.Kind == EDTCardKind::Blessing)
				{
					const FDTBlessingInfo Info = DTOffers::FindBlessing(Card.ContentId);
					const FName Key(DTCat::Archetypes[Info.Archetype].BlessingIds[0]);
					if (Info.Column > 0 && !P.Keepers.Owned.Contains(Key))
					{
						Fail(TEXT("amplifier or trait without its key"));
					}
					if (Info.Column == 3 && DTOffers::OwnedInArchetype(P.Keepers, Info.Archetype) < DTConst::KEEPER_TRAIT_PREREQ_CARDS)
					{
						Fail(TEXT("trait before its prerequisite cards"));
					}
					if (!DTOffers::IsCommitted(P) && Info.Path != P.Offer.PatronPath)
					{
						Fail(TEXT("non-Patron blessing before the L8 hand"));
					}
					if (!IsAllowedBlessingPath(P, Info.Path))
					{
						Fail(TEXT("third-Keeper blessing"));
					}
					if (DTOffers::IsCommitted(P) && Info.Path == P.Offer.SecondPath)
					{
						++SecondBlessings;
						if ((DTOffers::SpecFlags(DTCat::Archetypes[Info.Archetype].Effects[Info.Column]) & Excluded) != 0)
						{
							Fail(TEXT("second-Keeper blessing excluded by the Patron's IncompatFlags"));
						}
					}
					if (P.Keepers.Owned.Contains(Card.ContentId))
					{
						Fail(TEXT("owned blessing offered again"));
					}
				}
			}
		});
	}
	for (const TPair<FString, int32>& V : Violations)
	{
		AddError(FString::Printf(TEXT("%s: %d"), *V.Key, V.Value));
	}
	TestTrue(TEXT("many cards checked"), Cards > 10000);
	TestTrue(TEXT("second-Keeper blessings were offered"), SecondBlessings > 100);

	// (e) directly: Forge Patron leading Hammer (IncompatFlags Split) never sees Storm's Split blessings as second Keeper.
	FSimRun Sim(9);
	Sim.ChoosePatron(EDTPath::Forge);
	DTOffers::AddBlessing(Sim.P().Keepers, FName(TEXT("Blessing.Forge.Hammer.Key")));
	Sim.P().Offer.SecondPath = EDTPath::Storm;
	DTOffers::AddBlessing(Sim.P().Keepers, FName(TEXT("Blessing.Storm.ArcGrid.Key")));
	const TArray<FDTOfferCard> Pool = DTOffers::Candidates(Sim.P(), Sim.Pool);
	TestFalse(TEXT("Hammer excludes the Split key"), Pool.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Blessing.Storm.Tempest.Key")); }));
	TestTrue(TEXT("Hammer keeps the Chain amplifier"), Pool.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Blessing.Storm.ArcGrid.Amp1")); }));
	TestFalse(TEXT("Tide is a third Keeper here"), Pool.ContainsByPredicate([](const FDTOfferCard& C)
	{
		return C.Kind == EDTCardKind::Blessing && DTOffers::FindBlessing(C.ContentId).Path == EDTPath::Tide;
	}));
	return true;
}

// T-OFF-04
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferPerPlayerStreamTest, "Dusktracer.Offers.PerPlayerStream", DTOfferTest::Flags)
bool FDTOfferPerPlayerStreamTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	auto Play = [](int32 RerollsOfP0)
	{
		FSimRun Sim(4242, 2);
		Sim.ChoosePatron(EDTPath::Storm, 0);
		Sim.ChoosePatron(EDTPath::Tide, 1);
		TArray<FString> Out;
		for (int32 Level = 2; Level <= 12; ++Level)
		{
			for (int32 Owner = 0; Owner < 2; ++Owner)
			{
				Sim.P(Owner).Level = Level;
				FDTOfferHand Hand = Sim.Hand(EDTHandKind::Level, Level, Owner);
				for (int32 Reroll = 0; Owner == 0 && Reroll < RerollsOfP0; ++Reroll)
				{
					Hand = DTOffers::BuildHand(Sim.P(0), Sim.Pool, Hand.HandKind, Level, Sim.P(0).Offer.Rng);
				}
				Out.Add(FString::Printf(TEXT("p%d "), Owner) + HandString(Hand));
				Sim.Pick(Hand, SteeredSlot(Sim.P(Owner), Hand), Owner);
			}
		}
		return Out;
	};
	const TArray<FString> A = Play(0);
	const TArray<FString> B = Play(0);
	const TArray<FString> C = Play(2);
	TestTrue(TEXT("same seed and choices replay identical hands"), A == B);
	bool bP1Same = true;
	bool bP0Changed = false;
	for (int32 Index = 0; Index < A.Num(); ++Index)
	{
		if (A[Index].StartsWith(TEXT("p1")))
		{
			// Player 1's own picks follow player 1's own hands only; rerolls by player 0 never reach them.
			bP1Same &= A[Index] == C[Index];
		}
		else
		{
			bP0Changed |= A[Index] != C[Index];
		}
	}
	TestTrue(TEXT("rerolling for one player leaves the other's hands identical"), bP1Same);
	TestTrue(TEXT("the rerolling player's hands did change"), bP0Changed);
	return true;
}

// T-OFF-05
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferRarityTest, "Dusktracer.Offers.Rarity", DTOfferTest::Flags)
bool FDTOfferRarityTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	FRandomStream Rng(5);
	constexpr int32 Rolls = 100000;
	int32 Epic = 0;
	int32 Rare = 0;
	for (int32 Roll = 0; Roll < Rolls; ++Roll)
	{
		const EDTRarity R = DTOffers::RollRarity(Rng);
		Epic += R == EDTRarity::Epic ? 1 : 0;
		Rare += R == EDTRarity::Rare ? 1 : 0;
	}
	TestTrue(TEXT("Epic share"), FMath::Abs(static_cast<double>(Epic) / Rolls - DTConst::OFFER_RARITY_EPIC_P) < 0.005);
	TestTrue(TEXT("Rare share"), FMath::Abs(static_cast<double>(Rare) / Rolls - (1.0 - DTConst::OFFER_RARITY_EPIC_P) * DTConst::OFFER_RARITY_RARE_P) < 0.006);

	FSimRun Sim(5);
	Sim.Weapons.Give(Sim.Run, 0, PulseRifle, 1);
	FDTOfferCard Upgrade;
	Upgrade.Kind = EDTCardKind::WeaponUpgrade;
	Upgrade.ContentId = PulseRifle;
	Upgrade.TargetWeapon = PulseRifle;
	Upgrade.Rarity = EDTRarity::Rare;
	TestEqual(TEXT("Rare upgrade adds its levels"), DTOffers::ApplyPickState(Sim.P(), EDTHandKind::Level, Upgrade).TargetLevel, 1 + DTConst::OFFER_RARE_UPGRADE_LEVELS);
	Upgrade.Rarity = EDTRarity::Epic;
	TestEqual(TEXT("Epic upgrade adds its levels"), DTOffers::ApplyPickState(Sim.P(), EDTHandKind::Level, Upgrade).TargetLevel, 1 + DTConst::OFFER_EPIC_UPGRADE_LEVELS);
	Sim.Weapons.LevelUp(Sim.Run, 0, 0, DTWeapons::GetMaxLevel(DTWeapons::FindWeapon(PulseRifle)) - 1);
	TestEqual(TEXT("an upgrade is capped at the top level"), DTOffers::ApplyPickState(Sim.P(), EDTHandKind::Level, Upgrade).TargetLevel,
		DTWeapons::GetMaxLevel(DTWeapons::FindWeapon(PulseRifle)));
	const FDTPickOutcome Applied = DTOffers::ApplyPickState(Sim.P(), EDTHandKind::Level, Upgrade);
	DTOffers::ApplyToSim(Sim.Weapons, Sim.Run, 0, Applied);
	TestEqual(TEXT("the capped upgrade lands on the weapon"), Sim.P().Weapons[0].Level, DTWeapons::GetMaxLevel(DTWeapons::FindWeapon(PulseRifle)));

	// Only upgrades carry rarity in S1; other kinds show Common in level-up hands.
	bool bOnlyUpgradesRare = true;
	FSimRun Run2(6);
	Run2.ChoosePatron(EDTPath::Forge);
	PlayRun(Run2, 15, 0, true, [&](const FDTOfferHand& Hand, FSimRun&)
	{
		for (const FDTOfferCard& Card : Hand.Cards)
		{
			if (Hand.HandKind == EDTHandKind::Level && Card.Kind != EDTCardKind::WeaponUpgrade && Card.Rarity != EDTRarity::Common && !Card.bLocked)
			{
				bOnlyUpgradesRare = false;
			}
		}
	});
	TestTrue(TEXT("non-upgrade cards show Common in level-up hands"), bOnlyUpgradesRare);
	return true;
}

// T-OFF-06
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferGuaranteeTest, "Dusktracer.Offers.Guarantee", DTOfferTest::Flags)
bool FDTOfferGuaranteeTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	FSimRun Sim(12);
	Sim.ChoosePatron(EDTPath::Tide);
	Sim.Weapons.Give(Sim.Run, 0, PulseRifle, 1);
	int32 Blessings = 0;
	constexpr int32 Hands = 5000;
	bool bAllGuaranteed = true;
	for (int32 Index = 0; Index < Hands; ++Index)
	{
		const FDTOfferHand Hand = DTOffers::BuildHand(Sim.P(), Sim.Pool, EDTHandKind::Level, 3, Sim.P().Offer.Rng);
		const FDTOfferCard& First = Hand.Cards[0];
		bAllGuaranteed &= Hand.bGuaranteeFilled && First.bGuaranteed
			&& (First.Kind == EDTCardKind::Blessing || First.Kind == EDTCardKind::WeaponUpgrade);
		Blessings += First.Kind == EDTCardKind::Blessing ? 1 : 0;
	}
	TestTrue(TEXT("the first card is in the guaranteed set"), bAllGuaranteed);
	const double Share = static_cast<double>(Blessings) / Hands;
	TestTrue(FString::Printf(TEXT("pre-L8 blessing share %.3f matches"), Share), FMath::Abs(Share - DTConst::OFFER_GUARANTEE_BLESSING_P) < 0.025);

	// After L8: the committed floor and one owned-archetype card.
	int32 Checked = 0;
	for (int32 Seed = 1; Seed <= 60; ++Seed)
	{
		FSimRun Run(Seed);
		Run.ChoosePatron(EDTPath::Storm);
		PlayRun(Run, 20, 0, false, [&](const FDTOfferHand& Hand, FSimRun& S)
		{
			if (Hand.HandKind != EDTHandKind::Level || !DTOffers::IsCommitted(S.P()))
			{
				return;
			}
			const TArray<FDTOfferCard> Legal = DTOffers::Candidates(S.P(), S.Pool);
			const int32 LegalBlessings = Legal.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing; }).Num();
			const bool bOwnedLegal = Legal.ContainsByPredicate([&S](const FDTOfferCard& C)
			{
				return C.Kind == EDTCardKind::Blessing && DTOffers::OwnedInArchetype(S.P().Keepers, DTOffers::FindBlessing(C.ContentId).Archetype) > 0;
			});
			const int32 InHand = Hand.Cards.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing; }).Num();
			const bool bOwnedIn = Hand.Cards.ContainsByPredicate([&S](const FDTOfferCard& C)
			{
				return C.Kind == EDTCardKind::Blessing && DTOffers::OwnedInArchetype(S.P().Keepers, DTOffers::FindBlessing(C.ContentId).Archetype) > 0;
			});
			++Checked;
			if (InHand < FMath::Min(DTConst::OFFER_MIN_COMMITTED_BLESSINGS, LegalBlessings))
			{
				AddError(FString::Printf(TEXT("seed %d L%d: %d committed blessings"), Seed, Hand.Level, InHand));
			}
			if (bOwnedLegal && !bOwnedIn)
			{
				AddError(FString::Printf(TEXT("seed %d L%d: no owned-archetype blessing"), Seed, Hand.Level));
			}
		});
	}
	TestTrue(TEXT("post-L8 hands checked"), Checked > 300);

	// An empty guaranteed set: no Patron, nothing owned.
	FSimRun Empty(13);
	const FDTOfferHand Hand = DTOffers::BuildHand(Empty.P(), Empty.Pool, EDTHandKind::Level, 2, Empty.P().Offer.Rng);
	TestFalse(TEXT("empty guaranteed set: guarantee_filled = false"), Hand.bGuaranteeFilled);
	TestEqual(TEXT("empty guaranteed set: the hand still fills"), Hand.Cards.Num(), DTConst::OFFER_CARDS);
	return true;
}

// T-OFF-07
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferStarterPoolTest, "Dusktracer.Offers.StarterPool", DTOfferTest::Flags)
bool FDTOfferStarterPoolTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	const FDTOfferPool Fresh;
	int32 Weapons = 0;
	int32 StarterWeapons = 0;
	for (int32 W = 0; W < DTCat::NumWeapons; ++W)
	{
		Weapons += DTOffers::IsWeaponInPool(W, Fresh) ? 1 : 0;
		StarterWeapons += DTCat::Weapons[W].bStarter ? 1 : 0;
		if (DTOffers::IsWeaponInPool(W, Fresh) && !DTCat::Weapons[W].bStarter)
		{
			AddError(FString::Printf(TEXT("%s in the pool without its starter flag"), DTCat::Weapons[W].Id));
		}
	}
	TestEqual(TEXT("weapons: the starter rows within the cap"), Weapons, FMath::Min(StarterWeapons, DTConst::POOL_STARTER_WEAPONS));
	int32 Mods = 0;
	int32 StarterMods = 0;
	for (int32 M = 0; M < DTCat::NumModifiers; ++M)
	{
		Mods += DTOffers::IsModInPool(M, Fresh) ? 1 : 0;
		StarterMods += DTCat::Modifiers[M].bStarter ? 1 : 0;
	}
	TestEqual(TEXT("modifiers: the starter rows within the cap"), Mods, FMath::Min(StarterMods, DTConst::POOL_STARTER_MODIFIERS));
	FDTOfferPool Unlocked;
	Unlocked.Unlocked.Add(GravityMine);
	TestTrue(TEXT("DT.Unlock puts Gravity Mine in the pool"), DTOffers::IsWeaponInPool(DTWeapons::FindWeapon(GravityMine), Unlocked));
	TestFalse(TEXT("Gravity Mine is not a starter"), DTOffers::IsWeaponInPool(DTWeapons::FindWeapon(GravityMine), Fresh));

	int32 Keys = 0;
	for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
	{
		Keys += DTOffers::IsStarterKey(Row) ? 1 : 0;
	}
	TestEqual(TEXT("blessings: every archetype key within the cap"), Keys, FMath::Min(DTCat::NumArchetypes, DTConst::POOL_STARTER_BLESSINGS));

	FSimRun Sim(21);
	Sim.ChoosePatron(EDTPath::Storm);
	auto Blessings = [&Sim]()
	{
		return DTOffers::Candidates(Sim.P(), Sim.Pool).FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing; });
	};
	TArray<FDTOfferCard> Start = Blessings();
	TestEqual(TEXT("fresh Patron pool: only its archetype keys"), Start.Num(), DTConst::KEEPER_ARCHETYPES_PER_KEEPER);
	TestTrue(TEXT("fresh pool: keys only"), !Start.ContainsByPredicate([](const FDTOfferCard& C) { return DTOffers::FindBlessing(C.ContentId).Column != 0; }));
	DTOffers::AddBlessing(Sim.P().Keepers, FName(TEXT("Blessing.Storm.Tempest.Key")));
	const TArray<FDTOfferCard> After = Blessings();
	TestTrue(TEXT("a key unlocks its amplifiers"), After.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Blessing.Storm.Tempest.Amp1")); })
		&& After.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Blessing.Storm.Tempest.Amp2")); }));
	TestTrue(TEXT("the trait waits for its prerequisite cards"), After.ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Blessing.Storm.Tempest.Trait")); })
		== (DTConst::KEEPER_TRAIT_PREREQ_CARDS <= 1));
	DTOffers::AddBlessing(Sim.P().Keepers, FName(TEXT("Blessing.Storm.Tempest.Amp1")));
	TestTrue(TEXT("the trait joins after the prerequisite"), Blessings().ContainsByPredicate([](const FDTOfferCard& C) { return C.ContentId == FName(TEXT("Blessing.Storm.Tempest.Trait")); })
		== (DTConst::KEEPER_TRAIT_PREREQ_CARDS <= 2));
	return true;
}

// T-OFF-08
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferSecondKeeperTest, "Dusktracer.Offers.SecondKeeper", DTOfferTest::Flags)
bool FDTOfferSecondKeeperTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	const TArray<EDTPath> Keepers = DTOffers::RunKeepers();
	TestTrue(TEXT("S1 Keepers: Storm, Forge, Tide"), Keepers == TArray<EDTPath>({ EDTPath::Storm, EDTPath::Forge, EDTPath::Tide }));
	FSimRun Sim(31);
	const FDTOfferHand Patron = Sim.Hand(EDTHandKind::Patron, 1);
	TestEqual(TEXT("Patron hand: every Keeper up to the constant"), Patron.Cards.Num(), FMath::Min(Keepers.Num(), DTConst::OFFER_KEEPER_CHOICES));
	for (int32 Slot = 0; Slot < Patron.Cards.Num(); ++Slot)
	{
		TestEqual(TEXT("Patron hand: Keeper cards in Path order"), DTOffers::ParseKeeper(Patron.Cards[Slot].ContentId), Keepers[Slot]);
		TestEqual(TEXT("Patron hand: unweighted"), Patron.Cards[Slot].Weight, DTConst::OFFER_WEIGHT_BASE);
	}
	TestFalse(TEXT("Patron hand takes no tools"), DTOffers::HandAllowsTools(EDTHandKind::Patron));
	const FDTPickOutcome Chosen = Sim.Pick(Patron, 1);
	TestTrue(TEXT("Patron pick commits the Patron"), Chosen.bPatron && Sim.P().Offer.PatronPath == EDTPath::Forge && Sim.P().Keepers.Primary == EDTPath::Forge);

	TestEqual(TEXT("L8 becomes the Keeper hand"), DTOffers::ResolveKind(Sim.P(), EDTHandKind::Level, DTConst::OFFER_SECOND_KEEPER_LEVEL), EDTHandKind::Keeper);
	const FDTOfferHand Second = Sim.Hand(EDTHandKind::Level, DTConst::OFFER_SECOND_KEEPER_LEVEL);
	TestEqual(TEXT("Keeper hand kind"), Second.HandKind, EDTHandKind::Keeper);
	TestEqual(TEXT("Keeper hand: the non-Patron Keepers, at most the constant"), Second.Cards.Num(), FMath::Min(Keepers.Num() - 1, DTConst::OFFER_KEEPER_CHOICES));
	TestFalse(TEXT("Keeper hand: only Keeper cards, never the Patron"), Second.Cards.ContainsByPredicate([](const FDTOfferCard& C)
	{
		return C.Kind != EDTCardKind::Keeper || DTOffers::ParseKeeper(C.ContentId) == EDTPath::Forge;
	}));
	TestFalse(TEXT("Keeper hand takes no tools"), DTOffers::HandAllowsTools(EDTHandKind::Keeper));
	int32 TideSlot = Second.Cards.IndexOfByPredicate([](const FDTOfferCard& C) { return DTOffers::ParseKeeper(C.ContentId) == EDTPath::Tide; });
	const FDTPickOutcome Commit = Sim.Pick(Second, TideSlot);
	TestTrue(TEXT("the pick commits the second Keeper"), !Commit.bPatron && Sim.P().Offer.SecondPath == EDTPath::Tide && Sim.P().Keepers.bCommitDone);
	TestEqual(TEXT("after the commit L8 is a plain level hand"), DTOffers::ResolveKind(Sim.P(), EDTHandKind::Level, DTConst::OFFER_SECOND_KEEPER_LEVEL), EDTHandKind::Level);

	bool bThird = false;
	PlayRun(Sim, 20, 3, true, [&](const FDTOfferHand& Hand, FSimRun&)
	{
		bThird |= Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C)
		{
			return C.Kind == EDTCardKind::Blessing && DTOffers::FindBlessing(C.ContentId).Path == EDTPath::Storm;
		});
	});
	TestFalse(TEXT("the other Keeper's blessings never appear after the pick"), bThird);
	return true;
}

// T-OFF-09
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferReforgingTest, "Dusktracer.Offers.Reforging", DTOfferTest::Flags)
bool FDTOfferReforgingTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	for (int32 Seed = 1; Seed <= 40; ++Seed)
	{
		FSimRun Sim(Seed);
		Sim.ChoosePatron(static_cast<EDTPath>(Seed % 3));
		PlayRun(Sim, DTConst::OFFER_REFORGING_LEVEL - 1, 0, Seed % 2 == 0, [](const FDTOfferHand&, FSimRun&) {});
		TestEqual(TEXT("L16 is the Reforging hand"), DTOffers::ResolveKind(Sim.P(), EDTHandKind::Level, DTConst::OFFER_REFORGING_LEVEL), EDTHandKind::Reforging);
		const FDTOfferHand Hand = Sim.Hand(EDTHandKind::Level, DTConst::OFFER_REFORGING_LEVEL);
		TestEqual(TEXT("Reforging: exactly the card count"), Hand.Cards.Num(), DTConst::OFFER_REFORGING_CARDS);
		TSet<FString> Ids;
		float Last = TNumericLimits<float>::Max();
		bool bOrdered = true;
		for (const FDTOfferCard& Card : Hand.Cards)
		{
			Ids.Add(Identity(Card));
			if (Card.Kind == EDTCardKind::Blessing)
			{
				bOrdered &= Card.Weight <= Last;
				Last = Card.Weight;
				TestEqual(TEXT("filler blessings are Epic"), Card.Rarity, EDTRarity::Epic);
			}
		}
		TestEqual(TEXT("Reforging: distinct cards"), Ids.Num(), Hand.Cards.Num());
		TestTrue(TEXT("Reforging: the highest-weight legal blessings first"), bOrdered);
		const TArray<FDTOfferCard> Legal = DTOffers::Candidates(Sim.P(), Sim.Pool).FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Blessing; });
		float Best = 0.0f;
		for (const FDTOfferCard& C : Legal)
		{
			Best = FMath::Max(Best, C.Weight);
		}
		if (Legal.Num() > 0)
		{
			TestEqual(TEXT("Reforging: the first card is the best legal blessing"), Hand.Cards[0].Weight, Best);
		}
		FDTPlayerRunState Copy = Sim.P();
		TestTrue(TEXT("Reforging: the first card is pickable"), DTOffers::ApplyPickState(Copy, Hand.HandKind, Hand.Cards[0]).bValid);
	}
	// Every source missing and no blessing left: Reserve keeps it a real three-way choice.
	FSimRun Bare(99);
	Bare.P().Offer.PatronPath = EDTPath::Storm;
	for (int32 Row = 0; Row < DTConst::KEEPER_ARCHETYPES_PER_KEEPER; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			DTOffers::AddBlessing(Bare.P().Keepers, FName(DTCat::Archetypes[Row].BlessingIds[Column]));
		}
	}
	const FDTOfferHand Exhausted = Bare.Hand(EDTHandKind::Reforging, DTConst::OFFER_REFORGING_LEVEL);
	TestEqual(TEXT("exhausted Reforging still has its card count"), Exhausted.Cards.Num(), DTConst::OFFER_REFORGING_CARDS);
	TestTrue(TEXT("exhausted Reforging falls back to Reserve"), Exhausted.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Reserve; }));
	return true;
}

// T-OFF-10 (generator) and the subsystem's tool counts (world, not ticked).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferToolsTest, "Dusktracer.Offers.Tools", DTOfferTest::Flags)
bool FDTOfferToolsTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	FSimRun Sim(41);
	TestEqual(TEXT("rerolls start at the constant"), Sim.P().Offer.RerollsLeft, DTConst::OFFER_REROLLS);
	TestEqual(TEXT("banishes start at the constant"), Sim.P().Offer.BanishLeft, DTConst::OFFER_BANISH);
	TestEqual(TEXT("locks start at the constant"), Sim.P().Offer.LockLeft, DTConst::OFFER_LOCK);
	TestFalse(TEXT("Reserve refuses banish and lock"), DTOffers::CardAllowsBanishLock(DTOffers::MakeReserve(EDTReserveKind::Hone)));
	FDTOfferCard KeeperCard;
	KeeperCard.Kind = EDTCardKind::Keeper;
	TestFalse(TEXT("Keeper cards refuse banish and lock"), DTOffers::CardAllowsBanishLock(KeeperCard));
	KeeperCard.Kind = EDTCardKind::Reforging;
	TestFalse(TEXT("Reforging cards refuse banish and lock"), DTOffers::CardAllowsBanishLock(KeeperCard));
	TestFalse(TEXT("no tools in a Reforging hand"), DTOffers::HandAllowsTools(EDTHandKind::Reforging));

	Sim.ChoosePatron(EDTPath::Storm);
	Sim.Weapons.Give(Sim.Run, 0, PulseRifle, 2);
	FDTOfferHand Hand = Sim.Hand(EDTHandKind::Level, 3);
	Hand.Cards[2].bLocked = true;
	const FDTOfferCard Held = Hand.Cards[2];
	const FDTOfferHand Rerolled = DTOffers::BuildHand(Sim.P(), Sim.Pool, Hand.HandKind, 3, Sim.P().Offer.Rng, TArray<FDTOfferCard>({ Held }));
	TestTrue(TEXT("reroll keeps the locked card"), Rerolled.Cards.ContainsByPredicate([&Held](const FDTOfferCard& C) { return DTOffers::SameIdentity(C, Held) && C.bLocked; }));

	// Banish: the id leaves the pool for the run, and the slot refills with a different legal card.
	FDTOfferHand Fresh = Sim.Hand(EDTHandKind::Level, 3);
	const int32 Slot = Fresh.Cards.IndexOfByPredicate([](const FDTOfferCard& C) { return DTOffers::CardAllowsBanishLock(C); });
	const FDTOfferCard Gone = Fresh.Cards[Slot];
	Sim.P().Offer.Banished.Add(Gone.ContentId);
	DTOffers::RefillSlot(Sim.P(), Sim.Pool, Fresh, Slot, Sim.P().Offer.Rng);
	TestTrue(TEXT("banish refills the slot"), Fresh.Cards[Slot].ContentId != Gone.ContentId);
	TestFalse(TEXT("banished id leaves the pool"), DTOffers::Candidates(Sim.P(), Sim.Pool).ContainsByPredicate([&Gone](const FDTOfferCard& C) { return C.ContentId == Gone.ContentId; }));
	TestEqual(TEXT("banish keeps the card count"), Fresh.Cards.Num(), DTConst::OFFER_CARDS);

	// Lock: carried into the next level-up hand; refunded (dropped) when it turned illegal.
	FDTOfferCard Upgrade;
	Upgrade.Kind = EDTCardKind::WeaponUpgrade;
	Upgrade.ContentId = PulseRifle;
	Upgrade.TargetWeapon = PulseRifle;
	Upgrade.bLocked = true;
	Sim.P().Offer.Locked = Upgrade;
	const FDTOfferHand Next = Sim.Hand(EDTHandKind::Level, 4);
	TestTrue(TEXT("lock carries a card into the next level-up hand"), Next.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.bLocked && C.ContentId == FName(TEXT("Weapon.PulseRifle")); }));
	Sim.Weapons.LevelUp(Sim.Run, 0, 0, DTWeapons::GetMaxLevel(DTWeapons::FindWeapon(PulseRifle)));
	Sim.P().Offer.Locked = Upgrade;
	TArray<int32> Dropped;
	const FDTOfferHand Late = Sim.Hand(EDTHandKind::Level, 5, 0, &Dropped);
	TestEqual(TEXT("an illegal held card is dropped (and refunded by the flow)"), Dropped.Num(), 1);
	TestEqual(TEXT("the hand still fills"), Late.Cards.Num(), DTConst::OFFER_CARDS);

	// Through the subsystem: counts never go negative; Reserve refuses tools.
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTOfferToolsWorld")));
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	UDTOfferSubsystem* Offers = World->GetSubsystem<UDTOfferSubsystem>();
	TestNotNull(TEXT("offer subsystem in a game world"), Offers);
	if (Offers)
	{
		Offers->UpdateFlow();
		TestNotNull(TEXT("the run-start Patron hand opens first"), Offers->GetOpenHand(0));
		TestEqual(TEXT("it is the Patron hand"), Offers->GetOpenHand(0) ? Offers->GetOpenHand(0)->HandKind : EDTHandKind::Level, EDTHandKind::Patron);
		TestFalse(TEXT("no reroll in the Patron hand"), Offers->Reroll(0));
		TestTrue(TEXT("pick the Patron"), Offers->Pick(0, 0));
		Offers->BuildHand(0, EDTHandKind::Level, 2);
		for (int32 Index = 0; Index < DTConst::OFFER_REROLLS + 3; ++Index)
		{
			Offers->Reroll(0);
		}
		const FDTPlayerOfferState& Tools = World->GetSubsystem<UDTSimSubsystem>()->GetRun().GetPlayer(0).Offer;
		TestEqual(TEXT("rerolls stop at zero"), Tools.RerollsLeft, 0);
		for (int32 Index = 0; Index < DTConst::OFFER_BANISH + 3; ++Index)
		{
			Offers->Banish(0, 0);
		}
		TestEqual(TEXT("banishes stop at zero"), Tools.BanishLeft, 0);
		TestTrue(TEXT("lock a card"), Offers->Lock(0, 1));
		TestFalse(TEXT("no second lock"), Offers->Lock(0, 2));
		TestEqual(TEXT("locks stop at zero"), Tools.LockLeft, 0);
		TestTrue(TEXT("pick another card"), Offers->Pick(0, 0));
		TestTrue(TEXT("the held card waits for the next level-up hand"), Tools.Locked.IsSet());
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
	return true;
}

// T-OFF-11
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferAntiFrustrationTest, "Dusktracer.Offers.AntiFrustration", DTOfferTest::Flags)
bool FDTOfferAntiFrustrationTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	int32 Hands = 0;
	int32 Missing = 0;
	for (int32 Seed = 1; Seed <= 300; ++Seed)
	{
		FSimRun Sim(Seed);
		Sim.ChoosePatron(static_cast<EDTPath>(Seed % 3));
		Sim.Weapons.Give(Sim.Run, 0, PulseRifle, 1);
		Sim.P().Level = DTConst::OFFER_FORCE_WEAPON_LEVEL;
		const FDTOfferHand Hand = Sim.Hand(EDTHandKind::Level, DTConst::OFFER_FORCE_WEAPON_LEVEL);
		++Hands;
		Missing += Hand.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Weapon; }) ? 0 : 1;
	}
	TestEqual(FString::Printf(TEXT("below the weapon floor a new weapon is offered (%d hands)"), Hands), Missing, 0);

	FDTOfferInputGuard Guard;
	Guard.Open(100.0);
	TestFalse(TEXT("a confirm inside the guard picks nothing"), Guard.Allows(100.0 + 0.5 * DTConst::OFFER_INPUT_GUARD_S));
	TestTrue(TEXT("a confirm after the guard picks"), Guard.Allows(100.0 + DTConst::OFFER_INPUT_GUARD_S));
	TestTrue(TEXT("number keys are confirm-type"), FDTOfferInputGuard::IsConfirmType(EDTOfferAction::PickSlot));
	TestFalse(TEXT("navigation is never guarded"), FDTOfferInputGuard::IsConfirmType(EDTOfferAction::Next));

	for (EDTOfferAction Action : { EDTOfferAction::Prev, EDTOfferAction::Next, EDTOfferAction::Confirm, EDTOfferAction::PickSlot,
		EDTOfferAction::Reroll, EDTOfferAction::Banish, EDTOfferAction::Lock })
	{
		const TArray<FKey> Keys = DTOfferInput::KeysFor(Action);
		const bool bKeyboard = Keys.ContainsByPredicate([](const FKey& K) { return !K.IsGamepadKey() && !K.IsMouseButton(); });
		const bool bPad = Keys.ContainsByPredicate([](const FKey& K) { return K.IsGamepadKey(); });
		TestTrue(FString::Printf(TEXT("action %d has a keyboard key"), static_cast<int32>(Action)), bKeyboard);
		// Picking a slot directly is a keyboard shortcut; on a pad the focus moves and Confirm picks.
		TestTrue(FString::Printf(TEXT("action %d has a gamepad button"), static_cast<int32>(Action)), bPad || Action == EDTOfferAction::PickSlot);
	}
	TSet<FKey> Seen;
	bool bUnique = true;
	for (const FKey& Key : DTOfferInput::Keys())
	{
		bool bAlready = false;
		Seen.Add(Key, &bAlready);
		bUnique &= !bAlready && DTOfferInput::ForKey(Key).Action != EDTOfferAction::None;
	}
	TestTrue(TEXT("every bound key maps to exactly one action"), bUnique);
	return true;
}

// T-OFF-12
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferReserveTest, "Dusktracer.Offers.Reserve", DTOfferTest::Flags)
bool FDTOfferReserveTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	TestTrue(TEXT("at least as many Reserve kinds as cards"), static_cast<int32>(EDTReserveKind::Count) >= DTConst::OFFER_CARDS);
	FSimRun Sim(51);
	// Exhaust the pool: no Patron (no blessings), every weapon and modifier banished.
	for (int32 W = 0; W < DTCat::NumWeapons; ++W)
	{
		Sim.P().Offer.Banished.Add(DTWeapons::WeaponId(W));
	}
	for (int32 M = 0; M < DTCat::NumModifiers; ++M)
	{
		Sim.P().Offer.Banished.Add(DTWeapons::ModifierId(M));
	}
	TestEqual(TEXT("the pool is exhausted"), DTOffers::Candidates(Sim.P(), Sim.Pool).Num(), 0);
	for (EDTHandKind Kind : { EDTHandKind::Level, EDTHandKind::Chest, EDTHandKind::Reforging })
	{
		const FDTOfferHand Hand = Sim.Hand(Kind, 3);
		TestEqual(TEXT("an exhausted hand still reaches its count"), Hand.Cards.Num(), DTOffers::HandSize(Kind));
		TSet<FName> Kinds;
		for (const FDTOfferCard& Card : Hand.Cards)
		{
			TestEqual(TEXT("only Reserve fills an exhausted hand"), Card.Kind, EDTCardKind::Reserve);
			Kinds.Add(Card.ContentId);
		}
		TestEqual(TEXT("at most one card per Reserve kind"), Kinds.Num(), Hand.Cards.Num());
	}
	// Two legal cards left: both are shown, Reserve fills only the rest.
	Sim.P().Offer.Banished.Remove(PulseRifle);
	Sim.P().Offer.Banished.Remove(ArcBlade);
	const FDTOfferHand Thin = Sim.Hand(EDTHandKind::Level, 3);
	TestEqual(TEXT("thin pool: card count"), Thin.Cards.Num(), DTConst::OFFER_CARDS);
	TestEqual(TEXT("thin pool: no legal card left out"), Thin.Cards.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Weapon; }).Num(), 2);
	TestEqual(TEXT("thin pool: Reserve only in the empty slots"), Thin.Cards.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Reserve; }).Num(),
		DTConst::OFFER_CARDS - 2);
	TestTrue(TEXT("Reserve after the legal cards"), Thin.Cards[0].Kind == EDTCardKind::Weapon && Thin.Cards[1].Kind == EDTCardKind::Weapon);

	// Effects that live in run state: Spare Terms adds a reroll.
	const int32 Before = Sim.P().Offer.RerollsLeft;
	const FDTPickOutcome Spare = DTOffers::ApplyPickState(Sim.P(), EDTHandKind::Level, DTOffers::MakeReserve(EDTReserveKind::SpareTerms));
	TestTrue(TEXT("Spare Terms applies"), Spare.bValid);
	TestEqual(TEXT("Spare Terms adds its rerolls"), Sim.P().Offer.RerollsLeft, Before + DTConst::OFFER_RESERVE_SPARE_REROLLS);
	TestEqual(TEXT("Reserve carries no Keeper or archetype"), DTOffers::FindBlessing(DTOffers::ReserveId(EDTReserveKind::Hone)).Archetype, static_cast<int32>(INDEX_NONE));
	return true;
}

// T-OFF-13 (with T-PRE-08)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferCardIdentityTest, "Dusktracer.Offers.CardIdentity", DTOfferTest::Flags)
bool FDTOfferCardIdentityTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	TSet<int32> Borders;
	TSet<int32> Icons;
	TSet<FString> Labels;
	for (int32 Kind = 0; Kind < DTOfferStyle::NumCardKinds; ++Kind)
	{
		const FDTCardStyle Style = DTOfferStyle::ForKind(static_cast<EDTCardKind>(Kind));
		Borders.Add(static_cast<int32>(Style.Border));
		Icons.Add(static_cast<int32>(Style.Icon));
		Labels.Add(Style.LabelKey);
	}
	TestEqual(TEXT("one border per card kind"), Borders.Num(), DTOfferStyle::NumCardKinds);
	TestEqual(TEXT("one icon per card kind"), Icons.Num(), DTOfferStyle::NumCardKinds);
	TestEqual(TEXT("one label per card kind"), Labels.Num(), DTOfferStyle::NumCardKinds);
	const EDTCardFamily Reserve = DTOfferStyle::ForKind(EDTCardKind::Reserve).Family;
	for (EDTCardKind Kind : { EDTCardKind::Weapon, EDTCardKind::WeaponUpgrade, EDTCardKind::Mod, EDTCardKind::Blessing, EDTCardKind::Keeper })
	{
		TestTrue(TEXT("Reserve is distinct from every content kind's family"), DTOfferStyle::ForKind(Kind).Family != Reserve);
	}
	TestTrue(TEXT("tech kinds share the machined family"), DTOfferStyle::ForKind(EDTCardKind::Mod).Family == DTOfferStyle::ForKind(EDTCardKind::Weapon).Family);
	TestTrue(TEXT("Keeper kinds share the stone family"), DTOfferStyle::ForKind(EDTCardKind::Keeper).Family == DTOfferStyle::ForKind(EDTCardKind::Blessing).Family);

	TSet<int32> Pips;
	TSet<uint32> RarityColours;
	TSet<FString> RarityLabels;
	for (int32 Rarity = 0; Rarity < DTOfferStyle::NumRarities; ++Rarity)
	{
		Pips.Add(DTOfferStyle::RarityPips(static_cast<EDTRarity>(Rarity)));
		RarityColours.Add(DTOfferStyle::RarityToken(static_cast<EDTRarity>(Rarity)).DWColor());
		RarityLabels.Add(DTOfferStyle::RarityLabelKey(static_cast<EDTRarity>(Rarity)));
	}
	TestEqual(TEXT("rarity marker: unique pip count"), Pips.Num(), DTOfferStyle::NumRarities);
	TestEqual(TEXT("rarity marker: unique token"), RarityColours.Num(), DTOfferStyle::NumRarities);
	TestEqual(TEXT("rarity marker: unique label"), RarityLabels.Num(), DTOfferStyle::NumRarities);

	const FColor Forbidden[] = { DTPal::Enemy_Bullet, DTPal::Enemy_BulletCore, DTPal::Enemy_Muzzle, DTPal::Enemy_Body, DTPal::Enemy_Optic, DTPal::Telegraph };
	for (EDTCardFamily Family : { EDTCardFamily::Tech, EDTCardFamily::Stone, EDTCardFamily::Plain })
	{
		for (const FColor& Token : Forbidden)
		{
			TestTrue(TEXT("no card style reads an enemy or telegraph token"), DTOfferStyle::FamilyToken(Family) != Token);
		}
	}

	// Every key the screen can read exists in its string table (names, rules, labels, titles, tools, hints).
	int32 Missing = 0;
	for (const TPair<FString, FString>& Key : DTOfferText::AllKeys())
	{
		FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(FName(*Key.Key));
		FString Source;
		if (!Table.IsValid() || !Table->GetSourceString(Key.Value, Source) || Source.IsEmpty())
		{
			AddError(FString::Printf(TEXT("missing text %s.%s"), *Key.Key, *Key.Value));
			++Missing;
		}
	}
	TestEqual(TEXT("every card text key exists"), Missing, 0);

	// Blessing rules text gets every argument it names from expression.csv (no "{name}" left after formatting).
	for (int32 Row = 0; Row < DTCat::NumArchetypes; ++Row)
	{
		for (int32 Column = 0; Column < 4; ++Column)
		{
			FDTOfferCard Card;
			Card.Kind = EDTCardKind::Blessing;
			Card.ContentId = FName(DTCat::Archetypes[Row].BlessingIds[Column]);
			const FString Text = DTOfferText::Desc(Card).ToString();
			if (Text.Contains(TEXT("{")))
			{
				AddError(FString::Printf(TEXT("%s: unfilled argument in '%s'"), *Card.ContentId.ToString(), *Text));
			}
		}
	}
	return true;
}

// T-OFF-14
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferChestHandTest, "Dusktracer.Offers.ChestHand", DTOfferTest::Flags)
bool FDTOfferChestHandTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	for (int32 Seed = 1; Seed <= 200; ++Seed)
	{
		FSimRun Sim(Seed);
		Sim.ChoosePatron(static_cast<EDTPath>(Seed % 3));
		Sim.Weapons.Give(Sim.Run, 0, PulseRifle, 1 + Seed % 4);
		const FDTOfferHand Hand = Sim.Hand(EDTHandKind::Chest, 4);
		TestEqual(TEXT("chest: card count"), Hand.Cards.Num(), DTConst::OFFER_CHEST_CARDS);
		TestFalse(TEXT("chest: no guaranteed slot"), Hand.bGuaranteeFilled);
		for (const FDTOfferCard& Card : Hand.Cards)
		{
			TestFalse(TEXT("chest: no guaranteed card"), Card.bGuaranteed);
			TestTrue(TEXT("chest: no Keeper or Reforging card"), Card.Kind != EDTCardKind::Keeper && Card.Kind != EDTCardKind::Reforging);
			TestTrue(TEXT("chest: rarity at least Rare"), Card.Kind == EDTCardKind::Reserve || Card.Rarity != EDTRarity::Common);
		}
	}

	// Queue order through the subsystem: level-up hands in level order, then chests, one open at a time.
	FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTOfferQueueWorld")));
	Context.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	UDTOfferSubsystem* Offers = World->GetSubsystem<UDTOfferSubsystem>();
	if (Offers)
	{
		Offers->UpdateFlow();
		Offers->Pick(0, 0); // Patron
		FDTPlayerRunState& Player = World->GetSubsystem<UDTSimSubsystem>()->GetRun().GetPlayer(0);
		FDTOfferHand Chest;
		Chest.HandKind = EDTHandKind::Chest;
		Chest.Level = 2;
		FDTOfferHand L5;
		L5.Level = 5;
		FDTOfferHand L3;
		L3.Level = 3;
		Player.Offer.PendingHands.Add(Chest);
		Player.Offer.PendingHands.Add(L5);
		Player.Offer.PendingHands.Add(L3);
		TArray<FString> Order;
		for (int32 Guard = 0; Guard < 8 && (Offers->UpdateFlow(), Offers->GetOpenHand(0) != nullptr); ++Guard)
		{
			const FDTOfferHand* Open = Offers->GetOpenHand(0);
			Order.Add(FString::Printf(TEXT("%d:%d"), static_cast<int32>(Open->HandKind), Open->Level));
			TestEqual(TEXT("one open hand at a time; the rest stay queued"), Player.Offer.PendingHands.Num(), 3 - Order.Num() + 1);
			Offers->Pick(0, 0);
		}
		TestEqual(TEXT("queue: L3, L5, then the chest"), FString::Join(Order, TEXT(" ")), FString::Printf(TEXT("%d:3 %d:5 %d:2"),
			static_cast<int32>(EDTHandKind::Level), static_cast<int32>(EDTHandKind::Level), static_cast<int32>(EDTHandKind::Chest)));
		const FDTOfferPlayerStats* Stats = Offers->GetStats(0);
		TestEqual(TEXT("decisions: three hands, the Patron hand not counted"), Stats ? Stats->Decisions : -1, 3);
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
	return true;
}

// T-OFF-15 (the bot bound against humans stays GATED; this checks the budget sum and a steered headless run's Reserve share)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTOfferBudgetTest, "Dusktracer.Offers.Budget", DTOfferTest::Flags)
bool FDTOfferBudgetTest::RunTest(const FString& Parameters)
{
	using namespace DTOfferTest;
	TestEqual(TEXT("budget split sums to the decisions per run"),
		DTConst::OFFER_BUDGET_LEVEL_HANDS + DTConst::OFFER_BUDGET_CHEST_HANDS + DTConst::OFFER_BUDGET_MILESTONES, DTConst::DECISIONS_PER_RUN);
	int64 Offered = 0;
	int64 Reserve = 0;
	int32 Decisions = 0;
	constexpr int32 Runs = 30;
	const int32 MaxLevel = 1 + DTConst::OFFER_BUDGET_LEVEL_HANDS + DTConst::OFFER_BUDGET_MILESTONES;
	const int32 ChestEvery = FMath::Max(1, (MaxLevel - 1) / DTConst::OFFER_BUDGET_CHEST_HANDS);
	for (int32 Seed = 1; Seed <= Runs; ++Seed)
	{
		FSimRun Sim(Seed);
		Sim.ChoosePatron(static_cast<EDTPath>(Seed % 3));
		PlayRun(Sim, MaxLevel, ChestEvery, false, [&](const FDTOfferHand& Hand, FSimRun&)
		{
			++Decisions;
			Offered += Hand.Cards.Num();
			Reserve += Hand.Cards.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Reserve; }).Num();
		});
	}
	const double PerRun = static_cast<double>(Decisions) / Runs;
	TestTrue(FString::Printf(TEXT("steered decisions per run %.1f within tolerance"), PerRun),
		FMath::Abs(PerRun - DTConst::DECISIONS_PER_RUN) <= DTConst::OFFER_DECISIONS_TOLERANCE);
	const double Share = Offered > 0 ? static_cast<double>(Reserve) / Offered : 1.0;
	TestTrue(FString::Printf(TEXT("Reserve share %.3f within C.OFFER.RESERVE_SHARE_MAX"), Share), Share <= DTConst::OFFER_RESERVE_SHARE_MAX);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
