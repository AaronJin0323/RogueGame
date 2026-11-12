# S2 carry: `main` into `dev/s2` (Queue item 6, leg 1)

Branch `carry/s2`, cut from `dev/s2` f7e9a98. `main` merged at 4a9f803, then at its new tip 8f2579e on the scheduler's word (one more merge, clean). D-050 item 3: a fix lands on the lowest branch that has the defect and is carried forward by merge. Every command ran under the mask `FFFFF3FC` (the shell's affinity before the merge brought `Tools/Machine.ps1`, the helper after).

## Commits
| Commit | What |
|---|---|
| 07c63bd | the pick of 406268e (`-x`) |
| 7bea6b7 | merge of `main` 4a9f803 |
| f56cc4e | D-047 carried: the S2 exit reads loss |
| 89411ff | `C.GATE.TEST_FLOOR` 476 |
| b9a3273 | `enemies.md` drops a line that is false on `dev/s2` |
| b246064 | merge of `main` 8f2579e (clean) |

## The pick of 406268e
Conduit is not offered or drawn under a Patron with no status; Refit's draw leaves banished modifiers out (BUG, both S2 systems). Two conflicts, both from S3 context in the commit: `DTWeaponSim.h` carried `ShutStarter`/`IsStarterShut` (meta.md R-MET-25, S3), left out; `DTArsenalTests.cpp` (`Dusktracer.Arsenal.Refit`) had an R-MET-25 block, dropped, and its banished-and-dead block shut every starter with `ShutStarter`, so here the starters other than Conduit leave the draw as banished ids instead. The code of the fix (`DTWeaponSim.cpp`, `DTOfferGenerator.cpp`, `DTWeaponCatalog.*`) and `Dusktracer.Offers.DeadCard` applied as they are. Both tests pass (2 of 2, then in the suite).

## Conflicts of the first merge and how each was resolved
- `Docs/13_Decisions/**`, `Docs/STATE.md`, `AGENTS.md`: `main`'s side, whole. `dev/s2`'s `AGENTS.md` held no line `main` lacks (only older wording of the same lines). `git diff 8f2579e -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
- `modifiers.md`: `dev/s2` for R-MOD-02 and Out of scope; `main` for the Open line on the offer leg (T-MOD-01 is `Dusktracer.Offers.Legality`); the crit line stays `dev/s2`'s, which names R-KEE-30.
- `combat_core.md`: `dev/s2`'s `hero_damaged` line (Blast, Drain) and `main`'s `hero_healed` line; Text takes `main`'s seven-row budget with `Hud.KeyLabel` and keeps `dev/s2`'s Keeper meter line.
- `enemies.md`: `FDTStatusApply` stays `dev/s2`'s (`bConsume`); `FDTTelegraph` takes `main`'s shape list (the boss shapes exist on `dev/s2` too). Afterwards (b9a3273) `main`'s auto-merged Text line saying `ST_Enemies.csv` does not exist was dropped: `dev/s2` builds that table.
- `telemetry.md`: `run_end` takes `main`'s field list (the wall fields of R-TEL-08) and `dev/s2`'s list of reasons.
- `weapons.md`: tests T-WPN-14 to 18 and `Arsenal.Catalog` stay `dev/s2`'s, `main`'s T-WPN-19 joins them; Open takes `main`'s D-053 questions and drops the two lines `main` removed since the base (source credit, the Gravity Mine reference), and keeps `dev/s2`'s S2 lines.
- `08_Constants.md`, `constants.yaml`: every key of both sides (the `AFFIX.*`, `ELITE.*` and `DEATH.*` rows); `ENEMY_FIRE.CAP` takes `main`'s dependent T-TEC-10 (`main` renamed the old perf id to T-TEC-10).
- `11_Quality.md`: `main`'s gate table and §5 lock line, with `dev/s2`'s sentence on the S2 perf scene's load proof.
- `ST_Modifiers.csv`, `ST_UI.csv`, `ST_Weapons.csv`: every row of both sides (`Hud.KeyLabel` from `main`; the S2 modifier, weapon, Keeper meter, pause, settings, controls and reforging rows from `dev/s2`). No row both sides changed; 655 rows, no key twice, none lost, en and zh-Hans paired.
- `DTHUD.h`: both includes, a comment naming the Keeper meters and the key labels, `KeyLabel` beside `DrawEliteLabels`, both member sets.
- `Tools/Gate.ps1`: `main`'s header and Python step, with `dev/s2`'s `matrix` and `restart` named; the body auto-merged and holds both sides' steps.
- `Tools/Perf.ps1`: `main`'s size from `C.BUDGET.RES` and `GpuLock.ps1` beside `dev/s2`'s `-Scene S2` and its warmup.
- `Tools/probe.py`: `dev/s2`'s `SHOWCASE_S2` and `PICK`, `main`'s `CONFIGS` spacing and `CONTROL`.
- `Tools/tools_test.py`: both imports; `main`'s new cases whole. Then, as the rules ask: `S2Exit.ps1`, `RestartCheck.ps1` and `PackageRestartCheck.ps1` take the mask helper as their first statement and `Machine.ENTRY_PS` lists them; `GateMain`'s stand-in gate stubs `gen_matrix.py`, a step only `dev/s2` has, else the stand-in failed at `matrix` before the case it tests.
- Generated files: `gen_constants.py`, `gen_catalog.py` and `gen_matrix.py` regenerated; the auto-merged header, tables and matrix equalled the regenerated ones. The floor commit regenerates `DTConstantsGen.h`.
- Source auto-merged: `Combat/DTWeaponSim.cpp` holds one `DTKnockback::Push` in `EmitArc` (Hard Blow) beside the dash-volley and Chain Trigger work.

The second merge (8f2579e: `Hold-DTProcess`, T-PRE-07 built) was clean. No `dev/s2`-only script starts a commandlet or a cook of its own, so none gains `Hold-DTProcess`.

## Ids
- No `R-` or `T-` id has two definition lines. The two renumbered on `main`: T-KEE-23 is `Dusktracer.Keepers.S2.Stage` and T-KEE-39 `Dusktracer.Keepers.HardBlowKnockback`; T-WPN-14 is `Dusktracer.Arsenal.ScatterCannon` and T-WPN-19 `Dusktracer.Weapons.HammerKeyFallback`; one line each.
- The damage and dash band test, T-DIR-02: `dev/s2` never changed it from the base, so `main`'s D-047/D-048 line stands (session B, median over runs alive through the window).
- The asset-origin test, T-TEC-06: `Tools/catalog_lint.py` does not exist on the merged tree, so `main`'s line ("no tool holds it yet") is the true one.
- The render settings test, T-PRE-07: `main`'s whole line. The two `PLANNED(S0)` markers `dev/s2` carried (T-PRE-07, T-TEC-10) are gone with `main`'s lines; both tests are built. None is left.

## D-047 on `dev/s2`
`Tools/s2_exit.py` parsed `share below ... mean share`, a line `main`'s probe no longer prints, so the Keeper share row would have read NOT READ on every exit. It now parses `loss below ... mean loss ...; share, printed and never judged, mean ...` and judges the row by loss; `Tools/S2Exit.ps1` says so; the `S2Exit` case of `tools_test.py` follows. R-KEE-18 names no count of arms in `main`'s merged text. `D-047.md` still says "The carry to `dev/s2` is open": a decision file is `main`'s, so that line is left to the scheduler.

## Counts and floor
- Before (`dev/s2` f7e9a98): 471 tests, 471 pass (after `Localize.bat`; a test run on a fresh worktree without it fails 8 text-key tests). Floor 30.
- After: 476 found, 476 pass. `C.GATE.TEST_FLOOR` 476 (`08_Constants.md` row, one `number_change_log.csv` row). `tools_test.py`: 145 cases.

## Seed 9107 (`Tools\BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107`, one process)
| Arm | `dev/s2` f7e9a98 | carry/s2 b246064 |
|---|---|---|
| None | `7bd32faf` (death 501.9 s) | `00e0d4ec` (death 735.6 s) |
| Random | `f5270196` (victory) | `51369747` (victory) |
| Steered:Forge:Hammer | `c6573c69` (victory) | `e3293942` (victory) |
| Steered:Storm:Tempest | `d7c8b43c` (death 830.8 s) | `b046559b` (victory 1003.9 s) |
| Steered:Tide:Frost | `209a3446` (death 591.3 s) | `def51848` (death 810.0 s) |

The old ones equal `S2_dash_volley.md`'s. All five moved, as D-045 (Rusher arrivals), D-046 (a chest restores health) and Hard Blow's knockback make them. RestartCheck reads `276faf8e` on both gates, unchanged.

## Gate (`& Tools\Gate.ps1 -Base dev/s2`, b246064, start to end)
```
cpu:        CPU: PASS (28 processors, mask FFFFF3FC, 600 rounds)
tools:      TOOLS: PASS (145 tests)
nonunity:   26 file(s), 0 failed
tests:      476 run of 476 found, 0 failed, floor 476, 0 crash marker(s)
restart:    RESTART: PASS (seed 9107, input_hash 276faf8e / 276faf8e, steps 5400 / 5400, csv same=True, old run reads restart=True, input check=True)
GATE: PASS (b246064; 3 not run; mask FFFFF3FC)
```
Every other step passed: secrets, docs (0 errors, 7 warnings), constants, catalog, matrix, loc, build, locbuild, shipping, devgame, smoke en, zh-Hans and burst (T-PRE-07, mask held in every sample), seed, contrast 7/7 and the dormant line, windows. Not run: pad, scope, mac. A first whole gate at b9a3273 (before 8f2579e) read `GATE: PASS (b9a3273; 3 not run; mask FFFFF3FC)` the same way. No odd failure in either; no step was rerun.

## Readings this carry makes stale on `dev/s2`
- The S2 exit (540 runs, read at 8231171): every arm's runs move with D-045, D-046 and Hard Blow, and the share row is now judged by loss. It must be reread; it is not reread here (an exit battery runs alone, D-050 item 4).
- `S1_keeper_power.md` probes on `dev/s2` (T-KEE-15, T-KEE-18 at d827a58) and every seed 9107 hash in the `S2_*.md` notes.

## Not resolved by the rules
- `07_Technical.md` keeps `main`'s line "`Settings/` arrives with `dev/s2`": left as it is, nothing contradicts it in `Content/DT/`.

## Second carry: `main` 7f75ada into `dev/s2` 432bedd (`carry2/s2`)
`main` gained the telemetry marks (`hero_damaged` carries `arrival` and `charging`, `run_start` carries `bot_fast`; T-TEL-14, T-TEL-15) and the scheduler's records.
- Conflicts: `Docs/13_Decisions`, `STATE.md`, `AGENTS.md` take `main`'s side whole (`git diff 7f75ada -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty). `DTEnemyPool.h`/`.cpp`: `main`'s telemetry-only `ContactStep` beside `dev/s2`'s enemy-time `StepRoles(Dt, LeadDt, ...)`, `StepElites`, `StepBlasts` and `PublishTelegraphs(Out, Heroes)`. `number_change_log.csv`: every row of both sides. `08_Constants.md`, `constants.yaml` and the generated header: the floor only, set after.
- `dev/s2`'s other hero-hit writers, the Volatile blast and the Vampiric drain (`DTEnemyPool.cpp`), write `arrival` and `charging` false by `FDTHeroHit`'s defaults; no logic added. No `dev/s2` reader or test pins a whole `hero_damaged` or `run_start` line.
- Ids: T-TEL-14 and T-TEL-15 collide with nothing.
- Counts: 478 tests found, 478 pass; `C.GATE.TEST_FLOOR` 476 to 478; `tools_test.py` 147.
- Seed 9107, five arms: `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848` before (read on b246064, whose code 432bedd holds unchanged) and after (99dc843): unchanged. RestartCheck `276faf8e` in the gate.

```
tests:      478 run of 478 found, 0 failed, floor 478, 0 crash marker(s)
GATE: PASS (99dc843; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no odd failure.
## Third carry: `main` d17458f into `dev/s2` 431cd40 (`carry3/s2`)
`main` gained `dir_block.dropins_landed_by_role` (T-TEL-16) and the hold of a fast bot's decided run at its deciding step (T-TEL-17).
- Conflicts: the scheduler's records take `main`'s side whole (`git diff d17458f -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty). `telemetry.md`: `main`'s `dir_block` row beside `dev/s2`'s `run_end` reasons. `number_change_log.csv`: every row of both sides. The floor and the generated header. `dev/s2` added no field to `dir_block`.
- The bot: `dev/s2`'s bot v2 ends a decided run on the same path as v1, `UDTBotSubsystem::HandleStepEnd` (the fast loop `RunSteps` steps the sim and every step ends there), so `main`'s hold covers both; nothing added. T-TEL-17 passes. Bot v2 `Steered:Storm:Tempest` on seed 9107, run twice: `3b526497`, 57635 steps and `run_end.steps` 57635 both times.
- Pinned values: none moved. No `dev/s2` test or tool check pins a fast bot's step count or closing record; RestartCheck reads 5400 / 5400.
- Ids: T-TEL-16 and T-TEL-17 collide with nothing.
- Counts: 480 found, 480 pass; `C.GATE.TEST_FLOOR` 478 to 480; `tools_test.py` 148.
- Seed 9107, read on 431cd40 and on 8c4dd45: `input_hash` unchanged on all five (`00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`); the record's `steps` reads two lower on each (44139 to 44137, 59382 to 59380, 57751 to 57749, 60233 to 60231, 48600 to 48598), which is this fix.

```
tests:      480 run of 480 found, 0 failed, floor 480, 0 crash marker(s)
GATE: PASS (8c4dd45; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no odd failure.
## Fourth carry: `main` 458e0ad, then 9b2917f, into `dev/s2` a7137b4 (`carry4/s2`)
`main` gained the heavy-job slots and the alone hold (`Tools/Machine.ps1` `Enter-DTHeavy`, `Tools/Alone.ps1`, R-TEC-18 and T-TEC-18, `HeavyLocks` in `tools_test.py`), D-055 and D-056 with today's records, and one line in `12_Production.md §1` (the module sessions).
- Conflicts: none. Git merged `07_Technical.md`, `12_Production.md`, `BotRuns.ps1`, `Gate.ps1`, `Perf.ps1` and `tools_test.py` by itself; R-TEC-18 and T-TEC-18 collide with nothing on this branch. `git diff 9b2917f -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty. `main` moved to 9b2917f by records only (D-056) after the gate; that merge was clean and the docs lint and secret scan pass on it (83cc828).
- Scripts changed (one `Enter-DTHeavy` line right after `Set-DTAffinity`, and the names in `test_every_heavy_entry_takes_a_slot_and_play_does_not`): `S2Exit.ps1` (not under `-ReadOnly`, which only reads), `RestartCheck.ps1`, `PackageRestartCheck.ps1`. `main`'s own entry scripts came with the merge.
- Left alone: `gen_matrix.py` (the `matrix` step: Python that reads catalogs, no heavy job), `s2_exit.py`, `bot_compare.py`, `elite_read.py`, `evolve_read.py`, `reaction_read.py` (read runs and logs only), and the helpers and launchers `main` already leaves alone (`HiddenLaunch.ps1`, `WindowWatch.ps1`, `GpuLock.ps1`, `Play.bat`).
- **`S2Exit.ps1 -Parallel 6` is one heavy job.** It takes one slot for the whole battery; the `BotRuns.ps1` it starts inherits it, and its six (or twelve) bot launches at once are that one job's processes. So a slot is a job, not a core: an exit battery fills the machine on one slot while other slots stay free for others. That is why an exit battery runs alone (D-050 item 4): take the machine with `Tools\Alone.ps1 -Take` first, so no other heavy job starts beside it.
- Counts: 480 found, 480 pass, `C.GATE.TEST_FLOOR` 480 (unchanged: the carry adds no engine test); `tools_test.py` 155.
- Seed 9107 (`BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107`, on a65b7a7): `00e0d4ec`, `51369747`, `e3293942`, `b046559b`, `def51848`, all as before. The gate's restart step: `276faf8e / 276faf8e`.

```
tests:      480 run of 480 found, 0 failed, floor 480, 0 crash marker(s)
tools:      TOOLS: PASS (155 tests)
GATE: PASS (a65b7a7; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no odd failure. Log: `Saved\Logs\carry4_s2_gate.txt` in the carrying worktree.

## Fifth carry: `main` 75acd0d into `dev/s2` a132539 (`carry6/s2`)
`main` brought D-057 to D-064 and its records, the gate's slot taken at its first heavy step (`Start-HeavyStep`), the same window-watch fix, and session A's reading tools (`Tools/session_read.py`, `pressure.py`, `owner_read.py`, `power_split.py`, the export manifest).
- Conflicts and resolutions:
  - `Docs/13_Decisions`, `STATE.md` and `AGENTS.md` take `main`'s side whole; `git diff 75acd0d -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
  - `Tools/Gate.ps1`: `dev/s2`'s own heavy step, `restart`, now takes `Start-HeavyStep` like `main`'s; the old `Enter-DTHeavy` line at the gate's start is gone. `matrix` stays a text step (`Invoke-PyStep`, it launches nothing).
  - `Tools/WindowWatch.ps1`: `main`'s copy of the same fix (its comment names `dev/s2`).
  - `Validation/S1_kit.md`: `main`'s session A text. The kit describes `main`'s package, and `dev/s2`'s "Between runs" line about Restart run described the S2 build.
  - `Tools/tools_test.py`: `session_read` beside `dev/s2`'s matrix and `LimitWarnings` cases. The heavy-entry list takes `main`'s form without `Gate.ps1` and keeps `dev/s2`'s `S2Exit.ps1`, `RestartCheck.ps1` and `PackageRestartCheck.ps1`. `GateSlot`'s stand-in gate stubs `dev/s2`'s `gen_matrix.py`, as `GateMain`'s does.
- Scripts changed beyond `main`'s: `Tools/Gate.ps1` (the restart guard) and `Tools/tools_test.py` (the stub). No test pins behaviour that differs by branch.
- Counts: 480 tests, floor 480; `tools_test.py` 193.
- Seed 9107 on 460ba97, unchanged: None `00e0d4ec`, Random `51369747`, Steered:Forge:Hammer `e3293942`, Steered:Storm:Tempest `b046559b`, Steered:Tide:Frost `def51848`; RestartCheck `276faf8e`.

```
tools:      TOOLS: PASS (193 tests)
tests:      480 run of 480 found, 0 failed, floor 480, 0 crash marker(s)
GATE: PASS (460ba97; 3 not run; mask FFFFF3FC)
```
`dev/s2` had not moved when the leg was gated. Log: `Saved\Logs\c6s2_gate.txt` in the carry agent's worktree.
## Sixth carry: `main` 842259e into `dev/s2` fe107d7 (`carry7/s2`)
`main` brought `s1/crash-exit` (R-PKG-16 and T-PKG-16: a packaged Shipping game that crashes ends, set in `Source/Dusktracer/Dusktracer.cpp`; `Tools/CrashExitCheck.ps1`, `crash_exit.py`, their tools tests, `Validation/S1_crash_exit.md`), D-059 to D-065 and their records, and `offers.md` saying what the code does (D-062, D-063).
- Conflicts and resolutions:
  - `Docs/13_Decisions`, `STATE.md` and `AGENTS.md` take `main`'s side whole; `git diff 842259e -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
  - `offers.md` R-OFF-08: `main`'s unweighted L8 hand (D-062) with `dev/s2`'s line that an unlocked S2 Keeper keeps a seat in both hands (R-KEE-23).
  - `package.md`: `dev/s2`'s R-PKG-10 and T-PKG-10 beside `main`'s R-PKG-16 and T-PKG-16; the HUMAN line is `dev/s2`'s, which keeps the Restart run check in Shipping.
  - `Tools/tools_test.py`: both entry lists (`Machine.ENTRY_PS` and the heavy-entry list) hold `main`'s `CrashExitCheck.ps1` and `dev/s2`'s `S2Exit.ps1`, `RestartCheck.ps1` and `PackageRestartCheck.ps1`.
  - `Dusktracer.cpp` merged clean. `constants.yaml` did not change on either side, so the generated header is as the gate's `constants` step reads it.
- Scripts changed beyond `main`'s: none. No test pins behaviour that differs by branch.
- Counts: 480 tests, floor 480 (crash-exit adds no engine test); `tools_test.py` 193 to 199, the crash-exit cases.
- Seed 9107 on 46ed31b, unchanged: None `00e0d4ec`, Random `51369747`, Steered:Forge:Hammer `e3293942`, Steered:Storm:Tempest `b046559b`, Steered:Tide:Frost `def51848`; RestartCheck `276faf8e`.

```
tools:      TOOLS: PASS (199 tests)
tests:      480 run of 480 found, 0 failed, floor 480, 0 crash marker(s)
GATE: PASS (46ed31b; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run); no golden re-shot, no odd failure. `dev/s2` had not moved when the leg was gated. Log: `Saved\Logs\c7s2_gate.txt` in the carry agent's worktree.

`main` moved by tools and documents while the leg was gated (842259e to d23b7ef: the alone take claims the machine at once, `Tools/Alone.ps1` and `Machine.ps1` with their tools tests; D-066; the kit's crash-dump line). It merged clean as 4c53ad4, and `git diff d23b7ef -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty; no engine or content file changed, so the text steps ran again with every heavy step skipped:

```
tools:      TOOLS: PASS (203 tests)
GATE: PASS (4c53ad4; 14 not run; mask FFFFF3FC)
```
Log: `Saved\Logs\c7s2_text.txt`.

## Seventh carry: `main` 1e99644 into `dev/s2` 0d2f1ca, then `dev/s2` d1ceee1 (`carry8/s2`)
`main` brought:
- D-078: a Scatter Shell ring bursts once (`bRingChild` on a ring's child, R-MOD-05, `Dusktracer.Modifiers.ScatterOneGeneration`).
- D-080: the raised camera (`ARENA.CAMERA_FRAME_RADIUS_UU`, `ARENA.CAMERA_AIM_SHIFT_UU`), and the boss row and the XP block where the first visual wave puts them (R-UX-01, T-UX-01).
- The gate's `arms` step with `Tools/gate_arms.csv`, `Tools/bot_summary.py`'s emission block, the session reader on the drawn plane, and the records through its tip.

`main`'s game code is that of 1a9aa55; its later commits are tools and documents. `dev/s2` moved during the leg by tools and one note (d1ceee1, the probe's field calibration). It merged clean as cbde5b0, before any heavy step but the build: the gate ran on the merged tip.

Conflicts and resolutions:
- `Docs/13_Decisions`, `STATE.md` and `AGENTS.md` take `main`'s side whole; `git diff 1e99644 -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty.
- `Combat/DTWeaponSim.cpp`: both lines at a round's expiry, `dev/s2`'s `bPayday` and D-078's `bRingChild`. The header merged clean, with `bRingChild` beside `bRingStrike`.
- `Tests/Combat/DTModifierTests.cpp`: `dev/s2`'s file whole, with `main`'s `ScatterOneGeneration` before the closing `#endif`. A plain union of the two hunks cut the last lines of `dev/s2`'s `ChainTrigger.Singularity`, and the build said so; 04eaa7d mends it.
- `Boss/DTBossSubsystem.cpp`: `main`'s position (the name in `NameBox`) with `dev/s2`'s scale (`NameScale * TextScale`).
- `Game/DTHUD.cpp`: `main`'s banner position under the boss row, with `dev/s2`'s `TitleScale` (the setting's scale), and `dev/s2`'s reaction toast below it as it was.
- `Progression/DTXPBar.h`: `main`'s top-right block and `BlockSpan`, with `dev/s2`'s `TextScale` argument. The .cpp merged clean and uses both.
- `Tools/Gate.ps1`: `dev/s2`'s step list in the header with `main`'s `arms`; the body merged clean.
- `Tools/bot_summary.py`: `dev/s2`'s deaths by minute and `main`'s emission and refusal lines.
- `Tools/tools_test.py`: `main`'s `hashlib` import; the entry lists merged clean.
- `number_change_log.csv`: both sides' rows.
- `constants.yaml` and `08_Constants.md`: `GATE.TEST_FLOOR` is the merged count, 489. `08_Constants.md`'s row read 480 against the yaml's 486 before the carry, stale since D-060. No step compares the two values: the gate's `constants` step compares `constants.yaml` with the generated headers only, and `docs_lint.py` check 4 compares the two files' keys, not their values. `DTConstantsGen.h` is regenerated.
- `Tools/gate_arms.csv` came as `main`'s file. It now holds `dev/s2`'s own five rows, with a header line naming this carry.

**The boss row and the text-size setting** (read, mended nothing). On a 1920x1080 frame, with the name `The Warden` measured by `UFont::GetStringHeightAndWidth` at the setting's least (100 %) and largest (150 %) size, through `UDTBossSubsystem::BossRowLayout`:

| | 100 % | 150 % |
|---|---|---|
| bar box (`BossRowRect`) | x 729.1 to 1289.1, y 15.0 to 29.0 | x 750.7 to 1310.7, y 15.0 to 29.0 |
| name's line box | x 630.9 to 717.1, y 11.6 to 30.4 | x 609.3 to 738.7, y 6.9 to 35.1 |

- The bar's lower edge is y 29 at both sizes: the bar's box does not grow with the setting.
- The name stays left of the hero's column (x 960) at both sizes.
- T-UX-01 reads `BossRowRect`, which takes no text scale, so it reads the same row at every size.
- The name's line box reaches y 35.1 at 150 %, below the bar's edge, but off the column. The glyphs' own ink was not measured.

**Counts.**
- 489 tests (dev/s2 488, plus `ScatterOneGeneration`), 489 pass; floor 489, with a number-log row.
- `tools_test.py` 248.
- 08_Constants.md row mended.

**Seed 9107** on the merged build (`input_hash`, steps, `bot.csv` SHA-256, outcome, end):

| Arm | `dev/s2` before | carry8/s2 | Scatter Shell socketed |
|---|---|---|---|
| None | `00e0d4ec` | `00e0d4ec`, 44137, death 735.6 | none |
| Random | `51369747` | `15a228cd`, 59951, victory 999.2 | Ring Emitter at 194.7 s |
| Steered:Forge:Hammer | `e3293942` | `8831aba6`, 57482, victory 958.0 | Pulse Rifle at 569.5 s |
| Steered:Storm:Tempest | `b046559b` | `0948c7be`, 59239, victory 987.3 | Pulse Rifle at 579.0 s |
| Steered:Tide:Frost | `def51848` | `def51848`, 48598, death 810.0 | none |

The three that moved are the three that socket Scatter Shell (D-078). The two that socket none read as before.

**Goldens.** `arena_empty` en and zh-Hans were shot again with `Tools/image_check.py --update` on `Tools/Smoke.ps1` shots of the merged build. Against `dev/s2`'s old goldens the mean diff is 1.52 (en) and 1.59 (zh-Hans), of 12.0. The camera is the reason: the raised frame and the moved HUD.

**D-074's judged pair read again** (D-078 item 7): `Steered:Storm:Tempest:Tide -Carry Weapon.RingEmitter` and its hold, 15 seeds, v2, skill 1.0, on the merged build.

| Window | D-074 landed | carry8/s2 | Bar |
|---|---|---|---|
| before the boss | 1.01 | 1.00 | 0.95 |
| boss fight, by the boss's measure (median; pooled) | 1.07 | 1.22; 1.14 | 0.9 |
| boss fight, the old window | 1.12 | 1.18 | 0.9 |

Both lines PASS.

The emission block's last line:
- Arm: "Emission budget: some weapon and socket set passes 0.05 (2 weapon run(s) over it; worst 0.146, Weapon.RingEmitter [ChainTrigger+LingeringField+ScatterShell] in Steered-Storm-Tempest-Tide-Carry-RingEmitter_s4)."
- Hold: "Emission budget: some weapon and socket set passes 0.05 (1 weapon run(s) over it; worst 0.117, Weapon.RingEmitter [LingeringField+ScatterShell] in Steered-Storm-Tempest-Tide-Carry-RingEmitter_s11)."

**Readings this carry makes stale on `dev/s2`:**
- every reading with Scatter Shell socketed (D-078): seed 9107's Random, Forge:Hammer and Storm:Tempest arms;
- the D-074, D-071 and D-079 batteries where a run sockets it;
- the S2 exit battery;
- every golden and frame shot before the raised camera (D-080).

The whole gate (`& Tools\Gate.ps1 -Base dev/s2`) ran on 5a2efe1, which is the tree above with `main` 1879298 merged; `main` had moved by decision records before the gate began.

```
tools:      TOOLS: PASS (248 tests)
tests:      489 run of 489 found, 0 failed, floor 489, 0 crash marker(s)
arms:       PASS (None 00e0d4ec, Random 15a228cd, Steered:Forge:Hammer 8831aba6, Steered:Storm:Tempest 0948c7be, Steered:Tide:Frost def51848)
restart:    RESTART: PASS (seed 9107, input_hash 276faf8e / 276faf8e, steps 5400 / 5400)
GATE: PASS (5a2efe1; 3 not run; mask FFFFF3FC)
```
Every step passed but pad, scope and mac (not run). Log: `Saved\Logs\c8s2_gate.txt` in the carry agent's worktree.

`main` moved again by decision records while the leg was gated (1879298 to 06d4f7c: D-082, D-084). It merged clean as 57a6812, and `git diff 06d4f7c -- Docs/13_Decisions Docs/STATE.md AGENTS.md` is empty. The text steps were then run again with every heavy step skipped.
- The first two runs failed at `cpu`: `CPU: FAIL (processor 16)`, "processor 16: timeout 120 s".
- `cpu` is the processor canary, which reads the machine, not the tree, and it had passed in the whole gate an hour before. It was then skipped, and the text steps read:

```
tools:      TOOLS: PASS (248 tests)
GATE: PASS (57a6812; 16 not run; mask FFFFF3FC)
```
Log: `Saved\Logs\c8s2_text.txt`. The canary's fault on processor 16 is a finding for the machine, not for this carry.
