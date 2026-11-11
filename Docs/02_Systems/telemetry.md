# Telemetry — day-1 instrumentation (S0)

## Intent
Every run, from the first validation session on, leaves a per-interval CSV and a JSONL event log that a tool can read, so a complaint ("too slow", "I didn't see it", "the Keeper did nothing") is checked against numbers instead of memory (`11_Quality §3`, `Harness/H07 §3`). Damage and kills are attributed per player and per source, so who landed the damage of a real run can be read beside the value budget's own reading (`C.POWER.KEEPER_SHARE_L16`, by loss, `10_Tuning.md §3`).

## Rules
- **R-TEL-01** The telemetry clock is the sim step count, never wall time: `UDTTelemetrySubsystem` runs in `EDTSimPhase::Director` (end of each step, R-TEC-03), and a CSV interval closes exactly when `C.TEL.INTERVAL_S` of sim time has passed since the last one.
- **R-TEL-02** Every counter that has an owner is kept per player (R-TEC-01); the CSV has one row per player per interval with a `player` column. Events reach telemetry through the `FDTRunState` event sink with their `OwnerId`; an event whose owner is not a player of the run lands in no row and is counted in `run_end.unowned_events`.
- **R-TEL-03** Swarm TTK is the sim time from a Swarm enemy's first hit to its death, the reading `C.TTK.SWARM_STEERED_MAX` bounds: a one-shot kill reads zero, an elite of the role counts, and any other role gives no sample. It is measured without touching the enemy pool: telemetry mirrors each slot's role, stamps the slot's first hit, matches the events of `ResolveHits` to the hit list in order (one `Hit` per live target, then one `Kill` on death, R-ENM-05/06) and compacts the mirror as the pool compacts (R-ENM-07). The CSV reports the `C.TEL.TTK_PERCENTILE` of the interval's samples (nearest rank); a mismatch is counted in `run_end.mirror_desyncs` and those events still count, without source or TTK. How long an enemy walked in before the first hit is not measured: no reader needs it. Runs from before this rule hold spawn-to-death of every role in the same column; `Tools/pressure.py` refuses them.
- **R-TEL-04** Hits and kills are never logged one line per event: each step writes at most one `hit` and one `kill` record per player. The `hit` record carries the count, the damage and the damage by source; the `kill` record carries the count, the count by source and the step's TTK as sums (`ttk_n`, `ttk_sum_s`, plus `ttk_min_s`/`ttk_max_s` when `ttk_n` is above zero) so a tool recombines exact means. Every per-kill TTK sample still feeds the CSV percentile (R-TEL-03).
- **R-TEL-05** Damage, hits and kills are kept per (owner, source). A source is a small id on `FDTHit::SourceId` resolved through the name table `FDTTelemetrySources` (`Unknown`, `Stress`, later `Weapon.<Key>`, `Keeper.<Path>`, `Modifier.<Key>`); a hit nobody tagged is `Unknown`. Strings are never stored per hit.
- **R-TEL-06** The CSV header is pinned (column list in §Data); changing it is a RULE change to this spec and its test.
- **R-TEL-07** A session writes `Saved/Playtests/<seed>_<yyyymmdd_hhmmss>.csv` and `.jsonl` under the project's (or packaged build's) Saved directory (`FPaths::ProjectSavedDir`); this is the log export of the packaged build (`package.md` R-PKG-05, R-PKG-06: location and hand-over).
- **R-TEL-08** Telemetry is deterministic: the same seed and the same steps give a byte-identical CSV and identical JSONL except wall-clock fields. A wall-clock field is named `wall` or `wall_*` and exists only where sim time cannot measure the quantity (`run_start.wall`, UTC; time spent in hands, which pause the sim: `offer_pick.wall_decide_s`, `run_end.wall_paused_s`; what the machine did to the run: `run_end.wall_dropped_steps`, the steps the fixed-step clock dropped because a frame owed more than `C.SIM.MAX_SUBSTEPS` (R-TEC-03), and `run_end.wall_duration_s`, the wall seconds from `run_start` to `run_end`); tools never compare them. Each dropped step is one step of sim time the player waited for on the wall clock: a run with many was played in slow motion. No rule yet says how many set a run aside (Open). `run_start.bot_fast` is not a wall field: it is the steps per frame an active bot ran past the clock (`-DTBotFast`), on every run, zero for a person's run or a bot at real speed, so a run file says itself whether its wall fields are a person's (T-TEL-15).
- **R-TEL-09** Output is buffered in memory and appended off the game thread on one ordered pipe: flushed at every interval end and at run end (the last partial interval is written then, its rates over the elapsed time); the game thread never waits on disk except at run end.
- **R-TEL-10** Where the hero was: every `C.TEL.HERO_POS_INTERVAL_S` of sim time one `hero_pos` record per player, and on every `hero_damaged` the hero's position when the hit landed and the position the hit came from: the enemy, the point a round was fired from (never where it touched the hero, which is the hero's own position), the attack's origin, or the safe ring's nearest edge. A round's record adds its velocity and its time in the air (`vel_x vel_y flight_s`), so its whole line and the warning it gave read from the record. Every `hero_damaged` carries `arrival`: true when the hitter carries `DTEnemyFlags::Arrival` (a director drop-in, `director.md` R-DIR-07), by its body or by the band of one killed in its wind-up, false for every other hit. The flag is set at spawn and never cleared, so a drop-in Rusher's contact after its first charge is still marked; a Turret drop-in's rounds are not. Every `hero_damaged` also carries `charging`: true when the hitter is a Rusher, drop-in or not, that moved under its charge on the step it touched the hero, its last charge step included; false for every other hit, and for the band of a drop-in killed in its wind-up, which has no live hitter and is told by its `hit_kind` Charge. So a drop-in's charge reads `arrival` and `charging` true with `hit_kind` Contact (T-TEL-14). The rate is set so a run's JSONL grows less than a twentieth (`Docs/Validation/S1_run_telemetry.md`).
- **R-TEL-11** Damage has two readings. **Raw** is the hit as its emitter wrote it: `FDTHit::Damage`, crit, load and stats in, the Tank's plate, Shock and the enemy's remaining HP not yet considered; `FDTHeroHit::Damage`, the hero's i-frames and reductions not yet considered. **Applied** is the HP the hit removed: on an enemy after plate and Shock, clamped to the HP left, in whole points with the fraction carried per owner and source; on the hero `hero_damaged.amount`, after i-frames and reductions and not clamped, so a fatal hit reads whole. A `damage` field or column is applied, everywhere; a raw reading says so in its name (`damage_raw`, `damage_raw_by_attack`). Telemetry reads both where it matches hits to events (R-TEL-03).

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| CSV interval (sim time) | C.TEL.INTERVAL_S | CORE | R-TEL-01, T-TEL-01 |
| Rate unit of the `*_per_min` columns | C.TEL.RATE_UNIT_S | CORE | §Data |
| Swarm TTK percentile | C.TEL.TTK_PERCENTILE | CORE | R-TEL-03, C.TTK.SWARM_STEERED_MAX |
| Nearest-enemy distance percentile | C.TEL.NEAREST_PERCENTILE | CORE | `nearest_enemy_uu` |
| Sim time between `hero_pos` records | C.TEL.HERO_POS_INTERVAL_S | CORE | R-TEL-10, T-TEL-10 |
| Level at which the value budget is checked | C.TEL.POWER_CHECK_LEVEL | CORE | `Tools/probe.py`, `Tools/power_split.py`, C.POWER.KEEPER_SHARE_L16 |
| Step rate (the clock) | C.SIM.FIXED_HZ | CORE | R-TEL-01 |

## Data
CSV columns, in order (one row per player per interval; empty cell = no sample in the interval):

| Column | Type | Notes |
|---|---|---|
| `interval` | int | index from zero |
| `run_time_s` | float | sim time at the end of the interval |
| `player` | int | owner index (R-TEC-01) |
| `kills_per_min` | float | kills per `C.TEL.RATE_UNIT_S` |
| `swarm_ttk_s` | float | `C.TEL.TTK_PERCENTILE` of a Swarm enemy's first-hit-to-death times (R-TEL-03); compares with `C.TTK.SWARM_STEERED_MAX` |
| `damage_taken_per_min` | float | applied damage as a share of max HP (each hit ÷ max HP at that hit) per `C.TEL.RATE_UNIT_S`; compares with `C.DMG_TAKEN.BAND_MIN10_13` |
| `dashes_per_min` | float | `Dash` events (`combat_core.md`) |
| `level` | int | `FDTPlayerRunState::Level` at the interval end |
| `live_bullets`, `live_enemies` | int | at the interval end (world, repeated per player) |
| `recycled_bullets`, `refused_spawns` | int | during the interval (R-BUL-03, R-ENM-01) |
| `threat_actions_per_min` | float | threat actions started within the threat ring of this player's hero (`director.md`, `enemies.md`) |
| `nearest_enemy_uu` | float | `C.TEL.NEAREST_PERCENTILE` over steps of the nearest enemy to this player's hero |
| `hits_per_min`, `damage_dealt_per_min` | float | this player's hits and damage dealt; an event holds whole points and the fraction carries per owner and source, so ticks below one point add up |
| `seed`, `build`, `culture`, `input_device` | text | run seed; `DT_BUILD_HASH` (`Dusktracer.Build.cs`: short git hash, `+dirty` with local changes) or `unknown` without git; current culture; `kbm`, `pad` or `unknown`: last gameplay input device (`combat_core.md`); the column is the only device reading, since no input exists when `run_start` is written |

JSONL: one object per line; every record starts with `t` (sim seconds), `step`, `kind`, `owner` (`-1` = the run). A key appears once per object: a record's own sort of thing is `hit_kind`, `hand_kind` or `card_kind`, never a second `kind`. Runs from before the renames repeat `kind` in `hero_damaged` (hit kind) and `offer_pick` (card kind); the record kind comes first, so tools read old runs through `pressure.first_wins`.

| Kind | Fields | When |
|---|---|---|
| `run_start` | `seed players culture build interval_s step_s bot_fast wall` | session start (`wall`, UTC, is the only wall-clock field; `bot_fast`, R-TEL-08) |
| `kill` | `kills kills_by_source{name: kills} ttk_n ttk_sum_s`, `ttk_min_s ttk_max_s` (when `ttk_n` is above zero) | once per player per step with kills (R-TEL-04); `ttk_n` counts the Swarm kills, the only ones with a TTK (R-TEL-03) |
| `hit` | `hits damage damage_by_source{name: damage}` | once per player per step with hits (R-TEL-04) |
| `hero_pos` | `x y` (whole uu) | per player every `C.TEL.HERO_POS_INTERVAL_S` (R-TEL-10) |
| `sources` | `interval damage{} kills{} hits{}` by source name | per player at each interval end |
| `run_end` | `reason steps unowned_events mirror_desyncs`, then the per-player and run fields of the S1 specs, then `wall_dropped_steps wall_duration_s` (R-TEL-08) | session end |

S1 kinds are defined in the owning spec's §Telemetry, in this record format, aggregated per step per player whenever they can occur more than once per step (R-TEL-04): `offer` `offer_pick` `offer_tool` (`offers.md`); `keeper_pick` `keeper_commit` `intercession` `status` (`keepers.md`); `dash` `hero_damaged` `hero_healed` `hero_down` (`combat_core.md`); `xp` `level_up` `gems_merged` (`progression.md`); `weapon` `weapon_acquired` `weapon_level` (`weapons.md`); `modifier_socketed` `modifier_dormant` (`modifiers.md`); `dir_block` (`director.md`); `boss_start` `boss_phase` `boss_calibrated` `boss_end` (`boss.md`). `run_end` gains the per-player fields of `offers.md` and `progression.md`. A new kind or field is legal without a change here; a CSV column is not (R-TEL-06).

Code: `FDTTelemetryCore` (plain C++), `FDTTelemetryWriter` (pipe), `FDTTelemetrySources`, `UDTTelemetrySubsystem`; `ADTGameMode` starts the session with the run seed and ends it in `EndPlay`; the enemy subsystem reports spawns and brackets `ResolveHits`. Debug (not in Shipping, R-TEC-12): `DT.Stats` prints the interval in progress per player; `DT.Stats.Flush` queues the buffer now.

Tool: `Tools/power_split.py [run.csv|run.jsonl]` (default: newest in `Saved/Playtests`) prints per player the damage and kill share by source family (Keeper, Weapon, Modifier, Other) and one `POWER:` line: the Keeper damage share of a player who reached `C.TEL.POWER_CHECK_LEVEL`, or `n/a`. It is the attribution view of one session log, printed beside the probe's reading and never judged (D-047): `C.POWER.KEEPER_SHARE_L16` is read by loss in `Tools/probe.py` (`keepers.md` R-KEE-18).

## Acceptance
- T-TEL-01 `Dusktracer.Telemetry.IntervalSimClock`: no row one step before the interval ends, one row on its last step, and further rows follow steps with no wall time passing.
- T-TEL-02 `Dusktracer.Telemetry.PerPlayer`: two players' kills, damage and level land in their own rows; an event for a non-player is in no row.
- T-TEL-03 `Dusktracer.Telemetry.SwarmTtk`: first-hit-to-death times of Swarm enemies are exact through overkill, a stale index and compaction; a one-shot reads zero, a Tank gives no sample; the row reports the percentile; no desync.
- T-TEL-04 `Dusktracer.Telemetry.HitsPerStep`: N hits in one step give one `hit` record with the count and damage; two owners give one each; N kills by one player in one step give one `kill` record with `kills` = N, the counts by source and the TTK sums.
- T-TEL-05 `Dusktracer.Telemetry.Sources`: the name table is stable; kills and damage are credited per (owner, source) to the hit that landed them.
- T-TEL-06 `Dusktracer.Telemetry.CsvHeader`: the header equals the pinned column list.
- T-TEL-07 `Dusktracer.Telemetry.Files`: files go to `Saved/Playtests/<seed>_<yyyymmdd_hhmmss>.csv|.jsonl` and exist after a write.
- T-TEL-08 `Dusktracer.Telemetry.Determinism`: a real fight (enemy and bullet pools) twice with the same seed gives an identical CSV and JSONL except `wall` / `wall_*` fields; another seed differs.
- T-TEL-09 `Dusktracer.Telemetry.Buffered`: mid-interval output is only buffered events; run end writes the partial row and `run_end`; appends land in order.
- T-TEL-10 `Dusktracer.Telemetry.HeroPosition`: one `hero_pos` per player on the sim clock, in whole uu. `Dusktracer.Telemetry.RunEnd`: a fatal `hero_damaged` carries the two positions apart, `run_start` claims no device, its `wall` is UTC; `run_end.wall_dropped_steps` counts the steps two hitches inside the session dropped and none from a hitch before it, and `wall_duration_s` is there. `Dusktracer.Telemetry.RoundSource`: in a real world a round's `hero_damaged` reads its firing point, velocity and time in the air.
- T-TEL-11 `Dusktracer.Telemetry.CastWindow`: a cast's window reads raw and applied apart (overkill), counts only the cast's sources, only inside the window, and enemies once; a window the run cut short is written `complete` false.
- T-TEL-12 `Dusktracer.Telemetry.LoadoutRecords`: in a real world `UDTWeaponSubsystem` writes `weapon_acquired`, `weapon_level`, `modifier_socketed`, both `modifier_dormant` changes, and the interval's `weapon` record with its level, load rank and emissions.
- T-TEL-14 `Dusktracer.Telemetry.Mark.Arrival`: in a real world a drop-in Rusher's contact writes `hero_damaged.arrival` true with `hit_kind` Contact and a Rusher that walked in writes false; the band of a drop-in killed in its wind-up lands once as a Charge hit that carries the mark and reads `charging` false; a Rusher's contact reads `charging` true exactly on the steps it moves under its charge, drop-in or walker, and false in its wind-up (R-TEL-10).
- T-TEL-15 `Dusktracer.Telemetry.Mark.BotFast`: `run_start.bot_fast` is the steps per frame of an active bot's fast run, and zero for a bot at real speed, for a run with no active bot and for the plain core's default (R-TEL-08).
- HUMAN: after a stress-arena run, `Saved/Playtests` holds the run's pair of files and `python -I Tools/power_split.py` reads them.

## Telemetry
- This system is the telemetry; its own health is `run_end{unowned_events, mirror_desyncs}`.

## Text
- None: telemetry shows no player-facing text (`DT.Stats` is a debug log line).

## Out of scope
- The S1 record kinds listed in §Data (their specs own them); per-effect trigger / VFX / audio counts, outcome cause beyond the end-play reason.
- The value budget's verdict: `keepers.md` R-KEE-18 reads it by loss in a probe scene (`Tools/probe.py`, D-047). Attribution from a run's records is a view.

## Open
- S1 writer API (non-behavioural choices, `Telemetry/DTTelemetryCore.h` `Record*`/`Set*`, types in `DTTelemetryRecords.h`; producers call `UDTTelemetrySubsystem::Find(this)->Records()`): `run_end` puts the per-player fields of `offers.md`/`progression.md` under `per_player{owner: {...}}` and the run's `gems_*` at top level; `cards_offered`/`reserve_cards` count every shown card (rerolls and refills included), `hands_by_kind` counts a hand once, `decisions` excludes the Patron hand, `wall_paused_s` sums `offer_pick.wall_decide_s`; the per-interval count of `intercession_denied` is a record `intercession_denied{interval count}`; `weapon` is written for every owned weapon each interval; "within the threat ring" = within `C.DIR.THREAT_RING_MAX_UU` of the hero at the step end; source maps are ordered by name; the slot mirror (R-TEL-03, kept; `FDTWorldView::Kills` is not needed) skips stale-serial hits like the pool (R-ENM-23) → revisit with the S1 producers.
- Hero producers (`combat_core.md`) reach telemetry through `UDTHeroSubsystem::OnHeroEvent`, bound by the telemetry subsystem at begin play (the hero stays unaware of telemetry); `input_device` is read from the hero at each step end. `run_end`'s two machine fields are set by the telemetry subsystem at session end from `FDTFixedStep::GetDroppedSteps` less its value at session start and from the platform clock; a bot run at `-DTBotFast` runs nearly all its steps through `StepNow`, past the clock, so its two fields read its own few frames and not a player's (four bot arms are read in `Validation/S1_d045_d046.md`).
- `run_end.wall_dropped_steps` has no threshold: a slowed run is told by its slow share (`10_Tuning.md §2`, D-054), printed for people's runs only, and no share yet sets a run aside; no tool prints it yet (`Tools/pressure.py` follows) → the S1 gate decision after session A (D-048), from what the validators' machines read.
