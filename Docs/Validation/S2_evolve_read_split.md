# S2: `evolve_read.py` reads the time after the pick in two windows, split at the boss's start

**Build:** none. Tools and documents only, on `s2/evolve-read-split` from `dev/s2` f041733. No new run was made. Every run file read here was read in place, read-only, in two other worktrees.

## Why
D-070 judges its rule on two windows, each by itself, and asks for the split to be part of `Tools/evolve_read.py` with a tools test, "so that the reading is a tool's and not a scratch script's". The method and the numbers come from `Validation/pulse_wall_rounds_plan.md` on `main`, section "What the run files say".

## The reading
`python -I Tools\evolve_read.py --carry <arm> --hold <hold> [--check-windows <before> <fight>]`

Today's carry lines print first, unchanged. The split follows, once per carried weapon. A batch's run folders are grouped by their label's `Carry-<weapon>`, so a batch of ten weapons reads as ten pairs.

- **Before the boss:** from the `reforge` record to the `boss_start` record, or to `run_end` when the run never met the boss.
- **The boss fight:** from `boss_start` to `run_end`.
- **Which window a record belongs to:** each window is [start, end), so a hit exactly on the boss's start is the fight's.
- **Damage per minute:** from the `hit` records' `damage_by_source` at their exact times, all sources and by source. This way a short fight is read whole.
- **Kills per minute, before the boss only:** from the whole `sources` intervals inside the window. An interval runs from `interval` × `interval_s` to its record's `t`, so the last one, cut short by the run's end, counts at its own length.
- **Damage taken per minute:** from `hero_damaged`.
- **The fight's other figures:** its length, and the boss's `hp_left_frac` from `boss_end`.
- **Medians and counts:** medians are over every run with a pick before the boss, and over the runs that reached the boss in the fight; the line says how many. It also counts the runs that died before the boss.
- **Runs left out:** a run with no `reforge` record or no `run_end` is left out and counted. Only player 0's records are read.
- **Evolved against hold:** one line per window, each the ratio of the two medians. The whole-time figure, today's build damage per minute after the pick, prints beside them and is labelled "decides nothing".
- **Judging:** `--check-windows 0.95 0.5` prints `WINDOWS: PASS (...)` or `WINDOWS: FAIL (...)`, naming each weapon's window that missed, and sets the exit code. Without it, nothing is judged.

The definitions are those of the two scratch scripts the plan note used. Its by-source table and fight lengths came from the hit-record script, and the kills and damage taken from the interval script.

## The plan note's numbers, reproduced
Run files: `Saved\BotRuns\` of the `dev/s2` developer's worktree, batches `d061_hold`, `d061_arm_2`, `d061_arm_8` and `d067_arm` (Ring Emitter carry pair, 15 seeds, bot v2 at skill 1.0). Every figure below is the tool's printed line, and every one equals the plan note's.

| After the pick | Hold | As ported, `dps` 2 | As ported, `dps` 8 | D-067's carry |
|---|---|---|---|---|
| Died before the boss, of 15 | 0 | 8 | 7 | 6 |
| Before the boss: all sources, damage per minute | 16 942 | 13 604 | 15 632 | 15 162 |
| ... against the hold | 1.00 | 0.80 | 0.92 | 0.89 |
| Before the boss: kills per minute | 365 | 319 | 327 | 328 |
| Before the boss: damage taken per minute | 1.8 | 22.3 | 21.6 | 15.9 |
| Reached the boss, of 15 | 15 | 7 | 8 | 9 |
| Boss fight: length, s | 84 | 124 | 134 | 136 |
| Boss fight: all sources, damage per minute | 16 650 | 5 143 | 4 457 | 4 433 |
| ... against the hold | 1.00 | 0.31 | 0.27 | 0.27 |
| Boss fight: Weapon.RingEmitter, damage per minute | 5 793 | 170 | 717 | 203 |
| Boss fight: damage taken per minute | 11.1 | 23.3 | 30.8 | 18.0 |
| Boss HP left at the run's end | 0.00 | 0.25 | 0.14 | 0.07 |
| The whole time after the pick, against the hold (decides nothing) | 1.00 | 0.78 | 0.76 | 0.74 |

With `--check-windows 0.95 0.5`, each of the three arms prints `WINDOWS: FAIL` naming both windows. For example, `dps` 2: `WINDOWS: FAIL (RingEmitter before the boss 0.80 < 0.95; RingEmitter boss fight 0.31 < 0.5)`.

## The other evolutions under their hold
`S2_evolutions.md`'s batches still exist, in another worktree's `Saved\BotRuns\ev_arm` and `ev_hold`, 150 runs each. They are the same runs: the tool's whole-time medians repeat that note's build damage per minute exactly (Murmuration 15 458 / 18 593, Blade Halo 14 884 / 16 029, Vortex 15 809 / 16 901).

| Evolution (carried weapon) | Runs with a pick, evolved / hold | Before the boss, against the hold | Reached the boss, evolved / hold | Boss fight, against the hold | Fight length, s, evolved / hold | Whole time (decides nothing) |
|---|---|---|---|---|---|---|
| Murmuration (Seeker Swarm) | 15 / 15 | 0.98 (17 816 / 18 168) | 15 / 13 | 0.37 (6 176 / 16 895) | 103 / 54 | 0.83 |
| Blade Halo (Orbital Drones) | 15 / 15 | 0.83 (14 555 / 17 526) | 4 / 15 | 1.38 (11 943 / 8 663) | 45 / 92 | 0.93 |
| Vortex (Spiral Projector) | 13 / 13 | 0.98 (16 312 / 16 682) | 9 / 13 | 0.94 (12 780 / 13 549) | 93 / 67 | 0.94 |

- **Murmuration** is the Pulse Wall pattern: level before the boss, about a third of the hold in the fight, and the fight lasts nearly twice as long.
- **Blade Halo's** loss is before the boss: 11 of 15 runs die there, against 0 in the hold. Its fight figure rests on the 4 runs that got there.
- **Vortex** reads under its hold by a little in each window.

These are readings of a bot that kites, not of a player.

## Tools tests
Class `EvolveSplit` in `Tools/tools_test.py` uses small canned run files:
- a run that never meets the boss;
- one that meets the boss and wins, with a hit exactly on the boss's start;
- one that dies in the fight;
- a run with no pick, left out and counted.

It also covers the batch medians, each window against the hold, the check's PASS and both FAILs, a batch of two carried weapons read apart, and the command line in a folder whose name holds `[1]`.

`TOOLS: PASS (220 tests)`

`Docs/10_Tuning.md` does not describe `evolve_read.py`'s reading, so nothing was written there.
