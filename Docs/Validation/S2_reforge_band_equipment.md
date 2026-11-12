# D-082: the equipment that reads the Reforging band (2026-11-13)

Branch `s2/reforge-band-equipment`, cut from `dev/s2` `7b3ebf7` (which holds D-079, the Blade Halo with its bolts), with `dev/s2` `f55e675` (D-073) merged in before the gate (`f5e3769`). Class: SYSTEM, measuring equipment. No rule of play, no number of the game; with every switch off every run reads as before (check line 1).

## Builds, and what was read on each
| Build | What it is | Read on it |
|---|---|---|
| `7b3ebf7` (detached) | `dev/s2` before the equipment | the hold battery of check line 1 |
| `34c6ae5` | the equipment, before the merge | the three suites; check lines 1, 2, 3 and 5; the first reading (line 4) at 5x (no-pick scenes), 10x, 16x (the fixed health), 20x, 40x, 80x and 160x |
| `f5e3769` and the notes on top | the equipment merged with D-073 | the three suites again; the whole gate; the five seed 9107 arms |

`Tools\Build.bat` ended `Result: Succeeded` on each.

## Part A: the decline arm (`bot.md` R-BOT-20, T-BOT-20)
- `-DTBotDeclineReforge`: on bot v2, the Reforging hand is closed with no pick (`UDTOfferSubsystem::Decline`); every other hand is played as without it, and bot v1 does not read it.
- With `-DTBotCarry` it is the carry arm's second control beside the hold: the same run to that hand, then no pick. It models no player: a player at the hand must take a card. It is the band's denominator.
- `BotRuns.ps1 -DeclineReforge` passes it. The run folder's label ends `-Decline`, the `BOTRUNS:` line says `decline`, and `config=` says `decline_reforge=1`.
- Test `Dusktracer.Bot.DeclineReforge`: on, a Reforging hand is declined and a level hand takes the card it takes off, with and without the carry arm's hold; off, the Reforging hand takes a card; bot v1 is not reached.

## Part B: a Reforging pick in a probe scene
- A scene script takes `reforge <id>` (`Bot/DTProbeScript`): one card of `reforging.csv`, a Path transformation or a weapon's evolution (its target is its row's source weapon).
- `UDTBotSubsystem::ApplyProbe` applies it once the build is in, through `DTOffers::ApplyPickState`, the pick's own state path. A Reforging card has no world step in the pick (`ApplyWorld` does nothing for it), so the scene holds the card as a run that took it holds it.
- A script without the line reads exactly as before: the stage's no-pick scenes are T-KEE-18's full configuration, script and seed, and give the same window damage to the unit (check line 3).
- After the merge, the pick path hands back what an evolution refuses (D-073), so a scene does too. No snapshot of the first reading held a modifier its evolution refuses under today's rows. The evolution arms on the same seeds record no dormant socket at their pick: `d079_rule_tempest_arm` 0 of 15, `d072_arm` 0 of 15, `d074_port_arm` 0 of 15. So the merge changes no scene of that reading. Under the Blade Halo's row before D-079, the base arm's seed 1 had Pierce dormant; D-079 gave the halo the plain weapon's caps back.

## Part C: `Tools/probe.py reforge` and its reader
- `python -I Tools/probe.py reforge <dir> --builds <runs>[,...] [--path-builds <runs>[,...]] [--hp-mult 40]`, in ablation's shape: each level 16 snapshot (`bot.csv.L16`) on ablation's field and window.
  - Scenes: no pick; the Patron's transformation; the second Keeper's, if there is one; the evolution of each weapon at `C.OFFER.REFORGE_EVOLVE_MIN_LEVEL` or more.
  - Snapshots under `--path-builds` (the showcase arms at the exit) read the two transformations only.
  - A build's cards share its seed, and the scenes are listed card by card, as ablation's configurations are, so two runs of one seed never start together.
- `probe.py read <dir>`, per card:
  - The lift on each build (window damage with the card over the no-pick scene's, same snapshot and seed), the median over its builds, the lowest and highest, and `in band`, `under` or `over` against `C.REFORGE.POWER_MIN` to `C.REFORGE.POWER_MAX`.
  - Fewer than ten builds (`REFORGE_MIN_BUILDS`) reads `NOT READ`.
  - Beside every evolution: the Patron's transformation, and the second Keeper's, on the same builds. The hand offers both, so both are printed.
  - A last line counts the cards read and in band. It is printed only and sets no exit code.
- The ceiling (item 2):
  - Every scene's kills in its window are printed and written to `<dir>/kills.csv` against `REFILL_CEILING_KILLS` = 160, and the stage reads `SATURATED` if any scene is at or over it.
  - 160 is the lower edge of the 160 to 176 kills that `S1_keeper_power.md` §3 read on the strong arms at 10x, where their numbers pressed together.
  - It is a flag, not the refill's real limit (below).
- Item 7's sentence prints with every reading: "The field reads power when the weapon is used: the probe's hero takes no damage and walks inside the crowd, so a contact weapon reads at its best here; whether a build survives using it is the play arms'."
- Tools tests (`ReforgeStage`, seven):
  - the cards of a snapshot;
  - the no-pick scene as T-KEE-18's full configuration, script and seed, on five snapshots;
  - the reader's rows, both Keepers' cards beside an evolution, and every scene's kills;
  - `NOT READ` under ten builds;
  - a scene at the ceiling saturates the stage;
  - the exit's band step.

## Part D: the strike fields
- `FDTHit::bStrike` marks the hits of D-074's marked strikes: a Pulse Wall ring's (`AddFieldHit`) and a Blade Halo blade's (`StrikeBlades`).
- The run file's `weapon` record gains `strike_hits` and `strike_damage`, counted inside `hits` and `damage`, not added to them. Telemetry only: no sim state reads them. `weapons.md` and `telemetry.md` take the two names.
- What they cost, by arithmetic: one bool on a hit, and two integer adds per marked hit in the telemetry's resolve walk. No perf pair was asked.
- Tests:
  - `Dusktracer.Reforging.StrikeMarks` (T-WPN-20): Pulse Wall 168 marked hits, Blade Halo 180, a plain Ring Emitter 0.
  - `Dusktracer.Telemetry.Records.Weapons` counts a marked hit in both pairs of fields.
- `Tools/evolve_read.py`'s strike line, from `dev/s2`, now reads.
  - On the one evolved Ring Emitter run of check line 5: "strikes: 0.09 of the weapon's hits and 0.12 of its damage after the pick (1 runs)".
  - A run from before the fields still reads "not read". No second reader was written.

## Part E: the exit battery's step
- `Tools/S2Exit.ps1`, after the showcase arms, for each of the ten weapons with an evolution (`reforging.csv`), runs three carry batteries on `Steered:Storm:Tempest:Tide` at the first skill: the evolution, the hold and the decline.
- Then it runs the `reforge` stage over their level 16 snapshots (the evolutions and Path cards from the carry folders, the Path cards alone from the first skill's showcase folders), its scenes, and the reader's `REFORGE:` and `CEILING:` lines.
- `-NoBand` leaves the step out, so the older exit is reproduced; `-ReadOnly` reads the stage if it exists. The tools test checks the three batteries' switches and that every call passes the pool the exit gives its kind.

## Part F: R-RFG-18
- The power clause is item 8's sentence word for word.
- The open line now says what reads the band and that no card has been judged against it.

## The equipment's own check (item 10)
1. **With the switch off nothing moves.**
   - The hold battery, `-Carry Weapon.OrbitalDrones -CarryHold` on `Steered:Storm:Tempest:Tide`, 15 seeds, bot v2 at 1.0, on `34c6ae5` against `7b3ebf7`: 15 of 15 runs equal in outcome, end time, steps and `input_hash`.
   - The five seed 9107 arms on the gated tree are below.
2. **With the switch on** (the same battery with `-DeclineReforge`, on `34c6ae5`):
   - 15 of 15 runs hold no `reforge` record.
   - 15 of 15 runs equal the hold's run record for record up to the Reforging hand's time (its `offer` record). The records are compared as text, with only the wall-clock fields that R-TEL-08 names set aside. The hand came at 564 to 569 s, and 8 457 to 10 676 records were equal before it.
3. **A no-pick scene equals T-KEE-18's full configuration** to the unit on 8 snapshots of `Saved\BotRuns\s1_reread_r2` (seed 1 of each showcase arm; one arm has no level 16 snapshot at seed 1). Same script, same seed, same window damage on `34c6ae5`: 43 596, 133 993, 128 549, 102 970, 93 704, 161 555, 97 600 and 9 582. The tools test shows it on five synthetic snapshots too.
4. **The first reading**, on `34c6ae5`, read-only on the run folders: `d079_base_tempest_hold` and `d072_hold` (the other developer's tree) and `d074_hold` (this tree). That is 45 snapshots: three evolutions on 15 builds each, Storm's and Tide's transformations on 45. As D-085 item 8 amends this clause:
   - The play rate is 43 kills a window.
   - The stages that bracket it are 10x (98) and 20x (27).
   - The fixed health is 16x.
   - 69 of its 180 scenes read "at least" (they killed more than the 120 that stand).
   - The other stages are printed beside it. 160x is the far stage, "bodies that do not die".
5. **The strike fields**: the five arms are unmoved (line 1 and the 9107 arms). One carry run of the evolved Ring Emitter shows strikes in its `weapon` records: the last interval reads `"hits":180,"damage":3179,"strike_hits":11,"strike_damage":284`.

## The reading: the stage at the fixed health, 16x (D-085)
**The health.**
- The play rate is the median, over each battery's 15 runs, of the kills in the minute that holds the Reforging hand, divided by six for the ten-second window:
  - `d079_base_tempest_hold`: 259 a minute, 43.2 a window
  - `d072_hold`: 262, 43.7
  - `d074_hold`: 259, 43.2
  - So 43 a window for the reading.
- The no-pick scenes' median kills on the series: 5x 159, 10x 98, 20x 27, 40x 1.
- Interpolated between 10x and 20x on the logarithms of health and kills, that is 15.57x, rounded to **16x**.
- Each battery alone, with its own play rate and its own no-pick scenes:
  - `d079_base_tempest_hold`: 13.52x (14x); its medians 149, 78, 20, 0.
  - `d072_hold`: 19.18x (19x); its medians 200, 116, 41, 2.
  - `d074_hold`: 15.27x (15x); its medians 143, 90, 27, 3.
  - The widest is a factor of 1.42, under one stage of the series.
- The play rate (43) is under half the standing field (60 of 120), so item 4 does not raise it.
- The cost, said as item 3 asks: the field's bodies are about sixteen times the health play gives a body at that minute, because play's stream cannot keep a level 16 build busy at all.

**The stage** (180 scenes, `hp_scale` 42.88):
- The no-pick scenes kill a median of 38 in the window (highest 249), 10 of 45 of them over 120. Their window damage median is 26 748.
- Lift by damage is what is judged (item 5). The lift by kills is printed beside it, since the no-pick median is twenty kills or more.
- "Over 120" counts a card's scenes that killed more than the 120 that stand; their figure reads "at least" (item 6).
- Beside each evolution: the Patron's and the second Keeper's transformations on the same 15 builds, by damage / by kills.

| Card | Builds | Lift by damage | Lift by kills | Scenes over 120 | The Patron's card | The second Keeper's card | |
|---|---|---|---|---|---|---|---|
| Transform.Storm | 45 | 1.40 | 1.79 | 19 | - | - | |
| Transform.Tide | 45 | 1.16 | 1.25 | 9 | - | - | |
| Evolve.OrbitalDrones | 15 | 1.37 | 1.80 | 4 | Transform.Storm 1.17 / 1.70 | Transform.Tide 1.24 / 1.32 | |
| Evolve.RingEmitter | 15 | 2.20 | 3.46 | 13 | Transform.Storm 1.31 / 1.67 | Transform.Tide 1.17 / 1.22 | |
| Evolve.SeekerSwarm | 15 | 2.21 | 2.70 | 14 | Transform.Storm 1.52 / 2.00 | Transform.Tide 1.12 / 1.26 | |

- No card is split by item 10. Orbital Drones' lift by kills, 1.80, is on the band's upper edge and so inside it.
- **No card is judged** (D-085 item 9): a card is in band only with its other side too, the damage dealt to the boss a minute against the arm that declines, which this job does not read.
- Item 7's sentence stands beside every figure: the field reads power when the weapon is used; whether a build survives using it is the play arms'.
## The other stages, side by side
At 5x only the no-pick scenes were read (D-085's series): kills median 159 in the window.
Kills are inside the 10 s window. Damage is applied damage (`telemetry.md` R-TEL-11: the health a hit removed, clamped to what was left), so the window's damage holds no overkill. Lift is "by damage / by kills", the median over builds, same snapshot and seed. The 40x field is the probe's default (`FIELD_HP_MULT`).

| Scenes | 10x kills med / max, ≥160, damage | 20x | 40x | 80x | 160x |
|---|---|---|---|---|---|
| no pick (45) | 98 / 294, 9, 24 160 | 27 / 229, 2, 28 383 | 1 / 159, 0, 30 014 | 0 / 55, 0, 29 480 | 0 / 0, 0, 29 480 |
| Evolve.OrbitalDrones (15) | 114 / 331, 4, 31 019 | 41 / 202, 1, 34 999 | 1 / 144, 0, 37 710 | 0 / 94, 0, 37 943 | 0 / 0, 0, 37 943 |
| Evolve.RingEmitter (15) | 170 / 278, 10, 41 112 | 121 / 180, 1, 61 002 | 68 / 122, 0, 85 075 | 1 / 18, 0, 89 534 | 0 / 0, 0, 89 534 |
| Evolve.SeekerSwarm (15) | 198 / 669, 14, 53 325 | 139 / 379, 3, 68 836 | 107 / 245, 2, 92 375 | 17 / 177, 1, 118 790 | 0 / 111, 0, 118 790 |
| Transform.Storm (45) | 157 / 354, 21, 36 143 | 64 / 247, 5, 40 502 | 5 / 183, 1, 38 029 | 0 / 121, 0, 37 145 | 0 / 0, 0, 37 145 |
| Transform.Tide (45) | 118 / 313, 8, 26 193 | 32 / 228, 2, 31 782 | 4 / 141, 0, 35 199 | 0 / 92, 0, 34 521 | 0 / 0, 0, 34 521 |

| Lift, by damage / by kills | 10x | 20x | 40x | 80x | 160x |
|---|---|---|---|---|---|
| Evolve.OrbitalDrones | 1.27 / 1.44 | 1.36 / 1.86 | 1.46 / 1.69 | 1.40 / 1.08 | 1.40 / - |
| Evolve.RingEmitter | 1.91 / 1.76 | 2.03 / 4.32 | 2.83 / 26.25 | 3.30 / 3.00 | 3.28 / - |
| Evolve.SeekerSwarm | 1.98 / 1.81 | 2.38 / 3.40 | 2.79 / 15.86 | 4.23 / 4.00 | 5.05 / - |
| Transform.Storm | 1.42 / 1.63 | 1.41 / 1.51 | 1.36 / 2.75 | 1.33 / 1.11 | 1.32 / - |
| Transform.Tide | 1.12 / 1.12 | 1.17 / 1.25 | 1.27 / 1.92 | 1.30 / 1.34 | 1.33 / - |

Beside each evolution, the Patron's transformation (Storm) and the second Keeper's (Tide) on the same 15 builds, by damage, at 40x and 80x:
- Orbital Drones: 1.00 and 1.27; 1.23 and 1.36.
- Ring Emitter: 1.24 and 1.23; 1.27 and 1.20.
- Seeker Swarm: 1.47 and 1.27; 1.46 and 1.36.

The reader's own lines (each stage's `probe.md` has the full table):
- 40x: `CEILING: SATURATED (most kills in one scene's window 245 of the refill's ceiling 160, 153 %, RF-b26-Evolve.SeekerSwarm_s2601)` and `REFORGE: 5 card(s) read, 2 in band, 1 under, 2 over; 0 not read (fewer than 10 builds); printed, judged nowhere`.
- 80x: `CEILING: SATURATED (most kills in one scene's window 177 of the refill's ceiling 160, 111 %, RF-b26-Evolve.SeekerSwarm_s2601)` and `REFORGE: 5 card(s) read, 3 in band, 0 under, 2 over; ...`.

**These stages judge nothing**; D-085 fixed the health from them. What holds at every health read:
- The Blade Halo stays in the band, 1.27 to 1.46.
- The Pulse Wall (1.91 to 3.30) and Murmuration (1.98 to 5.05) are over it and above both Path cards on the same builds.
- The two Path cards sit at the band's lower edge, 1.12 to 1.42.

Item 7's sentence stands beside every figure: the field reads power when the weapon is used; whether a build survives using it is the play arms'.

Context:
- **Play's kill rate**: the holds' own runs kill, in the minute that holds the Reforging hand, a median of 259 (`d079_base_tempest_hold`), 262 (`d072_hold`) and 259 (`d074_hold`) a minute. That is about 43 in a ten-second window (ranges 250 to 271). The no-pick build kills 98 at 10x and 27 at 20x.
- **The refill** (`UDTBotSubsystem::ApplyProbe`, `UDTEnemySubsystem`'s keep-alive):
  - The field is 120 Swarm (`FIELD`), each `C.ENM.SWARM_HP` times the scene's `hp_scale`. Every step the pool is topped back up to 120 at once.
  - A killed body comes back that same step as a new Swarm on the spawn ring, `C.ENM.SPAWN_RING_RADIUS_UU` from the hero, and walks in at the Swarm's speed, about 5.6 s.
  - There is no hard kill limit. A window can kill the 120 that stand plus what walks in from the ring in time.
  - The highest kills in any scene of the five stages is 669 (10x, `RF-b28-Evolve.SeekerSwarm_s2801`). 160 stays as the flag only.
- At 80x and 160x a scene in which nothing dies gives the same damage to the unit at both healths (the no-pick median is 29 480 at both). With nothing dying, applied damage is the build's output on the same bodies, so the lift by damage stops depending on health except for a card that still kills.

## Expectations and tests
- New: `Dusktracer.Bot.DeclineReforge` (T-BOT-20) and `Dusktracer.Reforging.StrikeMarks` (T-WPN-20).
- Moved: `Dusktracer.Telemetry.Records.Weapons`. The weapon record's two new fields are expected beside the old ones, and one of the two Lance hits is marked: old `"hits":2,"damage":2}`, new `"hits":2,"damage":2,"strike_hits":1,"strike_damage":1}`. The idle and reset lines carry `0, 0`.
- `C.GATE.TEST_FLOOR`: 485 to 487 on this branch. The merge with D-073 (485 to 486) makes the merged count 488, with a number-log row for each.
- On `34c6ae5`: `Dusktracer.Reforging` 31 of 31, `Dusktracer.Bot` 26 of 26, `Dusktracer.Telemetry` 28 of 28.
- Tools tests: 231 of 231 before the merge.

## The gate
- `& Tools\Gate.ps1 -Base dev/s2` on the merged tree: `GATE: PASS (f5e3769; 3 not run; mask FFFFF3FC)`. Tests 488 run of 488 found, floor 488. Not run: pad (no package), scope (no brief in flight), mac (deferred, D-007).
- The suites on the merged build, before the gate: Reforging 32 of 32, Bot 26 of 26, Telemetry 28 of 28.
- The five seed 9107 arms on `f5e3769` (rule 24): None `00e0d4ec`, Random `51369747`, Steered:Forge:Hammer `e3293942`, Steered:Storm:Tempest `b046559b`, Steered:Tide:Frost `def51848`. All five equal the expected values.

## Not shown
- No card is judged by this job. The sixteen cards are read at the exit battery on the fixed field and against one body (D-085). D-082 says seventeen; `reforging.csv` holds six transformations and ten evolutions, and the record is being corrected.
- How a person values a card; whether 1.3 to 1.8 is the right band for this game.
- The play floors (item 4) for any card: they are the exit battery's carry batteries.
