# H01 — Principles / 原则

Everything in the harness is a consequence of these eight statements. When two harness rules seem to conflict, the lower-numbered principle wins.

## P1. Documents are instructions for executors, not essays for readers
An agent can act on a rule only if it can cite it, test it and tell when it is violated. So every rule carries an ID (`R-<SYS>-nn`), every number carries a tag (`CORE` / `GATED(<data>)` / `MEASURED(<ref>)`), and every system names its acceptance tests (`T-<SYS>-nn`). Prose that cannot be cited is background, and is marked as such.

## P2. One topic, one source of truth
Rules live in the system specs, numbers in the constants file, content in the catalog, decisions in the decision folder. Documents **reference** each other; they never **restate** a number or a rule. A lint fails on a bare number outside the constants file and on any dangling reference. A revision replaces the old text in the same commit and files a decision record; it never adds "where they differ, X wins".

## P3. Shape before rules, rules before content, content before presentation
Shape constants (length, budgets, caps, decision counts) are decided first and locked at the first gate. Rules may only depend on shape; content only on rules; presentation only on content. A change to a lower layer carries a mandatory sweep of every dependent above it. This is the most expensive principle to violate late.

## P4. Humans are the critical path, and they are scheduled first
Agents compress calendar time by one to two orders of magnitude. The thing that does not compress is the human who must feel, judge or approve the result. Every stage therefore ends with a human exit test, those sessions are booked before content is, and the plan is expressed in gates rather than dates.

## P5. A change is a replacement, a tune or a revert — never a stack
A change that cannot name what it replaces, which constant it tunes, or the condition under which it is reverted is an application for a new system, and goes through the system budget. "Keep it and add a patch" is not an outcome.

## P6. Measure what you claim; verify at the source
Reports, merge titles, session notes and bot numbers are compressions of the state. Act on the state: read the diff before titling the merge, grep the merged tree for the claimed fix, define the measurement before dispatching it, and treat simulated numbers as bounds until calibrated against humans.

## P7. Context is a budget: every reader sees only what it needs, and it is current
State is rewritten; logs are appended. The must-read set has a token limit. Decisions live outside the must-read set. Loops are closed (the implementer's output reaches the reviewer). Stale context is treated as wrong context and is deleted or regenerated.

## P8. Gates precede features; removal is as cheap as addition
Build, compile-per-file, release-config, tests, text/loc audit, rendered smoke and packaged-input checks exist before the first feature and run identically locally and in the pipeline. Features are data rows, experiment flags expire, strings are prefixed per feature, tests are parameterised from the catalog — so deleting is one action, and the system budget can actually be enforced.

---

### 中文摘要
P1 文档是给执行者的指令；P2 一个主题一个真相源；P3 形状 → 规则 → 内容 → 表现；P4 人是关键路径，先排人；P5 改变只能是替换、调参或撤回；P6 先定义再测量、在源头核验；P7 上下文是预算，状态重写、日志追加、循环闭合；P8 门槛先于功能，删除要和添加一样便宜。
