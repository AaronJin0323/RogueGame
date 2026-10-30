# Enemies — roles, enemy fire, statuses (S0 swarm core + S1 roles)

## Intent
Fill the arena with bodies for the player's bullet hell to cut through, and make every threat a readable skill check, never an HP wall (power with teeth; the player is the bullet hell, `00_Vision §2`). Four roles in S1: Swarm (harvest), Tank (flank or pierce), Rusher (dodge a projected line), Turret (weave through warm fire). Every enemy and every enemy round is data on the fixed step (`07_Technical §1`); this spec owns what each role does, the director (`director.md`) owns when and how many.

## Rules
### Pool and swarm (S0)
- **R-ENM-01** Swarm enemies are data in `FDTEnemyPool` (struct of arrays, never Actors, R-TEC-11). The pool holds at most `C.ENEMIES.CAP` enemies; a spawn beyond the cap is refused and counted, never queued or dropped silently.
- **R-ENM-02** Each step (`EDTSimPhase::Enemies`) every enemy steers toward the nearest hero in `FDTWorldView::HeroPositions`, accelerates by at most `C.ENM.STEER_ACCEL_UU_S2`, never exceeds its role speed (`C.ENM.SWARM_SPEED_UU_S` for the swarm), and stays inside `±C.ARENA.HALF_EXTENT_UU` less its radius.
- **R-ENM-03** Separation: an enemy is pushed away from neighbours closer than `C.ENM.SEPARATION_RADIUS_UU`, scaled by `C.ENM.SEPARATION_STRENGTH`. Neighbours come from an `FDTSpatialHash` built from positions at the start of the step (R-TEC-13), at most `C.ENM.MAX_NEIGHBOURS` per enemy, so the step stays linear in the population and independent of update order.
- **R-ENM-04** After moving, the pool publishes `EnemyPositions`, `EnemyRadii` (dense; index = enemy slot) and rebuilds `EnemyHash` in the view, so the bullet phase collides against this step's positions.
- **R-ENM-05** In `EDTSimPhase::ResolveHits` every `FDTHit` is applied: a `Hit` event with the damage actually dealt goes to the hit's owner, and a death emits exactly one `Kill` event to the owner of the killing hit, through `FDTRunState::ApplyEvent` (R-TEC-01). The hit list is then cleared.
- **R-ENM-06** An enemy dies at most once per step: hits after the killing hit, and hits naming a slot that does not exist, are ignored (no damage, no event).
- **R-ENM-07** Indices change only in `ResolveHits`, after all hits are consumed: dead enemies are swap-removed and every survivor keeps all its fields.
- **R-ENM-08** Randomness in the enemy system comes only from the `Spawn` stream (R-TEC-02); the same seed and the same inputs give bit-identical positions.

### Roles (S1)
- **R-ENM-09** Every enemy has a role (`Enemy.Swarm`, `Enemy.Tank`, `Enemy.Rusher`, `Enemy.Turret`) whose stats come from `FDTEnemyRoleDef` (generated `C.ROLE.*`, catalogue `09_Catalog/enemies.csv`). Role behaviour is a per-enemy state machine stepped in `EDTSimPhase::Enemies` from pool fields only (no Actor, no timer outside the step); the director's spawn-time HP and damage multipliers are stored per enemy and applied to the role base.
- **R-ENM-10** Tank: moves along its facing, which turns toward its target at most `C.ROLE.TANK_TURN_DEG_S`. A hit whose contact point lies within `±C.ROLE.TANK_FRONT_HALF_ARC_DEG` of the facing deals `C.ROLE.TANK_FRONT_DAMAGE_MULT` of its damage, unless the bullet pierces (spawned with `Pierce` above one), which ignores the plate. Skill check: flank or pierce.
- **R-ENM-11** Rusher: Approach → within `C.ROLE.RUSHER_TRIGGER_UU` of its target with its cooldown spent → WindUp for `C.ROLE.RUSHER_WINDUP_S`, standing still, direction locked at its start → Charge at `C.ROLE.RUSHER_CHARGE_SPEED_UU_S` for `C.ROLE.RUSHER_CHARGE_S` (no steering, no separation) → Recover `C.ROLE.RUSHER_RECOVER_S` → Approach with `C.ROLE.RUSHER_COOLDOWN_S` cooldown. The charge band telegraph has exactly the charge length and the Rusher's collision width.
- **R-ENM-12** Turret: static (speed zero, not moved by separation); its head turns toward its target at most `C.ROLE.TURRET_HEAD_TURN_DEG_S`. Within `C.ROLE.TURRET_FIRE_RANGE_UU`, holding an aim token (R-ENM-18) and with its period spent, it shows its aim line for `C.ROLE.TURRET_TELL_S`, then fires its pattern (`Turret.Burst` or `Turret.Fan`, chosen per spawn by the director) along the aim locked at the end of the tell; the next tell starts no sooner than `C.ROLE.TURRET_PERIOD_S` after the previous one.
- **R-ENM-13** Every enemy attack (Rusher charge, Turret burst, boss attacks) shows its red projected telegraph (`Pal.Telegraph`, `04_Presentation` R-PRE-03) at least `C.ENEMIES.TELEGRAPH_LEAD_MIN_S` before its damage can land, and the telegraph footprint equals the damage shape. An attack that cannot show its full lead does not fire. Swarm and Tank have no attack: their threat is their body.
- **R-ENM-14** Contact: an enemy whose circle overlaps a hero's body appends one `FDTHeroHit` per step with its role contact damage times its damage multiplier (a charging Rusher deals its contact damage). Hurt windows, dash immunity and damage taken belong to `combat_core.md`.

### Enemy fire (S1)
- **R-ENM-15** Enemy rounds are data in their own pool `FDTEnemyBulletPool`, owned by the enemy system: the same struct-of-arrays layout, dense swap-removal, integration and expiry as `FDTBulletPool` (R-BUL-01, R-BUL-02, R-BUL-06 apply as written), stepped in `EDTSimPhase::Enemies` after movement. It holds at most `C.ENEMY_FIRE.CAP` rounds; a spawn at the cap is refused and counted, never recycling a round in flight. Never an Actor per round.
- **R-ENM-16** Enemy rounds collide only with heroes: each live round is tested as a circle against every hero in `HeroPositions` (body radius from `combat_core.md`). A round touching a hero appends one `FDTHeroHit` (that hero's player, round damage, source role, attack id) and is removed; a round touching a hero flagged intangible in `HeroIntangible` passes through and lives on. Enemy rounds never hit enemies and never enter `EnemyHash`.
- **R-ENM-17** Enemy rounds are drawn by one instanced mesh on their own renderer (never the player-bullet ISM): a `Pal.Enemy.BulletCore` dark core inside a `Pal.Enemy.Bullet` rim at `C.ENEMY_FIRE.EMISSIVE` (R-PRE-02). Density LOD never dims them (R-PRE-06).
- **R-ENM-18** Aim tokens: at most `C.AIM.TOKENS` ranged enemies (`C.AIM.TOKENS_THREAT` while the director flags a Threat block) are in their tell or firing at one player at once. At each step start free tokens go to ready units that fired least recently (ties by serial, R-ENM-23); a unit without a token holds before its tell, so every shot still follows its full tell.
- **R-ENM-19** No fan within `C.AIM.FAN_MIN_RANGE_UU`: a ranged unit closer than that to its target fires its pattern as single aimed rounds.
- **R-ENM-20** Every aimed fan puts one round on a stationary player: a fan of odd count is centred on the aim; a fan of even count is shifted half a step so one round flies exactly along the aim, alternating sides round by round within a burst so the burst keeps its width and reads symmetric.

### Statuses (S1)
- **R-ENM-21** Statuses `Status.Shock`, `Status.Burn`, `Status.Chill`, `Status.Freeze` are pool fields applied from `FDTWorldView::StatusApplies` (written by Keeper effects, `keepers.md`) at the start of the next Enemies phase, before movement; an application naming a stale handle (R-ENM-23) is ignored. Shock lasts `C.STATUS.SHOCK_S`; Burn lasts `C.STATUS.BURN_S` and a refresh keeps the stronger damage per second, never stacking; Chill adds a stack lasting `C.STATUS.CHILL_S`, and the stack that reaches `C.STATUS.FREEZE_STACKS` consumes all stacks and applies Freeze for `C.STATUS.FREEZE_S`; Chill is refused while frozen. Each status remembers the owner and source of its last application.
- **R-ENM-22** Status effects: a Shocked enemy takes `C.STATUS.SHOCK_DAMAGE_TAKEN` more damage from every hit (applied in `ResolveHits`); Burn appends an `FDTHit` every `C.STATUS.BURN_TICK_S` with its owner and source (the last partial tick is flushed, so a full Burn deals damage per second times duration), so DoT kills go through R-ENM-05; each Chill stack slows movement by `C.STATUS.CHILL_SLOW_PER_STACK`; a frozen enemy neither moves, turns, nor advances its behaviour timers (a Rusher's wind-up pauses), and contact damage is unchanged.

### Identity and look (S1)
- **R-ENM-23** Slot generations: every spawn gets a `Serial` from a per-pool counter (never zero, never reused within a run), kept through swap-removal (R-ENM-07) and published as `EnemySerials`. An `FDTEnemyHandle{Index, Serial}` is valid only while the serial at that index matches. `FDTHit` carries the serial; `ResolveHits` ignores a hit with a stale handle (as R-ENM-06). Anything that remembers an enemy across steps (bullet hit memory, status applications, aim-token order, Keeper targets) keys it by serial, never by bare index.
- **R-ENM-24** Every enemy is drawn as two instanced parts per role: a hull in `Pal.Enemy.Body` without emissive, and a separate always-lit optic in `Pal.Enemy.Optic` at `C.ENEMIES.OPTIC_EMISSIVE` (R-PRE-04), placed where the role's model language puts it. The optic, not a tinted hull, carries readability; `C.ENM.OPTIC_TINT` no longer tints the hull once the optic part draws.

Render (S0, kept): one instanced mesh per part on a transient renderer actor, interpolating between the last two steps, facing the velocity (Tank: its facing; Turret: its head), at `C.SIM.PLANE_Z_UU`.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Live cap | C.ENEMIES.CAP | CORE | R-ENM-01, T-ENM-01 |
| Swarm radius, speed, HP | C.ENM.SWARM_RADIUS_UU, C.ENM.SWARM_SPEED_UU_S, C.ENM.SWARM_HP | GATED(S1) | T-ENM-02, C.TTK.SWARM_STEERED_MAX |
| Steering, separation, neighbours | C.ENM.STEER_ACCEL_UU_S2, C.ENM.SEPARATION_RADIUS_UU, C.ENM.SEPARATION_STRENGTH, C.ENM.MAX_NEIGHBOURS | GATED(S0/S1) | R-ENM-02, T-ENM-03, T-PERF-01 |
| Debug spawn ring | C.ENM.SPAWN_RING_RADIUS_UU | GATED(S1) | `DT.Spawn` |
| Hull tint (S0 stand-in for the optic) | C.ENM.OPTIC_TINT | GATED(S1) | R-ENM-24 |
| Emissive strength (S0 hull) | C.ENM.EMISSIVE_STRENGTH | GATED(S1) | 04_Presentation §2 |
| Kill-rate window | C.ENM.KILL_RATE_WINDOW_S | GATED(S0) | C.PRE.DENSITY_LOD_KILLS_PER_S |
| Optic and telegraph emissive | C.ENEMIES.OPTIC_EMISSIVE, C.ENEMIES.TELEGRAPH_EMISSIVE | GATED(S1) | R-ENM-24, R-ENM-13 |
| Minimum telegraph lead | C.ENEMIES.TELEGRAPH_LEAD_MIN_S | GATED(S1) | R-ENM-13, T-ENM-13, boss.md |
| Role stats and behaviour | C.ROLE.* | GATED(S1) | R-ENM-09…12, R-ENM-14, `09_Catalog/enemies.csv` |
| Role soft-cap shares | C.ROLE.TANK_SHARE_MAX, C.ROLE.RUSHER_SHARE_MAX, C.ROLE.TURRET_SHARE_MAX | GATED(S1) | director.md |
| Enemy-fire pool, rounds, patterns | C.ENEMY_FIRE.* | GATED(S1) | R-ENM-15…17, R-ENM-20 |
| Aim tokens, close-fan range, test ranges | C.AIM.* | GATED(S1) | R-ENM-18…20 |
| Status durations and effects | C.STATUS.* | GATED(S1) | R-ENM-21, R-ENM-22, boss.md |

## Data
| Field | Type | Notes |
|---|---|---|
| `Pos`, `PrevPos`, `Vel` | `FVector2f[]` | this step, previous step (render interpolation), uu per second |
| `Radius`, `Speed`, `MaxHp`, `Hp` | `float[]` | from the role; `Hp` at or below zero marks the enemy dead this step |
| `Role` | `EDTEnemyRole[]` | `Swarm Tank Rusher Turret` (`Boss` for the Warden hull, `boss.md`) |
| `Serial` | `uint32[]` | identity across steps (R-ENM-23) |
| `HpMul`, `DmgMul` | `float[]` | director multipliers at spawn time |
| `Facing`, `State`, `StateLeft`, `Cooldown`, `AimDir`, `PatternId`, `LastFireStep` | per enemy | role state machine (R-ENM-10…12, R-ENM-18) |
| status fields | per enemy | `StatusMask`, `ShockLeft`, `BurnLeft`, `BurnDps`, `BurnTickLeft`, `ChillStacks`, `ChillLeft`, `FreezeLeft`, last owner and source per status |
| `FDTEnemyRoleDef` | struct | role stats from `C.ROLE.*`; contact damage; attack and telegraph ids (`09_Catalog/enemies.csv`) |
| `FDTEnemyBulletPool` | SoA | `Pos PrevPos Vel Radius Damage LifeLeft TargetPlayer SourceRole AttackId` |
| `FDTHeroHit` | struct | `PlayerId Damage Position SourceRole AttackId`; appended to `FDTWorldView::HeroHits`, consumed and cleared by `combat_core.md` |
| `FDTStatusApply` | struct | `Handle Status Magnitude OwnerId SourceId`; appended to `FDTWorldView::StatusApplies` |
| `FDTTelegraph` | struct | `Shape(Band Ring Arc AimLine) Origin Dir Length Width Progress`; published per step, drawn in `Pal.Telegraph` at `C.ENEMIES.TELEGRAPH_EMISSIVE` |
| view additions | `FDTWorldView` | `EnemySerials`, `HeroHits`, `HeroIntangible` (written by the hero phase), `StatusApplies`, `bThreatBlock` (written by the director) |
| instance custom data | `float[4]` | `0-2` linear RGB, `3` emissive strength (`M_InstanceColor`; fallback `Fallback_EnemyMaterial`) |

Pool decision: a separate pool, not a flag in `FDTBulletPool`. A shared pool would let player fire at `C.BULLETS.CAP` recycle an enemy round out of the air (R-BUL-03), mix two collision targets in one hot loop (R-BUL-05 queries the enemy hash; enemy rounds test a few heroes), and put both sides in one ISM so density LOD could not dim one without the other (R-PRE-06). Same fixed step, same SoA, own cap.

Model language (grey box carries the silhouette and the optic; art book PL.C02–C09 is the detail reference):

| Role | Silhouette from above | Optic | Telegraph | Readability requirement |
|---|---|---|---|---|
| Swarm | small "Y" delta drone | nose | none | reads as a flock; optic visible at the cap |
| Tank | squat tracked frame + a tall plate; a horizontal bright edge | periscope over the plate | none | the bright plate edge shows the reduced-damage arc |
| Rusher | long shuttle, nose = charge direction | nose strip, turns `Pal.Telegraph` during the wind-up | ground band, charge length × collision width | the band is on the ground for the whole wind-up |
| Turret | hexagonal disc on a mast | mirror head | aim line to the locked aim | the head's facing shows the aim before the line |

Debug (not in Shipping, R-TEC-12): `DT.Spawn <N> [Role]`, `DT.Spawn.Keep <N>`, `DT.Spawn.Clear`, `DT.Enemies.Stats`, `DT.EnemyFire.Stats`, `DT.Status <Status> [N]`. Requests apply at the start of the next Enemies phase.

## Acceptance
- T-ENM-01 `Dusktracer.Enemies.Cap`: the pool fills to the cap; further single and ring spawns are refused and counted; a full pool steps and publishes.
- T-ENM-02 `Dusktracer.Enemies.Seek`: each enemy moves toward the nearer of two heroes; speed never exceeds the cap; an enemy pulled outward stays in the arena.
- T-ENM-03 `Dusktracer.Enemies.Separation`: two overlapping enemies (and two coincident ones) are pushed apart; a lone enemy gets no sideways push.
- T-ENM-04 `Dusktracer.Enemies.Publish`: published positions and radii equal the moved pool; the hash finds every enemy at its moved position.
- T-ENM-05 `Dusktracer.Enemies.KillOwner`: with two players, kills and damage route to the hit's owner; hits are cleared; the dead enemy is removed.
- T-ENM-06 `Dusktracer.Enemies.DiesOnce`: several lethal hits on one enemy in one step give one kill to the first owner; a stale index is ignored.
- T-ENM-07 `Dusktracer.Enemies.Compaction`: after killing several slots, every survivor keeps its own data and each appears once.
- T-ENM-08 `Dusktracer.Enemies.Determinism`: same seed gives bit-identical positions; another seed differs.
- T-ENM-09 PLANNED(S1) `Dusktracer.Enemies.RoleDefs`: each role spawns with its `C.ROLE.*` stats times its multipliers; role state machines advance only in the Enemies phase.
- T-ENM-10 PLANNED(S1) `Dusktracer.Enemies.TankPlate`: a hit inside the front arc deals the reduced damage, one outside it full damage, a piercing hit full damage; the facing never turns faster than the limit.
- T-ENM-11 PLANNED(S1) `Dusktracer.Enemies.RusherCharge`: the state sequence and durations; the charge covers exactly the telegraphed band; the direction does not change after the wind-up starts.
- T-ENM-12 PLANNED(S1) `Dusktracer.Enemies.TurretBurst`: no fire beyond range; tell, then the pattern along the locked aim; tell starts are at least the period apart.
- T-ENM-13 PLANNED(S1) `Dusktracer.Enemies.TelegraphLead`: for every attack in `09_Catalog/enemies.csv` the telegraph appears at least the minimum lead before damage and its footprint equals the damage shape.
- T-ENM-14 PLANNED(S1) `Dusktracer.Enemies.Contact`: an overlapping enemy appends one hero hit per step with role contact damage times its multiplier; a non-overlapping one appends none.
- T-ENM-15 PLANNED(S1) `Dusktracer.EnemyFire.Pool`: integration and expiry as T-BUL-01/02; at the cap a spawn is refused and counted and no live round is removed; player bullets at their cap never remove an enemy round.
- T-ENM-16 PLANNED(S1) `Dusktracer.EnemyFire.HitHero`: a round touching a hero gives one hero hit to that player and is removed; an intangible hero is passed through; rounds never append an `FDTHit`.
- T-ENM-17 PLANNED(S1) `Dusktracer.EnemyFire.NoActors`: rounds at the cap add no actor and one instance each on the enemy-fire renderer; instance colours equal the enemy tokens at every density.
- T-ENM-18 PLANNED(S1) `Dusktracer.Enemies.AimTokens`: many turrets over a long window never exceed the token count (Threat count in a Threat block); every turret still fires; a unit without a token holds before its tell.
- T-ENM-19 PLANNED(S1) `Dusktracer.Enemies.NoCloseFan`: a fan pattern fires single rounds inside the close range and a fan outside it.
- T-ENM-20 PLANNED(S1) `Dusktracer.Enemies.AimedFansHitStationary`: fans of every count up to `C.AIM.FAN_TEST_COUNT_MAX`, both sides, against a stationary hero across `C.AIM.TEST_RANGE_UU`: the nearest round's miss is within hero body radius plus round radius.
- T-ENM-21 PLANNED(S1) `Dusktracer.Enemies.StatusApply`: durations, Burn refresh keeps the stronger, Chill stacks reach Freeze and are consumed, Chill refused while frozen, stale handles ignored.
- T-ENM-22 PLANNED(S1) `Dusktracer.Enemies.StatusEffect`: Shock raises damage taken; a full Burn deals exactly damage per second times duration and its kill goes to the burner; Chill slows per stack; Freeze stops movement and pauses a wind-up; contact damage unchanged.
- T-ENM-23 PLANNED(S1) `Dusktracer.Enemies.SlotGenerations`: after an enemy dies and its slot is refilled (by swap and by a new spawn), an old handle is invalid, a hit with the old serial is ignored, and a bullet that hit the old enemy can hit the new one.
- T-ENM-24 PLANNED(S1) `Dusktracer.Enemies.OpticPart`: every live enemy has exactly one hull and one optic instance; the hull colour equals `Pal.Enemy.Body` with zero emissive; the optic equals `Pal.Enemy.Optic`.
- HUMAN: in PIE with `DT.Spawn` per role, a validator names each role from above by silhouette and optic alone, sees every Rusher band and Turret aim line before damage, flanks a Tank, and weaves a Turret burst; enemy rounds are never confused with player rounds.

## Telemetry
- `Hit{OwnerId, Amount, SourceId}` and `Kill{OwnerId, Role}` through `FDTRunState::ApplyEvent`; Burn ticks carry the burner's source.
- `ThreatAction{Role, AttackId}` per charge started and burst fired (feeds `threat_actions_per_min`, `telemetry.md`).
- `HeroHit{PlayerId, Amount, SourceRole, AttackId}` per hero hit (damage taken by role and attack; applied amount is `combat_core.md`'s).
- `DT.Enemies.Stats`: live by role, cap, kills, kills per second, refused spawns, stale hits ignored, statuses applied by kind. `DT.EnemyFire.Stats`: live, cap, spawned, refused, hero hits, passed through, expired, token waits.

## Text
- Prefix `ST_Enemies.Enemy.<Role>.Name` (one row per role: Scrapwing, Bulwark, Courser, Flare Pylon), en + zh-Hans; fiction never becomes a key. Status names live in the keyword table owned by `keepers.md`.

## Out of scope
- Elite affixes, Bruiser and Support roles, drop-in arrivals: S2.
- Spawn pacing, role mix over time, HP and damage scaling, which pattern a Turret gets: `director.md`. Damage taken, hurt windows, dash: `combat_core.md`. Status magnitudes and when Keepers apply them: `keepers.md`.
- Vertex animation; density LOD of kill effects (uses the kill-rate counter).

## Open
- Swarm radius, speed and HP are placeholders until the time-to-kill target is measured → S1 sessions against `C.TTK.SWARM_STEERED_MAX`.
- `bullets.md` R-BUL-04 still says hit memory holds enemy slots → its owner rewrites it to serials (R-ENM-23) when the S1 bullet change lands.
- Kill horizon: strong builds kill Rushers before `C.ROLE.RUSHER_TRIGGER_UU` → `director.md` (drop-ins, threat ring); a longer trigger is a TUNE after the first S1 session.
- Turret radius has no measured source → first S1 capture of the Turret grey box.
