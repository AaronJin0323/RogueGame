#include "Offers/DTOfferScreen.h"
#include "CanvasItem.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Offers/DTOfferGenerator.h"
#include "Offers/DTOfferStyle.h"

namespace DTOfferScreenLocal
{
	// Layout in reference pixels at a 1080-line canvas; everything scales with the canvas height.
	constexpr float RefHeight = 1080.0f;
	constexpr float CardW = 300.0f;
	constexpr float CardH = 470.0f;
	constexpr float CardGap = 26.0f;
	constexpr float CardPad = 16.0f;
	constexpr float SideMargin = 32.0f;
	constexpr float TitleTop = 0.12f;  // of the canvas height
	constexpr float CardsTop = 0.21f;
	constexpr float FocusRaise = 14.0f;
	constexpr float IconSize = 52.0f;
	constexpr float ToolW = 380.0f;
	constexpr float ToolH = 60.0f;
	constexpr float ToolGap = 24.0f;
	constexpr float RowGap = 36.0f;
	constexpr float PipSize = 12.0f;
	constexpr float Thin = 2.0f;
	constexpr float Heavy = 6.0f;
	constexpr float TitleScale = 2.2f;
	constexpr float NameScale = 1.45f;
	constexpr float LabelScale = 1.05f;
	constexpr float BodyScale = 1.1f;
	constexpr float DimAlpha = 0.68f;
	constexpr float PanelAlpha = 0.94f;
	constexpr float DisabledAlpha = 0.35f;
	constexpr int32 CircleSegments = 16;
	constexpr float DashLen = 10.0f;

	const TCHAR* const ToolKeys[] = { TEXT("Offer.Tool.Reroll"), TEXT("Offer.Tool.Banish"), TEXT("Offer.Tool.Lock") };
	// Key glyph names (device names, the same in every culture).
	const TCHAR* const ToolGlyphs[] = { TEXT("R / X"), TEXT("B / Y"), TEXT("L / RB") };
	const EDTOfferAction ToolActions[] = { EDTOfferAction::Reroll, EDTOfferAction::Banish, EDTOfferAction::Lock };

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

		void Tile(float X, float Y, float W, float H, const FLinearColor& Color) const
		{
			FCanvasTileItem Item(FVector2D(X, Y), FVector2D(W, H), Color);
			Item.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Item);
		}
		void Line(const FVector2D& A, const FVector2D& B, const FLinearColor& Color, float Thickness) const
		{
			FCanvasLineItem Item(A, B);
			Item.SetColor(Color);
			Item.LineThickness = Thickness * S;
			Canvas->DrawItem(Item);
		}
		void Rect(const FBox2D& R, const FLinearColor& Color, float Thickness) const
		{
			const float T = Thickness * S;
			Tile(R.Min.X, R.Min.Y, R.Max.X - R.Min.X, T, Color);
			Tile(R.Min.X, R.Max.Y - T, R.Max.X - R.Min.X, T, Color);
			Tile(R.Min.X, R.Min.Y, T, R.Max.Y - R.Min.Y, Color);
			Tile(R.Max.X - T, R.Min.Y, T, R.Max.Y - R.Min.Y, Color);
		}
		void Poly(const TArray<FVector2D>& Points, const FLinearColor& Color, float Thickness, bool bClosed = true) const
		{
			for (int32 Index = 0; Index + 1 < Points.Num(); ++Index)
			{
				Line(Points[Index], Points[Index + 1], Color, Thickness);
			}
			if (bClosed && Points.Num() > 2)
			{
				Line(Points.Last(), Points[0], Color, Thickness);
			}
		}
		FVector2D Measure(const FString& Text, float Scale) const
		{
			float W = 0.0f;
			float H = 0.0f;
			Canvas->TextSize(Font, Text, W, H, Scale * S, Scale * S);
			return FVector2D(W, H);
		}
		void Text(const FText& Line, float X, float Y, float Scale, const FLinearColor& Color) const
		{
			FCanvasTextItem Item(FVector2D(X, Y), Line, Font, Color);
			Item.Scale = FVector2D(Scale * S, Scale * S);
			Item.EnableShadow(FLinearColor::Black);
			Canvas->DrawItem(Item);
		}
		void Centered(const FText& Line, float CenterX, float Y, float Scale, const FLinearColor& Color) const
		{
			Text(Line, CenterX - 0.5f * Measure(Line.ToString(), Scale).X, Y, Scale, Color);
		}
		/** Greedy wrap: breaks after spaces, between CJK characters, or mid-word when a word alone is too wide. */
		TArray<FString> Wrap(const FString& Text, float Scale, float MaxWidth) const
		{
			TArray<FString> Lines;
			TArray<FString> Paragraphs;
			Text.ParseIntoArray(Paragraphs, TEXT("\n"), false);
			for (const FString& Paragraph : Paragraphs)
			{
				FString Current;
				int32 LastBreak = INDEX_NONE;
				for (int32 Index = 0; Index < Paragraph.Len(); ++Index)
				{
					const TCHAR Ch = Paragraph[Index];
					const FString Candidate = Current + Ch;
					if (!Current.IsEmpty() && Measure(Candidate, Scale).X > MaxWidth)
					{
						if (LastBreak != INDEX_NONE && LastBreak < Current.Len())
						{
							Lines.Add(Current.Left(LastBreak).TrimEnd());
							Current = Current.Mid(LastBreak).TrimStart() + Ch;
						}
						else
						{
							Lines.Add(Current.TrimEnd());
							Current = FString(1, &Ch).TrimStart();
						}
						LastBreak = INDEX_NONE;
					}
					else
					{
						Current = Candidate;
					}
					// Break opportunities: after a space, or after any wide (CJK) character.
					if (Ch == TEXT(' ') || Ch >= 0x2E80)
					{
						LastBreak = Current.Len();
					}
				}
				if (!Current.IsEmpty())
				{
					Lines.Add(Current);
				}
			}
			return Lines;
		}
	};

	void DrawIcon(const FPainter& P, EDTCardIcon Icon, const FVector2D& C, const FLinearColor& Color)
	{
		const float R = 0.5f * IconSize * P.S;
		TArray<FVector2D> Pts;
		switch (Icon)
		{
		case EDTCardIcon::Crosshair:
			for (int32 Seg = 0; Seg < CircleSegments; ++Seg)
			{
				const float A = 2.0f * UE_PI * Seg / CircleSegments;
				Pts.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * (0.7f * R));
			}
			P.Poly(Pts, Color, Thin);
			P.Line(C - FVector2D(R, 0), C + FVector2D(R, 0), Color, Thin);
			P.Line(C - FVector2D(0, R), C + FVector2D(0, R), Color, Thin);
			break;
		case EDTCardIcon::UpArrow:
			P.Poly({ C + FVector2D(-R, 0), C + FVector2D(0, -R), C + FVector2D(R, 0) }, Color, Thin * 1.5f, false);
			P.Poly({ C + FVector2D(-R, 0.6f * R), C + FVector2D(0, -0.4f * R), C + FVector2D(R, 0.6f * R) }, Color, Thin * 1.5f, false);
			break;
		case EDTCardIcon::Hex:
			for (int32 Side = 0; Side < 6; ++Side)
			{
				const float A = UE_PI / 6.0f + UE_PI / 3.0f * Side;
				Pts.Add(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * R);
			}
			P.Poly(Pts, Color, Thin);
			P.Tile(C.X - 0.25f * R, C.Y - 0.25f * R, 0.5f * R, 0.5f * R, Color);
			break;
		case EDTCardIcon::Diamond:
			P.Poly({ C + FVector2D(0, -R), C + FVector2D(0.7f * R, 0), C + FVector2D(0, R), C + FVector2D(-0.7f * R, 0) }, Color, Thin);
			P.Poly({ C + FVector2D(0, -0.45f * R), C + FVector2D(0.3f * R, 0), C + FVector2D(0, 0.45f * R), C + FVector2D(-0.3f * R, 0) }, Color, Thin);
			break;
		case EDTCardIcon::Crown:
			P.Poly({ C + FVector2D(-R, 0.6f * R), C + FVector2D(-R, -0.6f * R), C + FVector2D(-0.5f * R, 0), C + FVector2D(0, -R),
				C + FVector2D(0.5f * R, 0), C + FVector2D(R, -0.6f * R), C + FVector2D(R, 0.6f * R) }, Color, Thin * 1.5f);
			break;
		case EDTCardIcon::Anvil:
			P.Poly({ C + FVector2D(-R, -0.5f * R), C + FVector2D(R, -0.5f * R), C + FVector2D(0.4f * R, 0.1f * R), C + FVector2D(0.6f * R, 0.7f * R),
				C + FVector2D(-0.6f * R, 0.7f * R), C + FVector2D(-0.4f * R, 0.1f * R) }, Color, Thin);
			break;
		case EDTCardIcon::Cross:
			P.Rect(FBox2D(C - FVector2D(R, R) * 0.8f, C + FVector2D(R, R) * 0.8f), Color, Thin * 0.5f);
			P.Line(C - FVector2D(0.5f * R, 0), C + FVector2D(0.5f * R, 0), Color, Thin * 1.5f);
			P.Line(C - FVector2D(0, 0.5f * R), C + FVector2D(0, 0.5f * R), Color, Thin * 1.5f);
			break;
		}
	}

	void DrawBorder(const FPainter& P, EDTCardBorder Border, const FBox2D& R, const FLinearColor& Color)
	{
		const float S = P.S;
		const FVector2D Size = R.Max - R.Min;
		switch (Border)
		{
		case EDTCardBorder::TechDouble:
		{
			P.Rect(R, Color, Thin);
			const FBox2D Inner(R.Min + FVector2D(6, 6) * S, R.Max - FVector2D(6, 6) * S);
			P.Rect(Inner, Color, Thin * 0.5f);
			const float Cut = 18.0f * S;
			P.Line(R.Min + FVector2D(0, Cut), R.Min + FVector2D(Cut, 0), Color, Thin);
			P.Line(FVector2D(R.Max.X - Cut, R.Min.Y), FVector2D(R.Max.X, R.Min.Y + Cut), Color, Thin);
			P.Line(FVector2D(R.Min.X, R.Max.Y - Cut), FVector2D(R.Min.X + Cut, R.Max.Y), Color, Thin);
			P.Line(R.Max - FVector2D(0, Cut), R.Max - FVector2D(Cut, 0), Color, Thin);
			break;
		}
		case EDTCardBorder::TechChevron:
		{
			P.Rect(R, Color, Thin);
			const float W = 14.0f * S;
			for (int32 Index = -1; Index <= 1; ++Index)
			{
				const float X = R.Min.X + 0.5f * Size.X + Index * 2.0f * W;
				P.Poly({ FVector2D(X - W, R.Min.Y + 10 * S), FVector2D(X, R.Min.Y - 4 * S), FVector2D(X + W, R.Min.Y + 10 * S) }, Color, Thin, false);
				P.Poly({ FVector2D(X - W, R.Max.Y + 4 * S), FVector2D(X, R.Max.Y - 10 * S), FVector2D(X + W, R.Max.Y + 4 * S) }, Color, Thin, false);
			}
			break;
		}
		case EDTCardBorder::TechSocket:
		{
			P.Rect(R, Color, Thin);
			const float N = 10.0f * S;
			for (int32 Index = 1; Index <= 3; ++Index)
			{
				const float Y = R.Min.Y + Size.Y * Index / 4.0f - 0.5f * N;
				P.Tile(R.Min.X - 0.5f * N, Y, N, N, Color);
				P.Tile(R.Max.X - 0.5f * N, Y, N, N, Color);
			}
			break;
		}
		case EDTCardBorder::StoneHeavy:
			P.Rect(R, Color, Heavy);
			break;
		case EDTCardBorder::StoneCrown:
			P.Rect(R, Color, Heavy);
			P.Tile(R.Min.X, R.Min.Y, Size.X, 16.0f * S, Color);
			break;
		case EDTCardBorder::StoneInner:
			P.Rect(R, Color, Heavy);
			P.Rect(FBox2D(R.Min + FVector2D(12, 12) * S, R.Max - FVector2D(12, 12) * S), Color, Thin);
			break;
		case EDTCardBorder::PlainDashed:
		{
			const float Dash = DashLen * S;
			for (float X = R.Min.X; X < R.Max.X; X += 2.0f * Dash)
			{
				const float E = FMath::Min(X + Dash, R.Max.X);
				P.Line(FVector2D(X, R.Min.Y), FVector2D(E, R.Min.Y), Color, Thin);
				P.Line(FVector2D(X, R.Max.Y), FVector2D(E, R.Max.Y), Color, Thin);
			}
			for (float Y = R.Min.Y; Y < R.Max.Y; Y += 2.0f * Dash)
			{
				const float E = FMath::Min(Y + Dash, R.Max.Y);
				P.Line(FVector2D(R.Min.X, Y), FVector2D(R.Min.X, E), Color, Thin);
				P.Line(FVector2D(R.Max.X, Y), FVector2D(R.Max.X, E), Color, Thin);
			}
			break;
		}
		}
	}
}

// ---- input --------------------------------------------------------------------------------------------------------------

bool FDTOfferInputGuard::Allows(double Now) const
{
	return Now - OpenedAt >= static_cast<double>(DTConst::OFFER_INPUT_GUARD_S);
}

double FDTOfferInputGuard::Remaining(double Now) const
{
	return FMath::Max(0.0, static_cast<double>(DTConst::OFFER_INPUT_GUARD_S) - (Now - OpenedAt));
}

const TArray<FKey>& DTOfferInput::Keys()
{
	static const TArray<FKey> All = {
		EKeys::Left, EKeys::A, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_LeftStick_Left,
		EKeys::Right, EKeys::D, EKeys::Gamepad_DPad_Right, EKeys::Gamepad_LeftStick_Right,
		EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom,
		EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five,
		EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive,
		EKeys::R, EKeys::Gamepad_FaceButton_Left,
		EKeys::B, EKeys::Gamepad_FaceButton_Top,
		EKeys::L, EKeys::Gamepad_RightShoulder };
	return All;
}

FDTOfferCommand DTOfferInput::ForKey(const FKey& Key)
{
	FDTOfferCommand Command;
	if (Key == EKeys::Left || Key == EKeys::A || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::Gamepad_LeftStick_Left)
	{
		Command.Action = EDTOfferAction::Prev;
	}
	else if (Key == EKeys::Right || Key == EKeys::D || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::Gamepad_LeftStick_Right)
	{
		Command.Action = EDTOfferAction::Next;
	}
	else if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
	{
		Command.Action = EDTOfferAction::Confirm;
	}
	else if (Key == EKeys::R || Key == EKeys::Gamepad_FaceButton_Left)
	{
		Command.Action = EDTOfferAction::Reroll;
	}
	else if (Key == EKeys::B || Key == EKeys::Gamepad_FaceButton_Top)
	{
		Command.Action = EDTOfferAction::Banish;
	}
	else if (Key == EKeys::L || Key == EKeys::Gamepad_RightShoulder)
	{
		Command.Action = EDTOfferAction::Lock;
	}
	else
	{
		const FKey Numbers[] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
		const FKey Pad[] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive };
		for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(Numbers); ++Slot)
		{
			if (Key == Numbers[Slot] || Key == Pad[Slot])
			{
				Command.Action = EDTOfferAction::PickSlot;
				Command.Slot = Slot;
			}
		}
	}
	return Command;
}

TArray<FKey> DTOfferInput::KeysFor(EDTOfferAction Action)
{
	TArray<FKey> Out;
	for (const FKey& Key : Keys())
	{
		if (ForKey(Key).Action == Action)
		{
			Out.Add(Key);
		}
	}
	return Out;
}

// ---- drawing ------------------------------------------------------------------------------------------------------------

int32 UDTOfferScreen::CardAt(const FVector2D& Position) const
{
	for (int32 Slot = 0; Slot < CardRects.Num(); ++Slot)
	{
		if (CardRects[Slot].IsInside(Position))
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

EDTOfferAction UDTOfferScreen::ToolAt(const FVector2D& Position) const
{
	if (!bToolsShown)
	{
		return EDTOfferAction::None;
	}
	for (int32 Tool = 0; Tool < UE_ARRAY_COUNT(ToolRects); ++Tool)
	{
		if (ToolRects[Tool].IsInside(Position))
		{
			return DTOfferScreenLocal::ToolActions[Tool];
		}
	}
	return EDTOfferAction::None;
}

void UDTOfferScreen::DrawCard(UCanvas* Canvas, const FDTOfferCard& Card, const FDTPlayerRunState& Player, const FBox2D& Rect, int32 Slot, bool bFocus)
{
	using namespace DTOfferScreenLocal;
	FPainter P{ Canvas, GEngine->GetLargeFont(), Canvas->ClipY / RefHeight };
	const FDTCardStyle Style = DTOfferStyle::ForKind(Card.Kind);
	const FLinearColor Family = Token(DTOfferStyle::FamilyToken(Style.Family));
	const FLinearColor Text = Token(DTPal::UI_Text);
	const FVector2D Size = Rect.Max - Rect.Min;
	const float Pad = CardPad * P.S;
	const float Inner = Size.X - 2.0f * Pad;

	P.Tile(Rect.Min.X, Rect.Min.Y, Size.X, Size.Y, Token(DTPal::UI_Panel, PanelAlpha));
	if (bFocus)
	{
		P.Rect(FBox2D(Rect.Min - FVector2D(8, 8) * P.S, Rect.Max + FVector2D(8, 8) * P.S), Text, Thin);
	}
	DrawBorder(P, Style.Border, Rect, Family);

	float Y = Rect.Min.Y + Pad + (Style.Border == EDTCardBorder::StoneCrown ? 12.0f * P.S : 0.0f);
	// Slot number (number-key hint) and the kind label (label channel) on the first row.
	P.Text(FText::AsNumber(Slot + 1), Rect.Min.X + Pad, Y, LabelScale, Text);
	const FText Label = DTOfferText::Ui(Style.LabelKey);
	P.Text(Label, Rect.Max.X - Pad - P.Measure(Label.ToString(), LabelScale).X, Y, LabelScale, Family);
	Y += P.Measure(TEXT("0"), LabelScale).Y + 10.0f * P.S;

	DrawIcon(P, Style.Icon, FVector2D(Rect.Min.X + 0.5f * Size.X, Y + 0.5f * IconSize * P.S), Family);
	Y += IconSize * P.S + 12.0f * P.S;

	for (const FString& Line : P.Wrap(DTOfferText::Name(Card).ToString(), NameScale, Inner))
	{
		P.Centered(FText::FromString(Line), Rect.Min.X + 0.5f * Size.X, Y, NameScale, Text);
		Y += P.Measure(Line, NameScale).Y;
	}
	const FText Sub = DTOfferText::Line(Card, Player);
	if (!Sub.IsEmpty())
	{
		Y += 4.0f * P.S;
		for (const FString& Line : P.Wrap(Sub.ToString(), BodyScale, Inner))
		{
			P.Centered(FText::FromString(Line), Rect.Min.X + 0.5f * Size.X, Y, BodyScale, Family);
			Y += P.Measure(Line, BodyScale).Y;
		}
	}
	Y += 10.0f * P.S;
	P.Tile(Rect.Min.X + Pad, Y, Inner, 1.0f * P.S, Token(DTPal::UI_Text, DisabledAlpha));
	Y += 10.0f * P.S;
	for (const FString& Line : P.Wrap(DTOfferText::Desc(Card).ToString(), BodyScale, Inner))
	{
		P.Text(FText::FromString(Line), Rect.Min.X + Pad, Y, BodyScale, Text);
		Y += P.Measure(Line, BodyScale).Y;
	}

	// Rarity marker (own channel): 1-3 pips in the rarity token plus its label, bottom left.
	const float Pip = PipSize * P.S;
	const float BottomY = Rect.Max.Y - Pad - Pip;
	const FLinearColor Rarity = Token(DTOfferStyle::RarityToken(Card.Rarity));
	const int32 Pips = DTOfferStyle::RarityPips(Card.Rarity);
	for (int32 Index = 0; Index < Pips; ++Index)
	{
		P.Tile(Rect.Min.X + Pad + Index * (Pip + 4.0f * P.S), BottomY, Pip, Pip, Rarity);
	}
	const FText RarityLabel = DTOfferText::Ui(DTOfferStyle::RarityLabelKey(Card.Rarity));
	const float LabelH = P.Measure(RarityLabel.ToString(), LabelScale).Y;
	P.Text(RarityLabel, Rect.Min.X + Pad + 3.0f * (Pip + 4.0f * P.S) + 4.0f * P.S, BottomY + 0.5f * Pip - 0.5f * LabelH, LabelScale, Rarity);

	// Markers: guaranteed slot, held (locked) card.
	const TCHAR* Marker = Card.bLocked ? TEXT("Offer.Marker.Held") : (Card.bGuaranteed ? TEXT("Offer.Marker.Guaranteed") : nullptr);
	if (Marker)
	{
		const FText MarkerText = DTOfferText::Ui(Marker);
		P.Text(MarkerText, Rect.Max.X - Pad - P.Measure(MarkerText.ToString(), LabelScale).X, BottomY + 0.5f * Pip - 0.5f * LabelH, LabelScale, Text);
	}
}

void UDTOfferScreen::Draw(UCanvas* Canvas, const FDTOfferScreenModel& Model)
{
	using namespace DTOfferScreenLocal;
	CardRects.Reset();
	bToolsShown = false;
	if (!Canvas || !GEngine || !Model.Hand || !Model.Player || Model.Hand->Cards.Num() == 0)
	{
		return;
	}
	FPainter P{ Canvas, GEngine->GetLargeFont(), Canvas->ClipY / RefHeight };
	const FDTOfferHand& Hand = *Model.Hand;
	const FDTPlayerOfferState& Tools = Model.Player->Offer;
	const FLinearColor Text = Token(DTPal::UI_Text);

	P.Tile(0.0f, 0.0f, Canvas->ClipX, Canvas->ClipY, Token(DTPal::UI_Panel, DimAlpha));

	FFormatNamedArguments TitleArgs;
	TitleArgs.Add(TEXT("Level"), FText::AsNumber(Hand.Level));
	P.Centered(FText::Format(DTOfferText::Ui(DTOfferStyle::HandTitleKey(Hand.HandKind)), TitleArgs), 0.5f * Canvas->ClipX,
		Canvas->ClipY * TitleTop, TitleScale, Text);

	const int32 N = Hand.Cards.Num();
	float W = CardW * P.S;
	float Gap = CardGap * P.S;
	const float Avail = Canvas->ClipX - 2.0f * SideMargin * P.S;
	if (N * W + (N - 1) * Gap > Avail)
	{
		const float Shrink = Avail / (N * W + (N - 1) * Gap);
		W *= Shrink;
		Gap *= Shrink;
	}
	const float H = CardH * P.S;
	const float X0 = 0.5f * (Canvas->ClipX - (N * W + (N - 1) * Gap));
	const float Y0 = Canvas->ClipY * CardsTop;
	for (int32 Slot = 0; Slot < N; ++Slot)
	{
		const bool bFocus = Slot == Model.Focus;
		const float Raise = bFocus ? FocusRaise * P.S : 0.0f;
		const FBox2D Rect(FVector2D(X0 + Slot * (W + Gap), Y0 - Raise), FVector2D(X0 + Slot * (W + Gap) + W, Y0 - Raise + H));
		CardRects.Add(Rect);
		DrawCard(Canvas, Hand.Cards[Slot], *Model.Player, Rect, Slot, bFocus);
	}

	float Y = Y0 + H + RowGap * P.S;
	if (DTOffers::HandAllowsTools(Hand.HandKind))
	{
		bToolsShown = true;
		const int32 Counts[] = { Tools.RerollsLeft, Tools.BanishLeft, Tools.LockLeft };
		const FDTOfferCard* Focused = Hand.Cards.IsValidIndex(Model.Focus) ? &Hand.Cards[Model.Focus] : nullptr;
		const bool bFocusTakesTool = Focused && DTOffers::CardAllowsBanishLock(*Focused) && !Focused->bLocked;
		const float TW = ToolW * P.S;
		const float TH = ToolH * P.S;
		const float TX0 = 0.5f * (Canvas->ClipX - (3.0f * TW + 2.0f * ToolGap * P.S));
		for (int32 Tool = 0; Tool < 3; ++Tool)
		{
			const bool bEnabled = Counts[Tool] > 0 && (Tool == 0 || bFocusTakesTool);
			const FBox2D Rect(FVector2D(TX0 + Tool * (TW + ToolGap * P.S), Y), FVector2D(TX0 + Tool * (TW + ToolGap * P.S) + TW, Y + TH));
			ToolRects[Tool] = Rect;
			const float Alpha = bEnabled ? 1.0f : DisabledAlpha;
			P.Tile(Rect.Min.X, Rect.Min.Y, TW, TH, Token(DTPal::UI_Panel, PanelAlpha));
			P.Rect(Rect, Token(DTPal::UI_Text, Alpha), Thin);
			FFormatNamedArguments Args;
			Args.Add(TEXT("Tool"), DTOfferText::Ui(ToolKeys[Tool]));
			Args.Add(TEXT("Keys"), FText::AsCultureInvariant(ToolGlyphs[Tool]));
			Args.Add(TEXT("Count"), FText::AsNumber(Counts[Tool]));
			const FText Label = FText::Format(DTOfferText::Ui(TEXT("Offer.Tool.Button")), Args);
			const FVector2D LabelSize = P.Measure(Label.ToString(), BodyScale);
			P.Text(Label, Rect.Min.X + 0.5f * (TW - LabelSize.X), Rect.Min.Y + 0.5f * (TH - LabelSize.Y), BodyScale, Token(DTPal::UI_Text, Alpha));
		}
		Y += TH + 14.0f * P.S;
		P.Centered(DTOfferText::Ui(TEXT("Offer.Hint.Focus")), 0.5f * Canvas->ClipX, Y, BodyScale, Token(DTPal::UI_Text, 0.7f));
		Y += P.Measure(TEXT("0"), BodyScale).Y + 6.0f * P.S;
	}
	const FText Hint = DTOfferText::Ui(Model.bGuardActive ? TEXT("Offer.Hint.Guard") : TEXT("Offer.Hint.Pick"));
	P.Centered(Hint, 0.5f * Canvas->ClipX, Y, BodyScale, Text);
}
