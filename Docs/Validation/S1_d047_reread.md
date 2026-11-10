# S1 — D-047 reread: the nine arms at level sixteen, with the no-second-Keeper control (2026-11-11)

**Reading: `T-KEE-18: FAIL`. Loss is at or above `C.POWER.KEEPER_SHARE_L16` (0.6) on eight of nine arms; Storm/Tempest reads 0.555, below it, and is the one arm outside `C.POWER.ARCHETYPE_BAND` (±0.15 of the mean loss 0.804). Package gain is outside 2.0–2.4 on all nine. With the second Keeper's cards removed from both sides, Furnace's gain reads 3.23 and Whirlpool's 2.32, and Frost's 0.79. No rule, number or tool changed in this job. A bot reading on a probe field is a bound, not a human reading (O-008).**

## Build and runs
- Branch `s1/d047-reread` from `e88f395` (`main`). Its code is `221d4f0`'s, which holds D-045 and D-046: `git diff --stat 221d4f0 e88f395` lists `Docs/STATE.md` only.
- Editor: `Tools\Build.bat`, DusktracerEditor Development, `Result: Succeeded`, 0 warnings. The runs' `run_start.build` reads `e88f395+dirty`: the build log sat untracked in the worktree root while the stamp was taken (`git status --porcelain` counts untracked files). No tracked file differed from `e88f395`.
- Date 2026-11-11, 04:30 to 05:01 local. Bot v1 at skill 1.0, policy All, 600 steps a frame, first seed 1. These are the `Tools/BotRuns.ps1` defaults; `S1_probes.md` names no other values, so the defaults are used. `probe.py ablation` at its default of one seed per scene, as the old reading's 690 runs (115 builds × 6) were. Field: 120 swarm enemies at 40× HP, minute 10.5, warm-up 8 s, window 10 s (the constants at the top of `Tools/probe.py`).
- No other engine or compiler process was alive at any moment of the runs: a process list before the runs, then a list every 20 s from 04:31:16 to 05:05:41. The list flagged two `UnrealEditor-Cmd.exe` processes (PIDs 44108 and 39072, at 04:41:21 and 04:45:43) with no readable command line; both PIDs are named in the logs of this job's own runs at the same seconds (`AB-Tide-Frost-b13-no_keepers_s9201`, `AB-Storm-Tempest-b8-no_mods_s1401`), so both were launches of this job.

Command lines, run from the worktree root in PowerShell:

```
Tools\Build.bat
& Tools\BotRuns.ps1 -Arms Steered:Forge:Furnace,Steered:Forge:Hammer,Steered:Forge:Slam,Steered:Storm:ArcGrid,Steered:Storm:Downpour,Steered:Storm:Tempest,Steered:Tide:Frost,Steered:Tide:Stillness,Steered:Tide:Whirlpool -Seeds 15 -Parallel 14 -Out Saved\BotRuns\d047_builds
python -I Tools\probe.py ablation Saved\Probes\d047 --builds Saved\BotRuns\d047_builds
& Tools\BotRuns.ps1 -Jobs Saved\Probes\d047\jobs.csv -Parallel 14 -Out Saved\Probes\d047\runs
python -I Tools\probe.py read Saved\Probes\d047 --check
```

| Batch | Asked for | Ran | Failed | Rerun | Ended |
|---|---|---|---|---|---|
| Builds (`Saved\BotRuns\d047_builds`) | 135 (9 arms × 15 seeds) | 135 | 0 | 0 | 101 victories, 34 deaths; `BOTRUNS: PASS` |
| Scenes (`Saved\Probes\d047\runs`) | 1000 (125 builds × 8 configurations) | 1000 | 0 | 0 | 1000 at the scene's end (18.0 s, `bot_timeout`); `BOTRUNS: PASS (0 run(s) without a bot record)` |

Every run wrote a bot record and a `run_end`. No run was killed on the wall timeout.

## Logged or refused
- **Snapshots missing at level sixteen: 10 of 135 runs**, each a death before level sixteen: Forge/Slam seed 12 (418.9 s, level 12); Tide/Frost seeds 10, 12, 14 (458.9, 306.9, 203.3 s); Tide/Stillness seeds 2, 7, 10, 12 (542.7, 413.5, 675.6, 145.6 s); Tide/Whirlpool seeds 8, 12 (483.0, 217.4 s). 125 snapshots, every one read with all eight configurations.
- **One modifier refused:** `Bot: probe modifier Mod.Pierce on Weapon.PulseRifle refused (6)` in `AB-Tide-Whirlpool-b11-no_levels_s11501`; that scene ran without it. No other `LogDT` error in the scene runs, and none in the build runs.
- No fatal error and no unhandled exception in any log.

## The reading
`probe.py read --check` exits 1 and prints one verdict line:

```
T-KEE-18: FAIL (9 arms: loss below 0.6 on 1, gain outside the band on 9, outside the archetype band on 1; mean loss 0.804; share, printed and never judged, mean 0.555)
```

Old loss is `S1_probes.md` §2 (`Saved/Probes/ab`, build `017e926`, before D-045 and D-046). That reading was taken on the 10× field; the probe has read a 40× field since `b9856a8`, and `S1_keeper_power.md` §1–§2 fixed the faults its §5 names between the two. So the difference holds the field and those fixes as well as D-045 and D-046. The last reading on the 40× field, `S1_keeper_power.md` §5, is printed beside it for that reason. The scene holds swarm only, the director off and no hit on the hero, so D-045 (Rusher arrivals) and D-046 (chest restore) reach this reading only through the builds the bot runs hold at level sixteen.

| Arm | Second Keeper | Builds read | Full DPS | Loss | Share, not judged | Package gain | Control gain (builds) | Loss verdict | Band | Old loss, `S1_probes.md` (builds) | Difference | Loss, `S1_keeper_power.md` §5 | Difference |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| Forge/Furnace | Storm | 15 | 11413 | 0.891 | 0.652 | 1.95 | 3.23 (15) | PASS | IN | 0.756 (14) | +0.135 | 0.862 | +0.029 |
| Forge/Hammer | Storm | 15 | 13733 | 0.937 | 0.781 | 2.89 | 4.48 (15) | PASS | IN | 0.858 (14) | +0.079 | 0.933 | +0.004 |
| Forge/Slam | Storm | 14 | 8182 | 0.827 | 0.521 | 1.18 | 1.50 (14) | PASS | IN | 0.865 (15) | −0.038 | 0.853 | −0.026 |
| Storm/ArcGrid | Tide | 15 | 4369 | 0.783 | 0.433 | 2.85 | 3.46 (15) | PASS | IN | 0.759 (14) | +0.024 | 0.788 | −0.005 |
| Storm/Downpour | Tide | 15 | 6065 | 0.850 | 0.439 | 5.06 | 4.76 (15) | PASS | IN | 0.829 (15) | +0.021 | 0.857 | −0.007 |
| Storm/Tempest | Tide | 15 | 2607 | 0.555 | 0.316 | 1.38 | 1.48 (15) | FAIL | OUT | 0.572 (14) | −0.017 | 0.580 | −0.025 |
| Tide/Frost | Forge | 12 | 7379 | 0.766 | 0.491 | 0.94 | 0.79 (12) | PASS | IN | 0.756 (12) | +0.010 | 0.681 | +0.085 |
| Tide/Stillness | Forge | 11 | 7874 | 0.767 | 0.653 | 1.15 | 1.54 (11) | PASS | IN | 0.626 (10) | +0.141 | 0.862 | −0.095 |
| Tide/Whirlpool | Forge | 13 | 7889 | 0.859 | 0.709 | 1.29 | 2.32 (13) | PASS | IN | 0.770 (7) | +0.089 | 0.827 | +0.032 |

Mean loss 0.804 against 0.755 in `S1_probes.md`. Package gain inside 2.0–2.4: 0 of 9 (`S1_keeper_power.md` §5 read 1 of 9, Furnace 2.05).

### Per build
Each arm holds one second Keeper, the bot's default for its patron, so the reading has no arm with a different second Keeper. What varies inside an arm is how many of the second Keeper's cards a build holds at level sixteen.

| Arm | Second Keeper's cards held, mean | Loss, lowest build | Loss, highest build | Builds below 0.6 |
|---|---|---|---|---|
| Forge/Furnace | 2.8 | 0.752 | 0.972 | 0 of 15 |
| Forge/Hammer | 3.3 | 0.821 | 0.987 | 0 of 15 |
| Forge/Slam | 4.1 | 0.382 | 0.971 | 1 of 14 |
| Storm/ArcGrid | 3.7 | 0.390 | 0.902 | 1 of 15 |
| Storm/Downpour | 3.8 | 0.742 | 0.947 | 0 of 15 |
| Storm/Tempest | 3.8 | −0.293 | 0.902 | 4 of 15 |
| Tide/Frost | 3.5 | 0.055 | 0.962 | 2 of 12 |
| Tide/Stillness | 2.5 | 0.111 | 0.966 | 2 of 11 |
| Tide/Whirlpool | 3.5 | 0.631 | 0.947 | 0 of 13 |

The ten builds below 0.6, with the second Keeper's cards each holds: Forge/Slam b11 0.382 (2); Storm/ArcGrid b1 0.390 (2); Storm/Tempest b13 0.504 (4), b14 0.063 (4), b4 0.134 (2), b9 −0.293 (4); Tide/Frost b1 0.567 (3), b6 0.055 (0); Tide/Stillness b1 0.111 (0), b14 0.450 (1). Tempest b9 deals more DPS without its blessings than with them. Built by a scratch script over `probe.py`'s own `read_all` and `share_of`, not by a tool in the repo.

## The machine
`run_end.wall_dropped_steps` and `run_end.wall_duration_s` (D-046), summed over the runs.

| Batch | Runs | Runs with any dropped step | Dropped steps, total | Most in one run | Wall s, total | Sim s, total | Wall / sim |
|---|---|---|---|---|---|---|---|
| Builds | 135 | 130 | 2082 | 190 (Tide/Stillness seed 6) | 507.9 | 124111.3 | 0.0041 |
| Scenes | 1000 | 0 | 0 | 0 | 1125.0 | 18000.0 | 0.0625 |

Build runs per arm:

| Arm | Runs with any | Dropped, total | Most in one run | Wall s | Sim s | Wall / sim |
|---|---|---|---|---|---|---|
| Forge/Furnace | 15 | 186 | 88 | 57.6 | 14340.4 | 0.0040 |
| Forge/Hammer | 15 | 167 | 73 | 56.2 | 14637.3 | 0.0038 |
| Forge/Slam | 15 | 96 | 15 | 54.7 | 14172.2 | 0.0039 |
| Storm/ArcGrid | 14 | 172 | 108 | 55.8 | 14481.1 | 0.0039 |
| Storm/Downpour | 15 | 94 | 15 | 55.4 | 14718.2 | 0.0038 |
| Storm/Tempest | 15 | 134 | 17 | 56.5 | 14561.8 | 0.0039 |
| Tide/Frost | 15 | 573 | 189 | 61.9 | 12532.9 | 0.0049 |
| Tide/Stillness | 14 | 555 | 190 | 58.2 | 11904.8 | 0.0049 |
| Tide/Whirlpool | 12 | 105 | 16 | 51.5 | 12762.8 | 0.0040 |

Per build run, wall over sim lies between 0.0035 and 0.0099; the highest is Tide/Stillness seed 12, a 145.6 s death in 1.4 wall seconds. Per scene run it lies between 0.0548 and 0.0783. A bot run at 600 steps a frame runs nearly all its steps past the clock (`S1_d045_d046.md`), so a dropped step here is the fast-forward's, not a person's wait. No rule says how many set a run aside (`telemetry.md` Open), and none was applied.

## Against D-047's revert clause
The clause reads: "reverted if the reread shows loss at or above the floor on every arm whatever blessings are held".

- **Arms below `C.POWER.KEEPER_SHARE_L16` (0.6):** one, Storm/Tempest at 0.555. The other eight read 0.766 to 0.937.
- **Arms outside `C.POWER.ARCHETYPE_BAND` (±0.15 of 0.804, so 0.654 to 0.954):** one, Storm/Tempest.
- **Per build:** 10 of 125 builds read below 0.6, on five arms (Slam 1, Arc Grid 1, Tempest 4, Frost 2, Stillness 2). Furnace, Hammer, Downpour and Whirlpool have no build below 0.6.
- **Whatever blessings are held:** each arm here holds one second Keeper, so the reading covers the default pairing of each patron and the spread of how many of that Keeper's cards a build holds (0 to 4 or more). The pairings Storm with Forge, Tide with Storm and Forge with Tide were not read.
- **The control, `S1_probes.md` §7 item 2** (package gain near 1.0 on Furnace, Frost and Whirlpool):
  - Forge/Furnace: gain 1.95 with the second Keeper's cards held; 3.23 with them off both sides.
  - Tide/Frost: gain 0.94; 0.79 with them off both sides.
  - Tide/Whirlpool: gain 1.29; 2.32 with them off both sides, inside 2.0–2.4.
  - The other six arms: Hammer 2.89 → 4.48, Slam 1.18 → 1.50, Arc Grid 2.85 → 3.46, Downpour 5.06 → 4.76, Tempest 1.38 → 1.48, Stillness 1.15 → 1.54.

What was read: loss is below the floor on one arm of nine, so the reading does not show loss at or above the floor on every arm. The judgement of the clause is the scheduler's.

## Where the files are
Untracked, in this worktree: `Saved\BotRuns\d047_builds` (build runs, `summary.md`), `Saved\Probes\d047\probe.md` (the reading), `Saved\Probes\d047\runs` (scene runs), `Saved\Logs\` (`build_d047_reread.log`, `botruns_d047_builds.log`, `probe_ablation_d047.log`, `botruns_d047_probes.log`, `probe_read_d047.log`, `procwatch_d047.log`, `procwatch2_d047.log`).

## Not verified
- Any second Keeper other than each patron's default.
- The Shipping build: every run is the Development editor build.
- People: no person's build is in this reading (O-008, D-048).
