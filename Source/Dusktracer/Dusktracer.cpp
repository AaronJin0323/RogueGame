#include "Dusktracer.h"
#include "Internationalization/StringTableRegistry.h"
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
		LOCTABLE_FROMFILE_GAME("ST_UI", "ST_UI", "DT/Text/Generated/ST_UI.csv");
		LOCTABLE_FROMFILE_GAME("ST_Weapons", "ST_Weapons", "DT/Text/Generated/ST_Weapons.csv");
		LOCTABLE_FROMFILE_GAME("ST_Modifiers", "ST_Modifiers", "DT/Text/Generated/ST_Modifiers.csv");
		LOCTABLE_FROMFILE_GAME("ST_Keepers", "ST_Keepers", "DT/Text/Generated/ST_Keepers.csv");
		LOCTABLE_FROMFILE_GAME("ST_Keywords", "ST_Keywords", "DT/Text/Generated/ST_Keywords.csv");
		LOCTABLE_FROMFILE_GAME("ST_Reactions", "ST_Reactions", "DT/Text/Generated/ST_Reactions.csv");
		LOCTABLE_FROMFILE_GAME("ST_Enemies", "ST_Enemies", "DT/Text/Generated/ST_Enemies.csv");
		LOCTABLE_FROMFILE_GAME("ST_Reforging", "ST_Reforging", "DT/Text/Generated/ST_Reforging.csv");
		LOCTABLE_FROMFILE_GAME("ST_Codex", "ST_Codex", "DT/Text/Generated/ST_Codex.csv");
		LOCTABLE_FROMFILE_GAME("ST_KeeperLines", "ST_KeeperLines", "DT/Text/Generated/ST_KeeperLines.csv");
		LOCTABLE_FROMFILE_GAME("ST_World", "ST_World", "DT/Text/Generated/ST_World.csv");
		LOCTABLE_FROMFILE_GAME("ST_Discoveries", "ST_Discoveries", "DT/Text/Generated/ST_Discoveries.csv");
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FDusktracerModule, Dusktracer, "Dusktracer");
