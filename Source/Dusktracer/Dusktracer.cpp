#include "Dusktracer.h"
#include "HAL/ExceptionHandling.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogDT);
CSV_DEFINE_CATEGORY_MODULE(DUSKTRACER_API, DT, true);

/** Game module: registers the string tables (07_Technical R-TEC-08: namespace = table id, CSVs from Tools/loc_build.py --build). */
class FDusktracerModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
#if PLATFORM_WINDOWS && !WITH_EDITOR
		// 02_Systems/package.md R-PKG-16: a packaged game that crashes ends. The engine writes its minidump in process, and at a busy
		// moment that step stalls with every other thread suspended (Docs/Validation/S1_crash_exit.md). Off: GUseCrashReportClient false
		// skips the reporter path (crash folder, context, dump), and an empty MiniDumpFilenameW leaves WriteMinidump no file to open.
		// Shipping is off unless -DTCrashDump; any other packaged build keeps the engine's path unless -DTCrashNoDump. The engine sets
		// MiniDumpFilenameW in FEngineLoop::AppInit, before this module loads, and nowhere after it.
		const bool bNoDump = UE_BUILD_SHIPPING ? !FParse::Param(FCommandLine::Get(), TEXT("DTCrashDump")) : FParse::Param(FCommandLine::Get(), TEXT("DTCrashNoDump"));
		if (bNoDump)
		{
			GUseCrashReportClient = false;
			MiniDumpFilenameW[0] = TEXT('\0');
		}
		UE_LOG(LogDT, Log, TEXT("Crash handling: %s"), bNoDump ? TEXT("no crash folder and no minidump; the process ends") : TEXT("the engine's crash folder and minidump"));
#endif
		LOCTABLE_FROMFILE_GAME("ST_UI", "ST_UI", "DT/Text/Generated/ST_UI.csv");
		LOCTABLE_FROMFILE_GAME("ST_Weapons", "ST_Weapons", "DT/Text/Generated/ST_Weapons.csv");
		LOCTABLE_FROMFILE_GAME("ST_Modifiers", "ST_Modifiers", "DT/Text/Generated/ST_Modifiers.csv");
		LOCTABLE_FROMFILE_GAME("ST_Keepers", "ST_Keepers", "DT/Text/Generated/ST_Keepers.csv");
		LOCTABLE_FROMFILE_GAME("ST_Keywords", "ST_Keywords", "DT/Text/Generated/ST_Keywords.csv");
		LOCTABLE_FROMFILE_GAME("ST_Reactions", "ST_Reactions", "DT/Text/Generated/ST_Reactions.csv");
		LOCTABLE_FROMFILE_GAME("ST_Enemies", "ST_Enemies", "DT/Text/Generated/ST_Enemies.csv");
		LOCTABLE_FROMFILE_GAME("ST_Reforging", "ST_Reforging", "DT/Text/Generated/ST_Reforging.csv");
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDusktracerModule, Dusktracer, "Dusktracer");
