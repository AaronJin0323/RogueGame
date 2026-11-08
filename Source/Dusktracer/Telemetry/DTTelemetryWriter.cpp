#include "Telemetry/DTTelemetryWriter.h"
#include "Dusktracer.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
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
		// A reader (Export_Logs, a tester's editor) may hold the file: share it for reading, and retry a refused open briefly
		// rather than drop the rows, which the core has already handed over.
		const auto AppendText = [&Files, Utf8](const FString& Text, const FString& File)
		{
			for (int32 Try = 0; Try < 20; ++Try)
			{
				if (FFileHelper::SaveStringToFile(Text, *File, Utf8, &Files, FILEWRITE_Append | FILEWRITE_AllowRead))
				{
					return;
				}
				FPlatformProcess::Sleep(0.05f);
			}
			UE_LOG(LogDT, Warning, TEXT("Telemetry: could not append to %s"), *File);
		};
		if (!CsvText.IsEmpty())
		{
			AppendText(CsvText, CsvFile);
		}
		if (!JsonlText.IsEmpty())
		{
			AppendText(JsonlText, JsonlFile);
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
