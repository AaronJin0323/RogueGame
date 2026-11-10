#include "Hub/DTHubScreen.h"
#include "CanvasItem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Hub/DTHubTab.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferStyle.h"
#include "Settings/DTUserSettings.h"

namespace DTHubScreenLocal
{
	// Reference pixels at a 1080-line canvas; everything scales with the canvas height.
	constexpr float RefHeight = 1080.0f;
	constexpr float PanelW = 1400.0f;
	constexpr float Pad = 32.0f;
	constexpr float Thin = 2.0f;
	constexpr float Inset = 6.0f;
	constexpr float TitleScale = 2.0f;
	constexpr float BodyScale = 1.5f;
	constexpr float DetailScale = 1.25f;
	constexpr float HintAlpha = 0.75f;
	constexpr float SealedAlpha = 0.5f;

	const TCHAR* const UiKeys[] = { TEXT("Hub.Balance"), TEXT("Hub.Begin.NeedPatron"), TEXT("Hub.Notice.SaveAside"), TEXT("Hub.Notice.RunLost"), TEXT("Hub.Tree.Empty"), TEXT("Hub.Hint.Read"), TEXT("Hub.Stats.Balance"),
		TEXT("Hub.Stats.Earned"), TEXT("Hub.Stats.Runs"), TEXT("Hub.Stats.Wins"), TEXT("Hub.Stats.Repairs"), TEXT("Hub.Stats.Kills"), TEXT("Hub.Stats.Elites"),
		TEXT("Hub.Stats.Bosses"), TEXT("RunSummary.Time"), TEXT("Codex.Count"), TEXT("Codex.Locked"), TEXT("Menu.Hint.Pause"), TEXT("Pause.Quit") };

	FText Ui(const TCHAR* Key)
	{
		return FText::FromStringTable(TEXT("ST_UI"), Key);
	}

	FLinearColor Token(const FColor& Color, float Alpha = 1.0f)
	{
		FLinearColor Linear = DTPal::Linear(Color);
		Linear.A = Alpha;
		return Linear;
	}

	struct FPainter
	{
		UCanvas* Canvas = nullptr;
		UFont* Font = nullptr;
		float S = 1.0f;
		float T = 1.0f;

		void Tile(float X, float Y, float W, float H, const FLinearColor& Color) const
		{
			FCanvasTileItem Item(FVector2D(X, Y), FVector2D(W, H), Color);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
		void Frame(float X, float Y, float W, float H, const FLinearColor& Color) const
		{
			const float Line = Thin * S;
			Tile(X, Y, W, Line, Color);
			Tile(X, Y + H - Line, W, Line, Color);
			Tile(X, Y, Line, H, Color);
			Tile(X + W - Line, Y, Line, H, Color);
		}
		FVector2D Measure(const FString& Line, float Scale) const
		{
			float W = 0.0f;
			float H = 0.0f;
			Canvas->TextSize(Font, Line, W, H, Scale * S * T, Scale * S * T);
			return FVector2D(W, H);
		}
		void Text(const FText& Line, float X, float Y, float Scale, const FLinearColor& Color) const
		{
			FCanvasTextItem Item(FVector2D(X, Y), Line, Font, Color);
			Item.Scale = FVector2D(Scale * S * T, Scale * S * T);
			Item.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(Item);
		}
		void Centered(const FText& Line, float CenterX, float Y, float Scale, const FLinearColor& Color) const
		{
			Text(Line, CenterX - 0.5f * static_cast<float>(Measure(Line.ToString(), Scale).X), Y, Scale, Color);
		}
	};

	/** One list row: label, value on the right, a frame on focus. */
	struct FRow
	{
		FText Label;
		FText Value;
		float Alpha = 1.0f;
		bool bHeading = false;
	};

	TArray<FRow> Rows(const FDTHubView& View)
	{
		const FDTHubState& State = *View.State;
		TArray<FRow> Out;
		switch (State.Page)
		{
		case EDTHubPage::Root:
			for (int32 Row = 0; Row < static_cast<int32>(EDTHubRow::Num); ++Row)
			{
				const EDTHubRow Kind = static_cast<EDTHubRow>(Row);
				Out.Add({ DTHubText::RootLabel(Kind), Kind == EDTHubRow::Patron && State.Selected != EDTPath::None ? DTHubText::KeeperName(State.Selected) : FText::GetEmpty() });
			}
			break;
		case EDTHubPage::Patron:
			for (const FDTHubPatron& Patron : State.Patrons)
			{
				const FText Marker = !Patron.bOpen ? FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine.Locked"))
					: (Patron.Path == State.Selected ? FText::FromStringTable(TEXT("ST_World"), TEXT("World.Patron")) : FText::GetEmpty());
				Out.Add({ DTHubText::KeeperName(Patron.Path), Marker, Patron.bOpen ? 1.0f : SealedAlpha });
			}
			break;
		case EDTHubPage::Tree:
			if (View.Tree)
			{
				for (int32 Row = 0; Row < View.Tree->NumRows(); ++Row)
				{
					Out.Add({ View.Tree->RowLabel(Row), View.Tree->RowValue(Row), View.Tree->IsDimmed(Row) ? SealedAlpha : 1.0f, View.Tree->IsHeading(Row) });
				}
			}
			break;
		case EDTHubPage::Codex:
			for (const FDTCodexRow& Line : *View.Codex)
			{
				Out.Add({ Line.Title, FText::GetEmpty(), Line.bRead ? 1.0f : SealedAlpha });
			}
			break;
		default:
			for (const FDTHubStat& Stat : DTHubData::Stats(*View.Profile))
			{
				Out.Add({ Stat.Label, Stat.Value });
			}
			break;
		}
		return Out;
	}

	FText SubLine(const FDTHubView& View)
	{
		const FDTHubState& State = *View.State;
		switch (State.Page)
		{
		case EDTHubPage::Root: return FText::Format(Ui(TEXT("Hub.Balance")), FText::AsNumber(View.Profile->Currency));
		case EDTHubPage::Patron: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine.Prompt"));
		case EDTHubPage::Tree: return View.Tree ? View.Tree->Header() : Ui(TEXT("Hub.Tree.Empty"));
		case EDTHubPage::Codex:
		{
			const int32 Read = View.Codex->FilterByPredicate([](const FDTCodexRow& Row) { return Row.bRead; }).Num();
			return FText::Format(Ui(TEXT("Codex.Count")), FText::AsNumber(Read), FText::AsNumber(View.Codex->Num()));
		}
		default: return FText::GetEmpty();
		}
	}
}

TArray<FString> DTHubText::AllKeys()
{
	TArray<FString> Keys;
	for (const TCHAR* Key : DTHubScreenLocal::UiKeys)
	{
		Keys.Add(Key);
	}
	return Keys;
}

FText DTHubText::PageTitle(EDTHubPage Page)
{
	switch (Page)
	{
	case EDTHubPage::Patron: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine"));
	case EDTHubPage::Tree: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Contracts"));
	case EDTHubPage::Codex: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Codex"));
	case EDTHubPage::Stats: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.LedgerStone"));
	default: return FText::FromStringTable(TEXT("ST_World"), TEXT("World.Vigil"));
	}
}

FText DTHubText::RootLabel(EDTHubRow Row)
{
	switch (Row)
	{
	case EDTHubRow::Patron: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine"));
	case EDTHubRow::Tree: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Contracts"));
	case EDTHubRow::Codex: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Codex"));
	case EDTHubRow::Stats: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.LedgerStone"));
	case EDTHubRow::Settings: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Settings"));
	case EDTHubRow::Begin: return FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Begin"));
	default: return DTHubScreenLocal::Ui(TEXT("Pause.Quit"));
	}
}

FText DTHubText::KeeperName(EDTPath Path)
{
	return FText::FromStringTable(FName(TEXT("ST_Keepers")), FString(DTOffers::PathName(Path)) + TEXT(".Name"));
}

FText DTHubText::Notice(EDTMetaNotice Kind, const FString& AsideFile)
{
	switch (Kind)
	{
	case EDTMetaNotice::KeptAside: return FText::Format(DTHubScreenLocal::Ui(TEXT("Hub.Notice.SaveAside")), FText::FromString(AsideFile));
	case EDTMetaNotice::RunLost: return DTHubScreenLocal::Ui(TEXT("Hub.Notice.RunLost"));
	default: return FText::GetEmpty();
	}
}

FText DTHubText::Detail(const FDTHubView& View)
{
	const FDTHubState& State = *View.State;
	if (State.Page == EDTHubPage::Root)
	{
		// One line at a time: what the focused row needs comes before what the launch has to say.
		return State.RootRow() == EDTHubRow::Begin && State.Selected == EDTPath::None ? DTHubScreenLocal::Ui(TEXT("Hub.Begin.NeedPatron")) : View.Notice;
	}
	if (State.Page == EDTHubPage::Patron && State.Patrons.IsValidIndex(State.Focus))
	{
		const FDTHubPatron& Patron = State.Patrons[State.Focus];
		return Patron.bOpen ? FText::FromStringTable(FName(TEXT("ST_Keepers")), FString(DTOffers::PathName(Patron.Path)) + TEXT(".Desc"))
			: FText::FromStringTable(TEXT("ST_World"), TEXT("Hub.Shrine.LockedHint"));
	}
	if (State.Page == EDTHubPage::Codex && View.Codex->IsValidIndex(State.Focus))
	{
		return (*View.Codex)[State.Focus].Body;
	}
	if (State.Page == EDTHubPage::Tree && View.Tree && State.Focus < View.Tree->NumRows())
	{
		return View.Tree->RowDetail(State.Focus);
	}
	return FText::GetEmpty();
}

int32 DTHubScreen::Draw(UCanvas* Canvas, const FDTHubView& View, const FDTUserSettings& Settings)
{
	using namespace DTHubScreenLocal;
	if (!Canvas || !GEngine || !View.State || !View.Codex || !View.Profile)
	{
		return 0;
	}
	int32 Overflow = 0;
	const FDTHubState& State = *View.State;
	const FPainter P{ Canvas, GEngine->GetLargeFont(), Canvas->ClipY / RefHeight, static_cast<float>(Settings.TextPct) / DTConst::SETTINGS_TEXT_MIN_PCT };
	const FLinearColor Text = Token(DTPal::UI_Text);
	const FLinearColor Hint = Token(DTPal::UI_Text, HintAlpha);

	P.Tile(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, Token(DTPal::UI_Panel)); // no town: the hub is a page, not a view of the arena
	const float W = FMath::Min(PanelW * P.S, Canvas->ClipX - 2.0f * Pad * P.S);
	const float H = DTHubLayout::PanelHeight() * P.S;
	const float X = 0.5f * (Canvas->ClipX - W);
	const float Y0 = 0.5f * (Canvas->ClipY - H);
	const float Left = X + Pad * P.S;
	const float Right = X + W - Pad * P.S;
	const float Inner = Right - Left;
	const float CentreX = X + 0.5f * W;
	P.Frame(X, Y0, W, H, Hint);

	P.Centered(DTHubText::PageTitle(State.Page), CentreX, Y0 + Pad * 0.5f * P.S, TitleScale, Text);
	const FText Sub = SubLine(View);
	if (!Sub.IsEmpty())
	{
		P.Centered(Sub, CentreX, Y0 + DTMenuLayout::TitleHeight(P.T) * P.S, DetailScale, Hint);
	}

	const float RowH = DTMenuLayout::RowHeight(P.T) * P.S;
	const float TextH = static_cast<float>(P.Measure(TEXT("Mg"), BodyScale).Y);
	const bool bFocusable = State.Page != EDTHubPage::Stats;
	const TArray<FRow> List = Rows(View);
	float Y = Y0 + DTHubLayout::HeaderHeight(P.T) * P.S;
	const int32 Last = FMath::Min(List.Num(), State.Scroll + State.Window);
	for (int32 Row = State.Scroll; Row < Last; ++Row)
	{
		const float TextY = Y + 0.5f * (RowH - TextH);
		const FLinearColor Colour = List[Row].bHeading ? Hint : Token(DTPal::UI_Text, List[Row].Alpha);
		if (bFocusable && Row == State.Focus)
		{
			P.Frame(Left, Y + Inset * P.S, Inner, RowH - 2.0f * Inset * P.S, Text);
		}
		P.Text(List[Row].Label, Left + 2.0f * Inset * P.S, TextY, BodyScale, Colour);
		float Used = static_cast<float>(P.Measure(List[Row].Label.ToString(), BodyScale).X);
		if (!List[Row].Value.IsEmpty())
		{
			const float ValueW = static_cast<float>(P.Measure(List[Row].Value.ToString(), BodyScale).X);
			P.Text(List[Row].Value, Right - 2.0f * Inset * P.S - ValueW, TextY, BodyScale, Colour);
			Used += ValueW + 2.0f * Inset * P.S;
		}
		Overflow += Used > Inner - 4.0f * Inset * P.S ? 1 : 0;
		Y += RowH;
	}

	// Detail pane over the footer; wraps by the offer screen's rules, so zh breaks where zh may.
	float DetailY = Y0 + H - (DTHubLayout::DetailHeight(P.T) + DTHubLayout::FooterHeight(P.T)) * P.S;
	const FText Detail = DTHubText::Detail(View);
	if (!Detail.IsEmpty())
	{
		const float LineH = DTHubLayout::DetailHeight(P.T) * P.S / DTHubLayout::DetailLineCount();
		const TArray<FString> Lines = DTOfferText::WrapLines(Detail.ToString(), [&P](const FString& Part) { return static_cast<float>(P.Measure(Part, DetailScale).X); }, Inner);
		Overflow += Lines.Num() > DTHubLayout::DetailLineCount() ? 1 : 0;
		for (const FString& Line : Lines)
		{
			P.Text(FText::AsCultureInvariant(Line), Left, DetailY, DetailScale, Text);
			DetailY += LineH;
		}
	}
	const FText Footer = State.Page == EDTHubPage::Tree && View.Tree ? FText::FromStringTable(TEXT("ST_Tree"), TEXT("Tree.Hint"))
		: Ui(State.Page == EDTHubPage::Codex || State.Page == EDTHubPage::Stats ? TEXT("Hub.Hint.Read") : TEXT("Menu.Hint.Pause"));
	const float FooterY = Y0 + H - DTHubLayout::FooterHeight(P.T) * P.S;
	P.Centered(Footer, CentreX, FooterY + 8.0f * P.S, DetailScale, Hint);
	Overflow += P.Measure(Footer.ToString(), DetailScale).X > Inner ? 1 : 0;
	return Overflow;
}
