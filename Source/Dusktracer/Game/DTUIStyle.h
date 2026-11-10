// The shared UI style (02_Systems/hud.md R-HUD-08 to R-HUD-10): one type table, one panel painter and the HUD's colour
// duties, all on UCanvas and the engine font. The HUD, the XP bar and the boss bar draw through it; the offer screen, the
// hub and the menus keep their own painters until their own package moves them here.
#pragma once

#include "CoreMinimal.h"

class UCanvas;
class UFont;

/** A step of the type table (R-HUD-08): a multiplier on the engine font at a canvas DTUI::RefHeight lines tall. */
enum class EDTType : uint8
{
	/** The run clock. */
	Display,
	/** Banners, the level, the run's last line. */
	Title,
	/** Every readout and label a player reads in play. */
	Body,
	/** Developer lines only (package.md R-PKG-02); never player text. */
	Note,
	Num
};

/** The states a panel is drawn in (R-HUD-09). */
enum class EDTPanel : uint8
{
	/** Translucent dark with a faint border: a readout over the field. */
	Default,
	/** Darker, with a full border: what the player has focused. */
	Focus,
	/** As Default with a dashed border: sealed, and it says so in words too. */
	Locked,
	/** Opaque, no border: text that must hold its contrast whatever is behind it (modifiers.md R-MOD-08). */
	Solid,
	Num
};

/** How a panel state is filled and edged; colours are Pal.UI tokens only. */
struct FDTPanelLook
{
	float FillAlpha = 1.0f;
	float BorderAlpha = 0.0f;
	bool bDashed = false;
};

namespace DTUI
{
	/** The canvas height every layout number is written for. */
	inline constexpr float RefHeight = 1080.0f;

	/** The type table (hud.md Numbers), in EDTType order. */
	inline constexpr float TypeSteps[static_cast<int32>(EDTType::Num)] = { 2.6f, 1.8f, 1.5f, 1.0f };

	inline float CanvasScale(float ViewHeight) { return ViewHeight / RefHeight; }
	constexpr float TypeStep(EDTType Type) { return TypeSteps[static_cast<int32>(Type) < static_cast<int32>(EDTType::Num) ? static_cast<int32>(Type) : 0]; }
	DUSKTRACER_API FDTPanelLook PanelLook(EDTPanel State);
	/** A palette token as a linear colour at an opacity. */
	DUSKTRACER_API FLinearColor Token(const FColor& Color, float Alpha = 1.0f);
}

/** A colour duty of the HUD (R-HUD-10): what a mark means decides its token, never the other way round. */
enum class EDTHudDuty : uint8
{
	Text,
	Track,
	Hp,
	Xp,
	XpFlash,
	Boss,
	DashReady,
	DashRefill,
	RingIdle,
	RingReady,
	Num
};

namespace DTHudStyle
{
	/** The token a duty draws in. */
	DUSKTRACER_API FColor DutyToken(EDTHudDuty Duty);
	/** Opacity of a duty's mark, one unless the duty is a dimmed form of its token. */
	DUSKTRACER_API float DutyAlpha(EDTHudDuty Duty);
	DUSKTRACER_API FLinearColor Duty(EDTHudDuty Duty);
}

/** Draws on a canvas in reference pixels: S is the canvas scale, T the player's text size (settings.md R-SET-12). */
struct DUSKTRACER_API FDTUIPainter
{
	UCanvas* Canvas = nullptr;
	UFont* Font = nullptr;
	float S = 1.0f;
	float T = 1.0f;

	/** The drawn scale of a step: step x canvas x text size, times Mul for a line shrunk to its column. */
	float TextScale(EDTType Type, float Mul = 1.0f) const { return DTUI::TypeStep(Type) * S * T * Mul; }
	FVector2f Measure(const FString& Line, EDTType Type, float Mul = 1.0f) const;
	void Text(const FText& Line, const FVector2f& At, EDTType Type, const FLinearColor& Color, float Mul = 1.0f) const;
	void Tile(const FBox2f& Box, const FLinearColor& Color) const;
	/** A border inside Box, ThicknessRef reference pixels wide and never under one drawn pixel. */
	void Frame(const FBox2f& Box, const FLinearColor& Color, float ThicknessRef = 1.0f, bool bDashed = false) const;
	void Panel(const FBox2f& Box, EDTPanel State) const;
	/** A track on the panel tone with its fill from the left. */
	void Bar(const FBox2f& Box, float Fill01, const FLinearColor& Fill) const;
	/** An arc of a ring inscribed in Box, clockwise from the top, From01 to To01 of the circle. */
	void Arc(const FBox2f& Box, float From01, float To01, const FLinearColor& Color, float ThicknessRef, float InsetRef = 0.0f) const;
};
