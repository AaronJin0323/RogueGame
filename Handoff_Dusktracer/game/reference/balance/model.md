# Enemy EHP, XP curve, swarm TTK and run pressure (v0.1 model)

Regenerate: `python -I Tools/Balance/balance_model.py`. Timeline: main @ 91b2a28 (full 20-min timeline + 3 events); the XP
numbers below moved by < 1 % with it (proposal unchanged). Method and assumptions: [`README.md`](README.md). The
empirical counterpart of every section is in [`sims.md`](sims.md).

## 1. Enemy EHP per role / affix over time

Scaling (`URHSpawnTimeline`): HP × (1 + 0.12·min), damage × (1 + 0.05·min); elites × 5 HP (`FRHEliteWave.HPMul`);
Armored takes × 0.6 (EHP × 1.67); the Bulwark plate takes × 0.5 within ±60° of its facing (pierce ignores it);
Shielded absorbs the first 12 bullets whole; Splitter adds 3 Scrapwings. Full grid (every minute, every variant):
[`enemy_ehp.csv`](enemy_ehp.csv). **Max contact DPS** = contact damage ÷ the hero's 0.4 s hurt i-frames
(`ARHHero::HurtInvulnTime`): contact intake is capped per hit window, not by how many enemies touch the hero.

| Role (key) | Base HP | HP @5 | HP @10 | HP @15 | HP @20 | Elite @5 / @10 / @15 | Contact @0 → @20 | Max contact DPS @10 |
|---|---|---|---|---|---|---|---|---|
| Scrapwing (`Swarm`) | 14 | 22.4 | 30.8 | 39.2 | 47.6 | — | 6 → 12 | 22.5 |
| Bulwark (`Tank`) | 90 (180 front) | 144 | 198 | 252 | 306 | 720 / 990 / 1 260 (×2 front) | 12 → 24 | 45 |
| Courser (`Rusher`) | 32 | 51.2 | 70.4 | 89.6 | 108.8 | 256 / 352 / 448 | 14 → 28 | 52.5 |
| Flare Pylon (`Turret`) | 40 | 64 | 88 | 112 | 136 | — / 440 / 560 | 6 (shot 8) → 12 (16) | 22.5 |
| Breaker (`Bruiser`) | 150 | 240 | 330 | 420 | 510 | — / 1 650 / 2 100 | 14 (slam 18) → 28 (36) | 52.5 |
| Tallyman (`Support`) | 30 | 48 | 66 | 84 | 102 | — / — / 420 | 4 → 8 | 15 |

Elite worst cases (the 15:00 wave rolls 2 affixes): Armored Breaker 3 500 EHP @15, Armored Bulwark from the front
4 200 EHP @15 (2 100 through the side or with pierce). With a steered 15:00 build (Rail Lance L7 + Scatter L3 +
Rifle L2, single-target ≈ 220 DPS incl. Overcharge) that is 8–17 s of focus — inside the wave's 45 s `MaxDuration`,
so elites stay skill checks, not walls. An **out-of-range elite Flare Pylon** holds the whole Threat phase open for the
full 45 s (seen in the sims, §4 of [`sims.md`](sims.md)).

Pressure numbers that matter for "dying should feel like being caught, not chipped" (§3.5): a Courser hit at minute
10 is 21 (21 % of base HP), a Breaker slam 27, a Volatile blast 15 (unscaled). The 0.4 s window means a hero standing
in a Scrapwing crowd at minute 10 loses at most 22.5 HP/s — dead in 4.4 s without regen. The idle mortal runs die at
16 s at minute 0 (6 dmg × one hit / 0.4 s ≈ 15 HP/s, minus the first second of approach).

## 2. XP curve vs the GDD targets

Current curve (`URHRunTuning`: `XPFirstLevelUp` 10, `XPGrowthFactor` 1.18, per-level rounding): 10, 12, 14, 17, 20, 24,
28, 33, 39, 46, 54, 64, 76, 89, 105, 124 … Cumulative to L8 = 125 XP, to L16 = 631 XP.

XP income (`timeline_blocks()`: spawn rate × mix-weighted XP per role, elite waves at base role XP, Breath pauses),
assuming every regular spawn is killed:

| Minute | XP (full pickup) | Level, current curve, full pickup | Level **measured** (mean of 5 built sims) | GDD target |
|---|---|---|---|---|
| 1 | 108 | 7.4 | 5.4 | 2.3 |
| 2 | 286 | 11.8 | 9.2 | 3.5 |
| 4 | 931 | 18.1 | 15.4 | 6.1 |
| 5 | 1 372 | 20.4 | 17.2 | 7.4 |
| 8 | 2 807 | 24.5 | 21.0 | 10.9 |
| 10 | 4 096 | 26.8 | 22.6 | 13.1 |
| 12 | 5 607 | 28.7 | 24.2 | 15.4 |
| 15 | 8 345 | 31.0 | 26.0 | 18.0 |
| 20 | 12 867 | 33.6 | 27.4 | 22 |

**L8 lands at ~1:25 and L16 at ~3:45** (GDD: 5:30 / 12:30); a run reaches L29–34 at 20:00 (GDD: 20–24 level-ups).
The invulnerable built sims agree (L8 by 2:00 in all 5, L16 by 4:00–6:00, L24–29 at 20:00; [`sims.md`](sims.md)). The GDD §7 line
"L2 = 10 XP, +18 % each → ≈ 22 levels at 20:00" assumed roughly a third of today's income; the spawn timeline's rates
(1.6 → 5.7 /s, needed for the kills/min targets) were raised since without the curve following.

Consequences: both commitments (second god L8, fusion L16) happen in the first 4 minutes, weapons max out by minute 2
(Rare/Epic upgrade cards add 2–3 levels), and the second half of the run is "nothing left to pick" — power jumps (§3.5
source d) stop exactly when the threat beats start.

**Fitted replacement** ([`xp_table_proposal.csv`](xp_table_proposal.csv)): calibrate income to the sims (measured
pickup = 0.50 × the full-pickup model for a stationary hero, median over minutes 3–20 of the 5 built runs), keep a
quick first pick, fit to the anchors:

`XPTable = [15, 42, 115]`, then `XPGrowthFactor = 1.11` → 15, 42, 115, 128, 142, 158, 175, 194, 215, 239, 265, 294,
326, 362, 402, 446, 495, 549, 609, 676 … → L2 0:19 · **L8 5:35** · **L16 12:33** · L21 18:40 (20 level-ups by 20:00).

A single formula also fits (`XPFirstLevelUp` 104, growth 1.14) but puts the first offer at ~50 s, which leaves the
first Harvest without a pick. Caveats: the calibration hero stands still (a moving player picks up more) but was fully
built by minute 2 (a slower curve kills less early, so less XP). The two effects pull in opposite directions; re-run
`RunSims.ps1` after the change lands and adjust only `XPGrowthFactor` (±0.01 moves L16 by ~40 s).

## 3. Swarm TTK for a steered build vs ≤ 0.3 s

Per-build, per-minute estimates are in [`ttk_pressure.csv`](ttk_pressure.csv) under two level curves (`current_75pct`
and `gdd_target`). Summary under the GDD curve (the one the contract was written for):

| Minute | Steered (Storm, Lance first) | Scattered (4 weapons, no Tier 2) | Scattered, no Lance | Unbuilt (Rifle L1) |
|---|---|---|---|---|
| 2 | 0.0 s · Rifle1 Lance1 | 0.0 s · Rifle1 Scatter1 | 0.0 s | 0.55 s |
| 5 | 0.0 s · Rifle2 Lance3 | 0.0 s · + Drones1 Lance1 | 0.9 s | 1.1 s |
| 8 | 0.0 s · + Scatter1 (Tier 2) | 0.0 s | 0.9 s | 1.1 s |
| 10 | 0.0 s · Lance5 | 0.0 s | 0.9 s | 1.65 s |
| 12 | 0.0 s · Lance6 | 0.0 s | 0.9 s | 1.65 s |
| 15 | 0.0 s · Lance7 Scatter3 | 0.0 s | 0.55 s | 1.65 s |
| 18 | 0.0 s | 0.0 s | 0.55 s | 2.2 s |

**Verdict.** The ≤ 0.3 s contract holds for any build that owns the Rail Lance (30 dmg L1 one-shots Scrapwings until
9:30; 57 dmg L8 until 25:00 — the lance alone carries the kill share) and fails for every build without it from
minute ~4: Pulse Rifle, Scatter Cannon and Orbital Drones never out-scale ×(1 + 0.12·min) HP (§ one-shot horizon in
[`weapons.md`](weapons.md)). The steered/scattered distinction the GDD draws ("Tier 2 by 8:00 keeps TTK ≤ 0.3 s, a
scattered build sees > 1 s by 12:00") is **not produced by the Path**: in the starter pool, Tier 2 changes per-hit
damage by ≤ 7.5 % (Storm Shock on half the swarm) and the only Damage-% blessing is Overcharge (+24 % at R3). What
decides TTK is "has a high-damage weapon (Lance, later Flak / Mine) at a decent level", not steering.

The sims add one effect the model does not have: **Nearest targeting in a pressing crowd re-targets every shot**, so
single-bolt weapons spread their hits over several Scrapwings. Measured unbuilt TTK is 1.65 s at minute 1 (model
0.55 s), climbing to 7–15 s by minute 5–10 (the cap is full and every bolt goes to a different, just-arrived
Scrapwing). Multi-bolt volleys (Rifle L3+) and the Lance's pierce hide it.

## 4. Unbuilt / scattered pressure vs "overrun by ~12:00"

Clear ratio (build crowd damage on distinct HP ÷ arriving HP/s; < 1 = accumulating) under the GDD level curve:

| Minute | Arrivals /s | Arriving HP/s | Steered | Scattered | Scattered, no Lance | Unbuilt |
|---|---|---|---|---|---|---|
| 2 | 2.9 | 100 | 2.49 | 0.53 | 0.53 | 0.19 |
| 5 (Threat) | 2.5 | 120 | 3.33 | 3.11 | 0.65 | 0.16 |
| 8 | 4.8 | 272 | 1.92 | 1.67 | 0.34 | 0.07 |
| 10 (Threat) | 3.0 | 256 | 4.26 | 2.01 | 0.49 | 0.07 |
| 12 | 5.3 | 504 | 2.43 | 1.17 | 0.30 | 0.04 |
| 15 (Threat) | 3.0 | 326 | 5.34 | 2.12 | 0.57 | 0.06 |
| 18 | 5.7 | 704 | 2.85 | 1.28 | 0.26 | 0.03 |

- **Unbuilt** is overrun at once: L1 Rifle kills 0.9 Scrapwings/s against 1.6 arriving at 0:00; the 60 cap fills by
  2:00 in the no-pick sim and stays full (120 from 3:00, 200 from 9:00, 300 from 15:00). This is much earlier than
  12:00 — correct for "no build", and the idle mortal runs die before any of it matters (16 s).
- **Scattered with the Lance** is *not* overrun by 12:00 (ratio 1.17, dipping toward 1 only in the Harvest blocks
  after 12:00) — the GDD's "scattered build is overrun by ~12:00" does not happen while the Lance is in the pool and
  levelled at all.
- **Scattered without the Lance** is overrun from ~4:00 (ratio 0.3–0.65), i.e. *earlier* than the contract.
- Under the **current** (too fast) XP curve every build is maxed by ~3:00 and the gap between builds collapses; the
  stationary random sims are the evidence ([`sims.md`](sims.md)).

The contract ("TTK past 1 s by minute 12 and overrun" for a scattered build) therefore needs (a) the XP fix so builds
are still forming at 12:00, and (b) the non-Lance starters' per-hit damage raised and the Lance's crowd share lowered
so the four starters land in the same band. Numbers: [`recommendations.md`](recommendations.md) R1–R4.
