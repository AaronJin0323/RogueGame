// Keeper stingers in turn (02_Systems/audio.md R-AUD-11): ported from RogueHero's FRHKeeperStingerQueue. Engine-free; no reader
// in this job (the Keeper signatures family wires it).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"

/** What a stinger says. Precedence when several are due: Tier, then PassedOver, then Pick (wave 1 builds Pick only). */
enum class EDTStinger : uint8 { Pick, PassedOver, Tier, Count };

/**
 * One Keeper speaks at a time:
 *  - at most one stinger starts per C.AUD.STINGER_GAP_S, the highest precedence first, the oldest among equals;
 *  - an entry older than C.AUD.STINGER_EXPIRY_S is dropped (it would land late against its line);
 *  - a Tier for a Path drops that Path's pending Pick, and a Pick behind a pending Tier of its Path is dropped;
 *  - at most Capacity entries wait (one per Path in practice); more are dropped.
 * Paths are indices 0..NumPaths-1 in EDTPath order.
 */
struct DUSKTRACER_API FDTStingerQueue
{
	struct FEntry
	{
		int32 Path = INDEX_NONE;
		EDTStinger Kind = EDTStinger::Pick;
		double Time = 0.0;
	};

	static constexpr int32 NumPaths = static_cast<int32>(EDTPath::Count);
	static constexpr int32 Capacity = NumPaths;

	static int32 Precedence(EDTStinger Kind) { return Kind == EDTStinger::Tier ? 3 : (Kind == EDTStinger::PassedOver ? 2 : (Kind == EDTStinger::Pick ? 1 : 0)); }

	/** False when the entry is invalid, dropped by a Tier of its Path, or the queue is full. */
	bool Push(int32 Path, EDTStinger Kind, double Now);
	/** The stinger to start now, if any (removed). Call once per frame. */
	bool Pop(double Now, FEntry& Out);
	void Reset();
	int32 Num() const { return Entries.Num(); }
	int64 GetDropped() const { return Dropped; }

private:
	TArray<FEntry, TInlineAllocator<Capacity>> Entries;
	double LastStart = -1.0e9;
	int64 Dropped = 0;
};
