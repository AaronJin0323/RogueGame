# S1: D-045's Rusher hits with the landed drop-ins they are out of (third battery, 2026-11-12)

**Reading:** over the 270 steered runs at skill 1.0, 48 hits came from an arrival's own attack (35 live charges and 13 bands of drop-ins killed in their wind-up). The bot saw 13 552 landed Rusher drop-ins, so the ratio is 0.0035, or about one hit per 280 landed drop-ins. 40 of the 270 runs took at least one such hit. The five-way split equals the second battery's hit for hit, and every run's outcome equals it run for run. Nothing was tuned; a bot reading is a bound.

## Build and runs
- **Build:** branch `s1/m2-readings` from `main` 842259e. `Tools\Build.bat` (DusktracerEditor Development) gave `Result: Succeeded`; it compiled 7 actions. Every run file's `run_start.build` reads `842259e`, clean. `bot_fast` is 600.
  - `git diff --stat e4926d4 842259e -- Source Content Config Dusktracer.uproject` prints `Source/Dusktracer/Dusktracer.cpp | 17 +++++++++++++++++`, `1 file changed, 17 insertions(+)`. That is the crash-exit change (D-065, `package.md` R-PKG-16), which is compiled out of editors, so it cannot move a bot run.
- **Settings:** the nineteen arms of `S1_d045_sources_2.md`, fifteen seeds from first seed 1, skill 1.0, policy All, 600 steps a frame, `-MaxSeconds` 1100.
  - **`-Parallel 6`**, not the 14 of the earlier batteries: other sessions share this PC now.
- **Interruptions:** Windows restarted the PC at 04:44 on 2026-11-12 during the first skill 0.25 battery of `S1_skill_rows.md`, and the owner's play session paused heavy jobs from 20:12 to 20:21 on 2026-11-12. That second pause cut the 9107 control run `arms9107_b`, which was run again. Neither touched this battery, which had ended at 01:58.
- **Wall time:** 01:40:06 to 01:58:12, about 18 minutes. About 7 minutes of that went on waiting for a machine slot: all four were held by other worktrees' gates and batteries.

```
Tools\Build.bat
& Tools\BotRuns.ps1 -Arms None,Steered:Forge:Furnace,Steered:Forge:Furnace:Tide,Steered:Forge:Hammer,Steered:Forge:Hammer:Tide,Steered:Forge:Slam,Steered:Forge:Slam:Tide,Steered:Storm:ArcGrid,Steered:Storm:ArcGrid:Forge,Steered:Storm:Downpour,Steered:Storm:Downpour:Forge,Steered:Storm:Tempest,Steered:Storm:Tempest:Forge,Steered:Tide:Frost,Steered:Tide:Frost:Storm,Steered:Tide:Stillness,Steered:Tide:Stillness:Storm,Steered:Tide:Whirlpool,Steered:Tide:Whirlpool:Storm -Seeds 15 -Parallel 6 -Out Saved\BotRuns\d045_sources_3
python -I Tools\pressure.py Saved\BotRuns\d045_sources_3 --check
```

| Asked | Ran | Failed | Rerun | Outcomes |
|---|---|---|---|---|
| 285 | 285 | 0 | 0 | 192 victories, 91 deaths, 2 at the 1100 s cap; `BOTRUNS: PASS (no check requested; ...)` |

- Every run wrote its bot record and a `run_end`.
- No run log holds a `LogDT` error or warning, a fatal error, an unhandled exception or an assertion. Every one ends `LogExit: Exiting`.

## The same sim as the second battery
`bot.csv.txt` compared run for run with `E:\Dusktracer\.claude\worktrees\agent-a559188263b0e1f6c\Saved\BotRuns\d045_sources_2` (build 7f75ada, read only):
- Outcome, `end_s`, level at 630 s, dash and Intercession presses, picks, declines, `input_hash` and config are equal in **285 of 285** runs.
- `steps` reads 2 lower in 274 runs, 1 lower in 8 and equal in 3. This is the fast bot's close, which now holds the sim clock at the deciding step (T-TEL-17).

So the code between 7f75ada and 842259e moved no steered run.

## Rusher hits, five ways, out of the landed drop-ins (skill 1.0)
**Where the numbers come from.** `pressure.py` prints one `SOURCES:` line per arm and one overall:

`SOURCES: overall rusher hits, whole run: arrival charging 36 not charging 29; other charging 89 not charging 62; killed drop-in band 13; out of 14006 landed Rusher drop-ins over 285 run(s)`

**The table** comes from a scratch script, `m2_i_table.py` in the session scratchpad (not in the repo). For each run it calls `pressure.rusher_hits_from_jsonl` and `pressure.landed_dropins_from_jsonl`, sums them per arm, and counts the runs with at least one hit. Its per-arm sums equal `pressure.py`'s per-arm lines on every arm. Boss reach and wins come from `summary.csv`. The columns:
- **Arr. charge:** `arrival` and `charging` true.
- **Arr. other:** an arrival not charging.
- **Walked charge / Walked other:** `arrival` false.
- **Band:** `hit_kind` Charge, the band of a drop-in killed in its wind-up.
- **Own attack:** Arr. charge plus Band.

| Arm | Arr. charge | Arr. other | Walked charge | Walked other | Band | Landed Rusher drop-ins | Own attack / landed | Runs with an own-attack hit | Runs with an arrival charge hit | At the boss | Wins |
|---|---|---|---|---|---|---|---|---|---|---|---|
| None | 1 | 5 | 2 | 5 | 0 | 454 | 1 / 454 = 0.0022 | 1 | 1 | 1 | 0 |
| Forge/Furnace | 2 | 0 | 7 | 0 | 0 | 778 | 2 / 778 = 0.0026 | 2 | 2 | 14 | 13 |
| Forge/Furnace:Tide | 0 | 2 | 5 | 3 | 2 | 733 | 2 / 733 = 0.0027 | 2 | 0 | 11 | 7 |
| Forge/Hammer | 1 | 0 | 2 | 1 | 0 | 785 | 1 / 785 = 0.0013 | 1 | 1 | 15 | 15 |
| Forge/Hammer:Tide | 3 | 0 | 6 | 0 | 2 | 860 | 5 / 860 = 0.0058 | 3 | 2 | 14 | 9 |
| Forge/Slam | 3 | 3 | 4 | 2 | 2 | 795 | 5 / 795 = 0.0063 | 4 | 2 | 14 | 14 |
| Forge/Slam:Tide | 5 | 1 | 4 | 6 | 0 | 810 | 5 / 810 = 0.0062 | 3 | 3 | 13 | 7 |
| Storm/ArcGrid | 1 | 2 | 4 | 1 | 0 | 793 | 1 / 793 = 0.0013 | 1 | 1 | 14 | 13 |
| Storm/ArcGrid:Forge | 0 | 0 | 1 | 0 | 3 | 817 | 3 / 817 = 0.0037 | 3 | 0 | 15 | 14 |
| Storm/Downpour | 0 | 0 | 3 | 0 | 1 | 786 | 1 / 786 = 0.0013 | 1 | 0 | 15 | 14 |
| Storm/Downpour:Forge | 0 | 0 | 2 | 0 | 0 | 758 | 0 / 758 = 0.0000 | 0 | 0 | 15 | 15 |
| Storm/Tempest | 4 | 1 | 6 | 9 | 0 | 798 | 4 / 798 = 0.0050 | 3 | 3 | 13 | 12 |
| Storm/Tempest:Forge | 0 | 0 | 1 | 3 | 0 | 807 | 0 / 807 = 0.0000 | 0 | 0 | 15 | 13 |
| Tide/Frost | 2 | 1 | 2 | 8 | 1 | 715 | 3 / 715 = 0.0042 | 3 | 2 | 12 | 8 |
| Tide/Frost:Storm | 3 | 5 | 3 | 3 | 1 | 702 | 4 / 702 = 0.0057 | 3 | 2 | 11 | 8 |
| Tide/Stillness | 4 | 3 | 5 | 3 | 0 | 674 | 4 / 674 = 0.0059 | 3 | 3 | 9 | 7 |
| Tide/Stillness:Storm | 2 | 3 | 6 | 6 | 1 | 534 | 3 / 534 = 0.0056 | 3 | 2 | 8 | 6 |
| Tide/Whirlpool | 2 | 1 | 14 | 5 | 0 | 737 | 2 / 737 = 0.0027 | 2 | 2 | 12 | 10 |
| Tide/Whirlpool:Storm | 3 | 2 | 12 | 7 | 0 | 670 | 3 / 670 = 0.0045 | 3 | 3 | 11 | 7 |
| **Steered, 270 runs** | **35** | **24** | **87** | **57** | **13** | **13 552** | **48 / 13 552 = 0.0035** | **40** | **28** | **231** | **192** |

- **Against the second battery** (`S1_d045_sources_2.md`: 35 live charges in 28 of 270 runs, 13 bands, 24 walk-ins, 87 and 57 on foot): the split is equal on every count. The runs are the same sim, so it could not be otherwise. The landed count is what is new.
- **Boss reach and wins per arm** are printed here and judged nowhere. The last per-arm table on `main` (`S1_keeper_power.md` §5) is older than the chest restore, so this table replaces it as a reading of 842259e and judges nothing.

## What `--check` prints
- **T-DIR-02:** `NOT SET (the bands are drafts until the S1 gate decision sets them, D-046; 18/19 group(s) read; not read: None)`. Each steered arm prints its medians, for example Forge/Hammer `damage taken 0.000/min, dashes 5.00/min` and Tide/Stillness `0.028/min, 7.33/min`. None is not read: 1 run alive through 600 to 780 s.
- **T-DIR-18:** `FAIL (0/19 group(s) pass; ...; not read: None)`, swarm kill time 0.600 to 1.683 s on the eighteen steered arms against `C.TTK.SWARM_STEERED_MAX` 0.3.

Both are printed, and judged nowhere in this note.

## For D-045's revert clause
The clause reverts D-045 if two validators at session A call an arrival a hit they had no time to answer. This reading cannot answer that.
- **What it says.** A bot at skill 1.0 that dodges by its own rule is hit by an arrival's own attack about once per 280 landed Rusher drop-ins. 230 of 270 steered runs never are.
- **What it does not say.** It says nothing about whether a person sees the red band in time, and the clause is about a person.
- **Why the bot is a poor stand-in here.** The geometry the decision changed is the charge through the spot where the hero stood at the wind-up. That hits a hero who stands still, and a skill-1.0 bot rarely stands still.
- **What session A reads.** `Tools/session_read.py` D-045 prints the same ratio for the validators' runs, beside questions 8 and 14 of the kit. This battery is the bound that ratio is set against.

## Where the files are
Untracked, in this worktree:
- the runs: `Saved\BotRuns\d045_sources_3`, with `summary.csv` and `summary.md`;
- the logs: `Saved\Session\m2_build.log`, `m2_i.log`, `m2_i_pressure.log`, `m2_i_table.txt`.
