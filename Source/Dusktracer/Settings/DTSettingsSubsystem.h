// Loads the settings file at game start and applies changes to the engine (settings.md R-SET-03..08).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Settings/DTUserSettings.h"
#include "DTSettingsSubsystem.generated.h"

UCLASS()
class DUSKTRACER_API UDTSettingsSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Writes the file, if this process uses one (R-SET-05). */
	static void Persist();
	/** The settings file of this process, empty when it uses none (R-SET-05). */
	static FString ActivePath();

	/** Sets the application volume from the settings (R-SET-07). */
	static void ApplyVolume(const FDTUserSettings& Settings);
	/** Sets the scalability level and the window mode (R-SET-08). */
	static void ApplyDisplay(const FDTUserSettings& Settings);
	/** Switches the culture now; off the input path (R-SET-06). */
	static void ApplyCulture(const FDTUserSettings& Settings);
};
