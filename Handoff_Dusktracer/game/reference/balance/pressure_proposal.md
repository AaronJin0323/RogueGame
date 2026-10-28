# Mid-run pressure collapse — model and proposal (playtest 3, 2026-10-20)

**Status: proposal. Nothing is tuned yet.** For the scheduler, enemies and statuses. Source: the designer's run on `main`
3bcd288 (`Saved/Playtests/RunStats_20261019_145330_seed1361775151.csv`), Quicksilver → Shade, Pulse Rifle alone until
13:00. GDD §3.5 bands: damage taken 15–30 %/min at minute 10 and 30–50 at 18; dashes 4–8 and 6–10.

## 1. What the run shows

| Minute | Alive | Swarm spawn → death | Kills/min | Dashes/min | Damage %/min |
|---|---|---|---|---|---|
| 2 | 7 | 2.95 s | 152 | 0 | 0 |
| 5 | 10 | 1.75 s | 248 | 0 | 0 |
| 8 | 4 | 1.10 s | 263 | 1 | 0 |
| 10 | 5 | 0.75 s | 334 | 10 | 7.6 |
| 12 | 7 | 0.85 s | 366 | 3 | 0 |
| 16 | 9 | 0.73 s | 348 | 7 | 35.8 (15:00 wave + 16:00 Pylon ring) |
| 18 | 8 | 0.60 s | 420 | 0 | 9.6 |
| 19 | 7 | 0.65 s | 372 | 13 | 58.7 (17:30 nest + 18:30 wave) |

All the damage and nearly all the dashes come from the **threat beats** (10:00, 12:30, 15:00, 16:00, 17:30, 18:30).
Between beats, nothing reaches the hero.

## 2. Why: the kill horizon is outside every skill check

Enemies spawn on a ring 1 350–1 700 uu from the hero (`URHSpawnTimeline::RingMin/RingMax`) and walk in. The distance an
enemy covers before it dies is its speed × its spawn-to-death time:

| Role | Speed | Travel before death (measured spawn → death 0.6–1.0 s) | Dies at (from the hero) | Its skill check needs |
|---|---|---|---|---|
| Scrapwing | 330 uu/s | 200–330 uu | **~1 050–1 500 uu** | contact (~80 uu) |
| Courser | 240 uu/s | 150–240 uu | ~1 100–1 550 uu | charge trigger 750 uu |
| Breaker | 130 uu/s | 80–130 uu | ~1 250–1 600 uu | slam range 300 uu |
| Flare Pylon | static | — | where it spawns | FireRange 1 250 uu (R3) |

Little's law gives the same picture: alive ≈ arrival rate × lifetime ≈ 5–8 /s × 1 s ≈ 5–10, which is what the CSV shows.
A strong build has a **kill horizon of ~1 000 uu**, wider than every trigger range. Coursers die before they wind up,
Breakers before they slam, and Scrapwings never touch the hero. The skill checks GDD §3.5 relies on never happen. This
is not a DPS problem to fix with HP: raising HP until enemies survive 3–4 s would be the HP wall §3.5 forbids.

The Bot v2 runs agree: steered builds took ~0 %/min after 12:30 (`sims.md` §0). The bot dodges well, but the bigger
reason is that nothing arrives to dodge.

## 3. Proposal: telegraphed arrivals ("drop-ins")

The danger should not depend on the enemy surviving the walk. Some threats should **arrive already in range, with a
red telegraph**, so the skill check happens even when the enemy dies a moment later. Same damage numbers, no HP change.

| # | Change | Owner | Numbers |
|---|---|---|---|
| P1 | **Drop-in threats** (new director feature). A red ground marker appears, then 1.0 s later the enemy arrives there and acts at once. *Courser line:* arrives 1 000–1 200 uu out with its charge line already drawn (the 1 s marker is the wind-up), then charges 1 400 uu through where the hero stood. *Breaker drop:* a red slam circle (r 300) centred 0–200 uu from the hero; the Breaker lands at the end of the 1.2 s telegraph and slams. *Pylon perch:* arrives 1 000 uu out and fires its 3-shot burst after its 0.45 s aim line. | enemies (`URHSpawnDirector`, `FRHSpawnBlock`) + statuses (role behaviours) | Harvest from 6:00: one every 12 s, 10 s at 10:00, 7 s at 15:00, 5 s at 18:00; ×2 in Threat blocks. Mix 50 % Courser, 30 % Breaker, 20 % Pylon. Fairness: ≤ 1 drop-in telegraph active at once (≤ 2 in Threat), ≥ 1.5 s between landings, never inside a boss fight. |
| P2 | **Courser trigger range** 750 → 1 100 uu, charge duration 0.65 → 0.9 s (line ~1 350 uu). Regular Coursers wind up before the kill horizon, and the line still overshoots. | statuses (`MakeRusher`) | Wind-up 1.0 s unchanged (GDD §6) |
| P3 | **Threat-role spawn ring:** in Threat blocks, Courser / Breaker regular spawns use a 1 000–1 200 uu ring instead of 1 350–1 700. | enemies (`URHSpawnTimeline`, per-role ring, the R3 hook) | Harvest unchanged (§3.5 keeps Harvest safe) |
| P4 | **Measure it:** RH.Stats adds `threat_actions_per_min` (charges started, slams started, Pylon bursts) and `nearest_enemy_p10_uu`, so "enemies never get close" becomes a number. | integration (`URHRunStats`) | Targets: ≥ 4 threat actions/min at 10:00, ≥ 8 at 18:00 |

### Expected effect (model: a human misses ~25 % of telegraphs and dashes on ~50 % of the rest)

| Minute | Drop-ins/min (P1) | + regular charges that now fire (P2) | Dashes/min | Damage %/min (hit 25 % × Courser 21 / Breaker 27 dmg at 10:00; 28 / 36 at 18:00) | §3.5 band |
|---|---|---|---|---|---|
| 10 | 6 | ~2 | 4–6 | 6 × 0.25 × ~23 ≈ **35** → with ~1/3 dodged by walking ≈ **23** | 15–30 / 4–8 ✔ |
| 18 | 12 | ~3 | 7–9 | 12 × 0.25 × ~31 ≈ **93** → walked off ≈ **45** | 30–50 / 6–10 ✔ |

Harvest stays mostly safe: one beat every 10–12 s, telegraphed. Threat beats keep their current shape.

## 4. What I would not do

- Raise enemy HP or HPPerMinute (HP walls, §3.5).
- Make Harvest Scrapwings tanky so they reach contact (the 爽 one-shot must stay; swarm TTK is in band).
- Shorten telegraphs (Heat rule: never shorter telegraphs).

## 5. How it would be verified

10 Bot v2 steered runs plus 10 random, minutes 10 / 15 / 18: dashes, damage, `threat_actions_per_min`,
`nearest_enemy_p10_uu`. Then the next human playtest. The bot dodges better than a human, so its damage should land
near the bottom of each band.
