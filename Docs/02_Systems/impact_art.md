# Impact art — S4 wave package E

> Scope: `Validation/S4_wave1.md` §3 "E" (D-043; `D-055 item 6`). Two steps: the tap, R-IMP-01 and R-IMP-02 (branch `s4/e0-tap`), and the effects drawn from it, R-IMP-03 to R-IMP-10 (branch `s4/e-feedback`). Presentation only: every position, radius, role and count is read from the sim and none is written (`04_Presentation` R-PRE-11); no rule of another spec, no sim number and no token value moves. The readings are in `Validation/S4_e0_tap.md` and `Validation/S4_e_feedback.md`.

## Intent
A hit and a kill are shown and heard from one read-only record of what the hit resolve phase did. The record is the sim's own account of the step: one line per hit it applied, with the kill marked on the hit that made it. The eye (this package) and the ear (`audio.md`, the Sound module) read the same lines, so a spark and its sound can never disagree about what happened. The tap adds no rule, moves no number and is read by nothing in the sim.

What the eye gets, at silhouette grade: a round that lands throws a short cool spark off the hull; a hull that dies leaves a brief pale flash where it stood and a few dark pieces that scatter and are gone in half a second; a hero that loses health shows a warm ring at its feet. A kill stops being a hull that vanishes. None of it may cost the player a read: nothing drawn here can cover an enemy round, a telegraph, a hull or a hero, and when the fight is dense the effects thin out and merge instead of piling up.

## Rules
- **R-IMP-01** The tap. `FDTEnemyPool::ApplyHits` appends one `FDTImpact` to the pool's step list for every hit it applies: every hit that passes the dead check and the stale-serial check (`enemies.md` R-ENM-23), which is every hit that sends a `Hit` game event, in the order applied. A hit it ignores leaves no record. The list is emptied at the start of each `ResolveHits` and stands until the next one, so from the end of the phase it holds exactly that step's hits, and an empty list after a step with none; `FDTEnemyPool::GetStepImpacts()` is its read-only view. The tap never trims and never samples: a step's list is as long as the hits that step applied, which is never longer than the hit list the sim built for the step, and its storage is kept between steps. Each reader bounds what it raises from the list (R-IMP-02). Nothing in the sim reads the list, and it is no part of a save, a hash, a game event or a telemetry record.

  An `FDTImpact` carries:

  | Field | Type | What it is |
  |---|---|---|
  | `Position` | `FVector2f` | the contact point, as the hit's emitter wrote it (`FDTHit::Position`) |
  | `EnemyPosition` | `FVector2f` | the hull's centre when the hit resolved |
  | `EnemyRadius` | `float` | the hull's hit radius in uu; an elite's draw scale is the reader's to apply |
  | `Dealt` | `float` | health taken: after the plate, the shield, Armored and Shock, never more than the health left; 0 for an absorbed hit |
  | `MaxHp` | `float` | the hull's full health, so `Dealt` can be read as a share |
  | `EnemySerial` | `uint32` | names the enemy for the step; several records may share one |
  | `OwnerId` | `int32` | the player the hit belongs to (`07_Technical` R-TEC-01) |
  | `SourceId` | `uint16` | the weapon or system that dealt it, as on `FDTHit`; never gameplay |
  | `WeaponSlot` | `int8` | the owner's weapon slot, or `INDEX_NONE` for Keeper-only hits and stress fire |
  | `Role` | `EDTEnemyRole` | the hull's role; the Warden is `Boss` |
  | `Flags` | `uint8` | bits of `DTImpactFlags`, below |

  | Flag | Set when |
  |---|---|
  | `Kill` | this hit killed the enemy: exactly one per death and one per `Kill` game event; the record holds the dead hull's last position and radius |
  | `Crit` | `FDTHit::bCrit` |
  | `Pierce` | `FDTHit::bPierce` |
  | `FieldTick` | `FDTHit::bFieldTick`: a field's tick, not a hit to the rules (`keepers.md` R-KEE-16) |
  | `StatusTick` | the hit is a Burn tick the pool itself appended in this step's Enemies phase (`enemies.md` R-ENM-22) |
  | `Plate` | a Tank's front plate reduced it (`enemies.md` R-ENM-10) |
  | `Shield` | an elite's shield absorbed it (`elites.md` R-ELT-08) |
  | `Elite` | the hull is an elite |

- **R-IMP-02** Readers. A reader is presentation or audio code. It reads the step list once per sim step, after `EDTSimPhase::ResolveHits` of that step: a reader that looked once per rendered frame would miss every step but the last of a frame that ran several. It does so as a system registered with `UDTSimSubsystem` in a phase after `ResolveHits`, where it copies what it needs and writes nothing: no field of the pool, of `FDTWorldView` or of the run (R-PRE-11). Kills need no second list: the records flagged `Kill` are the step's deaths, in the order of `FDTWorldView::Kills`. A hit on a hero is not in the tap: `FDTWorldView::HeroHurt` says which heroes lost health this step, and a reader reads it the same way. A reader bounds its own output at density; the tap gives every reader the same whole step, so two readers never disagree about what was dropped.

- **R-IMP-03** The feed. `UDTImpactSubsystem` is this package's reader: registered in `EDTSimPhase::Director`, it is fed once per sim step and draws once per rendered frame. From a step it takes:
  - **counted hits**: every record with neither `FieldTick` nor `StatusTick` and without `Kill`. A tick is damage, not a blow, and raises nothing; a hit that kills is shown by its kill.
  - **kills**: every record with `Kill`, whatever dealt it, a tick included.
  - **hurt heroes**: every player whose `HeroHurt` entry is set this step.

  Every effect is born on the step that raised it and ages in sim time, by step index and the frame's interpolation alpha, so effects stop when the sim is paused or held and a held step always draws the same frame. An effect's variation (a piece's direction, speed, spin and shape) is a hash of the enemy's serial, the step and the piece's index: no sim stream is drawn from, no stream state is kept, and a frame does not depend on the frame rate.

- **R-IMP-04** Under everything that must be read. Every effect of this package is drawn with the camera-ray slide of `tracer_art.md` R-TRC-05: the material slides each vertex along its own camera ray down to a fixed height, so the effect covers the pixels it would cover at its own height and the depth test hides it wherever a telegraph, a round, a hull or a hero is drawn. Debris slides to `C.IMP.DEBRIS_Z_UU`, sparks and flashes to `C.IMP.FLASH_Z_UU`: both over the floor, the zones, the control cues and player tails, both under the lowest telegraph layer (`C.ENEMIES.TELEGRAPH_Z_UU`). So no effect can paint over a telegraph, an enemy round, an optic, a hull or a hero, whatever its blend and however many overlap. Sparks, kill flashes and the hero ring are unlit, so the sun does not move them off their token; debris is lit, as the hull it came from is, and casts no shadow. Colours are tokens by name (R-PRE-09): `Pal.Player.Core` for sparks and kill flashes, since no weapon hue exists (R-PRE-01); `Pal.Enemy.Body` for debris at zero emissive; `Pal.Enemy.Muzzle` for the hero ring (`04_Presentation` §3: "hit-on-hero flash"). No token is added and none changes.

- **R-IMP-05** Hit spark. A counted hit raises one spark at its contact point: a fan of three thin streaks, the middle one longest, opening away from the hull along the line from the hull's centre through the contact point (away from the owner's hero when the two coincide). It is `C.IMP.SPARK_LENGTH_UU` long and `C.IMP.SPARK_WIDTH_UU` wide at its full size, grows from `C.IMP.SPARK_START_RATIO` of that to all of it and fades from `C.IMP.SPARK_EMISSIVE` to nothing over `C.IMP.SPARK_S` (`M_Fx_Spark`, additive). Every counted hit has the same spark: a critical, a plated or a shielded hit is not told apart in this wave.

- **R-IMP-06** Kill flash. A kill raises one flash at the dead hull's centre: a soft-edged disc that grows from `C.IMP.KILL_FLASH_START_RATIO` to `C.IMP.KILL_FLASH_END_RATIO` of the hull's drawn radius (its hit radius, times `C.ELITE.DRAW_SCALE` for an elite) and fades from `C.IMP.KILL_FLASH_EMISSIVE` to nothing over `C.IMP.KILL_FLASH_S` (`M_Fx_Flash`, additive). No flash is drawn wider than `C.IMP.FLASH_RADIUS_MAX_UU`, the Warden's included.

- **R-IMP-07** Debris. A kill in the `PerKill` tier (R-IMP-08) also throws dark pieces of the hull: one per `C.IMP.DEBRIS_RADIUS_PER_PIECE_UU` of drawn radius, rounded, never fewer than `C.IMP.DEBRIS_PIECES_MIN` and never more than `C.IMP.DEBRIS_PIECES_MAX`. Each is one of the three shard meshes, `C.IMP.DEBRIS_SIZE_UU` across, in `Pal.Enemy.Body` with no emissive (`M_Fx_Debris`). They leave the hull's centre along the blow, from the contact point through the centre, spread up to `C.IMP.DEBRIS_SPREAD_DEG` to either side, at `C.IMP.DEBRIS_SPEED_UU_S` give or take `C.IMP.DEBRIS_SPEED_VARIATION`, slow with the time constant `C.IMP.DEBRIS_DRAG_S`, turn as they go, shrink to nothing over the last `C.IMP.DEBRIS_SHRINK_SHARE` of their life and are gone after `C.IMP.DEBRIS_S`. Nothing lies on the floor afterwards. Debris is generic at this grade: the same three shards for every role.

- **R-IMP-08** Density (`04_Presentation` R-PRE-06, the kill half; `D-056 ruling 5`). The kill rate is the pool's own (`FDTEnemyPool::GetKillRate`, window `C.ENM.KILL_RATE_WINDOW_S`), read once per step after the step's kills are counted. It puts the step in one of three tiers. A tier is left at a lower rate than it is entered, so a fight that sits near a threshold does not flicker between two looks:

  | Tier | Entered when the rate is | Left when the rate is | What the eye gets |
  |---|---|---|---|
  | `PerKill` | the start of a run | above `C.PRE.DENSITY_LOD_KILLS_PER_S` | every counted hit its spark, up to `C.IMP.SPARKS_PER_STEP_MAX` in a step; every kill its flash and its debris: as many effects as the step had hits and kills |
  | `Crowd` | above `C.PRE.DENSITY_LOD_KILLS_PER_S` | under `C.IMP.DENSITY_CROWD_OFF_KILLS_PER_S`, or above `C.IMP.DENSITY_FLOOD_KILLS_PER_S` | per-kill effects aggregate: no debris; the step's kills are grouped by a floor grid of `C.IMP.AGG_CELL_UU` and each occupied cell raises one flash, at most `C.IMP.AGG_PER_STEP_MAX` in a step; sparks thin to one per enemy per step and at most `C.IMP.SPARKS_DENSE_PER_STEP_MAX` in a step |
  | `Flood` | above `C.IMP.DENSITY_FLOOD_KILLS_PER_S` | under `C.IMP.DENSITY_FLOOD_OFF_KILLS_PER_S` | as `Crowd`, on a grid of `C.IMP.AGG_FLOOD_CELL_UU` with at most `C.IMP.AGG_FLOOD_PER_STEP_MAX` flashes in a step, and no sparks |

  - A rate that crosses two thresholds in one step moves two tiers in that step. A step is judged on the tier it ends in.
  - An aggregated flash sits at the centre of its cell's kills. Its radius is the root of the summed squares of theirs (the same lit area, drawn once), at the same emissive and never wider than `C.IMP.FLASH_RADIUS_MAX_UU`; when a step has more occupied cells than its cap, the fullest are raised.
  - Whatever the tier, live effects never pass `C.IMP.SPARK_LIVE_MAX` sparks, `C.IMP.FLASH_LIVE_MAX` flashes and `C.IMP.DEBRIS_LIVE_MAX` pieces: an effect that would pass its cap is not raised. An effect already live always finishes its life, so nothing pops when the tier changes.
  - Nothing on the enemy side reads the tier or the tap: hull, optic, elite light, round, trail, muzzle flash and telegraph are drawn the same with and without this package, in every tier.

- **R-IMP-09** Hero ring. A hero that lost health this step shows a ring at its feet in `Pal.Enemy.Muzzle`, centred on the hero and following it: its outer radius grows from `C.IMP.HERO_FLASH_START_RATIO` to `C.IMP.HERO_FLASH_END_RATIO` of `C.HERO.RADIUS_UU`, `C.IMP.HERO_FLASH_THICK_UU` thick, fading from `C.IMP.HERO_FLASH_EMISSIVE` to nothing over `C.IMP.HERO_FLASH_S` (`M_Fx_Ring`, additive). A second hit while it shows starts it again. It is this package's own instance, not a HUD element and not a screen effect: the camera does not move and sim time does not stop.
  With Screen flashes off (`DTSettings::AllowFlash`, `settings.md` R-SET-09) nothing in this package flashes: the hero ring and every kill flash are drawn at `C.IMP.REDUCED_EMISSIVE_RATIO` of their emissive and at their final size for their whole life, a steady mark that shows the same fact without a burst of light. Sparks and debris are unchanged.

- **R-IMP-10** Batched and read-only (`07_Technical` §5, R-PRE-11). Sparks are one instanced component, flashes one, hero rings one, debris one per shard mesh; never an actor or a component per effect. No sim array is written and no sim stream is drawn from: with the subsystem registered and drawing, seed `9107` gives the hashes it gave before.

- **R-IMP-11** The density reader. `UDTImpactSubsystem` is the one reader of the tier, for the eye and for the ear (`D-056 ruling 5`). It offers, read-only, from the end of a step's `Director` phase until the next step's: `GetDensityTier()`, the tier of the last step as an `EDTDensityTier` (`PerKill`, `Crowd`, `Flood`); `GetKillsPerSecond()`, the rate that tier was judged on; and `GetDensityStep()`, that step's index. `DTImpactArt::NextTier(Tier, KillsPerSecond)` is the pure rule behind it, for a test or a tool that folds a trace. The tier advances on every step whether or not effects are drawn (`dt.Impact.Draw`), and returns to `PerKill` when a run starts. No reader keeps a second tier of its own: two that did would disagree inside the bands. The reader writes nothing to the sim.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Height of debris | C.IMP.DEBRIS_Z_UU | GATED(S4) | R-IMP-04, T-IMP-04, `Tools/assets/fx_materials.py` |
| Height of sparks, flashes and the hero ring | C.IMP.FLASH_Z_UU | GATED(S4) | R-IMP-04, T-IMP-04, `Tools/assets/fx_materials.py` |
| Spark life | C.IMP.SPARK_S | GATED(S4) | R-IMP-05, T-IMP-05 |
| Spark length | C.IMP.SPARK_LENGTH_UU | GATED(S4) | R-IMP-05, T-IMP-05 |
| Spark width | C.IMP.SPARK_WIDTH_UU | GATED(S4) | R-IMP-05, T-IMP-05 |
| Spark size at birth | C.IMP.SPARK_START_RATIO | GATED(S4) | R-IMP-05, T-IMP-05 |
| Spark strength at birth | C.IMP.SPARK_EMISSIVE | GATED(S4) | R-IMP-05, T-IMP-05 |
| Sparks raised in a step in `PerKill` | C.IMP.SPARKS_PER_STEP_MAX | GATED(S4) | R-IMP-08, T-IMP-08 |
| Sparks raised in a step in `Crowd` | C.IMP.SPARKS_DENSE_PER_STEP_MAX | GATED(S4) | R-IMP-08, T-IMP-08 |
| Live sparks | C.IMP.SPARK_LIVE_MAX | GATED(S4) | R-IMP-08, T-IMP-08 |
| Kill flash life | C.IMP.KILL_FLASH_S | GATED(S4) | R-IMP-06, T-IMP-06 |
| Kill flash radius at birth, drawn radii | C.IMP.KILL_FLASH_START_RATIO | GATED(S4) | R-IMP-06, T-IMP-06 |
| Kill flash radius at its end, drawn radii | C.IMP.KILL_FLASH_END_RATIO | GATED(S4) | R-IMP-06, T-IMP-06 |
| Kill flash strength at birth | C.IMP.KILL_FLASH_EMISSIVE | GATED(S4) | R-IMP-06, T-IMP-06 |
| Widest flash | C.IMP.FLASH_RADIUS_MAX_UU | GATED(S4) | R-IMP-06, R-IMP-08, T-IMP-06 |
| Live flashes | C.IMP.FLASH_LIVE_MAX | GATED(S4) | R-IMP-08, T-IMP-08 |
| Aggregation cell | C.IMP.AGG_CELL_UU | GATED(S4) | R-IMP-08, T-IMP-08 |
| Aggregated flashes raised in a step | C.IMP.AGG_PER_STEP_MAX | GATED(S4) | R-IMP-08, T-IMP-08 |
| Rate under which `Crowd` is left | C.IMP.DENSITY_CROWD_OFF_KILLS_PER_S | GATED(S4) | R-IMP-08, R-IMP-11, T-IMP-08 |
| Rate above which `Flood` is entered | C.IMP.DENSITY_FLOOD_KILLS_PER_S | GATED(S4) | R-IMP-08, R-IMP-11, T-IMP-08 |
| Rate under which `Flood` is left | C.IMP.DENSITY_FLOOD_OFF_KILLS_PER_S | GATED(S4) | R-IMP-08, R-IMP-11, T-IMP-08 |
| Aggregation cell in `Flood` | C.IMP.AGG_FLOOD_CELL_UU | GATED(S4) | R-IMP-08, T-IMP-08 |
| Aggregated flashes raised in a step in `Flood` | C.IMP.AGG_FLOOD_PER_STEP_MAX | GATED(S4) | R-IMP-08, T-IMP-08 |
| Debris life | C.IMP.DEBRIS_S | GATED(S4) | R-IMP-07, T-IMP-07 |
| Debris piece size | C.IMP.DEBRIS_SIZE_UU | GATED(S4) | R-IMP-07, T-IMP-07, `Tools/blender/debris.py` |
| Drawn radius per piece | C.IMP.DEBRIS_RADIUS_PER_PIECE_UU | GATED(S4) | R-IMP-07, T-IMP-07 |
| Fewest pieces | C.IMP.DEBRIS_PIECES_MIN | GATED(S4) | R-IMP-07, T-IMP-07 |
| Most pieces | C.IMP.DEBRIS_PIECES_MAX | GATED(S4) | R-IMP-07, T-IMP-07 |
| Debris speed at birth | C.IMP.DEBRIS_SPEED_UU_S | GATED(S4) | R-IMP-07, T-IMP-07 |
| Debris speed variation | C.IMP.DEBRIS_SPEED_VARIATION | GATED(S4) | R-IMP-07, T-IMP-07 |
| Debris slow-down time constant | C.IMP.DEBRIS_DRAG_S | GATED(S4) | R-IMP-07, T-IMP-07 |
| Debris spread to either side | C.IMP.DEBRIS_SPREAD_DEG | GATED(S4) | R-IMP-07, T-IMP-07 |
| Share of life a piece shrinks over | C.IMP.DEBRIS_SHRINK_SHARE | GATED(S4) | R-IMP-07, T-IMP-07 |
| Live debris pieces | C.IMP.DEBRIS_LIVE_MAX | GATED(S4) | R-IMP-08, T-IMP-08 |
| Hero ring life | C.IMP.HERO_FLASH_S | GATED(S4) | R-IMP-09, T-IMP-09 |
| Hero ring radius at birth, hero radii | C.IMP.HERO_FLASH_START_RATIO | GATED(S4) | R-IMP-09, T-IMP-09 |
| Hero ring radius at its end, hero radii | C.IMP.HERO_FLASH_END_RATIO | GATED(S4) | R-IMP-09, T-IMP-09 |
| Hero ring thickness | C.IMP.HERO_FLASH_THICK_UU | GATED(S4) | R-IMP-09, T-IMP-09 |
| Hero ring strength at birth | C.IMP.HERO_FLASH_EMISSIVE | GATED(S4) | R-IMP-09, T-IMP-09 |
| Strength with Screen flashes off | C.IMP.REDUCED_EMISSIVE_RATIO | GATED(S4) | R-IMP-09, T-IMP-09 |

The first values are in `Docs/constants.yaml`. They were set on held frames and not on play (Open); where each started, and what it is in pixels at both frames, is in `Validation/S4_e_feedback.md`. Nothing is set under the coarser frame (D-043).

Read and never changed here: `C.PRE.DENSITY_LOD_KILLS_PER_S`, `C.ENM.KILL_RATE_WINDOW_S`, `C.ELITE.DRAW_SCALE`, `C.HERO.RADIUS_UU`, `C.SIM.FIXED_HZ`, `C.SIM.PLANE_Z_UU`, `C.ENEMIES.TELEGRAPH_Z_UU`, `C.PRE.TELEGRAPH_LAYER_UU`, `C.PRE.CUE_Z_UU`, `C.PRE.ZONE_Z_UU`, `C.PRE.ZONE_HALF_UU`, `C.TRC.TAIL_Z_UU`, `C.PRE.CONTRAST_MIN`, `C.PRE.CONTRAST_WATCH_MIN`.

## Data
| Asset or file | What it is |
|---|---|
| `Source/Dusktracer/Enemies/DTEnemyPool.h`, `.cpp` | the tap: `FDTImpact`, `DTImpactFlags`, `FDTEnemyPool::GetStepImpacts()`; the one sim-side place this package edits |
| `Source/Dusktracer/Presentation/DTImpactArt.*` | the pure functions: what a step raises, the tier rule (`NextTier`), aggregation, each effect's shape over its age, the hash variation |
| `Source/Dusktracer/Presentation/DTImpactSubsystem.*` | the reader of R-IMP-03 and R-IMP-11: fed once per step, holds the live effects and the density tier, hands the effects to the renderer once per frame |
| `Source/Dusktracer/Presentation/DTImpactRenderer.*` | the instanced components of R-IMP-10 |
| `Tools/assets/fx_materials.py` | the generator of the four materials, run by `Tools/EditorPython.bat`; no colour is typed in it |
| `Content/DT/Materials/M_Fx_Spark`, `M_Fx_Flash`, `M_Fx_Ring`, `M_Fx_Debris` | spark, flash and ring: unlit, additive, on the engine plane, the slide to `C.IMP.FLASH_Z_UU`; debris: lit, opaque, the slide to `C.IMP.DEBRIS_Z_UU` |
| `Tools/blender/debris.py`, `SourceArt/Fx/`, `Content/DT/Meshes/SM_Fx_Debris_A`, `_B`, `_C` | the three shards, built by `Tools/blender/Mesh.ps1` |
| `Tools/Scenes/impact_hit.json`, `impact_kill.json`, `impact_dense.json`, `impact_hero.json` | the staged frames: sparks on hulls that soak fire; kills in the `PerKill` tier; the contrast frame with kills in the `Crowd` tier; a hero losing health |
| `Tools/ImpactArt.ps1` | the package's acceptance run: the four scenes at 36 and 27 px/m into `Saved\ImpactArt\`, one line each |

Without a material or a shard mesh the effect that needs it is not drawn, logged once; the game runs as before. Licence rows for every asset here are already in `09_Catalog/licenses.csv` (`Validation/S4_p0.md` §3).

## Acceptance
- T-IMP-01 `Dusktracer.Impact.Tap`: a pool with staged enemies and a hit list that holds plain hits, two hits on one enemy, a hit that kills followed by another hit on the same enemy, a stale-serial hit, a Tank hit inside the front arc and a piercing one, a hit an elite's shield absorbs, a hit on an Armored elite, a hit on a Shocked enemy, a critical hit, a field tick, and a Burn tick made by `StepEnemies`. The step list holds one record per applied hit, in order, and none for the two ignored hits; the record count equals the `Hit` events the run state received, and the `Kill`-flagged count equals the `Kill` events and `FDTWorldView::Kills`, serial for serial in the same order; every field and every flag is as staged, and no flag is set where it was not staged; `Dealt` sums to the health the pool removed; the next `ResolveHits` replaces the list, and a step with no hit leaves it empty.
- T-IMP-02 `Dusktracer.Impact.Readers`: two readers registered with `UDTSimSubsystem` after `ResolveHits` each see the whole list of every step of a frame that ran several, the same records in the same order, and after both have read the pool's arrays, `FDTWorldView` and the run state are as they were.
- T-IMP-03 `Dusktracer.Impact.Feed`: a staged step with a known number of counted hits, ticks and kills in the `PerKill` tier raises exactly that many sparks and flashes and the stated pieces per kill, and nothing for a tick; the subsystem fed two steps inside one frame holds both steps' effects; an effect's age follows the step index and the alpha, does not advance while no step runs, and the same step and serial give the same pieces twice.
- T-IMP-04 `Dusktracer.Impact.UnderTelegraphs`: both heights lie over `C.TRC.TAIL_Z_UU` and the control cues and under the lowest telegraph layer and the lowest point of a drawn enemy round; the four materials have the stated shading and blend; the drawn instances carry `Pal.Player.Core`, `Pal.Enemy.Body` at zero emissive and `Pal.Enemy.Muzzle`.
- T-IMP-05 `Dusktracer.Impact.Shapes`: the spark opens away from the hull along the line from its centre through the contact point, and away from the owner's hero when the two coincide; it grows and fades as stated and is gone after its life.
- T-IMP-06 `Dusktracer.Impact.Shapes`: the flash follows its two ratios over its life, uses an elite's drawn radius, and is never wider than the widest radius, the Warden's included.
- T-IMP-07 `Dusktracer.Impact.Shapes`: the piece count follows the drawn radius inside its two limits; a piece leaves along the blow inside the spread, never travels further than its speed times the drag constant allows, shrinks over the stated share and is gone after its life.
- T-IMP-08 `Dusktracer.Pre.DensityLod` (T-PRE-06, the kill half) and `Dusktracer.Impact.Density`: a rate above `C.PRE.DENSITY_LOD_KILLS_PER_S` enters `Crowd` and one at it does not; a rate falling back stays in `Crowd` until it is under the exit rate; the same holds for `Flood` and its two rates; a rate that jumps across both thresholds moves two tiers in one step, up and down. A `Crowd` step with a known set of kills raises no debris, one flash per occupied cell up to the step's cap, each with the root-sum-square radius inside the widest radius, and at most one spark per enemy and the tier's cap of sparks; a `Flood` step raises no spark and at most its own cap of flashes on its own grid. A long run of steps at many times the highest threshold never passes a live cap. Enemy hull, optic, round, trail and telegraph instance data are equal with the subsystem drawing and with it off, in each tier.
- T-IMP-09 `Dusktracer.Impact.HeroRing`: a hero whose `HeroHurt` is set shows one ring that follows it, restarts on a second hit and ends after its life; with Screen flashes off the ring and the kill flashes draw at the reduced ratio and do not grow, and sparks and debris are unchanged.
- T-IMP-10 `Dusktracer.Impact.Feed` and `Dusktracer.Impact.Density`: no actor per effect; feeding and drawing leave the pool's arrays, the view and the run state as they were. `Tools/tools_test.py` `PresentationReadsOnly`. The sim proof of `Validation/S4_p0.md` §8 on seed `9107`, read first on the tap alone and again with the effects drawn.
- T-IMP-11 `Dusktracer.Impact.DensityReader`: over a staged trace of kill rates the subsystem's `GetDensityTier()` equals `NextTier` folded over the same rates, step for step, with `GetKillsPerSecond()` and `GetDensityStep()` naming the rate and the step it was judged on; it reads the same with `dt.Impact.Draw` off and on; it is `PerKill` again after a run starts; and asking changes nothing in the pool, the view or the run.
- Frames, without a window: gate step `contrast` on `Tools/Scenes/impact_dense.json`, all seven elements, with kills in the `Crowd` tier in the held frame: every element at or above `C.PRE.CONTRAST_MIN`, and the telegraph and the enemy round read against `C.PRE.CONTRAST_WATCH_MIN` before and after the package. Goldens of `impact_hit` and `impact_kill` on a held step. Perf in the S2 scene inside the package's allowance (`Validation/S4_wave1.md` §5, the GPU headroom call), reported with Caps and S4 as bounds.
- Human, not verified by any check here: whether a spark reads as a hit landing, whether a kill feels like a kill without shake or hit-stop, whether debris helps or is noise in a crowd, whether the hero ring is seen at all at play speed, and whether a crowded fight loses too much when debris stops. This is feel-changing work (H03 §4): it is merged to `dev/s4` only, and the owner plays it before it goes anywhere else.

## Telemetry
None. Nothing here is counted or recorded in a run file. `DT.Impact.Stats` prints the live counts to the log for a tool run.

## Text
None.

## Out of scope
- Camera shake and hit-stop, damage numbers, per-weapon hue, a hull that flashes when hit (R-ENM-24 keeps the hull dark), status layers and reaction effects, elite and Warden death set pieces, smoke and the ground haze of R-PRE-06, anything left on the floor, the low-HP vignette.
- Telling a critical, plated or shielded hit apart: the tap carries the flags; no effect reads them yet.
- All sound: `audio.md` reads the same tap.
- Everything `Validation/S4_wave1.md` §4 holds back.

## Open
- Every number above is a first setting read on held frames. Nothing was seen in motion or in play.
- The tiers' rates are the source's, and the ear's: `audio.md` R-AUD-08 holds the same three numbers under its own keys until it moves to this reader (`D-056 ruling 5`). Whether the eye wants other exits, and whether `Flood` should lose its sparks, is for a playtest.
- With Screen flashes off the hero ring and kill flashes stay as steady marks. Whether that setting should remove them altogether is a person's call.
- An effect is hidden by a hull or a hero it lies behind and also by one it lies in front of, because it is drawn at floor height, as tails are.
- `07_Technical` §4 lists the structs systems share; `FDTImpact` is not listed there by this package, which may not edit that file. A line for it is the scheduler's to place.
