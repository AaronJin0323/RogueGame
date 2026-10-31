// One profile on one file: open, bank, atomic write, and a bad file kept aside (02_Systems/meta.md). Plain C++: tests drive it on temp files.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSaveEnvelope.h"
#include "Meta/DTMetaProfile.h"

/** What Open found. Stuck: the file is there and cannot be moved or read, so the session writes nothing (R-MET-12). */
enum class EDTMetaOpen : uint8 { NoSave, Fresh, Read, KeptAside, Stuck };

/** Run-end systems: runs after the award, before the write (R-MET-19). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDTOnMetaBank, const FDTRunTally& /*Tally*/, FDTMetaProfile& /*Profile*/);

struct DUSKTRACER_API FDTMetaSession
{
	FDTMetaProfile Profile;
	/** The file; empty for a session with no save (R-MET-14). */
	FString Path;
	/** Migrations for older versions (R-MET-10). */
	FDTSaveEnvelope Envelope;
	/** The save version this build reads and writes. */
	int32 Version = DTConst::SAVE_VERSION;
	bool bWritable = false;
	/** Where the bad file went, when Open kept one aside. */
	FString AsidePath;
	FDTOnMetaBank OnBank;

	/** Empty InPath: no save. Never fails a launch. */
	EDTMetaOpen Open(const FString& InPath);
	/** Banks one run (R-MET-04, R-MET-05) and autosaves it (R-MET-13). With no file the profile stays as it was. */
	FDTRunAward Bank(const FDTRunTally& Tally, EDTRunEnd End);
	/** Writes the profile; false leaves the old file whole. */
	bool Write();
};

namespace DTMeta
{
	/** True when this process may use a save file: a person's launch, or a tool naming its own (R-MET-14). */
	DUSKTRACER_API bool UsesFile(bool bToolRun, bool bPathGiven);
	/** Saved/SaveGames/DTMeta.sav (G01_Name.md). */
	DUSKTRACER_API FString DefaultPath();
	/** Temp, flush, then one replacing rename (R-MET-11). */
	DUSKTRACER_API bool WriteAtomic(const FString& Path, const TArray<uint8>& Bytes);
	/** The first free <Path>.bad.<n>. */
	DUSKTRACER_API FString FreeAsidePath(const FString& Path);
}
