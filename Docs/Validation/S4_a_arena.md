# S4 wave 1, package A: arena ground and edge

For the scheduler. Scope: `Validation/S4_wave1.md` §3 "A" (D-043). Branch `s4/a-arena` from `dev/s4` at 55a504c. Spec: `02_Systems/arena_art.md` (R-ARN-01 to R-ARN-06). Every reading below was taken on this branch by a tool, with no window; no person looked at a frame.

## 1. What landed

| Item (gap row) | What it is | Proved by |
|---|---|---|
| Arena edge (§2.8, the edge row) | `SM_Env_Apron`: five steps down from the plate's edge, 50 uu each with a 240 uu slabbed tread, a last 100 uu fall, then a plain skirt 9000 uu past the plate on every side. Nothing above the floor, no fog, no collision | `Dusktracer.Arena.ApronReach`, `Dusktracer.Arena.GroundDoesNotCollide`, `Tools/arena_check.py --void` on ten edge frames |
| Floor (§2.8, the floor row) | `SM_Env_Plate` in place of the engine plane: coursed slabs 320 uu wide and 320 to 760 uu long, told by joints 12 uu wide and 0.25 uu deep. Albedo is `M_Ground`, untouched | `Dusktracer.Arena.PlateRelief`, `Tools/arena_check.py --floor`, the gate's contrast step |
| Stone master (§2.8, the stone row; the material alone) | `M_Env_Stone`: `Pal.Ground.Lit` falling to `Pal.Ground.Shadow` with depth and on every face that is not a top, with a faint emissive so shadowed stone is not the clear colour | T-ARN-04, `ArenaArt` in `Tools/tools_test.py`, the `palette` step |
| Keep neutrals (§2.5, the reading only) | §4 below | the frames |
| Spec | `02_Systems/arena_art.md`, six rules with their tests, fifteen `C.ARN.*` keys | gate step `docs` |
| Staging | `DT.Arena.Frame <radius>` (not in Shipping), ten edge scenes | the goldens |

Generators: `Tools/blender/env_arena.py` (the two meshes), `Tools/create_arena.py` (the stone master and the map).

## 2. Where this differs from the plan or the brief

- **Relief only, no roughness.** The plan says slabs are read "by shallow relief and roughness only"; roughness was not tried. One value on every slab.
- **The joints are faint, and that is the floor window, not a taste.** A joint's shaded chamfer is a floor pixel. At 0.6 uu deep its darkest pixels read Y 0.083, inside the dead zone the gap note's §4.3 names (the black band needs Y 0.10); at 0.35 uu, 0.095. They are 0.25 uu deep, where the darkest hundredth of the bare floor reads Y 0.104. Stronger slabs need tone, which needs the floor target the gap note leaves to a D-nnn.
- **The plate casts no shadow.** As a shadow caster the slab mesh dimmed its own flat tops under the 20 degree sun: mean floor Y 0.1122 to 0.1106, the lit-ground reading 0.114 to 0.112, which by the tool's arithmetic would have cost the round about 0.03 of its 0.07. Three map variants found it (engine plane with the apron: unchanged; slab plate without the apron: dimmed; slab plate casting no shadow: unchanged). The apron's first riser stands at the plate's edge and casts the edge's shadow down the steps, so nothing is lost.
- **`M_Ground` is not touched.** The plan lists it under files. It stays the committed bytes; `create_arena.py` rewrites the four older materials with the same content on every run and they are restored before a commit.
- **Collision.** The engine plane carried the engine's default collision shape and nothing read it: no trace, sweep or hit query exists outside `Tests/`. The slab plate has none (R-ARN-03). Seed 9107 did not move (§7).
- **One scene file became nine.** A scene file is one frame, and the acceptance asks for a corner and each side's middle at both frame radii. `edge_corner_wide` joins the gate (one launch); the eight side frames are `"gate": "none"`, run by hand with `arena_check.py`.
- **A debug command.** The 2000 uu frames need a camera the game does not have (R-UX-02). `DT.Arena.Frame` in `Debug/DTDebugArena.cpp` moves a tool run's camera with the game's own sums; it is outside the plan's file list.
- **Wider than asked.** The apron is sized for the 2000 uu frame in a 32:9 window, not only 16:9: a wider window shows more at the sides (R-UX-01). It costs four quads.
- **A floor of light on the apron.** The plate's edge shadow on shadow stone rendered at level 0 to 2, which no tool can tell from a hole. `C.ARN.SHADOW_FLOOR` 0.35 lifts the darkest stone to level 6 or more. It is emissive, under the bloom threshold, and only on the apron.
- **Shared files.** `Tools/tools_test.py` gains one import and one class at its end (`ArenaArt`, four tests); `constants.yaml`, `08_Constants.md` and `number_change_log.csv` gain the `C.ARN.*` block; `C.GATE.TEST_FLOOR` goes 524 to 527 for the three automation tests, as `S4_p0.md` §5 asks, and will meet the other packages' edits of the same line at merge. `licenses.csv`, `Gate.ps1`, `Smoke.ps1`, `Perf.ps1` and `contrast_check.py` are not edited. No limit needs moving.
- **Goldens.** Re-baselined here, in the package's own commit, as the brief and `S4_wave1.md` §5 item 3 say; `S4_p0.md` §4 says the scheduler does it at merge.

## 3. Contrast and the margin watch

Gate scene `contrast`, 1280x720, en, three runs before (the tree at 55a504c, built in this worktree) and three after.

| Element | vs lit ground, before | after |
|---|---|---|
| telegraph | 3.26, 3.27, 3.26 | 3.30, 3.30, 3.30 |
| enemy round | 3.18, 3.17, 3.17 | 3.21, 3.21, 3.21 |
| optic | 5.50, 5.50, 5.50 | 5.44, 5.44, 5.44 |
| tracer core | 5.95, 5.95, 5.95 | 5.88, 5.89, 5.85 |
| hero | 4.67, 4.67, 4.67 | 4.62, 4.62, 4.62 |
| gem | 4.02, 3.97, 4.05 | 4.01, 3.98, 3.97 |
| chest | 4.46, 4.41, 4.53 | 4.48, 4.43, 4.48 |

`WATCH: OK` on all six runs; 7/7 on all six. Shadow Y 0.013 before and after.

**Why the two watched readings rose.** The tool's lit-ground reading is the 90th percentile of bare ground, and it went from Y 0.114 to 0.116: the sunward chamfer of every joint is a few percent of the floor's pixels and brighter than a slab's top. The slab tops did not move (median 0.1119 before and after). A brighter reference helps what is carried by a dark tone (the band, the round's core) and costs what is carried by a bright one: optic 0.06, tracer core about 0.07, hero 0.05, all far above 3.0. Gem and chest stay inside their own run-to-run noise. This is the reading's arithmetic, not a better telegraph.

Bare floor, `arena_empty` en, `Tools/arena_check.py --floor`:

| | p1 | p5 | p50 | p90 | p95 | mean | mean sRGB |
|---|---|---|---|---|---|---|---|
| before | 0.1107 | 0.1113 | 0.1119 | 0.1139 | 0.1156 | 0.1122 | 105.8, 92.2, 72.3 |
| after | 0.1040 | 0.1113 | 0.1119 | 0.1145 | 0.1163 | 0.1123 | 105.8, 92.3, 72.4 |

Window `C.PRE.LIT_GROUND_Y_WINDOW` 0.106 to 0.16: p5 and p95 inside, p5 unmoved. The darkest hundredth, where the joints' shaded sides are, reads 0.104 against the black band's floor of 0.100 (`(C.PRE.CONTRAST_MIN - 1) x 0.05`). Between 0.100 and 0.106 the round's core no longer carries and its rim does (rim Y 0.44 reads 3.0 on any ground up to 0.113), so no joint pixel is in a dead zone of either element. In the fight frame the floor's p5 reads 0.1046 against 0.1057 before; P0 found that frame's low end is hull and shadow edges, which is why the window is judged on the empty arena.

"A staged telegraph and enemy round over a slab joint" is the gate's contrast frame itself: its telegraphs span several slabs and its 43 or 44 rounds lie across the pattern. No frame was staged with one element on one joint; the tool reads tones over the whole frame and could not tell the difference.

## 4. The floor reading (Keep neutrals)

Rendered bare floor after this package: mean sRGB 106, 92, 72, hue 35.7, saturation 0.32, Y 0.112. The gap note read 105, 91, 72, hue 35.8, saturation 0.32, Y 0.110 at the 2000 uu frame: the same floor. The saturation is still about three times the token's, cause unmeasured, as the gap note says; this package changes no light and no token, so it could not have moved it. The skirt's sunlit shadow stone renders about 57, 49, 39 (Y 0.032, the shadow token's own luminance) and the apron's darkest shadow at level 6 to 19.

## 5. Edge frames

`Tools/arena_check.py --void`, level `C.ARN.VOID_MAX_LEVEL` 2, HUD masked. Before this package the corner frame held 561001 such pixels of 921600.

| Scene | Hero | Frame | Pixels at or under the level | Darkest level |
|---|---|---|---|---|
| `edge_corner` | +X +Y corner | 1500 | 0 | 7 |
| `edge_corner_wide` | +X +Y corner | 2000 | 0 | 11 |
| `edge_xp` / `_wide` | middle of +X | 1500 / 2000 | 0 / 0 | 7 / 12 |
| `edge_xn` / `_wide` | middle of -X | 1500 / 2000 | 0 / 0 | 16 / 19 |
| `edge_yp` / `_wide` | middle of +Y | 1500 / 2000 | 0 / 0 | 16 / 19 |
| `edge_yn` / `_wide` | middle of -Y | 1500 / 2000 | 0 / 0 | 6 / 7 |

The other three corners were not shot. `Dusktracer.Arena.ApronReach` covers all eight stands by the camera's sums, at both radii, at 16:9 and at 32:9, on the apron's lowest surface. Goldens exist for all ten frames, en; `edge_corner` also zh-Hans.

Goldens re-baselined: `arena_empty_en`, `arena_empty_zh-Hans` (the frame moved 0.33 of 12 against the old golden: 39711 pixels of 921600 differ by more than two levels, the joints), `edge_corner_en`, `edge_corner_zh-Hans` (31.98 of 12: the apron). New: `edge_corner_wide_en` and the eight side frames. Not touched: `combat_staged` reads 0.57 (en) and 0.54 (zh-Hans) against its goldens, under the limit.

## 6. Perf

`Tools\Perf.ps1`, 1920x1080, RTX 4080, proxy x1.6. Every reading was taken while other agents built and ran their own games on the same GPU: one to four other editor or game processes at each start, CPU 16 to 80 percent. So "before" was read in the same sessions, on the same build with the map of 55a504c checked out, alternated with "after" run by run. A reading whose GPU p95 is several times its p50 is another process's frames, not this scene's; the median says more here than the p95, and both are given. All are bounds.

| Scene | Map | Runs | Game thread p50 / p95 ms | GPU p50 ms | GPU p95 ms | Proxied GPU p95 ms |
|---|---|---|---|---|---|---|
| Caps | before | 5 | 2.13 to 2.42 / 2.76 to 3.20 | 2.06, 2.07, 2.08, 2.08, 4.57 | 2.14, 2.52, 5.10, 8.52, 9.75 | 3.42 at the quietest |
| Caps | after | 5 | 2.13 to 2.77 / 2.78 to 3.56 | 2.10, 2.13, 5.20, 7.39, 8.84 | 2.14, 5.74, 7.78, 8.76, 10.51 | 3.43 at the quietest |
| S2 | before | 4 | 3.17 to 4.11 / 4.99 to 6.74 | 3.05, 5.68, 5.69, 6.31 | 6.95, 8.96, 9.66, 10.29 | no quiet run |
| S2 | after | 4 | 3.37 to 4.14 / 5.11 to 5.94 | 3.35, 5.92, 6.41, 6.77 | 7.31, 7.39, 11.18, 13.15 | no quiet run |
| S4 | before | 3 | 5.48 to 5.57 / 6.21 to 7.29 | 4.01, 4.13, 6.56 | 4.68, 6.57, 9.60 | 7.49 at the quietest |
| S4 | after | 3 | 5.46 to 5.63 / 6.49 to 7.36 | 4.09, 4.16, 6.61 | 4.73, 4.80, 7.93 | 7.56 at the quietest |

- **Caps.** The quietest pair reads 3.42 before and 3.43 after, proxied GPU p95; the undisturbed medians read 2.06 to 2.08 ms before and 2.10 to 2.13 after, about 0.05 ms of GPU, 0.08 proxied. The allowance is 1.2. In the Caps scene the hero stands at the origin and the apron is off screen, so this is the slab plate against the engine plane: 3722 triangles for 2, one draw.
- **S4.** One back-to-back pair reads 7.49 before and 7.56 after; medians 4.01 and 4.13 before, 4.09 and 4.16 after. P0's quiet readings were 7.16 and 7.26.
- **S2.** No run of either map was quiet: P0 read 4.51 proxied on a quiet machine and the best here is 11.12 before and 11.70 after. Nothing can be said from them but that neither map is worse than the other by more than the noise. This scene needs the scheduler's reading.
- **Game thread.** No difference that survives the spread on any scene: the ground adds two static actors and no tick.
- **Not measured:** a frame with the apron in it. No perf scene puts the hero at the edge. The apron is 3536 triangles in one draw with an opaque, untextured material; the two smoke frames at the corner are not a perf reading.

## 7. The sim proof

Seed 9107, all as `S4_p0.md` §8 lists them: `Tools/RestartCheck.ps1` `276faf8e`; five arms run one after another, `input_hash` None `7bd32faf`, Random `f5270196`, Steered:Forge:Hammer `c6573c69`, Steered:Storm:Tempest `d7c8b43c`, Steered:Tide:Frost `209a3446`; the five `bot.csv` files are byte-identical to P0's by SHA-256 (`fa79587d4caca00c`, `0f7040940d7dee3e`, `5a3568925a419583`, `4452bfe6dbb65b3b`, `9bd5ffcad19643d5`). Meta `ba2a3ac8` is the gate's line.

`GATE: PASS (a739d5f; 3 not run)` with `& Tools\Gate.ps1 -Base dev/s4` (pad: no package; scope; mac): 534 automation tests of 534 against the floor of 527, 260 tool tests, meta `ba2a3ac8`, restart `276faf8e`, contrast 7/7 with `WATCH: OK (telegraph 3.30, enemy_rim 3.21)`, nine smoke frames against their goldens (0.25 to 0.56 of 12).

## 8. Not verified

- No person looked at any frame. Whether the floor looks like the Keep, whether the joints are enough to call it paved, and whether the step down reads as the arena's edge in play are a person's calls. The agent viewed the frames: the corner, the -X side and the -Y side at 2000 show stone to every edge of the frame.
- Frames are 1280x720. At 1080p a joint is half as wide again in pixels and its two sides separate; the floor readings were not taken there.
- One weather, one sun. The plate's relief is read by the sun's angle, so a second weather re-reads §3.
- The joint's darkest pixels were read as a percentile of the empty arena. No telegraph was staged across one joint and read alone.
- Three of the four corners were not shot (§5).
- `Mesh.ps1 -Cook` was not run and no package was built: that the two meshes cook and load in a packaged game follows from the map referencing them, and was not seen.
- The perf readings are bounds taken while other agents worked (§6). The scheduler's reading on the merged tree is the verdict.
- The 32:9 reach is the camera's sums, not a frame: `Smoke.ps1` shoots 16:9 only.
- `DT.Arena.Frame` moves the camera only: at 2000 uu the frame is the old frame's, but nothing else of D-036's revert (limits of `contrast_check.py`, HUD) was staged with it.

## 9. On the merged tree (with package B)

`dev/s4` at 0ba754f, package B's Cast-off silhouettes, merged into `s4/a-arena`. One conflict, `number_change_log.csv`, kept both sides' rows; `DTConstantsGen.h` regenerated; `C.GATE.TEST_FLOOR` 527 to 531 for B's four tests, which B had not counted. The figures of §3 were read without B's hulls; these replace them.

Contrast, three runs on the merged tree, 7/7 and `WATCH: OK` each time:

| Element | vs lit ground | vs shadow |
|---|---|---|
| telegraph | 3.29, 3.29, 3.29 | 3.93, 3.93, 3.93 |
| enemy round | 3.21, 3.21, 3.20 | 7.32, 7.33, 7.33 |
| optic | 5.44, 5.44, 5.44 | 13.51, 13.51, 13.52 |
| tracer core | 5.89, 5.85, 5.84 | 14.62, 14.51, 14.51 |
| hero | 4.62, 4.62, 4.62 | 11.46, 11.46, 11.46 |
| gem | 4.01, 4.02, 3.96 | 9.96, 9.99, 9.85 |
| chest | 4.49, 4.48, 4.47 | 11.14, 11.13, 11.11 |

- Lit ground Y 0.116 and the bare floor's spread 0.1113 to 0.1163, as in §3: the floor is this package's and B does not move it.
- **Shadow Y 0.017, not the 0.013 of §3.** That is B's reading (`S4_b_enemies.md`: lower hulls, thinner shadows), and every "vs shadow" figure above is B's level, about 6 percent under what this package read alone (telegraph 4.19 to 3.93, round 7.83 to 7.33, optic 14.45 to 13.51). The plate adds nothing to it: B alone read telegraph 3.93, round 7.33 and optic 13.52 against shadow.
- Against lit ground the two packages add: B alone read telegraph 3.26 to 3.27 and round 3.18 to 3.19; with the slabs' brighter reference they read 3.29 and 3.20 to 3.21.

B's goldens stand on the merged tree and were not re-baselined: `combat_staged_en` 0.53, `combat_staged_zh-Hans` 0.54, `enemy_lineup_en` 0.46 of 12. `arena_empty_en` 0.31.

`GATE: PASS (d0301c2; 3 not run)` on the merge commit (pad: no package; scope; mac): 538 automation tests of 538 against the floor of 531, restart `276faf8e`, meta `ba2a3ac8`, contrast 7/7 with `WATCH: OK (telegraph 3.29, enemy_rim 3.20)`, ten smoke frames against their goldens. The five bot arms of §7 were not re-run on the merged tree.
