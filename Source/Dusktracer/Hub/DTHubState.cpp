#include "Hub/DTHubState.h"

namespace DTHubStateLocal
{
	// Reference pixels at a 1080-line canvas: presentation, not tuning.
	constexpr float PanelBase = 1000.0f;
	constexpr float SubLineBase = 44.0f;
	constexpr float DetailLineBase = 40.0f;
	constexpr int32 DetailLines = 3;
	constexpr float FooterBase = 60.0f;
}

void FDTHubState::Open(const TArray<FDTHubPatron>& InPatrons, EDTPath LastPatron)
{
	Patrons = InPatrons;
	Page = EDTHubPage::Root;
	Scroll = 0;
	Selected = EDTPath::None;
	for (const FDTHubPatron& Patron : Patrons)
	{
		if (Patron.Path == LastPatron && Patron.bOpen)
		{
			Selected = LastPatron;
		}
	}
	Focus = static_cast<int32>(Selected == EDTPath::None ? EDTHubRow::Patron : EDTHubRow::Begin);
}

int32 FDTHubState::NumRows() const
{
	switch (Page)
	{
	case EDTHubPage::Root: return static_cast<int32>(EDTHubRow::Num);
	case EDTHubPage::Patron: return Patrons.Num();
	case EDTHubPage::Tree: return TreeRows;
	case EDTHubPage::Codex: return CodexRows;
	default: return 0;
	}
}

void FDTHubState::Enter(EDTHubPage Target)
{
	Page = Target;
	Focus = 0;
	Scroll = 0;
	if (Target == EDTHubPage::Patron)
	{
		// On the chosen Keeper, else on the first the player can take.
		const int32 Chosen = Patrons.IndexOfByPredicate([this](const FDTHubPatron& Patron) { return Patron.Path == Selected; });
		const int32 Open = Patrons.IndexOfByPredicate([](const FDTHubPatron& Patron) { return Patron.bOpen; });
		Focus = FMath::Max(Chosen != INDEX_NONE ? Chosen : Open, 0);
	}
}

void FDTHubState::ToRoot(EDTHubRow Row)
{
	Page = EDTHubPage::Root;
	Focus = static_cast<int32>(Row);
	Scroll = 0;
}

void FDTHubState::AcceptRoot(FDTHubOutcome& Outcome)
{
	switch (RootRow())
	{
	case EDTHubRow::Patron: Enter(EDTHubPage::Patron); break;
	case EDTHubRow::Tree: Enter(EDTHubPage::Tree); break;
	case EDTHubRow::Codex: Enter(EDTHubPage::Codex); break;
	case EDTHubRow::Stats: Enter(EDTHubPage::Stats); break;
	case EDTHubRow::Settings: Outcome.Action = EDTHubAction::Settings; break;
	case EDTHubRow::Begin:
		if (Selected == EDTPath::None)
		{
			Enter(EDTHubPage::Patron); // a run needs its Patron first (R-VIG-07)
		}
		else
		{
			Outcome.Action = EDTHubAction::Begin;
		}
		break;
	default: Outcome.Action = EDTHubAction::Quit; break;
	}
}

void FDTHubState::KeepInWindow()
{
	Scroll = FMath::Clamp(Scroll, FMath::Max(0, Focus - Window + 1), Focus);
}

FDTHubOutcome FDTHubState::Apply(EDTMenuCommand Command)
{
	FDTHubOutcome Outcome;
	const int32 Rows = NumRows();
	switch (Command)
	{
	case EDTMenuCommand::Up:
	case EDTMenuCommand::Down:
		if (Rows > 0)
		{
			Focus = (Focus + Rows + (Command == EDTMenuCommand::Up ? -1 : 1)) % Rows;
			KeepInWindow();
		}
		break;
	case EDTMenuCommand::Left:
	case EDTMenuCommand::Right:
		// A long page moves a window at a time, and stops at its ends.
		if (Rows > 0 && (Page == EDTHubPage::Codex || Page == EDTHubPage::Tree))
		{
			Focus = FMath::Clamp(Focus + (Command == EDTMenuCommand::Left ? -Window : Window), 0, Rows - 1);
			KeepInWindow();
		}
		break;
	case EDTMenuCommand::Accept:
		if (Page == EDTHubPage::Root)
		{
			AcceptRoot(Outcome);
		}
		else if (Page == EDTHubPage::Patron)
		{
			if (Patrons.IsValidIndex(Focus) && Patrons[Focus].bOpen)
			{
				Selected = Patrons[Focus].Path;
				ToRoot(EDTHubRow::Begin);
			}
		}
		else if (Page == EDTHubPage::Tree && Rows > 0)
		{
			Outcome.Action = EDTHubAction::TreeAccept;
			Outcome.Row = Focus;
		}
		break;
	case EDTMenuCommand::Back:
		if (Page == EDTHubPage::Root)
		{
			Focus = static_cast<int32>(EDTHubRow::Quit); // nothing above the root, and Back never leaves the game by itself (R-VIG-04)
		}
		else
		{
			ToRoot(Page == EDTHubPage::Patron ? EDTHubRow::Patron : (Page == EDTHubPage::Tree ? EDTHubRow::Tree : (Page == EDTHubPage::Codex ? EDTHubRow::Codex : EDTHubRow::Stats)));
		}
		break;
	default:
		break;
	}
	return Outcome;
}

float DTHubLayout::PanelHeight()
{
	return DTHubStateLocal::PanelBase;
}

float DTHubLayout::HeaderHeight(float TextScale)
{
	return DTMenuLayout::TitleHeight(TextScale) + DTHubStateLocal::SubLineBase * TextScale;
}

float DTHubLayout::DetailHeight(float TextScale)
{
	return DTHubStateLocal::DetailLines * DTHubStateLocal::DetailLineBase * TextScale;
}

float DTHubLayout::FooterHeight(float TextScale)
{
	return DTHubStateLocal::FooterBase * TextScale;
}

int32 DTHubLayout::ListWindow(float TextScale, bool bDetail)
{
	const float Room = PanelHeight() - HeaderHeight(TextScale) - (bDetail ? DetailHeight(TextScale) : 0.0f) - FooterHeight(TextScale);
	return FMath::Max(1, FMath::FloorToInt(Room / DTMenuLayout::RowHeight(TextScale)));
}
