# S1: bot skill rows for the bracket of the S1 gate decision (2026-11-12)

**Reading.** Five skill rows, each the same nineteen arms on the same fifteen seeds. HP lost by 600 s rises as skill falls: medians 0.283, 0.400 and 0.824 of a bar at 1.0, 0.75 and 0.5, then at least 1.2 at 0.25 and 0.0, where most runs are dead before 600 s. Dashes per minute over the first ten minutes alive do not fall with skill: 3.50 at 1.0, then 6.40 to 6.80 at every lower row. The owner's 1.81 is under every row. Nothing was tuned, and no row is named as the one. A bot reading is a bound.

## Build and runs
- **Build:** branch `s1/m2-readings` from `main` 842259e. One `Tools\Build.bat` (DusktracerEditor Development), `Result: Succeeded`, and no rebuild after it. Every run file's `run_start.build` reads `842259e`, clean, with `bot_fast` 600.
  - `git diff --stat e4926d4 842259e -- Source Content Config Dusktracer.uproject` prints `Source/Dusktracer/Dusktracer.cpp | 17 +++++++++++++++++`. That is the crash-exit change (D-065), compiled out of editors.
- **Settings:** the nineteen arms of `S1_d045_sources_3.md`, fifteen seeds from first seed 1, policy All, 600 steps a frame, `-MaxSeconds` 1100, **`-Parallel 6`**. Only `-Skill` changes between rows. The skill 1.0 row is that note's battery.
- **The restart.** Windows restarted the PC at 04:44 during the first skill 0.25 battery, at 210 of 285 runs. That folder was moved aside unread (`Saved\BotRuns\skill_025_cut`), and the row was run again whole. The owner's play session paused heavy jobs from 20:12 to 20:21 on 2026-11-12; no battery of this note was running then, and none was run again because of it.

```
& Tools\BotRuns.ps1 -Arms <the nineteen arms, as in S1_d045_sources_3.md> -Seeds 15 -Skill 0.75 -Parallel 6 -Out Saved\BotRuns\skill_075
  (the same with -Skill 0.5 -Out Saved\BotRuns\skill_050, -Skill 0.25 -Out Saved\BotRuns\skill_025, -Skill 0.0 -Out Saved\BotRuns\skill_000)
python -I Tools\session_read.py <an empty folder> --bots Saved\BotRuns\d045_sources_3 Saved\BotRuns\skill_075 Saved\BotRuns\skill_050 Saved\BotRuns\skill_025 Saved\BotRuns\skill_000
python -I Tools\owner_read.py bots Saved\BotRuns\d045_sources_3 Saved\BotRuns\skill_075 Saved\BotRuns\skill_050 Saved\BotRuns\skill_025 Saved\BotRuns\skill_000 --t 600
```

| Skill | Folder | Asked | Ran | Failed | Rerun | Outcomes | Wall |
|---|---|---|---|---|---|---|---|
| 1.0 | `d045_sources_3` | 285 | 285 | 0 | 0 | 192 victories, 91 deaths, 2 at the cap | 01:40 to 01:58 |
| 0.75 | `skill_075` | 285 | 285 | 0 | 0 | 166 victories, 117 deaths, 2 at the cap | 03:21 to 03:32 |
| 0.5 | `skill_050` | 285 | 285 | 0 | 0 | 126 victories, 159 deaths | 03:33 to 03:42 |
| 0.25 | `skill_025` | 285 | 285 | 0 | 0 | 25 victories, 260 deaths | 19:04 to 19:13 |
| 0.0 | `skill_000` | 285 | 285 | 0 | 0 | 1 victory, 284 deaths | 19:53 to 20:01 |

- Every battery printed `BOTRUNS: PASS`.
- No run log holds a `LogDT` error, a fatal error, an unhandled exception or an assertion.
- The wall times include waits for the session lock's turn and a machine slot.

## What `-Skill` changes on `main`
Bot v1 only; there is no bot v2 and no `bot.md` on `main`.
- `-DTBotSkill=` sets `dt.Bot.Skill` (`Source/Dusktracer/Bot/DTBotSubsystem.cpp:38`, `:105`), clamped to 0 to 1 (`DTBotBrain.cpp:186`).
- **Reaction delay:** `(1 - skill) × 0.25 s` (`MaxReactionS`, `DTBotBrain.cpp:45`, `:198`). It sets the decision cadence and the delay before a dash from an imminent hit.
- **Movement noise:** a random angle of up to `(1 - skill) × 35°` each decision (`:46`, `:516`).
- **Aim noise:** manual aim at the nearest enemy with up to `(1 - skill) × 20°` (`:47`, `:538`). The subsystem puts player 0 in manual aim below 1.0 (`DTBotSubsystem.cpp:404`).

Nothing else changes with skill: not cards, attention, cues or the dash rate. It is a weaker bot made by noise and a manual-aim handicap, not a model of a player (`S1_owner_run.md` §7).

## The rows
**Where the numbers come from.**
- **HP lost by 600 s** and **dashes per minute over the first ten minutes alive** are `session_read.py`'s `BRACKET:` lines. That is the code that prints the validators' `HP600:` figures, so the rows and the validators read the same way.
  - HP lost by 600 s is the sum of hit shares with t at most 600 s, restores never subtracted. A killing hit counts whole, and restores let the sum pass 1.0.
  - The dash figure counts dashes up to the earlier of 600 s and the run's end, over that many minutes.
  - "At least" means runs that died before 600 s sit at or under the median: their value is a lower bound.
- **The two other dash figures the tools print** come from a scratch script (`m2_ii_rows.py`, in the session scratchpad, not in the repo):
  - **whole run:** `owner_read.py`'s definition, dashes over the run's end in minutes;
  - **window:** dashes per minute inside 600 to 780 s for runs alive through it, by `pressure.window_rates` and `pressure.alive_through`, the figure of `pressure.py`'s `PRESSURE:` line.
- **At the boss and wins** come from `bot.csv.txt` (at the boss means a victory or `end_s` at or past 900 s). They are printed and judged nowhere.

| Skill | Runs | Alive at 600 s | HP lost by 600 s, pooled median (arm medians) | Dashes/min, first ten minutes alive, pooled (arm medians) | Dashes/min, whole run, pooled (arm medians) | Dashes/min, 600 to 780 s: runs, pooled (arm medians) | At the boss | Wins |
|---|---|---|---|---|---|---|---|---|
| 1.0 | 285 | 257 | 0.283 (0.065 to 1.029) | 3.50 (2.30 to 4.80) | 4.45 (3.61 to 5.56) | 240, 5.33 (4.17 to 7.33) | 232 | 192 |
| 0.75 | 285 | 232 | 0.400 (0.135 to 1.213) | 6.61 (5.50 to 7.71) | 7.01 (5.74 to 8.36) | 213, 7.00 (5.00 to 9.67) | 202 | 166 |
| 0.5 | 285 | 203 | 0.824 (0.277 to 1.263) | 6.40 (5.00 to 7.60) | 6.70 (5.29 to 7.77) | 185, 6.67 (4.33 to 8.83) | 174 | 126 |
| 0.25 | 285 | 129 | at least 1.202 (0.738 to 1.324) | 6.80 (5.17 to 8.01) | 6.86 (5.49 to 8.06) | 90, 6.50 (2.83 to 9.83) | 75 | 25 |
| 0.0 | 285 | 27 | at least 1.198 (1.060 to 1.335) | 6.41 (5.10 to 7.40) | 6.43 (5.17 to 7.40) | 11, 6.00 (5.00 to 6.17) | 9 | 1 |
| **The owner, n = 1** (`S1_owner_run.md`) | 1 | no (died at 563.3 s) | 1.042 by 563.3 s, its end | | 1.81 | not alive in it | no | no |

- **Per arm.** `owner_read.py bots --t 600` prints one row per arm and folder: HP lost by 600.0 s (median and max), dashes by 600 s, dashes per minute over the whole run, boss, wins and the rest. It is kept in `Saved\Session\m2_ii_owner_bots.md`. Its per-arm HP medians are the arm medians above, by the same `hp_lost` rule.
- **The owner's row** is the only person on record. That run ended at 563.3 s, so its HP figure is its whole loss, a lower bound for 600 s; it is printed beside the table and not placed in it.

## Where a validator median would be bracketed
**HP lost by 600 s.** The row medians rise from 0.283 (1.0) through 0.400 (0.75) and 0.824 (0.5) to at least 1.2 at 0.25 and 0.0. Those two are lower bounds of about the same size, because most of their runs are dead before 600 s.
- Between 0.283 and 0.400: bracketed by 1.0 and 0.75.
- Between 0.400 and 0.824: by 0.75 and 0.5.
- Between 0.824 and about 1.2: by 0.5 and 0.25.
- Under 0.283: not bracketed, every row is over.
- Above about 1.2: not bracketed in any useful sense, because both rows past it are lower bounds.

**Dashes per minute over the first ten minutes alive.** This measure is not ordered by skill. Skill 1.0 reads 3.50, and every lower row reads between 6.40 and 6.80: 0.5 at 6.40, 0.0 at 6.41, 0.75 at 6.61, 0.25 at 6.80. Noise makes the bot dash more, not less.
- Under 3.50: not bracketed, every row is over. The owner's 1.81 is here.
- Between 3.50 and 6.40: bracketed by 1.0 below and 0.5 above.
- Between 6.40 and 6.80: the rows just under and over change with every 0.1, and the order of rows has no meaning there.

**Can the two measures bracket at the same row?** Only at skill 1.0, as the row just under. A validator median that HP brackets with 1.0 and 0.75 gets 1.0 and 0.5 from dashes, so the rows just over differ. Anywhere else the two point to different rows, or dashes does not bracket at all. The earlier sweep on two Hammer arms saw the same thing (`S1_owner_run.md` §7). This reading names no row; the S1 gate decision does that with session A's values.

## Where the files are
Untracked, in this worktree:
- the runs: `Saved\BotRuns\d045_sources_3`, `skill_075`, `skill_050`, `skill_025`, `skill_000`, and `skill_025_cut` (the cut battery, not read);
- the logs: `Saved\Session\m2_skill_*.log` and `.stamp`, `m2_ii_bracket.txt` (`session_read.py`'s output), `m2_ii_owner_bots.md`, and the scratch tables `m2_ii_rows*.txt`.
