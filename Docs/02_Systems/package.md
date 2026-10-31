# Package — the build validators run

## Intent
Validators play without the editor (O-008, `Validation/S1_kit.md`). One script makes a Win64 folder a host can copy to any PC, one launcher starts a run in either language, and one script hands the run data over. The build never sends anything anywhere: the host carries the zip by hand. This spec covers the build, the launchers, the hand-over and what a machine can prove about them; `telemetry.md` owns what a run writes.

## Rules
- **R-PKG-01** `Tools/Package.ps1` is headless and needs no human: it refreshes the text (`Tools/Localize.bat`), builds the editor target (the cook runs inside it) and runs `BuildCookRun` for `Win64`, opening no window. Output stays under the git-ignored `Saved/Package/<Config>/Windows/`; nothing binary is committed, uploaded or published.
- **R-PKG-02** The validator build is **Shipping**: frame times as a player gets them, no `DT.*` console (R-TEC-12), no developer lines on the HUD (language check, frame times), no engine log, so no Windows user or machine name in anything it writes. The `Test` config is not an option: the installed engine has no `UnrealGame` Test libraries. Shipping cannot show what it drew, so a **Development** build staged from the same cook is the proof build (R-PKG-07); it is never handed to a validator.
- **R-PKG-03** The cook carries what `LoadObject` reaches by string and the cook cannot see: every asset under `/Game/DT`, and `/Engine/BasicShapes`. The ICU preset is `CJK`, or `-culture=zh-Hans` falls back to English in a packaged build. `en` and `zh-Hans` are staged (`CulturesToStage`), with the generated string tables as UFS files.
- **R-PKG-04** The package holds `Play_en.bat` and `Play_zh-Hans.bat`: full screen, director on, bot off, and a fresh `-DTSeed` per launch, so a tester's second run is not the first run's seed. The packaged game has no restart: a run ends at death, the win, or Alt+F4, and the next run is a new launch.
- **R-PKG-05** Telemetry lands where `R-TEL-07` says, which for a Shipping package is `%LOCALAPPDATA%\Dusktracer\Saved\Playtests` (Shipping saves to the user directory) and for the Development proof build `Saved\Playtests` inside the package. An exit mid-run closes the run: `run_end` with `reason` `quit`. A run with no `run_end` is a crash or a killed process.
- **R-PKG-06** `ExportTelemetry.bat` zips the run files (`*.csv`, `*.jsonl`) with a `MANIFEST.txt` listing them, to the desktop, and prints the path. It reads nothing else unless `-IncludeLogs` is passed, which adds `Saved\Logs` and `Saved\Crashes`; those can name the Windows user and the machine, and it says so. No script or build transmits anything. The crash reporter is not staged.
- **R-PKG-07** `Tools/PadCheck.ps1` judges a package off-screen, in both cultures, with no window on the desktop. On the Shipping build: the bot plays past the first CSV interval and quits the game (`run_start` with the right `build` and `culture`, `run_end` `bot_timeout`), the export zip holds that run and `Tools/power_split.py` reads the exported files. On the Development build: `Run start` in the log, `Test.LocRoundTrip` drawn equal to `ST_UI.csv`, no crash, ensure or `LogDT` error, the screenshot passes `Tools/image_check.py`, and the exit mid-run leaves `run_end` `quit`. `-RenderOffScreen` creates no window, so no close message can reach the game: Alt+F4 itself is a human check.
- **R-PKG-08** A package has a stamp, `Saved/Package/package_stamp.json`: commit, dirty flag, configs, cultures, size. The gate step `pad` judges only a clean package whose commit has the same `Source`, `Content` and `Config` as `HEAD`; a missing, dirty or stale package is `NOT RUN`, a failed check is a gate failure.
- **R-PKG-09** A real pad needs a human: no machine check counts as one. `pad` stays `NOT RUN` until a run of this package exists whose CSV has `input_device` `pad`; only then is it `PASS`. A simulated input is never that evidence.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| CSV interval the check waits for | C.TEL.INTERVAL_S | CORE | R-PKG-07 |
| Screenshot delay of the proof run | C.SMOKE.SECONDS | CORE | R-PKG-07 |

## Data
- `package_stamp.json`: `commit`, `dirty`, `configs`, `cultures`, `size_bytes`, `wall`.
- A run's two files hold gameplay only (`telemetry.md` §Data): seed, build hash, culture, last input device, per-interval combat numbers, cards offered and picked, Keeper and weapon choices, and three wall-clock fields (`run_start.wall`, `offer_pick.wall_decide_s`, `run_end.wall_paused_s`). No name, account, machine name, path, hardware, IP or free text. The file name is the seed and the local start time.

## Acceptance
- T-PKG-01 `Tools/Package.ps1`: exits 0 and prints `PACKAGE: PASS`, with both configs under `Saved/Package/`.
- T-PKG-02 `Tools/PadCheck.ps1` Shipping half: `run_start` and `run_end`, a closed interval, `build` and `culture` right, en and zh-Hans.
- T-PKG-03 `Tools/PadCheck.ps1` Development half: drawn text, log, screenshot and `run_end` `quit`, en and zh-Hans.
- T-PKG-04 `Tools/PadCheck.ps1` export: the zip holds that run and its `MANIFEST.txt`, and `power_split.py` reads the exported files.
- T-PKG-05 `Tools/PadCheck.ps1` window count: no visible window of the game's process during the check.
- T-PKG-06 `Tools/Gate.ps1` step `pad`: `NOT RUN` without a current clean package; `NOT RUN` with one and no `pad` run; a failed check fails the gate.
- HUMAN: a validator on a gamepad plays this package and `input_device` reads `pad`; full-screen, Alt+F4 leaving `run_end` `quit` in Shipping, and the look of `zh-Hans` cards on a real display.

## Telemetry
- Adds none. The check reads `run_start`, `run_end` and the CSV `input_device` column (`telemetry.md`).

## Text
- None in the game. The launchers and `README.txt` are host tools, not player text; the kit carries the one sentence in both languages.

## Out of scope
- Installers, stores, signing, upload, update, save data, Mac (D-007).
- A restart or quit control in the game (S3 loop closure).

## Open
- The console key still exists in the Development proof build; it is never handed out.
- A Shipping crash leaves no log: the host notes the time and the run has no `run_end`. A validator crash can be replayed on the proof build with the seed from the file name.
