// Argument types of the S1 telemetry record writers (02_Systems/telemetry.md §Data; schemas in each owning spec's §Telemetry).
// Plain structs and enums: any package includes this (and DTTelemetryCore.h) without pulling in a world.
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"
#include "Core/DTSimTypes.h"

/** Last gameplay input device of a player (combat_core.md; CSV `input_device`). */
enum class EDTTelInputDevice : uint8
{
	Unknown,
	Kbm,
	Pad
};

/** offer_tool.tool (offers.md R-OFF-10). */
enum class EDTTelOfferTool : uint8
{
	Reroll,
	Banish,
	Lock
};

/** keeper_commit.role (keepers.md R-KEE-05): Patron at run start, Second at the L8 commitment. */
enum class EDTTelKeeperRole : uint8
{
	Patron,
	Second
};

/** keeper_pick.role: the blessing's role in its archetype (keepers.md Data, UDTBlessingDef Role). */
enum class EDTTelBlessingRole : uint8
{
	Key,
	Amp,
	Trait
};

/** keeper_pick (keepers.md §Telemetry), one per blessing taken. */
struct FDTTelKeeperPick
{
	FName Blessing;
	EDTPath Path = EDTPath::None;
	FName Archetype;
	EDTTelBlessingRole Role = EDTTelBlessingRole::Key;
	/** That Keeper's Favor and unlocked tier after the pick. */
	int32 Favor = 0;
	int32 Tier = 0;
	/** That Keeper's leading archetype after the pick; NAME_None = none (written as null). */
	FName Leading;
};

/** intercession (keepers.md §Telemetry), one per cast. NAME_None parts are written as null. */
struct FDTTelIntercession
{
	FName Base;
	FName Variant;
	FName Layer;
	int32 EnemiesHit = 0;
	float Damage = 0.0f;
};

/** dir_block (director.md R-DIR-15, §Telemetry), one per ended block. Strings are the timeline's spelling. */
struct FDTTelDirBlock
{
	int32 Block = 0;
	/** Harvest, Threat, Breath or Boss. */
	FString Phase;
	float RowStartSeconds = 0.0f;
	float StartSeconds = 0.0f;
	float EndSeconds = 0.0f;
	/** What ended it (e.g. duration, cleared, next_row, boss). */
	FString EndedBy;
	int32 SpawnedByRole[static_cast<int32>(EDTEnemyRole::Count)] = {};
	int32 SkippedAtCap = 0;
	int32 DropIns = 0;
	int32 ThreatActions = 0;
	int32 Kills = 0;
	/** Median distance from the killing player's hero at each kill; negative = no kill (written as null). */
	float KillHorizonP50UU = -1.0f;
	int32 AlivePeak = 0;
	/** By owner: applied damage as a share of max HP (see UDTTelemetrySubsystem run totals) and dashes during the block. */
	TArray<float> DamageTakenShare;
	TArray<int32> Dashes;
};

/** One player's part of boss_end (boss.md §Telemetry). */
struct FDTTelBossPlayer
{
	/** By catalog attack id (contact = NAME_None, written "Contact"). */
	TMap<FName, float> DamageTakenByAttack;
	int32 Dashes = 0;
	float HitsPerRing = 0.0f;
};

/** boss_end (boss.md §Telemetry); owner -1. */
struct FDTTelBossEnd
{
	/** e.g. win, death, quit. */
	FString Outcome;
	float FightTimeSeconds = 0.0f;
	float HpLeftFrac = 0.0f;
	int32 Phase = 0;
	/** By owner. */
	TArray<FDTTelBossPlayer> PerPlayer;
};

/** elite (elites.md §Telemetry): one per elite death. */
struct FDTTelElite
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

/** Spelling helpers shared by the writers and tests. */
namespace DTTelNames
{
	DUSKTRACER_API const TCHAR* InputDevice(EDTTelInputDevice Device);
	DUSKTRACER_API const TCHAR* OfferTool(EDTTelOfferTool Tool);
	DUSKTRACER_API const TCHAR* KeeperRole(EDTTelKeeperRole Role);
	DUSKTRACER_API const TCHAR* BlessingRole(EDTTelBlessingRole Role);
	DUSKTRACER_API const TCHAR* CardKind(EDTCardKind Kind);
	DUSKTRACER_API const TCHAR* HandKind(EDTHandKind Kind);
	DUSKTRACER_API const TCHAR* Rarity(EDTRarity Rarity);
	DUSKTRACER_API const TCHAR* EnemyRole(EDTEnemyRole Role);
	DUSKTRACER_API const TCHAR* Status(EDTStatus Status);
	DUSKTRACER_API const TCHAR* HeroHitKind(EDTHeroHitKind Kind);
}
