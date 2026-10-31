// The pause, settings and controls screens as a pure state machine: commands in, settings changed (settings.md R-SET-14..17).
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Settings/DTUserSettings.h"

enum class EDTMenuScreen : uint8 { None, Pause, Settings, Controls };
enum class EDTMenuCommand : uint8 { None, Up, Down, Left, Right, Accept, Back };
enum class EDTPauseRow : uint8 { Resume, Settings, Num };
/** What the Controls status line says. */
enum class EDTMenuStatus : uint8 { None, Capturing, Cancelled, Bound, Swapped, Reserved, WrongDevicePad, WrongDeviceKbm, ResetDone };

/** What one command did, for the subsystem to act on. */
struct FDTMenuOutcome
{
	bool bClosed = false;
	bool bSettingsChanged = false;
	bool bBindingsChanged = false;
	bool bLanguageRequested = false;
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

	void Open();
	void Close();
	int32 NumRows() const;
	/** The Controls row that resets, after the slots. */
	static int32 ResetRow() { return static_cast<int32>(EDTBindSlot::Num); }

	/** Navigation (R-SET-17). Ignored while capturing. */
	FDTMenuOutcome Apply(EDTMenuCommand Command, FDTUserSettings& Settings);
	/** The next key press during a capture (R-SET-15). */
	FDTMenuOutcome Capture(const FKey& Key, FDTUserSettings& Settings);
};

/** The fixed navigation keys (R-SET-17), never affected by the binding table. */
namespace DTMenuInput
{
	DUSKTRACER_API const TArray<FKey>& Keys();
	DUSKTRACER_API EDTMenuCommand ForKey(const FKey& Key);
	/** Esc and pad Menu end a capture. */
	DUSKTRACER_API bool CancelsCapture(const FKey& Key);
}

/** Layout of the screens in reference pixels at a 1080-line canvas (R-SET-12). */
namespace DTMenuLayout
{
	DUSKTRACER_API float RowHeight(float TextScale);
	DUSKTRACER_API float TitleHeight(float TextScale);
	/** Title, rows and footer together. */
	DUSKTRACER_API float ContentHeight(int32 Rows, float TextScale);
}
