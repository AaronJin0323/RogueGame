# S4 wave 1, package D: rounds and tracers

For the scheduler and the owner. Scope: `Validation/S4_wave1.md` §3 "D" (D-043); spec `02_Systems/tracer_art.md`. Branch `s4/d-tracers` from `dev/s4` at 55a504c. Presentation only: no sim number, no rule and no token value moved. Every launch went through a `Tools` script on the hidden desktop (R-TEC-17).

## 1. What changed

| Gap row | Before | Now | Rule |
|---|---|---|---|
| §2.3 r1, r3, r4, r10, r11 player round | a lit sphere at the hit radius | an unlit head in `Pal.Player.Core`, `C.TRC.HEAD_WIDTH_RATIO` of the hit radius wide and stretched `C.TRC.HEAD_STRETCH` along the flight (`M_Tracer`) | R-TRC-01 |
| §2.3 r1 Keeper trail | two stretched opaque spheres per round, only after the first blessing | one additive quad per round, bright at the head, fading and narrowing to a point; `C.TRC.TAIL_S` of flight long (`M_Tracer_Tail`) | R-TRC-02 |
| §2.3 r1 neutral trail | not drawn | `Pal.Player.TrailNeutral` until the first blessing of the primary Path (R-PRE-01) | R-TRC-03 |
| §2.3 r21 enemy round trail | none | a dark wedge of `Pal.Enemy.Bullet` behind the round, translucent and not additive, `C.TRC.ENEMY_TRAIL_S` of flight (`M_EnemyRound_Trail`); the round itself is untouched | R-TRC-04 |
| §2.3 r21, r24, §2.5 r11 muzzle flash | none; nothing drew `Pal.Enemy.Muzzle` | a pointed flash at the firing hull's edge for `C.TRC.MUZZLE_S` | R-TRC-07 |
| §2.3 r33 density, tracer half | `C.BULLETS.EMISSIVE_PLAYER` set once | head emissive, tail strength and tail width fall between `C.TRC.DIM_START_ROUNDS` and `C.TRC.DIM_FULL_ROUNDS` live player rounds, through one material parameter | R-TRC-06 |
| §2.5 r23 tests | planned | `Dusktracer.Pre.PlayerTracer`, `Dusktracer.Pre.TelegraphCoverage`, `Dusktracer.Pre.DensityLod` (tracer half) live, with three more under `Dusktracer.Tracer` | T-PRE-01, T-PRE-03, T-PRE-06 |

**How a tail is kept off rounds and telegraphs (R-TRC-05).** Additive tails over red were the known danger. The answer is depth, not blend: the two plane materials slide each vertex along its own camera ray down to `C.TRC.TAIL_Z_UU`, between the control cues and the lowest telegraph layer. The quad covers the pixels it would cover at the round's height, from any camera, but it lies under every telegraph layer, round, hull and hero, and the depth test hides it there. No tail pixel can land on a telegraph or a round, so the plan's fallback to a non-additive blend was not needed for the player tail. The enemy trail is not additive anyway, so overlapping trails do not add up to a glow.

**The enemy trail went through three settings.** The first, at the rim's hue with an emissive of 1 and a length over the burst gap, lowered no reading but joined rounds: the round finder of `Tools/contrast_check.py` read trail pixels next to a rim as rim, and two rounds of a burst became one blob with no dark centre (largest blob 327 to 343 px against 158 to 188 without trails; 69 rounds found against 72 to 74). The second was too faint to see. The third is darker and more opaque, and shorter than the gap between two rounds of a burst: the finder sees rounds as it did before (largest blob 158 px, §2).

Where this differs from the plan's text or the brief:
- The plan's acceptance says "the caps scene with trails on every round". The Caps scene's filler is the `Stress` source, which draws no tail, before and now (R-TRC-02): the scene with a tail on every round is `DT.PerfScene S4`. All three are read in §3, as the scheduler ruled.
- Files outside the plan's list, each the smallest edit that works: `Combat/DTBulletSubsystem.cpp` (the head's draw call lives there), one line in `Enemies/DTEnemySubsystem.cpp` (the call that draws trails), one read-only getter in `Enemies/DTEnemyBulletPool.h` (`GetOrigins`: the firing point the muzzle flash needs; the array existed for telemetry and nothing in the sim reads it), and the new `Source/Dusktracer/Presentation/DTTracerArt.*`.
- Two scenes, not one: `tracer_crossing` (gate step `contrast`) and `tracer_weapons` (a golden of the tracer look).
- `combat_staged` moved and was **not** re-baselined here: it is P0's golden, and packages B and C move it too. Its frames before and after are kept (§5).
- `C.GATE.TEST_FLOOR` rises from 524 to 530 for the six automation tests added.

## 2. Contrast, before and after

`Tools/Smoke.ps1 -Scene contrast` and `-Scene tracer_crossing`, three runs each side, 1280x720, en. Need `C.PRE.CONTRAST_MIN`; watch floor `C.PRE.CONTRAST_WATCH_MIN`. Before is `dev/s4` at 55a504c built in this worktree.

**The gate's frame** (`contrast`: a late minute of the director, 800 stress rounds, the Rifle): 7 of 7 on every run, `WATCH: OK` on every run.

| Element, vs lit ground | Before, three runs | After, three runs |
|---|---|---|
| telegraph (dark band 0.000) | 3.26, 3.26, 3.27 | 3.26, 3.26, 3.26 |
| enemy round (dark core 0.002; 43, 43, 44 rounds before, 44, 44, 44 after) | 3.18, 3.17, 3.17 | 3.18, 3.18, 3.18 |
| optic | 5.50, 5.50, 5.50 | 5.51, 5.51, 5.51 |
| tracer core | 5.95, 5.95, 5.95 | 5.79, 5.75, 5.75 |
| hero | 4.67, 4.67, 4.67 | 4.67, 4.67, 4.67 |
| gem | 4.05, 4.04, 4.00 | 4.06, 4.06, 4.01 |
| chest | 4.53, 4.53, 4.41 | 4.62, 4.54, 4.53 |

- Telegraph and enemy round did not move: both are inside the run-to-run noise P0 measured (0.01), and the round's reading did not fall.
- The tracer core fell by 0.16 to 0.20, on purpose: with about 830 live player rounds the head draws at about 0.85 of `C.BULLETS.EMISSIVE_PLAYER` (R-TRC-06), and its tone went from 0.928 to 0.895 to 0.901. Its pixels rose from 7257 to 7333 to 8556 to 8568: the stretched heads, and the Rifle's tails where they are pale enough to read as core. At the cap the head draws at `C.TRC.DIM_EMISSIVE_MIN`; that frame was not read by the contrast tool (§7).

**Inside the crossing** (`tracer_crossing`, new, gate step `contrast`, elements telegraph and enemy round): the hero stands at the frame's centre firing the Rifle, the Scatter Cannon, the Ring Emitter and the Spiral Projector at level 8 with no Keeper held (so the tails are the neutral trail); about ten Turret aim lines and several Rusher bands run through the fire, with about seventy enemy rounds, a Turret burst in flight and a slow ring. Sim step 300, no director, seed 0: the sim state is the same on every run and on both sides.

| Element, vs lit ground | Before, three runs | After, three runs |
|---|---|---|
| telegraph (dark band 0.000, about 18000 px; red 30487 to 30545 px before, 30098 to 30109 after) | 3.28, 3.28, 3.28 | 3.28, 3.28, 3.28 |
| enemy round (dark core 0.001) | 3.24, 3.25, 3.25 | 3.25, 3.26, 3.25 |
| rounds the finder accepts | 72, 73, 74 | 71, 71, 71 |

- Both readings hold inside dense fire, 0.15 or more over the watch floor. `WATCH: OK` on all six runs.
- The red's pixel count fell by about 1.3 percent. Tails cannot be the cause (they lie under the red); stretched heads are: a head is opaque, sits at the round's height and now covers about 1.3 times the area the sphere did. A head over a telegraph was there before and is the round itself, not its tail.
- One round fewer is accepted by the tool's round finder after than in the lowest run before. Two runs of the unchanged build differ by two (72 against 74): where two rounds overlap, a pixel decides whether the finder reads one blob with a dark centre. The one blob that differs between the first run of each side is such an overlapping pair, and it looks the same in both frames. The largest accepted blob is 157 to 159 px before and 158 px after, so no round is joined to its trail or to the next round. The count is reported because it leans the same way three times.
- The same frame half a second later (sim step 330, the burst just fired, aim lines gone), read once each way: with trails and tails 3.21 and 3.17 with 79 rounds; with the two plane materials taken out of the build 3.21 and 3.16 with 81 rounds. The lower telegraph figure is that frame's (only Rusher bands are left in it), not the art's.
- At the 2000 uu frame (`C.ARENA.CAMERA_FRAME_RADIUS_UU` set to 2000 for one build, then restored), read once: telegraph 3.23, enemy round 3.18, 70 rounds. The tool's pixel sizes are bound to the 1500 uu frame, so this is a look and not a judged reading; the tracers still show a head and a tail at 27 px/m.

## 3. Perf, before and after

> Replaced by `S4_perf_1080p.md` (2026-11-11): the readings below rendered 888x500 while recording 1920x1080, most on a busy machine; that note holds the reading of record at the real size, alone.

`Tools/Perf.ps1`, 1920x1080, RTX 4080, GPU p95 times `C.PERF.GPU_PROXY_4080_TO_4070`. Four other agents were building and launching on this machine throughout, so every line names what a process list and a CPU reading showed just before the run; `Saved/gpu.lock` sees none of it. A reading is a bound, and the scheduler's on the merged tree is the verdict (`S4_wave1.md` §2 rule 6).

The machine was never quiet for a whole pass, so the readings were taken two ways: alone, whenever a run could start, and **paired**: the module built from `dev/s4` at 55a504c and the module of 20c8fd3 swapped into the same build tree and read one after the other on the same scene, so both sides of a pair meet about the same outside load. A reading is void, and listed apart, when its GPU p95 is over one and a half times its own p50 or its p50 is over 1.2 times the scene's lowest: both are another process on the card.

| Scene | Side | Game thread p50 / p95 ms | GPU p50 / p95 ms | Proxied GPU p95 ms | What else ran |
|---|---|---|---|---|---|
| Caps | before, P0's two readings (`S4_p0.md` §7) | 2.07 / 2.70, 1.93 / 2.36 | 2.09, 2.08 (p95) | 3.34, 3.33 | nothing seen; one compile starting |
| Caps | before, alone | 1.94 / 2.40 | 2.06 / 2.09 | 3.34 | one other editor; CPU 59 % |
| Caps | before, paired | 2.04 / 2.50 | 2.22 / 2.52 | 4.03 | one other editor; CPU 30 % |
| Caps | before, paired | 2.00 / 2.42 | 2.06 / 2.09 | 3.34 | nothing seen; CPU 39 % |
| Caps | after, paired | 2.22 / 3.00 | 2.25 / 2.60 | 4.16 | two other editors; CPU 33 % |
| Caps | after, paired | 2.13 / 2.61 | 2.24 / 2.53 | 4.05 | nothing seen; CPU 23 % |
| Caps | after, alone | 2.05 / 2.57 | 2.06 / 2.10 | **3.35** | one other editor; CPU 7 % |
| Caps | after, alone | 2.09 / 2.59 | 2.23 / 2.53 | 4.04 | nothing seen; CPU 62 % |
| S2 | before, P0's quiet reading | 2.83 / 4.22 | 2.82 (p95) | 4.51 | nothing seen |
| S2 | before, alone | 2.95 / 4.52 | 2.55 / 2.82 | 4.52 | a compile, an editor, a commandlet; CPU 18 % |
| S2 | before, paired | 3.06 / 4.59 | 2.80 / 3.20 | 5.12 | one other editor; CPU 48 % |
| S2 | after, paired | 2.78 / 4.24 | 2.69 / 3.09 | 4.94 | one commandlet; CPU 40 % |
| S2 | after, paired | 3.18 / 5.20 | 2.69 / 3.10 | 4.96 | nothing seen; CPU 12 % |
| S2 | after, alone | 2.85 / 4.39 | 2.67 / 3.09 | **4.95** | nothing seen; CPU 34 % |
| S4 | before, P0's two quiet readings | 5.04 / 5.49, 4.97 / 5.42 | 4.54, 4.47 (p95) | 7.26, 7.16 | nothing seen |
| S4 | before, alone | 5.33 / 5.86 | 4.24 / 4.81 | 7.70 | four compiles; CPU 54 % |
| S4 | before, alone | 5.45 / 7.30 | 4.43 / 5.39 | 8.62 | one commandlet; CPU 15 % |
| S4 | before, paired | 5.39 / 6.28 | 4.00 / 4.66 | 7.46 | two commandlets; CPU 25 % |
| S4 | after, paired | 3.58 / 5.46 | 2.77 / 3.29 | 5.26 | two compiles, a commandlet; CPU 96 % |
| S4 | after, alone | 3.26 / 4.81 | 2.71 / 3.10 | **4.97** | one other editor; CPU 65 % |
| S4 | after, alone | 3.33 / 5.00 | 2.74 / 3.13 | **5.01** | one other editor; CPU 70 % |

Void, both sides, as proxied GPU p95: Caps before 14.87, 9.48 and 9.00, Caps after 9.56, 9.92 and 8.80; S2 before 10.12, 10.94 and 11.18, S2 after 10.48, 12.03 and 11.78; S4 before 12.83, S4 after 15.53. A first pass alone after the change, with three other editors running, read 9.5, 16.1 and 20.8 and is void whole. Two Caps runs with parts of the art taken out, meant to find what costs, were void too and show nothing.

- **Caps: no change that these readings can show.** The scene reads at one of two levels on both sides, about 2.06 or about 2.24 ms GPU p50, whichever module is in: 3.34 before and 3.35 after at the lower, 4.03 before and 4.04 to 4.16 after at the higher. Which level a run gets did not follow the module: the lower level was read with the old art and with the new, minutes apart. Against the allowance of 1.2 ms that is 0.0 at the lower level and 0.0 to 0.1 at the higher; were the two levels the art after all, the higher against the lower is 0.8. Game thread p50 2.0 to 2.2 ms on both sides.
- **S2: inside the allowance, direction not resolved.** After reads 4.94 to 4.96 on three runs; before reads 4.51 and 4.52 alone and 5.12 paired. That is between 0.2 lower and 0.45 higher. Game thread p95 4.2 to 5.2 after against 4.2 to 4.6 before: the spread of the after runs covers the before ones.
- **S4: lower on both threads.** Proxied GPU p95 4.97 to 5.26 against 7.16 to 8.62, about 2.2 ms lower against the quiet readings, and under the wave's ceiling of 8.0. Game thread p95 4.81 to 5.46 against 5.42 to 7.30, p50 3.3 to 3.6 against 5.0 to 5.5; within the pair, 5.46 against 6.28. It is not raised, and the two readings alone are at or under the wave's 5.0. Cause: a round's trail was two stretched spheres on a lit material and is now one quad of two triangles on an unlit one, so half the instances are written each frame; and at this count the tails draw fully dimmed and narrower.
- Trails are instanced: heads one component, tails one per pair of colours in use, enemy trails one, flashes one. No actor and no component per round (T-TRC-08).

None of this is a verdict (`S4_wave1.md` §2 rule 6): the paired runs bound the difference, and the scheduler's reading on the merged tree with nothing else running decides.

## 4. The sim proof

Commit 20c8fd3, the commit that holds every code and asset change of the package; the commit after it adds this note only.

- `& Tools\Gate.ps1 -Base dev/s4`: `GATE: PASS (20c8fd3; 3 not run)`. Not run: pad (no package), scope, mac. Tests: 537 run of 537 found, 0 failed, floor 530.
- Seed 9107, from the gate: restart `276faf8e`, meta `ba2a3ac8`. Both as recorded.
- `& Tools\BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107 -Seeds 1`, the arms one after another:

| Arm, seed 9107 | `input_hash` | `bot.csv` bytes | SHA-256, first 16 digits | Against `S4_p0.md` §8 |
|---|---|---|---|---|
| None | `7bd32faf` | 379 | `fa79587d4caca00c` | same |
| Random | `f5270196` | 662 | `0f7040940d7dee3e` | same |
| Steered:Forge:Hammer | `c6573c69` | 668 | `5a3568925a419583` | same |
| Steered:Storm:Tempest | `d7c8b43c` | 560 | `4452bfe6dbb65b3b` | same |
| Steered:Tide:Frost | `209a3446` | 418 | `9bd5ffcad19643d5` | same |

The fixed bot sample is byte-identical to P0's, and all seven hashes are the ones the earlier records hold. The one sim-side file touched is `Enemies/DTEnemyBulletPool.h`: a `const` getter over an array that already existed.

## 5. Goldens and frames

| Golden | Owner | Moved? | Done here |
|---|---|---|---|
| `tracer_weapons_en.png` | this package (new scene `Tools/Scenes/tracer_weapons.json`) | new | taken; a second run reads 0.33 against it (limit `C.SMOKE.GOLDEN_MEAN_DIFF_MAX`) |
| `combat_staged_en.png`, `combat_staged_zh-Hans.png` | P0 | yes: mean difference 0.94 and 0.91 against the golden, 0.41 and 0.38 before | **not re-baselined**: still inside the limit, and B and C move it first. To re-baseline at merge: `& Tools\Smoke.ps1 -Scene combat_staged -Culture en -Update`, and again with `zh-Hans` |
| `arena_empty_*`, `edge_corner_*` | P0 | no: 0.28 and 0.10 in en, as before | nothing |

`tracer_crossing` has no golden: it is judged by the contrast tool.

Frames kept in the worktree (git-ignored), 1280x720:

| Frame | Before (55a504c) | After |
|---|---|---|
| `combat_staged`, en | `Saved/S4Report/combat_staged_en_before.png` | `Saved/S4Report/combat_staged_en_after.png` |
| `combat_staged`, zh-Hans | `Saved/S4Report/combat_staged_zh-Hans_before.png` | `Saved/S4Report/combat_staged_zh-Hans_after.png` |
| `tracer_crossing`, en | `Saved/S4Report/tracer_crossing_en_before.png` | `Saved/S4Report/tracer_crossing_en_after.png` |
| `tracer_weapons`, en | none: the scene is new | `Saved/S4Report/tracer_weapons_en_after.png` |
| one overlapping pair of rounds in the crossing, ten times enlarged, before beside after | | `Saved/S4Report/crossing_detail_before_after.png` |
| a Turret burst just fired, with its muzzle flashes | | `Saved/S4Report/muzzle_flash_detail.png` |

## 6. What a person should look at

1. `tracer_weapons_en_after.png`: does a player round read as a tracer, a bright head with a short tail behind it? Are the tails too faint or too short at this frame? Can the four weapons be told apart by their fire, with one core colour and no per-weapon hue?
2. `tracer_crossing_en_before.png` beside `_after.png`: is the enemy round still the first thing found? Does the dark wedge behind a fast round help read which way it flies, or is it noise? No tail should show over red or over a round anywhere; `crossing_detail_before_after.png` shows a tail running under a round's rim.
3. `combat_staged`: stress rounds are now small ovals and dimmer (about 830 live rounds), and they carry no tail. Does the dimmed core still read as the player's?
4. `muzzle_flash_detail.png`: the flash is an orange tongue at the firing hull's edge for `C.TRC.MUZZLE_S`. Is it seen at all in play, and is orange next to a red aim line a problem?
5. In play, not in a frame: density dimming between `C.TRC.DIM_START_ROUNDS` and `C.TRC.DIM_FULL_ROUNDS` rounds, the tails in motion, and a second Keeper's tip colour.

## 7. Not verified

- No person looked at any frame. The agent viewed the frames named in §5 and crops of them.
- Nothing was seen in motion: every frame is a held sim step. Tails are translucent and write no velocity, so the temporal upscaler may smear them when they move; not looked at.
- The second Keeper's tip colour is proved on instance data (T-TRC-03) and shown in no frame: no scene stages a commitment. Only the neutral and the Storm tail were rendered.
- Fully dimmed player fire (the cap) was rendered only in the perf scenes; the contrast tool did not read a frame of it, so the tracer core at `C.TRC.DIM_EMISSIVE_MIN` is unread. The floor was chosen with room (an emissive of 2 on a near-white token), not measured.
- The camera-ray slide of R-TRC-05 was seen from the one gameplay camera, at two frame radii. Two local players with two cameras (`Tools/TwoWindowsCheck.ps1` is in the gate, but it reads no tail) and any other view were not looked at.
- The Warden's rounds: their trail and flash use the same code; no boss frame was rendered, and its hull hides its own flash (`tracer_art.md` Open).
- The muzzle flash's hue is close to the telegraph band the contrast tool reads as red. In the one frame with flashes the red's pixel count did not rise, so they read as orange there; a frame built to test that was not made.
- Perf: see §3. No reading here is a verdict.
- zh-Hans: the two new scenes show no text of their own and were shot in en only.
- `R-PRE-06` asks that enemy fire and telegraphs stay the brightest things in frame. A white core at half emissive is still brighter than a magenta rim in luminance; what the dimming buys is less white area and less glare, and whether that is enough is a human reading.

## 8. On the merged tree

`dev/s4` at 016bd45 (package A's slab plate and apron, package B's four hulls, package C's hero figure) merged into `s4/d-tracers`. §2 to §5 above are readings against 55a504c, on the flat floor with cube hulls and the cylinder hero; this section is the same art on the wave's floor, hulls and hero.

**The merge.** Four files conflicted, all shared: `Docs/constants.yaml`, `Docs/08_Constants.md` and `Docs/number_change_log.csv` keep every side's rows; `DTConstantsGen.h` was regenerated by `Tools/gen_constants.py`, not merged. `C.GATE.TEST_FLOOR` is `dev/s4`'s 533 plus this package's six tests, 539, with its change-log row. `Enemies/DTEnemySubsystem.cpp`, which B and this package both edit, merged by itself.

**Contrast**, three runs each, 1280x720, en. `WATCH: OK` on all six; nothing near 3.10.

| Frame | Element, vs lit ground | 55a504c with tracers (§2) | Merged tree, three runs |
|---|---|---|---|
| gate `contrast` | telegraph | 3.26, 3.26, 3.26 | 3.29, 3.29, 3.29 |
| gate `contrast` | enemy round (45 rounds) | 3.18, 3.18, 3.18 | 3.21, 3.21, 3.21 |
| gate `contrast` | optic | 5.51 | 5.45, 5.45, 5.45 |
| gate `contrast` | tracer core | 5.79, 5.75, 5.75 | 5.73, 5.70, 5.70 |
| gate `contrast` | hero | 4.67 | 4.95, 4.95, 4.95 |
| gate `contrast` | gem | 4.06, 4.06, 4.01 | 3.98, 3.97, 3.97 |
| gate `contrast` | chest | 4.62, 4.54, 4.53 | 4.50, 4.53, 4.53 |
| `tracer_crossing` | telegraph | 3.28, 3.28, 3.28 | 3.32, 3.32, 3.32 |
| `tracer_crossing` | enemy round | 3.25, 3.26, 3.25 | 3.28, 3.29, 3.28 |
| `tracer_crossing` | rounds the finder accepts | 71, 71, 71 | 71, 71, 71 |

- The slab floor's lit reference is brighter (bare floor Y p95 0.1163 against 0.1156), so the dual-tone elements, carried by their dark tone, read 0.03 to 0.04 higher, and the single bright tones read lower: the tracer core 5.70 to 5.73 against 5.75 to 5.79, the optic 5.45 against 5.51. The tracer core's tone itself did not move (0.897 to 0.902).
- Against the shadow the telegraph reads 3.93 to 3.94 in the gate frame (4.15 to 4.16 on the old floor): the floor's doing, not the tracers', and package A's to watch.
- One more frame, the crossing half a second later with a burst just fired, read once: telegraph 3.24, enemy round 3.20, 76 rounds.

**The muzzle flash against the new hulls.** Of the four roles only the Turret fires rounds; Swarm, Tank and Rusher fire none and have no flash, and the Warden keeps its cube. The Turret is now a disc on a mast over three feet. The flash still starts `C.ROLE.TURRET_RADIUS_UU` down the flight at floor height, which is now the foot of the hull under the disc's rim: in the burst frame it reads as coming out from under the disc, attached to the hull, neither floating clear of it nor hidden (it shows more than it did under the cube, whose skirt covered its first third). The offset was left as it is. `Saved/S4Report/muzzle_flash_detail_merged.png`.

**Goldens on the merged tree.** Every golden scene was judged against `dev/s4`'s goldens before anything was re-shot.

| Golden | Mean difference before the re-shoot (limit `C.SMOKE.GOLDEN_MEAN_DIFF_MAX`) | Done |
|---|---|---|
| `combat_staged_en`, `combat_staged_zh-Hans` | 1.21, 1.20 against `dev/s4`'s (the tracers) | re-shot |
| `tracer_weapons_en` | 1.63 against this package's own from the flat floor | re-shot |
| `enemy_lineup_en` | 0.63 | left |
| `arena_empty_en`, `_zh-Hans` | 0.32, 0.48 | left |
| `edge_corner_en`, `_zh-Hans`, `edge_corner_wide_en` | 0.42, 0.43, 0.34 | left |
| `edge_xn`, `edge_xp`, `edge_yn`, `edge_yp`, each plain and wide, en | 0.25 to 0.49 | left |
| `hero_lineup_en` | 0.28 | left |

None is near the limit, so none but this package's own three was re-shot. `tracer_crossing` has no golden; its frame on the merged tree is `Saved/S4Report/tracer_crossing_en_merged.png`, with `combat_staged_en_merged.png`, `combat_staged_zh-Hans_merged.png` and `tracer_weapons_en_merged.png` beside it.

**Gate and sim proof on the merge:** `& Tools\Gate.ps1 -Base dev/s4` on the merge commit: `GATE: PASS (62260d9; 3 not run)` (pad, scope, mac). Tests 546 run of 546 found, 0 failed, floor 539. Restart `276faf8e`, meta `ba2a3ac8`, both as recorded. The gate's own contrast step read telegraph 3.30 and enemy round 3.21 on its frame and 3.32 and 3.29 on `tracer_crossing`, `WATCH: OK` on both.

**A fault in the gate's `tools` step, not this package's.** Six runs of the gate stopped at `tools` with no verdict line before the seventh went through. `Tools/tools_test.py` dies in `HeroArt`, inside `Tools/hero_outline.py` (package C's, unchanged here): Python exits with an access violation (`-1073741819`) or raises a `TypeError` naming a `range_iterator` in plain arithmetic, at `draw`, a different error from run to run. `python -I Tools\hero_outline.py` alone does the same about one run in three, and so does a copy of `dev/s4`'s own `Tools` at 016bd45 exported beside this tree, so the merge is not the cause. Both Python 3.14.4 installs on this machine do it. Whether it is the interpreter or this machine under load was not settled; the step passes when it does not fault (277 tests), and it is a hazard for every gate on `dev/s4` until someone looks.

**Not verified on the merged tree:** perf was not read again (§3 is against 55a504c; B's hulls and A's floor change every scene's cost, and the scheduler's reading on the merged tree is the verdict); the five bot arms were not rerun (the gate's restart and meta hashes are the proof here); no person looked at a merged frame.
