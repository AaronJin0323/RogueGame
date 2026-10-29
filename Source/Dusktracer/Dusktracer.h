#pragma once

#include "CoreMinimal.h"
#include "ProfilingDebugging/CsvProfiler.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDT, Log, All);

/** CSV profiler category for live-load stats (Tools/Perf.ps1 reads DT/LiveBullets, DT/LiveEnemies). */
CSV_DECLARE_CATEGORY_MODULE_EXTERN(DUSKTRACER_API, DT);
