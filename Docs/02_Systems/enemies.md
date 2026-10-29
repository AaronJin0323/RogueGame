# Enemies — swarm simulation (S0)

## Intent
Fill the arena with enough bodies for the player's bullet hell to mean something: up to `C.ENEMIES.CAP` swarm enemies that close in on the nearest hero without stacking, die in one clean event that names its killer, and cost almost nothing per instance (pillar: power with teeth, `00_Vision §2.1`; bullets are data, `07_Technical §1`).

## Rules
- **R-ENM-01** Swarm enemies are data in `FDTEnemyPool` (struct of arrays, never Actors, R-TEC-11). The pool holds at most `C.ENEMIES.CAP` enemies; a spawn beyond the cap is refused and counted, never queued or dropped silently.
- **R-ENM-02** Each step (`EDTSimPhase::Enemies`) every enemy steers toward the nearest hero in `FDTWorldView::HeroPositions`, accelerates by at most `C.ENM.STEER_ACCEL_UU_S2`, never exceeds its role speed (`C.ENM.SWARM_SPEED_UU_S` for the swarm), and stays inside `±C.ARENA.HALF_EXTENT_UU` less its radius.
- **R-ENM-03** Separation: an enemy is pushed away from neighbours closer than `C.ENM.SEPARATION_RADIUS_UU`, scaled by `C.ENM.SEPARATION_STRENGTH`. Neighbours come from an `FDTSpatialHash` built from positions at the start of the step (R-TEC-13), at most `C.ENM.MAX_NEIGHBOURS` per enemy, so the step stays linear in the population and independent of update order.
- **R-ENM-04** After moving, the pool publishes `EnemyPositions`, `EnemyRadii` (dense; index = enemy slot) and rebuilds `EnemyHash` in the view, so the bullet phase collides against this step's positions.
- **R-ENM-05** In `EDTSimPhase::ResolveHits` every `FDTHit` is applied: a `Hit` event with the damage actually dealt goes to the hit's owner, and a death emits exactly one `Kill` event to the owner of the killing hit, through `FDTRunState::ApplyEvent` (R-TEC-01). The hit list is then cleared.
- **R-ENM-06** An enemy dies at most once per step: hits after the killing hit, and hits naming a slot that does not exist, are ignored (no damage, no event).
- **R-ENM-07** Indices change only in `ResolveHits`, after all hits are consumed: dead enemies are swap-removed and every survivor keeps all its fields.
- **R-ENM-08** Randomness in the enemy system comes only from the `Spawn` stream (R-TEC-02); the same seed and the same inputs give bit-identical positions.

Render (not a rule with a test yet): the subsystem draws every enemy with one instanced mesh on a transient renderer actor, interpolating between the last two steps, facing its velocity, at `C.SIM.PLANE_Z_UU`.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Live cap | C.ENEMIES.CAP | CORE | R-ENM-01, T-ENM-01 |
| Swarm radius | C.ENM.SWARM_RADIUS_UU | GATED(S1) | render scale, bullet collision |
| Swarm speed | C.ENM.SWARM_SPEED_UU_S | GATED(S1) | T-ENM-02 |
| Swarm HP | C.ENM.SWARM_HP | GATED(S1) | C.TTK.SWARM_STEERED_MAX |
| Steering acceleration | C.ENM.STEER_ACCEL_UU_S2 | GATED(S1) | R-ENM-02 |
| Separation radius | C.ENM.SEPARATION_RADIUS_UU | GATED(S0) | T-ENM-03 |
| Separation strength | C.ENM.SEPARATION_STRENGTH | GATED(S0) | T-ENM-03 |
| Neighbours per enemy | C.ENM.MAX_NEIGHBOURS | GATED(S0) | step cost, T-PERF-01 |
| Debug spawn ring | C.ENM.SPAWN_RING_RADIUS_UU | GATED(S1) | `DT.Spawn` |
| Optic tint of the body | C.ENM.OPTIC_TINT | GATED(S1) | 04_Presentation §3 |
| Emissive strength | C.ENM.EMISSIVE_STRENGTH | GATED(S1) | 04_Presentation §2 |
| Kill-rate window | C.ENM.KILL_RATE_WINDOW_S | GATED(S0) | C.PRE.DENSITY_LOD_KILLS_PER_S |

## Data
| Field | Type | Notes |
|---|---|---|
| `Pos`, `PrevPos` | `FVector2f[]` | this step and the previous one (render interpolation) |
| `Vel` | `FVector2f[]` | uu per second; render faces it |
| `Radius`, `Speed`, `MaxHp` | `float[]` | from the role (`FDTEnemyRoleDef`, generated constants) |
| `Hp` | `float[]` | at or below zero marks the enemy dead this step |
| `Role` | `EDTEnemyRole[]` | `Swarm` only in S0 |
| refused spawns, total kills | `int64` | `DT.Enemies.Stats` |
| kill rate | `FDTKillRateCounter` | kills per second over `C.ENM.KILL_RATE_WINDOW_S` |
| instance custom data | `float[4]` | `0-2` linear RGB, `3` emissive strength (`M_InstanceColor`; fallback `Fallback_EnemyMaterial`) |

Body colour: `Pal.Enemy.Body` mixed toward `Pal.Enemy.Optic` by `C.ENM.OPTIC_TINT`, read from `DTPal::` (generated from `04_Presentation §3`).

Debug (not in Shipping, R-TEC-12): `DT.Spawn <N>`, `DT.Spawn.Keep <N>`, `DT.Spawn.Clear`, `DT.Enemies.Stats`. Requests are queued and applied at the start of the next Enemies phase.

## Acceptance
- T-ENM-01 `Dusktracer.Enemies.Cap`: the pool fills to the cap; further single and ring spawns are refused and counted; a full pool steps and publishes.
- T-ENM-02 `Dusktracer.Enemies.Seek`: each enemy moves toward the nearer of two heroes; speed never exceeds the cap; an enemy pulled outward stays in the arena.
- T-ENM-03 `Dusktracer.Enemies.Separation`: two overlapping enemies (and two coincident ones) are pushed apart; a lone enemy gets no sideways push.
- T-ENM-04 `Dusktracer.Enemies.Publish`: published positions and radii equal the moved pool; the hash finds every enemy at its moved position.
- T-ENM-05 `Dusktracer.Enemies.KillOwner`: with two players, kills and damage route to the hit's owner; hits are cleared; the dead enemy is removed.
- T-ENM-06 `Dusktracer.Enemies.DiesOnce`: several lethal hits on one enemy in one step give one kill to the first owner; a stale index is ignored.
- T-ENM-07 `Dusktracer.Enemies.Compaction`: after killing several slots, every survivor keeps its own data and each appears once.
- T-ENM-08 `Dusktracer.Enemies.Determinism`: same seed gives bit-identical positions; another seed differs.
- HUMAN: in PIE, `DT.Spawn.Keep` at the cap shows a dense ring closing on the hero without stacking; enemies read against both ground references by their tint.

## Telemetry
- `Hit{OwnerId, Amount}` and `Kill{OwnerId}` through `FDTRunState::ApplyEvent` (per-player totals).
- `DT.Enemies.Stats` log line: live, cap, keep target, kills, kills per second, refused spawns.

## Text
- None: the enemy system shows no player-facing text in S0.

## Out of scope
- Roles other than the swarm, elites and their lights, enemy fire and telegraphs, statuses, director pacing and spawn timelines (S1).
- Vertex animation, the optic as a separate always-lit part, density LOD of kill effects (uses the kill-rate counter, S1).

## Open
- Swarm radius, speed and HP are placeholders until the time-to-kill target is measured → resolved by S1 sessions against `C.TTK.SWARM_STEERED_MAX`.
