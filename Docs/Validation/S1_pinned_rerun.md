# S1 — probe scenes rerun on single processors: do the engine's results move on processors 0 and 10? (2026-11-11)

**Reading: no result moved. 12 probe scenes ran on processor 0, on processor 10, on processor 2 and inside the D-052 mask. Each run's damage sum, `hit` count, end reason, step count and `wall_dropped_steps` equal the unpinned run of the same scene in `S1_d047_reread.md`, to the last digit, 48 of 48. The None arm of seed 9107 reads `input_hash` `fe30ec79`, the value on `main`, under all four conditions and unmasked. No engine run crashed or logged a fault. The method the job was given did not hold the engine on the processor: the engine sets its own process affinity back to all 32 processors, again and again, while it runs. The readings that count were taken with a watcher that pinned each engine process again on every reset.**

## Build and conditions
- Branch `s1/pinned-rerun` from `6f09c50` (`main`). Binaries are the reread's, built at `e88f395` and stamped `e88f395+dirty` (`S1_d047_reread.md`). `6f09c50` differs from `e88f395` in `Docs/` and `AGENTS.md` only (`git diff --name-only`), so no rebuild was made. No launch said the binaries were stale.
- Reference: the unpinned runs in `Saved\Probes\d047\runs`, taken on all 32 logical processors at `-Parallel 14`.
- Scenes: build b1 of Forge/Hammer, Storm/Tempest, Tide/Frost and Tide/Whirlpool. For each build, the `full`, `no_keepers` (no blessings) and `no_second` scenes are taken from `Saved\Probes\d047\jobs.csv` into `Saved\Probes\pinned\jobs12.csv`. The seeds are the reread's: 4601, 101, 9001, 11301. Each scene is 18 sim seconds, 1080 steps.
- The mask file `<git common dir>\dt-locks\affinity.txt` reads `FFFFFBFE`.

| Condition | Shell mask | Parallel |
|---|---|---|
| Processor 0 | `[IntPtr]0x1L` | 1 |
| Processor 10 | `[IntPtr]0x400L` | 1 |
| Processor 2, the control | `[IntPtr]0x4L` | 1 |
| Inside the mask | `[IntPtr]0xFFFFFBFEL` | 6 |

## What held the engine on the processor
- **The shell's mask does not reach the engine.** With the launching shell set to `0x1`, `0x400` or `0x4`, each `UnrealEditor-Cmd.exe` read `0xFFFFFFFF` once it had started. The hidden launch passes the mask on: from a shell set to `0x4`, a Python child started by `Start-DTHidden` reads `0x4`, and so does one started by `Process.Start`. The engine moves its own process mask back to all processors. It does so repeatedly while it runs, not only at start. The engine's `-processaffinity` code (`LaunchWindows.cpp`) runs only when that flag is given, and none of these launches gives it. What moves the mask was not traced.
- **The method used for the readings.** Each condition runs from a shell that sets its mask as its first statement. A watcher in that shell (a scratchpad script, `Start-Job`) looks every 50 ms for engine processes whose parent is the shell. It pins each one to the condition's mask within 0.13 to 0.36 s of its start, and pins it again each time its mask reads otherwise. Pinned again per condition: 131 times over 12 runs on processor 0, 109 on processor 10, 125 on processor 2 and 65 inside the mask. Each time the engine had all processors for up to one watcher pass, about 0.2 to 0.5 s. **So "on processor 0" means on processor 0 except for those gaps, not every instruction.** The wall times (below) show the pin held for most of each run.
- **Two sets of runs are set apart as not pinned.**
  - The first three conditions were run with the shell mask alone, as specified, into `Saved\Probes\pinned\cpu0`, `cpu10` and `cpu2`. The engine was not held in those runs. 11 of 12 scenes in each equal the reference. The twelfth, Tempest `full`, cannot be read there: the three conditions ran at once and wrote one telemetry file, `101_20261111_102841`, for that scene. It holds 2331 hits and 130788 damage, exactly three times the reference's 777 and 43596. This is the trap STATE names: two runs on one seed ending in one second share a file name.
  - The first mask run passed `0xFFFFFBFE` without the trailing `L`. That literal is negative, so the mask was never set (`AGENTS.md` names this trap). That run is kept as `mask_unset`: 12 of 12 scenes equal the reference, and none was pinned.
- Every pinned condition then ran on its own, one after another, so no two shared a seed in a second. Each condition's 12 telemetry files have 12 distinct names.

Command lines. Each `runheld.ps1` call is one condition: the mask as its first statement, the watcher, then BotRuns.

```
(Get-Process -Id $PID).ProcessorAffinity = [IntPtr]<mask>     # first statement
& Tools\BotRuns.ps1 -Jobs Saved\Probes\pinned\jobs12.csv -Parallel <1|6> -Out Saved\Probes\pinned\<condition>_held
& Tools\BotRuns.ps1 -Arms None -FirstSeed 9107 -Seeds 1 -Out Saved\BotRuns\seed9107_<condition>
```

`Tools\SeedCheck.ps1` was not used for the seed: its header shows it checks that a human launch draws a fresh seed, and it runs no arm on a fixed seed. The None arm of seed 9107 is read the way `S1_d045_d046.md` read it, with `BotRuns.ps1 -Arms None -FirstSeed 9107 -Seeds 1`. `input_hash` is taken from `bot.csv.txt`.

## Scenes
Compared fields: damage summed by `Tools/probe.py`'s own `read_run`, the count of `hit` records, `run_end.reason`, `run_end.steps` and `run_end.wall_dropped_steps`. "equal" means all five are equal to the last digit.

| Scene (seed) | Reference: damage, hits, steps, dropped | Processor 0 | Processor 10 | Processor 2 | Inside the mask |
|---|---|---|---|---|---|
| Hammer `full` (4601) | 145933, 989, 1080, 0 | equal | equal | equal | equal |
| Hammer `no_keepers` (4601) | 4587, 887, 1080, 0 | equal | equal | equal | equal |
| Hammer `no_second` (4601) | 117247, 982, 1080, 0 | equal | equal | equal | equal |
| Tempest `full` (101) | 43596, 777, 1080, 0 | equal | equal | equal | equal |
| Tempest `no_keepers` (101) | 8966, 377, 1080, 0 | equal | equal | equal | equal |
| Tempest `no_second` (101) | 44722, 769, 1080, 0 | equal | equal | equal | equal |
| Frost `full` (9001) | 90459, 973, 1080, 0 | equal | equal | equal | equal |
| Frost `no_keepers` (9001) | 39208, 974, 1080, 0 | equal | equal | equal | equal |
| Frost `no_second` (9001) | 24508, 971, 1080, 0 | equal | equal | equal | equal |
| Whirlpool `full` (11301) | 50697, 975, 1080, 0 | equal | equal | equal | equal |
| Whirlpool `no_keepers` (11301) | 4689, 249, 1080, 0 | equal | equal | equal | equal |
| Whirlpool `no_second` (11301) | 12004, 431, 1080, 0 | equal | equal | equal | equal |

Every end reason is `bot_timeout`, the scene's normal end at 18.0 s.

## Seed 9107, the None arm

| Condition | `input_hash` | Outcome | End s | Steps | `wall_dropped_steps` | `wall_duration_s` | Batch wall s | Pinned again |
|---|---|---|---|---|---|---|---|---|
| Unmasked (`seed9107_unset`) | `fe30ec79` | death | 636.3167 | 38181 | 77 | 4.06 | 9.8 | not pinned |
| Inside the mask | `fe30ec79` | death | 636.3167 | 38181 | 78 | 4.09 | 9.8 | 5 |
| Processor 2 | `fe30ec79` | death | 636.3167 | 38181 | 79 | 13.05 | 19.9 | 8 |
| Processor 0 | `fe30ec79` | death | 636.3167 | 38181 | 184 | 26.40 | 36.3 | 8 |
| Processor 10 | `fe30ec79` | death | 636.3167 | 38181 | 77 | 12.67 | 19.7 | 8 |

All five equal `fe30ec79`, the value on `main` (`S1_d045_d046.md`). `wall_dropped_steps` is a wall-clock field (R-TEL-08). It is the one compared field that differs here, 77 to 184, and it differs between runs as such a field may.

## Crashes and faults
Per condition: run logs scanned for `LogDT: Error`, `LogWindows: Error`, `Access violation`, `EXCEPTION_`, `Fatal error`, `Unhandled Exception`, `Assertion failed`, `Ensure condition failed`, `=== Critical error`; `LogDT` warnings; a session end other than the expected one; a log without `LogExit: Exiting`.

| Condition | Runs | Error lines | `LogDT` warnings | Early or odd end | Clean `LogExit` | Bot record and `run_end` |
|---|---|---|---|---|---|---|
| Processor 0 | 12 + 1 | 0 | 0 | 0 | 13 of 13 | 13 of 13 |
| Processor 10 | 12 + 1 | 0 | 0 | 0 | 13 of 13 | 13 of 13 |
| Processor 2 | 12 + 1 | 0 | 0 | 0 | 13 of 13 | 13 of 13 |
| Inside the mask | 12 + 1 | 0 | 0 | 0 | 13 of 13 | 13 of 13 |
| Not pinned (`cpu0`, `cpu10`, `cpu2`, `mask_unset`, `seed9107_unset`) | 49 | 0 | 0 | 0 | 49 of 49 | 49 of 49 |

- Windows Application log, 10:28 to the end of the runs: no event with id 1000, 1001, 1002, 1005 or 1026 that names `UnrealEditor`, `python` or `ShaderCompileWorker`. The log can be read: its last such events are three `python.exe` faults between 06:07 and 06:17, before this job.
- `BotRuns.ps1` does not print an engine's exit code, so none is recorded. Each batch printed `BOTRUNS: PASS` and exited 0, and no run was killed on the timeout.

## Wall times
| Condition | Batch wall s (12 scenes) | One scene, wall s per run (`run_end.wall_duration_s`) | Sum over 12 / 216 sim s |
|---|---|---|---|
| Processor 0 | 358.1 | 9.2 to 20.9 | 0.82 |
| Processor 10 | 208.1 | 5.2 to 12.4 | 0.55 |
| Processor 2 | 463.4 | 7.3 to 23.6 | 0.77 |
| Inside the mask, 6 at once | 55.0 | 1.0 to 1.3 | 0.07 |
| Reference, unpinned, 14 at once | — | 1.0 to 1.2 | — |

Another agent was building and gating in its own worktree during these runs, so the single-processor times include its load on those processors. No pinned scene dropped a step.

## What was read
On processors 0 and 10 the engine gave the same results as on processor 2, inside the mask and unpinned. That covers 12 probe scenes and one seeded 636-second run, each field equal to the last digit, and the seed 9107 hash `fe30ec79` under every condition. The engine did not fault on either processor: no crash, no error line, no early end and no Application log event, in 26 pinned runs there and 26 more on processor 2 and the mask. What this sample cannot show:
- 12 scenes of 18 sim seconds and one run of 636 sim seconds are a small part of what runs on this PC. A fault rare enough to miss these is not ruled out.
- The pin was not exact. The engine reset its mask 5 to 131 times per condition and held all processors for up to about half a second each time. Work done in those gaps ran elsewhere.
- Every run was single-threaded game logic under `-nullrhi`. No compile, link, shader compile, cook, package or rendered frame ran pinned.
- No Python step was rerun here. That is where `S4_py_fault.md` saw its faults.
- A wrong answer the sim does not record in these five fields would not show.
