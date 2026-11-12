# S2: `evolve_read.py` judges a boss fight on the damage the boss takes (D-081)

**Build:** none. This is Tools and documents only, on `s2/evolve-read-boss` from `dev/s2` 1106820. No new run was made. Every run file read here was read in place, read-only, in two other worktrees.

## Why
The Warden's health is set once from the damage it takes in its first seconds (`boss.md` R-BOS-09), and it is clamped. So a fight's length reads the calibration as much as the build.

D-081 therefore judges a boss-fight clause on the damage dealt to the boss a minute. It also keeps the old window as a printed line, and never prints a fight's length alone.

## What the reader prints now
`python -I Tools\evolve_read.py --carry <arm> --hold <hold> [--check-windows <before> <fight> [--judge-fight-on-old-window]]`

Everything before the boss fight's line prints as it did: the carry lines, the split's first lines and the before-the-boss window. The fight's block changes.

- **Damage to the boss a minute, per run:** `effective_max` (`boss_calibrated`) × (1 − `hp_left_frac`) / `fight_time_s` × 60, the last two from `boss_end`.
  - The batch prints the median over the runs that reached the boss, and the pooled figure (all damage over all seconds) beside it.
  - A run that reached the boss without either record is left out and counted: "(N not read)".
- **The length line:** the length, then the median effective max, then how many runs sit on the floor and how many on the ceiling.
  - The floor is `BOSS.HP_BASE` × `BOSS.HP_MIN_MULT` and the ceiling `BOSS.HP_BASE` × `BOSS.HP_MAX_MULT`. Both are read from the constants by `dtconst`.
- **The old window:** every source's damage a minute in the fight, the median over runs. It prints on its own line beside:
  - the hero's damage taken a minute in the fight;
  - the boss's health left as the median over the **lost** fights, with their count. A won fight's 0 no longer hides a lost one's remainder.
- **An evolution's own strikes** (D-081 item 6): if the carried weapon's `weapon` records after the pick hold `strike_hits` and `strike_damage`, the reader prints the strikes' share of the weapon's hits and damage. Otherwise it prints `strikes: not read (the run files hold no strike fields)`. No run file holds those fields yet.
- **Arm against hold, three lines:**
  - the before-the-boss window, as before;
  - damage to the boss a minute, as the ratio of the medians and the ratio of the pooled figures;
  - the old window's ratio.
  - The whole-time line still prints and decides nothing.
- **The check:** `--check-windows B F` judges the first number as before.
  - The second number is judged on the ratio of the medians of damage to the boss a minute.
  - The fight is judged only when ten runs of the arm have the boss's measure (`FIGHT_RUNS_MIN`, D-081 item 1). Below that, the line says `boss fight not judged (N runs reached it, 10 needed)` and the check fails.
- **The switch:** `--judge-fight-on-old-window` judges the fight on the old window instead, so that a verdict landed before D-081 prints as it was read. Its line says "old window". The ten-run count applies to it too.

## The five readings
Run files are in two worktrees, read in place:
- `E:\Dusktracer\.claude\worktrees\agent-a2b736d35d80303fb\Saved\BotRuns\`: D-072 and D-079.
- `E:\Dusktracer\.claude\worktrees\agent-a37e1827c3d77226d\Saved\BotRuns\`: D-074.

Each pair is fifteen seeds, bot v2 at skill 1.0. The bars are each rule's own.

| Pair (arm; hold) | Bars | Boss damage a minute, median: D-081, read | Pooled: D-081, read | Old window: landed, read |
|---|---|---|---|---|
| D-072 Tempest (`d072_arm`; `d072_hold`) | 0.95 0.6 | 0.99, 0.99 | 0.91, 0.91 | 0.91, 0.91 |
| D-074 Tempest (`d074_rule_arm`; `d074_hold`) | 0.95 0.9 | 1.07, 1.07 | 1.13, 1.13 | 1.12, 1.12 |
| D-074 Arc Grid (`d074_rule_arm_arcgrid`; `d074_hold_arcgrid`) | 0.95 0.9 | 1.17, 1.17 | 1.16, 1.16 | 1.15, 1.15 |
| D-079 Tempest (`d079_rule_tempest_arm`; `d079_base_tempest_hold`) | 0.95 0.9 | 1.04, 1.04 | 1.15, 1.15 | 1.03, 1.03 |
| D-079 Arc Grid (`d079_rule_arcgrid_arm`; `d079_base_arcgrid_hold`) | 0.95 0.9 | 1.35, 1.35 | 1.37, 1.37 | 1.35, 1.35 |

Every figure equals D-081's to two places.

The fight lines, arm then hold:

```
D-072 Tempest
  boss fight (15 of 15 runs): length 85 s, effective max 18571 (median), on the floor 0, on the ceiling 0; damage to the boss 13409 a minute (median), 13190 pooled
  boss fight (15 of 15 runs): length 79 s, effective max 18542 (median), on the floor 2, on the ceiling 0; damage to the boss 13539 a minute (median), 14501 pooled
SeekerSwarm evolved against hold, boss fight, damage to the boss a minute: 0.99 by the medians (13409 / 13539), 0.91 pooled (13190 / 14501); 15 and 15 runs read
SeekerSwarm evolved against hold, boss fight, the old window: 0.91 (13893 / 15311; 15 and 15 runs reached it)
WINDOWS: PASS (SeekerSwarm before the boss 1.07 >= 0.95, SeekerSwarm boss fight, damage to the boss 0.99 >= 0.6 (15 runs reached it; old window 0.91))
WINDOWS: PASS (SeekerSwarm before the boss 1.07 >= 0.95, SeekerSwarm boss fight, old window 0.91 >= 0.6 (15 runs reached it))

D-074 Tempest
  boss fight (15 of 15 runs): length 73 s, effective max 21359 (median), on the floor 1, on the ceiling 0; damage to the boss 16949 a minute (median), 17783 pooled
  boss fight (15 of 15 runs): length 84 s, effective max 21976 (median), on the floor 1, on the ceiling 0; damage to the boss 15882 a minute (median), 15767 pooled
RingEmitter evolved against hold, boss fight, damage to the boss a minute: 1.07 by the medians (16949 / 15882), 1.13 pooled (17783 / 15767); 15 and 15 runs read
RingEmitter evolved against hold, boss fight, the old window: 1.12 (18648 / 16650; 15 and 15 runs reached it)
WINDOWS: PASS (RingEmitter before the boss 1.01 >= 0.95, RingEmitter boss fight, damage to the boss 1.07 >= 0.9 (15 runs reached it; old window 1.12))
WINDOWS: PASS (RingEmitter before the boss 1.01 >= 0.95, RingEmitter boss fight, old window 1.12 >= 0.9 (15 runs reached it))

D-074 Arc Grid
  boss fight (15 of 15 runs): length 78 s, effective max 21784 (median), on the floor 1, on the ceiling 0; damage to the boss 16885 a minute (median), 16500 pooled
  boss fight (15 of 15 runs): length 80 s, effective max 20571 (median), on the floor 0, on the ceiling 0; damage to the boss 14430 a minute (median), 14210 pooled
    the old window: all sources 15104 a minute; damage taken 20.5 a minute; boss HP left in the lost fights 0.13 (1 lost)
RingEmitter evolved against hold, boss fight, damage to the boss a minute: 1.17 by the medians (16885 / 14430), 1.16 pooled (16500 / 14210); 15 and 15 runs read
RingEmitter evolved against hold, boss fight, the old window: 1.15 (17411 / 15104; 15 and 15 runs reached it)
WINDOWS: PASS (RingEmitter before the boss 0.99 >= 0.95, RingEmitter boss fight, damage to the boss 1.17 >= 0.9 (15 runs reached it; old window 1.15))
WINDOWS: PASS (RingEmitter before the boss 0.99 >= 0.95, RingEmitter boss fight, old window 1.15 >= 0.9 (15 runs reached it))

D-079 Tempest
  boss fight (13 of 15 runs): length 128 s, effective max 14871 (median), on the floor 3, on the ceiling 0; damage to the boss 9674 a minute (median), 9994 pooled
    the old window: all sources 11118 a minute; damage taken 25.1 a minute; boss HP left in the lost fights - (0 lost)
  boss fight (15 of 15 runs): length 93 s, effective max 9600 (median), on the floor 8, on the ceiling 0; damage to the boss 9295 a minute (median), 8669 pooled
    the old window: all sources 10810 a minute; damage taken 11.6 a minute; boss HP left in the lost fights - (0 lost)
OrbitalDrones evolved against hold, boss fight, damage to the boss a minute: 1.04 by the medians (9674 / 9295), 1.15 pooled (9994 / 8669); 13 and 15 runs read
OrbitalDrones evolved against hold, boss fight, the old window: 1.03 (11118 / 10810; 13 and 15 runs reached it)
WINDOWS: PASS (OrbitalDrones before the boss 0.99 >= 0.95, OrbitalDrones boss fight, damage to the boss 1.04 >= 0.9 (13 runs reached it; old window 1.03))
WINDOWS: PASS (OrbitalDrones before the boss 0.99 >= 0.95, OrbitalDrones boss fight, old window 1.03 >= 0.9 (13 runs reached it))

D-079 Arc Grid
  boss fight (15 of 15 runs): length 94 s, effective max 10035 (median), on the floor 6, on the ceiling 0; damage to the boss 7794 a minute (median), 8074 pooled
  boss fight (15 of 15 runs): length 121 s, effective max 9600 (median), on the floor 8, on the ceiling 0; damage to the boss 5770 a minute (median), 5881 pooled
OrbitalDrones evolved against hold, boss fight, damage to the boss a minute: 1.35 by the medians (7794 / 5770), 1.37 pooled (8074 / 5881); 15 and 15 runs read
OrbitalDrones evolved against hold, boss fight, the old window: 1.35 (9015 / 6680; 15 and 15 runs reached it)
WINDOWS: PASS (OrbitalDrones before the boss 1.00 >= 0.95, OrbitalDrones boss fight, damage to the boss 1.35 >= 0.9 (15 runs reached it; old window 1.35))
WINDOWS: PASS (OrbitalDrones before the boss 1.00 >= 0.95, OrbitalDrones boss fight, old window 1.35 >= 0.9 (15 runs reached it))
```

- The effective max, clamp counts and fight lengths equal `boss_fight_measure_plan.md`'s table.
- The before-the-boss figures of D-072 (1.07) and D-074 (1.01 and 0.99) equal their records. D-079's (0.99 and 1.00) were not compared: its record on `main` does not quote them.
- In each pair, the first `WINDOWS:` line is the check by the boss's measure and the second is the same check with `--judge-fight-on-old-window`.
- Every strike line reads `strikes: not read (the run files hold no strike fields)`.

**No landed verdict moves** (D-081 item 5): the switch prints each of the three rules' fights as they were judged, and the boss's measure passes the same bars.

## Tests
These are in `Tools/tools_test.py`, class `EvolveSplit`, on made-up run files:
- Damage to the boss a minute from the two records, median and pooled. One run on the floor and one off it give a median of 9 300 and a pooled figure of 9 360.
- A run without `boss_end` and one without `boss_calibrated` are left out and counted ("2 not read").
- The length line carries the effective max and the clamp counts. The floor and the ceiling come from the constants and are told apart.
- Health left is read over the lost fights only.
- `--check-windows`:
  - the fight is judged on the boss's measure while the old window would pass;
  - fewer than ten runs is "not judged" and fails;
  - the switch gives the old window's verdict on the same files.
- Strike fields present: the carried weapon's records after the pick only. Strike fields absent: "not read".

The existing tests of the split and the check were changed to the new tuple and lines. `TOOLS: PASS (225 tests)`.

## Changed elsewhere
`S2_evolve_read_split.md`: two sentences say that since D-081 the fight is judged on the damage the boss takes, with the switch for the old window, and that its quoted verdicts were read before that change.

## Not verified
- The strike line on real run files: none holds `strike_hits` or `strike_damage` until D-073's branch adds them.
- The ten-run count was not held by the tool before this change, although the clauses asked for it. Every landed reading had at least 13 runs reaching the boss, so no verdict depended on it.
- A bot reading is never a human reading.
