#include "Settings/DTMenuState.h"

namespace DTMenuStateLocal
{
	// Reference pixels at a 1080-line canvas: presentation, not tuning.
	constexpr float RowBase = 44.0f;
	constexpr float TitleBase = 96.0f;
	constexpr float FooterBase = 92.0f;
}

void FDTMenuState::Open()
{
	Screen = EDTMenuScreen::Pause;
	Focus = 0;
	Column = EDTBindDevice::Kbm;
	bCapturing = false;
	Status = EDTMenuStatus::None;
}

void FDTMenuState::Close()
{
	Screen = EDTMenuScreen::None;
	bCapturing = false;
	Status = EDTMenuStatus::None;
}

void FDTMenuState::OpenRunOver(EDTRunOverKind Kind)
{
	Close();
	Screen = EDTMenuScreen::RunOver;
	Focus = 0;
	RunOver = Kind;
	Pending = EDTMenuAction::None;
}

void FDTMenuState::AcceptPause(FDTMenuOutcome& Outcome)
{
	switch (static_cast<EDTPauseRow>(Focus))
	{
	case EDTPauseRow::Resume:
		Close();
		Outcome.bClosed = true;
		break;
	case EDTPauseRow::Settings:
		Screen = EDTMenuScreen::Settings;
		Focus = 0;
		break;
	case EDTPauseRow::Restart: Ask(EDTMenuAction::Restart); break;
	case EDTPauseRow::Abandon: Ask(EDTMenuAction::Abandon); break;
	default: Ask(EDTMenuAction::Quit); break;
	}
}

void FDTMenuState::Ask(EDTMenuAction Action)
{
	Pending = Action;
	Screen = EDTMenuScreen::Confirm;
	Focus = static_cast<int32>(EDTConfirmRow::Cancel); // R-SET-20
}

void FDTMenuState::BackToPause()
{
	const EDTPauseRow Row = Pending == EDTMenuAction::Restart ? EDTPauseRow::Restart : (Pending == EDTMenuAction::Abandon ? EDTPauseRow::Abandon : EDTPauseRow::Quit);
	Pending = EDTMenuAction::None;
	Screen = EDTMenuScreen::Pause;
	Focus = static_cast<int32>(Row);
}

int32 FDTMenuState::NumRows() const
{
	switch (Screen)
	{
	case EDTMenuScreen::Pause: return static_cast<int32>(EDTPauseRow::Num);
	case EDTMenuScreen::Settings: return static_cast<int32>(EDTSettingRow::Num);
	case EDTMenuScreen::Controls: return ResetRow() + 1;
	case EDTMenuScreen::Confirm: return static_cast<int32>(EDTConfirmRow::Num);
	case EDTMenuScreen::RunOver: return static_cast<int32>(EDTRunOverRow::Num);
	default: return 0;
	}
}

FDTMenuOutcome FDTMenuState::Apply(EDTMenuCommand Command, FDTUserSettings& Settings)
{
	FDTMenuOutcome Outcome;
	const int32 Rows = NumRows();
	if (bCapturing || Rows == 0 || Command == EDTMenuCommand::None)
	{
		return Outcome;
	}
	const auto Step = [this, &Settings, &Outcome](int32 Direction)
	{
		const EDTSettingRow Row = static_cast<EDTSettingRow>(Focus);
		if (Row == EDTSettingRow::Language && bLanguagePending)
		{
			return; // R-SET-06: one press, one switch
		}
		if (DTSettings::StepRow(Settings, Row, Direction))
		{
			Outcome.bSettingsChanged = true;
			if (Row == EDTSettingRow::Language)
			{
				Outcome.bLanguageRequested = true;
				bLanguagePending = true;
			}
		}
	};

	switch (Command)
	{
	case EDTMenuCommand::Up:
		Focus = (Focus + Rows - 1) % Rows;
		break;
	case EDTMenuCommand::Down:
		Focus = (Focus + 1) % Rows;
		break;
	case EDTMenuCommand::Left:
	case EDTMenuCommand::Right:
		if (Screen == EDTMenuScreen::Settings)
		{
			Step(Command == EDTMenuCommand::Right ? 1 : -1);
		}
		else if (Screen == EDTMenuScreen::Controls)
		{
			Column = Column == EDTBindDevice::Kbm ? EDTBindDevice::Pad : EDTBindDevice::Kbm;
		}
		break;
	case EDTMenuCommand::Accept:
		if (Screen == EDTMenuScreen::Pause)
		{
			AcceptPause(Outcome);
		}
		else if (Screen == EDTMenuScreen::Confirm)
		{
			if (Focus == static_cast<int32>(EDTConfirmRow::Confirm))
			{
				Outcome.Action = Pending;
			}
			else
			{
				BackToPause();
			}
		}
		else if (Screen == EDTMenuScreen::RunOver)
		{
			Outcome.Action = Focus == static_cast<int32>(EDTRunOverRow::Restart) ? EDTMenuAction::Restart : EDTMenuAction::Quit;
		}
		else if (Screen == EDTMenuScreen::Settings)
		{
			if (Focus == static_cast<int32>(EDTSettingRow::Controls))
			{
				Screen = EDTMenuScreen::Controls;
				Focus = 0;
				Column = EDTBindDevice::Kbm;
				Status = EDTMenuStatus::None;
			}
			else
			{
				Step(1);
			}
		}
		else if (Focus == ResetRow())
		{
			Settings.Bindings = FDTBindings::Defaults();
			Status = EDTMenuStatus::ResetDone;
			Outcome.bBindingsChanged = true;
		}
		else if (DTBindings::IsRebindable(static_cast<EDTBindSlot>(Focus), Column))
		{
			bCapturing = true;
			Status = EDTMenuStatus::Capturing;
			StatusSlot = static_cast<EDTBindSlot>(Focus);
		}
		break;
	case EDTMenuCommand::Back:
		if (Screen == EDTMenuScreen::Controls)
		{
			Screen = EDTMenuScreen::Settings;
			Focus = static_cast<int32>(EDTSettingRow::Controls);
			Status = EDTMenuStatus::None;
		}
		else if (Screen == EDTMenuScreen::Settings)
		{
			Screen = EDTMenuScreen::Pause;
			Focus = static_cast<int32>(EDTPauseRow::Settings);
		}
		else if (Screen == EDTMenuScreen::Confirm)
		{
			BackToPause();
		}
		else if (Screen != EDTMenuScreen::RunOver) // the run is over: there is nothing to go back to (R-SET-21)
		{
			Close();
			Outcome.bClosed = true;
		}
		break;
	default:
		break;
	}
	return Outcome;
}

FDTMenuOutcome FDTMenuState::Capture(const FKey& Key, FDTUserSettings& Settings)
{
	FDTMenuOutcome Outcome;
	if (!bCapturing)
	{
		return Outcome;
	}
	bCapturing = false;
	if (DTMenuInput::CancelsCapture(Key))
	{
		Status = EDTMenuStatus::Cancelled;
		return Outcome;
	}
	StatusSlot = static_cast<EDTBindSlot>(Focus);
	switch (DTBindings::Rebind(Settings.Bindings, StatusSlot, Column, Key, &StatusOther))
	{
	case EDTRebindResult::Bound:
		Status = EDTMenuStatus::Bound;
		Outcome.bBindingsChanged = true;
		break;
	case EDTRebindResult::Swapped:
		Status = EDTMenuStatus::Swapped;
		Outcome.bBindingsChanged = true;
		break;
	case EDTRebindResult::Reserved:
		Status = EDTMenuStatus::Reserved;
		break;
	case EDTRebindResult::WrongDevice:
		Status = Column == EDTBindDevice::Pad ? EDTMenuStatus::WrongDevicePad : EDTMenuStatus::WrongDeviceKbm;
		break;
	}
	return Outcome;
}

const TArray<FKey>& DTMenuInput::Keys()
{
	static const TArray<FKey> All = {
		EKeys::Up, EKeys::W, EKeys::Gamepad_DPad_Up, EKeys::Gamepad_LeftStick_Up,
		EKeys::Down, EKeys::S, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_LeftStick_Down,
		EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left,
		EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right,
		EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom,
		EKeys::Escape, EKeys::Gamepad_FaceButton_Right, EKeys::Gamepad_Special_Right };
	return All;
}

EDTMenuCommand DTMenuInput::ForKey(const FKey& Key)
{
	const int32 Index = Keys().IndexOfByKey(Key);
	constexpr int32 PerDirection = 4;
	constexpr int32 AcceptFirst = 4 * PerDirection;
	constexpr int32 BackFirst = AcceptFirst + 3;
	if (Index == INDEX_NONE)
	{
		return EDTMenuCommand::None;
	}
	if (Index >= BackFirst)
	{
		return EDTMenuCommand::Back;
	}
	if (Index >= AcceptFirst)
	{
		return EDTMenuCommand::Accept;
	}
	const EDTMenuCommand Directions[] = { EDTMenuCommand::Up, EDTMenuCommand::Down, EDTMenuCommand::Left, EDTMenuCommand::Right };
	return Directions[Index / PerDirection];
}

bool DTMenuInput::CancelsCapture(const FKey& Key)
{
	return Key == EKeys::Escape || Key == EKeys::Gamepad_Special_Right;
}

float DTMenuLayout::RowHeight(float TextScale)
{
	return DTMenuStateLocal::RowBase * TextScale;
}

float DTMenuLayout::TitleHeight(float TextScale)
{
	return DTMenuStateLocal::TitleBase * TextScale;
}

float DTMenuLayout::ContentHeight(int32 Rows, float TextScale)
{
	return TitleHeight(TextScale) + Rows * RowHeight(TextScale) + DTMenuStateLocal::FooterBase * TextScale;
}
