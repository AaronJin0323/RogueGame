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
	/** The one run-summary line (meta.md R-MET-06). */
	DUSKTRACER_API FText RunSummary(int64 Retained, int64 Balance);
	/** Summary.Waiting: the run's bank is in memory, not in the file yet (meta.md R-MET-11). */
	DUSKTRACER_API FText SaveWaiting();
	/** Summary.NotSaved: the session is stuck on its save, and nothing from the run was kept (meta.md R-MET-12, R-MET-06). */
	DUSKTRACER_API FText RunNotSaved();
	/**
	 * What the run-over screen says of a bank (meta.md R-MET-06): the line where Summary.Line goes, and the one under the panel.
	 * Not saved: Summary.NotSaved and nothing under it. Deferred: Summary.Line and Summary.Waiting. Else Summary.Line and the
	 * Discovery toast.
	 */
	DUSKTRACER_API void RunOverLines(bool bNotSaved, bool bDeferred, int64 Retained, int64 Balance, TConstArrayView<FName> Discovered, FText& OutLine, FText& OutToast);
	/** The one toast of a run's Discoveries (meta.md R-MET-44); empty with none. */
	DUSKTRACER_API FText DiscoveryToast(TConstArrayView<FName> Ids);
	/** The ST_UI key of the screen's title (run-over screens carry the HUD's end line, R-SET-21). */
	DUSKTRACER_API const TCHAR* TitleKey(const FDTMenuState& State);
	/** The ST_UI key of the line under a confirmation's rows; null from the hub, where no run ends (vigil.md R-VIG-11). */
	DUSKTRACER_API const TCHAR* ConfirmBodyKey(const FDTMenuState& State);
	/** The ST_UI key of a plain row (pause, confirmation, run-over screens). */
	DUSKTRACER_API const TCHAR* PlainRowKey(const FDTMenuState& State, int32 Row);
}

namespace DTMenuScreen
{
	/** RunSummary: the line under the run-over rows, empty until the run is banked. Toast: one line under the panel, on the same terms. */
	DUSKTRACER_API void Draw(UCanvas* Canvas, const FDTMenuState& State, const FDTUserSettings& Settings, const FText& RunSummary = FText::GetEmpty(), const FText& Toast = FText::GetEmpty());
}
