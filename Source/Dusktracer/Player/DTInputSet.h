// The gameplay input actions and mapping contexts, built in C++ at startup (02_Systems/combat_core.md R-CMB-02, R-CMB-05).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DTInputSet.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * IA_Move IA_Aim IA_Dash IA_Intercession IA_Pause and the two contexts IMC_Gameplay_KBM and IMC_Gameplay_Pad, both
 * added at the same priority so either device works at any time. Created with NewObject (no binary input assets, all
 * content authorable headless, R-TEC-06). There is no fire action (R-CMB-05).
 *
 * IA_Move and IA_Aim report screen axes (X right, Y up); ADTHero maps them onto the sim plane.
 */
UCLASS(Transient)
class DUSKTRACER_API UDTInputSet : public UObject
{
	GENERATED_BODY()

public:
	/** Builds every action and both contexts under Outer. */
	static UDTInputSet* Create(UObject* Outer);

	/** The five gameplay actions, in the order of combat_core.md Data. */
	TArray<UInputAction*> GetActions() const;
	TArray<UInputMappingContext*> GetContexts() const;

	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> Aim;
	UPROPERTY() TObjectPtr<UInputAction> Dash;
	UPROPERTY() TObjectPtr<UInputAction> Intercession;
	UPROPERTY() TObjectPtr<UInputAction> Pause;
	UPROPERTY() TObjectPtr<UInputMappingContext> Kbm;
	UPROPERTY() TObjectPtr<UInputMappingContext> Pad;

private:
	void Build();
};
