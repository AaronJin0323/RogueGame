# S4 wave 1, package E step 0: the hit and kill tap

For the scheduler. Scope: `Validation/S4_wave1.md` §3 "E" (D-043 item 6, D-055 item 6), first step only. Branch `s4/e0-tap` from `dev/s4` at fc5000b, then merged with `dev/s4` at bc59250 (validation notes only). Spec: `02_Systems/impact_art.md` R-IMP-01 and R-IMP-02. Every reading below was taken on this branch by a Tools script, with no window.

## 1. What changed

- **Class:** PRESENT. The rules are right; presentation and audio cannot yet see what a step's hits did.
- **Layer:** presentation, through a read-only tap on the one sim-side file the package may edit, the enemy pool.
- **What it replaces:** nothing. It adds a step list that no sim code reads.

The pool's `ApplyHits` appends one `FDTImpact` for every hit it applies (past the dead check and the stale-serial check, the hits that send a `Hit` event), in the order applied. `ResolveHits` empties the list before it applies, keeping its storage, and `GetStepImpacts()` is its read-only view. `FDTEnemyPool::Reset` empties it too. The tap writes only its list and two integers that mark the step's Burn ticks; no value, event, order of operations or existing field moved, and no presentation header is included (`tools_test.py` `PresentationReadsOnly` passes).

## 2. The record

`FDTImpact` (in `Enemies/DTEnemyPool.h`): `Position` (contact point), `EnemyPosition`, `EnemyRadius`, `Dealt`, `MaxHp`, `EnemySerial`, `OwnerId`, `SourceId`, `WeaponSlot`, `Role`, `Flags`. Laid out as two `FVector2f`, three `float`, `uint32`, `int32`, `uint16`, `int8`, `EDTEnemyRole`, `uint8`: 41 bytes of fields, 44 with padding (worked out from the layout, not printed by a build).

`DTImpactFlags`: `Kill` (this hit set the enemy dead), `Crit`, `Pierce`, `FieldTick` (copied from the hit), `StatusTick` (a Burn tick the pool appended in this step's Enemies phase), `Plate` (the Tank front-arc multiplier was applied), `Shield` (the elite shield branch absorbed it), `Elite` (the hull is an elite).

`StatusTick` comes from a range: `StepEnemies` notes `View.Hits.Num()` just before and just after `TickStatuses`, and `ResolveHits` hands that range to a private `ApplyHitsTapped` and clears it after. The public `ApplyHits` passes an empty range, so a caller with its own hit list never gets the flag.

## 3. The order of `FDTWorldView::Hits`

The range is only right if nothing between the Enemies phase and `ResolveHits` removes, inserts or reorders entries. Read on this branch, every write to `View.Hits` in `Source/Dusktracer` outside the tests:
- appends (`AddDefaulted_GetRef`) in the bullet pool, the weapon sims (`DTWeaponSim.cpp`, `DTWeaponSimKeepers.cpp`, `DTWeaponSimDuos.cpp`) and the Keeper sim;
- in-place flag writes on entries just appended: `bFieldTick` on a field's own ticks (`DTWeaponSim.cpp`) and `bCrit` on the last hit (`DTWeaponSimDuos.cpp`); neither moves an entry;
- one `Reset`, in `FDTEnemyPool::ResolveHits`, after the hits are applied.

Nothing else removes, inserts, sorts or swaps. The one whole assignment is `UDTSimSubsystem::StartRun` (`View = FDTWorldView()`), which starts a run between steps, not inside one; and the pool's own `Reset` clears the range with the list. Hits the Effects phase appends after a `ResolveHits` stay at the front of the next step's list, before the Burn ticks, so the noted range still covers exactly the ticks.

## 4. Proof

The before side is the baseline of record, `Validation/S4_p0.md` §8 (the second table, read on `dev/s4` fc5000b; restart and meta from the third carry's gate). It was not rebuilt here. The after side was read on this branch at 3cd9a21 (the tap plus its test, before the docs-form commit 08a4724, which touches only `impact_art.md`). The five arms ran in one `Tools\BotRuns.ps1` call, one after another, seed 9107; restart and meta are the gate's lines on 08a4724.

| Arm, seed 9107 | `input_hash` before / after | `steps` before / after | `bot.csv` bytes before / after | SHA-256 before (first 16) / after |
|---|---|---|---|---|
| None | `00e0d4ec` / `00e0d4ec` | 44137 / 44137 | 535 / 535 | `b6a4878cb23adae7` / `b6a4878cb23adae707d58ee87d628e4c507b3606adeeb1b59fbf7b1489beef64` |
| Random | `51369747` / `51369747` | 59380 / 59380 | 663 / 663 | `485ed85694ad5c92` / `485ed85694ad5c92e3721200729a4839966b90930cdbefac027e357d3ec9eaa2` |
| Steered:Forge:Hammer | `e3293942` / `e3293942` | 57749 / 57749 | 679 / 679 | `b3cda2804c6a4664` / `b3cda2804c6a466426747d8758872df6e4c3d53025229372d37d73325028319b` |
| Steered:Storm:Tempest | `b046559b` / `b046559b` | 60231 / 60231 | 680 / 680 | `cab7cae12d42f789` / `cab7cae12d42f7898aa93d64a69071e22d27a1b07b04ce6c912da6f5ad925611` |
| Steered:Tide:Frost | `def51848` / `def51848` | 48598 / 48598 | 563 / 563 | `dc02a5d3f44c2d11` / `dc02a5d3f44c2d11e8846cf35385de829e8607f6c1b8f2f74257b9ea0dd4767b` |

| Gate line | Before | After |
|---|---|---|
| restart (RestartCheck, seed 9107) | `276faf8e` | `276faf8e` (5400 / 5400 steps, csv same) |
| meta | `ba2a3ac8` | `ba2a3ac8` (1200 / 1200 steps, csv same) |

No hash, step count or `bot.csv` moved. The `bot.csv` files agree in every digit the baseline records (its first 16 of the SHA-256) and in length.

Tests: `RunTests.ps1 -Filter Dusktracer.Impact` 1 run of 1, 0 failed; `-Filter Dusktracer.Enemies` 23 run of 23, 0 failed. The gate's `tests` step: 573 run of 573 found, 0 failed, floor 573 (572 plus T-IMP-01).

Gate: `GATE: PASS (08a4724; 3 not run; mask FFFFF3FC)`. The three not run are `pad` (no package), `scope` (no brief in flight) and `mac` (D-007). Build, shipping and devgame have 0 warnings.

The docs step failed on the first gate, on the spec's form. It was fixed in form only (08a4724): the decision item on line 3 went into a code span, the second step's constants are named as a new `IMP` group and not as a `C.` key that does not exist yet, and R-IMP-02's test is listed as T-IMP-02 PLANNED(S4) for the second step to build with its first reader.

## 5. Not verified

- `sizeof(FDTImpact)` as a build prints it: 44 bytes is worked out from the layout. No assert was added.
- No reader exists yet, so R-IMP-02 (read once per sim step, after `ResolveHits`) is not exercised by anything. T-IMP-02 is planned, not written.
- The before side was not rebuilt on this branch: it is the `S4_p0.md` §8 table.
- Nothing is drawn or played: there is nothing for a person to look at or listen to.