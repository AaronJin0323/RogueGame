# Impact art — S4 wave package E

> Scope: `Validation/S4_wave1.md` §3 "E" (D-043; `D-055 item 6`). The package lands in two steps. This file holds the first: the tap, R-IMP-01 and R-IMP-02 (branch `s4/e0-tap`). The effects drawn from it are written here as R-IMP-03 onward by the second step (branch `s4/e-feedback`) before its first line of code; until then the sections marked "second step" bind nothing. Presentation and audio read the sim and never write it (`04_Presentation` R-PRE-11).

## Intent
A hit and a kill are shown and heard from one read-only record of what the hit resolve phase did. The record is the sim's own account of the step: one line per hit it applied, with the kill marked on the hit that made it. The eye (this package) and the ear (`audio.md`, the Sound module) read the same lines, so a spark and its sound can never disagree about what happened. The tap adds no rule, moves no number and is read by nothing in the sim.

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

## Numbers
None in the tap: it has no capacity, no rate and no threshold. The second step adds this package's presentation-only keys as a new `IMP` group of `constants.yaml`, each with its row in `08_Constants.md` and in `number_change_log.csv`.

## Data
| File | What it holds |
|---|---|
| `Source/Dusktracer/Enemies/DTEnemyPool.h` | `FDTImpact`, `DTImpactFlags`, `FDTEnemyPool::GetStepImpacts()` |
| `Source/Dusktracer/Enemies/DTEnemyPool.cpp` | the append in `ApplyHits`, the reset in `ResolveHits`, the Burn tick range noted in `StepEnemies` |

These two files are the pool, the one sim-side place this package edits. Asset keys reserved for the second step, each with its licence row already in `09_Catalog/licenses.csv`: `Tools/blender/debris*.py`, `Tools/assets/fx_*.py`, `SourceArt/Fx/`, `Content/DT/Meshes/SM_Fx_*`, `Content/DT/Materials/M_Fx_*`. How an asset is built, logged, staged in a scene and read: `Validation/S4_p0.md`.

## Acceptance
- T-IMP-01 `Dusktracer.Impact.Tap`: a pool with staged enemies and a hit list that holds plain hits, two hits on one enemy, a hit that kills followed by another hit on the same enemy, a stale-serial hit, a Tank hit inside the front arc and a piercing one, a hit an elite's shield absorbs, a hit on an Armored elite, a hit on a Shocked enemy, a critical hit, a field tick, and a Burn tick made by `StepEnemies`. The step list holds one record per applied hit, in order, and none for the two ignored hits; the record count equals the `Hit` events the run state received, and the `Kill`-flagged count equals the `Kill` events and `FDTWorldView::Kills`, serial for serial in the same order; every field and every flag is as staged, and no flag is set where it was not staged; `Dealt` sums to the health the pool removed; the next `ResolveHits` replaces the list, and a step with no hit leaves it empty.
- T-IMP-02 `Dusktracer.Impact.Readers` PLANNED(S4), built by the second step with its first reader: a reader registered with `UDTSimSubsystem` after `ResolveHits` sees the list of every step of a frame that ran several, the same whole list as a second reader, and writes no field of the pool, of `FDTWorldView` or of the run.
- The sim proof of `Validation/S4_p0.md` §8, read on the tap alone before any effect is drawn: gate step `seed`, `Tools/RestartCheck.ps1` and the five bot arms on seed `9107` give the hashes `dev/s4` gave before. `Tools/tools_test.py` `PresentationReadsOnly` passes: the pool includes no presentation header.
- Second step: the checks of `Validation/S4_wave1.md` §3 "E", run without a window; what only a person can judge is written here as a human line and reported as not verified.

## Telemetry
None. The tap is not a record: nothing is counted, written to a run file or exported.

## Text
None.

## Out of scope
- Any effect, sound, shake, hit-stop or damage number: the tap draws and plays nothing.
- Hits on a hero (`FDTWorldView::HeroHurt` already says so) and enemy fire.
- A cap, a ring or a history across steps: a reader that needs more than one step keeps its own.
- Everything `Validation/S4_wave1.md` §4 holds back.

## Open
- `07_Technical` §4 lists the structs systems share; `FDTImpact` is not listed there by this package, which may not edit that file. A line for it is the scheduler's to place.
