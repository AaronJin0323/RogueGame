# S3 verify: fixes read from code, now seen

Branch `fix/s3-verify`, from `dev/s3` at `1f95944`. Input: the "Not verified here" list of `S3_review_fixes.md` and the "Not verified" list of `13_Decisions/D-041.md`. Each item below was a fix proved on stand-ins or read from source. Here each got the smallest live reproduction that could fail: real processes or a real game world, run once with the fix switched off and once with it on. A switch was a local edit, never committed. No game rule or number moved; the one change to a tool's behaviour is `PackageHubCheck`'s stop (#63).

Every launch ran on the hidden desktop of `Tools/HiddenLaunch.ps1` with a `-userdir` under this worktree's `Saved`. Nothing read, wrote or listed `%LOCALAPPDATA%\Dusktracer`.

## Result

| Item | Reproduced | Fix off | Fix on | Kept as |
|---|---|---|---|---|
| Two processes on one save (#0) | yes, two real game processes | 0 of 4 scenes | 4 of 4 | `Tools/TwoWindowsCheck.ps1`, T-MET-48, by hand |
| A run ending on the frame Pause lands (#1, #8) | yes, whole frames of a game world under the engine's pause | 4 of 4 paused scenes stay open and bank as left | all close with their own end | `Dusktracer.Meta.PauseFrame`, T-MET-47, in the gate |
| D-041 with a pause on the shared step | yes, same world | 4 of 4 paused scenes bank no win | all win | `Dusktracer.Boss.RunEndPaused`, T-BOS-14, in the gate |
| D-041 with two heroes | at the sim's level only | 3 of 6 scenes | 6 of 6 | `Dusktracer.Boss.RunEndTwoHeroes`, T-BOS-15, in the gate |
| WinCheck's crash scan (#52) | yes, the real script on staged logs from a stand-in engine | 1 of 3 (the control) | 3 of 3 | `tools_test.py` `WinCheckCrash`, in the gate |
| PackageHubCheck's stop (#63, #107) | yes, real launches of the package beside a bystander game, then the whole check on a package of this branch | the bystander dies | the launch and its game end, the bystander lives; `PKGHUB: PASS` | `tools_test.py`, in the gate |
| Crash redaction (#35, #46) | yes, a real crash of the packaged Development game | the three ids are in the zip | 0 leaks | `Tools/CrashExportCheck.ps1`, T-PKG-15, by hand |

One gap the two-process fix does not cover is under item 1, with options. It needs a call and was not changed.

## 1. Two game processes on one save (#0)

`FDTMetaSession::Refresh` rereads the file before a bank or a purchase when another process wrote it since. It was tested on two sessions in one process (`Dusktracer.Meta.TwoWindows`).

**What was run.** `Tools/TwoWindowsCheck.ps1`: four scenes, each two `UnrealEditor-Cmd.exe -game` processes on one `-userdir`, so one `DTMeta.sav` at that folder's default path. The holder opens the save first and changes it last; the other process changes the file in between and exits. The order is forced, not timed: the other process starts only after the holder's log shows the save opened, and the holder's change is either a bot run still playing (the script fails the scene if the holder banked early) or `DT.Tree.BuyWhen`, which waits for a file the script writes. A third process reads the file afterwards with `DT.Meta.Dump`. Both commands are debug commands, not in Shipping; `BuyWhen` makes the same `DTTree::Buy` call a node's Accept makes. About three minutes, at most two game processes at once.

| Scene | Fix off | Fix on |
|---|---|---|
| A bank over a purchase: planted 400, the other buys a 400 node, the holder banks 23 | balance 423, no node: the purchase is gone and its price is back | balance 23, runs 1, `Trunk.ScatterCannon=1` |
| A bank over a bank: the other banks 23, the holder banks 23 | balance 23, runs 1: one run is gone | balance 46, runs 2 |
| A purchase over a purchase: planted 400, two nodes of 400 asked | second result 0 (bought): the save holds `Trunk.RailLance` alone, the first node is gone, 800 of nodes were sold for 400 | second result 5 (short), balance 0, `Trunk.ScatterCannon=1` |
| A purchase over a bank: planted 400, the other banks 23, the holder buys 400 | balance 0, runs 0: the run and its award are gone | balance 23, runs 1, the node |

Fix off: `TWOWINDOWS: FAIL (0 of 4 scenes, two processes on one save each, seed 9131)`. Fix on: `TWOWINDOWS: PASS (4 of 4 scenes, two processes on one save each, seed 9131)`, twice.

**Not a gate step.** Thirteen launches and three minutes, one of them a run that plays 60 s in real time to hold its save open. It is a hand check named in `meta.md` T-MET-48, as `PackageHubCheck` is. If the owner wants it in the gate it fits beside `meta`.

**A gap the fix does not cover: two writes inside one window.** `Bank` and `Buy` each read the file, change the profile and write it, with no lock between the read and the write. Two processes that both read before either writes still lose one change, exactly as before the fix; the fix narrows the window from a session's whole life to the few milliseconds of one change. Seen, not only read: ten trials of two processes started together on one save, each playing 30 s of bot at 60 steps a frame so both bank at about the same moment, the save read afterwards by a third process (a scratch script, not kept). In 9 trials the banks landed 0.6 to 1.1 s apart, the later process logged `Meta: another launch wrote ...; read again`, and the save held 2 runs. In 1 trial the banks landed 10 ms apart (`09.26.37:954` and `09.26.37:964`), neither process logged a reread, each logged `runs 1`, and the save held 1 run: one bank of the two (10 and 11 Ichor) is gone. That is with the fix on.

Options, none taken here because each is a rule about the save (`meta.md` R-MET-11), not a bug in what was built:
- A. Leave it. Two windows on one save is already an edge, and the window needs two run ends or purchases within milliseconds. Cost: a lost run or purchase stays possible and silent.
- B. A lock for the length of one change: open a sibling `DTMeta.sav.lock` with no sharing, reread, change, write, close. The handle dies with its process, so no stale lock. Needs a wait limit and a rule for a lock that cannot be had (write anyway, or refuse). Smallest change that closes the window.
- C. One game per save: a second launch on a save that is open refuses to start, or starts with no save and says so. Closes this and the stale balance a second window shows, at the cost of a new message and a rule for a crashed first window.

**Also not verified.** The hub page of a window that sat while the other one wrote: the balance it draws is the one it opened with until its own next bank or purchase. Read from the code, not looked at.

## 2. A run that ends on the frame Pause lands (#1, #8)

A death or a win closes its telemetry session, and so banks, on a next-tick world timer. `LevelTick.cpp` reads `IsPaused()` once at the top of a frame, ticks timers only when that read was false, and ticks tickables either way. So a pause that lands inside the frame of the run's last step leaves a timer that never fires. The fix (`UDTMenuSubsystem::WatchRunOver`) closes the run itself when the world is paused as the run-over screen opens. It was read from engine source; no harness could place a pause frame-exactly.

**What was run.** `Dusktracer.Meta.PauseFrame`. The world is built as a launch builds it: a game instance, the game mode made by `UWorld::SetGameMode`, a player controller with its player state, `BeginPlay` through the game mode. Frames are whole `UWorld::Tick` calls, so the engine decides what runs. The pause is the engine's own (`AGameModeBase::SetPause`), asked for where a key press is handled: inside the frame, after the frame has read its pause state and before the tickables (`FWorldDelegates::OnWorldPreActorTick`), by the call `ADTHero::OnPauseStarted` makes. The hand's pause is a level-up queued in the Pickups phase of the last step, as progression queues one. No hand picks itself: `dt.Offer.AutoPick` is 0 for the test, and the Patron's hand is picked through the offer system. The bank writes a real file, which a second session reads back.

| Scene | Fix off | Fix on |
|---|---|---|
| Death, no pause (control) | closes as `death` | closes as `death` |
| Win, no pause (control) | closes as `victory` | closes as `victory` |
| Death, Pause pressed on the frame | never closes; on leaving it banks as left: no `run_end` `death`, no summary line, 0 deaths on the save | closes as `death`: run-over screen with its title, summary line, `run_end` and `run_award` `death`, 1 run and 1 death on the save |
| Win, Pause pressed on the frame | never closes; banks as left: 0 wins on the save | closes as `victory`, 1 win on the save, the award with the boss |
| Death, a level-up hand in the last step | as the pressed Pause | as the pressed Pause |
| Win, a level-up hand in the last step | as the pressed Pause | as the pressed Pause |

Fix off: 30 assertions fail across the four paused scenes and none in the controls. Fix on: all pass. The controls prove the frames here do fire timers, so a scene that stays open is the pause's doing.

**Still not verified.** A real key through the operating system and Enhanced Input into `OnPauseStarted`; a frame drawn (the screen and the summary line are read from the menu's state and the recorder's award, which is what `Draw` reads); the same on a launched game. The summary line's text is `DTMenuText::RunSummary` and its strings are T-MET-06's.

## 3. D-041 with a pause on the shared step, and with two heroes

**A pause on the shared step.** `Dusktracer.Boss.RunEndPaused`, in the world of item 2: both orders of R-BOS-13 (the hero down in the step whose kills take the hull; both last hits dealt on one step) with no pause, with Pause pressed on the frame of that step, and with a level-up hand queued in it. All six end as a win on the run-over screen, in `boss_end`, `run_end` and `run_award`, with the boss in the award and one win in the save file, and no record says death. With the pause-frame fix off, the four paused scenes stay open and bank no win. With D-041's hold off (`UDTHeroSubsystem::HoldRunEnd` ignored), the three same-step scenes bank no win.

**Two heroes.** The game does not support two heroes above the sim: `ADTGameMode::BeginPlay` starts every run with one player, and the hero system takes its count when the world begins play. So there is no two-hero run with the game mode, the screen or the bank, and none was faked. At the sim's level two heroes do run in a test world: `Dusktracer.Boss.RunEndTwoHeroes` starts the run itself with two players before play begins and reads the two delegates the game mode binds (`OnAllHeroesDown`, `OnBossDefeated`), `boss_end` and `run_end`.

| Scene | D-041 off | D-041 on |
|---|---|---|
| One hero down earlier, the fight goes on; the last falls in the step whose kills take the hull | death | win, no death announced, two `hero_down` |
| Both fall in that step | death | win |
| One of two falls in that step, the other stands | win | win |
| The hull's last hit and both heroes' last hits dealt on one step | win, but a hit dealt with the kill takes HP | win, neither hero loses HP, no `hero_down` |
| Both down in turn, the hull standing (control) | death, announced once | death, announced once |
| The hull falls a step after both heroes (control) | death | death |

"D-041 off" is the hold ignored and a won run's hits applied. What a second player's fall should mean is still `coop.md`'s to say (D-004); this proves only that "every hero down" holds with two.

**Still not verified.** A run found in play that held either order. Two heroes through the game mode, the run-over screen and the bank, which do not exist.

## 5. WinCheck's crash scan (#52) and PackageHubCheck's stop (#63, #107)

**WinCheck.** The fix moved the crash scan ahead of the lost-seed skip; `tools_test.py` held only the order of two lines in the script. Now `tools_test.py` `WinCheckCrash` runs `Tools/WinCheck.ps1` itself, through `Tools/HiddenLaunch.ps1`, against a stand-in engine: a small console program built in the test under a made-up `UE_ROOT`, which copies the log staged for its seed to the `-ABSLOG` it was handed. Nothing of the game runs, so this holds the script's reading of a log and nothing about a real crash.

| Staged for seeds 71, 72 | Scan after the skip (the defect) | Scan first (the fix) |
|---|---|---|
| 71 lost with `=== Critical error: ===` and `Fatal error!`; 72 lost | `en seed 71 lost; next seed`, launches 72, `WIN: FAIL (en, no win on seeds 71 72)`, no log kept | `WIN: FAIL (en seed 71, 2 crash or ensure line(s); ...WinCheck_en_crash_71.log)`, 72 never launched, the kept log equals the staged one |
| 71 lost with `Ensure condition failed`; 72 lost | goes on to 72 | fails at 71 |
| both lost, no crash line (control) | `no win on seeds 71 72` | `no win on seeds 71 72`, no crash log |

**PackageHubCheck.** `Stop-Game` ended every `Dusktracer*` process whose path was under the package folder, then the launch. It is `Stop-DTHidden` on the launch's own process now: the stub and the game under it, by PID. Class BUG, in the tool. `tools_test.py` fails any `Stop-Process`, or a `taskkill` that is not on a tool's own `$Proc`, in any script under `Tools`. 

Seen on real processes, on a package of this branch built into this worktree's `Saved\Package` (`PACKAGE: PASS (eb13be8, Shipping+Development, 962 MB ...)`):

| Reading | Old `Stop-Game` | New `Stop-Game` |
|---|---|---|
| A bystander Shipping game from the same package on its own `-userdir`, then one launch of the check's own, stopped after 12 s | own 2 processes -> 0, bystander 2 -> 0: the bystander is killed | own 2 -> 0 (the stub and the game under it), bystander 2 -> 2 |
| The whole `Tools/PackageHubCheck.ps1` with a bystander game up from before its first launch to after its verdict | not run | `PKGHUB: PASS (package eb13be8, same cook=True, 13 tables read in en and zh-Hans, walks 1 en on seed 9107 + 1 zh-Hans, Shipping csv same as the walk=True, seed 9107 hash 276faf8e, owner's save unchanged=True, windows 0)`; the bystander's stub and game both alive at the end |

So every stop the check makes (D's boot killed at 20 s, E, G and both H runs) ends its own launch, files are free for the next launch, and nothing else from the package is touched. The old stop was shown on the two-launch reading only; the whole check was not run with it.

A first pass on package `ac1f8db` read `PKGHUB: FAIL` on E alone, `Shipping's run is not the en walk's first run: steps 1512 / 1512, retained 8 / 8`. The two CSVs differed in the build stamp, `ac1f8db+dirty` against `ac1f8db`: a file of this worktree was edited between the package's two game builds, so Development was stamped dirty. `S3_package.md` names that trap. The package was rebuilt from a tree left alone, and that is the pass above. No defect.

## 4. Crash redaction (#35, #46)

The export's id rule was fixed to take `<Key>value</Key>` and JSON as well as `key=value`, and proved on a made-up crash context (`tools_test.py` `ExportLogs`).

**Where a crash writes, checked before staging one.** `WindowsPlatformCrashContext.cpp` writes the crash folder to `FPaths::ProjectSavedDir()/Crashes/<guid>`, which `-userdir` moves with the rest of `Saved`, and then starts `CrashReportClient.exe` from the package, which would send the report when the launch is `-unattended`. This package ships no crash reporter (0 files named `CrashReportClient*` under `Saved\Package`), so the folder is written, the reporter fails to start, `-unattended` asks for no dialog, and nothing is sent. The editor build was not crashed: it starts a crash reporter of the engine's own as a monitor, which would send. The crash is the engine's `debug crash` console command on the Development build; Shipping does not have it, and nothing was added to the game.

**What was run.** `Tools/CrashExportCheck.ps1` on package `eb13be8`. It refuses to stage a crash when the package carries a crash reporter, and fails if no crash context appears under the `-userdir` it named. It reads the id values out of the real `CrashContext.runtime-xml`, runs `Tools/PackageFiles/Export_Logs.ps1` with `%LOCALAPPDATA%` pointed at the check's account for that child alone, and looks for each value in every file of the zip, as UTF-8 and UTF-16. The values are never printed.

| Reading | Export before `2679905` | Export now |
|---|---|---|
| The launch | the crash below, read again | exit code 3; crash folder under `Saved\CrashExport\account\Dusktracer\Saved\Crashes`: `CrashContext.runtime-xml`, `crash_launch.log`, `CrashReportClient.ini`, `UEMinidump.dmp` |
| Ids with a value in the real context | `MachineId`, `LoginId`, `EpicAccountId` | the same |
| The exported context | holds all three values | `<MachineId><id></MachineId>`, and the same for the other two |
| Verdict | `CRASHEXPORT: FAIL (... 4 file(s) in the zip, 3 leak(s) ...)` | `CRASHEXPORT: PASS (a real crash, 4 file(s) in its folder, 3 id value(s) read from it [EpicAccountId LoginId MachineId], 5 file(s) in the zip, 0 leak(s))` |

The dump stays behind in both. No user or machine name is in any file of the new zip.

**Still not verified.** A Shipping crash: it has no `debug crash` and writes no log, and no shipping crash path was added. A package that ships a crash reporter. A crash context with a `DeviceId` value: this machine's had none. Where Windows Error Reporting keeps its own record of a crashed process, if it keeps one: not looked for.

## Disk

`E:` had 0.04 GB free when the first package build reached its Shipping link (`LNK1180`). This worktree's build intermediates and its `Saved\Package`, `Saved\StagedBuilds` and `Saved\Cooked` were moved to the session's scratch folder on `C:` behind junctions, which freed 4.2 GB on `E:`. The gate's `shipping` step needs about 2 GB there, so a gate on `E:` as it stands can fail on space and not on the code.

## Gate and hashes

`& Tools\Gate.ps1 -Base dev/s3` at this branch's tip; its line, RestartCheck's hash and the five bot arms on seed 9107 are in the report of the tip. The only game source touched is `Meta/DTMetaSubsystem.cpp`, two debug commands compiled out of Shipping; the rest is tests, tools and these docs. The package's bot run on seed 9107 read `276faf8e` in both configs (H above).
