# H09 — Self-audit: the dashboard, the stop triggers, the five anti-patterns / 自检

## 1. The weekly dashboard (`templates/complexity_dashboard.csv`)
One row per week; complexity columns on the left, value columns on the right, read together.

| Complexity | Target | Value | Target |
|---|---|---|---|
| runtime systems / rule count | ≤ caps | core-feel bands met (%) | rising or stable |
| decisions per session (telemetry) | within shape constant | "would use again" (validation) | rising |
| strings total / glossary notes | ≤ table budgets | users who can describe the core in their words | rising |
| constants total / GATED unmeasured | GATED ≤ 5 | external pass rule | met |
| expired flags / unreplaced fallbacks | 0 past one stage | — | — |
| doc lines / lint errors / stale markers | lint 0 | — | — |
| tests ÷ rules | not falling | — | — |
| must-read set tokens | ≤ budget | — | — |
| RULE/SYSTEM changes without hypothesis | 0 | — | — |
| levers moved twice in one cycle | 0 | — | — |
| days since last non-owner validation | ≤ 3 | — | — |
| merges with unverified claims | 0 | — | — |
| implementer rounds that changed nothing | 0 | — | — |
| cost per merged feature | tracked | — | — |

**Rule:** complexity up while value flat for two consecutive weeks → stop adding content; run a consolidation window.

## 2. Stop triggers (when you notice yourself about to do one of these, stop and apply the rule)
1. Finalising a spec the same day the feedback arrived (`H04 §6`).
2. Answering a finding by adding a system (`H04 §4`).
3. Dispatching a job whose `done_when` cannot be observed in the job's environment (`H06 §3`).
4. Scheduling content before the next validation session is booked (`H03 §4`).
5. Titling a merge from a report instead of the diff (`H06 §9`).
6. Changing a shape constant without the dependency list open (`H04 §5`).
7. Letting a capped channel carry a decision (`H05 C4`).
8. Measuring before the method is written (`H07 §5`).

## 3. The five anti-patterns of agent-driven delivery
1. **Speed as the default virtue.** Throughput is not progress; the right pace is the one at which the humans who must judge the result can judge it.
2. **Additive bias.** The reflex to every finding is to add. Removal, merging and reverting must be first-class options.
3. **Premature finality.** Finalising feels like diligence; it is the absence of a settling period.
4. **Trusting one's own summaries.** Reports, titles, notes and bot numbers are compressions; act on the source.
5. **Reading "full auto" as "no human in the loop".** Autonomy is a shape, not an absence: the few calls only the owner can make still reach the owner, in a window, with a default.

## 4. The ritual
Weekly, 20 minutes: fill the dashboard row; list the stop triggers hit; list what was removed this week next to what was added; name the next consolidation window; publish the row where the owner reads it.

---
### 中文摘要
每周仪表盘（复杂度列与价值列并排，复杂度升而价值不升两周即停）；八个“停下来”的触发信号；代理驱动开发的五个反模式（速度即美德、加法偏好、过早定稿、相信自己的摘要、把全自动读成无人在环）；每周 20 分钟的仪式。
