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
