// DT.Codex.ShowLayer (03_Narrative R-NAR-06; R-TEC-12: compiled out of Shipping).

#include "Debug/DTDebugCodex.h"

#if !UE_BUILD_SHIPPING

#include "Dusktracer.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"

namespace DTDebugCodex
{
	static const TCHAR* const StoryTables[] = { TEXT("ST_Codex"), TEXT("ST_KeeperLines"), TEXT("ST_World") };

	TArray<FStoryRow> ReadStoryRows()
	{
		TArray<FStoryRow> Rows;
		for (const TCHAR* TableName : StoryTables)
		{
			const FName Table(TableName);
			const FStringTableConstPtr Found = FStringTableRegistry::Get().FindStringTable(Table);
			if (!Found.IsValid())
			{
				UE_LOG(LogDT, Warning, TEXT("DT.Codex.ShowLayer: %s is not registered"), TableName);
				continue;
			}
			Found->EnumerateKeysAndSourceStrings([&Rows, &Found, Table](const FTextKey& Key, const FString& Source)
			{
				FStoryRow Row;
				Row.Table = Table;
				Row.Key = Key.ToString();
				Row.Text = Source;
				Row.Layer = FCString::Atoi(*Found->GetMetaData(Key, TEXT("Layer")));
				Row.Unlock = Found->GetMetaData(Key, TEXT("Unlock"));
				Rows.Add(MoveTemp(Row));
				return true;
			});
		}
		Rows.Sort([](const FStoryRow& A, const FStoryRow& B)
		{
			if (A.Layer != B.Layer)
			{
				return A.Layer < B.Layer;
			}
			return A.Table == B.Table ? A.Key < B.Key : A.Table.LexicalLess(B.Table);
		});
		return Rows;
	}

	bool NeedsWin(const FString& Unlock)
	{
		return Unlock == TEXT("Win") || Unlock == TEXT("Wins3.FormIII") || Unlock == TEXT("Discovery.Win") || Unlock == TEXT("Kill.Boss");
	}

	int32 MaxLayerBeforeFirstWin(const TArray<FStoryRow>& Rows)
	{
		int32 Deepest = -1;
		for (const FStoryRow& Row : Rows)
		{
			if (!NeedsWin(Row.Unlock))
			{
				Deepest = FMath::Max(Deepest, Row.Layer);
			}
		}
		return Deepest;
	}

	static void ShowLayer(const TArray<FString>& Args)
	{
		if (Args.Num() != 1 || !Args[0].IsNumeric())
		{
			UE_LOG(LogDT, Warning, TEXT("DT.Codex.ShowLayer <n>: print every story row at layer n or shallower"));
			return;
		}
		const int32 Limit = FCString::Atoi(*Args[0]);
		const TArray<FStoryRow> Rows = ReadStoryRows();
		int32 Shown = 0;
		for (const FStoryRow& Row : Rows)
		{
			if (Row.Layer <= Limit)
			{
				UE_LOG(LogDT, Display, TEXT("LAYER %d %s %s [%s] %s"), Row.Layer, *Row.Table.ToString(), *Row.Key, *Row.Unlock, *Row.Text);
				++Shown;
			}
		}
		UE_LOG(LogDT, Display, TEXT("LAYER: %d of %d rows at layer %d or shallower; deepest before the first win %d"), Shown, Rows.Num(), Limit, MaxLayerBeforeFirstWin(Rows));
	}

	static FAutoConsoleCommand ShowLayerCmd(TEXT("DT.Codex.ShowLayer"),
		TEXT("DT.Codex.ShowLayer <n>: print every story row at layer n or shallower, and the deepest layer reachable before the first win"),
		FConsoleCommandWithArgsDelegate::CreateStatic(&ShowLayer));
}

#endif
