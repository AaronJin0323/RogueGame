// Impact tap test (02_Systems/impact_art.md T-IMP-01): drives FDTEnemyPool directly with a hand-built view and run state, and
// reads the read-only step list ApplyHits writes for presentation and audio (R-IMP-01).

#include "Misc/AutomationTest.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTElite.h"
#include "Enemies/DTEnemyPool.h"
#include "Enemies/DTEnemyTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTImpactTapTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Hz = static_cast<float>(DTConst::SIM_FIXED_HZ);
	constexpr float Tolerance = 1.0e-3f;
	constexpr float HpScale = 50.0f; // no staged hit but the killing one ends its enemy
	const FVector2f FarAway(3500.0f, 3500.0f);

	/** Counts Hit and Kill events, in order. */
	struct FEventCount : public IDTRunEventSink
	{
		int32 Hits = 0;
		int32 Kills = 0;
		virtual void OnRunEvent(const FDTGameEvent& Event, bool bRouted) override
		{
			Hits += Event.Kind == EDTGameEventKind::Hit ? 1 : 0;
			Kills += Event.Kind == EDTGameEventKind::Kill ? 1 : 0;
		}
	};

	static int32 StageEnemy(FDTEnemyPool& Pool, EDTEnemyRole Role, const FVector2f& At, bool bElite = false, uint8 Mask = 0)
	{
		FDTEnemySpawn Spec;
		Spec.Role = Role;
		Spec.Position = At;
		Spec.HpScale = HpScale;
		Spec.bElite = bElite;
		Spec.AffixMask = Mask;
		return Pool.Spawn(Spec, nullptr, false);
	}

	/** One staged hit on Index with its current serial; damage as a share of the enemy's full health. */
	static FDTHit MakeHit(const FDTEnemyPool& Pool, int32 Index, float Share, uint16 Source, int8 Slot)
	{
		FDTHit Hit;
		Hit.EnemyIndex = Index;
		Hit.EnemySerial = Pool.GetSerials()[Index];
		Hit.OwnerId = 0;
		Hit.Damage = Share * Pool.GetMaxHp()[Index];
		Hit.Position = Pool.GetPositions()[Index];
		Hit.SourceId = Source;
		Hit.WeaponSlot = Slot;
		return Hit;
	}

	/** What one record must hold: the hit it came from (index in the view) and what the pool must have made of it. */
	struct FExpected
	{
		int32 HitIndex = INDEX_NONE;
		float Dealt = 0.0f;
		uint8 Flags = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTImpactTap, "Dusktracer.Impact.Tap", DTImpactTapTest::Flags) // T-IMP-01
bool FDTImpactTap::RunTest(const FString& Parameters)
{
	using namespace DTImpactTapTest;
	FDTEnemyPool Pool;
	FDTRunState Run;
	Run.Start(1, 1);
	FDTWorldView View;
	View.HeroPositions = { FarAway };
	View.HeroIntangible = { false };
	FDTSimContext Context;
	Context.StepSeconds = 1.0f / Hz;
	Context.Run = &Run;
	Context.View = &View;

	const uint8 Shielded = DTElite::Bit(EDTAffix::Shielded);
	const uint8 Armored = DTElite::Bit(EDTAffix::Armored);
	const int32 Plain = StageEnemy(Pool, EDTEnemyRole::Swarm, FVector2f(-1200.0f, 0.0f));
	const int32 Killed = StageEnemy(Pool, EDTEnemyRole::Swarm, FVector2f(-900.0f, 0.0f));
	const int32 Tank = StageEnemy(Pool, EDTEnemyRole::Tank, FVector2f(-600.0f, 0.0f));
	const int32 Shield = StageEnemy(Pool, EDTEnemyRole::Swarm, FVector2f(-300.0f, 0.0f), true, Shielded);
	const int32 Armor = StageEnemy(Pool, EDTEnemyRole::Swarm, FVector2f(0.0f, 0.0f), true, Armored);
	const int32 Shocked = StageEnemy(Pool, EDTEnemyRole::Swarm, FVector2f(300.0f, 0.0f));
	const int32 Critted = StageEnemy(Pool, EDTEnemyRole::Swarm, FVector2f(600.0f, 0.0f));
	const int32 Field = StageEnemy(Pool, EDTEnemyRole::Swarm, FVector2f(900.0f, 0.0f));
	const int32 Burning = StageEnemy(Pool, EDTEnemyRole::Swarm, FVector2f(1200.0f, 0.0f));
	if (!TestEqual(TEXT("every enemy staged"), Pool.Num(), 9))
	{
		return false;
	}

	// Burn on one enemy; step until the Enemies phase appends its first tick, so the staged step holds one.
	FDTStatusApply Burn;
	Burn.Handle = Pool.GetHandle(Burning);
	Burn.Status = EDTStatus::Burn;
	Burn.Magnitude = 10.0f;
	Burn.OwnerId = 0;
	Burn.SourceId = 9;
	TestTrue(TEXT("Burn applied"), Pool.ApplyStatus(Burn));
	int64 Step = 0;
	bool bTicked = false;
	for (int32 Guard = 0; Guard < FMath::CeilToInt32(DTConst::STATUS_BURN_S * Hz) && !bTicked; ++Guard)
	{
		Context.StepIndex = Step++;
		Pool.StepEnemies(Context);
		bTicked = View.Hits.Num() > 0;
		if (!bTicked)
		{
			Pool.ResolveHits(Context);
		}
	}
	if (!TestTrue(TEXT("StepEnemies made a Burn tick"), bTicked) || !TestEqual(TEXT("one Burn tick"), View.Hits.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("the tick is on the burning enemy"), View.Hits[0].EnemyIndex, Burning);

	// The other phases' hits, appended after the Enemies phase as Weapons and Bullets do.
	Pool.SetFacing(Tank, FVector2f(1.0f, 0.0f));
	FDTStatusApply Shock;
	Shock.Handle = Pool.GetHandle(Shocked);
	Shock.Status = EDTStatus::Shock;
	Shock.OwnerId = 0;
	TestTrue(TEXT("Shock applied"), Pool.ApplyStatus(Shock));
	TArray<FExpected> Expected;
	const FDTHit BurnTick = View.Hits[0]; // a copy: the list grows below
	Expected.Add({ 0, FMath::Min(BurnTick.Damage, Pool.GetHp()[Burning]), DTImpactFlags::StatusTick });

	View.Hits.Add(MakeHit(Pool, Plain, 0.1f, 1, 0));
	Expected.Add({ View.Hits.Num() - 1, View.Hits.Last().Damage, 0 });
	View.Hits.Add(MakeHit(Pool, Plain, 0.2f, 2, 1));
	Expected.Add({ View.Hits.Num() - 1, View.Hits.Last().Damage, 0 });
	View.Hits.Add(MakeHit(Pool, Killed, 2.0f, 3, 2));
	Expected.Add({ View.Hits.Num() - 1, Pool.GetHp()[Killed], DTImpactFlags::Kill });
	View.Hits.Add(MakeHit(Pool, Killed, 0.1f, 4, 2)); // after the kill: ignored
	FDTHit Stale = MakeHit(Pool, Plain, 0.1f, 5, 0);
	Stale.EnemySerial = Pool.GetSerials()[Plain] + 1000; // names an enemy that is gone: ignored (R-ENM-23)
	View.Hits.Add(Stale);
	const FVector2f Front = Pool.GetPositions()[Tank] + FVector2f(Pool.GetRadii()[Tank], 0.0f);
	FDTHit Plate = MakeHit(Pool, Tank, 0.1f, 6, 0);
	Plate.Position = Front;
	View.Hits.Add(Plate);
	Expected.Add({ View.Hits.Num() - 1, Plate.Damage * DTConst::ROLE_TANK_FRONT_DAMAGE_MULT, DTImpactFlags::Plate });
	FDTHit Pierce = Plate;
	Pierce.SourceId = 7;
	Pierce.bPierce = true;
	View.Hits.Add(Pierce);
	Expected.Add({ View.Hits.Num() - 1, Pierce.Damage, DTImpactFlags::Pierce });
	View.Hits.Add(MakeHit(Pool, Shield, 0.3f, 8, 1));
	Expected.Add({ View.Hits.Num() - 1, 0.0f, static_cast<uint8>(DTImpactFlags::Shield | DTImpactFlags::Elite) });
	View.Hits.Add(MakeHit(Pool, Armor, 0.1f, 10, 0));
	Expected.Add({ View.Hits.Num() - 1, View.Hits.Last().Damage * DTConst::AFFIX_ARMORED_DAMAGE_TAKEN, DTImpactFlags::Elite });
	View.Hits.Add(MakeHit(Pool, Shocked, 0.1f, 11, 0));
	Expected.Add({ View.Hits.Num() - 1, View.Hits.Last().Damage * (1.0f + DTConst::STATUS_SHOCK_DAMAGE_TAKEN), 0 });
	FDTHit Crit = MakeHit(Pool, Critted, 0.1f, 12, 2);
	Crit.bCrit = true;
	View.Hits.Add(Crit);
	Expected.Add({ View.Hits.Num() - 1, Crit.Damage, DTImpactFlags::Crit });
	FDTHit Tick = MakeHit(Pool, Field, 0.05f, 13, INDEX_NONE);
	Tick.bFieldTick = true;
	View.Hits.Add(Tick);
	Expected.Add({ View.Hits.Num() - 1, Tick.Damage, DTImpactFlags::FieldTick });

	// What the pool held before the resolve: the records must copy it.
	const TArray<FDTHit> Hits = View.Hits;
	const TArray<FVector2f> Positions(Pool.GetPositions());
	const TArray<float> Radii(Pool.GetRadii());
	const TArray<float> MaxHps(Pool.GetMaxHp());
	const TArray<float> HpBefore(Pool.GetHp());
	const TArray<uint32> Serials(Pool.GetSerials());
	const TArray<EDTEnemyRole> Roles(Pool.GetRoles());

	FEventCount Events;
	Run.SetEventSink(&Events);
	Context.StepIndex = Step++;
	const int32 Kills = Pool.ResolveHits(Context);
	Run.SetEventSink(nullptr);

	const TConstArrayView<FDTImpact> Impacts = Pool.GetStepImpacts();
	TestEqual(TEXT("one record per applied hit, none for the two ignored"), Impacts.Num(), Expected.Num());
	TestEqual(TEXT("as many records as Hit events"), Impacts.Num(), Events.Hits);
	int32 Killing = 0;
	float DealtSum = 0.0f;
	TArray<uint32> KillSerials;
	for (int32 Index = 0; Index < FMath::Min(Impacts.Num(), Expected.Num()); ++Index)
	{
		const FDTImpact& Impact = Impacts[Index];
		const FExpected& Want = Expected[Index];
		const FDTHit& Hit = Hits[Want.HitIndex];
		const int32 Slot = Hit.EnemyIndex;
		const auto At = [Index](const TCHAR* Field) { return FString::Printf(TEXT("record %d: %s"), Index, Field); };
		TestEqual(*At(TEXT("contact point")), Impact.Position, Hit.Position);
		TestEqual(*At(TEXT("hull centre")), Impact.EnemyPosition, Positions[Slot]);
		TestEqual(*At(TEXT("radius")), Impact.EnemyRadius, Radii[Slot], Tolerance);
		TestEqual(*At(TEXT("dealt")), Impact.Dealt, Want.Dealt, Tolerance);
		TestEqual(*At(TEXT("full health")), Impact.MaxHp, MaxHps[Slot], Tolerance);
		TestEqual(*At(TEXT("serial")), Impact.EnemySerial, Serials[Slot]);
		TestEqual(*At(TEXT("owner")), Impact.OwnerId, Hit.OwnerId);
		TestEqual(*At(TEXT("source")), static_cast<int32>(Impact.SourceId), static_cast<int32>(Hit.SourceId));
		TestEqual(*At(TEXT("weapon slot")), static_cast<int32>(Impact.WeaponSlot), static_cast<int32>(Hit.WeaponSlot));
		TestEqual(*At(TEXT("role")), static_cast<int32>(Impact.Role), static_cast<int32>(Roles[Slot]));
		TestEqual(*At(TEXT("flags, and none unstaged")), static_cast<int32>(Impact.Flags), static_cast<int32>(Want.Flags));
		DealtSum += Impact.Dealt;
		if ((Impact.Flags & DTImpactFlags::Kill) != 0)
		{
			++Killing;
			KillSerials.Add(Impact.EnemySerial);
		}
	}
	TestEqual(TEXT("one Kill record per Kill event"), Killing, Events.Kills);
	TestEqual(TEXT("one Kill record per death"), Killing, Kills);
	TestEqual(TEXT("one Kill record per FDTKill"), Killing, View.Kills.Num());
	for (int32 Index = 0; Index < FMath::Min(KillSerials.Num(), View.Kills.Num()); ++Index)
	{
		TestEqual(TEXT("Kill records in the order of View.Kills"), KillSerials[Index], View.Kills[Index].EnemySerial);
	}

	// Dealt sums to the health the pool removed (by serial: the resolve compacted the dead away).
	float Removed = 0.0f;
	for (int32 Before = 0; Before < Serials.Num(); ++Before)
	{
		const int32 After = Pool.GetSerials().IndexOfByKey(Serials[Before]);
		Removed += HpBefore[Before] - (After == INDEX_NONE ? 0.0f : FMath::Max(0.0f, Pool.GetHp()[After]));
	}
	TestEqual(TEXT("Dealt sums to the health removed"), DealtSum, Removed, Tolerance * Expected.Num());

	// The next ResolveHits replaces the list; the Burn range went with its step, so a hit at index 0 is no status tick.
	const int32 Again = Pool.GetSerials().IndexOfByKey(Serials[Plain]);
	if (TestTrue(TEXT("the plain enemy lives"), Again != INDEX_NONE))
	{
		View.Hits.Add(MakeHit(Pool, Again, 0.1f, 14, 0));
		Context.StepIndex = Step++;
		Pool.ResolveHits(Context);
		TestEqual(TEXT("the next resolve replaces the list"), Pool.GetStepImpacts().Num(), 1);
		if (Pool.GetStepImpacts().Num() == 1)
		{
			TestEqual(TEXT("with its own hit"), static_cast<int32>(Pool.GetStepImpacts()[0].SourceId), 14);
			TestEqual(TEXT("and no flag"), static_cast<int32>(Pool.GetStepImpacts()[0].Flags), 0);
		}
	}
	Context.StepIndex = Step++;
	Pool.ResolveHits(Context);
	TestEqual(TEXT("a step with no hit leaves it empty"), Pool.GetStepImpacts().Num(), 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
