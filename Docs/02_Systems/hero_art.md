# Hero art — S4 wave package C

> The Reclaimer at silhouette grade (`Validation/S4_wave1.md` §1 and §3, package C; D-043): massing, proportions and token colours that read at the gameplay camera, with no texture, no fine detail and no motion beyond a rigid pose. Rows opened: `Validation/S4_artbook_gaps.md` §2.1 r1 to r5, r12, r15, r16. Every row stays open for a later wave.

## Intent
The hero stops being a cylinder and becomes a figure a player finds at a glance in a crowd and reads the facing of: a pale armoured body under a hood, broad pauldrons, forearms carried forward. It is drawn only. The sim's hero is the same point, radius, speed and dash as before, and nothing here is read by a sim file.

## Rules
- **R-HAR-01 Authored here, from nothing.** Every hero mesh is built by `Tools/blender/hero_reclaimer.py` from numbers typed in that file. Nothing from outside the repo is read or copied: no model, no texture, no reference image traced, no import input, no generation service (`13_Decisions/OPEN.md` O-009). The file opens nothing and calls no Blender importer; Blender only exports what the file's own table holds.
  - T-HAR-01 `Tools/tools_test.py` `HeroArt`: the source imports only the standard library, `dtconst`, `dtpalette` and `bpy`, opens no file and names no Blender importer, library or image; every committed `SourceArt/Hero/SM_Hero_*.fbx` holds exactly the corners the source's table builds. `Tools\blender\Mesh.ps1 -Check` compares the bytes and is run at the package's acceptance, not by the gate.
- **R-HAR-02 Rigid parts, no skeleton.** The figure is eleven static meshes, `SM_Hero_Torso`, `Pauldrons`, `Hood`, `Visor`, `Undersuit`, `Hips`, `Belt`, `ArmL`, `ArmR`, `LegL`, `LegR`, each one instance on `M_InstanceColor`, with no collision. No `SK_` asset, no skeleton, no vertex animation (`07_Technical` R-TEC-06). Every part is authored in the figure's own space, so the idle pose is the identity for all of them.
  - T-HAR-02 `Dusktracer.HeroArt.Parts`: the pawn's figure holds one instance of each part's own mesh on `M_InstanceColor` with no collision. `HeroArt` in `Tools/tools_test.py`: the pose's part table and the mesh source name the same parts in the same order, and no presentation file names a skeletal type. The `licence` gate step fails any `SK_` asset.
- **R-HAR-03 The silhouette.** The figure stands on the floor and is read from above. The pauldrons are its widest level; the hood stands clear above them and is narrower; forearms, boots, the breastplate's keel and the helm's peak all point where the hero faces. It is stockier than a person and drawn at `C.HERO_ART.DRAW_SCALE`, because a figure of a person's build shows too little armour from this camera (R-HAR-06). At both frames, `C.ARENA.CAMERA_FRAME_RADIUS_UU` and `C.HERO_ART.REVERT_FRAME_RADIUS_UU` on a screen `C.HERO_ART.MASK_SCREEN_HEIGHT_PX` high (`36` and `27` px per metre), its mask as the camera sees it:
  - shares no more than `C.HERO_ART.OUTLINE_OVERLAP_MAX` with any enemy hull's mask, both scaled to one area and laid centre on centre, at any pair of facings. A disc and a square of one area share `0.83`, so the limit asks for a little more difference than that. An enemy hull is every `SourceArt/Enemies/SM_Enemy_*.fbx` that is not an optic; while there is none, the engine cube every role draws;
  - differs between any two of eight facings, the opposite one included, in at least `C.HERO_ART.FACING_DIFF_MIN` of the pixels either covers, armour and dark told apart.
  - T-HAR-03 `Tools/hero_outline.py` (`HeroArt` in `Tools/tools_test.py`, gate step `tools`): the outline, facing and build readings at both frames, from the same table Blender exports. A hero drawn as the enemy's cube fails all three; that test is there.
- **R-HAR-04 Tokens.** Every plate, the hood included, is `Pal.Hero.Armour`: the token the contrast gate reads, lit by the scene and never self-lit. The helm's peak, the face plate, the rebreather and the belt are `Pal.Hero.DarkMetal`; the waist and neck are `Pal.Hero.Undersuit`. The two dark tokens are accents that mark the front; they carry no contrast duty and sit close to `Pal.Ground.Shadow`. The art book draws a black hood. It is armour-toned here because the hood is most of what the camera sees of the head, and a dark one takes the brightest part of the figure away from above.
  - T-HAR-04 `Dusktracer.HeroArt.Parts`: each part's instance colour is its token's linear value with zero emissive; the eight plate parts are `Pal.Hero.Armour`. `Tools/palette_lint.py`: both new tokens parse and sit outside the enemy hue band.
- **R-HAR-05 The pose is a pure function.** `DTHeroPose::Evaluate` maps the hero's facing, velocity and dash to one transform per part, with no clock and no memory, so a held step always draws the same figure. Idle: every part at the identity, turned to `AimDir`. Moving: the upper body leans `C.HERO_ART.MOVE_LEAN_DEG` along the velocity about the waist, the head levels back by `C.HERO_ART.HEAD_LEVEL_RATIO` of that, the left leg leads and the right trails by `C.HERO_ART.MOVE_STRIDE_DEG` about the hips, the arms swing `C.HERO_ART.MOVE_ARM_DEG` against the legs; all scale with speed up to `C.HERO.MOVE_SPEED_UU_S`. It is one held stride, not a walk cycle. Dashing: lean `C.HERO_ART.DASH_LEAN_DEG` along the dash, both legs trail `C.HERO_ART.DASH_LEG_DEG`, both arms `C.HERO_ART.DASH_ARM_DEG`, the hips drop `C.HERO_ART.DASH_DROP_UU`. A part is turned and moved, never stretched. The joints are `C.HERO_ART.WAIST_Z_UU`, `NECK_Z_UU`, `SHOULDER_Z_UU`, `SHOULDER_Y_UU`, `HIP_Z_UU`, `HIP_Y_UU`, read by the mesh source and the pose alike. The pose and the figure live under `Source/Dusktracer/Presentation/`; `Player/DTHero.cpp` is the only file outside presentation that names them.
  - T-HAR-05 `Dusktracer.HeroArt.Pose`: idle is the facing and the draw scale and nothing else; the same input gives the same pose; every part keeps one uniform scale; each part turns about its joint; lean, stride and swing go the way the hero travels at eight facings times eight directions; a dash leans further than a walk and trails both legs and arms; the stride is capped at the hero's speed. `PresentationReadsOnly` in `Tools/tools_test.py`: no sim file includes a presentation header.
- **R-HAR-06 The body still marks the hero.** The figure stands on the floor at the hero's sim position. `C.SIM.PLANE_Z_UU`, the cue heights of `04_Presentation` R-PRE-11 and the aim plane stay where they are: only the draw moved. The centre of the hit circle projects inside the figure's mask at every facing, and the pelvis stays over the hero's point in every pose. The empty arena's gate frame shows at least `C.HERO_ART.ARMOUR_PX_MIN` armour-tone pixels, what the cylinder showed, and the hero's contrast reading does not fall under the cylinder's.
  - T-HAR-06 `Tools/hero_outline.py`: the footprint at eight facings and both frames; `--frame` reads the armour's pixel count from an `arena_empty` shot, and `HeroArt` reads it from the committed golden. `Dusktracer.HeroArt.Pose`: the pelvis stays on the axis while moving and dashing. Gate step `contrast` (T-PRE-05) reads the hero against both ground references.
- **R-HAR-07 A staged frame.** `Tools/Scenes/hero_lineup.json` shows the figure as the game camera sees it: the hero idle at the centre and twelve drawn figures round it, four facings in the idle, moving and dashing poses. The figures are presentation only; the scene stages no sim state.
  - T-HAR-07 gate step `smoke` judges the scene against `Docs/Validation/goldens/hero_lineup_en.png`. `HeroArt`: the scene runs `DT.HeroArt.LineUp` alone, and that command's file names no subsystem and no hero state.
- **R-HAR-08 Reads, never writes.** Drawing reads `FDTHeroState` and changes nothing in it (`04_Presentation` R-PRE-11). No sim number, rule, hit radius, fire origin, dash or input moved with this package, and no existing token value.
  - T-HAR-08 `Dusktracer.HeroArt.Parts`: a stepped run's hero state is the same after the pawn is drawn in it, and the figure shows that state. The sim proof of `Validation/S4_p0.md` §8: seed `9107` reads the same hashes.

## Numbers
All presentation-only, tag GATED(S4), rows in `08_Constants.md` under Combat and director. `C.HERO_ART.DRAW_SCALE`; the six joints of R-HAR-05; the eight pose angles, offsets and ratios of R-HAR-05; the readings' frame and limits, `C.HERO_ART.REVERT_FRAME_RADIUS_UU`, `C.HERO_ART.MASK_SCREEN_HEIGHT_PX`, `C.HERO_ART.OUTLINE_OVERLAP_MAX`, `C.HERO_ART.FACING_DIFF_MIN`, `C.HERO_ART.ARMOUR_PX_MIN`. Part sizes are not constants: they are the mesh, typed once in the generator.

## Data
- Source: `Tools/blender/hero_reclaimer.py`. Build and import: `& Tools\blender\Mesh.ps1 -Source Tools\blender\hero_reclaimer.py`; `-Check` proves the committed files are what it builds.
- Assets: `SourceArt/Hero/SM_Hero_*.fbx`, `Content/DT/Meshes/SM_Hero_*.uasset`, under the licence rows P0 logged in `09_Catalog/licenses.csv`.
- Tokens: `Pal.Hero.DarkMetal` and `Pal.Hero.Undersuit`, rows of `04_Presentation` §3, values from the art book's PL.05.
- Code: `Presentation/DTHeroPose` (the pose and the part table), `Presentation/DTHeroFigure` (the component that draws it), `Player/DTHero` (the pawn that owns one), `Debug/DTDebugHeroArt.cpp` (`DT.HeroArt.LineUp`, not in Shipping).

## Acceptance
- Gate steps `tools`, `tests`, `smoke`, `contrast`, `palette`, `licence`, `docs` with the tests above; `Dusktracer.UX.CameraReach` and `Dusktracer.UX.CameraFixed` unchanged.
- `Tools\blender\Mesh.ps1 -Source Tools\blender\hero_reclaimer.py -Check` prints `MESH: PASS`.
- The sim proof of `Validation/S4_p0.md` §8.
- **Human line, not verified by any check here:** that a person finds the hero at a glance in a fight and names its facing, at either frame. The outline and facing readings are proxies: they compare masks, and a mask is not a person looking. The figure against package B's hulls is read by the same tool once those meshes are on the branch.

## Telemetry
None.

## Text
None.

## Out of scope
- Everything `Validation/S4_wave1.md` §3 holds out of package C: the Patron lamp, tabard, back rig, weapon mounts, conduit and docks, textures and inlay, the emissive layer, wear.
- A walk cycle, blending between poses over time, a downed pose, hit reactions.
- The art book's three silhouette tests (`Validation/S4_artbook_gaps.md` §2.1 r3): each needs the back rig, the lamp or the weapon mounts, which are held out. The build, outline and facing readings of R-HAR-03 stand in their place at this grade.
- A facing cue outside the body, such as an aim mark on the floor.

## Open
- **Size against the hit circle.** At `C.HERO_ART.DRAW_SCALE` the figure is taller than the art book's and wider across the pauldrons than the hit circle, and it covers more of the screen than the cylinder did, so it hides more of the floor behind it. That is the price of R-HAR-06's armour pixel floor at this camera. The scale is one constant; lowering it trades found-at-a-glance for floor. A person's reading decides.
- **The hood's tone** (R-HAR-04) departs from the art book for the same reason and waits on the same reading.
- **The pose snaps** between idle, moving and dashing, because it has no clock. Whether that reads as motion or as flicker is a human line.
