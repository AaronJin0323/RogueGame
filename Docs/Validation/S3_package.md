# S3 in the package: a person's path

Branch `dev/s3-realpath`, from `dev/s3` at `c6f02d2`. Package: `Tools/Package.ps1` at `f6c37b5`, clean tree, Shipping and Development, 961 MB. Check: `Tools/PackageHubCheck.ps1` (`02_Systems/package.md` R-PKG-13, T-PKG-13), `-RenderOffScreen -nosplash`, 1280x720, no window, no `Play.bat`, no launcher. A machine reading, not a validator session.

Before this, `Tools/VigilCheck.ps1` proved the hub in the editor, on a tool run forced through `-DTHub`. No launch had gone down the person's branch of `IsToolRun`, and nothing of S3 had been read in a package.

## Result

`PKGHUB: PASS (package f6c37b5, same cook=True, 13 tables read in en and zh-Hans, walks 1 en on seed 9107 + 1 zh-Hans, Shipping csv same as the walk=True, seed 9107 hash 276faf8e, owner's save unchanged=True, windows 0)`, 282 s. The table below reads this pass. Two more passes on this package also read `PASS`: `walks 1 en on seed 9107 + 2 zh-Hans`, and `-Seed 9108` with `walks 2 en on seed 9109 + 1 zh-Hans` (below).

The path works in the package. No cook, staging, text or save failure was found. The walk found one real failure in the game, in the seed, and an independent review found one in this check (both below); both are fixed. Shipping could not be walked, and what stands in for that is listed under Shipping.

## Which build proved what

| Claim | Build | Reading |
|---|---|---|
| Every table is staged | Both | 13 of 13 `DT/Text/Generated/ST_*.csv` in each `.pak`, by `UnrealPak -List`, with `Game.locres` for en and zh-Hans |
| Same cook in both builds | Both | all 4 `.utoc`/`.ucas` SHA-256 equal |
| Every table shows its CSV, in both languages | Development | 849 keys, 0 showing nothing; table below |
| A person's launch has no tool flag | Development, Shipping (D) | `Start-Game` refuses `-unattended`, `-nullrhi`, `-DTBot*`, `-DTHub`, `-DTNoSave`, `-DTMetaFile`, `-DTSettingsFile`, `-DTPoolGate`; each starts from the package's `Dusktracer.exe` |
| Boot lands in the hub | Development | no run, step or pawn; `Hub: open, balance 0, runs 0, last patron 'None'` |
| Every page opens, in both languages | Development | Contracts, Codex, Ledger-stone, Settings, Shrine: `PATHCHECK ...: ok`, shots read below |
| The Shrine's pick reaches the run | Development | en Storm, zh-Hans Forge; `run_start`, first `offer_pick` and the run all agree |
| A hand waits for a person | Development | `IsAutoPick` false; open a second past its guard; picked by Enter. The run file's `decisions` may not pass the driver's keys, so a hand that closes by itself fails. The Patron's hand takes no key: the Shrine's choice answers it |
| A first run is gated | Development | `Offers: pool gated, 0 unlocked` in every world; 73 ids shut, none in a hand. Thin: the Patron's hand and one Level hand a run |
| A real death reaches the run-over screen | Development | en seed 9107, step 1512; zh-Hans step 1588; `run_end` and `run_award` read `death` |
| The way back leads | Development | `Return to Vigil` on focus, past the guard, Accept lands on Begin with no `DTSeed` on the map |
| The bank | Development | en `Meta: banked 8` then 3, hub reads 8 then 11; zh-Hans 9 then 3, hub reads 9 then 12; each equals the run file's `run_award` |
| The save lands at the default path | Development | no `-DTMetaFile`: `<userdir>/Saved/SaveGames/DTMeta.sav`, 449 bytes |
| A second run | Development | keeps the Patron, takes a fresh seed, `run_end` `abandoned` |
| Quit to desktop | Development | exit code 0 on all 6 launches |
| The save outlives the process | Development | a new process reads 11 runs 2 Storm, and 12 runs 2 Forge, on Begin |
| Language switches live from the hub | Development | Settings, Right, Back: the hub redraws in the other language; `DTSettings.ini` holds it at the default path |
| No crash, no warning | Development | 0 lines of `: Warning: ` or `: Error: ` in 6 logs; no `Saved/Crashes` |
| A person's launch reads the default path | Shipping | bad file planted there, boot: `DTMeta.sav.bad.1` byte for byte, no new save. A tool run leaves the same file alone |
| The hub starts no run | Shipping | alive at 20 s, 0 run files. The runs in the next rows wrote theirs at 1.3 and 1.1 s; a check fails if that nears the wait |
| Shipping plays what the walk played | Shipping | same seed, first pool, no input: CSV byte-equal to the en walk's first run, SHA-256 `C76991C2...`; JSONL 137 lines each, 4 differ, in wall fields only |
| Shipping's bank reads | Shipping, Development | Shipping banks 8; a person's launch of Development reads 8, 1 run, Storm |
| Shipping banks on a Development save | Shipping, Development | 12 runs 2, plus 8 in zh-Hans; Development reads 20, 3 runs, Storm; Ledger-stone shows 20 |
| The owner's folder | none touched | no `SaveGames`, no `DTSettings.ini`, 142 run files and none of this check's, before and after. On the pass with no other track running, its newest file stayed 2026-10-31 23:25:43 |

## What the walk found

**Two runs of one launch played one seed.** `Tools/PackageHubCheck.ps1`'s first walk read `Hub: begin DTSeed=9107?DTPatron=Forge` twice in one process. `Play_*.bat` gives one `-DTSeed` and R-VIG-07 used it for every Begin, so a person's second run replayed the first: same spawns, same offers for the same picks. R-PKG-04 exists to stop exactly that. Class BUG. Replaced: `-DTSeed` names a launch's first run, every later run takes a fresh seed. Reading after: `DTSeed=9107` then `DTSeed=378613971`. Launchers untouched. **This is the one change under `Source/Dusktracer/Hub`:** `BeginRun` in `DTHubSubsystem.cpp`, no page. It holds whatever the launchers end up passing; with no `-DTSeed` the hub already took a fresh seed.

**A healthy build could print `FAIL`.** The walk demanded a Level hand before a hero who stands still dies, and whether one comes is the seed's. The zh-Hans walk draws a fresh seed on every pass, so a good build could fail on the draw. An independent review caught it, and it was reproduced on seed 9108: Storm and Forge both die at 24.58 s holding only the Patron's hand, `PATHCHECK a hand waits for a person's key: WRONG`. The check's own "no hand was offered" could not catch this, because it counted the Patron's hand, which every run has. Class BUG, in the tool; no game code. Replaced: the driver says `PATHCHECK no hand before the death`, and the check walks again on a clean profile, en on the next seed and E on the seed the walk played, zh-Hans on a fresh draw. The first pass on this package met it unprompted: draw `626393681` reached no hand and draw `1665657255` did, so the old check would have failed that build. `-Seed 9108` reads `walks 2 en on seed 9109`, with E's CSV equal on 9109.

2 of 13 Forge draws reached no hand, across the review's probes and these passes: about one in seven, from a small sample. At that rate four draws in a row miss about once in 2,000 passes, so a good build can still fail, rarely, and the line says why. Four is a bound, not a tuned number, and it decides nothing in the game.

## Traps a tool will meet

- **A temporary `LOCALAPPDATA` does not move a Shipping save.** The engine asks Windows (`SHGetKnownFolderPath`, `WindowsPlatformProcess.cpp`), not the environment, so it would have written into the owner's folder. `-userdir=<dir>` moves the save, the settings, the run files and the engine's `Saved/Config` together, and is what the check uses. Read in the engine source; deliberately not tried against the real folder.
- **Shipping ignores a map on its command line** (`UGameInstance::StartGameInstance`, `UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING`), so no `?DTSeed=...?DTPatron=...` can start a person's run there. The `$Map` that `PackageRestartCheck` passes to Shipping does nothing; `-DTSeed` is what it relies on. `PadCheck` names a map on its Development launch only.
- `Hub: open` logs an empty `LastPatron` as `None`. An abandoned run's `run_award` reads `end` `death`, as R-MET-05 counts it; `run_end` reads `abandoned`.
- A dirty Development build stamps `build` `<hash>+dirty`, which breaks the byte compare against a clean Shipping. Only a clean package reads E.
- `Tools/PadCheck.ps1` and `Tools/PackageRestartCheck.ps1` launch Shipping with no `-userdir`, so their tool runs land in the owner's folder and clean up after themselves there, by seed. They open no save, and `T-PKG-05` asks for that folder. Two tracks at once collided: the gate's `pad` failed here with `first record is not run_start` while another worktree's `PadCheck` ran on the same marker seed, both writing one `770077_<second>.jsonl`. `PadCheck` now takes its seed from its process, which alone would strand a killed check's files: its `finally` never runs and the next check has another seed. So each check names itself by PID in `%LOCALAPPDATA%\Dusktracer\PadCheck` while it runs, and the next one clears exactly what a dead one left. Not by the seed's range: a person's run can land in it, and that folder holds the owner's evidence. `PackageRestartCheck` was not run on this package: it deletes by seed in that folder, and this check reads its T-TEC-15 for it. `PadCheck` has since left that folder, because `S3_kit.md` has to run it on the owner's account: its Shipping launches take a `-userdir` under its own `Saved`, and `T-PKG-05` reads the run there. With nothing shared between worktrees, the PID bookkeeping went and the seed is fixed again.

## Shipping

Shipping has no log and no `-DTPathCheck`, an off-screen run has no window for a key, and a map cannot be named. So nothing leaves its hub, and no hook was added: R-PKG-02 keeps them out. What stands in:

| Launch | Flags | Proves |
|---|---|---|
| D boot | a person's: `-culture=en -DTSeed=9107` | the person's branch, the default path, no run |
| E run | `-unattended -DTPoolGate -DTMetaFile=<default path> -culture=en -DTSeed=9107` | play and bank, equal to the walk; a tool run by necessity |
| G run | as E, zh-Hans, on the zh-Hans walk's save | Shipping reads a Development save and adds to it |

The Development source differs from Shipping's by `#if !UE_BUILD_SHIPPING` blocks only, the cook is the same, and the run the two played was the same. That is a bridge, not the act.

## Text in the package

Keys and rows differing from en, as shown by the running game; each equals its CSV. en shows 0 localized in every table.

| Table | Keys | zh-Hans differs |
|---|---|---|
| ST_UI | 158 | 152 |
| ST_Weapons | 20 | 20 |
| ST_Modifiers | 32 | 32 |
| ST_Keepers | 300 | 300 |
| ST_Keywords | 52 | 52 |
| ST_Reactions | 62 | 62 |
| ST_Enemies | 14 | 13 |
| ST_Reforging | 32 | 32 |
| ST_Codex | 31 | 30 |
| ST_KeeperLines | 42 | 36 |
| ST_World | 45 | 45 |
| ST_Discoveries | 32 | 32 |
| ST_Tree | 29 | 29 |

The 13 tables were registered by hand one at a time, and the gather follows that macro, so a table left out would have no zh-Hans and no test would say so. `Tools/loc_build.py --check` now fails one (`Story` in `Tools/tools_test.py`), in the gate at no cost.

## The shots

`Saved/PackageHub/<culture>/shots/<page>.png`, 40 in all, not committed; each passes `Tools/image_check.py`. Read one by one at `1a848be`: en `root_fresh`, `run`, `hand`, `runover_death`, `ledger_banked`, `codex`, `contracts`, `root_switched`; zh-Hans `hand`, `shrine`, `codex`, `settings`, `runover_death`, `ledger_banked`, and the Ledger-stone after Shipping's bank. At `f6c37b5`, where only `DTPathCheck` differs in `Source`, both `hand` shots and that Ledger-stone were read again; the rest were not. Nothing is missing, no box glyph, no raw key. What a reader may want to weigh: the card screen's keyword note sits over the HP label in zh-Hans (`hand`, a hand's own layout). Hub pages match `S3_vigil.md`, which read them in the editor.

## Tool runs did not move

| Reading, seed 9107 | `dev/s3` (`c6f02d2`), built before any edit | `dev/s3-realpath` (editor `1a848be`, package `f6c37b5`) |
|---|---|---|
| Editor, `Tools/RestartCheck.ps1`, `input_hash` | 276faf8e | 276faf8e |
| Editor, steps | 5400 | 5400 |
| Package, Development, bot, 90 s | not read | 276faf8e |
| Package, Shipping, bot, 90 s | not read | 276faf8e |

## Readings worth keeping

- A Reclaimer who never moves is dead at 24.6, 25.2 and 26.5 s in this pass, banking 8 or 9 Ichor; other passes and the review's probes read 24.1 to 78.7 s, the last banking 30. One run per seed, no bot, no human: not a tuning note, and no number was touched.
- `LoadMap` on a hub path, boot and reloads: 0.015 to 0.057 s. Whole check: 282 s, 140 of them the two walks; a walk again adds about 70 s, more on a long draw.

## Decisions

- `-userdir`, not a temporary `LOCALAPPDATA`, isolates a launch (R-PKG-13).
- `-DTSeed` names a launch's first run (R-VIG-07, R-PKG-04).
- The driver is `Source/Dusktracer/Settings/DTPathCheck.*`, ticked from the menu like `DTRestartCheck`, so nothing of it sits under `Hub`, where the Contracts track works. It reads the save through log lines and includes no `Meta/` header.
- Not a gate step: 301 s on top of a package the gate does not build, so it would read `NOT RUN` on nearly every branch. A hand check after `Tools/Package.ps1`, named in R-PKG-13, as `PackageRestartCheck` is.
- No hook in Shipping. It stays a bridge.
- A walk stands still. Real keys drive the pages and hands; the hero never moves, so the run is seeded and comparable.
- A walk with no hand starts over, four walks at most, on the driver's own line only, so a walk that hung or went wrong is never retried. Not a fixed seed for zh-Hans: that walk is the launch with no `-DTSeed`, and any seed that reaches a hand can stop reaching one when the balance moves.
- A retry alone would hide a hand that closes by itself, because the driver reads that as no hand. So the run file's `decisions` must not pass the driver's keys. Tried by a fault in a scratch copy of the check, `-DTOfferAutoPick` on an en walk: four walks, seeds 9107 to 9110, each with no hand by the driver; verdicts `no hand came in 4 walks` and `1 hand(s) closed in the first run, only 0 by a key`.
- `PackageHubCheck` judges only a clean package with `HEAD`'s `Source`, `Content` and `Config`, as `PadCheck` does (R-PKG-08), before it clears anything, and its verdict names the package. Refusals tried: stale, dirty, an unknown commit, no stamp; each `NOT RUN`, exit 2, last pass's logs and shots kept.
- A killed `PadCheck` is cleared by the next one, from a record of running checks by PID. A real check killed in its Shipping stage left `817400_...jsonl` and its record; the next check cleared both, and the owner's other run files and evidence hashes did not move.

## Not proved

- Shipping past its hub: a key on any hub page, Begin, a person's run, the bank by a person, the way back. Needs real hands (`package.md` HUMAN).
- That the hub draws in Shipping: D reads no run, which is not a frame.
- A real key through the operating system, a real window, Fullscreen, a real pad: the proof sends key events into the player controller.
- At integration the launchers name no seed (D-025, D-028), so the seed text above describes this branch. `-DTSeed` still names a launch's first run.
- `Play_en.bat` and `Play_zh-Hans.bat` themselves: each opens a full-screen window, so they were not launched; their arguments were copied with `-fullscreen` swapped for off-screen. `-DTSeed=%RANDOM%%RANDOM%` overflows to 2147483647, the owner's run's seed; the seed agent has that, and launchers were not touched.
- Shipping's real save under `%LOCALAPPDATA%`: never aimed there, on purpose.
- A win through the hub, a Discovery on a person's save, a Codex row opening by play: a hero who stands still reaches none.
- Buying anything: the Contracts page has no factory on this base.
- The gated pool in depth: one Level hand a run. `Dusktracer.Meta.FirstRunPool` is the proof of the list; this is one real launch agreeing with it.
- Two seeds from one launch: this hand check alone holds R-VIG-07's fix. No gate step reads it, because `Tools/VigilCheck.ps1` begins once per process.
- `Tools/PackageRestartCheck.ps1` still judges whatever package it finds: no stamp guard, and it is not in this branch's scope.
- Restart run from the run-over screen, and the other Settings rows, in the package.
- Editor against package on any seed but 9107.

## Reread on 2026-11-11: `dev/s3` 81c638e, after the three carries of `main`
Taken alone on the machine, in the carry agent's worktree (`E:\Dusktracer\.claude\worktrees\agent-aa7a88e9be0750f6a`), on branch `s3/package-reread`. The package is that worktree's `Saved\Package`; the main checkout's packages were not touched. Stamp: commit `81c638e`, clean, Shipping and Development, 962 MB. Mask `FFFFF3FC` (D-052): the Tools apply it themselves, and every launch is held by the launch job's affinity limit.

Verdicts, as printed. Checks that did not pass were rerun once (D-052 item 4):
- `PACKAGE: PASS (81c638e, Shipping+Development, 962 MB under ...\Saved\Package)`
- `PKGRESTART: FAIL (4 failure(s), first: A log warning or error (35): LogThreadingWindows: Warning: Runnable thread Foreground Worker #0 call to SetThreadAffinity failed with: 0x1f; same cook=True, hash 276faf8e, csv same=True, shipping bridge=True, reloads ms 42 52 44, LoadMap s first 0.024752 reload 0.040488)`. Rerun: the same four failures and the same readings (reloads 38 39 36 ms). A third attempt, the second in a batch, stopped at once on `PKGRESTART: FAIL (script error: Access is denied)`.
- `PKGHUB: FAIL (script error: Access is denied)` right after stage A. Rerun: `PKGHUB: FAIL (7 failure(s), first: walk_en log warning or error (33): LogThreadingWindows: Warning: Runnable thread Foreground Worker #0 call to SetThreadAffinity failed with: 0x1f; package 81c638e, same cook=True, 13 tables read in en and zh-Hans, walks 1 en on seed 9107 + 1 zh-Hans, Shipping csv same as the walk=False, seed 9107 hash 276faf8e, owner's save unchanged=True, windows 0)`. Every walk read `PATHCHECK: PASS (0 wrong)`.
- `EXPORTCHECK: PASS (package 81c638e, zip 25 KB, 6 files, 6 scanned clean, raw log named the user 7 times, 0 game processes)`
- `CRASHEXPORT: FAIL (the crash launch did not end in 180 s)`. Rerun: the same.
- `Tools\Perf.ps1 -Package`, Caps: `PERF: PASS (1920x1080, game p50 1.34 / p95 1.76 ms, GPU p95 2.95 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)`, which is 4.72 ms proxied. `-Package -Scene S2`: `PERF: PASS (1920x1080, game p50 2.04 / p95 3.41 ms, GPU p95 3.74 ms, load 5810.00 bullets / 400.00 enemies, 1800 frames)`, 5.98 ms proxied. A first pair of runs is void and its rows in `Saved\Perf\perf.csv` are not readings: the agent's own command handed `-Package` to `-Cmds`, so it measured the editor with no load (`cmds='-Package'`, 0 bullets).
- `PADCHECK: FAIL (script error: Access is denied)` after its SEEDCHECK half passed. Rerun: `PADCHECK: NOT RUN (package 81c638e ok, en+zh-Hans, 0 windows; real pad needs a human, ...)`, exit code 2: the machine half passes, as expected.

By hand: the en walk's Development run file (`PackageHub\en\user\Saved\Playtests\9107_*.jsonl`) carries `"bot_fast":0` in `run_start` and `"dropins_landed_by_role":{}` in its `dir_block`. The run dies at 25.2 s with no drop-in landed.

**What differs from the reading at 00c270c, and why.**
1. **Every Development launch logs `LogThreadingWindows: Warning: ... SetThreadAffinity failed with: 0x1f`, and PKGRESTART and PKGHUB fail on any log warning.** Shipping writes no such line. These are the only failures of PKGRESTART (A to D), and six of the seven of PKGHUB. Everything those checks compare passes: same cook, hash `276faf8e` in Development and Shipping, CSVs equal, reloads, input check, the shipping bridge, every PATHCHECK, the owner's save untouched, no window. The likely cause, from the code and not proved: the launch job's affinity limit that the carry brought from `main` (D-052, `Tools/HiddenLaunch.ps1`), when an engine thread asks for a processor outside the mask. The checks were written before the limit existed. Not fixed here: it is a defect of the checks against the carried launcher, or of the limit, for the scheduler to place.
2. **PKGHUB's seventh failure, "Shipping's run is not the en walk's first run".** The two CSVs are byte for byte the same: header plus one row, `0,25.200,...,9107,81c638e,en,kbm`. The check compares whole intervals and drops the last row, the death frame's (`Get-WholeIntervals`, added by 00c3737 after 00c270c). The walk dies at 25.2 s with an interval of 60 s, so both files have one row and nothing is left to compare. The rule reads that empty comparison as a mismatch. Steps were 1512 / 1512 and retained 8 / 8, as in the first pass of `S3_verify.md`. This is a defect of the check on a run shorter than one interval, not a difference between the runs.
3. **CRASHEXPORT: the crash launch does not end.** `debug crash` crashes the game ("Crashing the gamethread at your request"). The crash folder appears under the `-userdir` with `CrashContext.runtime-xml` (58 KB) and a `UEMinidump.dmp` of 0 bytes, and the process is still alive at 180 s, when the check stops it by its PID. The same happened on both runs. The likely cause is the same job limit around a process writing its minidump; that is not proved. The redaction half was not reached.
4. **"Access is denied" as a script error**, once each in PKGHUB, PadCheck and the third PKGRESTART, each time when a check started after another check in the same shell. Every one passed that point when run on its own. The failing line is not printed, because the checks' trap reports the message only. These are odd failures as D-052 item 4 means them; each is named here with both results.
5. **Perf in the package at the real size**: Caps 1.76 ms game thread p95 and 4.72 ms proxied GPU p95, S2 3.41 ms and 5.98 ms. The last reading (game p95 1.78 ms, GPU 3.60 ms proxied) was taken at 888x500.

Logs: the carry agent's worktree, `Saved\Logs\s3pkg_package.txt`, `s3pkg_restart.txt` and `s3pkg_restart3.txt`, `s3pkg_hub.txt` and `s3pkg_hub_rerun.txt`, `s3pkg_export.txt`, `s3pkg_crash.txt` and `s3pkg_crash2.txt`, `s3pkg_perfcaps2.txt` and `s3pkg_perfs2_2.txt`, `s3pkg_pad.txt` and `s3pkg_pad2.txt`. Also `PkgRestart_*.log` and `PkgHub_*.log` there, `Saved\CrashExport\crash_launch.log`, and the crash folder under `Saved\CrashExport\account`.

## The package checks fixed (2026-11-11, `s3/pkg-checks`)
Branch `s3/pkg-checks`, from `dev/s3` 79117b5, with `s2/pkg-checks` merged into it (`S2_package.md`, "Package checks under the launch job's affinity limit"). This section covers the defects of the reread above. Class BUG; nothing in the game changed.

**1. The affinity warning.** The cause was shown on one package before any check changed: 33 `SetThreadAffinity failed with: 0x1f` lines inside the launch job's limit, none outside it (`S2_package.md`). `Tools/PackageHubCheck.ps1` now sets aside exactly that line on a launch the limit held, with `Split-DTLimitWarnings` (`Tools/HiddenLaunch.ps1`), as `Tools/PackageRestartCheck.ps1` does. It counts the lines and prints the count in its verdict; every other warning still fails. R-PKG-13 in `02_Systems/package.md` now states the hub check's log rule with the exception and names D-052.

**2. Step E's CSV comparison.** `Compare-RunCsv` compares whole intervals when both files have one. A run that ends inside the first interval has none, so both files are then compared by every row they have, and one file with whole intervals against one without is a mismatch. The line and the verdict say which comparison was made. The ReviewMinors case `test_step_e_compares_a_run_shorter_than_an_interval_by_its_rows` holds all four outcomes.

**3. The crash launch that does not end: the game's, not the check's.** Six launches of the Development package with the check's own arguments (`-unattended -nosound -culture=en -DTSeed=770099 -ExecCmds="debug crash"`), on the hidden desktop, each in a fresh `-userdir`:

| Run | Inside the launch job's limit (as the check launches) | Outside it (no limit) |
|---|---|---|
| 1 | ended in 2.5 s, `UEMinidump.dmp` 1,364,942 bytes | did not end, stopped by its PID at 120 s, `UEMinidump.dmp` 0 bytes |
| 2 | did not end, stopped at 60 s, 0 bytes | ended in 1.7 s, 1,586,405 bytes |
| 3 | ended in 1.6 s, 1,451,610 bytes | did not end, stopped at 60 s, 0 bytes |

A hung launch: the stub has one thread waiting (UserRequest), and the game has about 100 threads, all but one Wait/Suspended and one Wait/Unknown. Its log ends on `appError called: Fatal error ... Crashing the gamethread at your request` and `Windows GetLastError ... (0)`, with no `StaticShutdownAfterError`. A run that ends logs the callstack, `StaticShutdownAfterError` and `RequestExit`. The hang comes about half the time with the limit and without it, so it is the game's crash exit, not the check's launch. It went to the Mechanics session as its own finding, to be read on `main`'s code and on Shipping. `Tools/CrashExportCheck.ps1` is unchanged, and **its FAIL stands until the game's crash exit is settled.** Logs: `Saved\Diag\crash_in*` and `crash_out*` in the carry agent's worktree.

**4. "Access is denied" when one check follows another in one shell.** The cause and the fix are in `S2_package.md`, because the scripts are on `dev/s2`. The launch helper's desktop and jobs are static to the PowerShell process. A window watch then put the process in a job of its own, and Windows refused a new launch into the helper's older jobs. Under the mask that is not a processor fault (D-052 item 4): it is a handle that outlived its script. The watch now closes the helper's jobs before it starts. `tools_test.py` `WatchAfterLaunch` holds it.

**Gates.** `& Tools\Gate.ps1 -Base dev/s2` on `s2/pkg-checks` 95f7878: `GATE: PASS (95f7878; 3 not run; mask FFFFF3FC)`, `TESTS` 480 of 480 against the floor of 480, `TOOLS: PASS (151 tests)`. `& Tools\Gate.ps1 -Base dev/s3` on `s3/pkg-checks` 22d462a: `GATE: PASS (22d462a; 3 not run; mask FFFFF3FC)`, `TESTS` 547 of 547 against the floor of 547, `TOOLS: PASS (284 tests)`. Not run, in both: pad (its machine half reads in the gate, a real pad needs a human), scope and mac.

**Reading, 2026-11-11 19:34 to 19:47:** a fresh `Tools\Package.ps1` in the carry agent's worktree, then every check one after another in the same PowerShell process:
- `PACKAGE: PASS (22d462a, Shipping+Development, 962 MB under ...\Saved\Package)`
- `PKGRESTART: PASS (same cook=True, hash 276faf8e, csv same=True, shipping bridge=True, reloads ms 47 40 40, LoadMap s first 0.031225 reload 0.042603, limit warnings set aside 132 (D-052))`
- `PKGHUB: PASS (package 22d462a, same cook=True, 13 tables read in en and zh-Hans, walks 1 en on seed 9107 + 1 zh-Hans, Shipping csv same as the walk=True (all rows, no whole interval), seed 9107 hash 276faf8e, owner's save unchanged=True, windows 0, limit warnings set aside 198 (D-052))`
- `EXPORTCHECK: PASS (package 22d462a, zip 25 KB, 6 files, 6 scanned clean, raw log named the user 7 times, 0 game processes)`
- `CRASHEXPORT: FAIL (the crash launch did not end in 180 s)`: the game's crash exit, above. Counting this reading, the check's crash launch has failed to end three times out of three: twice on 81c638e, once here.
- `PADCHECK: NOT RUN (package 22d462a ok, en+zh-Hans, 0 windows; real pad needs a human, no run of this build reads input_device=pad)`, exit code 2: the machine half passes.

No "Access is denied" appeared: PKGHUB and PadCheck each started their window watch after other checks in the same process. Log: `Saved\Logs\final_chain.txt` in the carry agent's worktree.
