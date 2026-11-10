// The Reclaimer's pawn: samples input and draws the hero; never moves the sim (02_Systems/combat_core.md R-CMB-01, R-CMB-02).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Player/DTHeroTypes.h"
#include "Player/DTInputSet.h"
#include "DTHero.generated.h"

class UDTHeroFigureComponent;
class UDTHeroSubsystem;
struct FInputActionValue;

/**
 * Each frame, builds this player's FDTHeroInput from Enhanced Input (both mapping contexts active) and latches it into
 * UDTHeroSubsystem; the sim's Input phase hands it to the step. Drawn by the hero subsystem at the sim's interpolation
 * alpha (PrevPos -> Pos). Has no movement component, no collision and no physics: moving the actor changes nothing in
 * the sim. Drawn as the Reclaimer's figure (02_Systems/hero_art.md): rigid parts posed from the hero's sim state, facing the aim direction.
 */
UCLASS()
class DUSKTRACER_API ADTHero : public APawn
{
	GENERATED_BODY()

public:
	/** Rebuilds the input contexts from the live table (settings.md R-SET-14). */
	void RefreshBindings();

	ADTHero();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void PawnClientRestart() override;
	virtual void UnPossessed() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/** The OwnerId this pawn samples input for and draws. */
	int32 GetPlayerIndex() const { return PlayerIndex; }
	void SetPlayerIndex(int32 InPlayerIndex) { PlayerIndex = InPlayerIndex; }

	/** Places the pawn at the interpolated sim position and poses the figure from the state (facing, stride, dash). Reads the state only. */
	void DrawFromSim(const FDTHeroState& State, float Alpha);

	/** IA_Move / IA_Aim report screen axes (X right, Y up); the camera looks along +X, so screen up is +X, right is +Y. */
	static FVector2f ScreenToPlane(const FVector2D& Screen);

	UDTInputSet* GetInputSet() const { return InputSet; }

	/** The drawn figure (tests and staged frames read it). */
	UDTHeroFigureComponent* GetFigure() const { return Figure; }

	/** A dash or Intercession press counts only with the world running (settings.md R-SET-27). */
	static bool TakesPress(bool bWorldPaused) { return !bWorldPaused; }

private:
	void OnDashStarted(const FInputActionValue& Value);
	void OnIntercessionStarted(const FInputActionValue& Value);
	void OnPauseStarted(const FInputActionValue& Value);
	void AddMappingContexts();
	void RemoveMappingContexts();
	/** Samples the axes and the cursor into Frame (presses were set by the action callbacks). */
	void SampleFrame();
	UDTHeroSubsystem* GetHeroSubsystem() const;

	UPROPERTY(VisibleAnywhere, Category = "Hero")
	TObjectPtr<USceneComponent> Root;

	/** The drawn figure (presentation; the header is named in DTHero.cpp only, so nothing that includes this file depends on it). */
	UPROPERTY(VisibleAnywhere, Category = "Hero")
	TObjectPtr<UDTHeroFigureComponent> Figure;

	UPROPERTY(Transient)
	TObjectPtr<UDTInputSet> InputSet;

	FDTHeroInput Frame;
	FVector2D LastMousePos = FVector2D::ZeroVector;
	int32 PlayerIndex = 0;
	bool bMouseAim = false;
};
