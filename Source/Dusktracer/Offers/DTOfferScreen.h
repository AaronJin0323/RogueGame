// The offer screen (offers.md R-OFF-11 e/f, R-OFF-13; 04_Presentation R-PRE-08): a Canvas-drawn five-card hand with tool
// buttons. Drawn from ADTHUD::DrawHUD through UDTOfferSubsystem::DrawScreen; no binary UI assets (R-TEC-06).
// Keyboard + mouse and gamepad are live at once from the first screen; the confirm guard keeps a held button from picking.
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "UObject/Object.h"
#include "Core/DTRunState.h"
#include "DTOfferScreen.generated.h"

class UCanvas;

/** One screen command, from any device. */
enum class EDTOfferAction : uint8
{
	None,
	Prev,
	Next,
	Confirm,   // the focused card (or the hovered one for a click)
	PickSlot,  // number keys: that slot directly
	Reroll,
	Banish,    // the focused card
	Lock       // the focused card
};

struct FDTOfferCommand
{
	EDTOfferAction Action = EDTOfferAction::None;
	int32 Slot = INDEX_NONE;
};

/** R-OFF-11 (e): confirm-type commands are ignored for C.OFFER.INPUT_GUARD_S after a hand opens (wall time: the sim is paused). */
struct DUSKTRACER_API FDTOfferInputGuard
{
	double OpenedAt = 0.0;
	void Open(double Now) { OpenedAt = Now; }
	bool Allows(double Now) const;
	/** Seconds of guard left (0 when open for input). */
	double Remaining(double Now) const;
	static bool IsConfirmType(EDTOfferAction Action) { return Action == EDTOfferAction::Confirm || Action == EDTOfferAction::PickSlot; }
};

/** Bindings: every action has a keyboard key and a gamepad button; Confirm and the tools also take a mouse click. */
namespace DTOfferInput
{
	DUSKTRACER_API const TArray<FKey>& Keys();
	DUSKTRACER_API FDTOfferCommand ForKey(const FKey& Key);
	/** The keys bound to Action (PickSlot: the number keys), for hints and tests. */
	DUSKTRACER_API TArray<FKey> KeysFor(EDTOfferAction Action);
}

/** What the screen draws this frame. */
struct FDTOfferScreenModel
{
	const FDTOfferHand* Hand = nullptr;
	const FDTPlayerRunState* Player = nullptr;
	int32 Focus = 0;
	bool bGuardActive = false;
};

UCLASS(Transient)
class DUSKTRACER_API UDTOfferScreen : public UObject
{
	GENERATED_BODY()

public:
	/** Draws the hand over the frame and records the card and tool rectangles for mouse input. */
	void Draw(UCanvas* Canvas, const FDTOfferScreenModel& Model);

	/** Card slot under a canvas position (the last drawn layout), or INDEX_NONE. */
	int32 CardAt(const FVector2D& Position) const;
	/** Tool button under a canvas position: Reroll, Banish or Lock; None elsewhere. */
	EDTOfferAction ToolAt(const FVector2D& Position) const;
	bool HasLayout() const { return CardRects.Num() > 0; }

private:
	void DrawCard(UCanvas* Canvas, const FDTOfferCard& Card, const FDTPlayerRunState& Player, const FBox2D& Rect, int32 Slot, bool bFocus);
	/** The focused card's `<kw>` notes in a panel from Top down (offers.md R-OFF-13). */
	void DrawNotes(UCanvas* Canvas, const FDTOfferCard& Card, const FBox2D& CardRect, float Top);

	TArray<FBox2D> CardRects;
	FBox2D ToolRects[3];
	bool bToolsShown = false;
};
