# S1 — perf at the real 1080p, alone on the machine (2026-11-11)

**Reading: `PERF: PASS`, three runs of three, on `main` 8f2579e. Game thread p95 2.33 to 2.47 ms against a budget of 10. GPU p95 2.94 to 2.95 ms on the RTX 4080, which is 4.71 ms by the 1.6 proxy against the 12 ms of `C.BUDGET.GPU_MS_4070`. This is the first perf reading of the project taken at the size it names: every earlier one rendered 888x500 while recording 1920x1080 (`S1_gate_main.md`). It is one machine and one GPU, and the 4070 figure is still a proxy (D-011, O-012).**

## Conditions
- Build: `main` 8f2579e, the editor's Development build, in the scheduler's worktree.
- Tool: `Tools\Perf.ps1` with its defaults. The stress arena at its caps (`DT.Spawn.Keep 400`, `DT.Bullets 6000`), a warm-up of `C.PERF.WARMUP_S`, `C.PERF.CAPTURE_FRAMES` frames, the size of `C.BUDGET.RES` forced. The size is read from the capture: 1920x1080 rendered, 1920x1080 asked, in all three.
- Alone (D-050 item 4): no other engine or compiler process was alive before the runs or after them, and the run holds the machine-wide GPU lock. The carry's agent had reported and was not yet resumed.
- Under D-052: mask `FFFFF3FC`, the engine held by the launch job's limit, so 28 of the 32 logical processors.
- Three runs back to back, 12:51:01 to 12:52:16.
- The adapter line reads "GameViewer Virtual Display Adapter + OrayIddDriver Device + NVIDIA GeForce RTX 4080": two virtual displays of remote-desktop tools sit beside the card. The frame is rendered off screen by the 4080.

## Readings (ms)
| Run | Frame p50 / p95 | Game thread p50 / p95 | GPU p50 / p95 | Bullets / enemies (p50) | Verdict |
|---|---|---|---|---|---|
| 1 | 3.76 / 4.18 | 2.07 / 2.47 | 2.90 / 2.94 | 5975 / 388 | PASS |
| 2 | 3.68 / 4.02 | 2.03 / 2.44 | 2.91 / 2.95 | 5975 / 388 | PASS |
| 3 | 3.53 / 3.92 | 1.93 / 2.33 | 2.91 / 2.94 | 5975 / 388 | PASS |

Budgets: game thread p50 at most `C.BUDGET.GAME_THREAD_P50_MS`, p95 at most `C.BUDGET.GAME_THREAD_P95_MS`; GPU p95 times the proxy factor at most `C.BUDGET.GPU_MS_4070`. The load holds at least 0.95 of each cap, as the tool requires.

The three `PERF:` lines, verbatim:
- `PERF: PASS (1920x1080, game p50 2.07 / p95 2.47 ms, GPU p95 2.94 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)`
- `PERF: PASS (1920x1080, game p50 2.03 / p95 2.44 ms, GPU p95 2.95 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)`
- `PERF: PASS (1920x1080, game p50 1.93 / p95 2.33 ms, GPU p95 2.94 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)`

## Beside the old figures
STATE carried "game thread p95 about 2.5 ms, proxied GPU p95 about 3.7 ms", read at 888x500 on a busy machine. At the real size and alone the game thread reads the same and the proxied GPU reads 4.71 ms. The budget was never shown met before today; it is met now, on this scene, with room.

## What this does not show
- One GPU. The 4070 number is this card's time multiplied by a factor from raster benchmarks. No 3060-class or Steam Deck budget exists, and none was read (O-012, `plans_audit.md` 4.6).
- The perf scene grants no Keeper blessings, so a late run with Keeper effects on screen is not this scene.
- No render-thread reading and no hitch measure: p50 and p95 of 1800 frames only.
- The editor's Development build, not the Shipping package.
- `dev/s4`, where the art of wave 1 lives and where every package's perf allowance was read at 888x500. That reading is owed after the carry reaches `dev/s4`.

## Where the files are
Scheduler's worktree `E:\Dusktracer\.claude\worktrees\central-project-scheduler-5e72c5`: `Saved\Perf\perf.csv` (three rows stamped 8f2579e), the captures under `Saved\Profiling\CSV\`, and `Saved\Logs\Scheduler_perf_1080p_run1.log` to `run3.log`.
