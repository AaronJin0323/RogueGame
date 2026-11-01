#include "Hub/DTHubFlow.h"
#include "Core/DTRunSeed.h"
#include "Engine/EngineBaseTypes.h"
#include "Game/DTGameMode.h"
#include "Hub/DTHubTab.h"
#include "Meta/DTMetaSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace DTHubFlowLocal
{
	static TFunction<TSharedPtr<IDTHubTab>(UWorld*)> TreeFactory;

	static bool Forced()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("DTHub"));
	}
}

bool DTHubFlow::Wants(bool bToolRun, bool bForced, bool bSeedOption)
{
	return (!bToolRun || bForced) && !bSeedOption;
}

bool DTHubFlow::Enabled()
{
	return Wants(UDTMetaSubsystem::IsToolRun(), DTHubFlowLocal::Forced(), false);
}

bool DTHubFlow::WantsHub(const FURL& Url)
{
	return Wants(UDTMetaSubsystem::IsToolRun(), DTHubFlowLocal::Forced(), Url.HasOption(TEXT("DTSeed")));
}

int32 DTHubFlow::BeginSeed(const FDTRunSeed& Launch, bool bFirstRun, uint32 Entropy)
{
	// A person's fresh seed is this hub world's own; a tool that forced the hub keeps seed 0 for no run of its own, so it draws.
	const bool bNamed = Launch.Source == EDTSeedSource::Explicit && bFirstRun;
	return bNamed || Launch.Source == EDTSeedSource::Fresh ? Launch.Seed : ADTGameMode::NextSeed(Launch.Seed, Entropy);
}

void DTHubTabs::SetTreeFactory(TFunction<TSharedPtr<IDTHubTab>(UWorld*)> Factory)
{
	DTHubFlowLocal::TreeFactory = MoveTemp(Factory);
}

TSharedPtr<IDTHubTab> DTHubTabs::MakeTree(UWorld* World)
{
	return DTHubFlowLocal::TreeFactory ? DTHubFlowLocal::TreeFactory(World) : TSharedPtr<IDTHubTab>();
}
