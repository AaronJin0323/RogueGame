# S1 field tick (PAUSED, incomplete)

Status: **paused on the owner's request. The fix is written but has never been built or run. No after reading exists and no verdict is given.**

Port to `main` of the `dev/s2` fix for R-KEE-16 ("field ticks are not hits"): `bFieldTick` on `FDTHit`, set in `StepDeferred`, skipped in `FDTKeeperSim::CaptureHits`. It also stops ticks reaching `frozen_vuln` and the layer status, which read the same capture. The payday and Vector changes are not ported.

## Done
- The bug exists on `main`. `Dusktracer.Keepers.Param.FieldTick` seen red on main's production code (`80dd32a`): rolls 3008, expected 16; Chill on the enemy only the field touches 233, expected 0.
- Baseline below.

## Left, in order
1. Build; see `FieldTick` green.
2. `Tools\BotRuns.ps1` with the same arms, seeds and `-Parallel 12` on the fixed build; fill the after column.
3. `Tools\Gate.ps1` to PASS, verdict here. Docs lint has not been run on this file.

Open choice: `dev/s2` also named the new test in T-KEE-09 in `keepers.md`. Not ported, because the brief keeps T-KEE criteria unchanged.

## Baseline (before)
Bot readings at skill 1.0, 19 arms, seeds 1..15, built from `80dd32a` plus only the new test. Raw output: `Saved/BotRuns/field_before` in the worktree, git-ignored. A bot reading is never a human reading, and a shift of 2-3 on a 15-seed arm can be noise.

Steered: 198 runs reach the boss, 154 win. T-DIR-01 PASS 588.6 s. T-OFF-15 FAIL 197/198, which BotRuns reports as `FAIL` with exit 1; all 285 runs have a record. Both totals match `Docs/STATE.md`.

| Arm | Wins | Median end s | Level @630 | Keeper share |
|---|---|---|---|---|
| None | 0 | 588.6 | 9.467 | 0.17 |
| Forge-Furnace | 10 | 984.9 | 17.467 | 0.67 |
| Forge-Furnace-Tide | 6 | 814.9 | 17.133 | 0.582 |
| Forge-Hammer | 13 | 962.9 | 17.267 | 0.705 |
| Forge-Hammer-Tide | 10 | 968.1 | 17.467 | 0.673 |
| Forge-Slam | 11 | 977.4 | 16.8 | 0.642 |
| Forge-Slam-Tide | 3 | 943.1 | 16.533 | 0.544 |
| Storm-ArcGrid | 11 | 979.2 | 17.2 | 0.587 |
| Storm-ArcGrid-Forge | 12 | 976.6 | 17.533 | 0.681 |
| Storm-Downpour | 14 | 973.5 | 17.133 | 0.676 |
| Storm-Downpour-Forge | 14 | 990.8 | 17.133 | 0.743 |
| Storm-Tempest | 6 | 982.2 | 16.8 | 0.496 |
| Storm-Tempest-Forge | 14 | 986.6 | 17.667 | 0.599 |
| Tide-Frost | 7 | 967.0 | 14.6 | 0.511 |
| Tide-Frost-Storm | 9 | 1000.5 | 14.733 | 0.508 |
| Tide-Stillness | 3 | 957.1 | 14.133 | 0.404 |
| Tide-Stillness-Storm | 6 | 741.0 | 13.667 | 0.33 |
| Tide-Whirlpool | 3 | 682.0 | 14.067 | 0.405 |
| Tide-Whirlpool-Storm | 2 | 555.8 | 13.6 | 0.351 |
