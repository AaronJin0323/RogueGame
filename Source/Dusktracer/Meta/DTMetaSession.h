// One profile on one file: open, bank, atomic write, and a bad file kept aside (02_Systems/meta.md). Plain C++: tests drive it on temp files.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSaveEnvelope.h"
#include "Meta/DTMetaProfile.h"

/** What Open found. Stuck: the file is there and cannot be moved or read, so the session writes nothing (R-MET-12). */
enum class EDTMetaOpen : uint8 { NoSave, Fresh, Read, KeptAside, Stuck };

/** Run-end systems: runs after the award, before the write (R-MET-19). */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDTOnMetaBank, const FDTRunTally& /*Tally*/, FDTMetaProfile& /*Profile*/);

struct FDTMetaSession;

/**
 * The save's lock (R-MET-11, D-042): the sibling <save>.lock opened with no sharing, for the length of one change. The operating
 * system closes the handle when its process ends, however it ends, so a crash leaves no lock behind; a file of that name that
 * nobody holds blocks nothing. Waits up to C.SAVE.LOCK_WAIT_MS, asking every C.SAVE.LOCK_RETRY_MS.
 */
struct DUSKTRACER_API FDTMetaLock
{
	explicit FDTMetaLock(const FString& SavePath, int32 WaitMs = DTConst::SAVE_LOCK_WAIT_MS);
	~FDTMetaLock();
	FDTMetaLock(const FDTMetaLock&) = delete;
	FDTMetaLock& operator=(const FDTMetaLock&) = delete;
	bool IsHeld() const { return bHeld; }
	static FString PathFor(const FString& SavePath);

private:
	bool TryOpen(const FString& LockPath);
	bool bHeld = false;
	void* Handle = nullptr;
	int32 Descriptor = -1;
};

/**
 * One change to the profile, as a thing that can be made again: it runs on this session's profile, and runs once more on the
 * file's profile when another launch wrote the file before this change reached it. False: it does not apply to that profile,
 * and it changed nothing.
 */
using FDTMetaEdit = TFunction<bool(FDTMetaSession&)>;

/** How FDTMetaSession::Change went. Busy: the lock could not be had in time. Unsaved: made in memory, the write failed. */
enum class EDTMetaChange : uint8 { Written, Unsaved, Busy, Refused };

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
	/** The file's bytes as this session last read or wrote them; another process's write shows as a difference (R-MET-11). */
	TArray<uint8> Seen;
	/** The profile in memory is newer than the file: a write failed or the lock was busy. The next save point carries it. */
	bool bUnsaved = false;
	/** The changes the file does not hold yet, oldest first: made again on the file's profile if another launch wrote it meanwhile. */
	TArray<FDTMetaEdit> Pending;
	FDTOnMetaBank OnBank;

	/** Empty InPath: no save. Never fails a launch. */
	EDTMetaOpen Open(const FString& InPath);
	/** Banks one run (R-MET-04, R-MET-05) and autosaves it (R-MET-13). With no file the profile stays as it was. */
	FDTRunAward Bank(const FDTRunTally& Tally, EDTRunEnd End);
	/**
	 * One change to the save (R-MET-11): takes the lock, rereads the file if another launch wrote it, makes the edit, writes,
	 * lets the lock go. With the lock busy past its wait, bKeepIfBusy makes the edit in memory and keeps it for the next save
	 * point (a bank: a run cannot be asked for again); without it nothing is changed (a purchase: the player can ask again).
	 */
	EDTMetaChange Change(const FDTMetaEdit& Edit, bool bKeepIfBusy);
	/** A save point with no change of its own: writes what is waiting, under the lock. True when the file holds everything. */
	bool Flush();
	/**
	 * Writes the profile as it stands in memory, under the lock, with no reread: for a plant (tests, debug commands). The game's
	 * own changes go through Change. False leaves the old file whole.
	 */
	bool Write();
	/**
	 * When another launch on the same file wrote it since this session last did, read its profile and make this session's
	 * waiting changes again on top of it (two windows on one save). True when the profile was replaced. A file that no longer
	 * reads keeps this session's profile. Takes the lock itself.
	 */
	bool Refresh();

private:
	/** Refresh and Write for a caller that holds the lock. */
	bool Reread();
	bool WriteHeld();
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
