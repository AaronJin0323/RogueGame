# Telemetry — day-1 instrumentation (S0)

## Intent
Every run, from the first validation session on, leaves a per-interval CSV and a JSONL event log that a tool can read, so a complaint ("too slow", "I didn't see it", "the Keeper did nothing") is checked against numbers instead of memory (`11_Quality §3`, `Harness/H07 §3`). Damage and kills are attributed per player and per source, so the value budget (`C.POWER.KEEPER_SHARE_L16`) can be measured from real runs.

## Rules
- **R-TEL-01** The telemetry clock is the sim step count, never wall time: `UDTTelemetrySubsystem` runs in `EDTSimPhase::Director` (end of each step, R-TEC-03), and a CSV interval closes exactly when `C.TEL.INTERVAL_S` of sim time has passed since the last one.
- **R-TEL-02** Every counter that has an owner is kept per player (R-TEC-01); the CSV has one row per player per interval with a `player` column. Events reach telemetry through the `FDTRunState` event sink with their `OwnerId`; an event whose owner is not a player of the run lands in no row and is counted in `run_end.unowned_events`.
- **R-TEL-03** Swarm TTK is spawn-to-death sim time per enemy, measured without touching the enemy pool: telemetry mirrors the pool's spawn step per slot (stamped when the enemy system applies spawns), matches the events of `ResolveHits` to the hit list in order (one `Hit` per live target, then one `Kill` on death, R-ENM-05/06) and compacts the mirror as the pool compacts (R-ENM-07). The CSV reports the `C.TEL.TTK_PERCENTILE` of the interval's samples (nearest rank); a mismatch is counted in `run_end.mirror_desyncs` and those events still count, without source or TTK.
- **R-TEL-04** Hits are never logged one line per hit: each step writes at most one `hit` record per player, with the count, the damage and the damage by source.
- **R-TEL-05** Damage, hits and kills are kept per (owner, source). A source is a small id on `FDTHit::SourceId` resolved through the name table `FDTTelemetrySources` (`Unknown`, `Stress`, later `Weapon.<Key>`, `Keeper.<Path>`, `Modifier.<Key>`); a hit nobody tagged is `Unknown`. Strings are never stored per hit.
- **R-TEL-06** The CSV header is pinned (column list in §Data); changing it is a RULE change to this spec and its test.
- **R-TEL-07** A session writes `Saved/Playtests/<seed>_<yyyymmdd_hhmmss>.csv` and `.jsonl` under the project's (or packaged build's) Saved directory (`FPaths::ProjectSavedDir`); this is the log export of the packaged build.
- **R-TEL-08** Telemetry is deterministic: the same seed and the same steps give a byte-identical CSV and identical JSONL except the wall-clock field of `run_start`.
- **R-TEL-09** Output is buffered in memory and appended off the game thread on one ordered pipe: flushed at every interval end and at run end (the last partial interval is written then, its rates over the elapsed time); the game thread never waits on disk except at run end.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| CSV interval (sim time) | C.TEL.INTERVAL_S | CORE | R-TEL-01, T-TEL-01 |
| Rate unit of the `*_per_min` columns | C.TEL.RATE_UNIT_S | CORE | §Data |
| Swarm TTK percentile | C.TEL.TTK_PERCENTILE | CORE | R-TEL-03, C.TTK.SWARM_STEERED_MAX |
| Nearest-enemy distance percentile | C.TEL.NEAREST_PERCENTILE | CORE | `nearest_enemy_uu` |
| Level at which the value budget is checked | C.TEL.POWER_CHECK_LEVEL | CORE | `Tools/power_split.py`, C.POWER.KEEPER_SHARE_L16 |
| Step rate (the clock) | C.SIM.FIXED_HZ | CORE | R-TEL-01 |

## Data
CSV columns, in order (one row per player per interval; empty cell = no sample in the interval):

| Column | Type | Notes |
|---|---|---|
| `interval` | int | index from zero |
| `run_time_s` | float | sim time at the end of the interval |
| `player` | int | owner index (R-TEC-01) |
| `kills_per_min` | float | kills per `C.TEL.RATE_UNIT_S` |
| `swarm_ttk_s` | float | `C.TEL.TTK_PERCENTILE` of spawn-to-death times (R-TEL-03) |
| `damage_taken_per_min` | float | zero until the hero takes damage (S1) |
| `dashes_per_min` | float | zero until the hero dashes (S1) |
| `level` | int | `FDTPlayerRunState::Level` at the interval end |
| `live_bullets`, `live_enemies` | int | at the interval end (world, repeated per player) |
| `recycled_bullets`, `refused_spawns` | int | during the interval (R-BUL-03, R-ENM-01) |
| `threat_actions_per_min` | float | zero until threats exist (S1) |
| `nearest_enemy_uu` | float | `C.TEL.NEAREST_PERCENTILE` over steps of the nearest enemy to this player's hero |
| `hits_per_min`, `damage_dealt_per_min` | float | this player's hits and damage dealt |
| `seed`, `build`, `culture`, `input_device` | text | run seed; `DT_BUILD_HASH` or `unknown`; current culture; `unknown` until S1 |

JSONL: one object per line; every record starts with `t` (sim seconds), `step`, `kind`, `owner` (`-1` = the run).

| Kind | Fields | When |
|---|---|---|
| `run_start` | `seed players culture build input_device interval_s step_s wall` | session start (`wall` is the only wall-clock field) |
| `kill` | `source`, `ttk_s` (when known) | every kill |
| `hit` | `hits damage damage_by_source{name: damage}` | once per player per step with hits (R-TEL-04) |
| `sources` | `interval damage{} kills{} hits{}` by source name | per player at each interval end |
| `run_end` | `reason steps unowned_events mirror_desyncs` | session end |
| `offer_shown` (S1) | `offer_id cards[{key kind weight}] reroll_left` | planned |
| `offer_taken` / `offer_skipped` (S1) | `offer_id key` / `offer_id` | planned |
| `offer_rerolled` / `offer_banished` / `offer_locked` (S1) | `offer_id key` | planned |

Code: `FDTTelemetryCore` (plain C++), `FDTTelemetryWriter` (pipe), `FDTTelemetrySources`, `UDTTelemetrySubsystem`; `ADTGameMode` starts the session with the run seed and ends it in `EndPlay`; the enemy subsystem reports spawns and brackets `ResolveHits`. Debug (not in Shipping, R-TEC-12): `DT.Stats` prints the interval in progress per player; `DT.Stats.Flush` queues the buffer now.

Tool: `Tools/power_split.py [run.csv|run.jsonl]` (default: newest in `Saved/Playtests`) prints per player the damage and kill share by source family (Keeper, Weapon, Modifier, Other) and `POWER: n/a|PASS|FAIL`; the Keeper damage share is checked against `C.POWER.KEEPER_SHARE_L16` only when the player reached `C.TEL.POWER_CHECK_LEVEL`. It becomes the gate T-BAL-01 in S2.

## Acceptance
- T-TEL-01 `Dusktracer.Telemetry.IntervalSimClock`: no row one step before the interval ends, one row on its last step, and further rows follow steps with no wall time passing.
- T-TEL-02 `Dusktracer.Telemetry.PerPlayer`: two players' kills, damage and level land in their own rows; an event for a non-player is in no row.
- T-TEL-03 `Dusktracer.Telemetry.SwarmTtk`: spawn-to-death times are exact through overkill, a stale index and compaction; the row reports the percentile; no desync.
- T-TEL-04 `Dusktracer.Telemetry.HitsPerStep`: N hits in one step give one `hit` record with the count and damage; two owners give one each.
- T-TEL-05 `Dusktracer.Telemetry.Sources`: the name table is stable; kills and damage are credited per (owner, source) to the hit that landed them.
- T-TEL-06 `Dusktracer.Telemetry.CsvHeader`: the header equals the pinned column list.
- T-TEL-07 `Dusktracer.Telemetry.Files`: files go to `Saved/Playtests/<seed>_<yyyymmdd_hhmmss>.csv|.jsonl` and exist after a write.
- T-TEL-08 `Dusktracer.Telemetry.Determinism`: a real fight (enemy and bullet pools) twice with the same seed gives an identical CSV and JSONL except `run_start.wall`; another seed differs.
- T-TEL-09 `Dusktracer.Telemetry.Buffered`: mid-interval output is only buffered events; run end writes the partial row and `run_end`; appends land in order.
- HUMAN: after a stress-arena run, `Saved/Playtests` holds the run's pair of files and `python -I Tools/power_split.py` reads them.

## Telemetry
- This system is the telemetry; its own health is `run_end{unowned_events, mirror_desyncs}`.

## Text
- None: telemetry shows no player-facing text (`DT.Stats` is a debug log line).

## Out of scope
- Offer events (schema above, emitted by the offer system in S1), healing, per-effect trigger / VFX / audio counts, damage taken by source, threat actions, outcome cause beyond the end-play reason (S1 systems emit them).
- Ablation-based value budget (`model.md`): the S2 gate may replace attribution by ablation.

## Open
- Source tags on bullets: the bullet pool carries no source yet, so stress hits read `Unknown` → resolved when weapons land (S1) by a `SourceId` on `FDTBulletSpawn`.
- Build hash is `unknown` until the build defines `DT_BUILD_HASH` → resolved by a one-line build rule change.
