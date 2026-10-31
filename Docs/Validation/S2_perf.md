# S2 perf proof scene (2026-10-31)

Branch `dev/s2-perfproof`, from `dev/s2` f70d651. Class: tool, no game rule. Layer: `Debug/`, `Tools/Perf.ps1`, read-only counters and CSV timers in the sim. Replaces nothing: the caps scene and its budget stand. No game number moved. No human, and no rendered look, is read here.

## Reproduce
```
& Tools\Perf.ps1 -Scene S2                                      # the proof
& Tools\Perf.ps1 -Scene S2 -Cmds "DT.PerfScene S2 NoDuoFields"  # the control of the emit-cap reading
& Tools\Perf.ps1 -Scene S2 -Cmds "DT.PerfScene S2 NoReactions"  # must fail
& Tools\Perf.ps1 -Scene S2 -Cmds "DT.PerfScene S2 Hp=300"       # the crowd sweep
```
The switch keeps every budget of the caps scene and adds a proof: the verdict reads the capture window, not the settings. Sums are the span of `DT/S2Sum_*`, live counts the median of `DT/S2Live_*`. It fails when Steam is under `C.PERF.S2_STEAM_MIN_PER_S`, fewer pairs than `C.PERF.S2_PAIRS_MIN` fired, median elites are under `C.PERF.S2_ELITES_MIN`, an affix has none alive, no Shade stands, the window holds no lance, ring, kill, dash, duo field, discharge, husk or pillar, or the build changed in the window. `-Scene S2` also prints the `DT/` phase timers as a profile.

## The scene
- **Build.** Four weapons at level 8 with three sockets each (`C.WEAPON.SLOTS`): Rifle, Ring Emitter evolved to Pulse Wall, Seeker Swarm evolved, Spiral Projector.
- **Blessings.** All four cards of one archetype of all six Keepers, all 15 duo cards, all six Path rows. Forge and Tide are Patron and second, so Burn and Chill feed Steam.
- **Crowd.** `DT.Spawn.Keep 400` at `C.PERF.S2_CROWD_HP_SCALE`, with 24 elites held among them, the six affixes in turn over the four roles.
- **Hero.** Walks a circle and dashes whenever a charge is ready; takes no damage; earns no XP, so no hand changes the build.
- **Bullets.** Real rounds plus filler up to `C.PERF.S2_FILL_RATIO` of the cap. A filler round collides as any round does but deals nothing and belongs to no player, so it kills nothing and starts no reaction (`Dusktracer.Reactions.PerfFiller`). That is what lets weapons reach the crowd, which the caps scene's emitter does not.

## Proof
Four runs of the scene on the same game code, 1800 frames at 1920x1080. One run's window, in full:

| Reaction | Fires | Per s | | Counted | In the window |
|---|---|---|---|---|---|
| ForgeTide (Steam) | 3687 | 319 | | Window | 11.5 s, 693 steps |
| ShadeRadiance | 2846 | 247 | | Kills | 467, 40/s |
| StormForge | 2704 | 234 | | Lances | 510 |
| StormTide | 2614 | 226 | | Rings opened | 3 |
| TideRadiance | 1441 | 125 | | Shades raised / alive | 467 / 10 |
| ForgeRadiance | 1509 | 131 | | Duo fields opened | 72 |
| StormRadiance | 1426 | 124 | | Discharges | 2671 |
| TideShade | 458 | 40 | | Husks / pillars | 24 / 211 |
| StormShade | 455 | 39 | | Elites alive, median | 24 |
| ForgeShade | 452 | 39 | | Each affix alive, median | 2 to 5 |
| RadianceQuicksilver | 36 | 3 | | Tracked rounds, median | 1128 of 5808 |
| StormQuicksilver | 2 | 0.2 | | Fields live, median | 120 |
| ForgeQuicksilver, TideQuicksilver, ShadeQuicksilver | 0 | 0 | | Dashes | 4 |

12 of 15 pairs fired in the window. The three silent ones are dash reactions and Wildfire, which lit once before the window and stayed lit.

| Run | Bullets / enemies p50 | Game p50 / p95 ms | GPU p95 raw (proxied) | Verdict |
|---|---|---|---|---|
| S2 | 5808 / 400 | 3.18 / 4.86 | 5.73 (9.17) | PASS |
| S2 | 5804 / 400 | 4.02 / 5.94 | 4.82 (7.71) | PASS |
| S2 | 5808 / 400 | 3.21 / 4.90 | 5.82 (9.31) | PASS |
| S2 | 5806 / 400 | 4.31 / 6.32 | 5.95 (9.52) | PASS |
| Budget | 5700 / 380 | 8 / 10 | 12 | |
| Caps scene, same build | 5975 / 388 | 2.59 / 3.45 | 4.71 (7.54) | PASS |
| `NoReactions` | 5807 / 400 | 3.43 / 5.14 | 5.21 (8.34) | FAIL as it must: Steam 0, 0 pairs, no duo field |

Budgets pass on all four, so nothing was fixed and no profile was owed. The spread is contention: another agent's compiler was running in most of these runs (`Saved/gpu.lock` is per worktree and cannot see it), which by `11_Quality §5` voids them as readings of the budget margin. They are upper bounds of a passing run, not clean numbers. The capture runs uncapped near 130 fps, so about 0.4 steps fall in a frame: the p50 is mostly a frame with no step, and p95 is the figure to trust.

One frame's cost at p95 of each phase: observer 0.48, pool 0.42, weapon effects 0.35 (duos 0.19 of it), draws 0.69, whole sim tick 2.15 ms.

## How hard the crowd is decides this, and it is a scene setting
Every proof above stands at 1000× swarm HP. That number is mine, chosen after 30× failed, so the sweep is the honest result:

| Crowd HP | Kills/s | Enemies p50 | Game p50 / p95 ms | Verdict |
|---|---|---|---|---|
| 1000× | 40 | 400 | 3.2 to 4.3 / 4.9 to 6.3 | PASS |
| 300× | 181 | 397 | 4.45 / 6.12 | PASS |
| 100× | 7523 | 274 | 13.43 / 22.67 | FAIL |
| 30× | 9706 | 239 | 12.66 / 15.22 | FAIL |

The real kill rate sits near 20 a second (the arrival ceiling of `S1_keeper_power.md`), so 1000× already kills twice that and 300× nine times. Between 300× and 100× the build runs away: on-kill reactions rise from about 40 to thousands a second, and the crowd stays under the cap though it is refilled every step. That is not a state a run reaches: it needs all six Keepers at over 300 times the real kill rate. It is reported because the budget does fail there, and what costs the time at 100×, per frame: weapons 2.92, weapon effects 2.90, observer 2.19, duos 1.36 of the effects, tick 9.89 ms.

**A lead from that profile, not fixed:** a Shade that replaces the oldest at `shades_max` starts with its timer at zero, so at the cap every Marked kill fires a free volley of the first weapon, ahead of R-RFG-19's "on that weapon's cooldown". It likely carries much of `StepWeapons` at 100×, though no timer splits it. It is also a damage gain at any kill rate, so fixing it moves bot numbers: that is a BUG to triage on its own. `ObserveBullets` also rebuilds two hash maps per step, which is the first place to look if the observer ever matters. Neither is needed for this proof.

## Do duo fields spend the weapon's emit cap?
**Yes, slot 0's, and in a saturated slot they mostly lose.** Arc Furnace and Lightning Step open fields on weapon slot 0 through `TryEmit`, counted now as `DuoFields` and `DuoFieldsThrottled` (`Dusktracer.Reactions.Duo.FieldEmit`).

| Scene | Slot 0 spawned/s | Throttled/s | Duo fields opened/s | Dropped/s |
|---|---|---|---|---|
| Full | 390 | 2743 | 6.2 | 229 |
| `NoDuoFields` | 408 | 2369 | - | - |
| `NoReactions` | 402 | 1762 | - | - |

- Chains and children hold slot 0 at `C.WEAPON.SPAWN_RATE_MAX_PER_S` with or without the fields, so 97 % of duo fields are dropped, and those that open take 1.6 % of the slot. Spawned moves within the runs' noise; windows differ.
- So the fear of `S2_duos.md` runs the other way: a duo cannot throttle its rifle much, because fields spawn last in the step, after volleys and chains. At saturation it is the field that goes.
- A real arm is nowhere near this: Storm/ArcGrid:Forge fires 963 a run in `S2_duos.md`, about 1.4 a second, 0.35 % of the cap even if each opened a field. Whether a real arm's slot 0 saturates is not read here.

Every slot of this build runs at about the cap. It is the cap, not the crowd, that limits emission, so R-WPN-09 holds the scene.

## What this proves
- Both caps, all systems named in the brief, and counts of each, in one window, under the budgets, at a kill rate twice the real ceiling and a Steam rate 50 times the worst arm.
- The scene fails when reactions do not fire.
- The caps scene and its budget are unchanged.

## What it does not prove
- **Reachability.** No run owns six Keepers, 15 duo cards and six Path rows: a run has two Keepers and one L16 card. This bounds a real build from above on reactions; it does not read one.
- **Real rounds at the bullet cap.** 1128 of 5808 are tracked weapon rounds, held there by the spawn cap. The rest are filler, so the observer's cost at 6000 tracked rounds is unread, and no build here reaches it.
- **Intercession and Keeper casts:** none is pressed. Time Dilation is out of this job.
- **Bosses:** the director is off.
- **Shield and hit-reactive affixes on their own terms:** filler hits count against a Shielded elite's hits and against every hit-counting rule of an enemy.
- **The renderer on target hardware:** GPU is the 4080 proxy (D-011). Fields draw as discs and reaction cues are capped, so visual density is not a look, and contended GPU readings stand as bounds.
- **A clean margin:** see the contention note.
- **Frames at 60:** the capture is uncapped, so per-frame p50 reads under what a capped 60 fps frame would read.
