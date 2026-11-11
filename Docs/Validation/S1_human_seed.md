# S1 human seed

**Verdict: BUG fixed on `fix/human-seed`. Every human launch now draws a fresh, in-range, positive seed in one place, tools and explicit seeds stay exact, and an out-of-range `-DTSeed` is rejected. `input_hash` on seed 9107 is unchanged on 10 of 10 arms and lengths. No balance number moved.**

Class BUG, launch layer. It replaces the launcher's `-DTSeed=%RANDOM%%RANDOM%` and the saturating `FParse::Value` read; both are gone. R-PKG-04 is rewritten, R-TEC-16 is new, T-PKG-04 is rewritten and T-TEC-16 is new.

## The bug
- `Tools/PackageFiles/Play_*.bat` joined two `%RANDOM%` values into a number that often overflows `int32`. `FParse::Value` reads through `Atoi`, which saturates, so about 24 percent of launches ran on `2147483647`. `%RANDOM%` follows the clock, so about 69.5 percent did inside a 57.6 minute window every 167 minutes.
- `Tools\Play.bat` and a bare exe launch passed nothing and ran on seed `0`, every time. No rule covered that path.
- `Tools/PadCheck.ps1` matched a fragment of the launcher's text, so the gate could see neither fault.

Evidence: `%LOCALAPPDATA%\Dusktracer\Saved\Playtests\2147483647_20261031_231306.jsonl` and `.csv`: seed `2147483647`, build `996cd47`, `zh-Hans`, death at 563.3 s. One designer run, not a validator session. It stays a valid replay, and nothing here reads balance from it.

## How seed 0 was used before the default changed

| Reader | What it does with 0 | After |
|---|---|---|
| `UDTSimSubsystem::OnWorldBeginPlay` | placeholder run before the game mode starts the real one | unchanged |
| `FDTRngStreams` | an ordinary seed, no sentinel | unchanged |
| `Tools/Smoke.ps1`, gate `contrast` | pass no seed, with `-unattended`; the goldens are read on 0 | still 0: smoke `en` PASS on a `0_...` run file |
| `Tools/Perf.ps1` | passes no seed, with `-unattended` | still 0 by the rule; not run here |
| Gate `tests` | `-unattended`; test worlds pass their own seeds, none uses `ADTGameMode` | unchanged |
| `Tools/BotRuns.ps1`, `Tools/PadCheck.ps1`, `Tools/probe.py` | explicit seeds | exact |
| Python tools | read seeds from names and labels, no sentinel | unchanged |

Every tool launch on `main` passes `-unattended`, and so do the launch scripts checked on `dev/s3` (`ExportLogsCheck`, `PackageRestartCheck`, `Perf`). That is why the rule keeps 0 for tools rather than asking each tool to name it.

## Seed 9107
Same five arms, same command, before and after: `BotRuns -FirstSeed 9107 -Seeds 1 -Parallel 5`, at `-MaxSeconds 90` and at full length. Before is `996cd47`, built clean. After is `6a96eaa`, built clean from the commit, and all 10 run files carry that stamp. Raw output: `Saved/BotRuns/seed9107_base_*` and `seed9107_commit_*`. An earlier batch on the uncommitted tree, `seed9107_after_*`, read the same hashes.

| Length | Arm | Outcome | End s | Steps | `input_hash` before | After |
|---|---|---|---|---|---|---|
| 90 s | None | timeout | 90.0000 | 5400 | `0a3d4334` | `0a3d4334` |
| 90 s | Random | timeout | 90.0000 | 5400 | `3cf08150` | `3cf08150` |
| 90 s | Storm Tempest | timeout | 90.0000 | 5400 | `ed8a2e93` | `ed8a2e93` |
| 90 s | Forge Hammer | timeout | 90.0000 | 5400 | `b7678928` | `b7678928` |
| 90 s | Tide Frost | timeout | 90.0000 | 5400 | `7acb9893` | `7acb9893` |
| full | None | death | 675.4500 | 40529 | `1c738e97` | `1c738e97` |
| full | Random | victory | 987.2667 | 59238 | `2282c72a` | `2282c72a` |
| full | Storm Tempest | victory | 967.9000 | 58076 | `e96dc0a8` | `e96dc0a8` |
| full | Forge Hammer | victory | 981.3833 | 58885 | `9385c307` | `9385c307` |
| full | Tide Frost | death | 435.4500 | 26129 | `690cb5a3` | `690cb5a3` |

Press counts, picks and declines match too. Telemetry is a side reading on 6 of the 10: those match in CSV and in JSONL apart from `wall` fields and the build stamp. The other 4 cannot be compared: on the base full-length batch, two pairs of same-seed parallel runs started in the same second and shared a telemetry file name. Hashes are not touched by that, because each run writes its own `-DTBotOut` record.

`Steps` is the one field that is not exact. The `6a96eaa` batch read 26128 on Tide Frost, full length, where the table has 26129, and eight reruns of that arm on the same binary, four serial and four parallel, all read 26129. The record takes the step count after the run is over: the bot stops stepping at the end, but `UDTSimSubsystem::Tick` keeps stepping by the wall clock until telemetry closes, so the count holds a tail of 1 or 2 steps past `End s`. `End s` and `input_hash` do not include that tail, and neither moved.

## What the gate now proves
`Tools/SeedCheck.ps1` (gate step `seed`) runs each launcher with its `start` line echoing, so it judges what `cmd` really expands, then starts the game with exactly those arguments, `-fullscreen` swapped for off-screen flags and no `-unattended` (since review #43 no launcher names a window mode, so the check only adds the off-screen flags and fails a launcher that names one), and reads `run_start.seed` from the run file. Each launch has a `-userdir` of its own, so the file it reads can only be that launch's.

| Launch | Reading |
|---|---|
| `Tools\Play.bat`, both `Play_*.bat`, bare launch | a fresh seed each, in range, all different |
| `-DTSeed=9107`, no `-unattended` | exact |
| `-DTSeed=0`, `-DTSeed=2147483647`, tool with no seed | exact, 0 for the tool |
| `-DTSeed=2147483648`, `-DTSeed=3276732767` | rejected: exit code 2, no run file |

The package half ran on package `6a96eaa` through `Tools/SeedCheck.ps1 -Package`, on the real Shipping build: `Play_en.bat`, `Play_zh-Hans.bat` and a bare `Dusktracer.exe` each read a fresh seed, `-DTSeed=9107` is exact with and without `-unattended`, and `-DTSeed=3276732767` exits with code 2 and no run file. `pad` still reads NOT RUN, because a real pad needs a human.

That run was also the control for sharing the run folder, because another process could otherwise have had its file read and deleted as the check's own. Four Shipping decoys on seed `770078` wrote to the real `%LOCALAPPDATA%\Dusktracer\Saved\Playtests` during it. The check took 7 s, so only the first two overlapped it; the other two started after it ended. The check still read only its own seeds. All four decoys kept their files, each with `run_start` first and `run_end` last. After the decoys were removed by name, the other 142 files matched their SHA-256 from before the run. The two decoys that did not overlap prove nothing about the check.

The two controls show it fails on the bug itself:

| Control | Reading |
|---|---|
| The old launcher line back in `Play_en.bat` | FAIL: `Play_en.bat names a seed or left a variable: -DTSeed=2055317294` |
| Game defaults a human to 0, as `Tools\Play.bat` did | FAIL on all four human launches: `seed 0 is not in 1..2147483646` |

## Decisions
1. The game draws the seed, not the launcher, so `Tools\Play.bat`, the package and a bare exe share one path.
2. A tool is `FApp::IsUnattended()`: `-unattended` or an automation test. `-nullrhi` is not part of it, so every clause of the rule is proven live.
3. Reject, not wrap. A wrapped seed is a number nobody asked for, written into a run file as if they had. The exit is forced because a clean exit returns 0 on Windows; no run has started, so nothing is lost. Play in the editor stays open.
4. The read is strict: `12abc` used to run as 12 and is now rejected. So are `12,34` and `12)`: only whitespace ends the value, because by default `FParse::Value` also stops at `,` and `)`. The whole `int32` range stays valid, negatives included, as before.
5. A fresh seed is `1` to `2147483646`. `2147483647` is never drawn, so in a run file's name it means a replay or the old launcher, and it stays valid so the owner's run can be replayed.
6. The seed is decided once per world on `UDTSimSubsystem`, so the game mode and the bot read one number and Play in the editor draws again. `ADTGameMode::GetCommandLineSeed` is removed.
7. The rule is R-TEC-16, not in `package.md`: it covers launches outside the package, and R-PKG-10 is taken on `dev/s2`.
8. `SeedCheck` runs every launch on a hidden Win32 desktop and never uses `-nullrhi`, for the reason below.
9. `SeedCheck` gives every launch a private `-userdir` instead of working out which file in a shared folder is its own. A Shipping launch with no seed has no log and no marker to match, so no test on a shared folder can be sure. With a private folder the check never looks in `%LOCALAPPDATA%\Dusktracer` or `Saved\Playtests`, so it cannot read or delete a person's run, and its killed runs never reach `ExportTelemetry.ps1`. The last check's folders stay under `Saved\SeedCheck`, with each launch's log, until the next check.

## Windows opened while building the check
The first versions of `SeedCheck` showed windows on the owner's desktop, against the no-window rule:
- A dialog titled `Message`, for at least 21 s (42 samples, each 0.5 s or more apart). The script put its flags before the project path, and the editor needs the project first (`LaunchEngineLoop.cpp`). A second launch with the same order may have shown it too before it was stopped.
- A real game window for a moment on `UnrealEditor.exe -game -nullrhi`. The reading then was that `-nullrhi` drops the renderer, not the window, from `LogEngine: GameWindow did not exist. Was created`; `S1_no_windows.md` corrects it, see below. One was sampled visible and its log shows that line. The logs of the other nine have rotated away; they took the same path, so all ten are counted as having opened one.
- One visible-window sample on the first launch with no `-unattended`, cause not proven. On the hidden desktop the same launches showed a blank-titled `UAC Input Indicator` window owned by a short-lived process, which is the likely match. With `-LiveCoding=false` on those launches, the 10 editor launches of the final check showed no window at all there and logged no `LogLiveCoding` line, so the Live Coding console is the likely owner. No launch with Live Coding on was repeated to prove it, because that session's mutex is shared by every worktree.

The project path is now required first, launches run on the hidden desktop, and unattended launches go first, so a stale build fails before any launch that could prompt. The 11 logs kept from the final runs are all `-RenderOffScreen`, with and without `-unattended`, and none created a game window.

`Tools/BotRuns.ps1` was not changed. All 20 of its run logs here show the same `GameWindow` line. It starts with `-WindowStyle Hidden`; whether that window can be seen was not measured. `S1_no_windows.md` measured it: under `-nullrhi` that line comes with no native window, so the line is not evidence of one, and the reading above needs that correction.

## For the merge into dev/s2 and dev/s3
None of this is reachable on `main`, and none of it was run: it comes from reading both branches. Their launchers carry the old line, which this replaces. `Tools\Play.bat` on both passes `%CULTUREARG%`; `SeedCheck` reads the expanded line, so that needs no change.

Both branches need three changes by hand in the restart path:
- `ADTGameMode::GetCommandLineSeed` is gone here and is still called there. `ParseSeed` must fall back to `UDTSimSubsystem::GetLaunchSeed`, or a launch with no seed runs on 0 again. The gate's `seed` step fails if it does.
- `ParseSeed` reads `-DTSeed` through `FParse::Value` and the map's option through `LexFromString`. Both should go through `FDTRunSeed::ParseValue`.
- `NextSeed` is a second draw; it should use `FDTRunSeed::FreshFromEntropy`, so that one place holds.

`dev/s3` needs four more, for the hub. The three above do not get it to build, and `SeedCheck` cannot pass there as written:
- `UDTHubSubsystem::BeginRun` calls the removed `GetCommandLineSeed` (`DTHubSubsystem.cpp:69`), so the merge stops there at compile. Line 70 also treats 0 as no seed, against the explicit `0` of R-TEC-16 and R-VIG-07. It should take `UDTSimSubsystem::GetLaunchSeed`, which already means "`-DTSeed` when given, else fresh".
- `ADTGameMode::BeginPlay` returns into the hub before any seed is read, so `-DTSeed=abc` would sit in the hub instead of exiting with code 2. The rejection has to come first.
- `WantsHub` looks only at the map's option and ignores a command-line `-DTSeed`, so every launch with no `-unattended` stops in the hub: no `StartRun`, no telemetry session, no run file. The four fresh launches and the replay would each wait out the 900 s timeout, and the gate's `seed` step would fail after about 75 minutes. A run there starts only at Begin, and on an empty profile Begin opens the Shrine first (R-VIG-07). `SeedCheck` needs a way through that before this merge. `-DTHubCheck` is the existing way, but it is compiled out of Shipping, so T-PKG-04's fresh-seed reading on Shipping needs a decision at that merge.
- Two rules now say what a tool run is. `UDTMetaSubsystem::IsToolRun` (R-MET-14) also counts `-DTBot*` and `-DTNoSave`; `FDTRunSeed::IsToolRun` does not. A `-DTNoSave` launch with no seed and no `-unattended` would skip the hub and the save as a tool, yet draw a fresh seed as a person. Recommended: keep the two apart and say so in R-MET-14, because a launch that falls back to seed 0 unasked is the bug this branch removes.

All of this section landed at integration: `D-025` for the restart path, `D-028` for the hub, including the decision on Shipping.

A launch with no `-unattended` is not a tool run under R-SET-05 or R-MET-14, so `SeedCheck`'s launches do read `DTSettings.ini` on `dev/s2` and `DTMeta.sav` on `dev/s3`. Both live under `FPaths::ProjectSavedDir()`, which `-userdir` moves, so each launch sees an empty folder of its own and default settings: never a person's files, culture or display settings.

## Found on the way, not fixed here
`Tools/PadCheck.ps1` uses one fixed marker seed and deletes `<marker>_*` from `%LOCALAPPDATA%`, a folder every worktree shares. Two checks at once delete each other's run files. A file removed after `run_start` is flushed comes back at the first interval without it, which reads `first record is not run_start`, and the record count is one short. This happened between this branch's gate at `013b8ff` and `e5b-7`'s at `1a848be`: both failed at `pad`, by the timestamps of both logs, with no fault in either build. `SeedCheck` is out of this now (decision 9), but `PadCheck`'s own Shipping runs still use that folder, because `ExportTelemetry.ps1` reads it. The smallest fix is to delete only the files the check itself created. Until then, run one `pad` at a time.

## Not verified
- A person double-clicking a launcher: full screen, on a real display. The launches here swap `-fullscreen` for off-screen flags.
- Play in the editor: the redraw per world and the reject that leaves the editor open were not run. By reading, a rejected seed there starts no game mode run, no telemetry and no director, but the seed 0 placeholder from `UDTSimSubsystem::OnWorldBeginPlay` keeps stepping in that world, which is looser than R-TEC-16's "no run starts".
- `Tools/Perf.ps1` was not run.
- Mac: not run (D-007).
