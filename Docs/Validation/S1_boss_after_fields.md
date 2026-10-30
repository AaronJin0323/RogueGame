# S1 boss fights after field status (2026-10-30)

Goal: find why the field status of R-KEE-22 cost wins in the six arms without Tide (`S1_pull_rule.md` §4), and why T-OFF-15 read 199/201. Class: BUG, in the measuring instrument (Bot v1), not in the game.

**Verdict: field status never cost wins; a bot blind spot at the hazard ring did. With the bot fixed, status on reads 78 wins of 83 fights against 73 of 86 off. No game rule or number changed. T-OFF-15 still fails, 200/201, on a different run. T-DIR-01 passes. The fix also shows the Warden is a near-certain win for the bot (§5).** Layer: bot heuristics (`DTBotTuning`, which are tool parameters and never game numbers). Control: `pull_t2`, 15 seeds, 19 arms. Bot skill 1.0, headless; a bound, not a human reading.

## 1. Diagnosis (written before any behaviour change)
**The bot cannot see its way back into the phase III hazard ring.** A hero more than about 360 uu outside the ring stands in the hazard and dies there at full speed, with the ring plainly passable.

Read from telemetry and code:
- `S1_pull_rule.md` §4's "Hazard" column is `hit kind Hazard`, which `FDTBossCore::AddHeroHit` stamps on every direct boss attack. Split by `attack`, the 83 `pull_t2` fights took 354 hits from the ring and 19 from Sweep and DashCharge, so the phrase was right, but only by the attack field.
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

## 3. Fix and result, ring
`FDTBotBrain::DangerAt` grades the ring by the distance outside it. Test `Dusktracer.Bot.HazardRingInward`, seen failing first. The test needed a hero on the diagonal: at a wall the wall band already pushes inward. Nothing before the ring goes live changes, so these runs match `pull_t2` up to phase III.

| Six arms without Tide, 15 seeds | Fights | Wins | Took 3+ ring hits | Of those, died | Ring HP per 100 s | Boss rounds HP per 100 s |
|---|---|---|---|---|---|---|
| Before, status on | 83 | 40 | 41 | 39 | 44.7 | 7.9 |
| Before, status off | 84 | 57 | 22 | 22 | not read | not read |
| Ring fix, status on | 83 | **78** | 2 | 1 | 2.6 | 9.6 |
| Ring fix, status off | 86 | **73** | 6 | 4 | 4.6 | 13.8 |

**Hypothesis met, kept.** The gap is gone: status on is ahead by 5 wins, within what 15 seeds can tell. Past 3160 uu the win rate rises 15/50 → 47/50, level with inside it (31/33). Downpour:Forge reads 15/15 with the status, 13/14 without. The hero's wider stance is still there and now does no harm.

Ring deaths are not all gone. In the final 19-arm battery 4 of the 12 deaths in these six arms are still mostly ring damage (8–11 hits). Not traced: something scores higher than the ring and holds the bot outside.

## 4. T-OFF-15
Both low runs of `pull_t2` had the same shape: 4 chest hands of the 6 the director places (R-DIR-16), and 788 and 921 gems left on the floor. That is two hands short directly, and a chest's magnet (R-PRG-09) is worth levels, so more through level hands. Not a pacing fault: all six beats yielded a chest. A probe of chest positions shows why they stayed: Storm/ArcGrid:Forge seed 4 stood about 450 uu from the chest at (3654, -100) for 600 s. The director clamps a chest into the arena, and the bot's wall band scores over `GemSafeDanger` near the wall, which closes the pickup reward there. Of 201 runs at the boss, 32 missed at least one chest.

Fix: for a chest, and only a chest, the wall band's share leaves the move's danger. Test `Dusktracer.Bot.ChestAtWall`, seen failing first; a chest behind a telegraph still waits. Runs at the boss with all 6 chests: 169 → 198 of 201. Tide/Stillness:Storm seed 9 reads 29 and Storm/ArcGrid:Forge seed 4 reads 30, both wins.

**T-OFF-15 reads 200/201, mean 28.97, and still FAILS.** The miss is new: Tide/Stillness:Storm seed 6 at 24, a win at 930 s. It has all 6 chests, both milestones and 16 level hands, so the offer system delivered every hand its XP earned. The run is short of XP: L19, 2618 gems spawned against about 3700 for runs that read 28+, 860 kills by 540 s against about 1500 for seed 4 of the same arm, and 390 spawns refused at the cap. Almost all its damage is the Pulse Rifle. It died at 706 s in `pull_t2`, so the check never read it; it now lives to the boss. A slow build, the shape of Tide/Frost seed 11 in `S1_balance_pass2.md` §7, not a pacing fault.

**The test is not changed.** R-OFF-15 states the bound for a steered run to `C.RUN.LENGTH_TO_BOSS` with no exception for a slow build, and loosening a floor to admit one run is the wrong way round. The cause is kill throughput on the weakest archetype pair, a balance question outside this task.

## 5. What reads wrong
1. **The Warden barely kills the bot now.** In these six arms 74 of 86 fights are won, 9–15 wins per arm. Earlier win counts carried the blind spot, so win numbers in `S1_balance_pass1.md`, `S1_balance_pass2.md`, `S1_tide_expression.md` and `S1_pull_rule.md` read low, and win comparisons across arms there should not be relied on. Boss reach is not affected by the ring. `boss.md` Open already says the boss may kill too few; this is a skill 1.0 reading, a bound. Nothing was tuned.
2. **Window DPS varies a lot for one build:** 49 to 330 on Downpour:Forge, so effective HP varies with the opening 8 s. It follows R-BOS-09. No outcome pattern comes from it here.
3. **The chest fix changes runs before the boss,** so some arms move with no change to their game: None goes 1 → 0 at the boss, median unchanged, window damage 0.41 → 0.25; Tide/Stillness:Storm's median end 706 → 564 s; Tide/Whirlpool 5 → 4 at the boss, wins 2 → 0. These arms have 4–7 runs at the boss, too few to read.
4. `Tools/BotRuns.ps1 -Check T-DIR-01,T-OFF-15` from PowerShell joins the list into one string and silently runs no check, printing `PASS (no check requested)`. Read here with `Tools/bot_summary.py --check`. Left as is.
5. Hit kind `Hazard` covers Charge, Sweep and DashCharge as well as the ring, so a table labelled Hazard is not the ring.
6. What carries the status's position shift (§1) stays untraced. It no longer changes outcomes.

## 6. Before and after, 15 seeds
Reached boss / wins / median end s / damage share per min in 600–780 s / Keeper share at L16 (runs with a reading). Before = `pull_t2`. After = this tree, both bot fixes; `Saved/BotRuns/after_bot`.

| Arm | Before | After |
|---|---|---|
| None | 1 / 0 / 589 / 0.41 / 0.00 (0) | 0 / 0 / 589 / 0.25 / 0.00 (0) |
| Tide-Frost | 12 / 7 / 963 / 0.04 / 0.43 (13) | 11 / 9 / 956 / 0.11 / 0.39 (12) |
| Tide-Frost-Storm | 11 / 7 / 966 / 0.04 / 0.37 (13) | 11 / 10 / 954 / 0.03 / 0.37 (12) |
| Tide-Stillness | 6 / 4 / 742 / 0.14 / 0.27 (9) | 7 / 4 / 875 / 0.03 / 0.25 (10) |
| Tide-Stillness-Storm | 7 / 5 / 706 / 0.11 / 0.26 (7) | 6 / 6 / 564 / 0.02 / 0.23 (6) |
| Tide-Whirlpool | 5 / 2 / 527 / 0.03 / 0.31 (7) | 4 / 0 / 580 / 0.06 / 0.34 (7) |
| Tide-Whirlpool-Storm | 5 / 2 / 501 / 0.01 / 0.33 (5) | 4 / 2 / 556 / 0.04 / 0.27 (7) |
| Forge-Hammer-Tide | 12 / 4 / 961 / 0.07 / 0.59 (15) | 13 / 5 / 968 / 0.05 / 0.59 (15) |
| Forge-Furnace-Tide | 8 / 2 / 940 / 0.12 / 0.37 (14) | 8 / 3 / 920 / 0.13 / 0.37 (15) |
| Forge-Slam-Tide | 11 / 3 / 957 / 0.08 / 0.45 (15) | 10 / 6 / 945 / 0.09 / 0.44 (13) |
| Storm-Tempest | 13 / 7 / 979 / 0.06 / 0.41 (15) | 13 / 10 / 984 / 0.03 / 0.41 (14) |
| Storm-ArcGrid | 13 / 8 / 965 / 0.03 / 0.50 (13) | 13 / 10 / 974 / 0.03 / 0.51 (14) |
| Storm-Downpour | 15 / 9 / 970 / 0.03 / 0.59 (15) | 15 / 14 / 994 / 0.01 / 0.59 (15) |
| Forge-Hammer | 14 / 5 / 962 / 0.01 / 0.60 (15) | 14 / 13 / 976 / 0.02 / 0.59 (14) |
| Forge-Furnace | 11 / 5 / 956 / 0.08 / 0.45 (14) | 13 / 9 / 986 / 0.06 / 0.45 (14) |
| Forge-Slam | 14 / 7 / 973 / 0.03 / 0.47 (15) | 14 / 13 / 992 / 0.04 / 0.48 (15) |
| Storm-Tempest-Forge | 15 / 8 / 976 / 0.02 / 0.42 (15) | 15 / 12 / 988 / 0.02 / 0.42 (15) |
| Storm-ArcGrid-Forge | 14 / 8 / 980 / 0.03 / 0.60 (15) | 15 / 12 / 987 / 0.04 / 0.58 (15) |
| Storm-Downpour-Forge | 15 / 7 / 968 / 0.01 / 0.62 (15) | 15 / 15 / 972 / 0.01 / 0.61 (15) |

| Check | Before | After |
|---|---|---|
| T-DIR-01 (no-pick median death) | PASS 588.6 s | PASS 588.6 s |
| T-OFF-15 | FAIL 199/201, mean 28.66 | **FAIL 200/201**, mean 28.97 |
| `Tools/pressure.py --check` | FAIL 0/19 | FAIL 0/19 |

Steered arms in total: reached boss 201 → 201, wins 100 → 153.

All of this is the bot at skill 1.0, a bound; nothing here is a human reading (O-008).
