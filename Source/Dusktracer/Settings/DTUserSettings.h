// The player's settings as plain values, their file and the binding table (02_Systems/settings.md).
// Presentation and input only: sim code never includes this header (R-SET-01, R-SET-02, T-SET-02).
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

enum class EDTLanguage : uint8 { En, ZhHans, Num };
enum class EDTGraphics : uint8 { Epic, High, Medium, Low, Num };
enum class EDTWindow : uint8 { Fullscreen, Windowed, Num };
enum class EDTMotion : uint8 { Full, Reduced, Custom };

/** The rows of the settings screen, in screen order (R-SET-06 onward). */
enum class EDTSettingRow : uint8 { Language, Volume, Graphics, Window, TextSize, Motion, Shake, Flash, DangerOutline, Controls, Num };

/** One rebindable action (R-SET-13). */
enum class EDTBindSlot : uint8 { MoveUp, MoveDown, MoveLeft, MoveRight, Dash, Intercession, UiPrev, UiNext, UiReroll, UiBanish, UiLock, Num };
enum class EDTBindDevice : uint8 { Kbm, Pad, Num };
enum class EDTBindGroup : uint8 { InPlay, LevelUp };
enum class EDTRebindResult : uint8 { Bound, Swapped, Reserved, WrongDevice };

/** One key per slot and device; an invalid key is a fixed pad slot (the left stick, the D-pad). */
struct DUSKTRACER_API FDTBindings
{
	FKey Keys[static_cast<int32>(EDTBindSlot::Num)][static_cast<int32>(EDTBindDevice::Num)];

	const FKey& Get(EDTBindSlot Slot, EDTBindDevice Device) const { return Keys[static_cast<int32>(Slot)][static_cast<int32>(Device)]; }
	void Set(EDTBindSlot Slot, EDTBindDevice Device, const FKey& Key) { Keys[static_cast<int32>(Slot)][static_cast<int32>(Device)] = Key; }
	bool operator==(const FDTBindings& Other) const;
	/** The table of R-SET-13. */
	static FDTBindings Defaults();
};

namespace DTBindings
{
	DUSKTRACER_API EDTBindGroup GroupOf(EDTBindSlot Slot);
	/** False where the slot has no key to rebind on this device (Move on the pad: the left stick). */
	DUSKTRACER_API bool IsRebindable(EDTBindSlot Slot, EDTBindDevice Device);
	/** The fixed keys of R-SET-13 for this group. */
	DUSKTRACER_API bool IsReserved(const FKey& Key, EDTBindGroup Group);
	/** The key is a button of this device, bindable to an action. */
	DUSKTRACER_API bool IsOfDevice(const FKey& Key, EDTBindDevice Device);
	/** R-SET-14. Other is set to the slot that gave up the key when the result is Swapped. */
	DUSKTRACER_API EDTRebindResult Rebind(FDTBindings& Bindings, EDTBindSlot Slot, EDTBindDevice Device, const FKey& Key, EDTBindSlot* Other = nullptr);
	/** Every rebindable slot of the group holds a legal key and none repeats (R-SET-04). */
	DUSKTRACER_API bool IsLegal(const FDTBindings& Bindings, EDTBindDevice Device, EDTBindGroup Group);
	/** The name a prompt shows for a key: a pad button's short name, a keyboard key's display name. */
	DUSKTRACER_API FText Glyph(const FKey& Key);
	/** The ST_UI key of the slot's action name. */
	DUSKTRACER_API const TCHAR* ActionKey(EDTBindSlot Slot);
}

struct DUSKTRACER_API FDTUserSettings
{
	EDTLanguage Language = EDTLanguage::En;
	int32 VolumePct = 0;   // set by the constructor from the constants
	EDTGraphics Graphics = EDTGraphics::Epic;
	EDTWindow Window = EDTWindow::Fullscreen;
	int32 TextPct = 0;
	bool bShake = true;
	bool bFlash = true;
	bool bDangerOutline = false;
	FDTBindings Bindings;

	FDTUserSettings();
	bool operator==(const FDTUserSettings& Other) const;

	/** Full, Reduced or Custom, read from the two motion rows (R-SET-09). */
	EDTMotion GetMotion() const;
};

namespace DTSettings
{
	/** The values in force in this process. Starts at the defaults; UDTSettingsSubsystem loads the file into it. */
	DUSKTRACER_API FDTUserSettings& Live();

	// Read points for presentation (R-SET-09, R-SET-10, R-SET-12).
	DUSKTRACER_API float TextScale();
	DUSKTRACER_API bool AllowShake();
	DUSKTRACER_API bool AllowFlash();
	DUSKTRACER_API bool DangerOutline();

	/** The file's text (R-SET-03). */
	DUSKTRACER_API FString Serialize(const FDTUserSettings& Settings);
	/** Never fails: whatever does not parse stays at its default (R-SET-04). */
	DUSKTRACER_API FDTUserSettings Parse(const FString& Text);

	/** Writes through a sibling temporary file; on failure the old file stands and the result is false. */
	DUSKTRACER_API bool Save(const FString& Path, const FDTUserSettings& Settings);
	/** False, and defaults in Out, when the file is missing or unreadable. */
	DUSKTRACER_API bool Load(const FString& Path, FDTUserSettings& Out);
	DUSKTRACER_API FString DefaultPath();
	/** R-SET-05: an unattended process touches no file unless one was named. */
	DUSKTRACER_API bool UsesFile(bool bUnattended, bool bPathGiven);

	/** One step of the row in Direction (+1 or -1); true when a value changed. Controls is not a value. */
	DUSKTRACER_API bool StepRow(FDTUserSettings& Settings, EDTSettingRow Row, int32 Direction);
	/** The engine scalability level of a Graphics value. */
	DUSKTRACER_API int32 ScalabilityLevel(EDTGraphics Graphics);
	DUSKTRACER_API const TCHAR* CultureName(EDTLanguage Language);
}
