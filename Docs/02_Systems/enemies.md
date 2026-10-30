# Enemies — roles, enemy fire, statuses (S0 swarm core + S1 roles)

## Intent
Fill the arena with bodies for the player's bullet hell to cut through, and make every threat a readable skill check, never an HP wall (power with teeth; the player is the bullet hell, `00_Vision §2`). Four roles in S1: Swarm (harvest), Tank (flank or pierce), Rusher (dodge a projected line), Turret (weave through warm fire). Every enemy and every enemy round is data on the fixed step (`07_Technical §1`); this spec owns what each role does, the director (`director.md`) owns when and how many.

## Rules
### Pool and swarm (S0)
- **R-ENM-01** Swarm enemies are data in `FDTEnemyPool` (struct of arrays, never Actors, R-TEC-11). The pool holds at most `C.ENEMIES.CAP` enemies; a spawn beyond the cap is refused and counted, never queued or dropped silently.
- **R-ENM-02** Each step (`EDTSimPhase::Enemies`) every enemy steers toward the nearest hero in `FDTWorldView::HeroPositions`, accelerates by at most `C.ENM.STEER_ACCEL_UU_S2`, never exceeds its role speed (`C.ENM.SWARM_SPEED_UU_S` for the swarm), and stays inside `±C.ARENA.HALF_EXTENT_UU` less its radius. Queued `Displacements` (`07_Technical §4`) are added to positions before steering, also clamped to the arena; a frozen enemy is still displaced.
- **R-ENM-03** Separation: an enemy is pushed away from neighbours closer than `C.ENM.SEPARATION_RADIUS_UU`, scaled by `C.ENM.SEPARATION_STRENGTH`. Neighbours come from an `FDTSpatialHash` built from positions at the start of the step (R-TEC-13), at most `C.ENM.MAX_NEIGHBOURS` per enemy, so the step stays linear in the population and independent of update order.
- **R-ENM-04** After moving, the pool publishes `EnemyPositions`, `EnemyRadii` (dense; index = enemy slot) and rebuilds `EnemyHash` in the view, so the bullet phase collides against this step's positions.
- **R-ENM-05** In `EDTSimPhase::ResolveHits` every `FDTHit` is applied: a `Hit` event with the damage actually dealt goes to the hit's owner, and a death emits exactly one `Kill` event to the owner of the killing hit, through `FDTRunState::ApplyEvent` (R-TEC-01), and appends one `FDTKill` to `FDTWorldView::Kills` (`07_Technical §4`). The hit list is then cleared.
- **R-ENM-06** An enemy dies at most once per step: hits after the killing hit, and hits naming a slot that does not exist, are ignored (no damage, no event).
- **R-ENM-07** Indices change only in `ResolveHits`, after all hits are consumed: dead enemies are swap-removed and every survivor keeps all its fields; the pool then republishes positions, radii and serials and rebuilds `EnemyHash`, so Effects reads valid indices.
- **R-ENM-08** Randomness in the enemy system comes only from the `Spawn` stream (R-TEC-02); the same seed and the same inputs give bit-identical positions.

### Roles (S1)
- **R-ENM-09** Every enemy has a role (`Enemy.Swarm`, `Enemy.Tank`, `Enemy.Rusher`, `Enemy.Turret`) whose stats come from `FDTEnemyRoleDef` (generated `C.ROLE.*`, catalogue `09_Catalog/enemies.csv`). Role behaviour is a per-enemy state machine stepped in `EDTSimPhase::Enemies` from pool fields only (no Actor, no timer outside the step); the director's spawn-time `HpScale` and `DmgScale` (`director.md` R-DIR-10) are stored per enemy and applied to the role base. An elite (`bElite`) has `C.ROLE.ELITE_HP_MULT` times that HP and is otherwise its role (affixes are S2).
- **R-ENM-10** Tank: moves along its facing, which turns toward its target at most `C.ROLE.TANK_TURN_DEG_S`. A hit whose contact point lies within `±C.ROLE.TANK_FRONT_HALF_ARC_DEG` of the facing deals `C.ROLE.TANK_FRONT_DAMAGE_MULT` of its damage, unless the bullet pierces (spawned with `Pierce` above one), which ignores the plate. Skill check: flank or pierce.
- **R-ENM-11** Rusher: Approach → within `C.ROLE.RUSHER_TRIGGER_UU` of its target with its cooldown spent → WindUp for `C.ROLE.RUSHER_WINDUP_S`, standing still, direction locked at its start → Charge at `C.ROLE.RUSHER_CHARGE_SPEED_UU_S` for `C.ROLE.RUSHER_CHARGE_S` (no steering, no separation) → Recover `C.ROLE.RUSHER_RECOVER_S` → Approach with `C.ROLE.RUSHER_COOLDOWN_S` cooldown. The charge band telegraph has exactly the charge length and the Rusher's collision width.
- **R-ENM-12** Turret: static (speed zero, not moved by separation); its head turns toward its target at most `C.ROLE.TURRET_HEAD_TURN_DEG_S`. Within `C.ROLE.TURRET_FIRE_RANGE_UU`, holding an aim token (R-ENM-18) and with its period spent, it shows its aim line for `C.ROLE.TURRET_TELL_S`, then fires its pattern (`Turret.Burst` or `Turret.Fan`, drawn per spawn from the role's `fire_pattern` list in `enemies.csv` with the `Spawn` stream) along the aim locked at the end of the tell; the next tell starts no sooner than `C.ROLE.TURRET_PERIOD_S` after the previous one.
- **R-ENM-13** Every enemy attack (Rusher charge, Turret burst, boss attacks) shows its red projected telegraph (`Pal.Telegraph`, `04_Presentation` R-PRE-03) at least `C.ENEMIES.TELEGRAPH_LEAD_MIN_S` before its damage can land, and the telegraph footprint equals the damage shape. An attack that cannot show its full lead does not fire. Swarm and Tank have no attack: their threat is their body. Arrival (`bArrival`, a director drop-in, `director.md` R-DIR-07): a Rusher spawns in WindUp aimed at its nearest hero, a Turret spawns in its tell when it gets a token (else it holds as R-ENM-18); an arrival's attack whose telegraph is showing completes even if the unit dies: the charge band (as `Kind` Hazard) or the burst resolves from the telegraph at the step it would have landed.
- **R-ENM-14** Contact: an enemy whose circle overlaps a hero's body appends one `FDTHeroHit` per step with its role contact damage times its damage multiplier (a charging Rusher deals its contact damage). Hurt windows, dash immunity and damage taken belong to `combat_core.md`.

### Enemy fire (S1)
- **R-ENM-15** Enemy rounds are data in their own pool `FDTEnemyBulletPool`, owned by the enemy system: the same struct-of-arrays layout, dense swap-removal, integration and expiry as `FDTBulletPool` (R-BUL-01, R-BUL-02, R-BUL-06 apply as written), stepped in `EDTSimPhase::Enemies` after movement. It holds at most `C.ENEMY_FIRE.CAP` rounds; a spawn at the cap is refused and counted, never recycling a round in flight. Never an Actor per round.
- **R-ENM-16** Enemy rounds collide only with heroes: each live round is tested as a circle against every hero in `HeroPositions` (body radius from `combat_core.md`). A round touching a hero appends one `FDTHeroHit` (`Kind` Bullet, `07_Technical §4`) and is removed; a round touching a hero flagged intangible in `HeroIntangible` passes through and lives on. Enemy rounds never hit enemies and never enter `EnemyHash`.
- **R-ENM-17** Enemy rounds are drawn by one instanced mesh on their own renderer (never the player-bullet ISM): a `Pal.Enemy.BulletCore` dark core of `C.ENEMY_FIRE.CORE_RADIUS_RATIO` of the round's seen radius inside a `Pal.Enemy.Bullet` rim at `C.ENEMY_FIRE.EMISSIVE` (R-PRE-02), one unlit material (`M_EnemyRound`, the core's linear RGB in custom data `4-6`), so neither sun nor highlight lifts the core. A round is drawn at `C.ENEMY_FIRE.DRAW_RADIUS_RATIO` of its hit radius: at gameplay camera the hit circle is a few pixels, too few to show a core, and a round that looks larger than it hits errs toward the player. Density LOD never dims them (R-PRE-06).
- **R-ENM-18** Aim tokens: at most `C.AIM.TOKENS` ranged enemies (`C.AIM.TOKENS_THREAT` while the director flags a Threat block) are in their tell or firing at one player at once. At each step start free tokens go to ready units that fired least recently (ties by serial, R-ENM-23); a unit without a token holds before its tell, so every shot still follows its full tell.
- **R-ENM-19** No fan within `C.AIM.FAN_MIN_RANGE_UU`: a ranged unit closer than that to its target fires its pattern as single aimed rounds.
- **R-ENM-20** Every aimed fan puts one round on a stationary player: a fan of odd count is centred on the aim; a fan of even count is shifted half a step so one round flies exactly along the aim, alternating sides round by round within a burst so the burst keeps its width and reads symmetric.

### Statuses (S1)
- **R-ENM-21** This spec owns status state and behaviour; `keepers.md` decides when statuses are applied (R-KEE-09 to R-KEE-12). Statuses `Status.Shock`, `Status.Burn`, `Status.Chill`, `Status.Freeze` are pool fields applied from `FDTWorldView::StatusApplies` at the start of the next Enemies phase, before movement; an application naming a stale handle (R-ENM-23) is ignored. Shock lasts `C.STATUS.SHOCK_S`; a new Shock refreshes it and never stacks. Each Burn application is a stack with its own `C.STATUS.BURN_S` timer and its own damage per second; stacks cap at `C.STATUS.BURN_STACKS_MAX` plus the application's `StackBonus` (never above `C.STATUS.BURN_STACK_SLOTS`), and a stack past the cap replaces the oldest. Chill adds the application's stacks and refreshes a shared `C.STATUS.CHILL_S` timer; reaching the threshold (`FreezeAt`, default `C.STATUS.FREEZE_STACKS`) consumes all stacks and applies Freeze for `C.STATUS.FREEZE_S` × `FreezeScale`; Chill is refused while frozen. Each status remembers the owner and source of its last application (each Burn stack its own).
- **R-ENM-22** Status effects: a Shocked enemy takes `C.STATUS.SHOCK_DAMAGE_TAKEN` more damage from every hit (applied in `ResolveHits`); every Burn stack appends an `FDTHit` every `C.STATUS.BURN_TICK_S` with its owner and source (the last partial tick is flushed, so a full stack deals its damage per second times duration), so DoT kills go through R-ENM-05; each Chill stack slows movement by `C.STATUS.CHILL_SLOW_PER_STACK`; a frozen enemy neither moves, turns, nor advances its behaviour timers (a Rusher's wind-up pauses), and contact damage is unchanged.

### Identity and look (S1)
- **R-ENM-23** Slot generations: every spawn gets a `Serial` from a per-pool counter (never zero, never reused within a run), kept through swap-removal (R-ENM-07) and published as `EnemySerials`. An `FDTEnemyHandle{Index, Serial}` is valid only while the serial at that index matches. `FDTHit` carries the serial; `ResolveHits` ignores a hit with a stale handle (as R-ENM-06). Anything that remembers an enemy across steps (bullet hit memory, status applications, aim-token order, Keeper targets) keys it by serial, never by bare index.
- **R-ENM-24** Every enemy is drawn as two instanced parts per role: a hull in `Pal.Enemy.Body` without emissive, and a separate always-lit optic in `Pal.Enemy.Optic` at `C.ENEMIES.OPTIC_EMISSIVE` (R-PRE-04), placed where the role's model language puts it. The optic, not a tinted hull, carries readability.

Render (S0, kept): one instanced mesh per part on a transient renderer actor, interpolating between the last two steps, facing the velocity (Tank: its facing; Turret: its head), at `C.SIM.PLANE_Z_UU`.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Live cap | C.ENEMIES.CAP | CORE | R-ENM-01, T-ENM-01 |
| Swarm radius, speed, HP | C.ENM.SWARM_RADIUS_UU, C.ENM.SWARM_SPEED_UU_S, C.ENM.SWARM_HP | GATED(S1) | T-ENM-02, C.TTK.SWARM_STEERED_MAX |
| Steering, separation, neighbours | C.ENM.STEER_ACCEL_UU_S2, C.ENM.SEPARATION_RADIUS_UU, C.ENM.SEPARATION_STRENGTH, C.ENM.MAX_NEIGHBOURS | GATED(S0/S1) | R-ENM-02, T-ENM-03, T-PERF-01 |
| Debug spawn ring | C.ENM.SPAWN_RING_RADIUS_UU | GATED(S1) | `DT.Spawn` |
| Kill-rate window | C.ENM.KILL_RATE_WINDOW_S | GATED(S0) | C.PRE.DENSITY_LOD_KILLS_PER_S |
| Optic and telegraph emissive, telegraph height | C.ENEMIES.OPTIC_EMISSIVE, C.ENEMIES.TELEGRAPH_EMISSIVE, C.ENEMIES.TELEGRAPH_Z_UU | GATED(S1) | R-ENM-24, R-ENM-13 |
| Minimum telegraph lead | C.ENEMIES.TELEGRAPH_LEAD_MIN_S | GATED(S1) | R-ENM-13, T-ENM-13, boss.md |
| Role stats and behaviour | C.ROLE.* | GATED(S1) | R-ENM-09…12, R-ENM-14, `09_Catalog/enemies.csv` |
| Elite HP factor | C.ROLE.ELITE_HP_MULT | MEASURED | R-ENM-09, T-ENM-09 (role share caps are `C.DIR.SHARE_MAX_*`, `director.md`) |
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
| `HpScale`, `DmgScale` | `float[]` | director multipliers at spawn time (`FDTEnemySpawn`, `director.md`) |
| `PullReadyStep` | `int64[]` | first sim step a `Flag.Pull` may reach this enemy again (`keepers.md` R-KEE-21); swapped with the slot |
| `Flags`, `BeatId` | per enemy | `Elite Arrival Director` (debug spawns lack `Director` and drop nothing); beat tag; copied into `FDTKill` |
| `Facing`, `State`, `StateLeft`, `Cooldown`, `AimDir`, `PatternId`, `LastFireStep` | per enemy | role state machine (R-ENM-10…12, R-ENM-18) |
| status fields | per enemy | `StatusMask`, `ShockLeft`, `Burn[C.STATUS.BURN_STACK_SLOTS]{Left Dps TickLeft Owner Source}`, `BurnCap`, `ChillStacks`, `ChillLeft`, `FreezeLeft`, last owner and source per status |
| `FDTEnemyRoleDef` | struct | role stats from `C.ROLE.*`; contact damage; attack and telegraph ids (`09_Catalog/enemies.csv`) |
| `FDTEnemyBulletPool` | SoA | `Pos PrevPos Vel Radius Damage LifeLeft TargetPlayer SourceRole AttackId` |
| `FDTHeroHit`, `FDTHit`, `FDTKill`, `FDTDisplacement` | structs | `07_Technical §4` |
| `FDTStatusApply` | struct | `Handle Status Magnitude OwnerId SourceId StackBonus FreezeAt FreezeScale`; `Magnitude` = Burn damage per second or Chill stacks; appended to `FDTWorldView::StatusApplies` (owned here) |
| `FDTTelegraph` | struct | `Shape(Band Ring Arc AimLine) Origin Dir Length Width Progress`; published per step, drawn in `Pal.Telegraph` at `C.ENEMIES.TELEGRAPH_EMISSIVE` in the layers of `04_Presentation` R-PRE-05 |
| view fields | `FDTWorldView` | writers and readers: `07_Technical §4` |
| instance custom data | `float[4]` | `0-2` linear RGB, `3` emissive strength (`M_InstanceColor`, telegraphs the unlit `M_CueColor`; fallback `Fallback_EnemyMaterial`); enemy rounds add `4-6` core linear RGB (`M_EnemyRound`, R-ENM-17) |

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
- T-ENM-09 `Dusktracer.Enemies.RoleDefs`: each role spawns with its `C.ROLE.*` stats times its multipliers, an elite with the elite HP factor; role state machines advance only in the Enemies phase.
- T-ENM-10 `Dusktracer.Enemies.TankPlate`: a hit inside the front arc deals the reduced damage, one outside it full damage, a piercing hit full damage; the facing never turns faster than the limit.
- T-ENM-11 `Dusktracer.Enemies.RusherCharge`: the state sequence and durations; the charge covers exactly the telegraphed band; the direction does not change after the wind-up starts.
- T-ENM-12 `Dusktracer.Enemies.TurretBurst`: no fire beyond range; tell, then the pattern along the locked aim; tell starts are at least the period apart.
- T-ENM-13 `Dusktracer.Enemies.TelegraphLead`: for every attack in `09_Catalog/enemies.csv` the telegraph appears at least the minimum lead before damage and its footprint equals the damage shape; an arriving Rusher and Turret start their attack on their first step, and one killed mid-telegraph still lands it.
- T-ENM-14 `Dusktracer.Enemies.Contact`: an overlapping enemy appends one hero hit per step with role contact damage times its multiplier; a non-overlapping one appends none.
- T-ENM-15 `Dusktracer.EnemyFire.Pool`: integration and expiry as T-BUL-01/02; at the cap a spawn is refused and counted and no live round is removed; player bullets at their cap never remove an enemy round.
- T-ENM-16 `Dusktracer.EnemyFire.HitHero`: a round touching a hero gives one hero hit to that player and is removed; an intangible hero is passed through; rounds never append an `FDTHit`.
- T-ENM-17 `Dusktracer.EnemyFire.NoActors`: rounds at the cap add no actor and one instance each on the enemy-fire renderer; instance colours equal the enemy tokens at every density.
- T-ENM-18 `Dusktracer.Enemies.AimTokens`: many turrets over a long window never exceed the token count (Threat count in a Threat block); every turret still fires; a unit without a token holds before its tell.
- T-ENM-19 `Dusktracer.Enemies.NoCloseFan`: a fan pattern fires single rounds inside the close range and a fan outside it.
- T-ENM-20 `Dusktracer.Enemies.AimedFansHitStationary`: fans of every count up to `C.AIM.FAN_TEST_COUNT_MAX`, both sides, against a stationary hero across `C.AIM.TEST_RANGE_UU`: the nearest round's miss is within hero body radius plus round radius.
- T-ENM-21 `Dusktracer.Enemies.StatusApply`: durations; Burn stacks to its cap (with a bonus, never past the slots) and replaces the oldest; Chill stacks reach Freeze (default and lowered threshold) and are consumed; Chill refused while frozen; stale handles ignored.
- T-ENM-22 `Dusktracer.Enemies.StatusEffect`: Shock raises damage taken; a full Burn deals exactly damage per second times duration and its kill goes to the burner; Chill slows per stack; Freeze stops movement and pauses a wind-up; contact damage unchanged.
- T-ENM-23 `Dusktracer.Enemies.SlotGenerations`: after an enemy dies and its slot is refilled (by swap and by a new spawn), an old handle is invalid, a hit with the old serial is ignored, and a bullet that hit the old enemy can hit the new one.
- T-ENM-24 `Dusktracer.Enemies.OpticPart`: every live enemy has exactly one hull and one optic instance; the hull colour equals `Pal.Enemy.Body` with zero emissive; the optic equals `Pal.Enemy.Optic`.
- T-ENM-25 `Dusktracer.Enemies.TelegraphRender`: every published telegraph draws on one instanced renderer (a band or aim line one quad over its length and width, a ring or arc tessellated around its origin) from `C.ENEMIES.TELEGRAPH_Z_UU`, in the layers of `04_Presentation` R-PRE-05: a `Pal.Telegraph.Edge` band outside the footprint, a `Pal.Telegraph` fill ramping with `Progress`, a `Pal.Telegraph` rim on the footprint at `C.ENEMIES.TELEGRAPH_EMISSIVE`; the instance count follows `Telegraphs` every frame, an empty arena draws none, and no actor is added.
- T-ENM-26 `Dusktracer.Enemies.ThreatTelemetry`: a Turret burst inside the threat ring of the hero is forwarded from the enemy subsystem to telemetry and fills `threat_actions_per_min`; without the call the column stays zero.
- HUMAN: in PIE with `DT.Spawn` per role, a validator names each role from above by silhouette and optic alone, sees every Rusher band and Turret aim line before damage, flanks a Tank, and weaves a Turret burst; enemy rounds are never confused with player rounds.

## Telemetry
- `Hit{OwnerId, Amount, SourceId}` and `Kill{OwnerId, Role}` through `FDTRunState::ApplyEvent`; Burn ticks carry the burner's source.
- `ThreatAction{Role, AttackId}` per charge started and burst fired (feeds `threat_actions_per_min`, `telemetry.md`).
- `DT.Enemies.Stats`: live by role, cap, kills, kills per second, refused spawns, stale hits ignored, statuses applied by kind. `DT.EnemyFire.Stats`: live, cap, spawned, refused, hero hits, passed through, expired, token waits.

## Text
- Prefix `ST_Enemies.Enemy.<Role>.Name` (one row per role: Scrapwing, Bulwark, Courser, Flare Pylon), en + zh-Hans; fiction never becomes a key. Status names live in the keyword table owned by `keepers.md`.

## Out of scope
- Elite affixes, Bruiser and Support roles: S2.
- Spawn pacing, role mix and share caps over time, HP and damage scaling, when and where elites and arrivals come: `director.md`. Damage taken, hurt windows, dash: `combat_core.md`. When Keepers apply statuses, and Burn magnitude: `keepers.md`.
- Vertex animation; density LOD of kill effects (uses the kill-rate counter).

## Open
- Swarm radius, speed and HP are placeholders until the time-to-kill target is measured → S1 sessions against `C.TTK.SWARM_STEERED_MAX`.
- Kill horizon: strong builds kill Rushers before `C.ROLE.RUSHER_TRIGGER_UU` → `director.md` (drop-ins, threat ring); a longer trigger is a TUNE after the first S1 session.
- Turret radius has no measured source → first S1 capture of the Turret grey box.
- S1 implementation deviations (non-behavioural):
  - Telegraphs draw as flat quads of one instanced plane (`ADTTelegraphRenderer`). Ring and Arc have no producer: the renderer reads Ring as outer radius `Length`, thickness `Width` (none = filled disc) and Arc as a filled sector of radius `Length` around `Dir` whose full angle in degrees is `Width` → confirm or change with `boss.md`.
  - `HeroHits` is reset at the start of the Enemies phase (the hero has read it in the Hero phase), so the list stays bounded while no hero system is registered.
  - The optic part's size is `C.ENEMIES.OPTIC_SCALE`.
