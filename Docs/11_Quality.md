# 11 — Quality

> Gates, tests, instrumentation, validation, measurement. Method: `Harness/H07`. Numbers are `C.` keys.

## 1. Merge gates

One command runs them all on the merged tree: `& Tools\Gate.ps1` from PowerShell (`-AllFiles` for a full non-unity pass, `-Skip a,b` to skip steps, which then count as not run). It prints one line per step and ends with `GATE: PASS` or `GATE: FAIL at <step>`; a step that cannot run yet prints `NOT RUN (<reason>)` and is never counted as passed.

| Step | Catches | Tool | Status |
|---|---|---|---|
| `secrets` | keys/tokens in tracked files (public repo, D-006) | `Tools/secret_scan.py` | live |
| `docs` | docs lint (`Harness/H02 §4`) | `Tools/docs_lint.py` | live |
| `constants` | generated header out of date (R-TEC-07) | `Tools/gen_constants.py --check` | live |
| `catalog` | generated catalog header out of date against `Docs/09_Catalog/*.csv` | `Tools/gen_catalog.py --check` | live |
| `matrix` | `matrix.csv` out of date against the caps of weapons and modifiers | `Tools/gen_matrix.py --check` | live |
| `loc` | missing en/zh, forbidden words, round-trip string, argument match, glossary drift, zh style and budgets, missing rows (R-TEC-08) | `Tools/loc_build.py --check` | live |
| `licence` | an asset with no `licenses.csv` row, a licence outside the allowed set, an asset whose generator is not in this repo, anything external or AI-sourced while O-009 is open (R-PRE-10, R-TEC-06, D-043) | `Tools/catalog_lint.py` | live |
| `palette` | a token in the enemy hue band that is not enemy fire, a token row that does not parse, a colour spelled in an asset generator (R-PRE-02, R-PRE-09) | `Tools/palette_lint.py` | live |
| `build` | editor build, 0 compiler warnings | `Tools/Build.bat` | live |
| `locbuild` | the git-ignored generated text and `.locres`, rebuilt so tests and smoke never read stale or missing tables (R-TEC-08) | `Tools/Localize.bat` | live |
| `nonunity` | each `.cpp` compiles alone (R-TEC-09) | UBT `-SingleFile` per changed file (`-AllFiles` for all) | live |
| `shipping` | test/debug code leaking into Shipping (R-TEC-12) | `Tools/Build.bat Shipping` (game target) | live |
| `tools` | `Tools/tools_test.py`: the logic of the Python tools (hit kinds, probes, contrast, the licence and palette lints), every scene file, the mesh pipeline's rules, and no sim file including a presentation header (R-PRE-11) | Python, stdlib | live |
| `contrast` | every scene of `Tools/Scenes` with `"gate": "contrast"`: T-PRE-05 on an off-screen late frame (`Tools/contrast_check.py`, scene `contrast`); an element below `C.PRE.CONTRAST_MIN` or not found fails, unless `Docs/Validation/contrast_debts.txt` names it; then the HUD's dormant line (`modifiers.md` R-MOD-08) against its own panel, on a second staged frame (scene `dormant_line`), where a frame that shows no line fails; then any frame a package staged, judged on the elements its scene names. Each contrast frame also prints the lit floor's spread and a `WATCH` line against `C.PRE.CONTRAST_WATCH_MIN` and `C.PRE.LIT_GROUND_Y_WINDOW`: a reading for S4 wave 1, never a failure | game `-RenderOffScreen` | live |
| `settings` | a render setting of `04_Presentation §4` missing from the config or from the arena volume's generator; exposure that moves between the empty arena and the contrast frame (T-PRE-07) | `Tools/palette_lint.py --settings` | live |
| `tests` | automation suite `Dusktracer`, headless on the hidden desktop; crash scan; count ≥ `C.GATE.TEST_FLOOR` | `Tools/RunTests.ps1` | live |
| `smoke` | rendered runs on the arena against their goldens: every scene of `Tools/Scenes` with `"gate": "smoke"`, in each of its cultures, `arena_empty` first. A scene is a data file with its commands, its judge and its weather pin (`Tools/Smoke.ps1 -Scene`) | `Tools/Smoke.ps1` | live |
| `restart` | a real map reload against a fresh launch by input hash, and real keys through a pause and a reload (`settings.md` T-SET-28); a crash or ensure line in any of its logs fails | `Tools/RestartCheck.ps1` | live |
| `seed` | every human launch gets a fresh seed, tools and explicit seeds stay exact, an out-of-range seed is rejected (R-TEC-16): each launcher's real arguments started off-screen, through the hub, and `run_start.seed` read back, with `driver` `person` on a launcher and `tool` on a named seed (R-TEL-13) | `Tools/SeedCheck.ps1` | live |
| `meta` | the save on real launches, headless: no tool run touches it, a run banks it, a bad file goes aside, a win reaches a fight; the gated pools are read from the cards dealt (`meta.md` T-MET-16, T-MET-17, D-037) | `Tools/MetaCheck.ps1` | live |
| `vigil` | the hub on real launches, off-screen, en and zh-Hans: boot to a run and back on real keys, purchases read by a new process, every page at the largest text (`vigil.md` T-VIG-17) | `Tools/VigilCheck.ps1` | live |
| `firstrun` | R-OFF-15 on a person's first pool, which the `tests` step's T-OFF-15 cannot see (`meta.md` R-MET-25) | `Tools/BotRuns.ps1 -FreshPool -Check T-OFF-15 -ReadAll`, six runs, each one read at the boss or the step fails (D-037) | live |
| `win` | a win on a real gated save, on one launch: toast, file, Shrine, bestiary, Recollections (`meta.md` T-MET-29) | `Tools/WinCheck.ps1` | live |
| `earned` | a person's loop from an empty save: Ichor a run earned, spent by a real key, read by a new process, offered on the next run; then the doors' rows bought on the page and reached on gated runs (`meta.md` T-MET-30) | `Tools/EarnedCheck.ps1` | live |
| `windows` | a bot, a smoke and a test run show no window on the desktop, with a probe and a bypass control (R-TEC-17); no game outlives them | `Tools/WindowCheck.ps1` | live |
| `pad` | packaged build runs off-screen in both cultures, telemetry written and exported (`02_Systems/package.md`); a real pad needs a human, so the step reads NOT RUN until a run of this build has `input_device` `pad` | `Tools/PadCheck.ps1` | live, NOT RUN without a current clean package |
| `scope` | files outside a brief's allowed list | pipeline only | NOT RUN until the first brief |
| `mac` | Mac build | `Tools/MacGate.sh` | deferred (D-007) |

The floor `C.GATE.TEST_FLOOR` only rises: raise it in the same commit that adds tests.

## 2. Test taxonomy
Unit → system → end-to-end → bot regression → golden (images, `SoloParity`) → performance (p50, p95). Automation tests are named `Dusktracer.<System>.<Case>` and carry their `T-<SYS>-nn` id in a comment on the macro line; the docs lint fails a `T-` id named in a spec that no source or tool contains, unless it is marked `PLANNED(<stage>)`.

## 3. Instrumentation (implemented in S0 against the empty arena)
Per-minute CSV (`Saved/Playtests/<seed>_<time>.csv`) and a JSONL event log, both exported by the packaged build:
- core bands: kills/min, swarm TTK, damage taken/min by source, dashes/min, level-up minute
- threat actions/min, nearest-enemy distance p10, enemy bullets aimed at the player
- per visible effect: trigger / VFX / audio counts (so "I didn't see it" is checkable)
- healing, offers shown (all five cards + weights) / taken / skipped / rerolled / banished / locked
- outcome and cause, run seed, build hash, culture, input device

## 4. Human validation
Protocol, cadence and the note template: `Harness/H07 §4`, `Harness/templates/validation_note.md`. Notes go to `Docs/Validation/<date>_<stage>.md`; stage findings to `Docs/Validation/findings_S<n>.md`. Pass rules are written in the note before the session.

## 5. Measurement
GPU and performance runs hold `Saved/gpu.lock`; readings taken under contention are discarded. Report p50 and p95 against `C.BUDGET.*`. Bot numbers are bounds until calibrated against humans. A scene proves its load from counts taken in the capture window, never from its settings (`Tools/Perf.ps1 -Scene S2`, `Docs/Validation/S2_perf.md`; `-Scene S4` adds trailed rounds and live telegraphs to it, `Docs/Validation/S4_p0.md`).

## 6. Release checklist
`Harness/H07 §7`, plus: secret scan clean; licence log complete for every shipped asset.
