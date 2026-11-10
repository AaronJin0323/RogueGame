// The pause, settings, controls, confirmation and run-over screens as a pure state machine: commands in, settings
// changed, actions asked for (settings.md R-SET-14..17, R-SET-19..22).
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Settings/DTUserSettings.h"

enum class EDTMenuScreen : uint8 { None, Pause, Settings, Controls, Confirm, RunOver };
enum class EDTMenuCommand : uint8 { None, Up, Down, Left, Right, Accept, Back };
enum class EDTPauseRow : uint8 { Resume, Settings, Restart, Abandon, Quit, Num };
enum class EDTConfirmRow : uint8 { Confirm, Cancel, Num };
/** ReturnToHub comes after Num: it is a row only when the process has a hub, and then it is the first (vigil.md R-VIG-13). */
enum class EDTRunOverRow : uint8 { Restart, Quit, Num, ReturnToHub };
/** What ending the run a screen asks for; the confirmation holds one until it is answered. */
enum class EDTMenuAction : uint8 { None, Restart, Abandon, Quit, ReturnToHub };
/** How the run ended, for the run-over screen's title. */
enum class EDTRunOverKind : uint8 { Death, Victory, Abandoned };
/** What the Controls status line says. */
enum class EDTMenuStatus : uint8 { None, Capturing, Cancelled, Bound, Swapped, Reserved, WrongDevicePad, WrongDeviceKbm, ResetDone };

/** What one command did, for the subsystem to act on. */
struct FDTMenuOutcome
{
	bool bClosed = false;
	bool bSettingsChanged = false;
	bool bBindingsChanged = false;
	bool bLanguageRequested = false;
	EDTMenuAction Action = EDTMenuAction::None;
};

struct DUSKTRACER_API FDTMenuState
{
	EDTMenuScreen Screen = EDTMenuScreen::None;
	int32 Focus = 0;
	EDTBindDevice Column = EDTBindDevice::Kbm;
	bool bCapturing = false;
	EDTMenuStatus Status = EDTMenuStatus::None;
	EDTBindSlot StatusSlot = EDTBindSlot::Dash;
	EDTBindSlot StatusOther = EDTBindSlot::Dash;
	/** A culture switch is queued or running: further language steps are ignored (R-SET-06). */
	bool bLanguagePending = false;
	/** What the open confirmation will do (R-SET-20). */
	EDTMenuAction Pending = EDTMenuAction::None;
	EDTRunOverKind RunOver = EDTRunOverKind::Death;
	/** Opened from the hub: Back from Settings or a confirmation closes (vigil.md R-VIG-11). */
	bool bHubHost = false;
	/** This process has a hub: the run-over screen leads with the way back (vigil.md R-VIG-13). */
	bool bHubRow = false;

	void Open();
	/** The hub's Quit row: the confirmation alone, on Cancel. */
	void OpenQuitConfirm();
	/** What the run-over row at this position is. */
	EDTRunOverRow RunOverRowAt(int32 Row) const;
	void Close();
	/** The run ended: the only screen left for it (R-SET-21). */
	void OpenRunOver(EDTRunOverKind Kind);
	int32 NumRows() const;
	/** The Controls row that resets, after the slots. */
	static int32 ResetRow() { return static_cast<int32>(EDTBindSlot::Num); }

	/** Navigation (R-SET-17). Ignored while capturing. */
	FDTMenuOutcome Apply(EDTMenuCommand Command, FDTUserSettings& Settings);
	/** The next key press during a capture (R-SET-15). */
	FDTMenuOutcome Capture(const FKey& Key, FDTUserSettings& Settings);

private:
	void AcceptPause(FDTMenuOutcome& Outcome);
	void Ask(EDTMenuAction Action);
	void BackToPause();
};

/** The fixed navigation keys (R-SET-17), never affected by the binding table. */
namespace DTMenuInput
{
	DUSKTRACER_API const TArray<FKey>& Keys();
	DUSKTRACER_API EDTMenuCommand ForKey(const FKey& Key);
	/** Esc and pad Menu end a capture. */
	DUSKTRACER_API bool CancelsCapture(const FKey& Key);
	/**
	 * A left stick pushed up or down a little off its axis crosses the sideways threshold too, and sideways pages a hub list by a
	 * whole window. So a stick's direction key counts only while the other axis is not the larger one; X and Y are the stick's two
	 * axes now. Any other key counts, and so does a stick key with no axis reading (a simulated press).
	 */
	DUSKTRACER_API bool StickKeyCounts(const FKey& Key, float X, float Y);
	/**
	 * The pause and settings screens' reading of one poll (R-SET-17): forgets every key of Down that came up, then gives the
	 * command of each fixed key that went down since, in the keys' order, and holds it in Down until it comes up. IsDown is
	 * the keys as they count now, so a stick's slip off its axis is no press (StickKeyCounts).
	 */
	DUSKTRACER_API TArray<EDTMenuCommand> Presses(TSet<FKey>& Down, TFunctionRef<bool(const FKey&)> IsDown);
}

/** Layout of the screens in reference pixels at a 1080-line canvas (R-SET-12). */
namespace DTMenuLayout
{
	DUSKTRACER_API float RowHeight(float TextScale);
	DUSKTRACER_API float TitleHeight(float TextScale);
	/** Title, rows and footer together. */
	DUSKTRACER_API float ContentHeight(int32 Rows, float TextScale);
}
