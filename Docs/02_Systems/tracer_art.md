# Tracer art — S4 wave package D

> Scope: `Validation/S4_wave1.md` §3 "D" (D-043). Presentation only: every position, radius, speed and firing point is read from the sim and none is written (`04_Presentation` R-PRE-11); no rule of another spec and no token value moves. The readings are in `Validation/S4_tracers.md`.

## Intent
Player fire stops being spheres and becomes tracers: a bright stretched head with a short fading tail in the Keeper's colour, or the neutral trail before a Keeper is held. Enemy rounds gain a thin trail and a muzzle flash. The enemy round stays the most readable moving thing in the frame, and nothing drawn here can cover a round or a telegraph, however dense player fire gets.

## Rules
- **R-TRC-01** A player round's head is `Pal.Player.Core` at `C.BULLETS.EMISSIVE_PLAYER`, for every weapon and whatever Keepers the player holds (R-PRE-01). It is drawn apart from the hit radius: `C.TRC.HEAD_WIDTH_RATIO` of it wide, stretched by `C.TRC.HEAD_STRETCH` along the flight, centred on the round. A round at rest is drawn round. The head is unlit and opaque (`M_Tracer`), so it hides what lies under it and the sun does not move it off its token.
- **R-TRC-02** A moving player round has a tail: a flat quad from the head's centre back along the flight, as long as `C.TRC.TAIL_S` of flight, never under `C.TRC.TAIL_MIN_RATIO` head half-widths and never over `C.TRC.TAIL_MAX_UU`, `C.TRC.TAIL_WIDTH_RATIO` of the head's width at the head and narrowing to a point. Its strength is `C.TRC.TAIL_EMISSIVE` at the head and fades to nothing at the tip (`M_Tracer_Tail`, additive). A round at rest has none. Rounds of the `Stress` source (`DT.Bullets`, the perf filler) draw a head and no tail, as before.
- **R-TRC-03** The tail takes `Pal.Player.TrailNeutral` until the player owns a blessing of the primary Path, then `Pal.Keeper.<Path>` of that Path (R-PRE-01, `keepers.md` R-KEE-09). From the commitment the last `C.TRC.TAIL_TIP_SHARE` of the tail, from its tip, blends to the second Path's token; the body's colour and the head never change. No tail token is an enemy or telegraph token (R-PRE-02).
- **R-TRC-04** An enemy round has a trail behind it: `Pal.Enemy.Bullet` at `C.TRC.ENEMY_TRAIL_EMISSIVE` and `C.TRC.ENEMY_TRAIL_OPACITY` at the round, fading to nothing `C.TRC.ENEMY_TRAIL_S` of flight behind the drawn round, `C.TRC.ENEMY_TRAIL_WIDTH_RATIO` of the drawn round wide and narrowing to a point (`M_EnemyRound_Trail`, translucent, not additive). It has no halo and no ring; a round that flies less than its drawn radius in that time has none; and it is shorter than the gap between two rounds of a burst (`C.ENEMY_FIRE.BURST_GAP_S`), so it never joins one round to the next. The round itself is unchanged: its dark core, its rim and `C.ENEMY_FIRE.DRAW_RADIUS_RATIO` stay as `enemies.md` R-ENM-17 has them. The trail is darker than the rim: in the frame no trail pixel reads as rim, so the round's ring still ends at its own edge.
- **R-TRC-05** A tail, a trail and a flash are each drawn at the height `C.TRC.TAIL_Z_UU`: over the floor, the zones and the control cues, under every telegraph layer, every round, hull and hero. The material slides each vertex along its own camera ray down to that height, so the quad covers the pixels it would cover at the round's height from any camera, and the depth test then hides it wherever a telegraph, a round, a hull or a hero is drawn. A player tail therefore never paints over an enemy round or a telegraph, whatever its blend and however many overlap.
- **R-TRC-06** Player fire dims with its own density (R-PRE-06): up to `C.TRC.DIM_START_ROUNDS` live player rounds it draws undimmed; from `C.TRC.DIM_FULL_ROUNDS` the head's emissive is `C.TRC.DIM_EMISSIVE_MIN` of `C.BULLETS.EMISSIVE_PLAYER`, the tail's strength `C.TRC.DIM_TAIL_MIN` of `C.TRC.TAIL_EMISSIVE` and the tail's width `C.TRC.DIM_WIDTH_MIN` of its own; linear between, and never rising with the count. The dim is one material parameter per renderer and the width is in the transform already written each frame, so no per-instance data is rewritten. Nothing on the enemy side reads the count: round, trail, flash, telegraph and optic are the same at every density.
- **R-TRC-07** A round an enemy has just fired shows a muzzle flash for `C.TRC.MUZZLE_S`: `Pal.Enemy.Muzzle` at `C.TRC.MUZZLE_EMISSIVE` and `C.TRC.MUZZLE_OPACITY`, a pointed quad down the flight from the edge of the hull that fired (`C.ROLE.TURRET_RADIUS_UU` from the firing point at its centre), `C.TRC.MUZZLE_LENGTH_RATIO` drawn radii long and `C.TRC.MUZZLE_WIDTH_RATIO` drawn radii to each side when fresh, shrinking to nothing as it ends (R-PRE-02). The age is the distance flown over the speed, read from the pool; a round with no velocity has none.
- **R-TRC-08** Everything here is batched and read-only (`07_Technical` §5, R-PRE-11): heads are one instanced component, tails one per pair of body and tip colour in use, trails and flashes one each; never an actor or a component per round. No sim array is written, no sim stream is drawn from, and seed `9107` gives the hashes it gave before.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Head width over hit radius | C.TRC.HEAD_WIDTH_RATIO | GATED(S4) | R-TRC-01, T-TRC-01 |
| Head stretch along the flight | C.TRC.HEAD_STRETCH | GATED(S4) | R-TRC-01, T-TRC-01 |
| Tail length in seconds of flight | C.TRC.TAIL_S | GATED(S4) | R-TRC-02, T-TRC-02 |
| Shortest tail, head half-widths | C.TRC.TAIL_MIN_RATIO | GATED(S4) | R-TRC-02, T-TRC-02 |
| Longest tail | C.TRC.TAIL_MAX_UU | GATED(S4) | R-TRC-02, T-TRC-02 |
| Tail width over head width | C.TRC.TAIL_WIDTH_RATIO | GATED(S4) | R-TRC-02, T-TRC-02 |
| Tail strength, undimmed | C.TRC.TAIL_EMISSIVE | GATED(S4) | R-TRC-02, T-TRC-06 |
| Tip share of the tail | C.TRC.TAIL_TIP_SHARE | GATED(S4) | R-TRC-03, `Tools/assets/tracer_materials.py` |
| Height of tails, trails, flashes | C.TRC.TAIL_Z_UU | GATED(S4) | R-TRC-05, T-TRC-05, `Tools/assets/tracer_materials.py` |
| Dim start, live player rounds | C.TRC.DIM_START_ROUNDS | GATED(S4) | R-TRC-06, T-TRC-06 |
| Dim full, live player rounds | C.TRC.DIM_FULL_ROUNDS | GATED(S4) | R-TRC-06, T-TRC-06 |
| Head emissive floor | C.TRC.DIM_EMISSIVE_MIN | GATED(S4) | R-TRC-06, T-TRC-06 |
| Tail strength floor | C.TRC.DIM_TAIL_MIN | GATED(S4) | R-TRC-06, T-TRC-06 |
| Tail width floor | C.TRC.DIM_WIDTH_MIN | GATED(S4) | R-TRC-06, T-TRC-06 |
| Enemy trail, seconds of flight | C.TRC.ENEMY_TRAIL_S | GATED(S4) | R-TRC-04, T-TRC-04 |
| Enemy trail width over drawn radius | C.TRC.ENEMY_TRAIL_WIDTH_RATIO | GATED(S4) | R-TRC-04, T-TRC-04 |
| Enemy trail strength | C.TRC.ENEMY_TRAIL_EMISSIVE | GATED(S4) | R-TRC-04, T-TRC-04 |
| Enemy trail opacity | C.TRC.ENEMY_TRAIL_OPACITY | GATED(S4) | R-TRC-04, T-TRC-04 |
| Muzzle flash duration | C.TRC.MUZZLE_S | GATED(S4) | R-TRC-07, T-TRC-07 |
| Muzzle flash length | C.TRC.MUZZLE_LENGTH_RATIO | GATED(S4) | R-TRC-07, T-TRC-07 |
| Muzzle flash half-width | C.TRC.MUZZLE_WIDTH_RATIO | GATED(S4) | R-TRC-07, T-TRC-07 |
| Muzzle flash strength | C.TRC.MUZZLE_EMISSIVE | GATED(S4) | R-TRC-07, T-TRC-07 |
| Muzzle flash opacity | C.TRC.MUZZLE_OPACITY | GATED(S4) | R-TRC-07, T-TRC-07 |

Read and never changed here: `C.BULLETS.EMISSIVE_PLAYER` (the undimmed head), `C.ENEMY_FIRE.DRAW_RADIUS_RATIO`, `C.ENEMY_FIRE.EMISSIVE`, `C.SIM.PLANE_Z_UU`, `C.ENEMIES.TELEGRAPH_Z_UU`, `C.PRE.TELEGRAPH_LAYER_UU`, `C.PRE.CUE_Z_UU`, `C.PRE.ZONE_Z_UU`, `C.PRE.ZONE_HALF_UU`, `C.PRE.CONTRAST_MIN`, `C.PRE.CONTRAST_WATCH_MIN`.

## Data
| Asset or file | What it is |
|---|---|
| `Tools/assets/tracer_materials.py` | the generator of the three materials, run by `Tools/EditorPython.bat`; no colour is typed in it |
| `Content/DT/Materials/M_Tracer` | head: unlit, opaque; instance data `0-2` RGB, `3` emissive; scalar parameter `Dim` |
| `Content/DT/Materials/M_Tracer_Tail` | tail: unlit, additive, on the engine plane; instance data `0-2` body RGB, `3` strength, `4-6` tip RGB; `Dim`; the camera-ray slide of R-TRC-05 |
| `Content/DT/Materials/M_EnemyRound_Trail` | enemy trail and muzzle flash: unlit, translucent; instance data `0-2` RGB, `3` emissive, `4` opacity; no `Dim`; the same slide |
| `Source/Dusktracer/Presentation/DTTracerArt.*` | the pure functions: tokens, density, shapes, instance sync |
| `Combat/DTBulletRenderer.*`, `Combat/DTBulletSubsystem.cpp` | heads |
| `Keepers/DTKeeperSubsystem.*` | tails, one renderer per pair of body and tip colour in use |
| `Enemies/DTEnemyBulletRenderer.*` | trails and flashes; `FDTEnemyBulletPool::GetOrigins` is the read-only view of the firing points |
| `Tools/Scenes/tracer_crossing.json`, `tracer_weapons.json` | the staged frames of T-TRC-05 and of the tracer look |

Without a material the art falls back, logged once: no `M_Tracer`, the head stays on `M_InstanceColor` as before; no `M_Tracer_Tail` or `M_EnemyRound_Trail`, no tail, trail or flash is drawn.

## Acceptance
- T-TRC-01 `Dusktracer.Tracer.Shapes`: a head at rest is round at the width ratio; a moving head is stretched along its flight only, points down it and stays on the round; `Dusktracer.Pre.PlayerTracer` (T-PRE-01): for every weapon, Path and second Path the core is `Pal.Player.Core`, and the drawn heads carry it at `C.BULLETS.EMISSIVE_PLAYER`.
- T-TRC-02 `Dusktracer.Tracer.Shapes`: the tail runs from the head's centre back along the flight; its length is `C.TRC.TAIL_S` of flight inside its two limits and never falls as speed rises; its width follows the ratio and the width dim; a round at rest has none.
- T-TRC-03 `Dusktracer.Pre.PlayerTracer` (T-PRE-01): neutral before the first blessing, the leading Path's token after; a second Path changes only the tip, and only from the commitment; the drawn tails carry those tokens, and the neutral tail is drawn.
- T-TRC-04 `Dusktracer.Tracer.EnemyTrail`: trail length, direction and width; none for a slow round; the drawn trails carry `Pal.Enemy.Bullet` at the fixed strength, under the rim's; the round keeps its draw radius.
- T-TRC-05 `Dusktracer.Tracer.UnderTelegraphs`: `C.TRC.TAIL_Z_UU` lies under the lowest telegraph layer and the lowest point of a drawn enemy round, over cues and zones; the tail is additive, the enemy trail translucent and not additive, all three unlit. And gate step `contrast` on `Tools/Scenes/tracer_crossing.json`: dense played fire from four weapons crossing Turret aim lines, Rusher bands and enemy rounds; telegraph and enemy round read at or above `C.PRE.CONTRAST_MIN`, and the package reads them against `C.PRE.CONTRAST_WATCH_MIN` before and after.
- T-TRC-06 `Dusktracer.Pre.DensityLod` (T-PRE-06, the tracer half): the three dims are full up to the start count, at their floors from the full count, and never rise between; as drawn at no fire, a few rounds and the cap, the head and tail `Dim` follow the count while the instance still carries `C.BULLETS.EMISSIVE_PLAYER`; enemy round, enemy trail and telegraph instance data are equal at every density.
- T-TRC-07 `Dusktracer.Tracer.EnemyTrail`: a flash for a round just fired, from the hull's edge and pointing down the flight, smaller half way through and gone after `C.TRC.MUZZLE_S`; drawn flashes carry `Pal.Enemy.Muzzle`.
- T-TRC-08 `Dusktracer.Pre.PlayerTracer` and `Dusktracer.Tracer.EnemyTrail`: no actor per round, tail, trail or flash; drawing leaves the pool's positions as they were. `Tools/tools_test.py` `PresentationReadsOnly`: no sim file includes a presentation header. The sim proof of `Validation/S4_p0.md` §8 on seed `9107`.
- Human, not verified by any check here: whether a tracer reads as a tracer, whether the four weapons can be told apart by their fire, whether the enemy trail helps or distracts, and whether the muzzle flash is seen at all at play speed.

## Telemetry
None. Nothing here is counted or recorded.

## Text
None.

## Out of scope
- Per-weapon hue, muzzle bloom at the rig, hit sparks, the kill half of R-PRE-06 (`impact_art.md`), the weapons past their tracer, Reforging shapes, fields, arcs and mines, which keep their materials.
- Telegraphs: their renderer, layers and tokens are untouched; the Turret's three-flash tell and mirror pulse are held with them.
- The second weather, bloom, exposure, the scalability tiers.
- Everything `Validation/S4_wave1.md` §4 holds back.

## Open
- The density curve is a first setting, read on frames and not on play: where it starts, where it is full and how far it goes are for a playtest to move.
- The muzzle flash is drawn at floor height like a tail, so it starts at a Turret's edge and the Warden's larger hull hides its own. A flash over the hull would also sit over the optic and over the round it announces.
- A tail is hidden by a hull or a hero it passes behind and also by one it passes in front of, because it is drawn at floor height. The head is not.
