# S1: the probe by loss at the session build (2026-11-12)

**Reading: `T-KEE-18: FAIL`.**
- Loss is at or above `C.POWER.KEEPER_SHARE_L16` (0.6) on eight of nine arms. Storm/Tempest reads 0.555, below it, and is the one arm outside `C.POWER.ARCHETYPE_BAND` (±0.15 of the mean loss 0.799).
- Package gain is outside 2.0 to 2.4 on all nine.
- **Against the reread at 221d4f0** (`S1_d047_reread.md`): the three Storm arms and Forge/Slam read the same to the last digit. Forge/Hammer, Tide/Frost and Tide/Stillness moved, and Forge/Furnace and Tide/Whirlpool barely; the verdict line is the same but for the mean (0.804 then, 0.799 now).

This is pass rule 7's tool row on the session build hash. Nothing was tuned. A bot reading on a probe field is a bound.

## Build and runs
- **Build:** branch `s1/m2-readings` from `main` 842259e. One `Tools\Build.bat` (DusktracerEditor Development), `Result: Succeeded`. Every run file's `run_start.build` reads `842259e`, clean.
  - `git diff --stat e4926d4 842259e -- Source Content Config Dusktracer.uproject` prints `Source/Dusktracer/Dusktracer.cpp | 17 +++++++++++++++++`, the crash-exit change (D-065), compiled out of editors. So the probe reads the session A package's game code.
- **Method:** exactly `S1_d047_reread.md`'s.
  - Bot v1 at skill 1.0, policy All, 600 steps a frame, first seed 1.
  - `probe.py ablation` at one seed per scene.
  - Field: 120 swarm enemies at 40× HP, minute 10.5, warm-up 8 s, window 10 s.
  - The one difference: **`-Parallel 6`** instead of 14, because other sessions share this PC.
- **Interruptions:**
  - Windows restarted the PC at 04:44 on 2026-11-12, before any run of this reading.
  - The owner's play session paused heavy jobs from 20:12 to 20:21. That fell after the build runs and the ablation, and before the scene runs; no step of this reading was run again because of it.
- **Wall times:** build runs 20:03 to 20:08; scene runs 20:34 to 21:06.

```
& Tools\BotRuns.ps1 -Arms Steered:Forge:Furnace,Steered:Forge:Hammer,Steered:Forge:Slam,Steered:Storm:ArcGrid,Steered:Storm:Downpour,Steered:Storm:Tempest,Steered:Tide:Frost,Steered:Tide:Stillness,Steered:Tide:Whirlpool -Seeds 15 -Parallel 6 -Out Saved\BotRuns\probe_builds
python -I Tools\probe.py ablation Saved\Probes\session_build --builds Saved\BotRuns\probe_builds
& Tools\BotRuns.ps1 -Jobs Saved\Probes\session_build\jobs.csv -Parallel 6 -Out Saved\Probes\session_build\runs
python -I Tools\probe.py read Saved\Probes\session_build --check
```

| Batch | Asked for | Ran | Failed | Rerun | Ended |
|---|---|---|---|---|---|
| Builds (`Saved\BotRuns\probe_builds`) | 135 (9 arms × 15 seeds) | 135 | 0 | 0 | 106 victories, 29 deaths (the reread: 101 and 34); `BOTRUNS: PASS` |
| Scenes (`Saved\Probes\session_build\runs`) | 1000 (125 builds × 8 configurations) | 1000 | 0 | 0 | 1000 at the scene's end (18.0 s, `bot_timeout`); `BOTRUNS: PASS (0 run(s) without a bot record; ...)` |

- **Builds found: 125 of 135.** Ten runs died before level sixteen and wrote no snapshot:
  - Forge/Slam seed 12;
  - Tide/Frost seeds 10, 12, 14;
  - Tide/Stillness seeds 6, 7, 10, 12;
  - Tide/Whirlpool seeds 8, 12.
  The reread's set was the same except Stillness seed 2 where this one has seed 6. Stillness seed 6 now dies at 793.2 s, at level 10 at 630 s.
- **One modifier refused**, as at the reread: `Bot: probe modifier Mod.Pierce on Weapon.PulseRifle refused (6)` in `AB-Tide-Whirlpool-b11-no_levels_s11501`. No other `LogDT` error, no fatal error, no unhandled exception and no assertion in any log.

## The reading
`probe.py read --check` exits 1 and prints:

```
T-KEE-18: FAIL (9 arms: loss below 0.6 on 1, gain outside the band on 9, outside the archetype band on 1; mean loss 0.799; share, printed and never judged, mean 0.563)
```

At 221d4f0 it printed `T-KEE-18: FAIL (9 arms: loss below 0.6 on 1, gain outside the band on 9, outside the archetype band on 1; mean loss 0.804; share, printed and never judged, mean 0.555)`.

In the table, the control is the package gain again with the second Keeper's cards taken off both sides. It and the share are printed and never judged. Each value is shown now, with the reread's at 221d4f0 after the arrow.

| Arm | Builds | Full DPS | Loss | Package gain | Control | Share | Loss verdict | Band | Moved |
|---|---|---|---|---|---|---|---|---|---|
| Forge/Furnace | 15 (15) | 11145 ← 11413 | 0.889 ← 0.891 | 1.98 ← 1.95 | 3.23 ← 3.23 | 0.660 ← 0.652 | PASS | IN | slightly |
| Forge/Hammer | 15 (15) | 11868 ← 13733 | 0.927 ← 0.937 | 3.44 ← 2.89 | 4.42 ← 4.48 | 0.829 ← 0.781 | PASS | IN | **yes** |
| Forge/Slam | 14 (14) | 8182 = | 0.827 = | 1.18 = | 1.50 = | 0.521 = | PASS | IN | no |
| Storm/ArcGrid | 15 (15) | 4369 = | 0.783 = | 2.85 = | 3.46 = | 0.433 = | PASS | IN | no |
| Storm/Downpour | 15 (15) | 6065 = | 0.850 = | 5.06 = | 4.76 = | 0.439 = | PASS | IN | no |
| Storm/Tempest | 15 (15) | 2607 = | 0.555 = | 1.38 = | 1.48 = | 0.316 = | **FAIL** | **OUT** | no |
| Tide/Frost | 12 (12) | 6537 ← 7379 | 0.741 ← 0.766 | 0.90 ← 0.94 | 0.80 ← 0.79 | 0.504 ← 0.491 | PASS | IN | **yes** |
| Tide/Stillness | 11 (11) | 7546 ← 7874 | 0.760 ← 0.767 | 1.19 ← 1.15 | 1.46 ← 1.54 | 0.668 ← 0.653 | PASS | IN | **yes** |
| Tide/Whirlpool | 13 (13) | 7886 ← 7889 | 0.859 = | 1.28 ← 1.29 | 2.32 = | 0.696 ← 0.709 | PASS | IN | slightly |

- **Package gain inside 2.0 to 2.4:** 0 of 9, as at the reread. Whirlpool's control is 2.32, inside it, as before.
- **The arms that moved.**
  - **Forge/Hammer.** The expected one: Hard Blow now knocks back the enemies a cleave hits (D-053 item 3). It reads 14 % less full DPS, the same loss within 0.01, and a higher package gain (3.44) and share (0.829).
  - **Tide/Frost, Tide/Stillness, Forge/Furnace and Tide/Whirlpool.** Their bot runs changed since the reread, so other level-sixteen builds were rebuilt. Runs with the same `input_hash` as the reread's `d047_builds`: Furnace 8 of 15, Frost 5, Stillness 5, Whirlpool 4, against Hammer 0 and 15 of 15 on Slam and the three Storm arms. The Tide arms' second Keeper is Forge, the bot's default.
  - **The Storm arms and Forge/Slam** read the same to the last digit: their builds are the same.
  - From these numbers alone I cannot tell, for Frost and Stillness, how much is the build change and how much the code.

## The machine
`run_end.wall_dropped_steps` and `run_end.wall_duration_s`, summed over the runs, as the reread reported them. The sums come from a scratch script in the session scratchpad.

| Batch | Runs | Runs with any dropped step | Dropped steps, total | Most in one run | Wall s, total | Sim s, total | Wall / sim |
|---|---|---|---|---|---|---|---|
| Builds | 135 | 134 | 7147 (reread 2082) | 319 (Tide/Stillness seed 6) | 542.7 | 125382.5 | 0.0043 (reread 0.0041) |
| Scenes | 1000 | 0 | 0 | 0 | 435.2 | 18000.0 | 0.0242 (reread 0.0625) |

A bot run at 600 steps a frame runs nearly all its steps past the clock, so a dropped step here is the fast-forward's and not a person's wait. No rule sets a run aside for them, and none was applied. More dropped steps per build run fits a PC that other sessions share.

## Where the files are
Untracked, in this worktree:
- the build runs: `Saved\BotRuns\probe_builds`;
- the reading: `Saved\Probes\session_build\probe.md`;
- the scene runs: `Saved\Probes\session_build\runs`;
- the logs: `Saved\Session\m2_probe_builds.log`, `m2_probe_ablation.log`, `m2_probe_scenes.log`, `m2_probe_read.log`.

## Not verified
- Any second Keeper other than each patron's default, as at the reread.
- The Shipping build: every run here is the Development editor build.
- People: no person's build is in this reading.
