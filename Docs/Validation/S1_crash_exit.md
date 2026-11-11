# S1: does the packaged game end after a crash?

**Build:** none read yet. Phase 1 read the engine source and another agent's six logs; no package was built or launched. Each reading below will get its own `Build:` line.

## The question

Session A puts three outside players on the Shipping package of `main`. If a tester's game crashes and freezes instead of closing, they sit in front of a dead full-screen window. Another agent saw a Development package of `dev/s3` 81c638e hang on 3 of 6 crashes (`debug crash`, `-unattended`, each in a fresh `-userdir`). This note asks four things:

1. Does `main` hang, on the Development package?
2. Does Shipping hang?
3. Where does it stall?
4. What removes it?

The kit already tells the host what to do with a frozen game (`S1_kit.md`, "If the game freezes").

## The six logs (read only, another agent's worktree)

All six crash at the same line: `appError called: Fatal error: ... Crashing the gamethread at your request`. They then split in two.

| Launch | Log after the crash | `Saved\Crashes\<guid>\` |
|---|---|---|
| crash_in, crash_in_3, crash_out_2 (ended) | `Could not start crash report client using ../../../Engine/Binaries/...`, `=== Critical error: ===`, the callstack, `Executing StaticShutdownAfterError`, `RequestExit(1, LaunchWindowsStartup.ExceptionHandler)` | `CrashContext.runtime-xml` 55 to 58 KB, `UEMinidump.dmp` 1.36 to 1.59 MB |
| crash_in_2, crash_out, crash_out_3 (hung) | nothing after `Windows GetLastError: The operation completed successfully. (0)` | `CrashContext.runtime-xml` 55 to 58 KB, `UEMinidump.dmp` **0 bytes** |

So a hung launch has made its crash folder, written the crash context in full, and opened the minidump file, and then never writes a byte into it. In an ended launch the render thread still logs about 0.7 s after the crash (`LogRHI: ... new graphics PSO`) before the callstack appears: the other threads keep running while the crash is being handled.

## The crash path of a packaged game (engine source `E:\UE5\UE_5.8\Engine\Source`, read only)

Paths below are under `Runtime/Core/Private/Windows/` unless named. The launcher engine is no licensee build (`Engine/Build/Build.version`: `"IsLicenseeVersion": 0`).

1. **The crashing thread.** `ReportCrash` (`WindowsPlatformCrashContext.cpp:1826`) flushes the log (`GLog->Panic()`, :1836). It signals the crash reporting thread once (`OnCrashed`, :1841, :1510-1516) and waits for it in `WaitUntilCrashIsHandled` (:1845, :1519-1524). That wait has a 60 s timeout (`DefaultCrashHandlingTimeoutSecs`, :112), which `[CrashReportClient] CrashHandlingTimeoutSecs` in the engine ini can change (:191-200). When the wait returns, the guard in `LaunchWindows.cpp:283-294` runs `LaunchStaticShutdownAfterError` and `RequestExit`, the end the ended logs show.
2. **The crash reporting thread.** It is made at startup with below-normal priority (`FCrashReportingThread`, :1399-1403) and handles the crash in `HandleCrashInternal` (:1570). It first records every thread's stack (`CaptureAllThreadContexts`, :1663). That suspends and resumes one thread at a time (`WindowsPlatformStackWalk.cpp:377-403`), so it leaves nothing suspended. Then, because `GUseCrashReportClient` is true (`HAL/ExceptionHandling.cpp:24`), it calls `ReportCrashUsingCrashReportClient` (:1669, :969). The `#if USE_CRASH_REPORTER_MONITOR && WITH_EDITOR` branch (:1633) is compiled only into editors, so a packaged game never hands the crash to an out-of-process reporter.
3. **The report, in process.** A dialog is skipped when the game runs `-unattended` (:977) or when the console variable `WindowsPlatformCrashContext.ForceCrashReportDialogOff` is set (:978). `bSendUnattendedBugReports` stays true: its default is true (:989-993), and the licensee clause that would clear it (:1005-1010) does not apply. So the reporter path runs even under `-unattended`:
   - it makes `Saved\Crashes\<guid>` (:1028-1030) and writes `CrashContext.runtime-xml` (:1034);
   - it calls `CopyPlatformSpecificFiles` (:1040), which writes `UEMinidump.dmp` through `WriteMinidump` (:419) **in this process, on the crash reporting thread**, with `MiniDumpWriteDump` (:183). The dump type is `MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory` (:176-179);
   - it copies the log (:1050) and looks for `CrashReportClient.exe`, which the package does not stage (`package.md` R-PKG-06), so it logs `Could not start crash report client` (:1143);
   - without `-unattended` it then shows the dialog "The ... has crashed and will close" (:1150-1157) and waits for a click.
4. **Where it stalls.** The hung launches have a full `CrashContext.runtime-xml` and a 0-byte `UEMinidump.dmp`. Step 3 writes the context, then opens the dump file and calls `MiniDumpWriteDump`, so the stall is inside `MiniDumpWriteDump` at :183, on the crash reporting thread.
   - The reported state fits this: "about 100 threads, all but one suspended". `MiniDumpWriteDump` called on its own process suspends the process's other threads while it reads them. This comes from the Windows documentation, not the engine source; Microsoft recommends calling it from another process.
   - If a thread it suspended holds a lock the writer then needs (the process heap, the loader lock), the writer waits forever. The render thread logging during the handling (above) shows other threads are busy at that moment, which makes this a race: about half the launches lose it.
   - The game thread's 60 s timeout cannot rescue the process: that thread is one of the suspended ones. No thread inside the process is left to end it.

## What the source says can matter (Q3 varies these, ten launches each)

| Lever | Where it is read | What it can change |
|---|---|---|
| `-unattended` | :977, `LaunchWindows.cpp:211-214` | the dialog; it never skips the dump |
| `-nosound` | nowhere on this path | nothing expected; read once to show that |
| `WindowsPlatformCrashContext.ForceCrashReportDialogOff=1` (console variable, `[ConsoleVariables]`) | :978 | makes `bNoDialog` true without `-unattended` |
| `bSendUnattendedBugReports=False` in `[/Script/UnrealEd.CrashReportsPrivacySettings]` of the editor-settings ini | :992, :1013-1016 | together with `bNoDialog`: no reporter path at all, so no crash folder, no dump, no dialog. Whether a packaged game reads that ini is a reading to take |
| `WindowsPlatformCrashContext.MinidumpIncludeReferencedMemory=0` | :124-129, :176 | a smaller dump; it is still written in process |
| `[CrashReportClient] CrashHandlingTimeoutSecs` | :198 | nothing for a hang: the waiting thread is suspended |
| `-noexceptionhandler` | `LaunchWindows.cpp:235`, `:261` | drops the outer SEH guard; the unhandled-exception filter (:1452, :1810) still calls `ReportCrash` |
| `CrashReportClient` staged | :1116-1130 | the dump is still written in process first (:1040, before :1130), and the client could send a report, which R-PKG-06 forbids |

A project-side watchdog thread would be suspended by `MiniDumpWriteDump` like every other thread. Only something outside the process can end a hung game: a launcher that waits on it, or a person with Task Manager.

## Method

- **Tool.** `Tools/CrashExitCheck.ps1 -Config Development|Shipping -Runs 10 -TimeoutS 60`. It takes a machine slot (`Enter-DTHeavy`) and launches the package of this worktree's `Saved\Package` on the hidden desktop (`Start-DTHidden`), with these launch arguments:
  - `-windowed -RenderOffScreen -nosplash -ResX=1280 -ResY=720 -culture=en -DTSeed=770099 -unattended -nosound`
  - a fresh `-userdir` and `-ABSLOG` under `Saved\CrashExit\<stamp>\<n>`
  - the crash arguments (default `-ExecCmds="debug crash"`) and any `-Extra`.

  It waits up to 60 s per launch. A launch still alive then is recorded and ended by PID: thread count, how many are suspended, the threads that are not, the dump's size, visible windows on the hidden desktop, the log's last line. `-NoUnattended` and `-WithSound` give the two command-line variations. It refuses a package under the main checkout's `Saved\Package(s)`, the session A build. Lines and verdict come from `Tools/crash_exit.py`, tested in `Tools/tools_test.py` (`CrashExit`).
- **Q1.** `main`'s Development package, ten launches, and ten more if the first ten all end.
- **Q2.** Shipping, through a switch on a throwaway branch (`scratch/crash-switch`) that is never handed over: ten launches with a fatal error, ten with a write through a null pointer. The diff will be quoted here.
- **Q3.** The levers above, one at a time, ten launches each. For one hung launch, its threads' states and wait reasons. No debugger that can list a live process's stacks was found on this PC (no `cdb.exe` or `windbg` in the Windows Kits folders or on the PATH), and nothing is installed.
- **Q4.** The options with their readings, then a recommendation.

## Readings

Phase 2.
