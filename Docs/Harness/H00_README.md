# Gatework Harness v1.0 — a method for building software with AI agents
# 门槛工作法 v1.0 —— 用 AI 代理开发软件的方法集

> **What this is.** A project-agnostic set of working rules for teams where one or a few humans own the product and AI agents (planners, schedulers, implementers, reviewers) do most of the writing, coding and verification. It was distilled from a seven-day, 1 500-commit UE5 game project and from the failures that project paid for. Nothing in this folder mentions that game; the game lives in the sibling `game/` series.
> **这是什么。** 一套与具体项目无关的工作规则，适用于“少数人拥有产品、AI 代理（规划、调度、实现、评审）承担大部分写作、编码与验证”的团队。它从一个七天、1 500 次提交的 UE5 游戏项目及其付出代价的失败中提炼而来。本文件夹不涉及那个游戏；游戏内容在同级的 `game/` 系列里。

## Files / 文件

| File | What it governs | Read by |
|---|---|---|
| `H01_Principles.md` | The eight principles everything else follows from | everyone, once |
| `H02_Document_System.md` | The document set, the section contract, ID schemes, lint rules, writing order | planner, scheduler |
| `H03_Stage_Gate_Roadmap.md` | Planning by gates instead of calendar; stage template; scope valves; system budget | owner, scheduler |
| `H04_Change_Management.md` | How a change stays a change: triage classes, hypothesis line, layer rule, replace/tune/revert, consolidation | scheduler, owner |
| `H05_Context_Management.md` | What each reader sees when; state vs log; must-read budget; closing loops; staleness | scheduler, all agents |
| `H06_Agent_Pipeline.md` | Roles, topology, file ownership, briefs with tolerances, review inputs, cost controls, environment hygiene | scheduler, implementers, reviewers |
| `H07_Quality_Gates.md` | Merge gates, test taxonomy, day-1 instrumentation, human validation protocol, measurement discipline | scheduler, QA, implementers |
| `H08_Decision_Records.md` | Decision files, the OPEN list, the daily owner window | owner, scheduler |
| `H09_Self_Audit.md` | The weekly dashboard, stop triggers, the five anti-patterns | scheduler, owner |
| `templates/` | Fill-in files: section contract, brief, decision, stage, validation note, constants, logs, state block | all |

## Adopting the harness on day 0 / 第 0 天如何采用

1. Copy `harness/` into the new repository as `Docs/Harness/` (read-only for agents; changes go through a decision record).
2. Create the document skeleton from `H02 §2` and the `templates/`. Fill `00_Vision` and the shape-constant table first (`H02 §5`).
3. Write `AGENTS.md`/`CLAUDE.md` as the agents' entry point: the non-negotiables as references to rule IDs, the known-traps list, the brief tolerances (`H06 §5`).
4. Stand up every gate in `H07 §1` **before the first feature**. A gate learned by breaking the main branch is a failure of the harness, not of the agent.
5. Create the state block (`templates/state_block.md`) and the OPEN decisions file. Schedule the first three human validation sessions on the calendar before scheduling any content.
6. Start the weekly self-audit (`H09`) in week 1, when the numbers are still small.

## Reading order by role / 按角色的阅读顺序

- **Product owner (human):** H01 → H03 → H08 → H09. Everything else is the scheduler's.
- **Scheduler agent:** all of it, once; afterwards only the state block, `OPEN.md`, the stage table and the dashboard.
- **Implementing / reviewing agent:** `AGENTS.md` and the brief. The harness is cited by rule ID in the brief; the agent reads only the cited sections.

## 一页总览（中文）

- **文档是给执行者看的。** 每条规则有 ID，每个数字有标签（CORE / GATED / MEASURED），每个系统有验收测试名。一个主题只有一个真相源，文档间只引用不重述，lint 守门。
- **先定形状，再定规则，再定内容，最后定表现。** 形状常数（时长、预算、上限、决策数）在第一阶段锁定，所有后续规格引用它。
- **按门槛计划，不按日历。** 每阶段有进入条件、含真人的退出测试、以及“退出前不得开始”的清单。关键路径是必须去感受产品的人，不是代理。
- **改变只有三种合法结果：替换、调参、撤回。** 没有“叠加”。每条反馈先分类、写假设行、在能解释它的最低层修。新系统必须说明退役什么。
- **上下文 = 每个读者在每个时刻只看到它需要的、而且是最新的那一块。** 状态重写、日志追加、必读集合有预算、循环必须闭合、报告不是证据。
- **代理并行、合并串行、门槛先于功能。** 每个文件区域一个负责人；简报带容差；运行时观察类工作拆三段；成本计量。
- **每周自检。** 复杂度与价值指标并排；复杂度升而价值不升就停止加内容。
