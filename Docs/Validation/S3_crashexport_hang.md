# S3: CrashExportCheck survives the engine's crash hang (T-PKG-15)

**Build:** none. Tools and documents only, on `s3/crashexport-hang` from `dev/s3` 00332f5. No engine, game or package was launched by this job.

## Why

`Tools/CrashExportCheck.ps1` crashes the Development package on the engine's own crash path, which keeps its in-process minidump (R-PKG-16, D-065). That write can stall with every other thread suspended. The check launched on the line `S1_crash_exit.md` names as one that brings the hang back (the map first, `-nopause`): 5 of 10 such launches hung there, and `S3_package.md` read the check's own launch failing to end three times out of three, printing `CRASHEXPORT: FAIL (the crash launch did not end in 180 s)`. That FAIL says nothing about the export's redaction, which is what the check is for.

## The facts it rests on (all from `S1_crash_exit.md`)

1. **The line that never hung.** "With sound off, my default launch line never hung (40 launches)." That default is CrashExitCheck's, "`-windowed -RenderOffScreen -nosplash -ResX=1280 -ResY=720 -culture=en -DTSeed=770099`, `-unattended` and `-nosound` unless dropped, then the crash", and "each launch gets its own `-userdir` and log". The crash it used on Development is `debug crash` through `-ExecCmds`, the same one this check uses. Of the 40, 20 were Development `debug crash` launches (495f727, 20 of 20 ended), and the other 20 were Shipping with the throwaway switch. All 40 were on `main`'s code, not `dev/s3`'s.
2. **A hung launch's crash context.** The table of the first reading: the hung launches held "`CrashContext.runtime-xml` 55 to 58 KB, `UEMinidump.dmp` **0 bytes**", the same size as the ended ones, and the note reads "Every hung launch has a full crash context and a 0-byte dump". `S3_package.md` saw the same on `dev/s3` (58 KB and a 0-byte dump). Neither note says the file was opened or parsed. So "whole" is something the check tests by itself: the file exists, no process holds it (the check can open it with no sharing), and it parses as XML, so its root's closing tag is there. The first real reading of that test is owed on a package.
3. **How long an ended launch took.** The note gives no time per ended Development launch. What it does say: every launch it counted as ended exited inside its 60 s timeout (30 s where marked); the rerun with `-DTOfferAutoPick` "crashed and ended all ten in 4.2 to 4.5 s" (Shipping, crash at 3 s); the fix's Shipping game "closes in about 2 s". The engine's own wait for the crash handler is 60 s (`WaitUntilCrashIsHandled`), and in a hang that thread is suspended anyway. 60 s is the window the note's readings used and no ended launch came near it; the figures do not show it is too short, nor give a number to shorten it to. It stays 60 s.

None of the three contradicts the scheduler's ruling (2026-11-12 03:12), so the check was built as ruled.

## What changed

- `Tools/CrashExportCheck.ps1`: the launch line is CrashExitCheck's default, with no map first and no `-nopause`. Each launch gets its own account folder and `-userdir`, `Saved\CrashExport\<n>\account\Dusktracer`, so one launch's leftovers are never read as another's. `-Timeout` (default 60, was 180) is now per launch. A launch still running then is ended by its PID (`Stop-DTHidden`). The check then writes a record of it and asks `Tools/crash_export.py step` what to do: read that launch's context if it is whole, else launch again, three launches at most. What the check proves is unchanged: the ids come from the real context, the export runs with `%LOCALAPPDATA%` pointed at that launch's account, the zip's context reads `<id>`, the zip carries no dump, and no file in it holds an id value or a user or machine name. `-Export <other>` reads the same staged crash again (`Saved\CrashExport\chosen.txt` names its launch).
- `Tools/crash_export.py` (new, stdlib): the decision and the lines, from canned records, as `crash_exit.py` does for CrashExitCheck.
  - `crashexport: launch <n> ended in <s> s, exit <code>; context whole|partial (<why>)|absent`, or `... hung at <s> s and was ended by its PID; ...`
  - `CRASHEXPORT: PASS (<summary as before>; <k> launch(es), <h> hung and ended by PID)`
  - `CRASHEXPORT: FAIL (<summary>; <k> launch(es), <h> hung and ended by PID; first: <failure>)` when the redaction fails
  - `CRASHEXPORT: FAIL (no launch of 3 gave a whole CrashContext.runtime-xml: <h> hung, <e> ended without one)`
  - A hung launch is never a failure on its own. Exit codes are as before.
- The engine saves the context with `AutoDetect` (`GenericPlatformCrashContext.cpp`, `SerializeAsXML`): UTF-16 with a BOM when a character is not ANSI, otherwise 8-bit, and the declaration always says UTF-8. The reader decodes the bytes itself before parsing, so a whole file in either form does not read as torn.
- `Tools/tools_test.py`, class `CrashExport`: the first launch ends with a context; the first hangs with a whole context; the first hangs with none and the second ends with one; three launches and no context; a partial context (cut, empty, still held open) is not read as whole; a hung launch alone never yields FAIL; the step and result commands on a work folder; the script's launch line and its stop by PID.
- `Docs/02_Systems/package.md` T-PKG-15: one sentence. No new id.

Tool tests: `TOOLS: PASS (344 tests)`.

## The package reading is owed

This job ran no Development package of `dev/s3`. How the check behaves on a real crash is not verified: whether a hung launch's context passes the whole test, how many launches a run takes, and the zip from a hung launch's folder, which has no copied log beside the context. The reading, in a worktree of the branch with the heavy slot, after `git lfs checkout` and a build:

```
& Tools\Package.ps1 -Configs Development
& Tools\CrashExportCheck.ps1
```

The first command builds the package the check refuses to run without. The second prints the `CRASHEXPORT:` line.
