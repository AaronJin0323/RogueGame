// Name table for damage/kill sources (02_Systems/telemetry.md R-TEL-05). A hit carries a small id, never a string.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"

/**
 * Process-wide table of source names. Id 0 is always "Unknown" and id 1 "Stress" (the debug emitter); weapons, modifiers
 * and Keepers register "Weapon.<Name>", "Modifier.<Name>", "Keeper.<Path>" (once, at any time, from any package or
 * thread) and stamp the id on every FDTHit they make. Ids are stable for the life of the process; output never depends
 * on them (records name sources by string, sorted by name).
 */
class DUSKTRACER_API FDTTelemetrySources
{
public:
	static constexpr uint16 Unknown = 0;
	static constexpr uint16 Stress = 1;

	/** Returns the id of Name, adding it when new. Thread-safe. */
	static uint16 FindOrAdd(const FString& Name);

	/** "Weapon.<Name>" (weapons.md R-WPN-07). Takes the bare name or the content id "Weapon.<Name>". */
	static uint16 RegisterWeapon(FName WeaponId);
	/** "Modifier.<Name>" (modifiers.md R-MOD-09). Takes the bare name or the content id "Mod.<Name>" / "Modifier.<Name>". */
	static uint16 RegisterModifier(FName ModifierId);
	/** "Keeper.<Path>" (keepers.md R-KEE-13). EDTPath::None gives Unknown. */
	static uint16 RegisterKeeper(EDTPath Path);

	/** The name of Id, or "Unknown" for an id never registered. Thread-safe. */
	static FString GetName(uint16 Id);

	/** Number of registered names (ids are [0, Num)). */
	static int32 Num();

	/** "Storm", "Forge", ... ; "None" for EDTPath::None. */
	static const TCHAR* PathName(EDTPath Path);
};
