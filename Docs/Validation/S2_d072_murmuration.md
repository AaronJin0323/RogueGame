# S2: D-072, a Murmuration missile deals the row's damage on the source's cooldown

Branch `s2/d072-murmuration`, at `dev/s2`'s tip `d5d4a81` with no commits when this job took it, then `dev/s2` at `2818103` merged in (one documents file, the D-070 note). Class RULE (`reforging.md` R-RFG-11) with the row's cells set to the source's values, each cited and none tuned. **The hypothesis is met:** before the boss 1.07 of the hold, in the boss fight 0.91.

Build:
- Base: `d5d4a81`. The perf before pair, the port's own arm, the hold.
- Branch: `11c7ffa` (the rule, the cells, the tests). The perf after pair, the rule's arm, the hold read again. The merge of `2818103` brought no engine file, so those readings stand on the merged tree.
- Gate: `3059d82`, the branch with `dev/s2` at `2818103` merged in. Only this note comes after it.

## The record's hypothesis
"We believe that giving the Murmuration missile the row's damage on the source's cooldown moves the build's damage per minute in the boss fight from 0.37 of its hold to at least 0.6, ... and that before the boss the build stays at 0.95 of its hold or more" (D-072).

## The source, read here
Read-only, in the previous project's `Source/RogueHero/Combat` (outside this repository):
- `RHWeaponDef.cpp` 652 to 656, the Murmuration: life 6 s, pierce 200, cooldown 1.6, the Murmuration behaviour. No damage is set, so the plain missile's stands.
- `RHWeaponDef.cpp` 251 to 275, the plain Seeker Swarm: damage 9, cooldown 1.1. Its level rows multiply the cooldown by 0.8 at level 4 (268) and again at level 7 (271). The evolution copies them.
- `RHSpecComposition.cpp` 454 to 456: the behaviour sets retarget-on-hit and nothing else.
- `RHBulletSubsystem.cpp` 1051 to 1055: after a hit the round seeks a new target past its history.
- `RHBulletSubsystem.h` 121: a history of 8.

`cooldown_mult`, the evolved cooldown over the plain one at the levels an evolved weapon stands at:
- level 6: 1.6 × 0.8 = 1.28 over 1.1 × 0.8 = 0.88;
- levels 7 and 8: 1.6 × 0.64 = 1.024 over 1.1 × 0.64 = 0.704.

That is 1.4545 at all three levels (16 / 11), so that is the value taken.

## The cells, `reforging.csv` `Evolve.SeekerSwarm`
| Cell | Old | New | Source |
|---|---|---|---|
| `dmg_ratio` | 0.2 | 1 | `RHWeaponDef.cpp` 652 to 656 set no damage; the plain 9 at 255 |
| `cooldown_mult` | (none) | 1.4545 | 1.6 at 655 over 1.1 at 256, levels as above |
| `rehit_after` | (none) | 8 | `RHBulletSubsystem.h` 121 |

The capabilities stay as they were (ruling 4). Each cell has a number-log row tagged `[S]`.

## The code
- `Combat/DTReforge.cpp`: the Persist composition multiplies the row's cooldown by `cooldown_mult`.
- `Combat/DTWeaponSim.cpp`: a persisting round carries `rehit_after`.
- `Combat/DTBulletPool`: `HasHit` reads only that round's last `rehit_after` hits, where every other round reads all `C.BULLETS.HIT_MEMORY`.

A finding: every round in this port already keeps the last `C.BULLETS.HIT_MEMORY` (8) enemies it struck (`bullets.md` R-BUL-04), and a Murmuration's seek skipped those same eight. So before this rule a round could already return to an enemy once eight others came after it, as the source's does. `rehit_after` 8 reproduces that and makes it the row's cell. A TUNE can lower it; it cannot raise it past the memory the round keeps, and R-RFG-11 says so. The plain Seeker Swarm's rounds carry no window and take the old path unchanged.

## Tests
- `Dusktracer.Reforging.Murmuration` (T-RFG-11), extended in the same function, so the test count does not change.
  - A missile deals `dmg_ratio` of the row's damage, the row's own, every strike whole.
  - The evolved weapon launches at the row's cooldown times `cooldown_mult`.
  - A lone target is struck, at most once per missile.
  - One homing round among three enemies: under the row's `rehit_after` it strikes each once, and with a window of two it returns once two others came after it.
- No existing expectation moved.
- Results on the branch: `Dusktracer.Reforging` 26 of 26, `Dusktracer.Weapons` 15 of 15 (the plain Seeker Swarm among them, unchanged), `Dusktracer.Bullets` 8 of 8.

## Perf
The S2 scene holds the evolved Seeker Swarm (`Debug/DTDebugPerfScene.cpp` 61), so this bound sees the rule. All runs were read tonight, in turn; the owner's programs were running and were not touched.

| Run | Build | game p50 / p95 ms | SimTick p95 ms | GPU p95 ms | Verdict |
|---|---|---|---|---|---|
| before 1 | `d5d4a81` | 3.02 / 4.49 | 1.88 | 4.18 | PASS |
| before 2 | `d5d4a81` | 3.05 / 4.53 | 1.97 | 4.22 | PASS |
| after 1 | `11c7ffa` | 3.00 / 4.39 | 1.79 | 4.21 | PASS |
| after 2 | `11c7ffa` | 3.11 / 4.68 | 2.03 | 4.23 | PASS |

The worse after run against the worse before run is +0.15 ms, under the 0.3 line, and no run failed a budget. No stop.

## The reading
`& Tools\BotRuns.ps1 -Arms Steered:Storm:Tempest:Tide -Carry Weapon.SeekerSwarm -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 6 -Stagger 0.4`, read by `evolve_read.py --check-windows 0.95 0.6`.

- Before the boss: `1.07 (19526 / 18282; 15 and 15 runs)`.
- Boss fight: `0.91 (13893 / 15311; 15 and 15 runs reached it)`.
- `WINDOWS: PASS (SeekerSwarm before the boss 1.07 >= 0.95, SeekerSwarm boss fight 0.91 >= 0.6)`. **Met.**
- Whole time after the pick: 1.04, which decides nothing.
- The hold read again on the branch build (`d072_hold_branch`) equals step 0's hold run for run: 15 of 15, 0 differing in outcome, end, decisions, level, hits or Keeper share.

### Three columns on the same base and seeds (printed, judged nowhere)
| | Hold | Port's arm, `d5d4a81` | Rule's arm, `11c7ffa` |
|---|---|---|---|
| Build against its hold, before the boss | 1 | 0.97 | 1.07 |
| Build against its hold, boss fight | 1 | 0.40 | 0.91 |
| The weapon's own damage a minute, before the boss / fight | 4322 / 7501 | 6064 / 1794 | 7952 / 5769 |
| Keeper.Storm's damage a minute, before the boss / fight | 10148 / 4057 | 6672 / 1954 | 7626 / 4955 |
| Weapon hits a minute after the pick | 357 | 1551 | 484 |
| Damage taken a minute, before the boss / fight | 0.0 / 11.3 | 0.0 / 7.8 | 0.0 / 11.2 |
| The fight's length | 79 s | 106 s | 85 s |
| Reach / wins | 15 / 15 | 15 / 14 | 15 / 15 |

On the same base, the rule lifts the build in the boss fight from 0.40 to 0.91 of its hold and before the boss from 0.97 to 1.07. It triples the missile's damage in the fight and brings Keeper.Storm, which rides a hit, back above its hold there. The weapon's hits fall from 1551 to 484 a minute: fewer missiles, each with its whole hit.

The plan note's figures (0.98 before the boss, 0.37 in the fight) were read on an older tree; this tree's port reads 0.97 and 0.40.

`REFORGE.POWER_MAX` (1.8): before the boss the build reads 1.07 of its hold, well under it. No TUNE downward is called for.

## Discoveries
`dev/s3`'s `discoveries.csv` has no per-weapon counter for the Seeker Swarm; `Survive10` only opens the weapon.

## The gate
`GATE: PASS (3059d82; 3 not run; mask FFFFF3FC)` (`& Tools\Gate.ps1 -Base dev/s2`).
- `tests: 481 run of 481 found, 0 failed, floor 481`: no test function was added and the floor stays. `tools: TOOLS: PASS (220 tests)`.
- `restart: RESTART: PASS (seed 9107, input_hash 276faf8e / 276faf8e ...)`: unmoved.
- `dev/s2`'s gate prints no line of the five seed 9107 arm hashes; its log names seed 9107 only on the restart line, so those five were not read here.
- `pad`, `scope` and `mac` not run: no package, no brief in flight, D-007.

## Not shown
No person has played a Murmuration.
