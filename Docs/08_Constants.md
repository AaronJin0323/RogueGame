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
| C.BULLETS.CAP | 6000 | n | CORE | 07_Technical §budgets, T-TEC-10 |
| C.ENEMIES.CAP | 400 | n | CORE | 07_Technical §budgets, T-TEC-10 |
| C.OFFER.CARDS | 5 | n | CORE | offers.md §1 |
| C.OFFER.REROLLS | 2 | n | CORE | offers.md §6 |
| C.OFFER.BANISH | 1 | n | CORE | offers.md §6 |
| C.OFFER.LOCK | 1 | n | CORE | offers.md §6 |
| C.TTK.SWARM_STEERED_MAX | 0.3 | s | CORE | 10_Tuning §1 and §2, director.md R-DIR-18, T-DIR-18, S1_kit row 5 |
| C.DMG_TAKEN.BAND_MIN10_13 | 30–50 | %/min | CORE (a draft; set at the S1 gate, D-046) | director.md, T-DIR-02, 10_Tuning §2 |
| C.DASHES.BAND_MIN10_13 | 6–10 | /min | CORE (a draft; set at the S1 gate, D-046) | combat_core.md, director.md, 10_Tuning §2 |
| C.POWER.KEEPER_SHARE_L16 | ≥ 0.60 | ratio | CORE | 10_Tuning §3, T-KEE-18 (loss, D-047) |
| C.POWER.ARCHETYPE_BAND | ±0.15 | ratio | CORE | 10_Tuning §3, T-KEE-18 (loss, D-047) |
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
| C.PRE.CONTRAST_WATCH_MIN | 3.1 | ratio | GATED(S4) | Tools/contrast_check.py (the margin watch of `Validation/S4_wave1.md` §5 item 4: telegraph and round on lit ground; a package stops under it, the gate does not) |
| C.PRE.LIT_GROUND_Y_WINDOW | [0.106, 0.16] | Y | GATED(S4) | Tools/contrast_check.py (the lit floor's usable window, `Validation/S4_artbook_gaps.md` §4.3: the spread's 5th and 95th percentile are read against it; a reading, not a gate limit) |
| C.PRE.EXPOSURE_DRIFT_MAX | 0.05 | ratio | GATED(S4) | Tools/palette_lint.py, T-PRE-07 (the lit ground's median Y in a frame full of fire against the empty arena's: fixed exposure, R-PRE-07) |
| C.HUD.LOW_HP_FRAC | 0.3 | ratio | GATED(S4) | hud.md R-HUD-05, T-HUD-05 (at or under this share of maximum HP the HP block shows low HP; presentation only) |
| C.HUD.LOW_HP_PULSE_S | 0.8 | s | GATED(S4) | hud.md R-HUD-05, T-HUD-05 (one pulse of the HP bar's fill, on sim time) |
| C.HUD.LOW_HP_PULSE_MIN | 0.45 | ratio | GATED(S4) | hud.md R-HUD-05, T-HUD-05 (the fill's opacity at the bottom of a pulse) |
| C.HUD.TEXT_MIN_PX | 12 | px | GATED(S4) | hud.md R-HUD-08, T-HUD-08 (the shortest line of player text at the smoke window, default text size) |
| C.HUD.TRANSIENT_FLOOR_UU | 1050 | uu | GATED(S4) | hud.md R-HUD-02, T-HUD-02 (the lowest the banner and the toast under the boss row may reach up the hero's column, on the drawn plane; D-080: they are transient and not counted by the circle) |
| C.PRE.DENSITY_LOD_KILLS_PER_S | 20 | /s | GATED(S1) | 04_Presentation §2, T-PRE-06 |
| C.PRE.CUE_EMISSIVE | 1.6 | ratio | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.CUE_Z_UU | 1 | uu | GATED(S1) | 04_Presentation R-PRE-11, Combat/DTWeaponSubsystem |
| C.PRE.AURA_RING_UU | 24 | uu | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.AURA_PULSE_UU | 24 | uu | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.WALL_PLATE_RATIO | 1.6 | ratio | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.HOLD_FRAME_RATIO | 1.9 | ratio | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.HOLD_FRAME_UU | 10 | uu | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.CUE_HUE_EMISSIVE | 1.1 | ratio | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.ZONE_Z_UU | 0.5 | uu | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.ZONE_HALF_UU | 0.4 | uu | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.FROZEN_CUE_RATIO | 2.6 | ratio | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.FROZEN_CUE_UU | 12 | uu | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.CHARGE_RING_RATIO | 2.2 | ratio | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.CHARGE_RING_UU | 14 | uu | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.CHARGE_RING_GAP_UU | 28 | uu | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.CHARGE_TICKS | 12 | n | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.CHARGE_TICK_FILL | 0.6 | ratio | GATED(S1) | 04_Presentation R-PRE-11, T-PRE-11 |
| C.PRE.TELEGRAPH_EDGE_UU | 16 | uu | GATED(S1) | 04_Presentation R-PRE-05, T-ENM-25 |
| C.PRE.TELEGRAPH_RIM_UU | 16 | uu | GATED(S1) | 04_Presentation R-PRE-05, T-ENM-25 |
| C.PRE.TELEGRAPH_FILL_MIN | 0.5 | ratio | GATED(S1) | 04_Presentation R-PRE-05, T-ENM-25 |
| C.PRE.TELEGRAPH_LAYER_UU | 0.5 | uu | GATED(S1) | 04_Presentation R-PRE-05, T-ENM-25 |

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
| C.SAVE.LOCK_WAIT_MS | 250 | ms | CORE (D-042) | meta.md R-MET-11, T-MET-50 |
| C.SAVE.LOCK_RETRY_MS | 5 | ms | CORE (D-042) | meta.md R-MET-11, T-MET-50 |

## 5. Harness caps

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.GATED.DEBT_MAX | 5 | n | CORE | Harness/H03 §4 |
| C.GATE.TEST_FLOOR | 603 | n | CORE (equals the suite's count) | 11_Quality §1, Tools/Gate.ps1, Tools/RunTests.ps1 |
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
| C.ENM.MAX_NEIGHBOURS | 8 | n | GATED(S0) | enemies.md, T-TEC-10 |
| C.ENM.SPAWN_RING_RADIUS_UU | 1600 | uu | GATED(S1) | enemies.md, Debug/DTDebugEnemies |
| C.ENM.KILL_RATE_WINDOW_S | 1.0 | s | GATED(S0) | enemies.md, C.PRE.DENSITY_LOD_KILLS_PER_S |
| C.ENEMIES.OPTIC_EMISSIVE | 14.0 | ratio | GATED(S1) | enemies.md, R-ENM-24, T-ENM-24 |
| C.ENEMIES.OPTIC_SCALE | 0.35 | ratio | GATED(S1) | enemies.md R-ENM-24 (optic size as a fraction of the hull; grey box) |
| C.ENEMIES.TELEGRAPH_EMISSIVE | 1.1 | ratio | GATED(S1) | enemies.md, R-ENM-13, 04_Presentation.md |
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
| C.ROLE.RUSHER_ARRIVAL_OVERSHOOT_UU | 200 | uu | GATED(S1) | enemies.md, R-ENM-13, T-ENM-13 |
| C.ROLE.TURRET_HP | 40 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-09 |
| C.ROLE.TURRET_RADIUS_UU | 48 | uu | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-12 |
| C.ROLE.TURRET_CONTACT_DAMAGE | 6 | hp | GATED(S1) | enemies.md, 09_Catalog/enemies.csv, R-ENM-14 |
| C.ROLE.TURRET_HEAD_TURN_DEG_S | 140 | deg/s | GATED(S1) | enemies.md, R-ENM-12, T-ENM-12 |
| C.ROLE.TURRET_FIRE_RANGE_UU | 1250 | uu | GATED(S1) | enemies.md, R-ENM-12, T-ENM-12 |
| C.ROLE.TURRET_TELL_S | 0.75 | s | GATED(S1) | enemies.md, R-ENM-12, R-ENM-13, T-ENM-12 |
| C.ROLE.TURRET_PERIOD_S | 2.0 | s | GATED(S1) | enemies.md, R-ENM-12, T-ENM-12 |
| C.ROLE.ELITE_HP_MULT | 5 | x | MEASURED(reference balance/model.md) | enemies.md R-ENM-09, elites.md, director.md, T-ENM-09 |
| C.AFFIX.ARMORED_DAMAGE_TAKEN | 0.6 | ratio | MEASURED(reference balance/model.md) | elites.md, R-ELT-03, T-ELT-03 |
| C.AFFIX.SWIFT_SPEED_MULT | 1.4 | x | GATED(S2) | elites.md, R-ELT-04, T-ELT-04 |
| C.AFFIX.VOLATILE_RADIUS_UU | 240 | uu | GATED(S2) | elites.md, R-ELT-05, T-ELT-05 |
| C.AFFIX.VOLATILE_LEAD_S | 0.8 | s | GATED(S2) | elites.md, R-ELT-05, T-ELT-05, 09_Catalog/elites.csv |
| C.AFFIX.VOLATILE_DAMAGE | 15 | hp | GATED(S2) | elites.md, R-ELT-05, T-ELT-05 |
| C.AFFIX.SPLITTER_COUNT | 3 | n | GATED(S2) | elites.md, R-ELT-06, T-ELT-06 |
| C.AFFIX.VAMPIRIC_RANGE_UU | 420 | uu | GATED(S2) | elites.md, R-ELT-07, T-ELT-07 |
| C.AFFIX.VAMPIRIC_LINK_S | 1.0 | s | GATED(S2) | elites.md, R-ELT-07, T-ELT-07, 09_Catalog/elites.csv |
| C.AFFIX.VAMPIRIC_TICK_S | 1.0 | s | GATED(S2) | elites.md, R-ELT-07, T-ELT-07 |
| C.AFFIX.VAMPIRIC_DRAIN_DPS | 4 | hp_per_s | GATED(S2) | elites.md, R-ELT-07, T-ELT-07 |
| C.AFFIX.VAMPIRIC_HEAL_RATIO | 0.05 | ratio | GATED(S2) | elites.md, R-ELT-07, T-ELT-07 |
| C.AFFIX.VAMPIRIC_CABLE_UU | 6 | uu | GATED(S2) | elites.md, R-ELT-07, T-ELT-07 |
| C.AFFIX.SHIELD_HITS | 16 | n | GATED(S2) | elites.md, R-ELT-08, T-ELT-08 |
| C.AFFIX.SHIELD_PLATES | 6 | n | GATED(S2) | elites.md, R-ELT-11, T-ELT-11 |
| C.ELITE.FAVORED_WEIGHT | 3 | x | GATED(S2) | elites.md, R-ELT-02, T-ELT-02, 09_Catalog/timeline_arena1.csv |
| C.ELITE.DEBUG_AFFIXES | 1 | n | GATED(S2) | elites.md, Debug/DTDebugEnemies |
| C.ELITE.DRAW_SCALE | 1.3 | x | GATED(S2) | elites.md, R-ELT-11, T-ELT-11 |
| C.ELITE.LIGHT_EMISSIVE | 22.0 | ratio | GATED(S2) | elites.md, R-ELT-10, T-ELT-10, T-PRE-05 |
| C.ELITE.BLINK_STEP_S | 0.1 | s | GATED(S2) | elites.md, R-ELT-10, T-ELT-10 |
| C.ELITE.PART_RATIO | 0.35 | ratio | GATED(S2) | elites.md, R-ELT-11, T-ELT-11 |
| C.ELITE.SHIELD_ORBIT_DEG_S | 90 | deg_per_s | GATED(S2) | elites.md, R-ELT-11 |
| C.ELITE.LABELS_MAX | 12 | n | GATED(S2) | elites.md, R-ELT-12, T-ELT-12 |
| C.ENEMY_FIRE.CAP | 1024 | n | GATED(S1) | enemies.md, R-ENM-15, T-ENM-15, T-TEC-10 |
| C.ENEMY_FIRE.SHOT_SPEED_UU_S | 650 | uu/s | GATED(S1) | enemies.md, R-ENM-12 |
| C.ENEMY_FIRE.SHOT_RADIUS_UU | 16 | uu | GATED(S1) | enemies.md, R-ENM-16, T-ENM-20 |
| C.ENEMY_FIRE.SHOT_DAMAGE | 5 | hp | GATED(S1) | enemies.md, R-ENM-16 |
| C.ENEMY_FIRE.SHOT_LIFE_S | 4.0 | s | GATED(S1) | enemies.md, R-ENM-15 |
| C.ENEMY_FIRE.BURST_SHOTS | 3 | n | GATED(S1) | enemies.md, R-ENM-12, R-ENM-20 |
| C.ENEMY_FIRE.BURST_GAP_S | 0.12 | s | GATED(S1) | enemies.md, R-ENM-12 |
| C.ENEMY_FIRE.FAN_COUNT | 4 | n | GATED(S1) | enemies.md, R-ENM-19, R-ENM-20 |
| C.ENEMY_FIRE.FAN_SPREAD_DEG | 14 | deg | GATED(S1) | enemies.md, R-ENM-20, T-ENM-20 |
| C.ENEMY_FIRE.EMISSIVE | 3.5 | ratio | GATED(S1) | enemies.md, R-ENM-17, 04_Presentation.md |
| C.ENEMY_FIRE.CORE_RADIUS_RATIO | 0.6 | ratio | GATED(S1) | enemies.md, R-ENM-17, Tools/create_arena.py |
| C.ENEMY_FIRE.DRAW_RADIUS_RATIO | 1.8 | ratio | GATED(S1) | enemies.md, R-ENM-17, T-PRE-05, Enemies/DTEnemySubsystem |
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

## Tracer art (02_Systems/tracer_art.md)

S4 wave 1 package D (D-043): presentation only, no reader in the sim.

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.TRC.HEAD_WIDTH_RATIO | 0.85 | ratio | GATED(S4) | tracer_art.md R-TRC-01, T-TRC-01 (a player head's drawn half-width over the round's hit radius) |
| C.TRC.HEAD_STRETCH | 1.8 | ratio | GATED(S4) | tracer_art.md R-TRC-01, T-TRC-01 (a moving head's half-length over its half-width, along the flight) |
| C.TRC.CLEAR_ZONE_RATIO | 2.5 | ratio | GATED(S4) | tracer_art.md R-TRC-01, R-TRC-02, T-TRC-01, Presentation/DTTracerArt (the hero's clear zone: no head or tail of the player's own within this many hero radii of the hero; 0 draws every round, D-068) |
| C.TRC.TAIL_S | 0.08 | s | GATED(S4) | tracer_art.md R-TRC-02, T-TRC-02 (a player tail is the path of this many seconds of flight) |
| C.TRC.TAIL_MIN_RATIO | 6.0 | ratio | GATED(S4) | tracer_art.md R-TRC-02, T-TRC-02 (shortest tail, in drawn head half-widths, so a slow round still reads as trailed) |
| C.TRC.TAIL_MAX_UU | 150 | uu | GATED(S4) | tracer_art.md R-TRC-02, T-TRC-02 (longest tail) |
| C.TRC.TAIL_WIDTH_RATIO | 0.8 | ratio | GATED(S4) | tracer_art.md R-TRC-02, T-TRC-02 (the tail's half-width at the head over the head's half-width) |
| C.TRC.TAIL_EMISSIVE | 1.5 | ratio | GATED(S4) | tracer_art.md R-TRC-02, T-TRC-06 (undimmed tail strength at the head end, times the token) |
| C.TRC.TAIL_TIP_SHARE | 0.5 | ratio | GATED(S4) | tracer_art.md R-TRC-03, Tools/assets/tracer_materials.py (the share of the tail, from its tip, that takes the second Keeper's token) |
| C.TRC.TAIL_Z_UU | 1.25 | uu | GATED(S4) | tracer_art.md R-TRC-05, T-TRC-05, Tools/assets/tracer_materials.py (the height tails, trails and flashes are drawn at: over cues, under every telegraph layer) |
| C.TRC.DIM_START_ROUNDS | 150 | n | GATED(S4) | tracer_art.md R-TRC-06, T-TRC-06 (live player rounds up to which player fire draws undimmed) |
| C.TRC.DIM_FULL_ROUNDS | 2400 | n | GATED(S4) | tracer_art.md R-TRC-06, T-TRC-06 (live player rounds from which player fire draws fully dimmed) |
| C.TRC.DIM_EMISSIVE_MIN | 0.5 | ratio | GATED(S4) | tracer_art.md R-TRC-06, T-TRC-06 (fully dimmed head emissive over C.BULLETS.EMISSIVE_PLAYER) |
| C.TRC.DIM_TAIL_MIN | 0.4 | ratio | GATED(S4) | tracer_art.md R-TRC-06, T-TRC-06 (fully dimmed tail strength over C.TRC.TAIL_EMISSIVE) |
| C.TRC.DIM_WIDTH_MIN | 0.7 | ratio | GATED(S4) | tracer_art.md R-TRC-06, T-TRC-06 (fully dimmed tail width over its undimmed width) |
| C.TRC.ENEMY_TRAIL_S | 0.07 | s | GATED(S4) | tracer_art.md R-TRC-04, T-TRC-04 (an enemy round's trail is the path of this many seconds of flight) |
| C.TRC.ENEMY_TRAIL_WIDTH_RATIO | 0.7 | ratio | GATED(S4) | tracer_art.md R-TRC-04, T-TRC-04 (the trail's half-width at the round over the round's drawn radius) |
| C.TRC.ENEMY_TRAIL_EMISSIVE | 0.5 | ratio | GATED(S4) | tracer_art.md R-TRC-04, T-TRC-04 (trail strength, times Pal.Enemy.Bullet; the same at every density) |
| C.TRC.ENEMY_TRAIL_OPACITY | 0.9 | ratio | GATED(S4) | tracer_art.md R-TRC-04, T-TRC-04 (trail opacity at the round; under the round's rim in the frame) |
| C.TRC.MUZZLE_S | 0.1 | s | GATED(S4) | tracer_art.md R-TRC-07, T-TRC-07 (how long a muzzle flash lasts after its round is fired) |
| C.TRC.MUZZLE_LENGTH_RATIO | 5.0 | ratio | GATED(S4) | tracer_art.md R-TRC-07, T-TRC-07 (a fresh flash's length over the round's drawn radius) |
| C.TRC.MUZZLE_WIDTH_RATIO | 1.4 | ratio | GATED(S4) | tracer_art.md R-TRC-07, T-TRC-07 (a fresh flash's half-width over the round's drawn radius) |
| C.TRC.MUZZLE_EMISSIVE | 2.0 | ratio | GATED(S4) | tracer_art.md R-TRC-07, T-TRC-07 (flash strength, times Pal.Enemy.Muzzle) |
| C.TRC.MUZZLE_OPACITY | 0.8 | ratio | GATED(S4) | tracer_art.md R-TRC-07, T-TRC-07 (flash opacity at the muzzle) |

## Impact art (02_Systems/impact_art.md)

S4 wave 1 package E (D-043): presentation only, no reader in the sim.

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.IMP.DEBRIS_Z_UU | 1.4 | uu | GATED(S4) | impact_art.md R-IMP-04, T-IMP-04, Tools/assets/fx_materials.py (height of debris) |
| C.IMP.FLASH_Z_UU | 1.45 | uu | GATED(S4) | impact_art.md R-IMP-04, T-IMP-04, Tools/assets/fx_materials.py (height of sparks, flashes and the hero ring) |
| C.IMP.SPARK_S | 0.10 | s | GATED(S4) | impact_art.md R-IMP-05, T-IMP-05 (spark life) |
| C.IMP.SPARK_LENGTH_UU | 70 | uu | GATED(S4) | impact_art.md R-IMP-05, T-IMP-05 (spark length) |
| C.IMP.SPARK_WIDTH_UU | 44 | uu | GATED(S4) | impact_art.md R-IMP-05, T-IMP-05 (spark width) |
| C.IMP.SPARK_STREAK_RATIO | 0.3 | ratio | GATED(S4) | impact_art.md R-IMP-05, T-IMP-05 (streak width at the root, over the spark's width) |
| C.IMP.SPARK_START_RATIO | 0.4 | ratio | GATED(S4) | impact_art.md R-IMP-05, T-IMP-05 (spark size at birth) |
| C.IMP.SPARK_EMISSIVE | 3.0 | ratio | GATED(S4) | impact_art.md R-IMP-05, T-IMP-05 (spark strength at birth) |
| C.IMP.SPARKS_PER_STEP_MAX | 32 | n | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (sparks raised in a step in `PerKill`) |
| C.IMP.SPARKS_DENSE_PER_STEP_MAX | 12 | n | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (sparks raised in a step in `Crowd`) |
| C.IMP.SPARK_LIVE_MAX | 192 | n | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (live sparks) |
| C.IMP.KILL_FLASH_S | 0.12 | s | GATED(S4) | impact_art.md R-IMP-06, T-IMP-06 (kill flash life) |
| C.IMP.KILL_FLASH_START_RATIO | 0.8 | ratio | GATED(S4) | impact_art.md R-IMP-06, T-IMP-06 (kill flash radius at birth, drawn radii) |
| C.IMP.KILL_FLASH_END_RATIO | 1.7 | ratio | GATED(S4) | impact_art.md R-IMP-06, T-IMP-06 (kill flash radius at its end, drawn radii) |
| C.IMP.KILL_FLASH_EMISSIVE | 2.5 | ratio | GATED(S4) | impact_art.md R-IMP-06, T-IMP-06 (kill flash strength at birth) |
| C.IMP.FLASH_RADIUS_MAX_UU | 260 | uu | GATED(S4) | impact_art.md R-IMP-06, R-IMP-08, T-IMP-06 (widest flash) |
| C.IMP.FLASH_LIVE_MAX | 72 | n | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (live flashes) |
| C.IMP.AGG_CELL_UU | 400 | uu | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (aggregation cell) |
| C.IMP.AGG_PER_STEP_MAX | 6 | n | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (aggregated flashes raised in a step) |
| C.IMP.DENSITY_CROWD_OFF_KILLS_PER_S | 16 | /s | GATED(S4) | impact_art.md R-IMP-08, R-IMP-11, T-IMP-08 (rate under which `Crowd` is left) |
| C.IMP.DENSITY_FLOOD_KILLS_PER_S | 60 | /s | GATED(S4) | impact_art.md R-IMP-08, R-IMP-11, T-IMP-08 (rate above which `Flood` is entered) |
| C.IMP.DENSITY_FLOOD_OFF_KILLS_PER_S | 50 | /s | GATED(S4) | impact_art.md R-IMP-08, R-IMP-11, T-IMP-08 (rate under which `Flood` is left) |
| C.IMP.AGG_FLOOD_CELL_UU | 800 | uu | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (aggregation cell in `Flood`) |
| C.IMP.AGG_FLOOD_PER_STEP_MAX | 2 | n | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (aggregated flashes raised in a step in `Flood`) |
| C.IMP.DEBRIS_S | 0.45 | s | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (debris life) |
| C.IMP.DEBRIS_SIZE_UU | 28 | uu | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07, Tools/blender/debris.py (debris piece size) |
| C.IMP.DEBRIS_RADIUS_PER_PIECE_UU | 11 | uu | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (drawn radius per piece) |
| C.IMP.DEBRIS_PIECES_MIN | 3 | n | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (fewest pieces) |
| C.IMP.DEBRIS_PIECES_MAX | 8 | n | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (most pieces) |
| C.IMP.DEBRIS_SPEED_UU_S | 520 | uu/s | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (debris speed at birth) |
| C.IMP.DEBRIS_SPEED_VARIATION | 0.4 | ratio | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (debris speed variation) |
| C.IMP.DEBRIS_DRAG_S | 0.15 | s | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (debris slow-down time constant) |
| C.IMP.DEBRIS_SPREAD_DEG | 70 | deg | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (debris spread to either side) |
| C.IMP.DEBRIS_SHRINK_SHARE | 0.4 | ratio | GATED(S4) | impact_art.md R-IMP-07, T-IMP-07 (share of life a piece shrinks over) |
| C.IMP.DEBRIS_LIVE_MAX | 160 | n | GATED(S4) | impact_art.md R-IMP-08, T-IMP-08 (live debris pieces) |
| C.IMP.HERO_FLASH_S | 0.25 | s | GATED(S4) | impact_art.md R-IMP-09, T-IMP-09 (hero ring life) |
| C.IMP.HERO_FLASH_START_RATIO | 2.0 | ratio | GATED(S4) | impact_art.md R-IMP-09, T-IMP-09 (hero ring radius at birth, hero radii) |
| C.IMP.HERO_FLASH_END_RATIO | 3.0 | ratio | GATED(S4) | impact_art.md R-IMP-09, T-IMP-09 (hero ring radius at its end, hero radii) |
| C.IMP.HERO_FLASH_THICK_UU | 24 | uu | GATED(S4) | impact_art.md R-IMP-09, T-IMP-09 (hero ring thickness) |
| C.IMP.HERO_FLASH_EMISSIVE | 2.5 | ratio | GATED(S4) | impact_art.md R-IMP-09, T-IMP-09 (hero ring strength at birth) |
| C.IMP.REDUCED_EMISSIVE_RATIO | 0.4 | ratio | GATED(S4) | impact_art.md R-IMP-09, T-IMP-09 (strength with Screen flashes off) |

## Arena and measurement

Stress arena `L_Arena_Test` (built by `Tools/create_arena.py`), its camera, and the `smoke` / `perf` tools. Light and exposure become `C.PRE.*` keys when the two weathers are measured (`04_Presentation §7`).

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.ARENA.SUN_ELEVATION_DEG | 20 | deg | GATED(S0) | Tools/create_arena.py, 04_Presentation §4 |
| C.ARENA.SUN_YAW_DEG | -60 | deg | GATED(S0) | Tools/create_arena.py |
| C.ARENA.SUN_LUX | 11 | lux | GATED(S0) | Tools/create_arena.py, 04_Presentation.md, T-PRE-05 |
| C.ARENA.SKY_LIGHT_INTENSITY | 1.0 | scale | GATED(S0) | Tools/create_arena.py |
| C.ARENA.EXPOSURE_EV100 | 0.0 | EV100 | GATED(S0) | Tools/create_arena.py, 04_Presentation §4 (fixed: min = max) |
| C.ARENA.BLOOM_THRESHOLD | 1.0 | scale | GATED(S0) | Tools/create_arena.py, 04_Presentation §4 |
| C.ARENA.CAMERA_PITCH_DEG | 60 | deg below horizon | GATED(S1) | Game/DTCameraPawn |
| C.ARENA.CAMERA_FOV_DEG | 50 | deg (horizontal) | GATED(S1) | Game/DTCameraPawn |
| C.ARENA.CAMERA_FRAME_RADIUS_UU | 1258.2 | uu | GATED(S0) | 06_UX.md R-UX-01, Game/DTCameraPawn, Tools/session_read.py (D-036 and D-080: every attack starts on screen; the tightest frame whose reach on the drawn plane is equal up and down at the fairness line, counted to the HUD; the frame is fixed) |
| C.ARENA.CAMERA_AIM_SHIFT_UU | 140.6 | uu toward the camera | GATED(S0) | 06_UX.md R-UX-01, Game/DTCameraPawn, Tools/session_read.py (D-080: the camera aims this far from the hero toward itself, so the hero stands above the centre and the reach up equals the reach down; a constant, never moved in a run) |
| C.SMOKE.SECONDS | 10 | s | CORE | Tools/Smoke.ps1 |
| C.SMOKE.GOLDEN_MEAN_DIFF_MAX | 12.0 | 8-bit level | GATED(S0) | Tools/image_check.py, Tools/Smoke.ps1, Tools/image_check.py --exposure (T-PRE-07) |
| C.SMOKE.IMAGE_STDDEV_MIN | 4.0 | 8-bit level | CORE | Tools/image_check.py (not blank / uniform) |
| C.PERF.WARMUP_S | 10 | s | CORE | Tools/Perf.ps1, T-TEC-10 |
| C.PERF.CAPTURE_FRAMES | 1800 | n | CORE | Tools/Perf.ps1, T-TEC-10 |
| C.PERF.LOAD_MIN_RATIO | 0.95 | ratio | CORE | Tools/Perf.ps1, T-TEC-10 (median live bullets and enemies must reach this share of the caps, or the measurement is void) |
| C.PERF.GPU_PROXY_4080_TO_4070 | 1.6 | x | GATED(S4) | 07_Technical §5, Tools/Perf.ps1, T-TEC-10 (D-011) |
| C.PERF.S2_WARMUP_S | 20 | s | CORE | Tools/Perf.ps1 -Scene S2 (the crowd arrives and takes its statuses before the capture) |
| C.PERF.S2_ELITES | 24 | n | CORE | Debug/DTDebugPerfScene (elites held in the crowd, the six affixes in turn) |
| C.PERF.S2_CROWD_HP_SCALE | 1000 | x | CORE | Debug/DTDebugPerfScene (a scene setting, no game number: `Docs/Validation/S2_perf.md`) |
| C.PERF.S2_HERO_ORBIT_UU | 900 | uu | CORE | Debug/DTDebugPerfScene |
| C.PERF.S2_FILL_RATIO | 0.97 | ratio | CORE | Debug/DTDebugPerfScene (of C.BULLETS.CAP, weapon rounds included) |
| C.PERF.S2_STEAM_MIN_PER_S | 6 | per_s | CORE | Tools/Perf.ps1 -Scene S2 (the heaviest arm of `S2_duos.md`) |
| C.PERF.S2_PAIRS_MIN | 12 | n | CORE | Tools/Perf.ps1 -Scene S2 (pairs that fired in the window, of 15) |
| C.PERF.S2_ELITES_MIN | 12 | n | CORE | Tools/Perf.ps1 -Scene S2 (median live elites) |
| C.PERF.S4_TURRETS | 24 | n | CORE | Debug/DTDebugPerfScene (Turrets held so a tell is always live: `Docs/Validation/S4_p0.md`) |
| C.PERF.S4_TURRET_RING_UU | 350 | uu | CORE | Debug/DTDebugPerfScene (their ring round the origin: C.ROLE.TURRET_FIRE_RANGE_UU less C.PERF.S2_HERO_ORBIT_UU, so every point of the orbit is in range) |
| C.PERF.S4_TELEGRAPHS_MIN | 4 | n | CORE | Tools/Perf.ps1 -Scene S4 (median live telegraphs in the window) |
| C.PERF.S4_TRAILED_MIN_RATIO | 0.9 | ratio | CORE | Tools/Perf.ps1 -Scene S4 (median rounds the trail draw takes, of the median live rounds) |
| C.ARN.SLAB_COURSE_UU | 320 | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-01, Tools/blender/env_arena.py (width of one course of slabs on the plate) |
| C.ARN.SLAB_LENGTH_UU | [320, 760] | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-01, Tools/blender/env_arena.py (shortest and longest slab in a course) |
| C.ARN.JOINT_HALF_WIDTH_UU | 6 | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-01, T-ARN-01 (half the width of a joint: each slab's chamfer) |
| C.ARN.JOINT_DEPTH_UU | 0.25 | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-01, T-ARN-01 (how far a joint falls under the plate's top: the whole relief) |
| C.ARN.APRON_STEPS | 5 | n | GATED(S4) | 02_Systems/arena_art.md R-ARN-02, Tools/blender/env_arena.py (steps of the apron) |
| C.ARN.APRON_RISE_UU | 50 | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-02, R-ARN-04 (height of one step) |
| C.ARN.FALL_STEPS | 3 | n | GATED(S4) | 02_Systems/arena_art.md R-ARN-04, Tools/create_arena.py (the step at whose depth the stone's base colour reaches `Pal.Ground.Shadow`) |
| C.ARN.SHADOW_FLOOR | 0.35 | scale | GATED(S4) | 02_Systems/arena_art.md R-ARN-05, Tools/create_arena.py (emissive of the apron's stone, as a share of `Pal.Ground.Shadow`: stone in full shadow stays off the clear colour) |
| C.ARN.APRON_TREAD_UU | 240 | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-02 (depth of one step's tread) |
| C.ARN.APRON_SKIRT_DROP_UU | 100 | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-02 (the last fall, from the lowest step to the skirt) |
| C.ARN.APRON_REACH_UU | 9000 | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-02, T-ARN-02 (how far the skirt runs past the plate on every side) |
| C.ARN.FRAME_RADIUS_MAX_UU | 2000 | uu | GATED(S4) | 02_Systems/arena_art.md R-ARN-02, T-ARN-02, Debug/DTDebugArena (the widest frame the apron is sized for: the frame D-036 reverts to) |
| C.ARN.ASPECT_MAX | 3.5556 | ratio | GATED(S4) | 02_Systems/arena_art.md R-ARN-02, T-ARN-02 (the widest window the apron is sized for, 32:9) |
| C.ARN.LAYOUT_SEED | 4043 | n | GATED(S4) | 02_Systems/arena_art.md R-ARN-01, Tools/blender/env_arena.py (seed of the slab lengths: the generator's own stream, never a sim stream) |
| C.ARN.VOID_MAX_LEVEL | 2 | level | GATED(S4) | 02_Systems/arena_art.md R-ARN-05, Tools/arena_check.py (a pixel at or under this level in every channel is the clear colour) |


## Telemetry (02_Systems/telemetry.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.TEL.INTERVAL_S | 60.0 | s (sim time) | CORE | telemetry.md, Telemetry/DTTelemetryCore, T-TEL-01 |
| C.TEL.RATE_UNIT_S | 60.0 | s | CORE | telemetry.md (every `*_per_min` column is a count per this many seconds) |
| C.TEL.TTK_PERCENTILE | 0.50 | ratio | CORE | telemetry.md (`swarm_ttk_s` column), C.TTK.SWARM_STEERED_MAX |
| C.TEL.NEAREST_PERCENTILE | 0.10 | ratio | CORE | telemetry.md (`nearest_enemy_p10_uu` column) |
| C.TEL.HERO_POS_INTERVAL_S | 2.5 | s (sim time) | CORE | telemetry.md (`hero_pos` record), Telemetry/DTTelemetryCore, T-TEL-10 |
| C.TEL.POWER_CHECK_LEVEL | 16 | level | CORE | Tools/probe.py, Tools/power_split.py, C.POWER.KEEPER_SHARE_L16, T-KEE-18 |

## Keepers (02_Systems/keepers.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.KEEPER.ARCHETYPES_PER_KEEPER | 3 | n | CORE | keepers.md R-KEE-01, expression.csv, T-KEE-01 |
| C.KEEPER.BLESSINGS_PER_ARCHETYPE | 4 | n | CORE | keepers.md R-KEE-01 (Key, Amp1, Amp2, Trait), expression.csv, T-KEE-01 |
| C.KEEPER.TRAIT_PREREQ_CARDS | 2 | n | GATED(S1) | keepers.md R-KEE-04, offers.md, T-KEE-04 |
| C.KEEPER.PACKAGE_GAIN_BAND | 2.0–2.4 | × crowd DPS | GATED(S1) | keepers.md R-KEE-18, T-KEE-18 (reference D-57) |
| C.KEEPER.PULL_INTERVAL_S | 2.0 | s | GATED(S1) | keepers.md R-KEE-21, T-KEE-21 (D-016) |
| C.KEEPER.SCREEN_SCOPE_RADIUS_UU | 2000 | uu | GATED(S0) | keepers.md (`scope=Screen`: Echo's whole-screen strike, the Storm net, R-KEE-27's hunter reach, R-KEE-29's payday, R-KEE-33's sun, R-KEE-35's Shade mark and Sun Lance length), Combat/DTWeaponSim, Combat/DTWeaponSimKeepers, Keepers/DTKeeperSim (split from `C.ARENA.CAMERA_FRAME_RADIUS_UU` by D-036 at the same value; not tied to the frame) |
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
| C.STATUS.MARK_S | 5.0 | s | GATED(S2) | keepers.md, R-KEE-24, T-KEE-24 |
| C.STATUS.BLIND_S | 0.30 | s | GATED(S2) | keepers.md, R-KEE-25, T-KEE-25 |
| C.REACTION.ENEMY_COOLDOWN_S | 1.0 | s | GATED(S2) | reactions.md, R-REA-05, T-REA-05 |
| C.REACTION.FX_PER_S_MAX | 30 | per_s | GATED(S2) | reactions.md, R-REA-11, T-REA-11 |
| C.REACTION.CUE_S | 0.35 | s | GATED(S2) | reactions.md, R-REA-11, T-REA-11 |
| C.KEEPER.SEEK_FALLBACK_DMG | 0.25 | ratio | GATED(S2) | keepers.md, R-KEE-26, T-KEE-26 |
| C.KEEPER.RICOCHET_FALLBACK_RATE | 0.15 | ratio | GATED(S2) | keepers.md, R-KEE-34, T-KEE-34 |
| C.INTERCESSION.SHADE_DURATION_S | 6.0 | s | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.SHADE_TURN_DEG_S | 90 | deg_per_s | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.SHADE_EXECUTE_BELOW | 0.30 | ratio | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.RADIANCE_CHARGE_S | 0.5 | s | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.RADIANCE_BEAM_S | 2.0 | s | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.RADIANCE_BEAM_TICK_S | 0.1 | s | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.RADIANCE_BEAM_WIDTH_UU | 120 | uu | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.RADIANCE_BEAM_DMG | 0.5 | ratio | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.RADIANCE_FAN_DEG | 60 | deg | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.QUICKSILVER_DURATION_S | 4.0 | s | GATED(S2) | keepers.md, R-KEE-35, T-KEE-35 |
| C.INTERCESSION.QUICKSILVER_TIME_SCALE | 1.0 | ratio | GATED(S2) | keepers.md, R-KEE-37, T-KEE-37 |
| C.INTERCESSION.RETUNE_STEP_MAX | 2.0 | × per pass | GATED(S1) | keepers.md R-KEE-15, T-KEE-15 (weak ×2 up, strong ×0.5 down) |

## Offers and progression (02_Systems/offers.md, progression.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.OFFER.SECOND_KEEPER_LEVEL | 8 | level | CORE | 02_Systems/offers.md, 02_Systems/progression.md, T-OFF-08 |
| C.OFFER.REFORGING_LEVEL | 16 | level | CORE | 02_Systems/offers.md, 02_Systems/progression.md, T-OFF-09 |
| C.OFFER.REFORGING_CARDS | 3 | n | CORE | 02_Systems/offers.md, T-OFF-09 |
| C.OFFER.REFORGE_EVOLVE_MIN_LEVEL | 6 | level | GATED(S2) | 02_Systems/reforging.md, T-RFG-02 |
| C.REFORGE.POWER_MIN | 1.3 | ratio | GATED(S2) | 02_Systems/reforging.md |
| C.REFORGE.POWER_MAX | 1.8 | ratio | GATED(S2) | 02_Systems/reforging.md |
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
| C.OFFER.WEIGHT_DUO | 1.5 | x | GATED(S2) | reactions.md, R-REA-08, T-REA-08 |
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
| C.OFFER.KEYWORD_NOTES_MAX | 4 | n | GATED(S1) | 02_Systems/offers.md, R-OFF-13, T-OFF-17 |
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
| C.PICKUP.CHEST_RESTORE_HP_RATIO | 0.15 | ratio | GATED(S1) | 02_Systems/progression.md, R-PRG-09, T-PRG-09 |

## Combat and director (02_Systems/combat_core.md, director.md)

Sources: `01_GameDesign` §3.5, §4 and §7 (the 15-minute values after D-71), `balance/model.md`, `balance/pressure_proposal.md`, `balance/playtest5_verification.md`. Timeline rows live in `09_Catalog/timeline_arena1.csv`.

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.HERO.MAX_HP | 100 | hp | GATED(S1) | combat_core.md, T-CMB-10, T-DIR-02 |
| C.HUD.BANNER_S | 2.5 | s | GATED(S1) | director.md, T-DIR-17 |
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
| C.HERO_ART.DRAW_SCALE | 1.16 | ratio | GATED(S4) | hero_art.md R-HAR-03, Presentation/DTHeroPose, Tools/hero_outline.py (the figure is drawn at this scale: the smallest read that holds C.HERO_ART.ARMOUR_PX_MIN in the gate frame) |
| C.HERO_ART.WAIST_Z_UU | 105 | uu | GATED(S4) | hero_art.md R-HAR-05, Presentation/DTHeroPose, Tools/blender/hero_reclaimer.py (joint: the upper body leans about it; before the draw scale) |
| C.HERO_ART.NECK_Z_UU | 156 | uu | GATED(S4) | hero_art.md R-HAR-05, as WAIST_Z_UU (joint: the head levels about it) |
| C.HERO_ART.SHOULDER_Z_UU | 144 | uu | GATED(S4) | hero_art.md R-HAR-05, as WAIST_Z_UU (joint: an arm swings about it) |
| C.HERO_ART.SHOULDER_Y_UU | 33 | uu | GATED(S4) | hero_art.md R-HAR-05, as WAIST_Z_UU (the shoulder, and the pauldron over it, to either side) |
| C.HERO_ART.HIP_Z_UU | 92 | uu | GATED(S4) | hero_art.md R-HAR-05, as WAIST_Z_UU (joint: a leg swings about it) |
| C.HERO_ART.HIP_Y_UU | 15 | uu | GATED(S4) | hero_art.md R-HAR-05, as WAIST_Z_UU (a hip to either side) |
| C.HERO_ART.MOVE_LEAN_DEG | 10 | deg | GATED(S4) | hero_art.md R-HAR-05, T-HAR-05 (upper body lean at full speed) |
| C.HERO_ART.MOVE_STRIDE_DEG | 22 | deg | GATED(S4) | hero_art.md R-HAR-05, T-HAR-05 (each leg off vertical at full speed: one held stride) |
| C.HERO_ART.MOVE_ARM_DEG | 18 | deg | GATED(S4) | hero_art.md R-HAR-05, T-HAR-05 (each arm against its leg at full speed) |
| C.HERO_ART.DASH_LEAN_DEG | 32 | deg | GATED(S4) | hero_art.md R-HAR-05, T-HAR-05 (upper body lean along the dash) |
| C.HERO_ART.DASH_LEG_DEG | 30 | deg | GATED(S4) | hero_art.md R-HAR-05, T-HAR-05 (both legs trailing) |
| C.HERO_ART.DASH_ARM_DEG | 40 | deg | GATED(S4) | hero_art.md R-HAR-05, T-HAR-05 (both arms trailing) |
| C.HERO_ART.DASH_DROP_UU | 10 | uu | GATED(S4) | hero_art.md R-HAR-05, T-HAR-05 (hips lowered while dashing; before the draw scale) |
| C.HERO_ART.HEAD_LEVEL_RATIO | 0.5 | ratio | GATED(S4) | hero_art.md R-HAR-05, T-HAR-05 (the share of the lean the head takes back) |
| C.HERO_ART.REVERT_FRAME_RADIUS_UU | 2000 | uu | GATED(S4) | hero_art.md R-HAR-03, Tools/hero_outline.py (the frame D-036 reverts to: the second scale the silhouette is read at) |
| C.HERO_ART.MASK_SCREEN_HEIGHT_PX | 1080 | px | GATED(S4) | hero_art.md R-HAR-03, Tools/hero_outline.py (the screen the mask is drawn for) |
| C.HERO_ART.OUTLINE_OVERLAP_MAX | 0.8 | ratio | GATED(S4) | hero_art.md R-HAR-03, T-HAR-03 (most a hero mask may share with an enemy hull mask of the same area; a disc and a square share 0.83) |
| C.HERO_ART.FACING_DIFF_MIN | 0.15 | ratio | GATED(S4) | hero_art.md R-HAR-03, T-HAR-03 (least two facings must differ, as a share of the pixels either covers) |
| C.HERO_ART.ARMOUR_PX_MIN | 368 | px | GATED(S4) | hero_art.md R-HAR-06, T-HAR-06 (armour-tone pixels of the hero in the 1280x720 gate frame: what the cylinder showed, D-036) |
| C.DIR.BAND_WINDOW_S | 600–780 | s | GATED(S1) | director.md, T-DIR-02, T-DIR-09, T-DIR-18 |
| C.DIR.NOPICK_DEATH_BAND_S | 480–720 | s | GATED(S1) | director.md, T-DIR-01 |
| C.DIR.BOT_RUNS | 10 | n | GATED(S1) | director.md, T-DIR-01 |
| C.DEATH.FIRST_ATTEMPT_CUTOFF_S | 600 | s | GATED(S2) | bot.md, R-BOT-17, T-BOT-17 |
| C.DEATH.FIRST_ATTEMPT_SHARE | 0.5 | ratio | GATED(S2) | bot.md, R-BOT-17, T-BOT-17 |
| C.DEATH.STEERED_WIN_BAND | 0.30–0.40 | ratio | GATED(S2) | bot.md, R-BOT-17, T-BOT-17 |
| C.DEATH.BOSS_KILL_SHARE | 0.33 | ratio | GATED(S2) | bot.md, R-BOT-17, T-BOT-17 |
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
| C.DROP.XP_TANK | 4 | xp | GATED(S1) | director.md, progression.md, T-DIR-12 |
| C.DROP.XP_RUSHER | 2 | xp | GATED(S1) | director.md, progression.md, T-DIR-12 |
| C.DROP.XP_TURRET | 3 | xp | GATED(S1) | director.md, progression.md, T-DIR-12 |
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
| C.WEAPON.CONE_DEG | 40 | deg | GATED(S2) | weapons.md, T-WPN-14 |
| C.WEAPON.LINE_SECTORS | 72 | n | GATED(S2) | weapons.md, T-WPN-15 |
| C.WEAPON.SEEKER_TURN_DEG_S | 360 | deg/s | GATED(S2) | weapons.md, T-WPN-17 |
| C.WEAPON.SPIRAL_DEG_S | 300 | deg/s | GATED(S2) | weapons.md, T-WPN-18 |
| C.WEAPON.STREAM_ECHO_EVERY_N | 4 | n | GATED(S2) | weapons.md, T-WPN-18 |
| C.MOD.SOCKETS_PER_WEAPON | 3 | n | GATED(S1) | modifiers.md, offers.md, T-MOD-04 |
| C.MOD.MIN_COMPATIBLE | 4 | n per weapon | GATED(S1) | modifiers.md, T-MOD-07 |
| C.MOD.SPLIT_GENERATIONS_MAX | 2 | n | CORE | modifiers.md, keepers.md, T-MOD-05 |
| C.MOD.CRIT_CHANCE_CAP | 0.60 | ratio | CORE | modifiers.md, combat_core.md, T-MOD-05 |

## Settings (02_Systems/settings.md)
| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.SETTINGS.FILE_VERSION | 1 | n | CORE | settings.md, T-SET-03 |
| C.SETTINGS.VOLUME_MAX_PCT | 100 | pct | CORE | settings.md, T-SET-07 |
| C.SETTINGS.VOLUME_STEP_PCT | 10 | pct | CORE | settings.md, T-SET-07 |
| C.SETTINGS.TEXT_MIN_PCT | 100 | pct | CORE | settings.md, T-SET-12 |
| C.SETTINGS.TEXT_MAX_PCT | 150 | pct | CORE | settings.md, T-SET-12 |
| C.SETTINGS.TEXT_STEP_PCT | 25 | pct | CORE | settings.md, T-SET-12 |
| C.SETTINGS.OUTLINE_BAND_UU | 8 | uu | GATED(S3) | settings.md, T-SET-10 |
| C.SETTINGS.OUTLINE_ROUND_RATIO | 1.5 | ratio | GATED(S3) | settings.md, T-SET-10 |
| C.SETTINGS.OUTLINE_ROUND_DARK_RATIO | 1.8 | ratio | GATED(S3) | settings.md, T-SET-10 |
| C.SETTINGS.CVD_DELTA_E_MIN | 20 | delta_e | CORE | settings.md, T-SET-11 |
| C.SETTINGS.CVD_MUZZLE_DELTA_E_MIN | 15 | delta_e | CORE | settings.md, T-SET-11 |
| C.SETTINGS.RUN_OVER_GUARD_S | 1.0 | s | GATED(S3) | settings.md, T-SET-21 |
| C.META.CURRENCY_PER_MINUTE | 10 | currency | GATED(S3) | meta.md, T-MET-02 |
| C.META.CURRENCY_PER_KILL | 0.1 | currency | GATED(S3) | meta.md, T-MET-02 |
| C.META.CURRENCY_PER_ELITE | 50 | currency | GATED(S3) | meta.md, T-MET-02 |
| C.META.CURRENCY_BOSS_KILL | 500 | currency | GATED(S3) | meta.md, T-MET-02 |
| C.META.CURRENCY_HEAT_BONUS | 0.15 | ratio | GATED(S3) | meta.md, T-MET-02 |
| C.META.ACT_START | 1 | n | CORE | meta.md, T-MET-09 |
| C.META.ACT_MAX | 3 | n | CORE | meta.md, T-MET-09 |
| C.TREE.COST_TOOL | 400 | currency | GATED(S3) | meta.md, tree.csv, T-MET-21 |
| C.TREE.COST_BLESSING4 | 300 | currency | GATED(S3) | meta.md, tree.csv, T-MET-21 |
| C.TREE.COST_BLESSING5 | 500 | currency | GATED(S3) | meta.md, tree.csv, T-MET-21 |
| C.TREE.COST_REFORGE | 800 | currency | GATED(S3) | meta.md, tree.csv, T-MET-21 |
| C.TREE.COST_WEAPON | 400 | currency | GATED(S3) | meta.md, tree.csv, T-MET-21 |
| C.TREE.COST_WORKSHOP | 600 | currency | GATED(S3) | meta.md, tree.csv, T-MET-21 |
| C.TREE.TOOL_PER_RANK | 1 | n | GATED(S3) | meta.md, tree.csv, T-MET-20 |

## Narrative (03_Narrative.md)

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.NARR.PREWIN_LAYER_MAX | 2 | layer | CORE | 03_Narrative.md, T-NAR-03, T-NAR-06 |
| C.NARR.LAYER_MAX | 3 | layer | CORE | 03_Narrative.md, T-NAR-03 |

## Enemy art (02_Systems/enemy_art.md)

Presentation only (S4 wave 1 package B, D-043): no key here has a reader in the sim.

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.EAR.FLOOR_HIDE_MAX_RATIO | 1.25 | x | GATED(S4) | enemy_art R-EAR-05, T-EAR-05, Tools/enemy_art_check.py (floor a role hull and its optic may hide, against the cube and optic cube they replace: `Validation/S4_wave1.md` §5 item 6) |
| C.EAR.SILHOUETTE_OVERLAP_MAX | 0.75 | ratio | GATED(S4) | enemy_art R-EAR-07, T-EAR-07, Tools/enemy_art_check.py (greatest intersection over union of two roles' outlines at one area; two cubes read 1) |
| C.EAR.SILHOUETTE_PX_PER_M | [27, 36] | px_per_m | GATED(S4) | enemy_art R-EAR-07, T-EAR-07, Tools/enemy_art_check.py, Tools/EnemyArt.ps1 (the frame of D-036 at 1080 rows, and the frame it reverts to) |
| C.EAR.ELITE_LIGHT_VISIBLE_MIN | 1.0 | x | GATED(S4) | enemy_art R-EAR-04, T-EAR-04, Tools/enemy_art_check.py (the elite light's visible area over a role hull, against over the cube) |
| C.EAR.ROLE_TRIS_MAX | 400 | n | GATED(S4) | enemy_art R-EAR-03, T-EAR-03, Tools/enemy_art_check.py (hull plus optic of one role; C.ENEMIES.CAP of them is the scene's worst) |
| C.EAR.LINEUP_SPACING_UU | 320 | uu | GATED(S4) | enemy_art T-EAR-07, Debug/DTDebugEnemyArt, Tools/enemy_art_check.py (slot size of `DT.Enemies.LineUp`) |
| C.EAR.LINEUP_ROWS | 3 | n | GATED(S4) | enemy_art T-EAR-07, Debug/DTDebugEnemyArt, Tools/enemy_art_check.py (facings per role in the line-up) |
| C.EAR.FRAME_MASK_TOLERANCE | 0.06 | ratio | GATED(S4) | enemy_art T-EAR-07, Tools/enemy_art_check.py (how far a pixel's channel must sit from the slot's floor to count as drawn) |
| C.EAR.ELITE_PART_GAP_MAX_UU | 3 | uu | GATED(S4) | enemy_art R-EAR-08, T-EAR-08, Tools/enemy_art_check.py, Tests/Enemies/DTEnemyArtTests (widest gap between an elite part and its role hull, the shield's orbit excepted) |
| C.EAR.ELITE_LIGHT_AREA_MAX_RATIO | 0.28 | ratio | GATED(S4) | enemy_art R-EAR-08, T-EAR-08, Enemies/DTElite, Tools/elite_anchors.py (the elite light's top-view area over its hull's top-view bounding area; a larger optic's light is scaled down about its centre) |
| C.EAR.ELITE_PART_SINK_MAX_UU | 5 | uu | GATED(S4) | enemy_art R-EAR-08, T-EAR-08, Tools/enemy_art_check.py, Tests/Enemies/DTEnemyArtTests (deepest an elite part may sit inside its role hull) |

## Audio (02_Systems/audio.md)

S4 audio wave 1 (D-056, `Validation/S4_audio_wave1.md`): presentation only, no key here has a reader in the sim. The file limits are read by the gate step `audio` (`Tools/audio/audio_check.py`); the runtime keys wait for the runtime job.

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.AUD.SAMPLE_RATE_HZ | 48000 | Hz | GATED(S4) | audio R-AUD-16, T-AUD-16, Tools/audio/dtsynth.py (the one source of the generator's rate) |
| C.AUD.DANGER_BAND_HZ | [2000, 5000] | Hz | GATED(S4) | audio R-AUD-15, T-AUD-15 (the band kept for warnings, as the enemy hue band is kept for enemy fire) |
| C.AUD.LOW_ZONE_HZ | [40, 300] | Hz | GATED(S4) | audio R-AUD-15, T-AUD-15 (the heartbeat's band: at least half a low-zone file's energy) |
| C.AUD.AIR_LINE_HZ | 6300 | Hz | GATED(S4) | audio R-AUD-15, T-AUD-15 (no hiss above it) |
| C.AUD.SMALL_SPEAKER_HZ | 150 | Hz | GATED(S4) | audio R-AUD-16, T-AUD-16 (the second-order high-pass that stands in for a small speaker) |
| C.AUD.PLAYER_BAND_SHARE_MAX_DB | -30 | dB | GATED(S4) | audio R-AUD-15, T-AUD-15 (a player-side or interface file's share in the danger band, at its highest drawn pitch) |
| C.AUD.WARN_BAND_SHARE_MIN_DB | -3 | dB | GATED(S4) | audio R-AUD-15, T-AUD-15 (a warning's share in the danger band) |
| C.AUD.AIR_SHARE_MAX_DB | -30 | dB | GATED(S4) | audio R-AUD-15, T-AUD-15 (any file's share above the air line) |
| C.AUD.PLAYER_ATTACK_MIN_MS | 4 | ms | GATED(S4) | audio R-AUD-15, T-AUD-15 (a soft start: -40 dB to 90 % of the peak) |
| C.AUD.SPIKE_MAX_DB | 3 | dB | GATED(S4) | audio R-AUD-15, T-AUD-15 (the first 6 ms over the next 34 ms) |
| C.AUD.WARN_ATTACK_MAX_MS | 3 | ms | GATED(S4) | audio R-AUD-15, T-AUD-15 (a warning's hard start) |
| C.AUD.LEAD_MAX_MS | 2 | ms | GATED(S4) | audio R-AUD-16, T-AUD-16 (a one-shot's silence before its onset) |
| C.AUD.PEAK_MAX_DBFS | -1 | dBFS | GATED(S4) | audio R-AUD-16, T-AUD-16 |
| C.AUD.DC_MAX | 0.002 | ratio | GATED(S4) | audio R-AUD-16, T-AUD-16 (mean offset of full scale) |
| C.AUD.SMALL_SPEAKER_LOSS_MAX_DB | 6 | dB | GATED(S4) | audio R-AUD-16, T-AUD-16 (loudness a warning, a low-zone file or an interface file may lose through the small-speaker high-pass) |
| C.AUD.PAIR_DISTANCE_MIN_DB | 6 | dB | GATED(S4) | audio R-AUD-17, T-AUD-17 (spectral distance of two sounds of one confusion set) |
| C.AUD.MASK_SPREAD_DB | 5 | dB_per_band | GATED(S4) | audio R-AUD-18, T-AUD-18 (upward spread of masking per third octave, taken wider than the ear's) |
| C.AUD.MASK_WINDOW_MS | 100 | ms | GATED(S4) | audio R-AUD-18, T-AUD-18 (window and step of the audibility reading) |
| C.AUD.DANGER_SMR_MIN_DB | 12 | dB | GATED(S4) | audio R-AUD-18, T-AUD-18 (a warning over the worst player mix, second-best band of its zone) |
| C.AUD.HEARTBEAT_SMR_MIN_DB | 9 | dB | GATED(S4) | audio R-AUD-18, T-AUD-18 (the heartbeat over the worst player mix in the low zone) |
| C.AUD.WARN_OVER_MIX_MIN_DB | 6 | dB | GATED(S4) | audio R-AUD-18, T-AUD-18 (a warning's played loudness over the worst player mix) |
| C.AUD.MIX_CREST_MAX_DB | 14 | dB | GATED(S4) | audio R-AUD-18, T-AUD-18 |
| C.AUD.DENSITY_RISE_MAX_DB | 6 | dB | GATED(S4) | audio R-AUD-18, T-AUD-18 (loudness rise from the sparsest to the densest mix) |
| C.AUD.LIMIT_DBFS | -1 | dBFS | GATED(S4) | audio R-AUD-04, R-AUD-18, T-AUD-18 (the Master limiter, which the gate expects to stay idle) |
| C.AUD.VOICES_MAX | 32 | n | GATED(S4) | audio R-AUD-07, T-AUD-07 |
| C.AUD.VOICES_DANGER | 6 | n | GATED(S4) | audio R-AUD-05, R-AUD-07, T-AUD-05 (kept for danger) |
| C.AUD.VOICES_UI | 4 | n | GATED(S4) | audio R-AUD-07, T-AUD-07 (kept for the interface) |
| C.AUD.PLAYER_STARTS_PER_S_MAX | 60 | per_s | GATED(S4) | audio R-AUD-07, T-AUD-07 |
| C.AUD.STARTS_PER_FRAME_MAX | 4 | n | GATED(S4) | audio R-AUD-07, R-AUD-21, T-AUD-07 (danger first; a player voice past it waits a frame) |
| C.AUD.FIRE_DENSE_VOICES_PER_S | 6 | per_s | GATED(S4) | audio R-AUD-07, T-AUD-07, Audio/Observe/DTObserveFire, Tools/audio/mix_model.py (a weapon sounds at most this many times a second, at any rate) |
| C.AUD.AGGREGATE_MAX | 1.6 | ratio | GATED(S4) | audio R-AUD-06, T-AUD-06 (most gain a merged batch takes) |
| C.AUD.SCAN_MAX | 64 | n | GATED(S4) | audio R-AUD-02, T-AUD-02 (entries of a step buffer audio may read) |
| C.AUD.LAYER_FADE_PER_S | 3 | per_s | GATED(S4) | audio R-AUD-08, T-AUD-08 (slew of a layer's gain) |
| C.AUD.KILL_DENSE_VOICES_PER_S | 8 | per_s | GATED(S4) | audio R-AUD-08, T-AUD-08 |
| C.AUD.GRID_STEPS | 3 | steps | GATED(S4) | audio R-AUD-09, T-AUD-09 (sim steps; 50 ms at the fixed step) |
| C.AUD.WARN_MIN_INTERVAL_MS | 120 | ms | GATED(S4) | audio R-AUD-06, T-AUD-06 (one warning per shape per interval) |
| C.AUD.DUCK_DB | -6 | dB | GATED(S4) | audio R-AUD-05, T-AUD-05 |
| C.AUD.DUCK_ATTACK_MS | 20 | ms | GATED(S4) | audio R-AUD-05, T-AUD-05 |
| C.AUD.DUCK_RELEASE_MS | 250 | ms | GATED(S4) | audio R-AUD-05, T-AUD-05 |
| C.AUD.LOWHP_LOWEND_DUCK_DB | -12 | dB | GATED(S4) | audio R-AUD-05, T-AUD-05 (the low-end layer under the heartbeat) |
| C.AUD.STINGER_GAP_S | 0.45 | s | GATED(S4) | audio R-AUD-11, T-AUD-11 |
| C.AUD.STINGER_EXPIRY_S | 1.5 | s | GATED(S4) | audio R-AUD-11, T-AUD-11 |
| C.AUD.GAME_THREAD_MS_MAX | 0.05 | ms | GATED(S4) | audio R-AUD-21, T-AUD-21 (audio's share of the S4 scene's game thread p95, D-056 ruling 6) |
| C.AUD.FRAME_MS_WATCH | 0.2 | ms | GATED(S4) | audio R-AUD-21, T-AUD-21 (audio's worst frame is watched against it) |
| C.AUD.LOW_ZONE_SHARE_MIN_DB | -3 | dB | GATED(S4) | audio R-AUD-15, T-AUD-15, Tools/audio/audio_check.py (a low-zone file's share in C.AUD.LOW_ZONE_HZ: about half its energy) |
| C.AUD.PAIR_LENGTH_RATIO_MIN | 2 | x | GATED(S4) | audio R-AUD-17, T-AUD-17, Tools/audio/audio_check.py (two sounds of a confusion set told apart by active length when one is this many times the other) |
| C.AUD.KEY_SEMIS_MAX | 5 | semitones | GATED(S4) | audio R-AUD-10, R-AUD-15, T-AUD-15, Tools/audio/audio_check.py, Audio/DTAudioScheduler (highest degree a `key` sound is drawn at, over all five written notes: the band share is read there) |
| C.AUD.LADDER_SEMIS_MAX | 12 | semitones | GATED(S4) | audio R-AUD-15, T-AUD-15, Tools/audio/audio_check.py (highest step of a `ladder` sound, such as a gem streak) |
| C.AUD.LEVEL_JITTER_DB | 1 | dB | GATED(S4) | audio R-AUD-10, T-AUD-10, Audio/DTAudioScheduler (a `key` voice's level is drawn within this many dB either side of its catalog volume) |
| C.AUD.LOOP_SEAM_LEVEL_MAX_DB | 3 | dB | GATED(S4) | audio R-AUD-16, T-AUD-16, Tools/audio/audio_check.py (a loop's first and last 20 ms differ in level by at most this; the step rule stands beside it) |
| C.AUD.LOWHP_HOLD_DB | -4 | dB | GATED(S4) | audio R-AUD-12, T-AUD-12, Audio/Observe/DTObserveDanger, Tools/audio/mix_model.py (while the heartbeat runs the Player group is held this much lower, level; the beat adds no duck of its own) |
