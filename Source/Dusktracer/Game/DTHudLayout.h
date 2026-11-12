// Where every in-run readout sits (02_Systems/hud.md R-HUD-01 to R-HUD-07): a pure function of the viewport, the player's
// text size and the text to show, so tests walk it without a canvas. The HUD, the XP bar and the boss bar draw only the
// rects it returns.
#pragma once

#include "CoreMinimal.h"

/** What a rect of the layout holds. */
enum class EDTHudElem : uint8
{
	HpText,
	HpBar,
	DevLine,
	BossName,
	BossBar,
	Banner,
	Toast,
	Clock,
	Level,
	XpBar,
	XpText,
	LevelUp,
	Pending,
	Ring,
	IntercessionLabel,
	DashLabel,
	DashPip,
	ToolPanel,
	ToolLine,
	MeterLabel,
	MeterBar,
	AimMode,
	DormantPanel,
	DormantLine,
};

/** One Keeper meter line (keepers.md R-KEE-38): its text and whether a bar sits under it. */
struct FDTHudMeterLine
{
	FText Label;
	bool bBar = true;
};

/** What one player's readouts say. An empty text is a readout that is not shown now. */
struct FDTHudPlayerContent
{
	FText Hp;
	FText Level;
	FText Xp;
	FText LevelUp;
	FText Pending;
	FText Intercession;
	FText Dash;
	int32 DashPips = 0;
	TArray<FText> Tools;
	TArray<FDTHudMeterLine> Meters;
	FText AimMode;
	TArray<FText> Dormant;
};

/** Everything the layout places. */
struct FDTHudContent
{
	TArray<FDTHudPlayerContent> Players;
	/** Developer lines under the HP block; none in Shipping (package.md R-PKG-02). */
	TArray<FText> DevLines;
	FText Clock;
	FText BossName;
	FText Banner;
	FText Toast;
};

struct FDTHudRect
{
	EDTHudElem Kind = EDTHudElem::HpText;
	int32 Player = 0;
	/** Which of its kind for that player: a pip, a tool, a meter, a dormant socket. */
	int32 Index = 0;
	/** For a wrapped dormant line, the line within its panel. */
	int32 Line = 0;
	FBox2f Box = FBox2f(ForceInit);
	/** Index into FDTHudLayout::Rects of the panel this rect sits in, or INDEX_NONE. */
	int32 Parent = INDEX_NONE;
	/** Below one when the line was shrunk to fit its column. */
	float TextMul = 1.0f;
	/** The text of a wrapped line; empty for every other rect. */
	FString Wrapped;
};

struct DUSKTRACER_API FDTHudLayout
{
	TArray<FDTHudRect> Rects;

	const FDTHudRect* Find(EDTHudElem Kind, int32 Player = 0, int32 Index = 0, int32 Line = 0) const;
};

/** Size of a line of text at a drawn scale, in pixels. */
using FDTHudMeasure = TFunctionRef<FVector2f(const FString& Line, float Scale)>;

/** The low-HP state of an HP block (R-HUD-05). */
struct FDTHudLowHp
{
	/** HP is at or under C.HUD.LOW_HP_FRAC of its maximum: the bar is framed for as long as it holds. */
	bool bLow = false;
	/** Opacity of the bar's fill: one unless it is pulsing. */
	float FillAlpha = 1.0f;
};

namespace DTHud
{
	/** R-HUD-01. Culture enters through the content's text and Measure. */
	DUSKTRACER_API FDTHudLayout Layout(const FVector2f& View, float TextScale, const FDTHudContent& Content, FDTHudMeasure Measure);

	/**
	 * The boss row at the top centre: the name, then its bar on the same line. PlaceNameSize is the name at the default text size,
	 * which places the bar (the bar never moves or grows with the text-size setting: 06_UX R-UX-01 counts its lower edge); NameSize
	 * is the name as drawn, which grows leftward from the gap and shrinks by OutNameMul if it would leave the centre column.
	 */
	DUSKTRACER_API void BossRow(const FVector2f& View, const FVector2f& PlaceNameSize, const FVector2f& NameSize, FBox2f& OutName, FBox2f& OutBar,
		float* OutNameMul = nullptr);

	/** R-UX-01's ground circle: the farthest point an enemy attack can start plus a Tank's diameter, in uu. */
	DUSKTRACER_API float KeepOutRadiusUU();
	/** Where a point of the floor lands on screen: Ground is the offset from the hero, X up the screen and Y to its right. */
	DUSKTRACER_API FVector2f ProjectGround(const FVector2f& View, const FVector2f& Ground);
	/** The floor point under a pixel, as an offset from the hero; false above the horizon. */
	DUSKTRACER_API bool UnprojectGround(const FVector2f& View, const FVector2f& Pixel, FVector2f& OutGround);
	/** R-HUD-02: true when Box covers any of the keep-out circle as the solo camera frames it. */
	DUSKTRACER_API bool CoversKeepOut(const FVector2f& View, const FBox2f& Box);

	/** R-HUD-03: the parts of the frame Tools/contrast_check.py does not read (its HUD_MASKS), as fractions x0, y0, x1, y1. */
	DUSKTRACER_API TConstArrayView<FVector4f> ContrastMasks();
	/** True for the readouts that grow out of the bottom-left block as a build adds them; the masks do not hold these. */
	DUSKTRACER_API bool GrowsWithBuild(EDTHudElem Kind);

	/** R-HUD-05. SimSeconds is sim time, so the pulse stops with the sim; bAllowFlash is settings.md R-SET-09. */
	DUSKTRACER_API FDTHudLowHp LowHp(float Hp, float MaxHp, double SimSeconds, bool bAllowFlash);
}
