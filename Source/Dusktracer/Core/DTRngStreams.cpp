#include "Core/DTRngStreams.h"

namespace DTRng
{
	static uint64 SplitMix64(uint64 X)
	{
		X += 0x9E3779B97F4A7C15ull;
		X = (X ^ (X >> 30)) * 0xBF58476D1CE4E5B9ull;
		X = (X ^ (X >> 27)) * 0x94D049BB133111EBull;
		return X ^ (X >> 31);
	}
}

int32 FDTRngStreams::DeriveSeed(int32 InRunSeed, EDTRngStream Stream)
{
	const uint64 Mixed = DTRng::SplitMix64((static_cast<uint64>(static_cast<uint32>(InRunSeed)) << 8) | static_cast<uint64>(Stream));
	return static_cast<int32>(static_cast<uint32>(Mixed ^ (Mixed >> 32)));
}

void FDTRngStreams::Reset(int32 InRunSeed)
{
	RunSeed = InRunSeed;
	for (int32 Index = 0; Index < static_cast<int32>(EDTRngStream::Count); ++Index)
	{
		Streams[Index].Initialize(DeriveSeed(InRunSeed, static_cast<EDTRngStream>(Index)));
	}
}
