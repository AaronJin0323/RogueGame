# 11 — Quality

> Gates, tests, instrumentation, validation, measurement. Method: `Harness/H07`. Numbers are `C.` keys.

## 1. Merge gates

One command runs them all on the current tree, which for a merge is the merged tree: `& Tools\Gate.ps1` from PowerShell (`-AllFiles` for a full non-unity pass, `-Base <ref>` for the ref `nonunity` diffs against, `origin/main` if not given, `-Skip a,b` to skip steps, which then count as not run). It prints one line per step, in the order of the table, stops at the first failure, and ends with `GATE: PASS (<commit>; <n> not run)` or `GATE: FAIL at <step>`; a step that cannot run prints `NOT RUN (<reason>)` and is never counted as passed. A Python step whose script is missing also prints NOT RUN and the gate still passes, so read the not-run count.

| Step | Catches | Tool | Status |
|---|---|---|---|
| `secrets` | keys/tokens in tracked files (public repo, D-006) | `Tools/secret_scan.py` | live |
| `docs` | docs lint (`Harness/H02 §4`) | `Tools/docs_lint.py` | live |
| `constants` | generated constants or palette header out of date (R-TEC-07); a colour literal outside the palette | `Tools/gen_constants.py --check` | live |
| `catalog` | a malformed catalog; generated tables or native tags out of date (D-014) | `Tools/gen_catalog.py --check` | live |
| `loc` | missing en/zh, forbidden words, round-trip string (R-TEC-08) | `Tools/loc_build.py --check` | live |
| `tools` | the logic of the Python tools (hit kinds, probes, contrast); a tool launch past `Tools/HiddenLaunch.ps1` | `Tools/tools_test.py` | live |
| `build` | editor build, 0 compiler warnings | `Tools/Build.bat` | live |
| `locbuild` | stale or missing generated text: rebuilds the git-ignored string tables, `.po` and `.locres`, so tests and smoke read current text (R-TEC-08) | `Tools/Localize.bat` | live |
| `nonunity` | each `.cpp` compiles alone (R-TEC-09) | UBT `-SingleFile` per changed `.cpp` and the `.cpp` of each changed `.h` (`-AllFiles` for all) | live |
| `shipping` | code the Shipping game target cannot compile, or compiles with a warning (R-TEC-12) | `Tools/Build.bat Dusktracer Shipping` | live |
| `tests` | automation suite `Dusktracer`, headless on the hidden desktop; every test found completes; crash scan; count ≥ `C.GATE.TEST_FLOOR` | `Tools/RunTests.ps1` | live |
| `smoke` | a rendered run on the arena in en, then in zh-Hans: no crash, ensure or `LogDT` error, the drawn round-trip line equals `ST_UI.csv`, the screenshot against its golden | `Tools/Smoke.ps1` | live |
| `seed` | every human launch gets a fresh seed, tools and explicit seeds stay exact, an out-of-range seed is rejected (R-TEC-16): each launcher's real arguments started off-screen and `run_start.seed` read back | `Tools/SeedCheck.ps1` | live |
| `contrast` | T-PRE-05 on an off-screen late frame (`Tools/contrast_check.py`); an element below `C.PRE.CONTRAST_MIN` or not found fails, unless `Docs/Validation/contrast_debts.txt` names it; then the HUD's dormant line (`modifiers.md` R-MOD-08) against its own panel, on a second staged frame, where a frame that shows no line fails | `Tools/Smoke.ps1 -Contrast`, then `Tools/Smoke.ps1 -DormantLine` | live |
| `windows` | a bot, a smoke and a test run show no window on the desktop, with a probe and a bypass control (R-TEC-17); no game outlives them | `Tools/WindowCheck.ps1` | live |
| `pad` | packaged build runs off-screen in both cultures, telemetry written and exported (`02_Systems/package.md`); a real pad needs a human, so the step reads NOT RUN until a run of this build has `input_device` `pad` | `Tools/PadCheck.ps1` | live; NOT RUN without a current clean package or without a pad run |
| `scope` | files outside a brief's allowed list | none: the script holds no check and prints the line on every run | NOT RUN, always |
| `mac` | Mac build | `Tools/MacGate.sh`, which the gate does not call | NOT RUN, deferred (D-007) |

The floor `C.GATE.TEST_FLOOR` only rises: raise it in the same commit that adds tests.

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

Not written yet, each PLANNED with its stage:
- PLANNED(S1): healing, as a `hero_healed` record (D-046)
- PLANNED(S2): enemy bullets aimed at the player
- PLANNED(S2): a hand closed with no pick. Players cannot skip a hand; the bot's no-pick arm declines hands and counts them only in its own bot record
- PLANNED(S4): per visible effect, trigger / VFX / audio counts (so "I didn't see it" is checkable)

## 4. Human validation
Protocol, cadence and the note template: `Harness/H07 §4`, `Harness/templates/validation_note.md`. Notes go to `Docs/Validation/S<n>_<topic>.md`, named by stage and topic and not by date (`S0_exit.md`, `S1_owner_run.md`, `S1_kit.md`); stage findings to `Docs/Validation/findings_S<n>.md`, of which none exists on this branch. Pass rules are written in the note before the session.

## 5. Measurement
GPU and performance runs hold `Saved/gpu.lock`; readings taken under contention are discarded. Report p50 and p95 against `C.BUDGET.*`. Bot numbers are bounds until calibrated against humans.

## 6. Release checklist
`Harness/H07 §7`, plus: secret scan clean; licence log complete for every shipped asset.
