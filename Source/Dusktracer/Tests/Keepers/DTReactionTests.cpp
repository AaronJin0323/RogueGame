// Duo reactions and duo blessings (reactions.md, T-REA-01..12). A scene is the two Keepers' Keys, a far crowd the rifle cannot reach
// and hits written straight into the step, so each test names exactly what meets on which enemy.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Keepers/DTKeeperHarness.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Keepers/DTReactions.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferStyle.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTReactionTest
{
	using namespace DTKeeperTest;
	using DTCat::EReaction;

	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
	constexpr int32 Second = DTConst::SIM_FIXED_HZ;
	constexpr float ScaledHp = 1000.0f;
	/** Far outside the rifle's range, so only written hits land. */
	static const FVector2f Far(2000.0f, 0.0f);

	static int32 RowOf(EReaction Pair) { return static_cast<int32>(Pair); }

	static uint16 Src(EDTPath Path) { return DTKeepers::KeeperSource(Path); }

	static const TCHAR* KeyOf(EDTPath Path)
	{
		switch (Path)
		{
		case EDTPath::Storm: return TEXT("Storm.Tempest");
		case EDTPath::Forge: return TEXT("Forge.Hammer");
		case EDTPath::Tide: return TEXT("Tide.Frost");
		case EDTPath::Shade: return TEXT("Shade.Hound");
		case EDTPath::Radiance: return TEXT("Radiance.Lance");
		default: return TEXT("Quicksilver.Rebound");
		}
	}

	struct FDuoScene : FDTKeeperHarness
	{
		EReaction Pair;

		explicit FDuoScene(EReaction InPair, bool bDuo = false, bool bUnlock = true, int32 Seed = 7, const TCHAR* SecondKey = nullptr)
			: FDTKeeperHarness(Seed), Pair(InPair)
		{
			const DTCat::FReactionRow& R = DTCat::Reactions[RowOf(Pair)];
			if (bUnlock)
			{
				Weapons.Unlock(DTReactions::UnlockId());
			}
			Patron(R.PathA);
			Commit(R.PathB);
			Bless(Blessing(KeyOf(R.PathA), TEXT("Key")));
			Bless(Blessing(SecondKey ? SecondKey : KeyOf(R.PathB), TEXT("Key")));
			if (bDuo)
			{
				Bless(FName(R.BlessingId));
			}
			StepOnce(); // the rifle composes, so strongest damage is known
		}

		int32 Row() const { return RowOf(Pair); }
		uint16 Duo() const { return DTReactions::Source(Row()); }
		uint16 Rifle() const { return FDTTelemetrySources::RegisterWeapon(PulseRifle); }
		int64 Fires() const { return Weapons.GetReactionCounters(0, Row()).Fires; }
		float Strongest() const { return Weapons.GetLastSpec(0, 0).Damage; }
		uint32 Serial(int32 Enemy) const { return Enemies.GetSerials()[Enemy]; }

		int32 Add(const FVector2f& At, float Share = 1.0f)
		{
			FDTEnemySpawn Spawn;
			Spawn.Role = EDTEnemyRole::Swarm;
			Spawn.Position = At;
			Spawn.HpScale = ScaledHp;
			FRandomStream Rng(1);
			const int32 Enemy = Enemies.Spawn(Spawn, &Rng);
			Enemies.SetHp(Enemy, Enemies.GetMaxHp()[Enemy] * Share);
			return Enemy;
		}

		void Give(int32 Enemy, EDTStatus Status, float Magnitude = 1.0f)
		{
			FDTStatusApply Apply;
			Apply.Handle = FDTEnemyHandle{ Enemy, Serial(Enemy) };
			Apply.Status = Status;
			Apply.OwnerId = 0;
			Apply.Magnitude = Magnitude;
			Enemies.ApplyStatus(Apply);
		}

		void Mark(int32 Enemy) { Weapons.ApplyMark(0, Serial(Enemy), Step); }
		void Blind(int32 Enemy) { Weapons.ApplyBlind(0, Serial(Enemy), Step, 10.0f); }

		/** Writes a hit into the step; it resolves and reacts on the next StepOnce. */
		void Hit(int32 Enemy, uint16 Source, float Damage, bool bCrit = false, int8 Slot = INDEX_NONE)
		{
			Enemies.Publish(View);
			FDTHit& Out = View.Hits.AddDefaulted_GetRef();
			Out.EnemyIndex = Enemy;
			Out.EnemySerial = Serial(Enemy);
			Out.OwnerId = 0;
			Out.Damage = Damage;
			Out.Position = View.EnemyPositions[Enemy];
			Out.SourceId = Source;
			Out.WeaponSlot = Slot;
			Out.bCrit = bCrit;
		}

		void Dash(const FVector2f& At)
		{
			View.HeroPositions[0] = At;
			View.HeroIntangible[0] = true;
			StepMany(2);
			View.HeroIntangible[0] = false;
			StepMany(2);
		}

		int32 HitsOn(int32 Enemy, uint16 Source) const
		{
			int32 Count = 0;
			for (const FDTHit& Logged : HitLog)
			{
				Count += Logged.EnemySerial == Serial(Enemy) && Logged.SourceId == Source ? 1 : 0;
			}
			return Count;
		}

		uint8 Mask(int32 Enemy) const { return Enemies.GetStatusMask(Enemy); }
		bool Has(int32 Enemy, EDTStatus Status) const { return (Mask(Enemy) & (1 << static_cast<int32>(Status))) != 0; }
	};

	static TArray<FDTOfferCard> Duos(const FDTPlayerRunState& Player, const FDTOfferPool& Pool)
	{
		return DTOffers::Candidates(Player, Pool).FilterByPredicate([](const FDTOfferCard& C) { return DTReactions::FindDuo(C.ContentId) != INDEX_NONE; });
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaLive, "Dusktracer.Reactions.Live", DTReactionTest::Flags) // T-REA-01
bool FDTReaLive::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDTKeeperHarness H;
	H.Weapons.Unlock(DTReactions::UnlockId());
	H.Patron(EDTPath::Storm);
	H.Commit(EDTPath::Forge);
	TestEqual(TEXT("no blessing, no pair"), DTReactions::LiveMask(H.P().Keepers), 0u);
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	TestEqual(TEXT("one Keeper is no pair"), DTReactions::LiveMask(H.P().Keepers), 0u);
	H.Bless(Blessing(TEXT("Forge.Hammer"), TEXT("Key")));
	TestEqual(TEXT("both Keepers: their pair and no other"), DTReactions::LiveMask(H.P().Keepers), 1u << RowOf(EReaction::StormForge));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaStage, "Dusktracer.Reactions.Stage", DTReactionTest::Flags) // T-REA-02
bool FDTReaStage::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormForge, false, false);
	const int32 Enemy = S.Add(Far);
	S.Give(Enemy, EDTStatus::Burn, 10.0f);
	const int32 Before = S.Run.GetRng().Get(EDTRngStream::Combat).GetCurrentSeed();
	S.Hit(Enemy, Src(EDTPath::Storm), 100.0f);
	S.StepMany(3);
	TestEqual(TEXT("no unlock, no reaction"), S.Fires(), 0ll);
	TestEqual(TEXT("no stream drawn"), S.Run.GetRng().Get(EDTRngStream::Combat).GetCurrentSeed(), Before);
	FDTOfferPool Pool;
	S.P().Offer.PatronPath = EDTPath::Storm;
	S.P().Offer.SecondPath = EDTPath::Forge;
	TestEqual(TEXT("no unlock, no card"), Duos(S.P(), Pool).Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaSources, "Dusktracer.Reactions.Sources", DTReactionTest::Flags) // T-REA-03
bool FDTReaSources::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormForge);
	const int32 A = S.Add(Far);
	const int32 B = S.Add(Far + FVector2f(90.0f, 0.0f));
	S.Give(A, EDTStatus::Burn, 10.0f);
	S.Give(B, EDTStatus::Burn, 10.0f);
	S.Hit(A, DTKeepers::KeeperSource(EDTPath::Forge), 100.0f);
	S.StepMany(3);
	TestEqual(TEXT("a Forge hit is not a Storm hit"), S.Fires(), 0ll);
	S.Hit(A, S.Rifle(), 100.0f, false, 0);
	S.StepMany(3);
	TestEqual(TEXT("a plain weapon round is no Keeper's hit"), S.Fires(), 0ll);
	S.Hit(A, Src(EDTPath::Storm), 100.0f);
	S.StepMany(3);
	TestEqual(TEXT("a Storm hit fires"), S.Fires(), 1ll);
	TestTrue(TEXT("its hits carry the pair's source"), S.CountHits(S.Duo()) > 0);
	TestEqual(TEXT("the source is under Keeper."), FDTTelemetrySources::GetName(S.Duo()), FString(TEXT("Keeper.Duo.StormForge")));
	for (const FDTHit& Logged : S.HitLog)
	{
		if (Logged.SourceId == S.Duo())
		{
			TestEqual(TEXT("no slot, so no proc feeds on it"), static_cast<int32>(Logged.WeaponSlot), static_cast<int32>(INDEX_NONE));
		}
	}
	S.StepMany(Second);
	TestEqual(TEXT("a reaction's own hit starts nothing"), S.Fires(), 1ll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaPullContact, "Dusktracer.Reactions.PullContact", DTReactionTest::Flags) // T-REA-04
bool FDTReaPullContact::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDTKeeperHarness H;
	H.Weapons.Unlock(DTReactions::UnlockId());
	H.Patron(EDTPath::Tide);
	H.Commit(EDTPath::Forge);
	H.Bless(Blessing(TEXT("Tide.Whirlpool"), TEXT("Key")));
	H.Bless(Blessing(TEXT("Forge.Hammer"), TEXT("Key")));
	const int32 Near = H.AddEnemy(FVector2f(300.0f, 0.0f), ToughHp);
	const int32 Side = H.AddEnemy(FVector2f(300.0f, 60.0f), ToughHp);
	FDTStatusApply Burn;
	Burn.Status = EDTStatus::Burn;
	Burn.OwnerId = 0;
	Burn.Magnitude = 10.0f;
	Burn.Handle = FDTEnemyHandle{ Near, H.Enemies.GetSerials()[Near] };
	H.Enemies.ApplyStatus(Burn);
	Burn.Handle = FDTEnemyHandle{ Side, H.Enemies.GetSerials()[Side] };
	H.Enemies.ApplyStatus(Burn);
	H.StepMany(FireSteps);
	TestTrue(TEXT("a Pull landing on a Burning enemy is Steam"), H.Weapons.GetReactionCounters(0, RowOf(EReaction::ForgeTide)).Fires > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaCooldown, "Dusktracer.Reactions.Cooldown", DTReactionTest::Flags) // T-REA-05
bool FDTReaCooldown::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormForge);
	const int32 A = S.Add(Far);
	const int32 B = S.Add(Far + FVector2f(2000.0f, 0.0f));
	S.Give(A, EDTStatus::Burn, 10.0f);
	S.Give(B, EDTStatus::Burn, 10.0f);
	S.Hit(A, Src(EDTPath::Storm), 10.0f);
	S.Hit(A, Src(EDTPath::Storm), 10.0f);
	S.StepOnce();
	S.Hit(A, Src(EDTPath::Storm), 10.0f);
	S.StepOnce();
	TestEqual(TEXT("one fire per enemy per cooldown"), S.Fires(), 1ll);
	S.Hit(B, Src(EDTPath::Storm), 10.0f);
	S.StepOnce();
	TestEqual(TEXT("another enemy fires at once"), S.Fires(), 2ll);
	S.StepMany(FMath::CeilToInt32(DTConst::REACTION_ENEMY_COOLDOWN_S * Second));
	S.Give(A, EDTStatus::Burn, 10.0f);
	S.Hit(A, Src(EDTPath::Storm), 10.0f);
	S.StepOnce();
	TestEqual(TEXT("the cooldown runs out"), S.Fires(), 3ll);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaDeterministic, "Dusktracer.Reactions.Deterministic", DTReactionTest::Flags) // T-REA-06
bool FDTReaDeterministic::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Run = [](int64& OutFires)
	{
		FDuoScene S(EReaction::ForgeTide);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const int32 Enemy = S.Add(Far + FVector2f(70.0f * Index, 0.0f));
			S.Give(Enemy, EDTStatus::Burn, 5.0f);
			S.Give(Enemy, EDTStatus::Chill, 1.0f);
			S.Hit(Enemy, Src(EDTPath::Forge), 40.0f);
		}
		S.StepMany(Second);
		OutFires = S.Fires();
		return S.Damage(S.Duo());
	};
	int64 FiresA = 0;
	int64 FiresB = 0;
	const double A = Run(FiresA);
	const double B = Run(FiresB);
	TestTrue(TEXT("it fired"), FiresA > 0);
	TestEqual(TEXT("same fires"), FiresA, FiresB);
	TestEqual(TEXT("same damage"), A, B);
	return true;
}

// ---- the fifteen (T-REA-07) ----------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaPlasma, "Dusktracer.Reactions.Pair.StormForge", DTReactionTest::Flags) // T-REA-07
bool FDTReaPlasma::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormForge);
	const int32 Struck = S.Add(Far);
	const int32 Near = S.Add(Far + FVector2f(100.0f, 0.0f));
	const int32 Out = S.Add(Far + FVector2f(500.0f, 0.0f));
	S.Hit(Struck, Src(EDTPath::Storm), 100.0f);
	S.StepMany(3);
	TestEqual(TEXT("needs Burn"), S.Fires(), 0ll);
	S.Give(Struck, EDTStatus::Burn, 10.0f);
	S.Hit(Struck, Src(EDTPath::Storm), 100.0f);
	S.StepMany(3);
	TestEqual(TEXT("fires"), S.Fires(), 1ll);
	TestEqual(TEXT("the burst strikes in reach"), S.HitsOn(Near, S.Duo()), 1);
	TestEqual(TEXT("not outside"), S.HitsOn(Out, S.Duo()), 0);
	TestEqual(TEXT("60% of the hit"), S.Damage(S.Duo()), 2.0 * 100.0 * DTReactions::Param(S.Row(), TEXT("dmg")), 0.01);
	TestTrue(TEXT("the burst Burns what it strikes"), S.Has(Near, EDTStatus::Burn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaConduction, "Dusktracer.Reactions.Pair.StormTide", DTReactionTest::Flags) // T-REA-07
bool FDTReaConduction::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormTide);
	const int32 Struck = S.Add(Far);
	S.Give(Struck, EDTStatus::Chill, 1.0f);
	for (int32 Index = 1; Index <= 8; ++Index)
	{
		S.Give(S.Add(Far + FVector2f(30.0f * Index, 0.0f)), EDTStatus::Chill, 1.0f);
	}
	const int32 Warm = S.Add(Far + FVector2f(0.0f, 60.0f));
	S.Hit(Struck, Src(EDTPath::Storm), 100.0f);
	S.StepMany(3);
	TestEqual(TEXT("fires"), S.Fires(), 1ll);
	TestEqual(TEXT("up to max others at the hit's full damage"), S.Damage(S.Duo()), 100.0 * DTReactions::Param(S.Row(), TEXT("max")), 0.01);
	TestEqual(TEXT("not the struck one"), S.HitsOn(Struck, S.Duo()), 0);
	TestEqual(TEXT("only the chilled"), S.HitsOn(Warm, S.Duo()), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaWrit, "Dusktracer.Reactions.Pair.StormShade", DTReactionTest::Flags) // T-REA-07
bool FDTReaWrit::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormShade);
	const int32 Dying = S.Add(Far, 0.01f);
	const int32 Near = S.Add(Far + FVector2f(100.0f, 0.0f));
	const int32 Mid = S.Add(Far + FVector2f(200.0f, 0.0f));
	const int32 Further = S.Add(Far + FVector2f(300.0f, 0.0f));
	const uint32 NearSerial = S.Serial(Near);
	const uint32 MidSerial = S.Serial(Mid);
	const uint32 FurtherSerial = S.Serial(Further);
	S.Give(Dying, EDTStatus::Shock);
	S.Mark(Dying);
	S.Mark(Near);
	S.Mark(Mid);
	S.Mark(Further);
	S.Hit(Dying, Src(EDTPath::Shade), 1.0e6f);
	S.StepMany(3);
	TestEqual(TEXT("a Shocked Marked death fires"), S.Fires(), 1ll);
	const auto On = [&S](uint32 Serial) { return S.HitLog.FilterByPredicate([&](const FDTHit& H) { return H.EnemySerial == Serial && H.SourceId == S.Duo(); }).Num(); };
	TestEqual(TEXT("the nearest"), On(NearSerial), 1);
	TestEqual(TEXT("and the next"), On(MidSerial), 1);
	TestEqual(TEXT("two bolts only"), On(FurtherSerial), 0);
	TestEqual(TEXT("bolt damage"), S.Damage(S.Duo()), 2.0 * S.Strongest() * DTReactions::Param(S.Row(), TEXT("dmg")), 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaFlashArc, "Dusktracer.Reactions.Pair.StormRadiance", DTReactionTest::Flags) // T-REA-07
bool FDTReaFlashArc::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormRadiance);
	const int32 Struck = S.Add(Far);
	TArray<int32> Others;
	for (int32 Index = 1; Index <= 5; ++Index)
	{
		Others.Add(S.Add(Far + FVector2f(50.0f * Index, 0.0f)));
	}
	S.Give(Struck, EDTStatus::Shock);
	S.Hit(Struck, Src(EDTPath::Storm), 100.0f, false);
	S.StepMany(2);
	TestEqual(TEXT("needs a crit"), S.Fires(), 0ll);
	S.Hit(Struck, S.Rifle(), 100.0f, true);
	S.StepOnce();
	TestEqual(TEXT("any of the owner's crits fires"), S.Fires(), 1ll);
	TestTrue(TEXT("the arc's target is Blinded"), S.Weapons.IsBlinded(0, S.Serial(Others[0])));
	TestFalse(TEXT("a fourth is not"), S.Weapons.IsBlinded(0, S.Serial(Others[3])));
	S.StepMany(2);
	TestEqual(TEXT("three arcs at half the hit"), S.Damage(S.Duo()), 3.0 * 100.0 * DTReactions::Param(S.Row(), TEXT("dmg")), 0.01);
	TestTrue(TEXT("the Blind stuns"), S.Enemies.GetAux(Others[0]).IsFrozen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaLiveRail, "Dusktracer.Reactions.Pair.StormQuicksilver", DTReactionTest::Flags) // T-REA-07
bool FDTReaLiveRail::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	{
		// No ricochet in this scene: a rifle round's bounce would claim the same enemy first.
		FDuoScene S(EReaction::StormQuicksilver, false, true, 7, TEXT("Quicksilver.Momentum"));
		const int32 Crossed = S.Add(Far);
		const int32 Near = S.Add(Far + FVector2f(120.0f, 0.0f));
		S.Dash(Far);
		TestEqual(TEXT("a dash through an unshocked enemy does nothing"), S.Fires(), 0ll);
		S.View.HeroPositions[0] = FVector2f::ZeroVector; // out of the rifle's reach, so no enchantment Shocks them
		S.StepMany(2 * Second);
		S.Give(Crossed, EDTStatus::Shock);
		S.Dash(Far);
		TestTrue(TEXT("a dash through a Shocked enemy discharges"), S.Fires() >= 1);
		TestTrue(TEXT("the burst reaches its neighbour"), S.HitsOn(Near, S.Duo()) >= 1);
		TestTrue(TEXT("the Shock is spent"), S.StatusLog.ContainsByPredicate([](const FDTStatusApply& A) { return A.bConsume && A.Status == EDTStatus::Shock; }));
	}
	{
		FDuoScene S(EReaction::StormQuicksilver);
		for (int32 Index = 0; Index < 6; ++Index)
		{
			const int32 Enemy = S.Add(FVector2f(300.0f + 70.0f * Index, 0.0f));
			S.Give(Enemy, EDTStatus::Shock);
		}
		S.StepMany(FireSteps);
		TestTrue(TEXT("a ricochet off a Shocked enemy arcs"), S.Fires() > 0 && S.Damage(S.Duo()) > 0.0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaSteam, "Dusktracer.Reactions.Pair.ForgeTide", DTReactionTest::Flags) // T-REA-07
bool FDTReaSteam::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::ForgeTide);
	const int32 Struck = S.Add(Far);
	const int32 Near = S.Add(Far + FVector2f(120.0f, 0.0f));
	S.Give(Struck, EDTStatus::Burn, 10.0f);
	S.Hit(Struck, 1, 50.0f);
	S.StepMany(2);
	TestEqual(TEXT("Burn alone is no Steam"), S.Fires(), 0ll);
	S.Give(Struck, EDTStatus::Chill, 1.0f);
	S.Hit(Struck, Src(EDTPath::Tide), 100.0f);
	S.StepMany(3);
	TestEqual(TEXT("either way round, any hit"), S.Fires(), 1ll);
	TestEqual(TEXT("the blast is its dmg of the hit"), S.Damage(S.Duo()), 2.0 * 100.0 * DTReactions::Param(S.Row(), TEXT("dmg")), 0.01);
	TestFalse(TEXT("Burn is spent"), S.Has(Struck, EDTStatus::Burn));
	TestFalse(TEXT("Chill is spent"), S.Has(Struck, EDTStatus::Chill));
	TestTrue(TEXT("what it strikes is staggered"), S.Enemies.GetAux(Near).IsFrozen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaCremation, "Dusktracer.Reactions.Pair.ForgeShade", DTReactionTest::Flags) // T-REA-07
bool FDTReaCremation::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	{
		FDuoScene S(EReaction::ForgeShade);
		const int32 Dying = S.Add(Far, 0.01f);
		const int32 Near = S.Add(Far + FVector2f(200.0f, 0.0f));
		const uint32 NearSerial = S.Serial(Near);
		S.Give(Dying, EDTStatus::Burn, 10.0f);
		S.Mark(Dying);
		S.Hit(Dying, Src(EDTPath::Forge), 1.0e6f);
		S.StepMany(Second);
		TestEqual(TEXT("a Burning Marked death fires"), S.Fires(), 1ll);
		TestTrue(TEXT("the ember reaches the nearest"), S.HitLog.ContainsByPredicate([&](const FDTHit& H) { return H.EnemySerial == NearSerial && H.SourceId == S.Duo(); }));
		TestTrue(TEXT("and Burns it"), S.CountStatuses(EDTStatus::Burn, S.Duo()) > 0);
	}
	{
		FDuoScene S(EReaction::ForgeShade);
		const int32 Burning = S.Add(Far, 0.18f);
		const int32 Plain = S.Add(Far + FVector2f(500.0f, 0.0f), 0.18f);
		const uint32 BurningSerial = S.Serial(Burning);
		const uint32 PlainSerial = S.Serial(Plain);
		S.Give(Burning, EDTStatus::Burn, 0.001f);
		S.Mark(Burning);
		S.Mark(Plain);
		S.StepOnce();
		S.Enemies.Publish(S.View);
		const int32 Executed = S.Weapons.ExecuteMarked(S.Context(), 0, 0.15f, Src(EDTPath::Shade));
		TestEqual(TEXT("the threshold is execute_add higher on the Burning one only"), Executed, 1);
		TestTrue(TEXT("that one"), S.View.Hits.ContainsByPredicate([&](const FDTHit& H) { return H.EnemySerial == BurningSerial; }));
		TestFalse(TEXT("not the plain one"), S.View.Hits.ContainsByPredicate([&](const FDTHit& H) { return H.EnemySerial == PlainSerial; }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaFlare, "Dusktracer.Reactions.Pair.ForgeRadiance", DTReactionTest::Flags) // T-REA-07
bool FDTReaFlare::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::ForgeRadiance);
	const int32 Struck = S.Add(Far);
	S.Give(Struck, EDTStatus::Burn, 10.0f);
	S.Hit(Struck, Src(EDTPath::Forge), 5.0f, false);
	S.StepMany(2);
	TestEqual(TEXT("needs a crit"), S.Fires(), 0ll);
	const FDTEnemyAux& Aux = S.Enemies.GetAux(Struck);
	float Owed = 0.0f;
	for (const FDTBurnStack& Stack : Aux.Burn)
	{
		Owed += Stack.Left > 0.0f ? Stack.Left * Stack.Dps : 0.0f;
	}
	S.Hit(Struck, Src(EDTPath::Storm), 5.0f, true);
	S.StepMany(3);
	TestEqual(TEXT("fires"), S.Fires(), 1ll);
	TestEqual(TEXT("what the Burn owes, x mult"), static_cast<float>(S.Damage(S.Duo())), Owed * DTReactions::Param(S.Row(), TEXT("mult")), 3.0f);
	TestFalse(TEXT("the Burn is spent"), S.Has(Struck, EDTStatus::Burn));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaWildfire, "Dusktracer.Reactions.Pair.ForgeQuicksilver", DTReactionTest::Flags) // T-REA-07
bool FDTReaWildfire::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	{
		FDuoScene S(EReaction::ForgeQuicksilver);
		S.Bless(Blessing(TEXT("Quicksilver.Momentum"), TEXT("Key")));
		const int32 Struck = S.Add(Far);
		S.Hit(Struck, S.Rifle(), 10.0f, false, 0);
		S.StepMany(3);
		TestFalse(TEXT("no speed, no fire"), S.Has(Struck, EDTStatus::Burn));
		S.Weapons.GrantMomentum(0, 100.0f, true);
		S.StepMany(2);
		TestEqual(TEXT("bonus speed starts it"), S.Fires(), 1ll);
		S.Hit(Struck, S.Rifle(), 10.0f, false, 0);
		S.StepMany(3);
		TestTrue(TEXT("a weapon hit Burns"), S.Has(Struck, EDTStatus::Burn));
	}
	{
		FDuoScene S(EReaction::ForgeQuicksilver);
		S.Bless(Blessing(TEXT("Forge.Furnace"), TEXT("Key")));
		const FVector2f At(300.0f, 0.0f);
		const int32 Struck = S.Add(At);
		S.Give(Struck, EDTStatus::Burn, 1.0f);
		for (int32 Step = 0; Step < 2 * Second && S.Weapons.GetLiveFields() == 0; ++Step)
		{
			S.StepOnce();
		}
		TestTrue(TEXT("a field stands"), S.Weapons.GetLiveFields() > 0);
		S.Dash(At);
		TestTrue(TEXT("a dash across it starts Wildfire"), S.Weapons.GetWildfireLeft(0) > 0.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaDeepLedger, "Dusktracer.Reactions.Pair.TideShade", DTReactionTest::Flags) // T-REA-07
bool FDTReaDeepLedger::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::TideShade);
	const int32 Weak = S.Add(Far, 0.2f);
	const int32 Strong = S.Add(Far + FVector2f(0.0f, 2000.0f), 0.5f);
	const int32 Near = S.Add(Far + FVector2f(100.0f, 0.0f));
	const uint32 WeakSerial = S.Serial(Weak);
	const uint32 StrongSerial = S.Serial(Strong);
	const uint32 NearSerial = S.Serial(Near);
	S.Give(Weak, EDTStatus::Freeze);
	S.Give(Strong, EDTStatus::Freeze);
	S.Mark(Weak);
	S.Mark(Strong);
	S.Hit(Strong, 1, 1.0f);
	S.StepMany(3);
	TestEqual(TEXT("too healthy"), S.Fires(), 0ll);
	S.Hit(Weak, 1, 1.0f);
	S.StepMany(3);
	TestEqual(TEXT("fires at or below execute_below, with no execute of its own"), S.Fires(), 1ll);
	TestFalse(TEXT("executed"), S.Enemies.GetSerials().Contains(WeakSerial));
	TestTrue(TEXT("the other lives"), S.Enemies.GetSerials().Contains(StrongSerial));
	TestTrue(TEXT("it shatters onto its neighbour"), S.HitLog.ContainsByPredicate([&](const FDTHit& H) { return H.EnemySerial == NearSerial && H.SourceId == S.Duo(); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaRefraction, "Dusktracer.Reactions.Pair.TideRadiance", DTReactionTest::Flags) // T-REA-07
bool FDTReaRefraction::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::TideRadiance);
	const int32 Struck = S.Add(Far);
	S.Give(Struck, EDTStatus::Freeze);
	S.Hit(Struck, 1, 50.0f, false);
	S.StepMany(2);
	TestEqual(TEXT("needs a crit"), S.Fires(), 0ll);
	const int32 Before = S.Weapons.GetTrackedRounds();
	S.Hit(Struck, 1, 50.0f, true);
	S.StepOnce();
	TestEqual(TEXT("fires"), S.Fires(), 1ll);
	TestEqual(TEXT("shards fly"), S.Weapons.GetTrackedRounds() - Before, FMath::RoundToInt32(DTReactions::Param(S.Row(), TEXT("shards"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaBlackIce, "Dusktracer.Reactions.Pair.TideQuicksilver", DTReactionTest::Flags) // T-REA-07
bool FDTReaBlackIce::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::TideQuicksilver);
	const int32 Crossed = S.Add(Far);
	S.Dash(Far);
	TestFalse(TEXT("no Chill, no Freeze"), S.Enemies.GetAux(Crossed).IsFrozen());
	S.Give(Crossed, EDTStatus::Chill, 1.0f);
	S.StepMany(Second + 2);
	S.Dash(Far);
	TestTrue(TEXT("the dash Freezes the chilled"), S.Enemies.GetAux(Crossed).IsFrozen());

	const auto Bounces = [](bool bFrozen)
	{
		FDuoScene R(EReaction::TideQuicksilver);
		TArray<int32> Crowd;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Crowd.Add(R.Add(FVector2f(300.0f + 70.0f * Index, 0.0f)));
		}
		for (int32 Step = 0; Step < FireSteps; ++Step)
		{
			for (const int32 Enemy : Crowd)
			{
				if (bFrozen && Step % (Second / 2) == 0)
				{
					R.Give(Enemy, EDTStatus::Freeze);
				}
			}
			R.StepOnce();
		}
		return R.CountHits(Src(EDTPath::Quicksilver));
	};
	TestTrue(TEXT("a ricochet off a Frozen enemy spends no bounce"), Bounces(true) > Bounces(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaExposure, "Dusktracer.Reactions.Pair.ShadeRadiance", DTReactionTest::Flags) // T-REA-07
bool FDTReaExposure::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::ShadeRadiance);
	const int32 Exposed = S.Add(Far, 0.5f);
	const int32 Near = S.Add(Far + FVector2f(150.0f, 0.0f));
	const int32 Only = S.Add(Far + FVector2f(0.0f, 2000.0f));
	S.Mark(Exposed);
	S.Blind(Exposed);
	S.Mark(Only);
	S.Hit(Only, 1, 100.0f);
	S.StepMany(2);
	TestEqual(TEXT("Marked alone"), S.Fires(), 0ll);
	S.Hit(Exposed, 1, 100.0f);
	S.StepMany(2);
	TestEqual(TEXT("fires"), S.Fires(), 1ll);
	TestEqual(TEXT("it takes vuln more"), S.Damage(S.Duo()), 100.0 * DTReactions::Param(S.Row(), TEXT("vuln")), 0.01);
	S.Hit(Exposed, 1, 100.0f);
	S.StepMany(2);
	TestEqual(TEXT("the added damage is a state: it runs on every hit"), S.Damage(S.Duo()), 2.0 * 100.0 * DTReactions::Param(S.Row(), TEXT("vuln")), 0.01);
	TestEqual(TEXT("its count obeys the cooldown"), S.Fires(), 1ll);
	const uint32 NearSerial = S.Serial(Near);
	S.Hit(Exposed, 1, 1.0e7f);
	S.StepMany(3);
	TestTrue(TEXT("its death Marks the neighbour"), S.Weapons.IsMarked(0, NearSerial));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaRepossession, "Dusktracer.Reactions.Pair.ShadeQuicksilver", DTReactionTest::Flags) // T-REA-07
bool FDTReaRepossession::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::ShadeQuicksilver);
	const int32 Strong = S.Add(Far, 0.5f);
	S.Mark(Strong);
	S.Dash(Far);
	TestEqual(TEXT("too healthy"), S.Fires(), 0ll);
	const FVector2f Other = Far + FVector2f(0.0f, 2000.0f);
	const int32 Weak = S.Add(Other, 0.2f);
	const uint32 WeakSerial = S.Serial(Weak);
	S.Mark(Weak);
	S.Dash(Other);
	TestEqual(TEXT("fires"), S.Fires(), 1ll);
	TestFalse(TEXT("executed"), S.Enemies.GetSerials().Contains(WeakSerial));
	TestEqual(TEXT("refund of the dash cooldown"), S.Weapons.TakeDashRefund(0), DTConst::DASH_COOLDOWN_S * DTReactions::Param(S.Row(), TEXT("refund")), 0.001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaAfterglow, "Dusktracer.Reactions.Pair.RadianceQuicksilver", DTReactionTest::Flags) // T-REA-07
bool FDTReaAfterglow::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::RadianceQuicksilver);
	S.Bless(Blessing(TEXT("Quicksilver.Momentum"), TEXT("Key")));
	S.StepMany(2);
	const float Base = S.Weapons.GetLastSpec(0, 0).CritChance;
	S.Weapons.GrantMomentum(0, 10.0f, true);
	S.StepMany(2);
	const FDTFlagEntry* Speed = S.Built(EDTPath::Quicksilver, DTCat::EFlag::SpeedScale);
	const float Bonus = 10.0f * Speed->Params.Get(FName(TEXT("per_stack")));
	const float Want = FMath::Min(Base + 10.0f * Bonus * DTReactions::Param(S.Row(), TEXT("crit_per_10pct")), static_cast<float>(DTConst::MOD_CRIT_CHANCE_CAP));
	TestEqual(TEXT("crit chance per tenth of bonus speed"), S.Weapons.GetLastSpec(0, 0).CritChance, Want, 0.0001f);
	const int32 Struck = S.Add(Far);
	S.Hit(Struck, Src(EDTPath::Quicksilver), 10.0f, true);
	S.StepOnce();
	TestTrue(TEXT("a Quicksilver crit Blinds"), S.Weapons.IsBlinded(0, S.Serial(Struck)));
	return true;
}

// ---- offers ----------------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaOffer, "Dusktracer.Reactions.Offer", DTReactionTest::Flags) // T-REA-08
bool FDTReaOffer::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDTKeeperHarness H;
	FDTOfferPool Pool;
	Pool.Unlocked.Add(DTReactions::UnlockId());
	FDTPlayerRunState& P = H.P();
	P.Offer.PatronPath = EDTPath::Storm;
	P.Keepers.Primary = EDTPath::Storm;
	TestEqual(TEXT("not before the commitment"), Duos(P, Pool).Num(), 0);
	P.Offer.SecondPath = EDTPath::Tide;
	P.Keepers.Second = EDTPath::Tide;
	const TArray<FDTOfferCard> Cards = Duos(P, Pool);
	TestEqual(TEXT("the pair's card"), Cards.Num(), 1);
	const FName Id(TEXT("Blessing.Duo.StormTide"));
	if (Cards.Num() == 1)
	{
		TestEqual(TEXT("its id"), Cards[0].ContentId, Id);
		TestEqual(TEXT("a blessing card"), Cards[0].Kind, EDTCardKind::Blessing);
		TestEqual(TEXT("weighted"), Cards[0].Weight, DTConst::OFFER_WEIGHT_BASE * DTConst::OFFER_WEIGHT_DUO, 0.001f);
	}
	FRandomStream Rng(3);
	bool bSeen = false;
	for (int32 Try = 0; Try < 400; ++Try)
	{
		const FDTOfferHand Hand = DTOffers::BuildHand(P, Pool, EDTHandKind::Level, 9, Rng);
		for (const FDTOfferCard& Card : Hand.Cards)
		{
			if (Card.ContentId == Id)
			{
				bSeen = true;
				TestTrue(TEXT("Rare or better"), Card.Rarity != EDTRarity::Common);
			}
		}
	}
	TestTrue(TEXT("it reaches hands"), bSeen);
	FDTPlayerRunState Banished = P;
	Banished.Offer.Banished.Add(Id);
	TestEqual(TEXT("banish takes it"), Duos(Banished, Pool).Num(), 0);
	TestTrue(TEXT("it can be taken"), DTOffers::AddBlessing(P.Keepers, Id));
	TestEqual(TEXT("once per run"), Duos(P, Pool).Num(), 0);
	TestFalse(TEXT("never twice"), DTOffers::AddBlessing(P.Keepers, Id));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaFavor, "Dusktracer.Reactions.Favor", DTReactionTest::Flags) // T-REA-09
bool FDTReaFavor::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDTKeeperHarness H;
	H.Patron(EDTPath::Storm);
	H.Commit(EDTPath::Forge);
	H.Bless(Blessing(TEXT("Storm.Tempest"), TEXT("Key")));
	FDTKeeperRunState& K = H.P().Keepers;
	const FName Leading = K.Leading[static_cast<int32>(EDTPath::Storm)];
	const int32 Storm = K.Favor[static_cast<int32>(EDTPath::Storm)];
	const int32 Forge = K.Favor[static_cast<int32>(EDTPath::Forge)];
	TestTrue(TEXT("taken"), H.Bless(FName(TEXT("Blessing.Duo.StormForge"))));
	TestEqual(TEXT("Favor to the first"), K.Favor[static_cast<int32>(EDTPath::Storm)], Storm + DTConst::TIER_FAVOR_PER_BLESSING);
	TestEqual(TEXT("Favor to the second"), K.Favor[static_cast<int32>(EDTPath::Forge)], Forge + DTConst::TIER_FAVOR_PER_BLESSING);
	TestEqual(TEXT("not to the third"), K.Favor[static_cast<int32>(EDTPath::Tide)], 0);
	TestEqual(TEXT("no leading archetype moves"), K.Leading[static_cast<int32>(EDTPath::Storm)], Leading);
	TestTrue(TEXT("no leading archetype appears"), K.Leading[static_cast<int32>(EDTPath::Forge)].IsNone());
	H.Bless(Blessing(TEXT("Forge.Hammer"), TEXT("Key")));
	TestEqual(TEXT("later cards count it"), K.Favor[static_cast<int32>(EDTPath::Forge)], Forge + 2 * DTConst::TIER_FAVOR_PER_BLESSING);
	return true;
}

// ---- the duo blessings (T-REA-10) ----------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoArcFurnace, "Dusktracer.Reactions.Duo.StormForge", DTReactionTest::Flags) // T-REA-10
bool FDTDuoArcFurnace::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Fields = [](bool bDuo)
	{
		FDuoScene S(EReaction::StormForge, bDuo);
		const int32 Struck = S.Add(Far);
		S.Give(Struck, EDTStatus::Burn, 10.0f);
		S.Hit(Struck, Src(EDTPath::Storm), 100.0f);
		S.StepMany(2);
		return S.Weapons.GetLiveFields();
	};
	TestEqual(TEXT("no card, no ground"), Fields(false), 0);
	TestEqual(TEXT("the burst leaves a field"), Fields(true), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoStormSurge, "Dusktracer.Reactions.Duo.StormTide", DTReactionTest::Flags) // T-REA-10
bool FDTDuoStormSurge::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormTide, true);
	const int32 Struck = S.Add(Far);
	S.Give(Struck, EDTStatus::Chill, 1.0f);
	TArray<int32> Crowd;
	for (int32 Index = 1; Index <= 9; ++Index)
	{
		Crowd.Add(S.Add(Far + FVector2f(30.0f * Index, 0.0f)));
		S.Give(Crowd.Last(), EDTStatus::Chill, 1.0f);
	}
	S.Hit(Struck, Src(EDTPath::Storm), 100.0f);
	S.StepMany(3);
	TestEqual(TEXT("no limit"), S.Damage(S.Duo()), 900.0, 0.01);
	TestEqual(TEXT("every third jump Freezes"), S.FrozenEnemies(), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoSummons, "Dusktracer.Reactions.Duo.StormShade", DTReactionTest::Flags) // T-REA-10
bool FDTDuoSummons::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Run = [](bool bDuo, bool& bMarked)
	{
		FDuoScene S(EReaction::StormShade, bDuo);
		const int32 Dying = S.Add(Far, 0.01f);
		const int32 Near = S.Add(Far + FVector2f(100.0f, 0.0f));
		const uint32 Serial = S.Serial(Near);
		S.Give(Dying, EDTStatus::Shock);
		S.Mark(Dying);
		S.Hit(Dying, Src(EDTPath::Shade), 1.0e6f);
		S.StepMany(3);
		bMarked = S.Weapons.IsMarked(0, Serial);
		return S.Damage(S.Duo());
	};
	bool bMarked = false;
	TestEqual(TEXT("a bolt needs a Marked enemy"), Run(false, bMarked), 0.0);
	TestFalse(TEXT("nothing Marked"), bMarked);
	TestTrue(TEXT("with the card it strikes the unmarked"), Run(true, bMarked) > 0.0);
	TestTrue(TEXT("and Marks it"), bMarked);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoBlindingArc, "Dusktracer.Reactions.Duo.StormRadiance", DTReactionTest::Flags) // T-REA-10
bool FDTDuoBlindingArc::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormRadiance, true);
	const int32 Struck = S.Add(Far);
	const int32 Arced = S.Add(Far + FVector2f(100.0f, 0.0f));
	S.Give(Struck, EDTStatus::Shock);
	S.Hit(Struck, 1, 100.0f, true);
	S.StepOnce();
	const uint32 Serial = S.Serial(Arced);
	S.StepMany(Second / 2 + 1);
	TestTrue(TEXT("its Blind lasts blind_s"), S.Weapons.IsBlinded(0, Serial));
	const double Before = S.Damage(S.Duo());
	S.Hit(Arced, Src(EDTPath::Storm), 100.0f);
	S.StepMany(2);
	TestEqual(TEXT("Blinded enemies take arc_vuln more from Storm hits"), S.Damage(S.Duo()) - Before, 100.0 * DTReactions::DuoParam(S.Row(), TEXT("arc_vuln")), 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoLightningStep, "Dusktracer.Reactions.Duo.StormQuicksilver", DTReactionTest::Flags) // T-REA-10
bool FDTDuoLightningStep::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormQuicksilver, true);
	S.Dash(FVector2f(0.0f, 500.0f));
	TestTrue(TEXT("a dash leaves a Shocking field"), S.Weapons.GetLiveFields() > 0);
	S.StepMany(FMath::CeilToInt32(DTReactions::DuoParam(S.Row(), TEXT("trail_s"))) * Second + 2);
	TestEqual(TEXT("it fades"), S.Weapons.GetLiveFields(), 0);
	const int32 Crossed = S.Add(Far);
	const int32 Beyond = S.Add(Far + FVector2f(250.0f, 0.0f));
	S.Give(Crossed, EDTStatus::Shock);
	S.Dash(Far);
	TestEqual(TEXT("a discharge also arcs to the nearest out of its burst"), S.HitsOn(Beyond, S.Duo()), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoPressureCooker, "Dusktracer.Reactions.Duo.ForgeTide", DTReactionTest::Flags) // T-REA-10
bool FDTDuoPressureCooker::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::ForgeTide, true);
	const int32 Struck = S.Add(Far);
	S.Add(Far + FVector2f(120.0f, 0.0f));
	S.Give(Struck, EDTStatus::Burn, 10.0f);
	S.Give(Struck, EDTStatus::Chill, 1.0f);
	S.Hit(Struck, 1, 100.0f);
	S.StepMany(3);
	TestTrue(TEXT("the blast Burns"), S.CountStatuses(EDTStatus::Burn, S.Duo()) > 0);
	TestTrue(TEXT("and Chills"), S.CountStatuses(EDTStatus::Chill, S.Duo()) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoPyreDebt, "Dusktracer.Reactions.Duo.ForgeShade", DTReactionTest::Flags) // T-REA-10
bool FDTDuoPyreDebt::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Debt = [](bool bDuo)
	{
		FDuoScene S(EReaction::ForgeShade, bDuo);
		S.Bless(Blessing(TEXT("Shade.Economy"), TEXT("Key")));
		const int32 Dying = S.Add(Far, 0.01f);
		S.Add(Far + FVector2f(200.0f, 0.0f), 0.0001f);
		S.Give(Dying, EDTStatus::Burn, 10.0f);
		S.Mark(Dying);
		S.Hit(Dying, Src(EDTPath::Forge), 1.0e6f);
		S.StepMany(Second);
		return S.Weapons.GetDebt(0);
	};
	TestEqual(TEXT("the ember's kill counts as a Marked kill"), Debt(true) - Debt(false), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoSunspot, "Dusktracer.Reactions.Duo.ForgeRadiance", DTReactionTest::Flags) // T-REA-10
bool FDTDuoSunspot::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Run = [](bool bDuo, float& OutCritMult, bool& bCrit)
	{
		FDuoScene S(EReaction::ForgeRadiance, bDuo);
		const int32 Struck = S.Add(Far);
		S.Give(Struck, EDTStatus::Burn, 10.0f);
		S.Hit(Struck, 1, 5.0f, true);
		S.StepMany(3);
		OutCritMult = S.Weapons.BestCritMult(0);
		bCrit = S.HitLog.ContainsByPredicate([&S](const FDTHit& H) { return H.SourceId == S.Duo() && H.bCrit; });
		return S.Damage(S.Duo());
	};
	float CritMult = 1.0f;
	bool bCrit = false;
	const double Plain = Run(false, CritMult, bCrit);
	TestFalse(TEXT("no crit without the card"), bCrit);
	const double Crit = Run(true, CritMult, bCrit);
	TestTrue(TEXT("a crit with it"), bCrit);
	TestEqual(TEXT("x the best crit multiplier"), Crit, Plain * CritMult, Plain * 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoAfterburner, "Dusktracer.Reactions.Duo.ForgeQuicksilver", DTReactionTest::Flags) // T-REA-10
bool FDTDuoAfterburner::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::ForgeQuicksilver, true);
	S.Dash(FVector2f(0.0f, 300.0f));
	TestTrue(TEXT("every dash starts Wildfire"), S.Weapons.GetWildfireLeft(0) > 0.0f);
	TestEqual(TEXT("no Burning enemy, no speed"), S.Weapons.GetDuoHero(0).MoveSpeedMul, 1.0f, 0.0001f);
	const int32 Near = S.Add(FVector2f(0.0f, 500.0f));
	S.Give(Near, EDTStatus::Burn, 0.1f);
	S.StepMany(2);
	TestEqual(TEXT("speed with one near"), S.Weapons.GetDuoHero(0).MoveSpeedMul, 1.0f + DTReactions::DuoParam(S.Row(), TEXT("speed")), 0.0001f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoColdStorage, "Dusktracer.Reactions.Duo.TideShade", DTReactionTest::Flags) // T-REA-10
bool FDTDuoColdStorage::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::TideShade, true);
	const int32 Weak = S.Add(Far, 0.2f);
	const int32 Near = S.Add(Far + FVector2f(100.0f, 0.0f));
	S.Give(Weak, EDTStatus::Freeze);
	S.Mark(Weak);
	S.Hit(Weak, 1, 1.0f);
	const uint32 Serial = S.Serial(Near);
	S.StepMany(3);
	const int32 Now = S.Enemies.GetSerials().Find(Serial);
	TestTrue(TEXT("an execution Freezes the neighbour"), Now != INDEX_NONE && S.Enemies.GetAux(Now).IsFrozen());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoDiamondDust, "Dusktracer.Reactions.Duo.TideRadiance", DTReactionTest::Flags) // T-REA-10
bool FDTDuoDiamondDust::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Chills = [](bool bDuo)
	{
		FDuoScene S(EReaction::TideRadiance, bDuo);
		const int32 Struck = S.Add(Far);
		for (int32 Index = 0; Index < 5; ++Index)
		{
			S.Add(Far + FVector2f(150.0f + 40.0f * Index, 40.0f * (Index - 2)));
		}
		S.Give(Struck, EDTStatus::Freeze);
		S.Hit(Struck, 1, 50.0f, true);
		S.StepMany(Second);
		return S.CountStatuses(EDTStatus::Chill, S.Duo());
	};
	TestEqual(TEXT("shards alone do not Chill"), Chills(false), 0);
	TestTrue(TEXT("with the card they do"), Chills(true) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoGlide, "Dusktracer.Reactions.Duo.TideQuicksilver", DTReactionTest::Flags) // T-REA-10
bool FDTDuoGlide::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Bounces = [](bool bDuo)
	{
		FDuoScene R(EReaction::TideQuicksilver, bDuo);
		TArray<int32> Crowd;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			Crowd.Add(R.Add(FVector2f(300.0f + 70.0f * Index, 0.0f)));
		}
		for (int32 Step = 0; Step < FireSteps; ++Step)
		{
			for (const int32 Enemy : Crowd)
			{
				if (Step % (Second / 2) == 0)
				{
					R.Give(Enemy, EDTStatus::Chill, 1.0f);
				}
			}
			R.StepOnce();
		}
		return R.CountHits(Src(EDTPath::Quicksilver));
	};
	TestTrue(TEXT("a ricochet off a Chilled enemy spends no bounce"), Bounces(true) > Bounces(false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoSpotlight, "Dusktracer.Reactions.Duo.ShadeRadiance", DTReactionTest::Flags) // T-REA-10
bool FDTDuoSpotlight::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Run = [](bool bDuo)
	{
		FDuoScene S(EReaction::ShadeRadiance, bDuo);
		const int32 Blinded = S.Add(Far);
		S.Blind(Blinded);
		S.Hit(Blinded, 1, 100.0f);
		S.StepMany(2);
		return S.Damage(S.Duo());
	};
	TestEqual(TEXT("Blinded alone, no card"), Run(false), 0.0);
	TestEqual(TEXT("Blinded alone, the card"), Run(true), 100.0 * DTReactions::DuoParam(RowOf(EReaction::ShadeRadiance), TEXT("vuln")), 0.01);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoFastCollection, "Dusktracer.Reactions.Duo.ShadeQuicksilver", DTReactionTest::Flags) // T-REA-10
bool FDTDuoFastCollection::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::ShadeQuicksilver, true);
	const int32 Weak = S.Add(Far, 0.35f);
	S.Mark(Weak);
	S.Dash(Far);
	TestEqual(TEXT("works at below"), S.Fires(), 1ll);
	TestEqual(TEXT("an execution adds a charge"), S.Weapons.GetDuoHero(0).DashCharges, FMath::RoundToInt32(DTReactions::DuoParam(S.Row(), TEXT("charges"))));
	S.StepMany(FMath::CeilToInt32(DTReactions::DuoParam(S.Row(), TEXT("charge_s"))) * Second + 1);
	TestEqual(TEXT("for charge_s"), S.Weapons.GetDuoHero(0).DashCharges, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDuoLightspeed, "Dusktracer.Reactions.Duo.RadianceQuicksilver", DTReactionTest::Flags) // T-REA-10
bool FDTDuoLightspeed::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	const auto Crit = [](bool bDuo)
	{
		FDuoScene S(EReaction::RadianceQuicksilver, bDuo);
		S.Bless(Blessing(TEXT("Quicksilver.Momentum"), TEXT("Key")));
		S.StepMany(2);
		const float Base = S.Weapons.GetLastSpec(0, 0).CritChance;
		S.Weapons.GrantMomentum(0, 3.0f, true);
		S.StepMany(2);
		// Per stack of bonus speed: the duo's Favor can raise per_stack through a tier.
		return (S.Weapons.GetLastSpec(0, 0).CritChance - Base) / S.Built(EDTPath::Quicksilver, DTCat::EFlag::SpeedScale)->Params.Get(FName(TEXT("per_stack")));
	};
	TestEqual(TEXT("the crit bonus doubles"), Crit(true), Crit(false) * DTReactions::DuoParam(RowOf(EReaction::RadianceQuicksilver), TEXT("crit_scale")), 0.0001f);
	FDuoScene S(EReaction::RadianceQuicksilver, true);
	const int32 Struck = S.Add(Far);
	S.Weapons.TakeDashRefund(0);
	S.Hit(Struck, 1, 1.0f, true);
	S.Hit(Struck, 1, 1.0f, true);
	S.StepOnce();
	TestEqual(TEXT("a crit refunds, once per cooldown"), S.Weapons.TakeDashRefund(0), DTConst::DASH_COOLDOWN_S * DTReactions::DuoParam(S.Row(), TEXT("refund")), 0.0001f);
	return true;
}

// ---- reading it -----------------------------------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaCue, "Dusktracer.Reactions.Cue", DTReactionTest::Flags) // T-REA-11
bool FDTReaCue::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormForge);
	const int32 Count = DTConst::REACTION_FX_PER_S_MAX + 5;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const int32 Enemy = S.Add(FVector2f(-3500.0f + 1000.0f * (Index % 8), 1500.0f + 600.0f * (Index / 8)));
		S.Give(Enemy, EDTStatus::Burn, 10.0f);
		S.Hit(Enemy, Src(EDTPath::Storm), 10.0f);
	}
	S.StepOnce();
	TestEqual(TEXT("every effect ran"), S.Fires(), static_cast<int64>(Count));
	S.Enemies.Publish(S.View);
	TArray<FDTWeaponVisual> Out;
	S.Weapons.GatherVisuals(S.Run, S.View, Out);
	const auto Rings = [&Out](EDTPath Path)
	{
		return Out.FilterByPredicate([Path](const FDTWeaponVisual& V) { return V.Kind == FDTWeaponVisual::EKind::Arc && V.Path == Path && V.ArcDeg >= 360.0f; }).Num();
	};
	TestEqual(TEXT("the first Keeper's rings, to the cap"), Rings(EDTPath::Storm), DTConst::REACTION_FX_PER_S_MAX);
	TestEqual(TEXT("the second's"), Rings(EDTPath::Forge), DTConst::REACTION_FX_PER_S_MAX);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaToast, "Dusktracer.Reactions.Toast", DTReactionTest::Flags) // T-REA-12
bool FDTReaToast::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FDuoScene S(EReaction::StormForge);
	TestEqual(TEXT("nothing before a fire"), S.Weapons.GetToast(0, S.Step), static_cast<int32>(INDEX_NONE));
	const int32 Enemy = S.Add(Far);
	S.Give(Enemy, EDTStatus::Burn, 10.0f);
	S.Hit(Enemy, Src(EDTPath::Storm), 10.0f);
	S.StepOnce();
	TestEqual(TEXT("the first fire toasts"), S.Weapons.GetToast(0, S.Step), S.Row());
	TArray<FDTReactionEvent> Events = S.Weapons.TakeReactionEvents();
	TestEqual(TEXT("one event"), Events.Num(), 1);
	TestTrue(TEXT("marked first"), Events.Num() == 1 && Events[0].bFirst && Events[0].Fires == 1);
	S.StepMany(FMath::CeilToInt32(DTConst::HUD_BANNER_S * Second) + 1);
	TestEqual(TEXT("it ends"), S.Weapons.GetToast(0, S.Step), static_cast<int32>(INDEX_NONE));
	S.Give(Enemy, EDTStatus::Burn, 10.0f);
	S.Hit(Enemy, Src(EDTPath::Storm), 10.0f);
	S.StepOnce();
	TestEqual(TEXT("once a run"), S.Weapons.GetToast(0, S.Step), static_cast<int32>(INDEX_NONE));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTReaText, "Dusktracer.Reactions.Text", DTReactionTest::Flags) // T-REA-12
bool FDTReaText::RunTest(const FString& Parameters)
{
	using namespace DTReactionTest;
	FString Csv;
	TestTrue(TEXT("the table exists"), FFileHelper::LoadFileToString(Csv, *FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DT/Text/ST_Reactions.csv"))));
	TArray<FString> Lines;
	Csv.ParseIntoArrayLines(Lines);
	const auto Line = [&Lines](const FString& Key)
	{
		const FString* Found = Lines.FindByPredicate([&Key](const FString& L) { return L.StartsWith(Key + TEXT(",")); });
		return Found ? *Found : FString();
	};
	for (int32 Row = 0; Row < DTCat::NumReactions; ++Row)
	{
		const DTCat::FReactionRow& R = DTCat::Reactions[Row];
		const FString Pair(R.Pair);
		for (const FString& Key : { Pair + TEXT(".Name"), Pair + TEXT(".Toast"), TEXT("Duo.") + Pair + TEXT(".Name"), TEXT("Duo.") + Pair + TEXT(".Desc") })
		{
			TestFalse(*(TEXT("row ") + Key), Line(Key).IsEmpty());
		}
		// Every {arg} of a card names a parameter of its Duo cell (with _pct or _m).
		FString Desc = Line(TEXT("Duo.") + Pair + TEXT(".Desc"));
		int32 Open = INDEX_NONE;
		while (Desc.FindChar(TEXT('{'), Open))
		{
			int32 Close = INDEX_NONE;
			if (!Desc.FindChar(TEXT('}'), Close) || Close < Open)
			{
				break;
			}
			FString Arg = Desc.Mid(Open + 1, Close - Open - 1);
			Arg.RemoveFromEnd(TEXT("_pct"));
			Arg.RemoveFromEnd(TEXT("_m"));
			TestTrue(*FString::Printf(TEXT("%s arg %s is a parameter"), *Pair, *Arg), DTReactions::DuoParam(Row, *Arg, -1.0f) != -1.0f);
			Desc.RightChopInline(Close + 1);
		}
	}
	TestFalse(TEXT("the toast format"), Line(TEXT("Toast.Format")).IsEmpty());
	TestFalse(TEXT("the plaque"), Line(TEXT("Duo.Plaque")).IsEmpty());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
