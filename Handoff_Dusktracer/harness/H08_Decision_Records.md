# H08 — Decision records and the owner window / 决策记录与负责人窗口

## 1. Decision files
`13_Decisions/D-nnn.md`, one per decision, never a section inside a spec (`templates/decision.md`):
Title · Date · Decider (owner / scheduler) · Context (observations, validation notes, metrics cited) · Options A/B (≤ 3) · Decision · Hypothesis line · Impact list (rule IDs, constants, files, tests, text) · Swap-out point (if temporary) · Status (open / applied / reverted, with reason).

Index: `13_Decisions/INDEX.md` — one line per decision, generated.

## 2. Who decides what
- **Owner:** pillars, shape constants, anything that changes what the user feels or is promised, scope valves, go/no-go at gates.
- **Scheduler:** engineering and tuning trade-offs within the rules; triage classes (owner confirms); dispatch order; reverts of unmet hypotheses.
- **Full-auto mandates do not remove the owner window.** A mandate says "decide and keep moving"; it does not say "stop showing me the two or three calls a day only I can make well". Under full auto the window still exists; it is simply never blocking — unanswered items default to the scheduler's recommendation after the window closes, and the default is recorded as such.

## 3. The daily owner window (30 minutes)
Inputs: `13_Decisions/OPEN.md` (≤ 3 questions, each ≤ 3 lines + a recommendation) and one build to use. Outputs: class confirmations, decisions, and the owner's verbatim observations. Everything else runs automatically. Items not raised in the window are not raised in chat.

## 4. OPEN.md format (`templates/open_decisions.md`)
Per item: ID, class (RULE / SYSTEM / SHAPE / VALVE / GO-NOGO), one-paragraph context with evidence, options, recommendation, default-if-unanswered, expiry. Cleared every window; expired items are applied as their default and logged.

## 5. The hypothesis line
`We believe <change> moves <metric> from <now> to <target> because <observation>; measured at <session/job>; reverted if not met.`
Required on every RULE and SYSTEM decision and on large TUNE changes. It is copied into the brief and into the test job that measures it. A decision without one is a preference, not a decision.

---
### 中文摘要
每条决策一个文件、固定字段；负责人与调度者的决策边界；全自动授权不取消每日窗口（窗口不阻塞，过期按推荐默认执行并记录）；每日 30 分钟窗口的输入与输出；OPEN.md 格式；假设行是 RULE/SYSTEM 决策的必填项。
