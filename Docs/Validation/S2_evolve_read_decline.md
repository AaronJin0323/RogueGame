# S2: `evolve_read.py` takes the arm that declines the hand as the control, and reads each play floor with an interval (D-082)

**Build:** none. This is Tools and documents only, on `s2/evolve-read-decline` from `dev/s2` d1ceee1. No new run was made. Every run file read here was read in place, read-only, in two other worktrees.

## What the reader prints now
`python -I Tools\evolve_read.py --carry <arm>[,<arm>...] [--hold <hold>[,...]] [--control <decline>[,...]] [--check-windows <before> <fight>] [--check-floors [<before> <boss>]] [--judge-interval by-seed|by-arm] [--intervals]`

Without `--control` and `--intervals` it prints exactly what it printed before.

### The control
- `--control` names the folder of the arm that declines the Reforging hand.
  - A run of it has no `reforge` record, so its windows start at the Reforging hand's `offer` record. That is the same record `Tools/probe.py`'s play rate uses for the hand.
  - A run with neither record is left out and counted. The control's labels end `-Decline` (`BotRuns.ps1 -DeclineReforge`); that suffix is not part of the weapon.
- **The floor before the boss:** the median of each arm's damage a minute from its start to the boss's start (or the run's end), over the arm's runs that have a start and a `run_end`; card arm over control.
- **The boss floor:** the median of D-081's damage dealt to the boss a minute, over the arm's runs that reached the boss and have both of the boss's records; card arm over control. Beside it the line prints:
  - the pooled figure;
  - the calibrated health's medians, with the floor and ceiling counts;
  - the fight's length.
- Both floors are printed for the card's arm and, with `--hold`, for the hold's arm (the Path's card).
- The swarm's kills a minute are printed for each arm, against the control's (D-082 item 6): the last whole interval before the hand, then the whole intervals after it up to the boss.
- **The band's boss side:** one line, printed and judged nowhere. It says that by D-085 item 9 a card is in band when it is at least `C.REFORGE.POWER_MIN` on one side and at most `C.REFORGE.POWER_MAX` on both, and that the crowd's side comes from the probe's field. The tool judges floors and never the band.

### The intervals (D-082's ruling of 2026-11-13, amended at 04:17)
- **By seed: the judged interval.**
  - A battery's two arms share their seeds, so a seed with the figure on both arms is a pair.
  - A seed whose run lacks the figure on one arm leaves its pair: both arms' runs of that seed are dropped. For the boss floor this is a run that did not reach the boss; for the floor before it, a run with no window.
  - The pairs are drawn with replacement at their own count, and the ratio of the arm's median to the control's is taken over the drawn pairs. That is done 10 000 times from `random.Random(INTERVAL_SEED)`, so the same folders always print the same interval.
  - The interval runs from the 5th to the 95th of the hundred ("ninety in a hundred"). A resample whose control statistic is zero is left out, and the count is printed.
  - Each floor prints a line with its complete pairs, the seeds with the figure on one arm only, and the seeds with it on neither.
- **By arm: printed beside it, never judged.** Each arm's runs are resampled by themselves, every run that has the figure, at the arm's own count.
- `JUDGE_INTERVAL` is the switch, set to "by seed" by the ruling. `--judge-interval by-arm` reads one reading the other way.
- **The point figure stays D-081's**, the medians over every run of each arm that has the figure. When the same ratio taken on the complete pairs alone differs from it by more than a hundredth, it is printed beside it.
- **The word of a floor:**
  - `holds`: the whole judged interval is at or over the floor;
  - `UNDER`: the whole judged interval is under it;
  - `AT THE FLOOR`: the judged interval holds the floor;
  - `not judged`: the boss floor has fewer than ten runs reaching the boss on either arm, or fewer than ten complete pairs, since a judged interval rests on ten of what it is drawn from.
- If the two intervals stand on different sides of a floor, one line says so, and the judged one decides.

### The check
`--check-floors` judges the card arm's two floors against the control, at 0.95 and 0.95 unless two numbers follow it.

| Line | When | Exit code |
|---|---|---|
| `FLOORS: PASS` | every floor holds | 0 |
| `FLOORS: FAIL` | a floor is `UNDER`, or `not judged` | 1 |
| `FLOORS: AT THE FLOOR` | none of those, and at least one floor at the floor | 3 |

- The `AT THE FLOOR` line says "nothing is decided on this battery; a second battery on fresh seeds can be read together with this one".
- Exit 2 is a refusal, such as a seed in two batteries of one arm.
- `--check-windows` against the hold is unchanged: it is judged on the point figures as before, without an interval.

### Two batteries read as one
- Each arm's option takes one folder or several, comma-separated, and their runs are pooled before anything is computed.
- The same seed of one weapon in two folders of an arm is refused: `EVOLVE_READ: refused (seed N of <weapon> is in both A and B: ...)`, exit 2.
- The exit step does not run a second battery itself (D-082's ruling): one is ordered by hand, once, and read this way.

### `--intervals` on a reading against the hold
This prints one more line per window (before the boss, damage to the boss, the old window), with both intervals and the counts of runs and complete pairs. It changes no verdict line.

### `S2Exit.ps1`, only where the band step runs and reads its play arms
- `-BandSeeds Weapon.X=30,...` gives a card more seeds than `-Seeds` (15), and each seed count runs as its own battery of the three play arms.
- After the reader's lines, the step reads the three play arms with `--control` and `--check-floors`. That is printed only; the exit code stays `s2_exit.py`'s.

## The proof reading
The five Path-card pairs of the hand reading are in `E:\Dusktracer\.claude\worktrees\agent-a37e1827c3d77226d\Saved\BotRuns\pg_<path>_hold` and `pg_<path>_decline`. Each pair is a `-CarryHold` arm on `Weapon.PulseRifle` and the same arm with `-DeclineReforge`, fifteen seeds. The command for each:

```
python -I Tools\evolve_read.py --carry <BotRuns>\pg_<path>_hold --control <BotRuns>\pg_<path>_decline --check-floors
```

The reader's point figures equal the hand reading (`pg_play.md`) on every figure: before the boss, on the boss, the runs reaching it, the calibrated health's medians, the runs that died before the hand, and the kills a minute after it.

| Path card against no pick | Before the boss (runs) | Hand | On the boss (runs reaching it) | Hand |
|---|---|---|---|---|
| Forge | 1.003 (15, 15) | 1.00 | 0.948 (15, 15); health medians 23 164 / 20 746 | 0.95 (19 009 / 20 048) |
| Tide | 1.029 (11, 11) | 1.03 | not judged (11 and 9 runs; 9 complete pairs) | not judged, 9 |
| Shade | 1.026 (14, 14) | 1.03 | 1.372 (14, 14) | 1.37 |
| Radiance | 0.977 (15, 15) | 0.98 | 1.185 (15, 15) | 1.18 |
| Quicksilver | 0.999 (15, 15) | 1.00 | 1.462 (15, 15) | 1.46 |

The words, each from the tool's own lines (judged by seed, by arm beside it):

```
FLOORS: AT THE FLOOR (PulseRifle evolved against the decline arm: before the boss 1.003, 0.978 to 1.014 by seed, holds against 0.95 (15 and 15 runs), damage to the boss 0.948, 0.721 to 1.202 by seed, AT THE FLOOR against 0.95 (15 and 15 runs); nothing is decided on this battery; a second battery on fresh seeds can be read together with this one)
FLOORS: FAIL (PulseRifle evolved against the decline arm: before the boss 1.029, 0.982 to 1.076 by seed, holds against 0.95 (11 and 11 runs), damage to the boss not judged (11 and 9 runs reached it with the measure, 9 complete pairs; 10 needed on each and 10 pairs))
FLOORS: PASS (PulseRifle evolved against the decline arm: before the boss 1.026, 1.005 to 1.043 by seed, holds against 0.95 (14 and 14 runs), damage to the boss 1.372, 1.064 to 2.116 by seed, holds against 0.95 (14 and 14 runs))
FLOORS: AT THE FLOOR (PulseRifle evolved against the decline arm: before the boss 0.977, 0.961 to 1.015 by seed, holds against 0.95 (15 and 15 runs), damage to the boss 1.185, 0.881 to 1.495 by seed, AT THE FLOOR against 0.95 (15 and 15 runs); nothing is decided on this battery; a second battery on fresh seeds can be read together with this one)
FLOORS: PASS (PulseRifle evolved against the decline arm: before the boss 0.999, 0.973 to 1.043 by seed, holds against 0.95 (15 and 15 runs), damage to the boss 1.462, 0.985 to 1.795 by seed, holds against 0.95 (15 and 15 runs))
```

The lines are Forge, Tide, Shade, Radiance and Quicksilver, in that order. The disagreement line fires on Shade and on Quicksilver:

```
PulseRifle evolved against the control, on the boss: the two intervals stand on different sides of the floor (by seed holds, by arm AT THE FLOOR); the by seed one decides
```

Shade's interval by arm on the boss is 0.795 to 2.331; Quicksilver's is 0.842 to 1.812.

## The table: the landed batteries and the five Path cards, with intervals
The run files are those of D-081's re-read:
- D-072 and D-079 in `E:\Dusktracer\.claude\worktrees\agent-a2b736d35d80303fb\Saved\BotRuns\`;
- D-074 in `...\agent-a37e1827c3d77226d\Saved\BotRuns\`.

The landed pairs were read with `--check-windows 0.95 <bar> --intervals`. In the table, "Runs" is the runs on each arm and then the complete pairs; "Word" is the word the by-seed interval gives against the bar.

| Reading | Window | Bar | Point | By seed (judged) | By arm | Runs | Word |
|---|---|---|---|---|---|---|---|
| D-072 Murmuration, Tempest | before the boss | 0.95 | 1.068 | 1.051 to 1.092 | 1.048 to 1.094 | 15, 15; 15 | holds |
| | the boss (D-081) | 0.6 | 0.990 | 0.800 to 1.015 | 0.765 to 1.073 | 15, 15; 15 | holds |
| | the old window (judged then) | 0.6 | 0.907 | 0.758 to 0.973 | 0.720 to 1.035 | 15, 15; 15 | holds |
| D-074 Pulse Wall, Tempest | before the boss | 0.95 | 1.005 | 0.992 to 1.022 | 0.988 to 1.028 | 15, 15; 15 | holds |
| | the boss | 0.9 | 1.067 | 0.959 to 1.265 | 0.954 to 1.363 | 15, 15; 15 | holds |
| | the old window (judged then) | 0.9 | 1.120 | 0.977 to 1.275 | 0.968 to 1.339 | 15, 15; 15 | holds |
| D-074 Pulse Wall, Arc Grid | before the boss | 0.95 | 0.992 | 0.976 to 1.020 | 0.972 to 1.022 | 15, 15; 15 | holds |
| | the boss | 0.9 | 1.170 | 1.011 to 1.224 | 0.976 to 1.356 | 15, 15; 15 | holds |
| | the old window | 0.9 | 1.153 | 1.030 to 1.260 | 0.998 to 1.303 | 15, 15; 15 | holds |
| D-079 Blade Halo, Tempest | before the boss | 0.95 | 0.992 | 0.968 to 1.023 | 0.966 to 1.032 | 15, 15; 15 | holds |
| | the boss | 0.9 | 1.041 | 0.939 to 1.333 | 0.825 to 1.474 | 13, 15; 13 | holds |
| | the old window (judged then) | 0.9 | 1.028 | 0.899 to 1.311 | 0.863 to 1.478 | 13, 15; 13 | AT THE FLOOR |
| D-079 Blade Halo, Arc Grid | before the boss | 0.95 | 1.004 | 0.984 to 1.018 | 0.984 to 1.020 | 15, 15; 15 | holds |
| | the boss | 0.9 | 1.351 | 1.133 to 1.702 | 1.035 to 1.841 | 15, 15; 15 | holds |
| | the old window | 0.9 | 1.350 | 1.172 to 1.663 | 1.074 to 1.805 | 15, 15; 15 | holds |
| Forge against no pick | before the boss | 0.95 | 1.003 | 0.978 to 1.014 | 0.971 to 1.017 | 15, 15; 15 | holds |
| | the boss | 0.95 | 0.948 | 0.721 to 1.202 | 0.721 to 1.207 | 15, 15; 15 | AT THE FLOOR |
| Tide against no pick | before the boss | 0.95 | 1.029 | 0.982 to 1.076 | 0.956 to 1.089 | 11, 11; 11 | holds |
| | the boss | 0.95 | not judged | - | - | 11, 9; 9 | not judged |
| Shade against no pick | before the boss | 0.95 | 1.026 | 1.005 to 1.043 | 0.992 to 1.048 | 14, 14; 14 | holds |
| | the boss | 0.95 | 1.372 | 1.064 to 2.116 | 0.795 to 2.331 | 14, 14; 14 | holds |
| Radiance against no pick | before the boss | 0.95 | 0.977 | 0.961 to 1.015 | 0.961 to 1.016 | 15, 15; 15 | holds |
| | the boss | 0.95 | 1.185 | 0.881 to 1.495 | 0.864 to 1.542 | 15, 15; 15 | AT THE FLOOR |
| Quicksilver against no pick | before the boss | 0.95 | 0.999 | 0.973 to 1.043 | 0.961 to 1.059 | 15, 15; 15 | holds |
| | the boss | 0.95 | 1.462 | 0.985 to 1.795 | 0.842 to 1.812 | 15, 15; 15 | holds |

The landed verdicts stand on the point figures they were judged on; the intervals are printed beside them.

- **D-079's Tempest pair is the only landed pair that drops seeds.** On the boss, 13 of the rule's runs and 15 of the hold's reach it, so the two hold runs whose seeds the rule did not carry to the boss leave their pairs.
  - That moved its by-seed interval from 0.881 to 1.283 (every reaching run) to 0.939 to 1.333 (complete pairs) on the boss measure, which now holds.
  - The old window it was judged on reads 0.899 to 1.311, at the floor by one thousandth.
- **Before the boss** every interval is narrow, about 0.03 either side.
- **On the boss** the judged intervals run from about 0.1 either side (D-074 Arc Grid) to about 0.5 (Shade).

## How many seeds a pair needs for ten runs reaching the boss
The ten arms of the five Path-card pairs reached the boss with the measure on these counts of fifteen seeds:

| Path | Hold | Decline |
|---|---|---|
| Forge | 15 | 15 |
| Tide | 11 | 9 |
| Shade | 14 | 14 |
| Radiance | 15 | 15 |
| Quicksilver | 15 | 15 |

For each arm, the smallest seed count with at least ten reaching, nineteen times in twenty, at the arm's own rate (binomial):
- 10 seeds for the arms at 15 of 15;
- 12 for Shade's (0.93);
- 18 for Tide's hold (0.73);
- 23 for Tide's decline arm (0.60), the lowest rate of the ten.

At that lowest rate:
- 15 seeds give ten reaching with probability 0.40;
- 20 give 0.87;
- 25 give 0.99;
- 30, two batteries of fifteen, give 0.999.

**So thirty covers every arm read here, and 23 is the least that does at the lowest rate.** The pair also needs ten complete pairs. Tide's pair has 9 of 15, the same rate of 0.60, so it needs the same 23 seeds. Fifteen seeds are enough for every card here except Tide's Path card, whose decline arm dies before the boss in six of fifteen runs.

The band step's own pair, `Steered:Storm:Tempest:Tide`, reached the boss in 13 to 15 of 15 on every arm read for D-072, D-074 and D-079. No card of it needs more than fifteen by these counts, so `-BandSeeds` is empty by default. A card is given more there when its own battery shows a reach like Tide's.

## Tests
These are in `Tools/tools_test.py`, class `EvolveSplit`, on made-up runs:
- A decline run is split at its hand's offer; a decline run with no such offer is left out and counted; the old reading still leaves it out, and the old grouping is unchanged.
- The interval is the same on two calls and moves with the seed constant, both ways of resampling.
- Made-up pairs wholly over, wholly under and across the floor give `holds`, `UNDER` and `AT THE FLOOR`, with exit codes 0, 1 and 3. The judging interval is tested both by seed and by arm, and the default is by seed.
- Fewer than ten runs reaching the boss on the control is `not judged` and fails. So is ten runs on each arm with eight complete pairs, by seed; by arm the same case holds.
- Paired seeds whose control spreads wide: by seed holds, by arm is at the floor. The line saying so fires, and the unjudged interval is printed and decides nothing.
- The ratio on the complete pairs is printed where it differs by more than a hundredth, and not where it does not.
- On the command line:
  - the hold's arm is read against the control beside the card's;
  - the band's boss-side line appears;
  - the check's three lines appear;
  - `--judge-interval by-arm` works;
  - no second battery is asked for;
  - `--check-windows` is unchanged, and `--intervals` prints three lines and changes no verdict.
- Two folders of an arm are pooled, and a seed in both is refused with exit 2.
- `S2Exit.ps1`: no second battery of its own, the per-card seed count, and the play arms' read.

`TOOLS: PASS (248 tests)`.

## Not verified
- The band step's new read and `-BandSeeds` have not been run through `S2Exit.ps1`: no engine run was made.
- The decline run's hand is placed at its `offer` record. The hand script did the same, and every figure agrees.
- A second battery of Forge's or Tide's pair has not been read; none existed when this note was written.
- A bot reading is never a human reading.
