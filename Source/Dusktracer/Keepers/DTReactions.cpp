#include "Keepers/DTReactions.h"
#include "Keepers/DTKeeperSpec.h"
#include "Telemetry/DTTelemetrySources.h"

namespace DTReactionsLocal
{
	static float Find(const DTCat::FReactionParam* Params, int32 Num, const TCHAR* Name, float Default)
	{
		for (int32 Index = 0; Index < Num; ++Index)
		{
			if (FCString::Strcmp(Params[Index].Name, Name) == 0)
			{
				return Params[Index].Value;
			}
		}
		return Default;
	}
}

FName DTReactions::UnlockId()
{
	static const FName Id(TEXT("Reaction.Duos"));
	return Id;
}

int32 DTReactions::FindDuo(FName BlessingId)
{
	static const TMap<FName, int32> Rows = []()
	{
		TMap<FName, int32> Out;
		for (int32 Row = 0; Row < DTCat::NumReactions; ++Row)
		{
			Out.Add(FName(DTCat::Reactions[Row].BlessingId), Row);
		}
		return Out;
	}();
	const int32* Found = Rows.Find(BlessingId);
	return Found ? *Found : INDEX_NONE;
}

int32 DTReactions::FindPair(EDTPath A, EDTPath B)
{
	for (int32 Row = 0; Row < DTCat::NumReactions; ++Row)
	{
		const DTCat::FReactionRow& R = DTCat::Reactions[Row];
		if ((R.PathA == A && R.PathB == B) || (R.PathA == B && R.PathB == A))
		{
			return Row;
		}
	}
	return INDEX_NONE;
}

bool DTReactions::DuoCountsFor(int32 Row, EDTPath Path)
{
	return Row >= 0 && Row < DTCat::NumReactions && (DTCat::Reactions[Row].PathA == Path || DTCat::Reactions[Row].PathB == Path);
}

uint16 DTReactions::Source(int32 Row)
{
	static const TArray<uint16> Ids = []()
	{
		TArray<uint16> Out;
		for (int32 Index = 0; Index < DTCat::NumReactions; ++Index)
		{
			Out.Add(FDTTelemetrySources::FindOrAdd(DTCat::Reactions[Index].SourceName));
		}
		return Out;
	}();
	return Ids.IsValidIndex(Row) ? Ids[Row] : FDTTelemetrySources::Unknown;
}

uint32 DTReactions::LiveMask(const FDTKeeperRunState& Keepers)
{
	uint32 Mask = 0;
	for (int32 Row = 0; Row < DTCat::NumReactions; ++Row)
	{
		const DTCat::FReactionRow& R = DTCat::Reactions[Row];
		if (DTKeepers::OwnedOfPath(Keepers, R.PathA) > 0 && DTKeepers::OwnedOfPath(Keepers, R.PathB) > 0)
		{
			Mask |= 1u << Row;
		}
	}
	return Mask;
}

uint32 DTReactions::DuoMask(const FDTKeeperRunState& Keepers)
{
	uint32 Mask = 0;
	for (const FName& Id : Keepers.Owned)
	{
		const int32 Row = FindDuo(Id);
		Mask |= Row != INDEX_NONE ? 1u << Row : 0u;
	}
	return Mask;
}

float DTReactions::Param(int32 Row, const TCHAR* Name, float Default)
{
	const DTCat::FReactionRow& R = DTCat::Reactions[Row];
	return DTReactionsLocal::Find(R.Params, R.NumParams, Name, Default);
}

float DTReactions::DuoParam(int32 Row, const TCHAR* Name, float Default)
{
	const DTCat::FReactionRow& R = DTCat::Reactions[Row];
	return DTReactionsLocal::Find(R.DuoParams, R.NumDuoParams, Name, Default);
}
