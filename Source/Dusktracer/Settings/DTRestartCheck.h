// Development proofs of the run loop on a real launch, driven by Tools/RestartCheck.ps1 (settings.md R-SET-28). Not in Shipping.
#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
class UDTMenuSubsystem;

namespace DTRestartCheck
{
	/**
	 * Called from the menu tick, paused or not.
	 *   -DTRestartAtStep=<n> -DTRestartSeed=<s>  restarts the launch's first run on seed s at sim step n, through the menu's path.
	 *   -DTInputCheck                            real key events through the player controller: logs one INPUTCHECK verdict, then exits.
	 */
	void Tick(UDTMenuSubsystem* Menu);
}
#endif
