#include "Hub/DTHubData.h"
#include "Core/DTConstantsGen.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Offers/DTOfferGenerator.h"

namespace DTHubDataLocal
{
	static int64 Lifetime(const FDTMetaProfile& Profile, const FString& Name)
	{
		const int64* Found = Profile.LifetimeStats.Find(FName(*Name));
		return Found ? *Found : 0;
	}

	static FText Table(const TCHAR* TableName, const FString& Key)
	{
		return FText::FromStringTable(FName(TableName), Key);
	}

	/** The line of a row before the body: the table's rows have no titles of their own. */
	static FText Title(const FDTCodexSource& Row)
	{
		FString Tail;
		if (Row.Key.Split(TEXT("Codex.Keeper."), nullptr, &Tail))
		{
			return Table(TEXT("ST_Keepers"), Tail + TEXT(".Name"));
		}
		if (Row.Key.Split(TEXT("Codex.Enemy."), nullptr, &Tail))
		{
			return Table(TEXT("ST_Enemies"), FString::Printf(TEXT("Enemy.%s.Name"), *Tail));
		}
		if (Row.Key == TEXT("Codex.Warden.FormIII"))
		{
			return Table(TEXT("ST_Enemies"), TEXT("Enemy.Boss.Name"));
		}
		if (Row.Unlock.Split(TEXT("Discovery."), nullptr, &Tail))
		{
			return Table(TEXT("ST_Discoveries"), FString::Printf(TEXT("Discovery.%s.Name"), *Tail));
		}
		return Table(TEXT("ST_Codex"), Row.Key);
	}
}

bool DTHubData::IsUnlocked(const FString& Unlock, const FDTMetaProfile& Profile)
{
	FString Tail;
	if (Unlock == TEXT("Start"))
	{
		return true;
	}
	if (Unlock == TEXT("Win"))
	{
		return Profile.Wins > 0;
	}
	if (Unlock == TEXT("Wins3.FormIII"))
	{
		return Profile.Wins > DTConst::BOSS_FORM_III_WINS; // the kill itself is one more win than the wins that open the form
	}
	if (Unlock == TEXT("Kill.Boss"))
	{
		return DTHubDataLocal::Lifetime(Profile, TEXT("BossKills")) > 0;
	}
	if (Unlock.Split(TEXT("Kill."), nullptr, &Tail))
	{
		return DTHubDataLocal::Lifetime(Profile, TEXT("Kills.Role.") + Tail) > 0;
	}
	if (Unlock.Split(TEXT("Discovery."), nullptr, &Tail))
	{
		return Profile.Discoveries.Contains(FName(*Unlock)) || Profile.Discoveries.Contains(FName(*Tail));
	}
	return false;
}

int32 DTHubData::LayerReach(const FDTMetaProfile& Profile)
{
	return Profile.Wins > 0 ? DTConst::NARR_LAYER_MAX : DTConst::NARR_PREWIN_LAYER_MAX;
}

TArray<FDTCodexSource> DTHubData::ReadCodexTable()
{
	TArray<FDTCodexSource> Rows;
	const FStringTableConstPtr Found = FStringTableRegistry::Get().FindStringTable(FName(TEXT("ST_Codex")));
	if (Found.IsValid())
	{
		Found->EnumerateKeysAndSourceStrings([&Rows, &Found](const FTextKey& Key, const FString&)
		{
			FDTCodexSource Row;
			Row.Key = Key.ToString();
			Row.Layer = FCString::Atoi(*Found->GetMetaData(Key, TEXT("Layer")));
			Row.Unlock = Found->GetMetaData(Key, TEXT("Unlock"));
			Rows.Add(MoveTemp(Row));
			return true;
		});
	}
	return Rows;
}

TArray<FDTCodexRow> DTHubData::BuildCodex(const TArray<FDTCodexSource>& Sources, const FDTMetaProfile& Profile)
{
	const int32 Reach = LayerReach(Profile);
	TArray<FDTCodexSource> Listed = Sources.FilterByPredicate([Reach](const FDTCodexSource& Row)
	{
		return Row.Layer <= Reach && Row.Key != TEXT("Codex.EntryTitle"); // a format, not an entry
	});
	Listed.Sort([](const FDTCodexSource& A, const FDTCodexSource& B) { return A.Layer != B.Layer ? A.Layer < B.Layer : A.Key < B.Key; });
	TArray<FDTCodexRow> Book;
	for (const FDTCodexSource& Source : Listed)
	{
		FDTCodexRow Row;
		Row.Key = Source.Key;
		Row.Layer = Source.Layer;
		Row.bRead = IsUnlocked(Source.Unlock, Profile);
		Row.Title = Row.bRead ? DTHubDataLocal::Title(Source) : FText::FromStringTable(TEXT("ST_UI"), TEXT("Codex.Locked"));
		if (Row.bRead && Source.Key != TEXT("Codex.Root.Title"))
		{
			Row.Body = DTHubDataLocal::Table(TEXT("ST_Codex"), Source.Key);
		}
		Book.Add(MoveTemp(Row));
	}
	return Book;
}

TArray<FDTHubPatron> DTHubData::Patrons(const FDTMetaProfile& Profile, TFunctionRef<bool(FName)> InPool)
{
	const TArray<EDTPath> Seated = DTOffers::RunKeepers();
	TArray<FDTHubPatron> Rows;
	for (const EDTPath Path : DTOffers::AllKeepers())
	{
		const FName Id = DTOffers::KeeperId(Path);
		FDTHubPatron Row;
		Row.Path = Path;
		Row.bOpen = Seated.Contains(Path) || Profile.Unlocked.Contains(Id) || InPool(Id);
		Rows.Add(Row);
	}
	return Rows;
}

EDTPath DTHubData::ParsePath(FName Name)
{
	for (const EDTPath Path : DTOffers::AllKeepers())
	{
		if (Name == FName(DTOffers::PathName(Path)))
		{
			return Path;
		}
	}
	return EDTPath::None;
}

FText DTHubData::FormatSteps(int64 Steps)
{
	const int64 Seconds = Steps / DTConst::SIM_FIXED_HZ;
	return FText::AsCultureInvariant(FString::Printf(TEXT("%lld:%02lld:%02lld"), Seconds / 3600, (Seconds / 60) % 60, Seconds % 60));
}

TArray<FDTHubStat> DTHubData::Stats(const FDTMetaProfile& Profile)
{
	using DTHubDataLocal::Lifetime;
	const auto Number = [](int64 Value) { return FText::AsNumber(Value); };
	return {
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("Hub.Stats.Balance")), Number(Profile.Currency) },
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("Hub.Stats.Earned")), Number(Lifetime(Profile, TEXT("CurrencyEarned"))) },
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("Hub.Stats.Runs")), Number(Profile.Runs) },
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("Hub.Stats.Wins")), Number(Profile.Wins) },
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("Hub.Stats.Repairs")), Number(Profile.DeathsSinceWin) },
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("RunSummary.Time")), FormatSteps(Lifetime(Profile, TEXT("Steps"))) },
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("Hub.Stats.Kills")), Number(Lifetime(Profile, TEXT("Kills"))) },
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("Hub.Stats.Elites")), Number(Lifetime(Profile, TEXT("Elites"))) },
		{ FText::FromStringTable(TEXT("ST_UI"), TEXT("Hub.Stats.Bosses")), Number(Lifetime(Profile, TEXT("BossKills"))) } };
}

FString DTHubData::RunOptions(int32 Seed, EDTPath Patron, const FDTMetaProfile& Profile)
{
	FString Options = FString::Printf(TEXT("DTSeed=%d"), Seed);
	if (Patron != EDTPath::None)
	{
		Options += FString::Printf(TEXT("?DTPatron=%s"), DTOffers::PathName(Patron));
	}
	TArray<FString> Keepers;
	for (const EDTPath Path : DTOffers::AllKeepers())
	{
		const FName Id = DTOffers::KeeperId(Path);
		if (Profile.Unlocked.Contains(Id) && !DTOffers::RunKeepers().Contains(Path))
		{
			Keepers.Add(Id.ToString());
		}
	}
	if (Keepers.Num() > 0)
	{
		Options += TEXT("?DTUnlock=") + FString::Join(Keepers, TEXT(","));
	}
	return Options;
}
