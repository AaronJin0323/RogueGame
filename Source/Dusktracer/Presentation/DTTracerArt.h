// Tracer art (02_Systems/tracer_art.md): where a round's head, tail, trail and muzzle flash are drawn, which tokens they take, and how
// player fire dims with density. Pure functions of sim state the callers read; nothing here writes to the sim (04_Presentation R-PRE-11).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"

class UInstancedStaticMeshComponent;
class UWorld;

namespace DTTracerArt
{
	/** Trail slots: one per Path, then the neutral trail (R-TRC-03). */
	constexpr int32 NeutralSlot = static_cast<int32>(EDTPath::Count);
	constexpr int32 NumSlots = NeutralSlot + 1;
	/** Custom data of a tail instance (M_Tracer_Tail): 0-2 body RGB, 3 strength, 4-6 tip RGB. */
	constexpr int32 TailData = 7;
	/** Custom data of an enemy trail or muzzle flash instance (M_EnemyRound_Trail): 0-2 RGB, 3 emissive, 4 opacity. */
	constexpr int32 EnemyTrailData = 5;
	/** Side in uu of /Engine/BasicShapes/Plane, the mesh of every tail, trail and flash; the renderers warn when the loaded mesh differs. */
	constexpr float PlaneSizeUU = 100.0f;

	// ---- tokens (R-TRC-01, R-TRC-03) ----------------------------------------------------------------------------------------

	/** The head of every player round, whatever the weapon and whatever the player holds: Pal.Player.Core. */
	DUSKTRACER_API FColor HeadToken(FName WeaponId, const FDTKeeperRunState& Keepers);
	/** Slot of the tail's body: the primary Path once one of its blessings is owned, the neutral slot before. */
	DUSKTRACER_API int32 BodySlot(const FDTKeeperRunState& Keepers);
	/** Slot of the tail's tip: the second Path once the commitment is made, else the body's slot. */
	DUSKTRACER_API int32 TipSlot(const FDTKeeperRunState& Keepers);
	/** Pal.Keeper.<Path> for a Path's slot, Pal.Player.TrailNeutral for the neutral slot. */
	DUSKTRACER_API FColor SlotToken(int32 Slot);

	// ---- density (R-TRC-06, R-PRE-06) ---------------------------------------------------------------------------------------

	/** 0 at or under C.TRC.DIM_START_ROUNDS live player rounds, 1 at or over C.TRC.DIM_FULL_ROUNDS, linear between. */
	DUSKTRACER_API float Density(int32 LivePlayerRounds);
	/** Head emissive over C.BULLETS.EMISSIVE_PLAYER: 1 undimmed, C.TRC.DIM_EMISSIVE_MIN fully dimmed. */
	DUSKTRACER_API float HeadDim(int32 LivePlayerRounds);
	/** Tail strength over C.TRC.TAIL_EMISSIVE: 1 undimmed, C.TRC.DIM_TAIL_MIN fully dimmed. */
	DUSKTRACER_API float TailDim(int32 LivePlayerRounds);
	/** Tail width over its undimmed width: 1 undimmed, C.TRC.DIM_WIDTH_MIN fully dimmed. */
	DUSKTRACER_API float TailWidthDim(int32 LivePlayerRounds);

	// ---- shapes (R-TRC-01, R-TRC-02, R-TRC-04, R-TRC-07) --------------------------------------------------------------------
	// Step is the round's travel over the last fixed step (position minus previous position); At is where it is drawn.
	// InvMesh is one over the mesh's half-width in uu (heads, a sphere); InvPlane is one over the plane's full side in uu.

	/** A player head: C.TRC.HEAD_WIDTH_RATIO of the hit radius wide, stretched by C.TRC.HEAD_STRETCH along its flight; a round at rest is round. */
	DUSKTRACER_API FTransform Head(const FVector2f& At, const FVector2f& Step, float HitRadius, float InvMesh, double Z);
	/** Length in uu of a player tail: C.TRC.TAIL_S of flight, within C.TRC.TAIL_MIN_RATIO head half-widths and C.TRC.TAIL_MAX_UU. */
	DUSKTRACER_API float TailLength(float SpeedUUPerS, float HitRadius);
	/** A player tail from the head's centre back along the flight, the plane's +X at the head. False for a round at rest. */
	DUSKTRACER_API bool Tail(const FVector2f& At, const FVector2f& Step, float HitRadius, float WidthDim, float InvPlane, double Z, FTransform& Out);
	/** An enemy round's trail: C.TRC.ENEMY_TRAIL_S of flight behind the drawn round, from its centre back along its velocity. False for a round that flies less than its drawn radius in that time. */
	DUSKTRACER_API bool EnemyTrail(const FVector2f& At, const FVector2f& Velocity, float HitRadius, float InvPlane, double Z, FTransform& Out);
	/** Seconds since an enemy round was fired, from how far it has flown; negative for a round with no velocity. */
	DUSKTRACER_API float RoundAge(const FVector2f& Origin, const FVector2f& At, const FVector2f& Velocity);
	/** The flash of a round just fired: from the firing hull's edge (C.ROLE.TURRET_RADIUS_UU down the flight from the firing point), pointing down the flight and shrinking over C.TRC.MUZZLE_S. False once it is over. */
	DUSKTRACER_API bool MuzzleFlash(const FVector2f& Origin, const FVector2f& At, const FVector2f& Velocity, float HitRadius, float InvPlane, double Z, FTransform& Out);

	// ---- the hero's clear zone (R-TRC-01, R-TRC-02, D-068) ------------------------------------------------------------------

	/**
	 * Where a switch outside Shipping keeps its value: negative uses C.TRC.CLEAR_ZONE_RATIO (the default). Set by dt.Tracer.ClearZone and,
	 * at start, by the environment variable DT_TRACER_CLEAR_ZONE (Debug/DTDebugTracer.cpp); nothing in Shipping sets it.
	 */
	DUSKTRACER_API float& ClearZoneSetting();
	/** The ratio in force, in hero radii: the switch's value when it is zero or more, else C.TRC.CLEAR_ZONE_RATIO. Zero draws every round. */
	DUSKTRACER_API float ClearZoneRatio();

	/**
	 * The hero's clear zone for one frame: a disc of the ratio times C.HERO.RADIUS_UU round each hero's drawn position. A writer builds it
	 * once per frame and asks Hides for each round inside the loop that already writes its instance: one squared distance per round.
	 * Draw only: it reads the heroes and writes nothing.
	 */
	struct DUSKTRACER_API FClearZone
	{
		FClearZone() = default;
		/** Heroes by player index, as drawn; Ratio in hero radii (zero or less: the zone is off). */
		FClearZone(TConstArrayView<FVector2f> Heroes, float Ratio);
		/** The zone of World's heroes at the interpolation Alpha, at ClearZoneRatio(). Off when the world has no hero system. */
		static FClearZone OfWorld(const UWorld* World, float Alpha);

		bool IsOn() const { return RadiusSq > 0.0f && Heroes.Num() > 0; }
		/** True for a round of Owner drawn at At inside its hero's zone; a round whose owner is no hero is inside any hero's zone. Stress fire is player 0's, so the hero's. */
		bool Hides(int32 Owner, const FVector2f& At) const
		{
			if (RadiusSq <= 0.0f)
			{
				return false;
			}
			if (Heroes.IsValidIndex(Owner))
			{
				return FVector2f::DistSquared(At, Heroes[Owner]) < RadiusSq;
			}
			for (const FVector2f& Hero : Heroes)
			{
				if (FVector2f::DistSquared(At, Hero) < RadiusSq)
				{
					return true;
				}
			}
			return false;
		}

	private:
		float RadiusSq = 0.0f;
		TArray<FVector2f, TInlineAllocator<4>> Heroes;
	};

	// ---- instances ----------------------------------------------------------------------------------------------------------

	/**
	 * Makes Part's instance count equal Transforms.Num() (adding or removing only the tail) and batch-writes every transform.
	 * A new instance takes Row as its custom data (Part->NumCustomDataFloats floats); live instances keep theirs.
	 */
	DUSKTRACER_API void SyncInstances(UInstancedStaticMeshComponent* Part, TConstArrayView<FTransform> Transforms, TConstArrayView<float> Row);
}
