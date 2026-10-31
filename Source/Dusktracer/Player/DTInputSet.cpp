#include "Player/DTInputSet.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Settings/DTUserSettings.h"
#include "UObject/Package.h"

namespace DTInputSetLocal
{
	static UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, FName(Name), RF_Transient);
		Action->ValueType = Type;
		return Action;
	}

	/** Maps a key onto a 2D action as one screen axis: Y keys are swizzled from X, negative keys negated. */
	static void MapAxisKey(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bYAxis, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bYAxis)
		{
			UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Context);
			Swizzle->Order = EInputAxisSwizzle::YXZ;
			Mapping.Modifiers.Add(Swizzle);
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(Context));
		}
	}
}

UDTInputSet* UDTInputSet::Create(UObject* Outer)
{
	UDTInputSet* Set = NewObject<UDTInputSet>(Outer ? Outer : GetTransientPackage(), NAME_None, RF_Transient);
	Set->Build();
	return Set;
}

void UDTInputSet::Build()
{
	using DTInputSetLocal::MakeAction;

	Move = MakeAction(this, TEXT("IA_Move"), EInputActionValueType::Axis2D);
	Aim = MakeAction(this, TEXT("IA_Aim"), EInputActionValueType::Axis2D);
	Dash = MakeAction(this, TEXT("IA_Dash"), EInputActionValueType::Boolean);
	Intercession = MakeAction(this, TEXT("IA_Intercession"), EInputActionValueType::Boolean);
	// Triggers run through a pause, or the first frame after it reads a held key as a press (settings.md R-SET-27).
	Dash->bTriggerWhenPaused = true;
	Intercession->bTriggerWhenPaused = true;
	Pause = MakeAction(this, TEXT("IA_Pause"), EInputActionValueType::Boolean);
	Pause->bTriggerWhenPaused = true;

	Kbm = NewObject<UInputMappingContext>(this, FName(TEXT("IMC_Gameplay_KBM")), RF_Transient);
	Pad = NewObject<UInputMappingContext>(this, FName(TEXT("IMC_Gameplay_Pad")), RF_Transient);
	ApplyBindings(DTSettings::Live().Bindings);
}

void UDTInputSet::ApplyBindings(const FDTBindings& Bindings)
{
	using DTInputSetLocal::MapAxisKey;

	// combat_core.md Data, input mapping table; settings.md R-SET-13 for what a player may change.
	Kbm->UnmapAll();
	MapAxisKey(Kbm, Move, Bindings.Get(EDTBindSlot::MoveRight, EDTBindDevice::Kbm), false, false);
	MapAxisKey(Kbm, Move, Bindings.Get(EDTBindSlot::MoveLeft, EDTBindDevice::Kbm), false, true);
	MapAxisKey(Kbm, Move, Bindings.Get(EDTBindSlot::MoveUp, EDTBindDevice::Kbm), true, false);
	MapAxisKey(Kbm, Move, Bindings.Get(EDTBindSlot::MoveDown, EDTBindDevice::Kbm), true, true);
	Kbm->MapKey(Aim, EKeys::Mouse2D);
	Kbm->MapKey(Dash, Bindings.Get(EDTBindSlot::Dash, EDTBindDevice::Kbm));
	Kbm->MapKey(Intercession, Bindings.Get(EDTBindSlot::Intercession, EDTBindDevice::Kbm));
	Kbm->MapKey(Pause, EKeys::Escape);

	Pad->UnmapAll();
	Pad->MapKey(Move, EKeys::Gamepad_Left2D);
	Pad->MapKey(Aim, EKeys::Gamepad_Right2D);
	Pad->MapKey(Dash, Bindings.Get(EDTBindSlot::Dash, EDTBindDevice::Pad));
	Pad->MapKey(Intercession, Bindings.Get(EDTBindSlot::Intercession, EDTBindDevice::Pad));
	Pad->MapKey(Pause, EKeys::Gamepad_Special_Right);
}

TArray<UInputAction*> UDTInputSet::GetActions() const
{
	return { Move.Get(), Aim.Get(), Dash.Get(), Intercession.Get(), Pause.Get() };
}

TArray<UInputMappingContext*> UDTInputSet::GetContexts() const
{
	return { Kbm.Get(), Pad.Get() };
}
