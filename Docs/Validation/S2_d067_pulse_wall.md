# D-067: Pulse Wall carries out what its ring crosses (2026-11-12)

Branch `s2/d067-note`, from `dev/s2` at `c61d74e`: this note only. The rule was built on `s2/d067-pulse-wall` (tip `b5ac40c`, code `6f05bd7`), read there, and **not landed**: R-RFG-12 on `dev/s2` is as it was, and the test `Dusktracer.Reforging.PulseWallCarry` and `C.GATE.TEST_FLOOR` 482 exist on that branch only. Class: RULE (`reforging.md` R-RFG-12). No number moved. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

**Hypothesis: not met, on both clauses.** The nearest enemy after the pick stands at 232 uu at the median, inside the band's inner edge (500 uu); the build deals 0.74 of its hold. The record's clause for a miss is the scheduler's to apply. The whole gate was not run on the rule's branch (the coordinator's order: the verdict first).

Build:
- Before (step 0, the perf bound only): the build of `c61d74e`.
- After: the build of `1af3eac` (code `6f05bd7`; tests `e662a37`, `1af3eac`). `Tools\Build.bat`: `Result: Succeeded`. `Tools\RunTests.ps1 -Filter Dusktracer.Reforging`: `TESTS: PASS (27 run of 27 found, 0 failed, floor 0, 0 crash marker(s))`; `-Filter Dusktracer.Modifiers.ChainTrigger`: `TESTS: PASS (7 run of 7 found ...)`; `-Filter Dusktracer.Keepers.S2.Card.Quicksilver.Vector`: `TESTS: PASS (12 run of 12 found ...)`. The first try of the Reforging filter crashed in the new test (a serial view taken before the enemy was added); fixed in `1af3eac`, then the three filters above.

## What the rule's branch changed
- `StepForceField`: a force field with a band and a growth time (`CarriesOut()`: Pulse Wall only; the Vortex zone has no band) does, while `Age < GrowS`, only this: each enemy within the front is struck once for `DamagePerSecond * TickS` (`AddFieldHit`, an impact), remembered by serial in `FField::FrontStruck` (a set read with `Contains` only), and if it stands nearer than `min(CurrentRadius, Radius - BandUu)` it gets one displacement outward to that distance; an enemy at the centre is struck and not moved. No band tick and no band push while growing; the tick clock runs on. Held: today's code. `StrikeForceField` (the free volley): unchanged.
- `FField` is held only in the sim's `Fields` array (opened, stepped, removed, drawn); nothing copies, saves, restores or hashes it, so `FrontStruck` needs nothing else.
- The displacement system (`FDTEnemyPool::ApplyDisplacements`) has no per-step cap and no role resistance: only a stale serial and the boss are left, and positions are clamped to the arena. Nothing there made the carry false for a common role.
- R-RFG-12 and T-RFG-12 amended with the record's sentences and the cases. Test `Dusktracer.Reforging.PulseWallCarry`; `C.GATE.TEST_FLOOR` 481 to 482 (`number_change_log.csv`, result `pending`). One line of `Dusktracer.Reforging.PulseWall` moved, as approved: the Core enemy inside the opening ring now has the front's one strike (1, not 0); that harness applies no displacement, so the Core stays at the centre and the held ring leaves it alone after.
- What the new test reads: an enemy a third of the way in ends growth at the band's inner edge with exactly one hit of one tick; over the ring's life it takes **3 hits (the front's 1, then 2 by the held band) and ends at 603 uu**, pushed out of the ring. An enemy in the band's zone (not displaced) takes 13 hits over the life against 12 for a twin placed after growth: the front's one, then the same held ticks.

## The reading (the hypothesis' own)
```
& Tools\BotRuns.ps1 -Arms Steered:Storm:Tempest:Tide -Carry Weapon.RingEmitter -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 6 -Stagger 0.4 -Out Saved\BotRuns\d067_arm
& Tools\BotRuns.ps1 <the same> -CarryHold -Out Saved\BotRuns\d067_hold
python -I Tools\evolve_read.py --carry Saved\BotRuns\d067_arm --hold Saved\BotRuns\d067_hold
```
plus the scratch reading of `S2_d061_pulse_wall.md`'s second table (the same script, the same definitions).

`evolve_read.py`, evolved / hold: weapon dpm 2 316 / 3 420, build dpm 12 687 / 17 124 (**ratio 0.74**), reach 9 / 15, wins 4 / 15, all 15 took the evolution, 0 faults; one evolved run ended on the bot's timeout (`outcome=timeout`).

| After the pick | Hold | `dps` 2 before (D-061) | `dps` 2 after (D-067) |
|---|---|---|---|
| Keeper.Storm | 8 519 | 7 496 | 7 138 |
| Weapon.RingEmitter (Pulse Wall in the arm) | 3 191 | 1 960 | 2 296 |
| Keeper.Tide | 1 399 | 1 426 | 1 386 |
| Weapon.PulseRifle | 996 | 1 244 | 1 241 |
| Weapon.OrbitalDrones | 220 | 596 | 587 |
| All sources, damage per minute | 16 450 | 12 088 | 12 017 |
| Kills per minute | 264 | 248 | 240 |
| XP per minute | 351 | 344 | 328 |
| Level at the pick / at the end | 16 / 24 | 16 / 22 | 16 / 24 |
| Nearest enemy, uu | 477 | 194 | **232** |
| Live enemies | 14 | 37 | 30 |
| Damage taken per minute | 4.0 | 24.7 | 16.9 |
| ... by role (Rusher, Tank, Swarm, Turret, Boss) | 0, 0, 0, 0, 2.4 | 8.2, 8.4, 1.4, 1.4, 0 | 3.3, 2.4, 1.4, 1.2, 2.0 |
| How the runs end | 15 victories | 1 victory, 14 deaths | 4 victories, 10 deaths (Rusher 5, Boss 4, Tank 1), 1 timeout |

## Verdict
- Nearest enemy at the median against the band's inner edge (`radius_uu` 600 minus `band_uu` 100 = 500 uu): 232 uu. **Not met.**
- Build damage per minute against the hold's, at least 1.0: 0.74 (`evolve_read.py`; 12 017 against 16 450, 0.73, by the table). **Not met.**
- The carry works in the test as the rule says, and in the runs the crowd stands a little further out (194 to 232 uu) with fewer alive (37 to 30) and less damage taken (24.7 to 16.9, Tanks 8.4 to 2.4, Rushers 8.2 to 3.3). The build's output does not move, and the nearest enemy stays far inside the band. In the test an enemy carried to 500 uu is pushed out of the ring by the held band within about half a second; these run files do not show a single enemy's path, so where the inner crowd comes from in the runs is not traced.
- Output against `REFORGE.POWER_MIN` 1.3 to `REFORGE.POWER_MAX` 1.8 as `evolve_read.py` reads it: 0.74, under the band. Danger for people: damage taken 16.9 a minute against the hold's 4.0; whether a working wall is too safe is the first S2 session's question, never the bot's.

## The hold check
The new hold against `Saved\BotRuns\d061_hold`, run for run: outcome, end time, steps and `input_hash` are equal on all 15 runs. Nothing outside the evolved weapon moved.

## Where the hero stands
The ring opens at the hero and its place is then fixed, while the bot keeps moving. Read from the existing run files (`hero_pos` records, one every 2.5 s; a scratch script): for each run, windows of one ring life (`grow_s` + `hold_s` = 3.6 s) start at each `hero_pos` sample after the Reforging pick; the end position is linear between the two samples around the window's end. Per window: the straight-line distance from start to end, and the path along the samples inside it (a lower bound at this sampling). Medians over every window of the 15 runs.

| Batch | Windows | Straight-line distance over one ring life, median | Path, median (lower bound) | Windows ending more than 500 uu from the start |
|---|---|---|---|---|
| Hold (`d061_hold`) | 2 462 | 573 uu | 874 uu | 58 % |
| `dps` 2 before D-067 (`d061_arm_2`) | 1 877 | 508 uu | 756 uu | 51 % |
| D-067 (`d067_arm`) | 2 167 | 469 uu | 682 uu | 46 % |

In about half the ring lives the hero ends more than the band's inner edge (500 uu) from where the ring opened: the hero walks out of its own ring as often as it stays inside, so a crowd held at the band around the ring's centre is not held around the hero. The hold's bot moves more than either arm.

## The band against each role's walk (arithmetic, not a reading)
From `09_Catalog/enemies.csv` (`speed_uu_s`, whose cells name `C.ENM.SWARM_SPEED_UU_S`, `C.ROLE.TANK_SPEED_UU_S` and `C.ROLE.RUSHER_SPEED_UU_S` in `constants.yaml`; the Turret's cell is 0), `C.ROLE.RUSHER_CHARGE_SPEED_UU_S`, `C.AFFIX.SWIFT_SPEED_MULT`, and the Pulse Wall row of `reforging.csv` (`push_uu_s` 200, `band_uu` 100, `radius_uu` 600, so the inner edge is at 500 uu; ring life 3.6 s). Walking straight at the ring's centre, standing hero at the centre:

| Role | Walk, uu/s | Against the held band's push (200 uu/s) | Time to cross the band (100 uu) | Time from the inner edge to the centre (500 uu), nothing pushing inside |
|---|---|---|---|---|
| Tank | 165 | stopped (net 35 uu/s outward) | never | 3.0 s |
| Rusher, walking | 240 | crosses (net 40 uu/s) | 2.5 s | 2.1 s |
| Rusher, charging | 1 500 | crosses | 0.08 s | 0.33 s |
| Swarm | 280 | crosses (net 80 uu/s) | 1.25 s | 1.8 s |
| Swift Swarm (×1.4) | 392 | crosses (net 192 uu/s) | 0.5 s | 1.3 s |
| Turret | 0 | stays where it stands | never | never |
| Boss | ignores displacement | not pushed | — | — |

So with the hero standing still, a held band stops only Tanks (and leaves Turrets where they are); Swarm and Rushers cross it in 1.25 to 2.5 s walking, a charge in a tenth of a second, and anything once inside reaches the centre in about 2 to 3 s, within the 3.6 s ring life. Each new ring carries out what stands inside it.
## The perf bound (a bound on a shared machine, not a reading of record)
`& Tools\Perf.ps1 -Scene S2` twice before (the build of `c61d74e`) and twice after (the build of `1af3eac`). The S2 scene evolves the Ring Emitter: 3 rings in each capture window, so the wall is in it. What else ran on the PC during these four runs was not recorded; the two pairs ran 9 minutes apart, each under the session lock.

| Run | Game thread p50 / p95, ms | GPU p95, ms | `DT/GameThread/SimTick` mean / p95, ms |
|---|---|---|---|
| Before 1 | 2.95 / 4.42 | 3.87 | 0.90 / 1.84 |
| Before 2 | 2.91 / 4.29 | 3.88 | 0.90 / 1.83 |
| After 1 | 3.07 / 4.63 | 3.90 | 0.97 / 2.05 |
| After 2 | 2.97 / 4.53 | 3.91 | 0.94 / 1.94 |

The after runs read higher than the before runs by more than the spread between two runs of the same build: game p95 4.29 to 4.42 before against 4.53 to 4.63 after, and `SimTick` p95 1.83 to 1.84 against 1.94 to 2.05. All four pass the budget.

## Not done
The whole gate and the five-arm seed 9107 check were not run: the hypothesis is not met, and the coordinator's order was to report before the gate. No S2 kit file exists on `dev/s2` (`Docs/Validation` holds `S1_kit.md` only), so the kit question was not written.
