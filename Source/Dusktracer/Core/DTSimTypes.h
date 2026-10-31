// Shared data passed between simulation systems within one fixed step (07_Technical §4, R-TEC-14).
// Every field, writer, reader and shared struct is defined once in 07_Technical §4; specs cite it, never redefine it.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSpatialHash.h"

class FDTRunState;

/** Order of systems inside one fixed step. A system registers for exactly one phase; phases run in enum order. */
enum class EDTSimPhase : uint8
{
	Input,       // latch FDTHeroInput
	Hero,        // read last step's HeroHits, move, dash, aim
	Enemies,     // apply spawns, StatusApplies, Displacements; statuses; move; publish + hash; enemy fire -> HeroHits
	Weapons,     // fire: rounds, arc and mine hits, mine displacements
	Bullets,     // integrate bullets, collide against the enemy hash, append FDTHit
	ResolveHits, // apply hits: damage, deaths -> Hit/Kill events (owner-tagged, R-TEC-01), Kills, compact, republish
	Effects,     // batched hooks: Keeper flags, modifiers, enchantment procs, Intercession
	Pickups,     // gems and chests, XP, level-ups
	Director,    // pacing, drops, markers; telemetry closes the step
	Count
};

/** Enemy role: the key enemies are authored and tagged by (Enemy.<Role>, enemies.md). Boss = the Warden hull. */
enum class EDTEnemyRole : uint8
{
	Swarm,
	Tank,
	Rusher,
	Turret,
	Boss,
	Count
};

/** Elite affixes (Enemy.Affix.<Name>, elites.md), in 09_Catalog/elites.csv order; bits of an affix mask. */
enum class EDTAffix : uint8
{
	Armored,
	Swift,
	Volatile,
	Splitter,
	Vampiric,
	Shielded,
	Count
};

/** Enemy statuses (Status.<Name>, enemies.md R-ENM-21). */
enum class EDTStatus : uint8
{
	Shock,
	Burn,
	Chill,
	Freeze,
	/** Keeper state, per owner (keepers.md R-KEE-24): FDTEnemyPool refuses it. */
	Mark,
	/** Reaches enemies as a short Freeze (keepers.md R-KEE-25); FDTEnemyPool refuses the status itself. */
	Blind,
	Count
};

/** Names one enemy across steps (enemies.md R-ENM-23): valid only while the pool's serial at Index equals Serial. */
struct FDTEnemyHandle
{
	int32 Index = INDEX_NONE;
	uint32 Serial = 0; // never zero for a live enemy
};

/**
 * One hit on one enemy, resolved in ResolveHits. Damage is final (crit, load and stats applied by the emitter).
 * Written by weapons, bullets, enemies (Burn) and effects (resolved next step).
 */
struct FDTHit
{
	int32 EnemyIndex = INDEX_NONE;
	/** The enemy's serial when the hit was made; a hit whose serial no longer matches its index is ignored (R-ENM-23). 0 = not checked. */
	uint32 EnemySerial = 0;
	int32 OwnerId = INDEX_NONE;
	float Damage = 0.0f;
	FVector2f Position = FVector2f::ZeroVector;
	/** Which weapon/system dealt it: an id in FDTTelemetrySources (0 = Unknown). Telemetry only; never gameplay. */
	uint16 SourceId = 0;
	/** The owner's weapon that caused it; INDEX_NONE for Keeper-only hits and stress fire. */
	int8 WeaponSlot = INDEX_NONE;
	bool bCrit = false;
	/** Spawned with pierce above one (the Tank's plate reads it). */
	bool bPierce = false;
};

enum class EDTHeroHitKind : uint8
{
	Contact,
	Bullet,
	/** The boss's phase III hazard ring (boss_warden.csv attack `Hazard`). */
	HazardRing,
	/** A charge band: the Warden's Charge, a Rusher's charge. */
	Charge,
	Sweep,
	DashCharge,
	/** A Volatile elite's death blast (elites.md R-ELT-05). */
	Blast,
	/** A Vampiric elite's drain (elites.md R-ELT-07). */
	Drain
};

/** Damage to a hero, written in Enemies (contact, enemy rounds, boss hazards), read by the hero next step. */
struct FDTHeroHit
{
	int32 PlayerId = INDEX_NONE;
	/** Already scaled by the enemy's DmgScale. */
	float Damage = 0.0f;
	EDTHeroHitKind Kind = EDTHeroHitKind::Contact;
	EDTEnemyRole SourceRole = EDTEnemyRole::Swarm;
	/** Catalog attack id (enemies.csv `attack`, boss_warden.csv `attack`); NAME_None for contact. */
	FName AttackId;
	uint32 EnemySerial = 0;
	FVector2f Position = FVector2f::ZeroVector;
};

/** One death, filled by ResolveHits; read by effects and the director (drops, beat ends). */
struct FDTKill
{
	uint32 EnemySerial = 0;
	EDTEnemyRole Role = EDTEnemyRole::Swarm;
	FVector2f Position = FVector2f::ZeroVector;
	int32 OwnerId = INDEX_NONE;
	uint16 SourceId = 0;
	int8 WeaponSlot = INDEX_NONE;
	bool bElite = false;
	bool bDirector = false;
	int32 BeatId = INDEX_NONE;
};

/** A position change applied by enemies next step, before movement. A stale serial is ignored; the boss hull ignores all. */
struct FDTDisplacement
{
	int32 EnemyIndex = INDEX_NONE;
	uint32 EnemySerial = 0;
	FVector2f Delta = FVector2f::ZeroVector;
	int32 OwnerId = INDEX_NONE;
};

/** A status application (enemies.md Data), appended by effects, applied by enemies at the start of the next Enemies phase. */
struct FDTStatusApply
{
	FDTEnemyHandle Handle;
	EDTStatus Status = EDTStatus::Shock;
	/** Burn: damage per second; Chill: stacks. */
	float Magnitude = 0.0f;
	int32 OwnerId = INDEX_NONE;
	uint16 SourceId = 0;
	/** Burn: extra stacks above C.STATUS.BURN_STACKS_MAX (never above C.STATUS.BURN_STACK_SLOTS). */
	int32 StackBonus = 0;
	/** Chill stacks that freeze; 0 = C.STATUS.FREEZE_STACKS. */
	int32 FreezeAt = 0;
	float FreezeScale = 1.0f;
	/** Spends the status instead of applying it (reactions.md R-REA-07). */
	bool bConsume = false;
};

enum class EDTTelegraphShape : uint8
{
	Band,
	Ring,
	Arc,
	AimLine,
	// Boss shapes (boss.md R-BOS-05, R-BOS-07, boss_warden.csv `telegraph`).
	RingGap,    // a ring of rounds from Origin out to Length, minus its dodge lane: a gap of Width degrees centred on Dir
	SpiralLane, // a spiral's footprint as RingGap: Dir is the rotating lane sector, Width its full angle in degrees
	HazardRing  // the shrinking arena: everything outside the circle of radius Length around Origin
};

/** A projected telegraph (enemies.md Data), published per step by enemies and the boss, drawn in Pal.Telegraph. */
struct FDTTelegraph
{
	EDTTelegraphShape Shape = EDTTelegraphShape::Band;
	FVector2f Origin = FVector2f::ZeroVector;
	FVector2f Dir = FVector2f(1.0f, 0.0f);
	float Length = 0.0f;
	float Width = 0.0f;
	/** Zero at the start of the tell, one when the attack lands. */
	float Progress = 0.0f;
};

/** Where a director drop-in will land (director.md Data). */
struct FDTDropInMarker
{
	EDTEnemyRole Role = EDTEnemyRole::Swarm;
	FVector2f Position = FVector2f::ZeroVector;
	int64 LandStep = 0;
};

/** What systems publish for each other during a step (07_Technical §4). Owned by UDTSimSubsystem. */
struct FDTWorldView
{
	// Hero (Hero phase), by player index; overwritten each step. Stress maps leave a single point at the origin.
	TArray<FVector2f> HeroPositions;
	TArray<FVector2f> HeroAimDirs;
	TArray<bool> HeroIntangible;
	/** The hero lost HP to a hit this step: a touch refused by i-frames or a dash never sets it (Slam's HitTaken reads this). */
	TArray<bool> HeroHurt;
	// Enemies -> hero next step; cleared by the hero after reading.
	TArray<FDTHeroHit> HeroHits;

	// Published by the enemy system in Enemies (dense; index = enemy slot), valid for the rest of the step.
	TArray<FVector2f> EnemyPositions;
	TArray<float> EnemyRadii;
	TArray<uint32> EnemySerials;
	FDTSpatialHash EnemyHash;
	// Weapons -> Bullets: where a released orbiting round aims first (the live boss hull, R-BUL-09).
	bool bBossAlive = false;
	FVector2f BossPosition = FVector2f::ZeroVector;

	// Appended by weapons, bullets, enemies (Burn), effects (next step); consumed and cleared in ResolveHits.
	TArray<FDTHit> Hits;
	// Filled by ResolveHits; cleared at the next ResolveHits.
	TArray<FDTKill> Kills;
	// Effects -> enemies next step; cleared after applying.
	TArray<FDTStatusApply> StatusApplies;
	// Weapons, effects -> enemies next step before movement; cleared after applying.
	TArray<FDTDisplacement> Displacements;
	// Enemies, boss -> render; overwritten.
	TArray<FDTTelegraph> Telegraphs;
	// Director -> render; enemies next step (aim tokens); overwritten.
	TArray<FDTDropInMarker> DropInMarkers;
	bool bThreatBlock = false;
	// Keepers (Effects) -> enemies and boss next step (keepers.md R-KEE-37); one step of enemy time is the step times this.
	float EnemyTimeScale = 1.0f;
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
