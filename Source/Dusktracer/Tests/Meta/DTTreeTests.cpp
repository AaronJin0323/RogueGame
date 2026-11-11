// The Tree and the pool it gates (02_Systems/meta.md T-MET-20..25).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Meta.Tree; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Combat/DTReforge.h"
#include "Combat/DTWeaponCatalog.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "HAL/FileManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Meta/DTDiscovery.h"
#include "Meta/DTMetaSession.h"
#include "Meta/DTMetaSubsystem.h"
#include "Meta/DTTree.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTTreeTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	const FName StormAmps(TEXT("Branch.Storm.Blessing4"));
	const FName StormLast(TEXT("Branch.Storm.Blessing5"));
	const FName StormReforge(TEXT("Branch.Storm.Reforge"));
	const FName Reroll(TEXT("Trunk.Reroll"));

	static FString TempSave(const TCHAR* Name)
	{
		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("TreeTests")));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, Name);
		IFileManager::Get().Delete(*Path, false, true, true);
		return Path;
	}

	/** A file session holding Balance. */
	static void OpenRich(FDTMetaSession& Session, const TCHAR* Name, int64 Balance)
	{
		Session.Open(TempSave(Name));
		Session.Profile.Currency = Balance;
	}

	/** Every node with its prerequisites met, in catalog order, on a rich session; the S2 Keepers open first. */
	static void BuyAll(FAutomationTestBase& Test, FDTMetaSession& Session)
	{
		for (const DTCat::FTreeNodeRow& Node : DTCat::TreeNodes)
		{
			for (const FName& Need : DTTree::SplitIds(Node.Requires))
			{
				if (DTTree::FindNode(Need) == INDEX_NONE)
				{
					Session.Profile.Unlocked.Add(Need);
				}
			}
			Test.TestEqual(FString::Printf(TEXT("%s is bought"), Node.Id), DTTree::Buy(Session, FName(Node.Id)), EDTTreeBuy::Ok);
		}
	}

	static FDTOfferPool GatedPool(const FDTMetaProfile& Profile)
	{
		FDTOfferPool Pool;
		const FDTPoolGrant Grant = DTTree::MakeGrant(Profile, true);
		Pool.bMetaGate = Grant.bGated;
		for (const FName& Id : Grant.Unlocked)
		{
			if (DTOffers::IsTreeLocked(Id))
			{
				Pool.Unlocked.Add(Id);
			}
		}
		return Pool;
	}

	static void StartRun(FDTRunState& Run, EDTPath Patron)
	{
		Run.Start(7, 1);
		Run.GetPlayer(0).Offer.PatronPath = Patron;
	}

	static TArray<FName> BlessingCandidates(const FDTPlayerRunState& Player, const FDTOfferPool& Pool)
	{
		TArray<FName> Out;
		for (const FDTOfferCard& Card : DTOffers::Candidates(Player, Pool))
		{
			if (Card.Kind == EDTCardKind::Blessing)
			{
				Out.Add(Card.ContentId);
			}
		}
		return Out;
	}

	static void TakeEveryKey(FDTPlayerRunState& Player, EDTPath Path)
	{
		for (int32 Row = 0; Row < DTCat::NumAllArchetypes; ++Row)
		{
			if (DTCat::Archetypes[Row].Path == Path)
			{
				DTOffers::AddBlessing(Player.Keepers, FName(DTCat::Archetypes[Row].BlessingIds[0]));
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreeCatalog, "Dusktracer.Meta.TreeCatalog", DTTreeTest::Flags) // T-MET-20
bool FDTTreeCatalog::RunTest(const FString& Parameters)
{
	using namespace DTTreeTest;
	TestEqual(TEXT("six Trunk nodes and three per Path (R-MET-20)"), DTCat::NumTreeNodes, 6 + 3 * 6);
	for (const DTCat::FTreeNodeRow& Node : DTCat::TreeNodes)
	{
		const bool bPool = Node.Class == DTCat::ETreeClass::Pool;
		TestTrue(FString::Printf(TEXT("%s is pool or utility"), Node.Id), bPool || Node.Class == DTCat::ETreeClass::Utility);
		TestEqual(FString::Printf(TEXT("%s: a pool node grants nothing, a utility node grants a tool"), Node.Id), Node.Grant == DTCat::ETreeGrant::None, bPool);
		TestEqual(FString::Printf(TEXT("%s: only a pool node unlocks"), Node.Id), DTTree::SplitIds(Node.Unlocks).Num() > 0, bPool);
		TestTrue(FString::Printf(TEXT("%s has a rank and a price"), Node.Id), Node.Ranks >= 1 && Node.Cost > 0);
		TestFalse(FString::Printf(TEXT("%s is not a key the source cut"), Node.Id), FString(Node.Id).Contains(TEXT("Favor")) || FString(Node.Id).Contains(TEXT("Luck")));
	}

	// A full Tree reaches a run's start through the offer tools alone, and only those.
	FDTMetaSession Session;
	OpenRich(Session, TEXT("catalog.sav"), 1000000);
	BuyAll(*this, Session);
	const FDTPoolGrant Grant = DTTree::MakeGrant(Session.Profile, true);
	FDTRunState Plain;
	Plain.Start(7, 1);
	FDTRunState Full;
	Full.Start(7, 1);
	DTOffers::ApplyGrant(Full.GetPlayer(0).Offer, Grant);
	const FDTPlayerRunState& A = Plain.GetPlayer(0);
	const FDTPlayerRunState& B = Full.GetPlayer(0);
	TestEqual(TEXT("level"), B.Level, A.Level);
	TestEqual(TEXT("Favor"), B.Keepers.Favor[0], A.Keepers.Favor[0]);
	TestEqual(TEXT("Tier"), B.Keepers.Tier[0], A.Keepers.Tier[0]);
	TestEqual(TEXT("no card is granted"), B.Keepers.Owned.Num(), A.Keepers.Owned.Num());
	TestEqual(TEXT("no weapon is granted"), B.Weapons.Num(), A.Weapons.Num());
	TestEqual(TEXT("+1 Reroll"), B.Offer.RerollsLeft, DTConst::OFFER_REROLLS + DTConst::TREE_TOOL_PER_RANK);
	TestEqual(TEXT("+1 Banish"), B.Offer.BanishLeft, DTConst::OFFER_BANISH + DTConst::TREE_TOOL_PER_RANK);
	TestEqual(TEXT("+1 Lock"), B.Offer.LockLeft, DTConst::OFFER_LOCK + DTConst::TREE_TOOL_PER_RANK);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreeBuy, "Dusktracer.Meta.TreeBuy", DTTreeTest::Flags) // T-MET-21
bool FDTTreeBuy::RunTest(const FString& Parameters)
{
	using namespace DTTreeTest;
	FDTMetaSession Session;
	OpenRich(Session, TEXT("buy.sav"), DTConst::TREE_COST_BLESSING4);

	TestEqual(TEXT("an unknown node"), DTTree::CanBuy(Session, TEXT("Trunk.MaxHP")), EDTTreeBuy::UnknownNode);
	TestEqual(TEXT("a prerequisite first"), DTTree::CanBuy(Session, StormLast), EDTTreeBuy::Requires);
	TestEqual(TEXT("the node names it"), DTTree::GetNode(Session.Profile, StormLast).Missing[0], StormAmps);
	TestEqual(TEXT("a tile says Locked"), DTTree::GetNode(Session.Profile, StormLast).State, EDTTreeState::Locked);
	TestEqual(TEXT("an S2 Keeper's branch waits for that Keeper"), DTTree::GetNode(Session.Profile, TEXT("Branch.Shade.Blessing4")).Missing[0], FName(TEXT("Keeper.Shade")));
	TestEqual(TEXT("exact money buys"), DTTree::CanBuy(Session, StormAmps), EDTTreeBuy::Ok);
	TestEqual(TEXT("a rank is bought"), DTTree::Buy(Session, StormAmps), EDTTreeBuy::Ok);
	TestEqual(TEXT("the price is spent"), Session.Profile.Currency, 0LL);
	TestEqual(TEXT("the rank is kept"), Session.Profile.TreeRanks.FindRef(StormAmps), 1);
	TestTrue(TEXT("the unlocks land"), Session.Profile.Unlocked.Contains(TEXT("Blessing.Storm.Tempest.Amp1")) && Session.Profile.Unlocked.Contains(TEXT("Blessing.Storm.Tempest.Trait")));
	TestFalse(TEXT("a fresh save's archetype is no node's to write (R-MET-25)"), Session.Profile.Unlocked.Contains(TEXT("Blessing.Storm.ArcGrid.Amp1")));
	TestFalse(TEXT("the third archetype stays shut"), Session.Profile.Unlocked.Contains(TEXT("Blessing.Storm.Downpour.Amp1")));
	TestEqual(TEXT("maxed"), DTTree::Buy(Session, StormAmps), EDTTreeBuy::Maxed);
	TestEqual(TEXT("short"), DTTree::Buy(Session, StormLast), EDTTreeBuy::Short);
	TestEqual(TEXT("a refused buy spends nothing"), Session.Profile.TreeRanks.FindRef(StormLast), 0);

	// It autosaved: a reload reads the rank and the spend.
	FDTMetaSession Again;
	TestEqual(TEXT("the file is read"), Again.Open(Session.Path), EDTMetaOpen::Read);
	TestEqual(TEXT("the file has the rank"), Again.Profile.TreeRanks.FindRef(StormAmps), 1);
	TestEqual(TEXT("the file has the balance"), Again.Profile.Currency, 0LL);
	TestEqual(TEXT("the file has the unlocks"), Again.Profile.Unlocked.Num(), Session.Profile.Unlocked.Num());

	// A tool run buys nothing (R-MET-14).
	FDTMetaSession None;
	None.Open(FString());
	None.Profile.Currency = 1000000;
	TestEqual(TEXT("no file, no sale"), DTTree::Buy(None, StormAmps), EDTTreeBuy::NoSave);
	TestEqual(TEXT("nothing spent"), None.Profile.Currency, 1000000LL);

	TArray<FDTTreeNode> Nodes = DTTree::ListNodes(Session.Profile);
	TestEqual(TEXT("the list holds every node"), Nodes.Num(), DTCat::NumTreeNodes);
	TestEqual(TEXT("in catalog order"), Nodes[0].Id, Reroll);
	TestEqual(TEXT("a bought node reads Maxed"), DTTree::GetNode(Session.Profile, StormAmps).State, EDTTreeState::Maxed);
	Session.Profile.Currency = 0;
	TestEqual(TEXT("a tile says Short"), DTTree::GetNode(Session.Profile, Reroll).State, EDTTreeState::Short);
	Session.Profile.Currency = DTConst::TREE_COST_TOOL;
	TestEqual(TEXT("a tile says Open"), DTTree::GetNode(Session.Profile, Reroll).State, EDTTreeState::Open);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreePoolGate, "Dusktracer.Meta.PoolGate", DTTreeTest::Flags) // T-MET-22
bool FDTTreePoolGate::RunTest(const FString& Parameters)
{
	using namespace DTTreeTest;
	FDTRunState Run;
	StartRun(Run, EDTPath::Storm);
	FDTPlayerRunState& Player = Run.GetPlayer(0);
	TakeEveryKey(Player, EDTPath::Storm);

	FDTMetaProfile Empty;
	const FDTOfferPool Fresh = GatedPool(Empty);
	// With every Storm Key held, a fresh pool still offers Arc Grid's amplifiers (R-MET-25, D-064); its Trait waits for its cards.
	const int32 FreshCards = 2 + (DTConst::KEEPER_TRAIT_PREREQ_CARDS <= 1 ? 1 : 0);
	const TArray<FName> FreshStorm = BlessingCandidates(Player, Fresh);
	TestEqual(TEXT("a fresh gated pool has the first-run archetype's cards left"), FreshStorm.Num(), FreshCards);
	TestFalse(TEXT("and only Arc Grid's"), FreshStorm.ContainsByPredicate([](const FName& Id) { return !Id.ToString().StartsWith(TEXT("Blessing.Storm.ArcGrid.")); }));
	TestTrue(TEXT("the same run ungated has more"), BlessingCandidates(Player, FDTOfferPool()).Num() > FreshCards);

	FDTMetaProfile Two;
	Two.Unlocked.Add(TEXT("Blessing.Storm.Tempest.Amp1"));
	TArray<FName> One = BlessingCandidates(Player, GatedPool(Two));
	TestEqual(TEXT("one unlocked id is one more card"), One.Num(), FreshCards + 1);
	TestTrue(TEXT("that card"), One.Contains(FName(TEXT("Blessing.Storm.Tempest.Amp1"))));

	// A fresh save's Trait still needs C.KEEPER.TRAIT_PREREQ_CARDS owned cards of its archetype, the Key counted (R-KEE-04).
	{
		FDTRunState TraitRun;
		StartRun(TraitRun, EDTPath::Storm);
		FDTPlayerRunState& Holder = TraitRun.GetPlayer(0);
		const FName ArcTrait(TEXT("Blessing.Storm.ArcGrid.Trait"));
		const TCHAR* const ArcCards[] = { TEXT("Blessing.Storm.ArcGrid.Key"), TEXT("Blessing.Storm.ArcGrid.Amp1"), TEXT("Blessing.Storm.ArcGrid.Amp2") };
		for (int32 Owned = 1; Owned <= 3; ++Owned)
		{
			DTOffers::AddBlessing(Holder.Keepers, FName(ArcCards[Owned - 1]));
			TestEqual(FString::Printf(TEXT("a fresh save's Trait with %d owned card(s) of its archetype"), Owned), BlessingCandidates(Holder, Fresh).Contains(ArcTrait),
				Owned >= DTConst::KEEPER_TRAIT_PREREQ_CARDS);
		}
	}

	// Each S1 Keeper's first node writes exactly its one archetype's three ids, shut before and open after.
	{
		FDTMetaSession Each;
		OpenRich(Each, TEXT("gate_each.sav"), 1000000);
		for (const TCHAR* const Archetype : { TEXT("Storm.Tempest"), TEXT("Forge.Furnace"), TEXT("Tide.Whirlpool") })
		{
			const FString Label(Archetype);
			const FString Node = FString::Printf(TEXT("Branch.%s.Blessing4"), *Label.Left(Label.Find(TEXT("."))));
			TArray<FName> Want;
			for (const TCHAR* Column : { TEXT("Amp1"), TEXT("Amp2"), TEXT("Trait") })
			{
				Want.Add(FName(*FString::Printf(TEXT("Blessing.%s.%s"), Archetype, Column)));
			}
			const TSet<FName> Before = Each.Profile.Unlocked;
			TestEqual(Node, DTTree::Buy(Each, FName(*Node)), EDTTreeBuy::Ok);
			TArray<FName> Added = Each.Profile.Unlocked.Difference(Before).Array();
			Added.Sort(FNameLexicalLess());
			Want.Sort(FNameLexicalLess());
			TestTrue(FString::Printf(TEXT("%s writes exactly %s's three"), *Node, Archetype), Added == Want);
			for (const FName& Id : Want)
			{
				TestTrue(FString::Printf(TEXT("%s: shut fresh, open after"), *Id.ToString()), !DTOffers::IsOpen(Id, Fresh) && DTOffers::IsOpen(Id, GatedPool(Each.Profile)));
			}
		}
	}

	FDTMetaSession Session;
	OpenRich(Session, TEXT("gate.sav"), 1000000);
	TestEqual(TEXT("Blessing4"), DTTree::Buy(Session, StormAmps), EDTTreeBuy::Ok);
	const TArray<FName> Amps = BlessingCandidates(Player, GatedPool(Session.Profile));
	TestEqual(TEXT("the first-run archetype's and the bought one's amplifiers (traits wait for their prerequisite cards)"), Amps.Num(), 4);
	TestFalse(TEXT("the third archetype stays out"), Amps.ContainsByPredicate([](const FName& Id) { return Id.ToString().Contains(TEXT("Downpour")); }));
	TestEqual(TEXT("Blessing5"), DTTree::Buy(Session, StormLast), EDTTreeBuy::Ok);
	TestEqual(TEXT("then all six"), BlessingCandidates(Player, GatedPool(Session.Profile)).Num(), 6);
	TestEqual(TEXT("Reforge"), DTTree::Buy(Session, StormReforge), EDTTreeBuy::Ok);

	// A Trunk pool node opens what no Discovery names (R-MET-21).
	const int32 Scatter = DTWeapons::FindWeapon(TEXT("Weapon.ScatterCannon"));
	const int32 Rail = DTWeapons::FindWeapon(TEXT("Weapon.RailLance"));
	const int32 Fork = DTWeapons::FindModifier(TEXT("Mod.Fork"));
	const int32 Refit = DTWeapons::FindModifier(TEXT("Mod.Refit"));
	TestFalse(TEXT("a Trunk weapon is shut before its node"), DTOffers::IsWeaponInPool(Scatter, GatedPool(Session.Profile)));
	TestFalse(TEXT("a Workshop modifier is shut before its node"), DTOffers::IsModInPool(Fork, GatedPool(Session.Profile)));
	TestEqual(TEXT("Scatter Cannon"), DTTree::Buy(Session, TEXT("Trunk.ScatterCannon")), EDTTreeBuy::Ok);
	TestTrue(TEXT("its node opens the weapon"), DTOffers::IsWeaponInPool(Scatter, GatedPool(Session.Profile)));
	TestFalse(TEXT("and no other"), DTOffers::IsWeaponInPool(Rail, GatedPool(Session.Profile)));
	TestEqual(TEXT("Rail Lance"), DTTree::Buy(Session, TEXT("Trunk.RailLance")), EDTTreeBuy::Ok);
	TestTrue(TEXT("the Rail Lance is in"), DTOffers::IsWeaponInPool(Rail, GatedPool(Session.Profile)));
	TestEqual(TEXT("Workshop"), DTTree::Buy(Session, TEXT("Trunk.Workshop")), EDTTreeBuy::Ok);
	TestTrue(TEXT("the Workshop opens its first modifier"), DTOffers::IsModInPool(Fork, GatedPool(Session.Profile)));
	TestTrue(TEXT("and its last"), DTOffers::IsModInPool(Refit, GatedPool(Session.Profile)));

	// The Reforging hand: a shut Path row yields its slot and the hand stays whole (R-OFF-09).
	FDTRunState Late;
	StartRun(Late, EDTPath::Storm);
	FDTPlayerRunState& LatePlayer = Late.GetPlayer(0);
	LatePlayer.Offer.SecondPath = EDTPath::Forge;
	FRandomStream Rng(5);
	const FDTOfferHand Shut = DTOffers::BuildHand(LatePlayer, GatedPool(Empty), EDTHandKind::Reforging, DTConst::OFFER_REFORGING_LEVEL, Rng);
	TestEqual(TEXT("the hand is whole"), Shut.Cards.Num(), DTConst::OFFER_REFORGING_CARDS);
	TestFalse(TEXT("no shut Path row"), Shut.Cards.ContainsByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Reforging; }));
	const FDTOfferHand Open = DTOffers::BuildHand(LatePlayer, GatedPool(Session.Profile), EDTHandKind::Reforging, DTConst::OFFER_REFORGING_LEVEL, Rng);
	TestEqual(TEXT("an open row leads"), Open.Cards[0].ContentId, DTReforge::RowId(DTReforge::FindPath(EDTPath::Storm)));
	const FDTOfferHand Tool = DTOffers::BuildHand(LatePlayer, FDTOfferPool(), EDTHandKind::Reforging, DTConst::OFFER_REFORGING_LEVEL, Rng);
	TestEqual(TEXT("a tool run has both rows"), Tool.Cards.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Reforging; }).Num(), 2);

	// This very process is a tool run: the pool stays full whatever the save holds (R-MET-23).
	TestFalse(TEXT("automation does not gate"), UDTMetaSubsystem::GatesPool());
	TestFalse(TEXT("no session reads ungated"), UDTMetaSubsystem::GetRunGrant(nullptr).bGated);
	const FDTPoolGrant Off = DTTree::MakeGrant(Session.Profile, false);
	TestTrue(TEXT("an ungated grant hands nothing"), !Off.bGated && Off.Unlocked.Num() == 0 && Off.Rerolls == 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreeFirstRun, "Dusktracer.Meta.FirstRunPool", DTTreeTest::Flags) // T-MET-23
bool FDTTreeFirstRun::RunTest(const FString& Parameters)
{
	using namespace DTTreeTest;
	const FDTOfferPool Fresh = GatedPool(FDTMetaProfile());
	TestTrue(TEXT("a fresh pool is gated"), Fresh.bMetaGate);
	// Every Key is in, and of the other cards only the first-run archetype's of each S1 Keeper (R-MET-25, D-064).
	const TCHAR* const FirstRunKeys[] = { TEXT("Blessing.Storm.ArcGrid.Key"), TEXT("Blessing.Forge.Hammer.Key"), TEXT("Blessing.Tide.Frost.Key") };
	int32 Locked = 0;
	int32 S1Open = 0;
	int32 S1Shut = 0;
	int32 FirstRunRows = 0;
	for (int32 Row = 0; Row < DTCat::NumAllArchetypes; ++Row)
	{
		const FString Key(DTCat::Archetypes[Row].BlessingIds[0]);
		bool bFirstRun = false;
		for (const TCHAR* const FirstKey : FirstRunKeys)
		{
			bFirstRun |= Key == FirstKey;
		}
		const EDTPath Path = DTCat::Archetypes[Row].Path;
		const bool bS1 = Path == EDTPath::Storm || Path == EDTPath::Forge || Path == EDTPath::Tide;
		FirstRunRows += bFirstRun ? 1 : 0;
		for (int32 Column = 0; Column < 4; ++Column)
		{
			const FName Id(DTCat::Archetypes[Row].BlessingIds[Column]);
			const bool bOpen = DTOffers::IsOpen(Id, Fresh);
			TestEqual(*FString::Printf(TEXT("%s: a Key, or a card of a first-run archetype"), *Id.ToString()), bOpen, Column == 0 || bFirstRun);
			Locked += bOpen ? 0 : 1;
			S1Open += bS1 && bOpen ? 1 : 0;
			S1Shut += bS1 && !bOpen ? 1 : 0;
		}
	}
	TestEqual(TEXT("one first-run archetype per S1 Keeper"), FirstRunRows, 3);
	TestEqual(TEXT("an S1 Keeper's fresh pool: nine Keys and nine cards of the first-run archetypes"), S1Open, 18);
	TestEqual(TEXT("and none of the other eighteen"), S1Shut, 18);
	TestEqual(TEXT("three locked cards per archetype, but the first-run ones"), Locked, (DTCat::NumAllArchetypes - FirstRunRows) * 3);
	for (int32 Row = 0; Row < DTCat::NumReforges; ++Row)
	{
		TestEqual(FString::Printf(TEXT("%s: an evolution is in, a Path row is out"), DTCat::Reforges[Row].Id), DTOffers::IsOpen(DTReforge::RowId(Row), Fresh), DTCat::Reforges[Row].bEvolve);
	}
	int32 Weapons = 0;
	int32 Mods = 0;
	for (int32 Weapon = 0; Weapon < DTCat::NumWeapons; ++Weapon)
	{
		const bool bFirst = DTCat::Weapons[Weapon].bStarter && !DTOffers::IsTreeLocked(DTWeapons::WeaponId(Weapon));
		TestEqual(FString::Printf(TEXT("%s: a starter row with no first-run lock"), DTCat::Weapons[Weapon].Id), DTOffers::IsWeaponInPool(Weapon, Fresh), bFirst);
		Weapons += bFirst ? 1 : 0;
	}
	for (int32 Mod = 0; Mod < DTCat::NumModifiers; ++Mod)
	{
		const bool bFirst = DTCat::Modifiers[Mod].bStarter && !DTOffers::IsTreeLocked(DTWeapons::ModifierId(Mod));
		TestEqual(FString::Printf(TEXT("%s: a starter row with no first-run lock"), DTCat::Modifiers[Mod].Id), DTOffers::IsModInPool(Mod, Fresh), bFirst);
		Mods += bFirst ? 1 : 0;
	}
	TestEqual(TEXT("four weapons"), Weapons, 4);
	TestEqual(TEXT("four modifiers"), Mods, 4);
	TestTrue(TEXT("the starting weapon is in"), DTOffers::IsWeaponInPool(DTWeapons::FindWeapon(TEXT("Weapon.PulseRifle")), Fresh));
	FDTMetaProfile Opened;
	Opened.Unlocked.Add(TEXT("Mod.Conduit"));
	const FDTOfferPool Later = GatedPool(Opened);
	const int32 Conduit = DTWeapons::FindModifier(TEXT("Mod.Conduit"));
	TestFalse(TEXT("a first-run lock is shut"), DTOffers::IsModInPool(Conduit, Fresh));
	TestTrue(TEXT("a Discovery's key opens it"), DTOffers::IsModInPool(Conduit, Later));
	TestTrue(TEXT("an ungated pool holds it"), DTOffers::IsModInPool(Conduit, FDTOfferPool()));
	TestEqual(TEXT("three Keepers in a fresh run"), DTOffers::RunKeepers(Fresh).Num(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreeKeeperDoors, "Dusktracer.Meta.KeeperDoors", DTTreeTest::Flags) // T-MET-28
bool FDTTreeKeeperDoors::RunTest(const FString& Parameters)
{
	using namespace DTTreeTest;
	const FName Shade(TEXT("Keeper.Shade"));
	const FName Radiance(TEXT("Keeper.Radiance"));
	const FName Quicksilver(TEXT("Keeper.Quicksilver"));
	const FName Forge(TEXT("Keeper.Forge"));
	const FName Tide(TEXT("Keeper.Tide"));

	// Each door is a row a run can earn, and what it writes opens what waits on it.
	FDTRunTally Run;
	Run.Steps = 1;
	Run.Kills = 1000;
	FDTMetaProfile Saved;
	DTDiscovery::Apply(Run, Saved);
	TestFalse(TEXT("a thousand kills under level 10 open no Keeper: that pool reads over R-OFF-15"), Saved.Unlocked.Contains(Shade));
	Run.Kills = 0;
	Run.Counters.Add(TEXT("Best.Level"), 10);
	TestEqual(TEXT("a Keeper's branch waits for it"), DTTree::GetNode(Saved, TEXT("Branch.Shade.Blessing4")).State, EDTTreeState::Locked);
	DTDiscovery::Apply(Run, Saved);
	TestTrue(TEXT("ReachL10 writes Keeper.Shade"), Saved.Unlocked.Contains(Shade));
	TestTrue(TEXT("in the bank that opens the Flak Mortar"), Saved.Unlocked.Contains(TEXT("Weapon.FlakMortar")));
	TestEqual(TEXT("and that branch is for sale"), DTTree::GetNode(Saved, TEXT("Branch.Shade.Blessing4")).Missing.Num(), 0);
	Run.Counters.Add(TEXT("Best.WeaponLevel"), DTConst::WEAPON_LEVEL_MAX);
	Run.Counters.Add(TEXT("Best.PathsAtTier2"), 2);
	Run.Steps = 600 * DTConst::SIM_FIXED_HZ;
	DTDiscovery::Apply(Run, Saved);
	TestTrue(TEXT("WeaponL8 writes Keeper.Radiance"), Saved.Unlocked.Contains(Radiance));
	TestTrue(TEXT("Survive10 writes Keeper.Quicksilver"), Saved.Unlocked.Contains(Quicksilver));
	TestTrue(TEXT("TwoPaths6 writes the duos"), Saved.Unlocked.Contains(TEXT("Reaction.Duos")));
	// D-035: the duos' door opens their card; the reactions start with that card, never with the save alone.
	TestFalse(TEXT("the duos' id does not reach the sim at run start"), DTOffers::OpensAtRunStart(TEXT("Reaction.Duos")));
	TestTrue(TEXT("a Keeper's door does"), DTOffers::OpensAtRunStart(Shade));
	TestTrue(TEXT("a weapon's does"), DTOffers::OpensAtRunStart(TEXT("Weapon.FlakMortar")));
	for (int32 Row = 0; Row < DTCat::NumReactions; ++Row)
	{
		TestTrue(FString::Printf(TEXT("%s starts the reactions"), DTCat::Reactions[Row].BlessingId), DTOffers::OpensStage(FName(DTCat::Reactions[Row].BlessingId)));
	}
	TestFalse(TEXT("a Keeper's own blessing does not"), DTOffers::OpensStage(TEXT("Blessing.Storm.Tempest.Key")));

	// What the run's pool holds on that save: UDTOfferSubsystem::MakePool adds these through the weapon system's Unlock.
	FDTOfferPool Pool = GatedPool(FDTMetaProfile());
	Pool.Unlocked.Add(Shade);
	Pool.Unlocked.Add(Radiance);
	Pool.Unlocked.Add(Quicksilver);
	Pool.Unlocked.Add(TEXT("Reaction.Duos"));

	// A met Keeper holds no seat of its own; the Keeper a run leads with takes the first, however many the save has met (R-KEE-23). The whole table is T-MET-31.
	FDTRunState State;
	StartRun(State, EDTPath::None);
	FDTPlayerRunState& Player = State.GetPlayer(0);
	const FName Storm(TEXT("Keeper.Storm"));
	const auto Holds = [](const FDTOfferHand& Hand, FName Id) { return Hand.Cards.ContainsByPredicate([Id](const FDTOfferCard& Card) { return Card.ContentId == Id; }); };
	FRandomStream Rng(3);
	const FDTOfferHand Plain = DTOffers::BuildHand(Player, Pool, EDTHandKind::Patron, 1, Rng);
	TestTrue(TEXT("three met Keepers take no S1 Keeper's seat in Path order"), Holds(Plain, Storm) && Holds(Plain, Forge));
	Pool.LeadPatron = Shade;
	const FDTOfferHand Led = DTOffers::BuildHand(Player, Pool, EDTHandKind::Patron, 1, Rng);
	TestEqual(TEXT("the hand keeps its size"), Led.Cards.Num(), DTConst::OFFER_KEEPER_CHOICES);
	TestEqual(TEXT("the lead takes the first seat"), Led.Cards[0].ContentId, Shade);
	TestEqual(TEXT("Path order holds the next"), Led.Cards[1].ContentId, Storm);
	Pool.LeadPatron = NAME_None;
	FDTMetaProfile Played;
	Played.LastPatron = TEXT("Forge");
	TestEqual(TEXT("a gated grant carries the last Patron"), DTTree::MakeGrant(Played, true).LastPatron, FName(TEXT("Forge")));
	TestTrue(TEXT("an ungated grant does not"), DTTree::MakeGrant(Played, false).LastPatron.IsNone());

	// A save's Keepers all draw at L8, so meeting one shuts no other out; a tool run keeps the seat rule (R-KEE-23).
	Player.Offer.PatronPath = EDTPath::Storm;
	TSet<FName> Seen;
	for (int32 Seed = 0; Seed < 64; ++Seed)
	{
		FRandomStream Draw(Seed);
		const FDTOfferHand Hand = DTOffers::BuildHand(Player, Pool, EDTHandKind::Keeper, DTConst::OFFER_SECOND_KEEPER_LEVEL, Draw);
		TestEqual(TEXT("the L8 hand is whole"), Hand.Cards.Num(), DTConst::OFFER_KEEPER_CHOICES);
		for (const FDTOfferCard& Card : Hand.Cards)
		{
			Seen.Add(Card.ContentId);
		}
	}
	TestTrue(TEXT("an S1 Keeper still draws on a full profile"), Seen.Contains(Forge) && Seen.Contains(Tide));
	TestTrue(TEXT("and every met Keeper does"), Seen.Contains(Shade) && Seen.Contains(Radiance) && Seen.Contains(Quicksilver));
	FDTOfferPool Tool = Pool;
	Tool.bMetaGate = false;
	FRandomStream ToolDraw(1);
	const FDTOfferHand Seated = DTOffers::BuildHand(Player, Tool, EDTHandKind::Keeper, DTConst::OFFER_SECOND_KEEPER_LEVEL, ToolDraw);
	TestTrue(TEXT("a tool run seats the unlocked Keepers"), Holds(Seated, Shade) && Holds(Seated, Radiance) && Holds(Seated, Quicksilver));
	const FDTOfferHand ToolPatrons = DTOffers::BuildHand(Player, Tool, EDTHandKind::Patron, 1, ToolDraw);
	TestTrue(TEXT("in its Patron hand too"), Holds(ToolPatrons, Shade) && Holds(ToolPatrons, Radiance) && Holds(ToolPatrons, Quicksilver));

	// A met Keeper's Keys are in, its Amps wait for its branch (R-MET-25), and a committed pair's duo card reaches a gated pool.
	Player.Offer.PatronPath = EDTPath::Shade;
	TestTrue(TEXT("a met Keeper's Key is offered"), BlessingCandidates(Player, Pool).Contains(FName(TEXT("Blessing.Shade.Hound.Key"))));
	TakeEveryKey(Player, EDTPath::Shade);
	TestFalse(TEXT("its Amp stays shut with the Key held"), BlessingCandidates(Player, Pool).Contains(FName(TEXT("Blessing.Shade.Hound.Amp1"))));
	Player.Offer.PatronPath = EDTPath::Storm;
	Player.Offer.SecondPath = EDTPath::Forge;
	TestTrue(TEXT("the door opens the duo card"), BlessingCandidates(Player, Pool).Contains(FName(TEXT("Blessing.Duo.StormForge"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreePatronSeats, "Dusktracer.Meta.PatronSeats", DTTreeTest::Flags) // T-MET-31
bool FDTTreePatronSeats::RunTest(const FString& Parameters)
{
	using namespace DTTreeTest;
	constexpr int32 Draws = 256; // seeds per hand: every Keeper of the run has to show on one
	const TArray<EDTPath> MetKeepers = { EDTPath::Shade, EDTPath::Radiance, EDTPath::Quicksilver };
	const TArray<EDTPath> Seated = DTOffers::RunKeepers();
	FDTRunState State;
	StartRun(State, EDTPath::None);
	const FDTPlayerRunState& Player = State.GetPlayer(0);

	// Every set of met Keepers, under every lead a save can name: nobody, or any Keeper of the run.
	for (int32 Mask = 0; Mask < 1 << MetKeepers.Num(); ++Mask)
	{
		FDTOfferPool Pool = GatedPool(FDTMetaProfile());
		for (int32 Index = 0; Index < MetKeepers.Num(); ++Index)
		{
			if (Mask & (1 << Index))
			{
				Pool.Unlocked.Add(DTOffers::KeeperId(MetKeepers[Index]));
			}
		}
		const TArray<EDTPath> Keepers = DTOffers::RunKeepers(Pool);
		TArray<FName> Leads = { NAME_None };
		for (const EDTPath Path : Keepers)
		{
			Leads.Add(DTOffers::KeeperId(Path));
		}
		for (const FName& Lead : Leads)
		{
			Pool.LeadPatron = Lead;
			TSet<FName> Reached;
			int32 Whole = 0;
			int32 Led = 0;
			int32 WithStarter = 0;
			int32 Still = 0;
			for (int32 Seed = 1; Seed <= Draws; ++Seed)
			{
				FRandomStream Draw(Seed);
				const FDTOfferHand Hand = DTOffers::BuildHand(Player, Pool, EDTHandKind::Patron, 1, Draw);
				TSet<FName> Ids;
				bool bStarter = false;
				for (const FDTOfferCard& Card : Hand.Cards)
				{
					Ids.Add(Card.ContentId);
					bStarter |= Seated.Contains(DTOffers::ParseKeeper(Card.ContentId));
				}
				Reached.Append(Ids);
				Whole += Hand.Cards.Num() == DTConst::OFFER_KEEPER_CHOICES && Ids.Num() == Hand.Cards.Num() ? 1 : 0;
				Led += Lead.IsNone() || (Hand.Cards.Num() > 0 && Hand.Cards[0].ContentId == Lead) ? 1 : 0;
				WithStarter += bStarter ? 1 : 0;
				Still += Draw.GetCurrentSeed() == Seed ? 1 : 0;
			}
			const FString Case = FString::Printf(TEXT("%d met, lead '%s'"), FMath::CountBits(Mask), *Lead.ToString());
			TestEqual(FString::Printf(TEXT("%s: every hand is whole, three different Keepers: meta adds no seat"), *Case), Whole, Draws);
			TestEqual(FString::Printf(TEXT("%s: the lead keeps the first seat"), *Case), Led, Draws);
			TestEqual(FString::Printf(TEXT("%s: no hand is all met Keepers, so meta shuts no S1 Keeper out"), *Case), WithStarter, Draws);
			TestEqual(FString::Printf(TEXT("%s: every Keeper of the run reaches this hand across Restarts"), *Case), Reached.Num(), Keepers.Num());
			TestEqual(FString::Printf(TEXT("%s: only a met Keeper makes this hand draw"), *Case), Still, Mask == 0 ? Draws : 0);
		}
	}

	// Path order fills the seats before the last, so a Keeper met can never push Storm or Forge past the second.
	FDTOfferPool Full = GatedPool(FDTMetaProfile());
	for (const EDTPath Path : MetKeepers)
	{
		Full.Unlocked.Add(DTOffers::KeeperId(Path));
	}
	const FName Storm = DTOffers::KeeperId(EDTPath::Storm);
	const FName Forge = DTOffers::KeeperId(EDTPath::Forge);
	const FName Shade = DTOffers::KeeperId(EDTPath::Shade);
	FRandomStream Draw(5);
	TestEqual(TEXT("Storm's first with nobody leading"), DTOffers::BuildHand(Player, Full, EDTHandKind::Patron, 1, Draw).Cards[0].ContentId, Storm);
	Full.LeadPatron = Shade;
	TestEqual(TEXT("Storm's second behind a met lead"), DTOffers::BuildHand(Player, Full, EDTHandKind::Patron, 1, Draw).Cards[1].ContentId, Storm);
	Full.LeadPatron = Storm;
	TestEqual(TEXT("Forge's second behind Storm"), DTOffers::BuildHand(Player, Full, EDTHandKind::Patron, 1, Draw).Cards[1].ContentId, Forge);
	Full.LeadPatron = NAME_None;

	// A tool run keeps its rule and its stream: the unlocked Keepers seat first and nothing is drawn (R-KEE-23, R-MET-16).
	FDTOfferPool Tool = Full;
	Tool.bMetaGate = false;
	FRandomStream ToolDraw(5);
	const FDTOfferHand ToolHand = DTOffers::BuildHand(Player, Tool, EDTHandKind::Patron, 1, ToolDraw);
	TestEqual(TEXT("a tool run's hand reads the unlocked Keepers"), ToolHand.Cards[0].ContentId, Shade);
	TestEqual(TEXT("and it draws nothing"), ToolDraw.GetCurrentSeed(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreeGrant, "Dusktracer.Meta.TreeGrant", DTTreeTest::Flags) // T-MET-24
bool FDTTreeGrant::RunTest(const FString& Parameters)
{
	using namespace DTTreeTest;
	FDTMetaSession Session;
	OpenRich(Session, TEXT("grant.sav"), 1000000);
	TestEqual(TEXT("Reroll"), DTTree::Buy(Session, Reroll), EDTTreeBuy::Ok);
	FDTPoolGrant Grant = DTTree::MakeGrant(Session.Profile, true);
	TestEqual(TEXT("one rank is its rerolls"), Grant.Rerolls, DTConst::TREE_TOOL_PER_RANK);
	TestEqual(TEXT("no banish"), Grant.Banishes, 0);
	TestEqual(TEXT("no lock"), Grant.Locks, 0);
	Session.Profile.TreeRanks.Add(Reroll, 99);
	Session.Profile.TreeRanks.Add(TEXT("Trunk.FromALaterBuild"), 3);
	Grant = DTTree::MakeGrant(Session.Profile, true);
	TestEqual(TEXT("a rank above the catalog's reads as the catalog's"), Grant.Rerolls, DTConst::TREE_TOOL_PER_RANK);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreeBuyWrite, "Dusktracer.Meta.TreeBuyWrite", DTTreeTest::Flags) // T-MET-26
bool FDTTreeBuyWrite::RunTest(const FString& Parameters)
{
	using namespace DTTreeTest;
	FDTMetaSession Session;
	OpenRich(Session, TEXT("write.sav"), 1000000);
	const FString Blocker = Session.Path + TEXT(".tmp");
	IFileManager::Get().DeleteDirectory(*Blocker, false, true);
	IFileManager::Get().MakeDirectory(*Blocker, true);
	AddExpectedError(TEXT("could not write"), EAutomationExpectedErrorFlags::Contains, 0);
	TestEqual(TEXT("a failed write still buys"), DTTree::Buy(Session, StormAmps), EDTTreeBuy::Ok);
	TestEqual(TEXT("the rank is in memory"), Session.Profile.TreeRanks.FindRef(StormAmps), 1);
	TestFalse(TEXT("no file yet"), IFileManager::Get().FileExists(*Session.Path));
	IFileManager::Get().DeleteDirectory(*Blocker, false, true);
	TestEqual(TEXT("the next rank writes"), DTTree::Buy(Session, Reroll), EDTTreeBuy::Ok);
	FDTMetaSession Again;
	TestEqual(TEXT("the file is read"), Again.Open(Session.Path), EDTMetaOpen::Read);
	TestEqual(TEXT("it carries the first purchase"), Again.Profile.TreeRanks.FindRef(StormAmps), 1);
	TestEqual(TEXT("and the second"), Again.Profile.TreeRanks.FindRef(Reroll), 1);

	Again.bWritable = false; // what a stuck session is (R-MET-12)
	TestEqual(TEXT("a stuck session sells nothing"), DTTree::Buy(Again, StormLast), EDTTreeBuy::NoSave);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTTreeText,"Dusktracer.Meta.TreeText", DTTreeTest::Flags) // T-MET-25
bool FDTTreeText::RunTest(const FString& Parameters)
{
	const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	for (const TCHAR* Name : { TEXT("en"), TEXT("zh-Hans") })
	{
		FInternationalization::Get().SetCurrentCulture(Name);
		for (int32 Row = 0; Row < DTCat::NumTreeNodes; ++Row)
		{
			const FString Node = DTTree::NodeName(Row).ToString();
			const FString Desc = DTTree::NodeDesc(Row).ToString();
			TestFalse(FString::Printf(TEXT("%s %s has a name"), Name, DTCat::TreeNodes[Row].Id), Node.IsEmpty() || Node.Contains(TEXT("Tree.")));
			TestFalse(FString::Printf(TEXT("%s %s has a line"), Name, DTCat::TreeNodes[Row].Id), Desc.IsEmpty() || Desc.Contains(TEXT("{")) || Desc.Contains(TEXT("Tree.")));
		}
	}
	FInternationalization::Get().SetCurrentCulture(Culture);
	return true;
}

#endif
