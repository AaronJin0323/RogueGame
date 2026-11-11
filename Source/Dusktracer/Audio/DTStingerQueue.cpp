#include "Audio/DTStingerQueue.h"
#include "Core/DTConstantsGen.h"

bool FDTStingerQueue::Push(int32 Path, EDTStinger Kind, double Now)
{
	if (Path < 0 || Path >= NumPaths || Precedence(Kind) <= 0)
	{
		return false;
	}
	if (Kind == EDTStinger::Pick)
	{
		for (const FEntry& Entry : Entries)
		{
			if (Entry.Path == Path && Entry.Kind == EDTStinger::Tier)
			{
				++Dropped;
				return false; // the tier line already carries this Path's motif
			}
		}
	}
	if (Kind == EDTStinger::Tier)
	{
		Entries.RemoveAll([this, Path](const FEntry& Entry)
		{
			const bool bDrop = Entry.Path == Path && Entry.Kind == EDTStinger::Pick;
			Dropped += bDrop ? 1 : 0;
			return bDrop;
		});
	}
	if (Entries.Num() >= Capacity)
	{
		++Dropped;
		return false;
	}
	FEntry Entry;
	Entry.Path = Path;
	Entry.Kind = Kind;
	Entry.Time = Now;
	Entries.Add(Entry);
	return true;
}

bool FDTStingerQueue::Pop(double Now, FEntry& Out)
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		if (Now - Entries[Index].Time > DTConst::AUD_STINGER_EXPIRY_S)
		{
			Entries.RemoveAt(Index, EAllowShrinking::No);
			++Dropped;
		}
	}
	if (Entries.Num() == 0 || Now - LastStart < DTConst::AUD_STINGER_GAP_S)
	{
		return false;
	}
	int32 Best = 0;
	for (int32 Index = 1; Index < Entries.Num(); ++Index)
	{
		const int32 Mine = Precedence(Entries[Index].Kind);
		const int32 Theirs = Precedence(Entries[Best].Kind);
		if (Mine > Theirs || (Mine == Theirs && Entries[Index].Time < Entries[Best].Time))
		{
			Best = Index;
		}
	}
	Out = Entries[Best];
	Entries.RemoveAt(Best, EAllowShrinking::No);
	LastStart = Now;
	return true;
}

void FDTStingerQueue::Reset()
{
	Entries.Reset();
	LastStart = -1.0e9;
}
