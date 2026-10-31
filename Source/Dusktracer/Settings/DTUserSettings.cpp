#include "Settings/DTUserSettings.h"
#include "Core/DTConstantsGen.h"
#include "Dusktracer.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace DTUserSettingsLocal
{
	constexpr int32 NumSlots = static_cast<int32>(EDTBindSlot::Num);
	constexpr int32 NumDevices = static_cast<int32>(EDTBindDevice::Num);

	const TCHAR* const SlotNames[NumSlots] = { TEXT("MoveUp"), TEXT("MoveDown"), TEXT("MoveLeft"), TEXT("MoveRight"), TEXT("Dash"),
		TEXT("Intercession"), TEXT("UiPrev"), TEXT("UiNext"), TEXT("UiReroll"), TEXT("UiBanish"), TEXT("UiLock") };
	const TCHAR* const DeviceNames[NumDevices] = { TEXT("Kbm"), TEXT("Pad") };
	const TCHAR* const ActionKeys[NumSlots] = { TEXT("Controls.Action.Move.Up"), TEXT("Controls.Action.Move.Down"), TEXT("Controls.Action.Move.Left"),
		TEXT("Controls.Action.Move.Right"), TEXT("Controls.Action.Dash"), TEXT("Controls.Action.Ultimate"), TEXT("Controls.Action.UI.Prev"),
		TEXT("Controls.Action.UI.Next"), TEXT("Controls.Action.UI.Reroll"), TEXT("Controls.Action.UI.Banish"), TEXT("Controls.Action.UI.Lock") };
	const TCHAR* const GraphicsNames[] = { TEXT("Epic"), TEXT("High"), TEXT("Medium"), TEXT("Low") };
	const TCHAR* const WindowNames[] = { TEXT("Fullscreen"), TEXT("Windowed") };
	// Engine scalability levels, by EDTGraphics.
	constexpr int32 ScalabilityLevels[] = { 3, 2, 1, 0 };

	int32 Snap(int32 Value, int32 Min, int32 Max, int32 Step)
	{
		const int32 Clamped = FMath::Clamp(Value, Min, Max);
		return Min + (Clamped - Min + Step / 2) / Step * Step;
	}

	template <int32 N>
	int32 FindName(const TCHAR* const (&Names)[N], const FString& Value)
	{
		for (int32 Index = 0; Index < N; ++Index)
		{
			if (Value == Names[Index])
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	/** Parses a whole number; false for anything else. */
	bool ParseInt(const FString& Value, int32& Out)
	{
		if (Value.IsEmpty() || !Value.IsNumeric() || Value.Contains(TEXT(".")))
		{
			return false;
		}
		Out = FCString::Atoi(*Value);
		return true;
	}

	void ParseBool(const FString& Value, bool& Out)
	{
		if (Value == TEXT("1"))
		{
			Out = true;
		}
		else if (Value == TEXT("0"))
		{
			Out = false;
		}
	}
}

// ---- bindings -----------------------------------------------------------------------------------------------------------

bool FDTBindings::operator==(const FDTBindings& Other) const
{
	for (int32 Slot = 0; Slot < static_cast<int32>(EDTBindSlot::Num); ++Slot)
	{
		for (int32 Device = 0; Device < static_cast<int32>(EDTBindDevice::Num); ++Device)
		{
			if (Keys[Slot][Device] != Other.Keys[Slot][Device])
			{
				return false;
			}
		}
	}
	return true;
}

FDTBindings FDTBindings::Defaults()
{
	FDTBindings B;
	const auto Kbm = [&B](EDTBindSlot Slot, const FKey& Key) { B.Set(Slot, EDTBindDevice::Kbm, Key); };
	const auto Pad = [&B](EDTBindSlot Slot, const FKey& Key) { B.Set(Slot, EDTBindDevice::Pad, Key); };
	Kbm(EDTBindSlot::MoveUp, EKeys::W);
	Kbm(EDTBindSlot::MoveDown, EKeys::S);
	Kbm(EDTBindSlot::MoveLeft, EKeys::A);
	Kbm(EDTBindSlot::MoveRight, EKeys::D);
	Kbm(EDTBindSlot::Dash, EKeys::SpaceBar);
	Pad(EDTBindSlot::Dash, EKeys::Gamepad_RightShoulder);
	Kbm(EDTBindSlot::Intercession, EKeys::Q);
	Pad(EDTBindSlot::Intercession, EKeys::Gamepad_LeftShoulder);
	Kbm(EDTBindSlot::UiPrev, EKeys::A);
	Kbm(EDTBindSlot::UiNext, EKeys::D);
	Kbm(EDTBindSlot::UiReroll, EKeys::R);
	Pad(EDTBindSlot::UiReroll, EKeys::Gamepad_FaceButton_Left);
	Kbm(EDTBindSlot::UiBanish, EKeys::B);
	Pad(EDTBindSlot::UiBanish, EKeys::Gamepad_FaceButton_Top);
	Kbm(EDTBindSlot::UiLock, EKeys::L);
	Pad(EDTBindSlot::UiLock, EKeys::Gamepad_RightShoulder);
	return B;
}

EDTBindGroup DTBindings::GroupOf(EDTBindSlot Slot)
{
	return Slot >= EDTBindSlot::UiPrev ? EDTBindGroup::LevelUp : EDTBindGroup::InPlay;
}

bool DTBindings::IsRebindable(EDTBindSlot Slot, EDTBindDevice Device)
{
	if (Device == EDTBindDevice::Kbm)
	{
		return true;
	}
	return Slot != EDTBindSlot::MoveUp && Slot != EDTBindSlot::MoveDown && Slot != EDTBindSlot::MoveLeft && Slot != EDTBindSlot::MoveRight
		&& Slot != EDTBindSlot::UiPrev && Slot != EDTBindSlot::UiNext;
}

bool DTBindings::IsReserved(const FKey& Key, EDTBindGroup Group)
{
	static const TSet<FKey> Fixed = { EKeys::Escape, EKeys::Enter, EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right,
		EKeys::Gamepad_Special_Right, EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right,
		EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right,
		EKeys::Gamepad_LeftStick_Up, EKeys::Gamepad_LeftStick_Down, EKeys::Gamepad_LeftStick_Left, EKeys::Gamepad_LeftStick_Right,
		EKeys::Gamepad_RightStick_Up, EKeys::Gamepad_RightStick_Down, EKeys::Gamepad_RightStick_Left, EKeys::Gamepad_RightStick_Right };
	static const TSet<FKey> PicksCards = { EKeys::SpaceBar, EKeys::LeftMouseButton, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive };
	return Fixed.Contains(Key) || (Group == EDTBindGroup::LevelUp && PicksCards.Contains(Key));
}

bool DTBindings::IsOfDevice(const FKey& Key, EDTBindDevice Device)
{
	if (!Key.IsValid() || !Key.IsBindableToActions() || Key.IsAxis1D() || Key.IsAxis2D() || Key.IsAxis3D() || Key.IsTouch() || Key.IsGesture())
	{
		return false;
	}
	return Key.IsGamepadKey() == (Device == EDTBindDevice::Pad);
}

EDTRebindResult DTBindings::Rebind(FDTBindings& Bindings, EDTBindSlot Slot, EDTBindDevice Device, const FKey& Key, EDTBindSlot* Other)
{
	if (!IsOfDevice(Key, Device))
	{
		// A key that is neither device's button (an axis) has no column to name; the caller's status line still fits.
		return EDTRebindResult::WrongDevice;
	}
	const EDTBindGroup Group = GroupOf(Slot);
	if (IsReserved(Key, Group))
	{
		return EDTRebindResult::Reserved;
	}
	const FKey Old = Bindings.Get(Slot, Device);
	if (Old == Key)
	{
		return EDTRebindResult::Bound;
	}
	for (int32 Index = 0; Index < static_cast<int32>(EDTBindSlot::Num); ++Index)
	{
		const EDTBindSlot Holder = static_cast<EDTBindSlot>(Index);
		if (Holder != Slot && GroupOf(Holder) == Group && IsRebindable(Holder, Device) && Bindings.Get(Holder, Device) == Key)
		{
			Bindings.Set(Holder, Device, Old);
			Bindings.Set(Slot, Device, Key);
			if (Other)
			{
				*Other = Holder;
			}
			return EDTRebindResult::Swapped;
		}
	}
	Bindings.Set(Slot, Device, Key);
	return EDTRebindResult::Bound;
}

bool DTBindings::IsLegal(const FDTBindings& Bindings, EDTBindDevice Device, EDTBindGroup Group)
{
	TSet<FKey> Seen;
	for (int32 Index = 0; Index < static_cast<int32>(EDTBindSlot::Num); ++Index)
	{
		const EDTBindSlot Slot = static_cast<EDTBindSlot>(Index);
		if (GroupOf(Slot) != Group || !IsRebindable(Slot, Device))
		{
			continue;
		}
		const FKey& Key = Bindings.Get(Slot, Device);
		bool bRepeat = false;
		Seen.Add(Key, &bRepeat);
		if (bRepeat || !IsOfDevice(Key, Device) || IsReserved(Key, Group))
		{
			return false;
		}
	}
	return true;
}

FText DTBindings::Glyph(const FKey& Key)
{
	// Short device names, the same in every culture.
	static const TMap<FKey, FString> Pad = {
		{ EKeys::Gamepad_FaceButton_Left, TEXT("X") }, { EKeys::Gamepad_FaceButton_Top, TEXT("Y") },
		{ EKeys::Gamepad_LeftShoulder, TEXT("LB") }, { EKeys::Gamepad_RightShoulder, TEXT("RB") },
		{ EKeys::Gamepad_LeftTrigger, TEXT("LT") }, { EKeys::Gamepad_RightTrigger, TEXT("RT") },
		{ EKeys::Gamepad_LeftThumbstick, TEXT("L3") }, { EKeys::Gamepad_RightThumbstick, TEXT("R3") },
		{ EKeys::Gamepad_Special_Left, TEXT("View") } };
	if (const FString* Short = Pad.Find(Key))
	{
		return FText::AsCultureInvariant(*Short);
	}
	return Key.GetDisplayName(/*bLongDisplayName*/ false);
}

const TCHAR* DTBindings::ActionKey(EDTBindSlot Slot)
{
	return DTUserSettingsLocal::ActionKeys[static_cast<int32>(Slot)];
}

// ---- values -------------------------------------------------------------------------------------------------------------

FDTUserSettings::FDTUserSettings()
	: VolumePct(DTConst::SETTINGS_VOLUME_MAX_PCT)
	, TextPct(DTConst::SETTINGS_TEXT_MIN_PCT)
	, Bindings(FDTBindings::Defaults())
{
}

bool FDTUserSettings::operator==(const FDTUserSettings& Other) const
{
	return Language == Other.Language && VolumePct == Other.VolumePct && Graphics == Other.Graphics && Window == Other.Window
		&& TextPct == Other.TextPct && bShake == Other.bShake && bFlash == Other.bFlash && bDangerOutline == Other.bDangerOutline
		&& Bindings == Other.Bindings;
}

EDTMotion FDTUserSettings::GetMotion() const
{
	if (bShake && bFlash)
	{
		return EDTMotion::Full;
	}
	return !bShake && !bFlash ? EDTMotion::Reduced : EDTMotion::Custom;
}

FDTUserSettings& DTSettings::Live()
{
	static FDTUserSettings Settings;
	return Settings;
}

float DTSettings::TextScale() { return static_cast<float>(Live().TextPct) / DTConst::SETTINGS_TEXT_MIN_PCT; }
bool DTSettings::AllowShake() { return Live().bShake; }
bool DTSettings::AllowFlash() { return Live().bFlash; }
bool DTSettings::DangerOutline() { return Live().bDangerOutline; }

const TCHAR* DTSettings::CultureName(EDTLanguage Language)
{
	return Language == EDTLanguage::ZhHans ? TEXT("zh-Hans") : TEXT("en");
}

int32 DTSettings::ScalabilityLevel(EDTGraphics Graphics)
{
	return DTUserSettingsLocal::ScalabilityLevels[static_cast<int32>(Graphics)];
}

bool DTSettings::StepRow(FDTUserSettings& S, EDTSettingRow Row, int32 Direction)
{
	const auto Wrap = [Direction](auto Value, int32 Count)
	{
		return static_cast<decltype(Value)>((static_cast<int32>(Value) + Direction + Count) % Count);
	};
	const FDTUserSettings Was = S;
	switch (Row)
	{
	case EDTSettingRow::Language:
		S.Language = Wrap(S.Language, static_cast<int32>(EDTLanguage::Num));
		break;
	case EDTSettingRow::Volume:
		S.VolumePct = FMath::Clamp(S.VolumePct + Direction * DTConst::SETTINGS_VOLUME_STEP_PCT, 0, DTConst::SETTINGS_VOLUME_MAX_PCT);
		break;
	case EDTSettingRow::Graphics:
		S.Graphics = Wrap(S.Graphics, static_cast<int32>(EDTGraphics::Num));
		break;
	case EDTSettingRow::Window:
		S.Window = Wrap(S.Window, static_cast<int32>(EDTWindow::Num));
		break;
	case EDTSettingRow::TextSize:
		S.TextPct = FMath::Clamp(S.TextPct + Direction * DTConst::SETTINGS_TEXT_STEP_PCT, DTConst::SETTINGS_TEXT_MIN_PCT, DTConst::SETTINGS_TEXT_MAX_PCT);
		break;
	case EDTSettingRow::Motion:
	{
		// Custom is never stepped onto (R-SET-09): right goes Full, left goes Reduced; from either end the other one.
		const EDTMotion Motion = S.GetMotion();
		const bool bFull = Motion == EDTMotion::Custom ? Direction > 0 : Motion == EDTMotion::Reduced;
		S.bShake = bFull;
		S.bFlash = bFull;
		break;
	}
	case EDTSettingRow::Shake:
		S.bShake = !S.bShake;
		break;
	case EDTSettingRow::Flash:
		S.bFlash = !S.bFlash;
		break;
	case EDTSettingRow::DangerOutline:
		S.bDangerOutline = !S.bDangerOutline;
		break;
	default:
		return false;
	}
	return !(S == Was);
}

// ---- file ---------------------------------------------------------------------------------------------------------------

FString DTSettings::Serialize(const FDTUserSettings& S)
{
	using namespace DTUserSettingsLocal;
	FString Out = FString::Printf(TEXT("Version=%d\n"), DTConst::SETTINGS_FILE_VERSION);
	Out += FString::Printf(TEXT("Language=%s\n"), CultureName(S.Language));
	Out += FString::Printf(TEXT("Volume=%d\n"), S.VolumePct);
	Out += FString::Printf(TEXT("Graphics=%s\n"), GraphicsNames[static_cast<int32>(S.Graphics)]);
	Out += FString::Printf(TEXT("Window=%s\n"), WindowNames[static_cast<int32>(S.Window)]);
	Out += FString::Printf(TEXT("Text=%d\n"), S.TextPct);
	Out += FString::Printf(TEXT("Shake=%d\n"), S.bShake ? 1 : 0);
	Out += FString::Printf(TEXT("Flash=%d\n"), S.bFlash ? 1 : 0);
	Out += FString::Printf(TEXT("Outline=%d\n"), S.bDangerOutline ? 1 : 0);
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		for (int32 Device = 0; Device < NumDevices; ++Device)
		{
			if (DTBindings::IsRebindable(static_cast<EDTBindSlot>(Slot), static_cast<EDTBindDevice>(Device)))
			{
				Out += FString::Printf(TEXT("Bind.%s.%s=%s\n"), SlotNames[Slot], DeviceNames[Device], *S.Bindings.Keys[Slot][Device].ToString());
			}
		}
	}
	return Out;
}

FDTUserSettings DTSettings::Parse(const FString& Text)
{
	using namespace DTUserSettingsLocal;
	FDTUserSettings S;
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	for (const FString& Line : Lines)
	{
		FString Key;
		FString Value;
		if (!Line.Split(TEXT("="), &Key, &Value))
		{
			continue;
		}
		Key.TrimStartAndEndInline();
		Value.TrimStartAndEndInline();
		int32 Number = 0;
		if (Key == TEXT("Language"))
		{
			if (Value == CultureName(EDTLanguage::En) || Value == CultureName(EDTLanguage::ZhHans))
			{
				S.Language = Value == CultureName(EDTLanguage::ZhHans) ? EDTLanguage::ZhHans : EDTLanguage::En;
			}
		}
		else if (Key == TEXT("Volume"))
		{
			if (ParseInt(Value, Number))
			{
				S.VolumePct = Snap(Number, 0, DTConst::SETTINGS_VOLUME_MAX_PCT, DTConst::SETTINGS_VOLUME_STEP_PCT);
			}
		}
		else if (Key == TEXT("Text"))
		{
			if (ParseInt(Value, Number))
			{
				S.TextPct = Snap(Number, DTConst::SETTINGS_TEXT_MIN_PCT, DTConst::SETTINGS_TEXT_MAX_PCT, DTConst::SETTINGS_TEXT_STEP_PCT);
			}
		}
		else if (Key == TEXT("Graphics"))
		{
			const int32 Found = FindName(GraphicsNames, Value);
			S.Graphics = Found != INDEX_NONE ? static_cast<EDTGraphics>(Found) : S.Graphics;
		}
		else if (Key == TEXT("Window"))
		{
			const int32 Found = FindName(WindowNames, Value);
			S.Window = Found != INDEX_NONE ? static_cast<EDTWindow>(Found) : S.Window;
		}
		else if (Key == TEXT("Shake"))
		{
			ParseBool(Value, S.bShake);
		}
		else if (Key == TEXT("Flash"))
		{
			ParseBool(Value, S.bFlash);
		}
		else if (Key == TEXT("Outline"))
		{
			ParseBool(Value, S.bDangerOutline);
		}
		else if (Key.StartsWith(TEXT("Bind.")))
		{
			TArray<FString> Parts;
			Key.ParseIntoArray(Parts, TEXT("."));
			const int32 Slot = Parts.Num() == 3 ? FindName(SlotNames, Parts[1]) : INDEX_NONE;
			const int32 Device = Parts.Num() == 3 ? FindName(DeviceNames, Parts[2]) : INDEX_NONE;
			if (Slot != INDEX_NONE && Device != INDEX_NONE && DTBindings::IsRebindable(static_cast<EDTBindSlot>(Slot), static_cast<EDTBindDevice>(Device)))
			{
				S.Bindings.Keys[Slot][Device] = FKey(*Value);
			}
		}
	}

	// An illegal table falls back whole for its device and group (R-SET-04).
	const FDTBindings Defaults = FDTBindings::Defaults();
	for (const EDTBindDevice Device : { EDTBindDevice::Kbm, EDTBindDevice::Pad })
	{
		for (const EDTBindGroup Group : { EDTBindGroup::InPlay, EDTBindGroup::LevelUp })
		{
			if (DTBindings::IsLegal(S.Bindings, Device, Group))
			{
				continue;
			}
			for (int32 Slot = 0; Slot < NumSlots; ++Slot)
			{
				if (DTBindings::GroupOf(static_cast<EDTBindSlot>(Slot)) == Group)
				{
					S.Bindings.Set(static_cast<EDTBindSlot>(Slot), Device, Defaults.Get(static_cast<EDTBindSlot>(Slot), Device));
				}
			}
		}
	}
	return S;
}

bool DTSettings::Save(const FString& Path, const FDTUserSettings& Settings)
{
	IFileManager& Files = IFileManager::Get();
	Files.MakeDirectory(*FPaths::GetPath(Path), true);
	const FString Temp = Path + TEXT(".tmp");
	if (Files.DirectoryExists(*Temp) || !FFileHelper::SaveStringToFile(Serialize(Settings), *Temp)
		|| !Files.Move(*Path, *Temp, /*bReplace*/ true, /*bEvenIfReadOnly*/ true))
	{
		UE_LOG(LogDT, Warning, TEXT("Settings: could not write %s; the old file stands"), *Path);
		return false;
	}
	return true;
}

bool DTSettings::Load(const FString& Path, FDTUserSettings& Out)
{
	Out = FDTUserSettings();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return false;
	}
	Out = Parse(Text);
	return true;
}

FString DTSettings::DefaultPath()
{
	FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("DTSettings.ini")));
	FPaths::NormalizeFilename(Path);
	return Path;
}

bool DTSettings::UsesFile(bool bUnattended, bool bPathGiven)
{
	return !bUnattended || bPathGiven;
}
