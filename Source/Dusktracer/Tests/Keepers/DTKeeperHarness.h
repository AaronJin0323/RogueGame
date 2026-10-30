// The Keeper test harness (T-KEE-*): the weapon harness plus the Keeper sim, shared by every Keeper test file.
#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Tests/Combat/DTWeaponTestHarness.h"
#include "Core/DTCatalogGen.h"
#include "Keepers/DTKeeperSim.h"
#include "Keepers/DTKeeperSpec.h"
#include "Offers/DTOfferGenerator.h"

namespace DTKeeperTest
{
	using namespace DTWeaponTestHarness;

	static const FName PulseRifle(TEXT("Weapon.PulseRifle"));
	static const FName RingEmitter(TEXT("Weapon.RingEmitter"));
	static const FName OrbitalDrones(TEXT("Weapon.OrbitalDrones"));
	static const FName ArcBlade(TEXT("Weapon.ArcBlade"));

	static FName Blessing(const TCHAR* Archetype, const TCHAR* Column)
	{
		return FName(*FString::Printf(TEXT("Blessing.%s.%s"), Archetype, Column));
	}

	static int32 Row(const TCHAR* Archetype)
	{
		return DTKeepers::FindArchetype(FName(*FString::Printf(TEXT("Archetype.%s"), Archetype)));
	}

	/** The weapon harness plus the Keeper sim, in 07_Technical §4 phase order (enemies apply statuses and pulls, never move). */
	struct FDTKeeperHarness
	{
		FDTRunState Run;
		FDTWorldView View;
		FDTEnemyPool Enemies;
		FDTBulletPool Bullets;
		FDTWeaponSim Weapons;
		FDTKeeperSim Keepers;
		int64 Step = 0;
		TArray<FDTHit> HitLog;
		TArray<FDTStatusApply> StatusLog;
		TArray<FDTDisplacement> MoveLog;

		explicit FDTKeeperHarness(int32 Seed = 7, int32 NumPlayers = 1)
		{
			Run.Start(Seed, NumPlayers);
			Weapons.SetEnemies(&Enemies);
			View.HeroPositions.Init(FVector2f::ZeroVector, Run.GetNumPlayers());
			View.HeroAimDirs.Init(FVector2f(1.0f, 0.0f), Run.GetNumPlayers());
			View.HeroIntangible.Init(false, Run.GetNumPlayers());
			View.HeroHurt.Init(false, Run.GetNumPlayers());
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

		FDTPlayerRunState& P(int32 Owner = 0) { return Run.GetPlayer(Owner); }

		void Patron(EDTPath Path, int32 Owner = 0, bool bWeapon = true)
		{
			P(Owner).Keepers.Primary = Path;
			P(Owner).Offer.PatronPath = Path;
			if (bWeapon)
			{
				Weapons.Give(Run, Owner, PulseRifle, 1); // D-015: the run starts with the Pulse Rifle
			}
			Keepers.RebuildFlags(Run, Owner, Weapons);
		}

		void Commit(EDTPath Path, int32 Owner = 0)
		{
			P(Owner).Keepers.Second = Path;
			P(Owner).Keepers.bCommitDone = true;
			P(Owner).Offer.SecondPath = Path;
			Keepers.RebuildFlags(Run, Owner, Weapons);
		}

		bool Bless(FName Id, int32 Owner = 0)
		{
			const bool bOk = DTOffers::AddBlessing(P(Owner).Keepers, Id);
			Keepers.RebuildFlags(Run, Owner, Weapons);
			return bOk;
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

		/** A 5 x 5 block of tough enemies ahead of the hero (within the cleave and slam reach). */
		void AddBlock(float NearX = 200.0f, float Spacing = 45.0f)
		{
			for (int32 X = 0; X < 5; ++X)
			{
				for (int32 Y = -2; Y <= 2; ++Y)
				{
					AddEnemy(FVector2f(NearX + Spacing * X, Spacing * Y));
				}
			}
		}

		void StepOnce()
		{
			const FDTSimContext Ctx = Context();
			// Enemies: queued statuses and displacements first (no movement in these tests).
			for (const FDTStatusApply& Apply : View.StatusApplies)
			{
				StatusLog.Add(Apply);
				Enemies.ApplyStatus(Apply);
			}
			View.StatusApplies.Reset();
			for (const FDTDisplacement& Move : View.Displacements)
			{
				MoveLog.Add(Move);
				if (Enemies.IsHandleValid(FDTEnemyHandle{ Move.EnemyIndex, Move.EnemySerial }))
				{
					Enemies.SetPosition(Move.EnemyIndex, Enemies.GetPositions()[Move.EnemyIndex] + Move.Delta);
				}
			}
			View.Displacements.Reset();
			Enemies.Publish(View);
			Weapons.StepWeapons(Ctx, &Bullets);
			Bullets.Step(Dt, View);
			Weapons.ObserveBullets(Ctx);
			Keepers.CaptureHits(Ctx, &Enemies);
			HitLog.Append(View.Hits);
			Enemies.ResolveHits(Ctx);
			Enemies.Publish(View);
			Weapons.StepEffects(Ctx, &Bullets);
			Keepers.StepEffects(Ctx, &Weapons);
			++Step;
		}

		void StepMany(int32 Steps)
		{
			for (int32 Index = 0; Index < Steps; ++Index)
			{
				StepOnce();
			}
		}

		double Damage(uint16 Source) const
		{
			double Sum = 0.0;
			for (const FDTHit& Hit : HitLog)
			{
				Sum += Hit.SourceId == Source ? Hit.Damage : 0.0;
			}
			return Sum;
		}

		double TotalDamage() const
		{
			double Sum = 0.0;
			for (const FDTHit& Hit : HitLog)
			{
				Sum += Hit.Damage;
			}
			return Sum;
		}

		/** Keeper share of damage by source (Tools/power_split.py reads the same split from telemetry). */
		double KeeperShare(EDTPath Path) const
		{
			const double Total = TotalDamage();
			return Total > 0.0 ? Damage(DTKeepers::KeeperSource(Path)) / Total : 0.0;
		}

		int32 CountHits(uint16 Source) const
		{
			int32 Count = 0;
			for (const FDTHit& Hit : HitLog)
			{
				Count += Hit.SourceId == Source ? 1 : 0;
			}
			return Count;
		}

		int32 CountStatuses(EDTStatus Status, uint16 Source) const
		{
			int32 Count = 0;
			for (const FDTStatusApply& Apply : StatusLog)
			{
				Count += (Apply.Status == Status && Apply.SourceId == Source) ? 1 : 0;
			}
			return Count;
		}

		int32 FrozenEnemies() const
		{
			int32 Count = 0;
			for (int32 Index = 0; Index < Enemies.Num(); ++Index)
			{
				Count += Enemies.GetAux(Index).IsFrozen() ? 1 : 0;
			}
			return Count;
		}

		/** Crafts Count player hits on enemy 0 (weapon slot 0) and runs only capture + effects (statistical procs). */
		void CraftedStep(int32 Count, float Damage, int32 Owner = 0)
		{
			const FDTSimContext Ctx = Context();
			Enemies.Publish(View);
			View.Hits.Reset();
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FDTHit& Hit = View.Hits.AddDefaulted_GetRef();
				Hit.EnemyIndex = 0;
				Hit.EnemySerial = View.EnemySerials[0];
				Hit.OwnerId = Owner;
				Hit.Damage = Damage;
				Hit.WeaponSlot = 0;
			}
			Keepers.CaptureHits(Ctx, &Enemies);
			View.Hits.Reset();
			Keepers.StepEffects(Ctx, nullptr);
			++Step;
		}

		const FDTFlagEntry* Built(EDTPath Path, DTCat::EFlag Flag, int32 Owner = 0) const
		{
			return Keepers.GetBuiltFlags(Owner, Path).Entries.FindByPredicate([Flag](const FDTFlagEntry& E)
			{
				return E.Flag == Flag && E.Params.GetWord(DTFlagParam::On).IsNone();
			});
		}
	};

	/** Steps a harness for a few seconds of fire. */
	constexpr int32 FireSteps = 600;
}

#endif // WITH_DEV_AUTOMATION_TESTS
