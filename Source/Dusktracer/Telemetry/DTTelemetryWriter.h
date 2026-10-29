// Off-thread file appends for telemetry (02_Systems/telemetry.md R-TEL-08): the game thread never waits on disk.
#pragma once

#include "CoreMinimal.h"
#include "Tasks/Pipe.h"

/**
 * Appends text to the session's .csv and .jsonl in order on one background pipe. Open() only sets the paths; the
 * directory and files are created by the first append. Wait() blocks until every queued append is on disk (run end,
 * tests).
 */
class DUSKTRACER_API FDTTelemetryWriter
{
public:
	FDTTelemetryWriter();
	~FDTTelemetryWriter();

	/** Saved/Playtests under the project's (or packaged build's) Saved directory. */
	static FString GetDirectory();
	/** "<seed>_<yyyymmdd_hhmmss>" for a session that starts at Wall. */
	static FString MakeBaseName(int32 Seed, const FDateTime& Wall);

	void Open(const FString& BaseName);
	bool IsOpen() const { return !CsvPath.IsEmpty(); }
	const FString& GetCsvPath() const { return CsvPath; }
	const FString& GetJsonlPath() const { return JsonlPath; }

	/** Queues the text for both files (either may be empty). Returns immediately. */
	void Append(FString Csv, FString Jsonl);
	/** Blocks until the queue is empty. */
	void Wait();
	/** Waits, then forgets the paths. */
	void Close();

private:
	UE::Tasks::FPipe Pipe;
	FString CsvPath;
	FString JsonlPath;
};
