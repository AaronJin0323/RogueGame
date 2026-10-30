# Boss — the Warden template (S1: form I)

## Intent
The run ends in one fight of pure pattern reading: telegraphed charges, then bullet rings that always leave a readable lane, then rings with a spiral inside a shrinking arena (power with teeth: threats are skill checks, never HP walls; `00_Vision §2`). The Warden changes form across wins so the player feels they are uncovering something; every later boss fills the same template (form, phase table, ring slots, lane rule, HP formula, telemetry).

## Rules
- **R-BOS-01** Form by wins: the form is III at or above `C.BOSS.FORM_III_WINS` cumulative wins, II at or above `C.BOSS.FORM_II_WINS`, otherwise I; Heat and anything inside the run never change it. A form whose rows are missing in its catalogue plays form I and is counted. S1 ships form I only; form II (a phase-two counter to the player's main archetype) and form III are S2.
- **R-BOS-02** Every form is identifiable within `C.BOSS.IDENTIFY_S` of the fight start: it has its own silhouette (kitbash) and fires its signature attack (the catalogue row marked `signature`) inside that window.
- **R-BOS-03** The hull is one slot of role `Boss` in the enemy pool (R-ENM-01, never an Actor): weapons target it, bullets hit it through `ResolveHits`, its death is one `Kill` event to the killing hit's owner, and its stale handles follow R-ENM-23. The boss system moves it by script (no steering, no separation, and every `FDTDisplacement` naming it is ignored), keeps it inside the hazard ring, and keeps it at least the catalogue standoff from its target hero in phase I. Contact deals `C.BOSS.CONTACT_DAMAGE` through R-ENM-14; radius `C.BOSS.RADIUS_UU`.
- **R-BOS-04** Phases come from `09_Catalog/boss_warden.csv` and only advance: a phase starts when the effective HP fraction falls to its threshold or the fight time reaches its latest start, whichever comes first; one burst through several thresholds enters each phase in order. Each phase change starts a shift of `C.BOSS.PHASE_SHIFT_S` with no attack.
- **R-BOS-05** Every attack is a catalogue row naming its telegraph shape; it obeys R-ENM-13 (red projected telegraph, at least `C.ENEMIES.TELEGRAPH_LEAD_MIN_S` before damage, footprint equal to the damage shape). Boss rounds are enemy fire (R-ENM-15, R-ENM-16, R-ENM-17; source role `Boss`) with radius `C.BOSS.BULLET_RADIUS_UU`; aim tokens (R-ENM-18) do not limit the boss.
- **R-BOS-06** Ring slots: a ring of `count` slots puts its rounds at equal angles around the ring origin (the shoulder mirror heads in form I); the slot phase is a pure function of the fight step and the target bearing, never a random draw.
- **R-BOS-07** Dodge lane: every ring leaves one lane of contiguous empty slots whose chord at `C.BOSS.LANE_MEASURE_UU` from the origin is at least `C.BOSS.LANE_HERO_DIAMETERS` hero diameters plus two round diameters, centred within `±C.BOSS.LANE_BEARING_MAX_DEG` of the target hero's bearing (a crossing ring's lane is placed by R-BOS-08 instead), and drawn as the gap in the ring's telegraph. A spiral skips a rotating sector of the same width. A ring is always passable on foot.
- **R-BOS-08** No safe spot: every ring in phases that fire crossing rings is followed after `C.BOSS.CROSS_RING_DELAY_S` by a second ring whose lane is `C.BOSS.CROSS_LANE_OFFSET_DEG` away from the first, so a stationary hero is hit by one of the pair, and a moving hero can walk or dash through.
- **R-BOS-09** HP from build DPS: the reserve HP is `C.BOSS.HP_BASE` times `C.BOSS.HP_MAX_MULT`; the fight starts on an effective max HP of `C.BOSS.HP_BASE`. After `C.BOSS.CALIBRATE_S` of fight time the effective max is set once to (damage taken by the Warden over that window ÷ `C.BOSS.CALIBRATE_S`) × `C.BOSS.TARGET_FIGHT_S`, clamped to `C.BOSS.HP_MIN_MULT` … `C.BOSS.HP_MAX_MULT` times `C.BOSS.HP_BASE`; damage already dealt carries over. The director's HP multiplier never applies to the boss.
- **R-BOS-10** Fight bound: `C.BOSS.TARGET_FIGHT_S` is below `C.RUN.BOSS_MAX`, and each phase's latest start is below `C.RUN.BOSS_MAX`, so every build sees every phase; at `C.RUN.BOSS_MAX` the hazard ring has reached its minimum radius. No instant kill, no HP added after calibration.
- **R-BOS-11** Statuses on the boss: Freeze is refused; Chill stacks stop at `C.STATUS.BOSS_CHILL_STACK_MAX`; Shock and Burn apply as R-ENM-22.
- **R-BOS-12** The boss draws only from the `Boss` stream (R-TEC-02); the same seed and the same hero inputs give a bit-identical fight.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Fight cap | C.RUN.BOSS_MAX | CORE | R-BOS-10, T-BOS-10 |
| Form thresholds | C.BOSS.FORM_II_WINS, C.BOSS.FORM_III_WINS | GATED(S2) | R-BOS-01, T-BOS-01 |
| Identification window | C.BOSS.IDENTIFY_S | GATED(S1) | R-BOS-02, T-BOS-02 |
| Hull radius, contact damage | C.BOSS.RADIUS_UU, C.BOSS.CONTACT_DAMAGE | GATED(S1) | R-BOS-03 |
| Phase shift | C.BOSS.PHASE_SHIFT_S | GATED(S1) | R-BOS-04, T-BOS-04 |
| Round radius | C.BOSS.BULLET_RADIUS_UU | GATED(S1) | R-BOS-05, R-BOS-07 |
| Lane width, distance, bearing | C.BOSS.LANE_HERO_DIAMETERS, C.BOSS.LANE_MEASURE_UU, C.BOSS.LANE_BEARING_MAX_DEG | GATED(S1) | R-BOS-07, T-BOS-07 |
| Crossing ring | C.BOSS.CROSS_RING_DELAY_S, C.BOSS.CROSS_LANE_OFFSET_DEG | GATED(S1) | R-BOS-08, T-BOS-08 |
| HP formula | C.BOSS.HP_BASE, C.BOSS.CALIBRATE_S, C.BOSS.TARGET_FIGHT_S, C.BOSS.HP_MIN_MULT, C.BOSS.HP_MAX_MULT | GATED(S1) | R-BOS-09, T-BOS-09 |
| Telegraph lead | C.ENEMIES.TELEGRAPH_LEAD_MIN_S | GATED(S1) | R-BOS-05 |
| Boss Chill cap | C.STATUS.BOSS_CHILL_STACK_MAX | GATED(S1) | R-BOS-11 |
| Form I behaviour (from the catalogue notes; walk speed and round life ours) | C.BOSS.WALK_SPEED_UU_S, C.BOSS.OPENING_WALK_S, C.BOSS.CHARGE_RECOVER_S, C.BOSS.STANDOFF_HULL_RADII, C.BOSS.SWEEP_RANGE_UU, C.BOSS.SPIRAL_LANE_RATE_RATIO, C.BOSS.ADDS_ALIVE_MAX, C.BOSS.HAZARD_MIN_RADIUS_UU, C.BOSS.HAZARD_GRACE_S, C.BOSS.ROUND_LIFE_S | GATED(S1) | R-BOS-02, R-BOS-03, R-BOS-07, R-BOS-10, T-BOS-08 |

Per-phase and per-attack numbers (thresholds, latest starts, wind-ups, intervals, slots, speeds, damage, extents) are rows of `09_Catalog/boss_warden.csv`, not prose.

## Data
| Field | Type | Notes |
|---|---|---|
| `FDTBossFormDef` | data asset per form | form id, kitbash mesh, catalogue rows, signature attack |
| `boss_warden.csv` row | catalogue | `form phase enter_hp_frac latest_start_s attack telegraph signature windup_s interval_s count speed_uu_s damage extent_uu arc_deg notes` |
| `FDTBossBrain` | world-free state | `Phase`, `ShiftLeft`, `Action`, `ActionLeft`, `FightTime`, `RingPhase`, `LaneBearing`, `SpiralAngle`, `HazardRadius` |
| HP state | per fight | `ReserveHp`, `EffectiveMax`, `DamageInWindow`, `bCalibrated`; the bar and phases read the effective fraction |
| `FDTTelegraph` | `enemies.md` | ring telegraph with a lane gap; charge corridor; sweep sector; hazard ring |
| `Wins` | meta save | cumulative wins (form choice); owned by the meta save |

Phase table (form I; numbers in `09_Catalog/boss_warden.csv`):

| Phase | Enters | Attacks | Skill check |
|---|---|---|---|
| I | fight start | charge along a hull-width corridor; sweep sector when the hero is close | read the line, step aside |
| II | HP threshold or latest start | crossing radial rings from the shoulders; Tank adds (exempt from role shares) | find the lane, walk or dash the crossing |
| III | HP threshold or latest start | faster rings; two-arm spiral with a rotating lane; shrinking hazard ring | pattern reading in a closing space |

Debug (not in Shipping): `DT.Boss [Form]` starts the fight, `DT.Boss.Phase <n>` jumps, `DT.Boss.Stats` logs phase, effective HP and attack counts.

## Acceptance
- T-BOS-01 `Dusktracer.Boss.FormByWins`: wins below, at and above each threshold pick the right form; Heat never changes it; a form without rows plays form I and is counted.
- T-BOS-02 `Dusktracer.Boss.Identify`: the signature attack fires within the identification window from fight start in every phase-I opening the seed set produces.
- T-BOS-03 `Dusktracer.Boss.HullInPool`: the hull is one pool slot with no actor; bullets damage it; its death is one kill to the killing owner; it stays inside the hazard ring.
- T-BOS-04 `Dusktracer.Boss.Phases`: thresholds and latest starts advance phases in order, never back; a burst through two thresholds reports both; no attack starts during a shift.
- T-BOS-05 `Dusktracer.Boss.Telegraphs`: every attack row in `boss_warden.csv` names a telegraph; every attack shows it at least the minimum lead before damage with a matching footprint.
- T-BOS-06 `Dusktracer.Boss.RingSlots`: a ring spawns its count of rounds at equal angles minus its lane; two runs with the same seed and inputs give the same slots.
- T-BOS-07 `Dusktracer.Boss.DodgeLane`: for every ring and spiral the free chord at the measure distance is at least the lane width, the lane is within the bearing limit of the target hero, and the telegraph gap matches it.
- T-BOS-08 `Dusktracer.Boss.NoSafeSpot`: a stationary hero at any bearing and distance inside the arena is hit by a crossing pair; the crossing lane's offset is within its range.
- T-BOS-09 `Dusktracer.Boss.Calibrate`: with a constant incoming DPS the effective max equals the formula; very low and very high DPS clamp; dealt damage carries over; calibration happens once.
- T-BOS-10 `Dusktracer.Boss.FightBound`: with zero incoming damage every phase starts before `C.RUN.BOSS_MAX` and the hazard ring is at its minimum then.
- T-BOS-11 `Dusktracer.Boss.Statuses`: Freeze refused; Chill stops at its cap; Shock and Burn apply.
- T-BOS-12 `Dusktracer.Boss.Determinism`: same seed and scripted hero give bit-identical boss positions, rounds and HP.
- Also `Dusktracer.Boss.FullFight` (a headless fight at a fixed DPS: calibrated HP within the clamp, phases in order, every ring's lane at least the rule, the fight ends before `C.RUN.BOSS_MAX`, same seed same fight) and `Dusktracer.Boss.World` (the boss steps after the enemy pool in a game world; its telegraphs are published).
- HUMAN: a validator who has never seen the Warden names its phase-II and phase-III skill checks after one fight, says every ring had a way through, and does not describe the fight as a damage sponge.

## Telemetry
- JSONL records (`telemetry.md` format, `owner` `-1`): `boss_start{form, wins, players}`, `boss_phase{phase, fight_time_s, hp_frac, by_time}`, `boss_calibrated{window_dps, effective_max, clamped}`.
- `boss_end{outcome, fight_time_s, hp_left_frac, phase, per_player{owner: {damage_taken_by_attack{}, dashes, hits_per_ring}}}`.
- `ThreatAction{Role Boss, AttackId}` per attack started (same event as `enemies.md`).

## Text
- Prefix `ST_Enemies.Enemy.Boss.*`: the boss name (`Enemy.Boss.Name`, The Warden) and one row per phase banner if the HUD shows one, en + zh-Hans. The form is never named in text before its first win.

## Out of scope
- Forms II and III (S2): build counter, retreat behaviour, Keeper-colour phase, Codex unlock. Other arenas' bosses (later stages, same template).
- Pausing regular spawns for the fight and the run's win screen: `director.md` and the run flow. Damage taken and dash: `combat_core.md`.
- Control on the boss converting into a vulnerability: S2 (S1 refuses Freeze, caps Chill, ignores displacement).

## Open
- The boss kills too few players in the reference (every build that reached it won) → first S1 sessions set the target share of boss deaths; tune ring interval and damage rows, never HP.
- Two heroes: the lane follows one target hero → co-op rule when `coop.md` is written.
- `C.BOSS.HP_BASE` is the reference project's value, not ours → re-measured from S1 bot fights once weapons land.
- Built (S1): ring slots put `slot 0` exactly on the target hero's position at telegraph time (the slot phase is the target bearing); a first ring's lane takes a grid position inside the bearing limit chosen by a hash of the fight step (pure, R-BOS-06); the Boss stream draws only the crossing offset and the adds' angles. The first ring of a pair comes from one shoulder head, its crossing ring from the other.
- R-BOS-07 versus R-BOS-08: the crossing ring's lane is placed from the first ring's lane, so it is never within `C.BOSS.LANE_BEARING_MAX_DEG` of a hero standing in the first lane; the code applies the bearing limit to first rings and spirals only → reword R-BOS-07 to say so.
- T-BOS-08 holds because `slot 0` of every crossing ring flies at the target; "any distance" is bounded by the round range (`C.BOSS.ROUND_LIFE_S` times the ring speed), which covers the hazard arena; tested on eight bearings at three distances.
- The behaviour numbers of the catalogue `notes` column (doc-only, D-014) became `C.BOSS.*` constants; `C.BOSS.WALK_SPEED_UU_S` and `C.BOSS.ROUND_LIFE_S` are ours and unmeasured → first S1 sessions.
- Text: the boss name is `ST_UI` `Hud.BossName` (HUD text) until an `ST_Enemies` table exists → move it to `Enemy.Boss.Name` then.
- `boss_end` `damage_taken_by_attack` sums `FDTHeroHit` damage before the hero's own reductions (the boss cannot see them).
- The hazard ring is centred on the arena centre; a phase change does not cancel attacks already telegraphed, and a crossing ring completes its pair through a shift.
- Run end on the boss's death: `UDTBossSubsystem` logs `Run end reason=victory`, broadcasts `OnBossDefeated` and ends the telemetry session; `ADTGameMode` (run over) and `ADTHUD` (`DrawBossBar`) hook it. Wins come from the meta save (S3), zero until then.
