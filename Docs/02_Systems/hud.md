# HUD — S4 wave package F

> The in-run readouts, re-laid (`Validation/S4_wave1.md` §3 package F; D-043). Written from `Validation/S4_artbook_gaps.md` §2.4 rows r1 to r3, r7 to r11, r30 and r31, and from the two fences every plate of the art book ignores: the floor `06_UX` R-UX-01 keeps in view, and the pixels `Tools/contrast_check.py` reads. Where a plate and a fence disagree the fence wins, and the row says so under Open. Everything is `UCanvas` on the engine font (`S4_wave1.md` §5, the calls on the UI stack and on fonts).

## Intent
A player reads the run's state from the corners of the frame and never loses a threat behind a readout. The HUD shows what it showed before, in places that hold at every window, in both languages and at the largest text size. It adds no readout kind (`Harness/H04 §4`) beyond the three tool counters the plan names, changes no string's wording or timing, and reads sim state without writing it (`04_Presentation` R-PRE-11).

## Rules
- **R-HUD-01** One layout function. `DTHud::Layout` takes the viewport, the player's text size and the text to show, and returns a rect for every readout; culture enters only through that text and its measured size. `ADTHUD`, `FDTXPBar` and `UDTBossSubsystem::DrawBossBar` draw into those rects and place nothing themselves. Lengths are written for a canvas of the reference height (Numbers) and scale with the canvas height; text scales with the canvas height times the text size (`settings.md` R-SET-12). No two rects overlap and none leaves the screen, at any tested window, culture or text size. A line that would leave its column wraps (dormant sockets) or shrinks to the column (every other line); no reaction toast shrinks at a tested window.
- **R-HUD-02** The ground round the hero stays clear. No rect that stays on screen covers the circle of `06_UX` R-UX-01 (the farthest point an enemy attack can start plus a Tank's diameter round the hero), read on the drawn plane as the solo camera frames it, at every supported window, text size and culture. A piece that grows with the build is limited by the circle: it keeps to the room beside it, its lines wrapped to that width (the dormant panels) or shrunk to fit (the aim line, the meter labels). The banner and the toast are transient and are not counted (D-080); they lie only in the band under the boss row and never lower than `C.HUD.TRANSIENT_FLOOR_UU` up the hero's column. The run's last line (`Hud.RunOver`, `Hud.Victory`) is the one exception: the run is over when it shows. Elite labels are world marks under their hulls (`elites.md` R-ELT-12) and are not readouts of this layout.
- **R-HUD-03** The HUD stays under the contrast tool's masks. At the smoke window and the default text size, every readout that does not grow with a build lies inside `HUD_MASKS` of `Tools/contrast_check.py`, so no HUD pixel is read as a gameplay element. Keeper meters, the aim line and dormant sockets grow upward out of the bottom-left mask, as the dormant line did before this package; the tool's scenes stage none of them.
- **R-HUD-04** Places. Top left: the HP readout and its bar, one block per player, then outside Shipping the developer lines (`package.md` R-PKG-02); the product line is not drawn in a run. Top right: the run clock, with the level beside it, the XP bar under them, then the XP readout, the level-up line and the pending-hand count (`progression.md` R-PRG-11). Top centre, downward: the boss row (the name, then its bar on the same line), the director's banner (`director.md` R-DIR-17), the reaction toast under it (`reactions.md` R-REA-12). Bottom left, upward from the margin: the Intercession ring with its label and the dash beside it, the tool counters, the Keeper meters (`keepers.md` R-KEE-38), the aim line, the dormant sockets (`modifiers.md` R-MOD-08). A readout with nothing to say takes no place, and a build that adds lines moves none of the fixed blocks.
- **R-HUD-05** The HP block. The readout is `Hud.Hp`; the bar fills in `Pal.UI.Text`. At or under `C.HUD.LOW_HP_FRAC` of maximum HP, with the hero alive, the bar is framed in `Pal.UI.Text` for as long as it holds, and its fill pulses between solid and `C.HUD.LOW_HP_PULSE_MIN` once every `C.HUD.LOW_HP_PULSE_S` of sim time, so it stops with the sim. With Screen flashes off (`settings.md` R-SET-09) the frame stays and the pulse does not. Nothing turns red, and nothing outside the HP block changes: the vignette and the heartbeat are held (Out of scope).
- **R-HUD-06** The Intercession ring and the dash. The ring fills clockwise from the top with the meter, in the Patron's Keeper token, and in `Pal.Player.TrailNeutral` before a Patron is chosen. Full, it gains an inner ring in `Pal.Player.Core` and its label reads `Hud.IntercessionReady` in place of `Hud.Intercession`, so the state is carried by shape and words as well as colour. The dash is one pip per charge the hero can hold, on the row of `Hud.Dash`: a ready pip full in `Pal.UI.Text`, the one coming back filling from the left in the same token, dimmed.
- **R-HUD-07** Tool counters. One panel of three lines, Renegotiate, Refuse and Hold, each the tool's own name (`Offer.Tool.*`) with the uses the player has left, read from `FDTPlayerOfferState` and never written. The line's form is `Hud.Tool`. The HUD's staged values are held on goldens: `hud_staged` in both cultures and `hud_boss`.
- **R-HUD-08** One type table. `DTUI::TypeStep` holds every step (Numbers): each is a multiplier on the engine font at the reference height. Every string of the HUD, the XP bar and the boss bar draws at a step times the canvas scale times the text size, and no file of theirs keeps a scale of its own. At the smoke window and the default text size a line of player text is at least `C.HUD.TEXT_MIN_PX` tall in both cultures. The offer screen, the hub and the menus keep their own multipliers until their own packages move them onto the table (Open).
- **R-HUD-09** One panel painter. `FDTUIPainter` draws tiles, frames, bars, arcs and panels for the HUD's files; a panel has four states (Numbers): default, focus, locked and solid, each `Pal.UI.Panel` under a border of `Pal.UI.Text`. The HUD uses default for the tool counters and solid for a dormant socket, whose text must hold `C.PRE.CONTRAST_MIN` on its panel whatever is behind it (`modifiers.md` R-MOD-08). A locked panel is dashed and says so in words as well.
- **R-HUD-10** Colour duties. A mark's meaning picks its token (Numbers), and no HUD style reads `Pal.Enemy.*`, `Pal.Elite.*` or `Pal.Telegraph*` (`04_Presentation` R-PRE-08). The boss bar fills in `Pal.UI.Boss`, a token of its own, in place of `Pal.Enemy.Optic`. Bone (`Pal.UI.Text`) carries text, the player's HP and the dash; the XP bar keeps `Pal.Pickup.Gem`; Keeper tokens mark the ring and the Keeper meters; tracks are `Pal.UI.Panel`.

## Numbers
| Constant | Tag | Used by |
|---|---|---|
| C.HUD.LOW_HP_FRAC | GATED(S4) | R-HUD-05, T-HUD-05 |
| C.HUD.LOW_HP_PULSE_S | GATED(S4) | R-HUD-05, T-HUD-05 |
| C.HUD.LOW_HP_PULSE_MIN | GATED(S4) | R-HUD-05, T-HUD-05 |
| C.HUD.TEXT_MIN_PX | GATED(S4) | R-HUD-08, T-HUD-08 |
| C.HUD.BANNER_S | GATED(S1) | read, not owned: `director.md`, `reactions.md` |
| C.SETTINGS.TEXT_MIN_PCT, C.SETTINGS.TEXT_MAX_PCT, C.SETTINGS.TEXT_STEP_PCT | CORE | the text sizes T-HUD-01 and T-HUD-02 walk |
| C.ROLE.TURRET_FIRE_RANGE_UU, C.ROLE.TURRET_RADIUS_UU, C.DIR.THREAT_RING_MAX_UU, C.BOSS.HAZARD_MIN_RADIUS_UU, C.ROLE.TANK_RADIUS_UU | GATED(S1) | read, not owned: the keep-out radius of R-HUD-02, as `06_UX` R-UX-01 sums them |
| C.HUD.TRANSIENT_FLOOR_UU | GATED(S4) | R-HUD-02, T-HUD-02: the lowest the banner and the toast may reach up the hero's column, on the drawn plane (D-080) |
| C.ARENA.CAMERA_PITCH_DEG, C.ARENA.CAMERA_FOV_DEG, C.ARENA.CAMERA_FRAME_RADIUS_UU | GATED | read, not owned: the lens the keep-out circle is projected through |

Layout lengths are presentation, not tuning, and live beside the code that reads them, as the HUD's always did (`Game/DTHudLayout.cpp`, `Game/DTUIStyle.h`, `Game/DTHUD.cpp`). Reference pixels on a canvas `1080` lines tall:

| Length | Value | Length | Value |
|---|---|---|---|
| side margin | 40 | top margin, bottom margin | 24, 28 |
| HP bar, XP bar | 360 x 8, 360 x 10 | boss bar | up to 560 x 12, top 12 |
| gap in a block, between blocks | 6, 12 | corner block to centre column | 24 |
| ring | 84 across, stroke 6, ready ring stroke 2 inset 10 | dash pip | 34 x 6, gap 6 |
| Keeper meter bar | 240 x 8 | left column wrap width | 360 |
| tool panel padding, dormant padding | 8, 4 | low-HP frame | 2 wide, 3 clear of the bar |

| Type step | Multiplier | Used for |
|---|---|---|
| Display | 2.6 | the run clock |
| Title | 1.8 | banners, the level, the run's last line |
| Body | 1.5 | every readout and label |
| Note | 1.0 | developer lines only |

| Panel state | Fill opacity | Border opacity | Border |
|---|---|---|---|
| default | 0.55 | 0.1 | solid |
| focus | 0.8 | 1.0 | solid |
| locked | 0.55 | 0.5 | dashed, 8 on and 8 off |
| solid | 1.0 | none | none |

| Duty | Token | Opacity |
|---|---|---|
| text, HP fill, ready dash pip | `Pal.UI.Text` | 1.0 |
| refilling dash pip | `Pal.UI.Text` | 0.6 |
| track of every bar, pip and ring | `Pal.UI.Panel` | 1.0 |
| XP fill, XP fill in the level-up flash | `Pal.Pickup.Gem`, `Pal.Player.Core` | 1.0 |
| boss bar fill | `Pal.UI.Boss` | 1.0 |
| ring with no Patron, ready ring | `Pal.Player.TrailNeutral`, `Pal.Player.Core` | 1.0 |
| ring with a Patron, Keeper meters | `Pal.Keeper.<Path>` | 1.0 |

## Data
No asset: the HUD is drawn in code. `04_Presentation §3` gains one row, `Pal.UI.Boss`, generated into `Core/DTPaletteGen.h`. Scenes: `Tools/Scenes/hud_staged.json`, `Tools/Scenes/hud_boss.json`. Files: `Game/DTHudLayout.*` (layout, keep-out, low HP), `Game/DTUIStyle.*` (type table, painter, duties), `Game/DTHUD.*`, `Progression/DTXPBar.*`, the bar in `Boss/DTBossSubsystem.cpp`. The style and layout headers sit in `Game/`, not `Presentation/`, because the XP bar and the boss bar already draw from sim-side folders and include them.

## Acceptance
- T-HUD-01 `Dusktracer.Hud.Layout.Overlap`: with every readout up at once and the widest text each can hold (four Keeper meters, the aim line, a dormant line on every socket of one weapon with the longest names, the boss row, a banner, the widest toast), at the smoke window, the target window, `1440` lines and two ultrawide windows, in en and zh-Hans, at every text size the setting offers: no two rects overlap, none is off screen or empty, every line sits inside its panel, and no reaction's toast is shrunk at the largest text. Two players, each with the readouts every run shows, stack without overlap on the target window.
- T-HUD-02 `Dusktracer.Hud.Layout.KeepOut`: the keep-out radius is R-UX-01's sum; the projection puts the hero on the middle column above the centre by the aim shift and the frame's centre on the aim point (D-080), the floor's down-screen reach on the bottom edge and the drawn plane's reach past the circle, and a pixel and its floor point name each other; over the same windows, cultures, text sizes and content no rect that stays on screen, the growing ones included, covers the circle on the drawn plane, and the banner and the toast stand under the boss row and no lower than `C.HUD.TRANSIENT_FLOOR_UU` up the hero's column. The check is shown to catch a rect on the hero, a sliver across the frame and the boss bar's old place at the bottom centre.
- T-HUD-03 `Dusktracer.Hud.Layout.Masks`: at the smoke window and the default text size, in both cultures, every readout that does not grow with a build is inside a mask. `Tools/tools_test.py` `HudMasks` holds the masks in `Game/DTHudLayout.cpp` equal to `HUD_MASKS` in `Tools/contrast_check.py`.
- T-HUD-04 `Dusktracer.Hud.Layout.Places`: each readout is in the place R-HUD-04 gives it, at every window, culture and text size; a readout with nothing to say has no rect; the fixed blocks do not move when a build adds lines; a long dormant line wraps and stays clear of the keep-out; the HUD's source reads no product line and still draws the language check outside Shipping.
- T-HUD-05 `Dusktracer.Hud.LowHp`: low at and under the line and not above it, not for a fallen hero; the fill is solid at the start of a pulse, at its floor half way and periodic; with flashes off the state stays and the fill is solid; the HP fill is `Pal.UI.Text`.
- T-HUD-06 `Dusktracer.Hud.Ring`: each Path's ring token is its Keeper token and none is in the enemy hue band; no Patron gives the neutral trail tone; the ready ring is `Pal.Player.Core`; a refilling pip is the ready pip's token dimmed; one pip per charge, on the dash label's row.
- T-HUD-07 `Dusktracer.Hud.Tools`: three lines, each naming its tool and ending in the uses left, from a fresh state and from a spent one, never below zero; `Hud.Tool` carries both arguments in both cultures; the clock reads minutes and seconds; the scenes `hud_staged` and `hud_boss` exist, and gate step `smoke` judges their goldens.
- T-HUD-08 `Dusktracer.Hud.Type`: the steps fall from Display to Note; a line of Body text at the smoke window is at least `C.HUD.TEXT_MIN_PX` tall in Latin and in Han; a drawn scale is step times canvas times text size; the HUD's files carry no type scale of their own.
- T-HUD-09 `Dusktracer.Hud.Panels`: the four states differ as R-HUD-09 says; text on a solid panel holds `C.PRE.CONTRAST_MIN`; the HUD's files build no tile of their own.
- T-HUD-10 `Dusktracer.Hud.Tokens`: no duty's token is an enemy, elite or telegraph token or sits in the enemy hue band; the boss bar's duty is `Pal.UI.Boss` and holds `C.PRE.CONTRAST_MIN` on its track; no file that draws the HUD names an enemy, elite or telegraph token.
- Gate steps `loc`, `constants`, `palette`, `contrast` (seven of seven with the HUD drawn, the dormant line read on its own frame) and `smoke`.
- Human, not verified by any check: that the corners read at a glance in play, that the ring is found without looking for it, that `Pal.UI.Boss` reads as the boss's and not as a pickup, and that Body text at the smoke window is comfortable.

## Telemetry
None.

## Text
- `ST_UI` `Hud.Tool` (new): `{Tool} ×{Count}`, the same in en and zh-Hans; a form with no word of its own, marked `[invented]`.
- Unchanged and still read: `Hud.Hp`, `Hud.Dash`, `Hud.Intercession`, `Hud.IntercessionReady`, `Hud.AimMode`, `Hud.RunOver`, `Hud.Victory`, `Hud.Threat`, `Hud.BossIncoming`, the Keeper meter rows, `Level.*`, `Offer.Tool.Reroll`, `Offer.Tool.Banish`, `Offer.Tool.Lock`, `ST_Modifiers` `Dormant.Hud`, `ST_Reactions` toast rows, `Test.LocRoundTrip`. The boss's name is read by `UDTBossSubsystem` as `boss.md` Text has it.
- `Product.Name` stays in the table and is no longer drawn in a run.

## Out of scope
Everything `Validation/S4_wave1.md` §4 holds back, and by name: the Favor panel, the tier and passed-over lines, weapon cells and icons, damage numbers, HUD sigils, the low-HP vignette and heartbeat, fonts (package G), the key glyph beside the ring, the phase line under the clock (`director.md` R-DIR-17 bars numbers in banners). The offer screen, the hub, pause and settings are not touched. The co-op frame is `coop.md`'s: the layout stacks players, and only the solo frame is held by R-HUD-02 and R-HUD-03.

## Open
- **Against the plate (PL.20).** The ring is bottom left, not bottom centre, and the boss bar is top centre, not bottom centre: the keep-out circle reaches to within a few percent of the bottom edge there. The clock is top right, not top centre: the centre column must hold the boss row, a banner and a toast inside the keep-out's top edge at the largest text, and a fourth row does not fit. The tool counters are bottom left, not bottom right: the bottom right is outside the contrast tool's masks. The XP bar is a bar in the top-right block, not a line across the top edge (the gap note records the line as a known failure).
- **The other screens.** The offer screen, the hub and the menus still carry their own painters and multipliers; R-HUD-08 and R-HUD-09 bind the HUD's files only until those screens are opened.
- **Narrow windows.** Windows narrower than the target aspect are not walked by T-HUD-01 or T-HUD-02: the keep-out circle keeps its pixel size while the side columns shrink.
- **Elite labels** draw on the small engine font at the raw text size, outside the type table.
- `Pal.UI.Boss` is the scheduler's value, chosen clear of the enemy band; the owner's palette (D-008) may move it.
