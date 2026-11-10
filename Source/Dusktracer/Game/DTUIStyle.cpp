#include "Game/DTUIStyle.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Core/DTPaletteGen.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace DTUIStyleLocal
{
	// Panel states (hud.md Numbers): fill opacity, border opacity, dashed.
	constexpr FDTPanelLook PanelLooks[static_cast<int32>(EDTPanel::Num)] = {
		{ 0.55f, 0.1f, false }, { 0.8f, 1.0f, false }, { 0.55f, 0.5f, true }, { 1.0f, 0.0f, false } };
	constexpr float DashRef = 8.0f;        // length of a dash and of the gap after it
	constexpr float RefillAlpha = 0.6f;    // a dash pip coming back, against a ready one
	constexpr int32 CircleSegments = 64;   // of a full ring
}

FDTPanelLook DTUI::PanelLook(EDTPanel State)
{
	return DTUIStyleLocal::PanelLooks[FMath::Clamp(static_cast<int32>(State), 0, static_cast<int32>(EDTPanel::Num) - 1)];
}

FLinearColor DTUI::Token(const FColor& Color, float Alpha)
{
	FLinearColor Linear = DTPal::Linear(Color);
	Linear.A = Alpha;
	return Linear;
}

FColor DTHudStyle::DutyToken(EDTHudDuty Duty)
{
	switch (Duty)
	{
	case EDTHudDuty::Track: return DTPal::UI_Panel;
	case EDTHudDuty::Xp: return DTPal::Pickup_Gem;
	case EDTHudDuty::XpFlash: return DTPal::Player_Core;
	case EDTHudDuty::Boss: return DTPal::UI_Boss;
	case EDTHudDuty::RingIdle: return DTPal::Player_TrailNeutral;
	case EDTHudDuty::RingReady: return DTPal::Player_Core;
	// Bone carries text, the player's own HP and the dash (R-HUD-10).
	default: return DTPal::UI_Text;
	}
}

float DTHudStyle::DutyAlpha(EDTHudDuty Duty)
{
	return Duty == EDTHudDuty::DashRefill ? DTUIStyleLocal::RefillAlpha : 1.0f;
}

FLinearColor DTHudStyle::Duty(EDTHudDuty Duty)
{
	return DTUI::Token(DutyToken(Duty), DutyAlpha(Duty));
}

FVector2f FDTUIPainter::Measure(const FString& Line, EDTType Type, float Mul) const
{
	float W = 0.0f;
	float H = 0.0f;
	if (Canvas && Font)
	{
		const float Scale = TextScale(Type, Mul);
		Canvas->TextSize(Font, Line, W, H, Scale, Scale);
	}
	return FVector2f(W, H);
}

void FDTUIPainter::Text(const FText& Line, const FVector2f& At, EDTType Type, const FLinearColor& Color, float Mul) const
{
	FCanvasTextItem Item(FVector2D(At), Line, Font, Color);
	const float Scale = TextScale(Type, Mul);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor::Black);
	Canvas->DrawItem(Item);
}

void FDTUIPainter::Tile(const FBox2f& Box, const FLinearColor& Color) const
{
	const FVector2f Size = Box.GetSize();
	if (Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		return;
	}
	FCanvasTileItem Item(FVector2D(Box.Min), FVector2D(Size), Color);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void FDTUIPainter::Frame(const FBox2f& Box, const FLinearColor& Color, float ThicknessRef, bool bDashed) const
{
	const float Line = FMath::Max(1.0f, FMath::RoundToFloat(ThicknessRef * S));
	const FVector2f Size = Box.GetSize();
	const auto Run = [&](const FVector2f& From, bool bHorizontal, float Length)
	{
		const float Step = bDashed ? FMath::Max(2.0f, DTUIStyleLocal::DashRef * S) : Length;
		for (float At = 0.0f; At < Length; At += bDashed ? 2.0f * Step : Length)
		{
			const float Len = FMath::Min(Step, Length - At);
			const FVector2f Min = From + (bHorizontal ? FVector2f(At, 0.0f) : FVector2f(0.0f, At));
			Tile(FBox2f(Min, Min + (bHorizontal ? FVector2f(Len, Line) : FVector2f(Line, Len))), Color);
		}
	};
	Run(Box.Min, true, Size.X);
	Run(FVector2f(Box.Min.X, Box.Max.Y - Line), true, Size.X);
	Run(Box.Min, false, Size.Y);
	Run(FVector2f(Box.Max.X - Line, Box.Min.Y), false, Size.Y);
}

void FDTUIPainter::Panel(const FBox2f& Box, EDTPanel State) const
{
	const FDTPanelLook Look = DTUI::PanelLook(State);
	Tile(Box, DTUI::Token(DTPal::UI_Panel, Look.FillAlpha));
	if (Look.BorderAlpha > 0.0f)
	{
		Frame(Box, DTUI::Token(DTPal::UI_Text, Look.BorderAlpha), 1.0f, Look.bDashed);
	}
}

void FDTUIPainter::Bar(const FBox2f& Box, float Fill01, const FLinearColor& Fill) const
{
	Tile(Box, DTHudStyle::Duty(EDTHudDuty::Track));
	const float Width = Box.GetSize().X * FMath::Clamp(Fill01, 0.0f, 1.0f);
	Tile(FBox2f(Box.Min, FVector2f(Box.Min.X + Width, Box.Max.Y)), Fill);
}

void FDTUIPainter::Arc(const FBox2f& Box, float From01, float To01, const FLinearColor& Color, float ThicknessRef, float InsetRef) const
{
	From01 = FMath::Clamp(From01, 0.0f, 1.0f);
	To01 = FMath::Clamp(To01, 0.0f, 1.0f);
	// The canvas's own white square: a triangle list needs a texture, and the colour is carried by the vertices.
	const FTexture* const White = Canvas && Canvas->DefaultTexture ? Canvas->DefaultTexture->GetResource() : nullptr;
	if (!White || To01 <= From01)
	{
		return;
	}
	const FVector2f Centre = Box.GetCenter();
	const float Outer = 0.5f * FMath::Min(Box.GetSize().X, Box.GetSize().Y) - InsetRef * S;
	const float Inner = Outer - FMath::Max(1.5f, ThicknessRef * S);
	const int32 Segments = FMath::Max(1, FMath::CeilToInt32((To01 - From01) * DTUIStyleLocal::CircleSegments));
	// Clockwise from the top of the ring: screen Y grows downward.
	const auto Point = [&Centre](float Turn01, float Radius)
	{
		const float Angle = 2.0f * UE_PI * Turn01;
		return FVector2D(Centre.X + Radius * FMath::Sin(Angle), Centre.Y - Radius * FMath::Cos(Angle));
	};
	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(2 * Segments);
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const float A = FMath::Lerp(From01, To01, static_cast<float>(Segment) / Segments);
		const float B = FMath::Lerp(From01, To01, static_cast<float>(Segment + 1) / Segments);
		FCanvasUVTri First;
		First.V0_Pos = Point(A, Outer);
		First.V1_Pos = Point(B, Outer);
		First.V2_Pos = Point(A, Inner);
		First.V0_Color = First.V1_Color = First.V2_Color = Color;
		FCanvasUVTri Second;
		Second.V0_Pos = Point(B, Outer);
		Second.V1_Pos = Point(B, Inner);
		Second.V2_Pos = Point(A, Inner);
		Second.V0_Color = Second.V1_Color = Second.V2_Color = Color;
		Triangles.Add(First);
		Triangles.Add(Second);
	}
	FCanvasTriangleItem Item(Triangles, White);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}
