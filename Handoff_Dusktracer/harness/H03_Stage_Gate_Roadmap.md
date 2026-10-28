# H03 — Planning by gates / 按门槛计划

## 1. Why not a calendar
With agents, a six-week plan can be executed in two days. A calendar plan then stops being a plan and becomes a status log, and the one thing it was protecting — that the owner sees each milestone before the next exists — is lost. Plan in **stages with gates**; publish dates separately as forecasts.

## 2. The stage template (`templates/stage.md`)

| Field | Meaning |
|---|---|
| **Entry** | What must be true to start (previous gate passed; named inputs exist) |
| **Build** | The work, as references to system specs and catalog tables |
| **Exit test** | Always includes humans other than the owner; names the metrics and bands; names the artefact produced (e.g. `findings_<stage>.md`) |
| **Not before exit** | The list of things that may *not* start until this gate passes |
| **GATED numbers** | The numbers this stage sets, and the data they wait for |
| **Budget** | Token/cost cap for the stage; 15 % reserved for consolidation |

## 3. A generic stage ladder

| Stage | Purpose | Exit test (always with humans) | Not before exit |
|---|---|---|---|
| **S0 Foundations** | Repo, gates (`H07 §1`), instrumentation, architecture invariants, presentation contract on placeholders, shape-constant draft, value-budget tool, decision folder, agent entry doc, resource locks | Empty product runs within budgets on the named hardware/proxy; gates green; one end-to-end test string per language | any feature |
| **S1 Core slice** | The smallest build that produces the core feeling with placeholders; the core value system complete for a minimal subset | ≥ 3 non-owner humans use it; core-feel bands hold; they can describe the core in their own words; value split within budget; **shape constants locked** | breadth, meta systems, presentation fidelity |
| **S2 Core breadth** | The full core system set at minimal content; all ablation/measurement gates running nightly | gates measured and met; humans and bots both succeed at the core session | secondary modes, long-tail content |
| **S3 Loop closure** | Persistence, progression, first full package with log export, full localisation | External validation (fresh users, several sessions) with a pass rule (e.g. 2 of 3 criteria); `findings_S3.md` written | numbered specs for later versions |
| **S4 Presentation fidelity** | Art/UX/audio to the reference bar; performance tiers on real or agreed-proxy hardware | contrast/readability sheets pass; p50 and p95 within budget per tier; goldens re-baselined once | — |
| **S5 Content breadth** | Second content set, each with GATED numbers tied to S3 findings; GATED-debt limit enforced | each content set validated by humans before the next starts; external validation #2 | the third set before the second exits |
| **S6 Secondary modes** | Modes/multi-user built on S0 invariants | couch/multi-user test with humans; parity goldens unchanged | online/extended |
| **S7 Release** | Polish, accessibility, platform, onboarding (GATED on S5 data) | release checklist; re-certification on all tiers | — |

## 4. Rules that keep the ladder honest

- **GATED-debt limit.** When more than N numbers (default 5) wait on data, content stops and a validation session happens.
- **Scope valves** are written with a *trigger* (a date or a metric) and an *owner* who must write down why a valve did not close when its trigger fired. Opening a valve the other way (adding scope) goes through the system budget (`H04 §4`).
- **"Never cut" lists stay short** (≤ 6 items) and mean *never remove*, not *never reduce*.
- **Owner-play checkpoints are gates.** The pipeline may queue work past an unplayed checkpoint, but may not merge feel-changing systems past it.
- **Estimates name the critical path.** If the estimate is in agent-days, write next to it the human-days it implies (validation sessions, decision windows).
- **Later-version specs are sketches** (one page) until the current version's findings exist.

## 5. Calendar forecast
Keep a separate one-table forecast: stage → expected gate date → the human sessions booked for it. Update it weekly; never let it replace the stage table.

---
### 中文摘要
用阶段与门槛代替日历；每阶段有进入条件、含真人的退出测试、退出前不得开始的清单、GATED 数字与预算；通用八级阶梯 S0–S7；GATED 债务上限、带触发条件与负责人的缩减阀、短的“绝不砍”清单、负责人游玩检查点即门槛、估算必须写明人的关键路径；日历只作预测单独维护。
