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
- Package E's cost: E is not built.

## Where the files are
The carry agent's worktree `E:\Dusktracer\.claude\worktrees\agent-aa7a88e9be0750f6a`: `Saved\Logs\perf_devs4_1080p.txt` (the output of all seven runs), `Saved\Perf\perf.csv` (the last seven rows, stamped fc5000b), and the captures under `Saved\Profiling\CSV\`.
