// Story layers as the game reads them (03_Narrative T-NAR-06).

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Core/DTConstantsGen.h"
#include "Debug/DTDebugCodex.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTStoryLayerTest, "Dusktracer.Core.StoryLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDTStoryLayerTest::RunTest(const FString& Parameters)
{
	// T-NAR-06: Layer and Unlock reach the game as metadata, and nothing deeper than the cap needs no win.
	const TArray<DTDebugCodex::FStoryRow> Rows = DTDebugCodex::ReadStoryRows();
	TestTrue(TEXT("the story tables are loaded"), Rows.Num() > 0);
	TestEqual(TEXT("the deepest layer before the first win"), DTDebugCodex::MaxLayerBeforeFirstWin(Rows), DTConst::NARR_PREWIN_LAYER_MAX);

	const DTDebugCodex::FStoryRow* Root = Rows.FindByPredicate([](const DTDebugCodex::FStoryRow& Row) { return Row.Key == TEXT("Codex.Root.Title"); });
	TestNotNull(TEXT("the stop line exists"), Root);
	if (Root)
	{
		TestEqual(TEXT("the stop line's layer"), Root->Layer, DTConst::NARR_LAYER_MAX);
		TestEqual(TEXT("the stop line's unlock"), Root->Unlock, FString(TEXT("Win")));
	}
	TestFalse(TEXT("a Tier 3 line needs no win"), DTDebugCodex::NeedsWin(TEXT("Tier.3")));
	TestTrue(TEXT("the Win Recollection needs a win"), DTDebugCodex::NeedsWin(TEXT("Discovery.Win")));
	return true;
}

#endif
