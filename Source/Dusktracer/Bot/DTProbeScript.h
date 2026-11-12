// A probe scene for the bot (S1 tool; Docs/Validation/S1_probes.md): a build and a refilled field, written as text so
// Tools/probe.py can edit it between runs (drop the Keepers, the modifiers, a cast). Plain C++, no engine state.
//   patron Storm                               the Patron (the bot answers the Patron hand with it)
//   second Tide                                the second Keeper, committed at once
//   bless Blessing.Storm.Tempest.Key           one line per blessing, in pick order
//   weapon Weapon.PulseRifle 6 Mod.Fork ...    id, level, socketed modifiers; one line per weapon, in slot order
//   field 120                                  enemies kept alive (0 = no field)
//   hp_scale 25.5                              HP scale of those enemies
//   warmup_s 62                                seconds before the measured window
//   window_s 12                                seconds of the window; the run ends with it
//   cast_at_s 62                               Intercession press at this time; absent = the bot never casts
//   reforge Evolve.RingEmitter                 one Reforging card (a Path transformation or a weapon's evolution), applied as the pick
//                                              applies it (DTOffers::ApplyPickState) once the build is in; absent = none (D-082)
#pragma once

#include "CoreMinimal.h"
#include "Core/DTRunState.h"

struct FDTProbeWeapon
{
	FName Id;
	int32 Level = 1;
	TArray<FName> Mods;
};

struct DUSKTRACER_API FDTProbeScript
{
	EDTPath Patron = EDTPath::None;
	EDTPath Second = EDTPath::None;
	TArray<FName> Blessings;
	TArray<FDTProbeWeapon> Weapons;
	int32 Field = 0;
	float HpScale = 1.0f;
	float WarmupS = 0.0f;
	float WindowS = 0.0f;
	/** Below zero: no cast. */
	float CastAtS = -1.0f;
	/** Above zero: while the cast runs the route dashes this often (Slam's variant is its dash landings). */
	float CastDashS = 0.0f;
	/** D-082: one Reforging card's id (reforging.csv); none = no pick. An evolution's target is its row's source weapon. */
	FName Reforge;

	/** Replaces this script with Text; false and a reason on a bad line or a missing patron. */
	bool Parse(const FString& Text, FString& OutError);
	FString ToText() const;
	/** The build a player holds now (a `field` and timings stay at their defaults). */
	static FDTProbeScript FromPlayer(const FDTPlayerRunState& Player);
};
