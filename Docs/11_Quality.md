# 11 — Quality

> Gates, tests, instrumentation, validation, measurement. Method: `Harness/H07`. Numbers are `C.` keys.

## 1. Merge gates

One command runs them all on the current tree, which for a merge is the merged tree: `& Tools\Gate.ps1` from PowerShell (`-AllFiles` for a full non-unity pass, `-Base <ref>` for the ref `nonunity` diffs against, `origin/main` if not given, `-Skip a,b` to skip steps, which then count as not run). It prints one line per step, in the order of the table, stops at the first failure, and ends with `GATE: PASS (<commit>; <n> not run; mask <mask>)` or `GATE: FAIL at <step> (<commit>; mask <mask>)`, where the mask is the processor mask the run held (D-052; `none` on a machine without the mask file); a step that cannot run prints `NOT RUN (<reason>)` and is never counted as passed. A Python step whose script is missing fails, and so does a `-Base` that does not resolve to a commit.

| Step | Catches | Tool | Status |
|---|---|---|---|
| `cpu` | a logical processor that faults in plain code (D-052): the fault loop of `Validation/S4_py_fault.md §5`, one run pinned to each processor the mask leaves in; names the processor that faulted or summed wrong. Every Tools entry script applies the mask first (`Tools/Machine.ps1`), so everything after runs inside it | `Tools/cpu_canary.py` (size calibrated in `Validation/S1_gate_main.md`) | live |
| `secrets` | keys/tokens in tracked files (public repo, D-006) | `Tools/secret_scan.py` | live |
| `docs` | docs lint (`Harness/H02 §4`) | `Tools/docs_lint.py` | live |
| `constants` | generated constants or palette header out of date (R-TEC-07); a colour literal outside the palette | `Tools/gen_constants.py --check` | live |
| `catalog` | a malformed catalog; generated tables or native tags out of date (D-014) | `Tools/gen_catalog.py --check` | live |
| `matrix` | `matrix.csv` out of date against the caps of weapons and modifiers | `Tools/gen_matrix.py --check` | live |
| `loc` | missing en/zh, forbidden words, round-trip string, argument match, glossary drift, zh style and budgets, missing rows (R-TEC-08) | `Tools/loc_build.py --check` | live |
| `licence` | an asset with no `licenses.csv` row, a licence outside the allowed set, an asset whose generator is not in this repo, anything external or AI-sourced while O-009 is open (R-PRE-10, R-TEC-06, D-043) | `Tools/catalog_lint.py` | live |
| `palette` | a token in the enemy hue band that is not enemy fire, a token row that does not parse, a colour spelled in an asset generator (R-PRE-02, R-PRE-09) | `Tools/palette_lint.py` | live |
| `tools` | `Tools/tools_test.py`: the logic of the Python tools (hit kinds, probes, contrast, the licence and palette lints), every scene file, the mesh pipeline's rules, and no sim file including a presentation header (R-PRE-11); a tool launch past `Tools/HiddenLaunch.ps1` | `Tools/tools_test.py` | live |
| `build` | editor build, 0 compiler warnings | `Tools/Build.bat` | live |
| `locbuild` | stale or missing generated text: rebuilds the git-ignored string tables, `.po` and `.locres`, so tests and smoke read current text (R-TEC-08) | `Tools/Localize.bat` | live |
| `nonunity` | each `.cpp` compiles alone (R-TEC-09) | UBT `-SingleFile` per changed `.cpp` and the `.cpp` of each changed `.h` (`-AllFiles` for all); a `-Base` that does not resolve fails | live |
| `shipping` | code the Shipping game target cannot compile, or compiles with a warning (R-TEC-12) | `Tools/Build.bat Dusktracer Shipping` | live |
| `devgame` | code the Development game target cannot compile, or compiles with a warning: an editor-only API inside `WITH_DEV_AUTOMATION_TESTS` compiles in the editor target and is compiled out of Shipping | `Tools/Build.bat Dusktracer Development` | live |
| `contrast` | every scene of `Tools/Scenes` with `"gate": "contrast"`: T-PRE-05 on an off-screen late frame (`Tools/contrast_check.py`, scene `contrast`); an element below `C.PRE.CONTRAST_MIN` or not found fails, unless `Docs/Validation/contrast_debts.txt` names it; then the HUD's dormant line (`modifiers.md` R-MOD-08) against its own panel, on a second staged frame (scene `dormant_line`), where a frame that shows no line fails; then any frame a package staged, judged on the elements its scene names. Each contrast frame also prints the lit floor's spread and a `WATCH` line against `C.PRE.CONTRAST_WATCH_MIN` and `C.PRE.LIT_GROUND_Y_WINDOW`: a reading for S4 wave 1, never a failure | game `-RenderOffScreen` | live |
| `settings` | a render setting of `04_Presentation §4` missing from the config or from the arena volume's generator; exposure that moves between the empty arena and the contrast frame (T-PRE-07) | `Tools/palette_lint.py --settings` | live |
| `tests` | automation suite `Dusktracer`, headless on the hidden desktop; every test found completes; crash scan; count = `C.GATE.TEST_FLOOR` (fewer fails; more fails with a line naming the number to set) | `Tools/RunTests.ps1` | live |
| `smoke` | rendered runs on the arena against their goldens: every scene of `Tools/Scenes` with `"gate": "smoke"`, in each of its cultures, `arena_empty` first. A scene is a data file with its commands, its judge and its weather pin (`Tools/Smoke.ps1 -Scene`). Every run: no crash, ensure or `LogDT` error, the drawn round-trip line equals `ST_UI.csv`, the drawn Dash and Intercession labels carry a key (`combat_core.md` R-CMB-13), the game's process mask, sampled about every 0.2 s, stays inside the machine's mask (D-052); then T-PRE-07's burst half (`04_Presentation` R-PRE-07): the arena post-process volume (`Tools/render_settings_check.py`) and a burst frame with no exposure change against the calm frame | `Tools/Smoke.ps1 -Scene`, then `Tools/Smoke.ps1 -Burst` | live |
| `restart` | a real map reload against a fresh launch by input hash, and real keys through a pause and a reload (`settings.md` T-SET-28); a crash or ensure line in any of its logs fails | `Tools/RestartCheck.ps1` | live |
| `seed` | every human launch gets a fresh seed, tools and explicit seeds stay exact, an out-of-range seed is rejected (R-TEC-16): each launcher's real arguments started off-screen, through the hub, and `run_start.seed` read back, with `driver` `person` on a launcher and `tool` on a named seed (R-TEL-13) | `Tools/SeedCheck.ps1` | live |
| `meta` | the save on real launches, headless: no tool run touches it, a run banks it, a bad file goes aside, a win reaches a fight; the gated pools are read from the cards dealt (`meta.md` T-MET-16, T-MET-17, D-037) | `Tools/MetaCheck.ps1` | live |
| `vigil` | the hub on real launches, off-screen, en and zh-Hans: boot to a run and back on real keys, purchases read by a new process, every page at the largest text (`vigil.md` T-VIG-17) | `Tools/VigilCheck.ps1` | live |
| `firstrun` | R-OFF-15 on a person's first pool, which the `tests` step's T-OFF-15 cannot see (`meta.md` R-MET-25) | `Tools/BotRuns.ps1 -FreshPool -Check T-OFF-15 -ReadAll`, six runs, each one read at the boss or the step fails (D-037) | live |
| `win` | a win on a real gated save, on one launch: toast, file, Shrine, bestiary, Recollections (`meta.md` T-MET-29) | `Tools/WinCheck.ps1` | live |
| `earned` | a person's loop from an empty save: Ichor a run earned, spent by a real key, read by a new process, offered on the next run; then the doors' rows bought on the page and reached on gated runs (`meta.md` T-MET-30) | `Tools/EarnedCheck.ps1` | live |
| `windows` | a bot, a smoke and a test run show no window on the desktop, with a probe and a bypass control (R-TEC-17); no game outlives them | `Tools/WindowCheck.ps1` | live |
| `pad` | packaged build runs off-screen in both cultures, telemetry written and exported (`02_Systems/package.md`); a real pad needs a human, so the step reads NOT RUN until a run of this build has `input_device` `pad` | `Tools/PadCheck.ps1` | live; NOT RUN without a current clean package or without a pad run |
| `scope` | files outside a brief's allowed list | none: the script holds no check and prints the line on every run | NOT RUN, always |
| `mac` | Mac build | `Tools/MacGate.sh`, which the gate does not call | NOT RUN, deferred (D-007) |

The floor `C.GATE.TEST_FLOOR` is the suite's count: set it in the same commit that adds or removes tests, and after a merge to the merged tree's count.

## 2. Test taxonomy
Unit → system → end-to-end → bot regression → golden (images; the `SoloParity` golden is PLANNED(S6) and exists nowhere yet) → performance (p50, p95). Automation tests are named `Dusktracer.<System>.<Case>` and carry their `T-<SYS>-nn` id in a comment on the macro line; the docs lint fails a `T-` id named in a spec that no source or tool contains, unless it is marked `PLANNED(<stage>)`.

## 3. Instrumentation
The spec is `02_Systems/telemetry.md`; the record kinds of each system are in that system's own spec. A session writes one CSV row per player per `C.TEL.INTERVAL_S` of sim time (`Saved/Playtests/<seed>_<time>.csv`) and a JSONL event log; the packaged build exports both (`02_Systems/package.md`).

Written today:
- core bands: kills/min, swarm TTK, damage taken/min as a share of max HP (each applied hit is a `hero_damaged` record with its kind, role and attack), dashes/min, the level at each interval end and a `level_up` record per level
- threat actions/min, nearest-enemy distance at `C.TEL.NEAREST_PERCENTILE`
- per source: damage, hits and kills; per weapon: emissions; status applications; Intercession casts
- offers shown (every card with its weight, again after a reroll or a banish refill) / taken / rerolled / banished / locked
- outcome (`run_end.reason`), run seed, build hash, culture, input device. A death's cause has no field of its own: it reads from the last `hero_damaged` record before `hero_down`
- healing: each restore that reaches a living hero is a `hero_healed` record with the HP really restored and its source (D-046)
- what the machine did to the run: `run_end.wall_dropped_steps` and `run_end.wall_duration_s`, so a run played in slow motion on a slow PC reads apart from a normal one (`telemetry.md` R-TEL-08)

Not written yet, each PLANNED with its stage:
- PLANNED(S2): enemy bullets aimed at the player
- PLANNED(S2): a hand closed with no pick. Players cannot skip a hand; the bot's no-pick arm declines hands and counts them only in its own bot record
- PLANNED(S4): per visible effect, trigger / VFX / audio counts (so "I didn't see it" is checkable)

## 4. Human validation
Protocol, cadence and the note template: `Harness/H07 §4`, `Harness/templates/validation_note.md`. Notes go to `Docs/Validation/S<n>_<topic>.md`, named by stage and topic and not by date (`S0_exit.md`, `S1_owner_run.md`, `S1_kit.md`); stage findings to `Docs/Validation/findings_S<n>.md`, of which none exists on this branch. Pass rules are written in the note before the session.

## 5. Measurement
GPU and performance runs hold `<git common dir>/dt-locks/gpu.lock`, one lock for every worktree on the machine (`Tools/GpuLock.ps1`); readings taken under contention are discarded. Report p50 and p95 against `C.BUDGET.*`. Bot numbers are bounds until calibrated against humans. A scene proves its load from counts taken in the capture window, never from its settings (`Tools/Perf.ps1 -Scene S2`, `Docs/Validation/S2_perf.md`; `-Scene S4` adds trailed rounds and live telegraphs to it, `Docs/Validation/S4_p0.md`).

## 6. Release checklist
`Harness/H07 §7`, plus: secret scan clean; licence log complete for every shipped asset.
