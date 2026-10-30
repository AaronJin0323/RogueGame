// Struct-of-arrays enemy simulation (02_Systems/enemies.md, 07_Technical §4). Plain C++ so tests drive it directly.
#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTSimTypes.h"
#include "Core/DTSpatialHash.h"
#include "Enemies/DTEnemyBulletPool.h"
#include "Enemies/DTEnemyTypes.h"

class FDTRunState;

/** Kills per second over a sliding window of fixed steps (density LOD input, 04_Presentation R-PRE-06). */
class DUSKTRACER_API FDTKillRateCounter
{
public:
	void Reset();
	/** Records the kills of one step. StepSeconds sizes the window on first use. */
	void RecordStep(int32 Kills, float StepSeconds);
	float GetKillsPerSecond() const;

private:
	TArray<int32> Ring;
	int32 Cursor = 0;
	int32 WindowSum = 0;
	float WindowSeconds = 0.0f;
};

/** One Burn stack (R-ENM-21): its own timer, damage per second, owner and source. Left <= 0 = empty slot. */
struct FDTBurnStack
{
	float Left = 0.0f;
	float Dps = 0.0f;
	float Accum = 0.0f; // seconds burnt since the last tick
	int32 OwnerId = INDEX_NONE;
	uint16 SourceId = 0;
	uint32 Order = 0; // application order; the smallest live stack is the oldest
};

/** Role state machine and status state of one enemy (enemies.md Data). Cold per-enemy data, swapped with the slot. */
struct FDTEnemyAux
{
	// Role machine (R-ENM-10..12, R-ENM-18).
	EDTEnemyState State = EDTEnemyState::Init;
	float StateLeft = 0.0f;   // seconds left in WindUp, Recover, Tell; gap to the next volley in Fire
	float Cooldown = 0.0f;    // Rusher: charge cooldown; Turret: time until the next tell may start
	FVector2f AimDir = FVector2f(1.0f, 0.0f); // Rusher: locked charge direction; Turret: aim locked at the end of the tell
	FVector2f ChargeOrigin = FVector2f::ZeroVector;
	float ChargeLength = 0.0f; // band length = charge travel (clipped to the arena)
	float ChargeLeft = 0.0f;   // uu still to travel in this charge
	EDTFirePattern Pattern = EDTFirePattern::Burst;
	bool bCloseFan = false;    // R-ENM-19: decided at the end of the tell
	int32 VolleysLeft = 0;
	int32 VolleyIndex = 0;
	int32 TargetPlayer = INDEX_NONE;
	bool bToken = false;
	bool bFacingSet = false;   // false until the first step (or a test) sets the facing
	bool bArrivalAttack = false; // a drop-in Rusher starts its wind-up on its first step
	int64 LastFireStep = -1;   // aim-token order (R-ENM-18); never fired = -1, arrivals first

	// Statuses (R-ENM-21, R-ENM-22).
	float ShockLeft = 0.0f;
	int32 ShockOwner = INDEX_NONE;
	uint16 ShockSource = 0;
	FDTBurnStack Burn[DTConst::STATUS_BURN_STACK_SLOTS];
	int32 BurnCap = DTConst::STATUS_BURN_STACKS_MAX;
	int32 ChillStacks = 0;
	float ChillLeft = 0.0f;
	int32 ChillOwner = INDEX_NONE;
	uint16 ChillSource = 0;
	float FreezeLeft = 0.0f;
	int32 FreezeOwner = INDEX_NONE;
	uint16 FreezeSource = 0;

	// Elite (elites.md Data).
	uint8 AffixMask = 0;
	int32 ShieldHitsLeft = 0;
	int32 ShieldAbsorbed = 0;
	int32 DrainTarget = INDEX_NONE; // the hero on the cable
	float DrainLinkLeft = 0.0f;     // seconds until the cable drains
	float DrainTickLeft = 0.0f;     // seconds to the next drain, once linked
	float Drained = 0.0f;
	float Healed = 0.0f;
	float AliveSeconds = 0.0f;

	bool IsFrozen() const { return FreezeLeft > 0.0f; }
	int32 NumBurnStacks() const;
};

/** What a death left behind for the effects of this step (keepers.md: burning explosions, spread): the status mask is gone with the slot. */
struct FDTEnemyDeath
{
	uint32 Serial = 0;
	uint8 StatusMask = 0;
	/** Strongest live Burn stack, damage per second. */
	float BurnDps = 0.0f;
};

/** An arrival's attack that outlives its unit (R-ENM-13, D-013 §6): resolves from its telegraph at the step it lands. */
struct FDTOrphanAttack
{
	EDTEnemyRole Role = EDTEnemyRole::Rusher;
	FVector2f Origin = FVector2f::ZeroVector;
	FVector2f Dir = FVector2f(1.0f, 0.0f);
	float Length = 0.0f;
	float Width = 0.0f;
	float Left = 0.0f;  // seconds until the attack lands
	float Total = 0.0f; // telegraph duration (Progress)
	float DmgScale = 1.0f;
	float Damage = 0.0f; // Rusher band: contact damage already scaled
	float GapLeft = 0.0f;
	EDTFirePattern Pattern = EDTFirePattern::Burst;
	int32 TargetPlayer = INDEX_NONE;
	int32 VolleysLeft = 0;
	int32 VolleyIndex = 0;
	bool bCloseFan = false;
	bool bLanded = false;
	uint32 EnemySerial = 0;
};

/** A Volatile elite's blast waiting to land (elites.md R-ELT-05); outlives its unit, like an orphan attack. */
struct FDTEliteBlast
{
	FVector2f Origin = FVector2f::ZeroVector;
	float Left = 0.0f; // seconds until it lands
	EDTEnemyRole Role = EDTEnemyRole::Swarm;
	uint32 EnemySerial = 0;
};

/** Splitter children owed by a death, spawned at the start of the next Enemies phase (elites.md R-ELT-06). */
struct FDTEliteChildren
{
	FVector2f Origin = FVector2f::ZeroVector;
	float HpScale = 1.0f;
	float DmgScale = 1.0f;
	bool bDirector = false;
};

/** One elite's end (elites.md R-ELT-13): the enemy subsystem forwards it to telemetry. */
struct FDTEliteDeath
{
	uint32 Serial = 0;
	EDTEnemyRole Role = EDTEnemyRole::Swarm;
	uint8 AffixMask = 0;
	int32 OwnerId = INDEX_NONE;
	uint16 SourceId = 0;
	float AliveSeconds = 0.0f;
	int32 ShieldAbsorbed = 0;
	int32 ShieldLeft = 0;
	float Drained = 0.0f;
	float Healed = 0.0f;
	int32 BeatId = INDEX_NONE;
};

/** One threat action started this step (enemies.md Telemetry ThreatAction{Role, AttackId}); the subsystem forwards it to telemetry. */
struct FDTThreatAction
{
	FVector2f Origin = FVector2f::ZeroVector;
	EDTEnemyRole Role = EDTEnemyRole::Swarm;
	FName AttackId;
};

/** Counters for DT.Enemies.Stats and DT.EnemyFire.Stats (enemies.md Telemetry). */
struct FDTEnemyCounters
{
	int64 StaleHitsIgnored = 0;
	int64 StatusesApplied[static_cast<int32>(EDTStatus::Count)] = {};
	int64 StatusesIgnored = 0;  // stale handle, or refused (Chill while frozen, Freeze on the boss)
	int64 DisplacementsIgnored = 0;
	int64 ThreatActions[static_cast<int32>(EDTEnemyRole::Count)] = {}; // charges started, bursts fired
	int64 TokenWaits = 0;       // steps a ready ranged unit held for want of an aim token
	int64 AttacksSkippedForLead = 0;
};

/**
 * Every live enemy, dense: slot i of each array is enemy i. Indices are stable from the Enemies phase to the end of
 * ResolveHits and change only in Compact (swap-remove), after every consumer of the step has run.
 */
class DUSKTRACER_API FDTEnemyPool
{
public:
	FDTEnemyPool();

	/** Removes every enemy, round and orphan attack; counters (kills, refusals, serials) are kept unless bResetCounters. */
	void Reset(bool bResetCounters = true);

	/** Adds one enemy of Role at Position, unscaled, no flags. Returns its index, or INDEX_NONE (counted) at C.ENEMIES.CAP. */
	int32 Spawn(EDTEnemyRole Role, const FVector2f& Position);

	/**
	 * Adds one enemy from Spec (R-ENM-09). A Turret draws its fire pattern from Rng (the Spawn stream, R-ENM-08; the
	 * first catalogue pattern when null). bDirector marks a director spawn (it drops loot and is copied into FDTKill).
	 */
	int32 Spawn(const FDTEnemySpawn& Spec, FRandomStream* Rng, bool bDirector = false);

	/** Spawns up to Count enemies on a ring of Radius around Center; angles from Rng (the Spawn stream, R-TEC-02). Returns the number spawned. */
	int32 SpawnRing(EDTEnemyRole Role, int32 Count, const FVector2f& Center, float Radius, FRandomStream& Rng);
	/** As SpawnRing, every enemy from Template (its Position is replaced). */
	int32 SpawnRing(const FDTEnemySpawn& Template, int32 Count, const FVector2f& Center, float Radius, FRandomStream& Rng);

	/**
	 * EDTSimPhase::Enemies: applies View.StatusApplies and View.Displacements (then clears them), ticks statuses (Burn
	 * appends FDTHit), assigns aim tokens, steps the role machines and moves, publishes positions, radii, serials and
	 * the hash, publishes telegraphs, steps enemy fire, and appends contact and enemy-fire FDTHeroHit.
	 */
	void StepEnemies(const FDTSimContext& Context);

	/** Moves every enemy by one step of Dt towards the nearest of Heroes (role machines run with no run state). */
	void Move(float Dt, TConstArrayView<FVector2f> Heroes);

	/** Writes positions, radii, serials and the rebuilt hash into View (dense; index = enemy slot). */
	void Publish(FDTWorldView& View) const;

	/**
	 * EDTSimPhase::ResolveHits: applies every hit in Context.View->Hits (Tank plate, Shock), emits Hit and Kill events
	 * through Context.Run, fills View->Kills, clears the hits, compacts and republishes. Returns the number of kills.
	 */
	int32 ResolveHits(const FDTSimContext& Context);

	/**
	 * Applies Hits to the pool and emits events into Run; marks deaths but does not compact. Returns kills.
	 * A hit whose EnemySerial is non-zero and no longer matches its index is stale and ignored (R-ENM-23).
	 * Each death appends one FDTKill to OutKills when given.
	 */
	int32 ApplyHits(TConstArrayView<FDTHit> Hits, FDTRunState& Run, TArray<FDTKill>* OutKills = nullptr);

	/** Applies one status (R-ENM-21). Returns false when ignored (stale handle) or refused. */
	bool ApplyStatus(const FDTStatusApply& Apply);

	/** Spawns the Splitter children owed by last step's deaths (elites.md R-ELT-06). StepEnemies calls it; the subsystem calls it
	 *  earlier, before telemetry stamps the step's spawns, with the Spawn stream. Rng may be null. */
	void SpawnEliteChildren(FRandomStream* Rng);

	/** Swap-removes every dead enemy (an arrival dying mid-telegraph leaves its attack behind). Returns the number removed. */
	int32 Compact();

	/** Boss hull (boss.md R-BOS-03): the boss system places its slot by script; a stale handle is ignored. */
	bool SetScriptedPosition(const FDTEnemyHandle& Handle, const FVector2f& Position);

	/** Debug (DT.EnemyFire): while false no ranged unit starts a tell; rounds in flight keep flying. */
	void SetFireEnabled(bool bEnabled) { bFireEnabled = bEnabled; }
	bool IsFireEnabled() const { return bFireEnabled; }

	int32 Num() const { return Pos.Num(); }
	int32 GetCapacity() const { return Capacity; }
	int64 GetRefusedSpawns() const { return RefusedSpawns; }
	int64 GetTotalKills() const { return TotalKills; }
	const FDTKillRateCounter& GetKillRate() const { return KillRate; }
	const FDTEnemyCounters& GetCounters() const { return Counters; }
	/** Threat actions started in the last StepEnemies / Move (charges started, bursts fired, orphan attacks landing). */
	TConstArrayView<FDTThreatAction> GetStepThreatActions() const { return StepThreats; }
	int32 CountRole(EDTEnemyRole InRole) const;

	// Read access to the arrays (render, tests).
	TConstArrayView<FVector2f> GetPositions() const { return Pos; }
	TConstArrayView<FVector2f> GetPrevPositions() const { return PrevPos; }
	TConstArrayView<FVector2f> GetVelocities() const { return Vel; }
	/** Render facing: velocity for the swarm, the charge line for a Rusher, the hull for a Tank, the head for a Turret. */
	TConstArrayView<FVector2f> GetFacings() const { return Facing; }
	TConstArrayView<float> GetRadii() const { return Radius; }
	TConstArrayView<float> GetHp() const { return Hp; }
	TConstArrayView<float> GetMaxHp() const { return MaxHp; }
	TConstArrayView<float> GetSpeeds() const { return Speed; }
	TConstArrayView<float> GetContactDamage() const { return ContactDamage; }
	TConstArrayView<float> GetHpScales() const { return HpScale; }
	TConstArrayView<float> GetDmgScales() const { return DmgScale; }
	TConstArrayView<uint8> GetFlags() const { return Flags; }
	TConstArrayView<int32> GetBeatIds() const { return BeatId; }
	TConstArrayView<EDTEnemyRole> GetRoles() const { return Role; }
	const FDTEnemyAux& GetAux(int32 Index) const { return Aux[Index]; }
	/** Bitmask of EDTStatus currently on the enemy. */
	uint8 GetStatusMask(int32 Index) const;
	/** Deaths of the last ResolveHits, in the order of its FDTKill list. */
	TConstArrayView<FDTEnemyDeath> GetStepDeaths() const { return StepDeaths; }
	/** Run-unique, never zero, kept through swap-removal (R-ENM-23). */
	TConstArrayView<uint32> GetSerials() const { return Serial; }
	/** True while Handle still names the same enemy (its serial at its index matches). */
	bool IsHandleValid(const FDTEnemyHandle& Handle) const { return Serial.IsValidIndex(Handle.Index) && Handle.Serial != 0 && Serial[Handle.Index] == Handle.Serial; }
	FDTEnemyHandle GetHandle(int32 Index) const { return FDTEnemyHandle{ Index, Serial[Index] }; }
	bool IsDead(int32 Index) const { return Dead[Index] != 0; }
	const FDTEnemyBulletPool& GetRounds() const { return Rounds; }
	FDTEnemyBulletPool& GetRounds() { return Rounds; }
	TConstArrayView<FDTOrphanAttack> GetOrphanAttacks() const { return Orphans; }
	TConstArrayView<FDTEliteBlast> GetEliteBlasts() const { return Blasts; }
	/** Elite deaths of the last ResolveHits (elites.md R-ELT-13). */
	TConstArrayView<FDTEliteDeath> GetStepEliteDeaths() const { return StepEliteDeaths; }

	/** Volley directions (R-ENM-19, R-ENM-20): Count rounds Spread degrees apart around Aim; an even count is shifted half a
	 *  step so one round flies exactly along Aim, to the left on even volleys and to the right on odd ones. */
	static void FanDirections(const FVector2f& Aim, int32 Count, float SpreadDeg, int32 VolleyIndex, TArray<FVector2f>& Out);
	/** The directions one volley of Pattern fires (a close fan fires one aimed round). */
	static void VolleyDirections(const FVector2f& Aim, EDTFirePattern Pattern, bool bCloseFan, int32 VolleyIndex, TArray<FVector2f>& Out);

	/** Test hooks: place, wound, turn or re-pattern an enemy directly. */
	void SetPosition(int32 Index, const FVector2f& Position) { Pos[Index] = Position; PrevPos[Index] = Position; }
	void SetHp(int32 Index, float Value) { Hp[Index] = Value; }
	void SetFacing(int32 Index, const FVector2f& Dir);
	void SetPattern(int32 Index, EDTFirePattern InPattern) { Aux[Index].Pattern = InPattern; }

	/** keepers.md R-KEE-21: true when Handle may be pulled at StepIndex, and holds it for IntervalSteps; false for a stale handle or one still held. */
	bool TryPull(const FDTEnemyHandle& Handle, int64 StepIndex, int64 IntervalSteps);

private:
	void SwapRemove(int32 Index);
	void ResolveInit(TConstArrayView<FVector2f> Heroes);
	void ApplyDisplacements(TArray<FDTDisplacement>& Displacements);
	void TickStatuses(float Dt, TArray<FDTHit>* OutHits);
	void AssignTokens(TConstArrayView<FVector2f> Heroes, bool bThreatBlock);
	void StepRoles(float Dt, int64 StepIndex, TConstArrayView<FVector2f> Heroes);
	void PublishTelegraphs(TArray<FDTTelegraph>& Out) const;
	void FireVolley(const FVector2f& From, const FVector2f& Aim, EDTFirePattern InPattern, bool bClose, int32 Volley,
		int32 Target, float InDmgScale, uint32 InSerial);
	void StepOrphans(float Dt, FDTWorldView& View);
	void StepElites(float Dt, FDTWorldView& View);
	void StepBlasts(float Dt, FDTWorldView& View);
	void CountThreat(EDTEnemyRole InRole, const FVector2f& Origin);
	void AppendContacts(FDTWorldView& View) const;
	int32 NearestHero(const FVector2f& At, TConstArrayView<FVector2f> Heroes, FVector2f* OutDelta = nullptr) const;
	float ChargeLengthFrom(const FVector2f& From, const FVector2f& Dir, float EnemyRadius) const;

	int32 Capacity = 0;
	TArray<FVector2f> Pos;
	TArray<FVector2f> PrevPos;
	TArray<FVector2f> Vel;
	TArray<FVector2f> Facing;
	TArray<float> Radius;
	TArray<float> Hp;
	TArray<float> MaxHp;
	TArray<float> Speed;
	TArray<float> ContactDamage;
	TArray<float> HpScale;
	TArray<float> DmgScale;
	TArray<EDTEnemyRole> Role;
	TArray<uint8> Flags;
	TArray<int32> BeatId;
	TArray<uint8> Dead;
	TArray<uint32> Serial;
	TArray<FDTEnemyAux> Aux;
	TArray<int64> PullReadyStep; // first step a Pull may reach this enemy again (R-KEE-21)
	uint32 NextSerial = 1; // never zero; reset only with the counters (a new run)
	uint32 NextBurnOrder = 1;
	/** Damage below a whole point not yet reported, by (owner << 16) | source (ApplyHits). */
	TMap<uint32, float> DamageCarry;

	FDTEnemyBulletPool Rounds;
	TArray<FDTOrphanAttack> Orphans;
	TArray<FDTEliteBlast> Blasts;
	TArray<FDTEliteChildren> PendingChildren;
	TArray<FDTEliteDeath> StepEliteDeaths;
	TArray<FDTThreatAction> StepThreats;
	TArray<FDTEnemyDeath> StepDeaths;
	bool bFireEnabled = true;

	FDTSpatialHash NeighbourHash;
	TArray<int32> Candidates;
	TArray<FVector2f> NextVel;
	TArray<int32> TokenScratch;
	TArray<FVector2f> DirScratch;

	int64 RefusedSpawns = 0;
	int64 TotalKills = 0;
	FDTKillRateCounter KillRate;
	FDTEnemyCounters Counters;
};
