// A person's whole path on a real launch with real key events: no tool flag, the hub, a death, the bank (Tools/PackageHubCheck.ps1, package.md R-PKG-13). Not in Shipping.
#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
class UDTMenuSubsystem;

namespace DTPathCheck
{
	/**
	 * Called from the menu tick, paused or not, in every world of the process.
	 *   -DTPathCheck=Walk      fresh save: boot, every page, the Shrine, a run played out to a death, the hub, the Ledger-stone, a second run abandoned, the hub, Quit to desktop.
	 *   -DTPathCheck=Reboot    a later process on that save: a Patron and the Ledger-stone are still there, the language switches live, then Quit to desktop.
	 *   -DTPathPatron=<Path>   the Keeper the walk picks at the Shrine, Forge by default.
	 *   -DTPathShots=<dir>     a shot of each page.
	 * Logs what every string table shows in the running culture, and one PATHCHECK verdict before the last key quits the game.
	 */
	void Tick(UDTMenuSubsystem* Menu);
}
#endif
