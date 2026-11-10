# S4 wave 1: what can start on `dev/s4` before the exits

For the scheduler. A plan, written on `plan/s4-entry` from `dev/s3` at 9fafa8f. It starts nothing: it is the scope D-043 points at; D-043 was accepted on 2026-11-10, with the calls of §5 taken as recommended, and it stands as that record is accepted. Nothing was built, launched or measured for it.

Rows are cited from `Validation/S4_artbook_gaps.md` as section and position: "§2.6 r8" is the eighth row of the table in that note's §2.6, at 9fafa8f. Sizes are that note's (S up to 0.5 d, M 1 to 2 d, L 3 to 5 d, XL over a week, per session). The note was written at the 2000 uu frame; D-036 has since moved the frame to 1500 uu (36 px/m), and the note's pixel figures were not recomputed.

## 1. The four tests for a row

A row is in wave 1 only if all four hold.

- **(a) No open human reading under it.** Open readings: the frame (D-036 reverts to 2000 uu on one validator's report), D-036's three held items (screen-wide sim effects against the frame, spawn arrival, a Warden edge marker), the palette (D-008), every readability rule of `04_Presentation §2`, elites told apart (`elites.md`), Time Dilation's scale.
- **(b) No sim number and no rule moves.** No existing value in `constants.yaml`, no catalogue row, no rule text. New keys are presentation-only. Seed 9107 hashes cannot move.
- **(c) No AI-sourced asset.** O-009 is open. Every mesh and texture comes from a script in this repo, run headless: Blender 5.2.2 at the path in `STATE.md`, or editor Python (R-TEC-06, R-PRE-10).
- **(d) Checked without a window.** The contrast gate, golden frames through the hidden-desktop shot path (`Tools/Smoke.ps1`), the perf gate (`Tools/Perf.ps1`), and headless tests. A check that needs a person looking is not an acceptance check here; it is listed as not verified.

Most model rows are opened to **silhouette grade** and not closed: the massing, proportions and token colours that read at gameplay camera, with no texture, no fine panel detail and no motion beyond a rigid pose. Silhouette grade must read at 36 px/m and still read at 27 px/m, so a frame revert costs nothing. The rest of each row stays in the gap note for a later wave.

## 2. Rules every package keeps

1. **Sim proof at merge.** Gate step `seed` (seed 9107 `input_hash` on the arms the earlier records used), `Tools/RestartCheck.ps1` hashes, and one fixed `Tools/BotRuns.ps1` sample whose `bot.csv` is byte-identical before and after.
2. **Presentation reads, never writes** (R-PRE-11). A cosmetic effect draws its randomness from its own stream, never from a sim stream. New presentation code may include sim headers; no sim file includes a presentation header. P0 adds that check to `Tools/tools_test.py`.
3. **Spec before code** (AGENTS). Each package writes its own spec file under `Docs/02_Systems/` first, with the nine contract headings, so no two packages edit one spec.
4. **Tokens.** No existing token value changes. A new token gets its row in `04_Presentation §3` before it is drawn.
5. **Margin watch.** Each package reports the contrast gate's telegraph and round readings before and after (3.27 and 3.18 on lit ground when D-036 landed). The gate fails under `C.PRE.CONTRAST_MIN`; the wave stops a package earlier, at the floor chosen in §5 item 4.
6. **Perf is read at merge, one run at a time.** `Saved/gpu.lock` is per worktree and cannot see another agent's compiler (`S2_perf.md`), so a reading taken while other agents build is void. A package reports its own reading as a bound; the scheduler's reading on the merged tree, with nothing else running, is the verdict.
7. **No windows** (R-TEC-17). Every launch goes through a `Tools` script on the hidden desktop. Blender runs with `-b`.
8. **Licence row before the binary** (R-PRE-10). P0 creates `licenses.csv` with one glob row per wave 1 asset family, so no package edits it.
9. **Shared and generated files.** `constants.yaml`, `08_Constants.md` and `number_change_log.csv` take new rows only, each package in its own block. `DTConstantsGen.h`, `DTPaletteGen.h` and the `.locres` files are regenerated at merge, never merged. `Tools/Gate.ps1`, `Tools/Smoke.ps1`, `Tools/Perf.ps1` and `Tools/contrast_check.py` are edited by P0 only; a package that needs a limit moved says so in its report and the scheduler moves it at merge with the reading beside it.
10. **Nothing leaves `dev/s4`** (D-043 point 3).

## 3. The packages

P0 lands first and alone. A to F then run in parallel and merge one at a time in the order below, which is player-visible value per unit of risk. G waits on F and on a call.

| | Package | Gap rows opened | Gap-note size of those rows | Wave 1 share (estimate) |
|---|---|---|---|---|
| P0 | Foundations | 7 | 13.5 d | 8 d |
| A | Arena ground and edge | 4 | 13.5 d | 8 d |
| B | Cast-off silhouettes | 8 | 20.5 d | 11 d |
| C | Reclaimer silhouette | 8 | 17 d | 8 d |
| D | Rounds and tracers | 10 | 9 d | 7.5 d |
| E | Hit and kill feedback | 1, and halves of 3 shared with D | 4 d | 5.5 d |
| F | HUD pass | 11 | 5.5 d | 5.5 d |
| G | Type | 4 | 3 d | 3 d |
| | Total | 53 of 212 | 86 d | about 57 d |

### P0. Foundations (first, alone)

No pixel changes. Everything else is blocked on it.

- **Rows:** §2.5 r15 the headless Blender to import pipeline (L); §2.5 r18 the licence log and its check (M); §2.5 r20 the palette lint and the render-settings step, the checks R-PRE-02 and R-PRE-07 name as planned (S); §2.5 r12 the asset key list, for the wave 1 families only (S); §2.5 r19 the presentation specs, as one empty spec file per package with its headings (L, a small share); §2.8 r22 the weather pin and new goldens (M); §2.3 r48 the perf scene (M, the part that needs no art: telegraphs held live, and rounds that are drawn the way played rounds are).
- **Files and systems:** `Tools/blender/` (new: a launcher that runs the pinned Blender with `-b`, exports to `SourceArt/`, and an editor-Python import step), `SourceArt/` (new), `Docs/09_Catalog/licenses.csv` (new), `Tools/catalog_lint.py` (new, gate step), `Tools/palette_lint.py` (new), `Tools/Gate.ps1`, `Tools/Smoke.ps1` (scenes become data: one file per scene under `Tools/Scenes/`, so a package adds a file and does not edit the script), `Tools/Perf.ps1`, `Tools/contrast_check.py` (per-element limits in one block each; a lit-ground spread reading, the 5th and 95th percentile of lit floor luminance, next to today's single ground reading), `Tools/tools_test.py`, `Tools/create_arena.py` (reads tokens instead of a literal, same pixels).
- **Acceptance:** the gate passes on `dev/s4` with every `dev/s3` step and the new `licence` and `palette` steps; a one-triangle test mesh goes Blender to `SourceArt/` to `Content/DT/` and back out of the cook with one command and no window; `catalog_lint.py` fails a binary with no row (a test proves the failure); the `arena_empty` goldens are unchanged, read by the existing image check with no `-Update`; new goldens exist for a staged combat frame and an edge frame, en and zh-Hans; contrast 7/7 with the same seven readings as before; `Tools/Perf.ps1` Caps reads within noise of the last `dev/s3` reading.
- **Must not change:** any rendered pixel, any sim file, any existing constant.
- **Depends on:** the calls of §5 items 1, 3 and 4.

### A. Arena ground and edge

What a player sees first: today the plate stops at ±4000 uu and the camera follows the hero to 3960, so the frame shows void past the edge in play.

- **Rows:** §2.8 r5 the arena edge (L): an apron past the plate on all four sides, non-colliding, so no frame the camera can reach shows void. §2.8 r2 the floor (L): slab pattern on the play plate. §2.5 r4 Keep neutrals and the floor target (M): the reading only, see below. §2.8 r4 the stone master (L, the material alone; the architecture kit is later).
- **How it stays inside the rules:** R-PRE-05 says `M_Ground` stays `Pal.Ground.Lit`, and the gap note leaves about 5 percent to darken before the dark band fails. So on the play plate the albedo stays the token and slabs are read by shallow relief and roughness only. The apron may go darker, toward `Pal.Ground.Shadow`, because no telegraph or round is judged on it: the hero cannot stand there. The apron covers the reach of the 2000 uu frame, not only today's 1500, so a frame revert shows no void either (side reach 3556 uu at 2000, D-036's table).
- **Files and systems:** `Tools/create_arena.py`, `Content/DT/Maps/L_Arena_Test.umap` (regenerated; no other package touches the map), `Content/DT/Materials/M_Ground` and a new stone master, `Tools/blender/env_*.py`, `SourceArt/Env/`, `Content/DT/Meshes/SM_Env_*`, `Docs/02_Systems/arena_art.md`, one scene file for the edge frames.
- **Acceptance:** edge goldens with the hero at a corner and at the middle of each side, at both frame radii: an image check finds no pixel of the clear colour. Contrast 7/7, with telegraph and round not under the §5 item 4 floor. The lit-ground spread from P0 stays inside the usable window of the gap note's §4.3 (about Y 0.106 to 0.16) at both percentiles, and a staged telegraph and enemy round over a slab joint read at or above `C.PRE.CONTRAST_MIN`. Perf at caps inside the package allowance of §5 item 5. The sim proof of §2.
- **Must not change:** `C.ARENA.HALF_EXTENT_UU`, the hero clamp, any collision; the sun, sky, exposure, bloom or any post-process value; any token; the play plate's mean albedo.
- **Depends on:** P0; the calls of §5 items 2 and 4.
- **Not verified by this check:** whether the floor looks like the Keep. That is a person's judgement.

### B. Cast-off silhouettes

Four roles stop being cubes.

- **Rows:** §2.6 r8 the per-role render path (XL, a share: one instanced mesh per role, static, LODs; no vertex animation, no skeleton); §2.6 r7 draw size apart from hit radius (S); §2.6 r9 Scrapwing (M), r10 Bulwark (L), r11 Courser (L), r12 Flare Pylon (M), each to silhouette grade; §2.6 r4 the optic's shape and anchor per role (M); §2.7 r26 role render paths (S).
- **How it stays inside the rules:** hulls stay flat `Pal.Enemy.Body` with zero emissive on `M_InstanceColor`, one hull instance and one optic instance per enemy, so R-ENM-24 and its test hold as written. The Pylon is one mesh that turns whole, as the cube does. Elites draw the role mesh at `C.ELITE.DRAW_SCALE` with today's cube parts.
- **Files and systems:** `Enemies/DTEnemyRenderer.*`, the optic placement in `Enemies/DTEnemySubsystem.cpp`, `Tools/blender/enemy_<role>.py` (four files), `SourceArt/Enemies/`, `Content/DT/Meshes/SM_Enemy_*`, `Docs/02_Systems/enemy_art.md`, one line-up scene. The render path lands first inside the package; the four model scripts then touch one file each and can be four agents.
- **Acceptance:** `Dusktracer.Enemies.OpticPart` and `Dusktracer.Elites.Parts` pass unchanged. A line-up golden of the four roles at gameplay camera. A machine silhouette check on that frame: each role's hull mask, scale-normalised, overlaps every other role's by less than a stated share, at 36 and at 27 px/m. A bounds test: no hull hides more floor behind it than its cube does today, plus the allowance of §5 item 6 (rounds are opaque and depth-tested, and a tall hull hides them). The optic's contrast reading does not fall from today's 5.50 by more than the watch allows, and its lit pixel count does not fall. Perf at caps with `C.ENEMIES.CAP` hulls inside the allowance. The sim proof.
- **Must not change:** any hit radius, any `enemies.csv` row, separation, the optic's colour or emissive, the hull token, elite parts and lights, the Warden (it stays the cube).
- **Depends on:** P0; the call of §5 item 1.
- **Not verified by this check:** that a person names a role from its shape. `enemies.md` and `elites.md` keep that as a human check.

### C. Reclaimer silhouette

The hero stops being a cylinder.

- **Rows:** §2.1 r1 the armour set (XL, massing only); r2 hood and rebreather (M); r3 proportions and the silhouette tests (M); r4 hero height against the sim plane (S); r5 the rigid pose function in place of a rig (L, no `SK_` asset); r12 pauldrons as real geometry (M); r15 and r16 the dark-metal and undersuit tokens (S each).
- **How it stays inside the rules:** built from scratch in Blender (O-009's default); RogueHero's `base_*` parts and textures are never read. The armour stays `Pal.Hero.Armour`, the token the contrast gate reads; the two new tokens are added rows, and no existing value moves. The model stands on the floor while `C.SIM.PLANE_Z_UU`, the cue heights and the aim plane stay where they are: only the draw moves.
- **Files and systems:** `Player/DTHero.cpp` and `.h` (the visual part only), a new pose file under `Player/`, `Tools/blender/hero_*.py`, `SourceArt/Hero/`, `Content/DT/Meshes/SM_Hero_*`, `04_Presentation §3` (two rows), `Docs/02_Systems/hero_art.md`, one scene file.
- **Acceptance:** the gate's hero reading stays at or above `C.PRE.CONTRAST_MIN` on lit ground and on shadow, and the hero's armour-tone pixel count from above does not fall under today's (368 px in the 720p gate frame, D-036). The three silhouette tests of §2.1 r3 as machine checks on a top-down mask at 36 and 27 px/m. A test that the hit circle's centre projects inside the model's footprint at gameplay camera, so the body still marks where the hero is hit. Goldens for idle, moving and dashing poses. `Dusktracer.UX.CameraReach` and `Dusktracer.UX.CameraFixed` pass unchanged. The sim proof.
- **Must not change:** `C.HERO.RADIUS_UU`, the fire origin, dash, any input, `Pal.Hero.Armour`.
- **Depends on:** P0.
- **Held out of this package:** the Patron lamp, tabard, back rig, weapon mounts, conduit and docks (§2.1 r6 to r11, r13), textures and inlay (r14), the emissive layer (r17), wear (r18). Each waits on a call in the gap note's §6.3.

### D. Rounds and tracers

Player fire stops being spheres; enemy rounds gain their trail.

- **Rows:** §2.3 r1 the player round and Keeper trail (M): draw radius apart from hit radius, head stretch, tail fade, and the neutral trail R-PRE-01 asks for and nothing draws; §2.3 r3, r4, r10, r11 the tracer look on Pulse Rifle, Scatter Cannon, Ring Emitter and Spiral Projector (S each); §2.3 r21 the enemy round's trail and muzzle flash (M); §2.3 r24 the Flare Pylon's muzzle flash (S, the flash only); §2.5 r11 the muzzle token drawn (S); §2.3 r33 density LOD (M, the tracer half: emissive and trail width fall with the live player-round count, R-PRE-06); §2.5 r23 the tests R-PRE-01, R-PRE-03 and R-PRE-06 name as planned (M, the tracer and telegraph-coverage ones).
- **How it stays inside the rules:** R-PRE-01 as written: `Pal.Player.Core` on every head, the leading Keeper's token on the trail, no per-weapon hue. The enemy round keeps its dark core, its rim and `C.ENEMY_FIRE.DRAW_RADIUS_RATIO`; the trail sits behind it, with no halo and no ring, because the round passes with a thin margin and either would move it. Telegraphs are not touched. If additive tails over a telegraph pull its reading under the watch floor, the tail falls back to a non-additive blend inside this package.
- **Files and systems:** `Combat/DTBulletRenderer.*`, the trail parts in `Keepers/DTKeeperSubsystem.cpp`, `Enemies/DTEnemyBulletRenderer.*`, a new `M_Tracer` and the `M_EnemyRound` trail from editor Python, `Docs/02_Systems/tracer_art.md`, test files of its own, one scene file.
- **Acceptance:** the three planned tests live and passing. Contrast 7/7 with the round reading not under the watch floor, plus a staged frame of dense player fire crossing a live telegraph whose telegraph reading holds. A test that enemy-side emissive is the same at every density. Perf: the caps scene with trails on every round, inside the allowance; this is the package most likely to spend GPU, through overdraw. The sim proof.
- **Must not change:** any hit radius, speed or count; `C.BULLETS.EMISSIVE_PLAYER` as the undimmed level; the telegraph renderer; any token.
- **Depends on:** P0.

### E. Hit and kill feedback

A kill stops being a hull that vanishes.

- **Rows:** §2.3 r34 hit and kill layers (L): sparks on hit, a kill flash and debris, on a presentation subsystem fed once per sim step. Shared with D: §2.3 r33 (the kill half: above `C.PRE.DENSITY_LOD_KILLS_PER_S` per-kill effects aggregate), §2.5 r11 (the hit-on-hero flash in `Pal.Enemy.Muzzle`), §2.5 r23 (the kill half of the density test).
- **How it stays inside the rules:** the one sim-side edit of the wave: a read-only tap that copies the step's hits and kills to presentation before the resolve phase empties them. It writes nothing back and draws its randomness from a cosmetic stream. Sparks take `Pal.Player.Core`, since no weapon hue exists. The hero flash is drawn by this package's own actor and reads the Screen flashes setting (R-SET-09).
- **Files and systems:** a new `Presentation/` folder (impact subsystem and renderer), the tap in `Enemies/DTEnemyPool.cpp`, `Tools/blender/debris.py`, `Docs/02_Systems/impact_art.md`, tests of its own, one scene file.
- **Acceptance:** the sim proof runs first, on the tap alone, before any effect is drawn. A headless test that a staged step with a known number of hits and kills raises that many effects under the threshold, and a bounded, aggregated count above it. A test that enemy-side emissive is unchanged. Contrast 7/7 on a staged frame with kills in it. Goldens of a hit and of a kill on a held step. Perf in the S2 scene, which holds a kill rate, inside the allowance.
- **Must not change:** the order or content of any sim phase, any telemetry record, the camera (no shake), sim time (no hit-stop).
- **Depends on:** P0. No dependency on B: debris is generic at this grade.
- **Feel:** this is feel-changing work. By H03 §4 it is built and merged to `dev/s4` only; the owner plays it before it goes anywhere else.

### F. HUD pass

- **Rows:** §2.4 r1 the HP block and the product line (S); r2 the clock, banners and toast rows (S); r3 the XP bar, kept as a bar (S); r7 the Intercession ring (S); r8 dash pips (S); r9 the three tool counters (S); r10 the low-HP pulse, on the HP block only (S); r11 UI colour duties, with the boss bar's fill moved off the enemy token, a breach of R-PRE-08 that exists today (S); r30 one type table for every screen, on the engine font (S); r31 shared panel drawing (S); §2.7 r25 the boss name read from `ST_Enemies` (S).
- **How it stays inside the rules:** `UCanvas`, as the code and R-SET-18 have it (§5 item 7). Every string stays an FText from a table in both languages. No new HUD element kind is added past the caps of H04 §4: these are the existing readouts, re-laid.
- **Files and systems:** `Game/DTHUD.*`, `Progression/DTXPBar.*`, the bar fill in `Boss/DTBossSubsystem.cpp`, a new shared style file for the type table and panels, `ST_UI.csv` and `ST_Enemies.csv`, `Docs/02_Systems/hud.md`, one scene file.
- **Acceptance:** the layout is a pure function of viewport, culture and text scale, so a headless test walks 720p, 1080p, 1440p and 21:9, en and zh-Hans, text scale 100 and 150 percent, and finds no two rects overlapping and none off screen. A keep-out test: no HUD rect covers the ground circle R-UX-01 protects (the farthest attack start plus a Tank's diameter around the hero), which D-036 sized the frame for. A token test: no HUD style reads an enemy or telegraph token. HUD goldens in both languages with staged values. The `loc` gate step. Contrast 7/7 with the HUD drawn.
- **Must not change:** what any readout says or when; the offer screen, the hub, pause and settings; any binding.
- **Depends on:** P0; the call of §5 item 7.
- **Held out of this package:** the Favor panel, the tier and passed-over lines, weapon cells and their icons, damage numbers, HUD sigils (§2.4 r4 to r6, §2.3 r35, §2.2 r9): each needs a scope call or art that does not exist.

### G. Type (after F, and only on a call)

- **Rows:** §2.4 r24 Barlow Condensed (S); r25 Noto Sans SC (S); r29 the composite font and the bilingual rules (M); r33 the font licence record and shipping (S).
- **Why it is last and conditional:** fonts are OFL files from outside the repo, about 17 MB for these two families at two weights, on a public repo whose LFS quota is the owner's (D-006). The gap note read the Canvas font path from a header and did not run it. Every text width moves, so every HUD and screen golden and the hub's line counts are re-read.
- **Files and systems:** `Content/DT/Fonts/`, the style file F creates, `licenses.csv` rows with the OFL texts, `Tools/Package.ps1` staging.
- **Acceptance:** F's layout test passes again at every size, language and text scale; `catalog_lint.py` finds a row and a licence file for each font; smoke in both languages; the package check finds the fonts and their licence texts staged; a first step, before any font is added, proves on the hidden desktop that Canvas draws and measures a non-engine font.
- **Must not change:** any string; the zh rows waiting for a native reading stay marked as such.
- **Depends on:** F; the calls of §5 items 7 and 8.

## 4. Rows left for later, and why

The 159 rows outside wave 1, by the reason that holds them. A row with several reasons is listed under the first that would have to clear.

| Reason | Rows |
|---|---|
| Needs a D-036 reading (frame, screen-wide scope, spawn arrival, Warden marker) | §2.7 r10 to r23 the Warden (also needs the Breaker and Tallyman models and a size call); §2.3 r40 to r46 the Intercessions; §2.8 r3 conduits (under 2 px at either frame); §2.4 r16 card windows; the fine detail of every row opened to silhouette grade |
| Spends the contrast margin; waits for the wave 1 floor to be read | §2.8 r15 to r18 both weathers, the weather pick and contrast in both; §2.5 r6, r7, r17 sky, render settings, lighting; §2.8 r7 water, r9 skyline and fog; §2.8 r19 tiers; §2.3 r49, r50 gems and chests (2.7 and 2.8 on bright ground); the low-HP vignette of §2.4 r10 |
| Needs a human reading or a human check | §2.7 r1 to r9 elite parts and lights (two elites told apart is a human check); §2.3 r22, r23, r25 chevrons, the polar ring and the Hazard edge (they redraw telegraphs at a 3.27 margin); §2.3 r39 shake and hit-stop (feel, and the frame is fixed by R-UX-02); §2.3 r46 Time Dilation; §2.8 r20 perf on real 4070-class and 3060-class cards (D-011) |
| Needs a call that changes a rule or a spec | §2.6 r2, r3, r5, r6 and §2.5 r8, r14 the ceramic hull, guides, Ichor cell and hard-surface master (D-010, R-ENM-24); §2.7 r6 the Shielded dome and r24 the emissive budget (elite light 22 or 19.6); §2.3 r2 per-weapon hue and §2.5 r10 the player band; §2.3 r35 damage numbers; §2.3 r51 the Refit pickup (R-MOD-15); §2.4 r4 to r6, r20 Favor panel, tier line, weapon cells, a Legendary tier; everything R-VIG-15 bars: §2.2 r10, r11, r18 to r20, §2.4 r21 to r23; §2.8 r6, r8 rubble and buried machines (obstacle or dressing) |
| Needs what the book does not settle | §2.3 r19, r20 the ten Reforging rows with no plate; §2.3 r13 to r18 the six fusions the book draws and the game does not have; §2.3 r44 Closing and r52 the Quicksilver dash trail; §2.8 r10 the title backdrop; §2.1 r6 to r11, r13, r17, r18 lamp, tabard, rig, mounts, wear |
| Needs O-009 | None. The hero is built from scratch either way. O-009 only decides whether RogueHero's `base_*` parts may ever enter the repo |
| Needs audio decisions | §2.3 r36 to r38 the audio runtime, density layers and the 24 signatures; the heartbeat of §2.4 r10 |
| Needs real hulls or the per-step tap first (wave 1 builds those) | §2.3 r26 to r31 status layers; §2.3 r32 reaction effects; §2.3 r5 to r9, r12 the weapons past the tracer; §2.3 r53 effect counts (also a `telemetry.md` change) |
| Needs the sigil relief set, which needs a text ruling | §2.2 r1 to r9; §2.4 r14, r15, r17 to r19 card stone and anatomy; §2.8 r12 steles |
| Offline render work with no in-run surface yet | §2.2 r12 to r17, r21 the six shrines; §2.8 r11, r13, r14 colossi, ritual set, shrine sky |
| The rest of the foundations and the UI | §2.5 r1 to r3, r5, r9, r13, r16, r21, r22; §2.3 r47 the instancing and Niagara names; §2.4 r12, r13, r26 to r28, r32; §2.6 r1, r13, r14, r16; §2.1 r14; §2.8 r1 (done by D-036), r21 (settled by §5 item 3) |
| Not S4 | §2.6 r15 the Tallyman's data link (S5) |

## 5. Calls the scheduler makes before wave 1 starts

The first option of each is the recommendation.

1. **Hull colour in wave 1.** (A) Hulls stay `Pal.Enemy.Body`, as D-010 and R-ENM-24 have it; the ceramic question stays open. (B) Decide ceramic now and rewrite the rule. A keeps test (b) and costs a material swap later if the call goes the other way; B is a rule change before any human has seen a modelled hull.
2. **What lies past the plate.** (A) A stepped stone apron falling to shadow, non-colliding, wide enough for the 2000 uu frame, with no fog and no wall on the camera side. (B) The book's Keep wall and Breach. (C) Fog to hide the void. B has no plan on any board and hides floor on one side; C changes ambient light and so every rendered token.
3. **Goldens while art lands in steps.** (A) Each package re-baselines only the goldens of its own scenes in its merge commit, with before and after frames in its validation note; the exit's "re-baselined once" is read as one final re-baseline of the whole set after the art freeze. (B) Raise `C.SMOKE.GOLDEN_MEAN_DIFF_MAX` for the stage. (C) Hold all art on side branches and land it at once. Note under any option: the image check let a whole frame move pass at 1.08 of 12 (D-036), so a golden is a record, not a proof; the package checks above carry acceptance.
4. **The margin watch floor.** (A) A package stops and reports if telegraph or round on lit ground reads under 3.10; the gate still fails at `C.PRE.CONTRAST_MIN`. (B) The gate alone. (C) No package may lower either reading at all. A leaves a tenth for the weathers still to come; C would block the floor and the tracers on measurement noise.
5. **GPU headroom for wave 1.** Today at caps: about 3.4 to 4.0 ms proxied GPU p95 against `C.BUDGET.GPU_MS_4070` 12, game thread p95 about 2.5 against `C.BUDGET.GAME_THREAD_P95_MS` 10. (A) Wave 1 as a whole ends at or under 8.0 ms proxied GPU p95 and 5.0 ms game thread p95 at caps, with allowances over today of 1.2 ms for A, 0.8 for B, 0.1 for C, 1.2 for D, 0.5 for E and 0.1 for F. (B) First come, first served up to the budget. (C) A tighter 6.0 ms. A keeps a third of the budget for statuses, both weathers, Intercessions and the unknown in `C.PERF.GPU_PROXY_4080_TO_4070`, which stays a proxy until a real card is read.
6. **How tall a hull may stand.** (A) No hull hides more floor behind it than today's cube plus 25 percent. (B) Today's cube exactly. (C) No limit, read on the frame. The book's hulls are taller than the cubes and rounds are drawn opaque behind them.
7. **UI stack for wave 1.** (A) Stay on `UCanvas`, and make the stack call before any card or hub work. (B) Move to UMG and CommonUI first, as `07_Technical` line 13 says. A makes F cheap and possibly redone; B is an L row before one HUD pixel moves.
8. **Fonts now or later.** (A) Hold package G until the Canvas font path is proved and the owner's LFS headroom is known; F ships on the engine font. (B) Take G into wave 1 at once. Outside files on a public repo and the quota are the owner's ground (D-006).
9. **Which build the exits read.** (A) S1, S2 and S3 exit sessions use their own branch's package, never `dev/s4` (D-043 point 4). (B) Carry wave 1 down so validators see art. B changes what those exits measure and breaks D-043 point 3.
10. **The bar for wave 1.** (A) Silhouette grade as §1 defines it; "the reference bar" of the S4 exit stays undefined and is its own call before a second wave. (B) Define the reference bar first. The gap note's §7 says nothing defines it today.

## 6. What this plan does not know

- No row was re-read against the code at 9fafa8f. Files named above come from the gap note's Exists column (read at 5ccf82e) and a directory listing.
- Whether additive tracers fit the GPU allowance, and whether the Canvas font path works, are unknown until run.
- The shares in §3's last column are guesses; the gap note's sizes are for whole rows.
- The machine silhouette checks in B and C are proxies. They do not replace a person naming a role or finding the hero.
- No human days are sized anywhere.
