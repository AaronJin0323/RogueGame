// Weapon tests (02_Systems/weapons.md T-WPN-01..13).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Weapons; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Combat/DTReforge.h"
#include "Combat/DTBulletSubsystem.h"
#include "Combat/DTWeaponCatalog.h"
#include "Combat/DTWeaponCompose.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTWeaponTest
{
	using namespace DTWeaponTestHarness;

	static const FName PulseRifle(TEXT("Weapon.PulseRifle"));
	static const FName RingEmitter(TEXT("Weapon.RingEmitter"));
	static const FName OrbitalDrones(TEXT("Weapon.OrbitalDrones"));
	static const FName ArcBlade(TEXT("Weapon.ArcBlade"));
	static const FName GravityMine(TEXT("Weapon.GravityMine"));

	static FDTWeaponFlags OneFlag(DTCat::EFlag Flag, uint16 Source, EDTFlagTier Tier, TFunctionRef<void(FDTFlagParams&)> Fill)
	{
		FDTWeaponFlags Flags;
		Fill(Flags.Add(Flag, Source, Tier).Params);
		return Flags;
	}

	static bool IsS1(int32 Weapon) { return DTCat::Weapons[Weapon].NumLevels > 0; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsCatalogRows, "Dusktracer.Weapons.CatalogRows", DTWeaponTestHarness::TestFlags) // T-WPN-01
bool FDTWeaponsCatalogRows::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	int32 S1 = 0;
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		const DTCat::FWeaponRow& Row = DTCat::Weapons[Weapon];
		TestTrue(FString::Printf(TEXT("%s id is Weapon.*"), Row.Id), FString(Row.Id).StartsWith(TEXT("Weapon.")));
		TestEqual(FString::Printf(TEXT("%s found by id"), Row.Id), DTWeapons::FindWeapon(FName(Row.Id)), Weapon);
		TestNotEqual(FString::Printf(TEXT("%s pattern known"), Row.Id), DTWeapons::GetPattern(Weapon), EDTWeaponPattern::Unknown);
		TestNotEqual(FString::Printf(TEXT("%s target known"), Row.Id), DTWeapons::GetTarget(Weapon), EDTWeaponTarget::Unknown);
		if (!IsS1(Weapon))
		{
			continue;
		}
		++S1;
		TestEqual(FString::Printf(TEXT("%s has a row per level"), Row.Id), Row.NumLevels, DTConst::WEAPON_LEVEL_MAX);
		for (int32 Level = 1; Level <= DTConst::WEAPON_LEVEL_MAX; ++Level)
		{
			const DTCat::FWeaponLevelRow* L = DTWeapons::GetLevelRow(Weapon, Level);
			TestTrue(FString::Printf(TEXT("%s L%d row"), Row.Id, Level), L && L->Weapon == Weapon && L->Level == Level);
		}
		// A def from rows alone: composing level 1 with nothing else gives the row's numbers.
		FDTWeaponInstance Instance;
		Instance.Def = Weapon;
		Instance.Level = 1;
		const FDTWeaponSpec Spec = DTWeaponCompose::Compose(Instance, {}, FDTWeaponHeroStats(), 0);
		const DTCat::FWeaponLevelRow* L1 = DTWeapons::GetLevelRow(Weapon, 1);
		TestEqual(FString::Printf(TEXT("%s spec damage from the row"), Row.Id), Spec.Damage, L1->Damage);
		TestEqual(FString::Printf(TEXT("%s spec count from the row"), Row.Id), Spec.Count, L1->Count);
		TestEqual(FString::Printf(TEXT("%s spec cooldown from the row"), Row.Id), Spec.Cooldown, L1->Cooldown_s);
	}
	TestEqual(TEXT("all ten weapons have level rows"), S1, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsCaps, "Dusktracer.Weapons.Caps", DTWeaponTestHarness::TestFlags) // T-WPN-02
bool FDTWeaponsCaps::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		const DTCat::FWeaponRow& Row = DTCat::Weapons[Weapon];
		TestTrue(FString::Printf(TEXT("%s has caps"), Row.Id), Row.NumCaps > 0);
		for (int32 Index = 0; Index < Row.NumCaps; ++Index)
		{
			TestTrue(FString::Printf(TEXT("%s cap is one of the twelve"), Row.Id), Row.Caps[Index] < DTCat::ECap::Count);
		}
		TestTrue(FString::Printf(TEXT("%s: no conversion, declared caps"), Row.Id),
			DTWeapons::EffectiveCaps(Weapon, DTCat::EFlag::None, false) == DTWeapons::DeclaredCaps(Weapon));
	}
	TestEqual(TEXT("twelve cap ids"), static_cast<int32>(UE_ARRAY_COUNT(DTCat::CapIds)), static_cast<int32>(DTCat::ECap::Count));

	// Effective caps change with a conversion and only then (R-WPN-02, R-WPN-10).
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, PulseRifle, 1);
	const FDTCapList Before = H.Weapons.ComposeSlot(H.Run, 0, 0).EffectiveCaps;
	TestTrue(TEXT("pulse declared caps before"), Before == DTWeapons::DeclaredCaps(DTWeapons::FindWeapon(PulseRifle)));
	const uint16 Forge = DTWeapons::KeeperSource(EDTPath::Forge);
	H.Weapons.AddFlagSource(0, TEXT("Test.Field"), OneFlag(DTCat::EFlag::Field, Forge, EDTFlagTier::Key, [](FDTFlagParams& P) { P.Set(DTFlagParam::RadiusUu, 120.0f); }));
	TestTrue(TEXT("a non-conversion flag leaves caps"), H.Weapons.ComposeSlot(H.Run, 0, 0).EffectiveCaps == Before);
	H.Weapons.AddFlagSource(0, TEXT("Test.Hammer"), OneFlag(DTCat::EFlag::ConvertMelee, Forge, EDTFlagTier::Key, [](FDTFlagParams& P) { P.Set(DTFlagParam::Rounds, 2.0f); }));
	const FDTCapList After = H.Weapons.ComposeSlot(H.Run, 0, 0).EffectiveCaps;
	TestTrue(TEXT("conversion adds Melee"), After.Has(DTCat::ECap::Melee));
	TestFalse(TEXT("full conversion removes Projectile"), After.Has(DTCat::ECap::Projectile));
	H.Weapons.RemoveFlagSource(0, TEXT("Test.Hammer"));
	TestTrue(TEXT("caps return without the conversion"), H.Weapons.ComposeSlot(H.Run, 0, 0).EffectiveCaps == Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsLoad, "Dusktracer.Weapons.Load", DTWeaponTestHarness::TestFlags) // T-WPN-03
bool FDTWeaponsLoad::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	const FName Order[] = { PulseRifle, RingEmitter, OrbitalDrones, ArcBlade };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Order); ++Index)
	{
		TestEqual(TEXT("give within slots"), H.Weapons.Give(H.Run, 0, Order[Index], 1), EDTWeaponResult::Ok);
		// All level 1: rank follows acquisition order; damage follows the rank's factor.
		for (int32 Slot = 0; Slot <= Index; ++Slot)
		{
			const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, Slot);
			TestEqual(FString::Printf(TEXT("%d owned: slot %d rank"), Index + 1, Slot), Spec.LoadRank, Slot);
			const float Base = DTWeapons::GetLevelRow(DTWeapons::FindWeapon(Order[Slot]), 1)->Damage;
			TestEqual(FString::Printf(TEXT("%d owned: slot %d damage x factor"), Index + 1, Slot), Spec.Damage, Base * DTWeaponCompose::LoadFactor(Slot));
		}
	}
	TestEqual(TEXT("factor 1"), DTWeaponCompose::LoadFactor(0), DTConst::LOAD_FACTOR_1);
	TestEqual(TEXT("factor 4"), DTWeaponCompose::LoadFactor(3), DTConst::LOAD_FACTOR_4);
	TestEqual(TEXT("a pick beyond the slots is refused"), H.Weapons.Give(H.Run, 0, GravityMine, 1), EDTWeaponResult::NoSlot);
	TestEqual(TEXT("an owned weapon is not given twice"), H.Weapons.Give(H.Run, 0, PulseRifle, 1), EDTWeaponResult::AlreadyOwned);

	// A level-up that reorders levels reorders load: Arc Blade (acquired last) to level 3 leads.
	TestEqual(TEXT("level up"), H.Weapons.LevelUp(H.Run, 0, 3, 3), EDTWeaponResult::Ok);
	TestEqual(TEXT("leveled weapon is rank 0"), H.Weapons.ComposeSlot(H.Run, 0, 3).LoadRank, 0);
	TestEqual(TEXT("pulse drops to rank 1"), H.Weapons.ComposeSlot(H.Run, 0, 0).LoadRank, 1);
	// Ties go to the earlier weapon: Ring to level 3 ties Arc Blade; Ring was acquired first.
	H.Weapons.LevelUp(H.Run, 0, 1, 3);
	TestEqual(TEXT("tie: earlier ring leads"), H.Weapons.ComposeSlot(H.Run, 0, 1).LoadRank, 0);
	TestEqual(TEXT("tie: later arc second"), H.Weapons.ComposeSlot(H.Run, 0, 3).LoadRank, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsLevelTrack, "Dusktracer.Weapons.LevelTrack", DTWeaponTestHarness::TestFlags) // T-WPN-04
bool FDTWeaponsLevelTrack::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		if (!IsS1(Weapon))
		{
			continue;
		}
		for (int32 Level = 2; Level <= DTWeapons::GetMaxLevel(Weapon); ++Level)
		{
			const DTCat::FWeaponLevelRow& A = *DTWeapons::GetLevelRow(Weapon, Level - 1);
			const DTCat::FWeaponLevelRow& B = *DTWeapons::GetLevelRow(Weapon, Level);
			const int32 Axes = (A.Damage != B.Damage ? 1 : 0) + (A.Count != B.Count ? 1 : 0) + (A.Cooldown_s != B.Cooldown_s ? 1 : 0) +
				((A.Radius_uu != B.Radius_uu || A.AreaR_uu != B.AreaR_uu) ? 1 : 0);
			TestEqual(FString::Printf(TEXT("%s L%d differs on exactly one axis"), DTCat::Weapons[Weapon].Id, Level), Axes, 1);
			TestTrue(FString::Printf(TEXT("%s L%d: speed, range, pierce fixed"), DTCat::Weapons[Weapon].Id, Level),
				A.Speed_uu_s == B.Speed_uu_s && A.Range_uu == B.Range_uu && A.Pierce == B.Pierce);
		}
	}
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, PulseRifle, DTConst::WEAPON_LEVEL_MAX + 5);
	TestEqual(TEXT("give clamps to the cap"), H.Run.GetPlayer(0).Weapons[0].Level, DTConst::WEAPON_LEVEL_MAX);
	TestEqual(TEXT("no level past the cap"), H.Weapons.LevelUp(H.Run, 0, 0), EDTWeaponResult::MaxLevel);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsEvolutionColumn, "Dusktracer.Weapons.EvolutionColumn", DTWeaponTestHarness::TestFlags) // T-WPN-05
bool FDTWeaponsEvolutionColumn::RunTest(const FString& Parameters)
{
	// R-WPN-05: the `evolution` column names the weapon's row of reforging.csv.
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		const int32 Row = DTReforge::FindEvolve(Weapon);
		TestTrue(FString::Printf(TEXT("%s names an Evolve row"), DTCat::Weapons[Weapon].Id), Row != INDEX_NONE && DTCat::Reforges[Row].bEvolve);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsEmission, "Dusktracer.Weapons.Emission", DTWeaponTestHarness::TestFlags) // T-WPN-06
bool FDTWeaponsEmission::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	{
		// Aimed: Count rounds via Spawn, life = range / speed, toward the nearest enemy.
		FDTHarness H;
		H.AddEnemy(FVector2f(0.0f, 500.0f));
		H.Weapons.Give(H.Run, 0, PulseRifle, 1);
		H.Enemies.Publish(H.View);
		H.Weapons.StepWeapons(H.Context(), &H.Bullets);
		const DTCat::FWeaponLevelRow& L1 = *DTWeapons::GetLevelRow(DTWeapons::FindWeapon(PulseRifle), 1);
		TestEqual(TEXT("pulse: count rounds"), H.Bullets.Num(), L1.Count);
		TestEqual(TEXT("pulse: weapon source"), H.Bullets.GetSources()[0], FDTTelemetrySources::FindOrAdd(TEXT("Weapon.PulseRifle")));
		// One step moves a round by velocity x dt; it heads to the enemy (+Y). Life: it expires after range / speed.
		H.Bullets.Step(Dt, H.View);
		TestTrue(TEXT("pulse: round flies toward the target"), H.Bullets.GetPositions()[0].Y > 0.0f);
		const int32 LifeSteps = FMath::CeilToInt((L1.Range_uu / L1.Speed_uu_s) / Dt);
		FDTWorldView Empty;
		for (int32 Step = 1; Step < LifeSteps + 1; ++Step)
		{
			H.Bullets.Step(Dt, Empty);
		}
		TestEqual(TEXT("pulse: rounds expire at range / speed"), H.Bullets.Num(), 0);
	}
	{
		// Radial: fires regardless of targets, Count rounds evenly around.
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, RingEmitter, 1);
		H.StepOnce();
		TestEqual(TEXT("ring: count rounds with no enemy"), H.Bullets.Num(), DTWeapons::GetLevelRow(DTWeapons::FindWeapon(RingEmitter), 1)->Count);
	}
	{
		// Orbiter: drones fire one at a time, every cooldown / count.
		FDTHarness H;
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.Weapons.Give(H.Run, 0, OrbitalDrones, 1);
		const DTCat::FWeaponLevelRow& L1 = *DTWeapons::GetLevelRow(DTWeapons::FindWeapon(OrbitalDrones), 1);
		H.StepOnce();
		TestEqual(TEXT("drones: one bolt on the first step"), H.Bullets.GetCounters().Spawned, static_cast<int64>(1));
		const int32 Turn = FMath::CeilToInt((L1.Cooldown_s / L1.Count) / Dt);
		H.StepMany(Turn + 1);
		TestEqual(TEXT("drones: next drone after cooldown / count"), H.Bullets.GetCounters().Spawned, static_cast<int64>(2));
	}
	{
		// MeleeArc: no rounds; one FDTHit per enemy inside the arc, once per sweep; the second sweep faces back.
		FDTHarness H;
		H.bResolve = false;
		H.AddEnemy(FVector2f(100.0f, 0.0f));   // nearest: the sweep faces it
		H.AddEnemy(FVector2f(120.0f, 20.0f));  // inside the arc
		H.AddEnemy(FVector2f(0.0f, 150.0f));   // 90 degrees off: outside a 120-degree arc
		H.AddEnemy(FVector2f(-150.0f, 0.0f));  // behind
		H.Weapons.Give(H.Run, 0, ArcBlade, 1);
		H.Enemies.Publish(H.View);
		H.Weapons.StepWeapons(H.Context(), &H.Bullets);
		TestEqual(TEXT("arc: no rounds"), H.Bullets.Num(), 0);
		TestEqual(TEXT("arc L1: two enemies in the arc, one hit each"), H.View.Hits.Num(), 2);
		H.View.Hits.Reset();
		FDTHarness H5;
		H5.AddEnemy(FVector2f(100.0f, 0.0f));
		H5.AddEnemy(FVector2f(-150.0f, 0.0f));
		H5.Weapons.Give(H5.Run, 0, ArcBlade, 5); // count 2
		H5.Enemies.Publish(H5.View);
		H5.Weapons.StepWeapons(H5.Context(), &H5.Bullets);
		TestEqual(TEXT("arc L5: second sweep hits the one behind"), H5.View.Hits.Num(), 2);
	}

	// In a headless game world: the weapon system steps through the sim and adds no actor per round.
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTWeaponTestWorld")));
	WorldContext.SetCurrentWorld(World);
	World->InitializeActorsForPlay(FURL());
	World->BeginPlay();
	UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>();
	UDTBulletSubsystem* Bullets = World->GetSubsystem<UDTBulletSubsystem>();
	UDTWeaponSubsystem* Weapons = World->GetSubsystem<UDTWeaponSubsystem>();
	TestNotNull(TEXT("weapon subsystem in a game world"), Weapons);
	if (Sim && Bullets && Weapons)
	{
		int32 ActorsBefore = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++ActorsBefore;
		}
		TestEqual(TEXT("give in the world"), Weapons->Give(0, RingEmitter, 8), EDTWeaponResult::Ok);
		Sim->StepNow(2);
		Weapons->SimRender(0.0f);
		int32 ActorsAfter = 0;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			++ActorsAfter;
		}
		TestTrue(TEXT("ring rounds live in the pool"), Bullets->GetPool().Num() >= DTWeapons::GetLevelRow(DTWeapons::FindWeapon(RingEmitter), 8)->Count);
		TestEqual(TEXT("no actor per round"), ActorsAfter, ActorsBefore);
		TestEqual(TEXT("no weapon renderer without a weapon visual"), Weapons->GetRendererCount(), 0);
		Weapons->Give(0, OrbitalDrones, 1);
		Sim->StepNow(1);
		Weapons->SimRender(0.0f);
		TestEqual(TEXT("drones: one renderer for every drone, not one per drone"), Weapons->GetRendererCount(), 1);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsHitPath, "Dusktracer.Weapons.HitPath", DTWeaponTestHarness::TestFlags) // T-WPN-07
bool FDTWeaponsHitPath::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H(7, 2);
	H.View.HeroPositions[1] = FVector2f(0.0f, 1000.0f);
	const int32 Front = H.AddEnemy(FVector2f(60.0f, 0.0f), 1000.0f);
	H.AddEnemy(FVector2f(-60.0f, 0.0f), 1000.0f);
	H.AddEnemy(FVector2f(60.0f, 1000.0f), 1000.0f);
	H.Weapons.Give(H.Run, 0, PulseRifle, 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Mirror"));
	H.Weapons.Give(H.Run, 0, ArcBlade, 1);
	H.Weapons.Give(H.Run, 1, ArcBlade, 1);
	const uint16 Storm = DTWeapons::KeeperSource(EDTPath::Storm);
	H.Weapons.AddFlagSource(0, TEXT("Test.ArcGrid"), OneFlag(DTCat::EFlag::Chain, Storm, EDTFlagTier::Key, [](FDTFlagParams& P)
	{
		P.Set(DTFlagParam::Jumps, 1.0f);
		P.Set(DTFlagParam::RangeUu, 400.0f);
		P.Set(DTFlagParam::Dmg, 0.5f);
	}));

	// Weapons + Bullets, then look before ResolveHits: health unchanged, hits queued with owner, source and slot.
	FDTSimContext Ctx = H.Context();
	H.Enemies.Publish(H.View);
	H.Weapons.StepWeapons(Ctx, &H.Bullets);
	++H.Step;
	for (int32 Step = 0; Step < 3; ++Step)
	{
		H.Bullets.Step(Dt, H.View);
	}
	TestEqual(TEXT("health unchanged before ResolveHits"), H.Enemies.GetHp()[Front], 1000.0f);
	const uint16 Pulse = FDTTelemetrySources::FindOrAdd(TEXT("Weapon.PulseRifle"));
	const uint16 Arc = FDTTelemetrySources::FindOrAdd(TEXT("Weapon.ArcBlade"));
	const uint16 Mirror = FDTTelemetrySources::FindOrAdd(TEXT("Modifier.Mirror"));
	bool bPulse = false, bMirror = false, bArc0 = false, bArc1 = false;
	for (const FDTHit& Hit : H.View.Hits)
	{
		bPulse |= Hit.SourceId == Pulse && Hit.OwnerId == 0 && Hit.WeaponSlot == 0;
		bMirror |= Hit.SourceId == Mirror && Hit.OwnerId == 0 && Hit.WeaponSlot == 0;
		bArc0 |= Hit.SourceId == Arc && Hit.OwnerId == 0 && Hit.WeaponSlot == 1;
		bArc1 |= Hit.SourceId == Arc && Hit.OwnerId == 1 && Hit.WeaponSlot == 0;
		TestTrue(TEXT("every hit names a player"), Hit.OwnerId == 0 || Hit.OwnerId == 1);
	}
	TestTrue(TEXT("weapon round: Weapon.PulseRifle, slot 0"), bPulse);
	TestTrue(TEXT("mirror copy: Modifier.Mirror"), bMirror);
	TestTrue(TEXT("arc: Weapon.ArcBlade, slot 1"), bArc0);
	TestTrue(TEXT("player 1's arc: owner 1"), bArc1);

	// A Keeper flag's emission (a chain jump) carries Keeper.<Path> and the weapon slot.
	H.StepMany(4);
	bool bChain = false;
	for (const FDTHit& Hit : H.HitLog)
	{
		bChain |= Hit.SourceId == Storm && Hit.OwnerId == 0 && Hit.WeaponSlot != INDEX_NONE;
		TestFalse(TEXT("player 1 never gets player 0's Keeper"), Hit.SourceId == Storm && Hit.OwnerId == 1);
	}
	TestTrue(TEXT("chain jump: Keeper.Storm"), bChain);
	TestTrue(TEXT("health drops only in ResolveHits"), H.Enemies.GetHp()[Front] < 1000.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsFlagResolution, "Dusktracer.Weapons.FlagResolution", DTWeaponTestHarness::TestFlags) // T-WPN-08
bool FDTWeaponsFlagResolution::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		const FDTCapList Caps = DTWeapons::DeclaredCaps(Weapon);
		for (int32 Flag = 0; Flag < static_cast<int32>(DTCat::EFlag::Count); ++Flag)
		{
			const FDTFlagResolution Res = DTWeapons::ResolveFlag(Caps, static_cast<DTCat::EFlag>(Flag));
			TestTrue(FString::Printf(TEXT("%s x %s resolves"), DTCat::Weapons[Weapon].Id, DTCat::FlagIds[Flag]), Res.Compat != DTCat::ECompat::None);
			// Independent walk over the raw flag_compat row.
			DTCat::ECompat Expected = DTCat::ECompat::None;
			for (int32 Index = 0; Index < DTCat::Weapons[Weapon].NumCaps; ++Index)
			{
				const DTCat::ECompat Cell = DTCat::FlagCompat[Flag].Cells[static_cast<int32>(DTCat::Weapons[Weapon].Caps[Index])];
				if (Cell != DTCat::ECompat::None)
				{
					Expected = Cell;
					break;
				}
			}
			TestEqual(FString::Printf(TEXT("%s x %s matches the walk"), DTCat::Weapons[Weapon].Id, DTCat::FlagIds[Flag]), Res.Compat, Expected);
		}
	}
	// The S1 table of weapons.md R-WPN-08, spot checks.
	auto Res = [](FName Weapon, DTCat::EFlag Flag) { return DTWeapons::ResolveFlag(DTWeapons::DeclaredCaps(DTWeapons::FindWeapon(Weapon)), Flag).Compat; };
	TestEqual(TEXT("pulse split native"), Res(PulseRifle, DTCat::EFlag::Split), DTCat::ECompat::Native);
	TestEqual(TEXT("drones split fallback"), Res(OrbitalDrones, DTCat::EFlag::Split), DTCat::ECompat::Fallback);
	TestEqual(TEXT("arc split fallback"), Res(ArcBlade, DTCat::EFlag::Split), DTCat::ECompat::Fallback);
	TestEqual(TEXT("mine split native"), Res(GravityMine, DTCat::EFlag::Split), DTCat::ECompat::Native);
	TestEqual(TEXT("mine echo fallback"), Res(GravityMine, DTCat::EFlag::Echo), DTCat::ECompat::Fallback);
	TestEqual(TEXT("arc convert orbit fallback"), Res(ArcBlade, DTCat::EFlag::ConvertOrbit), DTCat::ECompat::Fallback);
	TestEqual(TEXT("ring field native"), Res(RingEmitter, DTCat::EFlag::Field), DTCat::ECompat::Native);

	// A fallback is expressed, not dropped: Split on the Arc Blade rings children from the hero every Nth sweep.
	FDTHarness H;
	H.AddEnemy(FVector2f(100.0f, 0.0f));
	H.Weapons.Give(H.Run, 0, ArcBlade, 1);
	H.Weapons.AddFlagSource(0, TEXT("Test.Tempest"), OneFlag(DTCat::EFlag::Split, DTWeapons::KeeperSource(EDTPath::Storm), EDTFlagTier::Key, [](FDTFlagParams& P)
	{
		P.Set(DTFlagParam::Chance, 1.0f);
		P.Set(DTFlagParam::Children, 4.0f);
	}));
	H.StepOnce();
	TestEqual(TEXT("arc + split: a ring of children"), H.Bullets.GetCounters().Spawned, static_cast<int64>(4));
	TestEqual(TEXT("children carry Keeper.Storm"), H.Bullets.GetSources()[0], DTWeapons::KeeperSource(EDTPath::Storm));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsComposeAndThrottle, "Dusktracer.Weapons.ComposeAndThrottle", DTWeaponTestHarness::TestFlags) // T-WPN-09
bool FDTWeaponsComposeAndThrottle::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	{
		// Order: level row -> modifiers -> flags -> hero stats -> load.
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, RingEmitter, 3);
		H.Weapons.Give(H.Run, 0, PulseRifle, 1);
		H.Weapons.Socket(H.Run, 0, 1, TEXT("Mod.Pierce"));
		FDTWeaponHeroStats Stats;
		Stats.DamageMult = 2.0f;
		Stats.CooldownMult = 0.5f;
		H.Weapons.SetHeroStats(0, Stats);
		const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, 1);
		const DTCat::FWeaponLevelRow& L1 = *DTWeapons::GetLevelRow(DTWeapons::FindWeapon(PulseRifle), 1);
		TestEqual(TEXT("damage = row x stats x load(rank 2)"), Spec.Damage, L1.Damage * 2.0f * DTConst::LOAD_FACTOR_2);
		TestEqual(TEXT("cooldown x stat"), Spec.Cooldown, L1.Cooldown_s * 0.5f);
		TestEqual(TEXT("pierce: row + modifier"), Spec.Pierce, L1.Pierce + FMath::RoundToInt(DTWeapons::ModifierParam(DTWeapons::FindModifier(TEXT("Mod.Pierce")), TEXT("pierce_add"))));
		TestEqual(TEXT("life = range / speed"), Spec.Life, L1.Range_uu / L1.Speed_uu_s);
	}
	{
		// Stacking: magnitudes add, ranges and times take the largest, factors multiply (one source, one `on`).
		const uint16 Forge = DTWeapons::KeeperSource(EDTPath::Forge);
		FDTFlagEntry A;
		A.Flag = DTCat::EFlag::Field;
		A.SourceId = Forge;
		A.Params.Set(DTFlagParam::RadiusUu, 120.0f);
		A.Params.Set(DTFlagParam::DurationS, 2.0f);
		A.Params.Set(DTFlagParam::Dps, 0.3f);
		A.Params.Set(DTFlagParam::DmgMult, 1.5f);
		FDTFlagEntry B = A;
		B.Params = FDTFlagParams();
		B.Params.Set(DTFlagParam::RadiusUu, 90.0f);
		B.Params.Set(DTFlagParam::DurationS, 3.0f);
		B.Params.Set(DTFlagParam::Dps, 0.1f);
		B.Params.Set(DTFlagParam::DmgMult, 2.0f);
		const TArray<FDTFlagEntry> Stacked = DTWeaponCompose::StackEntries({ A, B });
		TestEqual(TEXT("one entry per flag + source"), Stacked.Num(), 1);
		TestEqual(TEXT("range takes the largest"), Stacked[0].Params.Get(DTFlagParam::RadiusUu), 120.0f);
		TestEqual(TEXT("time takes the largest"), Stacked[0].Params.Get(DTFlagParam::DurationS), 3.0f);
		TestEqual(TEXT("magnitude adds"), Stacked[0].Params.Get(DTFlagParam::Dps), 0.4f, 1e-5f);
		TestEqual(TEXT("factor multiplies"), Stacked[0].Params.Get(DTFlagParam::DmgMult), 3.0f, 1e-5f);
		FDTFlagEntry C = B;
		C.SourceId = DTWeapons::KeeperSource(EDTPath::Tide);
		TestEqual(TEXT("another source stays its own entry"), DTWeaponCompose::StackEntries({ A, C }).Num(), 2);
		FDTFlagEntry Delta = B;
		Delta.Params = FDTFlagParams();
		Delta.Params.Set(DTFlagParam::RadiusUu, 30.0f, EDTParamOp::Add);
		TestEqual(TEXT("an explicit +x delta adds"), DTWeaponCompose::StackEntries({ A, Delta })[0].Params.Get(DTFlagParam::RadiusUu), 150.0f);
	}
	{
		// Spawn-rate cap: a Ring Emitter firing every step emits at most the cap per second and counts the rest.
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, RingEmitter, DTConst::WEAPON_LEVEL_MAX);
		FDTWeaponHeroStats Fast;
		Fast.CooldownMult = 0.0f;
		H.Weapons.SetHeroStats(0, Fast);
		const int32 Window = DTConst::SIM_FIXED_HZ;
		TArray<int64> SpawnedAt;
		for (int32 Step = 0; Step < 3 * Window; ++Step)
		{
			H.StepOnce();
			SpawnedAt.Add(H.Weapons.GetCounters(0, 0).Spawned);
		}
		int64 WorstWindow = 0;
		for (int32 Step = Window; Step < SpawnedAt.Num(); ++Step)
		{
			WorstWindow = FMath::Max(WorstWindow, SpawnedAt[Step] - SpawnedAt[Step - Window]);
		}
		const FDTWeaponCounters& C = H.Weapons.GetCounters(0, 0);
		TestTrue(TEXT("never more than the cap in any second"), WorstWindow <= DTConst::WEAPON_SPAWN_RATE_MAX_PER_S);
		TestTrue(TEXT("the rest is counted as throttled"), C.Throttled > 0);
		TestEqual(TEXT("every attempted round is spawned or throttled"), C.Spawned + C.Throttled,
			static_cast<int64>(3 * Window) * DTWeapons::GetLevelRow(DTWeapons::FindWeapon(RingEmitter), DTConst::WEAPON_LEVEL_MAX)->Count);
		TestEqual(TEXT("the pool saw exactly the spawned rounds"), H.Bullets.GetCounters().Spawned, C.Spawned);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsConversion, "Dusktracer.Weapons.Conversion", DTWeaponTestHarness::TestFlags) // T-WPN-10
bool FDTWeaponsConversion::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	const uint16 Forge = DTWeapons::KeeperSource(EDTPath::Forge);
	const uint16 Tide = DTWeapons::KeeperSource(EDTPath::Tide);
	auto Hammer = [](FDTFlagParams& P)
	{
		P.Set(DTFlagParam::Rounds, 2.0f);
		P.Set(DTFlagParam::ArcDeg, 140.0f);
		P.Set(DTFlagParam::RadiusUu, 380.0f);
		P.Set(DTFlagParam::DmgMult, 1.6f);
	};
	{
		// Two conversions: only the higher tier stays.
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, PulseRifle, 1);
		H.Weapons.AddFlagSource(0, TEXT("Test.Melee"), OneFlag(DTCat::EFlag::ConvertMelee, Forge, EDTFlagTier::Tier, Hammer));
		H.Weapons.AddFlagSource(0, TEXT("Test.Orbit"), OneFlag(DTCat::EFlag::ConvertOrbit, Tide, EDTFlagTier::Key, [](FDTFlagParams&) {}));
		const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, 0);
		TestTrue(TEXT("key ConvertOrbit beats tier ConvertMelee"), Spec.HasFlag(DTCat::EFlag::ConvertOrbit) && !Spec.HasFlag(DTCat::EFlag::ConvertMelee));
		H.Weapons.AddFlagSource(0, TEXT("Test.Melee"), OneFlag(DTCat::EFlag::ConvertMelee, Forge, EDTFlagTier::Capstone, Hammer));
		const FDTWeaponSpec Capstone = H.Weapons.ComposeSlot(H.Run, 0, 0);
		TestTrue(TEXT("capstone ConvertMelee beats key ConvertOrbit"), Capstone.HasFlag(DTCat::EFlag::ConvertMelee) && !Capstone.HasFlag(DTCat::EFlag::ConvertOrbit));
	}
	{
		// A fully converted volley is ONE arc of the summed damage x the ratio (x the flag's factor); no rounds fly.
		FDTHarness H;
		H.bResolve = false;
		H.AddEnemy(FVector2f(200.0f, 0.0f));
		H.AddEnemy(FVector2f(250.0f, 40.0f));
		H.Weapons.Give(H.Run, 0, PulseRifle, 1);
		H.Weapons.AddFlagSource(0, TEXT("Test.Hammer"), OneFlag(DTCat::EFlag::ConvertMelee, Forge, EDTFlagTier::Key, Hammer));
		H.Enemies.Publish(H.View);
		H.Weapons.StepWeapons(H.Context(), &H.Bullets);
		const DTCat::FWeaponLevelRow& L1 = *DTWeapons::GetLevelRow(DTWeapons::FindWeapon(PulseRifle), 1);
		TestEqual(TEXT("no rounds"), H.Bullets.Num(), 0);
		TestEqual(TEXT("one hit per enemy: one arc"), H.View.Hits.Num(), 2);
		const float Expected = L1.Damage * L1.Count * DTConst::WEAPON_CONVERT_MELEE_DAMAGE_RATIO * 1.6f;
		TestEqual(TEXT("arc damage"), H.View.Hits[0].Damage, Expected, 1e-3f);
		TestEqual(TEXT("a Keeper cleave carries the Keeper source (R-WPN-07 amended, D-015)"), H.View.Hits[0].SourceId, Forge);
	}
	{
		// A partial conversion keeps Projectile and the remaining rounds.
		FDTHarness H;
		H.bResolve = false;
		H.AddEnemy(FVector2f(200.0f, 0.0f));
		H.Weapons.Give(H.Run, 0, PulseRifle, 3); // count 3
		H.Weapons.AddFlagSource(0, TEXT("Test.Hammer"), OneFlag(DTCat::EFlag::ConvertMelee, Forge, EDTFlagTier::Key, [](FDTFlagParams& P) { P.Set(DTFlagParam::Rounds, 1.0f); }));
		const FDTWeaponSpec Spec = H.Weapons.ComposeSlot(H.Run, 0, 0);
		TestTrue(TEXT("partial: Projectile kept"), Spec.EffectiveCaps.Has(DTCat::ECap::Projectile));
		TestTrue(TEXT("partial: Melee added"), Spec.EffectiveCaps.Has(DTCat::ECap::Melee));
		H.Enemies.Publish(H.View);
		H.Weapons.StepWeapons(H.Context(), &H.Bullets);
		TestEqual(TEXT("partial: remaining rounds fly"), H.Bullets.Num(), 2);
		TestEqual(TEXT("partial: one arc hit"), H.View.Hits.Num(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsPalette, "Dusktracer.Weapons.Palette", DTWeaponTestHarness::TestFlags) // T-WPN-11
bool FDTWeaponsPalette::RunTest(const FString& Parameters)
{
	// Red or magenta: hue within 30 degrees of red or in the magenta band, with real saturation.
	auto IsWarnHue = [](const FColor& Token)
	{
		const FLinearColor Hsv = Token.ReinterpretAsLinear().LinearRGBToHSV(); // hue of the sRGB token as authored
		return Hsv.G > 0.25f && (Hsv.R >= 290.0f || Hsv.R < 20.0f);
	};
	TestTrue(TEXT("the rule flags enemy fire (sanity)"), IsWarnHue(DTPal::Enemy_Bullet) && IsWarnHue(DTPal::Enemy_Optic));
	TestTrue(TEXT("core is Pal.Player.Core"), DTWeaponPalette::CoreToken() == DTPal::Player_Core);
	for (int32 Bucket = 0; Bucket < DTWeaponPalette::NumBuckets(); ++Bucket)
	{
		TestFalse(FString::Printf(TEXT("bucket %d is not red or magenta"), Bucket), IsWarnHue(DTWeaponPalette::BucketToken(Bucket)));
	}
	FDTWeaponVisual Drone;
	Drone.Kind = FDTWeaponVisual::EKind::Drone;
	Drone.Path = EDTPath::Forge;
	TestEqual(TEXT("drones draw in the core"), DTWeaponPalette::BucketOf(Drone), 0);
	FDTWeaponVisual Arc;
	Arc.Kind = FDTWeaponVisual::EKind::Arc;
	TestTrue(TEXT("a neutral arc is TrailNeutral"), DTWeaponPalette::BucketToken(DTWeaponPalette::BucketOf(Arc)) == DTPal::Player_TrailNeutral);
	Arc.Path = EDTPath::Tide;
	TestTrue(TEXT("a Tide arc is Pal.Keeper.Tide"), DTWeaponPalette::BucketToken(DTWeaponPalette::BucketOf(Arc)) == DTPal::Keeper_Tide);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsDeterminism, "Dusktracer.Weapons.Determinism", DTWeaponTestHarness::TestFlags) // T-WPN-12
bool FDTWeaponsDeterminism::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	auto RunOnce = [](TArray<FDTHit>& OutHits, int64& OutSpawned)
	{
		FDTHarness H(1234);
		for (int32 Index = 0; Index < 24; ++Index)
		{
			const float Angle = 2.0f * UE_PI * Index / 24.0f;
			H.AddEnemy(FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * (150.0f + 20.0f * (Index % 5)), 400.0f);
		}
		H.Weapons.Give(H.Run, 0, PulseRifle, 2);
		H.Weapons.Give(H.Run, 0, RingEmitter, 2);
		H.Weapons.Give(H.Run, 0, OrbitalDrones, 2);
		H.Weapons.Give(H.Run, 0, ArcBlade, 2);
		H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.SplitCore"));
		FDTWeaponHeroStats Stats;
		Stats.CritChance = 0.3f;
		Stats.CritMult = 2.0f;
		H.Weapons.SetHeroStats(0, Stats);
		H.StepMany(150);
		OutHits = H.HitLog;
		OutSpawned = H.Bullets.GetCounters().Spawned;
	};
	TArray<FDTHit> A, B;
	int64 SpawnedA = 0, SpawnedB = 0;
	RunOnce(A, SpawnedA);
	RunOnce(B, SpawnedB);
	TestTrue(TEXT("hits happened"), A.Num() > 0);
	TestEqual(TEXT("same emissions"), SpawnedA, SpawnedB);
	TestEqual(TEXT("same hit count"), A.Num(), B.Num());
	bool bSame = A.Num() == B.Num();
	for (int32 Index = 0; bSame && Index < A.Num(); ++Index)
	{
		bSame = A[Index].EnemySerial == B[Index].EnemySerial && A[Index].Damage == B[Index].Damage && A[Index].SourceId == B[Index].SourceId &&
			A[Index].bCrit == B[Index].bCrit && A[Index].Position == B[Index].Position;
	}
	TestTrue(TEXT("identical hit sequences"), bSame);

	{
		// No enemy in range: a targeted weapon does not fire and keeps its cooldown ready.
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, PulseRifle, 1);
		H.StepMany(10);
		TestEqual(TEXT("no target: no rounds"), H.Bullets.GetCounters().Spawned, static_cast<int64>(0));
		TestTrue(TEXT("no target: cooldown ready"), H.Run.GetPlayer(0).Weapons[0].Cooldown <= 0.0f);
		H.AddEnemy(FVector2f(300.0f, 0.0f));
		H.StepOnce();
		TestTrue(TEXT("fires on the step a target appears"), H.Bullets.GetCounters().Spawned > 0);
	}
	{
		// Manual aim: a Nearest weapon fires along HeroAimDirs whenever ready, with or without a target.
		FDTHarness H;
		H.View.HeroAimDirs[0] = FVector2f(0.0f, 1.0f);
		H.Weapons.SetAimManual(0, true);
		H.Weapons.Give(H.Run, 0, PulseRifle, 1);
		H.StepOnce();
		TestTrue(TEXT("manual: fired without a target"), H.Bullets.Num() > 0);
		TestTrue(TEXT("manual: along the aim"), H.Bullets.Num() > 0 && H.Bullets.GetPositions()[0].Y > 0.0f && FMath::Abs(H.Bullets.GetPositions()[0].X) < 30.0f);
	}
	{
		// Crit rate over a seeded sample matches the composed chance (one roll per emission: here one sweep per step).
		FDTHarness H(99);
		H.bResolve = false;
		H.AddEnemy(FVector2f(100.0f, 0.0f));
		H.Weapons.Give(H.Run, 0, ArcBlade, 1);
		FDTWeaponHeroStats Stats;
		Stats.CritChance = 0.3f;
		Stats.CritMult = 2.0f;
		Stats.CooldownMult = 0.0f;
		H.Weapons.SetHeroStats(0, Stats);
		H.StepMany(3000);
		int32 Crits = 0, Total = 0;
		const uint16 Arc = FDTTelemetrySources::FindOrAdd(TEXT("Weapon.ArcBlade"));
		for (const FDTHit& Hit : H.HitLog)
		{
			if (Hit.SourceId == Arc)
			{
				++Total;
				Crits += Hit.bCrit ? 1 : 0;
			}
		}
		const float Rate = Total > 0 ? static_cast<float>(Crits) / Total : 0.0f;
		TestTrue(FString::Printf(TEXT("crit rate %.3f near 0.3 over %d emissions"), Rate, Total), Total > 1000 && FMath::Abs(Rate - 0.3f) < 0.04f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTWeaponsMine, "Dusktracer.Weapons.Mine", DTWeaponTestHarness::TestFlags) // T-WPN-13
bool FDTWeaponsMine::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	H.bResolve = false;
	TArray<FVector2f> Start;
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const FVector2f P(290.0f + 5.0f * Index, 20.0f);
		H.AddEnemy(P);
		Start.Add(P);
	}
	H.Weapons.Give(H.Run, 0, GravityMine, 1);
	const DTCat::FWeaponLevelRow& L1 = *DTWeapons::GetLevelRow(DTWeapons::FindWeapon(GravityMine), 1);
	const uint16 Source = FDTTelemetrySources::FindOrAdd(TEXT("Weapon.GravityMine"));
	H.StepOnce();
	TestEqual(TEXT("placed at the densest cell"), H.Weapons.GetLiveMines(0, 0), 1);
	TestEqual(TEXT("no hit on placement"), H.HitLog.Num(), 0);

	const int32 FirstTick = FMath::CeilToInt((DTConst::WEAPON_MINE_ARM_S + DTConst::WEAPON_MINE_PULL_TICK_S) / Dt);
	H.StepMany(FirstTick - 3);
	TestEqual(TEXT("armed but no tick yet: no pull"), H.View.Displacements.Num(), 0);
	H.StepMany(4);
	TestEqual(TEXT("first tick: one pull per enemy in the pull radius"), H.View.Displacements.Num(), 5);
	TestEqual(TEXT("first tick: one tick hit per enemy"), H.HitLog.Num(), 5);
	if (H.HitLog.Num() > 0)
	{
		TestEqual(TEXT("tick damage ratio"), H.HitLog[0].Damage, L1.Damage * DTConst::WEAPON_MINE_PULL_TICK_DAMAGE_RATIO, 1e-3f);
		TestEqual(TEXT("tick source"), H.HitLog[0].SourceId, Source);
	}
	const float PullStep = DTConst::WEAPON_MINE_PULL_SPEED_UU_S * DTConst::WEAPON_MINE_PULL_TICK_S;
	for (const FDTDisplacement& Move : H.View.Displacements)
	{
		TestTrue(TEXT("pull step <= speed x tick"), Move.Delta.Size() <= PullStep + 1e-3f);
		TestEqual(TEXT("pull owner"), Move.OwnerId, 0);
	}
	// Run to the detonation: six ticks then one full-damage hit per enemy within the blast.
	const int32 Detonation = FMath::CeilToInt((DTConst::WEAPON_MINE_ARM_S + DTConst::WEAPON_MINE_PULL_TICK_S * DTConst::WEAPON_MINE_PULL_TICKS) / Dt) + 1;
	H.HitLog.Reset();
	H.View.Displacements.Reset();
	// Keep it the only mine: stop the weapon from placing more by pushing its cooldown far out.
	H.Run.GetPlayer(0).Weapons[0].Cooldown = 1000.0f;
	H.StepMany(Detonation - FirstTick);
	int32 Full = 0, Ticks = 0;
	for (const FDTHit& Hit : H.HitLog)
	{
		Full += FMath::IsNearlyEqual(Hit.Damage, L1.Damage) ? 1 : 0;
		Ticks += FMath::IsNearlyEqual(Hit.Damage, L1.Damage * DTConst::WEAPON_MINE_PULL_TICK_DAMAGE_RATIO) ? 1 : 0;
	}
	TestEqual(TEXT("remaining ticks"), Ticks, 5 * (DTConst::WEAPON_MINE_PULL_TICKS - 1));
	TestEqual(TEXT("detonation: one hit per enemy"), Full, 5);
	TestEqual(TEXT("detonated mine is gone"), H.Weapons.GetLiveMines(0, 0), 0);
	for (int32 Index = 0; Index < Start.Num(); ++Index)
	{
		TestEqual(TEXT("enemy positions change only through displacement requests"), H.Enemies.GetPositions()[Index], Start[Index]);
	}

	// Live cap: placing one more than the cap detonates the oldest at once.
	FDTHarness Cap;
	Cap.bResolve = false;
	Cap.AddEnemy(FVector2f(300.0f, 0.0f));
	Cap.Weapons.Give(Cap.Run, 0, GravityMine, 1);
	FDTWeaponHeroStats Fast;
	Fast.CooldownMult = 0.0f;
	Cap.Weapons.SetHeroStats(0, Fast);
	int32 MaxLive = 0;
	for (int32 Step = 0; Step < 10; ++Step)
	{
		Cap.StepOnce();
		MaxLive = FMath::Max(MaxLive, Cap.Weapons.GetLiveMines(0, 0));
	}
	TestEqual(TEXT("never more than the live cap"), MaxLive, DTConst::WEAPON_MINE_LIVE_MAX);
	int32 FullHits = 0;
	for (const FDTHit& Hit : Cap.HitLog)
	{
		FullHits += FMath::IsNearlyEqual(Hit.Damage, L1.Damage) ? 1 : 0;
	}
	TestEqual(TEXT("each placement past the cap detonated the oldest"), FullHits, 10 - DTConst::WEAPON_MINE_LIVE_MAX);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
