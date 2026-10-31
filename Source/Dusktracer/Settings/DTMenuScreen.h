// Draws the pause, settings and controls screens on the HUD canvas (settings.md R-SET-18): grey box, tokens, ST_UI.
#pragma once

#include "CoreMinimal.h"
#include "Settings/DTMenuState.h"

class UCanvas;

namespace DTMenuText
{
	/** Every ST_UI key these screens can draw (T-SET-18). */
	DUSKTRACER_API TArray<FString> AllKeys();
	/** The focus hint of a row; empty when the row needs none. */
	DUSKTRACER_API FText RowHint(EDTSettingRow Row);
	DUSKTRACER_API FText RowLabel(EDTSettingRow Row);
	DUSKTRACER_API FText RowValue(EDTSettingRow Row, const FDTUserSettings& Settings);
	DUSKTRACER_API FText StatusLine(const FDTMenuState& State);
}

namespace DTMenuScreen
{
	DUSKTRACER_API void Draw(UCanvas* Canvas, const FDTMenuState& State, const FDTUserSettings& Settings);
}
