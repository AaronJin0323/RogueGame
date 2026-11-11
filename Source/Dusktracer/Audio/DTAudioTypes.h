// Shared types of the audio runtime (02_Systems/audio.md). Engine-free: no UObject, no world.
#pragma once

#include "CoreMinimal.h"
#include "Audio/DTSoundsGen.h"

/** A voice the scheduler decided to start: what the sink hands to the engine and what the voice log records. */
struct FDTAudioVoice
{
	/** Index into the catalog view (DTCat::ESoundId for the generated table). */
	int32 Sound = INDEX_NONE;
	/** Variant file: 0 is SW_<Family>_<Name>, n > 0 adds _n. */
	int32 Variant = 0;
	float Volume = 1.0f;
	float Pitch = 1.0f;
	/** The sim step it started on (run sounds), or the step current when it started (interface sounds). */
	int64 Step = 0;
	/** Real seconds when it started. */
	double Time = 0.0;
	/** The Player group's duck gain in dB when it started (R-AUD-05). */
	float DuckDb = 0.0f;
	/** Events this voice carries (a batch of one step is one voice). */
	int32 Count = 1;
};

/** Per sound id (R-AUD-20): Asked = Played + Merged + Dropped once nothing waits. */
struct FDTAudioCounts
{
	int64 Asked = 0;
	int64 Played = 0;
	int64 Merged = 0;
	int64 Dropped = 0;
};

/** Where started voices go: the engine (UDTAudioSubsystem's engine sink) or a recording (tests). */
class IDTAudioSink
{
public:
	virtual ~IDTAudioSink() = default;
	/** Sound is the resolved asset of the voice's variant, or nullptr when it does not exist. */
	virtual void Start(const DTCat::FSoundRow& Row, const FDTAudioVoice& Voice, class USoundBase* Sound) = 0;
	/** True for the sink that hands voices to the engine (the one an unattended process never makes, R-AUD-03). */
	virtual bool IsEngine() const { return false; }
};

/** A sink that keeps every voice it is given (tests, and the proof that a run's voices are a function of its seed). */
class FDTRecordingSink : public IDTAudioSink
{
public:
	virtual void Start(const DTCat::FSoundRow& Row, const FDTAudioVoice& Voice, class USoundBase* Sound) override { Voices.Add(Voice); }
	TArray<FDTAudioVoice> Voices;
};

namespace DTAudio
{
	/** The degrees of the key a `key` voice draws, in semitones (R-AUD-10, R-AUD-14: A minor pentatonic around the written pitch). */
	inline constexpr int32 KeyDegrees[] = { -5, -3, 0, 2, 4 };

	/** The catalog the game uses: the generated table. */
	inline TConstArrayView<DTCat::FSoundRow> Catalog() { return TConstArrayView<DTCat::FSoundRow>(DTCat::Sounds, DTCat::SoundCount); }

	/** splitmix64: audio's own stream is seeded from the run seed here, inside Audio/, and from no sim stream (R-AUD-01). */
	inline uint64 SplitMix(uint64& State)
	{
		uint64 Z = (State += 0x9E3779B97F4A7C15ull);
		Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ull;
		Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBull;
		return Z ^ (Z >> 31);
	}

	/** The audio stream's seed for a run seed ("audio" salts it away from every sim stream's derivation). */
	inline uint64 StreamSeed(int32 RunSeed)
	{
		uint64 State = (static_cast<uint64>(static_cast<uint32>(RunSeed)) << 32) ^ 0x617564696Full;
		return SplitMix(State);
	}
}
