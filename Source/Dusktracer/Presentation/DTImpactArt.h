// Impact art (02_Systems/impact_art.md R-IMP-03 to R-IMP-11): what a sim step raises from the hit and kill tap, the density tier rule, the
// aggregation of kills, each effect's shape over its age and the hash its variation comes from. Pure C++ over what the callers read from the
// sim; nothing here writes to the sim or draws from a sim stream (04_Presentation R-PRE-11). FDTImpactEffects holds the live effects in
// fixed slots so the renderer writes an instance once, when its effect is born.
#pragma once

#include "CoreMinimal.h"
#include "Enemies/DTEnemyPool.h"

/** The density tier of a step (R-IMP-08, R-IMP-11; D-056 ruling 5): the one tier the eye and the ear read. */
enum class EDTDensityTier : uint8
{
	PerKill,
	Crowd,
	Flood
};

namespace DTImpactArt
{
	/** Custom data of a spark instance (M_Fx_Spark): 0-2 linear RGB, 3 emissive at birth, 4 birth in seconds of sim time. */
	constexpr int32 SparkData = 5;
	/**
	 * Custom data of a flash instance (M_Fx_Flash): 0-2 linear RGB, 3 emissive at birth, 4 birth in seconds, 5 disc radius at birth and
	 * 6 at the end of its life, both over the plane's half side (never over 1 drawn), 7 fade: 1 grows and fades, 0 a steady mark (R-IMP-09).
	 */
	constexpr int32 FlashData = 8;
	/** Custom data of a hero ring (M_Fx_Ring): 0-2 linear RGB, 3 emissive at birth, 4 birth in seconds, 5 outer radius at birth over the plane's half side, 6 thickness over the half side, 7 fade. */
	constexpr int32 RingData = 8;
	/** Custom data of a debris piece (M_Fx_Debris): 0-2 linear RGB, lit, no emissive. */
	constexpr int32 DebrisData = 3;
	/** The three shard meshes (SM_Fx_Debris_A, _B, _C). */
	constexpr int32 NumShards = 3;
	/** Side in uu of /Engine/BasicShapes/Plane, the mesh of sparks, flashes and rings. */
	constexpr float PlaneSizeUU = 100.0f;

	// ---- the tier (R-IMP-08, R-IMP-11) ---------------------------------------------------------------------------------------

	/**
	 * The tier after a step judged on KillsPerSecond, from the tier before it. PerKill enters Crowd above C.PRE.DENSITY_LOD_KILLS_PER_S; Crowd
	 * leaves under C.IMP.DENSITY_CROWD_OFF_KILLS_PER_S and enters Flood above C.IMP.DENSITY_FLOOD_KILLS_PER_S; Flood leaves under
	 * C.IMP.DENSITY_FLOOD_OFF_KILLS_PER_S. A rate across two thresholds moves two tiers in one step. Pure: fold it over a trace to test a reader.
	 */
	DUSKTRACER_API EDTDensityTier NextTier(EDTDensityTier Tier, float KillsPerSecond);
	DUSKTRACER_API const TCHAR* TierName(EDTDensityTier Tier);

	// ---- variation (R-IMP-03) ------------------------------------------------------------------------------------------------

	/** A hash of an enemy's serial, a step and an index: the only source of an effect's variation. No stream, no state. */
	DUSKTRACER_API uint32 Hash(uint32 Serial, int64 Step, int32 Index);
	/** Hash mapped to [0, 1). */
	DUSKTRACER_API float Hash01(uint32 Serial, int64 Step, int32 Index);

	// ---- sim time (R-IMP-03) -------------------------------------------------------------------------------------------------

	/** Seconds of sim time a frame draws: StepsRun steps have run (UDTSimSubsystem::GetStepIndex) and the frame is Alpha of the way from the one before the last; a held step draws Alpha 1. */
	DUSKTRACER_API double NowSeconds(int64 StepsRun, float Alpha);
	/** An effect's age at Now: seconds since its birth step, never under 0. */
	DUSKTRACER_API float AgeSeconds(int64 Born, double Now);

	// ---- shapes over age (R-IMP-05, R-IMP-06, R-IMP-07, R-IMP-09) ------------------------------------------------------------
	// Age is seconds since the effect's birth step, in sim time. Each shape here is what the material draws from the same numbers.

	/** Which way a spark opens: from the hull's centre through the contact point; from the owner's hero through it when the two coincide; +X with neither. */
	DUSKTRACER_API FVector2f SparkDirection(const FVector2f& Contact, const FVector2f& HullCentre, const FVector2f* OwnerHero);

	struct FDTSparkShape
	{
		float Length = 0.0f;   // uu, along its direction
		float Width = 0.0f;    // uu, across
		float StreakRoot = 0.0f; // uu: each streak's width where it leaves the contact point, C.IMP.SPARK_STREAK_RATIO of Width
		float Emissive = 0.0f; // times Pal.Player.Core
		bool bAlive = false;
	};
	/** C.IMP.SPARK_START_RATIO to all of C.IMP.SPARK_LENGTH_UU by C.IMP.SPARK_WIDTH_UU (each streak C.IMP.SPARK_STREAK_RATIO of the width at its root), C.IMP.SPARK_EMISSIVE to nothing, over C.IMP.SPARK_S. */
	DUSKTRACER_API FDTSparkShape SparkAt(float Age);

	/** A hull's drawn radius: its hit radius, times C.ELITE.DRAW_SCALE for an elite. */
	DUSKTRACER_API float DrawnRadius(float HitRadius, bool bElite);

	struct FDTFlashShape
	{
		float Radius = 0.0f;   // uu
		float Emissive = 0.0f; // times Pal.Player.Core
		bool bAlive = false;
	};
	/**
	 * A kill flash of a hull of Drawn radius: C.IMP.KILL_FLASH_START_RATIO to C.IMP.KILL_FLASH_END_RATIO of it, never over C.IMP.FLASH_RADIUS_MAX_UU,
	 * C.IMP.KILL_FLASH_EMISSIVE to nothing over C.IMP.KILL_FLASH_S. With Screen flashes off: its final size and C.IMP.REDUCED_EMISSIVE_RATIO of its
	 * emissive for its whole life.
	 */
	DUSKTRACER_API FDTFlashShape FlashAt(float Drawn, float Age, bool bAllowFlash);

	/** Pieces a kill of Drawn radius throws: one per C.IMP.DEBRIS_RADIUS_PER_PIECE_UU, rounded, within C.IMP.DEBRIS_PIECES_MIN and _MAX. */
	DUSKTRACER_API int32 DebrisPieces(float Drawn);
	/** The blow: from the contact point through the hull's centre; a hashed direction when the two coincide (a Burn tick, a field). */
	DUSKTRACER_API FVector2f BlowDirection(const FVector2f& Contact, const FVector2f& HullCentre, uint32 Serial, int64 Step);

	/** What a piece is given at birth; everything after is a closed form of its age. */
	struct FDTPiece
	{
		FVector2f Origin = FVector2f::ZeroVector; // the hull's centre
		FVector2f Dir = FVector2f(1.0f, 0.0f);    // unit, inside C.IMP.DEBRIS_SPREAD_DEG of the blow
		float Speed = 0.0f;                       // uu/s at birth
		float Yaw = 0.0f;                         // radians at birth
		float Spin = 1.0f;                        // +1 or -1: which way it turns
		int32 Shard = 0;                          // which of the three meshes
	};
	/** Piece Index of the kill of Serial on Step, thrown along Blow from Origin. */
	DUSKTRACER_API FDTPiece MakePiece(uint32 Serial, int64 Step, int32 Index, const FVector2f& Origin, const FVector2f& Blow);

	struct FDTPieceAt
	{
		FVector2f Position = FVector2f::ZeroVector;
		float Yaw = 0.0f;   // radians
		float Scale = 0.0f; // 1 at full size, 0 gone
		bool bAlive = false;
	};
	/**
	 * Where a piece is at Age: it slows with the time constant C.IMP.DEBRIS_DRAG_S (distance Speed x Drag x (1 - e^(-Age/Drag)), never past
	 * Speed x Drag), turns by the angle it would roll through on its own size, shrinks to nothing over the last C.IMP.DEBRIS_SHRINK_SHARE of
	 * C.IMP.DEBRIS_S and is gone after it.
	 */
	DUSKTRACER_API FDTPieceAt PieceAt(const FDTPiece& Piece, float Age);

	struct FDTRingShape
	{
		float Outer = 0.0f;    // uu
		float Thick = 0.0f;    // uu
		float Emissive = 0.0f; // times Pal.Enemy.Muzzle
		bool bAlive = false;
	};
	/** The hero ring: outer radius C.IMP.HERO_FLASH_START_RATIO to _END_RATIO of C.HERO.RADIUS_UU, C.IMP.HERO_FLASH_THICK_UU thick, fading over C.IMP.HERO_FLASH_S. */
	DUSKTRACER_API FDTRingShape RingAt(float Age, bool bAllowFlash);

	// ---- aggregation (R-IMP-08) ----------------------------------------------------------------------------------------------

	struct FDTAggregate
	{
		FVector2f Centre = FVector2f::ZeroVector; // the centre of its cell's kills
		float Drawn = 0.0f;                       // the root of the summed squares of their drawn radii
		int32 Count = 0;
		int32 First = 0;                          // order of its first kill, for a stable choice among equal cells
		FIntPoint Cell = FIntPoint::ZeroValue;
	};
	/**
	 * Groups kills (positions and drawn radii) by a floor grid of CellUU and keeps the Cap fullest cells, fullest first, equal ones in the
	 * order of their first kill. Out is reset, its storage kept.
	 */
	DUSKTRACER_API void Aggregate(TConstArrayView<FVector2f> Positions, TConstArrayView<float> Drawn, float CellUU, int32 Cap, TArray<FDTAggregate>& Out);

	// ---- the step's own counts (R-IMP-03) ------------------------------------------------------------------------------------

	/** A counted hit: neither a field tick nor a status tick, and not the hit that killed. */
	DUSKTRACER_API bool IsCountedHit(const FDTImpact& Impact);
	DUSKTRACER_API bool IsKill(const FDTImpact& Impact);
}

/** One live spark, flash or ring: where it was born and how; its age is read against the step it was born on. */
struct FDTFxSpark
{
	FVector2f Contact = FVector2f::ZeroVector;
	FVector2f Dir = FVector2f(1.0f, 0.0f);
	int64 Born = 0;
};

struct FDTFxFlash
{
	FVector2f Centre = FVector2f::ZeroVector;
	float Drawn = 0.0f;
	bool bAllowFlash = true;
	int64 Born = 0;
};

struct FDTFxPiece
{
	DTImpactArt::FDTPiece Piece;
	int64 Born = 0;
};

struct FDTFxRing
{
	bool bAllowFlash = true;
	int64 Born = 0;
};

/** Effects held in fixed slots: a birth takes a free slot and is listed once, a death frees it and is listed once, so a renderer writes only those. */
template <typename T>
struct TDTFxSlots
{
	TArray<T> Items;
	TArray<uint8> Live;
	TArray<int32> Free;
	TArray<int32> Born; // slots born since the renderer last drained
	TArray<int32> Died; // slots freed since the renderer last drained
	int32 NumLive = 0;
	int32 Cap = 0;

	void Init(int32 InCap)
	{
		Cap = InCap;
		Items.Reset();
		Items.Reserve(InCap);
		Live.Reset();
		Live.Reserve(InCap);
		Free.Reset();
		Free.Reserve(InCap);
		Born.Reset();
		Born.Reserve(InCap);
		Died.Reset();
		Died.Reserve(InCap);
		NumLive = 0;
	}

	/** A slot for a new effect, or INDEX_NONE at the cap (the effect is not raised). */
	int32 Add(const T& Item)
	{
		if (NumLive >= Cap)
		{
			return INDEX_NONE;
		}
		int32 Slot;
		if (Free.Num() > 0)
		{
			Slot = Free.Pop(EAllowShrinking::No);
			Items[Slot] = Item;
			Live[Slot] = 1;
		}
		else
		{
			Slot = Items.Add(Item);
			Live.Add(1);
		}
		++NumLive;
		Born.Add(Slot);
		return Slot;
	}

	void Remove(int32 Slot)
	{
		if (Live.IsValidIndex(Slot) && Live[Slot])
		{
			Live[Slot] = 0;
			--NumLive;
			Free.Add(Slot);
			Died.Add(Slot);
		}
	}

	void Clear()
	{
		for (int32 Slot = 0; Slot < Live.Num(); ++Slot)
		{
			Remove(Slot);
		}
	}

	bool IsLive(int32 Slot) const { return Live.IsValidIndex(Slot) && Live[Slot] != 0; }
	int32 NumSlots() const { return Items.Num(); }
};

/** What one step gives the reader (R-IMP-03): the tap's records, the hurt heroes and where the heroes stand. */
struct FDTImpactStepInput
{
	int64 Step = 0;
	TConstArrayView<FDTImpact> Impacts;
	TConstArrayView<bool> HeroHurt;
	TConstArrayView<FVector2f> HeroPositions;
	EDTDensityTier Tier = EDTDensityTier::PerKill;
	bool bAllowFlash = true;
};

/** Counts of one fed step. */
struct FDTImpactStepCounts
{
	int32 CountedHits = 0;
	int32 Kills = 0;
	int32 Sparks = 0;
	int32 Flashes = 0;
	int32 Pieces = 0;
	int32 Rings = 0;
};

/**
 * The live effects of one world (R-IMP-03 to R-IMP-09). Plain C++, so tests drive it without a world. Feed is called once per sim step with
 * that step's input; it first ends what is over, then raises what the step raises in its tier, under every live cap.
 */
class DUSKTRACER_API FDTImpactEffects
{
public:
	FDTImpactEffects();

	/** Empties every slot (listing each live effect as died, so a renderer clears it) and the counters. */
	void Reset();
	/** Ends the effects whose life is over by the start of Step: an effect born on B is over once Step - 1 - B steps pass its life. */
	void Expire(int64 Step);
	/** Raises what Input's step raises (after Expire(Input.Step)). Returns its counts. */
	FDTImpactStepCounts Feed(const FDTImpactStepInput& Input);
	/** Ends every live effect now (dt.Impact.Draw 0). */
	void ClearAll();

	TDTFxSlots<FDTFxSpark> Sparks;
	TDTFxSlots<FDTFxFlash> Flashes;
	TDTFxSlots<FDTFxPiece> Pieces;
	/** One per player slot; its slot index is the player's. */
	TArray<FDTFxRing> Rings;
	TArray<uint8> RingLive;
	TArray<int32> RingsBorn;
	TArray<int32> RingsDied;
	int32 NumRingsLive() const;

	/** Effects not raised because a live cap was full, since the last Reset. */
	int64 RefusedSparks = 0;
	int64 RefusedFlashes = 0;
	int64 RefusedPieces = 0;

private:
	void RaiseFlash(const FVector2f& Centre, float Drawn, bool bAllowFlash, int64 Step, FDTImpactStepCounts& Counts);
	TArray<uint32> SparkSerials;
	TArray<FVector2f> KillPositions;
	TArray<float> KillDrawn;
	TArray<DTImpactArt::FDTAggregate> Cells;
};
