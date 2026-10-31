# 11 — Quality

> Gates, tests, instrumentation, validation, measurement. Method: `Harness/H07`. Numbers are `C.` keys.

## 1. Merge gates

One command runs them all on the merged tree: `& Tools\Gate.ps1` from PowerShell (`-AllFiles` for a full non-unity pass, `-Skip a,b` to skip steps, which then count as not run). It prints one line per step and ends with `GATE: PASS` or `GATE: FAIL at <step>`; a step that cannot run yet prints `NOT RUN (<reason>)` and is never counted as passed.

| Step | Catches | Tool | Status |
|---|---|---|---|
| `secrets` | keys/tokens in tracked files (public repo, D-006) | `Tools/secret_scan.py` | live |
| `docs` | docs lint (`Harness/H02 §4`) | `Tools/docs_lint.py` | live |
| `constants` | generated header out of date (R-TEC-07) | `Tools/gen_constants.py --check` | live |
| `loc` | missing en/zh, forbidden words, round-trip string (R-TEC-08) | `Tools/loc_build.py --check` | live |
| `build` | editor build, 0 compiler warnings | `Tools/Build.bat` | live |
| `nonunity` | each `.cpp` compiles alone (R-TEC-09) | UBT `-SingleFile` per changed file (`-AllFiles` for all) | live |
| `shipping` | test/debug code leaking into Shipping (R-TEC-12) | `Tools/Build.bat Shipping` (game target) | live |
| `tools` | `Tools/tools_test.py`: the logic of the Python tools (hit kinds, probes, contrast) | Python, stdlib | live |
| `contrast` | T-PRE-05 on an off-screen late frame (`Tools/contrast_check.py`, `Tools/Smoke.ps1 -Contrast`); an element below `C.PRE.CONTRAST_MIN` or not found fails, unless `Docs/Validation/contrast_debts.txt` names it | game `-RenderOffScreen` | live |
| `tests` | automation suite `Dusktracer`, headless; crash scan; count ≥ `C.GATE.TEST_FLOOR` | `UnrealEditor-Cmd -nullrhi` | live |
| `smoke` | rendered run on the arena, goldens, contrast | `Tools/Smoke.ps1` | NOT RUN until the S0 arena exists |
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
GPU and performance runs hold `Saved/gpu.lock`; readings taken under contention are discarded. Report p50 and p95 against `C.BUDGET.*`. Bot numbers are bounds until calibrated against humans. A scene proves its load from counts taken in the capture window, never from its settings (`Tools/Perf.ps1 -Scene S2`, `Docs/Validation/S2_perf.md`).

## 6. Release checklist
`Harness/H07 §7`, plus: secret scan clean; licence log complete for every shipped asset.
