// Per-player run state and owner-tagged events (07_Technical R-TEC-01).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRngStreams.h"

enum class EDTGameEventKind : uint8
{
	Hit,
	Kill,
	Pickup
};

/** Every hit, kill and pickup names the player it belongs to. */
struct FDTGameEvent
{
	EDTGameEventKind Kind = EDTGameEventKind::Hit;
	int32 OwnerId = INDEX_NONE;
	int32 Amount = 0;
};

/** Optional observer of every event offered to a run (telemetry). Called after routing, for routed and rejected events. */
class IDTRunEventSink
{
public:
	virtual ~IDTRunEventSink() = default;
	virtual void OnRunEvent(const FDTGameEvent& Event, bool bRouted) = 0;
};

/** Everything one player owns during a run. Systems add their fields here, never to a global. */
struct FDTPlayerRunState
{
	int32 PlayerIndex = INDEX_NONE;
	int32 Level = 1;
	int64 Xp = 0;
	int64 DamageDealt = 0;
	int32 Kills = 0;
	int32 Pickups = 0;
};

/** The run: its seed-derived streams and one state per player. There is no single-player shortcut. */
class DUSKTRACER_API FDTRunState
{
public:
	void Start(int32 RunSeed, int32 NumPlayers);

	int32 GetNumPlayers() const { return Players.Num(); }
	bool IsValidPlayer(int32 PlayerIndex) const { return Players.IsValidIndex(PlayerIndex); }
	FDTPlayerRunState& GetPlayer(int32 PlayerIndex) { return Players[PlayerIndex]; }
	const FDTPlayerRunState& GetPlayer(int32 PlayerIndex) const { return Players[PlayerIndex]; }
	FDTRngStreams& GetRng() { return Rng; }

	/** Routes an event to its owner. Returns false (and changes nothing) when the owner is not a player of this run. */
	bool ApplyEvent(const FDTGameEvent& Event);

	/** Installs (or clears with nullptr) the observer that sees every ApplyEvent. Survives Start(); not owned. */
	void SetEventSink(IDTRunEventSink* InSink) { Sink = InSink; }
	IDTRunEventSink* GetEventSink() const { return Sink; }

private:
	FDTRngStreams Rng;
	TArray<FDTPlayerRunState> Players;
	IDTRunEventSink* Sink = nullptr;
};
