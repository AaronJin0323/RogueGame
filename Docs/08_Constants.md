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

## 2. Budgets (from `G02` §07 and D-003)

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
| C.SIM.FIXED_HZ | 60 | Hz | CORE | 07_Technical invariant 3 |

## 3. Harness caps

| Key | Value | Unit | Tag | Dependents |
|---|---|---|---|---|
| C.GATED.DEBT_MAX | 5 | n | CORE | Harness/H03 §4 |
| C.SPEC.LINES_MAX | 300 | lines | CORE | lint |
| C.STATE.LINES_MAX | 100 | lines | CORE | STATE.md |

Everything else (HP scaling k, XP table, weapon damage, aim tokens) starts from the previous project's MEASURED values in `Handoff_Dusktracer/game/reference/balance/` and is tagged `GATED(S1/S2)` when its spec is written.
