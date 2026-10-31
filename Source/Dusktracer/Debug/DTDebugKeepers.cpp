// Keeper debug console commands (02_Systems/keepers.md; R-TEC-12: compiled out of Shipping).
//   DT.Bless <BlessingId> [player]   grant a blessing as an offer pick would (Owned, Favor, Tier, Leading) and rebuild flags
//   DT.Keeper.Stats                  per player Keepers, Favor, tiers, leading archetypes, built flags, procs, casts
//   DT.Intercede [player] [delay_s]  fill the player's Intercession meter and cast (through the hero's own activation)

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Containers/Ticker.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Keepers/DTKeeperSubsystem.h"
#include "Player/DTHeroSubsystem.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTDebugKeepers
{
	static UDTKeeperSubsystem* Find(UWorld* World)
	{
		UDTKeeperSubsystem* Keepers = World ? World->GetSubsystem<UDTKeeperSubsystem>() : nullptr;
		if (!Keepers)
		{
			UE_LOG(LogDT, Warning, TEXT("DT keepers: no Keeper subsystem in this world (Game/PIE worlds only)"));
		}
		return Keepers;
	}

	static int32 PlayerArg(const TArray<FString>& Args, int32 Index)
	{
		return Args.Num() > Index ? FCString::Atoi(*Args[Index]) : 0;
	}

	static void Bless(const TArray<FString>& Args, UWorld* World)
	{
		UDTKeeperSubsystem* Keepers = Find(World);
		if (!Keepers)
		{
			return;
		}
		if (Args.Num() < 1)
		{
			UE_LOG(LogDT, Log, TEXT("DT.Bless <BlessingId> [player], e.g. DT.Bless Blessing.Storm.Tempest.Key"));
			return;
		}
		FString Id = Args[0];
		if (!Id.StartsWith(TEXT("Blessing.")))
		{
			Id = TEXT("Blessing.") + Id;
		}
		const int32 Player = PlayerArg(Args, 1);
		const bool bOk = Keepers->Bless(Player, FName(*Id));
		UE_LOG(LogDT, Log, TEXT("DT.Bless %s player %d: %s"), *Id, Player, bOk ? TEXT("granted") : TEXT("refused (unknown id, owned, or no such player)"));
	}

	static FString FlagsText(const FDTWeaponFlags& Flags)
	{
		FString Out;
		for (const FDTFlagEntry& Entry : Flags.Entries)
		{
			Out += FString::Printf(TEXT(" %s{"), DTCat::FlagIds[static_cast<int32>(Entry.Flag)]);
			for (const FDTFlagParams::FNum& Num : Entry.Params.Nums)
			{
				Out += FString::Printf(TEXT("%s=%g;"), *Num.Name.ToString(), Num.Value);
			}
			for (const TPair<FName, FName>& Word : Entry.Params.Words)
			{
				Out += FString::Printf(TEXT("%s=%s;"), *Word.Key.ToString(), *Word.Value.ToString());
			}
			Out += TEXT("}");
		}
		return Out;
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		UDTKeeperSubsystem* Keepers = Find(World);
		UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
		if (!Keepers || !Sim)
		{
			return;
		}
		const FDTRunState& Run = Sim->GetRun();
		for (int32 Player = 0; Player < Run.GetNumPlayers(); ++Player)
		{
			const FDTKeeperRunState& K = Run.GetPlayer(Player).Keepers;
			const FDTKeeperCounters& C = Keepers->GetSim().GetCounters(Player);
			FString Owned;
			for (const FName& Id : K.Owned)
			{
				Owned += TEXT(" ") + Id.ToString();
			}
			UE_LOG(LogDT, Log, TEXT("DT.Keeper.Stats p%d primary=%s second=%s committed=%d owned=[%s ] casting=%d casts=%d rolled=%lld vuln_hits=%lld rebuilds=%d"),
				Player, FDTTelemetrySources::PathName(K.Primary), FDTTelemetrySources::PathName(K.Second), K.bCommitDone ? 1 : 0, *Owned,
				Keepers->GetSim().IsCasting(Player) ? 1 : 0, C.Casts, C.HitsRolled, C.FrozenVulnHits, C.Rebuilds);
			UE_LOG(LogDT, Log, TEXT("DT.Keeper.Stats p%d procs primary shock=%lld burn=%lld chill=%lld freeze=%lld; second shock=%lld burn=%lld chill=%lld freeze=%lld"),
				Player, C.PrimaryProcs[0], C.PrimaryProcs[1], C.PrimaryProcs[2], C.PrimaryProcs[3],
				C.SecondProcs[0], C.SecondProcs[1], C.SecondProcs[2], C.SecondProcs[3]);
			for (int32 P = 0; P < static_cast<int32>(EDTPath::Count); ++P)
			{
				const FDTWeaponFlags& Flags = Keepers->GetSim().GetBuiltFlags(Player, static_cast<EDTPath>(P));
				if (K.Favor[P] == 0 && Flags.Entries.Num() == 0)
				{
					continue;
				}
				UE_LOG(LogDT, Log, TEXT("DT.Keeper.Stats p%d %s favor=%d tier=%d leading=%s flags=[%s ]"), Player,
					FDTTelemetrySources::PathName(static_cast<EDTPath>(P)), K.Favor[P], K.Tier[P], *K.Leading[P].ToString(), *FlagsText(Flags));
			}
		}
	}

	/** The hero stats DT.Intercede swapped out, restored when the cast fires. */
	struct FPendingRestore
	{
		TWeakObjectPtr<UDTHeroSubsystem> Hero;
		int32 Player = INDEX_NONE;
		FDTHeroStats Stats;
		FDelegateHandle Handle;
	};
	static TArray<FPendingRestore> Pending;

	static void IntercedeNow(int32 Player, UWorld* World);

	static void Intercede(const TArray<FString>& Args, UWorld* World)
	{
		const int32 Player = PlayerArg(Args, 0);
		const float Delay = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 0.0f;
		if (Delay <= 0.0f)
		{
			IntercedeNow(Player, World);
			return;
		}
		// Captures and scripted runs: cast after Delay wall seconds (-ExecCmds run at start).
		TWeakObjectPtr<UWorld> WeakWorld(World);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakWorld, Player](float)
		{
			if (UWorld* W = WeakWorld.Get())
			{
				IntercedeNow(Player, W);
			}
			return false;
		}), Delay);
		UE_LOG(LogDT, Log, TEXT("DT.Intercede player %d: cast in %.1f s"), Player, Delay);
	}

	static void IntercedeNow(int32 Player, UWorld* World)
	{
		UDTKeeperSubsystem* Keepers = Find(World);
		if (!Keepers)
		{
			return;
		}
		UDTHeroSubsystem* Hero = World->GetSubsystem<UDTHeroSubsystem>();
		const FDTHeroState* State = Hero ? Hero->GetState(Player) : nullptr;
		if (!State)
		{
			// No hero (stress maps): cast directly.
			Keepers->Intercede(Player);
			UE_LOG(LogDT, Log, TEXT("DT.Intercede player %d: no hero; cast queued directly"), Player);
			return;
		}
		// A zero charge time fills the meter on the next step; the latched press then activates it (combat_core.md R-CMB-11).
		FPendingRestore& Restore = Pending.AddDefaulted_GetRef();
		Restore.Hero = Hero;
		Restore.Player = Player;
		Restore.Stats = State->Stats;
		FDTHeroStats Instant = State->Stats;
		Instant.IntercessionCooldownMul = 0.0f;
		Hero->SetStats(Player, Instant);
		Restore.Handle = Hero->OnIntercession.AddLambda([](int32 OwnerId, EDTPath Path)
		{
			for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
			{
				FPendingRestore& Item = Pending[Index];
				if (Item.Player != OwnerId)
				{
					continue;
				}
				if (UDTHeroSubsystem* H = Item.Hero.Get())
				{
					H->SetStats(OwnerId, Item.Stats);
					H->OnIntercession.Remove(Item.Handle);
				}
				Pending.RemoveAt(Index);
			}
		});
		FDTHeroInput Press;
		Press.bIntercessionPress = true;
		Hero->LatchInput(Player, Press);
		UE_LOG(LogDT, Log, TEXT("DT.Intercede player %d: meter filled, cast at the next step"), Player);
	}

	static void Reactions(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTKeeperSubsystem* Keepers = Find(World))
		{
			Keepers->LogReactions();
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs ReactionsCmd(TEXT("DT.Reactions.Stats"),
		TEXT("DT.Reactions.Stats: per player and pair, reaction fires and damage (reactions.md)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Reactions));
	static FAutoConsoleCommandWithWorldAndArgs BlessCmd(TEXT("DT.Bless"),
		TEXT("DT.Bless <BlessingId> [player]: grant a blessing (e.g. Blessing.Storm.Tempest.Key) and rebuild the Keeper flags"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Bless));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Keeper.Stats"),
		TEXT("DT.Keeper.Stats: per player Keepers, Favor, tiers, leading archetypes, built flags, enchantment procs and casts"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
	static FAutoConsoleCommandWithWorldAndArgs IntercedeCmd(TEXT("DT.Intercede"),
		TEXT("DT.Intercede [player] [delay_s]: fill the player's Intercession meter and cast at the next step (or after delay_s wall seconds)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Intercede));
}

#endif // !UE_BUILD_SHIPPING
