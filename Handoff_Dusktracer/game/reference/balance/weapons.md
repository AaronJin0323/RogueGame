# Weapon DPS table (v0.1, base weapons main @ f31c07c, fusions main @ 91b2a28)

Source: `Combat/RHWeaponDef.cpp` (`Make*` C++ factory fallbacks; there are no `DA_Weapon_*` assets yet). Full table, every
level 1–8 and every column, base weapons and the six fusions: [`weapons.csv`](weapons.csv). Regenerate with
`python -I Tools/Balance/balance_model.py`.

## How to read it

- **Single DPS**: one lone target (radius 40 uu) at 300 uu. Counts only the bullets whose geometry reaches it: Cone
  pellets spread over 40°, Ring/Spiral bullets spread over 360°, the second Arc Blade swing faces the other way, extra
  Flak shells / mines land within their `ShotSpreadRadius`. This is the boss / elite number.
- **Crowd DPS**: raw damage into a harvest crowd at 1 enemy per 100 × 100 uu (`CROWD_DENSITY = 1e-4`; DensestCluster
  and DensestLine targeting see 1.5× that). Pierce, blast radius, arc sweep and pull reach multiply the hits. AoE and
  pierce numbers are **linear in density**: halve the crowd and they halve. No overkill cap, so it overstates kills of
  14 HP Scrapwings; use the kills/s column for that.
- **Swarm hits-to-kill**: hits one Scrapwing needs at run minute 0/5/10/15/20 (HP 14 × (1 + 0.12·min) = 14 / 22.4 /
  30.8 / 39.2 / 47.6). **1 = one-shot**, the GDD §3.5 "dies in sheets" condition.
- Base hero stats (Damage 100 %, crit 5 % × 150 % → ×1.025 average). No modifiers, blessings or tiers.

## L1 / L4 / L8

| Weapon | L | Hit dmg | Count | CD s | Single DPS | Crowd DPS | Swarm hits-to-kill m0/5/10/15/20 | Swarm kills/s in crowd @ m10 |
|---|---|---|---|---|---|---|---|---|
| Pulse Rifle | 1 | 10 | 1 | 0.55 | 18.6 | 18.6 | 2/3/4/4/5 | 0.45 |
| Pulse Rifle | 4 | 14 | 2 | 0.44 | 65.2 | 65.2 | 1/2/3/3/4 | 1.52 |
| Pulse Rifle | 8 | 20 | 3 | 0.352 | 174.7 | 174.7 | 1/2/2/2/3 | 4.26 |
| Scatter Cannon | 1 | 6 | 5 | 0.9 | 14.6 | 34.2 | 3/4/6/7/8 | 0.93 |
| Scatter Cannon | 4 | 9 | 7 | 0.72 | 38.3 | 89.7 | 2/3/4/5/6 | 2.43 |
| Scatter Cannon | 8 | 13 | 9 | 0.576 | 91.2 | 208.2 | 2/2/3/4/4 | 5.21 |
| Orbital Drones | 1 | 7 | 2 | 0.8 | 17.9 | 17.9 | 2/4/5/6/7 | 0.50 |
| Orbital Drones | 4 | 10 | 3 | 0.64 | 48.0 | 48.0 | 2/3/4/4/5 | 1.17 |
| Orbital Drones | 8 | 14 | 4 | 0.512 | 112.1 | 112.1 | 1/2/3/3/4 | 2.60 |
| Rail Lance | 1 | 30 | 1 | 1.6 | 19.2 | 403.6 | 1/1/2/2/2 | 6.56 |
| Rail Lance | 4 | 42 | 1 | 1.28 | 33.6 | 706.3 | 1/1/1/1/2 | 16.4 |
| Rail Lance | 8 | 57 | 2 | 1.024 | 114.1 | 2 396 | 1/1/1/1/1 | 41.0 |
| Flak Mortar | 1 | 18 | 1 | 1.4 | 13.2 | 212.5 | 1/2/2/3/3 | 5.76 |
| Flak Mortar | 4 | 24 | 2 | 1.12 | 41.3 | 708.5 | 1/1/2/2/2 | 14.4 |
| Flak Mortar | 8 | 32 | 3 | 1.12 | 87.9 | 3 004 | 1/1/1/2/2 | 91.6 |
| Arc Blade | 1 | 14 | 1 | 0.7 | 20.5 | 139.6 | 1/2/3/3/4 | 3.24 |
| Arc Blade | 4 | 20 | 1 | 0.56 | 36.6 | 368.4 | 1/2/2/2/3 | 8.99 |
| Arc Blade | 8 | 28 | 2 | 0.448 | 64.1 | 1 925 | 1/1/2/2/2 | 33.5 |
| Seeker Swarm | 1 | 9 | 4 | 1.1 | 33.5 | 33.5 | 2/3/4/5/6 | 0.91 |
| Seeker Swarm | 4 | 12 | 6 | 0.88 | 83.9 | 83.9 | 2/2/3/4/4 | 2.27 |
| Seeker Swarm | 8 | 16 | 8 | 0.704 | 186.4 | 186.4 | 1/2/2/3/3 | 5.68 |
| Ring Emitter | 1 | 8 | 12 | 1.2 | 4.0 | 82.0 | 2/3/4/5/6 | 2.50 |
| Ring Emitter | 4 | 11 | 16 | 0.96 | 9.1 | 187.9 | 2/3/3/4/5 | 5.56 |
| Ring Emitter | 8 | 15 | 20 | 0.768 | 20.0 | 400.4 | 1/2/3/3/4 | 8.68 |
| Spiral Projector | 1 | 4 | 1 | 0.07 | 2.8 | 58.6 | 4/6/8/10/12 | 1.79 |
| Spiral Projector | 4 | 5 | 2 | 0.056 | 8.7 | 183.0 | 3/5/7/8/10 | 5.10 |
| Spiral Projector | 8 | 7 | 3 | 0.0448 | 23.4 | 480.5 | 2/4/5/6/7 | 13.4 |
| Gravity Mine | 1 | 40 | 1 | 2.2 | 24.2 | 1 439 | 1/1/1/1/2 | 35.1 |
| Gravity Mine | 4 | 52 | 1 | 1.76 | 39.4 | 3 021 | 1/1/1/1/1 | 56.7 |
| Gravity Mine | 8 | 67 | 2 | 1.408 | 126.8 | 12 460 | 1/1/1/1/1 | 181 |

## Fusions (L16 reforge, `MakeFusion*`, merged after the first pass)

A fusion replaces its base weapon at L16 and keeps that weapon's level and track (Shade × Seeker Swarm has its own
damage/size track). Derived effects are folded in as a crowd multiplier: Storm chain arc ×1.6, Forge crater ×1.8,
Shade "collect" on Marked ×2, Quicksilver blade contact ×1.5; Radiance uses its own crit (+35 % chance, +100 % crit
damage → ×1.6 average). These folds are rough; the fusion rows rank fusions, they do not replace a measured run.

| Fusion | L | Hit dmg | Count | Pierce | CD s | Single DPS | Crowd DPS | Swarm hits-to-kill m0/5/10/15/20 |
|---|---|---|---|---|---|---|---|---|
| Storm × Rail Lance | 4 | 57 | 1 | 40 | 1.28 | 45.6 | 1 925 | 1/1/1/1/1 |
| Storm × Rail Lance | 8 | 72 | 2 | 40 | 1.024 | 144.1 | 6 450 | 1/1/1/1/1 |
| Forge × Flak Mortar | 4 | 36 | 3 | — | 1.12 | 98.8 | 4 244 | 1/1/1/2/2 |
| Forge × Flak Mortar | 8 | 44 | 4 | — | 1.12 | 161.1 | 15 048 | 1/1/1/1/2 |
| Tide × Ring Emitter | 4 | 15 | 16 | all | 0.96 | 15.8 | 2 878 | 1/2/3/3/4 |
| Tide × Ring Emitter | 8 | 19 | 20 | all | 0.768 | 33.8 | 6 198 | 1/2/2/3/3 |
| Shade × Seeker Swarm | 4 | 34 | 3 hunters | — | 0.5 | 209.1 | 418 | 1/1/1/2/2 |
| Shade × Seeker Swarm | 8 | 52 | 3 hunters | — | 0.4 | 399.7 | 800 | 1/1/1/1/1 |
| Radiance × Pulse Rifle | 4 | 20 | 2 | 6 | 0.44 | 145.5 | 1 018 | 1/2/2/2/3 |
| Radiance × Pulse Rifle | 8 | 26 | 3 | 6 | 0.352 | 354.5 | 2 482 | 1/1/2/2/2 |
| Quicksilver × Orbital Drones | 4 | 14 | 3 | 2 (ricochet) | 0.64 | 67.3 | 303 | 1/2/3/3/4 |
| Quicksilver × Orbital Drones | 8 | 18 | 4 | 2 (ricochet) | 0.512 | 144.1 | 649 | 1/2/2/3/3 |

Reading: Forge and Storm fusions are crowd outliers (an order of magnitude over the Quicksilver and Shade fusions in a
crowd); Shade is the single-target fusion (hunters + ×4 on Marked); Quicksilver × Drones is the weakest in both columns
before its move-speed scaling (`MoveSpeedScaling` 0.5, not modelled) — the first fusion to check in a bot run.

## One-shot horizon (the §3.5 number that matters)

Last run minute at which **one hit** still kills a Scrapwing ([`swarm_oneshot.csv`](swarm_oneshot.csv)). "Steered"
adds the best per-hit damage a v0.1 new-player pool can give: Overcharge R3 (+24 % Damage) × Storm Tier 2 Shock
(+15 % taken) on half the swarm = ×1.33. No other blessing in the 18-blessing starter pool raises per-hit damage;
the modifiers that do (Heavy Slug +60 %, Crit Lens) are Discoveries (GDD §5.3 "starting pool 1–8").

| Weapon | L1 base | L8 base | L8 steered |
|---|---|---|---|
| Pulse Rifle | never | 3.6 | 7.5 |
| Scatter Cannon (per pellet) | never | never | 2.0 |
| Orbital Drones | never | 0.0 | 2.8 |
| Rail Lance | 9.5 | 25.6 | 36.9 |
| Flak Mortar | 2.4 | 10.7 | 17.1 |
| Arc Blade | 0.0 | 8.3 | 13.9 |
| Seeker Swarm | never | 1.2 | 4.4 |
| Ring Emitter | never | 0.6 | 3.6 |
| Spiral Projector | never | never | never |
| Gravity Mine | 15.5 | 31.5 | 44.8 |

**Reading.** Only the high-damage, low-rate weapons (Rail Lance, Gravity Mine, Flak Mortar to ~17:00) keep the
one-shot all run. The "density" weapons — Rifle, Drones, Seekers, Ring, Spiral, Scatter per pellet — never one-shot
after minute ~4–8 even fully levelled and steered, because their per-hit damage grows ×1.5–2.0 over L1→L8 while
Scrapwing HP grows ×3.4 over the run. Their swarm TTK is therefore their re-hit interval × (hits − 1): Pulse Rifle L8
at minute 10 needs 2 hits, 3 bolts per volley on the nearest → still ≈ 0 s; Ring Emitter / Spiral / Seekers spread
their bullets over many targets, so the second hit arrives a volley later (0.7–1.1 s). The Pulse-Rifle-only
measurements in [`sims.md`](sims.md) confirm the L1 number: median 0.467 s (model 0.55 s) at minute 0.

The fix proposals are in [`recommendations.md`](recommendations.md) (R2: per-hit damage on the three non-Lance
starters' tracks, with the proposed one-shot horizons in `swarm_oneshot.csv` rows `numbers=proposed`; R4: Rail
Lance pierce). The Discovery density weapons (Seekers, Ring, Spiral) are left as chip / screen-control weapons for
v0.1; revisit once the bot measures them in moving runs.

## Notes for owners

- **Orbital Drones** fire in turn: the weapon fires every `Cooldown / Count`, so DPS = `Count × Damage / Cooldown`.
- **Spiral Projector** `Count` is arms; each arm fires every `Cooldown` (0.07 s → 14.3 shots/s per arm).
- **Gravity Mine** adds 6 pull ticks × 5 % damage (+30 %) and pulls the cluster into its blast; it is the strongest
  crowd weapon by an order of magnitude if mines trigger in a dense crowd. Its single-target DPS is ordinary.
- **Rail Lance** pierce 20 is the crowd outlier among the four starting weapons (≈ 6× the Scatter Cannon's crowd DPS
  at L8) and ignores the Bulwark plate (`RHEnemySubsystem`: pierce bypasses `ShieldFrontDamageMult`). Both
  invulnerable sims that owned it had swarm TTK median ≤ 0.1 s through minute 10 ([`sims.md`](sims.md)).
