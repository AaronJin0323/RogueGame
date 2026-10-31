// Weapon and modifier debug console commands (R-TEC-12: compiled out of Shipping; D-013 DT.Unlock).

#include "CoreMinimal.h"
#include "Keepers/DTReactions.h"

#if !UE_BUILD_SHIPPING

#include "Dusktracer.h"
#include "Combat/DTWeaponCatalog.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTSimSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Offers/DTOfferGenerator.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTDebugWeapons
{
	static UDTWeaponSubsystem* Find(UWorld* World)
	{
		UDTWeaponSubsystem* Weapons = World ? World->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
		if (!Weapons)
		{
			UE_LOG(LogDT, Warning, TEXT("DT weapons: no weapon subsystem in this world (Game/PIE worlds only)"));
		}
		return Weapons;
	}

	static const TCHAR* ResultText(EDTWeaponResult Result)
	{
		switch (Result)
		{
		case EDTWeaponResult::Ok: return TEXT("ok");
		case EDTWeaponResult::UnknownId: return TEXT("unknown id");
		case EDTWeaponResult::NoSlot: return TEXT("no free slot");
		case EDTWeaponResult::AlreadyOwned: return TEXT("already owned");
		case EDTWeaponResult::MaxLevel: return TEXT("level out of range");
		case EDTWeaponResult::NoWeapon: return TEXT("weapon not owned");
		case EDTWeaponResult::Incompatible: return TEXT("incompatible (R-MOD-01)");
		case EDTWeaponResult::Duplicate: return TEXT("already socketed");
		case EDTWeaponResult::SocketsFull: return TEXT("sockets full");
		default: return TEXT("bad player");
		}
	}

	static void Give(const TArray<FString>& Args, UWorld* World)
	{
		UDTWeaponSubsystem* Weapons = Find(World);
		if (!Weapons || Args.Num() < 1)
		{
			UE_LOG(LogDT, Log, TEXT("DT.Give <WeaponId> [level] [player]"));
			return;
		}
		const FName Id(*Args[0]);
		const int32 Level = Args.Num() > 1 ? FCString::Atoi(*Args[1]) : 1;
		const int32 Player = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0;
		EDTWeaponResult Result = Weapons->Give(Player, Id, Level);
		if (Result == EDTWeaponResult::AlreadyOwned)
		{
			Result = Weapons->LevelUp(Player, Id, Level);
		}
		UE_LOG(LogDT, Log, TEXT("DT.Give %s L%d player %d: %s"), *Args[0], Level, Player, ResultText(Result));
	}

	static void Mod(const TArray<FString>& Args, UWorld* World)
	{
		UDTWeaponSubsystem* Weapons = Find(World);
		if (!Weapons || Args.Num() < 2)
		{
			UE_LOG(LogDT, Log, TEXT("DT.Mod <WeaponId> <ModId> [player]"));
			return;
		}
		const int32 Player = Args.Num() > 2 ? FCString::Atoi(*Args[2]) : 0;
		const EDTWeaponResult Result = Weapons->Socket(Player, FName(*Args[0]), FName(*Args[1]));
		UE_LOG(LogDT, Log, TEXT("DT.Mod %s %s player %d: %s"), *Args[0], *Args[1], Player, ResultText(Result));
	}

	static void Unlock(const TArray<FString>& Args, UWorld* World)
	{
		UDTWeaponSubsystem* Weapons = Find(World);
		if (!Weapons || Args.Num() < 1)
		{
			UE_LOG(LogDT, Log, TEXT("DT.Unlock <Weapon.*|Mod.*|Keeper.*|Reaction.Duos>"));
			return;
		}
		const FName Raw(*Args[0]);
		const int32 Weapon = DTWeapons::FindWeapon(Raw);
		const int32 Mod = DTWeapons::FindModifier(Raw);
		const bool bKeeper = DTOffers::ParseKeeper(Raw) != EDTPath::None;
		const bool bDuos = Raw == DTReactions::UnlockId();
		const FName Id = Weapon != INDEX_NONE ? DTWeapons::WeaponId(Weapon) : (Mod != INDEX_NONE ? DTWeapons::ModifierId(Mod) : (bKeeper || bDuos ? Raw : NAME_None));
		if (Id.IsNone())
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Unlock %s: unknown id"), *Args[0]);
			return;
		}
		Weapons->Unlock(Id);
		UE_LOG(LogDT, Log, TEXT("DT.Unlock %s: in pool"), *Id.ToString());
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		UDTWeaponSubsystem* Weapons = Find(World);
		UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
		if (!Weapons || !Sim)
		{
			return;
		}
		const FDTRunState& Run = Sim->GetRun();
		for (int32 Player = 0; Player < Run.GetNumPlayers(); ++Player)
		{
			const TArray<FDTWeaponInstance>& Owned = Run.GetPlayer(Player).Weapons;
			for (int32 Slot = 0; Slot < Owned.Num(); ++Slot)
			{
				const FDTWeaponSpec& Spec = Weapons->GetSim().GetLastSpec(Player, Slot);
				const FDTWeaponCounters& Counters = Weapons->GetSim().GetCounters(Player, Slot);
				FString Mods;
				for (const FDTWeaponInstance::FSocket& Socket : Owned[Slot].Sockets)
				{
					Mods += FString::Printf(TEXT(" %s%s"), *Socket.ModId.ToString(), Socket.bDormant ? TEXT("(dormant)") : TEXT(""));
				}
				FString Flags;
				for (const FDTResolvedFlag& Flag : Spec.Flags)
				{
					Flags += FString::Printf(TEXT(" %s:%s:%s"), DTCat::FlagIds[static_cast<int32>(Flag.Entry.Flag)],
						Flag.IsNative() ? TEXT("native") : (Flag.IsFallback() ? TEXT("fallback") : TEXT("none")),
						*FDTTelemetrySources::GetName(Flag.Entry.SourceId));
				}
				UE_LOG(LogDT, Log, TEXT("DT.Weapons.Stats p%d slot%d %s L%d load_rank=%d dmg=%.1f count=%d cd=%.2f spawned=%lld throttled=%lld impacts=%lld mines=%d mods=[%s ] flags=[%s ]"),
					Player, Slot, *Owned[Slot].Id.ToString(), Owned[Slot].Level, Spec.LoadRank + 1, Spec.Damage, Spec.Count, Spec.Cooldown,
					Counters.Spawned, Counters.Throttled, Counters.Impacts, Weapons->GetSim().GetLiveMines(Player, Slot), *Mods, *Flags);
			}
		}
		UE_LOG(LogDT, Log, TEXT("DT.Weapons.Stats fields=%d tracked_rounds=%d"), Weapons->GetSim().GetLiveFields(), Weapons->GetSim().GetTrackedRounds());
	}

	static FAutoConsoleCommandWithWorldAndArgs GiveCmd(TEXT("DT.Give"),
		TEXT("DT.Give <WeaponId> [level] [player]: give a weapon (or set its level when owned), e.g. DT.Give PulseRifle 3"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Give));
	static FAutoConsoleCommandWithWorldAndArgs ModCmd(TEXT("DT.Mod"),
		TEXT("DT.Mod <WeaponId> <ModId> [player]: socket a modifier into an owned weapon (compatibility enforced)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Mod));
	static FAutoConsoleCommandWithWorldAndArgs UnlockCmd(TEXT("DT.Unlock"),
		TEXT("DT.Unlock <Id>: put a non-starter weapon, modifier or Keeper in the run pool (D-013: Gravity Mine, Fork, Ricochet; keepers.md R-KEE-23)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Unlock));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Weapons.Stats"),
		TEXT("DT.Weapons.Stats: per weapon level, load rank, composed numbers, spawned/throttled/impacts, sockets and flags"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));
}

#endif // !UE_BUILD_SHIPPING
