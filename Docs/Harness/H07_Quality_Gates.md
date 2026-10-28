# H07 — Quality: gates, tests, instrumentation, human validation / 质量

## 1. Merge gates (exist before the first feature; identical locally and in the pipeline)

| Gate | Catches | Notes |
|---|---|---|
| Build, warnings as errors | shadowing, narrowing, unused, platform drift | 0 warnings is the bar from day 0 |
| Per-file compile of changed sources | missing includes hidden by unity/jumbo builds, file-scope leaks | the "it built in my worktree" failure |
| Release-configuration compile | test code outside guards, debug-only APIs | found at packaging otherwise |
| Headless test suite + crash scan + test-count floor | regressions, silent test loss | floor rises with the suite; "fewer tests than before" fails |
| Text / localisation audit | missing or stale translations, forbidden words, display names used as keys, hard-coded UI strings, length caps | mirrors a code test so both can run |
| **Rendered smoke + golden screenshots + contrast check** | visual regressions, vanished objects, inside-out meshes, palette violations | headless suites cannot see these |
| Packaged input check | input mapping in the shipped configuration | the device the owner does not own |
| Scope check | files outside `allowed_files` | fails the round |

A merge runs all gates on the merged tree. A gate added after a breakage is logged as a harness defect, not an agent defect.

## 2. Test taxonomy
Unit → system → end-to-end → scripted-run regression (bots) → golden (images, deterministic parity) → performance (p50 and p95). Names pair with rules (`T-<SYS>-nn` ↔ `R-<SYS>-nn`); tests are parameterised from the catalog so content removal leaves no orphans; a test that "passes trivially" is a finding.

## 3. Instrumentation from day 1
Decide the telemetry **before** the first validation session, not after the first complaint. Minimum: per-interval core metrics (the bands in the shape constants), outcomes by cause, every user decision shown and taken, counters for every effect that must be *noticed* (so "I didn't see it" is checkable), economy/health flows, the session seed. Written to CSV per interval + JSONL events; the first package already exports logs for validators.

## 4. Human validation protocol
- **Cadence:** ≥ 3 humans besides the owner, every 2–3 days from the first interactive build; the owner uses every feel-changing build. The "external" kit is a stage ritual, not a finale.
- **Conduct:** watch, don't explain; a fixed one-sentence intro; a timer; record verbatim words and the moment (timestamp) of each reaction; fixed post-session questions in a fixed order.
- **Record** (`templates/validation_note.md`): build hash, device, language, per-tester row, findings table → triage class → owner → hypothesis/decision.
- **Pass rules** are written before the session (e.g. 2 of 3 criteria).
- **Machine time is reserved:** automated runs and shared-binary rebuilds are held while validators use the product.

## 5. Measurement discipline
- Define the measurement (method, field, metric, bands) in `10_Tuning` **before** dispatching the job that measures it.
- Simulated/bot numbers are bounds until calibrated against humans; add a skill knob (e.g. "misses X % of cues") and report bot results as such.
- Exclusive resources are locked (file semaphore); readings taken under contention are discarded, not averaged.
- Report p50 and p95; budgets exist for both.
- Hardware targets are real devices or an agreed proxy factor written in `07_Technical`.

## 6. Number-change log (`templates/number_change_log.csv`)
One row per constant change: date, constant, from, to, evidence tag ([M]easured / [S]imulated / [H]uman), hypothesis/decision ID, re-measure job, result. Any constant with ≥ 3 rows in a stage is reviewed for oscillation.

## 7. Release checklist (minimum)
Release-config compile · package · packaged smoke on a quiet machine · packaged input check on the target device · log export verified · third-party runtime dependencies documented · version/tag applied to the release branch · findings file written for the build.

---
### 中文摘要
八道合并门槛（含渲染烟雾金图与打包版输入检查）先于第一个功能；测试分类与规则配对；第 1 天埋点；真人验证协议（节奏、方式、记录、预先写好的通过规则、预留机器时间）；测量纪律（先定义后测、模拟数字只是界、锁资源、p50/p95、真实硬件或代理系数）；数值变更日志；发布清单。
