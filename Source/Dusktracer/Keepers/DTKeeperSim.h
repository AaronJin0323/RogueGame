// The Keeper layer's step (02_Systems/keepers.md): rebuilds each player's Keeper flag sources on the weapon seam
// (R-KEE-03, R-KEE-07), rolls the enchantment on the step's player hits (R-KEE-09, R-KEE-10), expresses the Keeper
// parameters the weapon system has no mechanic for (Frost's frozen_vuln), and runs Intercession casts (R-KEE-14).
// Plain C++ (no UObject): tests drive it with a bare FDTRunState, FDTWorldView, FDTEnemyPool and FDTWeaponSim.
//
// Phases (07_Technical §4): CaptureHits in EDTSimPhase::Bullets after the bullet pool (it reads the hits ResolveHits is
// about to consume); StepEffects in EDTSimPhase::Effects (status procs, Intercession hits, pulls and freezes, all
// consumed next step). Rolls draw only from the Combat stream (R-TEC-02).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Keepers/DTKeeperSpec.h"
#include "Telemetry/DTTelemetryRecords.h"

class FDTEnemyPool;
class FDTWeaponSim;

/** Per player counters (DT.Keeper.Stats, tests). Monotonic within a run. */
struct FDTKeeperCounters
{
	/** Enchantment procs by status, primary and second (R-KEE-09). */
	int64 PrimaryProcs[static_cast<int32>(EDTStatus::Count)] = {};
	int64 SecondProcs[static_cast<int32>(EDTStatus::Count)] = {};
	/** Player hits the enchantment rolled on. */
	int64 HitsRolled = 0;
	int64 FrozenVulnHits = 0;
	int32 Casts = 0;
	int32 Rebuilds = 0;
};

/** A finished cast for telemetry (intercession{owner, base, variant, layer, enemies_hit, damage}). */
struct FDTKeeperCastRecord
{
	int32 OwnerId = INDEX_NONE;
	FDTTelIntercession Record;
};

class DUSKTRACER_API FDTKeeperSim
{
public:
	// --- flag sources (R-KEE-03, R-KEE-07) ---------------------------------------------------------------------------
	/** Replaces every Keeper flag source of OwnerId on Weapons with the current state (one source per Keeper; none when empty). */
	void RebuildFlags(const FDTRunState& Run, int32 OwnerId, FDTWeaponSim& Weapons);
	/** The flags the last rebuild handed to the weapon seam for Path (empty when none). */
	const FDTWeaponFlags& GetBuiltFlags(int32 OwnerId, EDTPath Path) const;

	// --- Intercession (R-KEE-14) ----------------------------------------------------------------------------------------
	/** Queues a cast for OwnerId; it is composed and started in the next StepEffects (read at cast time). */
	void QueueIntercession(int32 OwnerId);
	bool IsCasting(int32 OwnerId) const;
	/** The plan of OwnerId's running cast (empty plan when none). */
	const FDTIntercessionPlan& GetActivePlan(int32 OwnerId) const;
	/** Seconds left of OwnerId's cast that scales enemy time (R-KEE-37), and its whole length; 0 with none running. */
	float GetDilationLeft(int32 OwnerId) const;
	float GetDilationTotal(int32 OwnerId) const;

	// --- the step --------------------------------------------------------------------------------------------------------
	/** EDTSimPhase::Bullets, after the pool stepped: copies the player hits ResolveHits will apply (Enemies: frozen state, optional). */
	void CaptureHits(const FDTSimContext& Context, const FDTEnemyPool* Enemies);
	/** EDTSimPhase::Effects: enchantment procs, frozen_vuln, Intercession. Weapons gets flag rebuilds and reads damage. */
	void StepEffects(const FDTSimContext& Context, FDTWeaponSim* Weapons);

	/** Casts that ended since the last call (the subsystem forwards them to telemetry). */
	TArray<FDTKeeperCastRecord> TakeFinishedCasts() { return MoveTemp(Finished); }
	/** Players whose dash cooldown resets now (Slam's dash_cooldown_reset; the subsystem calls the hero, resolved next Hero phase). */
	TArray<int32> TakeDashResets() { return MoveTemp(DashResets); }

	/** Drops transient state (captured hits, running casts); keeps counters until ResetRun. */
	void ResetRun();

	const FDTKeeperCounters& GetCounters(int32 OwnerId) const;

	/** What Keeper flags ask of the hero (keepers.md R-KEE-34): the subsystem writes them into FDTHeroStats. */
	struct FHeroRiders
	{
		float DashCooldownMul = 1.0f;
		int32 DashCharges = 0;
	};
	FHeroRiders GetHeroRiders(int32 OwnerId) const;

private:
	struct FCapturedHit
	{
		int32 OwnerId = INDEX_NONE;
		uint32 EnemySerial = 0;
		float Damage = 0.0f;
		FVector2f Position = FVector2f::ZeroVector;
		bool bFrozen = false;
	};

	struct FCast
	{
		bool bActive = false;
		FDTIntercessionPlan Plan;
		float TimeLeft = 0.0f;
		/** The window's whole length, and what it scales enemy time to (R-KEE-37); 1 = not at all. */
		float TimeTotal = 0.0f;
		float TimeScale = 1.0f;
		float RingTimer = 0.0f;
		float NetTimer = 0.0f;
		float LayerTimer = 0.0f;
		int32 LayerChargeSteps = 0;
		bool bWasIntangible = false;
		/** Sun Lance: charge, beams to run, the beam running (R-KEE-35). */
		float ChargeLeft = 0.0f;
		float BeamLeft = 0.0f;
		float BeamTimer = 0.0f;
		int32 BeamsLeft = 0;
		FVector2f BeamDir = FVector2f(1.0f, 0.0f);
		TSet<uint32> EnemiesHit;
		double Damage = 0.0;
	};

	struct FPlayer
	{
		FDTWeaponFlags Built[static_cast<int32>(EDTPath::Count)];
		TArray<FDTSpecClause> Overlay[static_cast<int32>(EDTPath::Count)];
		FCast Cast;
		bool bQueued = false;
		FDTKeeperCounters Counters;
	};

	FPlayer& EnsurePlayer(int32 OwnerId);
	void BuildSerialIndex(const FDTWorldView& View);
	int32 IndexOfSerial(uint32 Serial) const;

	void Enchant(const FDTSimContext& Context, FPlayer& Player, const FCapturedHit& Hit, int32 Enemy);
	/** One status application with the owner's Keeper riders (R-KEE-11 Burn magnitude and stack bonus, R-KEE-12 Freeze). */
	void AppendStatus(FDTWorldView& View, FPlayer& Player, EDTStatus Status, int32 Enemy, uint32 Serial, float HitDamage,
		int32 OwnerId, EDTPath Path, float Stacks = 1.0f);

	void StartCast(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, FDTWeaponSim* Weapons);
	void StepCast(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, FDTWeaponSim* Weapons);
	void EndCast(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, FDTWeaponSim* Weapons);
	/** Hits every enemy within Radius of Center for Damage as Path's Keeper; returns enemies hit. */
	int32 HitCircle(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, const FVector2f& Center, float Radius,
		float Damage, EDTPath Path, float KnockbackUu, bool bBurn, int32 BurnApplications);
	void ApplyLayerStatus(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, int32 Enemy, float HitDamage);
	void TidePull(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player);
	/** on_cast hunters, paydays, Debt, Momentum of a variant or layer (R-KEE-35). */
	void ApplyCastGrants(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, FDTWeaponSim& Weapons, const FDTEffectSpec& Spec, EDTPath Path);
	void StepBeam(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, FDTWeaponSim* Weapons);
	void HitBeam(const FDTSimContext& Context, int32 OwnerId, FPlayer& Player, const FVector2f& Origin, const FVector2f& Dir, float Length, float Damage, FDTWeaponSim* Weapons);
	/** The strongest per-hit damage among OwnerId's weapons (Slam's dmg_ratio basis, keepers.md Data). */
	float StrongestDamage(const FDTSimContext& Context, int32 OwnerId, const FDTWeaponSim* Weapons) const;

	TArray<FPlayer> Players;
	TArray<FCapturedHit> Captured;
	TMap<uint32, int32> SerialToIndex;
	TArray<FDTKeeperCastRecord> Finished;
	TArray<int32> DashResets;
	int64 LastStep = -1;
};
