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
| C.GATE.TEST_FLOOR | 30 | n | CORE (rises only) | 11_Quality §1, Tools/Gate.ps1 |
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

## Arena and measurement

Stress arena `L_Arena_Test` (built by `Tools/create_arena.py`), its camera, and the `smoke` / `perf` tools. Light and exposure become `C.PRE.*` keys when the two weathers are measured (`04_Presentation §7`).

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.ARENA.SUN_ELEVATION_DEG | 20 | deg | GATED(S0) | Tools/create_arena.py, 04_Presentation §4 |
| C.ARENA.SUN_YAW_DEG | -60 | deg | GATED(S0) | Tools/create_arena.py |
| C.ARENA.SUN_LUX | 10 | lux | GATED(S0) | Tools/create_arena.py |
| C.ARENA.SKY_LIGHT_INTENSITY | 1.0 | scale | GATED(S0) | Tools/create_arena.py |
| C.ARENA.EXPOSURE_EV100 | 0.0 | EV100 | GATED(S0) | Tools/create_arena.py, 04_Presentation §4 (fixed: min = max) |
| C.ARENA.BLOOM_THRESHOLD | 1.0 | scale | GATED(S0) | Tools/create_arena.py, 04_Presentation §4 |
| C.ARENA.CAMERA_PITCH_DEG | 60 | deg below horizon | GATED(S1) | Game/DTCameraPawn |
| C.ARENA.CAMERA_FOV_DEG | 50 | deg (horizontal) | GATED(S1) | Game/DTCameraPawn |
| C.ARENA.CAMERA_FRAME_RADIUS_UU | 2000 | uu | GATED(S0) | Game/DTCameraPawn (fits the stress swarm) |
| C.SMOKE.SECONDS | 10 | s | CORE | Tools/Smoke.ps1 |
| C.SMOKE.GOLDEN_MEAN_DIFF_MAX | 12.0 | 8-bit level | GATED(S0) | Tools/image_check.py, Tools/Smoke.ps1 |
| C.SMOKE.IMAGE_STDDEV_MIN | 4.0 | 8-bit level | CORE | Tools/image_check.py (not blank / uniform) |
| C.PERF.WARMUP_S | 10 | s | CORE | Tools/Perf.ps1, T-TEC-10 |
| C.PERF.CAPTURE_FRAMES | 1800 | n | CORE | Tools/Perf.ps1, T-TEC-10 |
| C.PERF.LOAD_MIN_RATIO | 0.95 | ratio | CORE | Tools/Perf.ps1, T-TEC-10 (median live bullets and enemies must reach this share of the caps, or the measurement is void) |
| C.PERF.GPU_PROXY_4080_TO_4070 | 1.6 | x | GATED(S4) | 07_Technical §5, Tools/Perf.ps1, T-TEC-10 (D-011) |


## Telemetry (02_Systems/telemetry.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.TEL.INTERVAL_S | 60.0 | s (sim time) | CORE | telemetry.md, Telemetry/DTTelemetryCore, T-TEL-01 |
| C.TEL.RATE_UNIT_S | 60.0 | s | CORE | telemetry.md (every `*_per_min` column is a count per this many seconds) |
| C.TEL.TTK_PERCENTILE | 0.50 | ratio | CORE | telemetry.md (`swarm_ttk_s` column), C.TTK.SWARM_STEERED_MAX |
| C.TEL.NEAREST_PERCENTILE | 0.10 | ratio | CORE | telemetry.md (`nearest_enemy_p10_uu` column) |
| C.TEL.POWER_CHECK_LEVEL | 16 | level | CORE | Tools/power_split.py, C.POWER.KEEPER_SHARE_L16, T-BAL-01 |

## Keepers (02_Systems/keepers.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.KEEPER.ARCHETYPES_PER_KEEPER | 3 | n | CORE | keepers.md R-KEE-01, expression.csv, T-KEE-01 |
| C.KEEPER.BLESSINGS_PER_ARCHETYPE | 4 | n | CORE | keepers.md R-KEE-01 (Key, Amp1, Amp2, Trait), expression.csv, T-KEE-01 |
| C.KEEPER.TRAIT_PREREQ_CARDS | 2 | n | GATED(S1) | keepers.md R-KEE-04, offers.md, T-KEE-04 |
| C.KEEPER.PACKAGE_GAIN_BAND | 2.0–2.4 | × crowd DPS | GATED(S1) | keepers.md R-KEE-18, T-KEE-18 (reference D-57) |
| C.TIER.FAVOR_PER_BLESSING | 1 | favor | CORE | keepers.md R-KEE-07, T-KEE-07 |
| C.TIER.FAVOR_T1 | 3 | favor | CORE | keepers.md R-KEE-07, T-KEE-07 |
| C.TIER.FAVOR_T2 | 6 | favor | CORE | keepers.md R-KEE-07, T-KEE-07 |
| C.TIER.FAVOR_T3 | 9 | favor | CORE | keepers.md R-KEE-07, T-KEE-07 |
| C.TIER.LINE_WORDS_EN_MAX | 12 | words | CORE | keepers.md R-KEE-08, T-KEE-08 |
| C.TIER.LINE_CHARS_ZH_MAX | 20 | chars | CORE | keepers.md R-KEE-08, T-KEE-08 |
| C.ENCHANT.STATUS_CHANCE_PRIMARY | 0.15 | ratio per hit | GATED(S1) | keepers.md R-KEE-09, T-KEE-09 |
| C.ENCHANT.STATUS_CHANCE_SECOND | 0.05 | ratio per hit | GATED(S1) | keepers.md R-KEE-09, T-KEE-09 |
| C.ENCHANT.SHOCK_VULN | 0.15 | ratio | GATED(S1) | keepers.md R-KEE-10, T-KEE-10 |
| C.ENCHANT.SHOCK_DURATION_S | 2.0 | s | GATED(S1) | keepers.md R-KEE-10, T-KEE-10 |
| C.ENCHANT.BURN_DPS_RATIO | 0.20 | ratio of hit per s | GATED(S1) | keepers.md R-KEE-11, T-KEE-11 |
| C.ENCHANT.BURN_DURATION_S | 3.0 | s | GATED(S1) | keepers.md R-KEE-11, T-KEE-11 |
| C.ENCHANT.BURN_STACKS_MAX | 3 | n | GATED(S1) | keepers.md R-KEE-11, expression.csv, T-KEE-11 |
| C.ENCHANT.CHILL_SLOW_PER_STACK | 0.15 | ratio | GATED(S1) | keepers.md R-KEE-12, T-KEE-12 |
| C.ENCHANT.CHILL_DURATION_S | 2.0 | s | GATED(S1) | keepers.md R-KEE-12, T-KEE-12 |
| C.ENCHANT.FREEZE_STACKS | 3 | n | GATED(S1) | keepers.md R-KEE-12, expression.csv (Frost lowers it), T-KEE-12 |
| C.ENCHANT.FREEZE_DURATION_S | 1.5 | s | GATED(S1) | keepers.md R-KEE-12, T-KEE-12 |
| C.INTERCESSION.STORM_DURATION_S | 6.0 | s | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.STORM_SPLIT_CHILDREN | 3 | n | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.FORGE_DURATION_S | 5.0 | s | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.FORGE_RING_INTERVAL_S | 0.5 | s | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.FORGE_RING_RADIUS_UU | 600 | uu | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.TIDE_RADIUS_UU | 2000 | uu | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.TIDE_FREEZE_S | 3.0 | s | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.RETUNE_STEP_MAX | 2.0 | × per pass | GATED(S1) | keepers.md R-KEE-15, T-KEE-15 (weak ×2 up, strong ×0.5 down) |
