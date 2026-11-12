# Enemy art — S4 wave package B

> Cast-off silhouettes (`Validation/S4_wave1.md` §3 package B; D-043). Silhouette grade: massing, proportions and token colours that read at gameplay camera; no texture, no panel detail, no motion beyond turning whole. It replaces the cube of `enemies.md` as what is drawn for four roles and changes nothing the sim reads.

## Intent
Swarm, Tank, Rusher and Turret stop being cubes: each is told from the others by its outline, at the frame of D-036 and at the frame it reverts to. The hull stays the dark `Pal.Enemy.Body` silhouette and the optic stays the brightest point of the enemy (`enemies.md` R-ENM-24, unchanged). The boss hull stays the cube.

## Rules
- **R-EAR-01** Each of Swarm, Tank, Rusher and Turret is drawn as its own two instanced static meshes, `SM_Enemy_<Role>` and `SM_Enemy_<Role>_Optic`: one hull instance per live enemy of the role and one optic instance per enemy of the role that is not an elite, on `M_InstanceColor` with the tokens and emissive of R-ENM-24. No actor per enemy (R-TEC-11), no skeleton, no vertex animation. `EDTEnemyRole::Boss` has no mesh key and keeps the cube and its optic cube, where they were. Elite parts and lights stay the boxes of `elites.md` R-ELT-10 and R-ELT-11, placed by R-EAR-08.
- **R-EAR-02** Draw size is the mesh's, hit radius is the catalogue's. A role mesh is modelled in units of the hit circle's diameter, with its origin on the floor under the hit centre and +X as the facing, and one unit imports as the cube's edge. It is drawn at the cube's scale (hit diameter per unit; `C.ELITE.DRAW_SCALE` times that for an elite), standing on the floor, turned whole with the facing. How far a hull reaches past its hit circle is a property of its generator; no sim file reads it, and no `enemies.csv` row or radius moves.
- **R-EAR-03** Every role mesh is built headless from `Tools/blender/enemy_<role>.py` through the shape kit `Tools/enemy_shapes.py`: closed convex parts, wound outward, flat shaded, its own mirror across the facing axis, one material slot, colours by token name. Hull plus optic of a role stay within `C.EAR.ROLE_TRIS_MAX` triangles; at that size one level of detail is enough at `C.ENEMIES.CAP`, so no LOD is built at this grade. Nanite is off (`04_Presentation` §4).
- **R-EAR-04** The optic's shape and anchor are the role's: a lens block on the nose (Swarm), a periscope bar on the slab's top edge (Tank), a strip across the nose (Rusher), a six-sided mirror on the disc (Turret). It is modelled in the hull's frame and takes the hull's transform. Seen from the game camera with its own hull in front of it, its visible area is at no facing under the least the optic cube showed, and on average not under the cube's average. The elite light, placed by R-EAR-08, shows over the role hull at least `C.EAR.ELITE_LIGHT_VISIBLE_MIN` of the least today's cube light showed over the cube.
- **R-EAR-05** Rounds are opaque and depth-tested, so a hull hides what is behind it. Hull and optic together hide no more floor than `C.EAR.FLOOR_HIDE_MAX_RATIO` of what the cube and its optic cube hid, in area covered from the camera and in reach behind the hit centre; the role's worst facing is held against the cube's best.
- **R-EAR-06** The grey box is kept. The renderer still holds one cube per enemy and one optic cube per optic, in step every frame, and draws them in place of the role meshes when a role mesh is missing from the build (`Fallback_EnemyRoleMesh`, R-TEC-05: one missing mesh puts every role back, so no role draws at another grade) or while `dt.Exp.EnemyGreyBox` is on (not in Shipping), which gives a before and an after on one build. In play the cubes are not drawn.
- **R-EAR-07** Any two roles' outlines differ: at each density of `C.EAR.SILHOUETTE_PX_PER_M`, every facing of one against every facing of the other, the two masks brought to one area and one centre overlap (intersection over union) by at most `C.EAR.SILHOUETTE_OVERLAP_MAX`. Two cubes overlap wholly. This is a proxy: it shows that the outlines differ, and it does not replace a person naming the role, which `enemies.md` and `elites.md` keep as human checks.
- **R-EAR-08** An elite's parts are placed from its role's hull, not from its hit circle (D-076): the light covers the role's optic where that optic sits (`elites.md` R-ELT-10), a box of the optic's own bounds a little over, and each affix part of `elites.md` R-ELT-11 rests against the face of the hull it belongs to, a plate along each side, rear parts against the rear, the spike from the nose, the shield's orbit outside the hull's longest reach. Between a part and its hull there is no gap wider than `C.EAR.ELITE_PART_GAP_MAX_UU` (the shield's plates, which orbit clear of it, excepted) and no part is sunk deeper than `C.EAR.ELITE_PART_SINK_MAX_UU`, at any role and affix. The anchors are read from the role meshes that are drawn: `Tools/elite_anchors.py` measures them on the generators' own lists and writes `Enemies/DTEliteAnchorsGen.h`, never by hand. The grey box and the boss keep the cube's placement (R-EAR-06). Counts, tokens, the light's emissive and its blink are `elites.md`'s and do not change.

## Numbers
| Number | Key | Tag | Rule |
|---|---|---|---|
| Floor a hull may hide, against its cube | C.EAR.FLOOR_HIDE_MAX_RATIO | GATED(S4) | R-EAR-05 |
| Greatest outline overlap of two roles | C.EAR.SILHOUETTE_OVERLAP_MAX | GATED(S4) | R-EAR-07 |
| Densities the outlines are judged at | C.EAR.SILHOUETTE_PX_PER_M | GATED(S4) | R-EAR-07 |
| Elite light over a role hull, against over the cube | C.EAR.ELITE_LIGHT_VISIBLE_MIN | GATED(S4) | R-EAR-04 |
| Triangles of a role, hull plus optic | C.EAR.ROLE_TRIS_MAX | GATED(S4) | R-EAR-03 |
| Line-up slot size, facings per role | C.EAR.LINEUP_SPACING_UU, C.EAR.LINEUP_ROWS | GATED(S4) | T-EAR-07 |
| Channel distance from the floor that counts as drawn | C.EAR.FRAME_MASK_TOLERANCE | GATED(S4) | T-EAR-07 |
| Widest gap between an elite part and its hull | C.EAR.ELITE_PART_GAP_MAX_UU | GATED(S4) | R-EAR-08 |
| Deepest an elite part may sit in its hull | C.EAR.ELITE_PART_SINK_MAX_UU | GATED(S4) | R-EAR-08 |

All presentation-only: none has a reader in the sim. Proportions of a hull live in its generator, in units of the hit diameter, and are bounded by the rules above, not by keys.

## Data
- Asset keys, each with its licence row in `09_Catalog/licenses.csv`: `Tools/blender/enemy_<role>.py` (role keys `swarm`, `tank`, `rusher`, `turret`; fiction is never a key), `SourceArt/Enemies/SM_Enemy_<Role>.fbx` and `SM_Enemy_<Role>_Optic.fbx`, `Content/DT/Meshes/SM_Enemy_<Role>` and `SM_Enemy_<Role>_Optic`. Built by `Tools\blender\Mesh.ps1 -Source` (`Validation/S4_p0.md` §2).
- `ADTEnemyRenderer`: `GetRoleHull(Role)` and `GetRoleOptic(Role)` are what is drawn; `GetHull()` and `GetOptic()` are the grey box of R-EAR-06, which T-ENM-24 reads. `UDTEnemySubsystem::PlaceParts` is the grey box's placement; the renderer draws a role mesh from the hull's transform alone.
- Silhouettes from above, following `enemies.md` Model language: Swarm a "Y" (stem, two arms swept forward, a fan pod on each); Tank a wall with a body behind it (a slab wider than the machine across the front, two tracks trailing, open at the rear); Rusher a long shuttle tapered at both ends; Turret a six-sided disc on a mast over three feet, with a focus tube pointing along the facing.
- Fallback: `Fallback_EnemyRoleMesh` (`09_Catalog/fallbacks.csv`).
- Debug (not in Shipping): `DT.Enemies.LineUp` stands the four roles round the first hero, one column per role and `C.EAR.LINEUP_ROWS` facings each; `noshadow` stages them without cast shadows. It is drawn only: a second renderer, no enemy in the pool. `dt.Exp.EnemyGreyBox`. `dt.Exp.EliteCubeAnchors` on places elite parts from the hit circle on the role hulls too, the placement before R-EAR-08, for a before and an after on one build; `DT.Enemies.EliteLineUp` stands every role round the hero as an elite with each affix and with all of them (`cube` gives the placement before).
- Elite anchors: `Tools/elite_anchors.py` (regenerate after any generator changes; `--check` says whether the committed header is current) writes `Source/Dusktracer/Enemies/DTEliteAnchorsGen.h`, one row per role in drawn radii in the hull's frame; `DTElite::BuildParts` places from it.

## Acceptance
- T-EAR-01 `Dusktracer.EnemyArt.RolePath`: with every role and an elite live, each role's own components hold that role's enemies, one hull each and one optic per non-elite, on the role's own mesh (the cube for the boss), with the tokens of R-ENM-24 on the grey box's material; the cubes are hidden; the drawn total equals the live count; instances follow the count down.
- T-EAR-02 `Dusktracer.EnemyArt.Placement`: per role, the hull is over the enemy, on the floor, turned as the cube, at the hit diameter per unit and at `C.ELITE.DRAW_SCALE` for an elite; the optic takes the hull's transform; the boss hull and optic are the cube's transforms; hit radii are the catalogue's.
- T-EAR-03 `Dusktracer.EnemyArt.Meshes`, and `parts` and `triangles` of `Tools/enemy_art_check.py`: every role mesh loads, has one level of detail, one slot, no Nanite, nothing under the floor, and hull plus optic within `C.EAR.ROLE_TRIS_MAX`; every part is closed, convex and wound outward and the mesh is its own mirror. `Tools\blender\Mesh.ps1 -Check` per generator: the committed `SourceArt` file is what the generator builds.
- T-EAR-04 `optic` and `elite light` of `Tools/enemy_art_check.py`: R-EAR-04's two readings from the generators' geometry, every facing.
- T-EAR-05 `floor` of `Tools/enemy_art_check.py`: R-EAR-05's area and reach, every facing.
- T-EAR-06 `Dusktracer.EnemyArt.GreyBox`: `dt.Exp.EnemyGreyBox` on draws the cubes and hides every role component; off puts the role meshes back.
- T-EAR-07 `silhouette` of `Tools/enemy_art_check.py`: R-EAR-07 on the generators' geometry (gate step `tools`), and on frames the game captures of the line-up at each density (`Tools\EnemyArt.ps1`), where the mask is every pixel of a slot that is not floor: the hull and its optic with its glow, staged without cast shadows, whose shape is the sun's. Golden `enemy_lineup_en` (scene `Tools/Scenes/enemy_lineup.json`) records the line-up as played, shadows on.
- T-EAR-08 `elite parts` of `Tools/enemy_art_check.py` and `Dusktracer.EnemyArt.EliteAnchors`: on the generators' solids, every part of every role, each affix alone and all together and the shield over its orbit, within R-EAR-08's gap and sink, and the light's centre inside the optic's bounds; placed by the hit circle on the role hulls the same reading fails on every role (a tools test). The engine test holds the generated anchors equal to the committed meshes' bounds, places every part within the gap of the hull's bounds and the light's centre in the optic's bounds at the elite's scale, and with no anchors (the grey box, the boss) gives the hit circle's transforms exactly. Golden `elite_lineup_en` (scene `Tools/Scenes/elite_lineup.json`): every role as an elite with each affix and one with every affix.
- `Tools\EnemyArt.ps1` runs T-EAR-03 to T-EAR-07's tool halves in one command; it starts Blender in the background and the game on the hidden desktop, so it is the package's acceptance and not a gate step.
- Unchanged and passing: T-ENM-24 `Dusktracer.Enemies.OpticPart`, T-ELT-10 `Dusktracer.Pre.EliteLight`, T-ELT-11 `Dusktracer.Elites.Parts`.
- Human, not verified by any check here: a person names each role from its shape at gameplay camera, in a crowd and under fire (`enemies.md`); the Tank's slab reads as the facing that takes reduced damage; the Turret's tube shows its aim before the line.

## Telemetry
None.

## Text
None. Role names stay `ST_Enemies.Enemy.<Role>.Name`.

## Out of scope
Everything `Validation/S4_wave1.md` §4 holds back: the ceramic hull and any hull colour but `Pal.Enemy.Body` (D-010), textures and panel detail, Keeper guides, the Ichor cell and seal, vertex animation and gaits, a split Turret head, modelled elite parts, the Warden, the Breaker and Tallyman models, LODs past the triangle cap, the Rusher's optic turning `Pal.Telegraph` in its wind-up.

## Open
- The Rusher's body is narrower than its collision width, so the hull does not show how wide the charge is; the ground band does (R-ENM-13). Widening it brings its outline toward the Tank's.
- A Swarm's hull is wider than `C.ENM.SEPARATION_RADIUS_UU`, so neighbours in a flock overlap at the fan pods.
