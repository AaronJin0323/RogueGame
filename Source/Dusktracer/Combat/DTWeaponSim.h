// The weapon simulation (weapons.md, modifiers.md): per player it fires every owned weapon in EDTSimPhase::Weapons,
// mirrors its rounds through EDTSimPhase::Bullets and runs modifier and flag reactions in EDTSimPhase::Effects.
// Plain C++ (no UObject) so tests drive it with a bare FDTBulletPool, FDTEnemyPool and FDTWorldView.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Combat/DTWeaponCompose.h"
#include "Combat/DTWeaponFlags.h"

class FDTBulletPool;
struct FDTBulletSpawn;

/** Result of an inventory change (offers and debug commands read it). */
enum class EDTWeaponResult : uint8
{
	Ok,
	UnknownId,
	NoSlot,       // C.WEAPON.SLOTS weapons owned (R-WPN-03)
	AlreadyOwned,
	MaxLevel,     // C.WEAPON.LEVEL_MAX / the weapon's track
	NoWeapon,
	Incompatible, // R-MOD-01
	Duplicate,    // R-MOD-04: at most once per weapon
	SocketsFull,  // C.MOD.SOCKETS_PER_WEAPON
	BadPlayer
};

/** Per weapon counters for telemetry and DT.Weapons.Stats (weapons.md Telemetry). Monotonic within a run. */
struct FDTWeaponCounters
{
	int64 Spawned = 0;   // emissions that passed the spawn-rate cap
	int64 Throttled = 0; // emissions dropped by the spawn-rate cap (R-WPN-09)
	int64 Impacts = 0;   // hits this weapon's emissions made, seen by the weapon system
};

/** One weapon visual for the renderer: cool core or a zone tinted by the leading Keeper (R-WPN-11). */
struct FDTWeaponVisual
{
	enum class EKind : uint8 { Drone, Mine, MineZone, Field, Arc };
	EKind Kind = EKind::Drone;
	FVector2f Position = FVector2f::ZeroVector;
	FVector2f Dir = FVector2f(1.0f, 0.0f);
	float Radius = 0.0f;
	float ArcDeg = 0.0f;
	/** Zone tint: the owner's leading Keeper, or EDTPath::None for Pal.Player.TrailNeutral. Cores ignore it. */
	EDTPath Path = EDTPath::None;
};

class DUSKTRACER_API FDTWeaponSim
{
public:
	// --- inventory (offers, debug) ---------------------------------------------------------------------------
	/** Gives Weapon at Level (clamped to its track); refused beyond C.WEAPON.SLOTS or when already owned. */
	EDTWeaponResult Give(FDTRunState& Run, int32 OwnerId, FName WeaponId, int32 Level = 1, int32* OutSlot = nullptr);
	/** Raises an owned weapon one level (or to Level when > 0). */
	EDTWeaponResult LevelUp(FDTRunState& Run, int32 OwnerId, int32 Slot, int32 Level = 0);
	/** Sockets a modifier into one weapon (R-MOD-01, R-MOD-04); checked against the weapon's effective caps. */
	EDTWeaponResult Socket(FDTRunState& Run, int32 OwnerId, int32 Slot, FName ModId);
	/** Slot of WeaponId for OwnerId, or INDEX_NONE. */
	static int32 FindSlot(const FDTRunState& Run, int32 OwnerId, FName WeaponId);

	// --- the flag seam (keepers.md R-KEE-03) ------------------------------------------------------------------
	/** Adds (or replaces, by SourceTag) a flag source that applies to every weapon OwnerId owns now or later. */
	void AddFlagSource(int32 OwnerId, FName SourceTag, const FDTWeaponFlags& Flags);
	void RemoveFlagSource(int32 OwnerId, FName SourceTag);
	void ClearFlagSources(int32 OwnerId);
	/** Every entry of every source of OwnerId, in the order sources were added. */
	TArray<FDTFlagEntry> GetPlayerFlags(int32 OwnerId) const;

	// --- other inputs ------------------------------------------------------------------------------------------
	void SetHeroStats(int32 OwnerId, const FDTWeaponHeroStats& Stats);
	/** combat_core.md R-CMB-04 aim mode: Manual makes Nearest weapons fire along HeroAimDirs (R-WPN-12). */
	void SetAimManual(int32 OwnerId, bool bManual);

	// --- composition -------------------------------------------------------------------------------------------
	/** Composes one owned weapon now (R-WPN-09), with load ranks over the owner's weapons. */
	FDTWeaponSpec ComposeSlot(FDTRunState& Run, int32 OwnerId, int32 Slot) const;

	// --- the step ----------------------------------------------------------------------------------------------
	/** EDTSimPhase::Weapons: deferred strikes, mines, drones, volleys. Rounds go to Bullets (may be null in tests). */
	void StepWeapons(const FDTSimContext& Context, FDTBulletPool* Bullets);
	/** EDTSimPhase::Bullets, after the pool stepped: mirrors tracked rounds and captures this step's impacts. */
	void ObserveBullets(const FDTSimContext& Context);
	/** EDTSimPhase::Effects: modifier and flag reactions on the step's impacts and kills (R-KEE-10). */
	void StepEffects(const FDTSimContext& Context, FDTBulletPool* Bullets);

	/** Drops every transient (rounds, mines, fields, counters); keeps flag sources, stats and aim modes. */
	void ResetTransient();

	// --- read side -----------------------------------------------------------------------------------------------
	const FDTWeaponCounters& GetCounters(int32 OwnerId, int32 Slot) const;
	/** Composed spec of the last StepWeapons (empty spec when none). */
	const FDTWeaponSpec& GetLastSpec(int32 OwnerId, int32 Slot) const;
	int32 GetLiveMines(int32 OwnerId, int32 Slot) const;
	int32 GetLiveFields() const { return Fields.Num(); }
	int32 GetTrackedRounds() const { return Rounds.Num(); }
	/** Highest split generation of any round spawned this run (R-MOD-05 check). */
	int32 GetMaxGeneration() const { return MaxGeneration; }
	/** Appends what should be drawn this frame. */
	void GatherVisuals(const FDTRunState& Run, const FDTWorldView& View, TArray<FDTWeaponVisual>& Out) const;

private:
	struct FFlagSource
	{
		FName Tag;
		FDTWeaponFlags Flags;
	};

	struct FMine
	{
		FVector2f Position = FVector2f::ZeroVector;
		float Age = 0.0f;
		int32 TicksDone = 0;
		float Damage = 0.0f;
		uint16 SourceId = 0;
	};

	struct FWeaponRuntime
	{
		FDTWeaponSpec Spec;
		FDTWeaponCounters Counters;
		TArray<int32> EmitRing;
		int32 EmitCursor = 0;
		int32 EmitWindow = 0;
		float DroneAngleDeg = 0.0f;
		float DroneTimer = 0.0f;
		int32 NextDrone = 0;
		TArray<FMine> Mines;
		int32 StillCharges = 0;
		int32 MeleeActivations = 0;
		float ChainTriggerReadyIn = 0.0f;
	};

	struct FPlayerRuntime
	{
		TArray<FFlagSource> Sources;
		FDTWeaponHeroStats Stats;
		bool bAimManual = false;
		TArray<FWeaponRuntime> Weapons;
		FVector2f StillAnchor = FVector2f::ZeroVector;
		float StillTime = 0.0f;
		bool bHasAnchor = false;
		bool bWasIntangible = false;
	};

	/** What kind of contact an impact was; decides which reactions run (see DTWeaponSim.cpp). */
	enum class EImpact : uint8 { RoundHit, ArcHit, ArcSweep, MineHit, Detonation, Expiry };

	struct FImpact
	{
		EImpact Kind = EImpact::RoundHit;
		int32 OwnerId = INDEX_NONE;
		int8 Slot = INDEX_NONE;
		uint16 SourceId = 0;
		FVector2f Position = FVector2f::ZeroVector;
		/** Where the emission came from (round spawn point, hero for arcs) and its travel direction. */
		FVector2f Origin = FVector2f::ZeroVector;
		FVector2f Dir = FVector2f(1.0f, 0.0f);
		/** Damage of the emission before crit (what children and ratios scale from). */
		float BaseDamage = 0.0f;
		float HitDamage = 0.0f;
		/** Contact radius for strikes that repeat the impact (round radius, half reach, blast radius). */
		float Area = 0.0f;
		uint32 EnemySerial = 0;
		float EnemyRadius = 0.0f;
		FVector2f EnemyPosition = FVector2f::ZeroVector;
		int32 Generation = 0;
		int32 BouncesLeft = 0;
		bool bSpent = false;
		bool bCleave = false;
	};

	/** A tracked round: an exact mirror of a pool bullet (same float ops), so hits and expiry map back to it. */
	struct FRound
	{
		int32 OwnerId = INDEX_NONE;
		int8 Slot = INDEX_NONE;
		uint16 SourceId = 0;
		FVector2f Position = FVector2f::ZeroVector;
		FVector2f Velocity = FVector2f::ZeroVector;
		FVector2f Origin = FVector2f::ZeroVector;
		float LifeLeft = 0.0f;
		float Radius = 0.0f;
		float BaseDamage = 0.0f;
		int32 PierceLeft = 1;
		int32 Generation = 0;
		int32 BouncesLeft = 0;
	};

	struct FField
	{
		int32 OwnerId = INDEX_NONE;
		int8 Slot = INDEX_NONE;
		uint16 SourceId = 0;
		FVector2f Position = FVector2f::ZeroVector;
		float Radius = 0.0f;
		float TimeLeft = 0.0f;
		float DamagePerSecond = 0.0f;
	};

	struct FEcho
	{
		int32 OwnerId = INDEX_NONE;
		int8 Slot = INDEX_NONE;
		uint16 SourceId = 0;
		FVector2f Position = FVector2f::ZeroVector;
		float Delay = 0.0f;
		float Damage = 0.0f;
		float Radius = 0.0f;
		bool bLeaveField = false;
	};

	struct FArcFlash
	{
		int32 OwnerId = INDEX_NONE;
		FVector2f Position = FVector2f::ZeroVector;
		FVector2f Dir = FVector2f(1.0f, 0.0f);
		float Reach = 0.0f;
		float ArcDeg = 0.0f;
		float TimeLeft = 0.0f;
	};

	/** A status a Weapons-phase emission (a slam) applies; rolled in Effects on the enemy's serial (R-KEE-10). */
	struct FPendingStatus
	{
		uint32 EnemySerial = 0;
		float HitDamage = 0.0f;
		int32 OwnerId = INDEX_NONE;
		uint16 SourceId = 0;
		FDTFlagParams Params;
	};

	/** One firing context: who fires, from where, with what source. */
	struct FShot
	{
		int32 OwnerId = INDEX_NONE;
		int32 Slot = INDEX_NONE;
		uint16 SourceId = 0;
		FVector2f Origin = FVector2f::ZeroVector;
		float Damage = 0.0f;
		int32 Pierce = 1;
		float Speed = 0.0f;
		float Radius = 0.0f;
		float Life = 0.0f;
		bool bTrack = false;
		int32 Generation = 0;
		int32 Bounces = 0;
	};

	/** A shot of Slot's own rounds (spec speed, radius, life, pierce, damage, weapon source). */
	FShot MakeShot(int32 OwnerId, int32 Slot, const FWeaponRuntime& Runtime, uint16 SourceId, const FVector2f& Origin) const;
	TMap<uint32, int32> SerialToIndex;
	void BuildSerialIndex(const FDTWorldView& View);
	int32 IndexOfSerial(uint32 Serial) const;

	FPlayerRuntime& EnsurePlayer(int32 OwnerId);
	FWeaponRuntime* FindRuntime(int32 OwnerId, int32 Slot);
	void SyncPlayers(FDTRunState& Run);

	bool TryEmit(FWeaponRuntime& Runtime);
	bool RollCrit(const FDTSimContext& Context, const FDTWeaponSpec& Spec, float& InOutDamage) const;
	void SpawnRound(const FDTSimContext& Context, FDTBulletPool* Bullets, FWeaponRuntime& Runtime, const FShot& Shot,
		const FVector2f& Position, const FVector2f& Dir);
	/** One sweep: one FDTHit per enemy inside the arc (R-WPN-06), plus an ArcSweep impact. Returns enemies hit. */
	int32 EmitArc(const FDTSimContext& Context, FWeaponRuntime& Runtime, const FShot& Shot, const FVector2f& Dir,
		float ArcDeg, float Reach, bool bCleave);

	bool FindNearest(const FDTWorldView& View, const FVector2f& From, float Range, uint32 ExcludeSerial, int32& OutEnemy) const;
	/** The DensestCell targets: up to Count cell centres ranked by enemies within Range of the hero. */
	void FindDensestCells(const FDTWorldView& View, const FVector2f& Hero, float Range, int32 Count, TArray<FVector2f>& Out) const;

	/** Fires one volley of an Aimed, Radial, MeleeArc or Mine weapon. Returns false when it found no target. */
	bool FireVolley(const FDTSimContext& Context, FDTBulletPool* Bullets, FDTPlayerRunState& Player, int32 Slot,
		FWeaponRuntime& Runtime, uint16 SourceOverride, bool bFree);
	/** Steps the drones of an Orbiter weapon; fires one drone's bolt when its turn is ready. */
	void StepDrones(const FDTSimContext& Context, FDTBulletPool* Bullets, FDTPlayerRunState& Player, int32 Slot,
		FWeaponRuntime& Runtime, bool bFreeVolley);
	void PlaceMine(const FDTSimContext& Context, FWeaponRuntime& Runtime, int32 OwnerId, int32 Slot, const FVector2f& At,
		float Damage, uint16 SourceId);
	void StepMines(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime);
	void Detonate(const FDTSimContext& Context, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime, const FMine& Mine);

	/** Activation-time expressions: Split fallback rings, Slam counters (R-WPN-08). */
	void OnActivation(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot,
		FWeaponRuntime& Runtime, bool bMelee);
	void StepStillness(const FDTSimContext& Context, int32 OwnerId, FPlayerRuntime& Player);
	void StepSlamTriggers(const FDTSimContext& Context, int32 OwnerId, FPlayerRuntime& Player);
	void EmitSlam(const FDTSimContext& Context, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime, const FDTResolvedFlag& Slam);

	void StepDeferred(const FDTSimContext& Context);
	void SpawnField(int32 OwnerId, int32 Slot, uint16 SourceId, const FVector2f& At, const FDTFlagParams& Params, float BaseDamage);
	int32 HitCircle(const FDTSimContext& Context, int32 OwnerId, int32 Slot, uint16 SourceId, const FVector2f& Center,
		float Radius, float Damage, bool bRollCrit, const FDTWeaponSpec* Spec, TArray<FDTEnemyHandle>* OutHit = nullptr);

	void ReactToImpact(const FDTSimContext& Context, FDTBulletPool* Bullets, const FImpact& Impact);
	void EmitChildren(const FDTSimContext& Context, FDTBulletPool* Bullets, FWeaponRuntime& Runtime, const FImpact& Impact,
		const FDTFlagEntry& Entry, bool bRing, const FVector2f& From);
	void ApplyStatus(const FDTSimContext& Context, const FDTFlagParams& Params, const FDTEnemyHandle& Handle,
		float HitDamage, int32 OwnerId, uint16 SourceId);

	TArray<FPlayerRuntime> Players;
	TArray<FRound> Rounds;
	TArray<FField> Fields;
	TArray<FEcho> Echoes;
	TArray<FArcFlash> Arcs;
	TArray<FImpact> Impacts;
	TArray<FPendingStatus> PendingStatuses;
	TArray<int32> Candidates;
	int32 WeaponsHitsEnd = 0;
	int32 MaxGeneration = 0;
	int64 LastStep = -1;
	float MaxEnemyRadius = 0.0f;
};
