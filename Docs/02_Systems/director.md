# Director — pacing, threats and drops (S1)

## Intent
The director turns one run of `C.RUN.LENGTH_TO_BOSS` into a rhythm the player can read: Harvest (feel untouchable), Threat (a skill check you can see coming), Breath (a visible end to every spike), then the boss. Its core duty is the **kill horizon**: a steered build that clears everything far away must still meet threats, so threats arrive as telegraphed drop-ins instead of walking in to die (pillar one, `00_Vision §2.1`; `balance/pressure_proposal.md`). Difficulty comes from skill checks and the build, never from HP walls.

## Rules
- **R-DIR-01** Overwhelm contract: a bot that never picks a card (Bot v1, no-pick arm) is overwhelmed around minute ten: over `C.DIR.BOT_RUNS` seeds its median death time lies in `C.DIR.NOPICK_DEATH_BAND_S`. A build is what survives the run; the timeline is tuned against this, never by giving no-pick runs more HP.
- **R-DIR-02** Pressure contract: a steered human build's damage taken per minute, as a share of max HP over `C.DIR.BAND_WINDOW_S`, lands in `C.DMG_TAKEN.BAND_MIN10_13`, and its dashes per minute over the same window land in `C.DASHES.BAND_MIN10_13`. Missing the band is fixed by threat cadence, mix or placement, never by enemy HP or shorter telegraphs.
- **R-DIR-03** The director is one `IDTSimSystem` (`UDTDirector`) in `EDTSimPhase::Director`. Its clock is sim time since run start (it stops when the sim pauses). It reads the arena timeline, a data asset generated from `Docs/09_Catalog/timeline_<arena>.csv` (S1: `timeline_arena1.csv`); no pacing number lives in code. Its spawn requests are queued and applied at the start of the next `Enemies` phase (R-ENM-01 caps still apply).
- **R-DIR-04** Pacing blocks: `Harvest` rows form a step function (each row's values hold until the next `Harvest` row). A `Threat` row starts a beat at its start time that overlays the Harvest values with its own rate, mix and cap; the beat ends after its `duration_s` or the step its event enemies are all dead, whichever comes first. Every beat is followed by a `Breath` of `C.DIR.BREATH_S` in which regular spawns and drop-ins pause. Every beat plus its Breath ends before `C.RUN.LENGTH_TO_BOSS` (checked on the data, not repaired at run time).
- **R-DIR-05** Regular spawns: a credit grows by the active spawn rate each step and is capped at one, so a full field never banks a burst. Each whole credit spawns one enemy whose role is a weighted draw over the active mix, excluding roles at their alive share cap (`C.DIR.SHARE_MAX_TANK`, `C.DIR.SHARE_MAX_RUSHER`, `C.DIR.SHARE_MAX_TURRET`; Swarm has no share cap); with the alive cap reached (row `cap`, never above `C.ENEMIES.CAP`) the spawn is skipped and the credit spent. Positions lie on the ring from `C.DIR.SPAWN_RING_MIN_UU` to `C.DIR.SPAWN_RING_MAX_UU` around a living hero chosen uniformly, moved inside the arena when the ring leaves it.
- **R-DIR-06** Threat placement: during a beat, `Rusher` and `Turret` together hold at least `C.DIR.THREAT_SHARE_MIN` of the mix weight, and their regular spawns use the threat ring from `C.DIR.THREAT_RING_MIN_UU` to `C.DIR.THREAT_RING_MAX_UU`. `Turret` regular spawns use the threat ring in every block, so turrets never pile up out of the build's reach.
- **R-DIR-07** Kill horizon (core duty): drop-ins deliver threats independent of how far out the build kills. From the first Harvest row with a `dropin_interval_s`, a drop-in comes due every interval (divided by `C.DIR.DROPIN_THREAT_MULT` during a beat). The director picks the role by the row's drop-in weights and a landing point on the threat ring around a living hero, publishes a red ground marker there (`04_Presentation §2`) and, `C.DIR.DROPIN_LEAD_S` later, spawns the enemy flagged `Arrival`, so its threat action (charge, burst) starts in its first step and completes even if it dies (`enemies.md` owns the action). Before it lands a drop-in is director data: it cannot be hit or killed.
- **R-DIR-08** Drop-in fairness: at most `C.DIR.DROPIN_ACTIVE_MAX` markers are live at once (`C.DIR.DROPIN_ACTIVE_MAX_THREAT` during a beat); landings are at least `C.DIR.DROPIN_GAP_MIN_S` apart; none come during Breath or from `C.RUN.LENGTH_TO_BOSS` on. A due drop-in that would break a limit waits; at most one waits at a time (no backlog). Drop-ins never change HP, damage or telegraph durations.
- **R-DIR-09** Kill-horizon guarantee: with a debug build that kills every enemy on the step it lands, threat actions that start within the threat ring of a hero reach at least `C.DIR.THREAT_ACTIONS_MIN` per minute over `C.DIR.BAND_WINDOW_S`.
- **R-DIR-10** Scaling: every director spawn is stamped once with `HpScale = 1 + C.SCALE.HP_PER_MIN × m` and `DmgScale = 1 + C.SCALE.DMG_PER_MIN × m`, where `m` is run time in minutes (continuous); the enemy keeps both for life and scales its HP, contact damage and bullet damage by them. The director has no other HP or damage lever, and telegraph durations never scale.
- **R-DIR-11** Events: a `Threat` row's `event` spawns at the beat start, `event_count` times, tagged with the beat (event table in §Data); event enemies count toward the alive cap and the share caps. Elite rules (HP, affixes) are `enemies.md`'s.
- **R-DIR-12** XP drops: every kill of a director spawn drops one XP gem at the death position worth `C.DROP.XP_SWARM`, `C.DROP.XP_TANK`, `C.DROP.XP_RUSHER` or `C.DROP.XP_TURRET` by role, times `C.DROP.XP_ELITE_MULT` for an elite. With `C.DROP.GEMS_MAX` gems live, a new gem's value is added to the nearest live gem instead (lowest index on a tie), so XP is never lost. Debug and stress spawns drop nothing. Pickup, magnet and the level curve are `progression.md`'s.
- **R-DIR-13** Boss hand-off: at `C.RUN.LENGTH_TO_BOSS` regular spawns, beats and drop-ins stop, live markers are cancelled, and the director requests the boss (`Enemy.Boss.Warden`, `boss.md`). Enemies already alive stay unless `boss.md` clears them.
- **R-DIR-14** Randomness: every director draw (role, ring angle and radius, hero choice, drop-in role and point) comes from its own stream `EDTRngStream::Director`, appended to the enum so existing stream seeds are unchanged (R-TEC-02). The same seed and the same per-step hero inputs give an identical spawn and drop-in sequence.
- **R-DIR-15** Telemetry per block: when a Harvest row, beat or Breath ends, the director writes one `dir_block` record (§Telemetry); it never writes one line per spawn.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Run length to the boss | C.RUN.LENGTH_TO_BOSS | CORE | R-DIR-04, R-DIR-08, R-DIR-13 |
| Damage-taken band | C.DMG_TAKEN.BAND_MIN10_13 | CORE | R-DIR-02, T-DIR-02 |
| Dash band | C.DASHES.BAND_MIN10_13 | CORE | R-DIR-02 |
| Live enemy cap | C.ENEMIES.CAP | CORE | R-DIR-05 |
| Band window (minutes ten to thirteen) | C.DIR.BAND_WINDOW_S | GATED(S1) | R-DIR-02, R-DIR-09 |
| No-pick death band | C.DIR.NOPICK_DEATH_BAND_S | GATED(S1) | R-DIR-01 |
| Bot runs per arm | C.DIR.BOT_RUNS | GATED(S1) | R-DIR-01 |
| Breath length | C.DIR.BREATH_S | GATED(S1) | R-DIR-04 |
| Regular spawn ring | C.DIR.SPAWN_RING_MIN_UU, C.DIR.SPAWN_RING_MAX_UU | GATED(S1) | R-DIR-05 |
| Threat ring | C.DIR.THREAT_RING_MIN_UU, C.DIR.THREAT_RING_MAX_UU | GATED(S1) | R-DIR-06, R-DIR-07 |
| Alive share caps | C.DIR.SHARE_MAX_TANK, C.DIR.SHARE_MAX_RUSHER, C.DIR.SHARE_MAX_TURRET | GATED(S1) | R-DIR-05 |
| Threat-role share in a beat | C.DIR.THREAT_SHARE_MIN | GATED(S1) | R-DIR-06 |
| Drop-in marker lead | C.DIR.DROPIN_LEAD_S | GATED(S1) | R-DIR-07 |
| Drop-in cadence factor in a beat | C.DIR.DROPIN_THREAT_MULT | GATED(S1) | R-DIR-07 |
| Live drop-in markers | C.DIR.DROPIN_ACTIVE_MAX, C.DIR.DROPIN_ACTIVE_MAX_THREAT | GATED(S1) | R-DIR-08 |
| Gap between landings | C.DIR.DROPIN_GAP_MIN_S | GATED(S1) | R-DIR-08 |
| Threat actions floor | C.DIR.THREAT_ACTIONS_MIN | GATED(S1) | R-DIR-09 |
| HP slope per minute (k) | C.SCALE.HP_PER_MIN | GATED(S1) | R-DIR-10, C.TTK.SWARM_STEERED_MAX |
| Damage slope per minute (k') | C.SCALE.DMG_PER_MIN | GATED(S1) | R-DIR-10, R-DIR-02 |
| XP per role and elite factor | C.DROP.XP_SWARM, C.DROP.XP_TANK, C.DROP.XP_RUSHER, C.DROP.XP_TURRET, C.DROP.XP_ELITE_MULT | GATED(S1) | R-DIR-12, progression.md |
| Live gem cap | C.DROP.GEMS_MAX | GATED(S1) | R-DIR-12 |

Sources: slopes, rings, Breath, drop-in cadence and fairness are the previous project's 15-minute values (`01_GameDesign §3.5` and §7, D-71 step two; `balance/pressure_proposal.md` P1 and P3); share caps follow `balance/playtest5_verification.md` (pile-ups at minute twelve were an HP wall); the no-pick band restates "overrun around minute ten" against the old bot's median (`balance/sims.md §0`). Timeline rates, caps, mixes and drop-in intervals live in `timeline_arena1.csv`, all GATED(S1).

## Data
Timeline `Docs/09_Catalog/timeline_<arena>.csv`, one row per Harvest step, Threat beat or the boss:

| Column | Type | Notes |
|---|---|---|
| `start_s` | float | sim time; rows sorted; a Harvest and a Threat row may share a start |
| `phase` | `Harvest` / `Threat` / `Boss` | `Breath` is never a row: it follows each beat (R-DIR-04) |
| `duration_s` | float | Threat only: the longest the beat lasts |
| `spawn_rate_per_s` | float | regular spawns per second |
| `cap` | int | alive cap, at most `C.ENEMIES.CAP` |
| `w_swarm`, `w_tank`, `w_rusher`, `w_turret` | float | mix weights, any scale (`Enemy.Swarm Tank Rusher Turret`) |
| `dropin_interval_s` | float | Harvest only; empty = no drop-ins yet |
| `dropin_w_rusher`, `dropin_w_turret` | float | drop-in mix; roles without an arrival form in `enemies.md` must be zero |
| `event`, `event_count` | key, int | Threat only, event table below; Boss row: `Boss` |
| `note` | text | design note, never read by code |

Event table (S1 roles only; behaviours are `enemies.md`'s):

| Event | Spawns (`event_count`) | Skill check |
|---|---|---|
| `EliteWave` | elites of the active mix, on the threat ring | focus fire, read elite telegraphs |
| `SwarmRing` | swarm on a closed ring around the hero at the threat ring's inner radius | break out through a gap, dash |
| `RusherStampede` | rushers on one arc of the threat ring, wind-ups staggered | sidestep a volley of charge lines |
| `TurretNest` | turrets in a cluster on the threat ring | weave aimed bursts, push in to kill |
| `Boss` | the Warden (`boss.md`) | the boss fight |

Code: `UDTDirector` (`Enemies/`), `FDTTimelineRow`, `UDTTimelineAsset` (generated, `DA_Timeline_Arena1`), `FDTEnemySpawn{Role Position HpScale DmgScale bElite bArrival BeatId}` handed to the enemy subsystem, `FDTDropInMarker{Role Position LandStep}` published to `FDTWorldView::DropInMarkers` for rendering. Gems are stored by `progression.md`'s pickup system; the director only emits `GemDrop{Position, Value}`. Debug (not in Shipping, R-TEC-12): `DT.Dir.Time <s>` jumps the director clock, `DT.Dir.Stats` logs the block, credit, live markers and alive shares.

## Acceptance
- T-DIR-01 PLANNED(S1) `Tools/Bot` no-pick arm, `C.DIR.BOT_RUNS` seeds: the median death time lies in `C.DIR.NOPICK_DEATH_BAND_S` (a bot that never picks cards is overwhelmed around minute ten).
- T-DIR-02 PLANNED(S1) HUMAN-measured: in S1 validation sessions, a steered human build's `damage_taken_per_min` over `C.DIR.BAND_WINDOW_S` lands in `C.DMG_TAKEN.BAND_MIN10_13` and its `dashes_per_min` in `C.DASHES.BAND_MIN10_13`, read from the session CSV by `Tools/pressure.py` (to be written with the bot).
- T-DIR-03 PLANNED(S1) `Dusktracer.Director.ClockAndQueue`: the director follows sim steps, not wall time; spawns appear in the next `Enemies` phase; a paused sim advances nothing.
- T-DIR-04 PLANNED(S1) `Dusktracer.Director.Blocks`: a beat ends at its duration or the step its event enemies die; Breath follows for `C.DIR.BREATH_S` with no regular spawn and no drop-in; the shipped timeline's beats plus Breath all end before `C.RUN.LENGTH_TO_BOSS`.
- T-DIR-05 PLANNED(S1) `Dusktracer.Director.Spawns`: the credit never exceeds one; a role at its share cap is never drawn; at the alive cap nothing spawns and nothing is banked; every position lies in the ring band or was moved inside the arena.
- T-DIR-06 PLANNED(S1) `Dusktracer.Director.ThreatPlacement`: every Threat row of every timeline gives Rusher plus Turret at least `C.DIR.THREAT_SHARE_MIN`; threat-role spawns in a beat and every turret spawn lie on the threat ring.
- T-DIR-07 PLANNED(S1) `Dusktracer.Director.DropIn`: a marker is published, the enemy appears `C.DIR.DROPIN_LEAD_S` later at the marker with `bArrival`, bullets fired at the marker hit nothing; cadence halves by `C.DIR.DROPIN_THREAT_MULT` in a beat.
- T-DIR-08 PLANNED(S1) `Dusktracer.Director.DropInFairness`: never more live markers than the limit; landings at least `C.DIR.DROPIN_GAP_MIN_S` apart; none in Breath or from boss time; a blocked drop-in waits, and only one waits.
- T-DIR-09 PLANNED(S1) `Dusktracer.Director.KillHorizon`: headless run with an instant-kill debug build and an invulnerable hero; threat actions within the threat ring per minute over `C.DIR.BAND_WINDOW_S` reach `C.DIR.THREAT_ACTIONS_MIN`.
- T-DIR-10 PLANNED(S1) `Dusktracer.Director.Scaling`: spawns at two times carry the scales of their spawn minute and keep them; the stamps match the formulas from the generated constants.
- T-DIR-11 PLANNED(S1) `Dusktracer.Director.Events`: each event spawns its count with the beat tag; killing them all ends the beat early.
- T-DIR-12 PLANNED(S1) `Dusktracer.Director.GemDrops`: one gem per director kill with the role value (elite factor applied); none for debug spawns; at `C.DROP.GEMS_MAX` the value merges into the nearest gem and the total is kept.
- T-DIR-13 PLANNED(S1) `Dusktracer.Director.BossHandoff`: at boss time spawns, beats and drop-ins stop, markers clear, one boss request is made.
- T-DIR-14 PLANNED(S1) `Dusktracer.Director.Determinism`: same seed and inputs give the same spawn and drop-in sequence; another seed differs; drawing from the director stream leaves the `Spawn` stream's sequence unchanged.
- T-DIR-15 PLANNED(S1) `Dusktracer.Director.BlockTelemetry`: one `dir_block` record per ended block with counts that sum to the run totals.
- HUMAN: a validator can say, unprompted, when a Threat started and when it ended; they see a red marker before every drop-in lands.

## Telemetry
- `dir_block{block phase row_start_s start_s end_s ended_by spawned_by_role{} skipped_at_cap dropins threat_actions kills kill_horizon_p50_uu alive_peak}` per block (R-DIR-15); `kill_horizon_p50_uu` is the median distance from the killing player's hero at each kill; per player inside the record: `dmg_taken{owner: share_of_max_hp}`, `dashes{owner: n}`.
- CSV column `threat_actions_per_min` (`telemetry.md`) fed from threat actions that start within the threat ring of that player's hero; `nearest_enemy_uu` already exists.
- `run_end` gains nothing; the boss request is a `dir_block` with phase `Boss`.

## Text
- Prefix `ST_UI.Hud.*`, en + zh-Hans, budget two rows: `Hud.Threat` (beat start banner, optional), `Hud.BossIncoming`. Markers and Breath carry no text.

## Out of scope
- Heat and how it changes skill checks (S5); other arenas' timelines; Bruiser, Support and later roles (S2+).
- Enemy behaviour, telegraph shapes and durations, elite affixes, enemy bullets and aim tokens (`enemies.md`); the boss fight (`boss.md`).
- Gem pickup, magnet, XP curve and level timing (`progression.md`); chests and elite card hands (`offers.md`); co-op spawn rate per player count (`coop.md`).

## Open
- `EDTRngStream::Director` is new: `07_Technical` R-TEC-02 lists the streams → one line there when the director lands.
- The `Arrival` flag needs each S1 role's arrival form (Rusher charge line, Turret burst) → resolved by `enemies.md`; a role without one keeps a zero drop-in weight.
- Timeline numbers are the old 15-minute timeline reshaped to four roles and unmeasured here → resolved by Bot v1 runs (T-DIR-01, T-DIR-09) and the first S1 sessions (T-DIR-02).
- Whether drop-in cadence should adapt to the measured kill horizon instead of the timeline → decided after T-DIR-02 data; the fixed cadence stays the floor.
