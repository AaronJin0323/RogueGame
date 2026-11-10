# S4 wave 1, package F: the HUD pass

For the scheduler. Scope: `Validation/S4_wave1.md` §3 "F" (D-043). Branch `s4/f-hud` from `dev/s4` at 55a504c. Spec: `02_Systems/hud.md`. Every command below is typed in PowerShell at the repo root and opens nothing (R-TEC-17). Frames named here are kept under `Saved/S4Report/` of the package's worktree, not in the repo: only goldens have a licence row.

## 1. What landed

| Gap row | What it is | Proved by |
|---|---|---|
| §2.4 r1 HP block, product line | `Hud.Hp` and a 360 x 8 bar in `Pal.UI.Text`, top left. The product line is not drawn in a run; the developer lines stay under the block outside Shipping. No armour pips: no such stat | T-HUD-04, T-HUD-05 |
| §2.4 r2 clock, banners, toast | Clock top right with the level; boss row, banner and toast are three rows of the top centre, so none shares a row with the XP bar any more | T-HUD-01, T-HUD-04 |
| §2.4 r3 XP bar, kept as a bar | 360 x 10 in `Pal.Pickup.Gem` under the clock, with the XP readout, the level-up line and the pending count under it | T-HUD-04, T-PRG-11 unchanged |
| §2.4 r7 Intercession ring | 84 px ring, clockwise from the top, in the Patron's Keeper token; full, an inner ring in `Pal.Player.Core` and the label `Hud.IntercessionReady` | T-HUD-06 |
| §2.4 r8 dash pips | 34 x 6 bars on the row of `Hud.Dash`: ready in bone, the one coming back filling from the left at 60 % | T-HUD-06 |
| §2.4 r9 tool counters | One panel, three lines from `FDTPlayerOfferState`: Renegotiate, Refuse, Hold with the uses left. One new row, `Hud.Tool` | T-HUD-07 |
| §2.4 r10 low-HP pulse | On the HP block only: at or under `C.HUD.LOW_HP_FRAC` the bar is framed and its fill pulses on sim time; with Screen flashes off the frame alone. No vignette, no heartbeat | T-HUD-05 |
| §2.4 r11 colour duties | `Pal.UI.Boss` (new, #D9B36A) fills the boss bar in place of `Pal.Enemy.Optic`; the duties are one table in `Game/DTUIStyle`; no file that draws the HUD names an enemy, elite or telegraph token | T-HUD-10 |
| §2.4 r30 type table | `DTUI::TypeStep`: Display 2.6, Title 1.8, Body 1.5, Note 1.0 on the engine font at 1080 lines, times the canvas height, times the text size. The HUD, the XP bar and the boss bar use it | T-HUD-08 |
| §2.4 r31 panel drawing | `FDTUIPainter`: tile, frame, bar, arc, and a panel in four states. The HUD uses default (tools) and solid (dormant line) | T-HUD-09 |
| §2.7 r25 boss name from `ST_Enemies` | Not built here: it landed on `dev/s3` and reaches `dev/s4` by merge. The lookup line in `UDTBossSubsystem::DrawBossBar` is untouched, and a trial three-way merge of that file and of `ST_UI.csv` against `dev/s3` is clean | `git merge-file`, by hand |

The layout is one pure function, `DTHud::Layout(viewport, text size, text)`; `ADTHUD`, `FDTXPBar::Draw` and `DrawBossBar` draw into its rects.

## 2. Where this differs from the plan's text, the brief or the plate

- **Places follow two fences, not PL.20.** The keep-out circle of R-UX-01 (1416 uu) reaches to 3 % of the frame height from the bottom edge at the centre and to 14 % from the top. So the ring is bottom left, not bottom centre, and the boss bar is top centre, not bottom centre, where it covered the circle before this package (T-HUD-02 shows it). The clock is top right, not top centre: boss row, banner and toast must all fit above the circle at the largest text. The tool counters are bottom left, not bottom right: the bottom right is outside `HUD_MASKS` of `Tools/contrast_check.py`, which a package may not edit.
- **r30 and r31 bind the HUD's files only.** The offer screen, the hub and the menus keep their own painters and multipliers: they are outside the package's files and under "must not change".
- **The style and layout headers are in `Game/`,** not `Presentation/`: `Boss/DTBossSubsystem.cpp` and `Progression/DTXPBar.cpp` already draw, and a header under `Presentation/` may not be included from there (`PresentationReadsOnly`).
- **The plan walks four windows; the test walks five:** 1280x720, 1920x1080, 2560x1440, 2560x1080 and 3440x1440, at 100, 125 and 150 % text.
- **The boss row's name is still drawn by the boss subsystem,** which alone reads its row. The HUD's layout needs only the row's height, and takes it from a stand-in line of the same step and culture.
- **Text now follows the window's height.** Body text was the engine font at scale 1.0 at every window. It is still 1.0 at 720 lines and is 1.5 at 1080, so the smoke frames show readouts the size they were and the target window shows them half as large again; the developer lines are smaller at 720 lines (scale 0.67). T-HUD-08 holds a line of Body text at 720 lines at or above `C.HUD.TEXT_MIN_PX` 12 px in Latin and in Han; the measured height is not printed by a passing test and is not quoted here.
- No key glyph beside the ring and no phase line under the clock: each needs a new string or a `director.md` change.

## 3. Readings

`& Tools\RunTests.ps1 -Filter Dusktracer.Hud`: 10 of 10. The walk is 5 windows x 2 cultures x 3 text sizes, 30 cases, each with every readout up at once (four Keeper meters, the aim line, three dormant lines with the widest names, the boss row, a banner, the widest toast).

| Check | 1280x720 | 1920x1080 | 2560x1440 | 2560x1080 | 3440x1440 |
|---|---|---|---|---|---|
| T-HUD-01 no overlap, none off screen, en and zh-Hans, 100 / 125 / 150 % | pass | pass | pass | pass | pass |
| T-HUD-02 no rect on the keep-out circle, same walk | pass | pass | pass | pass | pass |
| T-HUD-01 every reaction's toast unshrunk at 150 % | pass | pass | pass | pass | pass |

T-HUD-03, the readouts every run shows under the contrast tool's masks at 1280x720 and 100 %: pass in en and in zh-Hans.

Contrast, gate scene `contrast` (1280x720, en), need `C.PRE.CONTRAST_MIN` 3.0, watch floor 3.1:

| Element | Before, 55a504c | After |
|---|---|---|
| telegraph vs lit ground | 3.26 | 3.26 |
| enemy round vs lit ground | 3.17 | 3.17 |
| optic | 5.50 | 5.50 |
| tracer core | 5.91 | 5.95 |
| hero (pixels) | 4.68 (368) | 4.68 (364) |
| gem | 4.01 | 4.06 |
| chest | 4.53 | 4.53 |
| dormant line on its panel | not read before | 8.68 |

`WATCH: OK` both times; bare lit ground Y p5 to p95 0.1113 to 0.1156 both times. Every difference is inside the scene's run-to-run noise (`S4_p0.md` §6).

Perf, `& Tools\Perf.ps1` (Caps, 1920x1080, RTX 4080, GPU p95 x 1.6), allowance + 0.1 ms proxied over today:

| Build | Game thread p50 / p95 ms | GPU p95 ms | Proxied | What else ran |
|---|---|---|---|---|
| 55a504c | 2.77 / 3.63 | 13.44 | 21.51 | two other `UnrealEditor`: void |
| 55a504c | 2.18 / 2.75 | 2.53 | 4.05 | two `cl`, three `UnrealEditor` at the start, CPU 85 %: void |
| 55a504c | 2.24 / 3.00 | 6.32 | 10.11 | one other `UnrealEditor`: void |
| f942341 | 2.05 / 2.50 | 2.09 | **3.34** | one `UnrealEditor-Cmd`, CPU 4 % |
| f942341 | 2.08 / 2.57 | 2.09 | **3.34** | one other `UnrealEditor`, CPU 67 % |
| f942341 | 2.07 / 2.57 | 2.08 | **3.33** | one `UnrealEditor-Cmd`, CPU 13 % |

No "before" of this package's own was taken on a quiet machine: other agents rendered through all three. The three "after" readings equal P0's two quiet readings of the same sim and render code (`S4_p0.md` §7: 3.33 and 3.34 ms proxied, game thread p95 2.36 and 2.70), so the HUD pass reads as no change against the allowance of 0.1 ms. That is a bound; the scheduler's reading on the merged tree is the verdict.

Sim proof on f942341: `RESTART: PASS` `276faf8e`, `META: PASS` `ba2a3ac8`; the five arms, run one after another:

| Arm, seed 9107 | `input_hash` | `bot.csv` bytes | SHA-256 | Same as `S4_p0.md` §8 |
|---|---|---|---|---|
| None | `7bd32faf` | 379 | `fa79587d4caca00c` | yes |
| Random | `f5270196` | 662 | `0f7040940d7dee3e` | yes |
| Steered:Forge:Hammer | `c6573c69` | 668 | `5a3568925a419583` | yes |
| Steered:Storm:Tempest | `d7c8b43c` | 560 | `4452bfe6dbb65b3b` | yes |
| Steered:Tide:Frost | `209a3446` | 418 | `9bd5ffcad19643d5` | yes |

Gate: `GATE: PASS (f942341; 3 not run)` (pad: no package; scope; mac), 541 automation tests of 541 against the floor of 534, 258 tool tests.

## 4. Goldens

New, this package's own, taken with `-Update` and judged again by the gate:

| Golden | Second run against it (limit 12.0) |
|---|---|
| `hud_staged_en.png` | 0.27 |
| `hud_staged_zh-Hans.png` | 0.27 |
| `hud_boss_en.png` | 0.25 |

Not re-baselined, P0's scenes, which every package moves (`S4_wave1.md` §5 item 3): the HUD moved in each, and each still passes the image check, which is why a golden is a record and not a proof.

| Golden | Before this package | After |
|---|---|---|
| `arena_empty_en.png` | 0.28 | 2.33 |
| `arena_empty_zh-Hans.png` | not read | 2.18 |
| `combat_staged_en.png` | 0.38 | 2.52 |
| `combat_staged_zh-Hans.png` | 0.41 | 2.34 |
| `edge_corner_en.png` | not read | 2.11 |
| `edge_corner_zh-Hans.png` | not read | 1.97 |

They are to be re-baselined once, at the merge that brings packages A to D under this one.

## 5. For the merge

- `dev/s4` moved to f5cf5fc (packages A and B) while this package was read. A trial merge (`git merge-tree`) conflicts in five shared files, all of the kind `S4_wave1.md` §2 rule 9 expects: `Docs/constants.yaml`, `Docs/08_Constants.md` and `Docs/number_change_log.csv` (keep both sides' rows; `C.GATE.TEST_FLOOR` becomes the merged tree's floor plus this package's ten tests), `Source/Dusktracer/Core/DTConstantsGen.h` (regenerate with `python -I Tools\gen_constants.py`), `Tools/tools_test.py` (keep both classes).
- After the `dev/s3` merge: `Hud.BossName` leaves `ST_UI`. No code of this package reads it; T-HUD tests look the name up in `ST_Enemies` first.
- Stale lines outside this package's files, left alone: `combat_core.md` Open still says the Intercession meter is a bar until the HUD pass; `boss.md` and the `Hud.BossName` context still say bottom centre; the `Hud.Hp` and `Level.*` context columns describe the old places.

## 6. On the merged tree

`dev/s4` at f0d9d17 (packages A, B, C and D) merged into `s4/f-hud` as 6edbf76. Conflicts were the five files of §5 and nothing else: both sides' rows kept, `DTConstantsGen.h` regenerated, every test class kept, `C.GATE.TEST_FLOOR` 539 to 549. §5's stale lines were brought up to date where they are plain description: the `Level.*` context column of `ST_UI` and `combat_core.md`'s note on the Intercession meter. `boss.md` names no place for the bar, so it is unchanged; the `Hud.BossName` context is left alone, because the `dev/s3` merge removes that row and an edit here would conflict with it.

**Gate.** `GATE: PASS (6edbf76; 4 not run)`: pad, scope, mac, and `tools`, skipped with `-Skip tools`. Run whole, the gate stopped at `tools` with no verdict line 15 times of 15: `Tools/tools_test.py` dies of an access violation inside `Tools/enemy_art_check.py` (the rasteriser's inner loop, lines 107 and 115) or `Tools/hero_outline.py` (line 186, `draw`), packages B's and C's pure-Python readers, on Python 3.14.4 with the machine idle (CPU 3 %). Run alone, the `EnemyArt` class finished 1 time in 3 and `HeroArt` 6 times in 34; every other class, 262 tests, passed in one process. The whole suite was then rerun until one process finished: the sixth gave `TOOLS: PASS (279 tests)`, on the re-shot goldens, and that run is the verdict. Of the six `HeroArt` runs that finished alone, two were read and passed, one printed FAILED without its lines being kept, and three were counted and not read. The fault is at the same source lines each time and does not follow machine load here, so it reads as the interpreter and not as this package; it was not looked into further.

Other gate lines: 556 automation tests of 556 against the floor of 549; `RESTART: PASS` `276faf8e`; `META: PASS` `ba2a3ac8`; `Dusktracer.Hud` 10 of 10 (the overlap and keep-out walks of §3, unchanged); dormant line 9.16 on its panel; `tracer_crossing` 2 of 2.

**Contrast, three readings** (1280x720, en, need 3.0, watch floor 3.1), all 7/7 and `WATCH: OK`:

| Element vs lit ground | Read 1 | Read 2 | Read 3 | The gate's own |
|---|---|---|---|---|
| telegraph | 3.29 | 3.29 | 3.29 | 3.29 |
| enemy round | 3.21 | 3.20 | 3.21 | 3.21 |
| optic | 5.45 | 5.45 | 5.45 | |
| tracer core | 5.73 | 5.73 | 5.73 | |
| hero (pixels) | 4.95 (390) | 4.95 (390) | 4.95 (390) | |
| gem | 4.01 | 3.99 | 4.00 | |
| chest | 4.59 | 4.50 | 4.59 | |

Bare lit ground Y p5 to p95 0.1113 to 0.1163 each time (window 0.106 to 0.16).

**The wave's one re-baseline.** Every golden was re-shot with `Tools\Smoke.ps1 -Scene <name> -Culture <c> -Update` on the merged tree. "Moved" is the new golden against the frame `dev/s4` held at f0d9d17 (for the three `hud_` frames, against this package's own golden of f942341), by `Tools/image_check.py`; "second run" is the gate's shot against the new golden. Limit 12.0.

| Golden | Moved | Second run |
|---|---|---|
| `arena_empty_en` | 2.20 | 0.31 |
| `arena_empty_zh-Hans` | 2.41 | 0.29 |
| `combat_staged_en` | 2.52 | 0.38 |
| `combat_staged_zh-Hans` | 2.39 | 0.27 |
| `edge_corner_en` | 2.07 | 0.24 |
| `edge_corner_zh-Hans` | 2.10 | 0.20 |
| `edge_corner_wide_en` | 2.13 | 0.25 |
| `enemy_lineup_en` | 2.68 | 0.26 |
| `hero_lineup_en` | 2.37 | 0.28 |
| `tracer_weapons_en` | 2.41 | 0.32 |
| `hud_staged_en` | 0.61 | 0.24 |
| `hud_staged_zh-Hans` | 0.64 | 0.11 |
| `hud_boss_en` | 0.61 | 0.25 |
| `edge_xn_en`, `edge_xn_wide_en` | 2.45, 2.44 | not in the gate |
| `edge_xp_en`, `edge_xp_wide_en` | 2.29, 2.07 | not in the gate |
| `edge_yn_en`, `edge_yn_wide_en` | 2.43, 2.38 | not in the gate |
| `edge_yp_en`, `edge_yp_wide_en` | 2.35, 2.23 | not in the gate |

The eight edge frames of package A are read by no gate step; they were re-shot too, so that no committed golden shows the old HUD. The first shot of `edge_yn_en` died at engine start (an access violation in `CoreUObject` before the run began); the second shot is the golden.

**The hero's armour in the new `arena_empty_en`** (package C's T-HAR-06, floor `C.HERO_ART.ARMOUR_PX_MIN` 368): 392 px at tone Y 0.772. The reader skips the HUD masks, and every readout of that frame lies under a mask (T-HUD-03), so the HUD neither covers a hero pixel nor adds one. The count on the frame `dev/s4` held was not read: the tool faulted on each of six tries.

Not done on the merged tree: perf, and the five bot arms (the gate's `restart` and `meta` hashes were read; the arms were not rerun).

## 7. Not verified

- No person looked at the HUD in play. The agent viewed `hud_staged` in both cultures, `hud_boss`, the contrast frame, and one probe frame in zh-Hans at 150 % text with the boss bar, a Keeper meter, the aim line and a dormant line up.
- The low-HP pulse and its frame were never drawn in a frame: no debug command lowers HP, and the staging commands hold HP full. T-HUD-05 reads the function, not pixels.
- The ready ring, a refilling dash pip, a banner, a toast and the level-up line were not shot; their rects are walked by the tests.
- The offer screen over the new HUD was not shot (a probe did not open a hand) and no test lays the two together. It draws its wash and cards over the HUD as before; its title starts at 12 % of the height, and whether the top blocks stay above it at the largest text was not checked.
- Windows narrower than 16:9 are not walked. Two players are walked only with the readouts every run shows, at 1920x1080; the co-op frame is `coop.md`'s.
- Whether `Pal.UI.Boss` reads as the boss's bar, whether Body text at 720 lines is comfortable, and whether the ring is found at a glance are a person's judgement.
- Perf: see §3. Only Caps was read, as the brief asks; S2 and S4 were not.
