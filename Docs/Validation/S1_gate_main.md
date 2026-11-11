# S1 gate on main: readings of the job `s1/gate-main`

For the scheduler. Class BUG and TUNE on tools; the tool half of D-052. Branch `s1/gate-main` from `main` at 6f09c50. Every run below was taken under the mask `FFFFFBFE` (every logical processor but 0 and 10), set by hand before part A and by the scripts after it.

## 1. CPU canary calibration (D-052 item 3)

The loop is `S4_py_fault.md §5` unchanged: `f(3000)`, compared each round with the sum the parent process worked out on a processor the mask leaves in. A run is one process pinned to one processor; it fails when it dies, sums wrong or outlives 120 s. 20 runs a row, one after another, on a machine at 2 to 40 % load from other agents' engine launches.

| Processor | Rounds a run | Runs | Failed | Wall for the 20 |
|---|---|---|---|---|
| 0 | 300 | 20 | 18 | 52 s |
| 0 | 600 | 20 | 19 | 82 s |
| 0 | 600 (again) | 20 | 20 | 38 s |
| 0 | 1000 | 20 | 20 | 85 s |
| 2 | 600 | 20 | 0 | 279 s |
| 2 | 1000 | 20 | 0 | 282 s |
| 1 | 600 | 20 | 0 | 187 s |
| 11 | 600 | 20 | 0 | 177 s |

Chosen size: 600 rounds a run (`ROUNDS` in `Tools/cpu_canary.py`). It reaches the bar on both readings at that size (19 and 20 of 20 on processor 0, 0 of 20 on processor 2); 300 rounds did not (18 of 20).

The whole step, mask `FFFFFBFE`, 30 processors:

| Parallel | Wall | Verdict |
|---|---|---|
| 8 | 65 s | `CPU: FAIL (processor 1)`: one access violation (exit 0xc0000005) on processor 1 |
| 16 | 35 s | `CPU: PASS (30 processors, mask FFFFFBFE, 600 rounds)` |

Chosen: 16 at a time (`PARALLEL`), 35 s, inside the 90 s budget.

**The processor 1 fault (D-052 item 4).** In the parallel-8 run the run pinned to processor 1 died with an access violation, the fault kind of `S4_py_fault.md §2`. Processor 1 is the sibling hyper-thread of processor 0. Pinned alone afterwards it passed 20 of 20 at 600 rounds, and the next full step (parallel 16) passed it too. `S4_py_fault.md` had 351 clean runs on processors other than 0 and 10. This is one fault in 21 runs of processor 1 here: evidence, not noise, and it goes to STATE and O-012. D-052's swap-out point ("the cpu step names a third processor") was met once; the mask file was not changed by this job (machine data, outside the brief).

## 2. Test floor (plans_audit 4.2)

| | Value |
|---|---|
| Old `C.GATE.TEST_FLOOR` | 30 |
| Tests the suite finds on this branch | 260 (`RunTests.ps1`: "260 run of 260 found") |
| New `C.GATE.TEST_FLOOR` | 260 |

`RunTests.ps1 -Floor <n>` now fails on a count above the floor as well as below, with `  ! <found> tests found, above the floor <n>: set C.GATE.TEST_FLOOR to <found> ...`. The first count run, before `locbuild` regenerated the text in this fresh worktree, had 6 tests failing on stale generated tables (Keepers.Keywords, Modifiers.RoundsTable, Offers.CardIdentity, Offers.KeywordNotes and two more); the gate runs `locbuild` before `tests`, and the gate's own reading is in section 4.

## 3. Perf.ps1 frame size (BUG)

| Run | Script | Flags | Size asked | Size rendered (CSV `systemresolution.resx/resy`) | `perf.csv` res column |
|---|---|---|---|---|---|
| before | `Perf.ps1` at 1e3e308 | `-windowed -RenderOffScreen -ResX=1920 -ResY=1080` | 1920x1080 | **888x500** | `1920x1080` (the flags, not the frame) |
| after | `Perf.ps1` at afa6d2f | the same plus `-ForceRes` | 1920x1080 (from `C.BUDGET.RES` "1080p60", 16:9) | **1920x1080** | `1920x1080` (read from the capture) |

Both were short runs to prove the size only (`-Warmup 3 -Frames 60 -Cmds "DT.Spawn.Keep 50"`, other agents' engines alive): their timings are no reading. Logs `Saved\Logs\S1gm_perf_before.log`, `S1gm_perf_after.log`. Earlier perf logs in other worktrees (2026-11-10) show the same: `systemresolution.resx` set to 1280, then 1066, then 888 as the engine shrank the window to fit the desktop. Every perf figure before this commit was taken at 888x500.

The size now comes from the capture's metadata line, is printed (`size 1920x1080 rendered, 1920x1080 asked`), written to `perf.csv` and to the `PERF:` line, and a different size ends `PERF: FAIL (...; rendered <w>x<h>, not <W>x<H>)`.

Other Tools scripts that pass `-ResX`/`-ResY` (none changed here):

| Script | Asks | Gets | Read from |
|---|---|---|---|
| `Tools/Smoke.ps1` (smoke, contrast, dormant line) | 1280x720 | 1280x720 | the screenshots (`Docs/Validation/goldens/arena_empty_*.png`, a recent `Saved/Smoke/contrast_en.png`) |
| `Tools/SeedCheck.ps1` | 1280x720 | 1280x720 | a recent launch log's `systemresolution.resx/resy` |
| `Tools/PadCheck.ps1` | 1280x720 | 1280x720 | the package screenshots `Saved/PadCheck/package_*.png` |
| `Tools/WindowCheck.ps1` (bypass control, no `-RenderOffScreen`) | 320x180 | not read: the window exists to be caught, nothing reads its frame | |

1280x720 fits the desktop, so the engine leaves it as asked; only 1920x1080 was shrunk.

## 4. Gate run (one run, start to end, no step rerun)

`& Tools\Gate.ps1 -Base main` at afa6d2f (`main` resolved to 1c670bf), mask `FFFFFBFE`, one run, no step rerun:

```
cpu:        CPU: PASS (30 processors, mask FFFFFBFE, 600 rounds)
secrets:    SECRETS: PASS (466 files, 0 hit(s))
docs:       DOCS: PASS (0 error(s), 8 warning(s), 196 rules, must-read ~15465 tokens)
constants:  CONSTANTS: PASS (constants + palette)
catalog:    CATALOG: PASS (8 catalogs, 18 C. cells resolved, 70 tags)
loc:        LOC: PASS (5 tables, 268 rows)
tools:      TOOLS: PASS (111 tests)
build:      rc 0, 0 warning(s)
locbuild:   LOCALIZE: PASS (Content\Localization\Game\<culture>\Game.locres)
nonunity:   0 file(s), 0 failed
shipping:   rc 0, 0 warning(s)
devgame:    rc 0, 0 warning(s)
tests:      260 run of 260 found, 0 failed, floor 260, 0 crash marker(s)
smoke:      SMOKE: PASS (arena_empty_en, ... image PASS (1280x720, ...))
smoke:      SMOKE: PASS (arena_empty_zh-Hans, ... image PASS (1280x720, ...))
seed:       PASS (4 human launches on fresh seeds, 4 exact, 2 rejected)
contrast:   CONTRAST: PASS (7/7 elements at or above 3.0)
contrast:   CONTRAST: PASS (dormant_line 13.62 on its panel, need 3.0)
windows:    PASS (0 shown by bot, smoke and test runs across 95 processes; probe caught; bypass caught; 4 lost; 38 s)
pad:        NOT RUN (no package; Tools\Package.ps1)
scope:      NOT RUN (no brief in flight)
mac:        NOT RUN (deferred, D-007)
GATE: PASS (afa6d2f; 3 not run; mask FFFFFBFE)
```

New steps: `cpu` 30 processors pass at 600 rounds; `devgame` (the Development game target) compiles with 0 warnings, so no editor-only API inside `WITH_DEV_AUTOMATION_TESTS` reaches the game target today; `tests` holds the floor both ways at 260. `nonunity` diffed against 1c670bf, which differs from this branch's base only in docs: no `.cpp` changed (the one `Source/` file, `DTConstantsGen.h`, has no `.cpp`). The smoke step's T-PRE-07 config half read 14 settings as `04_Presentation §4` says. Output kept in `Saved\Logs\S1gm_gate_run.txt`.

## 5. Engine launches and the mask (A7, added by the scheduler)

`S1_pinned_rerun.md` saw each `UnrealEditor-Cmd.exe` started from a masked shell read `0xFFFFFFFF` soon after start, and again and again while it ran.

**The cause is inside the engine process.** UE's worker threads set their own affinity: `FRunnableThreadWin::SetThreadAffinity` (`Runtime/Core/Private/Windows/WindowsRunnableThread.cpp:122-150`) calls `SetThreadGroupAffinity` with the thread's mask ANDed with `FProcessorGroupDesc::ThreadAffinities`, which `QueryCpuInformation` fills from the system's `ActiveProcessorMask` of each group (`WindowsPlatformMisc.cpp:2601`), not from the process mask. On this Windows 11 build a thread affinity wider than the process mask is accepted, and the process mask then reads as all processors. Probe (scratchpad `thread_affinity_probe.py`, one Python process): process mask set to `FFFFFBF0`, then `SetThreadGroupAffinity(current thread, FFFFFBFE)` returns ok and `GetProcessAffinityMask` reads `FFFFFFFF`. Each new engine thread that sets its affinity does the same, which is the "again and again". No other process is needed to explain it, so the ASUS and Logitech services were not looked at further.

**The engine's own switch cannot carry the mask.** The only callers of `SetProcessAffinityMask` in `Engine/Source` are `FWindowsPlatformProcess::SetProcessAffinity` (`WindowsPlatformProcess.cpp:135-198`), reached only from `-processaffinity=<n>` / `-processaffinityphysical=<n>` (`LaunchWindows.cpp:223-232`). The switch takes a count, not a mask: it pins the lowest `n` logical processors (`(1 << n) - 1`), or every other one of the lowest `2n` for the physical form (`& 0x5555...`). Both always include processor 0, so no value expresses "all but 0 and 10". (With it set, thread affinities are disabled, so the mask would hold, but on the wrong processors.) So no hidden run with the switch was taken: by the code, any value puts the engine on processor 0.

**What would hold it (not built: a different design, so `blocked`).** A job object's affinity limit. Probe (`job_affinity_probe.py`): a job with `JOB_OBJECT_LIMIT_AFFINITY` = `FFFFFBF0`, the process assigned to it, then the same `SetThreadGroupAffinity(FFFFFBFE)` fails with error 31 and both the thread and the process stay at `FFFFFBF0`. `Tools/HiddenLaunch.ps1` already puts every engine launch in a job of its own; giving that job the helper's mask as an affinity limit (none when the helper returns `none`) is the smallest change. Expect a `LogThreadingWindows: Warning: ... SetThreadAffinity failed` line per engine thread that asks for a wider mask; the smoke, tests and contrast steps would have to accept it. Then the check in the smoke step (sample the game's mask every 0.2 s for its life, fail on any sample outside the mask) and a `tools_test.py` case for the job limit's construction. Smallest scope change: allow the job's affinity limit in `Tools/HiddenLaunch.ps1` and the sampling in `Tools/Smoke.ps1`.

**The compiler stayed inside the mask.** During the editor build through the masked `Tools/Build.bat` (36 s, incremental): `cl.exe` (PID 11280), the UBT host `dotnet.exe` and the `cmd.exe` of `Build.bat` all read `FFFFFBFE`. During the gate's own build, shipping and devgame steps the compiles were too short to catch a `cl.exe`/`link.exe` in a sample. UBT and the compiler do not set thread affinities, so nothing widens them; one sample per process kind, not a sample every 0.2 s.

## 6. Blocked

**T-PRE-07, two of its three parts; `PLANNED(S0)` as a lint error.** Built: the config half in `Tools/Smoke.ps1` (every setting `04_Presentation §4` lists as set in `Config/DefaultEngine.ini` holds the value it names; 14 settings; the table is the source). Not built:
- the arena post-process volume's settings: they live in `L_Arena_Test.umap`, which only the engine reads. A reader needs an editor Python script run as a commandlet (as `Tools/create_arena.py` is) or an automation test; neither file is in this brief.
- a captured burst frame with no exposure change: needs a second shot inside one run (or a calm and a burst shot) and a comparison of the frame's exposure, in `Tools/image_check.py` or `Tools/contrast_check.py`; neither is in this brief.
So the `PLANNED(S0)` marker stays on the T-PRE-07 line and `docs_lint.py` does not make `PLANNED(S0)` an error in this commit: with the marker present it would fail the gate. Smallest scope change: allow `Tools/render_settings_check.py` (new, editor Python, reads the volume of `L_Arena_Test`) and its call from `Tools/Smoke.ps1`, plus a burst mode in `Tools/Smoke.ps1` and an exposure comparison in `Tools/image_check.py`; then the marker goes and the lint rule lands with them.

## 7. What was removed

- `Gate.ps1`: the NOT RUN branch for a missing Python step script; the gate's pass with an unresolved `-Base`.
- `Perf.ps1`: the lock at `Saved\gpu.lock` (each worktree its own); the `res` column written from the flags.
- `11_Quality §1`: "A Python step whose script is missing also prints NOT RUN and the gate still passes"; "the floor only rises".
- `07_Technical §8`: the CPU row's "rerun an odd failure once" as the handling (now D-052), the lock row's "In the tool: none yet", the dropped-step row's "telemetry does not write the count" (D-054).
