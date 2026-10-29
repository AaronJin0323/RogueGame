// S0 HUD: the localisation round-trip line (12_Production §2 exit test) and a frame-time readout.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DTHUD.generated.h"

class FDTHUDFrameTimes;

/** Draws ST_UI Product.Name and Test.LocRoundTrip in the current culture, plus fps / frame ms / game-thread ms. */
UCLASS()
class DUSKTRACER_API ADTHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

private:
	/** Receives the engine's per-frame unit times (IPerformanceDataConsumer); registered while the HUD is in play. */
	TSharedPtr<FDTHUDFrameTimes> FrameTimes;
};
