#include "Meta/DTMetaSession.h"
#include "Dusktracer.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include <fcntl.h>
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace DTMetaSessionLocal
{
	/** Replaces Dest with Temp in one operation (R-MET-11): the file is the old one or the new one, never neither. */
	static bool ReplaceFile(const FString& Temp, const FString& Dest)
	{
#if PLATFORM_WINDOWS
		return ::MoveFileExW(*Temp, *Dest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		return ::rename(TCHAR_TO_UTF8(*Temp), TCHAR_TO_UTF8(*Dest)) == 0;
#endif
	}
}

FString FDTMetaLock::PathFor(const FString& SavePath)
{
	return FPaths::ConvertRelativePathToFull(SavePath) + TEXT(".lock");
}

bool FDTMetaLock::TryOpen(const FString& LockPath)
{
#if PLATFORM_WINDOWS
	// No sharing: a second open fails while this handle lives. Delete on close: no file is left, and none to clean up after a crash.
	const HANDLE Opened = ::CreateFileW(*LockPath, GENERIC_WRITE | DELETE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
	if (Opened == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	Handle = Opened;
	return true;
#else
	// The file stays (removing it would race a waiter that has it open); the lock is on the descriptor and goes with the process.
	const int Opened = ::open(TCHAR_TO_UTF8(*LockPath), O_CREAT | O_RDWR | O_CLOEXEC, 0644);
	if (Opened < 0)
	{
		return false;
	}
	if (::flock(Opened, LOCK_EX | LOCK_NB) != 0)
	{
		::close(Opened);
		return false;
	}
	Descriptor = Opened;
	return true;
#endif
}

FDTMetaLock::FDTMetaLock(const FString& SavePath, int32 WaitMs)
{
	const FString LockPath = PathFor(SavePath);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(LockPath), true);
	const double Until = FPlatformTime::Seconds() + WaitMs / 1000.0;
	for (;;)
	{
		bHeld = TryOpen(LockPath);
		if (bHeld || FPlatformTime::Seconds() >= Until)
		{
			return;
		}
		FPlatformProcess::Sleep(DTConst::SAVE_LOCK_RETRY_MS / 1000.0f);
	}
}

FDTMetaLock::~FDTMetaLock()
{
	if (!bHeld)
	{
		return;
	}
#if PLATFORM_WINDOWS
	::CloseHandle(static_cast<HANDLE>(Handle));
#else
	::close(Descriptor);
#endif
}

bool DTMeta::UsesFile(bool bToolRun, bool bPathGiven)
{
	return !bToolRun || bPathGiven;
}

FString DTMeta::DefaultPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("DTMeta.sav"));
}

FString DTMeta::FreeAsidePath(const FString& Path)
{
	for (int32 Number = 1;; ++Number)
	{
		const FString Candidate = FString::Printf(TEXT("%s.bad.%d"), *Path, Number);
		if (!IFileManager::Get().FileExists(*Candidate))
		{
			return Candidate;
		}
	}
}

FString DTMeta::LostPath(const FString& Path)
{
	return Path + TEXT(".lost");
}

bool DTMeta::WriteAtomic(const FString& Path, const TArray<uint8>& Bytes)
{
	IFileManager& Files = IFileManager::Get();
	const FString Full = FPaths::ConvertRelativePathToFull(Path);
	const FString Temp = Full + TEXT(".tmp");
	Files.MakeDirectory(*FPaths::GetPath(Full), true);
	if (Files.DirectoryExists(*Temp))
	{
		return false;
	}
	TUniquePtr<IFileHandle> Handle(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*Temp));
	const bool bWritten = Handle && Handle->Write(Bytes.GetData(), Bytes.Num()) && Handle->Flush(/*bFullFlush*/ true);
	Handle.Reset(); // closed before the rename
	if (!bWritten)
	{
		return false;
	}
	// A scanner or an indexer can hold the old file for a moment; a few short tries ride that out.
	for (int32 Try = 0; Try < 3; ++Try)
	{
		if (DTMetaSessionLocal::ReplaceFile(Temp, Full))
		{
			return true;
		}
		FPlatformProcess::Sleep(0.05f);
	}
	return false;
}

EDTMetaOpen FDTMetaSession::Open(const FString& InPath)
{
	Profile = FDTMetaProfile();
	Path = InPath;
	AsidePath.Reset();
	StuckOn = EDTMetaNotice::None;
	Seen.Reset();
	bUnsaved = false;
	Pending.Reset();
	UnsavedRuns = 0;
	LostRuns = 0;
	bWritable = false;
	if (Path.IsEmpty())
	{
		return EDTMetaOpen::NoSave;
	}
	IFileManager& Files = IFileManager::Get();
	// What the launch before this one could not write (Close). Read once: the marker goes whether or not the count reads.
	const FString Marker = DTMeta::LostPath(Path);
	if (Files.FileExists(*Marker))
	{
		FString Count;
		FFileHelper::LoadFileToString(Count, *Marker, FFileHelper::EHashOptions::None, FILEREAD_Silent);
		LostRuns = FMath::Max(1, FCString::Atoi(*Count));
		Files.Delete(*Marker, /*bRequireExists*/ false, /*bEvenReadOnly*/ true, /*bQuiet*/ true);
		UE_LOG(LogDT, Warning, TEXT("Meta: the last launch could not save %d run(s) (%s); the hub says so once"), LostRuns, *Marker);
	}
	if (!Files.FileExists(*Path))
	{
		bWritable = true;
		return EDTMetaOpen::Fresh;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
	{
		UE_LOG(LogDT, Warning, TEXT("Meta: cannot read %s; this session writes nothing"), *Path);
		StuckOn = EDTMetaNotice::SaveUnreadable;
		return EDTMetaOpen::Stuck;
	}
	TArray<uint8> Payload;
	int32 Loaded = INDEX_NONE;
	const EDTSaveLoadResult Result = Envelope.Unwrap(Bytes, Payload, Loaded, Version);
	FDTMetaProfile Read;
	if (Result == EDTSaveLoadResult::Ok && DTMeta::Deserialize(Payload, Read) && DTMeta::IsValid(Read))
	{
		Profile = MoveTemp(Read);
		Seen = MoveTemp(Bytes);
		bWritable = true;
		return EDTMetaOpen::Read;
	}

	const FString Aside = DTMeta::FreeAsidePath(Path);
	if (!Files.Move(*Aside, *Path, /*bReplace*/ false, /*bEvenIfReadOnly*/ false, /*bAttributes*/ false, /*bDoNotRetryOrError*/ true))
	{
		UE_LOG(LogDT, Warning, TEXT("Meta: bad save %s could not be kept aside; this session writes nothing"), *Path);
		StuckOn = EDTMetaNotice::SaveNotMoved;
		return EDTMetaOpen::Stuck;
	}
	AsidePath = Aside;
	bWritable = true;
	UE_LOG(LogDT, Warning, TEXT("Meta: bad save (result %d) kept as %s; starting empty"), static_cast<int32>(Result), *Aside);
	return EDTMetaOpen::KeptAside;
}

bool FDTMetaSession::Write()
{
	if (!bWritable)
	{
		return false;
	}
	const FDTMetaLock Lock(Path);
	if (!Lock.IsHeld())
	{
		bUnsaved = true;
		UE_LOG(LogDT, Warning, TEXT("Meta: %s is locked by another launch; nothing was written and the next write carries this change"), *Path);
		return false;
	}
	return WriteHeld();
}

bool FDTMetaSession::Flush()
{
	if (!bUnsaved)
	{
		return true;
	}
	if (!bWritable)
	{
		return false;
	}
	const FDTMetaLock Lock(Path);
	if (!Lock.IsHeld())
	{
		UE_LOG(LogDT, Warning, TEXT("Meta: %s is locked by another launch; %d change(s) are still kept in memory"), *Path, Pending.Num());
		return false;
	}
	Reread();
	return WriteHeld();
}

bool FDTMetaSession::Close()
{
	if (!bUnsaved || Flush())
	{
		return true;
	}
	if (UnsavedRuns > 0 && bWritable && !Path.IsEmpty())
	{
		// Beside the save, not under its lock. A marker an earlier launch left unread is added to, not written over.
		const FString Marker = DTMeta::LostPath(Path);
		FString Old;
		const int32 Before = FFileHelper::LoadFileToString(Old, *Marker, FFileHelper::EHashOptions::None, FILEREAD_Silent) ? FMath::Max(0, FCString::Atoi(*Old)) : 0;
		if (FFileHelper::SaveStringToFile(FString::FromInt(Before + UnsavedRuns), *Marker))
		{
			UE_LOG(LogDT, Warning, TEXT("Meta: %d run(s) could not be saved; %s tells the next launch"), UnsavedRuns, *Marker);
		}
		else
		{
			UE_LOG(LogDT, Warning, TEXT("Meta: %d run(s) could not be saved, and %s could not be written either"), UnsavedRuns, *Marker);
		}
	}
	return false;
}

EDTMetaNotice FDTMetaSession::Notice() const
{
	// One line, the first that holds (vigil.md R-VIG-21): a stuck session is why nothing this launch does is kept.
	if (StuckOn != EDTMetaNotice::None)
	{
		return StuckOn;
	}
	if (!AsidePath.IsEmpty())
	{
		return EDTMetaNotice::KeptAside;
	}
	return LostRuns > 0 ? EDTMetaNotice::RunLost : EDTMetaNotice::None;
}

EDTMetaChange FDTMetaSession::Change(const FDTMetaEdit& Edit, bool bKeepIfBusy)
{
	if (!bWritable || Path.IsEmpty())
	{
		return EDTMetaChange::Refused;
	}
	const FDTMetaLock Lock(Path);
	if (!Lock.IsHeld())
	{
		if (!bKeepIfBusy)
		{
			UE_LOG(LogDT, Warning, TEXT("Meta: %s is locked by another launch; nothing was changed"), *Path);
			return EDTMetaChange::Busy;
		}
		if (!Edit(*this))
		{
			return EDTMetaChange::Refused;
		}
		Pending.Add(Edit);
		bUnsaved = true;
		UE_LOG(LogDT, Warning, TEXT("Meta: %s is locked by another launch; this change is kept in memory and written at the next save (%d waiting)"), *Path, Pending.Num());
		return EDTMetaChange::Busy;
	}
	Reread(); // another window on this save may have banked or bought since; this change lands on top of that
	if (!Edit(*this))
	{
		if (bUnsaved)
		{
			WriteHeld(); // still a save point for what was waiting
		}
		return EDTMetaChange::Refused;
	}
	Pending.Add(Edit);
	return WriteHeld() ? EDTMetaChange::Written : EDTMetaChange::Unsaved;
}

bool FDTMetaSession::WriteHeld()
{
	TArray<uint8> Bytes = FDTSaveEnvelope::Wrap(DTMeta::Serialize(Profile), Version);
	if (!DTMeta::WriteAtomic(Path, Bytes))
	{
		bUnsaved = true;
		UE_LOG(LogDT, Warning, TEXT("Meta: could not write %s; the old file stands and the next write carries this change"), *Path);
		return false;
	}
	Seen = MoveTemp(Bytes);
	bUnsaved = false;
	Pending.Reset();
	UnsavedRuns = 0;
	return true;
}

bool FDTMetaSession::Refresh()
{
	if (!bWritable || Path.IsEmpty())
	{
		return false;
	}
	const FDTMetaLock Lock(Path);
	return Lock.IsHeld() && Reread();
}

bool FDTMetaSession::Reread()
{
	if (!bWritable || Path.IsEmpty() || (bUnsaved && Pending.IsEmpty()))
	{
		return false; // a plant not yet on disk is no change that can be made again: this session's profile is the newer one
	}
	TArray<uint8> Bytes;
	if (!IFileManager::Get().FileExists(*Path) || !FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent) || Bytes == Seen)
	{
		return false;
	}
	TArray<uint8> Payload;
	int32 Loaded = INDEX_NONE;
	FDTMetaProfile Read;
	if (Envelope.Unwrap(Bytes, Payload, Loaded, Version) != EDTSaveLoadResult::Ok || !DTMeta::Deserialize(Payload, Read) || !DTMeta::IsValid(Read))
	{
		UE_LOG(LogDT, Warning, TEXT("Meta: %s changed under this session and does not read; this session's profile stands"), *Path);
		return false;
	}
	Profile = MoveTemp(Read);
	Seen = MoveTemp(Bytes);
	UE_LOG(LogDT, Log, TEXT("Meta: another launch wrote %s; read again, balance %lld, runs %d"), *Path, Profile.Currency, Profile.Runs);
	// What this session still owes the file is made again on what the other launch left.
	const TArray<FDTMetaEdit> Waiting = MoveTemp(Pending);
	Pending.Reset();
	for (const FDTMetaEdit& Edit : Waiting)
	{
		if (Edit(*this))
		{
			Pending.Add(Edit);
		}
		else
		{
			UE_LOG(LogDT, Warning, TEXT("Meta: a change kept in memory no longer applies to %s as another launch left it; it is dropped"), *Path);
		}
	}
	return true;
}

FDTRunAward FDTMetaSession::Bank(const FDTRunTally& Tally, EDTRunEnd End)
{
	FDTRunAward Award;
	if (Tally.Steps <= 0)
	{
		Award.Balance = Profile.Currency;
		return Award; // no run (R-MET-04)
	}
	Award.bBanked = true;
	if (Path.IsEmpty())
	{
		Award.Retained = DTMeta::ComputeAward(Tally);
		Award.Balance = Profile.Currency + Award.Retained;
		return Award;
	}
	// The run as an edit: made on this profile now, and again on the file's if another launch writes before this one can.
	const TSharedRef<TArray<FName>> Discovered = MakeShared<TArray<FName>>();
	const FDTMetaEdit Run = [Tally, End, Discovered](FDTMetaSession& Session)
	{
		DTMeta::ApplyRun(Session.Profile, Tally, End);
		const TSet<FName> Known = Session.Profile.Discoveries;
		Session.OnBank.Broadcast(Tally, Session.Profile);
		*Discovered = Session.Profile.Discoveries.Difference(Known).Array();
		Discovered->Sort(FNameLexicalLess());
		return true;
	};
	if (!bWritable)
	{
		Run(*this); // a stuck session (R-MET-12) plays on in memory and writes nothing
		Award.bNotSaved = true; // and the run-over screen says so (R-MET-06)
	}
	else
	{
		const EDTMetaChange Result = Change(Run, /*bKeepIfBusy*/ true);
		Award.bDeferred = Result == EDTMetaChange::Busy;
		UnsavedRuns += Result == EDTMetaChange::Busy || Result == EDTMetaChange::Unsaved ? 1 : 0; // the file does not hold this run yet
		Award.Waiting = Pending.Num();
	}
	Award.Retained = DTMeta::ComputeAward(Tally);
	Award.Discovered = *Discovered;
	Award.Balance = Profile.Currency;
	UE_LOG(LogDT, Log, TEXT("Meta: banked %lld, balance %lld, runs %d, wins %d"), Award.Retained, Award.Balance, Profile.Runs, Profile.Wins);
	return Award;
}
