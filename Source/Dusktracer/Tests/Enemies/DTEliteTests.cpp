// Elite tests (02_Systems/elites.md T-ELT-01..13). Drive FDTEnemyPool directly with a hand-built view and run state.

#include "Misc/AutomationTest.h"
#include "Core/DTCatalogGen.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRngStreams.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTElite.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemySubsystem.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Telemetry/DTTelemetryCore.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTEliteTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Hz = static_cast<float>(DTConst::SIM_FIXED_HZ);
	const FVector2f FarAway(3500.0f, 3500.0f);

	static int32 Steps(float Seconds) { return FMath::RoundToInt32(Seconds * Hz); }

	/** Counts Hit events (R-ELT-08: an absorbed hit still reports one). */
	struct FEventCount : public IDTRunEventSink
	{
		int32 Hits = 0;
		virtual void OnRunEvent(const FDTGameEvent& Event, bool bRouted) override { Hits += Event.Kind == EDTGameEventKind::Hit ? 1 : 0; }
	};

	struct FHarness
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTSimContext Context;
		int64 Step = 0;

		explicit FHarness(const FVector2f& Hero = FarAway)
		{
			Run.Start(1, 1);
			View.HeroPositions = { Hero };
			View.HeroIntangible = { false };
			Context.StepSeconds = 1.0f / Hz;
			Context.Run = &Run;
			Context.View = &View;
		}

		void Tick(FDTEnemyPool& Pool)
		{
			Context.StepIndex = Step++;
			Pool.StepEnemies(Context);
			Pool.ResolveHits(Context);
		}

		void Ticks(FDTEnemyPool& Pool, int32 Count)
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Tick(Pool);
			}
		}

		int32 HeroHits(EDTHeroHitKind Kind, float* OutDamage = nullptr) const
		{
			int32 Count = 0;
			for (const FDTHeroHit& Hit : View.HeroHits)
			{
				if (Hit.Kind == Kind)
				{
					++Count;
					if (OutDamage)
					{
						*OutDamage = Hit.Damage;
					}
				}
			}
			return Count;
		}

		const FDTTelegraph* Find(EDTTelegraphShape Shape) const
		{
			return View.Telegraphs.FindByPredicate([Shape](const FDTTelegraph& T) { return T.Shape == Shape; });
		}

		/** One hit resolved now. */
		void Hit(FDTEnemyPool& Pool, int32 Enemy, float Damage, int8 Slot = 0, const FVector2f* At = nullptr)
		{
			FDTHit Out;
			Out.EnemyIndex = Enemy;
			Out.EnemySerial = Pool.GetSerials()[Enemy];
			Out.OwnerId = 0;
			Out.Damage = Damage;
			Out.Position = At ? *At : Pool.GetPositions()[Enemy];
			Out.WeaponSlot = Slot;
			View.Hits.Add(Out);
			Context.StepIndex = Step;
			Pool.ResolveHits(Context);
		}
	};

	static int32 Elite(FDTEnemyPool& Pool, EDTEnemyRole Role, const FVector2f& At, uint8 Mask, float Hp = 1.0f, float Dmg = 1.0f, bool bDirector = false, int32 Beat = INDEX_NONE)
	{
		FDTEnemySpawn Spec;
		Spec.Role = Role;
		Spec.Position = At;
		Spec.bElite = true;
		Spec.AffixMask = Mask;
		Spec.HpScale = Hp;
		Spec.DmgScale = Dmg;
		Spec.BeatId = Beat;
		return Pool.Spawn(Spec, nullptr, bDirector);
	}

	static FDTStatusApply Burn(const FDTEnemyPool& Pool, int32 Enemy, float Dps)
	{
		FDTStatusApply Apply;
		Apply.Handle = Pool.GetHandle(Enemy);
		Apply.Status = EDTStatus::Burn;
		Apply.Magnitude = Dps;
		Apply.OwnerId = 0;
		return Apply;
	}

	constexpr uint8 Armored = DTElite::Bit(EDTAffix::Armored);
	constexpr uint8 Swift = DTElite::Bit(EDTAffix::Swift);
	constexpr uint8 Volatile = DTElite::Bit(EDTAffix::Volatile);
	constexpr uint8 Splitter = DTElite::Bit(EDTAffix::Splitter);
	constexpr uint8 Vampiric = DTElite::Bit(EDTAffix::Vampiric);
	constexpr uint8 Shielded = DTElite::Bit(EDTAffix::Shielded);
	constexpr uint8 AllAffixes = Armored | Swift | Volatile | Splitter | Vampiric | Shielded;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltSpawn, "Dusktracer.Elites.Spawn", DTEliteTest::Flags) // T-ELT-01
bool FDTEltSpawn::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	FDTEnemyPool Pool;
	const int32 First = Elite(Pool, EDTEnemyRole::Tank, FVector2f(0.0f, 0.0f), Armored);
	const int32 Second = Elite(Pool, EDTEnemyRole::Rusher, FVector2f(500.0f, 0.0f), Volatile | Shielded);
	FDTEnemySpawn Plain;
	Plain.Role = EDTEnemyRole::Swarm;
	Plain.AffixMask = Armored;
	const int32 Third = Pool.Spawn(Plain, nullptr, false);
	TestEqual(TEXT("an elite keeps its mask"), static_cast<int32>(Pool.GetAux(First).AffixMask), static_cast<int32>(Armored));
	TestEqual(TEXT("a mask on a non-elite is dropped"), static_cast<int32>(Pool.GetAux(Third).AffixMask), 0);
	TestEqual(TEXT("the role's radius stands"), Pool.GetRadii()[First], FDTEnemyRoleDef::Get(EDTEnemyRole::Tank).Radius);

	FHarness H;
	H.Hit(Pool, First, 1.0e9f);
	TestEqual(TEXT("the mask moves with swap-removal"), static_cast<int32>(Pool.GetAux(0).AffixMask), 0);
	TestEqual(TEXT("two survivors"), Pool.Num(), 2);
	const int32 Rusher = Pool.GetRoles()[0] == EDTEnemyRole::Rusher ? 0 : 1;
	TestEqual(TEXT("the survivor keeps its mask"), static_cast<int32>(Pool.GetAux(Rusher).AffixMask), static_cast<int32>(Volatile | Shielded));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltRoll, "Dusktracer.Elites.Roll", DTEliteTest::Flags) // T-ELT-02
bool FDTEltRoll::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	FRandomStream Rng(7);
	for (int32 Index = 0; Index < 400; ++Index)
	{
		const uint8 Mask = DTElite::Roll(EDTEnemyRole::Turret, 1 + Index % 2, 0.0f, Rng);
		if (FMath::CountBits(Mask) != static_cast<uint64>(1 + Index % 2) || DTElite::Has(Mask, EDTAffix::Swift))
		{
			AddError(FString::Printf(TEXT("turret roll %d gave mask %d"), Index, Mask));
			break;
		}
	}
	const auto FavouredShare = [&Rng](float Weight)
	{
		int32 Favoured = 0;
		const int32 Rolls = 6000;
		for (int32 Index = 0; Index < Rolls; ++Index)
		{
			const uint8 Mask = DTElite::Roll(EDTEnemyRole::Tank, 1, Weight, Rng);
			Favoured += (Mask & (Swift | Volatile | Shielded)) != 0 ? 1 : 0;
		}
		return static_cast<float>(Favoured) / Rolls;
	};
	const float Plain = FavouredShare(0.0f);
	const float Weighted = FavouredShare(static_cast<float>(DTConst::ELITE_FAVORED_WEIGHT));
	TestTrue(FString::Printf(TEXT("no favour: half (%.3f)"), Plain), FMath::IsNearlyEqual(Plain, 0.5f, 0.03f));
	TestTrue(FString::Printf(TEXT("favoured rows weigh the factor (%.3f)"), Weighted), FMath::IsNearlyEqual(Weighted, 0.75f, 0.03f));

	FDTRngStreams A(99);
	FDTRngStreams B(99);
	for (int32 Index = 0; Index < 16; ++Index)
	{
		DTElite::Roll(EDTEnemyRole::Tank, 2, 3.0f, B.Get(EDTRngStream::Elite));
	}
	TestEqual(TEXT("rolls leave the Director stream alone"), A.Get(EDTRngStream::Director).GetUnsignedInt(), B.Get(EDTRngStream::Director).GetUnsignedInt());
	TestEqual(TEXT("rolls leave the Spawn stream alone"), A.Get(EDTRngStream::Spawn).GetUnsignedInt(), B.Get(EDTRngStream::Spawn).GetUnsignedInt());

	TArray<int32> Counts;
	TArray<float> Favours;
	for (const DTCat::FTimelineRow& Row : DTCat::TimelineArena1)
	{
		if (Row.Event == DTCat::ETimelineEvent::EliteWave)
		{
			Counts.Add(Row.EventAffixes);
			Favours.Add(Row.EventFavor);
		}
	}
	if (TestEqual(TEXT("three elite waves"), Counts.Num(), 3))
	{
		TestEqual(TEXT("first wave one affix"), Counts[0], 1);
		TestEqual(TEXT("second wave one affix"), Counts[1], 1);
		TestEqual(TEXT("last wave two affixes"), Counts[2], 2);
		TestEqual(TEXT("the last wave is the favoured one"), Favours[2], static_cast<float>(DTConst::ELITE_FAVORED_WEIGHT));
		TestEqual(TEXT("the first is not"), Favours[0], 0.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltArmored, "Dusktracer.Elites.Armored", DTEliteTest::Flags) // T-ELT-03
bool FDTEltArmored::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	const float Share = DTConst::AFFIX_ARMORED_DAMAGE_TAKEN;
	{
		FDTEnemyPool Pool;
		FHarness H;
		const int32 E = Elite(Pool, EDTEnemyRole::Swarm, FVector2f::ZeroVector, Armored, 100.0f);
		const float Before = Pool.GetHp()[E];
		H.Hit(Pool, E, 10.0f);
		TestEqual(TEXT("a weapon hit deals the share"), Before - Pool.GetHp()[E], 10.0f * Share, 0.01f);
		const float Mid = Pool.GetHp()[E];
		H.Hit(Pool, E, 10.0f, INDEX_NONE);
		TestEqual(TEXT("a slotless hit deals the share"), Mid - Pool.GetHp()[E], 10.0f * Share, 0.01f);
		const float BeforeBurn = Pool.GetHp()[E];
		H.View.StatusApplies.Add(Burn(Pool, E, 10.0f));
		H.Ticks(Pool, Steps(DTConst::STATUS_BURN_S) + 2);
		TestEqual(TEXT("Burn deals the share"), BeforeBurn - Pool.GetHp()[0], 10.0f * DTConst::STATUS_BURN_S * Share, 0.1f);
	}
	{
		FDTEnemyPool Pool;
		FHarness H;
		const int32 E = Elite(Pool, EDTEnemyRole::Tank, FVector2f::ZeroVector, Armored, 100.0f);
		Pool.SetFacing(E, FVector2f(1.0f, 0.0f));
		const FVector2f Front(50.0f, 0.0f);
		const float Before = Pool.GetHp()[E];
		H.Hit(Pool, E, 10.0f, 0, &Front);
		TestEqual(TEXT("the plate multiplies with it"), Before - Pool.GetHp()[E], 10.0f * DTConst::ROLE_TANK_FRONT_DAMAGE_MULT * Share, 0.01f);
		FDTStatusApply Shock = Burn(Pool, E, 0.0f);
		Shock.Status = EDTStatus::Shock;
		Pool.ApplyStatus(Shock);
		const float Mid = Pool.GetHp()[E];
		H.Hit(Pool, E, 10.0f, 0, &Front);
		TestEqual(TEXT("Shock multiplies with both"), Mid - Pool.GetHp()[E],
			10.0f * DTConst::ROLE_TANK_FRONT_DAMAGE_MULT * Share * (1.0f + DTConst::STATUS_SHOCK_DAMAGE_TAKEN), 0.01f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltSwift, "Dusktracer.Elites.Swift", DTEliteTest::Flags) // T-ELT-04
bool FDTEltSwift::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	FDTEnemyPool Pool;
	FHarness H(FVector2f(-400.0f, 0.0f));
	const int32 E = Elite(Pool, EDTEnemyRole::Rusher, FVector2f(0.0f, 0.0f), Swift);
	TestEqual(TEXT("speed is multiplied"), Pool.GetSpeeds()[E], FDTEnemyRoleDef::Get(EDTEnemyRole::Rusher).Speed * DTConst::AFFIX_SWIFT_SPEED_MULT, 0.01f);
	H.Tick(Pool);
	const FDTEnemyAux& A = Pool.GetAux(E);
	TestEqual(TEXT("in wind-up"), static_cast<int32>(A.State), static_cast<int32>(EDTEnemyState::WindUp));
	const FVector2f Start = A.ChargeOrigin;
	const float Band = A.ChargeLength;
	H.Ticks(Pool, Steps(DTConst::ROLE_RUSHER_WINDUP_S + DTConst::ROLE_RUSHER_CHARGE_S) + 2);
	TestEqual(TEXT("the charge still covers its band"), FVector2f::Distance(Pool.GetPositions()[E], Start), Band, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltVolatile, "Dusktracer.Elites.Volatile", DTEliteTest::Flags) // T-ELT-05
bool FDTEltVolatile::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	const float Radius = static_cast<float>(DTConst::AFFIX_VOLATILE_RADIUS_UU);
	const float Touch = Radius + static_cast<float>(DTConst::HERO_RADIUS_UU);
	const int32 Lead = Steps(DTConst::AFFIX_VOLATILE_LEAD_S);
	const auto Run = [&](const FVector2f& Hero, bool bByBurn, int32& OutBlasts, float& OutDamage)
	{
		FDTEnemyPool Pool;
		FHarness H(Hero);
		const FVector2f At(100.0f, 0.0f);
		const int32 E = Elite(Pool, EDTEnemyRole::Turret, At, Volatile, 1.0f, 4.0f);
		Pool.SetFireEnabled(false);
		if (bByBurn)
		{
			Pool.SetHp(E, 0.1f);
			H.View.StatusApplies.Add(Burn(Pool, E, 100.0f));
			H.Ticks(Pool, Steps(DTConst::STATUS_BURN_TICK_S) + 1);
		}
		else
		{
			H.Hit(Pool, E, 1.0e9f);
		}
		TestEqual(TEXT("dead"), Pool.Num(), 0);
		TestEqual(TEXT("one blast"), Pool.GetEliteBlasts().Num(), 1);
		OutBlasts = 0;
		for (int32 Index = 0; Index < Lead + 2; ++Index)
		{
			H.Tick(Pool);
			if (Index + 1 < Lead - (bByBurn ? 1 : 0))
			{
				const FDTTelegraph* T = H.Find(EDTTelegraphShape::Ring);
				if (TestNotNull(TEXT("the ring stands for the lead"), T) && T)
				{
					TestEqual(TEXT("radius"), T->Length, Radius);
					TestEqual(TEXT("filled"), T->Width, 0.0f);
					TestEqual(TEXT("at the death"), T->Origin, At);
				}
				TestEqual(TEXT("no damage before the lead"), H.HeroHits(EDTHeroHitKind::Blast), 0);
			}
			OutBlasts += H.HeroHits(EDTHeroHitKind::Blast, &OutDamage);
		}
		TestEqual(TEXT("gone after it lands"), Pool.GetEliteBlasts().Num(), 0);
	};
	int32 Blasts = 0;
	float Damage = 0.0f;
	Run(FVector2f(100.0f + Touch - 1.0f, 0.0f), false, Blasts, Damage);
	TestEqual(TEXT("a hero touching takes one blast"), Blasts, 1);
	TestEqual(TEXT("unscaled"), Damage, static_cast<float>(DTConst::AFFIX_VOLATILE_DAMAGE));
	Run(FVector2f(100.0f + Touch - 1.0f, 0.0f), true, Blasts, Damage);
	TestEqual(TEXT("death by Burn blasts too"), Blasts, 1);
	Run(FVector2f(100.0f + Touch + 1.0f, 0.0f), false, Blasts, Damage);
	TestEqual(TEXT("a hero outside takes none"), Blasts, 0);
	for (const DTCat::FAffixRow& Row : DTCat::Affixes)
	{
		if (FCString::Strlen(Row.Attack) > 0)
		{
			TestTrue(FString::Printf(TEXT("%s shows the minimum lead"), Row.Id), Row.Lead_s >= DTConst::ENEMIES_TELEGRAPH_LEAD_MIN_S);
			TestTrue(FString::Printf(TEXT("%s names a telegraph"), Row.Id), FCString::Strlen(Row.Telegraph) > 0);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltSplitter, "Dusktracer.Elites.Splitter", DTEliteTest::Flags) // T-ELT-06
bool FDTEltSplitter::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	const int32 Count = DTConst::AFFIX_SPLITTER_COUNT;
	{
		FDTEnemyPool Pool;
		FHarness H;
		const FVector2f At(200.0f, 0.0f);
		const int32 E = Elite(Pool, EDTEnemyRole::Tank, At, Splitter, 2.0f, 3.0f, true, 7);
		H.Hit(Pool, E, 1.0e9f);
		TestEqual(TEXT("no child in the step of the death"), Pool.Num(), 0);
		H.Tick(Pool);
		if (TestEqual(TEXT("children on the next Enemies phase"), Pool.Num(), Count))
		{
			for (int32 Index = 0; Index < Count; ++Index)
			{
				TestEqual(TEXT("a Swarm"), static_cast<int32>(Pool.GetRoles()[Index]), static_cast<int32>(EDTEnemyRole::Swarm));
				TestEqual(TEXT("the elite's HP scale"), Pool.GetHpScales()[Index], 2.0f);
				TestEqual(TEXT("the elite's damage scale"), Pool.GetDmgScales()[Index], 3.0f);
				TestEqual(TEXT("swarm HP times its scale"), Pool.GetMaxHp()[Index], FDTEnemyRoleDef::Get(EDTEnemyRole::Swarm).MaxHp * 2.0f);
				TestEqual(TEXT("not elite"), Pool.GetFlags()[Index] & DTEnemyFlags::Elite, 0);
				TestEqual(TEXT("no beat tag"), Pool.GetBeatIds()[Index], INDEX_NONE);
				TestTrue(TEXT("a director parent's children drop"), (Pool.GetFlags()[Index] & DTEnemyFlags::Director) != 0);
				TestTrue(TEXT("around the death"), FVector2f::Distance(Pool.GetPositions()[Index], At) <= static_cast<float>(DTConst::ENM_SEPARATION_RADIUS_UU) + 30.0f);
			}
		}
	}
	{
		FDTEnemyPool Pool;
		FHarness H;
		const int32 E = Elite(Pool, EDTEnemyRole::Swarm, FVector2f::ZeroVector, Splitter);
		H.Hit(Pool, E, 1.0e9f);
		H.Tick(Pool);
		TestEqual(TEXT("children of a debug elite"), Pool.Num(), Count);
		TestEqual(TEXT("drop nothing"), Pool.GetFlags()[0] & DTEnemyFlags::Director, 0);
	}
	{
		FDTEnemyPool Pool;
		FHarness H;
		const int32 E = Elite(Pool, EDTEnemyRole::Swarm, FVector2f::ZeroVector, Splitter);
		while (Pool.Num() < Pool.GetCapacity())
		{
			Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(-2000.0f, 0.0f));
		}
		H.Hit(Pool, E, 1.0e9f);
		const int64 Refused = Pool.GetRefusedSpawns();
		H.Tick(Pool);
		TestEqual(TEXT("the cap holds"), Pool.Num(), Pool.GetCapacity());
		TestEqual(TEXT("the rest are refused and counted"), Pool.GetRefusedSpawns(), Refused + Count - 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltVampiric, "Dusktracer.Elites.Vampiric", DTEliteTest::Flags) // T-ELT-07
bool FDTEltVampiric::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	const int32 Link = Steps(DTConst::AFFIX_VAMPIRIC_LINK_S);
	const int32 Tick = Steps(DTConst::AFFIX_VAMPIRIC_TICK_S);
	const float Range = static_cast<float>(DTConst::AFFIX_VAMPIRIC_RANGE_UU);
	const FVector2f Near(Range - 50.0f, 0.0f);
	const float MaxHp = FDTEnemyRoleDef::Get(EDTEnemyRole::Turret).MaxHp * DTConst::ROLE_ELITE_HP_MULT;
	{
		FDTEnemyPool Pool;
		Pool.SetFireEnabled(false);
		FHarness H(Near);
		const int32 E = Elite(Pool, EDTEnemyRole::Turret, FVector2f::ZeroVector, Vampiric, 1.0f, 2.0f);
		Pool.SetHp(E, MaxHp * 0.5f);
		for (int32 Index = 0; Index < Link - 1; ++Index)
		{
			H.Tick(Pool);
			TestEqual(TEXT("no drain before the full link"), H.HeroHits(EDTHeroHitKind::Drain), 0);
			const FDTTelegraph* T = H.Find(EDTTelegraphShape::AimLine);
			if (TestNotNull(TEXT("the cable stands"), T) && T)
			{
				TestEqual(TEXT("the cable reaches the hero"), T->Length, Near.Size(), 0.5f);
				TestEqual(TEXT("cable width"), T->Width, static_cast<float>(DTConst::AFFIX_VAMPIRIC_CABLE_UU));
			}
		}
		H.Tick(Pool);
		float Damage = 0.0f;
		TestEqual(TEXT("the first drain lands at the link's end"), H.HeroHits(EDTHeroHitKind::Drain, &Damage), 1);
		const float Expected = static_cast<float>(DTConst::AFFIX_VAMPIRIC_DRAIN_DPS) * DTConst::AFFIX_VAMPIRIC_TICK_S * 2.0f;
		TestEqual(TEXT("scaled by DmgScale"), Damage, Expected, 0.01f);
		TestEqual(TEXT("one heal"), Pool.GetHp()[E], MaxHp * (0.5f + DTConst::AFFIX_VAMPIRIC_HEAL_RATIO), 0.01f);
		TestNotNull(TEXT("the cable stays while it drains"), H.Find(EDTTelegraphShape::AimLine));
		int32 Drains = 0;
		for (int32 Index = 0; Index < Tick - 1; ++Index)
		{
			H.Tick(Pool);
			Drains += H.HeroHits(EDTHeroHitKind::Drain);
		}
		TestEqual(TEXT("nothing between ticks"), Drains, 0);
		H.Tick(Pool);
		TestEqual(TEXT("the next tick"), H.HeroHits(EDTHeroHitKind::Drain), 1);
		Pool.SetHp(E, MaxHp - 0.5f);
		H.Ticks(Pool, Tick);
		TestEqual(TEXT("never above max"), Pool.GetHp()[E], MaxHp);
		TestEqual(TEXT("healed is recorded"), Pool.GetAux(E).Healed > 0.0f, true);

		H.View.HeroPositions[0] = FarAway;
		H.Tick(Pool);
		TestNull(TEXT("leaving range drops the cable"), H.Find(EDTTelegraphShape::AimLine));
		H.View.HeroPositions[0] = Near;
		int32 Early = 0;
		for (int32 Index = 0; Index < Link - 1; ++Index)
		{
			H.Tick(Pool);
			Early += H.HeroHits(EDTHeroHitKind::Drain);
		}
		TestEqual(TEXT("coming back takes the full link"), Early, 0);
	}
	{
		FDTEnemyPool Pool;
		Pool.SetFireEnabled(false);
		FHarness H(Near);
		const int32 E = Elite(Pool, EDTEnemyRole::Turret, FVector2f::ZeroVector, Vampiric);
		Pool.SetHp(E, MaxHp * 0.5f);
		H.View.HeroIntangible[0] = true;
		int32 Drains = 0;
		for (int32 Index = 0; Index < Link + 2 * Tick; ++Index)
		{
			H.Tick(Pool);
			Drains += H.HeroHits(EDTHeroHitKind::Drain);
		}
		TestEqual(TEXT("an intangible hero takes no drain"), Drains, 0);
		TestEqual(TEXT("and feeds no heal"), Pool.GetHp()[E], MaxHp * 0.5f);
	}
	{
		FDTEnemyPool Pool;
		Pool.SetFireEnabled(false);
		FHarness H(Near);
		const int32 E = Elite(Pool, EDTEnemyRole::Turret, FVector2f::ZeroVector, Vampiric);
		H.Ticks(Pool, 3);
		const float Left = Pool.GetAux(E).DrainLinkLeft;
		TestTrue(TEXT("linking"), Left > 0.0f);
		FDTStatusApply Freeze = Burn(Pool, E, 0.0f);
		Freeze.Status = EDTStatus::Freeze;
		H.View.StatusApplies.Add(Freeze);
		H.Ticks(Pool, 5);
		TestEqual(TEXT("frozen holds the link"), Pool.GetAux(E).DrainLinkLeft, Left, 1.0e-4f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltShielded, "Dusktracer.Elites.Shielded", DTEliteTest::Flags) // T-ELT-08
bool FDTEltShielded::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	FDTEnemyPool Pool;
	FHarness H;
	FEventCount Events;
	H.Run.SetEventSink(&Events);
	const int32 E = Elite(Pool, EDTEnemyRole::Tank, FVector2f::ZeroVector, Shielded, 100.0f);
	const float Full = Pool.GetHp()[E];
	const int32 Count = DTConst::AFFIX_SHIELD_HITS;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		H.Hit(Pool, E, 1.0e4f + Index);
	}
	TestEqual(TEXT("every weapon hit absorbed, however large"), Pool.GetHp()[E], Full);
	TestEqual(TEXT("each counted once"), Pool.GetAux(E).ShieldAbsorbed, Count);
	TestEqual(TEXT("each still reports one Hit"), Events.Hits, Count);
	H.Hit(Pool, E, 10.0f);
	TestEqual(TEXT("after the count, full damage"), Full - Pool.GetHp()[E], 10.0f, 0.01f);

	FDTEnemyPool Second;
	const int32 F = Elite(Second, EDTEnemyRole::Tank, FVector2f::ZeroVector, Shielded, 100.0f);
	const float SecondFull = Second.GetHp()[F];
	H.Hit(Second, F, 10.0f, INDEX_NONE);
	TestEqual(TEXT("a slotless hit passes at once"), SecondFull - Second.GetHp()[F], 10.0f, 0.01f);
	TestEqual(TEXT("and spends no shield"), Second.GetAux(F).ShieldHitsLeft, Count);
	H.View.StatusApplies.Add(Burn(Second, F, 60.0f));
	const float BeforeBurn = Second.GetHp()[F];
	H.Ticks(Second, Steps(DTConst::STATUS_BURN_S));
	TestTrue(TEXT("Burn passes"), Second.GetHp()[0] < BeforeBurn);
	TestEqual(TEXT("and spends no shield"), Second.GetAux(0).ShieldHitsLeft, Count);
	H.Run.SetEventSink(nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltDeterministic, "Dusktracer.Elites.Deterministic", DTEliteTest::Flags) // T-ELT-09
bool FDTEltDeterministic::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	const auto Run = [](int32 Seed, TArray<float>& Out)
	{
		FRandomStream Rng(Seed);
		FDTEnemyPool Pool;
		FHarness H(FVector2f(100.0f, 0.0f));
		for (int32 Index = 0; Index < 12; ++Index)
		{
			Elite(Pool, EDTEnemyRole::Swarm, FVector2f(300.0f + 30.0f * Index, 80.0f * Index - 400.0f), DTElite::Roll(EDTEnemyRole::Swarm, 2, 3.0f, Rng));
		}
		for (int32 Step = 0; Step < 240; ++Step)
		{
			if (Step % 40 == 0 && Pool.Num() > 0)
			{
				H.Hit(Pool, 0, 1.0e9f);
			}
			H.Tick(Pool);
			Out.Add(static_cast<float>(Pool.Num()));
			Out.Add(static_cast<float>(H.View.HeroHits.Num()));
			Out.Add(static_cast<float>(H.View.Telegraphs.Num()));
			for (int32 Index = 0; Index < Pool.Num(); ++Index)
			{
				Out.Add(Pool.GetPositions()[Index].X);
				Out.Add(Pool.GetHp()[Index]);
			}
		}
	};
	TArray<float> A;
	TArray<float> B;
	TArray<float> C;
	Run(3, A);
	Run(3, B);
	Run(4, C);
	TestTrue(TEXT("same seed, same run"), A == B);
	TestTrue(TEXT("another seed differs"), A != C);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltLight, "Dusktracer.Pre.EliteLight", DTEliteTest::Flags) // T-ELT-10, T-PRE-04
bool FDTEltLight::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	const float Step = DTConst::ELITE_BLINK_STEP_S;
	TSet<FString> Patterns;
	for (int32 Affix = 0; Affix < static_cast<int32>(EDTAffix::Count); ++Affix)
	{
		const uint8 Mask = DTElite::Bit(static_cast<EDTAffix>(Affix));
		const int32 Length = FCString::Strlen(DTCat::Affixes[Affix].Light);
		FString Sampled;
		float Low = 1.0e9f;
		float High = 0.0f;
		for (int32 Index = 0; Index < Length; ++Index)
		{
			Sampled += FString::Printf(TEXT("%.2f "), DTElite::LightLevel(Mask, (Index + 0.5f) * Step));
			const float Emissive = DTElite::LightEmissive(Mask, (Index + 0.5f) * Step);
			Low = FMath::Min(Low, Emissive);
			High = FMath::Max(High, Emissive);
		}
		Patterns.Add(Sampled);
		TestTrue(TEXT("never below the optic"), Low >= static_cast<float>(DTConst::ENEMIES_OPTIC_EMISSIVE) - 0.001f);
		TestEqual(TEXT("reaches the top"), High, static_cast<float>(DTConst::ELITE_LIGHT_EMISSIVE), 0.001f);
	}
	TestEqual(TEXT("one pattern per affix"), Patterns.Num(), static_cast<int32>(EDTAffix::Count));
	TestEqual(TEXT("the lead is the most dangerous"), static_cast<int32>(DTElite::Lead(Armored | Volatile | Swift)), static_cast<int32>(EDTAffix::Volatile));
	TestEqual(TEXT("no affix, no lead"), static_cast<int32>(DTElite::Lead(0)), static_cast<int32>(EDTAffix::Count));
	TestEqual(TEXT("no affix holds the top"), DTElite::LightEmissive(0, 0.33f), static_cast<float>(DTConst::ELITE_LIGHT_EMISSIVE));

	FDTEnemyPool Pool;
	Elite(Pool, EDTEnemyRole::Swarm, FVector2f::ZeroVector, Vampiric);
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(500.0f, 0.0f));
	TArray<FDTElitePart> Parts;
	DTElite::BuildParts(Pool, 0.0f, 0.0f, 100.0f, Parts);
	const FDTElitePart* Light = Parts.FindByPredicate([](const FDTElitePart& P) { return P.Affix == EDTAffix::Count; });
	if (TestNotNull(TEXT("a light on the elite"), Light) && Light)
	{
		TestEqual(TEXT("Pal.Elite.Light"), Light->Color, DTPal::Linear(DTPal::Elite_Light));
	}
	TestEqual(TEXT("one light per elite"), Parts.FilterByPredicate([](const FDTElitePart& P) { return P.Affix == EDTAffix::Count; }).Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltParts, "Dusktracer.Elites.Parts", DTEliteTest::Flags) // T-ELT-11
bool FDTEltParts::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	const auto CountOf = [](const TArray<FDTElitePart>& Parts, EDTAffix Affix)
	{
		return Parts.FilterByPredicate([Affix](const FDTElitePart& P) { return P.Affix == Affix; }).Num();
	};
	FDTEnemyPool Pool;
	FHarness H;
	const int32 E = Elite(Pool, EDTEnemyRole::Tank, FVector2f::ZeroVector, AllAffixes, 100.0f);
	TArray<FDTElitePart> Parts;
	DTElite::BuildParts(Pool, 0.0f, 0.0f, 100.0f, Parts);
	TestEqual(TEXT("a plate on each side"), CountOf(Parts, EDTAffix::Armored), 2);
	TestEqual(TEXT("two vanes"), CountOf(Parts, EDTAffix::Swift), 2);
	TestEqual(TEXT("a reactor"), CountOf(Parts, EDTAffix::Volatile), 1);
	TestEqual(TEXT("docked drones"), CountOf(Parts, EDTAffix::Splitter), static_cast<int32>(DTConst::AFFIX_SPLITTER_COUNT));
	TestEqual(TEXT("a spike"), CountOf(Parts, EDTAffix::Vampiric), 1);
	TestEqual(TEXT("all plates up"), CountOf(Parts, EDTAffix::Shielded), static_cast<int32>(DTConst::AFFIX_SHIELD_PLATES));
	const FLinearColor Body = DTPal::Linear(DTPal::Enemy_Body);
	const FLinearColor Lit = DTPal::Linear(DTPal::Elite_Light);
	for (const FDTElitePart& Part : Parts)
	{
		const bool bDark = Part.Affix == EDTAffix::Armored || Part.Affix == EDTAffix::Swift || Part.Affix == EDTAffix::Splitter;
		TestEqual(TEXT("each part in its token"), Part.Color, bDark ? Body : Lit);
		TestTrue(TEXT("dark parts are not lit, lit parts are"), bDark ? Part.Emissive == 0.0f : Part.Emissive > 0.0f);
	}
	for (int32 Index = 0; Index < DTConst::AFFIX_SHIELD_HITS / 2; ++Index)
	{
		H.Hit(Pool, E, 1.0f);
	}
	DTElite::BuildParts(Pool, 0.0f, 0.0f, 100.0f, Parts);
	TestEqual(TEXT("half the hits, half the plates"), CountOf(Parts, EDTAffix::Shielded), DTConst::AFFIX_SHIELD_PLATES / 2);
	for (int32 Index = 0; Index < DTConst::AFFIX_SHIELD_HITS; ++Index)
	{
		H.Hit(Pool, E, 1.0f);
	}
	DTElite::BuildParts(Pool, 0.0f, 0.0f, 100.0f, Parts);
	TestEqual(TEXT("no plates once it breaks"), CountOf(Parts, EDTAffix::Shielded), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltLabel, "Dusktracer.Elites.Label", DTEliteTest::Flags) // T-ELT-12
bool FDTEltLabel::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	TArray<FString> Keys;
	DTElite::LabelKeys(Armored | Volatile, Keys);
	if (TestEqual(TEXT("two names"), Keys.Num(), 2))
	{
		TestEqual(TEXT("lead first"), Keys[0], FString(TEXT("Affix.Volatile.Name")));
		TestEqual(TEXT("then the other"), Keys[1], FString(TEXT("Affix.Armored.Name")));
	}
	FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(FName(TEXT("ST_Enemies")));
	if (TestTrue(TEXT("ST_Enemies is registered"), Table.IsValid()))
	{
		for (const DTCat::FAffixRow& Row : DTCat::Affixes)
		{
			TArray<FString> One;
			FString Key = Row.NameKey;
			Key.RemoveFromStart(TEXT("ST_Enemies."));
			TestTrue(*FString::Printf(TEXT("%s exists"), *Key), Table->FindEntry(Key).IsValid());
		}
		TestTrue(TEXT("Affix.Join exists"), Table->FindEntry(TEXT("Affix.Join")).IsValid());
		for (const DTCat::FEnemyRow& Row : DTCat::Enemies)
		{
			FString Key = Row.NameKey;
			Key.RemoveFromStart(TEXT("ST_Enemies."));
			TestTrue(*FString::Printf(TEXT("%s exists"), *Key), Table->FindEntry(Key).IsValid());
		}
	}
	TestFalse(TEXT("a label has text"), DTElite::LabelText(Armored).IsEmpty());

	FDTEnemyPool Pool;
	for (int32 Index = 0; Index < DTConst::ELITE_LABELS_MAX + 4; ++Index)
	{
		Elite(Pool, EDTEnemyRole::Swarm, FVector2f(100.0f * (Index + 1), 0.0f), Armored);
	}
	Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(10.0f, 0.0f));
	TArray<int32> Labelled;
	DTElite::LabelledElites(Pool, FVector2f::ZeroVector, Labelled);
	TestEqual(TEXT("capped"), Labelled.Num(), static_cast<int32>(DTConst::ELITE_LABELS_MAX));
	if (Labelled.Num() > 0)
	{
		TestEqual(TEXT("nearest elite first, plain enemies never"), Pool.GetPositions()[Labelled[0]].X, 100.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTEltTelemetry, "Dusktracer.Elites.Telemetry", DTEliteTest::Flags) // T-ELT-13
bool FDTEltTelemetry::RunTest(const FString& Parameters)
{
	using namespace DTEliteTest;
	FDTTelemetryCore Records;
	FDTTelemetryConfig Config;
	Records.Start(Config);
	FString Csv;
	FString Jsonl;
	Records.TakeOutput(Csv, Jsonl);

	FDTEnemyPool Pool;
	FHarness H;
	const int32 E = Elite(Pool, EDTEnemyRole::Tank, FVector2f::ZeroVector, Shielded | Armored, 1.0f, 1.0f, false, 4);
	H.Ticks(Pool, 30);
	H.Hit(Pool, E, 1.0f);
	const int32 Plain = Pool.Spawn(EDTEnemyRole::Swarm, FVector2f(100.0f, 0.0f));
	H.Hit(Pool, Plain, 1.0e9f);
	UDTEnemySubsystem::ForwardEliteDeaths(Pool, Records);
	Records.TakeOutput(Csv, Jsonl);
	TestFalse(TEXT("a plain death writes no record"), Jsonl.Contains(TEXT("\"elite\"")));

	for (int32 Index = 0; Index < DTConst::AFFIX_SHIELD_HITS; ++Index)
	{
		H.Hit(Pool, 0, 1.0f);
	}
	H.Hit(Pool, 0, 1.0e9f);
	TestEqual(TEXT("one elite death"), Pool.GetStepEliteDeaths().Num(), 1);
	UDTEnemySubsystem::ForwardEliteDeaths(Pool, Records);
	Records.TakeOutput(Csv, Jsonl);
	TestTrue(TEXT("elite record"), Jsonl.Contains(TEXT("\"elite\"")));
	TestTrue(TEXT("names the role"), Jsonl.Contains(TEXT("\"role\":\"Tank\"")));
	TestTrue(TEXT("names the affixes"), Jsonl.Contains(TEXT("Armored")) && Jsonl.Contains(TEXT("Shielded")));
	TestTrue(TEXT("time alive"), Jsonl.Contains(TEXT("\"alive_s\":0.5")));
	TestTrue(TEXT("absorbed hits"), Jsonl.Contains(FString::Printf(TEXT("\"shield_absorbed\":%d"), DTConst::AFFIX_SHIELD_HITS)));
	TestTrue(TEXT("its beat"), Jsonl.Contains(TEXT("\"beat\":4")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
