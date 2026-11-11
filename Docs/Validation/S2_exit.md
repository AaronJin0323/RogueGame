# S2 exit readings (2026-10-31)

Branch `dev/s2-exit`, from `dev/s2` at 0747e3c. Class: SYSTEM (a tool) plus one spec rule. Layer: the measuring tools. Replaces nothing; no game number or behaviour changed, nothing was tuned, T-KEE criteria are unchanged. **A bot reading is never a human reading.** No human has played S2. This is a reading, not an exit: S2 has not passed.

## Reproduce
```
& Tools\S2Exit.ps1 -Out Saved\BotRuns\s2_exit -Parallel 12
& Tools\S2Exit.ps1 -Out Saved\BotRuns\s2_exit -ReadOnly
```
By hand, about an hour: the S2 row says nightly, nothing schedules it. 18 arms (nine S1 showcase with no second, the nine of `-ArmSet S2Keepers`), 15 seeds, bot v2 at skill 1.0 and 0.6, `-Unlock S2` so the full arsenal is in the pool, 540 runs. T-KEE-18 is builds 1 to 5 of the skill 1.0 batch, one seed, 480 scenes. T-KEE-15 is all 282 combinations of six Keepers, 2 seeds, 1128 scenes. Probes are v1, skill 1.0 (R-BOT-01), on the 40x field. The build stamp reads 0747e3c+dirty: the tree was these tools and constants over 0747e3c, no game source.

## Reread, 2026-11-10, `dev/s2` at d827a58: the current reading
Branch `docs/s2-reread`. Class: none, a reading. No game code, number, pass line or tool changed. Every run's build stamp reads `d827a58`, clean. **A bot reading is never a human reading**: these are bounds, and S2 has not passed.

```
& Tools\S2Exit.ps1 -Out Saved\BotRuns\s2_exit -Parallel 6
```
The battery as designed: the same 18 arms, seeds 1 to 15, bot v2 at skill 1.0 and 0.6, `-Unlock S2`, 540 runs; T-KEE-18 on builds 1 to 5 of the skill 1.0 batch, 462 scenes; T-KEE-15 on all 282 combinations, 2 seeds, 1128 scenes. Only `-Parallel` differs, 6 against 12, because this PC faults under heavy parallel load; a run is deterministic per seed, so that changes no reading. Launches went through `Tools/BotRuns.ps1` on the hidden desktop. Every run left a record, every cast scene has one cast, no scene took a wrong Patron, and no run was rerun.

`S2EXIT: 1 PASS, 3 FAIL, 2 NOT READ`, the same verdicts. "Before" is the 2026-10-31 reading at f2ef7ed (`S2_trace.md`), the last one taken before D-026 and D-027.

| Reading | Verdict | 0747e3c (below) | f2ef7ed (`S2_trace.md`) | d827a58, this reread | Moved |
|---|---|---|---|---|---|
| Keeper share | **FAIL** | 10/18, mean 0.598 | 8/18, mean 0.607 | 8/18 arms pass, mean 0.607 | no: all 18 rows equal f2ef7ed to the printed digit |
| Build bands | **FAIL** | 11/18 | 9/18 | 9/18 arms in band | no |
| Intercession | **FAIL** | 50/282 | 46/282 | 46/282 in band | no: all 282 means equal f2ef7ed to the unit |
| Bots win a full run | PASS, a bound | Storm 24, Forge 33, Tide 8, Shade 41, Radiance 34, Quicksilver 44 | Storm 24, Forge 41, Tide 5, Shade 39, Radiance 35, Quicksilver 42 | wins of 45: Storm 24, Forge 41, Tide 5, Shade 39, Radiance 35, Quicksilver 43 | Quicksilver +1, a real change of the sim (below) |
| Humans win a full run | NOT READ | human only | human only | human only (O-008) | no |
| Death histogram | NOT READ | human only | human only | human only; the bot's is below | no |

### What moved and why
- **D-026 and D-027 did not make these rows stale.** The three probe rows sum `hit` records whatever their source and count `intercession` records (`Tools/probe.py`); neither decision changed those. The reread proves it: the T-KEE-18 table and all 282 T-KEE-15 means equal the f2ef7ed reading (one SD differs by 2, Hammer with a Vector layer). What D-026 and D-027 made stale are two columns of `bot_summary.py` that no exit row reads, below.
- **One arm really moved: Quicksilver/Vector with Forge.** Paired by arm and seed against the f2ef7ed batch, 263 of 270 runs at skill 1.0 and 266 of 270 at 0.6 have the same `input_hash`; the 11 that differ are all Vector:Forge. Wins there 12 to 13 of 15 at skill 1.0 and 4 to 5 at 0.6, one outcome flipped at each skill. The first record that differs (seed 13, skill 1.0, 371.13 s) is a dash volley whose source reads `Keeper.Quicksilver` where it read `Keeper.Forge`: D-027's rule, a free volley no longer takes a Keeper conversion's source (`FDTWeaponSim`, `bFree`), and on `dev/s2` a Quicksilver dash volley is a free volley. The next hit then deals 6 for 7 and the run diverges. So D-027 is not attribution-only on this arm; D-027's own note confirmed "the input stream is unchanged" on Forge/Hammer only. Read from the code and the records, not bisected; `S2_dash_volley.md` then isolated it on that seed: the hit's damage is the same, and what moves play is the dash refund those kills now earn (the 6 for 7 is the record's rounding, not health). A shift of 1 on an arm is inside noise as a balance reading.
- **Attribution share at L16** (`summary.csv` `keeper_share_l16`, not an exit row): 10 of 270 runs moved at skill 1.0, 3 at 0.6. Arm means move by at most 0.006 (Hammer 0.789 to 0.785, Vector:Forge 0.418 to 0.424, Hunters:Forge, Frost and Whirlpool by 0.001). This is the Chain Trigger bias of D-027 at its size in these builds.
- **Cast damage** (`summary.csv` `cast_damage`, not an exit row) changed meaning with D-026 and the old column must not be compared: arm means at skill 1.0 read 0 to 2467 before and 365 to 1307 now. The four arms that read 0 (Momentum, Rebound, Economy, Hound) now read 365 to 862, and ArcGrid falls from 2467 to 1307.

### T-KEE-18 at d827a58: FAIL
Share below on 10 arms, gain outside on 16, outside the archetype band on 9. Equal to the f2ef7ed reading, which this note never held as a table.

| Arm | Builds | Full DPS | Share | Package gain | Share | Gain | Band |
|---|---|---|---|---|---|---|---|
| Forge/Furnace | 5 | 11288 | 0.559 | 1.59 | FAIL | FAIL | IN |
| Forge/Hammer | 5 | 13801 | 0.636 | 2.59 | PASS | FAIL | IN |
| Forge/Slam | 5 | 7807 | 0.541 | 1.06 | FAIL | FAIL | IN |
| Quicksilver/Momentum | 5 | 13754 | 0.527 | 2.15 | FAIL | PASS | IN |
| Quicksilver/Rebound | 5 | 10931 | 0.551 | 1.95 | FAIL | FAIL | IN |
| Quicksilver/Vector | 5 | 12306 | 0.795 | 0.97 | PASS | FAIL | OUT |
| Radiance/Lance | 5 | 13312 | 0.688 | 2.22 | PASS | PASS | IN |
| Radiance/Prism | 5 | 11567 | 0.587 | 1.54 | FAIL | FAIL | IN |
| Radiance/Solar | 3 | 7188 | 0.601 | 2.47 | PASS | FAIL | IN |
| Shade/Economy | 5 | 19297 | 0.380 | 2.59 | FAIL | FAIL | OUT |
| Shade/Hound | 5 | 5995 | 0.499 | 1.62 | FAIL | FAIL | IN |
| Shade/Hunters | 5 | 12388 | 0.795 | 0.96 | PASS | FAIL | OUT |
| Storm/ArcGrid | 2 | 5804 | 0.386 | 2.98 | FAIL | FAIL | OUT |
| Storm/Downpour | 4 | 7444 | 0.455 | 3.71 | FAIL | FAIL | OUT |
| Storm/Tempest | 3 | 4127 | 0.360 | 1.19 | FAIL | FAIL | OUT |
| Tide/Frost | 3 | 8968 | 0.829 | 0.97 | PASS | FAIL | OUT |
| Tide/Stillness | 4 | 10317 | 0.896 | 1.08 | PASS | FAIL | OUT |
| Tide/Whirlpool | 3 | 9510 | 0.835 | 1.03 | PASS | FAIL | OUT |

### T-KEE-15 at d827a58: FAIL

| Keeper | In band | Min | Max | Mean | Casts that lose damage |
|---|---|---|---|---|---|
| Storm | 9/46 | -400 | 725822 | 57469 | 1 |
| Forge | 16/45 | 21736 | 80552 | 51434 | 0 |
| Tide | 1/47 | -25083 | 28688 | 632 | 27 |
| Shade | 0/48 | -24630 | 3954879 | 597933 | 2 |
| Radiance | 19/48 | -15640 | 109839 | 27036 | 2 |
| Quicksilver | 1/48 | -21122 | 113504 | 8052 | 10 |

Shade's mean is still Economy's payday on a refilled field, and Tide's cast still deals no damage of its own (`S2_trace.md` items 1 and 3): nothing here was decided or tuned.

### Bot histogram at d827a58, printed, not judged

| Bot skill | Runs | Deaths | Dead before 600 s | Wins | Boss kills of reachers | Deaths by minute |
|---|---|---|---|---|---|---|
| 1.0 | 270 | 83 | 0.14 | 0.69 | 0.07 of 201 | 2:1 3:4 4:1 5:3 6:3 7:12 8:6 9:9 10:9 11:7 12:5 13:3 14:6 15:7 16:6 17:1 |
| 0.6 | 270 | 176 | 0.13 | 0.35 | 0.51 of 193 | 4:5 5:4 6:5 7:6 8:9 9:7 10:13 11:8 12:6 13:6 14:8 15:74 16:23 17:2 |

Against f2ef7ed: deaths 84 to 83 and 177 to 176, the two Vector:Forge flips. Skill 0.6 still loses 0.51 of its boss reachers against the stated third.

### Not read, and not checked
- Humans win a full run, and the three clauses of R-BOT-17: human only (O-008).
- The gate was not run for this note: it changes no code and no tool. `python Tools/docs_lint.py` passes.
- Which commit moved Vector:Forge was read from the code, not bisected.

### Addendum, 2026-11-10, at 8231171: the Quicksilver arms and the Vector probe rows, after the drones fix and D-038
Branch `fix/dash-volley-rule`, from `dev/s2` e40943c. A reading: nothing tuned, pass lines unchanged. **A bot reading is never a human reading**; these are bounds. Two things came after the reread above: the BUG fix that made an Orbital Drones or Sweep Beam dash volley Quicksilver's at `dmg_mult` (a4d14ad), and D-038, under which a dash volley acts on Blade Halo, Cyclone and Pulse Wall (`S2_dash_volley.md`).

```
& Tools\S2Exit.ps1 -Out Saved\BotRuns\s2_exit -Parallel 6
```
The whole battery again, the options of the reread above, because the reader takes all 18 arms and the probes take their builds from the skill 1.0 batch: three arms alone give no verdict line. 540 runs, 462 ablation scenes, 1128 cast scenes. Every run's build stamp reads `8231171`, clean; every run left a record; none was rerun. "Old" is the batch of the reread above, d827a58, paired run by run on `input_hash`.

`S2EXIT: 1 PASS, 3 FAIL, 2 NOT READ`, the same verdicts.

| Reading | Verdict | Old, d827a58 | New, 8231171 | Moved |
|---|---|---|---|---|
| Keeper share | **FAIL** | 8/18, mean 0.607 | 8/18, mean 0.606 | the Vector row alone, 0.795 to 0.781 |
| Build bands | **FAIL** | 9/18 | 9/18 | no |
| Intercession | **FAIL** | 46/282 | 46/282 | no row changes side; 30 of the 31 rows with Vector move in value |
| Bots win a full run | PASS, a bound | Quicksilver 43/45 | Quicksilver 43/45; the other five Keepers equal | no |

**Runs.** 533 of 540 keep their `input_hash`: 266 of 270 at skill 1.0, 267 of 270 at 0.6. All 15 arms without Quicksilver are equal run for run. The seven that differ:

| Skill | Arm, seed | Old | New |
|---|---|---|---|
| 1.0 | Vector:Forge 2 | `ebdf06d1` victory 1011.98 s, 117 dashes | `1089a40b` victory 1023.53 s, 144 dashes |
| 1.0 | Vector:Forge 6 | `63ddad25` victory 967.65 s, 138 | `a082f394` victory 983.23 s, 161 |
| 1.0 | Vector:Forge 9 | `23469e3f` victory 937.03 s, 156 | `5c1d6704` victory 963.95 s, 154 |
| 1.0 | Momentum:Storm 9 | `a33b0f06` victory 939.93 s, 62 | `d3bdfad3` victory 993.22 s, 62 |
| 0.6 | Vector:Forge 6 | `c959f287` death 881.18 s, 76 | `f1f728fd` death 461.38 s, 30 |
| 0.6 | Vector:Forge 9 | `1b9d746e` death 871.65 s, 84 | `382dc9f6` death 678.87 s, 56 |
| 0.6 | Momentum:Storm 9 | `2d8966cc` victory 962.38 s, 22 | `a7feb58e` victory 962.00 s, 23 |

- Each of the seven holds a dash volley (a Vector card or Slipstream Drive) with Orbital Drones: the drones fix. `S2_dash_volley.md` counted eight such runs; the eighth, Rebound:Storm seed 13 at 0.6, keeps its hash. Why it does was not traced.
- Seed 9 of Vector:Forge at 1.0 is the paired run of `S2_dash_volley.md`, the same two hashes.
- No outcome flips. Two losing runs at 0.6 die earlier, by 420 s and 193 s; two runs is no trend.
- **D-038 is not read by this battery.** No run of the 540 evolves Orbital Drones, Arc Blade or Ring Emitter (Reforging picks, counted from the records: ten evolutions, Flak Mortar three, Pulse Rifle two, and one each of Gravity Mine, Rail Lance, Scatter Cannon, Seeker Swarm and Spiral Projector; three are on Quicksilver arms, Flak Mortar, Pulse Rifle and Spiral Projector), the ablation builds are level 16 snapshots taken before the Reforging hand, and the cast scenes' loadout is Pulse Rifle, Arc Blade and Orbital Drones, none evolved (no scene file names an evolution). So no bot run here holds a dash volley with Blade Halo, Cyclone or Pulse Wall, and D-038's size in play is unread. Its tests are its only reading.

**The three Quicksilver arms**, 15 seeds each.

| Arm | Skill | Runs that differ | Wins old | Wins new | Reach the boss old / new | Median end old / new, s | Mean dash presses old / new | Keeper share old / new |
|---|---|---|---|---|---|---|---|---|
| Vector:Forge | 1.0 | 3 | 13 | 13 | 13 / 13 | 967.6 / 970.0 | 101.3 / 104.5 | 0.688 / 0.699 |
| Vector:Forge | 0.6 | 2 | 5 | 5 | 10 / 10 | 918.8 / 918.8 | 47.8 / 42.9 | 0.543 / 0.510 |
| Momentum:Storm | 1.0 | 1 | 15 | 15 | 15 / 15 | 952.8 / 952.9 | 49.3 / 49.3 | 0.587 / 0.587 |
| Momentum:Storm | 0.6 | 1 | 9 | 9 | 15 / 15 | 952.5 / 952.5 | 19.7 / 19.7 | 0.535 / 0.534 |
| Rebound:Storm | 1.0 | 0 | 15 | 15 | 15 / 15 | 958.6 / 958.6 | 37.5 / 37.5 | 0.705 / 0.705 |
| Rebound:Storm | 0.6 | 0 | 14 | 14 | 14 / 14 | 949.4 / 949.4 | 19.2 / 19.2 | 0.649 / 0.649 |

Keeper share here is `summary.csv` `keeper_share`, the attribution column, not the exit row. Deaths stay 83 and 176; at 0.6 the histogram moves two deaths (minute 14 to minutes 7 and 11), which lifts "dead before 600 s" from 0.13 to 0.14 as printed.

**T-KEE-18, the row that holds a Vector card.** The other 17 rows equal the table above to the printed digit.

| Arm | Builds | Full DPS old | new | Share old | new | Loss without blessings old | new | Gain old | new | Verdicts |
|---|---|---|---|---|---|---|---|---|---|---|
| Quicksilver/Vector | 5 | 12306 | 12914 | 0.795 | 0.781 | 0.879 | 0.880 | 0.97 | 0.97 | share PASS, gain FAIL, band OUT, as before |

The mean share over 18 arms goes 0.607 to 0.606, so the archetype band's centre moves with it; no arm changes side.

**T-KEE-15, every row that holds Vector**, mean marginal damage of one cast, 2 seeds. The probe loadout holds Orbital Drones at level 3 and the route dashes every 6 s, so each of these was stale. No row changes its IN or OUT.

| Keeper | Variant | Layer | Old | New |
|---|---|---|---|---|
| Quicksilver | Vector | none | 75 | 104 |
| Quicksilver | Vector | Radiance_Lance | 23642 | 23668 |
| Quicksilver | Vector | Forge_Slam | 8404 | 8433 |
| Quicksilver | Vector | Shade_Economy | 3794 | 3824 |
| Quicksilver | Vector | Tide_Stillness | 1796 | 1826 |
| Quicksilver | Vector | Tide_Frost | 783 | 822 |
| Quicksilver | Vector | Forge_Hammer | 739 | 870 |
| Quicksilver | Vector | Shade_Hunters | 690 | 719 |
| Quicksilver | Vector | Shade_Hound | -176 | -146 |
| Quicksilver | Vector | Radiance_Solar | -310 | -281 |
| Quicksilver | Vector | Storm_Downpour | -606 | -538 |
| Quicksilver | Vector | Storm_Tempest | -739 | -710 |
| Quicksilver | Vector | Storm_ArcGrid | -908 | -830 |
| Quicksilver | Vector | Tide_Whirlpool | -1082 | -1042 |
| Quicksilver | Vector | Radiance_Prism | -1518 | -1463 |
| Quicksilver | Vector | Forge_Furnace | -21122 | -21288 |
| Forge | Slam | Quicksilver_Vector | 80217 | 80206 |
| Forge | Hammer | Quicksilver_Vector | 44105 | 44138 |
| Forge | Furnace | Quicksilver_Vector | 42390 | 42642 |
| Radiance | Prism | Quicksilver_Vector | 61234 | 60902 |
| Radiance | Lance | Quicksilver_Vector | 24373 | 24365 |
| Radiance | Solar | Quicksilver_Vector | 16302 | 16302 |
| Shade | Economy | Quicksilver_Vector | 1991394 | 1991364 |
| Shade | Hunters | Quicksilver_Vector | 5998 | 5986 |
| Shade | Hound | Quicksilver_Vector | 2482 | 2470 |
| Storm | Downpour | Quicksilver_Vector | 58374 | 58118 |
| Storm | ArcGrid | Quicksilver_Vector | 31949 | 33351 |
| Storm | Tempest | Quicksilver_Vector | 28688 | 28676 |
| Tide | Stillness | Quicksilver_Vector | 678 | 668 |
| Tide | Frost | Quicksilver_Vector | -706 | -718 |
| Tide | Whirlpool | Quicksilver_Vector | -1357 | -1366 |

- 15 of the 16 rows with Vector as the variant rise by 26 to 131; Vector with a Furnace layer falls by 166. The rise is small because the cast changes little for Vector: the drones' dash bolts gain ×1.5 with and without the cast.
- With Vector as the layer the dash volley exists only during the cast, at ×1.0, so the fix moves its label and little else: 11 of 15 rows move by 33 or less, Solar by nothing. The four larger moves are ArcGrid +1402, Prism −332, Downpour −256 and Furnace +252, on SDs of 881, 8596, 6802 and 509 over 2 seeds. Not traced.
- The other 32 Quicksilver rows (Momentum and Rebound as the variant) move by 2 or less. A Quicksilver cast fires a dash volley for every variant (R-KEE-35), and its drones' bolts changed source at ×1.0, which moves the per-source rounding carry (R-TEL-05). Read from the code, not traced.
- The 219 rows with neither Quicksilver as the Keeper nor Vector as the layer equal the old means and SDs to the unit. Their printed ratios move in the second decimal where the Keeper's mean moved.

| Keeper | In band old | new | Mean old | new |
|---|---|---|---|---|
| Storm | 9/46 | 9/46 | 57469 | 57494 |
| Forge | 16/45 | 16/45 | 51434 | 51440 |
| Tide | 1/47 | 1/47 | 632 | 632 |
| Shade | 0/48 | 0/48 | 597933 | 597932 |
| Radiance | 19/48 | 19/48 | 27036 | 27029 |
| Quicksilver | 1/48 | 1/48 | 8052 | 8062 |

**`S1_keeper_power.md` §8 is not stale.** Its T-KEE-15 and T-KEE-18 probes ran the nine S1 showcase arms with nothing unlocked: Storm, Forge and Tide, 57 combinations, no Quicksilver card in any scene. They were not rerun. The probe rows that hold a Vector card are the S2 ones above.

**Seed 9107** on 8231171, unchanged: None `7bd32faf`, Random `f5270196`, Steered:Forge:Hammer `c6573c69`, Steered:Storm:Tempest `d7c8b43c`, Steered:Tide:Frost `209a3446` (`Tools\BotRuns.ps1`, defaults, one process); RestartCheck `276faf8e` on the gate.

**Not read.** D-038 in any bot run (above). Humans, and the three clauses of R-BOT-17 (O-008). Why Rebound:Storm seed 13 at 0.6 keeps its hash. Which hits moved the four larger layer rows.

### Addendum, 2026-11-11, at 146af0a: the reread after the three carries of `main`
Branch `s2/exit-reread-3`, from `dev/s2` 146af0a. A reading: nothing tuned, nothing fixed, pass lines unchanged. **A bot reading is never a human reading**; these are bounds. It was taken alone on the machine in the carry agent's worktree, under the mask `FFFFF3FC`.

```
& Tools\S2Exit.ps1 -Out Saved\BotRuns\s2_exit -Parallel 6
```
The whole battery, with the options of the two readings above. 540 runs (17:41 to about 17:59), 656 ablation scenes and 1128 cast scenes, ending at 18:35. Every run's `build` reads `146af0a`, every run left a record, and none was rerun.

`S2EXIT: 2 PASS, 2 FAIL, 2 NOT READ`, against 1 PASS, 3 FAIL, 2 NOT READ at 8231171.

| Reading | Verdict now | 8231171 | 146af0a | Moved, and why |
|---|---|---|---|---|
| Keeper share | **PASS** | FAIL, 8/18 arms by share, mean 0.606 | 18/18 arms lose at least 0.6 without blessings, mean loss 0.832; share printed and never judged, mean 0.642 | **The measure changed (D-047)**: the row now judges loss, not share. That is not a change in the game. Judged by share as before, 10 of 18 arms would read at least 0.6 |
| Build bands | **FAIL** | 9/18 within ±0.15 of the mean share | 17/18 within ±0.15 of the mean loss; Storm/Tempest OUT at 0.603 | **The measure changed (D-047)**: the band now sits around the mean loss |
| Intercession | **FAIL** | 46/282 | 45/282 | Forge 16 to 13 of 45, Tide 1 to 2 of 47, Quicksilver 1 to 2 of 48; Storm 9/46, Shade 0/48 and Radiance 19/48 the same. Below |
| Bots win a full run | PASS, a bound | Storm 24, Forge 41, Tide 5, Shade 39, Radiance 35, Quicksilver 43 (of 45, skill 1.0) | Storm 30, Forge 42, Tide 15, Shade 42, Radiance 35, Quicksilver 45 | more wins on five Keepers. Below |
| Humans win a full run | NOT READ | human only | human only (O-008) | no |
| Death histogram | NOT READ | human only | human only; the bot's is below | no |

**What reached `dev/s2` since 8231171.** These moved the runs: D-045 (an arriving Rusher charges through where the hero stood), D-046 (a chest restores health), Hard Blow's knockback, Hammer's Key on the Arc Blade, the strict touch comparison, the two sim fixes of 406268e (Conduit under a Patron with no status, Refit's draw), and the fast bot's hold at the deciding step. D-047 changed the reader only. **Paired runs: the 8231171 batch's run files are not at hand** (they were in that reading's worktree, not this one), so how many of the 540 keep their `input_hash` is not read.

**Runs.** Survival rose at both skills.

| Bot skill | Runs | Deaths | Dead before 600 s | Wins | Reach the boss | Boss kills of reachers | Deaths by minute |
|---|---|---|---|---|---|---|---|
| 1.0, 8231171 | 270 | 83 | 0.14 | 0.69 | 201 | 0.07 | (above) |
| 1.0, now | 270 | 61 | 0.09 | 0.77 | 233 | 0.10 | 2:1 3:4 4:1 6:4 7:7 8:3 9:3 10:3 11:4 13:4 14:3 15:8 16:15 17:1 |
| 0.6, 8231171 | 270 | 176 | 0.13 | 0.35 | 193 | 0.51 | (above) |
| 0.6, now | 270 | 121 | 0.09 | 0.55 | 230 | 0.35 | 4:2 5:5 6:2 7:11 8:3 9:2 10:5 11:4 12:3 13:1 14:2 15:46 16:34 17:1 |

By Keeper, wins / reach the boss, of 45: at skill 1.0 Storm 30/31, Forge 42/44, Tide 15/34, Shade 42/44, Radiance 35/35, Quicksilver 45/45; at 0.6 Storm 21/37, Forge 30/43, Tide 5/26, Shade 27/43, Radiance 27/37, Quicksilver 39/44.
- The data cannot split the rise among the changes above. One of them restores health: at skill 1.0, 264 of 270 runs hold a `hero_healed` record, and a run regains 32.9 HP on average (D-046). Tide gains most, 5 to 15 wins at skill 1.0.
- At skill 0.6 the boss kills 0.35 of those who reach it, down from 0.51. Deaths in minutes 4 to 14 fell from 77 to 40, and deaths in minutes 15 and 16 from 97 to 80.

**T-KEE-18 at 146af0a: FAIL** (`T-KEE-18: FAIL (18 arms: loss below 0.6 on 0, gain outside the band on 16, outside the archetype band on 1; mean loss 0.832; share, printed and never judged, mean 0.642)`).

| Arm | Builds | Full DPS 8231171 | now | Share 8231171 | now | Loss now | Gain 8231171 | now | Gain with no second Keeper |
|---|---|---|---|---|---|---|---|---|---|
| Forge/Furnace | 5 | 11288 | 10002 | 0.559 | 0.614 | 0.810 | 1.59 | 1.62 | 2.47 |
| Forge/Hammer | 5 | 13801 | 10437 | 0.636 | 0.712 | 0.892 | 2.59 | 3.78 | 7.27 |
| Forge/Slam | 5 | 7807 | 7446 | 0.541 | 0.578 | 0.826 | 1.06 | 1.04 | 1.08 |
| Quicksilver/Momentum | 5 | 13754 | 14697 | 0.527 | 0.639 | 0.928 | 2.15 | 2.20 | 4.16 |
| Quicksilver/Rebound | 5 | 10931 | 9256 | 0.551 | 0.579 | 0.929 | 1.95 | 1.42 | 1.80 |
| Quicksilver/Vector | 5 | 12914 | 11272 | 0.781 | 0.850 | 0.905 | 0.97 | 0.97 | 0.97 |
| Radiance/Lance | 5 | 13312 | 12434 | 0.688 | 0.725 | 0.808 | 2.22 | 2.59 | 10.17 |
| Radiance/Prism | 5 | 11567 | 11355 | 0.587 | 0.597 | 0.912 | 1.54 | 1.56 | 2.32 |
| Radiance/Solar | 3 → 4 | 7188 | 6612 | 0.601 | 0.614 | 0.745 | 2.47 | 2.24 | 2.92 |
| Shade/Economy | 5 | 19297 | 7354 | 0.380 | 0.349 | 0.700 | 2.59 | 1.20 | 1.00 |
| Shade/Hound | 5 | 5995 | 7265 | 0.499 | 0.562 | 0.850 | 1.62 | 1.64 | 1.52 |
| Shade/Hunters | 5 | 12388 | 10828 | 0.795 | 0.855 | 0.891 | 0.96 | 0.96 | 0.99 |
| Storm/ArcGrid | 2 → 3 | 5804 | 4896 | 0.386 | 0.444 | 0.743 | 2.98 | 2.88 | 3.71 |
| Storm/Downpour | 4 | 7444 | 7522 | 0.455 | 0.493 | 0.846 | 3.71 | 5.08 | 5.91 |
| Storm/Tempest | 3 → 5 | 4127 | 3862 | 0.360 | 0.426 | 0.603 | 1.19 | 1.23 | 1.39 |
| Tide/Frost | 3 → 4 | 8968 | 5235 | 0.829 | 0.862 | 0.755 | 0.97 | 0.77 | 0.81 |
| Tide/Stillness | 4 | 10317 | 6941 | 0.896 | 0.851 | 0.923 | 1.08 | 1.34 | 1.42 |
| Tide/Whirlpool | 3 | 9510 | 8406 | 0.835 | 0.798 | 0.918 | 1.03 | 1.03 | 1.74 |

- The builds are other builds: an L16 snapshot exists only for a run that reached level 16, and the runs changed (four arms have more builds now). No row is the same build as before, so a row's move mixes the builds with the rules.
- Package gain: 2 of 18 in band now (Momentum, Solar), as before (Momentum, Lance). Hammer's gain rose from 2.59 to 3.78; Hammer's Key now reaches the Arc Blade, which the snapshots hold. Economy's full DPS fell from 19297 to 7354. Neither was traced further.
- The column "gain with no second Keeper" is new with D-047's probe; the reading at 8231171 has none.

**T-KEE-15 at 146af0a: FAIL**, 45/282.

| Keeper | In band 8231171 | now | Mean 8231171 | now |
|---|---|---|---|---|
| Storm | 9/46 | 9/46 | 57494 | 56413 |
| Forge | 16/45 | 13/45 | 51440 | 42911 |
| Tide | 1/47 | 2/47 | 632 | 911 |
| Shade | 0/48 | 0/48 | 597932 | 600282 |
| Radiance | 19/48 | 19/48 | 27029 | 27012 |
| Quicksilver | 1/48 | 2/48 | 8062 | 7985 |

- Most rows the earlier addendum printed hold to the unit: of the 31 rows with Vector, Forge/Slam 80206, Forge/Furnace 42642, Radiance/Lance 24365, Shade/Economy 1991364 and the Quicksilver/Vector-variant rows read as they did, except Vector with a Forge_Hammer layer, 870 to 634. The cast scenes run with the director off on a fixed route and loadout, so D-045 and D-046 do not reach them.
- **Forge/Hammer moved**: with a Vector layer, 44138 at 8231171 against 9538 now, and Forge's mean fell from 51440 to 42911. The cast loadout holds the Arc Blade, which Hammer's Key now reaches, and Hard Blow's cleaves now push enemies. Which of the two moves the row was not traced.

**`S1_keeper_power.md` §8 not reread.** Its commands are recorded: the nine showcase arms at 15 seeds, ablation of their builds, and the cast scenes at 3 seeds. At this battery's pace (540 runs in about 18 minutes, 656 ablation scenes in about 13, 1128 cast scenes in about 23), its 135 runs, about 1080 ablation scenes and 342 cast scenes would take about 33 minutes. That is more than the half hour the job allows, so `STATE.md`'s "stale" stands.

**Seed 9107** on `dev/s2` after the third carry (`S2_carry.md`, Third carry): None `00e0d4ec`, Random `51369747`, Steered:Forge:Hammer `e3293942`, Steered:Storm:Tempest `b046559b`, Steered:Tide:Frost `def51848`; RestartCheck `276faf8e`.

**Not read.** Humans, and the three clauses of R-BOT-17 (O-008). The pairing by `input_hash` against 8231171 (files not at hand). Which of the carried changes moved which arm. D-038 in play: no run was checked for a dash volley on an evolved weapon.

Files: the carry agent's worktree, `Saved\BotRuns\s2_exit` (`s2_exit.md`, `bots_1.0`, `bots_0.6`, `ab`, `ik`) and `Saved\Logs\s2exit_console.txt`.
## Exit readings of 2026-10-31 at 0747e3c, replaced by the reread above

| Reading | Verdict | Value | Pass line |
|---|---|---|---|
| Keeper share | **FAIL** | 10/18 arms pass, mean 0.598 | `C.POWER.KEEPER_SHARE_L16` per arm |
| Build bands | **FAIL** | 11/18 arms in band | `C.POWER.ARCHETYPE_BAND` of the arm mean |
| Intercession | **FAIL** | 50/282 in band | `C.INTERCESSION.BAND` of the Keeper's mean |
| Bots win a full run | PASS, a bound | wins of 45: Storm 24, Forge 33, Tide 8, Shade 41, Radiance 34, Quicksilver 44 | at least one win per Keeper at skill 1.0 |
| Humans win a full run | NOT READ | human only | O-008 |
| Death histogram | NOT READ | human only; the bot's is below | `bot.md` R-BOT-17 |

"Bots win" is the row's "can win" read literally, the weakest honest line: it says nothing about balance.

`S2EXIT: 1 PASS, 3 FAIL, 2 NOT READ`.

## T-KEE-18 across six Keepers: FAIL
Share below on 8 arms, gain outside on 16, outside the archetype band on 7. Builds are few where the bot rarely reaches level 16 (Whirlpool 2, ArcGrid and Frost 3).

| Arm | Builds | Full DPS | Share | Package gain | Share | Gain | Band |
|---|---|---|---|---|---|---|---|
| Forge/Furnace | 5 | 10188 | 0.618 | 1.37 | PASS | FAIL | IN |
| Forge/Hammer | 5 | 13350 | 0.659 | 2.82 | PASS | FAIL | IN |
| Forge/Slam | 5 | 8796 | 0.554 | 1.01 | FAIL | FAIL | IN |
| Quicksilver/Momentum | 5 | 14282 | 0.520 | 2.17 | FAIL | PASS | IN |
| Quicksilver/Rebound | 5 | 10874 | 0.542 | 1.95 | FAIL | FAIL | IN |
| Quicksilver/Vector | 5 | 12512 | 0.724 | 0.98 | PASS | FAIL | IN |
| Radiance/Lance | 5 | 12393 | 0.737 | 2.14 | PASS | PASS | IN |
| Radiance/Prism | 5 | 10782 | 0.624 | 1.69 | PASS | FAIL | IN |
| Radiance/Solar | 5 | 6715 | 0.749 | 1.88 | PASS | FAIL | OUT |
| Shade/Economy | 5 | 10950 | 0.413 | 1.69 | FAIL | FAIL | OUT |
| Shade/Hound | 5 | 6177 | 0.485 | 1.59 | FAIL | FAIL | IN |
| Shade/Hunters | 5 | 9352 | 0.636 | 1.00 | PASS | FAIL | IN |
| Storm/ArcGrid | 3 | 7026 | 0.394 | 3.03 | FAIL | FAIL | OUT |
| Storm/Downpour | 4 | 7755 | 0.449 | 3.56 | FAIL | FAIL | IN |
| Storm/Tempest | 4 | 5554 | 0.312 | 1.11 | FAIL | FAIL | OUT |
| Tide/Frost | 3 | 8668 | 0.751 | 0.90 | PASS | FAIL | OUT |
| Tide/Stillness | 4 | 8109 | 0.792 | 1.09 | PASS | FAIL | OUT |
| Tide/Whirlpool | 2 | 9137 | 0.812 | 0.95 | PASS | FAIL | OUT |

All three Storm arms miss share, as `STATE` already says of the formula. These builds took S2 cards, so this table is not the no-unlock table of `S2_keepers.md`: its S2 arms read lower here (Hound 0.485 against 0.580, on other builds).

## T-KEE-15 across six Keepers: FAIL

| Keeper | In band | Min | Max | Mean |
|---|---|---|---|---|
| Storm | 8/46 | -2995 | 209354 | 46253 |
| Forge | 17/45 | 3620 | 80552 | 51727 |
| Tide | 0/47 | -10718 | 28688 | 1374 |
| Shade | 2/48 | -22714 | 2297951 | 429237 |
| Radiance | 21/48 | -15640 | 80156 | 27279 |
| Quicksilver | 2/48 | -18132 | 38384 | 8847 |

Tide has no band: its cast deals no damage (`S1_tide_rule.md`). Shade's mean is Economy's payday (`S2_keepers.md`).

## A second probe fault, found and fixed here
The first T-KEE-15 reading here was void. `BotRuns -Jobs` unlocked the scene's second Keeper too; an unlocked Keeper leads the hand (R-KEE-23), so an S2 second pushed Tide out of the Patron hand and the bot took the S2 Keeper as Patron: 27 Tide combinations, 108 runs. The first reading said Tide 4/47, total 54/282.

- Fix: only the Patron is unlocked; the scene seats its second itself. `Tools/BotRuns.ps1`.
- Guard: `probe.wrong_patrons` reads each log's first pick against the scene, and `Tools/s2_exit.py` makes a reading with a wrong Patron NOT READ, so this cannot pass silently again. It would have caught the Storm fault too.
- Control: full rerun, 255 of 282 combinations identical to the unit, the 27 faulted ones moved. No run in the final reading took a wrong Patron.
- Reach: this hits S1 Patrons with S2 seconds only. `S2_keepers.md`'s tables have S2 Patrons and all 258 logs picked them; T-KEE-18 here picked every Patron, and no arm there has an S2 second on Tide.

## Death histogram: the source gives a shape, the bots do not read it
The source states one: `01_GameDesign.md` §3.5, Difficulty targets, which `G03_Retrospective_EN_ZH.md`'s S2 row names ("death histogram matches §3.5"). It is now `bot.md` R-BOT-17, keys `C.DEATH.*`.

- A first-time player dies before 10:00 on roughly half of first attempts.
- A player who has learned to steer a Path wins about 30 to 40 percent.
- The boss kills about a third of those who reach it.

Each clause is about humans, so the verdict stays NOT READ until sessions exist. Only the win band is a real band: "roughly half" and "about a third" have no tolerance in the source and none was invented. The 10:00 was not scaled when the source's clock moved to a 15-minute run; it is written as stated and left open in `bot.md`. The source also says if nobody dies the threats are HP walls, and if everyone dies in harvest the swarm leaks damage: no number, not read.

Printed, not judged, all 18 arms:

| Bot skill | Runs | Deaths | Dead before 600 s | Wins | Boss kills of reachers | Deaths by minute |
|---|---|---|---|---|---|---|
| 1.0 | 270 | 86 | 0.13 | 0.68 | 0.10 of 205 | 2:1 3:4 4:2 5:3 6:5 7:6 8:9 9:6 10:5 11:9 12:5 13:1 14:9 15:9 16:11 17:1 |
| 0.6 | 270 | 176 | 0.14 | 0.35 | 0.53 of 198 | 4:5 5:4 6:6 7:9 8:5 9:9 10:8 11:7 12:6 13:9 14:4 15:86 16:17 17:1 |

**Skill 0.6's wins sit inside the band, and that means nothing:** no skill is a player (R-BOT-13), and it was not picked to land there. Its 86 deaths in minute 15 are the boss (81 last hits): 0.53 of reachers die to it against the stated third, the first thing a human session should check. Skill 1.0 is a ceiling, and the source reads a strong dodger's wins as an upper bound for humans.

## Misses to read, none tuned
- Tide is weakest by wins: 8 of 45 at skill 1.0, 1 of 45 at 0.6, while its share of a build reads highest.
- Solar:Tide 6 wins of 15, Tempest 5, ArcGrid 8; every other S2 arm has 13 or more.
- Quicksilver 44 of 45 and Shade 41 of 45 at skill 1.0, against Storm's 24, with Storm as the second in most of those arms.
- Gain misses on 16 of 18 arms; `C.KEEPER.PACKAGE_GAIN_BAND` may not suit a build with a second Keeper, not decided here.

## Gate
`& Tools\Gate.ps1 -Base dev/s2` at d821859: GATE: PASS, 403 tests of 403, tools 40, 3 not run (pad, scope, mac). The only Source change is the regenerated constants header, which has no `.cpp`, so nonunity read 0 files.

## Human only
- Humans win a full run; the three clauses of R-BOT-17; O-008 sessions cover both, with first-attempt runs recorded as first attempts.
- Whether the 10:00 cutoff stands; what tolerance "roughly half" and "about a third" carry.
