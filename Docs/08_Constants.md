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
| C.ENM.KILL_RATE_WINDOW_S | 1.0 | s | GATED(S0) | enemies.md, C.PRE.DENSITY_LOD_KILLS_PER_S |
| C.ENEMIES.OPTIC_EMISSIVE | 14.0 | ratio | GATED(S1) | enemies.md, R-ENM-24, T-ENM-24 |
| C.ENEMIES.OPTIC_SCALE | 0.35 | ratio | GATED(S1) | enemies.md R-ENM-24 (optic size as a fraction of the hull; grey box) |
| C.ENEMIES.TELEGRAPH_EMISSIVE | 2.0 | ratio | GATED(S1) | enemies.md, R-ENM-13, 04_Presentation.md |
| C.ENEMIES.TELEGRAPH_Z_UU | 2 | uu | GATED(S1) | enemies.md R-ENM-13 (telegraph quads above the floor, below hulls and rounds at C.SIM.PLANE_Z_UU) |
| C.ENEMIES.TELEGRAPH_LEAD_MIN_S | 0.75 | s | GATED(S1) | enemies.md, boss.md, R-ENM-13, T-ENM-13, R-BOS-05 |
| C.ROLE.SWARM_CONTACT_DAMAGE | 6 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-14 |
| C.ROLE.TANK_HP | 90 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-09 |
| C.ROLE.TANK_SPEED_UU_S | 165 | uu/s | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-10 |
| C.ROLE.TANK_RADIUS_UU | 58 | uu | GATED(S1) | enemies.md, 09_Catalog/enemies.csv |
| C.ROLE.TANK_CONTACT_DAMAGE | 12 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-14 |
| C.ROLE.TANK_TURN_DEG_S | 70 | deg/s | GATED(S1) | enemies.md, R-ENM-10, T-ENM-10 |
| C.ROLE.TANK_FRONT_HALF_ARC_DEG | 60 | deg | GATED(S1) | enemies.md, R-ENM-10, T-ENM-10 |
| C.ROLE.TANK_FRONT_DAMAGE_MULT | 0.5 | ratio | GATED(S1) | enemies.md, R-ENM-10, T-ENM-10 |
| C.ROLE.RUSHER_HP | 32 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-09 |
| C.ROLE.RUSHER_SPEED_UU_S | 240 | uu/s | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-11 |
| C.ROLE.RUSHER_RADIUS_UU | 40 | uu | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-11 |
| C.ROLE.RUSHER_CONTACT_DAMAGE | 14 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-14 |
| C.ROLE.RUSHER_TRIGGER_UU | 750 | uu | GATED(S1) | enemies.md, R-ENM-11, T-ENM-11 |
| C.ROLE.RUSHER_WINDUP_S | 1.0 | s | GATED(S1) | enemies.md, R-ENM-11, R-ENM-13, T-ENM-11 |
| C.ROLE.RUSHER_CHARGE_SPEED_UU_S | 1500 | uu/s | GATED(S1) | enemies.md, R-ENM-11, T-ENM-11 |
| C.ROLE.RUSHER_CHARGE_S | 0.65 | s | GATED(S1) | enemies.md, R-ENM-11, T-ENM-11 |
| C.ROLE.RUSHER_RECOVER_S | 0.8 | s | GATED(S1) | enemies.md, R-ENM-11, T-ENM-11 |
| C.ROLE.RUSHER_COOLDOWN_S | 2.0 | s | GATED(S1) | enemies.md, R-ENM-11, T-ENM-11 |
| C.ROLE.TURRET_HP | 40 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-09 |
| C.ROLE.TURRET_RADIUS_UU | 48 | uu | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-12 |
| C.ROLE.TURRET_CONTACT_DAMAGE | 6 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-14 |
| C.ROLE.TURRET_HEAD_TURN_DEG_S | 140 | deg/s | GATED(S1) | enemies.md, R-ENM-12, T-ENM-12 |
| C.ROLE.TURRET_FIRE_RANGE_UU | 1250 | uu | GATED(S1) | enemies.md, R-ENM-12, T-ENM-12 |
| C.ROLE.TURRET_TELL_S | 0.75 | s | GATED(S1) | enemies.md, R-ENM-12, R-ENM-13, T-ENM-12 |
| C.ROLE.TURRET_PERIOD_S | 2.0 | s | GATED(S1) | enemies.md, R-ENM-12, T-ENM-12 |
| C.ROLE.ELITE_HP_MULT | 5 | x | MEASURED(reference balance/model.md) | enemies.md R-ENM-09, director.md, T-ENM-09 |
| C.ENEMY_FIRE.CAP | 1024 | n | GATED(S1) | enemies.md, R-ENM-15, T-ENM-15, T-PERF-01 |
| C.ENEMY_FIRE.SHOT_SPEED_UU_S | 650 | uu/s | GATED(S1) | enemies.md, R-ENM-12 |
| C.ENEMY_FIRE.SHOT_RADIUS_UU | 16 | uu | GATED(S1) | enemies.md, R-ENM-16, T-ENM-20 |
| C.ENEMY_FIRE.SHOT_DAMAGE | 5 | hp | GATED(S1) | enemies.md, R-ENM-16 |
| C.ENEMY_FIRE.SHOT_LIFE_S | 4.0 | s | GATED(S1) | enemies.md, R-ENM-15 |
| C.ENEMY_FIRE.BURST_SHOTS | 3 | n | GATED(S1) | enemies.md, R-ENM-12, R-ENM-20 |
| C.ENEMY_FIRE.BURST_GAP_S | 0.12 | s | GATED(S1) | enemies.md, R-ENM-12 |
| C.ENEMY_FIRE.FAN_COUNT | 4 | n | GATED(S1) | enemies.md, R-ENM-19, R-ENM-20 |
| C.ENEMY_FIRE.FAN_SPREAD_DEG | 14 | deg | GATED(S1) | enemies.md, R-ENM-20, T-ENM-20 |
| C.ENEMY_FIRE.EMISSIVE | 3.5 | ratio | GATED(S1) | enemies.md, R-ENM-17, 04_Presentation.md |
| C.AIM.TOKENS | 3 | n | GATED(S1) | enemies.md, R-ENM-18, T-ENM-18 |
| C.AIM.TOKENS_THREAT | 4 | n | GATED(S1) | enemies.md, R-ENM-18, T-ENM-18 |
| C.AIM.FAN_MIN_RANGE_UU | 600 | uu | GATED(S1) | enemies.md, R-ENM-19, T-ENM-19 |
| C.AIM.TEST_RANGE_UU | 200–2000 | uu | GATED(S1) | enemies.md, T-ENM-20 |
| C.AIM.FAN_TEST_COUNT_MAX | 6 | n | GATED(S1) | enemies.md, T-ENM-20 |
| C.STATUS.SHOCK_S | 2.0 | s | GATED(S1) | enemies.md, keepers.md, R-ENM-21, T-ENM-21 |
| C.STATUS.SHOCK_DAMAGE_TAKEN | 0.15 | ratio | GATED(S1) | enemies.md, keepers.md, R-ENM-22, T-ENM-22 |
| C.STATUS.BURN_S | 3.0 | s | GATED(S1) | enemies.md, keepers.md, R-ENM-21, T-ENM-21 |
| C.STATUS.BURN_TICK_S | 0.5 | s | GATED(S1) | enemies.md, R-ENM-22, T-ENM-22 |
| C.STATUS.BURN_DPS_RATIO | 0.20 | ratio of hit per s | GATED(S1) | enemies.md, keepers.md R-KEE-11, T-KEE-11 |
| C.STATUS.BURN_STACKS_MAX | 3 | n | GATED(S1) | enemies.md, expression.csv (Furnace raises it), R-ENM-21, T-ENM-21 |
| C.STATUS.BURN_STACK_SLOTS | 4 | n | GATED(S1) | enemies.md (storage per enemy; base cap plus any bonus never exceeds it), expression.csv, T-ENM-21 |
| C.STATUS.CHILL_S | 2.0 | s | GATED(S1) | enemies.md, keepers.md, R-ENM-21, T-ENM-21 |
| C.STATUS.CHILL_SLOW_PER_STACK | 0.15 | ratio | GATED(S1) | enemies.md, keepers.md, R-ENM-22, T-ENM-22 |
| C.STATUS.FREEZE_STACKS | 3 | n | GATED(S1) | enemies.md, keepers.md, expression.csv (Frost lowers it), R-ENM-21, T-ENM-21 |
| C.STATUS.FREEZE_S | 1.5 | s | GATED(S1) | enemies.md, R-ENM-21, T-ENM-22 |
| C.STATUS.BOSS_CHILL_STACK_MAX | 2 | n | GATED(S1) | enemies.md, boss.md, R-BOS-11, T-BOS-11 |

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
| C.INTERCESSION.STORM_DURATION_S | 6.0 | s | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.STORM_SPLIT_CHILDREN | 3 | n | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.FORGE_DURATION_S | 5.0 | s | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.FORGE_RING_INTERVAL_S | 0.5 | s | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.FORGE_RING_RADIUS_UU | 600 | uu | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.TIDE_RADIUS_UU | 2000 | uu | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.TIDE_FREEZE_S | 3.0 | s | GATED(S1) | keepers.md R-KEE-14, T-KEE-14 |
| C.INTERCESSION.RETUNE_STEP_MAX | 2.0 | × per pass | GATED(S1) | keepers.md R-KEE-15, T-KEE-15 (weak ×2 up, strong ×0.5 down) |

## Offers and progression (02_Systems/offers.md, progression.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.OFFER.SECOND_KEEPER_LEVEL | 8 | level | CORE | 02_Systems/offers.md, 02_Systems/progression.md, T-OFF-08 |
| C.OFFER.REFORGING_LEVEL | 16 | level | CORE | 02_Systems/offers.md, 02_Systems/progression.md, T-OFF-09 |
| C.OFFER.REFORGING_CARDS | 3 | n | CORE | 02_Systems/offers.md, T-OFF-09 |
| C.OFFER.KEEPER_CHOICES | 3 | n | GATED(S1) | 02_Systems/offers.md, T-OFF-08 |
| C.OFFER.CHEST_CARDS | 3 | n | GATED(S1) | 02_Systems/offers.md, T-OFF-14 |
| C.OFFER.WEIGHT_BASE | 1.0 | x | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.WEIGHT_FAVOR | 0.3 | x/favor | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.WEIGHT_WEAPON_LEVEL | 0.25 | x/level | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.WEIGHT_COMMITTED | 1.5 | x | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.WEIGHT_MOMENTUM | 0.5 | x/card | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.WEIGHT_OTHER_ARCHETYPE | 0.8 | x | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.MOMENTUM_DAMP_AFTER | 2 | n | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.WEIGHT_MOD_AMPLIFIES | 2.0 | x | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.WEIGHT_MOD_NO_AMPLIFY | 0.7 | x | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.WEIGHT_WEAPON_FIT | 1.5 | x | GATED(S1) | 02_Systems/offers.md, T-OFF-02 |
| C.OFFER.GUARANTEE_BLESSING_P | 0.7 | ratio | GATED(S1) | 02_Systems/offers.md, T-OFF-06 |
| C.OFFER.MIN_COMMITTED_BLESSINGS | 2 | n | GATED(S1) | 02_Systems/offers.md, T-OFF-06 |
| C.OFFER.RARITY_RARE_P | 0.18 | ratio | GATED(S1) | 02_Systems/offers.md, T-OFF-05 |
| C.OFFER.RARITY_EPIC_P | 0.04 | ratio | GATED(S1) | 02_Systems/offers.md, T-OFF-05 |
| C.OFFER.RARE_UPGRADE_LEVELS | 2 | level | GATED(S1) | 02_Systems/offers.md, T-OFF-05 |
| C.OFFER.EPIC_UPGRADE_LEVELS | 3 | level | GATED(S1) | 02_Systems/offers.md, T-OFF-05 |
| C.OFFER.FORCE_WEAPON_LEVEL | 5 | level | GATED(S1) | 02_Systems/offers.md, T-OFF-11 |
| C.OFFER.FORCE_WEAPON_BELOW | 2 | n | GATED(S1) | 02_Systems/offers.md, T-OFF-11 |
| C.OFFER.INPUT_GUARD_S | 0.4 | s | GATED(S1) | 02_Systems/offers.md, T-OFF-11 |
| C.OFFER.RESERVE_RESTORE_HP_RATIO | 0.3 | ratio | GATED(S1) | 02_Systems/offers.md, T-OFF-12 |
| C.OFFER.RESERVE_HONE_DAMAGE_RATIO | 0.04 | ratio | GATED(S1) | 02_Systems/offers.md, T-OFF-12 |
| C.OFFER.RESERVE_SPARE_REROLLS | 1 | n | GATED(S1) | 02_Systems/offers.md, T-OFF-12 |
| C.OFFER.RESERVE_SALVAGE_RADIUS_RATIO | 0.1 | ratio | GATED(S1) | 02_Systems/offers.md, T-OFF-12 |
| C.OFFER.RESERVE_MEND_MAX_HP | 10 | hp | GATED(S1) | 02_Systems/offers.md, T-OFF-12 |
| C.OFFER.RESERVE_SHARE_MAX | 0.1 | ratio | GATED(S1) | 02_Systems/offers.md, T-OFF-15 |
| C.OFFER.BUDGET_LEVEL_HANDS | 20 | n | GATED(S1) | 02_Systems/offers.md, 02_Systems/progression.md, C.DECISIONS.PER_RUN, T-OFF-15 |
| C.OFFER.BUDGET_CHEST_HANDS | 6 | n | GATED(S1) | 02_Systems/offers.md, director.md, C.DECISIONS.PER_RUN, T-OFF-15 |
| C.OFFER.BUDGET_MILESTONES | 2 | n | GATED(S1) | 02_Systems/offers.md, 02_Systems/progression.md, C.DECISIONS.PER_RUN, T-OFF-15 |
| C.OFFER.DECISIONS_TOLERANCE | 3 | n | GATED(S1) | 02_Systems/offers.md, T-OFF-15 |
| C.OFFER.PAUSED_S_PER_MIN_MAX | 15 | s/min | GATED(S1) | 02_Systems/offers.md, T-OFF-15 |
| C.XP.GROWTH | 1.055 | x | GATED(S1) | 02_Systems/progression.md, 09_Catalog/xp_curve.csv, T-PRG-02 |
| C.XP.ANCHOR_TOLERANCE_S | 30 | s | GATED(S1) | 02_Systems/progression.md, T-PRG-03 |
| C.XP.REFERENCE_PICKUP_RATIO | 0.51 | ratio | MEASURED(reference balance/model.md) | 02_Systems/progression.md, 09_Catalog/xp_curve.csv, T-PRG-03 |
| C.PICKUP.RADIUS_UU | 150 | uu | GATED(S1) | 02_Systems/progression.md, T-PRG-08 |
| C.PICKUP.COLLECT_RADIUS_UU | 40 | uu | GATED(S1) | 02_Systems/progression.md, T-PRG-08 |
| C.PICKUP.MAGNET_ACCEL_UU_S2 | 6000 | uu/s2 | GATED(S1) | 02_Systems/progression.md, T-PRG-08 |
| C.PICKUP.MAGNET_SPEED_MAX_UU_S | 1800 | uu/s | GATED(S1) | 02_Systems/progression.md, T-PRG-08 |
| C.PICKUP.GEM_CAP | 1000 | n | GATED(S1) | 02_Systems/progression.md, T-PRG-07 |
| C.PICKUP.GEM_SIZE_UU | 24 | uu | GATED(S1) | 02_Systems/progression.md, Progression/DTPickupRenderer |
| C.PICKUP.CHEST_SIZE_UU | 70 | uu | GATED(S1) | 02_Systems/progression.md, Progression/DTPickupRenderer |
| C.PICKUP.EMISSIVE | 3.0 | ratio | GATED(S1) | 02_Systems/progression.md, 04_Presentation.md |

## Combat and director (02_Systems/combat_core.md, director.md)

Sources: `01_GameDesign` §3.5, §4 and §7 (the 15-minute values after D-71), `balance/model.md`, `balance/pressure_proposal.md`, `balance/playtest5_verification.md`. Timeline rows live in `09_Catalog/timeline_arena1.csv`.

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.HERO.MAX_HP | 100 | hp | GATED(S1) | combat_core.md, T-CMB-10, T-DIR-02 |
| C.HERO.RADIUS_UU | 40 | uu | GATED(S1) | combat_core.md, T-CMB-03, T-CMB-08 |
| C.HERO.MOVE_SPEED_UU_S | 600 | uu/s | GATED(S1) | combat_core.md, T-CMB-03 |
| C.HERO.HURT_IFRAMES_S | 0.4 | s | GATED(S1) | combat_core.md, T-CMB-09 |
| C.HERO.STICK_DEADZONE | 0.2 | ratio | GATED(S1) | combat_core.md, T-CMB-02 |
| C.HERO.INPUT_BUFFER_S | 0.15 | s | GATED(S1) | combat_core.md, T-CMB-02 |
| C.HERO.INTERCESSION_CHARGE_S | 60.0 | s | GATED(S1) | combat_core.md, keepers.md, T-CMB-11 |
| C.DASH.DURATION_S | 0.25 | s | GATED(S1) | combat_core.md, T-CMB-06 |
| C.DASH.DISTANCE_UU | 450 | uu | GATED(S1) | combat_core.md, T-CMB-06 |
| C.DASH.COOLDOWN_S | 3.0 | s | GATED(S1) | combat_core.md, T-CMB-06, T-CMB-07 |
| C.DASH.CHARGES | 1 | n | GATED(S1) | combat_core.md, T-CMB-06, T-CMB-07 |
| C.DIR.BAND_WINDOW_S | 600–780 | s | GATED(S1) | director.md, T-DIR-02, T-DIR-09 |
| C.DIR.NOPICK_DEATH_BAND_S | 480–720 | s | GATED(S1) | director.md, T-DIR-01 |
| C.DIR.BOT_RUNS | 10 | n | GATED(S1) | director.md, T-DIR-01 |
| C.DIR.BREATH_S | 6.0 | s | GATED(S1) | director.md, T-DIR-04 |
| C.DIR.SPAWN_RING_MIN_UU | 1350 | uu | GATED(S1) | director.md, T-DIR-05 |
| C.DIR.SPAWN_RING_MAX_UU | 1700 | uu | GATED(S1) | director.md, T-DIR-05 |
| C.DIR.THREAT_RING_MIN_UU | 1000 | uu | GATED(S1) | director.md, T-DIR-06, T-DIR-07 |
| C.DIR.THREAT_RING_MAX_UU | 1200 | uu | GATED(S1) | director.md, T-DIR-06, T-DIR-07 |
| C.DIR.SHARE_MAX_TANK | 0.1 | ratio | GATED(S1) | director.md, T-DIR-05 |
| C.DIR.SHARE_MAX_RUSHER | 0.1 | ratio | GATED(S1) | director.md, T-DIR-05 |
| C.DIR.SHARE_MAX_TURRET | 0.08 | ratio | GATED(S1) | director.md, T-DIR-05 |
| C.DIR.THREAT_SHARE_MIN | 0.25 | ratio | GATED(S1) | director.md, 09_Catalog/timeline_*.csv, T-DIR-06 |
| C.DIR.DROPIN_LEAD_S | 1.0 | s | GATED(S1) | director.md, T-DIR-07 |
| C.DIR.DROPIN_THREAT_MULT | 2 | x | GATED(S1) | director.md, T-DIR-07 |
| C.DIR.DROPIN_ACTIVE_MAX | 1 | n | GATED(S1) | director.md, T-DIR-08 |
| C.DIR.DROPIN_ACTIVE_MAX_THREAT | 2 | n | GATED(S1) | director.md, T-DIR-08 |
| C.DIR.DROPIN_GAP_MIN_S | 1.5 | s | GATED(S1) | director.md, T-DIR-08 |
| C.DIR.THREAT_ACTIONS_MIN | 4 | /min | GATED(S1) | director.md, T-DIR-09 |
| C.DIR.CHEST_DROP_UU | 400 | uu | GATED(S1) | director.md R-DIR-16, T-DIR-16 |
| C.SCALE.HP_PER_MIN | 0.16 | /min | GATED(S1) | director.md, C.TTK.SWARM_STEERED_MAX, T-DIR-10 |
| C.SCALE.DMG_PER_MIN | 0.0667 | /min | GATED(S1) | director.md, T-DIR-02, T-DIR-10 |
| C.DROP.XP_SWARM | 1 | xp | GATED(S1) | director.md, progression.md, T-DIR-12 |
| C.DROP.XP_TANK | 5 | xp | GATED(S1) | director.md, progression.md, T-DIR-12 |
| C.DROP.XP_RUSHER | 3 | xp | GATED(S1) | director.md, progression.md, T-DIR-12 |
| C.DROP.XP_TURRET | 4 | xp | GATED(S1) | director.md, progression.md, T-DIR-12 |
| C.DROP.XP_ELITE_MULT | 5 | x | GATED(S1) | director.md, T-DIR-12 |

## Boss (02_Systems/boss.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.BOSS.FORM_II_WINS | 1 | n | GATED(S2) | boss.md, R-BOS-01, T-BOS-01 |
| C.BOSS.FORM_III_WINS | 3 | n | GATED(S2) | boss.md, R-BOS-01, T-BOS-01 |
| C.BOSS.IDENTIFY_S | 10 | s | GATED(S1) | boss.md, R-BOS-02, T-BOS-02 |
| C.BOSS.RADIUS_UU | 340 | uu | GATED(S1) | boss.md, R-BOS-03 |
| C.BOSS.CONTACT_DAMAGE | 15 | hp | GATED(S1) | boss.md, R-BOS-03 |
| C.BOSS.PHASE_SHIFT_S | 1.5 | s | GATED(S1) | boss.md, R-BOS-04, T-BOS-04 |
| C.BOSS.BULLET_RADIUS_UU | 16 | uu | GATED(S1) | boss.md, R-BOS-05, R-BOS-07 |
| C.BOSS.LANE_HERO_DIAMETERS | 3 | hero diameters | GATED(S1) | boss.md, R-BOS-07, T-BOS-07 |
| C.BOSS.LANE_MEASURE_UU | 600 | uu | GATED(S1) | boss.md, R-BOS-07, T-BOS-07 |
| C.BOSS.LANE_BEARING_MAX_DEG | 40 | deg | GATED(S1) | boss.md, R-BOS-07, T-BOS-07 |
| C.BOSS.CROSS_RING_DELAY_S | 0.55 | s | GATED(S1) | boss.md, 09_Catalog/boss_warden.csv, R-BOS-08, T-BOS-08 |
| C.BOSS.CROSS_LANE_OFFSET_DEG | 80–120 | deg | GATED(S1) | boss.md, 09_Catalog/boss_warden.csv, R-BOS-08, T-BOS-08 |
| C.BOSS.HP_BASE | 16000 | hp | GATED(S1) | boss.md, R-BOS-09, T-BOS-09 |
| C.BOSS.CALIBRATE_S | 8.0 | s | GATED(S1) | boss.md, R-BOS-09, T-BOS-09 |
| C.BOSS.TARGET_FIGHT_S | 80 | s | GATED(S1) | boss.md, R-BOS-09, R-BOS-10, C.RUN.BOSS_MAX |
| C.BOSS.HP_MIN_MULT | 0.6 | ratio | GATED(S1) | boss.md, R-BOS-09, T-BOS-09 |
| C.BOSS.HP_MAX_MULT | 4.0 | ratio | GATED(S1) | boss.md, R-BOS-09, T-BOS-09 |
| C.BOSS.WALK_SPEED_UU_S | 260 | uu/s | GATED(S1) | boss.md, R-BOS-03 |
| C.BOSS.OPENING_WALK_S | 1.2 | s | GATED(S1) | boss.md, boss_warden.csv, R-BOS-02, T-BOS-02 |
| C.BOSS.CHARGE_RECOVER_S | 0.9 | s | GATED(S1) | boss.md, boss_warden.csv |
| C.BOSS.STANDOFF_HULL_RADII | 2.6 | ratio | GATED(S1) | boss.md, boss_warden.csv, R-BOS-03 |
| C.BOSS.SWEEP_RANGE_UU | 900 | uu | GATED(S1) | boss.md, boss_warden.csv |
| C.BOSS.SPIRAL_LANE_RATE_RATIO | 0.25 | ratio | GATED(S1) | boss.md, boss_warden.csv, R-BOS-07 |
| C.BOSS.ADDS_ALIVE_MAX | 4 | n | GATED(S1) | boss.md, boss_warden.csv |
| C.BOSS.HAZARD_MIN_RADIUS_UU | 1300 | uu | GATED(S1) | boss.md, boss_warden.csv, R-BOS-10, T-BOS-10 |
| C.BOSS.HAZARD_GRACE_S | 0.25 | s | GATED(S1) | boss.md, boss_warden.csv |
| C.BOSS.ROUND_LIFE_S | 9.0 | s | GATED(S1) | boss.md, R-BOS-05, T-BOS-08 |

## Weapons and modifiers (02_Systems/weapons.md, modifiers.md)

Per-weapon level stats and per-modifier parameters live in `09_Catalog/weapons.csv` and `09_Catalog/modifiers.csv` (tagged per row); this table holds the system-wide rules.

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.LOAD.FACTOR_1 | 1.0 | ratio | CORE | weapons.md, T-WPN-03 |
| C.LOAD.FACTOR_2 | 0.7 | ratio | CORE | weapons.md, T-WPN-03 |
| C.LOAD.FACTOR_3 | 0.55 | ratio | CORE | weapons.md, T-WPN-03 |
| C.LOAD.FACTOR_4 | 0.45 | ratio | CORE | weapons.md, T-WPN-03 |
| C.WEAPON.SLOTS | 4 | n | CORE | weapons.md, offers.md, T-WPN-03 |
| C.WEAPON.LEVEL_MAX | 8 | level | CORE | weapons.md, weapons.csv, T-WPN-04 |
| C.WEAPON.SPAWN_RATE_MAX_PER_S | 400 | /s per weapon | GATED(S1) | weapons.md, C.BULLETS.CAP, T-WPN-09 |
| C.WEAPON.CONVERT_MELEE_DAMAGE_RATIO | 0.8 | ratio of volley damage | GATED(S1) | weapons.md, T-WPN-10 |
| C.WEAPON.ARC_DEG | 120 | deg | GATED(S1) | weapons.md, T-WPN-06 |
| C.WEAPON.DRONE_ORBIT_RADIUS_UU | 160 | uu | GATED(S1) | weapons.md, T-WPN-06 |
| C.WEAPON.DRONE_ORBIT_DEG_S | 120 | deg/s | GATED(S1) | weapons.md, T-WPN-06 |
| C.WEAPON.MINE_ARM_S | 0.4 | s | GATED(S1) | weapons.md, T-WPN-13 |
| C.WEAPON.MINE_PULL_TICKS | 6 | n | GATED(S1) | weapons.md, T-WPN-13 (MEASURED reference: six ticks) |
| C.WEAPON.MINE_PULL_TICK_S | 0.2 | s | GATED(S1) | weapons.md, T-WPN-13 |
| C.WEAPON.MINE_PULL_TICK_DAMAGE_RATIO | 0.05 | ratio of mine damage | GATED(S1) | weapons.md, T-WPN-13 (MEASURED reference) |
| C.WEAPON.MINE_PULL_RADIUS_RATIO | 1.5 | ratio of blast radius | GATED(S1) | weapons.md, T-WPN-13 |
| C.WEAPON.MINE_PULL_SPEED_UU_S | 300 | uu/s | GATED(S1) | weapons.md, T-WPN-13 |
| C.WEAPON.MINE_LIVE_MAX | 4 | n per weapon | GATED(S1) | weapons.md, T-WPN-13 |
| C.WEAPON.CHILD_SPEED_UU_S | 900 | uu/s | GATED(S1) | weapons.md, T-WPN-08 (split children of a carrier with no round speed: arcs, mines) |
| C.WEAPON.CHILD_RANGE_UU | 450 | uu | GATED(S1) | weapons.md, T-WPN-08 (their range; life = range / speed) |
| C.WEAPON.CHILD_RADIUS_UU | 10 | uu | GATED(S1) | weapons.md, T-WPN-08 (their radius) |
| C.MOD.SOCKETS_PER_WEAPON | 3 | n | GATED(S1) | modifiers.md, offers.md, T-MOD-04 |
| C.MOD.MIN_COMPATIBLE | 4 | n per weapon | GATED(S1) | modifiers.md, T-MOD-07 |
| C.MOD.SPLIT_GENERATIONS_MAX | 2 | n | CORE | modifiers.md, keepers.md, T-MOD-05 |
| C.MOD.CRIT_CHANCE_CAP | 0.60 | ratio | CORE | modifiers.md, combat_core.md, T-MOD-05 |
