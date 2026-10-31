# Package — the build validators run

## Intent
Validators play without the editor (O-008, `Validation/S1_kit.md`). One script makes a Win64 folder a host can copy to any PC, one launcher starts a run in either language, and one double-click hands the run data and the game log over. The build never sends anything anywhere: the host carries the zip by hand. This spec covers the build, the launchers, the hand-over and what a machine can prove about them; `telemetry.md` owns what a run writes.

## Rules
- **R-PKG-01** `Tools/Package.ps1` is headless and needs no human: it refreshes the text (`Tools/Localize.bat`), builds the editor target (the cook runs inside it) and runs `BuildCookRun` for `Win64`, opening no window. Output stays under the git-ignored `Saved/Package/<Config>/Windows/`; nothing binary is committed, uploaded or published.
- **R-PKG-02** The validator build is **Shipping**: frame times as a player gets them, no `DT.*` console (R-TEC-12), no developer lines on the HUD (language check, frame times), no engine log, so no Windows user or machine name in anything it writes. The `Test` config is not an option: the installed engine has no `UnrealGame` Test libraries. Shipping cannot show what it drew, so a **Development** build staged from the same cook is the proof build (R-PKG-07); it is never handed to a validator.
- **R-PKG-03** The cook carries what `LoadObject` reaches by string and the cook cannot see: every asset under `/Game/DT`, and `/Engine/BasicShapes`. The ICU preset is `CJK`, or `-culture=zh-Hans` falls back to English in a packaged build. `en` and `zh-Hans` are staged (`CulturesToStage`), with the generated string tables as UFS files.
- **R-PKG-04** The package holds `Play_en.bat` and `Play_zh-Hans.bat`: full screen, director on, bot off, and a fresh `-DTSeed` per launch, so a tester's second launch is not the first one's seed. Within a launch, Restart run takes a fresh seed (`settings.md` R-SET-23).
- **R-PKG-05** Telemetry lands where `R-TEL-07` says, which for a Shipping package is `%LOCALAPPDATA%\Dusktracer\Saved\Playtests` (Shipping saves to the user directory) and for the Development proof build `Saved\Playtests` inside the package. An exit mid-run closes the run: `run_end` with `reason` `quit`. A run with no `run_end` is a crash or a killed process.
- **R-PKG-06** `Export_Logs.bat` makes one dated zip, `Dusktracer_Logs_<yyyy-MM-dd_HHmmss>.zip`, on the desktop: the run files (`*.csv`, `*.jsonl`) byte for byte, `Saved\Logs\*.log` and the text files of `Saved\Crashes`, with a `MANIFEST.txt` listing them. It prints the path and the tester's next step, in both languages. It starts no game. No script or build transmits anything. The crash reporter is not staged.
- **R-PKG-07** `Tools/PadCheck.ps1` judges a package off-screen, in both cultures, with no window on the desktop. On the Shipping build: the bot plays past the first CSV interval and quits the game (`run_start` with the right `build` and `culture`, `run_end` `bot_timeout`), the export zip holds that run and `Tools/power_split.py` reads the exported files. On the Development build: `Run start` in the log, `Test.LocRoundTrip` drawn equal to `ST_UI.csv`, no crash, ensure or `LogDT` error, the screenshot passes `Tools/image_check.py`, and the exit mid-run leaves `run_end` `quit`. `-RenderOffScreen` creates no window, so no close message can reach the game: Alt+F4 itself is a human check.
- **R-PKG-08** A package has a stamp, `Saved/Package/package_stamp.json`: commit, dirty flag, configs, cultures, size. The gate step `pad` judges only a clean package whose commit has the same `Source`, `Content` and `Config` as `HEAD`; a missing, dirty or stale package is `NOT RUN`, a failed check is a gate failure.
- **R-PKG-09** A real pad needs a human: no machine check counts as one. `pad` stays `NOT RUN` until a run of this package exists whose CSV has `input_device` `pad`; only then is it `PASS`. A simulated input is never that evidence.
- **R-PKG-10** `Tools/PackageRestartCheck.ps1` proves Restart run (`settings.md` R-SET-23, R-SET-24) in the package, off-screen. On the Development build, with real rendering: a restarted run matches a fresh launch by input hash, steps and CSV, the old file ends `restart`, the new file opens on `run_start`, the log holds no warning or error, real keys work after the reload, and a language and a volume changed in the menu survive it. Shipping has no hook that reaches a restart, so the check bridges the two builds: their `.utoc` and `.ucas` hashes match, and a fresh Shipping run's CSV equals the restarted Development run's. It is run by hand after `Tools/Package.ps1`, not by the gate.
- **R-PKG-11** The export redacts every text log it carries: any path under `\Users\` loses its next segment to `<user>` (either slash, doubled, short DOS form, any encoding), the Windows user name and machine name are replaced wherever they appear, and the engine's `MachineId`, `DeviceId` and `loginid` values become `<id>`. A short Latin name keeps word edges so `Al` leaves `Global` alone. Run files are never rewritten: they hold no name (§Data). A crash dump is binary, cannot be redacted, and stays behind; the manifest counts it. `Tools/ExportLogsCheck.ps1` proves this on a package and is run by hand after `Tools/Package.ps1`.
- **R-PKG-12** `Tools/Perf.ps1 -Package` reads the frame cost in the Development package with the same scenes, budgets and proof as the editor reading, for a change the editor cannot see (D-021). `perf.csv` marks the row `[package]`. Shipping has no CSV profiler (R-PKG-02).

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| CSV interval the check waits for | C.TEL.INTERVAL_S | CORE | R-PKG-07 |
| Screenshot delay of the proof run | C.SMOKE.SECONDS | CORE | R-PKG-07 |

## Data
- `package_stamp.json`: `commit`, `dirty`, `configs`, `cultures`, `size_bytes`, `wall`.
- A run's two files hold gameplay only (`telemetry.md` §Data): seed, build hash, culture, last input device, per-interval combat numbers, cards offered and picked, Keeper and weapon choices, and three wall-clock fields (`run_start.wall`, `offer_pick.wall_decide_s`, `run_end.wall_paused_s`). No name, account, machine name, path, hardware, IP or free text. The file name is the seed and the local start time.

## Acceptance
- T-PKG-01 `Tools/Package.ps1`: exits clean and prints `PACKAGE: PASS`, with both configs under `Saved/Package/`.
- T-PKG-02 `Tools/PadCheck.ps1` Development half: drawn text, log, screenshot and `run_end` `quit`.
- T-PKG-03 `Tools/PadCheck.ps1` cultures: `run_start.culture` and the drawn text are right in en and zh-Hans.
- T-PKG-04 `Tools/PadCheck.ps1` launchers: both `Play_*.bat` are in the package, with their culture and a fresh seed.
- T-PKG-05 `Tools/PadCheck.ps1` Shipping half: the run lands in `%LOCALAPPDATA%`, with `run_start`, `run_end`, a closed interval and the right `build`.
- T-PKG-06 `Tools/PadCheck.ps1` export: the zip holds that run and its `MANIFEST.txt`, no file type but run files, logs and text, and `power_split.py` reads the exported files.
- T-PKG-07 `Tools/PadCheck.ps1` windows: no visible window of the game's process during the check.
- T-PKG-08 `Tools/Gate.ps1` step `pad`: `NOT RUN` without a current clean package; a failed check fails the gate.
- T-PKG-09 `Tools/PadCheck.ps1` human half: `NOT RUN` with no `pad` run of this build, `PASS` only with one.
- T-PKG-10 `Tools/PackageRestartCheck.ps1`: prints `PKGRESTART: PASS` (R-PKG-10). Readings: `Validation/S2_package.md`.
- T-PKG-11 `Tools/ExportLogsCheck.ps1`: a Shipping run and a Development run from a copy inside the user profile, exported through the `.bat` with no game process: the runs equal their sources, the log holds `Run start` and `<user>`, no file holds a name, a machine id or a raw profile path, `power_split.py` and `pressure.py` read both runs; prints `EXPORTCHECK: PASS`. Tests `ExportLogs` in `Tools/tools_test.py` hold the redaction on a made-up profile in the gate. Readings: `Validation/S3_logs.md`.
- T-PKG-12 `Tools/Perf.ps1 -Package`: prints `PERF: PASS` on the caps scene and on `-Scene S2` in the package (`Validation/S3_logs.md`).
- HUMAN: a validator double-clicks `Export_Logs.bat` and sends the file back; a validator on a gamepad plays this package and `input_device` reads `pad`; full-screen, Alt+F4 leaving `run_end` `quit` in Shipping, Restart run pressed in Shipping leaving a `restart` file and a new file in `%LOCALAPPDATA%`, and the look of `zh-Hans` cards on a real display.

## Telemetry
- Adds none. The check reads `run_start`, `run_end` and the CSV `input_device` column (`telemetry.md`).

## Text
- None in the game. The launchers and `README.txt` are host tools, not player text; the kit carries the one sentence in both languages. `Export_Logs.ps1` prints its one instruction in en and zh-Hans itself, because a tester reads it; it is a tool line, not a string table row.

## Out of scope
- Installers, stores, signing, upload, update, save data, Mac (D-007).

## Open
- The console key still exists in the Development proof build; it is never handed out.
- Shipping writes no log (R-PKG-02), and the installed engine's Shipping libraries cannot be built with logging on, so a real tester's export holds run files only, and its manifest says so. A Shipping crash leaves no log: the host notes the time and the run has no `run_end`. A validator crash can be replayed on the proof build with the seed from the file name.
