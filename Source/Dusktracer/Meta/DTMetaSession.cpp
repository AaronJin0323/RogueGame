#include "Meta/DTMetaSession.h"
#include "Dusktracer.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/HideWindowsPlatformTypes.h"
#else
#include <stdio.h>
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
	return bWritten && DTMetaSessionLocal::ReplaceFile(Temp, Full);
}

EDTMetaOpen FDTMetaSession::Open(const FString& InPath)
{
	Profile = FDTMetaProfile();
	Path = InPath;
	AsidePath.Reset();
	bWritable = false;
	if (Path.IsEmpty())
	{
		return EDTMetaOpen::NoSave;
	}
	IFileManager& Files = IFileManager::Get();
	if (!Files.FileExists(*Path))
	{
		bWritable = true;
		return EDTMetaOpen::Fresh;
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent))
	{
		UE_LOG(LogDT, Warning, TEXT("Meta: cannot read %s; this session writes nothing"), *Path);
		return EDTMetaOpen::Stuck;
	}
	TArray<uint8> Payload;
	int32 Loaded = INDEX_NONE;
	const EDTSaveLoadResult Result = Envelope.Unwrap(Bytes, Payload, Loaded, Version);
	FDTMetaProfile Read;
	if (Result == EDTSaveLoadResult::Ok && DTMeta::Deserialize(Payload, Read) && DTMeta::IsValid(Read))
	{
		Profile = MoveTemp(Read);
		bWritable = true;
		return EDTMetaOpen::Read;
	}

	const FString Aside = DTMeta::FreeAsidePath(Path);
	if (!Files.Move(*Aside, *Path, /*bReplace*/ false, /*bEvenIfReadOnly*/ false, /*bAttributes*/ false, /*bDoNotRetryOrError*/ true))
	{
		UE_LOG(LogDT, Warning, TEXT("Meta: bad save %s could not be kept aside; this session writes nothing"), *Path);
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
	if (!DTMeta::WriteAtomic(Path, FDTSaveEnvelope::Wrap(DTMeta::Serialize(Profile), Version)))
	{
		UE_LOG(LogDT, Warning, TEXT("Meta: could not write %s; the old file stands"), *Path);
		return false;
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
	Award.Retained = DTMeta::ApplyRun(Profile, Tally, End);
	const TSet<FName> Known = Profile.Discoveries;
	OnBank.Broadcast(Tally, Profile);
	Award.Discovered = Profile.Discoveries.Difference(Known).Array();
	Award.Discovered.Sort(FNameLexicalLess());
	Award.Balance = Profile.Currency;
	Write();
	UE_LOG(LogDT, Log, TEXT("Meta: banked %lld, balance %lld, runs %d, wins %d"), Award.Retained, Award.Balance, Profile.Runs, Profile.Wins);
	return Award;
}
