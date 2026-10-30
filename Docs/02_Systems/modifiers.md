# Modifiers — S1 slice

## Intent
A modifier specialises one weapon: it changes *how* that weapon's rounds behave (split, pierce, pull, leave a zone, copy backwards), never how big its numbers are (`00_Vision §2`, pillar two). Keepers stay the main power source (pillar three); modifiers are the player's tech choices that make a Keeper flag land harder *mechanically*. A modifier is only ever offered, socketed or active on a weapon that can express it, and that is decided by capability tags in data, never by hand.

**The S1 ten** (`09_Catalog/modifiers.csv`; names from `reference/text_tables/ST_Modifiers.csv`):

| Modifier | Requires (any of) | Blocked by | Expresses | Mechanical fit |
|---|---|---|---|---|
| `Mod.SplitCore` | Projectile | Orbit, Lob, Mine, Melee | `Flag.Split` on hit, fan | Storm |
| `Mod.ScatterShell` | Expiry, Mine | — | `Flag.Split` at expiry or detonation, ring | Storm, Tide |
| `Mod.Pierce` | Projectile | — | more pierce | Storm, Tide |
| `Mod.Mirror` | Aimed, Melee | Homing | each volley copied backwards | Forge |
| `Mod.ChainTrigger` | any | — | a kill fires a free volley | Storm, Forge |
| `Mod.LingeringField` | any | — | `Flag.Field` at impacts, sweeps, detonations | Forge, Tide |
| `Mod.Backwash` | Projectile, Lob, Melee | — | `Flag.Pull` toward the round's origin | Tide |
| `Mod.Conduit` | any | — | `Flag.StatusApply` with the patron's status | Storm, Forge, Tide |
| `Mod.Fork` | Aimed, Orbit, Homing, Mine | — | side rounds or extra mines | Storm |
| `Mod.Ricochet` | Projectile | Orbit | `Flag.Ricochet` | Storm |

Cut from the reference list as multipliers: Overclock, Heavy Slug, Magnify, Crit Lens, Velocity Coil, Burst Capacitor, Lifeline. Deferred to S2 with their weapons: Homing Gyro, Orbit Lock, Boomerang, Spread Wave, Spillover, Refit.

## Rules
- **R-MOD-01** A modifier never appears on a weapon that cannot express it. It is compatible with a weapon only when the weapon's effective caps (`weapons.md` R-WPN-02) contain at least one of its `required_caps` (empty means any) and none of its `blocked_caps`, **and**, when it carries a `flag`, that flag resolves `native` on the weapon (`weapons.md` R-WPN-08). The offer system never builds an incompatible modifier→weapon card, the socket refuses one, and the composition never applies one.
- **R-MOD-02** Compatibility is one function of the catalogs (`weapons.csv`, `modifiers.csv`, `flag_compat.csv`) used by offers, sockets, composition and tools alike. Its full table is **generated**: `Tools/gen_matrix.py` (planned) writes `09_Catalog/matrix.csv`, one row per modifier, one column per weapon, cell `yes` / `no`. The matrix is never hand-edited and never union-merged; on a conflict it is regenerated. In S1, until the tool lands, the rule and the test (T-MOD-02) are the source; no matrix is committed by hand.
- **R-MOD-03** Every modifier is a specialiser: its effect is a behaviour (a flag kind, a pattern change, a trigger) with its own parameters in `modifiers.csv`. No modifier's effect is a flat damage, fire-rate, size or crit multiplier, and no modifier changes another weapon.
- **R-MOD-04** A modifier is socketed into exactly one weapon (the offer card is pre-bound: modifier → weapon), at most once per weapon (no ranks), and a weapon holds at most `C.MOD.SOCKETS_PER_WEAPON` modifiers. Stacking comes from combining different modifiers with each other and with flags.
- **R-MOD-05** Modifiers and flags feed one accumulator per weapon (`weapons.md` R-WPN-09): counts and magnitudes add, ranges and times take the largest, factors multiply. Two caps hold after every source is summed: a round is at most `C.MOD.SPLIT_GENERATIONS_MAX` split generations away from the weapon's original round (counting `Mod.SplitCore`, `Mod.ScatterShell` and `Flag.Split` together; a child at the limit never splits), and a weapon's composed crit chance is clamped to `C.MOD.CRIT_CHANCE_CAP`.
- **R-MOD-06** Fit with a Keeper archetype is mechanical: the `fits` column says *why* a modifier makes a flag work better (a split fan feeds more chain starts; a pull clusters the crowd for a field). A modifier's effect and parameters are identical whatever Keepers the player holds; there is never a numeric fit bonus. Whether fit changes offer weights is `offers.md`'s call, not this system's.
- **R-MOD-07** Every S1 weapon has at least `C.MOD.MIN_COMPATIBLE` compatible S1 modifiers, counted over the starter rows alone as well as over all S1 modifiers, so its sockets always leave a choice.
- **R-MOD-08** When a weapon's effective caps change (a conversion now, a reforge in S2), each socketed modifier is re-checked; one that has become incompatible goes **dormant**: it keeps its socket, is shown as dormant, and contributes nothing until the weapon can express it again. A dormant modifier is never silently removed.
- **R-MOD-09** Rounds, zones and hits a modifier *adds* (split children, fields, mirror copies, fork rounds, free volleys, bounces) carry the `Modifier.<Name>` telemetry source; a modifier that only changes the weapon's own rounds (pierce) leaves the weapon's source (`weapons.md` R-WPN-07).

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Sockets per weapon | C.MOD.SOCKETS_PER_WEAPON | GATED(S1) | R-MOD-04, T-MOD-04 |
| Minimum compatible per weapon | C.MOD.MIN_COMPATIBLE | GATED(S1) | R-MOD-07, T-MOD-07 |
| Split generations | C.MOD.SPLIT_GENERATIONS_MAX | CORE | R-MOD-05, T-MOD-05 |
| Crit chance cap | C.MOD.CRIT_CHANCE_CAP | CORE | R-MOD-05, T-MOD-05 |
| Starter modifier pool size | C.POOL.STARTER_MODIFIERS | CORE | R-MOD-07 (the `starter` rows) |
| Spawn rate cap per weapon | C.WEAPON.SPAWN_RATE_MAX_PER_S | GATED(S1) | R-MOD-05 |
| Per-modifier parameters | `09_Catalog/modifiers.csv` `param_n`/`value_n` | GATED(S1), from MEASURED reference text | R-MOD-03 |

## Data
| Field | Type | Notes |
|---|---|---|
| `modifiers.csv` | catalog | `id stage starter flag required_caps blocked_caps effect param_1 value_1 param_2 value_2 param_3 value_3 fits text_key tag ref` |
| `required_caps` / `blocked_caps` | `;`-separated `Cap.*` names | any-of / none-of; empty `required_caps` means any weapon |
| `flag` | `Flag.*` or empty | the behaviour kind the modifier grants; makes the native check of R-MOD-01 apply |
| `UDTModifierDef` | data asset, generated from the catalog | `Id` (`Mod.*` tag), `RequiredCaps`, `BlockedCaps`, `Flag`, `Params`, `Fits`, `TextKey` |
| `DTModCompat::IsCompatible(Weapon, Modifier)` | function | the single compatibility function of R-MOD-02 |
| `matrix.csv` | generated catalog (planned) | `modifier, <one column per weapon id>`; header comment names the generator |
| `FDTWeaponInstance.Sockets[]` | per weapon | `Mod.*` id plus `bDormant` (R-MOD-08) |

## Acceptance
- T-MOD-01 PLANNED(S1) `Dusktracer.Modifiers.NeverIncompatible`: for every S1 weapon × modifier, the offer generator (run with every modifier in the pool), the socket and the composition agree with `IsCompatible`; no incompatible pair is ever offered, socketed or applied, and every compatible flag-carrying pair resolves `native`.
- T-MOD-02 PLANNED(S1) `Dusktracer.Modifiers.MatrixGenerated`: `IsCompatible` over the catalogs reproduces the expected per-cell table built from the stated rule; once `Tools/gen_matrix.py` exists, its `--check` fails on any hand edit of `matrix.csv`.
- T-MOD-03 PLANNED(S1) `Dusktracer.Modifiers.Specialisers`: no modifier row carries a damage, fire-rate, size or crit multiplier parameter; every row names a behaviour.
- T-MOD-04 PLANNED(S1) `Dusktracer.Modifiers.Sockets`: a second copy on one weapon and a socket beyond the cap are refused; a modifier affects only its own weapon.
- T-MOD-05 PLANNED(S1) `Dusktracer.Modifiers.StackCaps`: `Mod.SplitCore` + `Mod.ScatterShell` + `Flag.Split` never produce a round past the generation cap; crit chance from all sources clamps at the cap.
- T-MOD-06 PLANNED(S1) `Dusktracer.Modifiers.NoFitBonus`: a modifier's composed effect is identical with and without the Keepers its `fits` names.
- T-MOD-07 PLANNED(S1) `Dusktracer.Modifiers.Coverage`: every S1 weapon has at least the minimum compatible modifiers, over the starter rows and over all S1 rows.
- T-MOD-08 PLANNED(S1) `Dusktracer.Modifiers.Dormant`: a `ConvertMelee` that converts every round of a Pulse Rifle makes its `Mod.Pierce` dormant (kept, inert); the modifier wakes when the conversion is gone.
- T-MOD-09 PLANNED(S1) `Dusktracer.Modifiers.Source`: split children and fields from a modifier carry `Modifier.<Name>`; pierced hits keep the weapon's source.
- HUMAN: a validator never sees a modifier card that obviously cannot work on its weapon, and can say how a chosen modifier changed what their Keeper does.

## Telemetry
- Every offer already logs its five cards (`offers.md`); the modifier system adds `modifier_socketed{weapon, modifier}` and `modifier_dormant{weapon, modifier, dormant}` (record `owner` = the player).
- Damage and kills from modifier-added emissions arrive under `Modifier.<Name>` (R-MOD-09, `telemetry.md` R-TEL-05).

## Text
- Prefix `ST_Modifiers.<Name>.Name` and `ST_Modifiers.<Name>.Desc` per modifier, plus `ST_Modifiers.Dormant` for the socket state; budget two rows per modifier plus one; en + zh-Hans. Card lines describe the behaviour, never a number the catalog may change.

## Out of scope
- How the `starter` column becomes the run pool and how offers weight modifiers (`offers.md` R-OFF-02, R-OFF-07).
- Reforge refunds and the S2 modifiers; `Tools/gen_matrix.py` itself (planned, S1 tooling brief).
- Flag meanings and Keeper archetypes (`keepers.md`); status behaviour (`enemies.md`).

## Open
- `Mod.ChainTrigger` snowball in dense crowds is bounded only by its interval and the spawn rate cap → measured in the first steered bot runs.
- Dormant modifiers may need a refund (a reroll) to avoid feeling like a lost pick → decided with `offers.md` after the S1 validation sessions.
- Crit has no S1 modifier source; the cap guards hero base crit and S2 Radiance → revisit when Radiance lands.
