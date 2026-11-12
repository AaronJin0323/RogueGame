# S4 wave 1 mend: an elite's parts sit on its role's hull (D-076, `s4/elite-anchors`)

> `02_Systems/enemy_art.md` R-EAR-08 and T-EAR-08. Draw code only (D-058 and D-076 item 3): the sim, the affix rules, the counts, the tokens, the light's emissive and its blink do not move.

## 1. The defect and its cause
Package B gave Swarm, Tank, Rusher and Turret their own hulls and kept `DTElite::BuildParts` as it was. That function placed every elite part from the hit radius as if the hull were the cube of that half-size, centred `C.SIM.PLANE_Z_UU` above the floor: the light one radius above the centre, a plate one radius out to each side, the rear parts one radius behind. The role hulls are not that cube (package B: Swarm 93 x 85 x 23 uu at a radius of 32, Tank 139 x 172 x 99 at 58, Rusher 146 x 56 x 40 at 40, Turret 123 x 98 x 66 at 48), so the light hung over the flat Swarm, the plates stood clear of the narrow Rusher, and `elites.md` R-ELT-10 ("in the same place as the role's optic") was unmet. `S4_b_enemies.md` §2 said so; no check read the parts against the new hulls.

## 2. The anchors, as the generators give them
`Tools/elite_anchors.py` measures each role on the lists `Tools/blender/enemy_<role>.py` states (through `Tools/enemy_shapes.py`, the lists Blender builds) and writes `Source/Dusktracer/Enemies/DTEliteAnchorsGen.h`; a tools test holds the committed header equal to what the generators give, and `Dusktracer.EnemyArt.EliteAnchors` holds it equal to the committed meshes' bounds. In drawn radii (hit radius x `C.ELITE.DRAW_SCALE`), in the hull's frame (+X the facing, +Z up from the floor):

| Anchor | Swarm | Tank | Rusher | Turret |
|---|---|---|---|---|
| Front / Rear | 1.50 / 1.40 | 1.00 / 1.40 | 1.80 / 1.84 | 1.52 / 1.04 |
| Half-width | 1.33 | 1.48 | 0.70 | 1.02 |
| Bottom / Top | 0.76 / 1.24 | 0.00 / 1.40 | 0.24 / 1.24 | 0.00 / 1.24 |
| Longest reach from above | 1.88 | 1.87 | 1.86 | 1.53 |
| Optic centre (along, up) | 0.36, 1.36 | 0.74, 1.55 | 1.30, 1.10 | 0.00, 1.31 |
| Optic half-size (along, across, up) | 0.48, 0.42, 0.12 | 0.26, 0.72, 0.15 | 0.42, 0.62, 0.10 | 0.62, 0.54, 0.07 |
| Nose on the centre line (along; bottom to top) | 0.60; 0.80 to 1.24 (fuselage) | 1.00; 0.04 to 1.40 (slab) | 1.80; 0.24 to 1.00 (body) | 1.52; 1.00 to 1.20 (focus tube) |
| Tail on the centre line (behind; bottom to top; half-width) | 1.40; 0.80 to 1.24; 0.14 | 0.24; 0.12 to 0.76; 0.80 (chassis) | 1.84; 0.24 to 1.00; 0.28 | 1.04; 0.00 to 0.32; 0.24 (rear foot) |
| Rear face (half-width; bottom to top) | 0.14; 0.80 to 1.24 | 1.24; 0.00 to 0.60 (tracks) | 0.28; 0.24 to 1.00 | 0.24; 0.00 to 0.32 |
| Side plate (inner face across; along from, to; bottom to top) | 0.24; -0.40 to -0.04 (fuselage, behind the arms) | 1.24; -1.40 to 0.60 (tracks) | 0.70; -1.24 to 1.00 (body) | 0.83; -0.48 to 0.48 (disc) |

The cube's anchors, the placement the grey box and the boss keep (R-EAR-06): front, rear and half-width 1, the cube from `C.SIM.PLANE_Z_UU` - R to + R, the light a cube of edge 2 x `C.ENEMIES.OPTIC_SCALE` radii at the nose (1 - `C.ENEMIES.OPTIC_SCALE` in front, the middle for Turret and boss) one radius over the plane.

## 3. The placement per affix
Every length times the elite's drawn radius R; P is `C.ELITE.PART_RATIO` x R. A part against a face sits at the face's middle height and never below that face's bottom. The shapes, counts and tokens are `elites.md` R-ELT-11's, unchanged.
- **Light:** a box of the optic's own bounds, `LightCover` 1.15 over, centred where the optic sits: it covers the optic it takes the place of (R-ELT-10). Its area seen from above is at most `C.EAR.ELITE_LIGHT_AREA_MAX_RATIO` 0.28 of the hull's top-view bounding area (front plus rear, by twice the half-width, from the same anchors); a larger one is scaled down about its centre, keeping its proportions. Before the cap the four read Swarm 0.138, Tank 0.139, Rusher 0.270, Turret 0.337: the first value asked, 0.35, caught none of them; 0.28 is the lowest that spares the Rusher, so only the Turret's light is scaled, by 0.91 along each side (its mirror's light from 1.43 x 1.23 to 1.30 x 1.13 radii seen from above). A cut deep enough to clear more of the Turret's disc would take the Rusher's nose strip too.
- **Armored:** a plate along each flat side of the widest part that runs along the hull (at least 0.4 of its length, standing at its greatest width for at least half its own length), as long as that side, cut where another part stands in its way, as tall as the part it rests on and at most one radius.
- **Swift:** each vane's front end at a corner of the rear face, the vane trailing back along its own line, at today's 30 degrees.
- **Volatile:** the reactor against the tail on the centre line.
- **Splitter:** the drones against the tail on the centre line, spread across its rear end, the outer drones flush with its sides, never closer than a drone apart.
- **Vampiric:** the spike from the nose on the centre line.
- **Shielded:** the orbit at the hull's longest reach plus 1.5 P, so the plates clear it by a part's size.

## 4. The geometry before and after
`Tools/enemy_art_check.py` `elite parts`, on the generators' solids (not their bounds), every role at its elite size, each affix alone and all together and the shield over its orbit. Worst gap and worst sink in uu; the limits proposed are `C.EAR.ELITE_PART_GAP_MAX_UU` 3 and `C.EAR.ELITE_PART_SINK_MAX_UU` 5.

| Role (elite R) | After: worst gap | After: worst sink | After: shield clear by | Before (hit circle on the role hull): worst gap | Before: worst sink | Light centre in the optic, before |
|---|---|---|---|---|---|---|
| Swarm (41.6) | 1.4 (Splitter) | 1.8 (Swift) | 14.6 | 25.5 (light) | 8.9 (shield) | no |
| Tank (75.4) | 1.3 (Vampiric) | 3.3 (Swift) | 26.6 | 47.1 (Splitter) | 14.1 (Armored) | yes |
| Rusher (52.0) | 0.0 | 2.3 (Swift) | 18.9 | 19.3 (light) | 15.6 (Volatile) | no |
| Turret (62.4) | 0.0 | 2.7 (Swift) | 25.9 | 27.4 (Splitter) | 0.0 | no |

With the role anchors every role passes; by the hit circle every role fails (a tools test holds that it does). The light's centre is inside the optic's bounds on every role after. The elite light reading (R-EAR-04) on the mended light: its least visible area over the facings is 1.88 (Swarm), 1.77 (Tank), 2.48 (Rusher) and 2.94 (Turret) times what today's cube light showed over the cube.

## 5. Lit pixels on the staged scene
`Tools\Frames.ps1 -Scene elite_lineup` at 1920x1080 and 1440x810, after (role anchors) and before (`-ExtraCmds "DT.Enemies.EliteLineUp 1 cube"`, the hit circle's placement on the same staging), 2026-11-12 20:46 to 20:47. A pixel is lit when its red is at least 200 and its blue at least 180 (the light's pink-white core and its bloom; the floor and the hulls are neither). Counted in each figure's slot; "light alone" sums the three columns whose only lit part is the light (Armored, Swift, Splitter).

| Role | 1920x1080 before | 1920x1080 after | 1440x810 before | 1440x810 after |
|---|---|---|---|---|
| Swarm | 624 | 959 | 344 | 526 |
| Tank | 1749 | 2430 | 993 | 1379 |
| Rusher | 946 | 1709 | 536 | 982 |
| Turret | 1423 | 3421 | 821 | 1948 |

On every role and at both sizes the light's lit pixels after are above before (1.4 to 2.4 times), as D-076 item 4 asks: not under today's. With the light's cap (`C.EAR.ELITE_LIGHT_AREA_MAX_RATIO` 0.28, shot again on the tree merged with `dev/s4` e61a850, 22:23), only the Turret moves: 3421 to 2854 at 1920x1080 and 1948 to 1623 at 1440x810, still 2.0 times its before; Swarm 961 and 526, Tank 2430 and 1381, Rusher 1709 and 979, as before within a few pixels. Seen at 3x the capped Turret shows a little more of its disc's rim; the light still covers most of the disc. Over all seven columns (the lit affix parts too) the same holds: 2182 to 3256, 6231 to 8321, 3302 to 5410 and 5318 to 10172 at 1920x1080.

## 6. The frames looked at
In `E:\Dusktracer\.claude\worktrees\mod-visual-b112f1\Saved\S4Report\elite-anchors\` (not in the repo): the line-up at both sizes before and after; 3x crops of every role with Armored, with Volatile and with every affix, before beside after (`sheet_<column>_before_after.png`); `tracer_crossing` and `combat_staged` at 1920x1080 after; and `armored_rusher_crossing_before_after_3x.png`, the Armored Rushers of `tracer_crossing` from `hero-zone\tracer_crossing_36_on_1.png` (before) beside the same crop after. What I see:
- **Before**, as D-076 says: on the Swarm the light hangs over the hull and the plates and drones stand beside it; on the Rusher the plates stand apart from the body with floor between, so an Armored Rusher reads as three dark bars under a light; on the Turret the light floats over the disc and the plates lie on the floor clear of the feet.
- **After**, every elite reads as one machine with things mounted on it: the light lies on the hull where the optic is (the Swarm's lens, the Tank's periscope bar, the Rusher's nose strip, the Turret's mirror), the reactor and the drones sit against the tail, the spike leaves the nose (the Turret's from its focus tube), the vanes trail from the rear corners, and the shield plates circle clear of the hull.
- **What reads less:** the Armored plates, now flush, read as a wider hull more than as plates: on the Tank they merge with the tracks, on the Turret they are thin strips under the disc's rim, on the Swarm they are short tabs behind the arms (its fuselage has a flat side only there). The light is larger than the cube was, on the Turret most of all (the mirror's bounds, nearly the disc). Whether an Armored elite is told from a plain one by its plates is the owner's question 8; this mend makes the parts sit, it does not make them louder.

## 7. Goldens
The goldens that hold elites, shot against the old golden first, then again with `-Update`, then against the new (`Tools\Smoke.ps1`, mean difference per pixel, the limit 12): `combat_staged_en` 0.57 against the old, 0.38 against the new; `combat_staged_zh-Hans` 0.59 and 0.43; `impact_hit_en` (twelve Armored Tanks) 1.07 and 0.33. New golden `elite_lineup_en` (0.24 against itself shot again). The old and new goldens are kept in `...\S4Report\elite-anchors\goldens\`. Every other golden: the gate's `smoke` step below.

## 8. Perf as a bound
`Tools\Perf.ps1 -Scene S4` on this one build (fc6197b's code), the role anchors (the default) and the hit circle (`-Cmds "DT.PerfScene S4,dt.Exp.EliteCubeAnchors 1"`) in turn, three rounds, 2026-11-12, logged from 20:59:37 to 21:05:22 (each time a run's end). The first run of round 1 took three tries: twice another session's measurement held the GPU lock. The machine was **not** quiet: before every run four to seven editor commandlets of other sessions were alive, an editor before five of the six, and a compiler before two. A bound, not a reading.

| Round | Placement | Game thread p50 / p95 | GPU p95 | `SimRender` mean / p50 / p95 | Load |
|---|---|---|---|---|---|
| 1 | role anchors | 5.47 / 8.34 | 6.02 | 1.01 / 0.97 / 1.35 | 5820 bullets / 400 enemies |
| 1 | hit circle | 4.89 / 7.75 | 4.24 | 0.98 / 0.96 / 1.23 | 5821 / 400 |
| 2 | role anchors | 4.79 / 7.45 | 4.27 | 0.87 / 0.89 / 1.13 | 5817 / 400 |
| 2 | hit circle | 7.26 / 13.06 (FAIL: over budget) | 4.26 | 1.12 / 1.00 / 1.42 | 5814 / 400 |
| 3 | role anchors | 3.70 / 5.79 | 4.17 | 0.71 / 0.66 / 0.99 | 5802 / 400 |
| 3 | hit circle | 4.93 / 7.61 | 4.27 | 0.92 / 0.93 / 1.21 | 5822 / 400 |

The same parts are written either way, so nothing was expected and nothing reads: the draw scope's mean with the role anchors less the hit circle is +0.03, -0.25, -0.21 ms, and the game thread swings by more than 7 ms between runs of one side. The one run over budget (round 2, hit circle) is the old placement and points to the machine. A quiet pair would give a reading; this is a bound that the mend costs nothing visible beside the load.

## 9. Contrast and the sim proof
From the gate of record below: `contrast` PASS on every scene, the contrast watch at telegraph 3.30, 3.30 and 3.32 and enemy rim 3.21, 3.23 and 3.25 on lit ground (floor 3.1; wave 1's 3.32 and 3.28 on `tracer_crossing`); `restart` PASS (seed 9107, input_hash 276faf8e both sides, 5400 steps), `seed` PASS (4 exact, 3 rejected), `meta` PASS (input_hash ba2a3ac8 both sides, 1200 steps): the sim did not move. `Tools\EnemyArt.ps1 -NoMeshCheck` passes (geometry with the elite parts reading, and the line-up frames at both sizes; no generator changed) and `Tools\HeroZone.ps1` passes.

## 10. Gate of record
`Tools\Gate.ps1 -Base dev/s4` on 77517a6, the branch with `dev/s4` e61a850 merged (Sound's audio import), started 2026-11-12 23:17:58 in the Visual worktree `agent-a76c15a416b3c6fea`: **GATE: PASS (77517a6; 3 not run; mask FFFFF3FC)**: build, shipping and the development game at 0 warnings; tests 598 run of 598 found, 0 failed, floor 598 (`dev/s4`'s 597 and `Dusktracer.EnemyArt.EliteAnchors`); every smoke golden passes, the four shot again here among them; vigil, firstrun, win, earned, contrast, settings and windows PASS; `pad`, `scope` and `mac` not run (no package, no brief in flight, deferred by D-007). A first whole run at 22:37 failed at `tools` (a tools test split in two by an edit, fixed in 77517a6); a second was stopped by this job's own watch at 23:09, when the owner's game started, during `vigil`, and was taken again whole. Other sessions' engine processes ran beside it: not a perf reading.

## 11. Not verified
- No person has looked at an elite on the new hulls, mended or not (D-076 Impact); the owner's sitting asks it (O-017 question 8).
- The Swarm's side plates are short (0.36 R): its fuselage has a flat side only behind the swept arms. Whether an Armored Swarm reads as armoured is a human check.
- The Armored plates are dark on a dark hull and read as a wider hull: that is the token's doing (`elites.md` R-ELT-11 keeps them `Pal.Enemy.Body`), and the models' package takes it up; nothing here changes it.
