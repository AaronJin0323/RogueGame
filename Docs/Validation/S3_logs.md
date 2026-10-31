# S3 log export: a tester hands back one file

Package `53d3547` on `dev/s3-logs`, Shipping and Development, from a clean tree. Rules: `02_Systems/package.md` R-PKG-06, R-PKG-11, R-PKG-12. Every launch was `-RenderOffScreen -nosplash`, no window.

## Result
`Export_Logs.bat` replaces `ExportTelemetry.bat` in the package. A double-click makes `Dusktracer_Logs_<yyyy-MM-dd_HHmmss>.zip` on the desktop and prints the path. `EXPORTCHECK: PASS (zip 16,197 KB, 146 files, 146 scanned clean, raw log named the user 4 times, 0 game processes)`.

**A real tester's zip holds no game log.** Shipping writes none (R-PKG-02), and the installed engine has no Shipping libraries with logging on, so this cannot be switched on from the project. Their zip holds run files, and the manifest says why. The log path was proved on the Development build, the only build that writes one. A Shipping crash still leaves nothing to read.

## What was run
`Tools/ExportLogsCheck.ps1`, seed 770088, 15 s of bot.

| Step | Reading |
|---|---|
| Shipping run from the package | CSV and JSONL in `%LOCALAPPDATA%` |
| Development run from a copy under `%TEMP%`, in a folder with a space | `Saved\Logs\Dusktracer.log`, 91,551 bytes |
| Raw log before the export | names the user 4 times, the machine, and `MachineId` and `loginid` with the same value |
| `Export_Logs.bat` through `cmd`, with `-OutDir`, polled every 100 ms | 0 game processes of either package |
| Report | `EXPORT: <path> (72 runs, 1 logs, 8 names redacted)`, then one line in English and one in Chinese |
| Unzip | 146 files; the 4 run files of this check hash equal to their sources; `MANIFEST.txt`; only `.csv`, `.jsonl`, `.log`, `.txt` |
| Readers on the exported runs | `power_split.py`: `POWER: n/a` on both, no player at the check level in 15 s; `pressure.py` rc 0 on both |
| Name scan, UTF-8 and UTF-16, every file and every path in the zip | no user name, machine name, machine id or raw profile path |
| Exported log | `C:/Users/<user>/AppData/...`, `LogInit: User: <user>`, `LogInit: Computer: <machine>`, `MachineId=<id>`; `Run start` kept |

Without `-OutDir`, the Shipping package's `.bat` wrote to `%USERPROFILE%\Desktop\` and reported that path, 70 runs, 0 logs. The test zip was then deleted.

`Tools/PadCheck.ps1` on the same package: `NOT RUN (package 53d3547 ok, en+zh-Hans, 0 windows; real pad needs a human ...)`, with the export half passing through the new script.

`Tools/tools_test.py` `ExportLogs` holds the redaction in the gate on a made-up profile: a Chinese user name, forward, back and doubled slashes, an 8.3 path, a GBK log, a short name beside `Global`.

## Found while proving it
- The engine logs a stable per-machine id as `MachineId` and `loginid`. It is not a user name, but it would have let two zips be tied to one PC, so it became `<id>`.
- A word-edge match missed a two-character Chinese name inside `张三的游戏`: Chinese has no word edges. Only short Latin names keep them.

## Frame cost in the package
`Tools/Perf.ps1 -Package`, the reading D-021 took by hand. One run each; other worktrees were not checked for GPU use, and `gpu.lock` is per worktree.

| Scene | Game p50 | Game p95 | GPU p95 raw | Proxied | Load | Verdict |
|---|---|---|---|---|---|---|
| Caps | 1.41 ms | 1.84 ms | 2.48 ms | 3.96 ms | 5975 bullets, 388 enemies | PASS |
| S2 | 2.88 ms | 4.95 ms | 3.27 ms | 5.24 ms | 5809 bullets, 400 enemies | PASS |

Budget: game 8 / 10 ms, GPU 12 ms. The change is about ten lines, so it went in.

## Not proved
- A person double-clicking the file: Explorer, the console window staying open at `pause`, and how the Chinese line draws on a console with an older font. The `.bat` ran through `cmd` with no console.
- A real Chinese Windows account: the test sets the environment on a made-up profile.
- A redirected desktop (OneDrive): the path comes from the shell folder call, not checked on such a PC.
- A real crash folder: the crash handling is proved on a made-up folder only, since no crash happened.
- Free text in a log that names a person some other way, such as a display name that differs from the account name.
