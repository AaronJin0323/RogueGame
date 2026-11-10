# Arena art — S4 wave package A

> The ground of `L_Arena_Test`: slabs on the play plate and a stepped stone apron past it (`Validation/S4_wave1.md` §3 package A, D-043; gap rows §2.8 floor, arena edge and stone master, §2.5 Keep neutrals as a reading). Silhouette grade: massing, joints and token colours, no texture. Presentation only: nothing here is read by the sim. Readings and frames: `Validation/S4_a_arena.md`.

## Intent
The frame never shows nothing. Before this package the plate stopped at `C.ARENA.HALF_EXTENT_UU` and a hero at the clamp saw black past it. Now the plate is paved, and past its edge stone steps fall away into shadow on every side, far enough that no frame the hero can make, at today's frame or at the one D-036 reverts to, reaches the end of the ground.

The floor is where every round, telegraph and cue is read (`04_Presentation` R-PRE-05), so the plate changes as little as a floor can: the same albedo, the same tone, and slabs told only by joints the low sun barely shades. The apron is where nothing is judged, because the hero cannot stand there, so it is allowed to go dark; it carries the edge of the arena as a step down, never as a wall.

## Rules
- **R-ARN-01** The play plate is one static mesh, `SM_Env_Plate`, over exactly `C.ARENA.HALF_EXTENT_UU`, wearing `M_Ground`, which stays `Pal.Ground.Lit` (R-PRE-05). Slabs are relief only: courses `C.ARN.SLAB_COURSE_UU` wide of slabs between the two lengths of `C.ARN.SLAB_LENGTH_UU`, cross joints staggered from course to course, lengths drawn from the generator's own stream seeded by `C.ARN.LAYOUT_SEED`. Every slab top is flat at the floor's height; a joint is two chamfers, each `C.ARN.JOINT_HALF_WIDTH_UU` wide, meeting `C.ARN.JOINT_DEPTH_UU` under the floor. Nothing is raised: the floor stays under the lowest cue layer `C.PRE.ZONE_Z_UU`, and a chamfer is flatter than `C.ARENA.SUN_ELEVATION_DEG`, so the sun lights both sides of every joint. The plate casts no shadow: nothing lies under it but its own joints.
- **R-ARN-02** Past the plate lies the apron, `SM_Env_Apron`: `C.ARN.APRON_STEPS` steps down from the plate's edge, each `C.ARN.APRON_RISE_UU` high with a tread `C.ARN.APRON_TREAD_UU` deep and slabbed like the plate, then one fall of `C.ARN.APRON_SKIRT_DROP_UU` to a plain skirt that runs `C.ARN.APRON_REACH_UU` past the plate on every side. No part of it stands above the floor, so no side of the arena hides floor from the camera, and there is no fog. It is sized for the frame radius `C.ARN.FRAME_RADIUS_MAX_UU` and for a window as wide as `C.ARN.ASPECT_MAX`, not only for `C.ARENA.CAMERA_FRAME_RADIUS_UU` at the target window: a hero anywhere the clamp allows makes no frame whose corner lands past the skirt.
- **R-ARN-03** The ground does not collide. Neither mesh carries a collision shape and both actors ask for none. The arena's edge is the sim's clamp (`combat_core.md`), and this spec moves no sim number: `C.ARENA.HALF_EXTENT_UU`, the hero's clamp, every pool's bounds and seed `9107` are as they were.
- **R-ARN-04** The apron wears the stone master `M_Env_Stone`. Its base colour is `Pal.Ground.Lit` falling to `Pal.Ground.Shadow`: by depth under the floor, reaching the shadow token at the depth of step `C.ARN.FALL_STEPS`, and at once on any face that is not a top, so every riser is shadow stone. Roughness and metal are `M_Ground`'s. No other colour enters it (R-PRE-09), and no token value changes.
- **R-ARN-05** No frame shows the clear colour. Stone in full shadow is still stone: `M_Env_Stone` adds an emissive of `C.ARN.SHADOW_FLOOR` times `Pal.Ground.Shadow`, far under the bloom threshold, so that the darkest pixel of the apron stays above `C.ARN.VOID_MAX_LEVEL` and a hole in the ground can be told from a shadow by a tool.
- **R-ARN-06** The slabs do not move the floor's tone. On the empty arena the lit floor's spread stays inside `C.PRE.LIT_GROUND_Y_WINDOW`, and the darkest hundredth of it, the joints' shaded side, stays at or above the luminance where a black band still meets `C.PRE.CONTRAST_MIN`, so a telegraph keeps its edge across a joint. The sun, the sky light, exposure, bloom and every post-process value are untouched (`04_Presentation` §4).

`DT.Arena.Frame <radius>` stages a tool run's camera at another frame radius for the edge frames of R-ARN-02. It is compiled out of Shipping, nothing in play calls it, and the game's frame stays fixed (`06_UX` R-UX-02).

## Numbers
All in `08_Constants.md`, "Arena and measurement", as `C.ARN.*`; each is presentation-only and new in this package (`number_change_log.csv`).

| Key | What it sets |
|---|---|
| `C.ARN.SLAB_COURSE_UU`, `C.ARN.SLAB_LENGTH_UU`, `C.ARN.LAYOUT_SEED` | the slab pattern |
| `C.ARN.JOINT_HALF_WIDTH_UU`, `C.ARN.JOINT_DEPTH_UU` | the whole relief of the plate |
| `C.ARN.APRON_STEPS`, `C.ARN.APRON_RISE_UU`, `C.ARN.APRON_TREAD_UU`, `C.ARN.APRON_SKIRT_DROP_UU`, `C.ARN.APRON_REACH_UU` | the apron's shape |
| `C.ARN.FRAME_RADIUS_MAX_UU`, `C.ARN.ASPECT_MAX` | the widest frame the apron is sized for |
| `C.ARN.FALL_STEPS`, `C.ARN.SHADOW_FLOOR` | the stone master |
| `C.ARN.VOID_MAX_LEVEL` | the clear colour, as the frame check reads it |

## Data
- `Tools/blender/env_arena.py` builds `SM_Env_Plate` and `SM_Env_Apron` through `Tools/blender/Mesh.ps1` (`SourceArt/Env/`, `Content/DT/Meshes/`); `-Check` proves the committed export is what the source builds.
- `Tools/create_arena.py` builds `M_Env_Stone` and the map: the two meshes at the origin, unscaled, not colliding, the plate in `M_Ground`. `Tools/CreateArena.bat` runs it. It rewrites the four older materials with the same content; only the map and `M_Env_Stone` are committed from a run.
- Scenes: `edge_corner` and `edge_corner_wide` (gate), and by hand `edge_xp`, `edge_xn`, `edge_yp`, `edge_yn` with their `_wide` twins: a corner and the middle of each side, at the game's frame and at `C.ARN.FRAME_RADIUS_MAX_UU`.
- Licence rows: `SourceArt/Env/*`, `SM_Env_*`, `M_Env_*` (`09_Catalog/licenses.csv`, logged by P0).

## Acceptance
- T-ARN-01 `Dusktracer.Arena.PlateRelief`: the plate's bounds are the arena's, its top is the floor and under the lowest cue layer, its lowest point is the joints' depth and no deeper, and a chamfer is flatter than the sun is low.
- T-ARN-02 `Dusktracer.Arena.ApronReach`: nothing of the apron stands above the floor; with the game's camera sums, every corner of the frame lands inside the skirt for a hero at each corner and each side's middle of the clamp, at both frame radii and at both the target and the widest window, judged on the apron's lowest surface where a ray lands farthest out; the same frames reach past the plate alone, so the check can fail.
- T-ARN-03 `Dusktracer.Arena.GroundDoesNotCollide`: neither mesh has a collision shape, every static mesh actor of the map asks for no collision and sits unmoved at the origin. Seed `9107` is the sim's proof: `Tools/RestartCheck.ps1` and the five `Tools/BotRuns.ps1` arms of `Validation/S4_p0.md` §8 read as before.
- T-ARN-04 the same test: the map holds one plate in `M_Ground` and one apron in `M_Env_Stone` and no other ground mesh; `ArenaArt` in `Tools/tools_test.py` holds the generator to the two ground tokens, and the `palette` gate step to no literal colour.
- T-ARN-05 `Tools/arena_check.py --void`: no pixel outside the HUD is at or under `C.ARN.VOID_MAX_LEVEL` in the ten edge frames.
- T-ARN-06 `Tools/arena_check.py --floor`: on the `arena_empty` frame the lit floor's spread is inside `C.PRE.LIT_GROUND_Y_WINDOW` and its darkest hundredth at or above the black band's floor. The gate's `contrast` step holds the seven elements and its WATCH line the margin; its `settings` step the render settings.
- Human, not verified by any check: whether the floor looks like the Keep, and whether the arena's edge reads as an edge in play.

## Telemetry
None.

## Text
None.

## Out of scope
- The Keep wall, the Breach, towers and every piece of architecture; rubble and tipped slabs; conduits; moss, stains, cracks, chipped corners and any texture or mask; water; fog and both weather rigs (`Validation/S4_wave1.md` §4).
- A new floor target or any change to a `Pal.Ground.*` value: the book's displayed tones against the token table's albedo is a D-nnn of its own (`Validation/S4_artbook_gaps.md` §4.3).
- Lighting, sky, exposure, post-process and scalability tiers.

## Open
- The joints are as faint as R-ARN-06 makes them: a deeper joint reads better and puts its shaded side under the black band's floor. Slabs told by tone need the floor target above.
- Roughness is one value on every slab. The plan allows roughness to tell slabs apart; it was not tried.
