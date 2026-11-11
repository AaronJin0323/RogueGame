# S2 fit data: Shade, Radiance and Quicksilver in `fits` (2026-11-11)

Build: `0a4b56d` on `s2/fit-and-unlock`, from `dev/s2` at `146af0a`. Class: BUG (data). Layer: the catalogs (`09_Catalog/weapons.csv`, `modifiers.csv`) and the catalog check. Replaces: empty S2 Paths in every `fits` cell. No number moved, no other column moved, no S1 Path's entry moved. Nothing was built, run or read in the engine for this note: it states what the data says, not what a run does.

## The defect
On `dev/s2` at `146af0a` no `fits` cell of `weapons.csv` or `modifiers.csv` names `Shade`, `Radiance` or `Quicksilver`. `fits` is read per Path (`offers.md` R-OFF-02, `Offers/DTOfferGenerator.cpp` `DTOffers::Weight`): a new weapon that fits a Path the player owns a card of is × `C.OFFER.WEIGHT_WEAPON_FIT` (1.5); a modifier that amplifies one is × `C.OFFER.WEIGHT_MOD_AMPLIFIES` (2.0), and one that amplifies nothing owned × `C.OFFER.WEIGHT_MOD_NO_AMPLIFY` (0.7). So a player holding only S2 Keepers saw every modifier at 0.7 and no weapon at 1.5. Bot v1 also scores a weapon card +10 when its `fits` names the bot's own Path (`Bot/DTBotBrain.cpp` `CardScore`, which bot v2's `CardScoreV2` starts from), so a Steered bot of an S2 Path never had that lift.

## Where the cells come from
Source: `Handoff_Dusktracer/game/reference/specs/keeper_identity_spec.md`.
- **Weapons: §3.7, the D-59 fit table (lines 246 to 254).** Line 183 says §3.7 supersedes the per-archetype "Best weapons" columns of §3.1 to §3.6, and line 188 says each archetype names three fit weapons used only for the resonance mark and the offer weight, which is what `fits` is. The §3.4 to §3.6 "Best weapons" columns (lines 140 to 142, 155 to 157, 170 to 172) are not used; see Judgement calls.
- **Modifiers: the "Modifiers that amplify it" columns of §3.4 to §3.6** (lines 140 to 142, 155 to 157, 170 to 172; §5.3, line 350, names this column as the modifier's amplification). §3.7 does not supersede them.

The catalog's grain is the Path (one bit per Path, `gen_catalog.py` `path_mask`), so a weapon or modifier gains a Path when the source names it for any archetype of that Path. The archetypes are the nine S2 rows of `expression.csv`: Shade Hound, Hunters (Collectors), Economy (Ledger); Radiance Lance, Prism, Solar; Quicksilver Vector, Momentum, Rebound.

## Weapons (every level row of a weapon carries the same cell)
| Weapon | Before | After | Source (D-59, `keeper_identity_spec.md`) |
|---|---|---|---|
| `Weapon.PulseRifle` | `Storm;Forge;Tide` | `Storm;Forge;Tide;Radiance;Quicksilver` | Lance :249, Vector :252, Rebound :254 |
| `Weapon.RingEmitter` | `Storm;Tide` | `Storm;Tide;Shade;Quicksilver` | Hound :246, Ledger :248, Rebound :254 |
| `Weapon.OrbitalDrones` | `Storm;Tide` | `Storm;Tide;Shade;Radiance;Quicksilver` | Collectors :247, Prism :250, Momentum :253 |
| `Weapon.ArcBlade` | `Forge;Tide` | `Forge;Tide;Radiance;Quicksilver` | Prism :250, Vector :252, Momentum :253 |
| `Weapon.GravityMine` | `Forge;Tide` | `Forge;Tide;Shade;Radiance` | Collectors :247, Solar :251 |
| `Weapon.ScatterCannon` | `Storm;Forge;Tide` | `Storm;Forge;Tide;Shade;Radiance;Quicksilver` | Hound :246, Prism :250, Vector :252 |
| `Weapon.RailLance` | `Storm;Tide` | `Storm;Tide;Radiance` | Lance :249, Solar :251 |
| `Weapon.FlakMortar` | `Storm;Forge;Tide` | `Storm;Forge;Tide;Radiance` | Lance :249, Solar :251 |
| `Weapon.SeekerSwarm` | `Storm;Tide` | `Storm;Tide;Shade;Quicksilver` | Collectors :247, Ledger :248, Rebound :254 |
| `Weapon.SpiralProjector` | `Storm;Tide` | `Storm;Tide;Shade;Quicksilver` | Hound :246, Ledger :248, Momentum :253 |

Each of the nine archetypes has its three D-59 weapons, all in the catalog. Every one can carry its archetype's key flag: `flag_compat.csv` has no `none` for `Seek`, `Hunter`, `Debt`, `Charge`, `CritSplit`, `Nova`, `VolleyOnDash`, `SpeedScale` or `Ricochet` on any primary capability (Arc Blade takes `Charge` and `VolleyOnDash` by their Melee fallbacks).

## Modifiers (the S1 notes are kept as they were; the new notes are appended)
| Modifier | Before | Added | Source (`keeper_identity_spec.md`) |
|---|---|---|---|
| `Mod.Homing` | `Storm: every round finds a chain start; Tide: stray rounds still reach the pulled crowd` | `Shade: every round hunts the crowd` | Hound :140 (Homing Gyro) |
| `Mod.Pierce` | `Storm: chains start from every pierced enemy; Tide: lines of Chill` | `Shade: a seeking round runs through the line it hunts; Radiance: lances carry through more of the line; Quicksilver: pierced rounds still bounce on` | Hound :140, Lance :155, Rebound :172 |
| `Mod.ChainTrigger` | `Storm: cascades in crowds; Forge: kills feed more cleaves` | `Shade: Marked kills feed more hunters` | Collectors :141 |
| `Mod.SplitCore` | `Storm: split fans fill the screen` | `Shade: hunters fire the fans too; Radiance: more rounds and more crits to split; Quicksilver: children are more rounds to bounce` | Collectors :141 (hunters inherit modifiers, :222), Prism :156, Rebound :172 |
| `Mod.Fork` | `Storm: denser fans` | `Shade: paydays fan wider; Radiance: side rounds are more crit chances; Quicksilver: dash volleys fan wider` | Ledger :142, Prism :156, Vector :170 |
| `Mod.ScatterShell` | `Storm: rings of children; Tide: rings seed the pull point` | `Shade: payday rounds burst into rings` | Ledger :142 |
| `Mod.LingeringField` | `Forge: burning ground; Tide: zones hold the pulled crowd` | `Radiance: zones hold the crowd for overkill novas` | Solar :157 |
| `Mod.Mirror` | `Forge: a second cleave behind you` | `Quicksilver: a dash volley also fires behind` | Vector :170 |
| `Mod.Ricochet` | `Storm: every bounce is another chain start` | `Quicksilver: more bounces that seek` | Rebound :172 (Ricochet Plate) |

Every pair can meet in a run: each modifier is compatible (`matrix.csv`) with at least one D-59 weapon of the archetype that names it (Split Core on Collectors through Seeker Swarm, Fork on Ledger through Seeker Swarm, Homing on Hound through Ring, Scatter and Spiral). No `required_caps`, `blocked_caps` or `IncompatFlags` cell rules a pair out.

## Left out, and why
- **Not in this catalog** (the source names them; `modifiers.md` cut all seven from the reference list as multipliers, R-MOD-03, so no row exists): Velocity Coil (Hound :140, Lance :155, Momentum :171), Crit Lens (Hound :140, Lance :155, Prism :156), Lifeline (Collectors :141), Burst Capacitor (Ledger :142), Heavy Slug (Lance :155, Solar :157), Magnify (Solar :157), Overclock (Vector :170, Momentum :171).
- **No catalog modifier for Quicksilver Momentum.** The source names only Velocity Coil and Overclock (:171), neither a row here. At the catalog's grain (the Path) Momentum is covered by the other Quicksilver rows (Pierce, Split Core, Fork, Mirror, Ricochet), so T-OFF-19 holds and a Momentum player's modifiers still read ×2.0; but no modifier is there *because* of Momentum. This is the one archetype gap; filling it is a content or design call.
- **Silent in the source:** `Mod.Conduit` (every hit applies the Patron's status; Shade's would be Mark, `expression.csv` Hound Key) is named by no S2 archetype, nor are `Mod.Backwash`, `Mod.OrbitLock`, `Mod.Boomerang`, `Mod.SpreadWave`, `Mod.Spillover` or `Mod.Refit` (`Mod.Refit` fits every S1 Path in this catalog with the same note; the source does not name it for S2). None was given an S2 Path.
- **The replaced columns.** The §3.4 to §3.6 "Best weapons" columns differ from D-59 (for example Hound: Pulse, Rail, Scatter, Ring at :140 against Ring, Spiral, Scatter at :246; Momentum: "any" at :171). They are not used.

## S1 fits come from the older columns (not moved)
The S1 Paths' weapon fits (Storm, Forge, Tide) were written from the older "Best weapons" columns of §3.1 to §3.3 and were not moved by this job, so S1 and S2 fits now come from two generations of the same source. Moving the S1 fits to D-59 would change S1 offer weights (and the bots' weapon score) on `main` and is not done here. Where D-59, read over the catalog's S1 archetypes (Tempest, Arc Grid, Downpour; Hammer, Furnace, Slam; Whirlpool, Frost, Stillness; `keeper_identity_spec.md` :235 to :245), and the CSV's S1 Paths disagree:

| Weapon | CSV S1 Paths | D-59 S1 Paths | CSV has, D-59 does not | D-59 has, CSV does not |
|---|---|---|---|---|
| `Weapon.PulseRifle` | Storm, Forge, Tide | Forge (Hammer), Tide (Frost) | Storm | none |
| `Weapon.RingEmitter` | Storm, Tide | Storm (Tempest), Forge (Slam), Tide (Whirlpool) | none | Forge |
| `Weapon.OrbitalDrones` | Storm, Tide | Storm (Arc Grid), Forge (Slam), Tide (Frost) | none | Forge |
| `Weapon.ArcBlade` | Forge, Tide | Storm (Arc Grid), Forge (Hammer, Slam) | Tide | Storm |
| `Weapon.GravityMine` | Forge, Tide | Storm (Downpour), Forge (Furnace), Tide (Whirlpool, Stillness) | none | Storm |
| `Weapon.ScatterCannon` | Storm, Forge, Tide | Storm (Tempest), Forge (Hammer) | Tide | none |
| `Weapon.SpiralProjector` | Storm, Tide | Storm (Tempest), Forge (Furnace) | Tide | Forge |

Rail Lance, Flak Mortar and Seeker Swarm agree. For comparison, the CSV's S1 Paths are the union of the older columns over the same archetypes (:93 to :127; Slam's "any" at :111 read as no list) with two differences: Tide also holds Arc Blade and Scatter Cannon, and Forge does not hold Orbital Drones. No change is made here.
## The check (T-OFF-19)
`Tools/gen_catalog.py --check` (gate step `catalog`) now fails when an archetype of `expression.csv` has no weapon and no modifier, of its stage or an earlier one, whose `fits` names its Path (`fit_gaps`). On `dev/s2` at `146af0a` it names 18 gaps (each of the nine S2 archetypes, weapon and modifier). On `0a4b56d` it names none. `Tools/tools_test.py` `CatalogFit` proves it on copies of the catalogs: a Path left out of the weapons or of the modifiers gives one gap per archetype of that Path, an S2 row does not cover an S1 archetype, and the gap fails `main --check`. This is a Python check, not a C++ automation test, so `C.GATE.TEST_FLOOR` does not move.

## The `-Unlock S2` expansion (change b, `31b32ad`, `10a55c5`, `03bdcbe`)
`Tools/BotRuns.ps1` `Expand-Unlock` knows two named sets. `S2Arsenal` is what `S2` meant before: every S2 row of `weapons.csv` and `modifiers.csv` and the S1 non-starters, 14 ids. `S2` is `S2Arsenal` plus `Reaction.Duos`, the pool id that gates the duo reactions and duo cards (`reactions.md` R-REA-02), which the old expansion never passed, so no exit run held them. Neither set unlocks a Keeper: each arm unlocks its own (`keepers.md` R-KEE-23). `Tools/S2Exit.ps1` gives its `-Unlock` (default `S2`) to the arm batteries, and `S2Arsenal` to every probe scene (`-Jobs`) when its `-Unlock` is `S2`; any other value goes to every call as given. So the exit's probe rows (T-KEE-18, T-KEE-15) are read without duo reactions, as before; the arm batteries now hold them, and every arm that seats a second Keeper will read differently.

Findings, not changes:
- Elites come in every run that lives past 225 s: the director's EliteWave rows (`timeline_arena1.csv`, `Director/DTDirector.cpp` sets the elite stream every step, R-ELT-02); only director-less probe scenes have none. Reforging comes in every run that reaches L16 (R-OFF-09, `Offers/DTOfferGenerator.cpp`). The expansion was never what kept either out.
- The bot takes an evolution in 10 of the 540 exit runs (`S2_exit.md`, D-038 paragraph). That is a matter for the battery's arms, not for the pool.
- Time Dilation acts only through a Quicksilver cast, and its scale stands at 1.0 (`C.INTERCESSION.QUICKSILVER_TIME_SCALE`, R-KEE-37); only the experiment cvar `dt.Exp.DilationScale` moves it, which is a number and is not touched.
## What this makes stale
Every bot reading on `dev/s2` taken before this change (the S2 exit battery and its rereads, `S2_exit.md`, and the probes) is stale for arms led or seconded by Shade, Radiance or Quicksilver: the offer weights of their weapons and modifiers moved, and the bots' weapon score (v1 and v2) moved for a bot whose own Path is one of them. Arms that hold only S1 Keepers are expected to be unmoved, because a Path the player does not own changes no factor of R-OFF-02 and the generated tables change only in those bits. That is expected, not shown: the gate's seed step and a rerun in phase 2 are what confirm it. No hash is claimed here.
