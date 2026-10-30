# S1 boss fights after field status (2026-10-30)

Goal: find why the field status of R-KEE-22 cost wins in the six arms without Tide (`S1_pull_rule.md` §4), and why T-OFF-15 read 199/201. Class: BUG, in the measuring instrument (Bot v1), not in the game. Layer: bot heuristics (`DTBotTuning`, which are tool parameters and never game numbers). Control: `pull_t2`, 15 seeds, 19 arms. Bot skill 1.0, headless; a bound, not a human reading.

## 1. Diagnosis (written before any behaviour change)
**The bot cannot see its way back into the phase III hazard ring.** A hero more than about 360 uu outside the ring stands in the hazard and dies there at full speed, with the ring plainly passable.

Read from telemetry and code:
- `S1_pull_rule.md` §4's "Hazard" column is `hit kind Hazard`, which `FDTBossCore::AddHeroHit` stamps on every direct boss attack. Split by `attack`, the 83 `pull_t2` fights took 176 hits from the ring and 7 from Sweep and DashCharge, so the phrase was right, but only by the attack field.
- The deaths are not a slow fight. Of 43 deaths in `pull_t2`, 38 are mostly ring damage, and the run ends at `hp_left_frac` 0.17–0.29, seconds after phase III opens at 0.30. Median time from phase III to the end: 8.7 s in deaths, 26.5 s in wins. The ring deals 8 HP per 0.5 s, so 100 HP lasts 6 s, and the telemetry shows that: first hit 3.5–4 s into the phase (shift 1.5 s, wind-up 2.0 s), then an unbroken run of 7–13 hits to death.
- Code: `FDTBotBrain::InTelegraph` treats the ring as a yes/no footprint, `Dist >= Length - Margin`. `ScoreMove` samples it 0.3 s and 0.6 s ahead, 360 uu at hero speed 600. Past that reach every direction scores the same, and what is left is `KiteWeight` (away from the enemies, so from the boss: outward) and inertia. Nothing says "inward".
- Positions, a probe of hero position every 0.25 s from 895 s on (no game change; runs reproduce: 40 wins on, 57 off):

| Six arms without Tide, 15 seeds | Fights | Wins | Outside the ring when it goes live | Took 3+ ring hits | Of those, died |
|---|---|---|---|---|---|
| Field status on | 83 | 40 | 39 | 41 (38 outside at activation) | 39 |
| Field status off | 84 | 57 | 21 | 22 (18 outside at activation) | 22 |

A ring hit run is nearly always fatal, and it starts from standing outside. Heroes within the look-ahead do step in: Storm/ArcGrid:Forge seed 5 goes 3415 → 2710 uu and takes no hit. Seed 4 sits at 3179 → 3199 and takes 13.

**Why field status moves this.** Both trees stand near the 2800 uu ring: the median hero radius at the start of phase III is 3318 uu on, 2975 uu off, and the share of fights past 3160 uu (ring plus look-ahead) is 0.60 on, 0.42 off. Downpour:Forge, the arm that loses most, goes 0.29 → 0.73. Wins follow that line in both trees: past it 15/50 on and 19/35 off, inside it 25/33 and 38/49. So the status does not make the fight harder. It shifts where the bot stands by a few hundred uu, across a cliff the bot should not have. I did not trace which path carries that shift (the hero radius already differs at boss start on some arms, and an arm's 15 seeds cannot carry it); what matters is that it should stop mattering once the cliff is gone, which §2 tests.

**Boss HP calibration is not the cause.** `boss_calibrated` follows R-BOS-09 as written (damage over the window, DoT included). Deaths show no pattern against it: Downpour:Forge deaths range 9.6k–25.7k and wins 12.0k–26.4k. No code here is wrong and no constant is touched. Window DPS is noisy for one build (one arm reads 49 to 330), which is a separate reading (§5).

## 2. Hypothesis
Written before measuring. We believe giving the bot a graded distance outside the ring moves the six no-Tide arms' ring deaths (`pull_t2`: 39 fights with 3+ ring hits ending in death) to near zero, and removes the gap between field status on and off (40 vs 57 wins) because the gap is a position shift across a bot blind spot. Measured by the same six arms with the status on and off. Reverted if not met.
