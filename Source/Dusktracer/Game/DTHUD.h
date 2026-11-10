// HUD: per-player hero state (combat_core.md Text: HP, dash charges, Intercession meter), the XP bar (progression.md
// R-PRG-11), the three tool counters, and outside Shipping the localisation round-trip line (12_Production §2 exit test)
// and a frame-time readout. Where each sits is 02_Systems/hud.md.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Game/DTKeeperMeters.h"
#include "DTHUD.generated.h"

class FDTHUDFrameTimes;
struct FDTPlayerOfferState;

namespace DTHudText
{
	/** The three tool counters (hud.md R-HUD-07): each tool's name from ST_UI Offer.Tool.* with the uses the player has left. */
	DUSKTRACER_API void Tools(const FDTPlayerOfferState& Offer, TArray<FText>& Out);
	/** The run clock, minutes and seconds of sim time since run start. */
	DUSKTRACER_API FText Clock(int64 StepIndex);
}

/**
 * Draws the in-run readouts into the rects of DTHud::Layout (hud.md R-HUD-01): top left the HP block and, outside
 * Shipping, ST_UI Test.LocRoundTrip in the current culture and fps / frame ms / game-thread ms; top right the clock, the
 * level and the XP bar; top centre the boss row, the director's banner and the reaction toast; bottom left the
 * Intercession ring (Hud.IntercessionReady when full), the dash pips with the refilling one partly filled, the tool
 * counters, and above them the Keeper meters a player holds (keepers.md R-KEE-38), Hud.AimMode while aiming by hand
 * and the dormant sockets (modifiers.md R-MOD-08). Hud.RunOver when every hero is down, Hud.Victory on a win.
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
	void DrawRun();
	void DrawEliteLabels();

	/** Receives the engine's per-frame unit times (IPerformanceDataConsumer); registered while the HUD is in play. */
	TSharedPtr<FDTHUDFrameTimes> FrameTimes;
	/** The round-trip line is logged once as drawn, so Tools/Smoke.ps1 can check the culture without reading pixels. */
	bool bLoggedRoundTrip = false;
	TArray<int32> LabelScratch;
	TArray<FDTHudMeter> MeterScratch;
};
