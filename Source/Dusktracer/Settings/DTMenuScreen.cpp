#include "Settings/DTMenuScreen.h"
#include "CanvasItem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace DTMenuScreenLocal
{
	// Reference pixels at a 1080-line canvas; everything scales with the canvas height.
	constexpr float RefHeight = 1080.0f;
	constexpr float PanelW = 1120.0f;
	constexpr float Pad = 32.0f;
	constexpr float Thin = 2.0f;
	constexpr float FocusInset = 6.0f;
	constexpr float TitleScale = 2.0f;
	constexpr float BodyScale = 1.5f;
	constexpr float HeaderScale = 1.2f;
	constexpr float HintScale = 1.25f;
	constexpr float DimAlpha = 0.72f;
	constexpr float PanelAlpha = 0.96f;
	constexpr float HintAlpha = 0.75f;
	constexpr float FixedAlpha = 0.5f;
	constexpr float LabelColumn = 0.42f;  // of the inner width: where the controls columns start
	constexpr float PadColumn = 0.72f;

	const TCHAR* const RowKeys[] = { TEXT("Settings.Language"), TEXT("Settings.MasterVolume"), TEXT("Settings.Graphics"), TEXT("Settings.Window"),
		TEXT("Settings.TextSize"), TEXT("Settings.Motion"), TEXT("Settings.Shake"), TEXT("Settings.Flash"), TEXT("Settings.DangerOutline"), TEXT("Settings.Controls") };
	const TCHAR* const GraphicsKeys[] = { TEXT("Settings.Graphics.Epic"), TEXT("Settings.Graphics.High"), TEXT("Settings.Graphics.Medium"), TEXT("Settings.Graphics.Low") };
	const TCHAR* const MotionKeys[] = { TEXT("Settings.Motion.Full"), TEXT("Settings.Motion.Reduced"), TEXT("Settings.Motion.Custom") };
	const TCHAR* const WindowKeys[] = { TEXT("Settings.Window.Fullscreen"), TEXT("Settings.Window.Windowed") };
	const TCHAR* const OtherKeys[] = { TEXT("Pause.Title"), TEXT("Pause.Resume"), TEXT("Pause.Settings"), TEXT("Settings.Title"), TEXT("Settings.ValueArrows"),
		TEXT("Settings.Percent"), TEXT("Settings.LanguageName.en"), TEXT("Settings.LanguageName.zhHans"), TEXT("Settings.On"), TEXT("Settings.Off"),
		TEXT("Settings.Controls.Open"), TEXT("Settings.Hint.Volume"), TEXT("Settings.Hint.Graphics"), TEXT("Settings.Hint.Shake"), TEXT("Settings.Hint.Flash"),
		TEXT("Settings.Hint.DangerOutline"), TEXT("Menu.Hint.Pause"), TEXT("Menu.Hint.Settings"), TEXT("Menu.Hint.Controls"), TEXT("Controls.Title"),
		TEXT("Controls.Header.Action"), TEXT("Controls.Header.KBM"), TEXT("Controls.Header.Pad"), TEXT("Controls.Fixed"), TEXT("Controls.PressKey"),
		TEXT("Controls.PressPad"), TEXT("Controls.Capturing"), TEXT("Controls.Cancelled"), TEXT("Controls.WrongDevice.Pad"), TEXT("Controls.WrongDevice.KBM"),
		TEXT("Controls.Reserved"), TEXT("Controls.Swapped"), TEXT("Controls.Bound"), TEXT("Controls.Reset"), TEXT("Controls.ResetDone") };

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
		FVector2D Measure(const FText& Line, float Scale) const
		{
			float W = 0.0f;
			float H = 0.0f;
			Canvas->TextSize(Font, Line.ToString(), W, H, Scale * S * T, Scale * S * T);
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
			Text(Line, CenterX - 0.5f * static_cast<float>(Measure(Line, Scale).X), Y, Scale, Color);
		}
	};
}

TArray<FString> DTMenuText::AllKeys()
{
	using namespace DTMenuScreenLocal;
	TArray<FString> Keys;
	const auto Add = [&Keys](const TCHAR* const* List, int32 Num)
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			Keys.Add(List[Index]);
		}
	};
	Add(RowKeys, UE_ARRAY_COUNT(RowKeys));
	Add(GraphicsKeys, UE_ARRAY_COUNT(GraphicsKeys));
	Add(MotionKeys, UE_ARRAY_COUNT(MotionKeys));
	Add(WindowKeys, UE_ARRAY_COUNT(WindowKeys));
	Add(OtherKeys, UE_ARRAY_COUNT(OtherKeys));
	for (int32 Slot = 0; Slot < static_cast<int32>(EDTBindSlot::Num); ++Slot)
	{
		Keys.Add(DTBindings::ActionKey(static_cast<EDTBindSlot>(Slot)));
	}
	return Keys;
}

FText DTMenuText::RowLabel(EDTSettingRow Row)
{
	return DTMenuScreenLocal::Ui(DTMenuScreenLocal::RowKeys[static_cast<int32>(Row)]);
}

FText DTMenuText::RowHint(EDTSettingRow Row)
{
	using DTMenuScreenLocal::Ui;
	switch (Row)
	{
	case EDTSettingRow::Volume: return Ui(TEXT("Settings.Hint.Volume"));
	case EDTSettingRow::Graphics: return Ui(TEXT("Settings.Hint.Graphics"));
	case EDTSettingRow::Motion:
	case EDTSettingRow::Shake: return Ui(TEXT("Settings.Hint.Shake"));
	case EDTSettingRow::Flash: return Ui(TEXT("Settings.Hint.Flash"));
	case EDTSettingRow::DangerOutline: return Ui(TEXT("Settings.Hint.DangerOutline"));
	default: return FText::GetEmpty();
	}
}

FText DTMenuText::RowValue(EDTSettingRow Row, const FDTUserSettings& S)
{
	using namespace DTMenuScreenLocal;
	const auto OnOff = [](bool bOn) { return Ui(bOn ? TEXT("Settings.On") : TEXT("Settings.Off")); };
	const auto Percent = [](int32 Value) { return FText::Format(Ui(TEXT("Settings.Percent")), FText::AsNumber(Value)); };
	switch (Row)
	{
	case EDTSettingRow::Language: return Ui(S.Language == EDTLanguage::ZhHans ? TEXT("Settings.LanguageName.zhHans") : TEXT("Settings.LanguageName.en"));
	case EDTSettingRow::Volume: return Percent(S.VolumePct);
	case EDTSettingRow::Graphics: return Ui(GraphicsKeys[static_cast<int32>(S.Graphics)]);
	case EDTSettingRow::Window: return Ui(WindowKeys[static_cast<int32>(S.Window)]);
	case EDTSettingRow::TextSize: return Percent(S.TextPct);
	case EDTSettingRow::Motion: return Ui(MotionKeys[static_cast<int32>(S.GetMotion())]);
	case EDTSettingRow::Shake: return OnOff(S.bShake);
	case EDTSettingRow::Flash: return OnOff(S.bFlash);
	case EDTSettingRow::DangerOutline: return OnOff(S.bDangerOutline);
	default: return Ui(TEXT("Settings.Controls.Open"));
	}
}

FText DTMenuText::StatusLine(const FDTMenuState& State)
{
	using DTMenuScreenLocal::Ui;
	const FText Action = Ui(DTBindings::ActionKey(State.StatusSlot));
	switch (State.Status)
	{
	case EDTMenuStatus::Capturing: return FText::Format(Ui(TEXT("Controls.Capturing")), Action);
	case EDTMenuStatus::Cancelled: return Ui(TEXT("Controls.Cancelled"));
	case EDTMenuStatus::Bound: return FText::Format(Ui(TEXT("Controls.Bound")), Action);
	case EDTMenuStatus::Swapped: return FText::Format(Ui(TEXT("Controls.Swapped")), Action, Ui(DTBindings::ActionKey(State.StatusOther)));
	case EDTMenuStatus::Reserved: return Ui(TEXT("Controls.Reserved"));
	case EDTMenuStatus::WrongDevicePad: return Ui(TEXT("Controls.WrongDevice.Pad"));
	case EDTMenuStatus::WrongDeviceKbm: return Ui(TEXT("Controls.WrongDevice.KBM"));
	case EDTMenuStatus::ResetDone: return Ui(TEXT("Controls.ResetDone"));
	default: return FText::GetEmpty();
	}
}

void DTMenuScreen::Draw(UCanvas* Canvas, const FDTMenuState& State, const FDTUserSettings& Settings)
{
	using namespace DTMenuScreenLocal;
	const int32 Rows = State.NumRows();
	if (!Canvas || !GEngine || Rows == 0)
	{
		return;
	}
	const FPainter P{ Canvas, GEngine->GetLargeFont(), Canvas->ClipY / RefHeight, static_cast<float>(Settings.TextPct) / DTConst::SETTINGS_TEXT_MIN_PCT };
	const FLinearColor Text = Token(DTPal::UI_Text);
	const FLinearColor Hint = Token(DTPal::UI_Text, HintAlpha);

	P.Tile(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, Token(DTPal::UI_Panel, DimAlpha));
	const float W = FMath::Min(PanelW * P.S, Canvas->ClipX - 2.0f * Pad * P.S);
	const float H = DTMenuLayout::ContentHeight(Rows, P.T) * P.S;
	const float X = 0.5f * (Canvas->ClipX - W);
	const float Y0 = 0.5f * (Canvas->ClipY - H);
	P.Tile(X, Y0, W, H, Token(DTPal::UI_Panel, PanelAlpha));
	P.Frame(X, Y0, W, H, Token(DTPal::UI_Text, HintAlpha));

	const TCHAR* TitleKey = State.Screen == EDTMenuScreen::Pause ? TEXT("Pause.Title") : (State.Screen == EDTMenuScreen::Settings ? TEXT("Settings.Title") : TEXT("Controls.Title"));
	const float RowH = DTMenuLayout::RowHeight(P.T) * P.S;
	const float Inner = W - 2.0f * Pad * P.S;
	const float Left = X + Pad * P.S;
	const float Right = X + W - Pad * P.S;
	const float KbmX = Left + LabelColumn * Inner;
	const float PadX = Left + PadColumn * Inner;
	P.Centered(Ui(TitleKey), X + 0.5f * W, Y0 + Pad * 0.5f * P.S, TitleScale, Text);

	float Y = Y0 + DTMenuLayout::TitleHeight(P.T) * P.S;
	if (State.Screen == EDTMenuScreen::Controls)
	{
		// Column headers share the title's band, under the title.
		const float HeaderY = Y - P.Measure(Ui(TEXT("Controls.Header.Action")), HeaderScale).Y - FocusInset * P.S;
		P.Text(Ui(TEXT("Controls.Header.Action")), Left, HeaderY, HeaderScale, Hint);
		P.Text(Ui(TEXT("Controls.Header.KBM")), KbmX, HeaderY, HeaderScale, Hint);
		P.Text(Ui(TEXT("Controls.Header.Pad")), PadX, HeaderY, HeaderScale, Hint);
	}
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		const float CentreY = Y + 0.5f * RowH;
		const float TextH = P.Measure(Ui(TEXT("Pause.Title")), BodyScale).Y;
		const float TextY = CentreY - 0.5f * TextH;
		const bool bFocus = Row == State.Focus;
		if (State.Screen == EDTMenuScreen::Pause)
		{
			if (bFocus)
			{
				P.Frame(Left, Y + FocusInset * P.S, Inner, RowH - 2.0f * FocusInset * P.S, Text);
			}
			P.Centered(Ui(Row == static_cast<int32>(EDTPauseRow::Resume) ? TEXT("Pause.Resume") : TEXT("Pause.Settings")), X + 0.5f * W, TextY, BodyScale, Text);
		}
		else if (State.Screen == EDTMenuScreen::Settings)
		{
			const EDTSettingRow SettingRow = static_cast<EDTSettingRow>(Row);
			if (bFocus)
			{
				P.Frame(Left - FocusInset * P.S, Y + FocusInset * P.S, Inner + 2.0f * FocusInset * P.S, RowH - 2.0f * FocusInset * P.S, Text);
			}
			P.Text(DTMenuText::RowLabel(SettingRow), Left + FocusInset * 2.0f * P.S, TextY, BodyScale, Text);
			FText Value = DTMenuText::RowValue(SettingRow, Settings);
			if (SettingRow != EDTSettingRow::Controls)
			{
				Value = FText::Format(Ui(TEXT("Settings.ValueArrows")), Value);
			}
			P.Text(Value, Right - FocusInset * 2.0f * P.S - static_cast<float>(P.Measure(Value, BodyScale).X), TextY, BodyScale, Text);
		}
		else if (Row == FDTMenuState::ResetRow())
		{
			if (bFocus)
			{
				P.Frame(Left, Y + FocusInset * P.S, Inner, RowH - 2.0f * FocusInset * P.S, Text);
			}
			P.Centered(Ui(TEXT("Controls.Reset")), X + 0.5f * W, TextY, BodyScale, Text);
		}
		else
		{
			const EDTBindSlot Slot = static_cast<EDTBindSlot>(Row);
			P.Text(Ui(DTBindings::ActionKey(Slot)), Left + FocusInset * 2.0f * P.S, TextY, BodyScale, Text);
			for (const EDTBindDevice Device : { EDTBindDevice::Kbm, EDTBindDevice::Pad })
			{
				const float CellX = Device == EDTBindDevice::Kbm ? KbmX : PadX;
				const float CellW = (Device == EDTBindDevice::Kbm ? PadX : Right) - CellX - FocusInset * P.S;
				if (bFocus && Device == State.Column)
				{
					P.Frame(CellX - FocusInset * P.S, Y + FocusInset * P.S, CellW, RowH - 2.0f * FocusInset * P.S, Text);
				}
				const bool bFixed = !DTBindings::IsRebindable(Slot, Device);
				FText Cell = bFixed ? Ui(TEXT("Controls.Fixed")) : DTBindings::Glyph(Settings.Bindings.Get(Slot, Device));
				if (bFocus && State.bCapturing && Device == State.Column)
				{
					Cell = Ui(Device == EDTBindDevice::Pad ? TEXT("Controls.PressPad") : TEXT("Controls.PressKey"));
				}
				P.Text(Cell, CellX + FocusInset * P.S, TextY, BodyScale, bFixed ? Token(DTPal::UI_Text, FixedAlpha) : Text);
			}
		}
		Y += RowH;
	}

	// Footer: the row's hint or the status line, then the device line.
	FText Line = FText::GetEmpty();
	if (State.Screen == EDTMenuScreen::Settings && State.Focus < static_cast<int32>(EDTSettingRow::Num))
	{
		Line = DTMenuText::RowHint(static_cast<EDTSettingRow>(State.Focus));
	}
	else if (State.Screen == EDTMenuScreen::Controls)
	{
		Line = DTMenuText::StatusLine(State);
	}
	Y += 10.0f * P.S;
	if (!Line.IsEmpty())
	{
		P.Centered(Line, X + 0.5f * W, Y, HintScale, Text);
	}
	Y += P.Measure(Ui(TEXT("Pause.Title")), HintScale).Y + 8.0f * P.S;
	const TCHAR* Footer = State.Screen == EDTMenuScreen::Pause ? TEXT("Menu.Hint.Pause") : (State.Screen == EDTMenuScreen::Settings ? TEXT("Menu.Hint.Settings") : TEXT("Menu.Hint.Controls"));
	P.Centered(Ui(Footer), X + 0.5f * W, Y, HintScale, Hint);
}
