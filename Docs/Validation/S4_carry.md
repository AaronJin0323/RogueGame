# S4 carry: `dev/s3` (with `dev/s2` and `main`) into `dev/s4` (Queue item 6, leg 3)

Branch `carry/s4`, cut from `dev/s4` 92ce1a5; `carry/s3b` merged at dd6a868 (`dev/s3` a92bc32 plus D-049's last rows in `findings_S3.md`), which holds `dev/s2` 432bedd and `main` 8f2579e (`S2_carry.md`, `S3_carry.md`). D-050 item 3. Every command ran under the mask `FFFFF3FC` (the shell's affinity before the merge brought `Tools/Machine.ps1`, the helper after). No art, number or rule was changed here.

## Commits
| Commit | What |
|---|---|
| 4748285 | merge of `carry/s3b` dd6a868 |
| e1ee2ec | T-ENM-24 says what it counts |
| d7f8117 | `C.GATE.TEST_FLOOR` 568 |
| c4a4a44 | `Dusktracer.EnemyArt`'s Nanite check compiles in the editor only |

## Conflicts and how each was resolved
- `Docs/13_Decisions/**` (with `INDEX.md`, `OPEN.md`), `Docs/STATE.md`, `AGENTS.md`: the incoming side, whole; `git diff carry/s3b -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty. `dev/s4` held no decision file the incoming side lacks. Two `dev/s4` lines are gone with it: D-003's Mac line named the Mac's account and its LAN address where `main` keeps them out of the repository, and D-024's status read only "applied" where `main` adds that D-044 replaced its last clause. Its `AGENTS.md` lines were older wording of `main`'s.
- `ST_UI.csv`: `main`'s `Hud.KeyLabel` row beside package F's contexts of the XP block rows.
- `combat_core.md`: package F's line (the Intercession meter is a ring since the HUD pass).
- `04_Presentation.md`, T-PRE-07: both checks exist and read different things, so both stay live and the one line names both: `Tools/palette_lint.py --settings` (gate step `settings`: the settings in the config and in the volume's generator, and the ground's exposure between the empty arena and the contrast frame) and the `smoke` step (the config on every smoke run, the built map's volume through `Tools/render_settings_check.py`, a burst frame against the calm frame through `image_check.py --exposure`).
- `07_Technical.md`, T-TEC-06: `dev/s4`'s line, since `Tools/catalog_lint.py` exists here.
- `08_Constants.md`, `constants.yaml`: package C's `HERO_ART.*` keys beside `main`'s T-DIR-18 dependent; the floor in d7f8117. `number_change_log.csv`: every row of both sides, none twice.
- `11_Quality.md`: `dev/s4`'s licence, palette, tools, contrast and settings rows; `main`'s build, locbuild, nonunity, shipping, devgame and tests rows; the smoke row names the scenes of `Tools/Scenes`, the key labels, the mask samples and the burst half. A second, older contrast row the merge left is removed.
- `DTConstantsGen.h`: regenerated.
- `Game/DTHUD.h`, `DTHUD.cpp`: package F's `DrawRun` and layout stand; the Dash and Intercession texts it lays out are now `main`'s key labels (`KeyLabel`, `Hud.KeyLabel`, the device in use), and `HudKeys` is logged once for `Tools/Smoke.ps1` outside Shipping. Both are on screen in package F's places (frames below).
- `Tools/Gate.ps1`: `main`'s header and steps with `dev/s4`'s named (licence, palette, settings, the scene-driven smoke and contrast). `dev/s4` keeps every staged command in a scene file (`Scenes` in `tools_test.py`), so the burst frame's fire moved from the script to `Tools/Scenes/burst.json`, gate `none`; the gate reads its `cmds`.
- `Tools/Perf.ps1`: `-Scene S4` beside `main`'s size from `C.BUDGET.RES` and `GpuLock.ps1`.
- `Tools/tools_test.py`: both sides' cases; the stand-in gate of `GateMain` stubs `catalog_lint.py` and `palette_lint.py`.
- The mask helper is the first statement of `Tools/EnemyArt.ps1` and `Tools/blender/Mesh.ps1`; `Tools/EditorPython.bat` and `Tools/blender/CookMesh.bat` take the batch form (`Machine.ps1 -DTRunFile`, which holds itself in the job). All four are listed in `Machine.ENTRY_PS` and `ENTRY_BAT`, whose check now accepts a helper one folder up. `Mesh.ps1` starts Blender and editor commandlets itself, so it calls `Hold-DTProcess`. `EnemyArt.ps1` starts the game through `Start-DTHidden` and Blender only through `Mesh.ps1`.
- `PLANNED(S0)`: none left on `dev/s4` outside a code span; the docs lint reads 0 errors.

## Fixed, because the carried gate found it
`main`'s `devgame` step compiles the Development game target, which `dev/s4` never built. It failed on `Dusktracer.EnemyArt`'s mesh test: `UStaticMesh::IsNaniteEnabled` is an editor API (C2039 at `DTEnemyArtTests.cpp` 249). c4a4a44 puts that one assertion inside `#if WITH_EDITOR`; the suite runs in the editor, so the check still runs there. First gate: `GATE: FAIL at devgame (d7f8117; mask FFFFF3FC)`, log `Saved\Logs\s4_gate_1_d7f8117.txt`.

## T-ENM-24
Its line now says it counts the sim's grey box, cubes hidden in play, and that the drawn role hulls are T-EAR-01's. The test's code is unchanged.

## Ids
No `R-` or `T-` id has two definition lines, and no new collision.

## Counts and floor
- Before (`dev/s4` 92ce1a5): 556 tests, 556 pass, floor 549; `tools_test.py` 279.
- After: 568 found, 568 pass; `C.GATE.TEST_FLOOR` 568 (`08_Constants.md` row, one `number_change_log.csv` row); `tools_test.py` 345.

## Seed 9107 (`Tools\BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107 -Seeds 1`, `S4_p0.md` §8)
| Arm | `dev/s4` 92ce1a5 | carry/s4 c4a4a44 |
|---|---|---|
| None | `7bd32faf` | `00e0d4ec` |
| Random | `f5270196` | `51369747` |
| Steered:Forge:Hammer | `c6573c69` | `e3293942` |
| Steered:Storm:Tempest | `d7c8b43c` | `b046559b` |
| Steered:Tide:Frost | `209a3446` | `def51848` |
| RestartCheck launch A | `276faf8e` | `276faf8e` |
| MetaCheck | `ba2a3ac8` | `ba2a3ac8` |

The old ones equal `S4_p0.md`'s. The new ones equal `dev/s2`'s and `dev/s3`'s after their carries: wave 1 moves no sim number, as `S4_wave1.md` (b) requires. The arms moved with D-045, D-046 and Hard Blow.

## Gate (`& Tools\Gate.ps1 -Base dev/s4`, `tools` live)
First run, on d7f8117: `GATE: FAIL at devgame (d7f8117; mask FFFFF3FC)` (above; log `Saved\Logs\s4_gate_1_d7f8117.txt`). Not an odd failure: a compile error that fails every time. After c4a4a44, once from start to end:
```
cpu:        CPU: PASS (28 processors, mask FFFFF3FC, 600 rounds)
tools:      TOOLS: PASS (345 tests)
devgame:    rc 0, 0 warning(s)
tests:      568 run of 568 found, 0 failed, floor 568, 0 crash marker(s)
smoke:      SMOKE: PASS (hud_staged_en, ... keys ok (Dash  [Space] | Intercession  [Q]), image PASS (1280x720, luma stddev 10.80, mean diff vs golden 0.35 (max 12.0)), mask FFFFF3FC, 64 sample(s), 0 outside)
smoke:      SMOKE: PASS (burst_en, ... RENDER: PASS (PostProcess_Presentation: 5 settings as 04_Presentation s4 says), image PASS (1280x720, luma stddev 35.82, exposure shift -0.79 levels against the calm frame (max 12.0)), ...)
settings:   SETTINGS: PASS (14 config and 9 volume settings; lit ground median Y 0.1119 empty, 0.1119 under fire, drift 0.000 (max 0.05))
GATE: PASS (c4a4a44; 3 not run; mask FFFFF3FC)
```
Every other step passed: secrets, docs, constants, catalog, matrix, loc, licence, palette, build, locbuild, nonunity (45 files), shipping, every smoke scene in its cultures (the key labels drawn and read in each, the mask held in every sample), restart, seed, meta, vigil, firstrun (6/6 at the boss), win, earned, contrast (7/7, the dormant line, the tracer crossing; WATCH OK), windows. Not run: pad, scope, mac. No golden failed and none was re-shot: package F's goldens read 0.30 to 0.36 mean difference with the key labels drawn, against a limit of 12. No odd failure.

## Frames for the scheduler (`Saved\S4Report\carry\` of this worktree, 1280x720, off screen through `Tools/Smoke.ps1`)
- `hud_run_keylabels_clock_hp_en.png`, `hud_run_keylabels_clock_hp_zh-Hans.png`: the gate's `hud_staged` shots; HP top left, clock, level and XP top right, the ring with `Intercession  [Q]` / `神临  [Q]` and `Dash  [Space]` / `冲刺  [空格键]`, tools, a Keeper meter and a dormant line bottom left, in package F's places.
- `hud_boss_bar_keylabels_en.png` (the gate's `hud_boss`), `hud_boss_bar_keylabels_zh-Hans.png` (taken after the gate, `-Name carry_hud_boss -Cmds "DT.God 1,DT.Boss" -Seconds 14`; it reads FAIL only for want of a golden): the Warden's bar top centre with HP, clock and the key labels in view.
- `crowd_tracers_crossing_en.png`: the contrast step's `tracer_crossing`, a crowd with player tracers, enemy rounds and telegraphs crossing.

## Readings this carry makes stale on `dev/s4`
- The five bot arms read on each package (P0, A, B, C, D, F notes): every hash moved, as above.
- Every perf reading on `dev/s4`: taken at 888x500 with other agents rendering; `Tools/Perf.ps1` now renders at `C.BUDGET.RES` and fails on another size, so no package's perf allowance is read until a reading is taken alone at the real size.
- The frames in `Saved\S4Report\A` to `F` of the main checkout that show the HUD: the Dash and Intercession labels now carry their keys. The goldens still pass and were not re-shot.

## Second carry: `carry2/s3` (`main` 7f75ada through `dev/s2` and `dev/s3`) into `dev/s4` a19c925 (`carry2/s4`)
- Conflicts: the scheduler's records take `main`'s side whole (`git diff 7f75ada -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty); `number_change_log.csv`, every row of both sides; the floor and the generated header. Nothing else conflicted: the telemetry marks and `run_start`'s three fields came in from `carry2/s3` as they are there, and package F's HUD and the wave's tools touch none of it.
- No `dev/s4` reader or test pins a whole `hero_damaged` or `run_start` line: the suite passed with no change to one. The hero hits of the Volatile blast and the Vampiric drain carry `arrival` and `charging` false by default, as on `dev/s2`.
- Ids: T-TEL-14 and T-TEL-15 collide with nothing.
- Counts: 570 found, 570 pass; `C.GATE.TEST_FLOOR` 568 to 570; `tools_test.py` 347.
- Seed 9107, five arms, unchanged before (c4a4a44, whose code a19c925 holds unchanged) and after (5980687): `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`; RestartCheck `276faf8e` and MetaCheck `ba2a3ac8` in the gate.

```
tests:      570 run of 570 found, 0 failed, floor 570, 0 crash marker(s)
GATE: PASS (5980687; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run): every smoke scene against its golden with the key labels, firstrun 6/6 at the boss, settings, contrast with WATCH OK. No golden re-shot; no odd failure.
## Third carry: `carry3/s3` (`main` d17458f through `dev/s2` and `dev/s3`) into `dev/s4` ae25e37 (`carry3/s4`)
- Conflicts: the scheduler's records take `main`'s side whole (`git diff d17458f -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty); `number_change_log.csv`, every row of both sides; the floor and the generated header. Nothing else conflicted. `dev/s4` added no field to `dir_block`.
- The bot: both versions end a decided run in `UDTBotSubsystem::HandleStepEnd`, where the hold sits; nothing added. T-TEL-17 passes; RestartCheck (`276faf8e`, 5400 / 5400) and MetaCheck (`ba2a3ac8`, 1200 / 1200) stay equal; firstrun 6/6 at the boss.
- Pinned values: none moved, since no test or tool check pins a fast bot's step count. `S4_p0.md` §8 and `S4_tracers.md` record each arm's `bot.csv` bytes and SHA-256 as the wave's sim proof: those are notes, and the files now differ in their `steps` line (two lower) while every `input_hash` holds, so a byte comparison against those notes reads stale until they are retaken.
- Ids: T-TEL-16 and T-TEL-17 collide with nothing.
- Counts: 572 found, 572 pass; `C.GATE.TEST_FLOOR` 570 to 572; `tools_test.py` 348.
- Seed 9107, read on ae25e37 and on e1fbb6d: every `input_hash` unchanged (`00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`); `steps` two lower on each (44139 to 44137, 59382 to 59380, 57751 to 57749, 60233 to 60231, 48600 to 48598), which is this fix.

```
tests:      572 run of 572 found, 0 failed, floor 572, 0 crash marker(s)
GATE: PASS (e1fbb6d; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run): every smoke scene against its golden, contrast with WATCH OK, settings. No golden re-shot; no odd failure.
## Fourth carry: `carry4/s3` (`main` 9b2917f) into `dev/s4` 0f8a973 (`carry4/s4`)
What came: the heavy-job slots and the alone hold (`Tools/Machine.ps1` `Enter-DTHeavy`, `Tools/Alone.ps1`, R-TEC-18 and T-TEC-18, `HeavyLocks`), D-055 and D-056 with today's records, the `12_Production.md §1` line, and the slot lines of `dev/s2` and `dev/s3`.
- Conflicts: none; `dev/s3`'s `BotRuns.ps1` resolution came with the merge. `git diff 9b2917f -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
- Scripts changed (one `Enter-DTHeavy` line right after `Set-DTAffinity`, and the names in `test_every_heavy_entry_takes_a_slot_and_play_does_not`): `EnemyArt.ps1` (Blender and the game; not when both `-NoFrames` and `-NoMeshCheck` leave only the Python geometry checks), `blender\Mesh.ps1` (Blender, the import commandlet and the cook; called by `EnemyArt.ps1`, it inherits that slot). `EditorPython.bat` and `blender\CookMesh.bat` already run through `Machine.ps1 -DTRunFile`, which takes the slot; the test now checks they do.
- Left alone: the Python that runs inside Blender or the editor (`blender\*.py`, `create_arena.py`, `enemy_shapes.py`, `dtpalette.py`), the checkers that read files or frames (`enemy_art_check.py`, `hero_outline.py`, `palette_lint.py`, `arena_check.py`, `catalog_lint.py`): none starts a heavy job itself. The audio plan names two heavy entry scripts that do not exist yet; they take the slot when they are written.
- Counts: 572 found, 572 pass, `C.GATE.TEST_FLOOR` 572 (unchanged); `tools_test.py` 355.
- Seed 9107, on 554c696: v1 `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`; v2 `00e0d4ec`, `3b526497`, `27a6b67f`, `dcb5b3e4`; in the gate RestartCheck `276faf8e / 276faf8e` and MetaCheck `ba2a3ac8 / ba2a3ac8`. All as before. Every smoke scene passed against its golden; no golden re-shot.

```
tests:      572 run of 572 found, 0 failed, floor 572, 0 crash marker(s)
tools:      TOOLS: PASS (355 tests)
GATE: PASS (554c696; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no odd failure. Log: `Saved\Logs\carry4_s4_gate.txt` in the carrying worktree.## Fifth and sixth carry: `main` 75acd0d, then d23b7ef, through `dev/s2` and `dev/s3` into `dev/s4` 3cb9534 (`carry6/s4`, then `carry7/s4`)
The fifth carry (`carry6/s4`, 97593a2: `carry6/s3` into `dev/s4` a155ace) was gated whole, but the owner's stop came before it was reported, so it counts as not run. `carry7/s4` is cut from `dev/s4` 3cb9534 (package E landed), takes `carry6/s4` and its resolutions as they were (a merge with no conflict), then `carry7/s3` (`main` d23b7ef through `dev/s2` 63c4e51 and `dev/s3` 00332f5), and is gated whole on that tree, 5031804.
What came: the gate's slot taken at its first heavy step (`Start-HeavyStep`) and the window-watch fix; session A's reading tools; D-057 to D-066 and their records; `main`'s crash-exit fix (R-PKG-16 and T-PKG-16, `Source/Dusktracer/Dusktracer.cpp`, `Tools/CrashExitCheck.ps1`, `crash_exit.py`); the alone take that claims the machine at once (`Tools/Alone.ps1`, `Machine.ps1`); `offers.md` saying what the code does (D-062, D-063); and `dev/s3`'s save-stuck line (`s3/save-stuck-line`: `Hub/`, `Meta/`, `Settings/DTMenuScreen.*`, `DTMenuSubsystem.cpp`, `ST_UI` rows).
- Conflicts and resolutions, all in the fifth carry's merge (the sixth's two merges into `dev/s4` had none):
  - `Docs/13_Decisions`, `STATE.md` and `AGENTS.md` take `main`'s side whole; `git diff d23b7ef -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty on b6468df.
  - `Tools/Gate.ps1`: `dev/s4`'s scene-driven `smoke` body and its `contrast` body stay as they are, with `if (Start-HeavyStep "smoke") {` and `if (Start-HeavyStep "contrast") {` for guards. `settings` is a text step (`palette_lint.py` launches nothing), so its old `Skipped` guard is gone and `Invoke-PyStep` honours `-Skip` itself; `test_the_gate_knows_its_heavy_steps` allows no `Skipped` guard. `licence`, `palette`, `audio` and `tools` stay text steps; `burst.json` is still read as `dev/s4` reads it.
  - `Tools/tools_test.py`: `GateSlot`'s stand-in gate stubs `dev/s4`'s text-step scripts too (`loc_build.py`, `catalog_lint.py`, `palette_lint.py`, `audio\audio_check.py`, with its subfolder made); both entry lists hold `main`'s, `dev/s2`'s, `dev/s3`'s and `dev/s4`'s scripts. `ImpactArt.ps1` (package E) takes its slot (`Enter-DTHeavy` right after `Set-DTAffinity`) and is in both lists.
  - Draw code: none conflicted. The save-stuck line and package E merged with no conflict in `*Renderer`, `Game/DTHUD.*`, `DTHudLayout.*`, `DTUIStyle.*` or `SimRender`, so no HUD or menu frame is owed for a conflict; every smoke scene passed against its golden anyway.
  - `Enemies/DTEnemyPool.cpp`: only `dev/s4` changed it (the hit and kill tap, R-IMP-01, 71452cf); nothing from `main`, `dev/s2` or `dev/s3` in these carries touches it, so the tap is `dev/s4`'s, unmerged.
  - `Dusktracer.cpp` came in as `dev/s3` resolved it (both sides' includes, the crash block and `DTTreeTab::Register()`). `constants.yaml` changed on no carried side; the header `Tools/gen_constants.py` writes from the merged file equals `dev/s4`'s, byte for byte.
- No test pins behaviour that differs by branch.
- Counts: 581 tests, floor 581 (`dev/s4`'s package E count; the carried side adds no engine test); `tools_test.py` 412.
- Seed 9107 on 5031804, unchanged: v1 `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`; v2 `00e0d4ec`, `3b526497`, `27a6b67f`, `dcb5b3e4`; RestartCheck `276faf8e / 276faf8e` (5400 / 5400); MetaCheck `ba2a3ac8 / ba2a3ac8` (1200 / 1200); firstrun 6/6 at the boss, mean decisions 29.167. Every smoke scene passed against its golden, `impact_hit` and `impact_kill` among them (largest mean diff 0.55 of 12); contrast WATCH OK in all three scenes; no golden re-shot.

```
tools:      TOOLS: PASS (412 tests)
tests:      581 run of 581 found, 0 failed, floor 581, 0 crash marker(s)
GATE: PASS (5031804; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no odd failure. Log: `Saved\Logs\c7s4_gate.txt` in the carry agent's worktree.

`dev/s4` then moved to 2c4555b by two documents (`S4_e_feedback.md` section 10, a pointer line in `S4_perf_1080p.md`). It merged clean as b6468df, and the text steps ran again with every heavy step skipped:

```
tools:      TOOLS: PASS (412 tests)
GATE: PASS (b6468df; 20 not run; mask FFFFF3FC)
```
Log: `Saved\Logs\c7s4_text.txt`.
