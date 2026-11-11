// HUD: the localisation round-trip line (12_Production §2 exit test), a frame-time readout, and per-player hero state
// (combat_core.md Text: HP, dash charges, Intercession meter), and the XP bar (progression.md R-PRG-11).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Game/DTKeeperMeters.h"
#include "Player/DTHeroTypes.h"
#include "DTHUD.generated.h"

class FDTHUDFrameTimes;
class UDTInputSet;
class UInputAction;

/**
 * Draws ST_UI Product.Name and Test.LocRoundTrip in the current culture, plus fps / frame ms / game-thread ms; bottom
 * left, one block per player read from UDTHeroSubsystem: HP bar, dash pips with the refilling one partly filled, the
 * Intercession meter (Hud.IntercessionReady when full), Hud.AimMode while aiming by hand, the Keeper meters a player holds
 * (keepers.md R-KEE-38); Hud.RunOver when every hero is down. The Dash and Intercession labels carry the key bound on the
 * device in use (combat_core.md R-CMB-13).
 */
UCLASS()
class DUSKTRACER_API ADTHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

	/**
	 * Label with the key Set binds to Action on Device, as Hud.KeyLabel lays them out (R-CMB-13). Read from the mapping
	 * context each call, so it follows a rebinding and a device switch; Label alone when the device binds no key.
	 */
	static FText KeyLabel(const FText& Label, const UDTInputSet* Set, const UInputAction* Action, EDTInputDevice Device);

private:
	void DrawHeroes();
	void DrawEliteLabels();

	/** Receives the engine's per-frame unit times (IPerformanceDataConsumer); registered while the HUD is in play. */
	TSharedPtr<FDTHUDFrameTimes> FrameTimes;
	/** The round-trip line is logged once as drawn, so Tools/Smoke.ps1 can check the culture without reading pixels. */
	bool bLoggedRoundTrip = false;
	TArray<int32> LabelScratch;
	TArray<FDTHudMeter> MeterScratch;
	/** The key labels are logged once as drawn for player 0, so Tools/Smoke.ps1 can read them in each culture. */
	bool bLoggedKeyLabels = false;
};
