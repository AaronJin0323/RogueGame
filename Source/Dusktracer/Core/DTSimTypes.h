// Shared data passed between simulation systems within one fixed step (07_Technical §4).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSpatialHash.h"

class FDTRunState;

/** Order of systems inside one fixed step. A system registers for exactly one phase. */
enum class EDTSimPhase : uint8
{
	Input,
	Hero,
	Enemies,     // move enemies, publish positions + hash into FDTWorldView
	Weapons,     // spawn bullets
	Bullets,     // integrate bullets, collide against the enemy hash, append FDTHit
	ResolveHits, // apply hits: damage, deaths -> kill events (owner-tagged, R-TEC-01)
	Effects,
	Director,
	Count
};

/** One bullet touching one enemy this step. Produced by the bullet system, consumed in ResolveHits. */
struct FDTHit
{
	int32 EnemyIndex = INDEX_NONE;
	int32 OwnerId = INDEX_NONE;
	float Damage = 0.0f;
	FVector2f Position = FVector2f::ZeroVector;
};

/** What systems publish for each other during a step. Owned by UDTSimSubsystem; reset per step where noted. */
struct FDTWorldView
{
	// Published by the enemy system in EDTSimPhase::Enemies (valid for the rest of the step).
	TArray<FVector2f> EnemyPositions;
	TArray<float> EnemyRadii;
	FDTSpatialHash EnemyHash;
	// Appended by the bullet system, consumed and cleared in ResolveHits.
	TArray<FDTHit> Hits;
	// Hero positions by player index (written in EDTSimPhase::Hero; stress maps leave a single point at the origin).
	TArray<FVector2f> HeroPositions;
};

/** Everything a system gets when it steps. */
struct FDTSimContext
{
	float StepSeconds = 0.0f;
	int64 StepIndex = 0;
	FDTRunState* Run = nullptr;
	FDTWorldView* View = nullptr;
};

/** A simulation system. Implemented by world subsystems that register with UDTSimSubsystem in OnWorldBeginPlay. */
class IDTSimSystem
{
public:
	virtual ~IDTSimSystem() = default;
	virtual void SimStep(const FDTSimContext& Context) = 0;
	/** Called once per rendered frame after the steps; Alpha in [0, 1) interpolates between the last two steps. */
	virtual void SimRender(float Alpha) {}
};
