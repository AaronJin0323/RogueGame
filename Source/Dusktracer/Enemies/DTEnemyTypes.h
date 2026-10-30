// Enemy-system types shared by the pool, the subsystem and its callers (02_Systems/enemies.md, director.md Data).
#pragma once

#include "CoreMinimal.h"
#include "Core/DTSimTypes.h"

/**
 * One spawn handed to the enemy subsystem (director.md: FDTEnemySpawn{Role Position HpScale DmgScale bElite bArrival BeatId}).
 * UDTEnemySubsystem::QueueSpawn marks it Director; debug spawns are not.
 */
struct FDTEnemySpawn
{
	EDTEnemyRole Role = EDTEnemyRole::Swarm;
	FVector2f Position = FVector2f::ZeroVector;
	/** Director scaling stamped once at spawn (director.md R-DIR-10): HP and damage multipliers kept for life. */
	float HpScale = 1.0f;
	float DmgScale = 1.0f;
	/** C.ROLE.ELITE_HP_MULT times the scaled HP; otherwise its role (R-ENM-09). */
	bool bElite = false;
	/** Bits of EDTAffix (DTElite::Bit); read only on an elite (elites.md R-ELT-01). */
	uint8 AffixMask = 0;
	/** Director drop-in (R-ENM-13): a Rusher spawns in WindUp, a Turret in its tell when it gets a token. */
	bool bArrival = false;
	/** Director beat tag, copied into FDTKill; INDEX_NONE outside a beat. */
	int32 BeatId = INDEX_NONE;
};

/** Per-enemy flags (enemies.md Data `Flags`). */
namespace DTEnemyFlags
{
	inline constexpr uint8 Elite = 1 << 0;
	inline constexpr uint8 Arrival = 1 << 1;
	inline constexpr uint8 Director = 1 << 2;
}

/** Turret fire patterns, drawn per spawn from the role's `fire_pattern` list (09_Catalog/enemies.csv). */
enum class EDTFirePattern : uint8
{
	Burst, // Turret.Burst: C.ENEMY_FIRE.BURST_SHOTS single aimed rounds
	Fan,   // Turret.Fan: C.ENEMY_FIRE.BURST_SHOTS fans of C.ENEMY_FIRE.FAN_COUNT rounds
	Count
};

/** Role state machine (R-ENM-10 to R-ENM-12). Init resolves on the enemy's first Enemies step. */
enum class EDTEnemyState : uint8
{
	Init,
	Approach, // Swarm, Tank, Rusher: move
	WindUp,   // Rusher: still, direction locked, band telegraph
	Charge,   // Rusher: straight line, no steering, no separation
	Recover,  // Rusher: still
	Idle,     // Turret: waiting for range, period and an aim token
	Tell,     // Turret: aim line, head tracking
	Fire,     // Turret: volleys along the locked aim
	Scripted  // Boss hull: moved by the boss system only
};

/** Role stats and attack ids (R-ENM-09), from the generated catalogue DTCat::Enemies; the boss hull from C.BOSS.*. */
struct DUSKTRACER_API FDTEnemyRoleDef
{
	float Radius = 0.0f;
	float Speed = 0.0f;
	float MaxHp = 0.0f;
	float ContactDamage = 0.0f;
	/** Catalogue attack id (enemies.csv `attack`); NAME_None for roles without an attack. */
	FName AttackId;
	/** The role's fire patterns in catalogue order (Turret only). */
	TArray<EDTFirePattern> Patterns;

	static const FDTEnemyRoleDef& Get(EDTEnemyRole Role);
};
