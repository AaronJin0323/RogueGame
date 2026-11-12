# S4 camera raised: D-080 picked onto `dev/s4`

`s4/camera-raised`, cut from `dev/s4` 7382f70 on 2026-11-13; `dev/s4` d8923c7 and 1618624 merged. The raised camera of D-080 (landed on `main` as 1a9aa55), picked as the camera's own commits; nothing of D-078, of `Tools/Gate.ps1` or of `Tools/gate_arms.csv`; `dev/s4` keeps wave 1's HUD.

## What was picked, and how

| From `main` | How |
|---|---|
| 3902aca: the two constants with their number-log rows, `ADTCameraPawn`, R-UX-01 and T-UX-01 in `06_UX` | cherry-pick, the number log's conflict kept both sides; the header regenerated on this tree; T-UX-01 rewritten to read wave 1's rows |
| 9d03a9c: `Tools/session_read.py` `frame()` and its pins in `Tools/tools_test.py` | cherry-pick; `frame()` on `dev/s4` was `main`'s before the change, so it applied as it is |
| 4c13514: `Docs/10_Tuning.md`'s off-frame paragraph and its Limits clause | by hunk: the paragraph's line replaced with `main`'s landed words, nothing else of the file |
| (brief item 6) `Tools/contrast_check.py`'s comment naming the 1500 frame | written here: the raised frame, 0.28 px per uu at the hero at 720 rows; comment only, no limit |

Not picked: `main`'s HUD commits, its goldens, the dated line in `S1_session_a_readiness.md`.

## Every reader of the camera's constants on this tree

| Reader | What it does now |
|---|---|
| `Game/DTCameraPawn` | `FollowTransform` = `FramingTransform(radius, shift)`; `FramingDistance` and `AimShift` are the one place the sums below read |
| `Game/DTGameMode.cpp` | spawns the camera at `FollowTransform` (unchanged) |
| `Game/DTHudLayout.cpp` (`FLens`, `ProjectGround`, `UnprojectGround`, `CoversKeepOut`, `KeepOutLeftEdge`) | the aim point shifted; a height above the floor; R-HUD-02's circle read on the drawn plane |
| `Debug/DTDebugArena.cpp` (`DT.Arena.Frame`) | stages through `FramingTransform`, centred unless a shift is given, so the 2000 uu edge frames stay as they were |
| `Tests/Game/DTCameraTests.cpp` | T-UX-01 and T-UX-02, below |
| `Tests/Game/DTHudTests.cpp` | T-HUD-02's projection checks: the hero on the middle column above the centre, the frame's centre at the aim point, the circle on the drawn plane |
| `Tests/Game/DTArenaArtTests.cpp` | T-ARN-02's frame corners: today's frame at its shift, the wide frame centred |
| `Tools/session_read.py` | `frame()` with the shift (Mechanics' tool, as on `main`) |
| `Tools/dtcamera.py` (new) | the Tools' one place for the camera's sum: the hero's actor on screen, above the centre since D-080 |
| `Tools/hero_zone.py`, `Tools/impact_ring.py` | the hero's place on screen from `dtcamera.project`, no longer the frame's centre |
| `Tools/enemy_art_check.py` | the line-up's camera aimed at the shift |
| `Tools/HeroZone.ps1`, `Tools/EnemyArt.ps1`, `Tools/ImpactArt.ps1`, `Tools/hero_outline.py` | size their frames from the radius to hold 36 and 27 px per metre (unchanged); see the readings below |
| `Tools/contrast_check.py` | the comment only |

## T-UX-01 on wave 1's HUD

Inputs: `DTHud::Layout` and `DTHud::BossRow` with the boss's real name in both cultures, at the default and the largest text size, at 1920x1080, 1280x720, 2560x1440 and 2560x1080. Only the boss bar crosses the hero's column; the name's line box, the corner blocks and every other fixed readout must stay 40 px off it; the banner and the toast are transient and not counted.

- The name's line, measured by the layout's own measure: 22 px at 1080 rows at the default size (33 at 150 %), 15 at 720, 30 at 1440. On a `dev/s4` frame (`hud_boss`, 7382f70) the name's ink is rows 15 to 29 and the bar's fill rows 17 to 28.
- The bar: y 17.0 to 29.0 at 1080 rows (lower edge y 29.0, `main`'s), the same at both text sizes.
- Reach on the drawn plane at 1258.2 / 140.6: 1416.01 up to y 29.0, 1416.05 down. Passes at every window and both text sizes.
- Not counted, and why: the name's line box (y 12 to 34 at the default size, 6.5 to 39.5 at 150 %), off the hero's column; the director's banner and the reaction toast, transient (D-080).

## The boss row and the text size (the scheduler's rule)

Before: `DTHud::BossRow`'s `S` was the canvas height over 1080 only, but the bar was centred on a row as tall as the name as drawn, so the text-size setting moved it: the bar's fill rows 17 to 28 at 100 % and 22 to 33 at 150 % (measured, en and zh-Hans). After (368a98a, 0d91bc8): the bar is placed by the name at the default size and its top is `BossBarTop` 17 by the window's scale; the name takes the text size, keeps its right edge at the gap, grows leftward and shrinks to the centre column if it would leave it.

| Frame (1920x1080) | Bar's fill rows | Name's ink rows, columns |
|---|---|---|
| en 100 %, before / after | 17-28 / 17-28 | 15-29, 623-723 / 15-29, 623-723 |
| en 150 %, before / after | 22-33 / 17-28 | 17-38, 598-748 / 12-32, 571-722 |
| zh-Hans 100 %, before / after | 17-28 / 17-28 | 15-30, 657-691 / 15-30, 657-691 |
| zh-Hans 150 %, before / after | 22-33 / 17-28 | 16-40, 648-700 / 11-34, 639-690 |

## R-HUD-02 at the raised frame (the Visual session's ruling)

- The circle is read on the drawn plane (`CoversKeepOut` through the camera's one place).
- The bottom-left block yields: a growing piece keeps to the room left of the circle. The dormant panels wrap their lines to that room and keep the asked text size: effective text size 125 % and 150 % (text factor 1.00) in both languages at 1920x1080 and 1280x720; the yield takes one more line at 150 % en (panel 2: 3 lines, x 36 to 271 at 1080). zh-Hans at 125 % and 150 % keeps 2 lines a panel.
- `C.HUD.TRANSIENT_FLOOR_UU` 1050: the banner and the toast stand under the boss row and no lower than this up the hero's column; the worst today is 1075.1 uu (2560x1440, en, 150 %, the toast).

## Goldens, shot once on 1618624 merged (b8948e5)

| Golden | Mean difference against the old one (of 12.0) | What moved |
|---|---|---|
| arena_empty en / zh-Hans | 1.22 / 1.22 | the hero higher and larger, the floor with the frame |
| combat_staged en / zh-Hans | 14.54 / 14.51 | the whole staged fight framed tighter: more of each hull, less of the outer ring |
| edge_corner en / zh-Hans | 5.17 / 5.22 | the plate's corner and the void at the raised frame |
| edge_xn, edge_xp, edge_yn, edge_yp en | 4.81, 6.82, 8.23, 2.23 | the plate's edge at the raised frame |
| edge_*_wide en (five) | 0.29 to 0.38 | nothing but noise: the wide frames are staged centred at 2000 as before |
| elite_lineup en | 14.14 | the line-up larger in the frame |
| enemy_lineup en | 3.67 | the line-up larger in the frame |
| hero_lineup en | 5.70 | the figures larger |
| hud_boss en | 7.02 | the Warden larger and nearer the bar; the boss row at 720 rows moves under a pixel (the bar's top 11.33 for 11.5) |
| hud_staged en / zh-Hans | 1.21 / 1.21 | the hero higher |
| impact_hit en, impact_kill en | 9.75, 5.33 | the staged hits larger |
| tracer_weapons en | 5.75 | the tracers larger |

## The sim proof, by hand

Seed 9107, the five arms one after another, read on b8948e5 and again on 46d129c (dev/s4 179d0c3 merged), both times: None `00e0d4ec` 44137 steps, Random `51369747` 59380, Steered:Forge:Hammer `e3293942` 57749, Steered:Storm:Tempest `b046559b` 60231, Steered:Tide:Frost `def51848` 48598; each `bot.csv` byte for byte the baseline of `S4_p0.md` §8 (SHA-256 `b6a4878cb23adae7`, `485ed85694ad5c92`, `b3cda2804c6a4664`, `cab7cae12d42f789`, `dc02a5d3f44c2d11`). Restart 276faf8e and meta a2a3ac8: the whole gate's lines on 46d129c.

## The whole gate

`GATE: PASS (46d129c; 3 not run; mask FFFFF3FC)` (pad: no package; scope; mac), on `dev/s4` 179d0c3 merged: 603 tests of 603 against the floor of 603, every golden at 0.13 to 0.40 against the goldens shot here, restart, seed, meta, vigil, first run, win, earned, contrast, settings and windows all PASS.

## Not verified

No person played the raised frame on this branch. The acceptance scripts' limits were not moved; where one no longer reads, it is listed in the hand-over for the Visual session to rule.
