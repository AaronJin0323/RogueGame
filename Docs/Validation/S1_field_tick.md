# S1 field tick

**Verdict: BUG fixed on `fix/field-tick`. The S1 bot effect is small overall: 154 → 151 wins, with flips nearly balanced (18 lost, 15 gained). Two arms moved 4 in boss reach, past the noise of 2-3, so they are worth a look. No number was tuned, and T-KEE criteria are unchanged.** A bot reading is never a human reading.

Class BUG, weapon and Keeper layers. It replaces the path by which a field's tick reached the Keepers. It is a port to `main` of the `dev/s2` fix for R-KEE-16 ("field ticks are not hits"): `bFieldTick` on `FDTHit`, set in `StepDeferred`, skipped in `FDTKeeperSim::CaptureHits`. The payday and Vector changes are not ported. T-KEE-09 now names the new test; its pass criteria stay as they were.

Gate: `GATE: PASS (a0a266d; 3 not run)`, 235 tests run of 235 found, 0 failed. Not run: pad, scope, mac.

## The test
`Dusktracer.Keepers.Param.FieldTick` (4 s, one Tide Key, one Field):

| | Expected | Red, `80dd32a` production code | Green |
|---|---|---|---|
| Hits rolled | 16 (the rifle's) | 3008 | Success |
| Chill on the enemy only the field touches | 0 | 233 | Success |

The fix also stops ticks reaching `frozen_vuln` and the layer status, because they read the same capture. A tick's damage is unchanged. A field's own `status` pulse (R-KEE-22) is separate code and untouched.

## Bot reading
19 arms, seeds 1..15, skill 1.0, `-Parallel 12`, same command both sides. Before is `80dd32a` production code plus the test; after adds the fix. All 285 runs have a record on each side. Raw output: `Saved/BotRuns/field_before` and `field_after` (git-ignored). BotRuns prints FAIL and exits 1 on both sides only because of T-OFF-15.

| | Before | After |
|---|---|---|
| Steered runs at the boss | 198 | 192 |
| Wins | 154 | 151 |
| T-DIR-01 | PASS 588.6 s | PASS 588.6 s |
| T-OFF-15 | FAIL 197/198 | FAIL 191/192 |

T-OFF-15 has the same count of misses: one run on each side. Paired by arm and seed, 115 of 285 runs are unchanged, including all 15 of the control `None`. The other 170 diverge: dropping the rolls also changes the Combat stream draws, so a changed run is not a pure measure of the fix. 18 wins became losses and 15 losses became wins.

| Arm | Boss before | Boss after | Wins before | Wins after | Lost | Gained | Unchanged | Median end s before | after | Keeper share before | after |
|---|---|---|---|---|---|---|---|---|---|---|---|
| None | 0 | 0 | 0 | 0 | 0 | 0 | 15/15 | 588.6 | 588.6 | 0.17 | 0.17 |
| Forge-Furnace | 14 | 14 | 10 | 11 | 1 | 2 | 0/15 | 984.9 | 970.6 | 0.67 | 0.67 |
| Forge-Furnace-Tide | 6 | 7 | 6 | 4 | 2 | 0 | 1/15 | 814.9 | 853.5 | 0.582 | 0.572 |
| Forge-Hammer | 14 | 14 | 13 | 14 | 0 | 1 | 1/15 | 962.9 | 971.5 | 0.705 | 0.712 |
| Forge-Hammer-Tide | 13 | 9 | 10 | 8 | 2 | 0 | 4/15 | 968.1 | 944.0 | 0.673 | 0.673 |
| Forge-Slam | 14 | 13 | 11 | 11 | 1 | 1 | 6/15 | 977.4 | 977.0 | 0.642 | 0.637 |
| Forge-Slam-Tide | 10 | 6 | 3 | 3 | 2 | 2 | 4/15 | 943.1 | 872.0 | 0.544 | 0.525 |
| Storm-ArcGrid | 12 | 12 | 11 | 11 | 0 | 0 | 12/15 | 979.2 | 979.2 | 0.587 | 0.584 |
| Storm-ArcGrid-Forge | 15 | 14 | 12 | 13 | 0 | 1 | 8/15 | 976.6 | 975.3 | 0.681 | 0.679 |
| Storm-Downpour | 15 | 15 | 14 | 14 | 0 | 0 | 9/15 | 973.5 | 993.8 | 0.676 | 0.67 |
| Storm-Downpour-Forge | 15 | 15 | 14 | 14 | 1 | 1 | 0/15 | 990.8 | 983.0 | 0.743 | 0.749 |
| Storm-Tempest | 10 | 11 | 6 | 6 | 0 | 0 | 12/15 | 982.2 | 985.4 | 0.496 | 0.497 |
| Storm-Tempest-Forge | 15 | 15 | 14 | 15 | 0 | 1 | 2/15 | 986.6 | 983.5 | 0.599 | 0.598 |
| Tide-Frost | 10 | 9 | 7 | 4 | 4 | 1 | 4/15 | 967.0 | 913.6 | 0.511 | 0.521 |
| Tide-Frost-Storm | 11 | 12 | 9 | 9 | 1 | 1 | 8/15 | 1000.5 | 978.3 | 0.508 | 0.503 |
| Tide-Stillness | 9 | 9 | 3 | 4 | 0 | 1 | 4/15 | 957.1 | 936.6 | 0.404 | 0.368 |
| Tide-Stillness-Storm | 6 | 7 | 6 | 5 | 2 | 1 | 10/15 | 741.0 | 743.7 | 0.33 | 0.368 |
| Tide-Whirlpool | 5 | 4 | 3 | 2 | 2 | 1 | 6/15 | 682.0 | 446.7 | 0.405 | 0.354 |
| Tide-Whirlpool-Storm | 4 | 6 | 2 | 3 | 0 | 1 | 9/15 | 555.8 | 780.1 | 0.351 | 0.362 |

## Reading
- Most arms moved within the 2-3 that can be noise on 15 seeds.
- Past it: boss reach on Forge-Hammer-Tide (13 → 9) and Forge-Slam-Tide (10 → 6), both -4. Tide-Frost's wins (7 → 4, -3) sit at the edge of it.
- All three carry Tide, so losing tick-rolled Chill and `frozen_vuln` is a plausible cause. That is a hypothesis: nothing here measured it, and 15 seeds cannot separate it from the stream change. Other Tide arms moved the other way.
- The two large median swings on Whirlpool arms are arms of 2-3 wins whose median sits across a bimodal gap, not a clear effect.
- Held for a human reading, untuned.
