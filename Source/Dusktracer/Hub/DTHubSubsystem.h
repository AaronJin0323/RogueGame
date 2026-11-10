// Runs the hub in a world (02_Systems/vigil.md): polls the menu keys, holds the pages, starts the run.
#pragma once

#include "CoreMinimal.h"
#include "Hub/DTHubData.h"
#include "InputCoreTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DTHubSubsystem.generated.h"

class IDTHubTab;
class UCanvas;

UCLASS()
class DUSKTRACER_API UDTHubSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }

	/** The game mode found a hub world: reads the profile and opens the root (R-VIG-01). */
	void Enter();
	bool IsActive() const { return bActive; }
	/** One command, as a key press gives it (tests and proofs). */
	void Command(EDTMenuCommand Command);
	void Draw(UCanvas* Canvas);
	const FDTHubState& GetState() const { return State; }
	const TArray<FDTCodexRow>& GetCodex() const { return Codex; }
	const IDTHubTab* GetTree() const { return Tree.Get(); }
	/** Strings of the last frame drawn that ran past their room (R-VIG-16). */
	int32 GetOverflow() const { return Overflow; }
#if !UE_BUILD_SHIPPING
	/** Proof seam (Tools/VigilCheck.ps1 launch Large, T-VIG-16): every Codex row read and every Keeper open, on this hub's pages only. */
	void ShowEverythingForCheck();
#endif
	/** Test seam: receives the map options instead of opening the map. */
	void SetBeginOverride(TFunction<void(const FString&)> InOverride) { BeginOverride = MoveTemp(InOverride); }

private:
	void PollKeys();
	/** The window at this text size and the Tree's rows, as the next command or frame needs them. */
	void SyncPage();
	void HandleOutcome(const FDTHubOutcome& Outcome);
	void BeginRun();
	void FinishBegin();
	bool IsAcceptHeld() const;
	const FDTMetaProfile* Profile() const;

	FDTHubState State;
	TArray<FDTCodexRow> Codex;
	FDTMetaProfile Empty;
	TSharedPtr<IDTHubTab> Tree;
	FDTHubKeys Keys;
	TFunction<void(const FString&)> BeginOverride;
	FString PendingOptions;
	int32 Overflow = 0;
	bool bActive = false;
	bool bBeginPending = false;
	bool bMenuLastFrame = false;
};
