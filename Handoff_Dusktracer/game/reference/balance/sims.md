# Empirical runs (W6 Wed; re-runs 2026-10-19 on main @ 3ca1450 and @ 315392a)

## 0. Bot v2 baseline on main @ 315392a (R10 weapons, R11 timeline, Warden tuning, RH.Bot v2)

25 full runs with `RH.Bot` v2 (threat map, telegraph dodging), patron rotating over the six Paths, a fresh
`-RHSaveSlot` per run (new-player pool), time scale 4, `-nullrhi`, one game at a time, each killed by PID:
10 × no picks (`bot3_worst`, seeds 1001–1010), 10 × random picks (`bot3_random`, 1101–1110), 5 × `steered`
(`bot3_steered`, 1201–1205: patron Path + its weapons, the GDD §3.5 "well-steered build"). Runs end at death, a boss
kill, or sim minute 26. Plus 9 late-game probes with steered picks (`seg3_10/15/18`: jump to 9:30 / 14:30 / 17:00 at
L13 / L18 / L20, 4 sim minutes each). Rows: `sim_runs.csv`, outcomes: `run_outcomes.csv`, histogram:
`death_histogram.csv`. Means below where a metric is spiky (damage taken, dashes); medians elsewhere.

### Outcomes and death-by-minute

| Arm | Won (Warden killed) | Died | Alive at 26:00 | Death minutes |
|---|---|---|---|---|
| steered (5) | **5** (21:20–21:47) | 0 | 0 | — |
| random (10) | **6** (21:10–26:02) | 4 | 0 | 9, 13, 13, 16 |
| no picks (10) | 0 | 9 | 1 (boss alive) | 10, 10, 14, 15, 15, 15, 16, 19, 20 (median **15:06**) |

The Warden: every build that reached it killed it (11 / 11), taking 0–65 damage (median 24) in fights of 70–361 s;
HP self-calibrates to a ~80 s fight (`TargetFightSeconds`). GDD §3.5 wants the boss to kill about a third of the
players who reach it and a steered player to win 30–40 % of runs.

### Against the GDD §3.5 bands

| Metric | Band | steered | random | no picks |
|---|---|---|---|---|
| Swarm TTK, minute 1 / 5 / 10 | ≤ 0.3 s | **0.00 / 0.02 / 0.27** ✔ | 0.00 / 0.43 / 0.38 | 0.00 / 0.00 / 0.50 |
| L8 / L16 (median) | 5:30 / 12:30 | **5:25 / 11:18** (L16 ~70 s early) | 5:14 / 11:06 | 8:08 / never |
| Live bullets, minute 2 / 10 / 18 | ≥ 150 / 800 / 2 000 | 1.2 / 28 / 74 ✘ (×0.01 / 0.04 / 0.04) | 1.4 / 23 / 47 (max 207) | ~1 |
| Damage taken %/min (mean), 10 / 15 / 18 | 15–30 / 30–50 / 30–50 | 6.2 / **0.0 / 0.0** ✘ | 5.6 / 0.4 / 0.6 ✘ | 20.5 / 5.9 / 6.1 |
| Dashes/min (mean), 10 / 15 / 18 | 4–8 / 6–10 / 6–10 | 3.5 / 1.8 / 2.8 ✘ | 3.6 / 2.8 / 2.9 ✘ | 4.9 / 2.9 / 3.0 |
| Kills/min, minute 2 / 10 / 18 | ≥ 60 / 250 / 400 | 120 / 316 / 397 ✔ ✔ ≈ | 114 / 279 / 393 | 95 / 88 / 28 |

Steered late-game probes (all 9 survived their 4 minutes): damage 0–30 %/min at 10:00 (two of three 0), 0–23 at 15:00,
0 at 18:00; dashes 1–5 /min; swarm TTK 0.0–0.45 s (0.35–1.65 at 18:00, where the regular spawns stop for the boss).

### Verdicts

- **Swarm TTK: fixed for steered builds** (≤ 0.3 s at 1 / 5 / 10 and 0.17–0.27 through 20:00). Random builds sit at
  0.4–0.5 s in minutes 5–12, which is what the contract asks of a scattered build until it catches up.
- **XP pace: fixed, slightly fast at L16.** Steered L8 5:25 (target 5:30), L16 11:18 (target 12:30; range 10:26–12:32).
- **Bullet density: not fixed.** 28 / 74 live player bullets at 10 / 18 for steered builds: 25–30× under the band.
- **Late-game pressure: now *under* the band.** Built runs take ~0–6 %/min at 10 / 15 / 18 and dash 2–4 /min. The bot
  author's note holds: Harvest phases barely hurt a kiter; only the 10:00 wave and the 12:30 stampede register
  (random runs: 14.7 %/min mean at minute 12, max 57). After 12:30, nothing reaches a built player.
- **"An unbuilt run is overrun by ~12:00": half true.** No-pick runs are overrun on screen and in TTK (swarm TTK over
  1 s from 7:00–11:00, alive at the 200 / 300 caps), but they die later: median 15:06, only 2 of 10 by 12:00, and
  one alive at 26:00. Death comes from the 15:00 elite wave, Pylon fire and Breaker slams, not from being swarmed.
- **Difficulty targets: too easy for builds.** Steered wins 5 / 5 (target 30–40 %) and random 6 / 10. The bot is a
  strong dodger (telegraph-aware), so read this as an upper bound for humans, but a 0-damage boss fight is a soft
  boss for anyone.

## 0b. Bot baseline on main @ 3ca1450 (R1 XP, R2/R4/R5 weapons, R3/R8 Pylons, R9 threat reserve) — superseded by §0

Superseded by [sims.md §0](#0-bot-v2-baseline-on-main--315392a-r10-weapons-r11-timeline-warden-tuning-rhbot-v2).

## 0c. Partial bot baseline (main @ 91b2a28) — superseded by §0

Superseded by [sims.md §0](#0-bot-v2-baseline-on-main--315392a-r10-weapons-r11-timeline-warden-tuning-rhbot-v2).

## Earlier runs (main @ f31c07c, no bot)

20 unattended runs, one at a time, `-game -nullrhi -LiveCoding=false`, `RH.TimeScale 4`, `RH.Offer.AutoPick`
(`Tools/Balance/RunSims.ps1`), RH.Stats per sim minute. Raw rows: [`sim_runs.csv`](sim_runs.csv); means per label ×
minute: [`sim_summary.csv`](sim_summary.csv); deaths: [`death_histogram.csv`](death_histogram.csv). Reproduce with
`RunSims.ps1` + `python -I Tools/Balance/parse_runs.py`.

| Label | Runs (seeds) | Setup | Ends |
|---|---|---|---|
| `mortal_random` | 10 (101–110) | AutoPick random, mortal | death |
| `mortal_first` | 5 (111–115) | AutoPick first (guaranteed slot), mortal | death |
| `god_random` | 3 (201–203) | AutoPick random, `RH.God 1` | sim minute 20 |
| `god_first` | 2 (301302303, 302) | AutoPick first, `RH.God 1` | sim minute 20 |
| `god_worst` | 2 (401, 402) | AutoPick worst (no picks: Pulse Rifle L1 all run), `RH.God 1` | sim minute 20 |

**Limits — read before using any number.** There is **no `RH.Bot` on main**: every hero stands still at the arena
centre, so (1) mortal runs measure stand-still survival only, (2) dashes/min and damage-taken/min are 0 or
meaningless, (3) gems are only collected inside the 150 uu pickup radius (measured pickup ≈ 50 % of the full-pickup
model), (4) Flare Pylons out of weapon range are never chased. Automated runs start with **patron None** (no patron
favor or ultimate). The **meta save persists between runs**: the first 20-minute run unlocked Discoveries, so all later
runs drew from an 8-weapon / 12-modifier pool (no stat bonuses, no tree nodes). Chest offers are not wired, the L16
fusion offer is skipped (no fusions, no Legendary fallback), the timeline has no events and nothing after 15:30, and
the boss does not spawn. One launch (seed 201, first try) died of **system out-of-memory at startup** (other sessions'
load) and was re-run.

## 1. Death-by-minute histogram (15 mortal runs)

| Death minute | `mortal_random` (10) | `mortal_first` (5) |
|---|---|---|
| 0:00–0:59 | **10** | **5** |
| ≥ 1:00 | 0 | 0 |

Run time at death: 14.7–17.9 s (median 15.6 s / 16.0 s); every hero died at **L1** with 32–47 kills/min, before the
first level-up (10 XP). Standing still, the Scrapwing stream at 1.6 /s outruns an L1 Pulse Rifle (0.9 kills/s) and
contact damage (6 per hit, one hit per 0.4 s i-frame window → 15 HP/s) ends the run.

**What this does and does not say.** It confirms the floor: a hero that never moves dies in the first Harvest, so the
game is not 无脑 in the "AFK wins" sense. It says **nothing** about the GDD difficulty targets (half of first-timers
dead before 10:00, 30–40 % steered win rate) — those need a moving player. The histogram must be re-run with
`RH.Bot 1` when the integration session lands it; `RunSims.ps1` takes one extra `RH.Later 2 RH.Bot 1` command.

## 2. Invulnerable runs: the §3.5 metrics per minute (means)

| Minute | Label | Kills/min | Swarm TTK median s | Live bullets avg | Alive | Hero level |
|---|---|---|---|---|---|---|
| 2 | god_first | 156 | 0.000 | 4.3 | 15 | 9.0 |
| 2 | god_random | 157 | 0.417 | 3.1 | 16 | 9.3 |
| 2 | god_worst | 53 | 3.0 | 0.0 | 60 (cap) | 6.0 |
| 10 | god_first | 286 | 0.017 | 2.3 | 79 | 23.5 |
| 10 | god_random | 282 | 0.350 | 13.0 | 82 | 22.0 |
| 10 | god_worst | 27 | 8.3 | 0.0 | 200 (cap) | 13.0 |
| 18 | god_first | 319 | 0.108 | 2.0 | 241 | 28.5 |
| 18 | god_random | 315 | 0.800 | 49.4 | 209 | 26.0 |
| 18 | god_worst | 17 | 34.1 | 0.0 | 300 (cap) | 15.0 |
| **§3.5 target** | steered | ≥ 60 / 250 / 400 | ≤ 0.3 | ≥ 150 / 800 / 2 000 | — | L8 5:30, L16 12:30 |

Per-run detail (final builds):

| Run | Final weapons | Swarm TTK m5 / m10 / m15 / m20 | Alive @15:00 (of which Pylons) |
|---|---|---|---|
| god_first 301302303 | Rifle L8, **Lance L8** | 0.00 / 0.00 / 0.00 / 0.02 | 111 (79) |
| god_first 302 | Rifle L8, Scatter L8, Drones L7 | 0.03 / 0.03 / 0.27 / 0.30 | 204 (180) |
| god_random 201 | Rifle L2, **Lance L1**, Drones L1, Ring L1 | 0.00 / 0.10 / **1.02 / 1.47** † | 26 (3) |
| god_random 202 | Rifle L2, Arc Blade L1, Seekers L1, Drones L4 | 0.37 / 0.53 / 0.80 / 0.70 | 204 (189) |
| god_random 203 | Rifle L7, Scatter L4, Seekers L1, Drones L1 | 0.37 / 0.42 / 0.58 / 0.75 | 204 (183) |

† The one Lance run of the random set never upgraded the Lance: L1 (30 dmg) one-shots Scrapwings until 9:30 in the
model, and the measured TTK is 0 s until minute 10 and then climbs past 1 s — the model's one-shot horizon, measured.
It also has almost no Pylons alive: the Lance's 1 600 range reaches the Pylon ring.

## 3. Findings (each feeds [`recommendations.md`](recommendations.md))

1. **XP is ~3× too fast (R1).** Every built run reached L8 by minute 2 and L16 by minute 4–6, and L24–29 by 20:00
   (GDD: L8 5:30, L16 12:30, 20–24 level-ups) — even though the stationary hero collects only about half the gems.
   The model's level curve at 50 % pickup matches the sims within ±1 level at every minute.
2. **Swarm TTK ≤ 0.3 s holds only while a high-damage weapon still one-shots, or for a fully-levelled multi-bolt
   Rifle (R2, R4).** god_first runs (Rifle L8 = 3 bolts per volley on the nearest; Lance L8) stay ≤ 0.3 s all run; the
   Lance-L1 run is at 0 s until minute 10 and past 1 s from minute 15. The random runs
   without the Lance sit at 0.35–0.5 s from minute 5 and 0.7–0.97 s by minute 15–20 — the model's
   "scattered, no Lance" row (0.55–0.9 s). The no-pick runs show the Nearest-retarget effect: TTK 1.4 s at minute 1,
   8 s at minute 10.
3. **Kills/min meets the §3.5 floor at minutes 2 and 10 (≥ 156 and ≥ 282) and misses minute 18 (301–336 vs ≥ 400)**
   — arrivals plateau at 5.7 /s after 15:30 because the timeline ends there (R8c, enemies session).
4. **Live player bullets are 1–2 orders of magnitude under target (R5):** 2–24 average at minute 10 (target ≥ 800),
   2–100 at minute 18 (target ≥ 2 000); the 100 is the only run with a Ring Emitter.
5. **Flare Pylons pile up out of range (R3):** 79–189 Pylons alive at 15:00 in every run without the Lance (85–93 % of
   everything alive), holding the alive cap and keeping elite Threat phases open for their full 45 s.
6. **The no-build run is overrun from ~1:30** (alive at the 60 cap by 2:00, every cap after that full) — "overrun"
   happens, far earlier than the ~12:00 the GDD assigns to a *scattered* build. That contract is analysed in
   [`model.md`](model.md) §4.
7. **Offer flow:** L16 commitment skipped in every run (no fusion, no Legendary fallback, R6); Rare / Epic weapon
   upgrades take a weapon L3 → L8 in three picks (R7).
