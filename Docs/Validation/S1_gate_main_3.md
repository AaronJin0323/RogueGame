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
