// Enemy time and Keeper meters (keepers.md R-KEE-37, R-KEE-38; T-KEE-37, T-KEE-38). Enemy tests step FDTEnemyPool as the
// Enemies phase does, with the scale in the view; the boss has its own test in DTBossTests.cpp (it needs that harness).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Keepers/DTKeeperHarness.h"
#include "Core/DTPaletteGen.h"
#include "Enemies/DTElite.h"
#include "Game/DTKeeperMeters.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Player/DTHeroSim.h"
#include "Serialization/Csv/CsvParser.h"

namespace DTDilationTest
{
	using namespace DTKeeperTest;

	constexpr int32 Second = DTConst::SIM_FIXED_HZ;
	constexpr float Half = 0.5f;
	constexpr float PosEps = 0.5f;

	/** One pool stepped alone: a far hero, and no weapons, so only enemy time moves it. */
	struct FRig
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTEnemyPool Pool;
		FRandomStream Rng;
		int64 Step = 0;

		explicit FRig(float Scale, const FVector2f& Hero = FVector2f(3000.0f, 0.0f)) : Rng(11)
		{
			Run.Start(7, 1);
			View.HeroPositions.Init(Hero, 1);
			View.HeroAimDirs.Init(FVector2f(1.0f, 0.0f), 1);
			View.HeroIntangible.Init(false, 1);
			View.EnemyTimeScale = Scale;
		}

		FDTSimContext Context()
		{
			FDTSimContext Ctx;
			Ctx.StepSeconds = Dt;
			Ctx.StepIndex = Step;
			Ctx.Run = &Run;
			Ctx.View = &View;
			return Ctx;
		}

		void StepOnce()
		{
			Pool.StepEnemies(Context());
			Pool.ResolveHits(Context());
			++Step;
		}

		int32 Add(EDTEnemyRole Role, const FVector2f& At, uint8 Affixes = 0, bool bArrival = false)
		{
			FDTEnemySpawn Spec;
			Spec.Role = Role;
			Spec.Position = At;
			Spec.bElite = Affixes != 0;
			Spec.AffixMask = Affixes;
			Spec.bArrival = bArrival;
			Spec.HpScale = 1000.0f;
			return Pool.Spawn(Spec, &Rng);
		}

		void Kill(int32 Index)
		{
			FDTHit Hit;
			Hit.EnemyIndex = Index;
			Hit.EnemySerial = Pool.GetSerials()[Index];
			Hit.OwnerId = 0;
			Hit.Damage = 1.0e9f;
			View.Hits.Add(Hit);
			Pool.ResolveHits(Context());
		}

		template <typename TPred>
		int32 StepsUntil(TPred Pred, int32 Limit = 20 * Second)
		{
			for (int32 N = 0; N < Limit; ++N)
			{
				if (Pred())
				{
					return N;
				}
				StepOnce();
			}
			return INDEX_NONE;
		}
	};

	/** Relative luminance of a palette token, as 04_Presentation reads contrast. */
	static float Luminance(const FColor& Token)
	{
		const FLinearColor L = DTPal::Linear(Token);
		return 0.2126f * L.R + 0.7152f * L.G + 0.0722f * L.B;
	}

	static const FDTHudMeter* Find(const TArray<FDTHudMeter>& Meters, const TCHAR* Key)
	{
		return Meters.FindByPredicate([Key](const FDTHudMeter& M) { return M.Key == FName(Key); });
	}

	static TArray<FDTHudMeter> Read(FDTKeeperHarness& H)
	{
		TArray<FDTHudMeter> Out;
		DTKeeperMeters::Build(H.Weapons.GetKeeperMeters(0), H.Keepers.GetDilationLeft(0), H.Keepers.GetDilationTotal(0), Out);
		return Out;
	}

	static uint32 HashOf(const FDTEnemyPool& Pool, uint32 Seed)
	{
		uint32 Hash = FCrc::MemCrc32(Pool.GetPositions().GetData(), Pool.GetPositions().Num() * sizeof(FVector2f), Seed);
		Hash = FCrc::MemCrc32(Pool.GetHp().GetData(), Pool.GetHp().Num() * sizeof(float), Hash);
		return FCrc::MemCrc32(Pool.GetRounds().GetPositions().GetData(), Pool.GetRounds().Num() * sizeof(FVector2f), Hash);
	}

	/** A Quicksilver cast over a real moving crowd, rounds in the air, in phase order; bCast false leaves the clock alone. */
	static uint32 RunScene(int32 Seed, bool bCast)
	{
		FDTKeeperHarness H(Seed);
		H.Patron(EDTPath::Quicksilver);
		FRandomStream Rng(Seed);
		for (int32 Index = 0; Index < 30; ++Index)
		{
			FDTEnemySpawn Spec;
			Spec.Role = Index % 5 == 0 ? EDTEnemyRole::Turret : Index % 7 == 0 ? EDTEnemyRole::Rusher : EDTEnemyRole::Swarm;
			Spec.Position = FVector2f(Rng.FRandRange(-1200.0f, 1200.0f), Rng.FRandRange(600.0f, 1500.0f));
			Spec.HpScale = 200.0f;
			H.Enemies.Spawn(Spec, &Rng);
		}
		uint32 Hash = 0;
		for (int32 Step = 0; Step < 8 * Second; ++Step)
		{
			if (bCast && Step == Second)
			{
				H.Keepers.QueueIntercession(0);
			}
			FDTSimContext Ctx = H.Context();
			H.Enemies.StepEnemies(Ctx);
			H.Weapons.StepWeapons(Ctx, &H.Bullets);
			H.Bullets.Step(Dt, H.View);
			H.Weapons.ObserveBullets(Ctx);
			H.Keepers.CaptureHits(Ctx, &H.Enemies);
			H.Enemies.ResolveHits(Ctx);
			H.Enemies.Publish(H.View);
			H.Weapons.StepEffects(Ctx, &H.Bullets);
			H.Keepers.StepEffects(Ctx, &H.Weapons);
			++H.Step;
			Hash = HashOf(H.Enemies, Hash);
		}
		return Hash;
	}
}

#define DT_DILATION_TEST(Class, Name) \
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(Class, "Dusktracer.Keepers.S2." Name, DTWeaponTestHarness::TestFlags) \
	bool Class::RunTest(const FString& Parameters)

DT_DILATION_TEST(FDTDilationScale, "Dilation.Scale") // T-KEE-37
{
	using namespace DTDilationTest;
	const float Scale = static_cast<float>(DTConst::INTERCESSION_QUICKSILVER_TIME_SCALE);
	const int32 Steps = FMath::RoundToInt(DTConst::INTERCESSION_QUICKSILVER_DURATION_S * Second);
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Quicksilver);
		H.StepOnce();
		TestEqual(TEXT("no cast, no scale"), H.View.EnemyTimeScale, 1.0f);
		H.Keepers.QueueIntercession(0);
		H.StepOnce();
		TestEqual(TEXT("the step of the cast writes it"), H.View.EnemyTimeScale, Scale);
		H.StepMany(Steps - 2);
		TestEqual(TEXT("it holds through the window"), H.View.EnemyTimeScale, Scale);
		H.StepMany(3);
		TestFalse(TEXT("the cast is over"), H.Keepers.IsCasting(0));
		TestEqual(TEXT("and the scale with it"), H.View.EnemyTimeScale, 1.0f);
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Storm);
		H.Keepers.QueueIntercession(0);
		H.StepMany(2);
		TestTrue(TEXT("another Keeper casts"), H.Keepers.IsCasting(0));
		TestEqual(TEXT("and time stands as it was"), H.View.EnemyTimeScale, 1.0f);
	}
	{
		FDTKeeperHarness H(7, 2);
		H.Patron(EDTPath::Quicksilver, 0);
		H.Patron(EDTPath::Quicksilver, 1);
		H.Keepers.QueueIntercession(0);
		H.StepMany(Second);
		H.Keepers.QueueIntercession(1);
		H.StepMany(2);
		TestEqual(TEXT("two casts never multiply"), H.View.EnemyTimeScale, Scale);
		H.StepMany(Steps - Second);
		TestFalse(TEXT("the first is over"), H.Keepers.IsCasting(0));
		TestEqual(TEXT("the second still holds it"), H.View.EnemyTimeScale, Scale);
	}
	return true;
}

DT_DILATION_TEST(FDTDilationEnemies, "Dilation.Enemies") // T-KEE-37
{
	using namespace DTDilationTest;
	for (const EDTEnemyRole Role : { EDTEnemyRole::Swarm, EDTEnemyRole::Tank })
	{
		FRig Full(1.0f);
		FRig Slow(Half);
		const int32 A = Full.Add(Role, FVector2f(0.0f, 0.0f));
		const int32 B = Slow.Add(Role, FVector2f(0.0f, 0.0f));
		for (int32 N = 0; N < 2 * Second; ++N)
		{
			Slow.StepOnce();
		}
		for (int32 N = 0; N < Second; ++N)
		{
			Full.StepOnce();
		}
		TestTrue(TEXT("it left its spot"), Full.Pool.GetPositions()[A].X > 10.0f);
		const float Off = FVector2f::Distance(Full.Pool.GetPositions()[A], Slow.Pool.GetPositions()[B]);
		// Steering integrates per step, so the walks differ by about one step of it at full speed, never more.
		TestTrue(*FString::Printf(TEXT("two seconds at half are one second of walking, off by %.2f"), Off), Off <= Full.Pool.GetSpeeds()[A] * Dt);
	}
	{
		FRig Rig(Half);
		FDTEnemyRoundSpawn Round;
		Round.Velocity = FVector2f(120.0f, 0.0f);
		Round.Radius = 5.0f;
		Round.Life = 1.0f;
		Rig.Pool.GetRounds().Spawn(Round);
		for (int32 N = 0; N < Second; ++N)
		{
			Rig.StepOnce();
		}
		TestTrue(TEXT("a round flies at half speed"), FMath::IsNearlyEqual(Rig.Pool.GetRounds().GetPositions()[0].X, 60.0f, PosEps));
		for (int32 N = 0; N < Second + 2; ++N)
		{
			Rig.StepOnce();
		}
		TestEqual(TEXT("and its life runs in enemy time"), Rig.Pool.GetRounds().Num(), 0);
	}
	{
		// Recover and cooldown slow; the swing of a Rusher's whole charge does too.
		const auto ChargeSteps = [](float Scale)
		{
			FRig Rig(Scale, FVector2f(300.0f, 0.0f));
			const int32 R = Rig.Add(EDTEnemyRole::Rusher, FVector2f(0.0f, 0.0f));
			const FDTEnemyPool& Pool = Rig.Pool;
			Rig.StepsUntil([&] { return Pool.GetAux(R).State == EDTEnemyState::Charge; });
			return Rig.StepsUntil([&] { return Pool.GetAux(R).State == EDTEnemyState::Recover; });
		};
		TestTrue(TEXT("a charge takes twice the steps at half"), FMath::Abs(ChargeSteps(Half) - 2 * ChargeSteps(1.0f)) <= 2);
	}
	{
		const auto BurstSteps = [](float Scale)
		{
			FRig Rig(Scale, FVector2f(900.0f, 0.0f));
			const int32 T = Rig.Add(EDTEnemyRole::Turret, FVector2f(0.0f, 0.0f));
			const FDTEnemyPool& Pool = Rig.Pool;
			Rig.StepsUntil([&] { return Pool.GetAux(T).State == EDTEnemyState::Fire; });
			return Rig.StepsUntil([&] { return Pool.GetAux(T).State != EDTEnemyState::Fire; });
		};
		TestTrue(TEXT("volley gaps slow"), BurstSteps(Half) > BurstSteps(1.0f));
	}
	return true;
}

DT_DILATION_TEST(FDTDilationLeads, "Dilation.Leads") // T-KEE-37, R-DIR-10
{
	using namespace DTDilationTest;
	const float Scale = Half;
	{
		const auto WindUp = [](float S)
		{
			FRig Rig(S, FVector2f(300.0f, 0.0f));
			const int32 R = Rig.Add(EDTEnemyRole::Rusher, FVector2f(0.0f, 0.0f));
			const FDTEnemyPool& Pool = Rig.Pool;
			Rig.StepsUntil([&] { return Pool.GetAux(R).State == EDTEnemyState::WindUp; });
			return Rig.StepsUntil([&] { return Pool.GetAux(R).State == EDTEnemyState::Charge; });
		};
		TestTrue(TEXT("the wind-up is seen"), WindUp(1.0f) > 0);
		TestEqual(TEXT("a Rusher's wind-up does not stretch"), WindUp(Scale), WindUp(1.0f));
	}
	{
		const auto Tell = [](float S)
		{
			FRig Rig(S, FVector2f(900.0f, 0.0f));
			const int32 T = Rig.Add(EDTEnemyRole::Turret, FVector2f(0.0f, 0.0f));
			const FDTEnemyPool& Pool = Rig.Pool;
			Rig.StepsUntil([&] { return Pool.GetAux(T).State == EDTEnemyState::Tell; });
			return Rig.StepsUntil([&] { return Pool.GetAux(T).State == EDTEnemyState::Fire; });
		};
		TestTrue(TEXT("the tell is seen"), Tell(1.0f) > 0);
		TestEqual(TEXT("a Turret's tell does not stretch"), Tell(Scale), Tell(1.0f));
	}
	{
		const auto Link = [](float S, int32* OutTick)
		{
			FRig Rig(S, FVector2f(50.0f, 0.0f));
			Rig.Add(EDTEnemyRole::Tank, FVector2f(0.0f, 0.0f), DTElite::Bit(EDTAffix::Vampiric));
			const auto Drained = [&] { return Rig.View.HeroHits.ContainsByPredicate([](const FDTHeroHit& H) { return H.Kind == EDTHeroHitKind::Drain; }); };
			Rig.StepOnce();
			const int32 First = Rig.StepsUntil(Drained);
			Rig.StepOnce();
			*OutTick = Rig.StepsUntil(Drained);
			return First;
		};
		int32 TickFull = 0;
		int32 TickSlow = 0;
		const int32 Full = Link(1.0f, &TickFull);
		TestEqual(TEXT("a Vampiric link does not stretch"), Link(Scale, &TickSlow), Full);
		TestTrue(TEXT("its drain ticks do"), TickSlow > TickFull);
	}
	{
		const auto Blast = [](float S)
		{
			FRig Rig(S);
			Rig.Kill(Rig.Add(EDTEnemyRole::Swarm, FVector2f(0.0f, 0.0f), DTElite::Bit(EDTAffix::Volatile)));
			const FDTEnemyPool& Pool = Rig.Pool;
			return Rig.StepsUntil([&] { return Pool.GetEliteBlasts().Num() == 0; });
		};
		TestTrue(TEXT("a blast waits"), Blast(1.0f) > 0);
		TestEqual(TEXT("a Volatile blast does not stretch"), Blast(Scale), Blast(1.0f));
	}
	{
		const auto Orphan = [](float S)
		{
			FRig Rig(S);
			const int32 R = Rig.Add(EDTEnemyRole::Rusher, FVector2f(0.0f, 0.0f), 0, true);
			Rig.StepOnce();
			Rig.Kill(R);
			const FDTEnemyPool& Pool = Rig.Pool;
			return Pool.GetOrphanAttacks().Num() == 1 ? Rig.StepsUntil([&] { return Pool.GetOrphanAttacks().Num() == 0; }) : INDEX_NONE;
		};
		TestTrue(TEXT("the attack outlives its unit"), Orphan(1.0f) > 0);
		TestEqual(TEXT("an orphan attack does not stretch"), Orphan(Scale), Orphan(1.0f));
	}
	return true;
}

DT_DILATION_TEST(FDTDilationStatuses, "Dilation.Statuses") // T-KEE-37
{
	using namespace DTDilationTest;
	const auto Burnt = [](float S, float* OutFreeze)
	{
		FRig Rig(S);
		const int32 A = Rig.Add(EDTEnemyRole::Tank, FVector2f(0.0f, 0.0f));
		const int32 B = Rig.Add(EDTEnemyRole::Tank, FVector2f(800.0f, 800.0f));
		const float Before = Rig.Pool.GetHp()[A];
		FDTStatusApply Burn;
		Burn.Handle = Rig.Pool.GetHandle(A);
		Burn.Status = EDTStatus::Burn;
		Burn.Magnitude = 100.0f;
		Burn.OwnerId = 0;
		Rig.View.StatusApplies.Add(Burn);
		FDTStatusApply Freeze;
		Freeze.Handle = Rig.Pool.GetHandle(B);
		Freeze.Status = EDTStatus::Freeze;
		Freeze.OwnerId = 0;
		Rig.View.StatusApplies.Add(Freeze);
		for (int32 N = 0; N < Second; ++N)
		{
			Rig.StepOnce();
		}
		*OutFreeze = Rig.Pool.GetAux(B).FreezeLeft;
		return Before - Rig.Pool.GetHp()[A];
	};
	float FreezeFull = 0.0f;
	float FreezeSlow = 0.0f;
	const float Full = Burnt(1.0f, &FreezeFull);
	TestTrue(TEXT("the Burn deals"), Full > 0.0f);
	TestEqual(TEXT("a cast does not thin a Burn"), Burnt(Half, &FreezeSlow), Full);
	TestEqual(TEXT("nor stretch a Freeze"), FreezeSlow, FreezeFull);
	return true;
}

DT_DILATION_TEST(FDTDilationSimTime, "Dilation.SimTime") // T-KEE-37, R-CMB-12
{
	using namespace DTDilationTest;
	// The hero: the same inputs under the scale give bit-identical states, dash and meter included.
	const auto Walk = [](float S)
	{
		FDTRunState Run;
		Run.Start(7, 1);
		FDTWorldView View;
		View.HeroPositions.Init(FVector2f::ZeroVector, 1);
		View.HeroAimDirs.Init(FVector2f(1.0f, 0.0f), 1);
		View.HeroIntangible.Init(false, 1);
		View.HeroHurt.Init(false, 1);
		View.EnemyTimeScale = S;
		FDTHeroSim Hero;
		Hero.Reset(1);
		FDTSimContext Ctx;
		Ctx.StepSeconds = Dt;
		Ctx.Run = &Run;
		Ctx.View = &View;
		for (int32 N = 0; N < 3 * Second; ++N)
		{
			FDTHeroInput In;
			In.Move = FVector2f(1.0f, 0.0f);
			In.bDashPress = N == Second;
			Hero.SetStepInput(0, In);
			Ctx.StepIndex = N;
			Hero.Step(Ctx);
		}
		return Hero.GetState(0);
	};
	const FDTHeroState A = Walk(1.0f);
	const FDTHeroState B = Walk(Half);
	TestTrue(TEXT("the hero's position"), A.Pos == B.Pos);
	TestEqual(TEXT("the dash charges"), A.DashCharges, B.DashCharges);
	TestEqual(TEXT("the refill"), A.DashRefillLeft, B.DashRefillLeft);
	TestEqual(TEXT("the Intercession meter"), A.Intercession01, B.Intercession01);

	// Weapons: the same scene deals the same damage on a scene whose enemies stand, cast or no cast of time.
	const auto Fired = [](float S)
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Storm);
		H.AddEnemy(FVector2f(500.0f, 0.0f));
		H.View.EnemyTimeScale = S;
		H.StepMany(2 * Second);
		return H.TotalDamage();
	};
	TestTrue(TEXT("the rifle fires"), Fired(1.0f) > 0.0);
	TestEqual(TEXT("player fire reads sim time"), Fired(Half), Fired(1.0f));
	return true;
}

DT_DILATION_TEST(FDTDilationDeterminism, "Dilation.Determinism") // T-KEE-37, R-KEE-19
{
	using namespace DTDilationTest;
	const uint32 With = RunScene(3, true);
	TestEqual(TEXT("the same seed twice is bit-identical"), RunScene(3, true), With);
	TestNotEqual(TEXT("the cast changes the scene"), RunScene(3, false), With);
	TestNotEqual(TEXT("another seed differs"), RunScene(4, true), With);
	return true;
}

DT_DILATION_TEST(FDTHudMeters, "Hud.Meters") // T-KEE-38
{
	using namespace DTDilationTest;
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Storm);
		TestEqual(TEXT("nothing held, no meter"), Read(H).Num(), 0);
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Shade);
		H.Bless(Blessing(TEXT("Shade.Economy"), TEXT("Key")));
		H.StepOnce();
		H.Weapons.AddDebt(0, 3);
		TArray<FDTHudMeter> Meters = Read(H);
		const FDTHudMeter* Debt = Find(Meters, TEXT("Hud.Debt"));
		if (TestNotNull(TEXT("Debt shows"), Debt))
		{
			TestEqual(TEXT("its value"), Debt->Value, 3.0f);
			const FDTKeeperMeterRead Raw = H.Weapons.GetKeeperMeters(0);
			TestTrue(TEXT("its maximum"), Raw.DebtMax > 3 && Debt->Max == static_cast<float>(Raw.DebtMax));
			TestEqual(TEXT("its fill"), Debt->Fill01, 3.0f / static_cast<float>(Raw.DebtMax));
			TestTrue(TEXT("Shade's token"), Debt->Color == DTPal::Keeper_Shade);
		}
		TestEqual(TEXT("Debt alone"), Meters.Num(), 1);
		H.Weapons.ForcePayday(0, 2.0f, 0.0f);
		Meters = Read(H);
		const FDTHudMeter* Payday = Find(Meters, TEXT("Hud.Payday"));
		TestTrue(TEXT("a waiting payday reads full"), Payday && Payday->Fill01 == 1.0f);
		TestNull(TEXT("and takes Debt's line"), Find(Meters, TEXT("Hud.Debt")));
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Quicksilver);
		H.Bless(Blessing(TEXT("Quicksilver.Momentum"), TEXT("Key")));
		H.StepOnce();
		H.Weapons.GrantMomentum(0, 10.0f, true);
		const TArray<FDTHudMeter> Meters = Read(H);
		const FDTHudMeter* Momentum = Find(Meters, TEXT("Hud.Momentum"));
		if (TestNotNull(TEXT("Momentum shows"), Momentum))
		{
			TestEqual(TEXT("its value"), Momentum->Value, 10.0f);
			TestEqual(TEXT("its cap"), Momentum->Max, 30.0f);
			TestTrue(TEXT("Quicksilver's token"), Momentum->Color == DTPal::Keeper_Quicksilver);
		}
		FDTKeeperMeterRead Open;
		Open.bMomentum = true;
		Open.Momentum = 40.0f;
		TArray<FDTHudMeter> OpenMeters;
		DTKeeperMeters::Build(Open, 0.0f, 0.0f, OpenMeters);
		const FDTHudMeter* NoCap = Find(OpenMeters, TEXT("Hud.MomentumOpen"));
		TestTrue(TEXT("no cap, no bar"), NoCap && NoCap->Fill01 < 0.0f);
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Shade);
		H.Bless(Blessing(TEXT("Shade.Hunters"), TEXT("Key")));
		H.StepOnce();
		H.Weapons.RaiseHunters(0, 2, FVector2f::ZeroVector, 6.0f, 0.25f);
		const TArray<FDTHudMeter> Meters = Read(H);
		const FDTHudMeter* Shades = Find(Meters, TEXT("Hud.Shades"));
		if (TestNotNull(TEXT("Shades show"), Shades))
		{
			TestEqual(TEXT("two walking"), Shades->Value, 2.0f);
			TestTrue(TEXT("of the cap"), Shades->Max >= 2.0f);
		}
	}
	{
		FDTKeeperHarness H;
		H.Patron(EDTPath::Quicksilver);
		H.Keepers.QueueIntercession(0);
		H.StepMany(2);
		const TArray<FDTHudMeter> Meters = Read(H);
		const FDTHudMeter* Cast = Find(Meters, TEXT("Hud.Dilation"));
		if (TestNotNull(TEXT("the cast shows"), Cast))
		{
			TestTrue(TEXT("seconds left"), Cast->Value > 3.0f && Cast->Value <= static_cast<float>(DTConst::INTERCESSION_QUICKSILVER_DURATION_S));
			TestTrue(TEXT("running down"), Cast->Fill01 > 0.9f && Cast->Fill01 <= 1.0f);
		}
		H.StepMany(FMath::RoundToInt(DTConst::INTERCESSION_QUICKSILVER_DURATION_S * Second));
		TestNull(TEXT("gone with the cast"), Find(Read(H), TEXT("Hud.Dilation")));
	}
	return true;
}

DT_DILATION_TEST(FDTHudText, "Hud.Text") // T-KEE-38
{
	FString Csv;
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("DT/Text/ST_UI.csv"));
	if (!TestTrue(TEXT("ST_UI reads"), FFileHelper::LoadFileToString(Csv, *Path)))
	{
		return false;
	}
	const FCsvParser Parser(Csv);
	TMap<FString, TPair<FString, FString>> ByKey;
	for (const TArray<const TCHAR*>& Row : Parser.GetRows())
	{
		if (Row.Num() >= 3)
		{
			ByKey.Add(Row[0], TPair<FString, FString>(Row[1], Row[2]));
		}
	}
	for (const TCHAR* Key : { TEXT("Hud.Debt"), TEXT("Hud.Payday"), TEXT("Hud.Momentum"), TEXT("Hud.MomentumOpen"), TEXT("Hud.Shades"), TEXT("Hud.Dilation") })
	{
		const TPair<FString, FString>* Text = ByKey.Find(Key);
		if (TestNotNull(Key, Text))
		{
			TestFalse(*(FString(Key) + TEXT(" en")), Text->Key.IsEmpty());
			TestFalse(*(FString(Key) + TEXT(" zh-Hans")), Text->Value.IsEmpty());
			TestEqual(*(FString(Key) + TEXT(" args")), Text->Key.Contains(TEXT("{Value}")), Text->Value.Contains(TEXT("{Value}")));
			TestEqual(*(FString(Key) + TEXT(" max")), Text->Key.Contains(TEXT("{Max}")), Text->Value.Contains(TEXT("{Max}")));
		}
	}
	return true;
}

DT_DILATION_TEST(FDTHudContrast, "Hud.Contrast") // T-KEE-38
{
	using namespace DTDilationTest;
	const float Back = Luminance(DTPal::UI_Panel);
	for (const FColor& Fill : { DTPal::Keeper_Shade, DTPal::Keeper_Quicksilver })
	{
		const float Ratio = (Luminance(Fill) + 0.05f) / (Back + 0.05f);
		TestTrue(*FString::Printf(TEXT("%s holds 3:1 on the panel: %.2f"), *Fill.ToHex(), Ratio), Ratio >= 3.0f);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
