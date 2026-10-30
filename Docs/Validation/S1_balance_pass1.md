# S1 balance pass 1 — XP economy (2026-10-30)

Goal: runs reach L16 and the boss often enough that the S1 bands are measurable. Order: instrument → scored test → diagnosis (this section, written before any number changes) → TUNE → re-measure (`H06 §3`, `H04 §3`). Bot v1 at skill 1.0, headless, same seeds as `S1_bot_baseline.md`.

## 1. Instrument and scored test (done before the diagnosis)
- **BUG, telemetry:** hero events never reached telemetry. `UDTTelemetrySubsystem` now binds `UDTHeroSubsystem::OnHeroEvent` (`dash`, `hero_damaged`, `hero_down`, `intercession_denied`) and reads `input_device` each step (`Dusktracer.Telemetry.HeroRecords`).
- **BUG, progression (found while fixing `gems_left`):** `UDTGemSubsystem::Spawn` and `SpawnChest` ran the run-restart check with the current step index. During the Director phase that index equals the last Pickups step, so **every director drop emptied the floor, gems and chests alike** (R-PRG-07 says gems never expire). Only gems collected within the step of their drop counted; chests vanished at the next kill. `run_end.gems_left` read 0–1 because the floor really was empty. A drop now treats only a smaller index as a rewind (`Dusktracer.Progression.DropsSurvive`).
- **T-DIR-02 tool:** `Tools/pressure.py` reads the window from the JSONL hero records; `Tools/bot_summary.py` uses it and also reports the XP economy per run: level-up times, XP collected, XP spawned by role (`dir_block.spawned_by_role` × `C.DROP.*`), kills/min and the full-pickup timeline model.

The measure that matches `C.XP.REFERENCE_PICKUP_RATIO` is **XP collected / full-pickup model XP** (`balance/model.md` §2: measured pickup = 0.50 × the model that kills every spawn the timeline asks for). The baseline's "gem pickup ratio" (gems collected / gems dropped, by count) is a different quantity: its 0.12–0.17 was not comparable to 0.51.

## 2. Measurements

Full-pickup model of `timeline_arena1.csv` with today's drops (Swarm 1, Tank 4, Rusher 2, Turret 3, elite × 5): **1234 XP at 300 s, 3506 XP at 630 s, 6277 at 900 s**. At the reference ratio 0.51 this reaches **L8 at 324 s** (target 300 ± 30) and **L16 at 712 s** (target 630 ± 30, missed by 82 s); L20 at 900 s against the L23 the hand budget needs. The curve's own reference income (previous project, `xp_curve.csv` `ref_reached_s`) is about 1.2× the S1 timeline's at 630 s.

Bot runs, at 630 s or the run end (mean per arm):

| Arm | Build | Level @630 | t L8 s (runs) | t L16 s (runs) | XP collected | XP spawned | Model XP | Kills/min | Collected / spawned | Collected / model |
|---|---|---|---|---|---|---|---|---|---|---|
| Storm/Tempest | baseline (95c8d7d, gem wipe) | 4.4 | — | — (0) | ≈ 290 | ≈ 1900 | 3506 | — | ≈ 0.15 | ≈ 0.08 |
| None (10) | gem fix | 9.5 | 378 (10) | — (0) | 1012 | 1532 | 2918 | 115 | 0.67 | 0.35 |
| Storm/Tempest (5) | gem fix | 14.2 | 239 (5) | 570 (2) | 1989 | 1882 | 2684 | 177 | 1.05 | 0.74 |
| Forge/Hammer (5) | gem fix | 17.2 | 249 (5) | 570 (5) | 2556 | 2755 | 3506 | 206 | 0.93 | 0.73 |
| Tide/Frost (5) | gem fix | 11.0 | 354 (5) | 681 (2) | 1313 | 1652 | 2631 | 144 | 0.78 | 0.48 |

(Model XP is lower than 3506 where runs ended before 630 s. XP collected includes XP-gain multipliers and the elite bonus, which the spawned column leaves out, so collected/spawned can exceed 1.)

XP spawned by role up to 630 s (Forge/Hammer, all killed; counts × today's drop value): Swarm 1879 × 1, Tank 120 × 4, Rusher 171 × 2, Turret 18 × 3 = 2755. The timeline asks for about 3506 worth: the alive share caps (`C.DIR.SHARE_MAX_*`, 0.08–0.10 of the alive enemies) turn most Turret and many Tank and Rusher draws into Swarm when a build keeps the field small, so real spawned XP is about 0.79 of the model. Up to 300 s: Swarm 733, Tank 36, Rusher 33, Turret 2.

Other readings after the gem fix (no number changed): T-DIR-01 PASS (no-pick median 592.9 s, was 452.6); T-OFF-15 PASS (7/7 Steered runs at the boss, mean 29 decisions); Forge/Hammer wins 4/5; Storm/Tempest and Tide/Frost still die before the boss in 8 of 10 runs.

## 3. Diagnosis (which layer is wrong)
1. **Pickup (progression code) — the dominant fault, a BUG, fixed in §1.** Every director drop wiped the floor. Collected / model went from ≈ 0.08 to 0.48–0.74 for Steered bots with no number changed; level at 630 s rose from 3–5 to 11–17.
2. **Bot collection — not a bottleneck any more.** The bot collects 0.78–0.93 of the XP it causes to drop and 0.73–0.74 of the model for the arms that survive (0.48 for Tide/Frost, which kills less), above the reference 0.51 (a stationary sim hero; a moving player picks up more, `model.md` §2). No bot change in this pass: lowering a tool's skill to match a sim number would not make it read more like a human. The bot's level times are therefore early bounds, about 1.4× a reference-ratio player's income.
3. **Drop values (director data) — short by about 1.2× against R-PRG-03.** At the reference ratio the S1 timeline's model income reaches L8 at 324 s (inside tolerance) and L16 at 712 s (82 s late), and the level at boss time (L20) is short of the hand budget (L23). The curve is calibrated to the reference income (R-PRG-03/04, T-PRG-03); the S1 timeline replaced the income, not the curve, so the layer to tune is the drops, not `C.XP.GROWTH`.
4. **Spawn density — not changed.** Rates feed the pressure and overwhelm contracts (R-DIR-01/02; T-DIR-01 now passes). Share caps lower real income below the model (item 3 measurement); they are a pressure lever, recorded as a gap, not tuned for XP.

## 4. Changes (TUNE, GATED only; one `number_change_log.csv` row each)
Swarm stays 1 (it is ~70 % of the income: Swarm 2 would put L8 at 219 s); the threat roles carry the difference, which also rewards killing them.

| Constant | From | To | Hypothesis |
|---|---|---|---|
| `C.DROP.XP_TANK` | 4 | 5 | together with the two rows below, the model income at the reference ratio reaches L8 at 295 s (from 324), L16 at 635 s (from 712) and L22 at boss time (from L20; L23 at 924 s, within 30 s); measured by the Steered bot arms reaching L16 in more runs (2/5 → ≥ 3/5 for Storm/Tempest and Tide/Frost) without T-OFF-15 decisions leaving 28 ± 3; reverted if not met |
| `C.DROP.XP_RUSHER` | 2 | 3 | as above |
| `C.DROP.XP_TURRET` | 3 | 4 | as above |

Alternatives measured on the model and rejected: Tank 5, Rusher 3, Turret 3 (L16 648 s, L21 at boss time); Tank 6, Rusher 3, Turret 4 (L8 282 s, L16 616 s, both nearer the edge); Swarm 2 (L8 219 s, outside tolerance).

## 5. Results
Re-measure with the `S1_bot_baseline.md` commands and seeds, drops at Tank 5, Rusher 3, Turret 4 (commit 767e40d):

| Arm | Build | Level @630 | t L8 s | t L16 s (runs) | Reached boss | Decisions at the boss | Collected / model |
|---|---|---|---|---|---|---|---|
| None (10) | gem fix | 9.5 | 378 | — (0) | 1 alive at 1100 s | — | 0.35 |
| None (10) | + drops | 9.5 (deaths identical) | 372 | — (0) | 1 | — | 0.31 |
| Storm/Tempest (5) | gem fix | 14.2 | 239 | 570 (2) | 0 | — | 0.74 |
| Storm/Tempest (5) | + drops | 15.6 | 229 | 524 (3) | 1 | 32 | 0.68 |
| Forge/Hammer (5) | gem fix | 17.2 | 249 | 570 (5) | 5 (4 wins) | 28–30 | 0.73 |
| Forge/Hammer (5) | + drops | 18.4 | 234 | 520 (5) | 5 (4 wins) | 30–31 | 0.69 |
| Tide/Frost (5) | gem fix | 11.0 | 354 | 681 (2) | 2 | 27–28 | 0.48 |
| Tide/Frost (5) | + drops | 10.8 | 347 | 662 (2) | 1 (1 win) | 30 | 0.40 |

(Collected / model falls with the drops because the model rises by the full 1.2× while the spawned XP rises by only ~1.1×: few threat roles actually spawn under the share caps.)

Against the hypothesis: Storm/Tempest reached L16 in 3/5 (was 2/5), Tide/Frost stayed at 2/5, and T-OFF-15 failed (6/7 runs at the boss inside 28 ± 3, mean 30.6; Storm/Tempest seed 1 took 32 decisions). **Not met → reverted** (64b2387; `H04 §3` step 7, the log keeps both the attempt and the revert). The batteries after the revert equal the gem-fix batteries exactly (deterministic seeds).

What the pass leaves (numbers: `S1_bot_baseline.md`, re-measure section):
- **The XP economy works for the bot:** Steered bots reach L8 at 239–354 s and L16 at 570–681 s; 9 of 15 Steered runs reach L16, 7 reach the boss (Forge/Hammer 4 wins); T-DIR-01 and T-OFF-15 pass; Keeper share at L16 is measurable (3 of 9 L16 runs pass `C.POWER.KEEPER_SHARE_L16`).
- **Reference ratio vs bot:** R-PRG-03 still reads L16 at 712 s for a player collecting 0.51 of the model, while the bot collects ~0.73 and runs ahead of the anchors. The drop change helps the first and hurts the second; the next move needs a human pickup reading (O-008) to decide between the drops (re-apply this change) and `C.XP.REFERENCE_PICKUP_RATIO` (re-measure it on S1). No bot change: making the tool collect less to imitate a stationary sim would not make it more human.
- **Share caps** keep real income at ~0.79 of the timeline model for a build that clears the field (gap, director layer).
- **Pressure (T-DIR-02) is now measurable and split by build:** Forge/Hammer 0.01 damage share/min and 5.7 dashes/min in 600–780 s; Storm/Tempest 0.8–2.2/min and dies; no arm sits inside 0.30–0.50 on its own.
