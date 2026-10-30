// HUD: the localisation round-trip line (12_Production §2 exit test), a frame-time readout, and per-player hero state
// (combat_core.md Text: HP, dash charges, Intercession meter).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DTHUD.generated.h"

class FDTHUDFrameTimes;

/**
 * Draws ST_UI Product.Name and Test.LocRoundTrip in the current culture, plus fps / frame ms / game-thread ms; bottom
 * left, one block per player read from UDTHeroSubsystem: HP bar, dash pips with the refilling one partly filled, the
 * Intercession meter (Hud.IntercessionReady when full), Hud.AimMode while aiming by hand; Hud.RunOver when every hero
 * is down.
 */
UCLASS()
class DUSKTRACER_API ADTHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

private:
	void DrawHeroes();

	/** Receives the engine's per-frame unit times (IPerformanceDataConsumer); registered while the HUD is in play. */
	TSharedPtr<FDTHUDFrameTimes> FrameTimes;
	/** The round-trip line is logged once as drawn, so Tools/Smoke.ps1 can check the culture without reading pixels. */
	bool bLoggedRoundTrip = false;
};
