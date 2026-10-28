# H05 — Context management / 上下文管理

**Definition.** Context management is deciding what each reader needs to see at each moment, and guaranteeing that small piece is current. Most chaos in agent projects is not missing information; it is too much, too old, or information that has drifted through several hops.

## 1. How context fails (observed)

| Failure | Typical evidence | Consequence |
|---|---|---|
| State mixed with log | a "rewrite before compaction" file that grew into hundreds of appended lines; a decision log of hundreds of entries inside a must-read document | no reader can finish it; current state buried |
| Must-read set without a budget | entry doc + five design docs + the whole log | agents read the head, skip the middle, or truncate |
| Capped channel used for decisions | "undelivered, resend after cap" lists | decisions delayed for hours; duplicate work |
| Open loop | the implementer's final message saved to disk but never shown to the reviewer | repeated identical stop conditions; empty rounds |
| Acting on reports | merge titled with a fix that was not in it | the scheduler's model of the repo drifts |
| Stale context unquestioned | an outdated agents file; session titles from week 1; "pre-redesign, the spec wins" | agents treat the old as true |
| Uncommitted = non-existent | an agent lost with its working tree | work and its context vanish together |
| Context not priced | 800k-token reviews | expensive and imprecise |

## 2. Rules

- **C1 State and log are separate.** The state block (`templates/state_block.md`) is ≤ 100 lines, at a fixed path with a fixed heading, **rewritten** at every change and before every compaction. Logs are append-only and retrieved on demand (one decision per file + an index). Agents read the state, not the log.
- **C2 The must-read set has a token budget** (default ≈ 20k tokens: agents entry doc + vision + shape constants). Everything else is loaded **by reference**: briefs cite `R-`/`T-`/`C-` IDs and `doc §section`; the agent reads only what is cited. Lint warns when the must-read set grows past budget.
- **C3 Decisions live outside the must-read set** (`13_Decisions/D-nnn.md`). Spec text shows the present only; no "decided on <date>" in body text.
- **C4 Capped or lossy channels only wake.** Decisions, tasks and lists travel as files in a shared folder (addressable, re-readable, durable). A message says "read file X". Address sessions by stable ID, never by display title.
- **C5 Close every loop.** The implementer's final message, gate outputs and a diff summary are inputs to the review prompt. An empty diff is read as a stop report; its reason feeds the next brief or ends the job as `blocked` after round 1.
- **C6 Reports are compressions; checkpoints return to the source.** Grep the merged tree for a claimed fix before titling a merge; at each stage gate, rebuild the state block from the repository (by a human or an independent agent), not from the previous narrative. Keep raw observations verbatim and interpretations separate and attributed.
- **C7 Stale is wrong.** Identity, ownership and in-flight work live in the state block; generated artefacts (loc files, matrices, constants headers, trap lists) are regenerated or deleted, never hand-patched; lint catches stale markers; the agents' entry doc is reconciled with the specs at every stage gate.
- **C8 Checkpoints are commits.** Every compiling step is committed; losing an agent costs at most one step.
- **C9 Rewrite before compaction, re-read after, with a size cap.** The ritual works; its failure mode is the re-read file growing. Move anything no longer "current" to an archive file when rewriting.
- **C10 Main context synthesises; reading is delegated with numbered questions.** For large bodies (thousands of commits, tens of thousands of doc lines): parallel sub-agents, each with a concrete question list and an output cap (≈ 1 500 words); large tool outputs go to disk and are read in slices; the main context holds conclusions and evidence. Anti-pattern: the whole diff in one review.
- **C11 Context is priced.** Caps for brief length, review diff size, must-read set and per-job tokens, all on the dashboard; cache the stable prefix; load the unstable part by reference; when a review exceeds its cap, ask the implementer to split the commit.

## 3. Entry points by reader

| Reader | Reads at start | Reads by reference while working | Never reads |
|---|---|---|---|
| Owner | `00_Vision`, shape constants, `OPEN.md`, latest validation note | spec sections cited by OPEN items | scheduler state, raw job reports |
| Scheduler | state block, stage table, `OPEN.md`, queue | job reports, diffs, spec sections | the whole decision history |
| Implementer / reviewer | agents entry doc, the brief | cited rule sections, relevant constants rows, relevant catalog rows, previous round's final message + gate output | other systems' specs, decision history, raw user quotes |

---
### 中文摘要
八种上下文失效方式；十一条规则（状态与日志分离、必读集合预算、决策出必读、有上限通道只用于唤醒、闭合循环、报告非证据、陈旧即错误、检查点即提交、压缩前重写后重读且有上限、主上下文综合阅读外包、上下文计价）；三种读者各自的入口。
