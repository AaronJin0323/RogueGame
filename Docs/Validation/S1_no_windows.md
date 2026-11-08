# S1 no windows

**Verdict: BUG fixed on `fix/no-windows`. Every tool launch of an engine binary now runs on a hidden Win32 desktop with no console window. A new gate step reads the real desktop for any window of the launched tree, with two controls, and the launch guard fails on a new launch past the helper. `input_hash` on seed 9107 is unchanged on 5 of 5 arms, images are within limits, and no tool wrote to `%LOCALAPPDATA%\Dusktracer` during the checks read here. One thing went wrong while building it: a window reached the owner's desktop, see below.**

Class BUG, tool layer. It replaces the desktop class inside `SeedCheck.ps1`, `Start-Process` in `BotRuns`, `Smoke`, `Perf` and `PadCheck`, the direct call in the gate's `tests` step, the sampled window loop in `PadCheck`, and `PadCheck`'s run files in the owner's folder. All of those are gone. R-TEC-17 and T-TEC-17 are new. R-PKG-05, 06, 07, 08, T-PKG-05 and T-PKG-07 are rewritten. `Docs/STATE.md` line 9 still says `-nullrhi` or `-RenderOffScreen`; a track does not edit that file, so it changes at integration.

## What really shows a window
All five launches below ran past the helper, on a hidden desktop, with the hook on that desktop. A hook on the real desktop saw 0 windows while they ran. Bot `None`, 6 s of sim at real time, so each stayed up through its first frames. The last is the launch the bypass control uses.

| Launch | Windows shown | `GameWindow did not exist` in the log |
|---|---|---|
| `UnrealEditor.exe -game -nullrhi -unattended` | 0 | yes |
| `UnrealEditor-Cmd.exe -game -nullrhi -unattended` | 0 | yes |
| `UnrealEditor.exe -game -nullrhi`, no `-unattended` | 0 | yes |
| `UnrealEditor.exe -game -windowed -RenderOffScreen` | 0 | no |
| `UnrealEditor.exe -game -windowed`, real RHI | 3: `UnrealWindow`, 2 UAC input indicator windows | no |

So the `GameWindow` line is not evidence of a native window. It is logged for the Slate `SWindow`, and `FSlateApplication::MakeWindow` skips `InitializeWindow` when `FApp::CanEverRender()` is false, which `-nullrhi` makes it (`SlateApplication.cpp:2043`). The brief's reading, that `-nullrhi` creates a real game window, rests on that line and one sample. I could not reproduce the sample: 3 launches, hook on the desktop, 0 events. That is 3 launches of one build on one machine, and the sample that was seen is still unexplained. The auto-memory note on this rule names Unreal's splash as a window that was on screen in runs with no `-nosplash`, which would fit; that was not measured here.

The rule does not depend on which reading is right: no flag is the rule, the helper is.

## The window that reached the owner's desktop
The first bypass control was `Start-Process UnrealEditor-Cmd.exe` from the stand-in hidden desktop, with no `-WindowStyle Hidden`. `UnrealEditor-Cmd.exe` is a console binary, so it got a new console, and Windows handed that console to the default terminal. Windows Terminal is started outside the launch and opens on the real desktop whatever desktop the launch was on.

The step's hook on the real desktop recorded it: `UnrealEditor-Cmd.exe class PseudoConsoleWindow` in the tree and `WindowsTerminal.exe class CASCADIA_HOSTING_WINDOW_CLASS` outside it. No `WindowsTerminal` process was left two minutes later, and no later run showed one. The launch lived 7 s (13:53:16 to 13:53:23 local, 2026-11-01), so a terminal window was most likely on the owner's screen for about that long. That is a breach of the rule by this work, and it is the only one the hook saw.

Two things changed for it. The helper sets `CREATE_NO_WINDOW` on every launch, so a console binary gets a console with no window and nothing to hand over. The bypass control now starts `UnrealEditor.exe`, a GUI binary, by a raw `CreateProcess`, so it has no console at all.

It also shows a limit of the hidden desktop: it stops windows, not a console handed to another process. And it may be part of what the owner saw. Whether `-WindowStyle Hidden` on `UnrealEditor-Cmd.exe`, as `BotRuns` ran, avoids the hand-over was not measured, because the test would open a window.

## Proof it holds
Gate step `windows` (`Tools/WindowCheck.ps1`), in the full gate at `d79d8e3`: `PASS (0 shown by bot, smoke and test runs across 69 processes; probe caught; bypass caught; 4 lost; 47 s)`. The slowest of 7 runs was 85 s, with other worktrees busy. Run alone with the bypass control's `-userdir`: the same reading in 41 s.

| Reading | Result |
|---|---|
| Bot run through `BotRuns.ps1` | PASS, 0 windows |
| Smoke run through `Smoke.ps1` | PASS, 0 windows |
| Test run through `RunTests.ps1` (`Dusktracer.Tec.RunSeed`) | 4 of 4 PASS, 0 windows |
| Probe on the real desktop | caught, in the tree |
| Bypass on the stand-in desktop | caught, `UnrealWindow`, 3 windows, reads `WINDOWS: FAIL` |
| Game or editor left running | 0; one `CrashReportClientEditor.exe` still ending, logged |

The probe is a process started past the helper that shows a 1 x 1 window, alpha 0, off every screen, no focus, no taskbar button, for 0.2 s. It runs on the real desktop in every gate, so the step fails if the hook goes blind. It is a window object no one can see, and it is the only thing this step puts on that desktop.

The bypass control runs in every gate too. A real window cannot be tried on the owner's desktop, so `WindowCheck.ps1 -Bypass` runs on a hidden desktop as its own stand-in, where the hook and the raw launch inherit that desktop. It refuses to run on `Default`, and the refusal was read. It proves a bypass fails the step; the probe proves the hook is alive on the real desktop. No single control proves both at once, by the rule's own terms. It has a `-userdir` of its own, `Saved\WindowCheck\bypass`, emptied each step, for two reasons: the telemetry race below, and its 320x180 video mode, which `UGameEngine::PreExit` saved to `Saved\Config\WindowsEditor\GameUserSettings.ini` on every gate. A person's `-windowed` launch with no size would then open at that size; that effect is read from the engine, not run. Read after the change: the bot run's file in `Saved\Playtests`, the control's in its own folder, 320x180 only in that folder's settings file.

Also watched from the real desktop with no window of the tree: `Package.ps1` end to end, 190 processes (text, cook, stage, UAT), and `PadCheck.ps1`, where the hook replaces the sampled loop for T-PKG-07.

Helper behaviour read on real runs: exit codes held (a harmless `cmd /c exit 7` reads 7, and the seed step reads exit code 2 on a rejected seed); a process dead on arrival keeps its code, because it starts suspended until the handle is held; `BotRuns -Jobs` with 4 probe scenes in parallel PASS; `BotRuns -Timeout 5` killed both runs by PID, reported them and left 0 strays; `Close-DTHidden` ends what is left, read on a harmless `ping`. Commandlets, `Tools\Localize.bat` and the cook: 0 `GameWindow` lines in `Localize.log` and in `Package_Shipping.log`, which carries the cook's log, and 0 windows in the watched `Package.ps1`. They stay as they are. `Tools\CreateArena.bat` is the same `-run=` shape and was not run, because it rewrites content.

Two faults found in review were read on harmless stand-ins, a scratch executable in place of the engine binary, each before and after its fix:

| Stand-in | Before | After |
|---|---|---|
| `UnrealEditor-Cmd.exe` that logs every test complete, then hangs; `RunTests.ps1 -Timeout 3` | `! killed after 3 s`, then `TESTS: PASS`, exit 0 | `TESTS: FAIL (1 run of 1 found, ..., killed after 3 s)`, exit 1 |
| `Dusktracer.exe` with a child, as the packaged stub has; script killed by `Stop-Process -Force` | child still running | child gone |
| The same executable named anything else | child still running | child still running, so a shared daemon lives |

## Detection limit
The hook is `SetWinEventHook` on `EVENT_OBJECT_SHOW`, out of context, so an event queues however short the flash is. The tree is a job with this script in it: a process joins at birth and a job message names it even after it exits.
- It reads one desktop, the script's. A window from outside the tree is counted, not judged, by image and class; never a title, because those windows are the owner's. So a window the OS shows for the tree (WerFault, a UAC prompt) or a terminal started by COM, as above, is not a failure. The step's log lists them. The `PseudoConsoleWindow` above was judged, because conhost names the client as owner.
- A window given `WS_VISIBLE` by `SetWindowLong` fires no event.
- A window whose thread is gone or closed to this process when the event is read counts as lost, not shown. A step reads 4 lost: one `csrss.exe` window as each child PowerShell starts (thread read by `Win32_Thread`), which no tree owns. A tree window would have to be destroyed, with its thread, before the event was read.
- A job message is not guaranteed. A window's process is also asked `IsProcessInJob` when the event is read.

## Nothing else moved
`input_hash` on seed 9107, on the hidden desktop, on the build this branch started from:

| Arm | Expected | Read, serial | Read, 5 at once |
|---|---|---|---|
| None | 1c738e97 | 1c738e97 | 1c738e97 |
| Random | 2282c72a | 2282c72a | 2282c72a |
| Steered:Storm:Tempest | e96dc0a8 | e96dc0a8 | e96dc0a8 |
| Steered:Forge:Hammer | 9385c307 | 9385c307 | 9385c307 |
| Steered:Tide:Frost | 690cb5a3 | 690cb5a3 | 690cb5a3 |

The parallel run read `BOTRUNS: FAIL` for a reason that is not this change: two arms on one seed reached `Run start` in the same second under load, shared `9107_20261101_141024.csv`, and `bot_summary.py` met a second header. The hashes come from the bot record, which is per run. `BotRuns.ps1`'s line that starts are spaced so no two telemetry names collide holds only for distinct seeds. It is left as it is here. The first form of the `windows` step had the same race: the bypass control and the bot run start about 2 s apart on seed 9107, and names only reach the second, so under load they could append to one file and fail the gate with no code change. A doubled copy of a real run fails `bot_summary.py` the same way, exit 1 at `pressure.py`; the same start in one second was not forced. The control's own `-userdir` ends it.

Images, from the gate at `d79d8e3`, on the hidden desktop:

| Image | Reading | Limit |
|---|---|---|
| `arena_empty_en` | mean diff vs golden 0.24, luma stddev 12.42 | 12.0 |
| `arena_empty_zh-Hans` | mean diff vs golden 0.25, luma stddev 12.26 | 12.0 |
| `package_en`, Development | mean diff vs golden 0.26, luma stddev 12.44 | 12.0 |
| `package_zh-Hans`, Development | mean diff vs golden 0.25, luma stddev 12.27 | 12.0 |
| contrast | 7/7 elements at or above 3.0 | 3.0 |

Tests: 251 run of 251 found, 0 failed, on the hidden desktop. Seed: 4 human launches on fresh seeds, 4 exact, 2 rejected.

`Perf.ps1` on one build, with no cost seen from the hidden desktop. Default is `main`'s script, the old `Start-Process`, in the same worktree; hidden is this branch's. The hook read 0 windows of the tree on all 8 turns. The readings are in ms, 1920x1080, at caps. Other worktrees were using the GPU throughout, so the first four are the comparison and the last four show the noise. "Others" is the count of other engine processes before and after the turn.

| Turn | Desktop | Others | Game p50 / p95 | GPU p50 / p95 | Verdict |
|---|---|---|---|---|---|
| 1 | default | 1, 6 | 2.19 / 2.99 | 5.04 / 5.67 | PASS |
| 2 | hidden | 6, 1 | 2.11 / 2.84 | 2.10 / 5.75 | PASS |
| 3 | default | 1, 1 | 2.09 / 2.67 | 2.05 / 5.11 | PASS |
| 4 | hidden | 1, 6 | 2.13 / 2.74 | 2.04 / 2.10 | PASS |
| 5 | default | 7, 7 | 2.77 / 3.81 | 5.83 / 15.94 | FAIL, GPU |
| 6 | hidden | 7, 7 | 2.75 / 4.26 | 2.20 / 8.36 | FAIL, GPU |
| 7 | default | 7, 6 | 2.57 / 3.61 | 2.23 / 11.59 | FAIL, GPU |
| 8 | hidden | 6, 6 | 2.81 / 3.76 | 4.15 / 14.24 | FAIL, GPU |

The game thread agrees within 0.1 ms at the lower load. GPU p95 moves by more between two turns on one desktop than between the desktops. A reading on an idle machine was not possible while other worktrees ran, so this bounds the cost and does not measure it: no fixed reading for the budget should come from these. The FAIL turns are contention on both sides.

## %LOCALAPPDATA%\Dusktracer
A listing of the folder with size, write time and SHA-256 of every file, taken before and after the full gate at `d79d8e3`, whose `pad` step ran the whole of `PadCheck.ps1` on a clean package: 477 entries, 307 files, 0 lines differ.

| Listing | When (local) | Entries | Files |
|---|---|---|---|
| Before | 14:23:22 | 477 | 307 |
| After | 14:32:51 | 477 | 307 |

An earlier listing, at 13:44, differs from these, and not by this branch: `770077_*` run files were removed and `GameUserSettings.ini` and the pipeline cache were rewritten, at 13:43 to 13:45. That is `PadCheck` on another worktree, on the old path. It is also what this rule stops: the crash reporter, cache and settings of a tool launch landing in the owner's folder, and two worktrees sharing one run file. The latest write in the folder stayed at 13:45:13 through two full `PadCheck` runs here, at about 14:17 and 14:30.

## Decisions
1. `Start-DTHidden` returns a real `Process`, so `$Proc.HasExited`, `.Id`, `.WaitForExit` and `.ExitCode` work as they did after `Start-Process -PassThru`. Merging a script is a one-token swap.
2. Two jobs per script, on top of the desktop: a stray on a hidden desktop has no taskbar button, so nobody would find it. For the editor only the launched process is in the job; its children leave silently, because killing a shared `zenserver` would hurt another worktree's editor. For a packaged game, `Dusktracer*.exe`, the whole tree stays: `Dusktracer.exe` is a stub whose real game is its child, which `SeedCheck -Package` starts, and the package ships no daemon (only the game and the VC redistributables are executables under `Saved\Package`). `Start-DTHidden` picks by image name, so the merge stays a one-token swap.
3. A daemon a launch starts inherits the desktop and keeps the empty object alive until it ends. One desktop was seen after its script had gone; it was gone on the next look, after `UnrealTraceServer` and `zenserver` had restarted, which fits, though the holder was not read. Nothing is shown there, so nothing fights it.
4. `CREATE_NO_WINDOW` on every launch, for the reason above; a GUI binary ignores it.
5. `Tools/RunTests.ps1` takes the launch out of the gate, so the `tests` step and the `windows` step run the same launch, with a timeout it never had, and a person or agent has a safe way to run a filter. A run killed on that timeout fails, whatever its log counts.
6. `Tools/WindowWatch.ps1` is separate from the helper, because only checks need it.
7. `ExportTelemetry.ps1 -UserDir`: the check exports its own runs and no longer reads the owner's. The package's own scripts now count in the stale check, so an older package reads `NOT RUN`, not a false FAIL.
8. The `pad` human half still reads `%LOCALAPPDATA%\Dusktracer\Saved\Playtests`, read only: the pad run it looks for is a person's.
9. The rule stays with the launch guard in `tools_test.py`, by text. It catches `Start-Process`, the other process APIs, `&` or `.` on a binary or on a variable holding one (`${Exe}`, `$script:Exe`, an assignment over two lines), a binary called by bare name or path, and `cmd`, `powershell`, `pwsh` or `start` given one. A launch hidden behind a function or alias it cannot follow, in a member or hashtable value, or in a `.bat` called by name, is not caught.
10. `// Run headless:` comments in about 20 test files still name a raw `UnrealEditor-Cmd.exe` line. They are left: editing them would touch `Source`, for no behaviour, and put them all through `nonunity`. AGENTS.md is what an agent reads first, and `RunTests.ps1` is the way.
11. The bypass control takes a `-userdir`, not a second seed: a seed alone would still leave its video mode in the checkout's settings file.

## For the merge into dev/s2 and dev/s3
From reading both branches; none of it was run. The swap is `Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru [-WindowStyle Hidden]` to `Start-DTHidden $Exe $ArgList`, the dot-source line after `$UERoot`, and `& taskkill /PID $Proc.Id /T /F | Out-Null` to `Stop-DTHidden $Proc`. `tools_test.py` fails until each is done: run over their scripts as they stand, the guard reads 10 lines on `dev/s2` and 17 on `dev/s3`, and 8 on `main` before this change, which is the list below.

| Script | On | Needs |
|---|---|---|
| `BotRuns`, `Smoke`, `Perf`, `PadCheck`, `SeedCheck`, `Gate` | both | as here; take this side, keep their extra arguments. `S2Exit` launches through `BotRuns`: nothing |
| `RestartCheck` | both | the swap |
| `PackageRestartCheck` | both | the swap, plus a `-userdir` per config: it runs Shipping into `%LOCALAPPDATA%` and `Remove-Runs` deletes there |
| `MetaCheck`, `WinCheck` | s3 | the swap |
| `VigilCheck` | s3 | the swap, twice (`$Exe` and `$CmdExe`) |
| `ExportLogsCheck` | s3 | the swap for the game, plus a `-userdir`: `$ShipRuns` and `Remove-Runs` are in the owner's folder. `Start-Process cmd.exe` with redirected stdin and stdout, line 92, runs `Export_Logs.bat` and no engine binary: add it to `LAUNCH_EXCEPTIONS` |
| `PackageHubCheck` | s3 | the swap; it already names `-userdir`. Drop `$null = $Proc.Handle`. `[DTHubWindows]::Visible` samples the real desktop and reads nothing now: use `Start-DTWindowWatch` |
| `PadCheck` | s3 | its `$Running` folder under `%LOCALAPPDATA%\Dusktracer\PadCheck`, `Get-MarkerSeed` by PID and `Remove-MarkerRuns` all exist to survive the shared folder. The `-userdir` removes the need: drop them |
| `Export_Logs.ps1` | s3 | the same `-UserDir` as `ExportTelemetry.ps1` here, which it replaces. It must stay ASCII |
| `Perf -Package` | s3 | a `-userdir`, and `$CsvDir` follows it |
| `Gate` `tests` step, `windows` step | both | same step on both; put `windows` after their last launch step |

Done on `integrate/w3-s2` and `integrate/w3-s3`, and `EarnedCheck` too, which arrived after this list (the swap). Where the list was not followed: `ExportLogsCheck` points `%LOCALAPPDATA%` at its own `account` folder for the export's `cmd.exe` alone, as `dev/s3-kit`'s `PadCheck` already did, since `Export_Logs.ps1` reads that variable; the copy's Development launch names its own `Dusktracer` folder as `-userdir`, where it wrote anyway, so the raw log still carries the user name. `PadCheck` is `dev/s3-kit`'s, which already dropped `$Running`, `Get-MarkerSeed` and `Remove-MarkerRuns` and runs on a `-userdir`; it takes the swap, `WindowWatch` in place of its sampler, and `Tools/PackageFiles` in its stale check (R-PKG-08). `PackageHubCheck` takes `WindowWatch` in place of `[DTHubWindows]::Visible`. None of the package checks was run: no package was built on either branch.
