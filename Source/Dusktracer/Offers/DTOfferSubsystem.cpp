#include "Offers/DTOfferSubsystem.h"

#include "Combat/DTReforge.h"
#include "Keepers/DTReactions.h"
#include "Combat/DTWeaponSubsystem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimSubsystem.h"
#include "Dusktracer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Player/DTHeroSubsystem.h"
#include "Progression/DTGemSubsystem.h"
#include "Telemetry/DTTelemetryRecords.h"
#include "Telemetry/DTTelemetrySubsystem.h"

namespace DTOfferSubsystemLocal
{
	static TAutoConsoleVariable<int32> CVarAutoPick(
		TEXT("dt.Offer.AutoPick"),
		-1,
		TEXT("Offer hands: -1 = pick slot 0 at once when the process runs -unattended (smoke, perf) or with -DTOfferAutoPick; ")
		TEXT("0 = never (a player picks); 1 = always."),
		ECVF_Default);

	/** Queue order (R-OFF-14): Patron, then level-up hands by level, then chest hands; stable. */
	int32 Priority(const FDTOfferHand& Hand)
	{
		switch (Hand.HandKind)
		{
		case EDTHandKind::Patron: return 0;
		case EDTHandKind::Chest: return 2;
		default: return 1;
		}
	}

	EDTTelBlessingRole Role(int32 Column)
	{
		return Column == 0 ? EDTTelBlessingRole::Key : (Column == 3 ? EDTTelBlessingRole::Trait : EDTTelBlessingRole::Amp);
	}
}

bool UDTOfferSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDTOfferSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Sim = Collection.InitializeDependency<UDTSimSubsystem>();
	Collection.InitializeDependency<UDTWeaponSubsystem>();
	Collection.InitializeDependency<UDTHeroSubsystem>();
	Screen = NewObject<UDTOfferScreen>(this);
}

void UDTOfferSubsystem::Deinitialize()
{
	if (UDTSimSubsystem* SimSys = Sim.Get())
	{
		SimSys->OnStepEnd.Remove(StepEndHandle);
	}
	Flows.Reset();
	Screen = nullptr;
	Super::Deinitialize();
}

void UDTOfferSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The run starts in ADTGameMode::BeginPlay, right after this; pausing now means the run-start Patron hand opens
	// (first Tick, which runs while paused) before the sim steps once (R-OFF-08, D-013).
	SetPaused(true);
	if (UDTSimSubsystem* SimSys = Sim.Get())
	{
		StepEndHandle = SimSys->OnStepEnd.AddUObject(this, &UDTOfferSubsystem::HandleStepEnd);
	}
}

TStatId UDTOfferSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDTOfferSubsystem, STATGROUP_Tickables);
}

bool UDTOfferSubsystem::IsAutoPick()
{
	const int32 Mode = DTOfferSubsystemLocal::CVarAutoPick.GetValueOnGameThread();
	if (Mode >= 0)
	{
		return Mode > 0;
	}
	return FApp::IsUnattended() || FParse::Param(FCommandLine::Get(), TEXT("DTOfferAutoPick"));
}

FDTRunState* UDTOfferSubsystem::GetRun() const
{
	UDTSimSubsystem* SimSys = Sim.Get();
	return SimSys ? &SimSys->GetRun() : nullptr;
}

void UDTOfferSubsystem::SyncPlayers()
{
	FDTRunState* Run = GetRun();
	UDTSimSubsystem* SimSys = Sim.Get();
	if (!Run || !SimSys)
	{
		return;
	}
	// A new run (StartRun) resets every player's offer state: start the flow over with it.
	const bool bRestarted = SimSys->GetStepIndex() < LastStep || Flows.Num() != Run->GetNumPlayers();
	LastStep = SimSys->GetStepIndex();
	if (bRestarted)
	{
		Flows.Reset();
		Flows.SetNum(Run->GetNumPlayers());
	}
	for (int32 Owner = 0; Owner < Run->GetNumPlayers(); ++Owner)
	{
		FDTPlayerRunState& Player = Run->GetPlayer(Owner);
		FPlayerFlow& Flow = Flows[Owner];
		if (Flow.bOpen && Player.Offer.PendingHands.Num() == 0)
		{
			Flow.bOpen = false;
		}
		const bool bPatronQueued = Player.Offer.PendingHands.ContainsByPredicate([](const FDTOfferHand& H) { return H.HandKind == EDTHandKind::Patron; });
		if (Player.Offer.PatronPath == EDTPath::None && !bPatronQueued)
		{
			FDTOfferHand Request;
			Request.HandKind = EDTHandKind::Patron;
			Request.Level = Player.Level;
			Player.Offer.PendingHands.Add(Request);
		}
	}
}

void UDTOfferSubsystem::BuildHand(int32 OwnerId, EDTHandKind HandKind, int32 Level)
{
	FDTRunState* Run = GetRun();
	if (!Run || !Run->IsValidPlayer(OwnerId))
	{
		return;
	}
	FDTPlayerRunState& Player = Run->GetPlayer(OwnerId);
	FDTOfferHand Request;
	Request.HandKind = HandKind;
	Request.Level = Level > 0 ? Level : Player.Level;
	Player.Offer.PendingHands.Add(Request);
	UpdateFlow();
}

FDTOfferPool UDTOfferSubsystem::MakePool(int32 OwnerId) const
{
	FDTOfferPool Pool;
	const UWorld* World = GetWorld();
	UDTWeaponSubsystem* Weapons = World ? World->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	FDTRunState* Run = GetRun();
	if (!Weapons || !Run || !Run->IsValidPlayer(OwnerId))
	{
		return Pool;
	}
	for (int32 Weapon = 0; Weapon < DTCat::NumWeapons; ++Weapon)
	{
		if (!DTCat::Weapons[Weapon].bStarter && Weapons->IsInPool(DTWeapons::WeaponId(Weapon)))
		{
			Pool.Unlocked.Add(DTWeapons::WeaponId(Weapon));
		}
	}
	for (int32 Mod = 0; Mod < DTCat::NumModifiers; ++Mod)
	{
		if (!DTCat::Modifiers[Mod].bStarter && Weapons->IsInPool(DTWeapons::ModifierId(Mod)))
		{
			Pool.Unlocked.Add(DTWeapons::ModifierId(Mod));
		}
	}
	if (Weapons->IsInPool(DTReactions::UnlockId()))
	{
		Pool.Unlocked.Add(DTReactions::UnlockId()); // reactions.md R-REA-02
	}
	for (const EDTPath Path : DTOffers::AllKeepers())
	{
		if (Weapons->IsInPool(DTOffers::KeeperId(Path)) && !DTOffers::RunKeepers().Contains(Path))
		{
			Pool.Unlocked.Add(DTOffers::KeeperId(Path));
		}
	}
	// Sockets are checked against the caps the weapon expresses now (conversions included), as UDTWeaponSubsystem::Socket does.
	for (int32 Slot = 0; Slot < Run->GetPlayer(OwnerId).Weapons.Num(); ++Slot)
	{
		Pool.SlotCaps.Add(Weapons->GetSim().ComposeSlot(*Run, OwnerId, Slot).EffectiveCaps);
	}
	return Pool;
}

void UDTOfferSubsystem::ShowHand(int32 OwnerId, const FDTOfferHand& Hand)
{
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		FDTRunState* Run = GetRun();
		Telemetry->Records().RecordOffer(OwnerId, Hand, Run->GetPlayer(OwnerId).Offer);
	}
	FDTOfferPlayerStats& Stats = Flows[OwnerId].Stats;
	Stats.CardsOffered += Hand.Cards.Num();
	Stats.ReserveCards += Hand.Cards.FilterByPredicate([](const FDTOfferCard& C) { return C.Kind == EDTCardKind::Reserve; }).Num();
}

bool UDTOfferSubsystem::OpenNext(int32 OwnerId)
{
	FDTRunState* Run = GetRun();
	FDTPlayerRunState& Player = Run->GetPlayer(OwnerId);
	FPlayerFlow& Flow = Flows[OwnerId];
	TArray<FDTOfferHand>& Pending = Player.Offer.PendingHands;
	if (Flow.bOpen || Pending.Num() == 0)
	{
		return false;
	}
	int32 Next = 0;
	for (int32 Index = 1; Index < Pending.Num(); ++Index)
	{
		const int32 A = DTOfferSubsystemLocal::Priority(Pending[Index]);
		const int32 B = DTOfferSubsystemLocal::Priority(Pending[Next]);
		if (A < B || (A == B && A == 1 && Pending[Index].Level < Pending[Next].Level))
		{
			Next = Index;
		}
	}
	FDTOfferHand Hand = Pending[Next];
	Pending.RemoveAt(Next);
	if (Hand.Cards.Num() == 0)
	{
		const EDTHandKind Kind = DTOffers::ResolveKind(Player, Hand.HandKind, Hand.Level);
		TArray<FDTOfferCard> Carried;
		if (Kind == EDTHandKind::Level && Player.Offer.Locked.IsSet())
		{
			Carried.Add(Player.Offer.Locked.GetValue());
		}
		TArray<int32> Dropped;
		const int32 Level = Hand.Level;
		Hand = DTOffers::BuildHand(Player, MakePool(OwnerId), Kind, Level, Player.Offer.Rng, Carried, &Dropped);
		if (Carried.Num() > 0)
		{
			// The held card is spent into this hand, or refunded when it has turned illegal (R-OFF-10).
			Player.Offer.Locked.Reset();
			if (Dropped.Num() > 0)
			{
				++Player.Offer.LockLeft;
				UE_LOG(LogDT, Log, TEXT("Offer p%d: held card no longer legal; lock refunded"), OwnerId);
			}
		}
	}
	if (Hand.HandId == INDEX_NONE)
	{
		Hand.HandId = Flow.NextHandId++;
	}
	Pending.Insert(Hand, 0);
	Flow.bOpen = true;
	Flow.Focus = 0;
	Flow.OpenedWall = FPlatformTime::Seconds();
	Flow.Guard.Open(Flow.OpenedWall);
	Flow.Stats.HandsByKind[static_cast<int32>(Hand.HandKind)]++;
	// Keys held when the hand opens never act (R-OFF-11 e): they must be released and pressed again.
	Flow.Down.Reset();
	Flow.bMouseDown = false;
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		for (const FKey& Key : DTOfferInput::Keys())
		{
			if (PC->IsInputKeyDown(Key))
			{
				Flow.Down.Add(Key);
			}
		}
		Flow.bMouseDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
		float MX = 0.0f;
		float MY = 0.0f;
		Flow.LastMouse = PC->GetMousePosition(MX, MY) ? FVector2D(MX, MY) : FVector2D::ZeroVector;
	}
	ShowHand(OwnerId, Hand);
	UE_LOG(LogDT, Log, TEXT("Offer p%d: hand %d kind=%d level=%d cards=%d"), OwnerId, Hand.HandId, static_cast<int32>(Hand.HandKind), Hand.Level, Hand.Cards.Num());
	return true;
}

void UDTOfferSubsystem::UpdateFlow()
{
	FDTRunState* Run = GetRun();
	if (!Run)
	{
		return;
	}
	SyncPlayers();
	for (int32 Owner = 0; Owner < Flows.Num(); ++Owner)
	{
		OpenNext(Owner);
	}
	SetPaused(IsAnyHandOpen());
	SetCursor(IsAnyHandOpen() && !IsAutoPick());
	UDTSimSubsystem* SimSys = Sim.Get();
	if (SimSys && SimSys->IsPauseRequested() && !IsAnyHandOpen() && !IsAnyHandPending())
	{
		SimSys->ClearPauseRequest();
	}
}

void UDTOfferSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (!World || !World->HasBegunPlay() || !GetRun() || GetRun()->GetNumPlayers() == 0)
	{
		return;
	}
	UpdateFlow();
	if (IsAutoPick())
	{
		// Unattended: every hand takes slot 0 at once (a pick opens the next queued hand).
		for (int32 Owner = 0; Owner < Flows.Num(); ++Owner)
		{
			int32 Guard = 0;
			while (Flows[Owner].bOpen && Guard++ < 64)
			{
				Pick(Owner, 0);
			}
		}
		return;
	}
	HandleLocalInput();
}

const FDTOfferHand* UDTOfferSubsystem::GetOpenHand(int32 OwnerId) const
{
	FDTRunState* Run = GetRun();
	if (!Run || !Run->IsValidPlayer(OwnerId) || !Flows.IsValidIndex(OwnerId) || !Flows[OwnerId].bOpen)
	{
		return nullptr;
	}
	const TArray<FDTOfferHand>& Pending = Run->GetPlayer(OwnerId).Offer.PendingHands;
	return Pending.Num() > 0 ? &Pending[0] : nullptr;
}

bool UDTOfferSubsystem::IsAnyHandOpen() const
{
	for (int32 Owner = 0; Owner < Flows.Num(); ++Owner)
	{
		if (GetOpenHand(Owner))
		{
			return true;
		}
	}
	return false;
}

void UDTOfferSubsystem::SetFocus(int32 OwnerId, int32 Slot)
{
	if (Flows.IsValidIndex(OwnerId))
	{
		Flows[OwnerId].Focus = Slot;
	}
}

int32 UDTOfferSubsystem::GetFocus(int32 OwnerId) const
{
	return Flows.IsValidIndex(OwnerId) ? Flows[OwnerId].Focus : 0;
}

const FDTOfferPlayerStats* UDTOfferSubsystem::GetStats(int32 OwnerId) const
{
	return Flows.IsValidIndex(OwnerId) ? &Flows[OwnerId].Stats : nullptr;
}

int32 UDTOfferSubsystem::GetHoneStacks(int32 OwnerId) const
{
	return Flows.IsValidIndex(OwnerId) ? Flows[OwnerId].HoneStacks : 0;
}

// ---- tools ------------------------------------------------------------------------------------------------------------

bool UDTOfferSubsystem::Reroll(int32 OwnerId)
{
	const FDTOfferHand* Open = GetOpenHand(OwnerId);
	FDTRunState* Run = GetRun();
	if (!Open || !DTOffers::HandAllowsTools(Open->HandKind))
	{
		return false;
	}
	FDTPlayerRunState& Player = Run->GetPlayer(OwnerId);
	if (Player.Offer.RerollsLeft <= 0)
	{
		return false;
	}
	const FDTOfferHand Old = *Open;
	const TArray<FDTOfferCard> Carried = Old.Cards.FilterByPredicate([](const FDTOfferCard& C) { return C.bLocked; });
	FDTOfferHand Fresh = DTOffers::BuildHand(Player, MakePool(OwnerId), Old.HandKind, Old.Level, Player.Offer.Rng, Carried);
	Fresh.HandId = Flows[OwnerId].NextHandId++;
	Fresh.RerollOf = Old.HandId;
	--Player.Offer.RerollsLeft;
	Player.Offer.PendingHands[0] = Fresh;
	Flows[OwnerId].Focus = 0;
	Flows[OwnerId].Stats.Rerolls++;
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		Telemetry->Records().RecordOfferTool(OwnerId, Old.HandId, EDTTelOfferTool::Reroll, INDEX_NONE, NAME_None);
	}
	ShowHand(OwnerId, Fresh);
	return true;
}

bool UDTOfferSubsystem::Banish(int32 OwnerId, int32 Slot)
{
	const FDTOfferHand* Open = GetOpenHand(OwnerId);
	FDTRunState* Run = GetRun();
	if (!Open || !DTOffers::HandAllowsTools(Open->HandKind) || !Open->Cards.IsValidIndex(Slot))
	{
		return false;
	}
	FDTPlayerRunState& Player = Run->GetPlayer(OwnerId);
	const FDTOfferCard Card = Open->Cards[Slot];
	if (Player.Offer.BanishLeft <= 0 || !DTOffers::CardAllowsBanishLock(Card) || Card.bLocked)
	{
		return false;
	}
	--Player.Offer.BanishLeft;
	Player.Offer.Banished.Add(Card.ContentId);
	Flows[OwnerId].Stats.Banishes++;
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		Telemetry->Records().RecordOfferTool(OwnerId, Open->HandId, EDTTelOfferTool::Banish, Slot, Card.ContentId);
	}
	// Every card of the banished id leaves the hand; each slot is refilled by a fresh legal draw (R-OFF-10).
	FDTOfferHand& Hand = Player.Offer.PendingHands[0];
	const FDTOfferPool Pool = MakePool(OwnerId);
	for (int32 Index = 0; Index < Hand.Cards.Num(); ++Index)
	{
		if (Hand.Cards[Index].ContentId == Card.ContentId && !Hand.Cards[Index].bLocked)
		{
			DTOffers::RefillSlot(Player, Pool, Hand, Index, Player.Offer.Rng);
		}
	}
	ShowHand(OwnerId, Hand);
	return true;
}

bool UDTOfferSubsystem::Lock(int32 OwnerId, int32 Slot)
{
	const FDTOfferHand* Open = GetOpenHand(OwnerId);
	FDTRunState* Run = GetRun();
	if (!Open || !DTOffers::HandAllowsTools(Open->HandKind) || !Open->Cards.IsValidIndex(Slot))
	{
		return false;
	}
	FDTPlayerRunState& Player = Run->GetPlayer(OwnerId);
	FDTOfferCard& Card = Player.Offer.PendingHands[0].Cards[Slot];
	if (Player.Offer.LockLeft <= 0 || Player.Offer.Locked.IsSet() || !DTOffers::CardAllowsBanishLock(Card) || Card.bLocked)
	{
		return false;
	}
	--Player.Offer.LockLeft;
	Card.bLocked = true;
	Player.Offer.Locked = Card;
	Flows[OwnerId].Stats.Locks++;
	if (UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this))
	{
		Telemetry->Records().RecordOfferTool(OwnerId, Open->HandId, EDTTelOfferTool::Lock, Slot, Card.ContentId);
	}
	return true;
}

// ---- picks ------------------------------------------------------------------------------------------------------------

bool UDTOfferSubsystem::Pick(int32 OwnerId, int32 Slot)
{
	const FDTOfferHand* Open = GetOpenHand(OwnerId);
	FDTRunState* Run = GetRun();
	if (!Open || !Open->Cards.IsValidIndex(Slot))
	{
		return false;
	}
	FDTPlayerRunState& Player = Run->GetPlayer(OwnerId);
	const FDTOfferHand Hand = *Open;
	const FDTOfferCard Card = Hand.Cards[Slot];
	FPlayerFlow& Flow = Flows[OwnerId];
	const double Decide = FPlatformTime::Seconds() - Flow.OpenedWall;

	const FDTPickOutcome Outcome = DTOffers::ApplyPickState(Player, Hand.HandKind, Card);
	Player.Offer.PendingHands.RemoveAt(0);
	Flow.bOpen = false;
	if (Hand.HandKind != EDTHandKind::Patron)
	{
		++Flow.Stats.Decisions;
	}
	Flow.Stats.WallPausedSeconds += Decide;

	UDTTelemetrySubsystem* Telemetry = UDTTelemetrySubsystem::Find(this);
	if (Telemetry)
	{
		Telemetry->Records().RecordOfferPick(OwnerId, Hand.HandId, Slot, Card, Decide);
	}
	ApplyWorld(OwnerId, Outcome);
	if (Outcome.bValid && Card.Kind == EDTCardKind::Reforging && Telemetry)
	{
		const bool bEvolve = DTCat::Reforges[Outcome.ReforgeRow].bEvolve;
		TArray<FName> Dormant;
		int32 Level = 0;
		if (const FDTWeaponInstance* Weapon = Player.Weapons.FindByPredicate([&Outcome](const FDTWeaponInstance& W) { return W.Id == Outcome.Weapon; }))
		{
			Level = Weapon->Level;
			const FDTCapList Caps = DTReforge::Caps(Outcome.ReforgeRow);
			for (const FDTWeaponInstance::FSocket& Socket : Weapon->Sockets)
			{
				if (!DTModCompat::IsCompatible(Caps, DTWeapons::FindModifier(Socket.ModId)))
				{
					Dormant.Add(Socket.ModId);
				}
			}
		}
		Telemetry->Records().RecordReforge(OwnerId, bEvolve, Card.ContentId, Card.TargetWeapon, Level, Dormant);
	}
	if (Outcome.bValid && Card.Kind == EDTCardKind::Blessing)
	{
		const FDTBlessingInfo Info = DTOffers::FindBlessing(Outcome.Blessing);
		if (Telemetry && Info.IsValid())
		{
			FDTTelKeeperPick Rec;
			Rec.Blessing = Outcome.Blessing;
			Rec.Path = Info.Path;
			Rec.Archetype = FName(DTCat::Archetypes[Info.Archetype].Id);
			Rec.Role = DTOfferSubsystemLocal::Role(Info.Column);
			Rec.Favor = Player.Keepers.Favor[static_cast<int32>(Info.Path)];
			Rec.Tier = Player.Keepers.Tier[static_cast<int32>(Info.Path)];
			Rec.Leading = Player.Keepers.Leading[static_cast<int32>(Info.Path)];
			Telemetry->Records().RecordKeeperPick(OwnerId, Rec);
		}
		const int32 Duo = DTReactions::FindDuo(Outcome.Blessing);
		if (Telemetry && Duo != INDEX_NONE)
		{
			const EDTPath First = DTCat::Reactions[Duo].PathA;
			FDTTelKeeperPick Rec;
			Rec.Blessing = Outcome.Blessing;
			Rec.Path = First;
			Rec.Role = EDTTelBlessingRole::Duo;
			Rec.Favor = Player.Keepers.Favor[static_cast<int32>(First)];
			Rec.Tier = Player.Keepers.Tier[static_cast<int32>(First)];
			Rec.Leading = Player.Keepers.Leading[static_cast<int32>(First)];
			Telemetry->Records().RecordKeeperPick(OwnerId, Rec);
		}
		OnBlessingTaken.Broadcast(OwnerId, Outcome.Blessing);
	}
	if (Outcome.bValid && Card.Kind == EDTCardKind::Keeper)
	{
		if (Telemetry)
		{
			const FName Leading = Outcome.bPatron || Player.Keepers.Primary == EDTPath::None
				? NAME_None : Player.Keepers.Leading[static_cast<int32>(Player.Keepers.Primary)];
			Telemetry->Records().RecordKeeperCommit(OwnerId, Outcome.bPatron ? EDTTelKeeperRole::Patron : EDTTelKeeperRole::Second, Outcome.Keeper, Leading);
		}
		OnKeeperChosen.Broadcast(OwnerId, Outcome.Keeper, Outcome.bPatron);
	}
	UE_LOG(LogDT, Log, TEXT("Offer p%d: picked slot %d %s%s%s"), OwnerId, Slot, *Card.ContentId.ToString(),
		Card.TargetWeapon.IsNone() ? TEXT("") : TEXT(" -> "), Card.TargetWeapon.IsNone() ? TEXT("") : *Card.TargetWeapon.ToString());
	UpdateFlow();
	return true;
}

bool UDTOfferSubsystem::Decline(int32 OwnerId)
{
	const FDTOfferHand* Open = GetOpenHand(OwnerId);
	if (!Open || Open->HandKind == EDTHandKind::Patron)
	{
		return false;
	}
	const int32 HandId = Open->HandId;
	GetRun()->GetPlayer(OwnerId).Offer.PendingHands.RemoveAt(0);
	Flows[OwnerId].bOpen = false;
	UE_LOG(LogDT, Log, TEXT("Offer p%d: hand %d declined (no pick)"), OwnerId, HandId);
	UpdateFlow();
	return true;
}

void UDTOfferSubsystem::ApplyWorld(int32 OwnerId, const FDTPickOutcome& Outcome)
{
	UWorld* World = GetWorld();
	UDTWeaponSubsystem* Weapons = World ? World->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	if (!Outcome.bValid)
	{
		UE_LOG(LogDT, Warning, TEXT("Offer p%d: pick had no effect (kind %d)"), OwnerId, static_cast<int32>(Outcome.Kind));
		return;
	}
	EDTWeaponResult Result = EDTWeaponResult::Ok;
	switch (Outcome.Kind)
	{
	case EDTCardKind::Weapon:
		Result = Weapons ? Weapons->Give(OwnerId, Outcome.Weapon, 1) : EDTWeaponResult::BadPlayer;
		break;
	case EDTCardKind::WeaponUpgrade:
		Result = Weapons ? Weapons->LevelUp(OwnerId, Outcome.Weapon, Outcome.TargetLevel) : EDTWeaponResult::BadPlayer;
		break;
	case EDTCardKind::Mod:
		Result = Weapons ? Weapons->Socket(OwnerId, Outcome.Weapon, Outcome.Mod) : EDTWeaponResult::BadPlayer;
		break;
	case EDTCardKind::Reserve:
		ApplyReserve(OwnerId, Outcome.Reserve);
		break;
	default:
		break;
	}
	if (Result != EDTWeaponResult::Ok)
	{
		UE_LOG(LogDT, Warning, TEXT("Offer p%d: weapon change for %s refused (%d)"), OwnerId, *Outcome.Weapon.ToString(), static_cast<int32>(Result));
	}
}

void UDTOfferSubsystem::ApplyReserve(int32 OwnerId, EDTReserveKind Kind)
{
	UWorld* World = GetWorld();
	UDTWeaponSubsystem* Weapons = World ? World->GetSubsystem<UDTWeaponSubsystem>() : nullptr;
	UDTHeroSubsystem* Hero = World ? World->GetSubsystem<UDTHeroSubsystem>() : nullptr;
	UDTGemSubsystem* Gems = World ? World->GetSubsystem<UDTGemSubsystem>() : nullptr;
	const FDTHeroState* State = Hero ? Hero->GetState(OwnerId) : nullptr;
	switch (Kind)
	{
	case EDTReserveKind::Hone:
		if (Weapons && Flows.IsValidIndex(OwnerId))
		{
			// Stacks multiplicatively; the stat system (FDTStatSet) will own this slice when it lands.
			FDTWeaponHeroStats Stats;
			Stats.DamageMult = FMath::Pow(1.0f + DTConst::OFFER_RESERVE_HONE_DAMAGE_RATIO, static_cast<float>(++Flows[OwnerId].HoneStacks));
			Weapons->SetHeroStats(OwnerId, Stats);
		}
		break;
	case EDTReserveKind::Mend:
		if (State)
		{
			FDTHeroStats Stats = State->Stats;
			Stats.MaxHpBonus += static_cast<float>(DTConst::OFFER_RESERVE_MEND_MAX_HP);
			Hero->SetStats(OwnerId, Stats);
		}
		break;
	case EDTReserveKind::Restore:
		if (State)
		{
			Hero->Heal(OwnerId, DTConst::OFFER_RESERVE_RESTORE_HP_RATIO * State->MaxHp);
		}
		break;
	case EDTReserveKind::Salvage:
		if (Gems && Flows.IsValidIndex(OwnerId))
		{
			FDTProgressionStats Stats = Gems->GetStats(OwnerId);
			Stats.PickupRadiusMul = FMath::Pow(1.0f + DTConst::OFFER_RESERVE_SALVAGE_RADIUS_RATIO, static_cast<float>(++Flows[OwnerId].SalvageStacks));
			Gems->SetStats(OwnerId, Stats);
		}
		break;
	default:
		break; // SpareTerms is run state (applied by DTOffers::ApplyPickState)
	}
}

// ---- pause, cursor, input -----------------------------------------------------------------------------------------------

bool UDTOfferSubsystem::IsAnyHandPending() const
{
	const FDTRunState* Run = GetRun();
	if (!Run)
	{
		return false;
	}
	for (int32 Owner = 0; Owner < Run->GetNumPlayers(); ++Owner)
	{
		if (Run->GetPlayer(Owner).Offer.PendingHands.Num() > 0)
		{
			return true;
		}
	}
	return false;
}

void UDTOfferSubsystem::HandleStepEnd()
{
	// A level-up or chest inside this step queued a hand: stop on this step, not at the frame boundary (R-TEC-14).
	UDTSimSubsystem* SimSys = Sim.Get();
	if (SimSys && !SimSys->IsPauseRequested() && IsAnyHandPending())
	{
		SimSys->RequestPause();
	}
}

bool UDTOfferSubsystem::CanUnpause() const
{
	return !IsAnyHandOpen();
}

void UDTOfferSubsystem::SetPaused(bool bWanted)
{
	UWorld* World = GetWorld();
	AGameModeBase* GameMode = World ? World->GetAuthGameMode() : nullptr;
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!GameMode || !PC)
	{
		return;
	}
	if (bWanted && !bPausedByOffers)
	{
		bPausedByOffers = GameMode->SetPause(PC, FCanUnpause::CreateUObject(this, &UDTOfferSubsystem::CanUnpause));
	}
	else if (!bWanted && bPausedByOffers)
	{
		bPausedByOffers = false;
		GameMode->ClearPause();
	}
}

void UDTOfferSubsystem::SetCursor(bool bShow)
{
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || bShow == bCursorShown)
	{
		return;
	}
	if (bShow)
	{
		bCursorWasShown = PC->ShouldShowMouseCursor();
		PC->SetShowMouseCursor(true);
	}
	else
	{
		PC->SetShowMouseCursor(bCursorWasShown);
	}
	bCursorShown = bShow;
}

void UDTOfferSubsystem::HandleLocalInput()
{
	// Solo: the first local player drives player 0's hand (co-op hands are the co-op release, D-004).
	constexpr int32 Owner = 0;
	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!PC || !Flows.IsValidIndex(Owner) || !GetOpenHand(Owner) || !Screen)
	{
		return;
	}
	FPlayerFlow& Flow = Flows[Owner];
	const double Now = FPlatformTime::Seconds();
	TArray<FDTOfferCommand> Commands;
	for (const FKey& Key : DTOfferInput::Keys())
	{
		const bool bDown = PC->IsInputKeyDown(Key);
		if (bDown && !Flow.Down.Contains(Key))
		{
			Commands.Add(DTOfferInput::ForKey(Key));
		}
		if (bDown)
		{
			Flow.Down.Add(Key);
		}
		else
		{
			Flow.Down.Remove(Key);
		}
	}
	float MX = 0.0f;
	float MY = 0.0f;
	if (PC->GetMousePosition(MX, MY) && Screen->HasLayout())
	{
		const FVector2D Mouse(MX, MY);
		if (!Mouse.Equals(Flow.LastMouse))
		{
			const int32 Hover = Screen->CardAt(Mouse);
			if (Hover != INDEX_NONE)
			{
				Flow.Focus = Hover;
			}
		}
		Flow.LastMouse = Mouse;
		const bool bMouseDown = PC->IsInputKeyDown(EKeys::LeftMouseButton);
		if (bMouseDown && !Flow.bMouseDown)
		{
			const int32 Clicked = Screen->CardAt(Mouse);
			const EDTOfferAction Tool = Screen->ToolAt(Mouse);
			if (Clicked != INDEX_NONE)
			{
				Commands.Add({ EDTOfferAction::PickSlot, Clicked });
			}
			else if (Tool != EDTOfferAction::None)
			{
				Commands.Add({ Tool, INDEX_NONE });
			}
		}
		Flow.bMouseDown = bMouseDown;
	}

	for (const FDTOfferCommand& Command : Commands)
	{
		const FDTOfferHand* Hand = GetOpenHand(Owner);
		if (!Hand)
		{
			return;
		}
		const int32 Num = Hand->Cards.Num();
		if (FDTOfferInputGuard::IsConfirmType(Command.Action) && !Flow.Guard.Allows(Now))
		{
			continue; // R-OFF-11 (e)
		}
		switch (Command.Action)
		{
		case EDTOfferAction::Prev: Flow.Focus = (Flow.Focus + Num - 1) % FMath::Max(1, Num); break;
		case EDTOfferAction::Next: Flow.Focus = (Flow.Focus + 1) % FMath::Max(1, Num); break;
		case EDTOfferAction::Confirm: Pick(Owner, Flow.Focus); return;
		case EDTOfferAction::PickSlot:
			if (Command.Slot < Num)
			{
				Pick(Owner, Command.Slot);
				return;
			}
			break;
		case EDTOfferAction::Reroll: Reroll(Owner); break;
		case EDTOfferAction::Banish: Banish(Owner, Flow.Focus); break;
		case EDTOfferAction::Lock: Lock(Owner, Flow.Focus); break;
		default: break;
		}
	}
}

void UDTOfferSubsystem::DrawScreen(UCanvas* Canvas)
{
	constexpr int32 Owner = 0;
	FDTRunState* Run = GetRun();
	const FDTOfferHand* Hand = GetOpenHand(Owner);
	if (!Screen || !Run || !Hand)
	{
		return;
	}
	FDTOfferScreenModel Model;
	Model.Hand = Hand;
	Model.Player = &Run->GetPlayer(Owner);
	Model.Focus = Flows[Owner].Focus;
	Model.bGuardActive = !Flows[Owner].Guard.Allows(FPlatformTime::Seconds());
	Screen->Draw(Canvas, Model);
}
