# S1: does the packaged game end after a crash?

**Build:** each reading below names the commit it read. Every package was built in this job's own worktree with `Tools/Package.ps1`: `495f727` (`main` c945547 plus the tool), `f15ef0a`, `1c74e37` and `0d3f1c0` (throwaway `scratch/crash-switch`, never handed over), and `9c704d4` (`s1/crash-exit`). The handed-over fix is `049721f` on `s1/crash-exit`.

## Answer

Yes, the packaged game hangs after a crash, on `main`, in both configurations. The engine writes its crash dump inside the crashing process. With every other thread suspended, that write sometimes stalls forever, and nothing in the process can end it.

- **Who meets it.** With sound on, as a tester plays, it stalled in 5 to 8 of 10 crashed Development launches and in 3 to 5 of 10 Shipping crashes that came right after the run started.
- **The fix.** `s1/crash-exit` 049721f turns that step off in Shipping: a crash then writes nothing, shows nothing, and the process exits. It ended every crashed Shipping launch it was given (20 fatal, 10 null write), and 20 of 20 Development launches given `-DTCrashNoDump`. It is the rule `package.md` R-PKG-16, tested by T-PKG-16.
- **The host's side.** The kit's freeze line (`S1_kit.md`, "If the game freezes") stays as the host's answer for anything else that hangs.

## The six logs of the first reading (read only, another agent's worktree, `dev/s3` 81c638e)

All six crash at the same line: `appError called: Fatal error: ... Crashing the gamethread at your request`. Their launch line was the map `/Game/DT/Maps/L_Arena_Test` first, then `-windowed -RenderOffScreen -nosplash -ResX=1280 -ResY=720 -unattended -nopause -nosound -culture=en -DTSeed=770099 -ExecCmds="debug crash"`.

| Launch | Log after the crash | `Saved\Crashes\<guid>\` |
|---|---|---|
| crash_in, crash_in_3, crash_out_2 (ended) | `Could not start crash report client ...`, `=== Critical error: ===`, the callstack, `Executing StaticShutdownAfterError`, `RequestExit(1, LaunchWindowsStartup.ExceptionHandler)` | `CrashContext.runtime-xml` 55 to 58 KB, `UEMinidump.dmp` 1.36 to 1.59 MB |
| crash_in_2, crash_out, crash_out_3 (hung) | nothing after `Windows GetLastError: The operation completed successfully. (0)` | `CrashContext.runtime-xml` 55 to 58 KB, `UEMinidump.dmp` **0 bytes** |

## Where it stalls (engine source `E:\UE5\UE_5.8\Engine\Source`, read only)

Paths are under `Runtime/Core/Private/Windows/` unless named. The launcher engine is no licensee build (`Engine/Build/Build.version`: `"IsLicenseeVersion": 0`).

1. **The crashing thread.** `ReportCrash` (`WindowsPlatformCrashContext.cpp:1826`) flushes the log (:1836) and signals the crash reporting thread (:1841, :1510-1516). It then waits for that thread in `WaitUntilCrashIsHandled` (:1845, :1519-1524), with a 60 s timeout (:112, :191-200). When the wait returns, `LaunchWindows.cpp:283-294` runs `LaunchStaticShutdownAfterError` and `RequestExit`.
2. **The crash reporting thread** (`FCrashReportingThread`, :1399-1403) handles the crash in `HandleCrashInternal` (:1570).
   - It records every thread's stack, one thread suspended and resumed at a time (:1663; `WindowsPlatformStackWalk.cpp:377-403`).
   - Because `GUseCrashReportClient` is true (`HAL/ExceptionHandling.cpp:24`), it then calls `ReportCrashUsingCrashReportClient` (:1669, :969).
   - The branch that hands the crash to an out-of-process reporter is compiled into editors only (:1633).
3. **The report, in process.** `bSendUnattendedBugReports` stays true, so the reporter path runs even under `-unattended` (:989-1016). It:
   - makes `Saved\Crashes\<guid>` and writes `CrashContext.runtime-xml` (:1028-1034);
   - writes `UEMinidump.dmp` through `CopyPlatformSpecificFiles` → `WriteMinidump` (:1040, :419), **in this process, on the crash reporting thread**, with `MiniDumpWriteDump` (:183);
   - copies the log (:1050) and looks for `CrashReportClient.exe`, which is not staged (:1116-1143).
   Without `-unattended` it then opens a dialog (:1150-1157). In a packaged game that dialog cannot appear (see the readings).
4. **The stall.** Every hung launch has a full crash context and a 0-byte dump, so it stops inside `MiniDumpWriteDump` (:183).
   - **Thread names** (read with `GetThreadDescription` on 8 hung launches of `1c74e37`): the one thread not suspended is `CrashReportingThread`, in a wait Windows reports as `Unknown`. Every other thread is suspended, every time: `GameThread`, `RenderThread 0`, `RHIThread`, the workers, `AudioMixerRenderThread(1)`, `AudioMixerNullCallbackThread` and `WasapiServiceWatcher`.
   - **The mechanism (from Windows' documentation, not this source).** `MiniDumpWriteDump` on its own process suspends the other threads while it reads them, and it waits forever if a suspended thread holds a lock it needs.
   - **The game thread's 60 s timeout cannot help:** that thread is suspended too.
   - **What the readings cannot show:** which lock it is, or which thread holds it. No debugger that lists a live process's stacks is on this PC, and none was installed.

**Why no ini or launch-argument route turns it off.**
- `bSendUnattendedBugReports` is read from the editor-settings ini (:992). That ini is loaded only `WITH_EDITOR` (`ConfigCacheIni.cpp:6637-6656`), so a packaged game reads an empty file name and the value stays true.
- In Shipping, `-ini:` overrides are compiled out (`ConfigCacheIni.h:57`).
- `ForceCrashReportDialogOff` (:978) only drops the dialog.
- `MinidumpIncludeReferencedMemory=0` (:124-129, :176) shrinks the dump, and the readings show it still hangs.
- `CrashHandlingTimeoutSecs` (:198) times a thread that is suspended.
- Staging `CrashReportClient` writes the dump in process first (:1040 comes before :1130), and the client could send a report, which R-PKG-06 forbids.

**What the fix does.** In the game module's startup (`Source/Dusktracer/Dusktracer.cpp`, Windows only, never an editor):
- `GUseCrashReportClient = false` sends the handler to :1671-1675, where it only writes `MiniDumpFilenameW`.
- `MiniDumpFilenameW[0] = 0` leaves `WriteMinidump` no file to open (:141-146), so `MiniDumpWriteDump` never runs.
- The engine sets `MiniDumpFilenameW` in `FEngineLoop::AppInit` (`LaunchEngineLoop.cpp:6504`, from `:6518`), before game modules load, and nowhere after.
- A crash before the game module has loaded still takes the engine's path.

## Readings

`CrashExitCheck.ps1` launches each build on the hidden desktop. Each launch gets its own `-userdir` and log under `Saved\CrashExit\<stamp>\<n>`, and these arguments: `-windowed -RenderOffScreen -nosplash -ResX=1280 -ResY=720 -culture=en -DTSeed=770099`, `-unattended` and `-nosound` unless dropped, then the crash.

Each launch counts as one of three outcomes:
- **ended:** the process exited;
- **hung after a crash:** alive at the timeout, with a crash folder, a dump or the fatal line;
- **void:** alive with no sign of a crash.

The timeout is 60 s, or 30 s where marked. "Run start" is `debug crash` through `-ExecCmds`, about 20 ms after the map loads. On Shipping the crash comes from the throwaway switch: `-DTCrashAfter=<s>` with `Fatal` (`LowLevelFatalError`) or `Null` (a write through a null pointer).

| Build | Config | Sound | `-unattended` | Crash | Argument | Launches | Ended | Hung after a crash | Void |
|---|---|---|---|---|---|---|---|---|---|
| 495f727 | Development | off | yes | `debug crash`, run start | | 20 | 20 | 0 | 0 |
| f15ef0a | Shipping | off | yes | fatal, 3 s | | 10 | 10 | 0 | 0 |
| f15ef0a | Shipping | off | yes | null, 3 s | | 10 | 10 | 0 | 0 |
| f15ef0a | Shipping | off | no | fatal, 3 s (30 s timeout) | | 10 | 0 | 0 | **10** |
| f15ef0a | Shipping | off | no | fatal, 3 s (30 s timeout) | `-DTOfferAutoPick` | 10 | 10 | 0 | 0 |
| 495f727 | Development | **on** | yes | `debug crash`, run start | | 10 | 4 | **6** | 0 |
| f15ef0a | Shipping | on | yes | fatal, 3 s | | 10 | 10 | 0 | 0 |
| f15ef0a | Shipping | on | yes | null, 3 s | | 10 | 10 | 0 | 0 |
| f15ef0a | Shipping | on | yes | fatal, 0.02 s | | 10 | 7 | **3** | 0 |
| 495f727 | Development | on | yes | `debug crash`, run start | `-ini:` `MinidumpIncludeReferencedMemory=0` | 10 | 5 | **5** | 0 |
| 495f727 | Development | off | **no** | `debug crash`, run start (30 s timeout) | | 10 | 1 | **9** | 0 |
| 1c74e37 | Development | on | yes | `debug crash`, run start | | 10 | 2 | **8** | 0 |
| 1c74e37 | Development | on | yes | `debug crash`, run start | `-DTCrashNoDump` (proof) | 20 | 20 | 0 | 0 |
| 1c74e37 | Shipping | on | yes | fatal, 0.02 s | `-DTCrashNoDump` (proof) | 20 | 20 | 0 | 0 |
| 1c74e37 | Development | on | no | `debug crash`, run start (30 s timeout) | `-DTCrashNoDump` (proof) | 10 | 10 | 0 | 0 |
| 1c74e37 | Shipping | on | yes | null, 0.02 s | `-DTCrashNoDump` (proof) | 10 | 10 | 0 | 0 |
| 0d3f1c0 | Shipping | on | yes | fatal, 0.02 s | none (the fix's default) | 20 | 20 | 0 | 0 |
| 0d3f1c0 | Shipping | on | yes | null, 0.02 s | none | 10 | 10 | 0 | 0 |
| 0d3f1c0 | Shipping | on | yes | fatal, 0.02 s | `-DTCrashDump` (control) | 10 | 5 | **5** | 0 |
| 0d3f1c0 | Development | on | yes | `debug crash`, run start | `-DTCrashNoDump` | 20 | 20 | 0 | 0 |
| 0d3f1c0 | Development | on | yes | `debug crash`, run start | none (control) | 10 | 5 | **5** | 0 |
| 0d3f1c0 | Shipping | on | yes | fatal, 70 s | none | 1 | 1 | 0 | 0 |
| 0d3f1c0 | Shipping | on | yes | fatal, 70 s | `-DTCrashDump` | 2 | 2 | 0 | 0 |
| 9c704d4 | Development | off | yes | `debug crash`, run start, the map first and `-nopause` | | 10 | 5 | **5** | 0 |

- **The void run.** Without `-unattended`, the Patron hand waits for a person, so the run never starts and the 3 s switch never fires. Those ten launches read nothing about a hang. The rerun with `-DTOfferAutoPick` crashed and ended all ten in 4.2 to 4.5 s.
- **Proof of the fix.** In every launch with the fix in force, no `Saved\Crashes` folder and no `.dmp` file exists anywhere in the launch's userdir. The Development log says `Crash handling: no crash folder and no minidump; the process ends`, then shows the fatal line, `Critical error` and `StaticShutdownAfterError`.
- **What brings the hang back.** With sound off, my default launch line never hung (40 launches). The first reading's own line (the map first and `-nopause`) brings it back: 5 of 10 hung. Sound on, a crash right after load, or no `-unattended` each bring it back too. All of these move what the other threads are doing at the moment of the crash, so a tester's crash can meet it at any time.
- **Dialogs.** None appeared on the hidden desktop in any launch. Without `-unattended`, the Development log of the one launch that ended reads `Failed to create dialog. The specified resource type cannot be found in the image file. Error: 0x715 (1813)`, then `Message dialog closed, result: Ok, title: The UE-Dusktracer Game has crashed and will close`.
  - The packaged exe holds no dialog template, so `FWindowsDialog::Show` fails before it shows anything (`WindowsPlatformMisc.cpp:2000-2024`). This does not depend on the desktop.
  - A tester on a visible full-screen game therefore never sees a crash dialog: the game closes, or, before the fix, it freezes.
  - What decides whether the dialog is tried: `-unattended` (:977), `ForceCrashReportDialogOff` (:978), and `WindowsPlatform.DisableMessageBox` (`WindowsPlatformMisc.cpp:132-137`, :2119-2123).

**What a host gets back after a crash.** The same seed, crashed early (0.02 s) and late (70 s, after the hero had already died at 27.8 s with nobody playing):

| Launch | Run file `.jsonl` | `.csv` | `run_end` | Cut line |
|---|---|---|---|---|
| fix, early crash, ended | 196 bytes, `run_start` only | none yet | no | none |
| `-DTCrashDump`, early crash, ended | 196 bytes, `run_start` only | none yet | no | none |
| `-DTCrashDump`, early crash, hung and ended by PID | 196 bytes, `run_start` only | none yet | no | none |
| fix, crash at 70 s | 23 923 bytes, 161 records, ends `run_end` at 27.8 s | 2 lines | yes, from the death before the crash | none |
| `-DTCrashDump`, crash at 70 s (both launches) | 23 923 bytes, the same 161 records; only `run_start` and `run_end` differ, in their wall-clock fields (R-TEL-08) | 2 lines | yes | none |

The run file is the same with the fix and without. It is whole up to the last flush, every line is JSON, and what the run held in memory since its last interval or end is lost either way (R-TEL-09). A host gets back the same run file. With the fix there is no crash folder beside it.

**What a tester's PC no longer keeps after a Shipping crash:** the folder `Saved\Crashes\UECC-Windows-<guid>_0000\` under the user directory (`%LOCALAPPDATA%\Dusktracer\Saved`), with three files:
- `CrashContext.runtime-xml`, 86 to 123 KB as measured;
- `CrashReportClient.ini`, 239 bytes;
- `UEMinidump.dmp`, 1.04 to 4.03 MB, or 0 bytes in a hung launch.

The engine also leaves `Saved\Config\CrashReportClient\UECC-Windows-<guid>\CrashReportClient.ini` (239 bytes), as every launch did. `ExportTelemetry.ps1` only ever took the crash folder with `-IncludeLogs` (R-PKG-06).

## Options

| Option | A tester sees after a crash | Sends or shows | Writes, and whether it can name the user or machine | `dev/s3` R-PKG-11 and its test line | Kind | Risk |
|---|---|---|---|---|---|---|
| Kit line only (step 0) | the game freezes in about half the crashes; the host waits 15 s and ends it in Task Manager | nothing | the crash folder; the context and dump can name both | unchanged | none | a tester in front of a dead full-screen window, as often as not |
| **No in-process dump in Shipping by default (built, R-PKG-16)** | the game closes in about 2 s | nothing; no dialog can appear in any case | nothing beyond the run file | unchanged: the Development proof build keeps the engine's path and its crash folder for the redaction check of R-PKG-11 and its test line. The Development path still hangs, 5 to 9 of 10 here, so a check that crashes the proof build must expect a hang | `Source` (game module) | low: two engine globals set once at startup; a crash during engine start, before the module loads, keeps the engine's path |
| The same, by a launch argument in `Play_*.bat` only | the same, when started through the launcher | nothing | nothing | unchanged | `Source` + `PackageFiles` | a person starting `Dusktracer.exe` directly still meets the hang, and R-PKG-02 still has a crash folder to answer for |
| `MinidumpIncludeReferencedMemory=0` (`Config`) | a freeze as often (5 of 10) | nothing | a smaller dump | unchanged | `Config` | does not remove it |
| No-reporter ini keys | none: a packaged game never reads them | | | | | unreachable |
| A watchdog thread in the game | a freeze: it is suspended with the rest | | | | `Source` | cannot work |
| A watcher outside the process (a launcher that ends a game whose crash folder appeared seconds ago) | the game closes after N s | nothing | the crash folder | unchanged | `PackageFiles` | not built and not tested; a second process on a tester's PC |
| `CrashReportClient` staged | the same freeze (the dump comes first) | a report could be sent | the folder | unchanged | packaging | breaks R-PKG-06 |

**Recommendation:** the built option, Shipping off by default. `s1/crash-exit` 049721f, R-PKG-16 and T-PKG-16.
- R-PKG-02 already promises a Shipping build writes nothing that can name the user or the machine, and a crash folder can.
- A person may start the exe without the launcher.
- `-DTCrashDump` gives a developer the engine's dump back for a diagnosis.

**The kit.** Done on this branch: `S1_kit.md` "What the zip holds" now reads "This build writes no engine log and no crash dump", and its "Crash or freeze" bullet names no dump or crash folder.

## Not tested

- **The lock itself.** No debugger, so no stack of the stalled writer.
- **A real audio device and a visible desktop.** On the hidden desktop the mixer runs `AudioMixerNullCallbackThread`, which may mean a null device, so a tester's PC may stall more or less often. No launch was made on a visible desktop (the no-window rule).
- **Other PCs.** Every reading is this one PC's.
- **A Shipping crash before the game module loads.** The fix does not cover it.
- **Shipping on demand without the throwaway switch.** A handed-over Shipping build cannot be crashed on demand, so T-PKG-16 runs on Development and Shipping was proved here once.
- **`dev/s3`'s own code.** Only `main` was measured.
