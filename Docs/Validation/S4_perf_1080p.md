# S4 wave 1: perf at the real 1080p, alone on the machine (2026-11-11)

**Reading: `PERF: PASS`, seven runs of seven, on `dev/s4` fc5000b: Caps three times, the S4 scene three times, the S2 scene once, each at 1920x1080 rendered. At caps the game thread p95 reads 2.45 to 2.53 ms and the GPU p95 3.01 to 3.02 ms, 4.82 to 4.83 ms by the 1.6 proxy. The S4 scene reads 4.76 to 4.78 ms and 3.71 to 3.72 ms (5.94 to 5.95 proxied), the S2 scene 4.20 ms and 3.67 ms (5.87). All are inside `C.BUDGET.*` and inside wave 1's ceiling (`S4_wave1.md` §5 item 5), with package E not yet built. This is the first perf reading of `dev/s4` at the size it names. It is one machine and one GPU, and the 4070 figure is still a proxy (D-011, O-012).**

## Conditions
- Build: `dev/s4` fc5000b, the editor's Development build, in the carry agent's worktree. The binaries are the third carry's gate build of e1fbb6d; fc5000b differs from it only by `Validation/S4_carry.md`. The `commit` column of each `perf.csv` row reads fc5000b.
- Taken by the scheduler from 17:04:52, the first run's start, to the last run, which started at 17:08:06. It was alone (D-050 item 4): no other engine or compiler process was alive, and every run held the machine-wide GPU lock (`Tools/GpuLock.ps1`).
- Tool: `Tools\Perf.ps1` with its defaults, then `-Scene S4` and `-Scene S2`. The size comes from `C.BUDGET.RES`, is forced, and is read back from the capture.
- Under D-052: `Tools\Perf.ps1` applies the machine's mask `FFFFF3FC` itself (`Tools/Machine.ps1`), and the launch job holds the engine to it, so 28 of 32 logical processors were in use. The tool does not print the mask.
- The adapter line reads "GameViewer Virtual Display Adapter + OrayIddDriver Device + NVIDIA GeForce RTX 4080". The frame is rendered off screen by the 4080, as in `S1_perf_1080p.md`.

## Readings (ms)
| Run | Scene | Size rendered | Frame p50 / p95 | Game thread p50 / p95 | GPU p50 / p95 | Proxied GPU p95 | Load (p50) | Verdict |
|---|---|---|---|---|---|---|---|---|
| 17:04:52 | Caps | 1920x1080 | 3.69 / 4.11 | 2.10 / 2.53 | 2.96 / 3.02 | 4.83 | 5975 bullets / 388 enemies | PASS |
| 17:05:16 | Caps | 1920x1080 | 3.56 / 3.95 | 2.06 / 2.50 | 2.96 / 3.01 | 4.82 | 5975 / 388 | PASS |
| 17:05:41 | Caps | 1920x1080 | 3.50 / 3.86 | 2.05 / 2.45 | 2.97 / 3.02 | 4.83 | 5975 / 388 | PASS |
| 17:06:05 | S4 | 1920x1080 | 4.69 / 5.62 | 3.27 / 4.77 | 3.45 / 3.72 | 5.95 | 5816 / 400; 12 telegraphs, 5816 rounds trailed | PASS |
| 17:06:45 | S4 | 1920x1080 | 4.76 / 5.62 | 3.28 / 4.76 | 3.45 / 3.71 | 5.94 | 5816 / 400; 12 telegraphs, 5816 trailed | PASS |
| 17:07:26 | S4 | 1920x1080 | 4.72 / 5.70 | 3.27 / 4.78 | 3.45 / 3.72 | 5.94 | 5815 / 400; 12 telegraphs, 5815 trailed | PASS |
| 17:08:06 | S2 | 1920x1080 | 4.67 / 6.05 | 2.78 / 4.20 | 3.38 / 3.67 | 5.87 | 5810 / 400 | PASS |

Every run reads "size 1920x1080 rendered, 1920x1080 asked" and captured 1800 frames. The `PERF:` lines, verbatim:
- `PERF: PASS (1920x1080, game p50 2.10 / p95 2.53 ms, GPU p95 3.02 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)`
- `PERF: PASS (1920x1080, game p50 2.06 / p95 2.50 ms, GPU p95 3.01 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)`
- `PERF: PASS (1920x1080, game p50 2.05 / p95 2.45 ms, GPU p95 3.02 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)`
- `PERF: PASS (1920x1080, game p50 3.27 / p95 4.77 ms, GPU p95 3.72 ms, load 5816.00 bullets / 400.00 enemies, 1800 frames)`
- `PERF: PASS (1920x1080, game p50 3.28 / p95 4.76 ms, GPU p95 3.71 ms, load 5816.00 bullets / 400.00 enemies, 1800 frames)`
- `PERF: PASS (1920x1080, game p50 3.27 / p95 4.78 ms, GPU p95 3.72 ms, load 5815.00 bullets / 400.00 enemies, 1800 frames)`
- `PERF: PASS (1920x1080, game p50 2.78 / p95 4.20 ms, GPU p95 3.67 ms, load 5810.00 bullets / 400.00 enemies, 1800 frames)`

## Against the budgets and the wave's ceiling
| | Budget | Caps | S4 scene | S2 scene |
|---|---|---|---|---|
| Game thread p50 | `C.BUDGET.GAME_THREAD_P50_MS` 8 | 2.05 to 2.10 | 3.27 to 3.28 | 2.78 |
| Game thread p95 | `C.BUDGET.GAME_THREAD_P95_MS` 10 | 2.45 to 2.53 | 4.76 to 4.78 | 4.20 |
| Proxied GPU p95 | `C.BUDGET.GPU_MS_4070` 12 | 4.82 to 4.83 | 5.94 to 5.95 | 5.87 |
| Wave 1 ceiling, game thread p95 at caps | 5.0 | 2.45 to 2.53 | 4.76 to 4.78 | |
| Wave 1 ceiling, proxied GPU p95 at caps | 8.0 | 4.82 to 4.83 | 5.94 to 5.95 | |

At caps, as the plan writes the ceiling, the wave reads 2.5 ms of game thread and 4.8 ms of proxied GPU, against 5.0 and 8.0. On the S4 scene, which draws every round's trail and holds telegraphs live, it reads 4.8 and 5.9: the game thread is 0.22 to 0.24 ms under the 5.0 ceiling, and the GPU 2.05 ms under 8.0. Package E (hit and kill feedback, an allowance of 0.5 ms) is not built.

## Beside `main`'s reading of record
`S1_perf_1080p.md`, `main` 8f2579e, the Caps scene, three runs, alone, at 1920x1080:

| Caps | `main` 8f2579e | `dev/s4` fc5000b | Difference |
|---|---|---|---|
| Game thread p95 | 2.33 to 2.47 | 2.45 to 2.53 | about +0.1 |
| GPU p95 | 2.94 to 2.95 | 3.01 to 3.02 | +0.07 |
| Proxied GPU p95 | 4.71 to 4.72 | 4.82 to 4.83 | +0.11 |

The two trees differ by S2, S3 and wave 1 together, so the difference is not the wave's alone. The Caps scene draws no trail and holds no telegraph, so most of what the wave added is not on this scene. Only the S4 scene shows that, and `main` has no S4 scene to compare it against.

## What this replaces
Every earlier perf figure on `dev/s4` rendered 888x500 while recording 1920x1080 (`S1_gate_main.md`), and most were taken while other agents built and rendered. Each of these notes now carries a pointer line at the top of its perf section and is otherwise unchanged:
- `S4_p0.md` §7: the baselines, Caps 3.33 to 3.42 ms proxied and the S4 scene's 7.16 to 7.26 ms;
- `S4_a_arena.md` §6, `S4_b_enemies.md` §5, `S4_c_hero.md` §5 and `S4_tracers.md` §3: the per-package readings;
- `S4_f_hud.md`'s perf table.

The S4 scene read 7.16 and 7.26 ms proxied on P0 at the smaller size and 5.94 and 5.95 here at the larger one. The earlier figures came from a GPU shared with other agents' games, which a process list could not see (`S4_p0.md` §7). This note does not try to explain the difference further.

## What this does not show
- One GPU. The 4070 figure is this card's time multiplied by a factor from raster benchmarks (D-011). No 3060-class or Steam Deck reading exists (O-012).
- The editor's Development build, not the Shipping package.
- No render-thread reading and no hitch measure: p50 and p95 of 1800 frames only.
- The perf scenes grant no Keeper blessings and play in one weather; Intercessions, statuses and the second weather are not on any scene here.
- Package E's cost: not in these runs. The reading with the tap and package E, on 3cb9534, is `S4_e_feedback.md` §10.
- The clear zone's cost (D-068): not in these runs. A comparison on one evening is the section below; no record with the zone exists yet.

## The clear zone against its base, one evening (2026-11-12): a comparison, not a record

**Conditions.** The scheduler's window `w2`. Three trees, each clean: `base`, `dev/s4` 64b40cf before the zone (`agent-aa7a88e9be0750f6a`); `zone`, d09d4c9, the clear zone's branch as handed over (`agent-a4ffafc84344cbaea`); `audio`, 1834e13, the audio runtime on top (`agent-a82ff951d2dcf6259`). The machine was taken at 18:07:57 and released at 18:21:18; the eighteen runs were logged from 18:09:16 to 18:21:18, each time a run's end (as in `S4_e_feedback.md` §10), the three builds in turn in each scene and round. `Tools\Perf.ps1 -Scene S4` and `-Scene S2` at their defaults, 1800 frames, three rounds; every run rendered 1920x1080 and asked 1920x1080. The machine was alone as to the sessions' own jobs: no other engine or compiler process of ours ran. It was **not quiet**: after Windows Update restarted the PC at 04:44, the owner's own programs were running (the scheduler names an Android emulator at about half a core and a video app). They are the owner's; nobody touches them.

**The control.** Right after (window `control`, released 18:25:25), `main` f5d7334 at Caps read game thread p95 2.82, 2.75 and 2.81 ms (p50 2.14 to 2.23) and GPU p95 3.83 to 3.91 ms, against its reading of record in `S1_perf_1080p.md` (8f2579e, alone: 2.33 to 2.47 ms and 2.94 to 2.95 ms, the table above): about 0.35 ms more game thread and about 30 percent more GPU time. The base tree at Caps read 3.64, 2.93 and 2.97 ms, against 2.55 to 2.57 for 3cb9534 alone the night before (`S4_e_feedback.md` §10). So every build read slower than its record that evening, unchanged ones too.

**Base against zone**, one row per scene and round (ms; GPU p95 raw, proxied x1.6 in brackets; `SimRender` and `SimTick` are the `DT/GameThread/*` scope rows, mean / p95):

| Scene, round | Base: game thread p50 / p95 | Base: GPU p95 | Base: `SimRender` | Base: `SimTick` | Zone: game thread p50 / p95 | Zone: GPU p95 | Zone: `SimRender` | Zone: `SimTick` | Game thread p95, zone less base | `SimRender` mean, zone less base |
|---|---|---|---|---|---|---|---|---|---|---|
| S4, 1 | 3.48 / 5.02 | 4.17 (6.67) | 0.64 / 0.77 | 1.07 / 2.18 | 3.59 / 5.21 | 4.14 (6.63) | 0.68 / 0.93 | 1.15 / 2.29 | +0.19 | +0.04 |
| S4, 2 | 3.53 / 5.07 | 4.18 (6.69) | 0.64 / 0.75 | 1.08 / 2.19 | 3.60 / 5.27 | 4.16 (6.65) | 0.69 / 0.96 | 1.17 / 2.30 | +0.20 | +0.05 |
| S4, 3 | 3.76 / 5.36 | 4.13 (6.61) | 0.65 / 0.87 | 1.10 / 2.38 | 3.49 / 5.06 | 4.58 (7.32) | 0.66 / 0.79 | 1.09 / 2.20 | -0.30 | +0.01 |
| S2, 1 | 3.18 / 4.74 | 4.10 (6.56) | 0.52 / 0.70 | 0.93 / 2.11 | 3.03 / 4.57 | 4.08 (6.53) | 0.54 / 0.72 | 0.97 / 1.99 | -0.17 | +0.02 |
| S2, 2 | 3.10 / 4.69 | 4.09 (6.54) | 0.52 / 0.72 | 0.96 / 2.01 | 2.93 / 4.37 | 4.08 (6.53) | 0.50 / 0.56 | 0.88 / 1.89 | -0.32 | -0.02 |
| S2, 3 | 3.21 / 4.89 | 4.50 (7.20) | 0.54 / 0.74 | 1.01 / 2.17 | 3.09 / 4.61 | 4.48 (7.17) | 0.54 / 0.69 | 0.97 / 2.03 | -0.28 | 0.00 |

**What it shows.**
- In the S4 scene the zone reads higher than its base in two rounds (+0.19, +0.20) and lower in the third (-0.30). The spread between a build's own three rounds is larger than any of those differences: the base's game thread p95 spans 5.02 to 5.36 (0.34), the zone's 5.06 to 5.27 (0.21).
- In the S2 scene the zone reads lower in all three rounds (-0.17, -0.32, -0.28); the base spans 4.69 to 4.89 (0.20), the zone 4.37 to 4.61 (0.24).
- The step: `SimTick` rises with the zone in the S4 scene's first two rounds (mean 1.07 and 1.08 to 1.15 and 1.17) and falls in the third (1.10 to 1.09); in the S2 scene it is level or lower. `SimTick` contains `SimRender` (`UDTSimSubsystem::Tick` opens the draw scope inside the step scope), so half of those two rises is the draw scope's own.
- The GPU is level: 4.17 and 4.18 against 4.14 and 4.16 in the S4 scene's first two rounds, and within 0.02 in all three rounds of the S2 scene. In the S4 scene's third round the zone and audio builds read 4.58 and 4.61 and the base 4.13: the round moved, not one build, since the S2 scene's third round reads 4.48 to 4.53 for all three.
- The one place the zone can be seen is the draw scope's mean in the S4 scene, a few hundredths of a millisecond (+0.04 and +0.05) in two of three rounds; the S2 scene does not show it.
- So the zone shows no cost beyond the spread, which is what D-068 item 4 asked; its constant stays.

**What it does not show.**
- It compares two builds with each other on one loaded evening and replaces no record. Whether the S4 scene stands under the wave's 5.0 ms game thread ceiling is read again on a quiet machine; until then no branch is judged against the night before's absolutes.
- The two builds are two trees, so a difference of a few hundredths may be the tree and not the zone. A pair on one build with the switch (`DT_TRACER_CLEAR_ZONE` 0 against the default) would tell: it was taken the same evening, not alone, and is the part below.
- The window also read a third build, the audio runtime; its lines are in the same files and belong to the runtime's own note.

**The pair on one build.** `s4/zone-goldens` at 6047ef3 (`dev/s4` with the zone and the audio runtime; no code change on the branch), built in the Visual worktree `agent-a76c15a416b3c6fea`, 2026-11-12. `Tools\Perf.ps1 -Scene S4` six times in turn, zone off (`DT_TRACER_CLEAR_ZONE` 0) and on (unset), three rounds, logged from 18:58:19 to 19:01:57 (each time a run's end). A first attempt at 18:51 lost its first round to another session's measurement holding the GPU lock and was taken again whole. Before each run the other engine processes alive: three editor commandlets of `agent-a37e1827c3d77226d` before round 1 off; six editor commandlets of `agent-a2b736d35d80303fb` before round 2 on; none before the other four. The owner's own programs were running throughout. It is a pair of one evening on a machine that was not quiet: no figure of it stands beside a record.

| Round | Zone | Frame p50 / p95 | Game thread p50 / p95 | GPU p50 / p95 | `SimRender` mean / p50 / p95 | `SimTick` mean / p50 / p95 | Load |
|---|---|---|---|---|---|---|---|
| 1 | off | 4.97 / 5.96 | 3.42 / 4.98 | 3.56 / 4.16 | 0.62 / 0.61 / 0.71 | 1.04 / 0.62 / 2.18 | 5817 bullets / 400 enemies |
| 1 | on | 5.33 / 7.11 | 3.72 / 5.79 | 3.54 / 4.24 | 0.71 / 0.66 / 0.99 | 1.23 / 0.75 / 2.58 | 5813 / 400 |
| 2 | off | 5.16 / 6.46 | 3.54 / 5.30 | 3.55 / 4.21 | 0.66 / 0.64 / 0.92 | 1.13 / 0.66 / 2.37 | 5814 / 400 |
| 2 | on | 5.09 / 6.34 | 3.55 / 5.21 | 3.54 / 4.21 | 0.67 / 0.64 / 0.94 | 1.12 / 0.66 / 2.30 | 5821 / 400 |
| 3 | off | 5.09 / 6.11 | 3.46 / 5.05 | 3.55 / 4.20 | 0.64 / 0.63 / 0.77 | 1.08 / 0.64 / 2.21 | 5815 / 400 |
| 3 | on | 6.30 / 8.48 | 4.72 / 7.28 | 3.59 / 4.25 | 0.85 / 0.88 / 1.11 | 1.63 / 1.00 / 3.44 | 5816 / 400 |

On less off, by round: game thread p95 +0.81, -0.09, +2.23; `SimRender` mean +0.09, +0.01, +0.21; `SimRender` p95 +0.28, +0.02, +0.34; `SimTick` mean +0.19, -0.01, +0.55. Spread across a side's three rounds: off, game thread p95 0.32, `SimRender` mean 0.04; on, game thread p95 2.07, `SimRender` mean 0.18.

The verdict, by the brief's rule (a cost only if `SimRender` mean reads higher with the zone on in all three rounds and the smallest of those differences is larger than the off side's own spread): `SimRender` mean reads higher with the zone on in all three rounds, but the smallest difference, +0.01 in round 2, is under the off side's spread of 0.04. **By the rule the pair shows no cost.** It is not a clean no: two of the three rounds read higher in the draw scope with the zone on, and the third "on" run (round 3) read slower everywhere, its frame, step and game thread as well as its draw scope, with no engine process of ours alive before it, which points to the machine and not to the zone. A quiet pair would settle it.

**Where the files are.** In the scheduler's worktree: `E:\Dusktracer\.claude\worktrees\central-project-scheduler-5e72c5\Saved\Logs\cps_window_w2.txt` (one line per run), `E:\Dusktracer\.claude\worktrees\central-project-scheduler-5e72c5\Saved\Logs\cps_window_w2.raw.txt` (the whole output), `E:\Dusktracer\.claude\worktrees\central-project-scheduler-5e72c5\Saved\Logs\cps_window_control.txt` and `E:\Dusktracer\.claude\worktrees\central-project-scheduler-5e72c5\Saved\Logs\cps_window_control.raw.txt`.

## Where the files are
The carry agent's worktree `E:\Dusktracer\.claude\worktrees\agent-aa7a88e9be0750f6a`: `Saved\Logs\perf_devs4_1080p.txt` (the output of all seven runs), `Saved\Perf\perf.csv` (the last seven rows, stamped fc5000b), and the captures under `Saved\Profiling\CSV\`.
