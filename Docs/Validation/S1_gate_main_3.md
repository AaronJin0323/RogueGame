# S1 gate on main, third job: readings of `s1/gate-main-3`

For the scheduler. Class BUG on tools. Branch `s1/gate-main-3` from `main` at 4a9f803. Mask `FFFFF3FC` (every logical processor but 0, 1, 10 and 11).

## 1. Does the job limit make the engine die at start?

**Launch form.** The seed step's "explicit 0" launch of `Tools/SeedCheck.ps1`, unchanged: `UnrealEditor.exe "<project>" -game -windowed -RenderOffScreen -nosplash -ResX=1280 -ResY=720 -LiveCoding=false -unattended -DTSeed=0 -userdir="<own folder>"`, through `Start-DTHidden`, read until `run_start` appears in the launch's own `Saved\Playtests` (or the process exits by itself, or 300 s), then stopped by PID. One fresh PowerShell per launch, so each has its own desktop and jobs.
- **limit**: as the Tools launch today, the editor's job carrying `FFFFF3FC` as `JOB_OBJECT_LIMIT_AFFINITY`.
- **plain**: the same launch with `[DTHiddenDesktop]::Affinity` set to 0 before the first start, so the job has no limit and the engine inherits the plain process mask `FFFFF3FC`. The machine's mask file was not touched.

Twelve blocks of 25, alternating limit, plain, limit, ...; at most four launches at a time, starts 1.2 s apart. Scratchpad scripts `one_launch.ps1` and `limit_driver.ps1`; each launch's folder, with its log, is `Saved\S1gm3_limit\<limit|plain>\<nnn>\` in this worktree (a `result.json` per launch).

| Condition | Launches | `run_start` read | `InheritedContext.cpp` assertion | Other crash or exit by itself | Crash folders | Start to `run_start`, s: median / p10 / p90 / max |
|---|---|---|---|---|---|---|
| limit | 150 | 150 | 0 | 0 | 0 | 7.2 / 6.2 / 8.8 / 38.9 |
| plain | 150 | 150 | 0 | 0 | 0 | 7.2 / 6.4 / 8.9 / 13.9 |

0 against 0 in 150 each says the rate under either condition is below about one in fifty at this launch form; it does not say the rate is zero, and it does not tell the two conditions apart. The one crash on record stays the one of `S1_gate_main_2.md §4`. The 38.9 s start under the limit is one launch; the next highest under the limit was within the plain range. The process mask read `FFFFF3FC` on every sample under both conditions before `run_start` (the widening of the plain condition comes later in a run, as `S1_pinned_rerun.md` saw).

The stop rule (two or more assertions under the limit, none without) was not met, so items 2 and 3 went on.

## 2. Commandlets and the cook inside the job

**Change, one mechanism.** `Tools/HiddenLaunch.ps1` gains `Hold-DTProcess` (`[DTHiddenDesktop]::HoldSelf`): the calling process joins a job made by the same `NewJob` as a launch, so it carries the same `JOB_OBJECT_LIMIT_AFFINITY` (none when the helper returns `none`); every process it starts from then on is born in that job. That job has no kill on close and allows no breakaway, so a daemon the work starts stays held and outlives the script.
- `Tools/Localize.bat` and `Tools/CreateArena.bat` already run again as a child of `Tools/Machine.ps1 -DTRunFile`; that mode now dot-sources `HiddenLaunch.ps1` and calls `Hold-DTProcess` before it runs the batch file, so the batch file, its commandlet and the commandlet's children are held. The batch files themselves did not change.
- `Tools/Package.ps1` calls `Hold-DTProcess` right after the mask, so UAT, UBT, the cook and everything they start are held.
- Cases in `tools_test.py`: a batch file with `Localize.bat`'s own head, run through the holder under a one-processor mask, starts a child whose thread asks for the whole machine: refused (error 31), process mask unchanged; `Package.ps1` holds itself before `RunUAT.bat`, `Machine.ps1` before it runs the file, and both batch files go through the holder.

**Readings.**
- `Localize.bat` by hand, its `GatherText` commandlet sampled about every 0.2 s for its life: 23 samples at `FFFFF3FC`, 2 reads of 0 as the process ended, none outside. In the last job's gate the same commandlet read `FFFFFFFF` on 10 of 10 samples. The gate's `locbuild` sampling is in section 4.
- `Package.ps1 -Configs Shipping`: the cook (`UnrealEditor-Cmd ... -run=Cook`, PID 23716) sampled for its life: 46 samples at `FFFFF3FC`, 1 read of 0 at its end, none outside. **Deviation:** the run was meant to be killed after a minute of cook; with a warm cook the whole package finished in 46 s (`PACKAGE: PASS (49b6e56+dirty, Shipping, 380 MB ...)`) before the kill. The package is from a dirty tree, so no gate judges it; it sits in this worktree's `Saved\Package\Shipping` and was not used for anything.

**Not held, by design.** Children that leave an editor launch's own job by silent breakaway (`Tools/HiddenLaunch.ps1`'s editor job keeps `SilentBreakawayOk` so a daemon other worktrees use outlives the script): `ShaderCompileWorker.exe` (shader compiles), `UnrealTraceServer.exe` (the trace server), `zenserver.exe` (the storage server). They keep the plain inherited mask; whether their threads widen it was not read. The bypass control of `Tools/WindowCheck.ps1:78` stays outside the job on purpose. Under `Hold-DTProcess` (commandlets, the cook) nothing can break away.

## 3. Two leftovers

- `04_Presentation §4`: "and T-PRE-07 is not built" becomes "T-PRE-07 reads this volume and a burst frame's exposure in the gate's `smoke` step".
- `C.SMOKE.GOLDEN_MEAN_DIFF_MAX` lists `Tools/image_check.py --exposure (T-PRE-07)` among its dependents in `constants.yaml` and in its `08_Constants` row. No value changed: `gen_constants.py` rewrote nothing and `--check` passes; no row in the number log.

## 4. Gate run

`& Tools\Gate.ps1 -Base main` at c992422 (`main` 4a9f803), mask `FFFFF3FC`, one run from start to end, no step rerun, no odd failure:

```
cpu:        CPU: PASS (28 processors, mask FFFFF3FC, 600 rounds)
tools:      TOOLS: PASS (122 tests)
locbuild:   LOCALIZE: PASS (Content\Localization\Game\<culture>\Game.locres)
devgame:    rc 0, 0 warning(s)
tests:      261 run of 261 found, 0 failed, floor 261, 0 crash marker(s)
smoke:      SMOKE: PASS (arena_empty_en, ..., mask FFFFF3FC, 84 sample(s), 0 outside)
smoke:      SMOKE: PASS (arena_empty_zh-Hans, ..., mask FFFFF3FC, 84 sample(s), 0 outside)
smoke:      SMOKE: PASS (burst_en, ..., RENDER: PASS (...), image PASS (... exposure shift +1.72 levels ...), mask FFFFF3FC, 84 sample(s), 0 outside)
seed:       PASS (4 human launches on fresh seeds, 4 exact, 2 rejected)
windows:    PASS (0 shown by bot, smoke and test runs across 97 processes; probe caught; bypass caught; 4 lost; 38 s)
pad:        NOT RUN (package 49b6e56 came from a dirty tree)
GATE: PASS (c992422; 3 not run; mask FFFFF3FC)
```

Every other step passed (full output in `Saved\Logs\S1gm3_gate_run.txt`). The `pad` step now names the package of section 2 instead of "no package"; it still reads NOT RUN.

**`locbuild` sampled from outside the gate** (scratchpad `sample_cmdlet.ps1`, about every 0.2 s): the `GatherText` commandlet (PID 49004) read `FFFFF3FC` on 23 samples and 0 on 2 as it ended; none outside the mask. The last job's gate read the same step at `FFFFFFFF`.