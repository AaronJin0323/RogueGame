# S4 wave 1, package C: the Reclaimer's silhouette

For the scheduler. Scope: `Validation/S4_wave1.md` §3 "C" (D-043). Branch `s4/c-hero` from `dev/s4` at 55a504c, then merged with `dev/s4` at f5cf5fc (packages A and B). Spec: `02_Systems/hero_art.md` (R-HAR-01 to R-HAR-08). Every reading below was taken on this branch by a tool, with no window. No person looked at a frame; the agent viewed the frames named in §7.

## 1. What landed

| Item (gap row, `S4_artbook_gaps.md` §2.1) | What it is | Proved by |
|---|---|---|
| Armour set, hood, pauldrons (r1, r2, r12), massing only | Eleven rigid static meshes `SM_Hero_*`, authored as numbers in `Tools/blender/hero_reclaimer.py`: torso, pauldrons, hood, visor, undersuit, hips, belt, two arms, two legs. One object and one slot each, flat shaded | `Mesh.ps1 -Check` (11 files are what the source builds), `HeroArt` in `Tools/tools_test.py`, `Dusktracer.HeroArt.Parts` |
| Proportions and silhouette tests (r3) | `Tools/hero_outline.py`: the figure's mask at the gameplay camera, eight facings, at 36 and 27 px per metre | §3 below |
| Height against the sim plane (r4) | The figure stands on the floor; `C.SIM.PLANE_Z_UU`, the cue heights and the aim plane did not move | footprint check in the tool; the sim proof |
| Rigid pose in place of a rig (r5) | `Presentation/DTHeroPose`: a pure function of facing, velocity and dash. Idle, one held stride, dash. No `SK_` asset | `Dusktracer.HeroArt.Pose` |
| Dark-metal and undersuit tokens (r15, r16) | `Pal.Hero.DarkMetal` #2A292E and `Pal.Hero.Undersuit` #2B2A2E, new rows of `04_Presentation` §3. `Pal.Hero.Armour` is unchanged and is every plate | gate steps `palette` and `constants` |
| Staged frame | Scene `hero_lineup`, command `DT.HeroArt.LineUp` (not in Shipping): four facings in three poses round the hero | gate step `smoke` |

Nothing from RogueHero, its import inputs or any generation service was read or copied (`OPEN.md` O-009). A tool test holds that the generator opens no file and calls no importer.

Where this differs from the plan's text:
- The pose is under `Presentation/`, not `Player/`, so the include check sees it. `Player/DTHero.cpp` is listed in `RENDERER_OWNERS` as the one file outside presentation that names the figure; `DTHero.h` does not.
- The art book's three silhouette tests each need the back rig, the lamp or the weapon mounts, all held out of this package. Outline, facing and build readings stand in.
- The hood is armour-toned, not the book's black: a dark hood takes the brightest part of the figure away from above.
- The mesh pipeline needed no fix. Several objects in one mesh, several slots and smoothing groups are still unproved: this package used none.

## 2. Draw scale and what the figure covers

The plan asks that the hero's armour-tone pixel count in the 720p gate frame not fall under the cylinder's 368. A first figure of a person's build read 211. The landed figure is stockier and drawn at `C.HERO_ART.DRAW_SCALE` 1.16:

| Draw scale | Armour-tone pixels, empty arena |
|---|---|
| 1.14 | 361 |
| 1.16 (landed) | 388 to 394 over the runs of §4 |
| 1.18 | 403 |
| the cylinder | 366 to 368 |

At 1.16 the figure is 215 uu tall and 115 uu across the pauldrons. The hit circle is 80 uu wide, so the body is drawn 44 percent wider than what can be hit. Its mask against the cylinder's, from the tool:

| Frame | Least, by facing | Most | Mean |
|---|---|---|---|
| 36 px/m | 1.12 | 1.51 | 1.27 |
| 27 px/m | 1.07 | 1.48 | 1.23 |

So it hides about a quarter more of the screen than the cylinder did, and half as much again at its widest facing. Package B's hulls are held to the cube plus 25 percent (`S4_wave1.md` §5 item 6); no such limit is written for the hero and the tool prints this figure without judging it. The scale is one constant. Lowering it gives floor back and takes armour pixels away.

## 3. Silhouette readings

`python -I Tools/hero_outline.py`, on the merged tree, against package B's exported hulls in `SourceArt/Enemies/`. The share is intersection over union of the hero's mask and the hull's, both scaled to one area and laid centre on centre, the worst over eight hero facings and twenty-four hull turns. Limit `C.HERO_ART.OUTLINE_OVERLAP_MAX` 0.80; a disc and a square of one area share 0.83.

| Hull | 36 px/m | 27 px/m |
|---|---|---|
| `SM_Enemy_Rusher` | 0.78 | 0.78 |
| `SM_Enemy_Turret` | 0.76 | 0.75 |
| `SM_Enemy_Tank` | 0.60 | 0.62 |
| `SM_Enemy_Swarm` | 0.40 | 0.41 |
| the engine cube (before B) | 0.78 | 0.78 |

The Rusher and the Turret pass with 0.02 and 0.04 to spare. The reading compares outlines only: it does not see that the hero is the one bright neutral on screen and every hull is dark. The hull files are read as stored, with no object transform applied; B's files carry none that the reader looks for, and that was not checked beyond the reading being plausible.

Facing: any two of eight facings differ in 0.35 of the pixels either covers, the opposite facing included, at both frames (limit `C.HERO_ART.FACING_DIFF_MIN` 0.15). Armour is 0.89 of the mask at its least. The hit circle's centre projects inside the mask at every facing.

## 4. Contrast

Gate scene `contrast` (1280x720, en), need `C.PRE.CONTRAST_MIN` 3.0, watch floor `C.PRE.CONTRAST_WATCH_MIN` 3.1. Three runs each, on lit ground.

| Element | Flat floor, cylinder (55a504c) | Flat floor, figure (6b04a79) | Slab floor, cylinder (A's note) | Slab floor, figure (merged) |
|---|---|---|---|---|
| hero | 4.67, 4.67, 4.64 | 4.99, 4.99, 4.99 | 4.62, 4.62, 4.62 | 4.94, 4.93, 4.94 |
| hero armour-tone pixels | 368, 367, 368 | 388, 393, 391 | not read here | 389, 391, 394 |
| telegraph | 3.27, 3.26, 3.26 | 3.27, 3.26, 3.26 | | 3.29, 3.29, 3.29 |
| enemy round | 3.17, 3.17, 3.17 | 3.17, 3.17, 3.17 | | 3.21, 3.21, 3.20 |

`WATCH: OK` on every run; 7/7 elements. The hero's tone rose from Y 0.717 to 0.771: the figure's sunward facets are brighter than the cylinder's flat top. On the slab floor the hero reads 0.05 lower than on the flat one, as the cylinder did, because the lit-ground reference rose from Y 0.114 to 0.116 with the joints' chamfers. Against shadow the figure reads 12.27 to 12.30 on the merged tree. Telegraph and round on the merged tree are A's and B's readings, unmoved by the hero.

## 5. Perf

Caps scene, 1920x1080. No clean pair was possible: other agents' games were rendering during every run, and `Saved/gpu.lock` cannot see them.

| When | Game thread p95 ms | GPU p50 / p95 ms | What else ran |
|---|---|---|---|
| before | 2.70 | 5.00 / 5.71 | another agent's game throughout |
| before | 2.87 | 5.23 / 6.03 | another agent's game throughout |
| after | 2.75 | 2.13 / 5.61 | nothing at the start; a game and a compiler by the end |
| after | 3.76 | 11.50 / 13.59 | two other 1080p games: void |

The allowance is 0.1 ms proxied GPU p95 and it is neither confirmed nor refuted by these p95 figures. The one run that began quiet reads GPU p50 2.13 ms, P0's quiet level. The change is about a thousand triangles in eleven components of one instance each. The scheduler's reading on a quiet machine is the verdict. Not read on the merged tree.

## 6. Sim proof and gate

Seed 9107 on 6b04a79, before the merge: RestartCheck `276faf8e`, meta `ba2a3ac8`, None `7bd32faf`, Random `f5270196`, Steered:Forge:Hammer `c6573c69`, Steered:Storm:Tempest `d7c8b43c`, Steered:Tide:Frost `209a3446`. The five `bot.csv` files read the SHA-256 digests of `S4_p0.md` §8 byte for byte.

On the merge with `dev/s4` at f5cf5fc, commit 07b76f0: `GATE: PASS (07b76f0; 3 not run)` (pad: no package; scope; mac). 540 automation tests of 540 against `C.GATE.TEST_FLOOR` 533 (A's three, B's four, the hero's two on top of 524), 277 tool tests. The gate's own lines read restart `276faf8e` and meta `ba2a3ac8`. The five bot arms were not run again on the merged tree by this package. This note was committed after that gate and changes no other file.

Before the merge, on 6b04a79, the gate passed on its third run. The first failed at `firstrun`: two parallel bot arms on seed 9100 ended in the same second and wrote one telemetry file name, so the summary read a doubled header. The second failed at `earned`: the engine asserted in a DerivedDataCache IO thread four seconds into a launch. Neither is hero code and neither came back. One tool-test run also showed an error that was not captured and did not come back in eight later runs.

## 7. Goldens

The merge took `dev/s4`'s goldens as they were. Those frames still show the cylinder, so the gate's image check on the merged tree reads how far the figure, and one run's noise, moved each from it. The limit is `C.SMOKE.GOLDEN_MEAN_DIFF_MAX` 12.

| Golden | Mean diff against `dev/s4`'s frame | Re-shot |
|---|---|---|
| `arena_empty_en` | 0.43 | yes: `HeroArt` reads the armour pixels from this file, and `dev/s4`'s shows the cylinder's 367 |
| `arena_empty_zh-Hans` | 0.47 | no |
| `combat_staged_en` | 0.68 | no |
| `combat_staged_zh-Hans` | 0.65 | no |
| `edge_corner_en` | 0.25 | no |
| `edge_corner_zh-Hans` | 0.32 | no |
| `edge_corner_wide_en` | 0.34 | no |
| `enemy_lineup_en` | 0.64 | no |
| `hero_lineup_en` | this package's own | yes, on the slab floor; a second run reads 0.27 against it |

No golden moved past the limit, or near it, so none was re-shot for that reason: six committed goldens show a cylinder the game no longer draws, and the image check passes them. That is `S4_wave1.md` §5 item 3 again: a golden is a record, not a proof. The re-shot `arena_empty_en` reads 0.30 on a second run. The eight other edge scenes are not gate scenes and were not shot here.

Frames kept outside the repo, under the worktree's `Saved/S4Report/`: the empty arena and the staged fight before and after, the line-up, and enlarged crops of the hero.

## 8. Not verified, and the open human questions

Not verified by any check here:
- No frame was rendered at the 27 px/m frame. That scale is read by the mask tool only.
- The moving and dashing poses were seen in the line-up, on drawn figures. A test checks that the pawn shows a dash from real sim state; no frame of a sim-driven stride or dash was shot.
- No package was built, so the cooked meshes were not loaded by a packaged game.
- Perf, as §5 says.

Questions only a person can answer:
1. **Is the hero found at a glance in a fight?** In the staged fight frame the hero stands in a hull's shadow among rounds, and the agent's own view of the enlarged crop is that the figure is busier and not clearly easier to find than the cylinder's clean bright top was. The contrast reading rose; that is tone, not finding.
2. **Is the facing read at a glance?** The cues are the dark helm peak, the forearms, the boots and the dark face and belt seen from the front. They read on an enlarged crop. At native size the agent would not call it at a glance.
3. **Is the size right?** §2: wider than the hit circle and a quarter more screen than the cylinder. Does the body mislead about where the hero is hit, and does it hide rounds that matter?
4. **Does the armour-toned hood read as the Reclaimer?** It departs from the art book.
5. **Do the poses read as motion?** The pose has no clock, so it snaps between idle, stride and dash, and the stride is held, not cycled.
