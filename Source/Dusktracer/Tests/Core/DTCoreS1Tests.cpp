// S1 core package tests: shared sim types, enemy serials, RNG append, per-player run state, generated catalogs.
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "GameplayTagContainer.h"
#include "Combat/DTBulletPool.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Core/DTTagsGen.h"
#include "Enemies/DTEnemyPool.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTCoreS1Test
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;

	static TArray<int32> Draw(FRandomStream& Stream, int32 Count)
	{
		TArray<int32> Out;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Out.Add(Stream.RandHelper(1 << 30));
		}
		return Out;
	}

	static void OneEnemyView(FDTWorldView& View, const TArray<FVector2f>& Positions, const TArray<uint32>& Serials)
	{
		View.EnemyPositions = Positions;
		View.EnemyRadii.Init(20.0f, Positions.Num());
		View.EnemySerials = Serials;
		View.EnemyHash.Build(View.EnemyPositions);
	}

	static FDTBulletSpawn Resting(int32 Pierce)
	{
		FDTBulletSpawn Spawn;
		Spawn.Radius = 5.0f;
		Spawn.Damage = 3.0f;
		Spawn.Life = 100.0f;
		Spawn.OwnerId = 0;
		Spawn.Pierce = Pierce;
		return Spawn;
	}

	struct FRecorder : IDTSimSystem
	{
		TArray<FString>* Log = nullptr;
		FString Name;
		virtual void SimStep(const FDTSimContext& Context) override { Log->Add(Name); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEnmSlotGenerations, "Dusktracer.Enemies.SlotGenerations", DTCoreS1Test::Flags) // T-ENM-23
bool FDTEnmSlotGenerations::RunTest(const FString& Parameters)
{
	using namespace DTCoreS1Test;
	FDTRunState Run;
	Run.Start(1, 1);
	FDTEnemyPool Pool;
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(0.0f, 0.0f));
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f, 0.0f));
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(200.0f, 0.0f));
	const TArray<uint32> Before(Pool.GetSerials());
	TestEqual(TEXT("one serial per enemy"), Before.Num(), 3);
	TestTrue(TEXT("serials are never zero and distinct"), Before[0] != 0 && Before[0] != Before[1] && Before[1] != Before[2] && Before[0] != Before[2]);

	const FDTEnemyHandle Old = Pool.GetHandle(0);
	TestTrue(TEXT("fresh handle is valid"), Pool.IsHandleValid(Old));

	// Kill enemy 0; compaction swaps enemy 2 into slot 0 and keeps its serial.
	FDTHit Kill;
	Kill.EnemyIndex = 0;
	Kill.EnemySerial = Before[0];
	Kill.OwnerId = 0;
	Kill.Damage = static_cast<float>(DTConst::ENM_SWARM_HP) * 2.0f;
	TestEqual(TEXT("one kill"), Pool.ApplyHits(TArray<FDTHit>{ Kill }, Run), 1);
	Pool.Compact();
	TestEqual(TEXT("two left"), Pool.Num(), 2);
	TestEqual(TEXT("the swapped enemy keeps its serial"), Pool.GetSerials()[0], Before[2]);
	TestEqual(TEXT("the untouched enemy keeps its serial"), Pool.GetSerials()[1], Before[1]);
	TestFalse(TEXT("the old handle is invalid after the swap"), Pool.IsHandleValid(Old));

	// A hit still naming the dead enemy's serial is stale and ignored.
	FDTHit Stale = Kill;
	const float HpBefore = Pool.GetHp()[0];
	TestEqual(TEXT("stale hit kills nothing"), Pool.ApplyHits(TArray<FDTHit>{ Stale }, Run), 0);
	TestEqual(TEXT("stale hit deals no damage"), Pool.GetHp()[0], HpBefore);
	FDTHit Unchecked = Stale;
	Unchecked.EnemySerial = 0;
	Unchecked.Damage = 1.0f;
	Pool.ApplyHits(TArray<FDTHit>{ Unchecked }, Run);
	TestEqual(TEXT("serial 0 is not checked (S0 callers)"), Pool.GetHp()[0], HpBefore - 1.0f);

	// A new spawn never reuses a serial, and serials survive a clear (Reset without counters).
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(300.0f, 0.0f));
	TestFalse(TEXT("a new spawn gets a new serial"), Before.Contains(Pool.GetSerials()[2]));
	const uint32 Last = Pool.GetSerials()[2];
	Pool.Reset(false);
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f::ZeroVector);
	TestTrue(TEXT("a clear keeps counting"), Pool.GetSerials()[0] > Last);

	FDTWorldView View;
	Pool.Publish(View);
	TestEqual(TEXT("serials are published"), View.EnemySerials, TArray<uint32>(Pool.GetSerials()));

	// Bullet hit memory follows the serial, not the slot: the same enemy moved to another index is not hit again,
	// a new enemy in the old slot is.
	FDTWorldView First;
	OneEnemyView(First, { FVector2f(0.0f, 0.0f) }, { 10u });
	FDTBulletPool Bullets;
	Bullets.Spawn(Resting(5));
	Bullets.Step(1.0f / DTConst::SIM_FIXED_HZ, First);
	TestEqual(TEXT("first hit"), First.Hits.Num(), 1);
	FDTWorldView Second;
	OneEnemyView(Second, { FVector2f(0.0f, 0.0f), FVector2f(0.0f, 0.0f) }, { 11u, 10u });
	Bullets.Step(1.0f / DTConst::SIM_FIXED_HZ, Second);
	if (TestEqual(TEXT("only the new enemy is hit"), Second.Hits.Num(), 1))
	{
		TestEqual(TEXT("the new enemy's slot"), Second.Hits[0].EnemyIndex, 0);
		TestEqual(TEXT("the new enemy's serial"), Second.Hits[0].EnemySerial, 11u);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreHitFractions, "Dusktracer.Core.Hit.FractionsAddUp", DTCoreS1Test::Flags)
bool FDTCoreHitFractions::RunTest(const FString& Parameters)
{
	// A field ticks a fraction of a point each step: the events must add up to what the enemy lost (R-TEL-05).
	FDTRunState Run;
	Run.Start(1, 1);
	FDTEnemyPool Pool;
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f::ZeroVector);
	Pool.SetHp(0, 100000.0f);
	FDTHit Tick;
	Tick.EnemyIndex = 0;
	Tick.OwnerId = 0;
	Tick.Damage = 0.1f;
	for (int32 Step = 0; Step < 1000; ++Step)
	{
		Pool.ApplyHits(TArray<FDTHit>{ Tick }, Run);
	}
	TestTrue(TEXT("1000 ticks of 0.1 report 100 damage"), FMath::Abs(Run.GetPlayer(0).DamageDealt - 100) <= 1);
	Tick.Damage = 40.0f;
	const int64 Before = Run.GetPlayer(0).DamageDealt;
	Pool.ApplyHits(TArray<FDTHit>{ Tick }, Run);
	TestTrue(TEXT("a whole hit still reports itself"), FMath::Abs(Run.GetPlayer(0).DamageDealt - Before - 40) <= 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreHitFields, "Dusktracer.Core.Hit.S1Fields", DTCoreS1Test::Flags)
bool FDTCoreHitFields::RunTest(const FString& Parameters)
{
	using namespace DTCoreS1Test;
	const FDTHit Default;
	TestEqual(TEXT("default serial is unchecked"), Default.EnemySerial, 0u);
	TestEqual(TEXT("default weapon slot is none"), static_cast<int32>(Default.WeaponSlot), static_cast<int32>(INDEX_NONE));
	TestFalse(TEXT("default not crit"), Default.bCrit);
	TestFalse(TEXT("default not pierce"), Default.bPierce);

	FDTWorldView View;
	OneEnemyView(View, { FVector2f(0.0f, 0.0f), FVector2f(500.0f, 0.0f) }, { 7u, 8u });
	FDTBulletPool Bullets;
	FDTBulletSpawn Piercing = Resting(3);
	Piercing.WeaponSlot = 2;
	Piercing.bCrit = true;
	Bullets.Spawn(Piercing);
	FDTBulletSpawn Single = Resting(1);
	Single.Position = FVector2f(500.0f, 0.0f);
	Bullets.Spawn(Single);
	Bullets.Step(1.0f / DTConst::SIM_FIXED_HZ, View);
	TestEqual(TEXT("two hits"), View.Hits.Num(), 2);
	for (const FDTHit& Hit : View.Hits)
	{
		if (Hit.EnemyIndex == 0)
		{
			TestEqual(TEXT("serial carried"), Hit.EnemySerial, 7u);
			TestEqual(TEXT("weapon slot carried"), static_cast<int32>(Hit.WeaponSlot), 2);
			TestTrue(TEXT("crit carried"), Hit.bCrit);
			TestTrue(TEXT("pierce above one"), Hit.bPierce);
		}
		else
		{
			TestEqual(TEXT("serial carried"), Hit.EnemySerial, 8u);
			TestEqual(TEXT("stress fire has no weapon slot"), static_cast<int32>(Hit.WeaponSlot), static_cast<int32>(INDEX_NONE));
			TestFalse(TEXT("not crit"), Hit.bCrit);
			TestFalse(TEXT("pierce one is not pierce"), Hit.bPierce);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreRngS0Seeds, "Dusktracer.Core.Rng.S0SeedsUnchanged", DTCoreS1Test::Flags)
bool FDTCoreRngS0Seeds::RunTest(const FString& Parameters)
{
	using namespace DTCoreS1Test;
	// Seeds of the S0 streams for run seed 1234, computed independently (Python splitmix64) before Director was appended.
	const int32 Expected[] = { 1786364496, -841269501, 1497403913, -1207201845, -986258746, -376620078 };
	const EDTRngStream S0[] = { EDTRngStream::Spawn, EDTRngStream::Offer, EDTRngStream::Combat, EDTRngStream::Boss, EDTRngStream::Weather, EDTRngStream::Loot };
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(S0)); ++Index)
	{
		TestEqual(*FString::Printf(TEXT("S0 stream %d keeps its index"), Index), static_cast<int32>(S0[Index]), Index);
		TestEqual(*FString::Printf(TEXT("S0 stream %d keeps its seed"), Index), FDTRngStreams::DeriveSeed(1234, S0[Index]), Expected[Index]);
	}
	TestEqual(TEXT("Director appended after Loot"), static_cast<int32>(EDTRngStream::Director), static_cast<int32>(EDTRngStream::Loot) + 1);
	TestEqual(TEXT("Director is the last stream"), static_cast<int32>(EDTRngStream::Count), static_cast<int32>(EDTRngStream::Director) + 1);
	TestEqual(TEXT("Director seed pinned"), FDTRngStreams::DeriveSeed(1234, EDTRngStream::Director), -1513086827);

	// Drawing Director never moves an S0 stream.
	FDTRngStreams A(1234);
	FDTRngStreams B(1234);
	Draw(A.Get(EDTRngStream::Director), 500);
	TestEqual(TEXT("Spawn unaffected by Director draws"), Draw(A.Get(EDTRngStream::Spawn), 16), Draw(B.Get(EDTRngStream::Spawn), 16));

	// Per-player Offer streams: one per player, independent of each other and of the shared stream.
	FDTRunState Run;
	Run.Start(1234, 2);
	FDTRunState Again;
	Again.Start(1234, 2);
	TestEqual(TEXT("same seed, same player stream"), Draw(Run.GetPlayer(1).Offer.Rng, 8), Draw(Again.GetPlayer(1).Offer.Rng, 8));
	TestNotEqual(TEXT("players have their own stream"), Draw(Run.GetPlayer(0).Offer.Rng, 8), Draw(Run.GetPlayer(1).Offer.Rng, 8));
	TestNotEqual(TEXT("player seed differs from the shared seed"), FDTRngStreams::DerivePlayerSeed(1234, EDTRngStream::Offer, 0),
		FDTRngStreams::DeriveSeed(1234, EDTRngStream::Offer));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreSimPickupsPhase, "Dusktracer.Core.Sim.PickupsBeforeDirector", DTCoreS1Test::Flags)
bool FDTCoreSimPickupsPhase::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Effects < Pickups < Director"), EDTSimPhase::Effects < EDTSimPhase::Pickups && EDTSimPhase::Pickups < EDTSimPhase::Director);
	TestEqual(TEXT("Pickups directly after Effects"), static_cast<int32>(EDTSimPhase::Pickups), static_cast<int32>(EDTSimPhase::Effects) + 1);
	TestEqual(TEXT("Director is the last phase"), static_cast<int32>(EDTSimPhase::Count), static_cast<int32>(EDTSimPhase::Director) + 1);

	UDTSimSubsystem* Sim = NewObject<UDTSimSubsystem>(GetTransientPackage());
	Sim->StartRun(3, 2);
	TArray<FString> Log;
	DTCoreS1Test::FRecorder Director, Pickups, Effects;
	Director.Log = Pickups.Log = Effects.Log = &Log;
	Director.Name = TEXT("Director");
	Pickups.Name = TEXT("Pickups");
	Effects.Name = TEXT("Effects");
	Sim->Register(EDTSimPhase::Director, &Director);
	Sim->Register(EDTSimPhase::Pickups, &Pickups);
	Sim->Register(EDTSimPhase::Effects, &Effects);
	Sim->StepNow(1);
	TestEqual(TEXT("Effects, Pickups, Director"), Log, TArray<FString>{ TEXT("Effects"), TEXT("Pickups"), TEXT("Director") });
	const FDTWorldView& View = Sim->GetView();
	TestEqual(TEXT("one aim direction per player"), View.HeroAimDirs.Num(), 2);
	TestEqual(TEXT("one intangible flag per player"), View.HeroIntangible.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreRunStateS1, "Dusktracer.Core.RunState.S1Fields", DTCoreS1Test::Flags)
bool FDTCoreRunStateS1::RunTest(const FString& Parameters)
{
	FDTRunState Run;
	Run.Start(5, 2);
	const FDTPlayerRunState& P = Run.GetPlayer(1);
	TestEqual(TEXT("level starts at one"), P.Level, 1);
	TestEqual(TEXT("no XP"), P.XP, 0);
	TestEqual(TEXT("rerolls from C.OFFER.REROLLS"), P.Offer.RerollsLeft, DTConst::OFFER_REROLLS);
	TestEqual(TEXT("banish from C.OFFER.BANISH"), P.Offer.BanishLeft, DTConst::OFFER_BANISH);
	TestEqual(TEXT("lock from C.OFFER.LOCK"), P.Offer.LockLeft, DTConst::OFFER_LOCK);
	TestFalse(TEXT("nothing locked"), P.Offer.Locked.IsSet());
	TestTrue(TEXT("no Patron yet"), P.Offer.PatronPath == EDTPath::None && P.Keepers.Primary == EDTPath::None);
	TestEqual(TEXT("no Favor"), P.Keepers.Favor[static_cast<int32>(EDTPath::Storm)], 0);

	TestTrue(TEXT("pickup routes"), Run.ApplyEvent({ EDTGameEventKind::Pickup, 1, 9 }));
	TestEqual(TEXT("pickup adds to XPTotal"), Run.GetPlayer(1).XPTotal, static_cast<int64>(9));
	TestEqual(TEXT("other player untouched"), Run.GetPlayer(0).XPTotal, static_cast<int64>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTCoreCatalogTables, "Dusktracer.Core.Catalog.Tables", DTCoreS1Test::Flags)
bool FDTCoreCatalogTables::RunTest(const FString& Parameters)
{
	int32 S1Weapons = 0, StarterWeapons = 0;
	for (const DTCat::FWeaponRow& W : DTCat::Weapons)
	{
		S1Weapons += W.Stage == 1 ? 1 : 0;
		TestEqual(*FString::Printf(TEXT("%s has eight levels"), W.Id), W.NumLevels, 8);
		for (int32 L = 0; L < W.NumLevels; ++L)
		{
			const DTCat::FWeaponLevelRow& Row = DTCat::WeaponLevels[W.FirstLevel + L];
			TestTrue(*FString::Printf(TEXT("%s L%d belongs to it"), W.Id, L + 1), Row.Level == L + 1 && &DTCat::Weapons[Row.Weapon] == &W);
		}
		StarterWeapons += W.bStarter ? 1 : 0;
		TestTrue(*FString::Printf(TEXT("%s has capabilities"), W.Id), W.NumCaps > 0 && W.CapMask != 0);
	}
	TestEqual(TEXT("five S1 weapons"), S1Weapons, 5);
	TestEqual(TEXT("ten weapons"), DTCat::NumWeapons, 10);
	TestEqual(TEXT("starter weapons = C.POOL.STARTER_WEAPONS"), StarterWeapons, DTConst::POOL_STARTER_WEAPONS);

	int32 StarterMods = 0;
	for (const DTCat::FModifierRow& M : DTCat::Modifiers)
	{
		StarterMods += M.bStarter ? 1 : 0;
	}
	TestEqual(TEXT("sixteen modifiers"), DTCat::NumModifiers, 16);
	TestEqual(TEXT("starter modifiers = C.POOL.STARTER_MODIFIERS"), StarterMods, DTConst::POOL_STARTER_MODIFIERS);
	TestEqual(TEXT("nine archetypes"), DTCat::NumArchetypes, 9);
	TestEqual(TEXT("twenty flags in flag_compat"), DTCat::NumFlags, 20);
	for (int32 F = 0; F < DTCat::NumFlags; ++F)
	{
		TestEqual(TEXT("flag_compat rows in EFlag order"), FString(DTCat::FlagCompat[F].Flag), FString(DTCat::FlagIds[F]));
	}
	TestEqual(TEXT("four S1 enemy roles"), DTCat::NumEnemies, 4);
	TestTrue(TEXT("boss attacks"), DTCat::NumBossAttacks > 0);
	TestEqual(TEXT("thirty XP levels"), DTCat::NumXpLevels, 30);
	TestTrue(TEXT("timeline rows"), DTCat::NumTimelineArena1 > 0);

	// Every cell that named a C. key holds that constant's value.
	TestTrue(TEXT("C. cells exist"), DTCat::NumConstantRefs >= 18);
	for (const DTCat::FConstantRef& Ref : DTCat::ConstantRefs)
	{
		TestEqual(*FString::Printf(TEXT("%s resolves %s"), Ref.Cell, Ref.Key), Ref.Table, Ref.Constant);
	}
	TestEqual(TEXT("Tank HP is the constant"), DTCat::Enemies[1].Hp, static_cast<float>(DTConst::ROLE_TANK_HP));

	// Every catalog id is a registered native gameplay tag.
	TArray<const TCHAR*> Ids;
	for (const DTCat::FWeaponRow& W : DTCat::Weapons) { Ids.Add(W.Id); }
	for (const DTCat::FModifierRow& M : DTCat::Modifiers) { Ids.Add(M.Id); }
	for (const DTCat::FArchetypeRow& A : DTCat::Archetypes) { Ids.Add(A.Id); Ids.Add(A.KeeperId); }
	for (const DTCat::FEnemyRow& E : DTCat::Enemies) { Ids.Add(E.RoleTag); }
	for (const TCHAR* Id : DTCat::FlagIds) { Ids.Add(Id); }
	for (const TCHAR* Id : DTCat::CapIds) { Ids.Add(Id); }
	for (const TCHAR* Id : Ids)
	{
		TestTrue(*FString::Printf(TEXT("%s is a gameplay tag"), Id), FGameplayTag::RequestGameplayTag(FName(Id), false).IsValid());
	}
	TestEqual(TEXT("native tag matches its id"), DTTag::Weapon_PulseRifle.GetTag().ToString(), FString(TEXT("Weapon.PulseRifle")));
	TestTrue(TEXT("status tags exist"), DTTag::Status_Freeze.GetTag().IsValid());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
