#include "Settings/DTSettingsSubsystem.h"
#include "Dusktracer.h"
#include "Core/DTConstantsGen.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Scalability.h"

namespace DTSettingsSubsystemLocal
{
	/** -DTSettingsFile=<path>, empty when absent. */
	static FString PathOverride()
	{
		FString Path;
		FParse::Value(FCommandLine::Get(), TEXT("DTSettingsFile="), Path);
		return Path;
	}
}

FString UDTSettingsSubsystem::ActivePath()
{
	const FString Override = DTSettingsSubsystemLocal::PathOverride();
	if (!DTSettings::UsesFile(FApp::IsUnattended(), !Override.IsEmpty()))
	{
		return FString();
	}
	return Override.IsEmpty() ? DTSettings::DefaultPath() : Override;
}

void UDTSettingsSubsystem::Persist()
{
	const FString Path = ActivePath();
	if (!Path.IsEmpty())
	{
		DTSettings::Save(Path, DTSettings::Live());
	}
}

void UDTSettingsSubsystem::ApplyVolume(const FDTUserSettings& Settings)
{
	FApp::SetVolumeMultiplier(static_cast<float>(Settings.VolumePct) / DTConst::SETTINGS_VOLUME_MAX_PCT);
}

void UDTSettingsSubsystem::ApplyDisplay(const FDTUserSettings& Settings)
{
	if (!FApp::CanEverRender() || GIsEditor)
	{
		return;
	}
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Levels.SetFromSingleQualityLevel(DTSettings::ScalabilityLevel(Settings.Graphics));
	Scalability::SetQualityLevels(Levels);

	// An off-screen run has no window to change (and must never make one).
	UGameUserSettings* Window = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (Window && !FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")))
	{
		Window->SetFullscreenMode(Settings.Window == EDTWindow::Windowed ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
		Window->ApplyResolutionSettings(/*bCheckForCommandLineOverrides*/ true);
	}
}

void UDTSettingsSubsystem::ApplyCulture(const FDTUserSettings& Settings)
{
	FInternationalization::Get().SetCurrentCulture(DTSettings::CultureName(Settings.Language));
}

void UDTSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const FString Path = ActivePath();
	FDTUserSettings Loaded;
	const bool bFile = !Path.IsEmpty() && DTSettings::Load(Path, Loaded);
	DTSettings::Live() = Loaded;
	ApplyVolume(Loaded);
	FString Culture;
	if (bFile && !FParse::Value(FCommandLine::Get(), TEXT("culture="), Culture))
	{
		ApplyCulture(Loaded);
	}
	else
	{
		// The command line wins for this launch (R-SET-06), a first launch keeps the engine's choice; the row shows what runs.
		DTSettings::Live().Language = FInternationalization::Get().GetCurrentCulture()->GetName().StartsWith(TEXT("zh")) ? EDTLanguage::ZhHans : EDTLanguage::En;
	}
	if (bFile)
	{
		ApplyDisplay(Loaded);
	}
	UE_LOG(LogDT, Log, TEXT("Settings: %s %s"), bFile ? TEXT("read") : TEXT("no file;"), *Path);
}
