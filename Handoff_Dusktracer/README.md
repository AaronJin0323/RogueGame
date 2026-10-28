# Handoff — Dusktracer / 曳光

This folder is a complete handoff kit for rebuilding the game from zero in a new agent session. It has **two independent series**:

| Series | Folder | Nature | Reusable for |
|---|---|---|---|
| **Gatework Harness v1.0** | `harness/` | the working method: document system, stage gates, change and context management, agent pipeline, quality gates, decision records, self-audit, templates | **any** software project built with AI agents |
| **Dusktracer game series** | `game/` | the name, the design-document plan, the retrospective and reflection of the previous project, and the previous project's documents, specs, text tables, tags, balance and playtest notes as reference | this game only |

Start with `game/G00_START_HERE.md` (it tells the new agent what to read, in what order, and what is binding vs reference). The kickoff prompt for the new agent is in its §9.

这个文件夹是在新的代理会话里从零重做游戏的完整交接包，分两个互相独立的系列：`harness/` 是与项目无关的工作方法（门槛工作法 v1.0，可用于任何 AI 代理参与的软件开发）；`game/` 是本游戏专属的内容（名字、文档计划、复盘与反思、以及前作的全部参考材料）。从 `game/G00_START_HERE.md` 开始；给新代理的启动提示词在它的 §9。

## Provenance
Assembled 2026-10-28 by the review session of the RogueHero project from `main` c7f832c9 and the review session's own documents (2026-10-24 → 10-26). Files under `game/reference/` are verbatim copies of the previous project's files at that commit, except `02_TechnicalDesign.md`, which is split into §1–16 (architecture) and §17 (decision-log history). `MANIFEST.md` lists every file with its origin path.

## Not included on purpose
Source code, binaries, content assets, images and audio of the previous project. The new project is a rebuild under a new architecture contract; the reference material is enough to re-derive what is worth keeping, and copying code would carry the old project's accretion with it.
