// Tracer art debug switch (R-TEC-12: compiled out of Shipping; 02_Systems/tracer_art.md R-TRC-01, D-068).
// dt.Tracer.ClearZone sets the hero's clear zone in hero radii for frames and perf bounds: negative (the default) uses
// C.TRC.CLEAR_ZONE_RATIO, 0 draws every round. Outside Shipping it starts from the environment variable DT_TRACER_CLEAR_ZONE when that is
// set, so a script that cannot pass a console variable (Tools\Perf.ps1, Tools\Smoke.ps1) can still shoot both ways. Draw only.

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "Presentation/DTTracerArt.h"

namespace DTDebugTracer
{
	/** dt.Tracer.ClearZone's start: DT_TRACER_CLEAR_ZONE when set (a number of hero radii; negative uses the constant), else -1. */
	static float& StartValue()
	{
		float& Setting = DTTracerArt::ClearZoneSetting();
		const FString Env = FPlatformMisc::GetEnvironmentVariable(TEXT("DT_TRACER_CLEAR_ZONE"));
		if (!Env.IsEmpty())
		{
			Setting = FCString::Atof(*Env);
		}
		return Setting;
	}

	static FAutoConsoleVariableRef CVarClearZone(TEXT("dt.Tracer.ClearZone"), StartValue(),
		TEXT("The hero's clear zone in hero radii (D-068): negative (default) uses C.TRC.CLEAR_ZONE_RATIO; 0 draws every player round. Starts from DT_TRACER_CLEAR_ZONE when set."));
}

#endif // !UE_BUILD_SHIPPING
