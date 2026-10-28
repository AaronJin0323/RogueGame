// Versioned, checksummed save envelope with an upgrade-only migration chain (07_Technical R-TEC-04).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTConstantsGen.h"

enum class EDTSaveLoadResult : uint8
{
	Ok,
	BadHeader,
	Corrupt,
	TooNew,
	MigrationMissing,
	MigrationFailed
};

/** Upgrades a payload from version N to N+1; returns false if it cannot. */
using FDTSaveMigration = TFunction<bool(TArray<uint8>& Payload)>;

/**
 * Layout (little-endian): magic "DTSV", uint32 version, uint32 payload size, uint32 CRC32 of the payload, payload.
 * Every save goes through Wrap; every load through Unwrap. Versions only go up: a save newer than the build is rejected.
 */
struct DUSKTRACER_API FDTSaveEnvelope
{
	/** Migrations keyed by the version they upgrade FROM. */
	TMap<int32, FDTSaveMigration> Migrations;

	static TArray<uint8> Wrap(const TArray<uint8>& Payload, int32 Version = DTConst::SAVE_VERSION);

	/** On Ok, OutPayload is at CurrentVersion and OutLoadedVersion is the version found on disk. */
	EDTSaveLoadResult Unwrap(const TArray<uint8>& Bytes, TArray<uint8>& OutPayload, int32& OutLoadedVersion,
		int32 CurrentVersion = DTConst::SAVE_VERSION) const;
};
