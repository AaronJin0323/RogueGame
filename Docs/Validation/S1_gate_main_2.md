# S1 gate on main, second job: readings of `s1/gate-main-2`

For the scheduler. Class BUG on tools. Branch `s1/gate-main-2` from `main` at bd582b0. The mask file reads `FFFFF3FC` (every logical processor but 0, 1, 10 and 11); every run below was taken under it, applied by the Tools themselves.

## 1. Engine launches inside the mask (D-052)

**Change.** `Tools/HiddenLaunch.ps1` gives both launch jobs (editor, packaged game) the helper's mask as `JOB_OBJECT_LIMIT_AFFINITY` (`[DTHiddenDesktop]::Affinity`, flags built by `JobFlags`); with the helper's `none` the jobs carry no limit, as before. `Tools/Smoke.ps1` samples its game's process mask about every 0.2 s from launch to exit and fails on any sample outside the mask; the count is on the `SMOKE:` line (`mask FFFFF3FC, <n> sample(s), <k> outside`).

**Why the job and not the process mask.** `S1_gate_main.md §5`: the engine's worker threads ask for every processor (`FRunnableThreadWin::SetThreadAffinity`, the system's `ActiveProcessorMask`), and Windows 11 widens the process mask to match. A job's affinity limit refuses the request instead (error 31). Proof inside `tools_test.py` (`EngineMask.test_a_launched_thread_cannot_widen_the_process`): a child started by `Start-DTHidden` under a one-processor limit asks `SetThreadGroupAffinity` for the whole machine mask, is refused, and its process mask stays.

| Reading | Result |
|---|---|
| `Smoke.ps1 -Culture en` (by hand, before the gate) | `SMOKE: PASS`, `mask FFFFF3FC, 319 sample(s), 0 outside` |
| `RunTests.ps1 -Floor 260` (by hand) | `TESTS: PASS (260 run of 260 found, 0 failed, floor 260, 0 crash marker(s))` |
| `BotRuns.ps1 -Arms None -FirstSeed 9107 -Seeds 1` | `input_hash=fe30ec79`, death at 636.3167 s, 38181 steps, as `S1_pinned_rerun.md` (`wall_dropped_steps` 81) |
| The gate's smoke runs (en, zh-Hans, burst) | section 4 |

**What the engine logs when a thread is refused.** One line per thread that asks, at Warning: `LogThreadingWindows: Warning: Runnable thread <name> call to SetThreadAffinity failed with: 0x1f`. 35 lines in a smoke run (the `Foreground Worker` and `Background Worker` threads and the rest of the pool), 35 in the automation test run. No gate check counts them: no Tools script matches `Warning` in an engine log (`Smoke.ps1` judges crash, ensure, `LogDT` errors and reports other `Error:` lines unjudged; `RunTests.ps1` judges test results and crash markers; the `: warning` count of `Gate.ps1` reads compiler output only). The automation framework did not turn them into test failures (260 of 260 pass). Nothing was relaxed.

**Engine launches in `Tools/` that do not go through the job.**

| Script, line | Launch | Mask it runs under |
|---|---|---|
| `Tools/WindowCheck.ps1:78` | `[Diagnostics.Process]::Start($Info)`, the bypass control: a raw launch of `UnrealEditor.exe -game` on a stand-in desktop, on purpose | the process mask only, so the engine widens it |
| `Tools/Localize.bat:28` | `UnrealEditor-Cmd.exe ... -run=GatherText`, a commandlet run by the batch file | the process mask only |
| `Tools/CreateArena.bat:18` | `UnrealEditor-Cmd.exe ... -run=pythonscript`, a commandlet | the process mask only |
| `Tools/Package.ps1` | `RunUAT.bat BuildCookRun`, whose cook starts `UnrealEditor-Cmd.exe` | the process mask only |
| children of an editor in the job | the editor's job allows silent breakaway (a shared daemon must outlive the script), so `ShaderCompileWorker`, `zenserver` and `UnrealTraceServer` started by it leave the job | inherited process mask; whether they widen it was not read |

None was changed (outside this item's files, or on purpose for the control).

## 2. T-PRE-07

**Built, both halves that were missing.**
- The volume: `Tools/render_settings_check.py`, editor Python run as a headless commandlet (`UnrealEditor-Cmd -run=pythonscript`) by `Tools/Smoke.ps1 -Burst` through `Start-DTHidden`. It loads `L_Arena_Test`, finds its one unbound post-process volume and reads the bindings of `04_Presentation §4`: unbound; depth of field off (`depth_of_field_focal_distance` overridden to 0); exposure fixed (`auto_exposure_min_brightness` and `_max_brightness` overridden to `C.ARENA.EXPOSURE_EV100`); bloom threshold overridden to `C.ARENA.BLOOM_THRESHOLD`. It saves nothing. Reading: `RENDER: PASS (PostProcess_Presentation: 5 settings as 04_Presentation s4 says)`.
- The burst frame: `Tools/Smoke.ps1 -Burst -Name burst -Cmds "DT.Bullets 6000"` shoots the arena with a burst of fire at the smoke step; `Tools/image_check.py --exposure` compares it with the calm `arena_empty` shot of the same culture that the smoke step left. A change of exposure moves every pixel the same way, a burst moves only what it covers: the median over all pixels of the signed luma difference must stay within `C.SMOKE.GOLDEN_MEAN_DIFF_MAX` (12 levels, the tolerance a frame keeps against its reference; reused, not a new number). Reading by hand: luma stddev 36.51 against 11.72 calm (the burst is in the frame), exposure shift +1.28 levels. The measure's two cases are pinned in `tools_test.py` (`Pre07`): a frame with a third of it covered by white reads 0, a frame that darkens all over reads its shift. No in-engine negative control was run: no console variable found in the renderer's eye-adaptation code moves exposure past a volume whose minimum equals its maximum without a config change.
- The gate's `smoke` step runs the burst after the two cultures.

**Then the marker and the lint rule.** `PLANNED(S0)` left the T-PRE-07 line. `docs_lint.py` reports `PLANNED(<stage>)` outside a code span as an error for a stage in `PASSED_STAGES` (S0, D-012); other stages stay warnings. Case: `Pre07.test_planned_for_a_passed_stage_is_an_error`.

**Open, outside this job's files.** `04_Presentation §4`'s closing paragraph still says "T-PRE-07 is not built" (line 106); the brief allowed the T-PRE-07 line only. `C.SMOKE.GOLDEN_MEAN_DIFF_MAX` does not list `Tools/image_check.py --exposure` among its dependents in `constants.yaml` (a forbidden file); it already lists the smoke check.

## 3. The slow share (D-054)

`Tools/pressure.py` prints one line per run: `SLOW: <run> share <s> (<d> dropped of <n> steps)`, `run_end.wall_dropped_steps` over `steps` plus `wall_dropped_steps`. No threshold and no verdict.

| Run | Line |
|---|---|
| `Saved\BotRuns\s1gm2_seed9107\None_s9107\...` | `SLOW: ... not read (bot arm None at -DTBotFast; 81 dropped steps)` |
| a smoke run in `Saved\Playtests` | `SLOW: ... share 0.0000 (0 dropped of 600 steps)` |

**The telemetry holds nothing that tells a fast bot run from a person's session.** `run_start` has no bot or fast-step field, the bot's input reads `kbm` like a person's, and no record kind is bot-only (seed 9107's None run: `dash dir_block hero_damaged hero_down hero_healed hero_pos hit intercession keeper_commit kill level_up offer offer_pick run_end run_start sources weapon weapon_acquired xp`). `pressure.py` therefore uses the only mark there is, the one it already groups by: a run inside a BotRuns arm folder (`<arm>_s<seed>`) is a bot arm's, and BotRuns always passes `-DTBotFast`. A bot run copied out of its folder, or a tool run (the smoke run above), reads as a person's session. Field needed (`Source/` is not this job's): `run_start.bot` with the `-DTBotFast` steps per frame (0 or absent for a person), so a run says what drove it.

Cases: `SlowShare` (a session run's share beside its count; a bot arm's run "not read", never zero; a run without the field "not read").

## 4. Gate run (one run, start to end, no step rerun)

GATE_RUN_PLACEHOLDER

## 5. Blocked

Nothing in items 1 to 3. Open items are listed in their sections: the engine launches outside the job (section 1), the stale sentence of `04_Presentation §4` and the dependents row (section 2), the telemetry field (section 3).
