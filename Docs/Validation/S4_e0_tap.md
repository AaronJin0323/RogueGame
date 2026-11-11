# S4 wave 1, package E step 0: the hit and kill tap

For the scheduler. Scope: `Validation/S4_wave1.md` §3 "E" (D-043 item 6, D-055 item 6), first step only. Branch `s4/e0-tap` from `dev/s4` at fc5000b. Spec: `02_Systems/impact_art.md` R-IMP-01 and R-IMP-02. Every reading below was taken on this branch by a Tools script, with no window.

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

| Reading | Before (sim of fc5000b) | After (the tap) |
|---|---|---|
| seed 9107 None `input_hash` | | |
| seed 9107 Random `input_hash` | | |
| seed 9107 Forge:Hammer `input_hash` | | |
| seed 9107 Storm:Tempest `input_hash` | | |
| seed 9107 Tide:Frost `input_hash` | | |
| restart | | |
| meta | | |
| bot.csv SHA-256, None | | |
| bot.csv SHA-256, Random | | |
| bot.csv SHA-256, Steered:Forge:Hammer | | |
| bot.csv SHA-256, Steered:Storm:Tempest | | |
| bot.csv SHA-256, Steered:Tide:Frost | | |

Tests: not run yet. Floor `C.GATE.TEST_FLOOR` 573 (572 plus T-IMP-01).

Gate: not run yet.

## 5. Not verified

- Everything in §4 until it is filled.
- `sizeof(FDTImpact)` as a build prints it.
- No reader exists yet, so R-IMP-02 (read once per sim step, after `ResolveHits`) is not exercised by anything.
- Nothing is drawn or played: there is nothing for a person to look at or listen to.
