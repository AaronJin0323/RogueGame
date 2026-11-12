# D-070: Pulse Wall is the source's disc (2026-11-12)

Branch `s2/d070-pulse-wall-disc`, from `dev/s2` at `d5d4a81`. Class: RULE (`reforging.md` R-RFG-12), with three cells of `Evolve.RingEmitter` set to the previous project's source values and two cells of `flag_compat.csv` (D-070, rulings 1 to 13). **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

**Hypothesis: not met, on the boss-fight clause.** Before the boss the build deals 0.97 of its hold (the bar is 0.95: met). In the boss fight it deals 0.40 (the bar is 0.5: not met). `evolve_read.py` printed `WINDOWS: FAIL (RingEmitter boss fight 0.40 < 0.5)`. As ruled, nothing is reverted here, the whole gate was not run, and the second pair was run anyway (below). The record's clause for a miss is the scheduler's to apply.

The record's hypothesis: "We believe that making the wall the source's disc moves the build's damage per minute against its hold in two windows, each judged by itself: before the boss, from 0.80 to at least 0.95 ... and in the boss fight, from about 0.3 to at least 0.5 ...; reverted, the rule and its three cells put back, if the build reads under 0.95 of its hold before the boss or under 0.5 of it in the boss fight."

Build:
- Perf "before": the build of `f041733`, whose engine code equals `d5d4a81`'s (`git diff f041733 d5d4a81 -- Source Content Config` is empty).
- The branch: `f210007` (cells), `fe91995` (code and rule text), `719f1ee` (tests, floor, number log), `c7df085` (the ring draws its outer arc once). Built at `c7df085` (`Result: Succeeded`).
- Tests on that build: `-Filter Dusktracer.Reforging` `TESTS: PASS (27 run of 27 found, 0 failed, floor 0, 0 crash marker(s))`, `-Filter Dusktracer.Modifiers` `TESTS: PASS (20 run of 20 found ...)`, `-Filter Dusktracer.Keepers.S2.Card.Quicksilver.Vector` `TESTS: PASS (12 run of 12 found ...)`. The first build of `719f1ee` failed `.PulseWallLook` (two arcs where one was meant): the drawing loop drew the outer arc twice; fixed in `c7df085`.

## The cells (old, new, source)
The source lines are the previous project's source: `RHWeaponDef.cpp` 658 to 668 (`WallPush` 220, `DerivedDamageMult` 1.5, caps Radial and Area), `RHSpecComposition.cpp` 656 to 672 and 693, `RHBulletSubsystem.cpp` 901 to 909 and 957 to 962, `RHEnemySubsystem.cpp` 60, 683, 878 and 1794.

| Cell | Old | New | From |
|---|---|---|---|
| `reforging.csv` `Evolve.RingEmitter` `caps` | `Projectile;Radial;Area;Expiry` | `Radial;Area` | the source's caps |
| `band_uu` | 100 | removed (the whole disc) | the source's wall is a zone |
| `dps` | 2 | 6 | `DerivedDamageMult` 1.5 of a round a tick; `dps` × `tick_s` = 1.5, so 1.5 / 0.25 = 6 |
| `push_uu_s` | 200 every step | 73, applied once a tick (18.25 uu a tick) | a kick of 220 decaying at 10 a second before each 60 Hz step: 220 × (1 − 10/60) / 10 ≈ 18.3 uu a tick, four ticks a second ≈ 73 uu a second on inverse mass 1; flat for every role that can be moved (ruling 2) |
| `flag_compat.csv` `Flag.StatusApply`, column `Area` | none | native | the source's wall keeps ApplyPatronStatus (`RHSpecComposition.cpp` 661); ruling 10 |
| `flag_compat.csv` `Flag.Chain`, column `Area` | none | native | the port's own, kept because it acts on a ring's strike today; read again at the TUNE step (ruling 10) |

Each `reforging.csv` cell and the floor have a `number_change_log.csv` row marked `[S]` (the floor `[M]`), result `pending`. The `flag_compat.csv` cells have none: no `flag_compat.csv` change has ever had a log row (its last changes, `bc8c605`, `a2a0569`, `b69aa12`, have none), so this note is their record.

**`matrix.csv` regenerated: no cell moved** (`git diff` empty). The plain weapons' caps all hold a delivery cap ahead of Area on which both flags were native already. Computed over every weapon and every evolution row: the only resolutions that move are `Evolve.RingEmitter`'s `Chain` and `StatusApply`, from none to native.

## The rule in code (`Combat/DTWeaponSim.cpp`, `.h`)
- `StepForceField`: with no band, every enemy inside the current radius is struck each tick for `DamagePerSecond × TickS` (an impact, as before). Pulse Wall (`FField::IsWall()`: a force field that grows and ticks; the Vortex pull zone does neither) pushes once a tick by `push_uu_s × tick_s`, outward. The Vortex zone still moves every step as before. An enemy at the centre is struck and not moved. The boss ignores displacement in the pool and stays.
- `StrikeForceField` (the free volley, Chain Trigger's and the dash's): its band skip no longer applies, so it strikes everything inside once more, at the volley's source and scale, and pushes nothing.
- `ReactToImpact`: a flag that resolves none is skipped (`!IsNative() && !IsFallback()`), so "a none flag does nothing there" (ruling 11) is true for every reaction. Before this, `StatusApply` and `Chain` (and `Seek`, `CritSplit`, `Stillness`, the Momentum echo) ran without asking. No other weapon has a none flag (T-RFG-01), so this changes nothing elsewhere. This line is my judgement call, made so that the rule's sentence is true; the scheduler may take it out.
- The drawing: one beaded arc at the outer edge, no inner arc, no disc (ruling 8).
- Volley-time reads of a flag (Charge's every n-th shot, Stillness's charge, Split's `every_n_volley` ring, Echo's `every_n_volley`) were not audited for a none resolution on Pulse Wall.

## The fourteen modifiers (step 1, from code)
Ten go dormant: Split Core, Scatter Shell, Pierce, Lingering Field, Backwash, Ricochet, Homing, Orbit Lock, Boomerang and Spread Wave. Four stay live, each bounded:
- Conduit (ruling 10): one status application per struck enemy, bounded by the status caps.
- Chain Trigger: one free volley per `min_interval_s`.
- Spillover: one jump per kill.
- Refit: once per `interval_s`.

None multiplies without a bound.

As ported, nine were live in name and did nothing on the wall. Backwash and Conduit acted on every strike, and Chain Trigger, Spillover and Refit on kills. A dormant socket still counts for `Best.EliteKillMods` (`dev/s3` `Meta/DTRunRecorder.cpp:119` counts `Sockets.Num()`). The full table is kept with this job's working files.

## Expectations that moved
| Test | Old | New | Why |
|---|---|---|---|
| `Dusktracer.Reforging.Catalog` (T-RFG-01) | every evolution resolves every flag | the same, except Pulse Wall: `Chain` and `StatusApply` native, every other flag none, asserted flag by flag | ruling 11's stated exception |
| `.PulseWall` | Core at the centre: 0 hits ("what the ring passed is left alone") | Core struck each tick, at least as often as the rim, each hit one tick of the row | the whole disc strikes |
| `.PulseWallLook` | outer arc and the band's inner edge | exactly one 360° arc, at the outer edge | no band (ruling 8) |
| `Dusktracer.Modifiers.ChainTrigger.PulseWall` | the inside: 0 hits after the kill | the inside: one Chain Trigger strike per kill | the free volley strikes what stands inside |
| `Card.Quicksilver.Vector.PulseWall` | the inside: 0 hits after the dash | the inside: one Keeper.Quicksilver strike per dash | the same |
| `.PulseWallGap` | — | unchanged | — |

New test `Dusktracer.Reforging.PulseWallDisc`: an enemy inside is struck every tick for `dps × Damage × tick_s` and pushed once a tick by `push_uu_s × tick_s`, outward. Its run reads 6 hits, 6 pushes, from 200 uu to 309.5 uu. The boss is struck and stays; an enemy outside the radius is untouched; one ring is live; Pierce is dormant and Conduit live on the evolved weapon. `C.GATE.TEST_FLOOR` 481 to 482.

## The perf bound (a bound on a shared machine, not a reading of record)
`& Tools\Perf.ps1 -Scene S2`, all read tonight. The S2 scene evolves the Ring Emitter (3 rings per capture window).

| Run | Build | Game thread p50 / p95, ms | `SimTick` mean / p95, ms | GPU p95, ms | PERF |
|---|---|---|---|---|---|
| before 1 | `f041733` | 3.02 / 4.51 | 0.91 / 1.90 | 4.55 | PASS |
| before 2 | `f041733` | 3.05 / 4.55 | 0.93 / 1.93 | 4.60 | PASS |
| after 1 | `c7df085` | 3.06 / 4.33 | 0.99 / 1.83 | 9.01 | FAIL (GPU over budget) |
| after 2 | `c7df085` | 3.32 / 4.60 | 1.04 / 2.08 | 8.97 | FAIL (GPU over budget) |
| base again | `d5d4a81`, detached | 3.22 / 4.73 | 1.08 / 2.18 | 8.99 | FAIL (GPU over budget) |
| after again | `c7df085` | 3.04 / 4.58 | 0.89 / 1.99 | 4.14 | PASS |

- **Game thread: under the stop line.** The worse after run (4.60) is 0.05 ms above the worse before run (4.55), and the after build never reads more than 0.3 ms above the base read beside it (4.58 against 4.73).
- **GPU:** both after runs failed the GPU budget, and so did the base read again on `dev/s2`'s own code at the same time. The after run read straight after it passed. The GPU load was the machine's, not the change's.
- **What else was running:** at 18:37 `Tools\Alone.ps1 -Status` showed the Sound session's gate holding a machine slot since 18:27 (`mod-sound-bafcf9`, `Gate.ps1`), beside my build. The owner's own programs were running (an Android emulator, as the coordinator said); I did not look further.

## The reading
```
& Tools\BotRuns.ps1 -Arms Steered:Storm:Tempest:Tide -Carry Weapon.RingEmitter -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 6 -Stagger 0.4 -Out Saved\BotRuns\d070_arm
& Tools\BotRuns.ps1 <the same> -CarryHold -Out Saved\BotRuns\d070_hold
python -I Tools\evolve_read.py --carry Saved\BotRuns\d070_arm --hold Saved\BotRuns\d070_hold --check-windows 0.95 0.5
```

| Judged pair (Tempest) | Evolved | Hold | Ratio |
|---|---|---|---|
| Before the boss, all sources a minute | 16 401 | 16 942 | **0.97** (bar 0.95: met) |
| ... Keeper.Storm / Weapon.RingEmitter / Keeper.Tide | 7 590 / 5 536 / 1 395 | 8 801 / 2 799 / 1 478 | |
| ... kills a minute, damage taken a minute | 335, 14.6 | 365, 1.8 | |
| Boss fight, all sources a minute | 6 586 (11 of 15 reached it) | 16 650 (15 of 15) | **0.40** (bar 0.5: not met) |
| ... Keeper.Storm / Weapon.RingEmitter / Weapon.PulseRifle | 2 635 / 2 069 / 1 199 | 5 900 / 5 793 / 1 001 | |
| ... fight length, damage taken a minute | 112 s, 29.4 | 84 s, 11.1 | |
| The whole time after the pick (decides nothing) | 14 556 | 17 124 | 0.85 |

Printed, judged nowhere, for the evolved arm (the scratch reading of `S2_d061_pulse_wall.md`):
- nearest enemy after the pick 280 uu, live enemies 28;
- damage taken 17.6 a minute (Boss 3.8, Rusher 3.7, Turret 1.2);
- kills 246 a minute;
- reach 11 of 15, wins 6 of 15; deaths 9 (Boss 5, one each Rusher, Turret, Tank, Swarm).

Against `REFORGE.POWER_MAX` 1.8: before the boss the build reads 0.97 of its hold, under the band (`REFORGE.POWER_MIN` 1.3).

Beside the plan note's earlier columns:
- as ported, before the boss 0.80 to 0.92, the fight 0.27 to 0.31;
- D-067 (whole time) 0.74;
- this rule: before the boss 0.97, the fight 0.40, whole time 0.85.

The fight's loss is Keeper.Storm (5 900 to 2 635) and the wall's own damage on the boss (5 793 to 2 069). Ruling 12 said this could happen: the Split children do not come back.

**The hold check:** the new hold against `Saved\BotRuns\d061_hold`, run for run: outcome, end time, steps and `input_hash` equal on all 15.

## The second pair (ruling 13, printed and judged nowhere)
`Steered:Storm:ArcGrid:Tide`. Arc Grid's cards are all `Chain` and the Shock that rides its jumps, and a ring's strike starts a chain (the table kept with the job's files). It differs from the judged pair in the archetype alone.

The same commands with `-Arms Steered:Storm:ArcGrid:Tide`, `-Out Saved\BotRuns\d070_arm_arcgrid` and `d070_hold_arcgrid`, read by `evolve_read.py --check-windows 0.95 0.5` (printed only).

| Second pair (Arc Grid) | Evolved | Hold | Ratio |
|---|---|---|---|
| Before the boss, all sources a minute | 16 489 | 17 080 | 0.97 |
| ... Keeper.Storm / Weapon.RingEmitter / Keeper.Tide | 10 029 / 3 150 / 964 | 10 063 / 2 731 / 1 159 | |
| Boss fight, all sources a minute | 5 378 (12 of 15 reached it) | 15 104 (15 of 15) | 0.36 |
| ... Weapon.RingEmitter / Keeper.Storm / Weapon.PulseRifle | 1 728 / 1 469 / 1 135 | 5 240 / 3 871 / 1 026 | |
| ... fight length, boss HP left | 126 s, 0.03 | 80 s, 0.00 | |
| The whole time after the pick | 14 112 | 17 022 | 0.83 |

More, from the scratch reading:
- nearest enemy after the pick 317 uu against 511 uu; live enemies 21 against 11;
- damage taken 14.9 a minute against 6.0 (Boss 4.4, Rusher 3.4, Turret 1.3 against Boss 2.7);
- kills 240 against 262 a minute;
- wins 6 against 14 of 15.

What it tells apart: the Arc Grid build, whose Keeper rides chains and not split children, reads the same before the boss (0.97) and no better in the fight (0.36 against the Tempest pair's 0.40). So the fight's shortfall is the weapon's, not one archetype's. Against the boss the wall deals about a third of the plain rings' damage (1 728 against 5 240 here, 2 069 against 5 793 in the judged pair), and the Keeper damage that rides its strikes falls with it.

## Not done
The whole gate and the five-arm seed 9107 check were not run (a miss: ruling 12 and the brief). The look of a disc that strikes is the Visual session's (ruling 8).
