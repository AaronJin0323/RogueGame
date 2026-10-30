# S1 Bot v1 baseline (2026-10-30)

First measurement with Bot v1 (`Source/Dusktracer/Bot/`) on `main` at 965d62d plus the bot (Keepers package merged: Patron, starting Pulse Rifle, flag sources, Intercession). Numbers only; nothing was tuned. Bot skill 1.0 (no reaction delay or noise), headless, 600 fixed steps per frame, sim cap 1100 s.

## Reproduce
```
& Tools\Build.bat
& Tools\BotRuns.ps1 -Check T-DIR-01 -Out Saved\BotRuns\S1_nopick
& Tools\BotRuns.ps1 -Arms Steered:Storm:Tempest,Steered:Forge:Hammer,Steered:Tide:Frost -Seeds 5 -Check T-OFF-15 -Out Saved\BotRuns\S1_steered
```
Seeds 1..10 (no-pick) and 1..5 (each Steered arm) through `-DTSeed`. Each run folder holds the bot record (`bot.csv`, `bot.csv.txt`), the telemetry pair and the log; `summary.csv` / `summary.md` per batch (`Tools/bot_summary.py`). The same seed gives the same input hash and telemetry (checked: four repeat runs of seed 1, and `Dusktracer.Bot.Determinism`).

Second Keepers (fixed, `FDTBotConfig::DefaultSecond`): Storm → Tide, Forge → Storm, Tide → Forge.

## Results vs bands

| Measure | Band | None (10) | Storm/Tempest (5) | Forge/Hammer (5) | Tide/Frost (5) |
|---|---|---|---|---|---|
| Outcome | — | 10 deaths | 5 deaths | 5 deaths (1 in the boss fight) | 5 deaths |
| Median death s | no-pick: `C.DIR.NOPICK_DEATH_BAND_S` 480–720 | **452.6** (215–751) | 697.3 (591–807) | 846.5 (756–989) | 512.0 (432–755) |
| Level at 630 s (mean) | L16 ref at 622 s (`xp_curve.csv`) | 3.1 | 4.4 | 4.6 | 3.2 |
| Decisions (mean) | `C.DECISIONS.PER_RUN` 28 ± 3 | 0 (declined) | 4.2 | 4.2 | 2.2 |
| Damage taken, share of max HP / min, 600–780 s | `C.DMG_TAKEN.BAND_MIN10_13` 0.30–0.50 | 0.217 (n=3 alive in window) | 0.275 (n=4) | 0.185 (n=5) | 0.043 (n=1) |
| Dashes / min, 600–780 s | `C.DASHES.BAND_MIN10_13` 6–10 | 7.15 | 8.06 | 6.38 | 6.95 |
| Keeper damage share (whole run, attribution) | `C.POWER.KEEPER_SHARE_L16` ≥ 0.60 at L16 | 0.18 | 0.39 | 0.66 | 0.01 |
| Reserve share of offered cards | `C.OFFER.RESERVE_SHARE_MAX` ≤ 0.10 | 0.0 | 0.0 | 0.0 | 0.0 |
| Gem pickup ratio (collected / spawned) | reference 0.51 (`C.XP.REFERENCE_PICKUP_RATIO`) | 0.17 | 0.15 | 0.12 | 0.15 |

Window rates average only the time the hero was alive inside 600–780 s; runs that died before 600 s have none (n above).

Checks: `T-DIR-01: FAIL (median end 452.6 s over 10 seeds, band 480-720 s)`; `T-OFF-15: FAIL (0/1 runs at the boss with decisions 28+-3 …; mean decisions 6)`. Keeper share at L16: **not measurable** — no run reached L16 (highest level 7), so `Tools/power_split.py` reports `POWER: n/a`; the column above is the whole-run attribution share at the level reached.

## Gaps (reported, not tuned)
1. **XP income is far below the curve.** Steered bots hold about 290 XP at 630 s (L4–5) against 2170 for L16 at the reference 622 s. At the bot's pickup ratio (~0.15) the director spawned roughly 1900 XP by then, so even full pickup lands near L15 and the reference ratio (0.51) near L7–8. Everything downstream inherits this: few decisions (T-OFF-15), no L8 second Keeper in most runs, no L16 power check, weak builds. Owner: `progression.md` Open (re-measure the curve against the S1 timeline, `C.XP.GROWTH` first) and the director's drop values.
2. **No-pick death 452.6 s, below the band.** Spread 215–751 s. The reading is bot-sensitive: with a weaker gem pull an earlier bot build read a median of 552 s on seeds 1–4 (452 s now), so calibrate the bot against human no-pick sessions before tuning the timeline.
3. **Steered runs die before the boss** (14 of 15; one reached it and died at 89 s of the fight to the shrinking hazard ring and the leftover field). With L4–6 builds this is the XP gap, not yet a pressure reading.
4. **Pressure band:** damage taken in the window is below 0.30–0.50 for every arm on average, with large spread (0.03–0.58 per run): the bot dies to bursts (the one traced death was pinned in an arena corner by swarm and rushers) rather than steady chip damage. Dashes per minute sit inside 6–10 for every arm.
5. **Tide/Frost** never saw a Frost blessing before dying in four of five runs (Keeper share 0.01); Forge/Hammer took its Key, two Amps and the Trait and reached 0.66 Keeper share at L4–7; Storm/Tempest 0.39.
6. **Telemetry hero columns are zero on main:** `damage_taken_per_min` and `dashes_per_min` in the session CSV stay 0 and no `dash` / `hero_damaged` records are written, because hero events do not reach telemetry yet. The bot counts them from `UDTHeroSubsystem::OnHeroEvent` in its own record; T-DIR-02 (`Tools/pressure.py`) needs the producers first.
7. `run_end.gems_left` reads 0 in runs where gems are visibly left on the field (gems never expire, R-PRG-07); check the telemetry counter.

## Bot v1 limits (read the numbers with these)
- Kites from the centroid of enemies within 1300 uu, dodges telegraph footprints (Band, AimLine, Ring, Arc, RingGap/SpiralLane lanes, HazardRing) and enemy rounds and bodies by closest approach over 0.6 s, dashes when a hit is under 0.22 s away, casts Intercession at a full meter with 12 enemies within 700 uu. Its heuristics are tool parameters in `DTBotBrain.cpp` (`DTBotTuning`), not game numbers.
- Pickup ratio 0.12–0.17 is well below the reference human 0.51; the bot is a weaker collector than a player, which lowers levels further.
- No rerolls, banishes or locks; Steered card order: archetype blessings (Key first) > new weapon (fit to the Path +10) > upgrade > second-Keeper blessing > other blessing > modifier > Reserve.

## Re-measure after S1 balance pass 1 (2026-10-30)
Same commands and seeds, on the pass-1 branch: hero telemetry bound, the gem-wipe bug fixed (every director drop used to empty the floor, `S1_balance_pass1.md` §1), `Tools/pressure.py` reading the window from telemetry. The drop-value TUNE of that pass was not met and was reverted, so every number is the game's numbers as before; this table is the gem fix alone. `-Out` may now be relative (`BotRuns.ps1` resolves it).

| Measure | Band | None (10) | Storm/Tempest (5) | Forge/Hammer (5) | Tide/Frost (5) |
|---|---|---|---|---|---|
| Outcome | — | 9 deaths, 1 alive at 1100 s | 5 deaths | **4 boss wins**, 1 death in the boss fight | 5 deaths (2 in the boss fight) |
| Median death s | no-pick: 480–720 | **592.9** (417–743) | 551.7 (272–664) | 955.3 (940–977) | 478.2 (390–984) |
| Level at 630 s (mean) | L16 ref at 622 s | 9.5 | 14.2 | 17.2 | 11.0 |
| t L8 / t L16 s (runs reaching) | `C.LEVEL.L8_MIN` 300, `C.LEVEL.L16_MIN` 630 | 378 (10) / — (0) | 239 (5) / 570 (2) | 249 (5) / 570 (5) | 354 (5) / 681 (2) |
| Decisions (mean) | 28 ± 3 | 0 (declined) | 16.4 | 29.6 | 16.6 |
| Damage taken, share / min, 600–780 s (telemetry) | 0.30–0.50 | 0.223 (n=4) | 1.526 (n=2) | 0.011 (n=5) | 0.052 (n=2) |
| Dashes / min, 600–780 s (telemetry) | 6–10 | 10.25 | 13.70 | 5.67 | 6.67 |
| Keeper damage share (whole run) / POWER at L16 | ≥ 0.60 at L16 | 0.17 / n/a | 0.38 / 0 of 2 pass | 0.64 / 3 of 5 pass | 0.22 / 0 of 2 pass |
| Reserve share | ≤ 0.10 | 0.0 | 0.0 | 0.0 | 0.0 |
| XP collected / full-pickup model (to 630 s) | reference 0.51 | 0.35 | 0.74 | 0.73 | 0.48 |
| Gems collected / dropped (count, old column) | — | 0.88 | 0.96 | 0.96 | 0.92 |

Checks: `T-DIR-01: PASS (median end 592.9 s over 10 seeds, band 480-720 s; 1 run(s) did not die)`; `T-OFF-15: PASS (7/7 runs at the boss with decisions 28+-3 and Reserve share <= 0.1; mean decisions 29)`; `Tools/pressure.py --check` (T-DIR-02 is human-measured; bot readings are bounds): Steered batteries `PASS (mean over 9 reading(s): damage taken 0.357/min, dashes 7.67/min)` only because the pooled mean averages Forge/Hammer's 0.01 with Storm/Tempest's 0.8–2.2 — no arm is inside the band on its own; no-pick `FAIL (0.223/min, 10.25 dashes/min)`.

Status of the gaps above: 1 (XP income) resolved by the bug fix — Steered bots now level ahead of the anchors (they collect ~0.73 of the model against the reference 0.51); 2 (no-pick death) now inside the band; 3 (Steered deaths before the boss) 7 of 15 runs reach the boss, Storm/Tempest none; 6 and 7 (hero telemetry, `gems_left`) fixed. Still open: 4 (pressure) and 5 (Tide/Frost weak), and the new readings below.

Remaining gaps (reported, not tuned in this pass):
1. **Pressure split by build.** Forge/Hammer takes almost no damage in 600–780 s (0.01/min, dashes 5.7/min, below both bands); Storm/Tempest takes 0.8–2.2/min and dies (dashes 13.7, above). No arm sits in 0.30–0.50 — this is now a build-power and threat reading, measurable for T-DIR-02.
2. **Bot pickup above the reference.** Collected / model 0.73 (Storm, Forge) vs `C.XP.REFERENCE_PICKUP_RATIO` 0.51: bot level times are early bounds (L16 ≈ 570 s). At the reference ratio the S1 timeline model itself reaches L16 only at 712 s; which side moves (drops, or the reference ratio) needs a human pickup reading (O-008).
3. **Share caps cut the XP the timeline asks for.** Spawned XP is ~0.79 of the model for a build that clears the field, because the alive share caps redraw most Turret and many Tank and Rusher spawns as Swarm.
4. **Storm/Tempest dies before the boss in 5/5** at L14–18; Tide/Frost reaches the boss 2/5 with a Keeper share under 0.60.

## Re-measure after S1 balance pass 2 (2026-10-30)
Part A (Keeper parameters, Reserve effects) and the arc contact-point fix are in; every tuned number of that pass was reverted (`S1_balance_pass2.md` §6). `-Parallel 14` reads identically to serial runs. All nine archetypes, seeds 1..5; `Steered:<Path>:<Archetype>:<SecondPath>` picks the second Keeper.

| Arm | Boss | Wins | Median end s | Dmg share/min | Keeper share L16 |
|---|---|---|---|---|---|
| None (10) | 1/10 | 0 | 592.9 | 0.223 | — |
| Storm/Tempest | 1/5 | 0 | 749 | 0.22 | 0.43 |
| Storm/ArcGrid | 0/5 | 0 | 481 | 0.17 | 0.46 |
| Storm/Downpour | 1/5 | 1 | 398 | 0.11 | 0.44 |
| Forge/Hammer | 5/5 | 4 | 956 | 0.02 | 0.59 |
| Forge/Furnace | 5/5 | 3 | 995 | 0.02 | 0.41 |
| Forge/Slam | 5/5 | 0 | 949 | 0.04 | 0.44 |
| Tide/Frost | 2/5 | 0 | 847 | 0.12 | 0.26 |
| Tide/Whirlpool | 0/5 | 0 | 89 | — | — |
| Tide/Stillness | 0/5 | 0 | 724 | 0.34 | 0.32 |
| Storm/Tempest:Forge | 5/5 | 4 | 993 | 0.01 | 0.36 |
| Storm/ArcGrid:Forge | 5/5 | 3 | 970 | 0.02 | 0.59 |

Checks: `T-DIR-01: PASS (median end 592.9 s over 10 seeds; 1 run(s) did not die)`; `T-OFF-15: PASS (29/29 runs at the boss, mean decisions 28.7)`; `Tools/pressure.py --check` now judges per arm: FAIL, 0/10 arms inside both bands. Gap 1 above reads differently now: the bot's fixed second Keeper decides the arm. Tide as a Keeper is what loses (0–2 of 15 at the boss), not Storm (`S1_balance_pass2.md` §6).

## Bot changes after D-016 (2026-10-30)
Two instrument fixes, no game change (`S1_boss_after_fields.md`): the hazard ring scores by the distance outside it, so the bot walks in from past its look-ahead; the wall band no longer closes the pickup reward for a chest. Win counts in every earlier section carry the ring blind spot and read low.

Later bot changes: `S1_boss_after_fields.md` (ring grade, chests at walls); `S1_probes.md` §5 (ring grade `RingGradeUU`), §1 (probe route, off in runs).

Bot v2 (`02_Systems/bot.md`, `S2_bot_v2.md`) sits beside this bot. Every command here stays v1: `-BotVersion` defaults to 1 and v1's input stream is unchanged (R-BOT-01).
