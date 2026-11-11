# S4 wave 1, package E: hit and kill feedback

For the scheduler. Scope: `Validation/S4_wave1.md` §3 "E" (D-043; `D-055 item 6`; D-056 ruling 5), second step. Branch `s4/e-feedback` from `s4/e0-tap` at 2e04621. Spec: `02_Systems/impact_art.md` R-IMP-03 to R-IMP-11. Every reading below was taken on this branch by a Tools script, with no window. Two grants beyond the brief's files, both `dev/s4` only: the row `Settings.Hint.Flash` of `Content/DT/Text/ST_UI.csv` and the last sentence of `settings.md` R-SET-09 (the Screen flashes row now says what Off does); and the `HeroHurt` cell of `07_Technical.md` §4. One more edit outside them: `Tests/Settings/DTSettingsTests.cpp`, T-SET-02's list of settings readers, gains `Presentation/DTImpactSubsystem.cpp` (R-SET-02: a new reader is added there on purpose). No person looked at a frame in motion; the agent looked at the held frames named in §5.

## 1. What changed

- **Class:** PRESENT. The rules are right; a hit and a kill could not be seen.
- **Layer:** presentation. `Presentation/DTImpactArt.*` (pure functions and the live effects), `Presentation/DTImpactSubsystem.*` (the reader, registered in `EDTSimPhase::Director`), `Presentation/DTImpactRenderer.*` (one instanced component each for sparks, flashes and hero rings, one per shard mesh for debris), `Debug/DTDebugImpact.cpp`. No sim file changed: the sim-side tap is step 0 (`S4_e0_tap.md`).
- **What it replaces:** nothing. A kill used to be a hull that vanished.

| Item | What it is | Proved by |
|---|---|---|
| The reader (R-IMP-03) | Fed once per sim step after `ResolveHits`: counted hits, kills, hurt heroes. Every effect is born on its step and ages in sim time; variation is a hash of serial, step and index | `Dusktracer.Impact.Feed`, `Dusktracer.Impact.Readers` |
| Under everything (R-IMP-04) | All four materials slide each vertex down its camera ray: debris to `C.IMP.DEBRIS_Z_UU`, the rest to `C.IMP.FLASH_Z_UU` | `Dusktracer.Impact.UnderTelegraphs`; the pixel test of §2 |
| Hit spark (R-IMP-05) | Three streaks in `Pal.Player.Core`, additive, grown and faded in the material | `Dusktracer.Impact.Shapes`; frame `impact_hit` |
| Kill flash (R-IMP-06) | A soft disc in `Pal.Player.Core`, additive, never wider than `C.IMP.FLASH_RADIUS_MAX_UU` | `Dusktracer.Impact.Shapes`; frame `impact_kill` |
| Debris (R-IMP-07) | Three shards (`Tools/blender/debris.py`: a wedge of 8 triangles, a bent plate of 24, a bar of 16), lit, `Pal.Enemy.Body`, no shadow | `Dusktracer.Impact.Shapes`; `Mesh.ps1 -Check`; frame `impact_kill` |
| Density (R-IMP-08) | `PerKill`, `Crowd`, `Flood` from the pool's kill rate, with exits under the entries; aggregated flashes on a grid; live caps | `Dusktracer.Impact.Density`, `Dusktracer.Pre.DensityLodKill`; frame `impact_dense` |
| Hero ring (R-IMP-09) | A ring in `Pal.Enemy.Muzzle` at a hurt hero's feet, following it; steady and reduced with Screen flashes off | `Dusktracer.Impact.HeroRing`; frame `impact_hero` |
| Batched, read-only (R-IMP-10) | Never an actor or a component per effect; nothing of the sim written | `Dusktracer.Impact.Feed`; §4 |
| The density reader (R-IMP-11) | `GetDensityTier()`, `GetKillsPerSecond()`, `GetDensityStep()` on `UDTImpactSubsystem`; `DTImpactArt::NextTier` | `Dusktracer.Impact.DensityReader` |

How the game thread is spared: a spark, flash or ring instance is written once, at birth (its transform and custom data: colour, strength, birth time, shape ratios), and its material draws its growth and fade from one parameter, `Now` (seconds of sim time), set once per component per frame while something in it is live. A death zeroes the instance's scale. A ring's centre follows its hero, so a live ring (at most one per player) is written each frame. **Debris is the exception:** its live pieces' transforms are written each frame from the closed form `DTImpactArt::PieceAt`, because the pieces are lit and turn, and a turning vertex in the material would keep its normal. At most `C.IMP.DEBRIS_LIVE_MAX` pieces, only in the `PerKill` tier. The perf readings of §3 are what that costs.

Debug: `DT.Impact.Stats [hold]` (the tier and rate, the peak rate since run start, the last step a hero lost health, live counts, hits and kills over the last second, refusals by cap); `dt.Impact.Draw 0|1`, starting from the environment variable `DT_IMPACT_DRAW`; `DT_IMPACT_STATS_S` prints the line on a timer. `Tools/ImpactArt.ps1` shoots the four scenes at 36 and 27 px/m and reads each held frame's stats line.

### First values and what they are in pixels

| Key | Value | At 36 px/m | At 27 px/m |
|---|---|---|---|
| `C.IMP.SPARK_LENGTH_UU` by `_WIDTH_UU` | 70 by 44 uu | 25 by 16 px | 19 by 12 px |
| `C.IMP.KILL_FLASH_*` on a Swarm drone (radius 32) | 0.8 to 1.7 radii | 18 to 39 px across | 14 to 29 px |
| the same on a Tank (58) | | 33 to 71 px | 25 to 53 px |
| `C.IMP.FLASH_RADIUS_MAX_UU` | 260 uu | 187 px across | 140 px |
| `C.IMP.DEBRIS_SIZE_UU` | 22 uu | 7.9 px | 5.9 px |
| `C.IMP.HERO_FLASH_*` ring | outer 52 to 96 uu, 20 uu thick | 37 to 69 px across, 7.2 px thick | 28 to 52 px, 5.4 px |
| pieces per kill | Swarm 3, Rusher and Turret 4, Tank 5, an elite one more or so | | |

The table of the 40 first values is in `Docs/constants.yaml` (block `impact_art`). The live caps follow from the per-step caps and the lives (32 sparks a step for 6 steps is 192; in `PerKill` at most 20 kills fall in the rate's window, each with at most 8 pieces, so 160). Nothing is under 5 px at 27 px/m.

**One first value moved.** `C.IMP.FLASH_Z_UU` 1.6 to 1.45: `Dusktracer.Impact.UnderTelegraphs` found 1.6 over the Danger outlines band, which `C.ENEMIES.TELEGRAPH_Z_UU` less `C.PRE.TELEGRAPH_LAYER_UU` puts at 1.5 (`settings.md` R-SET-10). With that setting on, a flash at 1.6 would cover the outer band of a telegraph (R-IMP-04). 1.45 is over debris (1.4) and player tails (1.25). Logged in `number_change_log.csv`.

## 2. Contrast, before and after

Before is the `s4/e0-tap` build (2e04621), after this branch (built at 5923438 and later; the effects' code unchanged since). `Tools\Smoke.ps1`, 1280x720, en, three runs each, all after an `arena_empty` shot of the same build. Readings vs lit ground; need `C.PRE.CONTRAST_MIN` 3.0, watch floor `C.PRE.CONTRAST_WATCH_MIN` 3.1.

| Scene, side, run | telegraph | enemy round | optic | tracer core | hero | gem | chest |
|---|---|---|---|---|---|---|---|
| contrast, before, 1 | 3.29 | 3.21 | 5.45 | 5.70 | 4.95 | 3.95 | 4.53 |
| contrast, before, 2 | 3.28 | 3.20 | 5.47 | 5.71 | 4.96 | 3.99 | 4.50 |
| contrast, before, 3 | 3.26 | 3.17 | 5.50 | 5.75 | 4.99 | 3.99 | 4.56 |
| contrast, after, 1 | 3.29 | 3.20 | 5.46 | 5.71 | 4.96 | 4.03 | 4.50 |
| contrast, after, 2 | 3.29 | 3.20 | 5.46 | 5.71 | 4.96 | 4.05 | 4.50 |
| contrast, after, 3 | 3.28 | 3.20 | 5.46 | 5.71 | 4.96 | 4.03 | 4.50 |
| tracer_crossing, before, 1 to 3 | 3.32, 3.32, 3.32 | 3.28, 3.26, 3.28 | | | | | |
| tracer_crossing, after, 1 to 3 | 3.32, 3.32, 3.32 | 3.29, 3.28, 3.29 | | | | | |
| impact_dense, effects off (`DT_IMPACT_DRAW=0`), 1 | 3.30 | 3.21 | 5.41 | 5.74 | 4.94 | 3.94 | 4.36 |
| impact_dense, effects off, 2 | 3.29 | 3.21 | 5.43 | 5.74 | 4.96 | 4.01 | 4.45 |
| impact_dense, effects off, 3 | 3.30 | 3.23 | 5.41 | 5.73 | 4.94 | 4.00 | 4.47 |
| impact_dense, after, 1 | 3.30 | 3.21 | 5.41 | 5.72 | 4.94 | 3.98 | 4.41 |
| impact_dense, after, 2 | 3.30 | 3.23 | 5.41 | 5.74 | 4.94 | 4.01 | 4.48 |
| impact_dense, after, 3 | 3.30 | 3.22 | 5.41 | 5.72 | 4.94 | 3.97 | 4.43 |

Every run: `CONTRAST: PASS` (7/7 on `contrast` and `impact_dense`, 2/2 on `tracer_crossing`) and `WATCH: OK` (bare lit ground Y p5 to p95 0.1113 to 0.1163). The telegraph and the enemy round never read under 3.17 on either side, so nothing came near 3.10. `impact_dense` did not exist on the `s4/e0-tap` build's tree run here; its before side is the same build with `dt.Impact.Draw` 0 (`DT_IMPACT_DRAW=0`), the same sim state without one effect.

The held `impact_dense` frame is in `Crowd` (rate 44 kills/s; 57 sparks and 5 flashes live; peak rate since run start 253, so the tier went through `Flood` and came back).

**The pixel test (R-IMP-04).** `impact_dense` with effects off, then on, same held step: every pixel the contrast tool classes as telegraph or enemy round in the off frame, compared in the on frame, more than 8 levels counted (`Saved\E_readings`, a stdlib script over `Tools/contrast_check.py classify`):

| Pair | telegraph pixels, changed, largest change | enemy round pixels, changed, largest change |
|---|---|---|
| off 1 against off 2 (rendering noise: TSR and the director) | 21577, 495, 40 | 9590, 1990, 42 |
| off 1 against on 1 | 21577, 188, 63 | 9590, 769, 110 |
| off 1 against on 2 | 21577, 400, 49 | 9590, 1759, 110 |

The counts are inside the noise; the largest changes are not. Nine pixels change by more than 60. Enlarged, the largest (110, at 806,491 of 1280x720) is the edge of a Swarm drone's optic, which the tool's hue class counts with the round's, beside a spark that lies on the floor behind the drone: the temporal upscaler blends the bright spark into the optic's edge pixels. No effect is drawn over a telegraph or a round; a bright effect next to one moves its edge pixels.

## 3. Perf, on and off

`Tools\Perf.ps1`, 1920x1080, RTX 4080 (proxy x1.6 to the 4070), effects on and off by `DT_IMPACT_DRAW` (Perf.ps1 passes no console variable), alternating so both sides meet the same load; `DT_IMPACT_STATS_S` 2 left the stats line in each log. "Busy" is the process list just before the run: other sessions were compiling and running editors through most of these, so every figure is a bound and the reading of record is the scheduler's, alone.

| Run | Effects | Game thread p50 / p95 ms | GPU p95 ms (proxied) | Busy before |
|---|---|---|---|---|
| S2, 1 | on | 3.05 / 4.83 | 3.69 (5.90) | none |
| S2, 1 | off | 3.24 / 5.92 | 3.67 (5.88) | two `cl` |
| S2, 2 | on | 2.85 / 4.31 | 3.68 (5.89) | none |
| S2, 2 | off | 3.54 / 6.16 | 3.70 (5.92) | none |
| S4, 1 | on | 3.66 / 6.22 | 3.77 (6.03) | two `cl` |
| S4, 1 | off | 3.37 / 5.00 | 3.73 (5.98) | one `cl` |
| S4, 2 | on | 3.38 / 5.02 | 3.74 (5.99) | an editor commandlet |
| S4, 2 | off | 3.46 / 5.17 | 3.78 (6.05) | an editor |
| S4, 3 (after the spark-cap change) | on | 3.47 / 5.26 | 3.76 (6.01) | an editor commandlet |
| S4, 3 | off | 4.37 / 7.09 | 8.28 (13.24) | two `cl`, an editor |
| S4, 4 | on | 3.56 / 5.25 | 3.74 (5.98) | an editor |
| S4, 4 | off | 4.64 / 8.16 | 3.71 (5.94) | an editor commandlet |
| Caps | on | 2.25 / 2.89 | 3.12 (4.99) | an editor |

Against the reading of record on fc5000b (S2 game p95 4.20, GPU p95 3.67; S4 4.76 to 4.78; Caps 2.45 to 2.53, GPU 4.82 to 4.83 proxied): the GPU moves by nothing measurable in S2 (on 3.68 and 3.69, off 3.67 and 3.70; the allowance is 0.5 ms proxied). The game thread is not readable here: in each pair but one the run with effects reads faster than the run without, by up to 1.9 ms, which is the load moving, not the package. The one pair the other way (S4 pass 1, +1.22 ms) had two compiles running during the effects run.

**The brief's stop line is crossed, and on both sides.** Every S4 run with effects reads over 4.95 ms game thread p95 (5.02 to 6.22), and so does every run without (5.00 to 8.16): today's machine sits over the reading of record whichever way the package is set. So the 0.17 ms the package may spend is neither shown spent nor shown free; only a quiet pair can say.

Load the effects saw (stats line at the end of each window): S2 about 32 to 38 thousand counted hits per second and 32 to 35 kills per second (`Crowd`; peak 138, so `Flood` was crossed in the run's first seconds), 84 live sparks (the `Crowd` cap of 12 a step for 7 steps), 4 to 7 flashes, no debris; S4 about 38.7 thousand hits and 44 kills per second, `Crowd`, 84 sparks, 5 flashes, no debris. Neither scene raises debris or a hero ring, so the per-frame debris writes are not exercised by the perf scenes. The first S2 and S4 runs counted spark refusals per hit (56 and 74 thousand); since 65fed66 a step stops trying once the live cap is full.

How the frame time is held down: a spark, flash or ring is written once; `Now` is one parameter write per component per frame; a frame with nothing live touches no component; storage is kept (no allocation in the steady state); debris, at most 160 pieces and only in `PerKill`, is the one per-frame transform write.

## 4. The sim proof

With the effects drawing (`dt.Impact.Draw` 1, the default), `Tools\BotRuns.ps1`, the five arms on seed 9107 one after another (`Saved\BotRuns\20261111_220039`), against the baseline of record, `Validation/S4_p0.md` §8 (the second table):

| Arm | `input_hash` | `steps` | `bot.csv` bytes | SHA-256 |
|---|---|---|---|---|
| None | `00e0d4ec` | 44137 | 535 | `b6a4878cb23adae7...` |
| Random | `51369747` | 59380 | 663 | `485ed85694ad5c92...` |
| Steered:Forge:Hammer | `e3293942` | 57749 | 679 | `b3cda2804c6a4664...` |
| Steered:Storm:Tempest | `b046559b` | 60231 | 680 | `cab7cae12d42f789...` |
| Steered:Tide:Frost | `def51848` | 48598 | 563 | `dc02a5d3f44c2d11...` |

Every value is the baseline's, arm for arm. Restart and meta: the gate's lines, §8.

## 5. Goldens and frames

New goldens, this package's own scenes: `Docs/Validation/goldens/impact_hit_en.png`, `impact_kill_en.png` (`Smoke.ps1 -Update`). No other golden was re-shot. `combat_staged_en` against its golden: mean difference 0.54 before (the `s4/e0-tap` build) and 0.50 after; the gate's smoke step prints every golden's.

`Tools\ImpactArt.ps1`: `IMPACT ART ACCEPTANCE: PASS (meshes, 8 frames, each holding its effect)`. The stats of each held frame are the same at both densities (the sim is the same):

| Scene | Held step | Tier, rate (peak) | Live sparks, flashes, pieces, rings |
|---|---|---|---|
| `impact_hit` | 359 | PerKill, 1 (1) | 2, 0, 0, 0 (21 hits in the last second) |
| `impact_kill` | 479 | PerKill, 10 (15) | 4, 2, 21, 0 |
| `impact_dense` | 719 | Crowd, 44 (253) | 57, 5, 0, 0 |
| `impact_hero` | 499 | PerKill, 1 (2) | 0, 0, 0, 1; the hero lost health on step 499 |

The frames, with 3x crops and before/after pairs of `combat_staged` and `tracer_crossing`, are in the Visual session's `Saved\S4Report\E\`. What the agent saw in them:
- **Kill flashes** read as flat pale discs, nearly the tone of a tracer head. At 36 px/m two flashes beside the hero in `impact_kill` look like two large rounds lying on the floor. They are never over a hull or a round.
- **Debris** is a scatter of small dark shards, readable as shards in the 3x crop, close to the floor's dark marks at the real size. Most pieces in the frame are a few pixels apart from their kill's flash.
- **Sparks** are thin pale streaks at a hull's edge, readable in the `impact_dense` crop; in `impact_hit` only two are live and they are lost among the player's own rounds at full frame.
- **The hero ring is mostly hidden on the step it is born.** Its outer radius then is 1.3 hero radii, most of it under the hero and the Rusher's band in `impact_hero`; a sliver shows. It grows to 2.4 radii over 0.2 s, so it shows later in its life, which no held frame here records.
- Aggregated flashes in `impact_dense` are soft grey discs on the floor between hulls; no telegraph, round or optic is covered.
- Nothing looked broken: no effect over a hull, a round, a telegraph or the hero, no effect floating off the floor, no effect left after its life.

## 6. What a person should look at

- Whether a spark reads as a hit landing, and whether a kill reads as a kill without shake or hit-stop.
- Whether the debris helps or is noise, and whether it is missed in a crowd, where it stops.
- Whether the hero ring is seen at all at play speed: it lies at the hero's feet, under the hero and any hull on it.
- Screen flashes off: whether the steady marks are right or should go.

## 7. Not verified

- **Perf of record.** Every reading in §3 was taken with other sessions' compiles and editors running, and the S4 stop line of the brief is crossed with and without effects there. The scheduler's reading of the tap and package E together, alone, is taken: §10.
- `combat_staged` and every other golden but this package's two: the gate's smoke step prints their mean differences against their goldens; `combat_staged_en` read 0.50 after against 0.54 before. None was re-shot.
- The hero ring later in its life: no held frame shows it grown. At its birth it is mostly under the hero.
- Whether the bounds fix (`SetBoundsScale` 2 on every effect component) holds every effect at the screen's edge: no frame was staged at the edge.
- Screen flashes off: tested in `Dusktracer.Impact.HeroRing` (the shapes and the instance data); no frame was shot with the setting off.
- The flash hint's fit is measured in `Dusktracer.Impact.HeroRing` from the menu font's width at the largest text size (both languages fit); no frame of the settings screen was shot.
- What only a person can judge (§6).
- Shipping and the packaged build: the gate's `shipping` step compiles it; `DT.Impact.*` and `dt.Impact.Draw` are not in Shipping, effects draw there at the default.

## 8. Second pass: what the eye gets at play size

The scheduler looked at every frame of the first pass and named three things that did not do their job at the game's size: the spark was a hairline, the hero ring was hidden when it was born, and debris was a few specks. This pass changes numbers and one shape. No rule's meaning, cap, tier or anything of the density reader Sound calls moved.

| Key | Before | After | Why (the frame) |
|---|---|---|---|
| `C.IMP.SPARK_STREAK_RATIO` (new; R-IMP-05 gains its sentence) | streaks a fixed 0.24 of the width at the root in the material | 0.3, a material parameter the renderer sets from the constant | one-pixel lines in the 3x crop; no spark could be found in `impact_hit_36` |
| `C.IMP.HERO_FLASH_START_RATIO`, `_END_RATIO` | 1.3 to 2.4 hero radii | 2.0 to 3.0 (outer radius 80 to 120 uu) | born under the hero's own figure and the Rusher's band; a sliver showed |
| `C.IMP.HERO_FLASH_THICK_UU` | 20 uu | 24 uu | a wider ring, a thicker band |
| `C.IMP.HERO_FLASH_S` | 0.20 s | 0.25 s | a wider ring shows a little longer |
| `C.IMP.DEBRIS_SIZE_UU` | 22 uu (7.9 px at 36 px/m) | 28 uu (10.1 px; 7.6 at 27) | dark specks on a floor of dark shadows |

These are this package's own presentation keys, first set without a frame; D-043's "no existing value changes" protects the sim's numbers, and no sim file reads any of them. Each has its row in `number_change_log.csv`.

- The spark's streak width is the material's `StreakRatio` parameter, set by `ADTImpactRenderer::InitRender` from the constant, so the next tuning needs no asset rebuild; `M_Fx_Spark` was regenerated once for the parameter (and `M_Fx_Ring` for the ring's new life, which the material holds). T-IMP-05 gains the root-width check on the shape function and on the renderer's material.
- Debris size is baked in the generator, so the three shards were rebuilt with `Tools/blender/Mesh.ps1` from the constant (now 28.0, 27.4 and 28.0 uu on their longest sides); `Mesh.ps1 -Check` passes.
- `impact_hit` now holds 28 live sparks on its held step (step 389, armoured Tanks, the four weapons at level 8, no kill in the window).
- `Tools/ImpactArt.ps1` shoots `impact_hero` again six steps after the hurt step (`impact_hero_late`), and each hero frame beside a twin with effects off; `Tools/impact_ring.py` counts the pixels in `Pal.Enemy.Muzzle`'s hue round the hero that the twin lacks.

| Frame | Ring pixels | Floor |
|---|---|---|
| `impact_hero_36` (hurt step 499) | 262 | 250 |
| `impact_hero_27` | 151 | 140 |
| `impact_hero_late_36` (step 505) | 913 | reported |
| `impact_hero_late_27` | 507 | reported |

The born frame passes its floor by 12 and 11 pixels: on the step it is born the ring is at its smallest and the Rusher's band covers its lower side. By six steps later it has grown and shows whole.

Contrast after this pass, three runs each (before: §2):

| Scene | telegraph | enemy round | tracer core |
|---|---|---|---|
| `contrast` | 3.29, 3.26, 3.26 | 3.21, 3.18, 3.18 | 5.70, 5.78, 5.79 (first pass 5.71 every run) |
| `tracer_crossing` | 3.32, 3.32, 3.32 | 3.28, 3.26, 3.29 | |
| `impact_dense` | 3.29, 3.30, 3.30 | 3.21, 3.22, 3.22 | 5.71, 5.73, 5.72 (first pass 5.72 to 5.74) |

7 of 7 on `contrast` and `impact_dense` every run, `WATCH: OK` every run, nothing under 3.18. The tracer core did not move: in these frames the sparks sit in the `Crowd` cap or next to hulls, too few pixels to shift its median. The pixel test again (`impact_dense`, effects off against on, more than 8 levels): telegraph 409 and 257 pixels changed, largest 43 and 37 (noise between two runs with effects off: 495, largest 40); enemy round 1233 and 1599, largest 110 and 111 (noise 1990, largest 42). The largest are the same nine-odd edge pixels of drone optics beside a spark as in §2.

Goldens: `impact_hit_en` and `impact_kill_en` re-shot. `combat_staged_en` reads 0.48 against its golden (0.54 before the package). The other goldens' mean differences are in the gate's smoke lines.

What the agent saw (`Saved\S4Report\E2\`, each crop beside its first-pass crop):
- **The ring** is now an orange ring round the hero's feet, outside the figure, on the step it is born; its lower part is under the Rusher's band, as it must be. Six steps later it is a full ring, the clearest effect of the package.
- **Debris** reads as small dark shards rather than specks in the 3x crop; at full frame it is still small.
- **Sparks** read as short bright splashes at a hull's edge in the 3x crop. At full frame in `impact_hit` they share the frame with the player's own pale rounds and tails at four weapons of level 8, and are hard to tell from them; a person should judge whether a spark reads as a hit.
- No effect over a hull, a round, a telegraph or the hero.

The perf of record is the scheduler's, taken alone (§10); this pass adds no work per frame (the same instance writes, one more parameter set once at start).

The whole gate run on 4f3fd5e (the first pass merged with `dev/s4` a23a931) read `GATE: PASS (4f3fd5e; 3 not run; mask FFFFF3FC)`, 581 tests of 581. It is information only: `dev/s4` then moved with engine code, and the gate of record is the one on the second pass, merged.

## 9. The gate of record

On 9781b94 (the second pass merged with `dev/s4` a155ace), `Tools\Gate.ps1 -Base dev/s4`, whole: `GATE: PASS (9781b94; 3 not run; mask FFFFF3FC)`. The three not run are `pad` (no package), `scope` (no brief in flight) and `mac` (D-007).
- `tests`: 581 run of 581 found, 0 failed, floor 581.
- `restart`: seed 9107, `input_hash` 276faf8e / 276faf8e, steps 5400 / 5400, csv same. `meta`: `input_hash` ba2a3ac8 / ba2a3ac8, steps 1200 / 1200, csv same.
- `contrast`: 7/7 on `contrast`, 7/7 on `impact_dense`, 2/2 on `tracer_crossing`; `WATCH: OK` on each (telegraph 3.29, 3.30, 3.32; enemy round 3.21, 3.21, 3.27).
- `audio`: `AUDIO: PASS` (no sound yet; the step's tool tests pass).
- Build, shipping and devgame: 0 warnings; nonunity 6 files, 0 failed.
- Goldens' mean differences against their goldens (smoke step): arena_empty 0.35 and 0.33, combat_staged 0.43 (en) and 0.53 (zh-Hans), edge_corner 0.30 and 0.29, edge_corner_wide 0.27, enemy_lineup 0.30, hero_lineup 0.32, hud_boss 0.30, hud_staged 0.33 and 0.36, tracer_weapons 0.38; this package's impact_hit 0.34 and impact_kill 0.31.

## 10. The perf reading of record (the scheduler's, alone)

**Conditions.** `dev/s4` at 3cb9534 (this package as landed, with the tap of `s4/e0-tap`), read in this worktree by the scheduler on 2026-11-12: the machine held alone (D-050 item 4; `ALONE: TAKEN` at 02:21:49 once the last running gate had ended); the nine runs were logged from 02:22:16 to 02:27:26, each time a run's end, and the hold was released as the last one ended. `Tools\Perf.ps1` at its defaults (Caps: `DT.Spawn.Keep 400,DT.Bullets 6000`, warmup 10 s), then `-Scene S4` and `-Scene S2` (warmup 20 s), 1800 frames each, three runs a scene. Every run rendered 1920x1080 and asked 1920x1080. Adapter, as the tool prints it: `GameViewer Virtual Display Adapter + OrayIddDriver Device + NVIDIA GeForce RTX 4080`. The tool prints no processor mask and no commit column; the commit is the one the reading names.

| Logged (the run's end) | Scene | Frame p50 / p95 ms | Game thread p50 / p95 ms | GPU p50 / p95 ms | Proxied GPU p95 ms | Load | Verdict |
|---|---|---|---|---|---|---|---|
| 02:22:16 | Caps | 3.81 / 4.21 | 2.10 / 2.55 | 3.05 / 3.11 | 4.98 | 5975 bullets / 388 enemies | PASS |
| 02:22:42 | Caps | 3.84 / 4.24 | 2.10 / 2.57 | 3.05 / 3.11 | 4.98 | 5975 / 388 | PASS |
| 02:23:09 | Caps | 3.85 / 4.28 | 2.09 / 2.56 | 3.05 / 3.11 | 4.97 | 5975 / 388 | PASS |
| 02:23:52 | S4 | 4.89 / 5.81 | 3.37 / 4.89 | 3.46 / 3.75 | 6.00 | 5818 / 400; 12 telegraphs, 5818 rounds trailed | PASS |
| 02:24:35 | S4 | 4.79 / 5.80 | 3.32 / 4.83 | 3.47 / 3.74 | 5.98 | 5817 / 400; 12 telegraphs, 5817 trailed | PASS |
| 02:25:18 | S4 | 4.79 / 5.75 | 3.30 / 4.80 | 3.47 / 3.74 | 5.99 | 5816 / 400; 12 telegraphs, 5816 trailed | PASS |
| 02:26:00 | S2 | 4.98 / 6.29 | 2.88 / 4.38 | 3.39 / 3.71 | 5.94 | 5806 / 400 | PASS |
| 02:26:43 | S2 | 5.01 / 6.30 | 2.89 / 4.35 | 3.39 / 3.70 | 5.92 | 5807 / 400 | PASS |
| 02:27:26 | S2 | 4.82 / 6.13 | 2.85 / 4.30 | 3.40 / 3.70 | 5.93 | 5809 / 399 | PASS |

```
PERF: PASS (1920x1080, game p50 2.10 / p95 2.55 ms, GPU p95 3.11 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)
PERF: PASS (1920x1080, game p50 2.10 / p95 2.57 ms, GPU p95 3.11 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)
PERF: PASS (1920x1080, game p50 2.09 / p95 2.56 ms, GPU p95 3.11 ms, load 5975.00 bullets / 388.00 enemies, 1800 frames)
PERF: PASS (1920x1080, game p50 3.37 / p95 4.89 ms, GPU p95 3.75 ms, load 5818.00 bullets / 400.00 enemies, 1800 frames)
PERF: PASS (1920x1080, game p50 3.32 / p95 4.83 ms, GPU p95 3.74 ms, load 5817.00 bullets / 400.00 enemies, 1800 frames)
PERF: PASS (1920x1080, game p50 3.30 / p95 4.80 ms, GPU p95 3.74 ms, load 5816.00 bullets / 400.00 enemies, 1800 frames)
PERF: PASS (1920x1080, game p50 2.88 / p95 4.38 ms, GPU p95 3.71 ms, load 5806.00 bullets / 400.00 enemies, 1800 frames)
PERF: PASS (1920x1080, game p50 2.89 / p95 4.35 ms, GPU p95 3.70 ms, load 5807.00 bullets / 400.00 enemies, 1800 frames)
PERF: PASS (1920x1080, game p50 2.85 / p95 4.30 ms, GPU p95 3.70 ms, load 5809.00 bullets / 399.00 enemies, 1800 frames)
```

**Before and after.** Before is `S4_perf_1080p.md` (`dev/s4` fc5000b, alone, the same tool and size, neither the tap nor package E). A move between two ranges is taken low end against low end and high end against high end; where a side has one run, that run stands against both ends of the other.

| Scene | Game thread p95, fc5000b | 3cb9534 | Move | Proxied GPU p95, fc5000b | 3cb9534 | Move |
|---|---|---|---|---|---|---|
| Caps | 2.45 to 2.53 | 2.55 to 2.57 | +0.10 at the low end, +0.04 at the high end | 4.82 to 4.83 | 4.97 to 4.98 | +0.15 |
| S4 scene | 4.76 to 4.78 | 4.80 to 4.89 | +0.04 to +0.11 | 5.94 to 5.95 | 5.98 to 6.00 | +0.04 to +0.05 |
| S2 scene | 4.20 (one run) | 4.30 to 4.38 | +0.10 to +0.18 | 5.87 (one run) | 5.92 to 5.94 | +0.05 to +0.07 |

**Where the game thread's move sits.** The tool prints profile rows (the `DT/GameThread/*` scopes of the CSV) for the S4 and S2 scenes; Caps prints none. Before is the raw output of the fc5000b reading (`E:\Dusktracer\.claude\worktrees\agent-aa7a88e9be0750f6a\Saved\Logs\perf_devs4_1080p.txt`, the scheduler's), after this reading's.

| Scope | S4 before | S4 after | S2 before (one run) | S2 after |
|---|---|---|---|---|
| `DT/GameThread/SimRender` mean | 0.58 | 0.60 to 0.62 | 0.45 | 0.47 to 0.49 |
| `DT/GameThread/SimRender` p95 | 0.63 | 0.66 to 0.76 | 0.50 | 0.54 to 0.57 |
| `DT/GameThread/SimTick` mean | 0.96 to 0.97 | 0.99 to 1.03 | 0.80 | 0.85 to 0.89 |
| `DT/GameThread/SimTick` p95 | 2.08 to 2.09 | 2.12 to 2.17 | 1.81 | 1.87 to 1.91 |
| `DT/GameThread/ImpactStep` p95 | (no such scope) | 0.00, 0.00, 0.00 | (no such scope) | 0.01, 0.00, 0.00 |

The package's own step scope reads nothing. The draw scope (where every renderer writes, the impact renderer among them) and the step (which now carries the tap) each move by a few hundredths. The code opens `SimRender` inside `SimTick` (`UDTSimSubsystem::Tick` opens both scopes in one function body, `SimRender` after the steps), so `SimTick` includes `SimRender`. A scope's p95 does not add up to the frame's p95, so this places the move and does not split it exactly.

**What it shows.**
- All nine runs are inside the budgets: game thread p50 at most 3.37 ms against `C.BUDGET.GAME_THREAD_P50_MS` 8, p95 at most 4.89 against `C.BUDGET.GAME_THREAD_P95_MS` 10, proxied GPU p95 at most 6.00 against `C.BUDGET.GPU_MS_4070` 12.
- Against package E's allowance of 0.5 ms of proxied GPU: the S2 scene, where the allowance is written, moves by 0.05 to 0.07; the S4 scene by 0.04 to 0.05; Caps by 0.15, the most of the three and still under a third of the allowance.
- Against the wave's ceiling of 5.0 ms game thread p95 on the S4 scene: 4.80 to 4.89, inside, with 0.11 left at the worst run, of which 0.05 is the audio's (D-056 ruling 6). The three S4 runs spread by 0.09, more than the low end of the move, so the game thread's move is read as 0.04 to 0.11, inside the spread at its low end. Wave 1 has no game thread left to spend after the audio.
- This reading is the tap (`s4/e0-tap`) and package E together against a tree that had neither; it does not split the two.

**What it does not show.**
- One GPU and a proxy factor (D-011): no 3060-class or Steam Deck reading (O-012).
- The editor's Development build, not the Shipping package.
- Three runs a scene, and the S2 scene's before is one run.
- No render-thread reading and no hitch measure.
- The perf scenes raise no debris and no hero ring (§3), so the one per-frame transform write of the package is not exercised.
- The scenes' kill rates sit in `Crowd`: the files count 41.1 to 42.2 kills a second in the S4 scene and 44.7 to 45.7 in the S2 scene, inside `Crowd`'s band, so `PerKill` and `Flood` are not measured at load.

**Where the files are.** `E:\Dusktracer\.claude\worktrees\agent-a76c15a416b3c6fea\Saved\Logs\cps_perf_e_feedback_3cb9534.txt` (one line per run) and `E:\Dusktracer\.claude\worktrees\agent-a76c15a416b3c6fea\Saved\Logs\cps_perf_e_feedback_3cb9534.raw.txt` (the whole output); the CSV captures are under that worktree's `Saved\Profiling\CSV\`.
