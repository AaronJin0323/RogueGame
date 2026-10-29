// Name table for damage/kill sources (02_Systems/telemetry.md R-TEL-05). A hit carries a small id, never a string.
#pragma once

#include "CoreMinimal.h"

/**
 * Process-wide table of source names. Id 0 is always "Unknown" and id 1 "Stress" (the debug emitter); later systems
 * register "Weapon.<Key>", "Keeper.<Path>", "Modifier.<Key>" once at startup and stamp the id on every FDTHit they make.
 * Game thread only.
 */
class DUSKTRACER_API FDTTelemetrySources
{
public:
	static constexpr uint16 Unknown = 0;
	static constexpr uint16 Stress = 1;

	/** Returns the id of Name, adding it when new. Ids are stable for the life of the process. */
	static uint16 FindOrAdd(const FString& Name);

	/** The name of Id, or "Unknown" for an id never registered. */
	static const FString& GetName(uint16 Id);

	/** Number of registered names (ids are [0, Num)). */
	static int32 Num();

private:
	static TArray<FString>& Names();
};
