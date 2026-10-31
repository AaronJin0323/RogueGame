// The profile of this launch (02_Systems/meta.md): picks the path, opens one session, and answers the boss's one question.
#pragma once

#include "CoreMinimal.h"
#include "Meta/DTMetaSession.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DTMetaSubsystem.generated.h"

UCLASS()
class DUSKTRACER_API UDTMetaSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Unattended, automation, a bot or -DTNoSave (R-MET-14). */
	static bool IsToolRun();
	/** The file this process uses: -DTMetaFile, else the default for a person's launch, else empty. */
	static FString ActivePath();

	/** The session of WorldContext's game instance, or nullptr (test worlds have none). */
	static FDTMetaSession* FindSession(const UObject* WorldContext);
	/** R-MET-17: 0 with no session. */
	static int32 GetWins(const UObject* WorldContext);

private:
	FDTMetaSession Session;
};
