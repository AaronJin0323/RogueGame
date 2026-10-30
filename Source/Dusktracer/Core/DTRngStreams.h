// Per-system random streams derived from the run seed (07_Technical R-TEC-02).
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

enum class EDTRngStream : uint8
{
	Spawn,
	Offer,
	Combat,
	Boss,
	Weather,
	Loot,
	Director, // S1; appended so the seeds of earlier streams never change (R-TEC-02)
	Count
};

/** One FRandomStream per system, each seeded from the run seed and its stream index, so drawing from one never moves another. */
struct DUSKTRACER_API FDTRngStreams
{
	FDTRngStreams() { Reset(0); }
	explicit FDTRngStreams(int32 InRunSeed) { Reset(InRunSeed); }

	void Reset(int32 InRunSeed);

	FRandomStream& Get(EDTRngStream Stream) { return Streams[static_cast<int32>(Stream)]; }
	int32 GetRunSeed() const { return RunSeed; }

	/** Platform-independent seed for one stream (splitmix64 of the run seed and the stream index). */
	static int32 DeriveSeed(int32 InRunSeed, EDTRngStream Stream);

	/** Seed of one player's own copy of a stream (offers.md: a per-player Offer stream); independent of the shared one. */
	static int32 DerivePlayerSeed(int32 InRunSeed, EDTRngStream Stream, int32 PlayerIndex);

private:
	int32 RunSeed = 0;
	FRandomStream Streams[static_cast<int32>(EDTRngStream::Count)];
};
