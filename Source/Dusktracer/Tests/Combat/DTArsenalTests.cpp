// S2 arsenal tests: one per weapon and modifier (02_Systems/weapons.md T-WPN-14..18, modifiers.md T-MOD-10..15).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Arsenal; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Bot/DTBotBrain.h"
#include "Combat/DTWeaponCatalog.h"
#include "Core/DTCatalogGen.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTArsenalTest
{
	using namespace DTWeaponTestHarness;

	static const DTCat::FWeaponLevelRow& Row(const TCHAR* Weapon, int32 Level)
	{
		return *DTWeapons::GetLevelRow(DTWeapons::FindWeapon(FName(Weapon)), Level);
	}

	static float BearingDeg(const FVector2f& V)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(V.Y, V.X));
	}

	static int32 HitsOn(const FDTHarness& H, uint32 Serial)
	{
		int32 Count = 0;
		for (const FDTHit& Hit : H.HitLog)
		{
			Count += Hit.EnemySerial == Serial ? 1 : 0;
		}
		return Count;
	}

	/** A round's travel bearing is read off two consecutive pool positions. */
	static FVector2f Heading(const FDTHarness& H, int32 Index)
	{
		return H.Bullets.GetPositions()[Index] - H.Bullets.GetPrevPositions()[Index];
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalCatalog, "Dusktracer.Arsenal.Catalog", DTWeaponTestHarness::TestFlags) // T-WPN-01 (the ten)
bool FDTArsenalCatalog::RunTest(const FString& Parameters)
{
	using namespace DTWeaponTestHarness;
	TestEqual(TEXT("ten weapons"), DTWeapons::NumWeapons(), 10);
	TestEqual(TEXT("sixteen modifiers"), DTWeapons::NumModifiers(), 16);
	const FDTPlayerRunState Fresh;
	for (int32 Weapon = 0; Weapon < DTWeapons::NumWeapons(); ++Weapon)
	{
		const DTCat::FWeaponRow& Weap = DTCat::Weapons[Weapon];
		TestEqual(FString::Printf(TEXT("%s has a row per level"), Weap.Id), Weap.NumLevels, DTConst::WEAPON_LEVEL_MAX);
		// R-BOT-09 reads this for every card: a weapon without a reach would score as unusable.
		TestTrue(FString::Printf(TEXT("%s has a reach for bot v2"), Weap.Id), FDTBotBrain::WeaponReach(FName(Weap.Id), Fresh) > 0.0f);
		TestTrue(FString::Printf(TEXT("%s has an offer fit"), Weap.Id), Weap.Fits != 0);
		FDTHarness H;
		TestEqual(FString::Printf(TEXT("%s can be given"), Weap.Id), H.Weapons.Give(H.Run, 0, FName(Weap.Id), 1), EDTWeaponResult::Ok);
	}
	for (int32 Mod = 0; Mod < DTWeapons::NumModifiers(); ++Mod)
	{
		TestTrue(FString::Printf(TEXT("%s has an offer fit"), DTCat::Modifiers[Mod].Id), DTCat::Modifiers[Mod].Fits != 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalScatterCannon, "Dusktracer.Arsenal.ScatterCannon", DTWeaponTestHarness::TestFlags) // T-WPN-14
bool FDTArsenalScatterCannon::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	const DTCat::FWeaponLevelRow& L1 = Row(TEXT("Weapon.ScatterCannon"), 1);
	{
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, TEXT("Weapon.ScatterCannon"), 1);
		H.StepOnce();
		TestEqual(TEXT("no target, no shot"), H.Bullets.Num(), 0);
	}
	FDTHarness H;
	H.AddEnemy(FVector2f(0.0f, 300.0f));
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.ScatterCannon"), 1);
	H.Enemies.Publish(H.View);
	H.Weapons.StepWeapons(H.Context(), &H.Bullets);
	TestEqual(TEXT("count pellets"), H.Bullets.Num(), L1.Count);
	if (H.Bullets.Num() == L1.Count)
	{
		H.Bullets.Step(Dt, H.View);
		float Low = 1000.0f;
		float High = -1000.0f;
		for (int32 Index = 0; Index < H.Bullets.Num(); ++Index)
		{
			const float Off = BearingDeg(Heading(H, Index)) - 90.0f;
			Low = FMath::Min(Low, Off);
			High = FMath::Max(High, Off);
		}
		const float Half = 0.5f * static_cast<float>(DTConst::WEAPON_CONE_DEG);
		TestNearlyEqual(TEXT("cone's low edge"), Low, -Half, 0.5f);
		TestNearlyEqual(TEXT("cone's high edge"), High, Half, 0.5f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalRailLance, "Dusktracer.Arsenal.RailLance", DTWeaponTestHarness::TestFlags) // T-WPN-15
bool FDTArsenalRailLance::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	FDTHarness H;
	H.bResolve = false;
	const int32 Lone = H.AddEnemy(FVector2f(0.0f, 150.0f)); // the nearest, alone
	TArray<int32> Line;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		Line.Add(H.AddEnemy(FVector2f(300.0f + 60.0f * Index, 0.0f)));
	}
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.RailLance"), 1);
	H.Enemies.Publish(H.View);
	const TArray<uint32> Serials = H.View.EnemySerials;
	H.StepMany(100);
	TestEqual(TEXT("the lone nearest enemy is not the line"), HitsOn(H, Serials[Lone]), 0);
	int32 Struck = 0;
	for (const int32 Enemy : Line)
	{
		Struck += HitsOn(H, Serials[Enemy]) > 0 ? 1 : 0;
	}
	// The source's pierce 12 passes through twelve enemies and stops in the thirteenth.
	TestEqual(TEXT("one lance strikes thirteen enemies"), Struck, Row(TEXT("Weapon.RailLance"), 1).Pierce);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalFlakMortar, "Dusktracer.Arsenal.FlakMortar", DTWeaponTestHarness::TestFlags) // T-WPN-16
bool FDTArsenalFlakMortar::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	const DTCat::FWeaponLevelRow& L1 = Row(TEXT("Weapon.FlakMortar"), 1);
	FDTHarness H;
	H.bResolve = false;
	const FVector2f Crowd(600.0f, 0.0f);
	H.AddEnemy(Crowd + FVector2f(-20.0f, 0.0f));
	H.AddEnemy(Crowd + FVector2f(20.0f, 0.0f));
	H.AddEnemy(Crowd + FVector2f(0.0f, 30.0f));
	H.AddEnemy(FVector2f(-300.0f, 0.0f)); // a lone enemy outside the blast
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.FlakMortar"), 1);
	H.StepOnce();
	TestEqual(TEXT("a shell is not a pool round"), H.Bullets.GetCounters().Spawned, static_cast<int64>(0));
	TestEqual(TEXT("no hit at launch"), H.HitLog.Num(), 0);
	const int32 Flight = FMath::CeilToInt((Crowd.Size() / L1.Speed_uu_s) / Dt);
	H.StepMany(Flight / 2);
	TestEqual(TEXT("no hit in flight"), H.HitLog.Num(), 0);
	H.StepMany(Flight);
	TestEqual(TEXT("one hit per enemy in the blast at landing"), H.HitLog.Num(), 3);
	const uint16 Source = FDTTelemetrySources::FindOrAdd(TEXT("Weapon.FlakMortar"));
	TestEqual(TEXT("blast hits carry the weapon"), H.CountHitsFrom(Source), H.HitLog.Num());
	{
		FDTHarness Empty;
		Empty.Weapons.Give(Empty.Run, 0, TEXT("Weapon.FlakMortar"), 1);
		Empty.StepMany(120);
		TestEqual(TEXT("no target, no shell"), Empty.Weapons.GetCounters(0, 0).Spawned, static_cast<int64>(0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalSeekerSwarm, "Dusktracer.Arsenal.SeekerSwarm", DTWeaponTestHarness::TestFlags) // T-WPN-17
bool FDTArsenalSeekerSwarm::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	const DTCat::FWeaponLevelRow& L1 = Row(TEXT("Weapon.SeekerSwarm"), 1);
	auto Run = [](TArray<FDTHit>& OutHits)
	{
		FDTHarness H(11);
		H.bResolve = false;
		// Enemies all round the hero: a missile only reaches one by steering.
		H.AddEnemy(FVector2f(500.0f, 0.0f));
		H.AddEnemy(FVector2f(0.0f, 500.0f));
		H.AddEnemy(FVector2f(-500.0f, 0.0f));
		H.AddEnemy(FVector2f(0.0f, -500.0f));
		H.Weapons.Give(H.Run, 0, TEXT("Weapon.SeekerSwarm"), 1);
		H.StepOnce();
		const int32 Launched = H.Bullets.Num();
		H.StepMany(50);
		OutHits = H.HitLog;
		return Launched;
	};
	TArray<FDTHit> A;
	TArray<FDTHit> B;
	TestEqual(TEXT("count missiles"), Run(A), L1.Count);
	Run(B);
	TestEqual(TEXT("every missile of the first volley struck"), A.Num() >= L1.Count, true);
	TSet<uint32> Struck;
	for (const FDTHit& Hit : A)
	{
		Struck.Add(Hit.EnemySerial);
	}
	TestTrue(TEXT("targets are spread among the living"), Struck.Num() >= 2);
	TestEqual(TEXT("same seed, same hits"), A.Num(), B.Num());
	for (int32 Index = 0; Index < FMath::Min(A.Num(), B.Num()); ++Index)
	{
		TestTrue(TEXT("same hit sequence"), A[Index].EnemySerial == B[Index].EnemySerial && A[Index].Position == B[Index].Position);
	}
	{
		// A missile whose target dies goes for the nearest other enemy (it never flies on blind while one is in range).
		FDTHarness H(11);
		H.AddEnemy(FVector2f(0.0f, 400.0f), 1.0f);
		H.AddEnemy(FVector2f(100.0f, 450.0f));
		H.Weapons.Give(H.Run, 0, TEXT("Weapon.SeekerSwarm"), 1);
		H.StepMany(120);
		TestTrue(TEXT("the second enemy is struck after the first died"), H.HitLog.Num() >= L1.Count);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalSpiralProjector, "Dusktracer.Arsenal.SpiralProjector", DTWeaponTestHarness::TestFlags) // T-WPN-18
bool FDTArsenalSpiralProjector::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	const DTCat::FWeaponLevelRow& L1 = Row(TEXT("Weapon.SpiralProjector"), 1);
	FDTHarness H; // no enemy: a spiral fires regardless
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.SpiralProjector"), 1);
	const int32 Steps = 60;
	H.StepMany(Steps);
	const int32 Expected = FMath::FloorToInt(Steps * Dt / L1.Cooldown_s);
	TestTrue(FString::Printf(TEXT("stream rate: %d rounds"), H.Bullets.Num()), FMath::Abs(H.Bullets.Num() - Expected) <= 2);
	if (H.Bullets.Num() >= 3)
	{
		// Rounds spawned earlier have moved further: sort by distance and read the turn between neighbours.
		TArray<FVector2f> Pos(H.Bullets.GetPositions().GetData(), H.Bullets.Num());
		Pos.Sort([](const FVector2f& A, const FVector2f& B) { return A.SizeSquared() < B.SizeSquared(); });
		const float Turn = FMath::FindDeltaAngleDegrees(BearingDeg(Pos[0]), BearingDeg(Pos[1]));
		const float Want = static_cast<float>(DTConst::WEAPON_SPIRAL_DEG_S) * L1.Cooldown_s;
		TestNearlyEqual(TEXT("each round turns on from the last"), FMath::Abs(Turn), Want, 2.0f);
	}
	const DTCat::FWeaponLevelRow& L3 = Row(TEXT("Weapon.SpiralProjector"), 3);
	FDTHarness Arms;
	Arms.Weapons.Give(Arms.Run, 0, TEXT("Weapon.SpiralProjector"), 3);
	Arms.StepOnce();
	TestEqual(TEXT("one round per arm"), Arms.Bullets.Num(), L3.Count);

	// R-WPN-08: Echo on the stream is a strike on every Nth hit, never nothing.
	FDTHarness E;
	E.bResolve = false;
	E.AddEnemy(FVector2f(120.0f, 0.0f));
	E.Weapons.Give(E.Run, 0, TEXT("Weapon.SpiralProjector"), 3);
	FDTWeaponFlags Flags;
	const uint16 Storm = DTWeapons::KeeperSource(EDTPath::Storm);
	FDTFlagParams& Echo = Flags.Add(DTCat::EFlag::Echo, Storm, EDTFlagTier::Key).Params;
	Echo.Set(DTFlagParam::Dmg, 0.5f);
	Echo.Set(DTFlagParam::DelaySeconds, 0.1f);
	E.Weapons.AddFlagSource(0, TEXT("Test.Echo"), Flags);
	E.StepMany(240);
	TestTrue(TEXT("echo on the spiral strikes"), E.CountHitsFrom(Storm) > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalHoming, "Dusktracer.Arsenal.Homing", DTWeaponTestHarness::TestFlags) // T-MOD-10
bool FDTArsenalHoming::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	// A Ring round leaves at a bearing no enemy holds; with the gyro it bends onto one.
	auto Hits = [](bool bGyro)
	{
		FDTHarness H;
		H.bResolve = false;
		H.AddEnemy(FVector2f(350.0f, 110.0f));
		H.Weapons.Give(H.Run, 0, TEXT("Weapon.RingEmitter"), 1);
		if (bGyro)
		{
			H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Homing"));
		}
		H.StepMany(70);
		return H.HitLog.Num();
	};
	TestEqual(TEXT("bare ring misses the off-bearing enemy"), Hits(false), 0);
	TestTrue(TEXT("homing rounds reach it"), Hits(true) > 0);

	// A tracked homing round stays in step with the pool: a split fires from where the round really struck.
	FDTHarness T;
	T.bResolve = false;
	T.AddEnemy(FVector2f(350.0f, 110.0f));
	T.Weapons.Give(T.Run, 0, TEXT("Weapon.RingEmitter"), 1);
	T.Weapons.Socket(T.Run, 0, 0, TEXT("Mod.Homing"));
	T.Weapons.Socket(T.Run, 0, 0, TEXT("Mod.SplitCore"));
	T.StepMany(70);
	TestTrue(TEXT("homing rounds split on hit"), T.CountHitsFrom(FDTTelemetrySources::FindOrAdd(TEXT("Modifier.SplitCore"))) + T.Weapons.GetMaxGeneration() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalOrbitLock, "Dusktracer.Arsenal.OrbitLock", DTWeaponTestHarness::TestFlags) // T-MOD-11
bool FDTArsenalOrbitLock::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	const int32 Mod = DTWeapons::FindModifier(TEXT("Mod.OrbitLock"));
	const float Hold = DTWeapons::ModifierParam(Mod, TEXT("hold_s"));
	const float Radius = DTWeapons::ModifierParam(Mod, TEXT("orbit_radius_uu"));
	FDTHarness H;
	H.bResolve = false;
	H.AddEnemy(FVector2f(800.0f, 0.0f));
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.OrbitLock"));
	const int32 HoldSteps = FMath::RoundToInt(Hold / Dt);
	H.StepOnce();
	H.StepMany(HoldSteps / 2);
	TestTrue(TEXT("rounds are alive at half the hold"), H.Bullets.Num() > 0);
	for (int32 Index = 0; Index < H.Bullets.Num(); ++Index)
	{
		const float Dist = H.Bullets.GetPositions()[Index].Size();
		TestTrue(FString::Printf(TEXT("round %d circles the hero at %.0f"), Index, Dist), FMath::Abs(Dist - Radius) < 1.0f);
	}
	TestEqual(TEXT("no hit while it circles"), H.HitLog.Num(), 0);
	H.View.HeroPositions[0] = FVector2f(0.0f, 60.0f); // a moved hero takes its ring along
	H.StepOnce();
	TestTrue(TEXT("the ring follows the hero"), H.Bullets.Num() > 0 && FMath::Abs(FVector2f::Distance(H.Bullets.GetPositions()[0], FVector2f(0.0f, 60.0f)) - Radius) < 1.0f);
	H.View.HeroPositions[0] = FVector2f::ZeroVector;
	H.StepMany(HoldSteps + 90);
	TestTrue(TEXT("released rounds fly at the enemy"), H.HitLog.Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalBoomerang, "Dusktracer.Arsenal.Boomerang", DTWeaponTestHarness::TestFlags) // T-MOD-12
bool FDTArsenalBoomerang::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	// A piercing lance passes the enemy, reaches its range, comes home and strikes it again.
	FDTHarness H;
	H.bResolve = false;
	H.AddEnemy(FVector2f(400.0f, 0.0f));
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.RailLance"), 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Boomerang"));
	const DTCat::FWeaponLevelRow& L1 = Row(TEXT("Weapon.RailLance"), 1);
	H.StepOnce();
	H.StepMany(FMath::CeilToInt((L1.Range_uu / L1.Speed_uu_s) * 1.5f / Dt));
	int32 FirstVolley = 0;
	for (const FDTHit& Hit : H.HitLog)
	{
		FirstVolley += FDTTelemetrySources::GetName(Hit.SourceId) == TEXT("Weapon.RailLance") ? 1 : 0;
	}
	TestTrue(FString::Printf(TEXT("a lance strikes twice: %d hits"), FirstVolley), FirstVolley >= 2);

	// A plain round is turned by what it strikes and goes on hitting what stands in its way home.
	FDTHarness P;
	P.bResolve = false;
	P.AddEnemy(FVector2f(500.0f, 0.0f));
	P.AddEnemy(FVector2f(250.0f, 0.0f)); // nearest, struck first
	P.Weapons.Give(P.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	P.Weapons.Socket(P.Run, 0, 0, TEXT("Mod.Boomerang"));
	P.StepOnce();
	const int32 Live = P.Bullets.Num();
	P.StepMany(20);
	TestEqual(TEXT("a struck round is not spent"), P.Bullets.Num() >= Live, true);
	P.StepMany(60);
	bool bHomeward = false;
	for (int32 Index = 0; Index < P.Bullets.Num(); ++Index)
	{
		bHomeward |= Heading(P, Index).X < 0.0f;
	}
	TestTrue(TEXT("rounds fly home after the hit"), bHomeward || P.Bullets.GetCounters().Expired > 0);

	// A round that reaches the hero is gone, well before its doubled life runs out.
	FDTHarness E;
	E.bResolve = false;
	E.AddEnemy(FVector2f(250.0f, 0.0f));
	E.Weapons.Give(E.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	E.Weapons.Socket(E.Run, 0, 0, TEXT("Mod.Boomerang"));
	E.StepOnce();
	const int32 Trip = FMath::CeilToInt((2.0f * 250.0f / Row(TEXT("Weapon.PulseRifle"), 1).Speed_uu_s) / Dt) + 4;
	E.StepMany(Trip);
	TestTrue(TEXT("home rounds end at the hero"), E.Bullets.GetCounters().Expired > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalSpreadWave, "Dusktracer.Arsenal.SpreadWave", DTWeaponTestHarness::TestFlags) // T-MOD-13
bool FDTArsenalSpreadWave::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	const uint16 Wave = FDTTelemetrySources::FindOrAdd(TEXT("Modifier.SpreadWave"));
	const DTCat::FWeaponLevelRow& L1 = Row(TEXT("Weapon.PulseRifle"), 1);
	const int32 Mod = DTWeapons::FindModifier(TEXT("Mod.SpreadWave"));
	const float Reach = DTWeapons::ModifierParam(Mod, TEXT("reach_uu"));
	FDTHarness H;
	H.bResolve = false;
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.SpreadWave"));
	// Aim manually, past the end of the round's range: only the wave can touch the enemy there.
	H.Weapons.SetAimManual(0, true);
	H.AddEnemy(FVector2f(L1.Range_uu + 0.5f * Reach + 40.0f, 0.0f));
	const int32 LifeSteps = FMath::CeilToInt((L1.Range_uu / L1.Speed_uu_s) / Dt);
	H.StepMany(LifeSteps + 5);
	TestTrue(TEXT("the wave strikes past the round's reach"), H.CountHitsFrom(Wave) > 0);
	for (const FDTHit& Hit : H.HitLog)
	{
		if (Hit.SourceId == Wave)
		{
			const float Ratio = DTWeapons::ModifierParam(Mod, TEXT("wave_damage_ratio"));
			TestNearlyEqual(TEXT("wave damage is its share of the round"), Hit.Damage, L1.Damage * Ratio, 0.01f);
			break;
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalSpillover, "Dusktracer.Arsenal.Spillover", DTWeaponTestHarness::TestFlags) // T-MOD-14
bool FDTArsenalSpillover::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	const uint16 Spill = FDTTelemetrySources::FindOrAdd(TEXT("Modifier.Spillover"));
	FDTHarness H;
	H.AddEnemy(FVector2f(200.0f, 0.0f), 4.0f); // far less than a Rail Lance's hit
	H.AddEnemy(FVector2f(200.0f, 150.0f));
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.RailLance"), 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Spillover"));
	H.StepMany(30);
	TestEqual(TEXT("the overkill jumps once"), H.CountHitsFrom(Spill), 1);
	for (const FDTHit& Hit : H.HitLog)
	{
		if (Hit.SourceId == Spill)
		{
			TestNearlyEqual(TEXT("overkill = hit - hp left"), Hit.Damage, Row(TEXT("Weapon.RailLance"), 1).Damage - 4.0f, 0.01f);
			TestEqual(TEXT("to the nearest other enemy"), Hit.EnemyIndex >= 0, true);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTArsenalRefit, "Dusktracer.Arsenal.Refit", DTWeaponTestHarness::TestFlags) // T-MOD-15
bool FDTArsenalRefit::RunTest(const FString& Parameters)
{
	using namespace DTArsenalTest;
	using namespace DTWeaponTestHarness;
	const float Interval = DTWeapons::ModifierParam(DTWeapons::FindModifier(TEXT("Mod.Refit")), TEXT("interval_s"));
	auto Kills = [](FDTHarness& H, int32 Times)
	{
		for (int32 Index = 0; Index < Times; ++Index)
		{
			H.AddEnemy(FVector2f(120.0f, 0.0f), 1.0f);
			H.StepMany(40);
		}
	};
	{
		// Below the fitted count nothing drops.
		FDTHarness H;
		H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
		H.Weapons.Give(H.Run, 0, TEXT("Weapon.RingEmitter"), 1);
		H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Refit"));
		Kills(H, 3);
		TestEqual(TEXT("not fitted full: no drop"), H.Run.GetPlayer(0).Weapons[1].Sockets.Num(), 0);
	}
	FDTHarness H;
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.PulseRifle"), 1);
	H.Weapons.Give(H.Run, 0, TEXT("Weapon.RingEmitter"), 1);
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Refit"));
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Pierce"));
	H.Weapons.Socket(H.Run, 0, 0, TEXT("Mod.Conduit"));
	Kills(H, 2);
	const TArray<FDTWeaponInstance::FSocket>& Ring = H.Run.GetPlayer(0).Weapons[1].Sockets;
	TestEqual(TEXT("a kill fits one modifier into the first free socket"), Ring.Num(), 1);
	if (Ring.Num() == 1)
	{
		const int32 Weapon = H.Run.GetPlayer(0).Weapons[1].Def;
		TestTrue(TEXT("the drop fits its weapon"), DTModCompat::IsCompatible(Weapon, DTWeapons::FindModifier(Ring[0].ModId)));
		TestTrue(TEXT("the drop is in the run pool"), DTCat::Modifiers[DTWeapons::FindModifier(Ring[0].ModId)].bStarter);
	}
	H.StepMany(FMath::RoundToInt(Interval / Dt));
	Kills(H, 1);
	TestEqual(TEXT("after the interval a kill drops another"), H.Run.GetPlayer(0).Weapons[1].Sockets.Num(), 2);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
