// DT.PerfScene S2 [NoDuoFields|NoReactions] [Hp=<x>]: the S2 perf proof scene (Docs/Validation/S2_perf.md; R-TEC-12: not in Shipping).
// A fixed build owning every S2 system, a crowd held at the cap and a hero walking through it; the bullet cap is met by filler rounds
// that hit and deal nothing (DTPerfScene::FillerRound). Every frame it writes what fired to the CSV capture as DT/S2Sum_* (monotonic:
// the capture reads last minus first) and DT/S2Live_* (the capture reads the median); Tools/Perf.ps1 -Scene S2 judges them.
// NoDuoFields drops the two duo cards that open fields (the control of the emit-cap reading); NoReactions leaves Reaction.Duos out of
// the pool, so the proof must fail. Hp= replaces C.PERF.S2_CROWD_HP_SCALE (the sweep of S2_perf.md).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Debug/DTPerfScene.h"

#include "Combat/DTBulletSubsystem.h"
#include "Combat/DTReforge.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Core/DTSimTypes.h"
#include "Dusktracer.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Enemies/DTElite.h"
#include "Enemies/DTEnemySubsystem.h"
#include "HAL/IConsoleManager.h"
#include "Keepers/DTKeeperSubsystem.h"
#include "Keepers/DTReactions.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHero.h"
#include "Player/DTHeroSubsystem.h"
#include "Progression/DTGemSubsystem.h"
#include "Telemetry/DTTelemetrySources.h"

FDTBulletSpawn DTPerfScene::FillerRound(float Angle)
{
	FDTBulletSpawn Spawn;
	Spawn.Radius = static_cast<float>(DTConst::BULLETS_STRESS_RADIUS_UU);
	Spawn.Damage = 0.0f;
	Spawn.Life = static_cast<float>(DTConst::BULLETS_STRESS_LIFE_S);
	Spawn.OwnerId = INDEX_NONE;
	Spawn.Pierce = DTConst::BULLETS_STRESS_PIERCE;
	Spawn.SourceId = FDTTelemetrySources::Stress;
	Spawn.Velocity = FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * static_cast<float>(DTConst::BULLETS_STRESS_SPEED_UU_S);
	return Spawn;
}

namespace DTDebugPerfScene
{
	/** One weapon of the build, at the top level: at most C.WEAPON.SLOTS, the Rifle first as the Patron gives it. */
	struct FWeaponLine
	{
		const TCHAR* Id;
		const TCHAR* Evolve;
		const TCHAR* Mods[3];
	};
	static const FWeaponLine Build[] =
	{
		{ TEXT("Weapon.PulseRifle"), nullptr, { TEXT("Mod.SplitCore"), TEXT("Mod.Pierce"), TEXT("Mod.Mirror") } },
		{ TEXT("Weapon.RingEmitter"), TEXT("Evolve.RingEmitter"), { TEXT("Mod.LingeringField"), TEXT("Mod.ScatterShell"), TEXT("Mod.Conduit") } },
		{ TEXT("Weapon.SeekerSwarm"), TEXT("Evolve.SeekerSwarm"), { TEXT("Mod.Pierce"), TEXT("Mod.Spillover"), TEXT("Mod.LingeringField") } },
		{ TEXT("Weapon.SpiralProjector"), nullptr, { TEXT("Mod.Pierce"), TEXT("Mod.Backwash"), TEXT("Mod.Spillover") } }
	};
	/** One archetype of every Keeper, every card of it. Forge and Tide are the two that enchant: Burn and Chill are what Steam reads. */
	static const TCHAR* const Archetypes[] =
	{
		TEXT("Archetype.Forge.Furnace"), TEXT("Archetype.Tide.Frost"), TEXT("Archetype.Storm.ArcGrid"),
		TEXT("Archetype.Shade.Hound"), TEXT("Archetype.Radiance.Prism"), TEXT("Archetype.Quicksilver.Momentum")
	};
	static const TCHAR* const PathRows[] =
	{
		TEXT("Transform.Storm"), TEXT("Transform.Forge"), TEXT("Transform.Tide"), TEXT("Transform.Shade"), TEXT("Transform.Radiance"), TEXT("Transform.Quicksilver")
	};
	static const EDTEnemyRole EliteRoles[] = { EDTEnemyRole::Swarm, EDTEnemyRole::Tank, EDTEnemyRole::Rusher, EDTEnemyRole::Turret };

	static void Sum(const FString& Name, int64 Value)
	{
		FCsvProfiler::RecordCustomStat(FName(*(TEXT("S2Sum_") + Name)), CSV_CATEGORY_INDEX(DT), static_cast<int32>(Value), ECsvCustomStatOp::Set);
	}

	static void Live(const FString& Name, int32 Value)
	{
		FCsvProfiler::RecordCustomStat(FName(*(TEXT("S2Live_") + Name)), CSV_CATEGORY_INDEX(DT), Value, ECsvCustomStatOp::Set);
	}

	class FScene : public IDTSimSystem
	{
	public:
		TWeakObjectPtr<UWorld> World;
		bool bNoDuoFields = false;
		bool bNoReactions = false;
		float HpScale = static_cast<float>(DTConst::PERF_S2_CROWD_HP_SCALE);

		void LatchInput()
		{
			UWorld* W = World.Get();
			UDTHeroSubsystem* Hero = W ? W->GetSubsystem<UDTHeroSubsystem>() : nullptr;
			const FDTHeroState* State = Hero ? Hero->GetState(0) : nullptr;
			if (!State)
			{
				return;
			}
			// The pawn latches its own empty input every frame (as the bot's mute, UDTBotSubsystem).
			for (TActorIterator<ADTHero> It(W); It; ++It)
			{
				It->SetActorTickEnabled(false);
			}
			// A circle round the origin: the tangent, with a pull onto the radius. Each dash as soon as it is ready.
			const float Orbit = static_cast<float>(DTConst::PERF_S2_HERO_ORBIT_UU);
			const FVector2f Out = State->Pos.IsNearlyZero() ? FVector2f(1.0f, 0.0f) : State->Pos.GetSafeNormal();
			FDTHeroInput Input;
			Input.Move = (FVector2f(-Out.Y, Out.X) + Out * ((Orbit - State->Pos.Size()) / Orbit)).GetSafeNormal();
			Input.bDashPress = State->DashCharges > 0;
			Hero->LatchInput(0, Input);
		}

		void Stand(FDTRunState& Run)
		{
			UWorld* W = World.Get();
			UDTWeaponSubsystem* Weapons = W->GetSubsystem<UDTWeaponSubsystem>();
			UDTKeeperSubsystem* Keepers = W->GetSubsystem<UDTKeeperSubsystem>();
			UDTEnemySubsystem* Enemies = W->GetSubsystem<UDTEnemySubsystem>();
			UDTHeroSubsystem* Hero = W->GetSubsystem<UDTHeroSubsystem>();
			UDTGemSubsystem* Gems = W->GetSubsystem<UDTGemSubsystem>();
			FDTPlayerRunState& Player = Run.GetPlayer(0);
			Hero->SetNoDamage(true);
			// The build is fixed: no XP, so no level-up hand changes it during the capture. Gems still drop, fly and land.
			FDTProgressionStats Progress = Gems->GetStats(0);
			Progress.XpGainMul = 0.0f;
			Gems->SetStats(0, Progress);

			if (!bNoReactions)
			{
				Weapons->Unlock(DTReactions::UnlockId());
			}
			Player.Keepers.Primary = EDTPath::Forge;
			Player.Keepers.Second = EDTPath::Tide;
			Player.Keepers.bCommitDone = true;
			Player.Offer.PatronPath = EDTPath::Forge;
			Player.Offer.SecondPath = EDTPath::Tide;
			for (const TCHAR* Id : Archetypes)
			{
				for (const DTCat::FArchetypeRow& Row : DTCat::Archetypes)
				{
					for (int32 Column = 0; FCString::Strcmp(Row.Id, Id) == 0 && Column < UE_ARRAY_COUNT(Row.BlessingIds); ++Column)
					{
						Keepers->Bless(0, FName(Row.BlessingIds[Column]));
					}
				}
			}
			for (int32 Row = 0; !bNoReactions && Row < DTCat::NumReactions; ++Row)
			{
				const bool bField = Row == static_cast<int32>(DTCat::EReaction::StormForge) || Row == static_cast<int32>(DTCat::EReaction::StormQuicksilver);
				if (!bNoDuoFields || !bField)
				{
					Keepers->Bless(0, FName(DTCat::Reactions[Row].BlessingId));
				}
			}
			for (const FWeaponLine& Line : Build)
			{
				const FName Id(Line.Id);
				EDTWeaponResult Result = Weapons->Give(0, Id, DTConst::WEAPON_LEVEL_MAX);
				if (Result == EDTWeaponResult::AlreadyOwned)
				{
					Result = Weapons->LevelUp(0, Id, DTConst::WEAPON_LEVEL_MAX);
				}
				UE_LOG(LogDT, Log, TEXT("PerfScene: %s result %d"), Line.Id, static_cast<int32>(Result));
				for (const TCHAR* Mod : Line.Mods)
				{
					Weapons->Unlock(FName(Mod));
					const EDTWeaponResult Socketed = Weapons->Socket(0, Id, FName(Mod));
					UE_LOG(LogDT, Log, TEXT("PerfScene: %s %s result %d"), Line.Id, Mod, static_cast<int32>(Socketed));
				}
				if (Line.Evolve)
				{
					Grant(Player, FName(Line.Evolve), Id);
				}
			}
			for (const TCHAR* Row : PathRows)
			{
				Grant(Player, FName(Row), NAME_None);
			}
			Enemies->SetKeepHpScale(HpScale);
			Enemies->SetKeepPopulation(DTConst::ENEMIES_CAP); // HoldElites queues first, so an elite takes the first slot a death frees
			UE_LOG(LogDT, Log, TEXT("PerfScene: build stands (duos %d, duo fields %d)"), bNoReactions ? 0 : 1, bNoDuoFields ? 0 : 1);
		}

		static void Grant(FDTPlayerRunState& Player, FName Id, FName Weapon)
		{
			FDTOfferCard Card;
			Card.Kind = EDTCardKind::Reforging;
			Card.ContentId = Id;
			Card.TargetWeapon = Weapon;
			const bool bOk = DTOffers::ApplyPickState(Player, EDTHandKind::Reforging, Card).bValid;
			UE_LOG(LogDT, Log, TEXT("PerfScene: %s %s"), *Id.ToString(), bOk ? TEXT("owned") : TEXT("REFUSED"));
		}

		void HoldElites(const FDTSimContext& Context)
		{
			UWorld* W = World.Get();
			UDTEnemySubsystem* Enemies = W->GetSubsystem<UDTEnemySubsystem>();
			if (Context.StepIndex % DTConst::SIM_FIXED_HZ != 0)
			{
				return;
			}
			const FVector2f Hero = Context.View && Context.View->HeroPositions.Num() ? Context.View->HeroPositions[0] : FVector2f::ZeroVector;
			int32 Elites = 0;
			for (const uint8 Flags : Enemies->GetPool().GetFlags())
			{
				Elites += (Flags & DTEnemyFlags::Elite) != 0 ? 1 : 0;
			}
			for (int32 Missing = DTConst::PERF_S2_ELITES - Elites; Missing > 0; --Missing)
			{
				// As tough as the crowd: every affix stands for the whole capture, not for the moment it lands.
				const float Angle = NextElite * static_cast<float>(UE_GOLDEN_RATIO) * static_cast<float>(UE_TWO_PI);
				FDTEnemySpawn Elite;
				Elite.Role = EliteRoles[NextElite % UE_ARRAY_COUNT(EliteRoles)];
				Elite.bElite = true;
				Elite.AffixMask = static_cast<uint8>(1 << (NextElite % static_cast<int32>(EDTAffix::Count)));
				Elite.HpScale = HpScale;
				Elite.Position = Hero + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * static_cast<float>(DTConst::ENM_SPAWN_RING_RADIUS_UU);
				Enemies->QueueSpawn(Elite);
				++NextElite;
			}
		}

		void Fill(const FDTSimContext& Context)
		{
			UWorld* W = World.Get();
			UDTBulletSubsystem* Bullets = W->GetSubsystem<UDTBulletSubsystem>();
			FDTBulletPool& Pool = Bullets->GetPool();
			const int32 Target = FMath::RoundToInt32(DTConst::BULLETS_CAP * static_cast<float>(DTConst::PERF_S2_FILL_RATIO));
			// Faster than the rounds expire: some are spent on the crowd, and the pool must stand at Target, not under it.
			constexpr float Refill = 2.0f;
			Carry += Refill * static_cast<float>(Target) / static_cast<float>(DTConst::BULLETS_STRESS_LIFE_S) * Context.StepSeconds;
			int32 Count = FMath::FloorToInt32(Carry);
			Carry -= static_cast<float>(Count);
			Count = FMath::Min(Count, FMath::Max(0, Target - Pool.Num()));
			const float TwoPi = static_cast<float>(UE_TWO_PI);
			for (int32 Shot = 0; Shot < Count; ++Shot)
			{
				Pool.Spawn(DTPerfScene::FillerRound(Phase + TwoPi * static_cast<float>(Shot) / static_cast<float>(Count)));
			}
			Phase = FMath::Fmod(Phase + FMath::DegreesToRadians(static_cast<float>(DTConst::BULLETS_STRESS_SPIN_DEG_S)) * Context.StepSeconds, TwoPi);
		}

		virtual void SimStep(const FDTSimContext& Context) override
		{
			UWorld* W = World.Get();
			if (!W || !Context.Run)
			{
				return;
			}
			if (!bStanding)
			{
				// The run-start Patron hand is picked by -unattended; its Rifle is the build's first weapon.
				if (Context.Run->GetPlayer(0).Keepers.Primary == EDTPath::None)
				{
					return;
				}
				Stand(*Context.Run);
				bStanding = true;
			}
			HoldElites(Context);
			Fill(Context);
		}

		virtual void SimRender(float Alpha) override
		{
			UWorld* W = World.Get();
			UDTSimSubsystem* Sim = W ? W->GetSubsystem<UDTSimSubsystem>() : nullptr;
			const UDTWeaponSubsystem* Weapons = W ? W->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
			const UDTEnemySubsystem* Enemies = W ? W->GetSubsystem<UDTEnemySubsystem>() : nullptr;
			const UDTHeroSubsystem* Hero = W ? W->GetSubsystem<UDTHeroSubsystem>() : nullptr;
			const UDTOfferSubsystem* Offers = W ? W->GetSubsystem<UDTOfferSubsystem>() : nullptr;
			if (!bStanding || !Sim || !Weapons || !Enemies || !Hero || !Offers)
			{
				return;
			}
			const FDTWeaponSim& Combat = Weapons->GetSim();
			for (int32 Row = 0; Row < DTCat::NumReactions; ++Row)
			{
				Sum(FString(TEXT("Fire_")) + DTCat::Reactions[Row].Pair, Combat.GetReactionCounters(0, Row).Fires);
			}
			const FDTReforgeRead Read = Combat.GetReforgeRead(0);
			Sum(TEXT("Lances"), Read.Lances);
			Sum(TEXT("Shades"), Read.Shades);
			Sum(TEXT("Rings"), Combat.GetFieldsOpened());
			Sum(TEXT("Discharges"), Read.Discharges);
			Sum(TEXT("Husks"), Read.Husks);
			Sum(TEXT("Pillars"), Read.Pillars);
			Sum(TEXT("Kills"), Enemies->GetTotalKills());
			Sum(TEXT("Steps"), Sim->GetStepIndex());
			const FDTOfferPlayerStats* Picks = Offers->GetStats(0);
			Sum(TEXT("Decisions"), Picks ? Picks->Decisions : 0);
			Sum(TEXT("Dashes"), Hero->GetState(0) ? Hero->GetState(0)->Dashes : 0);
			const TArray<FDTWeaponInstance>& Owned = Sim->GetRun().GetPlayer(0).Weapons;
			for (int32 Slot = 0; Slot < Owned.Num(); ++Slot)
			{
				const FDTWeaponCounters& C = Combat.GetCounters(0, Slot);
				Sum(FString::Printf(TEXT("Spawned%d"), Slot), C.Spawned);
				Sum(FString::Printf(TEXT("Throttled%d"), Slot), C.Throttled);
				Sum(FString::Printf(TEXT("DuoFields%d"), Slot), C.DuoFields);
				Sum(FString::Printf(TEXT("DuoFieldsThrottled%d"), Slot), C.DuoFieldsThrottled);
			}
			Live(TEXT("Rounds"), Combat.GetTrackedRounds());
			Live(TEXT("Fields"), Combat.GetLiveFields());
			Live(TEXT("Shades"), Combat.GetLiveShades(0));
			int32 Elites = 0;
			int32 Affixes[static_cast<int32>(EDTAffix::Count)] = {};
			const FDTEnemyPool& Pool = Enemies->GetPool();
			const TConstArrayView<uint8> Flags = Pool.GetFlags();
			for (int32 Index = 0; Index < Flags.Num(); ++Index)
			{
				if ((Flags[Index] & DTEnemyFlags::Elite) != 0)
				{
					++Elites;
					for (int32 Affix = 0; Affix < static_cast<int32>(EDTAffix::Count); ++Affix)
					{
						Affixes[Affix] += DTElite::Has(Pool.GetAux(Index).AffixMask, static_cast<EDTAffix>(Affix)) ? 1 : 0;
					}
				}
			}
			Live(TEXT("Elites"), Elites);
			for (int32 Affix = 0; Affix < static_cast<int32>(EDTAffix::Count); ++Affix)
			{
				Live(FString::Printf(TEXT("Affix%d"), Affix), Affixes[Affix]);
			}
		}

	private:
		bool bStanding = false;
		int32 NextElite = 0;
		float Carry = 0.0f;
		float Phase = 0.0f;
	};

	static TMap<TWeakObjectPtr<UWorld>, TUniquePtr<FScene>>& Scenes()
	{
		static TMap<TWeakObjectPtr<UWorld>, TUniquePtr<FScene>> Map;
		return Map;
	}

	static void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
	{
		TUniquePtr<FScene> Scene;
		if (Scenes().RemoveAndCopyValue(World, Scene))
		{
			if (UDTSimSubsystem* Sim = World->GetSubsystem<UDTSimSubsystem>())
			{
				Sim->Unregister(Scene.Get());
				Sim->OnStepEnd.RemoveAll(Scene.Get());
			}
		}
	}

	static void Start(const TArray<FString>& Args, UWorld* World)
	{
		UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
		if (!Sim || Args.Num() < 1 || !Args[0].Equals(TEXT("S2"), ESearchCase::IgnoreCase) || Scenes().Contains(World))
		{
			UE_LOG(LogDT, Warning, TEXT("DT.PerfScene S2 [NoDuoFields|NoReactions] [Hp=<x>]: game worlds only, once"));
			return;
		}
		static bool bCleanupBound = false;
		if (!bCleanupBound)
		{
			FWorldDelegates::OnWorldCleanup.AddStatic(&OnWorldCleanup);
			bCleanupBound = true;
		}
		TUniquePtr<FScene>& Scene = Scenes().Add(World, MakeUnique<FScene>());
		Scene->World = World;
		Scene->bNoDuoFields = Args.Contains(TEXT("NoDuoFields"));
		Scene->bNoReactions = Args.Contains(TEXT("NoReactions"));
		for (const FString& Arg : Args)
		{
			if (Arg.StartsWith(TEXT("Hp=")))
			{
				Scene->HpScale = FCString::Atof(*Arg.RightChop(3));
			}
		}
		Sim->Register(EDTSimPhase::Weapons, Scene.Get());
		Sim->OnStepEnd.AddRaw(Scene.Get(), &FScene::LatchInput);
		UE_LOG(LogDT, Log, TEXT("DT.PerfScene S2 started"));
	}

	static FAutoConsoleCommandWithWorldAndArgs StartCmd(TEXT("DT.PerfScene"),
		TEXT("DT.PerfScene S2 [NoDuoFields|NoReactions] [Hp=<x>]: the S2 perf proof scene (Tools/Perf.ps1 -Scene S2)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Start));
}

#endif // !UE_BUILD_SHIPPING
