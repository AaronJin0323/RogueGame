# 08 — Constants (the only place numbers live)

> Mirrored by `constants.yaml`; code and data defaults are generated from the YAML (`Tools/gen_constants.py`, S0). Tags: `CORE` · `GATED(<source>)` · `MEASURED(<ref>)`. Changing a `CORE` shape constant needs a decision record and a dependency sweep (`Harness/H04 §5`).
>
> **Status: §1 confirmed by the owner (D-001); locks formally at S1 exit.** Source: `Handoff_Dusktracer/game/G02_Blueprint_DocSet_zh.md` part 3 §08 and §07.

## 1. Shape constants (lock at S1 exit)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.RUN.LENGTH_TO_BOSS | 900 | s | CORE | director.md, timeline_*.csv, T-DIR-01 |
| C.RUN.BOSS_MAX | 120 | s | CORE | boss.md |
| C.LEVEL.L8_MIN | 300 | s | CORE (curve GATED(S1)) | progression.md, offers.md §4 |
| C.LEVEL.L16_MIN | 630 | s | CORE (curve GATED(S1)) | progression.md, offers.md §5 |
| C.DECISIONS.PER_RUN | 28 | n | CORE | offers.md §9, dashboard |
| C.POOL.STARTER_WEAPONS | 4 | n | CORE | offers.md §3 |
| C.POOL.STARTER_MODIFIERS | 8 | n | CORE | offers.md §3 |
| C.POOL.STARTER_BLESSINGS | 18 | n | CORE | offers.md §3, keepers.md |
| C.BULLETS.CAP | 6000 | n | CORE | 07_Technical §budgets, T-PERF-01 |
| C.ENEMIES.CAP | 400 | n | CORE | 07_Technical §budgets, T-PERF-01 |
| C.OFFER.CARDS | 5 | n | CORE | offers.md §1 |
| C.OFFER.REROLLS | 2 | n | CORE | offers.md §6 |
| C.OFFER.BANISH | 1 | n | CORE | offers.md §6 |
| C.OFFER.LOCK | 1 | n | CORE | offers.md §6 |
| C.TTK.SWARM_STEERED_MAX | 0.3 | s | CORE | 10_Tuning §1 |
| C.DMG_TAKEN.BAND_MIN10_13 | 30–50 | %/min | CORE | director.md, T-DIR-02 |
| C.DASHES.BAND_MIN10_13 | 6–10 | /min | CORE | combat_core.md |
| C.POWER.KEEPER_SHARE_L16 | ≥ 0.60 | ratio | CORE | 10_Tuning §3, T-BAL-01 |
| C.POWER.ARCHETYPE_BAND | ±0.15 | ratio | CORE | 10_Tuning §3 |
| C.INTERCESSION.BAND | ±0.20 of mean | ratio | CORE | keepers.md §7 |

## 2. Simulation (07_Technical §4)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.ARENA.HALF_EXTENT_UU | 4000 | uu | GATED(S1) | 07_Technical §4, Core/DTSpatialHash |
| C.HASH.CELL_UU | 200 | uu | GATED(S0) | 07_Technical §4, T-TEC-13 |
| C.SIM.PLANE_Z_UU | 50 | uu | CORE | 07_Technical §4 |

## 3. Presentation (04_Presentation)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.PRE.CONTRAST_MIN | 3.0 | ratio | CORE | 04_Presentation §2, T-PRE-05 |
| C.PRE.DENSITY_LOD_KILLS_PER_S | 20 | /s | GATED(S1) | 04_Presentation §2, T-PRE-06 |

## 4. Budgets (from `G02` §07 and D-003)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.BUDGET.RES | 1080p60 | — | CORE | 11_Quality §perf |
| C.BUDGET.GAME_THREAD_P50_MS | 8 | ms | CORE | 11_Quality §perf |
| C.BUDGET.GAME_THREAD_P95_MS | 10 | ms | CORE | 11_Quality §perf |
| C.BUDGET.GPU_MS_4070 | 12 | ms | CORE | 11_Quality §perf |
| C.BUDGET.MAC_TARGET_HW | M2 MacBook Air | — | CORE (D-003) | 11_Quality §perf |
| C.BUDGET.MAC_RES_FPS | TBD | — | GATED(S0: empty-arena measurement on the M2 Air) | 11_Quality §perf |
| C.BUDGET.MAC_FRAME_P50_MS | TBD | ms | GATED(S0) | 11_Quality §perf |
| C.BUDGET.MAC_FRAME_P95_MS | TBD | ms | GATED(S0) | 11_Quality §perf |
| C.SIM.FIXED_HZ | 60 | Hz | CORE | 07_Technical R-TEC-03, T-TEC-03 |
| C.SIM.MAX_SUBSTEPS | 2 | n | CORE | 07_Technical R-TEC-03, T-TEC-03 |
| C.SAVE.VERSION | 1 | n | CORE | 07_Technical R-TEC-04, T-TEC-04 |

## 5. Harness caps

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.GATED.DEBT_MAX | 5 | n | CORE | Harness/H03 §4 |
| C.GATE.TEST_FLOOR | 21 | n | CORE (rises only) | 11_Quality §1, Tools/Gate.ps1 |
| C.SPEC.LINES_MAX | 300 | lines | CORE | lint |
| C.STATE.LINES_MAX | 100 | lines | CORE | STATE.md |

Everything else (HP scaling k, XP table, weapon damage, aim tokens) starts from the previous project's MEASURED values in `Handoff_Dusktracer/game/reference/balance/` and is tagged `GATED(S1/S2)` when its spec is written.

## Enemies (02_Systems/enemies.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.ENM.SWARM_RADIUS_UU | 32 | uu | GATED(S1) | enemies.md, T-ENM-03 |
| C.ENM.SWARM_SPEED_UU_S | 280 | uu/s | GATED(S1) | enemies.md, T-ENM-02 |
| C.ENM.SWARM_HP | 10 | hp | GATED(S1) | enemies.md, C.TTK.SWARM_STEERED_MAX, T-ENM-05 |
| C.ENM.STEER_ACCEL_UU_S2 | 2400 | uu/s² | GATED(S1) | enemies.md |
| C.ENM.SEPARATION_RADIUS_UU | 72 | uu | GATED(S0) | enemies.md, T-ENM-03 |
| C.ENM.SEPARATION_STRENGTH | 1.5 | ratio | GATED(S0) | enemies.md, T-ENM-03 |
| C.ENM.MAX_NEIGHBOURS | 8 | n | GATED(S0) | enemies.md, T-PERF-01 |
| C.ENM.SPAWN_RING_RADIUS_UU | 1600 | uu | GATED(S1) | enemies.md, Debug/DTDebugEnemies |
| C.ENM.OPTIC_TINT | 0.35 | ratio | GATED(S1) | enemies.md, 04_Presentation §3 |
| C.ENM.EMISSIVE_STRENGTH | 2.0 | ratio | GATED(S1) | enemies.md, 04_Presentation §2 |
| C.ENM.KILL_RATE_WINDOW_S | 1.0 | s | GATED(S0) | enemies.md, C.PRE.DENSITY_LOD_KILLS_PER_S |

## Bullets (02_Systems/bullets.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.BULLETS.HIT_MEMORY | 8 | n | GATED(S1) | bullets.md, Combat/DTBulletPool, T-BUL-04 |
| C.BULLETS.EMISSIVE_PLAYER | 4.0 | ratio | GATED(S1) | bullets.md, 04_Presentation, Combat/DTBulletSubsystem |
| C.BULLETS.STRESS_RADIUS_UU | 8 | uu | GATED(S0) | bullets.md, Debug/DTDebugBullets |
| C.BULLETS.STRESS_SPEED_UU_S | 800 | uu/s | GATED(S0) | bullets.md, Debug/DTDebugBullets |
| C.BULLETS.STRESS_LIFE_S | 4.0 | s | GATED(S0) | bullets.md, Debug/DTDebugBullets |
| C.BULLETS.STRESS_PIERCE | 1000 | n | GATED(S0) | bullets.md, Debug/DTDebugBullets |
| C.BULLETS.STRESS_DAMAGE | 1.0 | hp | GATED(S0) | bullets.md, Debug/DTDebugBullets |
| C.BULLETS.STRESS_SPIN_DEG_S | 90 | deg/s | GATED(S0) | bullets.md, Debug/DTDebugBullets |
