# Bullets — S0 player-bullet core

## Intent
The player is the bullet hell: thousands of player rounds must be live at once, collide against the swarm and stay readable at budget (`00_Vision §2`, `07_Technical §1`). Bullets are data, not Actors: a struct-of-arrays pool stepped in the fixed-step sim, collided through the shared enemy hash and drawn as instances.

## Rules
- **R-BUL-01** Live bullets are stored densely in `FDTBulletPool` (struct of arrays, indices `[0, Num)`); removal swaps the last bullet into the hole, and one `Step` advances every live bullet exactly once, never skipping or repeating one.
- **R-BUL-02** A bullet is removed in the step its life reaches zero or its position leaves the square `±C.ARENA.HALF_EXTENT_UU`; each removal is counted by cause.
- **R-BUL-03** At most `C.BULLETS.CAP` bullets are live. A spawn at the cap replaces the oldest live bullet and increments the `Recycled` counter; a spawn is never dropped silently.
- **R-BUL-04** A bullet hits distinct enemies: it may hit at most its `Pierce` enemies, never hits the same enemy twice while that enemy is among its last `C.BULLETS.HIT_MEMORY` hits (remembered by enemy `Serial`, `enemies.md`), and is removed the step its pierce is exhausted.
- **R-BUL-05** Collision queries `FDTWorldView::EnemyHash` (R-TEC-13) with the bullet radius plus the largest enemy radius in the view, then tests circles exactly; each contact appends exactly one `FDTHit` carrying the bullet's `OwnerId` and damage. The bullet system never applies damage itself (that is `EDTSimPhase::ResolveHits`).
- **R-BUL-06** The step is deterministic: the same spawns against the same enemies give bit-identical positions and the same hit sequence.
- **R-BUL-07** Bullets are drawn by one `UInstancedStaticMeshComponent` on one transient `ADTBulletRenderer` per world (R-TEC-11): never an Actor or component per bullet. Each frame the instances interpolate `PrevPos → Pos` with the sim alpha at height `C.SIM.PLANE_Z_UU`; instances are added or removed only at the tail when the live count changes, and transforms are written in one batch.
- **R-BUL-08** Every bullet carries a telemetry source id (`FDTBulletSpawn::SourceId`, `FDTTelemetrySources`, `telemetry.md` R-TEL-05; `Unknown` when untagged), kept through swap-removal and set anew when its slot is recycled; every `FDTHit` it appends carries that id. `DT.Bullets` stress emitters spawn with `Stress`.
- **R-BUL-09** A round may carry a motion (`FDTBulletMotion`); a round without one stays a straight line and costs one byte test. The pool runs it in `Step`, between life and integration, so a moving round is still data: *homing* turns the velocity toward its target at no more than `TurnDegS`, keeping its speed; a dead or missing target is replaced by the nearest enemy within `SeekUu`, else it flies on. *Orbit* holds the round on a circle around its owner's hero for the hold and on release re-aims it once: at the boss if one lives (`FDTWorldView::BossPosition`), else at the nearest enemy within `ReleaseRange`, else on along the orbit. *Boomerang* turns home when its life falls to `ReturnAt` (hit memory cleared, pierce restored, so the way home strikes what the way out struck), or at the hit that would spend it (memory kept, pierce restored); it steers to the hero and ends as an expiry on reaching it. A mirrored moving round reports its position, velocity, life and pierce each step before collision (`GetRoundReports`), and the weapon system reads that rather than redoing the motion. Determinism (R-BUL-06) holds.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Live bullet cap | C.BULLETS.CAP | CORE | R-BUL-03, T-BUL-03 |
| Arena half extent | C.ARENA.HALF_EXTENT_UU | GATED(S1) | R-BUL-02, T-BUL-02 |
| Hash cell | C.HASH.CELL_UU | GATED(S0) | R-BUL-05 |
| Sim plane height | C.SIM.PLANE_Z_UU | CORE | R-BUL-07 |
| Step rate | C.SIM.FIXED_HZ | CORE | R-BUL-01 |
| Hit memory per bullet | C.BULLETS.HIT_MEMORY | GATED(S1) | R-BUL-04, T-BUL-04 |
| Player bullet emissive | C.BULLETS.EMISSIVE_PLAYER | GATED(S1) | R-BUL-07, `04_Presentation` R-PRE-01 |
| Stress bullet radius | C.BULLETS.STRESS_RADIUS_UU | GATED(S0) | `DT.Bullets` |
| Stress bullet speed | C.BULLETS.STRESS_SPEED_UU_S | GATED(S0) | `DT.Bullets` |
| Stress bullet life | C.BULLETS.STRESS_LIFE_S | GATED(S0) | `DT.Bullets` |
| Stress pierce | C.BULLETS.STRESS_PIERCE | GATED(S0) | `DT.Bullets` |
| Stress damage | C.BULLETS.STRESS_DAMAGE | GATED(S0) | `DT.Bullets` |
| Stress ring spin | C.BULLETS.STRESS_SPIN_DEG_S | GATED(S0) | `DT.Bullets` |

## Data
| Field | Type | Notes |
|---|---|---|
| `FDTBulletSpawn.Position` / `Velocity` | `FVector2f` | sim plane, uu and uu/s |
| `FDTBulletSpawn.Radius` / `Damage` / `Life` | `float` | uu, damage per hit, seconds |
| `FDTBulletSpawn.OwnerId` | `int32` | player index (R-TEC-01) |
| `FDTBulletSpawn.Pierce` | `int32` | distinct enemies it may hit; one means no pierce |
| `FDTBulletSpawn.SourceId` | `uint16` | telemetry source (R-BUL-08); defaults to `Unknown` |
| `FDTBulletSpawn.WeaponSlot` / `bCrit` | `int8` / `bool` | copied into every hit (S1, `weapons.md` R-WPN-12); none / false for stress |
| Pool arrays | SoA | `Pos PrevPos Vel Radius Damage LifeLeft OwnerId PierceLeft SourceId WeaponSlot Crit Serial HitCount HitRing`; `bPierce` of a hit = spawned `Pierce` above one |
| `HitRing` | `int32[C.BULLETS.HIT_MEMORY]` per bullet | serials of the last enemies hit (ring) |
| `FDTBulletSpawn.Motion` | `FDTBulletMotion` | R-BUL-09; all zero = straight |
| `FDTHit` | `Core/DTSimTypes.h` | fields and lifetime: `07_Technical §4` (the one definition); appended to `FDTWorldView::Hits` |
| Instance custom data | `float` × four | `RGB` linear from `Pal.Player.Core`, then emissive strength; read by `M_InstanceColor` |
| `Fallback_BulletMaterial` | code path | `/Engine/BasicShapes/BasicShapeMaterial` when `/Game/DT/Materials/M_InstanceColor` is missing (R-TEC-05) |

API: `UDTBulletSubsystem::Spawn(const FDTBulletSpawn&)`, `GetPool()`, `GetRenderer()`; weapons spawn in `EDTSimPhase::Weapons`, the pool steps in `EDTSimPhase::Bullets`.

## Acceptance
- T-BUL-01 `Dusktracer.Bullets.IntegrateExpire`: one step moves a bullet by velocity × step and keeps the old position; life expiry on the right step; half the bullets expiring in one step leaves every survivor stepped exactly once.
- T-BUL-02 `Dusktracer.Bullets.ArenaBounds`: bullets crossing either edge are removed and counted; an inner bullet stays.
- T-BUL-03 `Dusktracer.Bullets.CapRecycle`: at `C.BULLETS.CAP` a spawn replaces the oldest live bullet (even after swaps), counts one recycle, and keeps the count at the cap.
- T-BUL-04 `Dusktracer.Bullets.PierceDistinct`: a slow piercing bullet overlapping each enemy for several steps hits distinct enemies in path order, then is removed as spent.
- T-BUL-05 `Dusktracer.Bullets.SingleHit`: one contact gives exactly one hit with the right enemy, owner and damage; a piercing bullet resting in an enemy hits it once.
- T-BUL-06 `Dusktracer.Bullets.Determinism`: two identical runs give bit-identical positions and hit sequences.
- T-BUL-07 `Dusktracer.Tec.Bullets.NoActors` (also T-TEC-11): in a headless game world, `C.BULLETS.CAP` bullets add no actor and exactly one instance each; the instance count follows the live count down.
- T-BUL-08 `Dusktracer.Bullets.HitSource`: hits carry their bullet's source, including a bullet swapped into a freed slot and a bullet that recycled the oldest slot; an untagged spawn is `Unknown`.
- T-BUL-09 `Dusktracer.Bullets.Motion`: a motion stays with its bullet through swap-removal; a mirrored moving round is reported at the pool's own position. Homing, orbit and return behaviour are read through their modifiers (`modifiers.md` T-MOD-10, T-MOD-11, T-MOD-12).
- HUMAN: in the stress arena, `DT.Bullets` at `C.BULLETS.CAP` shows a spinning ring of cool-white rounds with no visible stutter; `DT.Bullets.Stats` reports the live count near the target.

## Telemetry
- `FDTBulletCounters{Spawned, Recycled, Expired, OutOfBounds, Spent, Hits}` per pool, monotonic.
- `DT.Bullets.Stats` logs `live/cap target spawned recycled expired out spent hits` (not in Shipping, R-TEC-12).

## Text
- None: the bullet system has no player-facing text. Console help strings are developer-only English.

## Out of scope
- Weapons and fire patterns, modifiers (`weapons.md`, `modifiers.md`), per-weapon colours and trails (`04_Presentation` R-PRE-01); enemy rounds are a separate pool (`enemies.md` R-ENM-15).
- Damage application, kills and their events (`ResolveHits`, enemy system).
- Niagara A/B and measured performance at the cap (`07_Technical` R-TEC-10, T-TEC-10).

## Open
- Hit memory moves from enemy slot index to enemy `Serial` (`enemies.md` S1) → resolved by the S1 enemy work; until it lands a reused slot may be skipped.
- The boss hull (`boss.md`, radius `C.BOSS.RADIUS_UU`) widens every bullet query while it lives (R-BUL-05 adds the largest radius) → test it as a separate circle if `T-TEC-10` shows the cost.
- Recycling scans for the oldest live bullet (linear in the cap) → keep while recycling is rare; revisit if `Recycled` climbs in play telemetry.
- `M_InstanceColor` must be cooked for packaged builds (no hard reference yet) → resolved when the arena map or a data asset references it.
