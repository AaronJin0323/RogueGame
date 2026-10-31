// The hub as a pure state machine: commands in, a page, a focus and what to do next (02_Systems/vigil.md R-VIG-03..07).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Settings/DTMenuState.h"

enum class EDTHubPage : uint8 { Root, Patron, Tree, Codex, Stats };
/** Root rows, in screen order (R-VIG-03). */
enum class EDTHubRow : uint8 { Patron, Tree, Codex, Stats, Settings, Begin, Quit, Num };
enum class EDTHubAction : uint8 { None, Settings, Begin, Quit, TreeAccept };

struct FDTHubPatron
{
	EDTPath Path = EDTPath::None;
	bool bOpen = false;
};

struct FDTHubOutcome
{
	EDTHubAction Action = EDTHubAction::None;
	/** The Tree row for TreeAccept. */
	int32 Row = INDEX_NONE;
};

struct DUSKTRACER_API FDTHubState
{
	EDTHubPage Page = EDTHubPage::Root;
	int32 Focus = 0;
	/** First list row drawn on a scrolling page. */
	int32 Scroll = 0;
	/** List rows the screen fits at this text size. */
	int32 Window = 1;
	TArray<FDTHubPatron> Patrons;
	/** The chosen Patron, None until one is. */
	EDTPath Selected = EDTPath::None;
	int32 CodexRows = 0;
	/** The book's first read row: where the page opens (R-VIG-08). */
	int32 CodexFirst = 0;
	int32 TreeRows = 0;
	/** Tree rows focus passes over (R-VIG-20). */
	TSet<int32> TreeHeadings;

	/** Opens the root; LastPatron is chosen when it is open (R-VIG-06). */
	void Open(const TArray<FDTHubPatron>& InPatrons, EDTPath LastPatron);
	int32 NumRows() const;
	FDTHubOutcome Apply(EDTMenuCommand Command);
	EDTHubRow RootRow() const { return static_cast<EDTHubRow>(Focus); }

private:
	void Enter(EDTHubPage Target);
	void ToRoot(EDTHubRow Row);
	void AcceptRoot(FDTHubOutcome& Outcome);
	void KeepInWindow();
	bool IsHeading(int32 Row) const;
	/** One row on, wrapping, past any heading. */
	void StepFocus(int32 Step);
	/** Off a heading the focus landed on: onward, or back from the end of the page. */
	void LeaveHeading(int32 Step);
};

/** Layout of the hub in reference pixels at a 1080-line canvas (R-VIG-16). */
namespace DTHubLayout
{
	DUSKTRACER_API float PanelHeight();
	DUSKTRACER_API float HeaderHeight(float TextScale);
	DUSKTRACER_API float DetailHeight(float TextScale);
	/** Lines the detail pane holds at any text size. */
	DUSKTRACER_API int32 DetailLineCount();
	DUSKTRACER_API float FooterHeight(float TextScale);
	/** List rows that fit under the header, over the detail pane when the page has one; never under one. */
	DUSKTRACER_API int32 ListWindow(float TextScale, bool bDetail = true);
}
