// The offer flow in a game world (02_Systems/offers.md): queues and opens hands per player, pauses the sim while a hand
// is open (07_Technical R-TEC-14), reads the screen's input, applies picks and writes telemetry. The cards themselves come
// from the pure generator (Offers/DTOfferGenerator.h).
//
// Hand flow:
//   - Anyone queues a hand by appending to FDTPlayerOfferState::PendingHands (progression: level-up and chest pickup) or
//     through BuildHand(OwnerId, HandKind). A queued hand with no cards is a request: it is built when it opens, so it
//     sees every earlier pick. Order: the Patron hand, then level-up hands in level order (L8 -> Keeper hand, L16 ->
//     Reforging hand), then chest hands (R-OFF-14). PendingHands[0] is the open hand while one is open.
//   - The run-start Patron hand is queued for every player without a Patron; the world is paused from OnWorldBeginPlay,
//     so it opens before the first sim step (D-013, R-OFF-08).
//   - While any hand is open the game is paused through AGameModeBase::SetPause with a CanUnpause guard (the pause
//     menu cannot unpause an open hand). A hand that becomes pending inside a step requests the sim pause at that step's
//     end (UDTSimSubsystem::OnStepEnd -> RequestPause), so no later step of the same frame runs before the hand opens;
//     the request is cleared once no hand is open or pending.
//   - dt.Offer.AutoPick (default -1 = on when the process runs -unattended, e.g. Tools/Smoke.ps1 and Perf.ps1; 0 off;
//     1 on; also -DTOfferAutoPick) picks slot 0 of every hand at once, so unattended runs never wait on a hand.
//
// Seams: OnBlessingTaken(OwnerId, BlessingId) after a blessing is appended to FDTKeeperRunState::Owned (the keepers
// package turns it into weapon flags and Intercession content); OnKeeperChosen(OwnerId, Path, bPatron) after a commit.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DTRunState.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferScreen.h"
#include "DTOfferSubsystem.generated.h"

class UCanvas;
class UDTSimSubsystem;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDTOnBlessingTaken, int32 /*OwnerId*/, FName /*BlessingId*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FDTOnKeeperChosen, int32 /*OwnerId*/, EDTPath /*Path*/, bool /*bPatron*/);

/** Per player counts for DT.Offer.Stats (the run_end numbers live in telemetry). */
struct FDTOfferPlayerStats
{
	int32 Decisions = 0;
	int32 HandsByKind[static_cast<int32>(EDTHandKind::Patron) + 1] = {};
	int32 CardsOffered = 0;
	int32 ReserveCards = 0;
	int32 Rerolls = 0;
	int32 Banishes = 0;
	int32 Locks = 0;
	double WallPausedSeconds = 0.0;
};

UCLASS()
class DUSKTRACER_API UDTOfferSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	// --- offers.md API -----------------------------------------------------------------------------------------------
	/** Queues a hand request for OwnerId (Level 0 = the player's current level). */
	void BuildHand(int32 OwnerId, EDTHandKind HandKind, int32 Level = 0);
	bool Reroll(int32 OwnerId);
	bool Banish(int32 OwnerId, int32 Slot);
	bool Lock(int32 OwnerId, int32 Slot);
	bool Pick(int32 OwnerId, int32 Slot);
	/**
	 * Tool path for the Bot v1 no-pick arm (director.md R-DIR-01): closes the open hand with no pick and no effect; not a
	 * decision. Never offered to players; the run-start Patron hand cannot be declined (D-013). Returns false when no
	 * declinable hand is open.
	 */
	bool Decline(int32 OwnerId);

	/** Opens queued hands and sets the pause now (Tick does this every frame). */
	void UpdateFlow();
	/** The open hand of OwnerId, or nullptr. */
	const FDTOfferHand* GetOpenHand(int32 OwnerId) const;
	bool IsAnyHandOpen() const;
	int32 GetFocus(int32 OwnerId) const;
	/** Debug and capture: move the focus to Slot of the open hand. */
	void SetFocus(int32 OwnerId, int32 Slot);
	const FDTOfferPlayerStats* GetStats(int32 OwnerId) const;
	int32 GetHoneStacks(int32 OwnerId) const;
	/** Reserve kinds whose effect lives outside the run state (R-OFF-12); a pick calls it, tests may too. */
	void ApplyReserve(int32 OwnerId, EDTReserveKind Kind);
	/** The pool the generator reads for OwnerId now: unlocks and each weapon's composed caps. */
	FDTOfferPool MakePool(int32 OwnerId) const;

	/** HUD hook (ADTHUD::DrawHUD): draws the local player's open hand, if any. */
	void DrawScreen(UCanvas* Canvas);

	/** dt.Offer.AutoPick resolved for this process. */
	static bool IsAutoPick();

	FDTOnBlessingTaken OnBlessingTaken;
	FDTOnKeeperChosen OnKeeperChosen;

private:
	struct FPlayerFlow
	{
		bool bOpen = false;
		int32 NextHandId = 0;
		int32 Focus = 0;
		FDTOfferInputGuard Guard;
		double OpenedWall = 0.0;
		TSet<FKey> Down;
		FVector2D LastMouse = FVector2D::ZeroVector;
		bool bMouseDown = false;
		int32 HoneStacks = 0;
		int32 SalvageStacks = 0;
		FDTOfferPlayerStats Stats;
	};

	FDTRunState* GetRun() const;
	void SyncPlayers();
	bool OpenNext(int32 OwnerId);
	void ShowHand(int32 OwnerId, const FDTOfferHand& Hand);
	void ApplyWorld(int32 OwnerId, const FDTPickOutcome& Outcome);
	void HandleLocalInput();
	void SetPaused(bool bWanted);
	bool CanUnpause() const;
	/** UDTSimSubsystem::OnStepEnd: requests the sim pause when any player has a hand pending (07_Technical R-TEC-14). */
	void HandleStepEnd();
	bool IsAnyHandPending() const;
	void SetCursor(bool bShow);

	TArray<FPlayerFlow> Flows;
	TWeakObjectPtr<UDTSimSubsystem> Sim;
	int64 LastStep = -1;
	FDelegateHandle StepEndHandle;
	bool bPausedByOffers = false;
	bool bCursorShown = false;
	bool bCursorWasShown = false;

	UPROPERTY(Transient)
	TObjectPtr<UDTOfferScreen> Screen;
};
