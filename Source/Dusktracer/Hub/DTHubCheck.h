// The hub proved on a real launch with real key events and shots of every page (Tools/VigilCheck.ps1, vigil.md R-VIG-17). Not in Shipping.
#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
class UDTHubSubsystem;

namespace DTHubCheck
{
	/** Runs from the hub subsystem's tick in every world; -DTHubCheck=First|Reboot|Gated|Large starts it. */
	void Tick(UDTHubSubsystem* Hub);
}
#endif
