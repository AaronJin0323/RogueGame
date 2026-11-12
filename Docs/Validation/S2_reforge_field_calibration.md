# S2: the probe fixes the band's field by the rate play kills at (D-085)

**Build:** none. This is Tools and documents only, on `s2/reforge-field-calibration`, cut from `s2/reforge-band-equipment` f5e3769 because the stage existed only there, and merged with `dev/s2` 0d2f1ca once that branch landed. No new run was made. The scene folders and play runs read here were read in place, read-only, in two other worktrees, and nothing was written into them.

## What the stage does now
The reforge stage is two passes, and `S2Exit.ps1`'s band step calls both:
- `python -I Tools\probe.py reforge-series <dir> --builds <runs> [--path-builds <runs>]` writes the no-pick scenes of every level 16 snapshot into `<dir>\series`, on three sets of health multiples:
  - the series 5, 10, 20 and 40;
  - one halving (2.5) and one doubling (80);
  - the far candidates 160, 320 and 640.
- `python -I Tools\probe.py reforge <dir> --builds ... --calibrated` reads that series and fixes the health. It then writes the fixed stage (every card and no pick, `RF-`) and the far stage (`RD-`). It writes no scene when no health is found.
- `python -I Tools\probe.py read <dir>` prints everything.
  - `--series` and `--far` name stage folders read one at a time, such as the stages of the check below.
  - `--out` sends `probe.md` and `kills.csv` elsewhere, so a folder that is only read is left as it was.
- One stage at a health given by hand (`reforge --hp-mult`) still works as D-082 had it.

**What the reader prints:**
- **`PLAY:`, the play rate.**
  - Its source is the run folders of the snapshots read (`builds.csv`). In each run's own file, the hand's time is the Reforging hand's `offer` record. The minute is player 0's `sources` interval that holds that time. Its kills are scaled to the probe's window.
  - The `offer` record is used for every run, whether it holds, declines or picks: every run that reached the hand has one. A run without it is left out and counted.
  - The line gives the median over the runs and the range.
- **`SERIES:`:** each stage's no-pick median and highest kills in the window.
- **`HEALTH:`:** where the no-pick median meets the play rate.
  - It is interpolated between the two neighbouring stages that bracket the rate, linearly on the logarithms of health and kills, and rounded to a whole multiple.
  - If the series does not bracket the rate, it is extended by the one halving or the one doubling, and the line says so. It is never extrapolated, and a stage that kills nothing has no logarithm to bracket with.
  - If the play rate is over half the standing field (`FIELD` 120, so 60), the target becomes 60, the health is rounded up, and the line says so.
  - The same procedure is run for each battery (each play run folder) on its own builds. Those results are printed on one line and never used. `HEALTH: NOT FIRM` names any battery whose own health is more than a factor of two from the pooled one.
- **`FAR:`:** the first of 160, 320 and 640 at which no no-pick scene kills anything, "bodies that do not die". It is printed and never judged.
- **The fixed stage's table, per card:**
  - the lift by damage (the judged figure, applied damage), with its range and builds;
  - the lift by kills beside it, where the no-pick median is twenty kills or more;
  - the no-pick median kills;
  - how many of the card's scenes killed past the 120 that stand;
  - "at least" where any did;
  - the same two lifts over the builds whose no-pick scene stays inside the field (ruling 12);
  - the crowd's side only: `in band`, `under`, `over`, `split` or `NOT READ` (fewer than ten builds);
  - each evolution beside both Keepers' transformations on the same builds, as before.
- **`split`** is said and not settled, with the rule that marked it in brackets:
  - damage and kills on different sides of a bound, over all builds or over the inside builds (item 10, read on both sets);
  - or the two medians by damage more than a tenth apart (ruling 12).
- **`FIELD:`:** how many no-pick scenes killed past the 120 that stand, how many stayed inside, and their median against the room-above bound. `FIELD: CROWDED` is printed when more than half are past; the stage is still read, and no verdict word moves by it.
- **Two `REFORGE:` lines:** the counts, including the cards' scenes over the 120; and that no card is said to be in band here, because by D-085 item 9 that needs the boss's side, which comes from play.
- **The far stage's table:** the same columns, with no side.
- The flag of 160 and the word SATURATED are gone. `kills.csv` marks each fixed scene over the field.

## The check against the reading that exists
The `dev/s2` developer's stages, `E:\Dusktracer\.claude\worktrees\agent-a37e1827c3d77226d\Saved\Probes\`, are each 45 level 16 snapshots of three hold batteries:
- `eq_rf5`: 5x, no-pick scenes only;
- `eq_rf10`, `eq_rf20`, `eq_rf` (40x), `eq_rf80`, `eq_rf160`: every card;
- `eq_rf16`: the fixed stage, every card.

The play runs are `d079_base_tempest_hold` and `d072_hold` (the `dev/s3` developer's tree) and `d074_hold` (the `dev/s2` developer's).

```
python -I Tools\probe.py read <Probes>\eq_rf16 --series <Probes>\eq_rf5,<Probes>\eq_rf10,<Probes>\eq_rf20,<Probes>\eq_rf,<Probes>\eq_rf80,<Probes>\eq_rf160 --far <Probes>\eq_rf160 --out <scratch>
```

```
PLAY: the play rate 43.2 kills a window (10 s): the median over 45 run(s), 41.7 to 45.2, of the kills in the minute that holds the Reforging hand (its offer record, each run's own file), scaled to the window
SERIES: no pick, kills in the window, median (highest) over each stage's scenes: 5x 159 (476); 10x 98 (294); 20x 27 (229); 40x 1 (159); 80x 0 (55); 160x 0 (0)
HEALTH: 16x fixed (15.54 interpolated) where the no-pick median meets 43.17, between 10x (98) and 20x (27), on the logarithms of health and kills
HEALTH: each battery alone, printed and never used: d072_hold 19x (19.18; play 43.7); d079_base_tempest_hold 14x (13.52; play 43.2); d074_hold 15x (15.27; play 43.2)
FAR: bodies that do not die at 160x (no pick kills nothing in 45 scene(s)); printed and never judged
| Card | Builds | Lift by damage (median) | Lowest | Highest | Lift by kills (median) | No-pick kills (median) | Scenes past the 120 that stand | Builds with the no-pick scene inside the field | Lift by damage, inside | Lift by kills, inside | Crowd's side | ...
| Transform.Storm | 45 | at least 1.40 | 0.97 | 2.50 | at least 1.79 | 38 | 19 of 45 | 35 | at least 1.44 | at least 2.23 | split (damage and kills, inside builds) | - | - | - | - |
| Transform.Tide | 45 | at least 1.16 | 0.82 | 3.39 | at least 1.25 | 38 | 9 of 45 | 35 | at least 1.21 | at least 1.42 | split (damage and kills, inside builds) | - | - | - | - |
| Evolve.OrbitalDrones | 15 | at least 1.37 | 0.98 | 3.41 | at least 1.80 | 29 | 4 of 15 | 11 | at least 1.69 | at least 2.24 | split (damage and kills, inside builds; all and inside builds by damage 0.33 apart) | Transform.Storm (15 builds) | 1.17 | Transform.Tide (15 builds) | 1.24 |
| Evolve.RingEmitter | 15 | at least 2.20 | 1.18 | 6.27 | at least 3.46 | 36 | 13 of 15 | 13 | at least 2.26 | at least 3.72 | over | Transform.Storm (15 builds) | 1.31 | Transform.Tide (15 builds) | 1.17 |
| Evolve.SeekerSwarm | 15 | at least 2.21 | 1.31 | 7.56 | at least 2.70 | 66 | 14 of 15 | 11 | at least 2.21 | at least 3.02 | over | Transform.Storm (15 builds) | 1.52 | Transform.Tide (15 builds) | 1.12 |
FIELD: 10 of 45 no-pick scene(s) at 16x killed past the 120 that stand (highest 249), 35 inside it (ruling 12: each card is read on all its builds and on those inside); the room-above rule reads their median, 38, against 60
REFORGE: the crowd's side at 16x: 5 card(s) read, 0 in band, 0 under, 2 over, 3 split; 0 not read (fewer than 10 builds); 59 card scene(s) over the 120 that stand ("at least"); printed, judged nowhere
REFORGE: no card is said to be in band here: by D-085 item 9 a card is in band when it is at least C.REFORGE.POWER_MIN on one side and at most C.REFORGE.POWER_MAX on both, and the boss's side comes from play
| Transform.Storm | 45 | 1.32 | 1.00 | 2.44 | - | 0 | 0 of 45 | 45 | 1.32 | - | - | - | - | - |
| Transform.Tide | 45 | 1.33 | 0.92 | 3.46 | - | 0 | 0 of 45 | 45 | 1.33 | - | - | - | - | - |
| Evolve.OrbitalDrones | 15 | 1.40 | 1.05 | 3.34 | - | 0 | 0 of 15 | 15 | 1.40 | - | Transform.Storm (15 builds) | 1.17 | Transform.Tide (15 builds) | 1.29 |
| Evolve.RingEmitter | 15 | 3.28 | 1.88 | 12.32 | - | 0 | 0 of 15 | 15 | 3.28 | - | Transform.Storm (15 builds) | 1.27 | Transform.Tide (15 builds) | 1.20 |
| Evolve.SeekerSwarm | 15 | 5.05 | 1.94 | 19.08 | - | 0 | 0 of 15 | 15 | 5.05 | - | Transform.Storm (15 builds) | 1.46 | Transform.Tide (15 builds) | 1.36 |
FAR: bodies that do not die at 160x: 5 card(s) printed, judged nowhere
```

Against the hand reading the scheduler sent:

| Figure | Hand reading | This reader |
|---|---|---|
| No-pick median kills, 5x, 10x, 20x, 40x | 159, 98, 27, 1 | 159, 98, 27, 1 |
| Play rate, each battery | 43.2, 43.7, 43.2 | 43.2, 43.7, 43.2; pooled 43.2 (43.17) over the 45 runs |
| Fixed health | 15.57, rounded to 16 | 15.54, rounded to 16 |
| Each battery alone | 13.52, 19.18, 15.27 | 13.52, 19.18, 15.27; none NOT FIRM |
| No-pick scenes past 120, highest | 10 of 45, 249; 35 inside | 10 of 45, 249; 35 inside |
| Storm: damage (kills), all then inside | 1.40 (1.79), 1.44 (2.23); 35 builds; 19 scenes past | the same |
| Tide | 1.16 (1.25), 1.21 (1.42); 9 past | the same |
| Blade Halo | 1.37 (1.80), 1.69 (2.24); 11 builds; 0.33 apart, marked; 4 past | the same, marked |
| Pulse Wall | 2.20 (3.46), 2.26 (3.72); 13 builds; 13 past | the same |
| Murmuration | 2.21 (2.70), 2.21 (3.02); 11 builds; 14 past | the same |
| Split over all builds; over the inside builds | none; three | none; three (Storm, Tide, the halo) |
| Far stage | 160x, median and highest 0 | 160x, 45 scenes with no kill |

- **Every lift equals the hand reading to the hundredth.** So does every lowest and highest of the old reader's `probe.md` on `eq_rf16`, by damage.
- **The one figure that differs is the fixed health before rounding: 15.54 here, 15.57 by hand.**
  - D-085 item 1 takes the play rate as the median over the runs. Over the 45 runs that is 259 kills, 43.17 a window, and it gives 15.54.
  - 15.57 is what a rate of 43.0 gives.
  - Both round to 16. The per-battery figures, each from its own runs' median, agree to the hundredth.
- **The widest spread between batteries** is a factor of 1.42 (19.18 over 13.52). The reader compares each battery with the pooled health, as D-085 item 4 asks; both readings say firm.
- **Each verdict word follows from the lifts:**
  - Storm and Tide are split on the inside builds, by damage against kills.
  - The halo is split on the inside builds, and by its two damage medians.
  - The wall and Murmuration are over on both sets and by both measures.
  - So the counts are 2 over and 3 split.
- **The far stage** equals the plan note's 160x row. The same reader on `eq_rf20` gives the plan note's 20x row and its lifts by kills (1.51, 1.25, 1.86, 4.32, 3.40).

## Tests
These are in `Tools/tools_test.py`, class `ReforgeStage`, on made-up scene folders and play runs:
- **The play rate from run files:** the minute that holds the hand's offer, scaled to the window; a run without the hand left out and counted.
- **The interpolation:**
  - inside the 10x to 20x bracket (15.53);
  - the series extended by one halving, the halving not read, and no bracket after the halving;
  - the doubling;
  - a stage that kills nothing.
- **Room above:** a play rate of 80 sets the target to 60 and rounds the health up.
- **The pooled health against each battery's, and NOT FIRM:** one battery at 16x and one at 65x give a pooled 49x through the doubling. The 16x battery is then NOT FIRM, and the pooled health is still the one used.
- **"At least" and `split`:**
  - a scene of 121 kills is "at least", one of 120 is not;
  - damage in band with kills over is split on both sets;
  - under twenty no-pick kills the lift by kills is not printed and splits nothing;
  - the far stage's table has no side.
- **The inside builds (ruling 12):**
  - a build past the field is left out of the second set;
  - fewer than ten inside builds mark nothing;
  - two damage medians 0.35 apart are marked;
  - a card split on the inside builds only is marked as such.
- **A crowded field** is said, and the stage is still read.
- **The far stage's stop:** 160x still kills, so 320x is the far stage; with no far stage read, it is not found.
- **Fewer than ten builds** is `NOT READ`.
- **The two passes write their scenes:** the series on nine multiples, no pick only. The calibrated pass writes nothing without a series.
- **S2Exit's band step** calls both passes and selects the new lines.

`TOOLS: PASS (239 tests)`.

## Not verified
- **The two passes end to end through `S2Exit.ps1`:** they were not run, because no engine scene was launched for this note. The check read stages that had been written one at a time by hand.
- **The 2.5x, 320x and 640x stages:** no real scene exists at any of them, so the halving, the second doubling of the far stage and the room-above case are shown by the tests only.
- **The boss's side** of any card. By D-085 item 9 no card is in band or out of it as a whole from this stage.
- A bot reading is never a human reading.
