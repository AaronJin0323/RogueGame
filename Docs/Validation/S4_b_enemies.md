# S4 wave 1, package B: Cast-off silhouettes

For the scheduler. Scope: `Validation/S4_wave1.md` §3 "B" (D-043). Branch `s4/b-enemies` from `dev/s4` at 55a504c. Spec: `02_Systems/enemy_art.md`. Every command below is typed in PowerShell at the repo root and opens nothing (R-TEC-17). Frames named here are kept under `Saved/S4Report/` of the package's worktree, not in the repo: only goldens have a licence row.

## 1. What landed

| Gap row | What it is | Proved by |
|---|---|---|
| §2.6 r8, §2.7 r26 role render path | `ADTEnemyRenderer`: a hull and an optic instanced component per role, each on the role's own mesh; the boss keeps the cube | T-EAR-01, T-EAR-02 |
| §2.6 r7 draw size apart from hit radius | A role mesh is modelled in units of the hit diameter, origin on the floor; drawn at the cube's scale, standing on the floor. No key, no sim reader | T-EAR-02 |
| §2.6 r9 to r12 the four hulls | `Tools/blender/enemy_swarm.py`, `enemy_tank.py`, `enemy_rusher.py`, `enemy_turret.py` through `Tools/enemy_shapes.py`; eight meshes `SM_Enemy_<Role>` and `SM_Enemy_<Role>_Optic` | T-EAR-03, `Mesh.ps1 -Check` |
| §2.6 r4 optic shape and anchor | Lens block on the nose, periscope bar on the slab, strip across the nose, six-sided mirror on the disc; modelled in the hull's frame | T-EAR-04 |
| Call 6, floor hidden | Area and reach against the cube and its optic cube, worst facing against best | T-EAR-05 |
| Silhouette difference | Outline overlap of every pair of roles at 36 and 27 px/m, on the generators and on captured frames | T-EAR-07 |

Readings of `& Tools\EnemyArt.ps1` (geometry, meshes, frames: all PASS):

| Role | Triangles | Drawn, uu | Floor hidden, x cube | Reach behind, x cube | Optic seen, uu² least / mean (cube) | Elite light, x cube |
|---|---|---|---|---|---|---|
| Swarm, R 32 | 96 | 93 x 85 x 23 | 0.52 | 1.05 | 734 / 781 (550 / 605) | 1.25 |
| Tank, R 58 | 72 | 139 x 172 x 99 | 1.11 | 1.22 | 2430 / 2810 (1800 / 2022) | 1.20 |
| Rusher, R 40 | 52 | 146 x 56 x 40 | 0.99 | 1.07 | 1308 / 1360 (868 / 934) | 1.25 |
| Turret, R 48 | 128 | 123 x 98 x 66 | 0.59 | 1.02 | 2031 / 2038 (1292 / 1343) | 1.22 |

Limit `C.EAR.FLOOR_HIDE_MAX_RATIO` 1.25. The Tank's reach, 1.22, is the tight one: its slab stands at the front edge.

Greatest outline overlap of any facing of one role with any facing of another (intersection over union at one area; limit `C.EAR.SILHOUETTE_OVERLAP_MAX` 0.75; two cubes read 1):

| Pair | Generators, 36 px/m | Generators, 27 px/m | Frame 1920x1080, 36 px/m | Frame 1440x810, 27 px/m |
|---|---|---|---|---|
| Swarm / Tank | 0.43 | 0.44 | 0.48 | 0.48 |
| Swarm / Rusher | 0.43 | 0.44 | 0.52 | 0.52 |
| Swarm / Turret | 0.46 | 0.48 | 0.57 | 0.57 |
| Tank / Rusher | 0.63 | 0.65 | 0.59 | 0.59 |
| Tank / Turret | 0.72 | 0.71 | 0.66 | 0.65 |
| Rusher / Turret | 0.64 | 0.65 | 0.62 | 0.65 |

The generators are read at 24 facings each; a frame holds three facings per role, so its figure is a sample. The 27 px/m frame keeps today's framing and lowers the frame's height to 810 rows, which gives the pixel density the 2000 uu frame would have at 1080 rows; the camera itself was not moved. Two compact shapes of one area overlap by well over half whatever they are, so 0.75 is a low bar: the check shows the outlines are not the same, no more.

## 2. Where this differs from the plan's text or the brief

- **T-ENM-24 passes unchanged, and reads the grey box.** The test counts `GetHull()` and `GetOptic()` as one component holding every enemy, and reads the elite's scale at a fixed index. One component cannot hold four meshes. The renderer therefore keeps the cubes as they were, one per enemy and in step every frame, and hides them; what is drawn is in the per-role components, which T-EAR-01 and T-EAR-02 read. The cubes are also the mesh fallback and the `dt.Exp.EnemyGreyBox` reference (R-EAR-06), so they are not there for the test alone, but the old test no longer reads what is on screen. Recommended at merge or after: point T-ENM-24 at the role components. The test file is outside this package's files.
- **No LOD.** The plan's share of §2.6 r8 says "LODs". Each role is 52 to 128 triangles; `C.EAR.ROLE_TRIS_MAX` (400) stands in. A LOD chain is a later call.
- **Files outside the plan's list.** `Enemies/DTEnemySubsystem.h` (one static function, one array), `Debug/DTDebugEnemyArt.cpp` (new: `DT.Enemies.LineUp`; no existing command places an enemy at a point or holds one still), `Tests/Enemies/DTEnemyArtTests.cpp` (new), `Tools/enemy_shapes.py`, `Tools/enemy_art_check.py`, `Tools/EnemyArt.ps1` (new).
- **Shared files, additive.** `constants.yaml`, `08_Constants.md`, `number_change_log.csv`: one block of eight `C.EAR.*` keys at the end. `fallbacks.csv`: one row. `Tools/tools_test.py`: two imports and one class, `EnemyArt`, placed before `PerfSceneS4`. `DTConstantsGen.h` is regenerated.
- **`C.GATE.TEST_FLOOR` was not raised.** Four automation tests were added (535 found). `S4_p0.md` §5 says to raise the floor with them; `S4_wave1.md` §2 rule 9 says shared constants take new rows only, and four packages raising one value would collide. The scheduler raises it at merge.
- **The line-up's frames are staged without cast shadows.** At dusk a hull's shadow is longer than the hull and has the sun's shape, not the role's. The golden `enemy_lineup_en` keeps the shadows.
- **Elites.** The plan says elites draw the role mesh at `C.ELITE.DRAW_SCALE` with today's cube parts. They do. The parts were placed for a cube: side plates and the light float clear of the lower hulls.

## 3. The mesh pipeline

`Tools/blender/Mesh.ps1`, `build_mesh.py` and `import_mesh.py` were not changed.

- Several objects: proved. A hull is five to seven mesh objects; the import joins them, and `-Check` finds every `SourceArt` file byte-equal to a fresh build.
- One material slot per mesh: every object of a mesh shares one material. Two slots on one mesh were not tried.
- Smoothing: not used. Every part is flat shaded; nothing here shows how a smooth-shaded part imports.
- A shared helper cannot live in `Tools/blender/`: `MeshPipeline.test_every_mesh_source_states_its_family_and_meshes` asks every `.py` there for `FAMILY` and `MESHES`. `Tools/enemy_shapes.py` sits one folder up; `build_mesh.py` already puts `Tools/` on the path, so a source imports it by name. Package C can do the same.
- A source need not import `bpy` at module level. Keeping `bpy` inside one function lets plain Python read the same geometry Blender builds, which is how the checks above see the hulls.
- Blender +Y is Unreal -Y, as P0 read. The four meshes are their own mirror across the facing axis, and a check holds them to it, so the flip cannot show.

## 4. Contrast, before and after

Gate scene `contrast`, 1280x720, en, three runs each, need `C.PRE.CONTRAST_MIN` 3.0, watch floor 3.1.

| Element, vs lit ground | Before (55a504c) | After |
|---|---|---|
| telegraph | 3.26, 3.26, 3.26 | 3.26, 3.27, 3.26 |
| enemy round rim | 3.18, 3.17, 3.17 | 3.18, 3.19, 3.18 |
| optic | 5.50, 5.50, 5.50 | 5.50, 5.48, 5.50 |
| optic, lit pixels | 2325, 2333, 2317 | 3275, 3285, 3274 |
| tracer core | 5.95, 5.95, 5.95 | 5.94, 5.93, 5.95 |
| hero | 4.67, 4.67, 4.67 | 4.66, 4.65, 4.66 |
| gem | 3.97, 4.05, 3.97 | 4.00, 4.01, 3.96 |
| chest | 4.49, 4.50, 4.49 | 4.54, 4.52, 4.52 |

`WATCH: OK` on all six runs. The optic's tone is 0.855 before and after: it is still the enemy's brightest point, and it shows about 41 percent more lit pixels.

One reading moved that the watch does not cover: **the frame's shadow tone rose from Y 0.013 to 0.017**, so every "vs shadow" ratio fell by about 6 percent (telegraph 4.18 to 3.93, round 7.78 to 7.33, optic 14.44 to 13.52). The role hulls are lower and thinner than the cubes and cast less deep shadow, so the darkest floor in the frame is less dark. All stay above 3.0. Lit ground in the fight frame reads p5 0.1076 (0.1057 before): less floor is under a soft shadow edge.

## 5. Perf

> Replaced by `S4_perf_1080p.md` (2026-11-11): the readings below rendered 888x500 while recording 1920x1080, most on a busy machine; that note holds the reading of record at the real size, alone.

`Tools\Perf.ps1`, grey box (`-Cmds "dt.Exp.EnemyGreyBox 1,..."`) against role meshes on one build, alternating, two rounds. **Every reading is void by `S4_wave1.md` §2 rule 6**: another agent's game or compiler was running at each one, and GPU p95 read 6 to 17 ms where P0's quiet runs read 2.1. They are kept as a bound on the difference, not as a level. Twenty-two readings over three passes:

| Scene | Draw | Game thread p50 / p95 ms | GPU p50 / p95 ms | Proxied GPU p95 ms | What else ran (process list and CPU just before) |
|---|---|---|---|---|---|
| Caps | grey box | 2.93 / 4.19 | 4.73 / 5.92 | 9.47 | two editors, CPU 76 % |
| Caps | meshes | 2.18 / 2.80 | 2.09 / 5.98 | 9.57 | an editor, a build host, CPU 70 % |
| Caps | grey box | 2.33 / 3.07 | 6.63 / 8.78 | 14.05 | two editors, CPU 71 % |
| Caps | meshes | 2.44 / 3.16 | 5.09 / 8.02 | 12.83 | a compile, an editor, CPU 60 % |
| Caps | grey box | 2.31 / 3.13 | 5.36 / 8.16 | 13.06 | two editors, a commandlet, CPU 19 % |
| Caps | meshes | 3.16 / 3.93 | 2.80 / 4.44 | 7.10 | a compile, editors, CPU 74 % |
| Caps | grey box | 2.21 / 2.83 | 2.08 / 2.46 | 3.94 | three editors, three commandlets, CPU 93 % |
| Caps | meshes | 2.31 / 2.85 | 5.64 / 8.57 | 13.71 | a compile, four editors, CPU 10 % |
| Caps | grey box | 2.76 / 3.76 | 4.79 / 8.13 | 13.01 | a compile, five editors, CPU 73 % |
| Caps | meshes | 2.64 / 3.57 | 6.40 / 8.63 | 13.81 | four editors, two commandlets, CPU 90 % |
| S2 | grey box | 3.16 / 4.70 | 5.89 / 6.68 | 10.69 | two editors, CPU 32 % |
| S2 | meshes | 3.33 / 5.22 | 5.94 / 7.03 | 11.24 | an editor, CPU 80 % |
| S2 | grey box | 3.99 / 5.52 | 6.73 / 10.34 | 16.54 | two editors, CPU 63 % |
| S2 | meshes | 3.67 / 5.39 | 3.61 / 6.13 | 9.81 | an editor, CPU 70 % |
| S2 | grey box | 3.06 / 4.55 | 2.55 / 2.82 | **4.51** | five commandlets, CPU 94 % |
| S2 | meshes | 3.11 / 4.63 | 2.56 / 2.84 | **4.54** | a link, two editors, CPU 29 % |
| S4 | grey box | 5.69 / 7.27 | 7.40 / 11.72 | 18.75 | a compile, an editor, CPU 91 % |
| S4 | meshes | 6.88 / 10.99 | 6.20 / 9.93 | 15.89 | an editor, CPU 43 % |
| S4 | grey box | 6.01 / 7.34 | 10.65 / 12.50 | 20.00 | two editors, CPU 62 % |
| S4 | meshes | 5.81 / 7.60 | 6.53 / 16.82 | 26.91 | two editors, CPU 57 % |
| S4 | grey box | 5.36 / 5.97 | 4.23 / 5.01 | 8.02 | a build host, three editors, CPU 26 % |
| S4 | meshes | 5.47 / 6.35 | 4.00 / 4.67 | 7.47 | a compile, three editors, CPU 14 % |

The one pair that reads like a quiet run is S2's last: 4.51 ms proxied with the grey box, which is P0's own S2 figure, against 4.54 with the meshes. On Caps no pair is clean; the least contended samples put GPU p50 at 2.08 ms for the grey box and 2.09 ms for the meshes, and P0's quiet Caps p95 (3.33 to 3.34 proxied) was reached once, by the grey box at 3.94. The before readings taken on 55a504c, before any change, were as contended (Caps 9.72, S2 10.93, S4 19.52 proxied) and are void too.

What they bound: no reading shows the role meshes costing more than the grey box beyond the run-to-run spread, which is several milliseconds here. `DT/GameThread/SimRender`, the game-thread work this package touches, reads 0.48 ms mean on S2 both ways in the quiet pair and within 0.03 ms in the others. The allowance of 0.8 ms proxied GPU p95 on Caps is neither shown met nor shown broken: the scheduler's quiet reading on the merged tree is the verdict.

**A finding about `Tools\Perf.ps1` itself.** Its log records `systemresolution.resx="888"`, `resy="500"`: without `-ForceRes` the engine shrinks the 1920x1080 it is asked for to 888x500 on this desktop (the comment at the top of `Debug/DTDebugOffers.cpp` already says so). Every perf reading on this machine, P0's baselines included, is at about a fifth of the pixels of 1080p. `Tools\EnemyArt.ps1` passes `-ForceRes` and its judge refuses a frame at the wrong density. `Perf.ps1` is the scheduler's file.

## 6. Goldens

Re-baselined in this package's commit: `combat_staged_en`, `combat_staged_zh-Hans` (4.70 and 4.61 mean difference against the old goldens, limit 12.0: the old ones still passed, so the re-baseline is a record), and the new `enemy_lineup_en`. Package C moves `combat_staged` again after this.

## 7. The sim proof

On 89339d3, the commit that holds every code, mesh and golden change of the package (the commit after it adds this note only):

- `& Tools\Gate.ps1 -Base dev/s4`: `GATE: PASS (89339d3; 3 not run)` (pad: no package; scope: no brief in flight; mac: deferred, D-007). 535 automation tests of 535, floor 524; 263 tool tests.
- Seed 9107: the gate's `restart` line reads `276faf8e` and its `meta` line `ba2a3ac8`.
- `& Tools\BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107 -Seeds 1`, one arm at a time:

| Arm, seed 9107 | `input_hash` | `bot.csv` bytes | SHA-256, first 16 digits |
|---|---|---|---|
| None | `7bd32faf` | 379 | `fa79587d4caca00c` |
| Random | `f5270196` | 662 | `0f7040940d7dee3e` |
| Steered:Forge:Hammer | `c6573c69` | 668 | `5a3568925a419583` |
| Steered:Storm:Tempest | `d7c8b43c` | 560 | `4452bfe6dbb65b3b` |
| Steered:Tide:Frost | `209a3446` | 418 | `9bd5ffcad19643d5` |

All seven hashes are the ones `S4_p0.md` §8 lists, and each `bot.csv` is byte-identical to P0's. No sim file is in the package's diff: `Enemies/DTEnemySubsystem.cpp` changes only `SimRender` and adds the static `PlaceParts`, which the draw alone calls.

## 8. Not verified

- No person looked at a frame. The agent viewed the line-up and the staged combat frame once each.
- That a person names each role from its shape, in a crowd, under fire, at either frame. The silhouette reading is a proxy.
- That the Tank's slab reads as the reduced-damage facing, or the Turret's tube as its aim.
- Perf at a level: see §5.
- The 27 px/m reading stands in for the 2000 uu frame by pixel density; the camera was not moved to 2000 uu.
- The frame-level silhouette check is on hulls without cast shadows. With shadows on, outlines merge with their shadows; nothing here measures that.
- Elite role hulls with affix parts were drawn in the staged combat frame and checked by geometry for the light only; no check reads the affix parts against the new hulls.
- `Fallback_EnemyRoleMesh` was not exercised by removing a mesh; the grey box path it shares was, through `dt.Exp.EnemyGreyBox`.
- The eight meshes cook alone without error (`Tools\blender\CookMesh.bat`: 8 packages cooked, each named in the cook's `ReferencedSet.txt`). They were not loaded by a packaged game: no package was built, so the gate's `pad` step did not run.
- Two material slots on one mesh, and smooth shading, through the pipeline.
