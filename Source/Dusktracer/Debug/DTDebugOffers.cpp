// Offer debug console commands (offers.md; R-TEC-12: compiled out of Shipping).
//   DT.Offer [Level|Chest|Keeper|Reforging|Patron] [player]   queue a hand (built when it opens)
//   DT.Offer.Pick <slot> [player] · DT.Offer.Reroll [player] · DT.Offer.Banish <slot> [player] · DT.Offer.Lock <slot> [player]
//   DT.Offer.Stats                                            per player tools, Keepers, hands, decisions, Reserve share
// Capture hook: -DTOfferShot=<png> picks the Patron (slot 0), gives two weapons, opens a level-up hand and writes a
// screenshot of it after a few wall seconds, then quits (the hand pauses the world, so it runs on the core ticker).
// -DTOfferShotPatron=<slot> picks that Patron instead, -DTOfferShotPatron=-1 shoots the Patron hand itself;
// -DTOfferShotFocus=<slot> moves the focus first (keyword notes follow it).

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "Combat/DTWeaponSubsystem.h"
#include "Containers/Ticker.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferSubsystem.h"
#include "UnrealClient.h"

namespace DTDebugOffers
{
	static UDTOfferSubsystem* Find(UWorld* World)
	{
		UDTOfferSubsystem* Offers = World ? World->GetSubsystem<UDTOfferSubsystem>() : nullptr;
		if (!Offers)
		{
			UE_LOG(LogDT, Warning, TEXT("DT offers: no offer subsystem in this world (Game/PIE worlds only)"));
		}
		return Offers;
	}

	static int32 PlayerArg(const TArray<FString>& Args, int32 Index)
	{
		return Args.Num() > Index ? FCString::Atoi(*Args[Index]) : 0;
	}

	static const TCHAR* HandName(EDTHandKind Kind)
	{
		switch (Kind)
		{
		case EDTHandKind::Chest: return TEXT("Chest");
		case EDTHandKind::Keeper: return TEXT("Keeper");
		case EDTHandKind::Reforging: return TEXT("Reforging");
		case EDTHandKind::Patron: return TEXT("Patron");
		default: return TEXT("Level");
		}
	}

	static void Offer(const TArray<FString>& Args, UWorld* World)
	{
		UDTOfferSubsystem* Offers = Find(World);
		if (!Offers)
		{
			return;
		}
		EDTHandKind Kind = EDTHandKind::Level;
		if (Args.Num() > 0)
		{
			bool bKnown = false;
			for (int32 Index = 0; Index <= static_cast<int32>(EDTHandKind::Patron); ++Index)
			{
				if (Args[0].Equals(HandName(static_cast<EDTHandKind>(Index)), ESearchCase::IgnoreCase))
				{
					Kind = static_cast<EDTHandKind>(Index);
					bKnown = true;
				}
			}
			if (!bKnown)
			{
				UE_LOG(LogDT, Log, TEXT("DT.Offer [Level|Chest|Keeper|Reforging|Patron] [player]"));
				return;
			}
		}
		const int32 Player = PlayerArg(Args, 1);
		Offers->BuildHand(Player, Kind);
		UE_LOG(LogDT, Log, TEXT("DT.Offer %s player %d: queued"), HandName(Kind), Player);
	}

	static void Pick(const TArray<FString>& Args, UWorld* World)
	{
		UDTOfferSubsystem* Offers = Find(World);
		if (!Offers || Args.Num() < 1)
		{
			UE_LOG(LogDT, Log, TEXT("DT.Offer.Pick <slot> [player] (slot from 0)"));
			return;
		}
		const int32 Player = PlayerArg(Args, 1);
		const bool bOk = Offers->Pick(Player, FCString::Atoi(*Args[0]));
		UE_LOG(LogDT, Log, TEXT("DT.Offer.Pick %s player %d: %s"), *Args[0], Player, bOk ? TEXT("ok") : TEXT("no open hand or bad slot"));
	}

	static void Reroll(const TArray<FString>& Args, UWorld* World)
	{
		if (UDTOfferSubsystem* Offers = Find(World))
		{
			UE_LOG(LogDT, Log, TEXT("DT.Offer.Reroll: %s"), Offers->Reroll(PlayerArg(Args, 0)) ? TEXT("ok") : TEXT("refused"));
		}
	}

	static void Banish(const TArray<FString>& Args, UWorld* World)
	{
		UDTOfferSubsystem* Offers = Find(World);
		if (Offers && Args.Num() > 0)
		{
			UE_LOG(LogDT, Log, TEXT("DT.Offer.Banish: %s"), Offers->Banish(PlayerArg(Args, 1), FCString::Atoi(*Args[0])) ? TEXT("ok") : TEXT("refused"));
		}
	}

	static void Lock(const TArray<FString>& Args, UWorld* World)
	{
		UDTOfferSubsystem* Offers = Find(World);
		if (Offers && Args.Num() > 0)
		{
			UE_LOG(LogDT, Log, TEXT("DT.Offer.Lock: %s"), Offers->Lock(PlayerArg(Args, 1), FCString::Atoi(*Args[0])) ? TEXT("ok") : TEXT("refused"));
		}
	}

	static void Stats(const TArray<FString>& Args, UWorld* World)
	{
		UDTOfferSubsystem* Offers = Find(World);
		UDTSimSubsystem* Sim = World ? World->GetSubsystem<UDTSimSubsystem>() : nullptr;
		if (!Offers || !Sim)
		{
			return;
		}
		const FDTRunState& Run = Sim->GetRun();
		for (int32 Player = 0; Player < Run.GetNumPlayers(); ++Player)
		{
			const FDTPlayerRunState& P = Run.GetPlayer(Player);
			const FDTOfferPlayerStats* S = Offers->GetStats(Player);
			if (!S)
			{
				continue;
			}
			FString Hands;
			for (int32 Kind = 0; Kind <= static_cast<int32>(EDTHandKind::Patron); ++Kind)
			{
				Hands += FString::Printf(TEXT(" %s=%d"), HandName(static_cast<EDTHandKind>(Kind)), S->HandsByKind[Kind]);
			}
			FString Owned;
			for (const FName& Id : P.Keepers.Owned)
			{
				Owned += TEXT(" ") + Id.ToString();
			}
			const double ReserveShare = S->CardsOffered > 0 ? static_cast<double>(S->ReserveCards) / S->CardsOffered : 0.0;
			UE_LOG(LogDT, Log, TEXT("DT.Offer.Stats p%d level=%d patron=%s second=%s rerolls=%d banish=%d lock=%d held=%s banished=%d pending=%d"),
				Player, P.Level, DTOffers::PathName(P.Offer.PatronPath), DTOffers::PathName(P.Offer.SecondPath), P.Offer.RerollsLeft,
				P.Offer.BanishLeft, P.Offer.LockLeft, P.Offer.Locked.IsSet() ? *P.Offer.Locked->ContentId.ToString() : TEXT("none"),
				P.Offer.Banished.Num(), P.Offer.PendingHands.Num());
			UE_LOG(LogDT, Log, TEXT("DT.Offer.Stats p%d decisions=%d (budget %d) hands[%s ] cards=%d reserve=%d (%.1f%%, max %.0f%%) tools used r%d b%d l%d wall_paused=%.1fs hone=%d owned=[%s ]"),
				Player, S->Decisions, DTConst::DECISIONS_PER_RUN, *Hands, S->CardsOffered, S->ReserveCards, 100.0 * ReserveShare,
				100.0 * DTConst::OFFER_RESERVE_SHARE_MAX, S->Rerolls, S->Banishes, S->Locks, S->WallPausedSeconds, Offers->GetHoneStacks(Player), *Owned);
		}
	}

	static FAutoConsoleCommandWithWorldAndArgs OfferCmd(TEXT("DT.Offer"),
		TEXT("DT.Offer [Level|Chest|Keeper|Reforging|Patron] [player]: queue a hand of that kind (opens and pauses at once)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Offer));
	static FAutoConsoleCommandWithWorldAndArgs PickCmd(TEXT("DT.Offer.Pick"),
		TEXT("DT.Offer.Pick <slot> [player]: pick a card of the open hand (slot from 0)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Pick));
	static FAutoConsoleCommandWithWorldAndArgs RerollCmd(TEXT("DT.Offer.Reroll"),
		TEXT("DT.Offer.Reroll [player]: reroll the open hand"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Reroll));
	static FAutoConsoleCommandWithWorldAndArgs BanishCmd(TEXT("DT.Offer.Banish"),
		TEXT("DT.Offer.Banish <slot> [player]: banish a card of the open hand"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Banish));
	static FAutoConsoleCommandWithWorldAndArgs LockCmd(TEXT("DT.Offer.Lock"),
		TEXT("DT.Offer.Lock <slot> [player]: hold a card of the open hand for the next level-up hand"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Lock));
	static FAutoConsoleCommandWithWorldAndArgs StatsCmd(TEXT("DT.Offer.Stats"),
		TEXT("DT.Offer.Stats: per player tools, Keepers, hands by kind, decisions, cards offered and Reserve share"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&Stats));

	// ---- -DTOfferShot capture -------------------------------------------------------------------------------------------
	struct FShot
	{
		FString Path;
		double Start = -1.0;
		int32 Stage = 0;
		int64 LastSize = -1;
	};

	static UWorld* GameWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE) && World->HasBegunPlay())
			{
				return World;
			}
		}
		return nullptr;
	}

	static bool StepShot(FShot& Shot)
	{
		constexpr double SetupAfterS = 2.0;
		constexpr double ShootAfterS = 5.0;
		constexpr double GiveUpAfterS = 60.0;
		UWorld* World = GameWorld();
		UDTOfferSubsystem* Offers = World ? World->GetSubsystem<UDTOfferSubsystem>() : nullptr;
		if (!Offers)
		{
			return true;
		}
		const double Now = FPlatformTime::Seconds();
		if (Shot.Start < 0.0)
		{
			Shot.Start = Now;
		}
		const double T = Now - Shot.Start;
		if (Shot.Stage == 0 && T >= SetupAfterS)
		{
			int32 Patron = 0;
			FParse::Value(FCommandLine::Get(), TEXT("DTOfferShotPatron="), Patron);
			if (Patron >= 0)
			{
				Offers->Pick(0, Patron); // the run-start Patron hand
				if (UDTWeaponSubsystem* Weapons = World->GetSubsystem<UDTWeaponSubsystem>())
				{
					Weapons->Give(0, FName(TEXT("Weapon.PulseRifle")), 2);
					Weapons->Give(0, FName(TEXT("Weapon.ArcBlade")), 1);
				}
				Offers->BuildHand(0, EDTHandKind::Level, 3);
			}
			int32 Focus = 0;
			FParse::Value(FCommandLine::Get(), TEXT("DTOfferShotFocus="), Focus);
			Offers->SetFocus(0, Focus);
			Shot.Stage = 1;
		}
		else if (Shot.Stage == 1 && T >= ShootAfterS)
		{
			FScreenshotRequest::RequestScreenshot(Shot.Path, /*bInShowUI*/ true, /*bAddFilenameSuffix*/ false);
			Shot.Stage = 2;
		}
		else if (Shot.Stage == 2)
		{
			const int64 Size = IFileManager::Get().FileSize(*Shot.Path);
			if ((Size > 0 && Size == Shot.LastSize) || T > GiveUpAfterS)
			{
				UE_LOG(LogDT, Log, TEXT("DT offer shot: %s (%lld bytes); quitting"), *Shot.Path, Size);
				FPlatformMisc::RequestExit(false, TEXT("DT offer shot written"));
				return false;
			}
			Shot.LastSize = Size;
		}
		return true;
	}

	static FDelayedAutoRegisterHelper ShotRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []()
	{
		FString Path;
		if (!FParse::Value(FCommandLine::Get(), TEXT("DTOfferShot="), Path) || Path.IsEmpty())
		{
			return;
		}
		TSharedRef<FShot> Shot = MakeShared<FShot>();
		Shot->Path = FPaths::ConvertRelativePathToFull(Path);
		IFileManager::Get().Delete(*Shot->Path, false, true, true);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Shot](float) { return StepShot(*Shot); }), 0.25f);
		UE_LOG(LogDT, Log, TEXT("DT offer shot: level-up hand to %s"), *Shot->Path);
	});
}

#endif // !UE_BUILD_SHIPPING
