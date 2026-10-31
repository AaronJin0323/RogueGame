# Weapons — S1 slice, S2 arsenal

## Intent
Weapons are the carriers of the build: each one is a base pattern that fills the screen with cool player fire, and the Keepers change *how* it behaves (`00_Vision §2`, pillars two and three). A weapon is a data row with capability tags, not a class: it declares what it can express, emits rounds into the bullet pool or hits through the shared hit list, and never decides damage outcomes itself. S1 ships the five weapons with stage `S1` in `09_Catalog/weapons.csv`; the five with stage `S2` joined with the arsenal slice.

**The S1 five and why.** Each covers a capability the others lack, and Storm, Forge and Tide each get natural fits (D-002):

| Weapon | Primary caps | Why it is in S1 | Natural fits |
|---|---|---|---|
| `Weapon.PulseRifle` | Projectile, Aimed, Expiry | the baseline aimed bolt; every projectile flag and modifier is native | Storm, Forge, Tide |
| `Weapon.RingEmitter` | Projectile, Radial, Expiry | the danmaku weapon: unaimed, long-lived, screen-filling | Storm, Tide |
| `Weapon.OrbitalDrones` | Orbit, Projectile | the orbit carrier: proves flag fallbacks on a non-flight primary | Storm, Tide |
| `Weapon.ArcBlade` | Melee, Area | the melee carrier: no rounds at all, hits through `FDTHit` | Forge, Tide |
| `Weapon.GravityMine` | Mine, Area | the mine carrier and native Pull: Tide control and Forge zones | Forge, Tide |

**The S2 five** (level rows from `reference/balance/weapons.csv`, all non-starters until meta, S3: `DT.Unlock`):

| Weapon | Pattern, target | Rule | Identity |
|---|---|---|---|
| `Weapon.ScatterCannon` | Cone, Nearest | R-WPN-14 | close-range pellets |
| `Weapon.RailLance` | Line, DensestLine | R-WPN-15 | a slow lance through a crowd's longest line |
| `Weapon.FlakMortar` | Lob, DensestCell | R-WPN-16 | shells that burst on landing |
| `Weapon.SeekerSwarm` | Homing, RandomAlive | R-WPN-17 | missiles that pick their own targets |
| `Weapon.SpiralProjector` | Spiral, None | R-WPN-18 | an unaimed stream, the Ring's danmaku sibling |

## Rules
**List and capability tags**
- **R-WPN-01** Every weapon is one row set in `09_Catalog/weapons.csv` keyed `Weapon.<Name>`; its behaviour comes from its `pattern` (one of `Aimed Radial Orbiter MeleeArc Mine`, S2 adds `Cone Line Lob Homing Spiral`) and its data. No weapon has its own code path beyond its pattern kind; adding a weapon of an existing pattern is a data change only.
- **R-WPN-02** A weapon declares its capability tags `Cap.*` in the `caps` column, most specific first (`Cap.Projectile Beam Orbit Melee Lob Mine Continuous Homing Area Radial Expiry Aimed`). Tags are data (`07_Technical §2`). The weapon's *effective* caps are its declared caps plus or minus any active conversion (R-WPN-10), recomputed whenever a conversion is gained.

**Load**
- **R-WPN-03** A player owns at most `C.WEAPON.SLOTS` weapons. Owned weapons are ranked by level (highest first, ties by acquisition order, earliest first) and every damage value a weapon produces is multiplied by its rank's load factor: `C.LOAD.FACTOR_1`, `C.LOAD.FACTOR_2`, `C.LOAD.FACTOR_3`, `C.LOAD.FACTOR_4`. The rank is recomputed on every level-up and acquisition; the HUD shows it as "Load".

**Level track**
- **R-WPN-04** A weapon levels from the first level to `C.WEAPON.LEVEL_MAX`; each level is one catalog row, and consecutive rows differ on exactly one axis of `damage`, `count`, `cooldown_s` or size (`radius_uu` with `area_r_uu`). Level values are the base the composition starts from (R-WPN-09); leveling is an offer pick (`offers.md`).

**L16 Reforging**
- **R-WPN-05** A weapon's `evolution` column names its row in `09_Catalog/reforging.csv`; `reforging.md` owns what the row does and when it reaches the L16 hand. A reforge that swaps a weapon's caps re-runs modifier compatibility (`modifiers.md` R-MOD-08).

**Ballistics and the bullet-subsystem interface**
- **R-WPN-06** Weapons step in `EDTSimPhase::Weapons`, per player, after `Enemies` published `FDTWorldView`. By carrier:
  - *Projectile rounds* (`Aimed`, `Radial`, the drones' bolts) are emitted only through `UDTBulletSubsystem::Spawn(FDTBulletSpawn)` with `OwnerId`, `SourceId`, `Pierce` from the composed spec and `Life` = `range_uu` / `speed_uu_s`; never an Actor or component per round (R-BUL-07).
  - *Orbiter*: `count` drones circle the hero at `C.WEAPON.DRONE_ORBIT_RADIUS_UU` and `C.WEAPON.DRONE_ORBIT_DEG_S`; the weapon fires one drone at a time, every `cooldown_s` / `count`, at the enemy nearest that drone within `range_uu`. Drones are weapon state, not bullets, and deal no contact damage.
  - *MeleeArc*: a sweep of `C.WEAPON.ARC_DEG` and reach `radius_uu` toward the nearest enemy; a second sweep faces the opposite way. The sweep queries `FDTWorldView::EnemyHash` and appends one `FDTHit` per enemy inside the arc; one sweep hits an enemy at most once. Its hit position is the rim of the enemy facing the sweep's origin, the contact point `enemies.md` R-ENM-10 reads.
  - *Mine*: see R-WPN-13.
- **R-WPN-07** A weapon never changes enemy health, status or death state: every point of damage it causes reaches the enemy as an `FDTHit` resolved in `EDTSimPhase::ResolveHits` (R-BUL-05). Each hit's `SourceId` names its creator: the weapon's base pattern uses its one source `Weapon.<Name>`; an extra emission created by a modifier uses `Modifier.<Name>`, one created by a Keeper flag uses `Keeper.<Path>` (`telemetry.md` R-TEL-05). Reshaping the weapon's own rounds (conversion, charge, stillness) keeps `Weapon.<Name>` when a modifier or the weapon itself causes it, and takes `Keeper.<Path>` when a Keeper flag causes it (a Keeper `Flag.ConvertMelee` cleave is the Keeper's, `keepers.md` R-KEE-13; D-015).

**Flag expression**
- **R-WPN-08** A behaviour flag `Flag.<Name>` (meaning: `keepers.md` §2; a flag no S1 Keeper grants, such as `Ricochet`, means what the `modifiers.csv` row granting it says) is expressed on a weapon by walking its effective caps in listed order through `09_Catalog/flag_compat.csv`; the first cap whose cell is not `none` decides: `native` runs the flag as specified, `fallback:<text>` runs that substitute. Every flag resolves to `native` or `fallback` on every weapon in the catalog, so a blessing always means something on every weapon. The S1 resolutions:

| Flag | Pulse, Ring | Drones (Orbit first) | Arc Blade (Melee first) | Gravity Mine (Mine first) |
|---|---|---|---|---|
| Split | native | ring of children from the hero, every Nth activation | same, every Nth sweep | native: children burst from the detonation |
| Echo | native | native | native | the detonation repeats in place |
| ConvertOrbit | native | one more drone | the sweep becomes a full spin | mines circle the hero before they drop |
| ConvertMelee | native | larger drone contact area | larger arc | larger blast |
| Seek | native | drones pick the target class first | more damage vs the target class | mines drop on the target class |
| Charge, VolleyOnDash | native | native | every Nth sweep charged / a sweep at the dash landing | native |
| Ricochet | native | faster orbit | faster sweep | the mine hops once to the next cluster |
| Slam Field Pull Push Stillness Hunter Nova SpeedScale Chain StatusApply | native | native | native | native |

**Composition and caps**
- **R-WPN-09** A weapon's spec is composed in one order: level row → modifiers → flags → hero stats (`FDTStatSet`) → load factor. Parameters stack the same way for modifiers and flags: counts and magnitudes add, ranges and times take the largest, damage and scale factors multiply, then the per-flag caps (`modifiers.md` R-MOD-05). The product of every multiplying behaviour (echo, split, chain, fork, hunters, extra volleys) is limited to `C.WEAPON.SPAWN_RATE_MAX_PER_S` emissions per weapon; emissions over it are dropped and counted as `Throttled`, never silently. The global `C.BULLETS.CAP` recycle still applies (R-BUL-03).
- **R-WPN-10** A weapon has at most one active pattern conversion (`ConvertMelee` or `ConvertOrbit`); the highest-tier source wins (capstone over key over tier over trait) and conversions never stack. `ConvertMelee` folds every converted round of a volley into one arc whose damage is the converted rounds' summed damage × `C.WEAPON.CONVERT_MELEE_DAMAGE_RATIO`; it is one arc, never one arc per round. A conversion adds its cap (`Melee` or `Orbit`) to the effective caps and removes the original carrier cap only when it converts every round.

**Readability, targeting, determinism**
- **R-WPN-11** Every round, drone bolt, arc, slam ring, field and mine a weapon draws has the cool `Pal.Player.Core` core; tails, arcs, rings and zones take the leading Keeper's `Pal.Keeper.<Path>` or `Pal.Player.TrailNeutral` (R-PRE-01, R-PRE-02). No weapon visual uses red or magenta; melee arcs are never red.
- **R-WPN-12** Targeting is per pattern (`target` column): `Nearest` (to the hero), `NearestToDrone`, `DensestCell` (the enemy-hash cell holding the most enemies within `range_uu`), `None` (Radial fires on cooldown regardless). Ties break on the lowest enemy index. A targeted weapon with no enemy in range does not fire and keeps its cooldown ready. When the player's aim mode is `Manual` (`combat_core.md` R-CMB-04), a `Nearest` weapon fires along `FDTWorldView::HeroAimDirs` instead, whenever ready. Crit is rolled once per emission (a round, or one arc or mine hit) against the composed `CritChance`; a crit multiplies that emission's damage by `CritMult` and sets `bCrit` on its hits. All weapon randomness draws from the `Combat` stream (R-TEC-02); the same seed and inputs give the same emissions and hits.
- **R-WPN-14** Cone: `count` pellets spaced evenly across `C.WEAPON.CONE_DEG`, centred on the target; the spread is geometry, never a roll. A `Mirror` copies each pellet backwards; a `Fork` adds rounds at its own angles.
- **R-WPN-15** Line: enemies within `range_uu` are binned by bearing into `C.WEAPON.LINE_SECTORS` sectors; the sector holding the most (ties: lowest enemy index, R-WPN-12) gives the aim, the mean bearing of its enemies. Rounds leave as `Aimed` lanes. `pierce` counts enemies, so the source's pierce plus the first hit is the row's value.
- **R-WPN-16** Lob: `count` shells go to the densest cells as a Mine's do, repeating the densest cells when the crowd is thin. A shell is weapon state, not a pool round: it flies for distance over `speed_uu_s`, touches nothing on the way and lands as a detonation (one hit per enemy within `area_r_uu`, `Detonation` reactions, `Weapon.<Name>` source). A round-carried flag or modifier that needs flight does not reach it; its hits pull to the launch point for `Mod.Backwash`.
- **R-WPN-17** Homing: each missile draws its own target from the enemies within `range_uu` (`Combat` stream), leaves on a fan of `C.WEAPON.CONE_DEG` around that bearing and steers at `C.WEAPON.SEEKER_TURN_DEG_S` (`bullets.md` R-BUL-09). A missile whose target dies seeks the nearest enemy within `range_uu`.
- **R-WPN-18** Spiral: `count` arms, spaced evenly, each firing one round every `cooldown_s`, regardless of targets; the stream turns by `C.WEAPON.SPIRAL_DEG_S` × `cooldown_s` per volley. Echo on the stream (`Continuous` first in the walk) is `fallback`: every `C.WEAPON.STREAM_ECHO_EVERY_N`-th stream hit strikes again after the flag's delay. A conversion folds its rounds into an arc as on any round pattern (R-WPN-10).
- **R-WPN-13** A Gravity Mine is placed at its target cell, arms after `C.WEAPON.MINE_ARM_S`, then pulls for `C.WEAPON.MINE_PULL_TICKS` ticks of `C.WEAPON.MINE_PULL_TICK_S`: each tick moves enemies within `area_r_uu` × `C.WEAPON.MINE_PULL_RADIUS_RATIO` toward it at `C.WEAPON.MINE_PULL_SPEED_UU_S` (as `FDTDisplacement` entries in `FDTWorldView::Displacements`, `07_Technical §4`, never by writing enemy positions) and hits them for `damage` × `C.WEAPON.MINE_PULL_TICK_DAMAGE_RATIO`; then it detonates, one `FDTHit` per enemy within `area_r_uu`. At most `C.WEAPON.MINE_LIVE_MAX` mines per weapon are live; placing one more detonates the oldest at once.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Weapon slots | C.WEAPON.SLOTS | CORE | R-WPN-03, T-WPN-03 |
| Load factors by rank | C.LOAD.FACTOR_1 … C.LOAD.FACTOR_4 | CORE | R-WPN-03, T-WPN-03 |
| Level cap | C.WEAPON.LEVEL_MAX | CORE | R-WPN-04, T-WPN-04 |
| Spawn rate cap per weapon | C.WEAPON.SPAWN_RATE_MAX_PER_S | GATED(S1) | R-WPN-09, T-WPN-09 |
| Melee conversion damage | C.WEAPON.CONVERT_MELEE_DAMAGE_RATIO | GATED(S1) | R-WPN-10, T-WPN-10 |
| Arc width | C.WEAPON.ARC_DEG | GATED(S1) | R-WPN-06, T-WPN-06 |
| Drone orbit | C.WEAPON.DRONE_ORBIT_RADIUS_UU, C.WEAPON.DRONE_ORBIT_DEG_S | GATED(S1) | R-WPN-06, T-WPN-06 |
| Mine arm, pull, live cap | C.WEAPON.MINE_ARM_S, C.WEAPON.MINE_PULL_TICKS, C.WEAPON.MINE_PULL_TICK_S, C.WEAPON.MINE_PULL_TICK_DAMAGE_RATIO, C.WEAPON.MINE_PULL_RADIUS_RATIO, C.WEAPON.MINE_PULL_SPEED_UU_S, C.WEAPON.MINE_LIVE_MAX | GATED(S1) | R-WPN-13, T-WPN-13 |
| Bullet cap | C.BULLETS.CAP | CORE | R-WPN-09 |
| Per-level stats | `09_Catalog/weapons.csv` rows | GATED(S1), GATED(S2), from MEASURED `reference/balance/weapons.csv` | R-WPN-04, T-WPN-04 |
| Cone, line, spiral, seeker | C.WEAPON.CONE_DEG, C.WEAPON.LINE_SECTORS, C.WEAPON.SPIRAL_DEG_S, C.WEAPON.SEEKER_TURN_DEG_S, C.WEAPON.STREAM_ECHO_EVERY_N | GATED(S2) | R-WPN-14, R-WPN-15, R-WPN-17, R-WPN-18 |

## Data
| Field | Type | Notes |
|---|---|---|
| `weapons.csv` | catalog | `id stage starter caps pattern target fits level damage count cooldown_s radius_uu speed_uu_s range_uu pierce area_r_uu evolution tag ref`; `starter` = in the starter pool (`offers.md` R-OFF-07); every row carries its reach (bot v2 reads it, `bot.md` R-BOT-09) |
| `flag_compat.csv` | catalog | one row per `Flag.*` (all eighteen), one column per `Cap.*`, cell `native` / `fallback:<text>` / `none` |
| `UDTWeaponDef` | data asset, generated from the catalog | `Id` (`Weapon.*` tag), `Caps` (ordered `Cap.*`), `Pattern`, `Target`, `Levels[]`, `TextKey` |
| `FDTWeaponInstance` | per player, in the run state | `Def`, `Level`, `AcquiredOrder`, `Cooldown`, `Sockets[]` (`Mod.*`), `Conversion`, `ShotCounter`, `SourceId` |
| `FDTWeaponSpec` | composed per weapon (R-WPN-09) | `Damage Count Cooldown Radius Speed Life Pierce AreaRadius CritChance CritMult EffectiveCaps Behaviours[]` |
| `FDTBulletSpawn` | `bullets.md` | filled from the spec; `SourceId` per R-WPN-07; `WeaponSlot`, `bCrit` per R-WPN-12 |
| `FDTHit`, `FDTDisplacement` | `07_Technical §4` | hits from arcs, mine ticks and detonations; mine pulls; never a direct damage or position write |

## Acceptance
- T-WPN-01 `Dusktracer.Weapons.CatalogRows`: every `weapons.csv` row parses; every S1 weapon has a row for each level up to `C.WEAPON.LEVEL_MAX` and a known pattern; a weapon def is built from rows alone.
- T-WPN-02 `Dusktracer.Weapons.Caps`: every catalog cap is one of the twelve `Cap.*` tags; effective caps change with a conversion and only then.
- T-WPN-03 `Dusktracer.Weapons.Load`: with one to `C.WEAPON.SLOTS` weapons, rank and damage follow the factors; a level-up that reorders levels reorders load; ties go to the earlier weapon; a pick beyond the slots is refused.
- T-WPN-04 `Dusktracer.Weapons.LevelTrack`: consecutive levels differ on exactly one axis; the level cap holds.
- T-WPN-05 `Dusktracer.Weapons.EvolutionColumn`: every weapon names one Evolve row of `reforging.csv` (the L16 hand is T-RFG-02).
- T-WPN-06 `Dusktracer.Weapons.Emission`: in a headless world each S1 weapon emits per its pattern (rounds via `Spawn` with life from range and speed, drones in turn, arcs as hits, one hit per enemy per sweep) and adds no actor per round.
- T-WPN-07 `Dusktracer.Weapons.HitPath`: enemy health is unchanged until `ResolveHits`; every hit carries the right owner and the source of its creator (weapon, modifier or Keeper).
- T-WPN-08 `Dusktracer.Weapons.FlagResolution`: for every flag and every catalog weapon the resolution is `native` or `fallback` and matches the walk over `flag_compat.csv`.
- T-WPN-09 `Dusktracer.Weapons.ComposeAndThrottle`: composition order and stacking are as stated; a weapon stacked past the spawn rate cap emits no more than the cap and counts the rest.
- T-WPN-10 `Dusktracer.Weapons.Conversion`: two conversions on one weapon leave only the higher-tier one; a converted volley is one arc with the stated damage; a partial conversion keeps `Projectile`.
- T-WPN-11 `Dusktracer.Weapons.Palette`: every weapon visual's core is `Pal.Player.Core` and no weapon visual resolves to a red or magenta token (pairs with T-PRE-01).
- T-WPN-12 `Dusktracer.Weapons.Determinism`: same seed and inputs give identical emissions and hits; a targeted weapon with no enemy in range does not fire (in `Manual` aim a `Nearest` weapon fires along the aim); crit rates over a seeded sample match the composed chance.
- T-WPN-13 `Dusktracer.Weapons.Mine`: arm, pull ticks, detonation and the live cap behave as stated; enemy positions change only through displacement requests.
- T-WPN-14 `Dusktracer.Arsenal.ScatterCannon`: count pellets across the cone, centred on the target; no target, no shot.
- T-WPN-15 `Dusktracer.Arsenal.RailLance`: the lance takes the densest line over the nearest enemy and strikes as many enemies as its pierce.
- T-WPN-16 `Dusktracer.Arsenal.FlakMortar`: no pool round, no hit in flight, one hit per enemy in the blast at landing; no target, no shell.
- T-WPN-17 `Dusktracer.Arsenal.SeekerSwarm`: missiles reach enemies off their launch bearing, spread over the living, same seed same hits; a dead target is replaced.
- T-WPN-18 `Dusktracer.Arsenal.SpiralProjector`: fires with no enemy at the stated rate, each round turned on from the last, one per arm; Echo strikes through its fallback.
- `Dusktracer.Arsenal.Catalog`: ten weapons with full level tracks, a reach and a fit; sixteen modifiers with a fit (T-WPN-01 over the arsenal).
- HUMAN: a validator can say in their own words what each S1 weapon does, and can see the difference when a Keeper flag changes it (split fans on the Ring Emitter, a cleave from the Pulse Rifle, the mine's pull).

## Telemetry
- Per weapon and player, per interval: `weapon{source, level, load_rank, spawned, throttled, hits, damage}` (schema owned by `telemetry.md`; hits and damage already arrive by source through R-TEL-05).
- `weapon_acquired{source}` and `weapon_level{source, level}` (record `owner` = the player) when an offer pick changes the loadout.

## Text
- Prefix `ST_Weapons.<Name>.Name` and `ST_Weapons.<Name>.Desc` per weapon (names from `reference/text_tables/ST_Weapons.csv`), plus `ST_Weapons.Load` for the HUD label; budget two rows per weapon plus one; en + zh-Hans. Offer lines name the weapon's pattern, never a number the catalog may change.

## Out of scope
- Weapon evolutions (`reforging.md`).
- How the `starter` column becomes the run pool, the starting loadout (`offers.md` R-OFF-07, `C.POOL.STARTER_WEAPONS`), the L16 hand (`offers.md` R-OFF-09).
- Flag meanings and parameters, status procs (`keepers.md`); status behaviour and `ResolveHits` (`enemies.md`).
- Per-weapon trails and VFX assets (`04_Presentation`).

## Open
- Source credit for conversions keeps the weapon's source, which may understate the Keeper share at L16 → checked against `Tools/power_split.py` on the first steered bot runs.
- The MEASURED reference shows Gravity Mine an order of magnitude over every other weapon in a dense crowd, and Ring Emitter and Drones never one-shot a late swarm → tuned in the S1 catalog pass against `C.TTK.SWARM_STEERED_MAX`.
- Speeds and ranges are first guesses (no MEASURED value) → set in the S1 feel pass. The S2 five's are logged with hypotheses in `number_change_log.csv` and read in `Validation/S2_arsenal.md`.
- `ConvertMelee` folds a Scatter Cannon's whole volley, or the Spiral's stream at its own cadence, into arcs (R-WPN-10) → read in the Keeper arms of `Validation/S2_arsenal.md`; no number changed yet.
- A Lob's cells are chosen at launch; a shell does not follow the crowd.
- Implementation notes (S1 build): the flag seam is `UDTWeaponSubsystem::AddFlagSource(OwnerId, SourceTag, FDTWeaponFlags)`; entries stack per (flag, telemetry source, `on` word), so Keeper lines with another trigger need their own `on` (`Volley`, `Pulled`, `ChainHit`, `Cleave`). Split children of a carrier with no round speed (arcs, mines) use `C.WEAPON.CHILD_*`. Field ticks every fixed step. Hit-taken and dash-landing slams fire once per player, from the highest-load weapon carrying Slam.
- `FDTWeaponSim::SetEnemies` gives the weapon side a read-only view of the enemy pool: status masks at impact (`FImpact::StatusMask`, a lethal hit still reads it), elites and the boss, and the step's death records (`FDTEnemyPool::GetStepDeaths`). Weapons still write only hits, statuses and displacements.
- Seek, Hunter, Debt, Nova, Charge, CritSplit, VolleyOnDash, SpeedScale and the Ricochet fallbacks are `keepers.md` §9. Not yet expressed, for lack of a source or input → with keepers/enemies: Stillness `window_s`, ConvertOrbit native, Push; a key-only ConvertMelee size fallback (drones, mine) reads only `radius_scale`.
- Rounds that split, bounce or pull to their origin are mirrored by the weapon system to recover generation, origin and expiry; a round recycled at `C.BULLETS.CAP` is missed → revisit if `Recycled` climbs.
