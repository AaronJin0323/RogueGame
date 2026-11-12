# Modifiers — S1 slice, S2 arsenal

## Intent
A modifier specialises one weapon: it changes *how* that weapon's rounds behave (split, pierce, pull, leave a zone, copy backwards), never how big its numbers are (`00_Vision §2`, pillar two). Keepers stay the main power source (pillar three); modifiers are the player's tech choices that make a Keeper flag land harder *mechanically*. A modifier is only ever offered, socketed or active on a weapon that can express it, and that is decided by capability tags in data, never by hand.

**The S1 ten** (`09_Catalog/modifiers.csv`; names from `reference/text_tables/ST_Modifiers.csv`):

| Modifier | Requires (any of) | Blocked by | Expresses | Mechanical fit |
|---|---|---|---|---|
| `Mod.SplitCore` | Projectile | Orbit, Lob, Mine, Melee | `Flag.Split` on hit, fan | Storm |
| `Mod.ScatterShell` | Expiry, Mine | — | `Flag.Split` at expiry or detonation, ring | Storm, Tide |
| `Mod.Pierce` | Projectile | — | more pierce | Storm, Tide |
| `Mod.Mirror` | Aimed, Melee | Homing | each volley copied backwards | Forge |
| `Mod.ChainTrigger` | any | — | a kill fires a free volley (R-MOD-16) | Storm, Forge |
| `Mod.LingeringField` | any | — | `Flag.Field` at impacts, sweeps, detonations | Forge, Tide |
| `Mod.Backwash` | Projectile, Lob, Melee | — | `Flag.Pull` toward the round's origin | Tide |
| `Mod.Conduit` | any | — | `Flag.StatusApply` with the patron's status | Storm, Forge, Tide |
| `Mod.Fork` | Aimed, Orbit, Homing, Mine | — | side rounds or extra mines | Storm |
| `Mod.Ricochet` | Projectile | Orbit | `Flag.Ricochet` | Storm |

The fit column names the S1 Paths only; the fits for Shade, Radiance and Quicksilver are in the `fits` column of `modifiers.csv`, from the amplify columns of `keeper_identity_spec.md` §3.4 to §3.6 (`Docs/Validation/S2_fit_data.md`).

**The S2 six** (all non-starters until meta, S3: `DT.Unlock`):

| Modifier | Requires (any of) | Blocked by | Expresses | Rule |
|---|---|---|---|---|
| `Mod.Homing` | Projectile | Orbit, Homing, Beam | rounds steer to the nearest enemy | R-MOD-10 |
| `Mod.OrbitLock` | Projectile | Orbit, Homing, Beam | rounds circle the hero, then one aim | R-MOD-11 |
| `Mod.Boomerang` | Projectile | Orbit, Homing | rounds return, hitting on the way | R-MOD-12 |
| `Mod.SpreadWave` | Expiry, Orbit | Lob | a round's end is a short wave | R-MOD-13 |
| `Mod.Spillover` | any | — | a kill's overkill jumps on | R-MOD-14 |
| `Mod.Refit` | any | — | a full weapon's kill fits another modifier | R-MOD-15 |

Cut from the reference list as multipliers: Overclock, Heavy Slug, Magnify, Crit Lens, Velocity Coil, Burst Capacitor, Lifeline. The reference's matrix differs from these rows where S1 already chose: Split Core and Ricochet stay off the Drones (their flag must resolve `native`), Backwash stays on the Arc Blade. The generated `matrix.csv` is the table.

## Rules
- **R-MOD-01** A modifier never appears on a weapon that cannot express it. It is compatible with a weapon only when the weapon's effective caps (`weapons.md` R-WPN-02) contain at least one of its `required_caps` (empty means any) and none of its `blocked_caps`, **and**, when it carries a `flag`, that flag resolves `native` on the weapon (`weapons.md` R-WPN-08). The offer system never builds an incompatible modifier→weapon card, the socket refuses one, and the composition never applies one.
- **R-MOD-02** Compatibility is one function of the catalogs (`weapons.csv`, `modifiers.csv`, `flag_compat.csv`) used by offers, sockets, composition and tools alike. Its full table is **generated**: `Tools/gen_matrix.py` writes `09_Catalog/matrix.csv`, one row per modifier, one column per weapon, cell `yes` / `no` (gate step `matrix`). The matrix is never hand-edited and never union-merged; on a conflict it is regenerated.
- **R-MOD-03** Every modifier is a specialiser: its effect is a behaviour (a flag kind, a pattern change, a trigger) with its own parameters in `modifiers.csv`. No modifier's effect is a flat damage, fire-rate, size or crit multiplier, and no modifier changes another weapon.
- **R-MOD-04** A modifier is socketed into exactly one weapon (the offer card is pre-bound: modifier → weapon), at most once per weapon (no ranks), and a weapon holds at most `C.MOD.SOCKETS_PER_WEAPON` modifiers. Stacking comes from combining different modifiers with each other and with flags.
- **R-MOD-05** Modifiers and flags feed one accumulator per weapon (`weapons.md` R-WPN-09): counts and magnitudes add, ranges and times take the largest, factors multiply. Two caps hold after every source is summed: a round is at most `C.MOD.SPLIT_GENERATIONS_MAX` split generations away from the weapon's original round (counting `Mod.SplitCore`, `Mod.ScatterShell` and `Flag.Split` together; a child at the limit never splits), and a weapon's composed crit chance is clamped to `C.MOD.CRIT_CHANCE_CAP`.
- **R-MOD-06** Fit with a Keeper archetype is mechanical: the `fits` column says *why* a modifier makes a flag work better (a split fan feeds more chain starts; a pull clusters the crowd for a field). A modifier's effect and parameters are identical whatever Keepers the player holds; there is never a numeric fit bonus. Whether fit changes offer weights is `offers.md`'s call, not this system's.
- **R-MOD-07** Every weapon has at least `C.MOD.MIN_COMPATIBLE` compatible S1 modifiers, counted over the starter rows alone as well as over every modifier, so its sockets always leave a choice.
- **R-MOD-08** When a weapon's effective caps change through a blessing's conversion, each socketed modifier is re-checked; one that has become incompatible goes **dormant**: it keeps its socket, is shown as dormant, and contributes nothing until the weapon can express it again. A dormant modifier is never silently removed. Shown has two places, in `ST_Modifiers.Dormant.*` text on the panel tone (`04_Presentation` R-PRE-08): the HUD lists every dormant socket, weapon and modifier, above the hero block for as long as it sleeps and draws nothing when none does; and a card whose pick would make a live socket dormant names it before the pick, in a box above the rarity row. The card reads the composition on copies of the weapons, never the card's flags or the round count, so it names whatever the composition silences: a conversion that leaves a round flying still adds its cap and silences a modifier that blocks it (`Mod.SplitCore` under `Melee`), and one that converts every round also silences what needs the carrier cap it removes (`weapons.md` R-WPN-10). Only a blessing can silence a socket: a level can only give a weapon `Projectile` back, and no modifier is blocked by it. An evolution silences none: what its caps refuse leaves the socket at the pick and comes back as a reroll (`reforging.md` R-RFG-04, D-073), and its card names that in the same box (`ST_Modifiers.HandBack.*`).
- **R-MOD-09** Rounds, zones and hits a modifier *adds* (split children, fields, mirror copies, fork rounds, free volleys, bounces) carry the `Modifier.<Name>` telemetry source, also when a Keeper conversion folds them into a cleave; a modifier that only changes the weapon's own rounds (pierce) leaves the weapon's source (`weapons.md` R-WPN-07, D-027).
- **R-MOD-10** `Mod.Homing`: the weapon's rounds steer (`bullets.md` R-BUL-09) at `turn_deg_s` onto the nearest enemy within `seek_range_uu` of the round. Children, bounces and other rounds that are not the weapon's own volley fly straight.
- **R-MOD-11** `Mod.OrbitLock`: the weapon's rounds circle the hero at `orbit_radius_uu` for `hold_s`, added to their flight; they can hit while circling, and the hold moves with the hero. At release each round aims once, at the boss or else the nearest enemy within `range_uu` × `range_ratio`, else flies on along its orbit.
- **R-MOD-12** `Mod.Boomerang`: the weapon's rounds fly their range and return, for twice the flight, through the rules of R-BUL-09. A round stopped by a hit turns home at once; the enemy that turned it is not struck again by that round on the way back.
- **R-MOD-13** `Mod.SpreadWave`: a round of the weapon's generation-zero volley whose life runs out strikes an arc of `arc_deg` and `reach_uu` onward, each enemy once, for `wave_damage_ratio` of the round. The wave starts no reaction.
- **R-MOD-14** `Mod.Spillover`: when the weapon's hit kills, the damage past the enemy's health jumps to the nearest other enemy within `seek_range_uu`, next step, as one hit carrying `Modifier.Spillover`; a jump's own overkill does not jump. Health and Shock are read before the step resolves; the Tank's plate is not, so the figure is an estimate.
- **R-MOD-15** `Mod.Refit`: a kill by a weapon with at least `modifiers_fitted` socketed, at most once per `interval_s`, fits a random compatible modifier from the run pool into the first weapon with a free socket (`Combat` stream). No pickup exists in S2: the fit is immediate. With nothing to fit the interval is not spent.
- **R-MOD-16** `Mod.ChainTrigger`: a kill by the weapon fires one free volley of it, at most once per `min_interval_s`, as `Modifier.ChainTrigger` (R-MOD-09) at the weapon's damage with no multiplier. The free volley spends no cooldown, counts as no activation, and takes no charge or Stillness. It acts on every weapon, plain and evolved, each in its own shape, and never starts again what lives one per weapon (D-039; `reforging.md` R-RFG-07, R-RFG-08, R-RFG-12): Blade Halo fires its bolts and its blades each strike once (D-079); Sweep Beam with a beam live gives one tick of the beam along the densest line; Pulse Wall fires its rounds, and with a ring live the ring strikes what stands inside it once more (D-074). Two weapon rules stand over it: a resting Cyclone does not sweep (R-RFG-10), and Singularity's mines join the live mine, which keeps its source (R-RFG-14).

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
| `matrix.csv` | generated catalog | `modifier, <one column per weapon id>`; header comment names the generator |
| `FDTWeaponInstance.Sockets[]` | per weapon | `Mod.*` id plus `bDormant` (R-MOD-08) |
| `DTWeaponCompose::DormantSockets(Player)` | function | the sockets dormant now, read by the HUD |
| `DTOffers::DormantAfterPick(Player, Card)` | function | the live sockets a pick would make dormant; pure, read once per hand shown |

## Acceptance
- T-MOD-01 `Dusktracer.Modifiers.NeverIncompatible`: for every weapon × modifier, the offer generator (run with every modifier in the pool), the socket and the composition agree with `IsCompatible`; no incompatible pair is ever offered, socketed or applied, and every compatible flag-carrying pair resolves `native`.
- T-MOD-02 `Dusktracer.Modifiers.MatrixGenerated`: `IsCompatible` over the catalogs reproduces the expected per-cell table built from the stated rule; the `--check` of `Tools/gen_matrix.py` fails on any hand edit of `matrix.csv`, and every cell equals `IsCompatible`.
- T-MOD-03 `Dusktracer.Modifiers.Specialisers`: no modifier row carries a damage, fire-rate, size or crit multiplier parameter; every row names a behaviour.
- T-MOD-04 `Dusktracer.Modifiers.Sockets`: a second copy on one weapon and a socket beyond the cap are refused; a modifier affects only its own weapon.
- T-MOD-05 `Dusktracer.Modifiers.StackCaps`: `Mod.SplitCore` + `Mod.ScatterShell` + `Flag.Split` never produce a round past the generation cap; crit chance from all sources clamps at the cap.
- T-MOD-06 `Dusktracer.Modifiers.NoFitBonus`: a modifier's composed effect is identical with and without the Keepers its `fits` names.
- T-MOD-07 `Dusktracer.Modifiers.Coverage`: every weapon has at least the minimum compatible modifiers, over the starter rows and over all rows.
- T-MOD-08 `Dusktracer.Modifiers.Dormant`: a `ConvertMelee` that converts every round of a Pulse Rifle makes its `Mod.Pierce` dormant (kept, inert); the modifier wakes when the conversion is gone. `Dusktracer.Modifiers.DormantShown`: `DTOffers::DormantAfterPick` names exactly the sockets the pick then silences in the sim, Split Core and not Scatter Shell on a conversion that leaves a round flying, all four sockets of the worst legal card, nothing for a socket already asleep or for a card without a conversion; reading it changes no run state and draws from no stream; `DTWeaponCompose::DormantSockets` lists them after the pick; an Intercession moves no socket; both text rows carry both names. `Dusktracer.Modifiers.DormantTable`: for every weapon, level and compatible modifier, the card and the sim agree with the table built by hand from R-WPN-10 and the catalogs of what `Blessing.Forge.Hammer.Key` silences, and no level-up silences a live socket.
- T-MOD-09 `Dusktracer.Modifiers.Source`: split children and fields from a modifier carry `Modifier.<Name>`; pierced hits keep the weapon's source. `Dusktracer.Modifiers.FreeVolleySource`: under a Keeper `ConvertMelee` the weapon's own volley is the Keeper's and a `Mod.ChainTrigger` free volley is the modifier's.
- T-MOD-10 `Dusktracer.Arsenal.Homing`: a ring round reaches an enemy off every bearing only with the gyro; a tracked homing round still splits on hit.
- T-MOD-11 `Dusktracer.Arsenal.OrbitLock`: rounds hold the radius, follow the hero, and strike after release.
- T-MOD-12 `Dusktracer.Arsenal.Boomerang`: a piercing lance strikes on the way out and home; a struck plain round turns home unspent; a round home is gone.
- T-MOD-13 `Dusktracer.Arsenal.SpreadWave`: the wave strikes past the round's reach, for its share.
- T-MOD-14 `Dusktracer.Arsenal.Spillover`: one jump of the hit's overkill, from its own source.
- T-MOD-15 `Dusktracer.Arsenal.Refit`: nothing below the fitted count; a kill fits a compatible pool modifier into the first free socket; again after the interval.
- T-MOD-16 `Dusktracer.Modifiers.ChainTrigger.EveryWeapon`: on all ten weapons, plain and evolved, the socket takes the modifier, it is not dormant, and one kill adds damage. `.March`: against a crowd that walks in and dies, no form kills less with the modifier than without. `.BladeHalo`, `.PulseWall`, `.SweepBeam`: every blade once with no turn spent; one strike of the live ring's band, or one tick of the beam on the densest line, with the live one not started again and its own ticks unspent; with none live the free volley starts one as the modifier's. `.Cyclone`, `.Singularity`: the two weapon rules that stand.
- HUMAN: a validator never sees a modifier card that obviously cannot work on its weapon, and can say how a chosen modifier changed what their Keeper does. A validator who takes a card that puts a modifier to sleep can say so, and which one, without being told.

## Telemetry
- Every offer already logs its five cards (`offers.md`); the modifier system adds `modifier_socketed{weapon, modifier}` and `modifier_dormant{weapon, modifier, dormant}` (record `owner` = the player). `modifier_dormant` is written for each change composition finds (R-MOD-08), asleep and awake.
- Damage and kills from modifier-added emissions arrive under `Modifier.<Name>` (R-MOD-09, `telemetry.md` R-TEL-05).

## Text
- Prefix `ST_Modifiers.<Name>.Name` and `ST_Modifiers.<Name>.Desc` per modifier, plus `ST_Modifiers.Dormant.Hud` and `ST_Modifiers.Dormant.Card` for the socket state and `ST_Modifiers.HandBack.Card` and `ST_Modifiers.HandBack.Rerolls` for an evolution's hand-back (`reforging.md` R-RFG-04); budget two rows per modifier plus four; en + zh-Hans. Card lines describe the behaviour, never a number the catalog may change.

## Out of scope
- How the `starter` column becomes the run pool and how offers weight modifiers (`offers.md` R-OFF-02, R-OFF-07).
- Flag meanings and Keeper archetypes (`keepers.md`); status behaviour (`enemies.md`).

## Open
- `Mod.ChainTrigger` snowball in dense crowds is bounded only by its interval and the spawn rate cap → measured in the first steered bot runs. Since D-039 that covers Blade Halo, Pulse Wall and Sweep Beam too, and no bot run holds those pairs yet.
- The card says "one free volley"; on Pulse Wall and Sweep Beam with one live the kill gets one tick of it (R-MOD-16) → read with humans.
- `modifier_socketed` has no caller in the game yet, for offers or `Mod.Refit` → wired with the telemetry owner's next pass.
- `Mod.Refit` has no pickup; the reference's drop is a socket fit → a real pickup waits for art (S4).
- Boomerang gives a lone target one hit per round, not the source's two (R-MOD-12) → read with humans.
- Modifiers a blessing's conversion puts to sleep may need a refund (a reroll) to avoid feeling like a lost pick → decided with `offers.md` after the S1 validation sessions.
- Crit has no S1 modifier source; the cap guards hero base crit and S2 Radiance (`keepers.md` R-KEE-30).
- `Dusktracer.Modifiers.NeverIncompatible` checks the full catalog cross product (declared and converted caps), the socket and the composition. The offer-generator leg of T-MOD-01 is `Dusktracer.Offers.Legality` (`offers.md` T-OFF-03): seeded runs, one seed in four with Gravity Mine, Fork and Ricochet unlocked, show no modifier card `DTModCompat::IsCompatible` refuses (offers call it, `Combat/DTWeaponCatalog.h`).
- Flag-carrying modifiers feed the weapon's flag accumulator as their own entries (source `Modifier.<Name>`, tier below every Keeper source); Pierce, Mirror, Fork and ChainTrigger compose into the spec directly.
