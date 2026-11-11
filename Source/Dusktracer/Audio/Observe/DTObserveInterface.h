// The interface family's observer (02_Systems/audio.md Data, "Interface events"): the menu, the hub and the offer hand, read
// once a frame from their state getters and counters. The Step sound (a settings value moved) is not raised in this job.
#pragma once

#include "CoreMinimal.h"
#include "Audio/DTAudioObserver.h"
#include "Audio/DTSoundsGen.h"
#include "Hub/DTHubState.h"
#include "Settings/DTMenuState.h"

/** What the interface shows in one frame: all the observer compares. */
struct FDTInterfaceView
{
	EDTMenuScreen Screen = EDTMenuScreen::None;
	int32 MenuFocus = 0;
	EDTBindDevice MenuColumn = EDTBindDevice::Kbm;
	EDTMenuStatus MenuStatus = EDTMenuStatus::None;
	bool bHub = false;
	EDTHubPage HubPage = EDTHubPage::Root;
	int32 HubFocus = 0;
	/** The local player's open hand (player 0, as the offer screen reads it), INDEX_NONE when none is open. */
	int32 HandId = INDEX_NONE;
	int32 RerollOf = INDEX_NONE;
	int32 HandFocus = 0;
	/** The local player's offer counters, which only grow within a world. */
	int32 Decisions = 0;
	int32 Rerolls = 0;
	int32 Banishes = 0;
	int32 Locks = 0;
};

/** The interface's sounds from two views a frame apart. Pure, so the tests stage views. */
namespace DTInterfaceEvents
{
	/** How deep a menu screen sits: deeper is an Open, shallower a Back. */
	DUSKTRACER_API int32 Depth(EDTMenuScreen Screen);
	DUSKTRACER_API void Diff(const FDTInterfaceView& Was, const FDTInterfaceView& Now, TFunctionRef<void(DTCat::ESoundId Sound, int32 Count)> Raise);
	/** The view of a world now. */
	DUSKTRACER_API FDTInterfaceView Read(const UWorld& World);
}

class DUSKTRACER_API FDTObserveInterface : public IDTAudioFamily
{
public:
	virtual void OnFrame(const UWorld& World, IDTAudioRequests& Out) override;
	// No Reset: the interface belongs to the world, not to a run, and its counters only grow within a world.

private:
	FDTInterfaceView Last;
};
