#include "Dusktracer.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogDT);

/** Game module: registers the string tables (07_Technical R-TEC-08: namespace = table id, CSVs from Tools/loc_build.py --build). */
class FDusktracerModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		LOCTABLE_FROMFILE_GAME("ST_UI", "ST_UI", "DT/Text/Generated/ST_UI.csv");
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDusktracerModule, Dusktracer, "Dusktracer");
