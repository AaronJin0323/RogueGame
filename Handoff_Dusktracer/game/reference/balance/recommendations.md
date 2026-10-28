# Balance recommendations v0.1 (W6 Wed)

Each item names the exact file / asset / field, the owner area, the evidence and the expected effect. Ordered by
impact on the GDD §3.5 feel. All current values are C++ factory fallbacks (no `DA_` assets on main): change the
fallback, or author the `DA_` asset with these values. Any number that departs from a GDD number needs a dated
TDD §17 line in the same commit (CLAUDE.md). After R1–R3 land, re-run `Tools/Balance/RunSims.ps1` + `parse_runs.py`
and compare with [`sims.md`](sims.md).

Evidence keys: **[M]** analytic model ([`model.md`](model.md), [`weapons.md`](weapons.md)); **[S]** sims
([`sims.md`](sims.md)).

---

## Re-run on main @ 315392a with RH.Bot v2 (2026-10-19): R13–R16

[`sims.md`](sims.md) §0: swarm TTK and XP pace are now in band for steered builds (R10 and R1 worked). The game has
lost its teeth after 12:30: built runs take ~0 %/min and dash 2–3 /min from 15:00, steered runs win 5 / 5, and every
build that reached the Warden killed it with 0–65 damage. Bullet density is still 25–30× under the band. In order of
§3.5 impact:

### R13 — Ramp the 15:00–19:30 threat beats (they hold today's pressure since R11) · owner **Enemies**

All in `Enemies/RHSpawnTimeline.cpp` `MakeDefault` / `DA_SpawnTimeline_Main`:

| Beat | Field | Now | Proposed |
|---|---|---|---|
| 15:00 elite wave `Wave(900.f, …)` | `Count` | 4 | **5** |
| 16:00 Pylon ring `Event(960.f, Nest, Turret, …)` | `Count` | 3 | **5** |
| 17:30 Pylon nest `Event(1050.f, Nest, Turret, …)` | `Count` | 6 | **8** |
| 17:30 Breaker pair `Event(1055.f, Ring, Bruiser, …)` | `Count` | 2 | **3** |
| 18:30 elite wave `Wave(1110.f, …)` | `Count` | 2 | **3** |
| 19:30 Pylon ring `Event(1170.f, Nest, Turret, …)` | `Count` | 3 | **5** |
| Threat rows 15:00 / 17:30 / 19:30 (`{900.f, …}`, `{1050.f, …}`, `{1170.f, …}`) | Rusher / Turret share | 0.18 / 0.10 | **0.24 / 0.14** (Swarm 0.50 → 0.40) |

- **Why.** [S] Steered means at 15 / 18: 0.0 / 0.0 %maxHP/min (band 30–50), dashes 1.8 / 2.8 (band 6–10); random
  0.4 / 0.6 %/min. Only the 10:00 wave and the 12:30 stampede register. More skill checks (Pylon fire, Breaker slams,
  Courser lines), not more HP or Harvest damage. §3.5 keeps Harvest safe by design, so the band has to come from the
  beats. This also moves the no-pick death (median 15:06) toward the ~12:00 contract.
- **Check:** 10 × `-Pick random` + 5 × `steered`. Target steered 15–30 %/min at 10, 30–50 at 15–18, dashes 6–10. Back
  the counts off one step if steered drops below ~50 % wins.

### R14 — The Warden must be able to kill · owner **Boss**

`Boss/RHBossTypes.h` `FRHBossTuning` (on `DA_Enemy_Boss`):

| Field | Now | Proposed |
|---|---|---|
| `BulletDamage` | 12 | **16** |
| `RingInterval` / `RingIntervalEnraged` | 4.0 / 2.5 s | **3.2 / 2.0 s** |
| `DashChargeInterval` / `DashChargeIntervalEnraged` | 7 / 5 s | **5.5 / 4 s** |
| `TargetFightSeconds` | 80 s | **100 s** (more pattern exposure at the same calibrated DPS) |

- **Why.** [S] 11 / 11 builds that reached the Warden killed it; damage to the hero 0–65 (median 24) over 70–361 s;
  steered fights 80–108 s with 0–46 damage. GDD §3.5: the boss kills about a third of the players who reach it.
- **Effect (expected):** ~1.6× the ring and charge exposure and ~1.3× the bullet damage → about 2× the damage taken
  per fight (median ~50, upper quartile ≥ 100 = death). The bot is a strong dodger, so a third of *bot* deaths
  probably means more for humans. Confirm with the next human playtest (start from `L_Hub`).

### R15 — Bullet density: the §3.5 row needs a design decision · owner **Design (GDD §3.5)**, then **Combat** · decision brief: [`r15_bullet_targets.md`](r15_bullet_targets.md)

- [S] Steered live player bullets 1.2 / 28 / 74 at 2 / 10 / 18 (band 150 / 800 / 2 000); best random run 207 at 18:00.
  Two weapon passes (R2/R5 more bullets, R10 back to readable one-shots) moved it by ~2×, not 30×. This is the R5
  conflict still open. **Proposal:** re-baseline the row to what a readable steered build emits, **≥ 10 / ≥ 50 /
  ≥ 120**, and carry the 爽 (source b) on impact and kill effects instead. Otherwise Combat needs a per-weapon
  bullet budget (~30× today), which tests the 6 000 cap (TDD §5). Not a balance number, so route to the GDD owner.

### R16 — XP: nudge the growth so L16 lands at ~12:00 · owner **Build / RunTuning**

| Where | Field | Now | Proposed |
|---|---|---|---|
| `Build/RHRunTuning.h` / `DA_RunTuning` | `XPGrowthFactor` | 1.11 | **1.12** |

- [S] Steered L8 5:25 (target 5:30), L16 11:18 (target 12:30). [M] +0.01 ≈ +40 s at L16 and ~+10 s at L8 → L16 ≈
  12:00, L8 ≈ 5:35. Do this after R13; harder late beats also slow late XP slightly.

---

## Re-run on main @ 3ca1450 (2026-10-19): R10–R12 come first

The bot baseline ([`sims.md`](sims.md) §0) measured the merged R1–R9: all 20 runs died between 6:29 and 10:21. Swarm
TTK is 0.5 s from 0:00, kills/min 30–110, live bullets 2–10, and the late-game probes take 4–7× the §3.5 damage band.
R1 (XP) and R9 (threat reserve) work as designed. The regression is player kill power: R2 was shipped as "same DPS
in more, smaller bullets" (Pulse Rifle 2.5 dmg × 3 / 0.25 s), which drops per-hit damage 4× and with it the one-shot
§3.5 is built on.

### R10 — Pulse Rifle per-hit damage back above Scrapwing HP · owner **Combat** · highest impact

| Where | Field | Now (3ca1450) | Proposed |
|---|---|---|---|
| `Combat/RHWeaponDef.cpp` `MakePulseRifle` | `Base.Damage` | 2.5 | **14** |
| | `Base.Count` | 3 | **2** |
| | `Base.Cooldown` | 0.25 | **0.5** |
| | L2 `DamageAdd` | +2 | **+5** |
| | L6 `DamageAdd` | +2 | **+8** |
| | L3 / L7 count, L4 / L8 rate, L5 size | +1 / +2, ×0.8, ×1.25 | unchanged |
| `MakeOrbitalDrones` | `Base.Damage` / L2 / L6 | 4.5 / +3 / +3 | **8 / +4 / +6** (cooldown 0.4 unchanged) |

- **Why.** [S] Measured on 3ca1450: swarm TTK 0.50 s at minutes 1–2 in all 20 runs (target ≤ 0.3), kills/min 76–112
  at minute 2 and 23–97 at minute 5. The model's "all 3 bolts land on the nearest" assumption predicted 2 kills/s and
  0.25 s; the crowd re-targeting spreads them. Six-hit Scrapwings at 0:00 cannot die in sheets.
- **Effect** [M]: L1 one-shots a Scrapwing at 0:00 (14 ≥ 14) with 4 bolts/s and 4.0 kills/s (now 2.0 modelled,
  ~1.5 measured). L3 at 3:00 needs 2 hits from a 3-bolt volley (TTK ≈ 0). L8 is 27 dmg × 5 bolts / 0.32 s = 16 bolts/s,
  2 hits through 20:00. DPS L1 57 (now 31), L8 430 (now 250). Bullet count is lower than now (4 vs 12 bolts/s at L1).
  Bullet density belongs to the danmaku weapons and modifiers (R5), not to cutting the starter weapon's damage.
- **Check after landing:** `RunSims.ps1 -Bot -Pick random` × 10. Expect swarm TTK ≤ 0.3 s at minutes 1–5, kills/min
  ≥ 150 at minute 2, and L8 back near 5:30 (R1 was fitted to that kill rate).

### R11 — Do not raise late-game pressure yet; soften the 7:30 ring until R10 is measured · owner **Enemies**

| Where | Field | Now | Proposed (temporary) |
|---|---|---|---|
| `Enemies/RHSpawnTimeline.cpp` `MakeDefault` (`Event(450.f, Ring, Swarm, …)`) | `Count` | 48 | **36** |
| same | `Distance` | 1400 | **1600** |

- **Why.** [S] 7 of 10 random-pick runs and 3 of 10 no-pick runs die in minute 7, i.e. at the ring. The segment
  probes at 10 / 15 / 18 take 110–207 %/min (target 15–30 / 30–50) and die within 22–91 s. The pressure is too high
  for today's kill power, not too low. The human playtest's 9.6 %/min at 15:00 came from the pre-tuning build, which
  had L41 and 4× the per-hit damage.
- Re-measure after R10. If late pressure then falls under the band, ramp from the measured numbers (the enemies
  session's baseline is the table in [`sims.md`](sims.md) §0).

### R12 — Keep R1 as shipped; re-check after R10 · owner **Build / RunTuning**

- [S] L8 now lands at ~7:00 (was 1:30 before R1, target 5:30). The XPTable was calibrated on runs that killed
  150–290/min; today's runs kill 30–110/min. With R10 restoring kill rate, R1 should land near its fit. Change
  `XPGrowthFactor` (±0.01 ≈ ±40 s at L16) only after that re-run.

---

## R1 — XP curve: slow it ×3–4 so L8 lands at 5:30 and L16 at 12:30 · owner **Build / RunTuning**

| Where | Field | Now | Proposed |
|---|---|---|---|
| `Build/RHRunTuning.h` / `DA_RunTuning` (`URHRunTuning`) | `XPTable` | empty | `[15, 42, 115]` |
| same | `XPGrowthFactor` | 1.18 | **1.11** |
| same | `XPFirstLevelUp` | 10 | unchanged (ignored while `XPTable` is set) |

- **Why.** [M][S] With the current curve L8 is reached by 2:00 and L16 by 4:00–6:00 in all 5 built sims, L24–29 at
  20:00 (GDD §3: L8 ≈ 5:30, L16 ≈ 12:30, 20–24 level-ups) — with a stationary hero that collects only half the gems. Both commitments happen in the first four minutes,
  weapons are L8 by minute 2, and from ~10:00 there is nothing left to pick — the §3.5 "power jumps" stop exactly
  where the threat beats start, and the steered-vs-scattered contract cannot exist because every build is complete.
- **Effect** ([`xp_table_proposal.csv`](xp_table_proposal.csv), income = model × the measured 0.50 pickup): L2 0:19,
  L8 5:35, L16 12:33, L21 18:40 (20 level-ups by 20:00). XP to next: 15, 42, 115, 128, 142, 158, 175, 194, 215 … Tuning knob after the re-run: `XPGrowthFactor` ±0.01 ≈ ±40 s on L16.
- **Docs.** GDD §7 says "L2 = 10 XP, each level +18 %". The GDD's own anchors (§3) win; log the new curve in
  TDD §17 as the W6 Wed balance pass.
- **Test.** `RHStatsTests.cpp` (`XP L%d->L%d` / "matches Week-1 formula") asserts the old curve and must be updated
  by the owner together with the data.

## R2 — Starter weapons: raise per-hit damage of Pulse Rifle / Scatter Cannon / Orbital Drones · owner **Combat**

`Combat/RHWeaponDef.cpp` (`MakePulseRifle`, `MakeScatterCannon`, `MakeOrbitalDrones`) / `DA_Weapon_*`:

| Weapon | Field | Now | Proposed | L8 hit, now → proposed |
|---|---|---|---|---|
| Pulse Rifle | `Base.Damage` | 10 | **15** | 20 → 28 |
| | L2 `DamageAdd` | +4 | **+5** | |
| | L6 `DamageAdd` | +6 | **+8** | |
| Scatter Cannon | `Base.Damage` | 6 | **8** | 13 → 18 per pellet |
| | L2 `DamageAdd` | +3 | **+4** | |
| | L6 `DamageAdd` | +4 | **+6** | |
| Orbital Drones | `Base.Damage` | 7 | **9** | 14 → 20 |
| | L2 `DamageAdd` | +3 | **+4** | |
| | L6 `DamageAdd` | +4 | **+7** | |

- **Why.** [M] Scrapwing HP grows ×3.4 over the run (14 → 47.6) while these weapons' per-hit damage grows ×1.5–2.2
  over L1→L8; they stop one-shooting at minute 0–3.6 even at L8 (one-shot horizon, [`weapons.md`](weapons.md)). The
  L1 Pulse Rifle needs **2 hits on a 14 HP Scrapwing at 0:00** — the very first seconds of every run miss the §3.5
  one-shot. [S] The no-pick runs measure swarm TTK 1.4 s at minute 1 (Nearest re-targets inside a pressing crowd).
- **Effect.** [M] L1 Rifle one-shots until 0:36, L2 until 3:36, L6/L8 until 8:18 (13:54 with Overcharge R3 +
  Shock). A scattered no-Lance build's estimated swarm TTK drops from 0.9 s to 0.0–0.55 s. DPS rises ≈ +40 % per
  weapon; unbuilt runs are still overrun from ~2:00 (clear ratio 0.28 vs 0.19), so the "unbuilt dies" side holds.
- Rate, count and size tracks stay as they are (GDD §5.2: each level is exactly one of the four).

## R3 — Flare Pylons spawn out of every starter weapon's range and pile up · owner **Enemies**

| Where | Field | Now | Proposed |
|---|---|---|---|
| `Enemies/RHEnemyDef.cpp` `MakeTurret` / `DA_Enemy_Turret` | `FireRange` | 1500 | **1250** |
| `Enemies/RHSpawnTimeline.cpp` `MakeDefault` (6:00–15:30 loop) | `Pylon` share | 0.06 / 0.08 | **0.05 / 0.06** |
| `URHSpawnTimeline` / spawn director | per-role spawn ring | shared `RingMin/RingMax` 1350–1700 | Turret ring **900–1200** (new field, code change in `FRHSpawnDirectorCore::Step`) |

- **Why.** [S] Pylons are static and spawn on the shared 1 350–1 700 uu ring, beyond Pulse Rifle (1 300), Drones
  (900) and Scatter (650) range. In every invulnerable built run without the Rail Lance (range 1 600) they are
  79–189 of the 111–204 enemies alive at 15:00 (85–93 % of everything alive; the one Lance-owning random run had 3)
  — they hold the alive cap (fewer Harvest spawns, fewer kills, less XP) and an out-of-range **elite** Pylon keeps the
  15:00 Threat open for its full 45 s `MaxDuration`. A moving player will walk to them (they are the "priority
  target", GDD §6), so the sims overstate it, but the cap share is real. With `FireRange` 1 250 a Pylon must stand
  inside the Pulse Rifle's 1 300 to shoot, i.e. it is always answerable; the ring change makes it spawn there. Until
  the ring lands, a Pylon spawned at 1 350+ only opens fire once the hero closes to 1 250 — it becomes an
  approach-and-kill target instead of free chip damage, which is the GDD §6 "priority target" read.
- The re-key of the ring is the only code change in this list; until it lands, the `FireRange` + share change is the
  data-only half.

## R4 — Rail Lance: pierce 20 → 12 · owner **Combat**

| Where | Field | Now | Proposed |
|---|---|---|---|
| `Combat/RHWeaponDef.cpp` `MakeRailLance` / `DA_Weapon_RailLance` | `Base.Pierce` | 20 | **12** |

- **Why.** [M] The Lance is the only starter with crowd output: 404 crowd DPS at L1 (≈ 12× the Scatter Cannon),
  2 400 at L8. Any build that owns it keeps swarm TTK at 0 s and a clear ratio ≥ 1.17 all run — including the
  scattered build the GDD says must be overrun by ~12:00 — and every build without it is overrun from ~4:00 (clear
  ratio 0.3–0.65). Difficulty lives in "did the Lance show up", not in steering (§3.5 scaling contract). [S] the
  random runs without the Lance sit at 0.35–0.5 s swarm TTK from minute 5 and 0.7–0.97 s by 15–20; Lance runs stay
  at 0 s while the Lance one-shots (L1 until ~10:00, measured), see [`sims.md`](sims.md) §2.
- **Effect** (with R1 + R2, GDD level curve): steered clear ratio 1.4–3.6 all run (swarm TTK 0 s); scattered with the
  Lance 1.56 at 10:00 → **0.93 at 12:00** → 0.98 at 18:00 (on the edge of overrun from 12:00, as the contract says);
  scattered without the Lance 0.4–0.9. Pierce 12 (13 hits) still clears most of a lane: a 1 900 uu lance crosses
  ≈ 16 Scrapwings at mean harvest density and ≈ 24 on the DensestLine it aims at, so the cut is ≈ −40 % in the
  thickest packs and ≈ −20 % at mean density. It keeps the Bulwark-plate bypass and the
  one-shot horizon (L1 until 9:30, L8 until 25:00). Identity "line clear; pierce-native" is unchanged.

## R5 — Live player bullets: the §3.5 targets are 1–2 orders of magnitude above what the weapons emit · owner **Design (GDD §3.5) + Combat**

- **Evidence.** [S] The sims measured `live_bullets_avg` 2–24 at minute 10 and 2–100 at minute 18 (targets ≥ 150 / 800 / 2 000 at minutes 2 / 10 / 18). The
  stationary hero is the worst case (the crowd hugs the hero, bullets die within frames), but the modifiers session's
  stress test measured 74–95 average with **4 weapons at L8 + 3 modifiers each + 400 enemies** (sibling note
  `modifiers.md`). [M] Steady-state emission × lifetime for the densest L8 weapons: Spiral ≈ 100, Ring ≈ 40, Scatter ≈
  6, Rifle ≈ 9 — a full 4-weapon danmaku build tops out near 150–200 without Split / Fork / Scatter Shell.
- **Proposal.** This is a doc-vs-numbers conflict, not a tuning nudge, so per CLAUDE.md it is flagged rather than
  "fixed" here: either (a) re-baseline the §3.5 bullet row once `RH.Bot` measures a moving player (suggested
  ≥ 30 / ≥ 150 / ≥ 400, which a steered build with one danmaku weapon reaches), or (b) keep the targets and give
  Combat a bullet budget per weapon (e.g. Ring Emitter `Base.Count` 12 → 16 with L3/L6 `CountAdd` +4 → +8; Spiral
  Projector `Base.Lifetime` 1.5 → 2.5). (a) is recommended: VFX density (§3.5 source b) is better served by impact /
  kill effects than by bullets that only exist to be counted, and (b) pushes toward the 6 000-bullet cap (TDD §5) for a metric, not a feeling.

## R6 — L16 commitment is skipped when no fusion exists · owner **Build / Offers** · **fixed on main** (fusions + Legendaries, 4fb4f7a)

- **Evidence.** [S] `OfferFlow: L16 fusion (L16) offer has no legal card; skipped` in every run that reached L16.
  GDD §5.5: "If no fusion is available at L16 … the player instead gets a **Legendary card**". The offer system is
  sacred (CLAUDE.md non-negotiable 5): the fallback must exist before fusions do. Not a number, but it removes one of
  the 2 commitments every run today.
- **Proposal.** Legendary fallback in `URHOfferFlow` (a 4th-tier blessing of the highest-favor god); until 4th-tier
  blessing data exists, offer 3 Epic-rarity cards of the two committed gods (`URHOfferTuning::FusionLevel` stays 16).

## R7 — Rarity magnitudes (the `URHOfferTuning` W6 Wed TODO) · owner **Build / Offers**

| Where | Field | Now | Proposed |
|---|---|---|---|
| `Build/RHOfferTuning.cpp` / `DA_OfferTuning` | `UpgradeLevelsByRarity` | {1, 2, 3} | **{1, 2, 2}** |
| same | `RarityRolledKinds` | {WeaponUpgrade} | unchanged (no Blessing / Modifier rarity in v0.1) |
| same | `RarityRollWeight` / `LuckRarityScale` | {0.78, 0.18, 0.04} / 0.10 | unchanged |

- **Why.** [S] Rare/Epic weapon upgrades took the Pulse Rifle from L3 to L8 in three picks (L4 Rare +2, L6 Epic +2
  to the cap). With R1 there are ~21 picks per run for 4 × 7 weapon levels + 12 sockets + 15 blessing ranks, so +3
  levels per Epic is a third of a weapon track in one card. {1, 2, 2} keeps Rare/Epic exciting (Epic also gets the
  lower `RarityBaseWeight` 0.3), average 1.22 levels per card. Blessing/Modifier rarity stays off: a modifier is
  taken once (GDD §5.3) and blessing ranks already feed favor 1:1 (§5.4); a magnitude there changes the favor
  economy and is a design call, not balance.

## R9 — A full alive cap freezes the threat mix · owner **Enemies** (from the bot baseline, n = 2 + 2: confirm on re-run)

| Where | Field | Now | Proposed |
|---|---|---|---|
| `Enemies/RHSpawnDirector.cpp` `FRHSpawnDirectorCore::Step` | over-cap rule | spawn dropped when `NumAlive >= Cap` | **non-Swarm roles may exceed the cap by 15 %** (`Cap × 1.15` for Tank / Rusher / Turret / Bruiser / Support picks; Swarm keeps `Cap`) |
| `DA_SpawnTimeline_Main` / `URHSpawnTimeline` | new field | — | `ThreatCapAllowance = 0.15` (data, so it can be tuned to 0 to restore today's rule) |

- **Why.** [S] An unbuilt bot run sits at the cap from 1:00; every later spawn is dropped, so by 10:00 it has seen 494
  spawns against 1 832 in a built run, almost no Coursers / Pylons / Breakers, and takes 0 % damage until 10:00. Built
  runs free the cap and get the threat roles — they died earlier (8:22, 10:30) than unbuilt ones (11:09, 15:02,
  15:23). §3.5: threats are skill checks that must keep arriving; "difficulty lives in the build".
- **Effect (expected).** The unbuilt run keeps meeting Coursers and Pylons on schedule, so it dies when its build
  can't answer them (toward the ~12:00 contract) instead of when the 10:00 wave arrives; built runs are unchanged
  (they are under the cap). Swarm still saturates first, so the screen-full read stays.

## R8 — Small numbers (low impact; batch with the owners' next data pass)

| # | Owner | Where | Field | Now | Proposed | Why |
|---|---|---|---|---|---|---|
| a | Enemies | `Enemies/RHEliteDef.cpp` `MakeFallback(Shielded)` | `ShieldHits` | 12 | **16** | [M] A Pulse Rifle L8 (8.5 bolts/s) strips 12 in 1.4 s, Spiral L8 (67 shots/s) in 0.2 s; 16 keeps the "density alone doesn't kill it" read for one more second. |
| b | Enemies | `Enemies/RHEnemySubsystem.cpp` kill block (`K.XP = Types[...].XP`) | elite XP | base role XP (3–8) | **× 5 (elite HP mult)** | [M] An elite costs 5× the time and drops 4 XP; with R1 slowing levels, elites should pay. Adds ≈ 1 level over the 3 waves. |
| c | Enemies | `Enemies/RHSpawnTimeline.cpp` | 15:30 → 20:00 blocks | — | **done on main** (rates 7.0 → 8.0 /s, 3 events) | [M] the minute-18 kill target (≥ 400/min) needs ≥ 6.7 /s; main now has 7.4–7.8 /s there. |

---

### Not recommended (checked, fine as is)

- **HP / damage scaling** (`HPPerMinute` 0.12, `DmgPerMinute` 0.05): with R1–R3 they produce the §3.5 contract in the
  model; changing them would move every other table.
- **Hurt i-frames** 0.4 s: caps contact intake at contact ÷ 0.4 (22.5 HP/s from Scrapwings at minute 10) — "caught,
  not chipped" as written.
- **Elite HP ×5**: 8–17 s of focus for a steered build at 15:00 on the worst affix combo, inside the 45 s Threat.
