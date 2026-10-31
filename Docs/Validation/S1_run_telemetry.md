# S1 run telemetry fixes

Class **BUG**, layer telemetry. Every change is read-only on the sim; it replaces what the records said, adds no gameplay. Base `996cd47` (S1 build). The trigger was one designer run, not a validator session: `2147483647_20261031_231306.jsonl` + `.csv`, build `996cd47`.

## What it read before
- Two casts logged `enemies_hit` 0, `damage` 0 (t 429.3 and 550.2) with about 1040 and 1100 `Keeper.Storm` applied in the six seconds before each record.
- `swarm_ttk_s` read 1.4 to 5.2 s against a 0.3 s bound: it was spawn to death of every role, so no run could read `C.TTK.SWARM_STEERED_MAX`.
- Every weapon and modifier record was absent. The last `dir_block` ended at 562.5 s, so the 0.8 s up to `hero_down` at 563.3 s, the fatal hit among them, sat in no block.
- `run_start.wall` read `23:13:06Z` for a local 23:13. `run_start.input_device` read `unknown`, the only value it could hold.

## Decisions (placed in the specs; D-numbers are assigned at integration)
1. **Weapon records are written by `UDTWeaponSubsystem`**, the chokepoint every production path uses (offers, the Patron's starting weapon, bot and debug loadouts), not by `FDTWeaponSim`, which stays free of telemetry. Emissions and level and load rank are reported each step. Dormancy comes from `Compose`'s existing change list, which `StepWeapons` now keeps.
2. **`swarm_ttk_s` keeps its column name** (R-TEL-06) and takes the true meaning: first hit to death of a Swarm enemy (R-TEL-03). An elite of the role counts; other roles give no sample; a one-shot reads zero. `kill.ttk_*` follows it. **Spawn-to-death is gone, not renamed:** nothing reads it (`Tools/` on `main` and on `dev/s2` hold no TTK reader at all), so the task's condition for keeping it under an honest name is not met.
3. **A cast's record is a window, not a tag.** A hit counts when it lands from `Keeper.<Base>` or the layer's `Keeper.<Path>` between the cast's start and one step after its end. Tagging hits was rejected: `FoldClause` merges the overlay into the Keeper's own flags, so a Split the cast widened and a Split a blessing grants are one flag and no tag can tell them apart, and it would thread a field through the bullet pool for nothing. The price is stated in `keepers.md`: passive Keeper damage in the window counts, so the record is attribution; R-KEE-15's marginal stays `Tools/probe.py`'s.
4. **`damage` is applied everywhere, a raw reading says so** (R-TEL-11). The record's `damage` was raw and now means what `hit`, `sources` and `weapon` mean; `damage_raw` is new. A cast the run cut short is written with `complete` false, and `bot_summary.py` keeps it out of the means.
5. **The fatal block closes in the Hero phase of the fatal step** (R-DIR-15), bound to `OnAllHeroesDown`, because the sim keeps stepping until the session closes (up to a whole frame's steps in a fast bot run). Other ends close through a new `OnSessionEnding` on the telemetry subsystem, `ended_by` the session's reason. The boss fight is now a block: `OpenBlock` after the zero-length `request` record, closed at the run's end.
6. **`run_start.input_device` is removed**, not fixed: no input exists when `run_start` is written, and the CSV column already reads the real device per interval. `wall` is `FDateTime::UtcNow`; the file name keeps the local time (R-TEL-07).
7. **`C.TEL.HERO_POS_INTERVAL_S` is 2.5 s**, whole uu. 2.0 s leaves no margin on the lean runs (below). `FDTHeroHit::Position` had no reader, so the three writers that stored the hero's own position there now store the source: a landed Rusher band its origin, the Sweep its origin, the ring its nearest safe edge.

## Readings
Seed 9107, `Tools/BotRuns.ps1`, five arms, base `996cd47` against this branch. The base ran twice: the hashes repeated.

| Arm | Outcome | End s | `input_hash` base = fix | JSONL bytes base -> fix | Growth | `hero_pos` alone |
|---|---|---|---|---|---|---|
| Steered Forge Hammer, Storm | victory | 981.38 | `9385c307` | 4305865 -> 4325693 | +0.46 % | 0.67 % |
| Steered Storm Downpour, Forge | victory | 970.17 | `0e2d73e8` | 2858325 -> 2880479 | +0.78 % | 1.00 % |
| Steered Storm Tempest | victory | 967.90 | `e96dc0a8` | 2089282 -> 2108821 | +0.94 % | 1.37 % |
| Steered Tide Frost, Storm | death | 481.48 | `36f48262` | 502096 -> 517814 | +3.13 % | 2.82 % |
| None (no picks) | death | 675.45 | `1c738e97` | 514552 -> 534719 | +3.92 % | 3.86 % |

Growth is every new record and field together, weapon and modifier records included, less the 2 to 4 % of `kill` bytes that non-Swarm kills no longer spend on TTK. Without that saving the None arm grows 4.55 %. At 2.0 s it would read 4.9 %, or 5.5 % without the saving, which is why 2.5 s. The owner's run writes 1.0 KB/s, like these short arms: 2.5 s adds an estimated 2.9 % to it (computed, not measured).

**The sim did not move.** On all five arms the hash, outcome, end time, picks and dashes match; `kill` records match step for step in count and source; the CSV matches outside `swarm_ttk_s` and `build`, but for the Tide arm's last partial row. That row covers one more step after the death (28891 steps against 28890). The base does the same between its own two runs (28891, 28890): wall time decides how many steps run before the session closes. It is not this change.

**Swarm TTK**, median of the interval cells against `C.TTK.SWARM_STEERED_MAX` 0.3 s:

| Arm | Old column | New column | Band |
|---|---|---|---|
| Storm Tempest | 5.317 | 0.383 | HIGH |
| Forge Hammer, Storm | 3.767 | 0.700 | HIGH |
| Storm Downpour, Forge | 2.150 | 1.333 | HIGH |
| Tide Frost, Storm (dies at 481 s) | 8.000 | 0.017 | IN |
| None (dies at 675 s) | 10.283 | 0.000 | IN |

No number was tuned on this; a bot at skill 1.0 is a bound. The None arm reads zero because the Rifle's two rounds land in one step and kill together. The median hides a long tail: its mean over the run is 6.4 s and one Swarm enemy died 426 s after its first hit, a straggler that the crowd kept out of the nearest-target pick. That is real, not stale stamps: on the same sim, for every step with kills, the new maximum and sum never exceed the old spawn-to-death ones on any arm, and that enemy had lived 472 s.

**Casts**, old `enemies_hit`/`damage` -> new `enemies_hit`/`damage` (applied), raw:
- Storm Downpour, Forge: 0/0 -> 25/358, raw 489; 0/0 -> 51/965, raw 1136; 0/0 -> 55/1099, raw 1181.
- Storm Tempest with Frost, the last casts: 0/0 -> 82/3726, raw 4414; 0/0 -> 55/3213, raw 3591.
- Forge Hammer: 6/228 -> 48/550, raw 2271; 3/114 -> 52/1349, raw 2948. Raw runs far above applied: most of it is overkill.
- Tide Frost: 146/0 -> 146/42, raw 42. The freeze is the cast; what its Keeper's sources landed in the window is small.

Passive damage in the window is real: on Storm Tempest the same sources landed 1504 in the six seconds before the 3213 cast. Read `damage` as what landed during the cast, not what the cast added.

**`dir_block`:** blocks now sum to the run's kills on every arm (None 1306 -> 1307 of 1307, Tide Frost 1167 -> 1317 of 1317, the wins gain the boss fight's 15 to 36), and `dmg_taken` sums to the `hero_damaged` shares (1.080, 1.053, 0.791, 0.419, 0.500). Tide Frost's last block: Threat, 450.0 to 481.47, `ended_by` death, 150 kills, the block its base run lost.

**Fatal hits now read:** None: Swarm contact, hero (3960, 3642) against the arena's east edge, source (3963, 3584), 58 uu. Tide Frost: Tank contact, hero (-1635, 1038), source (-1727, 1073), 98 uu. Whether either was dodgeable is a human reading of the trace; the data now exists.

**Weapon records** on the wins: 3 `weapon_acquired`, 9 `weapon_level`, 2 to 4 `modifier_socketed`, 38 to 46 `weapon`. No arm produced a `modifier_dormant`: no bot build converted a socketed weapon. `Dusktracer.Telemetry.LoadoutRecords` proves that path in a real world.

## R-TEL-08
No wall-clock field was added. `run_start.wall` stays the only change, to UTC. `window_s` is sim time. The tests' `StripWall` (`DTTelemetryTests.cpp`, `DTBotTests.cpp`) drop `wall` and `wall_*`; neither pattern touches `window_s`, `hero_pos` or the hit positions, which T-TEL-08 now requires identical across runs. `input_hash` mixes the bot's inputs only, and no tool on `main` compares JSONL.

## Not read here
- A human run on this build: the new columns have bot readings only.
- `Tools/S2Exit.ps1` is not on `main`. On `dev/s2` its bot_summary reads `intercession.damage` and picks up the new meaning at merge; no tool there reads TTK.
- The R-KEE-15 and T-KEE-18 probes were not rerun: they read `hit` records, which did not change.
