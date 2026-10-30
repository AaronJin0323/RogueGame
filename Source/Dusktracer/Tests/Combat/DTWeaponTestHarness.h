// A headless fixed-step loop for weapon and modifier tests: the real enemy pool, bullet pool and weapon sim stepped in
// 07_Technical §4 phase order, without a world. Enemies never move here, so any position change would be a bug.
#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/DTBulletPool.h"
#include "Combat/DTWeaponSim.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Enemies/DTEnemyPool.h"
#include "Misc/AutomationTest.h"

namespace DTWeaponTestHarness
{
	constexpr EAutomationTestFlags TestFlags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr float Dt = 1.0f / static_cast<float>(DTConst::SIM_FIXED_HZ);
	/** Enemy HP for tests that must not kill (far above any S1 weapon's damage). */
	constexpr float ToughHp = 1.0e9f;

	struct FDTHarness
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTEnemyPool Enemies;
		FDTBulletPool Bullets;
		FDTWeaponSim Weapons;
		int64 Step = 0;
		bool bResolve = true;
		/** Every hit of every step, as ResolveHits saw it. */
		TArray<FDTHit> HitLog;
		/** Kills to hand Effects next step (the enemy package fills View.Kills; these tests do it by hand). */
		TArray<FDTKill> PendingKills;

		explicit FDTHarness(int32 Seed = 7, int32 NumPlayers = 1)
		{
			Run.Start(Seed, NumPlayers);
			Weapons.SetEnemies(&Enemies);
			View.HeroPositions.Init(FVector2f::ZeroVector, Run.GetNumPlayers());
			View.HeroAimDirs.Init(FVector2f(1.0f, 0.0f), Run.GetNumPlayers());
			View.HeroIntangible.Init(false, Run.GetNumPlayers());
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

		int32 AddEnemy(const FVector2f& Position, float Hp = ToughHp)
		{
			const int32 Index = Enemies.Spawn(EDTEnemyRole::Swarm, Position);
			if (Index != INDEX_NONE)
			{
				Enemies.SetHp(Index, Hp);
			}
			return Index;
		}

		/** Weapons -> Bullets -> (observe) -> ResolveHits -> Effects, as in the sim. */
		void StepOnce()
		{
			const FDTSimContext Ctx = Context();
			Enemies.Publish(View);
			Weapons.StepWeapons(Ctx, &Bullets);
			Bullets.Step(Dt, View);
			Weapons.ObserveBullets(Ctx);
			HitLog.Append(View.Hits);
			if (bResolve)
			{
				Enemies.ResolveHits(Ctx);
			}
			else
			{
				View.Hits.Reset();
			}
			Enemies.Publish(View);
			View.Kills = PendingKills;
			PendingKills.Reset();
			Weapons.StepEffects(Ctx, &Bullets);
			++Step;
		}

		void StepMany(int32 Steps)
		{
			for (int32 Index = 0; Index < Steps; ++Index)
			{
				StepOnce();
			}
		}

		int32 CountHitsFrom(uint16 Source) const
		{
			int32 Count = 0;
			for (const FDTHit& Hit : HitLog)
			{
				Count += Hit.SourceId == Source ? 1 : 0;
			}
			return Count;
		}
	};
}

#endif // WITH_DEV_AUTOMATION_TESTS
