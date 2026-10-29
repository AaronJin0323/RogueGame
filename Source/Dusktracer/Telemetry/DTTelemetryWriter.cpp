#include "Telemetry/DTTelemetryWriter.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FDTTelemetryWriter::FDTTelemetryWriter()
	: Pipe(TEXT("DTTelemetryWriter"))
{
}

FDTTelemetryWriter::~FDTTelemetryWriter()
{
	Wait();
}

FString FDTTelemetryWriter::GetDirectory()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Playtests")));
}

FString FDTTelemetryWriter::MakeBaseName(int32 Seed, const FDateTime& Wall)
{
	return FString::Printf(TEXT("%d_%s"), Seed, *Wall.ToString(TEXT("%Y%m%d_%H%M%S")));
}

void FDTTelemetryWriter::Open(const FString& BaseName)
{
	Wait();
	const FString Base = FPaths::Combine(GetDirectory(), BaseName);
	CsvPath = Base + TEXT(".csv");
	JsonlPath = Base + TEXT(".jsonl");
}

void FDTTelemetryWriter::Append(FString Csv, FString Jsonl)
{
	if (!IsOpen() || (Csv.IsEmpty() && Jsonl.IsEmpty()))
	{
		return;
	}
	Pipe.Launch(TEXT("DTTelemetryAppend"), [CsvFile = CsvPath, JsonlFile = JsonlPath, CsvText = MoveTemp(Csv), JsonlText = MoveTemp(Jsonl)]()
	{
		IFileManager& Files = IFileManager::Get();
		Files.MakeDirectory(*FPaths::GetPath(CsvFile), true);
		const FFileHelper::EEncodingOptions Utf8 = FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM;
		if (!CsvText.IsEmpty())
		{
			FFileHelper::SaveStringToFile(CsvText, *CsvFile, Utf8, &Files, FILEWRITE_Append);
		}
		if (!JsonlText.IsEmpty())
		{
			FFileHelper::SaveStringToFile(JsonlText, *JsonlFile, Utf8, &Files, FILEWRITE_Append);
		}
	});
}

void FDTTelemetryWriter::Wait()
{
	Pipe.WaitUntilEmpty();
}

void FDTTelemetryWriter::Close()
{
	Wait();
	CsvPath.Reset();
	JsonlPath.Reset();
}
