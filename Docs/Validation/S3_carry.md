# S3 carry: `dev/s2` (with `main`) into `dev/s3` (Queue item 6, leg 2)

Branch `carry/s3`, cut from `dev/s3` 5368248; `dev/s2` merged at 432bedd, which holds `main` 8f2579e and leg 1 (`S2_carry.md`). D-050 item 3. Every command ran under the mask `FFFFF3FC` (the shell's affinity before the merge brought `Tools/Machine.ps1`, the helper after). No cherry-pick: 406268e is on `dev/s3` in its own form.

## Commits
| Commit | What |
|---|---|
| 60a507b | merge of `dev/s2` 432bedd |
| 3f8802a | D-049 carried: `findings_S3.md`, `S3_kit.md`, `Tools/s3_exit.py` |
| 76774c2 | `C.GATE.TEST_FLOOR` 543 |

## Conflicts and how each was resolved
- `Docs/13_Decisions/**` (with `INDEX.md`, `OPEN.md`), `Docs/STATE.md`, `AGENTS.md`: the incoming side, whole. `dev/s3` held no decision file and no `AGENTS.md` line the incoming side lacks. `git diff 432bedd -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
- 406268e (`DTWeaponSim.h`, `DTArsenalTests.cpp`): `dev/s3`'s form stands, `ShutStarter`, `IsStarterShut` and the R-MET-25 block of `Dusktracer.Arsenal.Refit`; `dev/s2`'s adapted pick gives way. The result is 406268e as it was on `dev/s3`.
- `telemetry.md`: `hero_healed` (from `main`) in the kinds line beside `meta.md`'s `run_award` and `save_deferred`.
- `07_Technical.md` §4: `Kills` is read by the boss (R-BOS-13) and by the run recorder (R-MET-03, R-MET-41); `StatusApplies` by enemies and by Keeper telemetry.
- `08_Constants.md`, `constants.yaml`: the floor row only (set in 76774c2); every other key merged on its own.
- `number_change_log.csv`: every row of both sides.
- `11_Quality.md`: `main`'s rows for constants, catalog, locbuild, nonunity, shipping, devgame, tests, smoke and contrast; `dev/s3`'s for matrix, restart, seed (through the hub, with `driver`), meta, vigil, firstrun, win and earned.
- `DTConstantsGen.h`: regenerated, never resolved by hand.
- `Tools/Gate.ps1`: `main`'s header and steps with `dev/s3`'s named; one copy each of 1b074e2's two fixes (a missing Python step fails, a base that does not resolve fails). Live and kept: matrix, restart, meta, vigil, firstrun, win, earned.
- `Tools/Perf.ps1`: `dev/s3`'s `-Package` (its `$Target`, the package stamp, the `[package]` row) beside `main`'s size from `C.BUDGET.RES`, `-ForceRes`, the size check and the measured size in `perf.csv`.
- `Tools/Smoke.ps1`: `main`'s key labels and mask samples, and `dev/s3`'s "killed at the timeout is a hang" condition.
- `Tools/tools_test.py`: both sides' imports and cases.
- Brought in by the rules: the mask helper is the first statement of `BotRuns.ps1` (`dev/s3` had a `-ReadAll` check above it) and of the nine `dev/s3` entry scripts `CrashExportCheck`, `EarnedCheck`, `ExportLogsCheck`, `MetaCheck`, `PackageHubCheck`, `S3Exit`, `TwoWindowsCheck`, `VigilCheck` and `WinCheck`, all listed in `Machine.ENTRY_PS`. None starts a commandlet or a cook of its own: every one launches through `Start-DTHidden`, whose job carries the limit, and the package checks start the built package, not `Package.ps1`. So none calls `Hold-DTProcess`. The PadCheck and WinCheck stand-in trees of `tools_test.py` copy `Machine.ps1`, which those scripts now load.
- `Validation/S4_artbook_gaps.md`: its two mentions of the `PLANNED(S0)` marker (about T-PRE-07, which is built now) are put in code spans, since `main`'s lint makes the bare marker an error. No spec on `dev/s3` carried one.

## Ids
No `R-` or `T-` id has two definition lines after the merge, and no new collision. T-KEE-23 is `Dusktracer.Keepers.S2.Stage`, T-KEE-39 `Dusktracer.Keepers.HardBlowKnockback`, T-WPN-14 `Dusktracer.Arsenal.ScatterCannon`, T-WPN-19 `Dusktracer.Weapons.HammerKeyFallback`, one line each.

## D-049 and D-047 on `dev/s3`
- D-049 (Impact): the Pass rule row and call 1 of `findings_S3.md`, the exit line and the Pass rule row of `S3_kit.md`, and the Pass rule row's reason in `Tools/s3_exit.py` now name the designer's two of three (another run asked for by at least two, a build named by at least two, the owner still wants to play) and its stop line. D-034's status line is a decision record and is left to the scheduler on `main`. Left as they are, outside D-049's list: `findings_S3.md`'s Fresh row and call 2, and its three validator rows "Not the owner, and never played any build", which D-049's "a fresh save" and "may return" now contradict.
- D-047: `Tools/s3_exit.py` reads no T-KEE-18 line, so nothing follows there.

## Counts and floor
- Before (`dev/s3` 5368248): 539 tests, 539 pass. Floor 532 (510 in `STATE.md`).
- After: 543 found, 543 pass. `C.GATE.TEST_FLOOR` 543 (`08_Constants.md` row, one `number_change_log.csv` row). `tools_test.py`: 277 cases.

## Seed 9107, one process per call
`Tools\BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107` (bot v1, the five arms of `S3_verify.md`'s report) and `-BotVersion 2 -Arms None,Steered:Storm:Tempest,Steered:Tide:Frost:Forge,Steered:Shade:Hound:Storm` (`S3_tree.md`'s four).

| Arm | `dev/s3` 5368248 | carry/s3 76774c2 |
|---|---|---|
| v1 None | `7bd32faf` | `00e0d4ec` (death 735.6 s) |
| v1 Random | `f5270196` | `51369747` (victory) |
| v1 Steered:Forge:Hammer | `c6573c69` | `e3293942` (victory) |
| v1 Steered:Storm:Tempest | `d7c8b43c` | `b046559b` (victory) |
| v1 Steered:Tide:Frost | `209a3446` | `def51848` (death 810.0 s) |
| v2 None | `7bd32faf` | `00e0d4ec` |
| v2 Steered:Storm:Tempest | `b5fe3b6c` | `3b526497` (victory 960.6 s) |
| v2 Steered:Tide:Frost:Forge | `dd5bf564` | `27a6b67f` (death 921.0 s) |
| v2 Steered:Shade:Hound:Storm | `bdba5258` | `dcb5b3e4` (victory 969.1 s) |
| RestartCheck launch A | `276faf8e` | `276faf8e` |
| MetaCheck | `ba2a3ac8` | `ba2a3ac8` |

The old ones equal the notes'. The v1 arms now equal `dev/s2`'s after its carry (`S2_carry.md`), so the S3 systems still move no tool run. Every bot arm moved, as D-045, D-046 and Hard Blow make them; restart and meta play 90 s and 20 s and did not.

## Gate (`& Tools\Gate.ps1 -Base dev/s3`, 76774c2, once from start to end)
```
cpu:        CPU: PASS (28 processors, mask FFFFF3FC, 600 rounds)
tests:      543 run of 543 found, 0 failed, floor 543, 0 crash marker(s)
restart:    RESTART: PASS (seed 9107, input_hash 276faf8e / 276faf8e, steps 5400 / 5400, csv same=True, old run reads restart=True, input check=True)
meta:       META: PASS (input_hash ba2a3ac8 / ba2a3ac8, steps 1200 / 1200, ...)
firstrun:   BOTRUNS: PASS (T-OFF-15: PASS (6/6 runs at the boss with decisions 28+-3 and Reserve share <= 0.1; mean decisions 29.167); ...)
GATE: PASS (76774c2; 3 not run; mask FFFFF3FC)
```
Every other step passed: secrets, docs (0 errors, 8 warnings), constants, catalog, matrix, loc, tools (277), build, locbuild, nonunity (22 files), shipping, devgame, smoke en, zh-Hans and burst (mask held in every sample), seed, vigil, win, earned, contrast 7/7 and the dormant line, windows. Not run: pad, scope, mac. No odd failure; no step was rerun. Firstrun reads six of six at the boss after D-045, D-046 and Hard Blow: nothing to report against D-037.

## Readings this carry makes stale on `dev/s3`
- Every seed 9107 bot hash in the `S3_*.md` notes (`S3_tree.md`'s four arms, the five arms of `S3_verify.md`'s report), as the table above shows.
- The package checks read at 00c270c (PKGRESTART, PKGHUB, EXPORTCHECK, PERF, PADCHECK's machine half): no package of this tree exists; they must be reread on a new package.
- MetaCheck and firstrun (D-037): read again by this gate on the merged tree and passing (lines above). The figures `S3_tree.md` quotes from MetaCheck's 600 s pool window (19 and 21 hands, hash `b55324a1`) play long enough for D-045, D-046 and Hard Blow to act and were not printed again here: treat them as stale until reread. `S3_verify.md`'s package hash on seed 9107 and the `STATE.md` package lines read at 00c270c are stale with the package checks above.
- The S3 exit tooling's fixtures are not readings and do not move.

## Second carry: `carry2/s2` (`main` 7f75ada in `dev/s2`) into `dev/s3` dd6a868 (`carry2/s3`)
- Conflicts: the scheduler's records take `main`'s side whole (`git diff 7f75ada -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty). `run_start` carries both sides' fields in one record, `config` and `driver` (R-TEL-13, `dev/s3`) and `bot_fast` (R-TEL-08, `main`): `DTTelemetryCore.cpp` writes all three and `DTTelemetrySubsystem.cpp` sets all three; `telemetry.md`'s record row and test list (T-TEL-13 to T-TEL-15) and `package.md`'s list of what a run file holds say both. `number_change_log.csv`: every row of both sides. Generated files regenerated.
- No `dev/s3` reader or test pins a whole `hero_damaged` or `run_start` line: the suite passed with no change to one. `dev/s3`'s tools read `run_start` by key, so a new key changes nothing for them.
- Ids: T-TEL-14 and T-TEL-15 collide with nothing.
- Counts: 545 found, 545 pass; `C.GATE.TEST_FLOOR` 543 to 545; `tools_test.py` 279.
- Seed 9107, unchanged before (76774c2, whose code dd6a868 holds unchanged) and after (8dbb124): v1 `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`; v2 `00e0d4ec`, `3b526497`, `27a6b67f`, `dcb5b3e4`; RestartCheck `276faf8e` and MetaCheck `ba2a3ac8` in the gate.

```
tests:      545 run of 545 found, 0 failed, floor 545, 0 crash marker(s)
GATE: PASS (8dbb124; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run), firstrun 6/6 at the boss; no odd failure.
## Third carry: `carry3/s2` (`main` d17458f in `dev/s2`) into `dev/s3` bb0c100 (`carry3/s3`)
- Conflicts: the scheduler's records take `main`'s side whole (`git diff d17458f -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty). `package.md`: `dev/s3`'s list of what a run file holds, with "how many drop-ins landed" added from `main`. `telemetry.md`: T-TEL-16 and T-TEL-17 beside `dev/s3`'s HUMAN line. `number_change_log.csv`: every row of both sides. The floor and the generated header. `dev/s3` added no field to `dir_block`.
- The bot: both versions end a decided run in `UDTBotSubsystem::HandleStepEnd`; the hold is there and nothing was added. T-TEL-17 passes; MetaCheck (`ba2a3ac8`, 1200 / 1200) and RestartCheck (`276faf8e`, 5400 / 5400) stay equal; firstrun reads 6/6 at the boss.
- Pinned values: none moved. No `dev/s3` test or tool check pins a fast bot's step count or closing record.
- Ids: T-TEL-16 and T-TEL-17 collide with nothing.
- Counts: 547 found, 547 pass; `C.GATE.TEST_FLOOR` 545 to 547; `tools_test.py` 280.
- Seed 9107, read on bb0c100 and on 054a7d2: every `input_hash` unchanged (v1 `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`; v2 `00e0d4ec`, `3b526497`, `27a6b67f`, `dcb5b3e4`); each record's `steps` reads two lower (v2: 58150 to 58148, 57637 to 57635, 55264 to 55262), which is this fix.

```
tests:      547 run of 547 found, 0 failed, floor 547, 0 crash marker(s)
GATE: PASS (054a7d2; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no odd failure.
## Fourth carry: `carry4/s2` (`main` 9b2917f) into `dev/s3` 79117b5 (`carry4/s3`)
What came: the heavy-job slots and the alone hold (`Tools/Machine.ps1` `Enter-DTHeavy`, `Tools/Alone.ps1`, R-TEC-18 and T-TEC-18, `HeavyLocks`), D-055 and D-056 with today's records, the `12_Production.md §1` line, and `dev/s2`'s slot lines.
- Conflict: `Tools/BotRuns.ps1`, the line after the mask: the slot line goes right after `Set-DTAffinity`, then `dev/s3`'s `-ReadAll` check (`-ReadAll` still launches, so it takes the slot). Everything else merged by itself. `git diff 9b2917f -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
- Scripts changed (one `Enter-DTHeavy` line right after `Set-DTAffinity`, and the names in `test_every_heavy_entry_takes_a_slot_and_play_does_not`): `CrashExportCheck.ps1`, `EarnedCheck.ps1`, `ExportLogsCheck.ps1`, `MetaCheck.ps1`, `PackageHubCheck.ps1`, `TwoWindowsCheck.ps1` (its two games are one job), `VigilCheck.ps1`, `WinCheck.ps1`.
- Left alone: `S3Exit.ps1` (its header: "Starts no game and writes nothing"; it runs `s3_exit.py` on the validators' logs), `s3_exit.py` and the other readers (Python, no heavy job). The `firstrun` gate step is a `BotRuns.ps1` call inside the gate, so it runs on the gate's slot.
- Counts: 547 found, 547 pass, `C.GATE.TEST_FLOOR` 547 (unchanged); `tools_test.py` 287.
- Seed 9107, on ede45e6: v1 `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`; v2 `00e0d4ec`, `3b526497`, `27a6b67f`, `dcb5b3e4`; in the gate RestartCheck `276faf8e / 276faf8e` and MetaCheck `ba2a3ac8 / ba2a3ac8`. All as before.

```
tests:      547 run of 547 found, 0 failed, floor 547, 0 crash marker(s)
tools:      TOOLS: PASS (287 tests)
GATE: PASS (ede45e6; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no odd failure. Log: `Saved\Logs\carry4_s3_gate.txt` in the carrying worktree.

## Fifth carry: `carry6/s2` (`main` 75acd0d in `dev/s2`) into `dev/s3` ac106f4 (`carry6/s3`)
- Conflicts and resolutions:
  - `Docs/13_Decisions`, `STATE.md` and `AGENTS.md` take `main`'s side whole; `git diff 75acd0d -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
  - `Tools/Gate.ps1`: `dev/s3`'s heavy steps (`restart`, `meta`, `vigil`, `firstrun`, `win`, `earned`) take `Start-HeavyStep`. `matrix` is a text step. No `Skipped` guard is left on a step that launches.
  - `Tools/PackageFiles/ExportTelemetry.ps1`: `main` changed its manifest text, and `dev/s3` had deleted it for `Export_Logs.ps1`. It stays deleted, and `main`'s new manifest facts go into `Export_Logs.ps1`'s manifest in `main`'s words: every hit with its source and both positions, a drop-in or a charging Rusher, health restored, enemies dropped in, a bot at fast speed, five clock readings, and that the export takes every run in the folder. `dev/s3`'s own lines and its zh line are kept.
  - `Docs/02_Systems/package.md`: `main`'s list of what a run file holds, with `dev/s3`'s `config`, `driver` and its line on the save and `DTSettings.ini`; `main`'s line on PadCheck's human half beside `dev/s3`'s Shipping-log line.
  - `Validation/S1_kit.md`: `main`'s session A text, as on `dev/s2`; the kit is `main`'s package.
  - `Tools/tools_test.py`: the heavy-entry list without `Gate.ps1`, with `dev/s3`'s scripts kept.
- Tests that pin behaviour differing by branch: `main`'s `Manifest` case read `ExportTelemetry.ps1`, which `dev/s3` does not ship. It now reads `Export_Logs.ps1`'s manifest block, with every one of its phrases. `dev/s3`'s `ReadAll.test_the_gate_step_asks_for_it` pinned the old guard text of the firstrun step; its pin follows `Start-HeavyStep`, and the check is unchanged.
- Scripts changed beyond `main`'s: `Tools/Gate.ps1`, `Tools/PackageFiles/Export_Logs.ps1` (the manifest lines) and `Tools/tools_test.py`.
- Counts: 547 tests, floor 547; `tools_test.py` 326.
- Seed 9107 on fdc10f1, unchanged: v1 `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`; v2 `00e0d4ec`, `3b526497`, `27a6b67f`, `dcb5b3e4`; RestartCheck `276faf8e`; MetaCheck `ba2a3ac8`; firstrun 6/6 at the boss.

```
tools:      TOOLS: PASS (326 tests)
tests:      547 run of 547 found, 0 failed, floor 547, 0 crash marker(s)
GATE: PASS (fdc10f1; 3 not run; mask FFFFF3FC)
```
`dev/s3` then moved by one documents-only commit (d1dfec0, `S3_kit.md`). It was merged (403e732), and the text steps reran with every heavy step skipped, taking no slot: `GATE: PASS (403e732; 19 not run; mask FFFFF3FC)`, `TOOLS: PASS (326 tests)`. Logs: `Saved\Logs\c6s3_gate.txt` and `c6s3_text.txt` in the carry agent's worktree.
## Sixth carry: `carry7/s2` (`main` d23b7ef through `dev/s2`) into `dev/s3` 892c617 (`carry7/s3`)
What came: `main`'s crash-exit fix (R-PKG-16 and T-PKG-16, `Source/Dusktracer/Dusktracer.cpp`, `Tools/CrashExitCheck.ps1`, `crash_exit.py`), the alone take that claims the machine at once (`Tools/Alone.ps1`, `Machine.ps1`), D-059 to D-066 and their records, `offers.md` saying what the code does (D-062, D-063), and `dev/s2`'s side of the same merge.
- Conflicts and resolutions:
  - `Docs/13_Decisions`, `STATE.md` and `AGENTS.md` take `main`'s side whole; `git diff d23b7ef -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
  - `offers.md`: R-OFF-07 and T-OFF-07 keep `dev/s3`'s Tree and gated-pool text with `main`'s trait count (R-KEE-04, `C.KEEPER.TRAIT_PREREQ_CARDS`); R-OFF-08 keeps `dev/s3`'s seat and hub text with `main`'s unweighted L8 hand (D-062, a text correction that holds on every branch); T-OFF-14 is `main`'s Reserve line, T-OFF-15 `dev/s3`'s with `-ReadAll`.
  - `package.md`: R-PKG-02 takes `main`'s crash-folder clause; R-PKG-03 and R-PKG-04 are `dev/s3`'s; R-PKG-11 to R-PKG-14 and T-PKG-11 to T-PKG-15 stand beside R-PKG-16 and T-PKG-16; the HUMAN line is `dev/s3`'s; the Open line keeps `dev/s3`'s Shipping-log sentence and says a Shipping crash leaves no dump.
  - `Source/Dusktracer/Dusktracer.cpp`: both sides' includes (`Meta/DTTreeTab.h` and `main`'s `Misc/CommandLine.h`, `Misc/Parse.h`); the crash-exit block and `DTTreeTab::Register()` both stand.
  - `Tools/tools_test.py`: both entry lists hold `CrashExitCheck.ps1` beside `dev/s3`'s scripts.
  - `constants.yaml` did not change on either side; the gate's `constants` step reads the header as generated.
- Scripts changed beyond the two sides: none. No test pins behaviour that differs by branch.
- Left as it is, by the scheduler's word: `Tools/CrashExportCheck.ps1` (T-PKG-15) crashes the Development proof build, which keeps the engine's crash path (R-PKG-16), so it can still meet the hang `Validation/S1_crash_exit.md` counts. A carry does not change the check.
- Counts: 547 tests, floor 547 (crash-exit adds no engine test); `tools_test.py` 326 to 336.
- Seed 9107 on 5fe90ae, unchanged: v1 `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`; v2 `00e0d4ec`, `3b526497`, `27a6b67f`, `dcb5b3e4`; RestartCheck `276faf8e`; MetaCheck `ba2a3ac8`; firstrun 6/6 at the boss.

```
tools:      TOOLS: PASS (336 tests)
tests:      547 run of 547 found, 0 failed, floor 547, 0 crash marker(s)
GATE: PASS (5fe90ae; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no golden re-shot, no odd failure. `dev/s3` and `main` had not moved when the leg was gated. Log: `Saved\Logs\c7s3_gate.txt` in the carry agent's worktree.
