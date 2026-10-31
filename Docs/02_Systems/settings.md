# Settings — the pause, settings and controls screens (S3 row)

## Intent
A player can make the game fit them before the first card: language, sound, display, keys, text size, motion and danger outlines, kept across launches. Nothing here is ever allowed to change a run: the same seed plays the same with every setting at any value.

Written from `Handoff_Dusktracer/game/G02_Blueprint_DocSet_zh.md` (UX/UI: rebinding, text size `150 %`, colour-blind check, reduced motion; B backs out of every screen, no dead ends), `G03` and `12_Production` (the S3 row), `reference/original_docs/02_TechnicalDesign_s17_decision_log_HISTORY.md` (the 2026-10-20 accessibility entry and the 2026-10-19 language entry), `03_Roadmap.md` (remap, text size, colour-blind check of the palette) and `reference/text_tables/ST_UI.csv` + `zh_overrides.csv` (rows and wording). `[invented]` marks what the source does not give; it is this spec's choice and the first thing to revisit.

## Rules
### Sim isolation
- **R-SET-01** No setting reaches the sim. A run with every setting changed (culture included) gives the same steps, inputs and telemetry as a run at the defaults (R-TEC-02); the one telemetry field that differs is the `culture` tag every record carries. Settings change only what is drawn, heard or which physical key sends an action.
- **R-SET-02** The settings header is read only by presentation and input code: the list is in T-SET-02, and a new reader is added there on purpose. No file of a sim system may include it. Hit-stop and slow motion, which the source lists, are left out for this reason: here both would change sim time (Out of scope).

### The file
- **R-SET-03** Settings live in one plain-text local file, `Saved/Config/DTSettings.ini` (`-DTSettingsFile=<path>` overrides it), `Key=Value` lines beginning with `Version=C.SETTINGS.FILE_VERSION`. It is per machine, written whole on every change to a sibling temporary file and moved over the old one. It uses no save slot, envelope or checksum: saves are a later item and never read it. `[invented]` the file name, its format and the write-through; the source used `GameUserSettings.ini`, and this file keeps every engine ini out of it.
- **R-SET-04** A bad file never stops a launch. A missing, empty or unreadable file gives the defaults; an unknown key is ignored; a value that fails to parse or is out of range leaves that one setting at its default; a binding table that is illegal (R-SET-13, R-SET-14) falls back whole for that device and group. Volume and text size snap to their steps.
- **R-SET-05** Tool runs are immune: an `-unattended` process neither reads nor writes the file unless `-DTSettingsFile` is given, so a person's settings cannot move a golden, a perf reading or a bot number. `[invented]`

### Rows
Order on the screen, with the default first:

| Row | Values | Effect now |
|---|---|---|
| Language | English, 中文 | every string, live |
| Master volume | `C.SETTINGS.VOLUME_STEP_PCT` steps up to `C.SETTINGS.VOLUME_MAX_PCT`; default `C.SETTINGS.VOLUME_MAX_PCT` | the application volume; the game has no sound yet |
| Graphics | Epic, High, Medium, Low | engine scalability level |
| Window `[invented]` | Fullscreen, Windowed | window mode; the row's name comes from the brief's "display" |
| Text size | `C.SETTINGS.TEXT_MIN_PCT` to `C.SETTINGS.TEXT_MAX_PCT` by `C.SETTINGS.TEXT_STEP_PCT` | HUD, offer screen, these screens |
| Motion | Full, Reduced, Custom | sets the two rows below |
| Screen shake | On, Off | read by the camera when shake exists |
| Screen flashes | On, Off | read by the flash when one exists |
| Danger outlines | Off, On | R-SET-10 |
| Controls | opens the screen | R-SET-13 |

- **R-SET-06** Language switches live and one press changes it once: the switch runs on the next frame, and a press while one is queued is ignored (the source's five-second freeze). The saved language applies at launch unless `-culture=` is on the command line; `Tools/Play.bat` passes it only when given one.
- **R-SET-07** Master volume is the application volume multiplier, so any sound added later obeys it with no further wiring.
- **R-SET-08** Graphics sets one engine scalability level for every group, and Window the window mode; both apply on change and at launch when a file was read. Only Epic is measured: the contrast and perf gates run there.
- **R-SET-09** Motion Reduced turns Screen shake and Screen flashes off, Full turns them on, and either row changed by hand shows Custom; Custom is never stepped onto directly. `DTSettings::AllowShake` and `DTSettings::AllowFlash` are the read points for the effects when they land; nothing in the game shakes or flashes yet, and the rows' focus hints say so.
- **R-SET-10** Danger outlines (off by default): an achromatic assist for telling danger by shape, not hue. Every red telegraph gets a band of `Pal.Assist.Light`, `C.SETTINGS.OUTLINE_BAND_UU` wide, outside its dark edge (R-PRE-05); every enemy round draws over a `Pal.Assist.Light` disc of `C.SETTINGS.OUTLINE_ROUND_RATIO` of its radius on a `Pal.Assist.Dark` disc of `C.SETTINGS.OUTLINE_ROUND_DARK_RATIO`. It never changes a footprint, never touches a control cue (R-PRE-11) and draws nothing while off. `[invented]` the sizes; the source gives only the colours and the stacking idea.
- **R-SET-11** Colour-vision check: under protan, deutan and tritan simulation (`Machado 2009` at full severity, `CIE76` distance) enemy rounds and telegraphs stay at least `C.SETTINGS.CVD_DELTA_E_MIN` from every player round token, and the enemy muzzle at least `C.SETTINGS.CVD_MUZZLE_DELTA_E_MIN`. The method and thresholds are the source's. Known debts, named in T-SET-11 and failing it if they grow or vanish: Forge's trail against the telegraph and against the muzzle, both under deutan. Danger outlines are the player's answer until the token question is settled (Open).
- **R-SET-12** Text size multiplies the drawn scale of every string on the HUD, the XP bar, the boss bar, the offer screen and these screens. Rows and cards grow with it so nothing leaves its panel at the largest size.

### Rebinding
- **R-SET-13** The slot table, one key per device:

| Group | Slot | Keyboard and mouse | Pad |
|---|---|---|---|
| In play | Move up, down, left, right | `W S A D` | fixed: left stick |
| In play | Dash | `Space` | `RB` |
| In play | Intercession | `Q` | `LB` |
| Level-up | Previous, Next card | `A D` | fixed: D-pad and left stick |
| Level-up | Renegotiate | `R` | `X` |
| Level-up | Refuse | `B` | `Y` |
| Level-up | Hold | `L` | `RB` |

  Fixed, never rebindable: Pause and Back (`Esc`, pad Menu, pad `B`), Accept (`Enter`, pad `A`), arrow keys, D-pad, both sticks, the mouse's aim; in the Level-up group also `Space`, the number keys and the left mouse button, which pick cards. Prompts follow the table: the offer tool buttons show the bound keys.
- **R-SET-14** Binding a key takes effect at once. A key already held by another slot of the same group and device swaps with it; a fixed key is refused (`Controls.Reserved`); a key of the other device is refused (`Controls.WrongDevice.*`). Groups never conflict: they are never live together. Reset restores every slot.
- **R-SET-15** Capture starts on Accept, ignores keys already held when it starts, and ends on the next press; `Esc` or pad Menu cancels and changes nothing.

### Screens
- **R-SET-16** There is no front screen yet, so the pause screen is the way in: Pause (`Esc`, pad Menu) opens it from any moment of play, the run-start Patron hand included, and pauses the world. Its rows are Resume and Settings. Closing never unpauses an open hand. A hand under the menu takes no input, its confirm guard (R-OFF-11) restarts when the menu closes, keys held then must be released first, and the time in the menu is not decision time.
- **R-SET-17** Every row is reachable and usable from the keyboard alone and from the pad alone: up and down move focus and wrap, left and right step a value, Accept activates a row and steps a value forward, Back returns one screen, and Back from Pause closes it. No screen is a dead end. Keyboard navigation is the arrows plus `W A S D`, and `Space` is a second Accept; pad navigation is the D-pad and left stick. These stay fixed whatever the table says. `[invented]` the second keys.
- **R-SET-18** Grey-box, Canvas-drawn like the offer screen, no binary UI asset (R-TEC-06). Colour comes only from `Pal.UI.*` tokens (R-PRE-08, R-PRE-09); focus is carried by a frame, not by hue alone. All text is ST_UI, en and zh-Hans.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| File format version | C.SETTINGS.FILE_VERSION | CORE | R-SET-03 |
| Volume ceiling and step | C.SETTINGS.VOLUME_MAX_PCT, C.SETTINGS.VOLUME_STEP_PCT | CORE | R-SET-07 |
| Text size range and step | C.SETTINGS.TEXT_MIN_PCT, C.SETTINGS.TEXT_MAX_PCT, C.SETTINGS.TEXT_STEP_PCT | CORE | R-SET-12 |
| Outline sizes | C.SETTINGS.OUTLINE_BAND_UU, C.SETTINGS.OUTLINE_ROUND_RATIO, C.SETTINGS.OUTLINE_ROUND_DARK_RATIO | GATED(S3) | R-SET-10 |
| Colour-vision thresholds | C.SETTINGS.CVD_DELTA_E_MIN, C.SETTINGS.CVD_MUZZLE_DELTA_E_MIN | CORE | R-SET-11 |

## Data
| Field | Type | Notes |
|---|---|---|
| `FDTUserSettings` | struct | the values of the rows, plus the bindings; no engine state |
| `FDTBindings` | table | one `FKey` per slot and device; empty for a fixed pad slot |
| `FDTMenuState` | struct | screen, focus, capture; pure, driven by commands |
| `DTSettings.ini` | text | keys `Version Language Volume Graphics Window Text Shake Flash Outline` and `Bind.<Slot>.<Device>` |

## Acceptance
- T-SET-01 `Dusktracer.Settings.SimUntouched`: two bot runs of one seed, defaults against every setting changed and the culture switched, end with equal steps, input stream, hero state and telemetry.
- T-SET-02 `Dusktracer.Settings.Readers`: only the listed presentation and input files include the settings header.
- T-SET-03 `Dusktracer.Settings.File`: save then load round-trips every field; the file carries the version, no envelope, and a failed write keeps the old file.
- T-SET-04 `Dusktracer.Settings.BadFile`: missing, empty, garbage, out-of-range, off-step, unknown-key and illegal-binding input each give the rule's result.
- T-SET-05 `Dusktracer.Settings.ToolRuns`: unattended with no override neither reads nor writes; with the override it does.
- T-SET-06 `Dusktracer.Settings.LanguageOnePress`: eight presses queue one switch; the next is taken only after it ran.
- T-SET-07 `Dusktracer.Settings.Volume`: the row steps and clamps; applying sets the application multiplier.
- T-SET-08 `Dusktracer.Settings.Display`: Graphics maps each value to its scalability level; Window steps both ways.
- T-SET-09 `Dusktracer.Settings.Motion`: Reduced and Full set both rows; a mix reads Custom; the read points follow.
- T-SET-10 `Dusktracer.Settings.DangerOutline`: off adds no instance anywhere; on adds the bands and discs in the assist tokens at the stated sizes; a cue renderer gains none; telegraph red keeps its footprint.
- T-SET-11 `Dusktracer.Settings.ColourVision`: the distances hold for the palette except the named debts, and those are exactly the debts.
- T-SET-12 `Dusktracer.Settings.TextSize`: the steps give their scales; the largest size fits every screen's rows in a canvas at `C.BUDGET.RES`.
- T-SET-13 `Dusktracer.Settings.BindingTable`: the defaults equal the table and `combat_core.md` mapping; the input contexts and `DTOfferInput` follow a changed table; prompts follow.
- T-SET-14 `Dusktracer.Settings.Rebind`: bound, swapped, reserved and wrong-device results; no duplicates in a group and device after any sequence; reset.
- T-SET-15 `Dusktracer.Settings.Capture`: a key held at the start is ignored; cancel changes nothing.
- T-SET-16 `Dusktracer.Settings.PauseOverHand`: open and close over a hand keeps the world paused, blocks hand input and restarts the guard; alone it pauses and resumes.
- T-SET-17 `Dusktracer.Settings.Navigation`: every row of every screen is reached and operated by the keyboard list and by the pad list; Back chains out from the deepest screen.
- T-SET-18 `Dusktracer.Settings.TextKeys`: every menu key resolves in ST_UI; menu code holds no colour outside the tokens.
- HUMAN: off-screen shots cannot judge these. Keys and pad on real hands; Windowed and Fullscreen on a real desktop; volume once sound exists; a reader's view of the outlines in play, and of the largest text on the offer screen.

## Telemetry
None. Settings are not run data; telemetry stays equal under R-SET-01 but for its existing `culture` tag.

## Text
Prefixes `ST_UI.Pause.*`, `ST_UI.Settings.*`, `ST_UI.Controls.*`, `ST_UI.Menu.*`; budget `70` rows; en and zh-Hans. Source wording is kept where it exists; `Pause.Title`, `Menu.*`, `Settings.Window*`, `Settings.Hint.*` and the zh of rows missing from `zh_overrides.csv` are `[invented]`, drawn from `zh_glossary.md`.

## Out of scope
- Hit-stop, slow motion: R-SET-02. They return when a spec makes them presentation-only.
- Steam Deck and Custom graphics values, resolution list, Keeper remarks (no remarks exist), Export logs, Send report, Licences, Abandon run: their systems are not built, or belong to packaging.
- Music and effects volumes: the source has one master row.
- Mouse hover and click on these screens: the brief asks for keyboard and pad.
- A title screen. Saves.

## Open
| Question | Data that resolves it |
|---|---|
| Change `Pal.Keeper.Forge` and `Pal.Keeper.Quicksilver` trails to the source's combat colours (`#FFC17A`, `#B3FB60`), or keep them and rely on outlines? | an owner call; a token change is PRESENT and runs through the Combat renderer |
| Do Medium and Low hold R-PRE-05? | T-PRE-05 at each level; the gate reads Epic only |
