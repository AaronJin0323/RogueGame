// The weapon simulation (weapons.md, modifiers.md): per player it fires every owned weapon in EDTSimPhase::Weapons,
// mirrors its rounds through EDTSimPhase::Bullets and runs modifier and flag reactions in EDTSimPhase::Effects.
// Plain C++ (no UObject) so tests drive it with a bare FDTBulletPool, FDTEnemyPool and FDTWorldView.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"
#include "Combat/DTBulletPool.h"
#include "Combat/DTWeaponCompose.h"
#include "Combat/DTWeaponFlags.h"

class FDTEnemyPool;

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

/** One weapon visual for the renderer: cool core or a zone tinted by the leading Keeper (R-WPN-11), or a control cue (R-PRE-11). */
struct FDTWeaponVisual
{
	enum class EKind : uint8 { Drone, Mine, MineZone, Field, Arc, StillAura, StillCharge, FrozenWall, FrozenHold, FrozenPlain };
	EKind Kind = EKind::Drone;
	FVector2f Position = FVector2f::ZeroVector;
	FVector2f Dir = FVector2f(1.0f, 0.0f);
	float Radius = 0.0f;
	float ArcDeg = 0.0f;
	/** StillAura: 1 as the aura pulses, falling to 0 by the next pulse. */
	float Pulse01 = 0.0f;
	/** StillCharge: charges held (the most any weapon holds), and how far the next one has gathered (0 at charges_max). */
	int32 Charges = 0;
	float Charge01 = 0.0f;
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

	/** The read side of enemy state: statuses, elite and role, death records (R-KEE-10: read in Effects, never written). */
	void SetEnemies(FDTEnemyPool* InEnemies) { Enemies = InEnemies; }
	/** Non-starter ids in the run pool (DT.Unlock): Mod.Refit draws only from the pool. */
	void Unlock(FName Id) { Unlocked.Add(Id); }

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
	/** Fields opened this run (Vortex zones among them). */
	int32 GetFieldsOpened() const { return FieldsOpened; }
	int32 GetLiveHusks() const { return Husks.Num(); }
	int32 GetLivePillars() const;
	/** Where a Blade Halo's blades orbit now (R-RFG-07). */
	float GetBladeRadius(int32 OwnerId, int32 Slot) const;
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
		bool bCharged = false;
		/** Where it came from (a mine: its own position; a shell: the hero at launch). */
		FVector2f Origin = FVector2f::ZeroVector;
	};

	/** A Flak shell in the air: bursts at Target when its flight ends (R-WPN-16); no pool round. */
	struct FShell
	{
		FVector2f Origin = FVector2f::ZeroVector;
		FVector2f Target = FVector2f::ZeroVector;
		float Flight = 0.0f;
		float Left = 0.0f;
		float Damage = 0.0f;
		uint16 SourceId = 0;
		bool bCharged = false;
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
		TArray<FShell> Shells;
		float SpiralDeg = 0.0f;
		/** Hits of the stream so far (Echo's fallback on Continuous, R-WPN-08). */
		int32 StreamHits = 0;
		float RefitReadyIn = 0.0f;
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
		/** pulled_vuln: serial -> step the mark ends (Pull, keepers.md). */
		TMap<uint32, int64> PulledUntilStep;
		float SpreadReadyIn = 0.0f;
		/** freeze_until_hit: enemies whose Freeze is being held, oldest first, and those struck since they froze. */
		TArray<uint32> HeldFrozen;
		TSet<uint32> FrozenStruck;
		/** charged_aura_uu: seconds until the aura may pulse again. */
		float AuraReadyIn = 0.0f;
		/** after_hit_taken_mult: a hit was taken since the last slam. */
		bool bSlamBoosted = false;
		/** every_n: sky strikes so far. */
		int32 SkyStrikes = 0;
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
		/** Bitmask of EDTStatus on the struck enemy when the hit landed (a lethal hit still reads it). */
		uint8 StatusMask = 0;
		/** The emission came from a Stillness-charged volley. */
		bool bCharged = false;
		/** A slam's ember (keepers.md §2): applies ember_status and runs no other reaction. */
		bool bEmber = false;
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
		bool bCharged = false;
		bool bEmber = false;
		/** Non-zero: a moving round; the pool reports its moves (R-BUL-09). */
		uint32 RoundId = 0;
		int32 Pierce = 1;
		bool bBoomerang = false;
		bool bReturning = false;
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
		/** The emission's damage (a Burn scales with it, R-KEE-11) and the flag's params, for the status it names. */
		float BaseDamage = 0.0f;
		FDTFlagParams Params;
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
		/** every_n: this strike falls on the whole screen around the hero. */
		bool bScreen = false;
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

	/** An arc that struck Serial; if that blow kills, it forks (fork_on_kill, read at the step's kills). */
	struct FArcMark
	{
		uint32 Serial = 0;
		int32 OwnerId = INDEX_NONE;
		int8 Slot = INDEX_NONE;
		uint16 SourceId = 0;
		float Damage = 0.0f;
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
		bool bCharged = false;
		bool bEmber = false;
		FDTBulletMotion Motion;
	};

	/** A kill's estimated overkill, waiting to see that the kill happened (R-MOD-14). */
	struct FOverkill
	{
		uint32 Serial = 0;
		int32 OwnerId = INDEX_NONE;
		int8 Slot = INDEX_NONE;
		uint16 SourceId = 0;
		float Damage = 0.0f;
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
		float ArcDeg, float Reach, bool bCleave, bool bImpacts = true);

	bool FindNearest(const FDTWorldView& View, const FVector2f& From, float Range, uint32 ExcludeSerial, int32& OutEnemy) const;
	/** The DensestCell targets: up to Count cell centres ranked by enemies within Range of the hero. */
	void FindDensestCells(const FDTWorldView& View, const FVector2f& Hero, float Range, int32 Count, TArray<FVector2f>& Out) const;

	/** DensestLine (R-WPN-15): the bearing sector holding the most enemies in Range, as a direction. False when none. */
	bool FindDensestLine(const FDTWorldView& View, const FVector2f& Hero, float Range, FVector2f& OutDir) const;
	/** Enemies within Range of From, lowest index first (the RandomAlive draw must not depend on hash order). */
	void EnemiesInRange(const FDTWorldView& View, const FVector2f& From, float Range, TArray<int32>& Out) const;

	/** Fires one volley of any pattern but Orbiter. Returns false when it found no target. */
	bool FireVolley(const FDTSimContext& Context, FDTBulletPool* Bullets, FDTPlayerRunState& Player, int32 Slot,
		FWeaponRuntime& Runtime, uint16 SourceOverride, bool bFree);
	/** Steps the drones of an Orbiter weapon; fires one drone's bolt when its turn is ready. */
	void StepDrones(const FDTSimContext& Context, FDTBulletPool* Bullets, FDTPlayerRunState& Player, int32 Slot,
		FWeaponRuntime& Runtime, bool bFreeVolley);
	void PlaceMine(const FDTSimContext& Context, FWeaponRuntime& Runtime, int32 OwnerId, int32 Slot, const FVector2f& At,
		float Damage, uint16 SourceId, bool bCharged);
	void StepMines(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime);
	void LaunchShell(FWeaponRuntime& Runtime, const FShot& Shot, const FVector2f& Target);
	void StepShells(const FDTSimContext& Context, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime);
	void Detonate(const FDTSimContext& Context, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime, const FMine& Mine);

	/** Activation-time expressions: Split fallback rings, Slam counters (R-WPN-08). */
	void OnActivation(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot,
		FWeaponRuntime& Runtime, bool bMelee);
	void StepStillness(const FDTSimContext& Context, int32 OwnerId, FPlayerRuntime& Player);
	/** Spends one Stillness charge on Shot (keepers.md §2); charged_freeze_ring_uu queues its Freeze for Effects. */
	void SpendCharge(const FDTSimContext& Context, int32 OwnerId, FWeaponRuntime& Runtime, const FDTResolvedFlag& Still, FShot& Shot);
	/** Effects: Frost's frozen_blocks_enemies and freeze_until_hit, Stillness's charged aura. */
	void StepHeldControl(const FDTSimContext& Context, int32 OwnerId, FPlayerRuntime& Player);
	void StepSlamTriggers(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, FPlayerRuntime& Player);
	void EmitSlam(const FDTSimContext& Context, FDTBulletPool* Bullets, int32 OwnerId, int32 Slot, FWeaponRuntime& Runtime, const FDTResolvedFlag& Slam);

	void StepDeferred(const FDTSimContext& Context);
	void SpawnField(int32 OwnerId, int32 Slot, uint16 SourceId, const FVector2f& At, const FDTFlagParams& Params, float BaseDamage);
	int32 HitCircle(const FDTSimContext& Context, int32 OwnerId, int32 Slot, uint16 SourceId, const FVector2f& Center,
		float Radius, float Damage, bool bRollCrit, const FDTWeaponSpec* Spec, TArray<FDTEnemyHandle>* OutHit = nullptr);

	void ReactToImpact(const FDTSimContext& Context, FDTBulletPool* Bullets, const FImpact& Impact);
	/** Mod.Spillover: replays the step's hits against enemy health, so overkill is known before ResolveHits spends it. */
	void EstimateOverkill(const FDTWorldView& View);
	void SpillOverkill(const FDTSimContext& Context);
	/** Mod.Refit: a random pool modifier into the first weapon with a free socket that can use one. */
	bool TryRefit(const FDTSimContext& Context, FDTPlayerRunState& Player, FWeaponRuntime& Source);
	void EmitChildren(const FDTSimContext& Context, FDTBulletPool* Bullets, FWeaponRuntime& Runtime, const FImpact& Impact,
		const FDTFlagEntry& Entry, bool bRing, const FVector2f& From);
	void ApplyStatus(const FDTSimContext& Context, const FDTFlagParams& Params, const FDTEnemyHandle& Handle,
		float HitDamage, int32 OwnerId, uint16 SourceId);
	/** Bitmask of EDTStatus on an enemy (0 without an enemy pool or for a stale handle). */
	uint8 StatusMaskOf(int32 Enemy, uint32 Serial) const;
	/** One arc jump onto Enemy: a hit of Damage (dmg_vs_shocked applied), plus every on=ChainHit status. Returns false at the spawn cap. */
	bool ArcJump(const FDTSimContext& Context, FWeaponRuntime& Runtime, const FDTFlagEntry& Chain, const TArray<FDTResolvedFlag>& Flags,
		int32 OwnerId, int8 Slot, int32 Enemy, float Damage, bool bMark);
	/** Pulled enemies whose final positions touch take crush_dmg x the triggering hit as the Keeper (keepers.md §2). */
	void CrushContacts(const FDTSimContext& Context, const FDTFlagEntry& Pull, const TArray<TPair<int32, FVector2f>>& Moved, float HitDamage, int32 OwnerId);
	/** Kill-time reactions: arc forks, burning explosions, Burn spread. */
	void ReactToKills(const FDTSimContext& Context, TConstArrayView<FArcMark> Marks);
	void ForkArcs(const FDTSimContext& Context, const FDTKill& Kill, const FArcMark& Mark);
	/** An area burst of shatter_dmg x the strongest weapon around Center, as the Keeper, applying shatter_status. */
	void ShatterAt(const FDTSimContext& Context, const FDTFlagEntry& Entry, int32 OwnerId, int32 Slot, const FVector2f& Center, float Radius);
	/** cluster_freeze_min: enough touching pulled enemies Freeze together and, with shatter, burst (keepers.md §2). */
	void FreezeCluster(const FDTSimContext& Context, const FDTFlagEntry& Pull, const TArray<TPair<int32, FVector2f>>& Moved, const FImpact& Impact);
	/** Enemies within Radius of Center, lowest index first. */
	void EnemiesInCircle(const FDTWorldView& View, const FVector2f& Center, float Radius, TArray<int32>& Out) const;
	/** The status word with this owner's Frost riders (freeze_stacks, freeze_duration_scale), as hits carry them. */
	FDTFlagParams StatusParams(const FDTWeaponSpec& Spec, FName StatusWord) const;
	void ExplodeBurning(const FDTSimContext& Context, const FDTKill& Kill, const FDTFlagEntry& Entry, float Radius);
	void SpreadBurn(const FDTSimContext& Context, const FDTKill& Kill, const FDTFlagEntry& Entry, float Radius, float BurnDps);
	/** burn_tick_rate of OwnerId's flags: Burn applications carry that much more damage per second (R-KEE-11). */
	float BurnRate(int32 OwnerId) const;
	/** The strongest per-hit damage among OwnerId's composed weapons (the explosion's basis, as Slam's dmg_ratio). */
	float StrongestDamage(int32 OwnerId) const;
	/** Live elites and the boss, nearest to the query first (retarget=EliteOrBoss); collected once per Effects phase. */
	const TArray<int32>& PriorityTargets(const FDTWorldView& View);

	TArray<FPlayerRuntime> Players;
	TArray<FRound> Rounds;
	TArray<FField> Fields;
	struct FHusk
	{
		int32 OwnerId = INDEX_NONE;
		FVector2f Position = FVector2f::ZeroVector;
		float Left = 0.0f;
		float ReadyIn = 0.0f;
	};
	TArray<FHusk> Husks;
	int32 FieldsOpened = 0;
	TArray<FEcho> Echoes;
	TArray<FArcFlash> Arcs;
	TArray<FImpact> Impacts;
	TArray<FPendingStatus> PendingStatuses;
	TArray<FArcMark> ArcMarks;
	TArray<FOverkill> Overkills;
	TSet<FName> Unlocked;
	const FDTBulletPool* LastBullets = nullptr;
	uint32 NextRoundId = 0;
	/** Enemies struck this step by a melee arc or a slam whose flag carries burning_explode_on_death_uu. */
	TSet<uint32> StepCleaved;
	TSet<uint32> StepSlammed;
	/** Enemies already taken by a cluster this step (statuses land next step, so the mask cannot tell). */
	TSet<uint32> StepClustered;
	TArray<int32> Candidates;
	TArray<int32> Priority;
	bool bPriorityBuilt = false;
	FDTEnemyPool* Enemies = nullptr;
	int32 WeaponsHitsEnd = 0;
	int32 MaxGeneration = 0;
	int64 LastStep = -1;
	float MaxEnemyRadius = 0.0f;
};
