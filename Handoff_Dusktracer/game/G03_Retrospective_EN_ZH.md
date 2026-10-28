# RogueHero — Retrospective and a from-zero plan / 复盘与从零重做的计划

> Written 2026-10-24 by the review session, read-only, against `main` 5ac48d81. Sources: the five design docs, TDD §17 (576 entries), `Docs/Design/*`, all `Docs/Playtests/*`, `Docs/Balance/*`, `Saved/Cowork/log.md` + 12 job reports, the scheduler's dispatch log and the full git history (1 497 commits, 2026-10-18 → 10-24).
> 本文由评审会话于 2026-10-24 只读写成，基准 `main` 5ac48d81。依据：五份设计文档、TDD §17（576 条决策）、`Docs/Design/*`、全部 `Docs/Playtests/*`、`Docs/Balance/*`、`Saved/Cowork/log.md` 与 12 份任务报告、调度日志，以及完整 git 历史（7 天 1 497 次提交）。

Each section has the same shape: **What happened** (evidence) → **What it cost** → **From zero, I would…**. The last section turns it into a gate-based roadmap.
每节结构相同：**发生了什么**（证据）→ **代价**→ **若从零开始，我会……**。最后一节汇总为按“门槛”而非日历推进的路线图。

---

## 0. The one-paragraph verdict / 一段话结论

**EN.** The project proved that a documented design plus gated AI agents can build an enormous amount of a UE5 game in a week: a 6-week prototype roadmap was executed in about 48 hours, and v0.2 / v0.3 followed within four days. The cost was that almost every *design* truth was discovered after the code existed: the designer's three deepest findings (blessings don't drive power — 16 % vs ≥ 60 %; L16 must be a qualitative transformation; mechanics combining is the source of 爽) each triggered a redesign of systems that had already shipped, and the run length itself changed (20 → 15 min) after three arenas were authored against 20. The engineering lessons are smaller and cheaper (unity builds, union-merged localization, headless-only tests), and the process lessons are about rhythm: the game moved faster than the people who had to feel it. A redo should spend the first stage buying *information* (a grey-box slice, three humans, a power-budget tool) before buying content, and should let scope valves actually close.

**中文。** 本项目证明：一套写好的设计文档 + 带门槛的 AI 代理，一周内就能做出极大体量的 UE5 游戏——6 周的原型路线图约 48 小时执行完毕，v0.2 / v0.3 在随后四天内完成。代价是几乎所有**设计**上的真相都是在代码已经存在之后才发现的：设计师三条最深的反馈（祝福不驱动强度——16 % 对 ≥ 60 % 的目标；L16 必须是质变；爽来自机制叠加）各自引发了对已上线系统的重做，连局长本身也在三张地图按 20 分钟写完之后才改成 15 分钟。工程教训更小也更便宜（unity 编译、union 合并的本地化、只跑无渲染的测试），流程教训关乎节奏：游戏跑得比必须去“感受”它的人更快。重做时第一阶段应先买**信息**（灰盒切片、三位真人、强度预算工具），再买内容，并且让缩减阀真的能关上。

---

## 1. Gameplay — the core feel / 核心手感

### What happened / 发生了什么
- GDD §3.5 "爽 with teeth" was correct and measurable from day 1: `RH.Stats` wrote kills/min, swarm TTK, live bullets, dashes/min and damage/min per minute, and the bands were used as acceptance tests.
- But the first data-rich human playtest (2026-10-19, `Docs/Playtests/2026-10-19_human.md`) came after Weeks 2–4 of content: L20 at 5:30 (XP 3× too fast), 7–57 live bullets against a target of 2 000, 0–2 dashes/min, bored after 15:00.
- Pressure then oscillated for three days: too low (P1, P3 "enemies never get close") → drop-in threats → too much (P4 died at 7:04 with 61 %/min; P5 "too dense") → aim tokens + pile-up caps → the Outfall unwinnable at Heat 0 (P6). Each fix was tuned against a bot that dodges telegraphs humans miss (`Docs/Balance/playtest5_verification.md`).
- The GDD's live-bullet target was renegotiated from 150/800/2 000 to 5/40/100 (R15) once a prototype existed. Swarm TTK ≤ 0.3 s turned out to be the real metric.
- Run length 20 → 15 min (D-71, 2026-10-23) after three timelines, the identity showcase gate (measured "at 18:00"), progression v2 and L24 had all assumed 20.

- GDD §3.5“爽 with teeth”从第一天起就是正确且可测的：`RH.Stats` 每分钟写出击杀/分、小怪 TTK、存活子弹、冲刺/分、承伤/分，区间直接当验收标准。
- 但第一次有数据的真人试玩（10-19）发生在第 2–4 周内容堆完之后：5:30 到 20 级（经验快 3 倍）、存活子弹 7–57 对目标 2 000、冲刺 0–2/分、15 分钟后无聊。
- 之后压力曲线来回摆了三天：太低（P1、P3“敌人根本近不了身”）→ 空投威胁 → 太高（P4 在 7:04 阵亡，承伤 61 %/分；P5“弹幕太密”）→ 瞄准令牌 + 堆叠上限 → 外泄区在热度 0 打不赢（P6）。每次修正都对着一个比人更会躲预警的机器人调。
- GDD 的存活子弹目标在原型出现后从 150/800/2 000 改成 5/40/100（R15）；真正的指标是小怪 TTK ≤ 0.3 s。
- 局长 20 → 15 分钟（D-71，10-23），而此时三张时间线、流派展示门槛（“在 18:00 测”）、进阶 v2 和 L24 都已按 20 分钟写好。

### What it cost / 代价
Four XP-curve rewrites, five pressure passes, a full timeline re-author ×3 arenas, and specs that still contradict each other on run length (GDD §3.5 "minute 18", Refusal 15→22, Outfall/Sump at 20:00).
经验曲线改了四次、压力调了五轮、三张地图的时间线全部重写，且各规格书至今在局长上互相矛盾。

### From zero / 从零开始
1. **Buy feel before content.** Stage 1 is a grey-box slice: 1 arena, 3 Keepers, 5 weapons, ~10 modifiers, 4 roles (Swarm, Tank, Rusher, Turret) + 1 boss, the 5-card offer. Nothing else until three humans (not the designer) have played it on pad and KBM and the §3.5 bands hold for a steered build.
2. **Lock the "shape constants" at the fun gate, in one table, owned by one doc:** run length, L8/L16 minutes, decision count per run, pool size, bullet cap, enemy cap. Every later spec cites the table instead of restating numbers. Changing a constant is a decision with a grep-list of dependents.
3. **Calibrate the bot to humans before trusting it:** a "misses X % of telegraphs" skill knob, and bot damage treated as a lower bound. Human-only metrics (boredom, "did you notice", readability) are never delegated to the bot.
4. **Pressure is designed as a curve, not patched:** the harvest/threat/breath rhythm with drop-ins and a kill-horizon rule (GDD §3.5 + `pressure_proposal.md`) belongs in the Stage-1 director, not in week-6 proposals.

1. **先买手感再买内容。** 第一阶段是灰盒切片：1 张地图、3 位司者、5 把武器、约 10 个改装、4 种角色（Swarm/Tank/Rusher/Turret）+ 1 个 Boss、5 卡选择。三位非设计师的真人用手柄和键鼠都玩过、§3.5 区间对引导型构筑成立之前，不加任何东西。
2. **在趣味门槛处把“形状常数”锁进一张表、归一份文档管：** 局长、L8/L16 分钟、每局决策次数、卡池规模、子弹上限、敌人上限。此后所有规格书引用这张表而非重抄数字；改一个常数就是一次带依赖清单的决策。
3. **先让机器人对齐真人再信它：** 加一个“漏掉 X % 预警”的技巧旋钮，机器人承伤只当下界。无聊感、“注意到了吗”、可读性这类只有人能判断的指标永远不交给机器人。
4. **压力按曲线设计而不是打补丁：** 收割/威胁/喘息节奏 + 空投 + 击杀地平线规则在第一阶段的导演里就要有，而不是第六周的提案。

---

## 2. Mechanics design — the power budget / 机制设计——强度预算

### What happened / 发生了什么
- v0.1 shipped blessings as stat modifiers; weapons × modifiers carried the power. The designer felt it at playtest 2 ("weapons-with-modifiers feel isolated from the Keepers"), the ablation proved it on 10-20 (`Docs/Balance/power_split.md`: blessings 16–19 % of the L16 gain vs ≥ 60 %; a second weapon ×2.3; Radiance modifiers ×10.7).
- Three redesigns followed in 36 hours, each superseding the previous one: *imbue* (10-19) → *build depth* with branches, transformations, duo reactions (10-20 morning) → *Keeper identity* with 20 archetypes and a behaviour-flag layer (10-20, superseding build-depth branches within hours) → *archetype balance pass* R2–R31 (10-21) → *Intercession variants* (100 combinations) and *progression v2* after playtest 6.
- Modifier compatibility was done three times (`Capability.*` tags → build_depth §3 matrix → designer bug fix). The GDD done-when "all 20 modifiers on all 10 weapons" had been met and then became wrong by design.
- Each layer created the next problem: more cards → more hands → pool exhausted at L22+ → reserve cards; more mechanics → confusing text → 58 keyword hover-notes retrofitted into every string table.
- Scope valves were never pulled; everything grew: 6 fusions → 23 transformations, 6 roles → 11, 15 Discoveries → ~40, 30 blessings → 72.

- v0.1 的祝福是数值加成，强度全在武器×改装上。设计师在第 2 次试玩就感觉到了，10-20 的消融实验证实（祝福占 L16 增益 16–19 %，目标 ≥ 60 %；第二把武器 ×2.3；司耀的改装 ×10.7）。
- 36 小时内接连三次重做、层层覆盖：附魔 → 构筑深度（分支、蜕变、双司反应）→ 司者身份（20 个流派 + 行为标志层，几小时内取代了构筑深度的分支）→ 流派平衡 R2–R31 → 试玩 6 之后又加神临变体（100 种组合）与进阶 v2。
- 改装兼容性做了三遍。GDD 里“20 个改装对 10 把武器全部可用”的验收先达成、后被设计推翻。
- 每一层都制造下一个问题：更多卡 → 更多手牌 → L22+ 卡池耗尽 → 备用卡；更多机制 → 文本难懂 → 58 个关键词悬浮注释回填到所有字符串表。
- 缩减阀一个没拉，所有东西都在长：6 融合 → 23 蜕变，6 角色 → 11，15 发现 → 约 40，30 祝福 → 72。

### From zero / 从零开始
1. **Design the power budget first, as one number set with a tool.** Day-0 deliverable: `power_split.py`-style ablation on a refilled field plus the target split (e.g. Keeper ≥ 60 % / weapons ≤ 25 % / modifiers ≤ 15 % at L16). Every blessing, modifier and weapon is authored against that budget, not discovered against it.
2. **Keeper identity is the core, not an upgrade.** The behaviour-flag layer (blessing → weapon behaviour), archetype keys + amplifiers, Load factors and imbue are Stage-1 systems for 3 Keepers. Stat-only blessings never exist.
3. **One vocabulary for "Keeper expression":** tiers, imbue, transformation, Intercession variant, duo reaction are designed as one table per Keeper from the start, so the L16 transformation and the Intercession variant are rows, not later specs.
4. **Capability tags on weapons and modifiers from day 0** (`Cap.Continuous`, melee/orbit/projectile…), the 20 × 10 matrix generated from them, and a test that a modifier never appears on a weapon it cannot express.
5. **A system budget with a retirement rule:** a new mechanic must name the mechanic it merges with or retires. "Never cut" stays a 6-item list; valves get a trigger (a date or a metric) and an owner who must say why one did not close.
6. **Decide pool size × hands × decisions × run length together** (the pillar-4 "no menus except the pick" promise), before any second arena.

1. **先设计强度预算：一组数字 + 一个工具。** 第 0 天交付：补满场地上的消融脚本 + 目标占比（如 L16 时司者 ≥ 60 % / 武器 ≤ 25 % / 改装 ≤ 15 %）。所有祝福、改装、武器按预算编写，而不是事后被预算揭穿。
2. **司者身份是核心而非升级包。** 行为标志层、流派钥匙 + 放大器、武器负载、附魔，都是第一阶段 3 位司者的系统；纯数值祝福从不存在。
3. **“司者表达”一套词汇：** 阶、附魔、蜕变、神临变体、双司反应从一开始就是每位司者一张表里的几行，而不是后来的几份规格书。
4. **武器与改装第 0 天就带能力标签**，20 × 10 矩阵由标签生成，并有测试保证改装不会出现在无法表达它的武器上。
5. **系统预算 + 退役规则：** 新机制必须说明合并或退役哪个旧机制。“绝不砍”维持 6 条；每个缩减阀都有触发条件（日期或指标）和一个必须解释“为什么没关”的负责人。
6. **卡池规模 × 手牌次数 × 决策数 × 局长一起定**，然后才能有第二张地图。

---

## 3. Game design process and documents / 游戏设计流程与文档

### What happened / 发生了什么
- Docs-first worked: five docs, binding sections, done-when lines, D-n decisions, owner tables. CLAUDE.md "docs win over code" held the project together across 9 parallel sessions.
- But the docs became a palimpsest: GDD §5.4/§5.6 keep superseded text with "where they differ, the spec wins"; `02_TechnicalDesign.md` is in 564 commits (38 % of all), §17 has 29 duplicate entries and out-of-order dates; specs titled FINAL were "how they are implemented" (`keeper_identity_spec §12.0`: "Committed API — these names are binding").
- Full-game specs (v0.2, v0.3, v0.4, 1.0) were all written on 10-20, before any v0.1 external data existed. Their GATED numbers are still gated; the external playtest kit (`EXTERNAL_playtest.md`) has never been run and `v0.1_findings.md` does not exist.
- The decision cadence inverted: under the full-auto mandate, D-1…D-37 were "decided as recommended" by the scheduler, playtest-6 specs were finalised with no designer input, and the designer's reactions arrived as playtest notes after the systems shipped.

- 文档先行是对的：五份文档、约束性章节、验收行、D-n 决策、负责人表；“文档胜过代码”让 9 个并行会话没有散架。
- 但文档成了层层覆写的羊皮纸：GDD §5.4/§5.6 保留着被取代的文本并注明“以规格书为准”；TDD 出现在 38 % 的提交里，§17 有 29 条重复、日期乱序；标为 FINAL 的规格书其实是“按实现写的”。
- v0.2 / v0.3 / v0.4 / 1.0 的规格书全在 10-20 一天写完，当时没有任何 v0.1 外部数据。GATED 的数字至今还在 gate 里；外部试玩清单从未执行，`v0.1_findings.md` 不存在。
- 决策节奏倒置：全自动授权下 D-1…D-37 由调度者“按建议通过”，试玩 6 的规格书在没有设计师输入的情况下定稿，设计师的反应变成系统上线后的试玩笔记。

### From zero / 从零开始
1. **One source of truth per topic, no "spec wins" notes.** When a spec amends the GDD, the GDD paragraph is replaced in the same commit; the old text goes to a dated `Docs/Decisions/D-nnn.md` file. The decision log is a folder of small files (merge-safe, one writer per entry), not a 250k-token section.
2. **Write the next version's spec only after the current version's data exists.** v0.2 structure may be *sketched* (one page), but numbered specs wait for `v0.1_findings.md`.
3. **A daily 30-minute design window with the designer**, even in full-auto: the scheduler brings ≤ 3 A/B questions and one build to play; the designer answers in the window; everything else is auto. Design findings arrive before the system ships, not after.
4. **Spec hygiene gates:** a spec is not FINAL until its numbers cite a measurement; a doc lint checks that the shape-constant table is the only place run length / level minutes appear.
5. **Keep what worked:** the five-doc split, binding §-references in briefs, Path/role keys, done-when lines, the §17 habit (in its new file form), the "stop and say so" rule for conflicts.

1. **每个主题只有一个真相，不写“以规格书为准”。** 规格书修订 GDD 时同一提交替换原段落，旧文进入带日期的 `Docs/Decisions/D-nnn.md`。决策日志是一个小文件文件夹（合并安全、一条一作者），不是 25 万 token 的章节。
2. **只有当前版本的数据存在后才写下一版规格。** v0.2 结构可以先画一页草图，带编号的规格书等 `v0.1_findings.md`。
3. **每天 30 分钟设计师窗口**，全自动模式下也如此：调度者带 ≤ 3 个 A/B 问题和一个可玩版本，设计师在窗口内回答，其他全部自动。设计发现发生在系统上线前而非之后。
4. **规格卫生门槛：** 数字未引用测量之前不得标 FINAL；文档 lint 检查局长 / 升级分钟只出现在形状常数表中。
5. **保留有效做法：** 五文档拆分、简报里的约束性章节引用、Path/角色键、验收行、§17 习惯（改为文件形式）、冲突时“停下并说明”的规则。

---

## 4. Art and readability / 美术与可读性

### What happened / 发生了什么
- The readability contract (Art §3: cool player fire, warm/magenta danger, red laser telegraphs, 3:1 contrast, no motion blur) was right and caught real failures: enemy fire unreadable at 1 300+ own bullets (P3), Slagger shells (P6), hero point light (P4).
- On 2026-10-19 the designer made the art book the v0.1 bar for everything and withdrew valve 7. G1–G18 (5–7 solo-weeks) were done in ~30 hours by 3–4 parallel sessions, procedural/CC0 only, with GPU-quiet windows to measure. It worked, and the game looks like the book.
- But art landed *before* the core-feel findings: imbue colour rule reversed (Keeper colour on tails, 10-20) after tracers were built; stone-tablet cards → tech cards for weapons/modifiers after P5; rarity reveal redone; XP bar lost in the HUD re-layout and restored; fonts flip-flopped three times in 3 hours; environment meshes found inside-out, resetting contrast baselines.
- Rendering is never tested: every gate runs `-nullrhi`; a magenta card-text regression reached main and needed an "URGENT" fix; playtest item #4 (enemies vanish after a card pick) cannot be reproduced headless.

- 可读性契约（Art §3）是对的，且抓到了真实问题：1 300+ 己方子弹时敌弹不可读、渣炮炮弹、主角点光源。
- 10-19 设计师把美术集定为 v0.1 全部内容的标准并撤回第 7 阀。G1–G18（5–7 个独立周）由 3–4 个并行会话约 30 小时完成，只用程序化/CC0 资产，靠 GPU 静默窗口测量。它成功了，游戏看起来就是美术集。
- 但美术落地在核心手感结论**之前**：拖尾建好后才反转附魔配色规则；P5 之后石板卡改成科技卡；稀有度揭示重做；HUD 重排丢了经验条又补回；字体三小时内翻转三次；环境网格发现内外翻转，对比度基线重置。
- 渲染从未被测试：所有门槛都跑 `-nullrhi`；洋红卡牌文字回归进了主干要“紧急”修；试玩项 #4（选卡后敌人消失）无法在无渲染下复现。

### From zero / 从零开始
1. **Readability contract on grey-box at Stage 0; art-book fidelity after the fun gate.** The contract (palette tokens, contrast sheet, telegraph language, density LOD) is code and tests before any mesh.
2. **Decide the colour rules once, with the designer, before the first tracer:** player core, Keeper tail colour, enemy fire, elite status light, rarity ladder — a one-page palette the art sessions cannot reinterpret.
3. **A rendered smoke gate from day 0:** `-RenderOffscreen` screenshots at fixed seeds/minutes, compared to golden images with a tolerance, plus a contrast check script. This catches magenta text, vanished enemies and inside-out meshes before merge.
4. **Keep:** procedural/CC0/OFL-only sourcing with the licence log, hard-surface rigid-part + VAT enemies (no skeletal pipeline needed), GPU-quiet measurement windows (made a semaphore, not a chat message).
5. **UI identity per card kind is a Stage-1 requirement** (P1 "cards too pale", P5 tech vs stone): frame, icon, label and rarity read separately from the first level-up screen.

1. **第 0 阶段在灰盒上落实可读性契约；美术集精度在趣味门槛之后。** 契约（调色板令牌、对比度表、预警语言、密度 LOD）先是代码和测试，再是网格。
2. **第一条拖尾之前和设计师一次性定好配色规则：** 己方弹芯、司者拖尾色、敌弹、精英状态灯、稀有度阶梯——一页调色板，美术会话不得重新诠释。
3. **第 0 天就有渲染烟雾门槛：** 固定种子/分钟的离屏截图对比金图（带容差）+ 对比度脚本。洋红文字、消失的敌人、翻转的网格在合并前被抓住。
4. **保留：** 仅程序化/CC0/OFL 资产 + 许可日志、硬表面刚体 + VAT 敌人（无需骨骼管线）、GPU 静默测量窗口（做成信号量而不是聊天消息）。
5. **按卡种区分的 UI 身份是第一阶段需求：** 边框、图标、标签、稀有度从第一张升级界面起就分开可读。

---

## 5. Narrative and localization / 叙事与本地化

### What happened / 发生了什么
- Right from day 0: Path/role keys with fiction only in string tables (the Greek re-key took 2 minutes, leftovers 5 hours); en + zh-Hans from the first table; Layer gating with `RH.Codex.ShowLayer`; length caps; `TextAudit.py` rules A–F.
- Wrong at first: Chinese was translated from English (结账 for "Closing"); P3 found *all* zh descriptive text hard to read → a full plain-language rewrite that reverted the previous clarity pass; P6 found terms confusing → keyword notes retrofitted into every table.
- Localization was the single biggest merge-pain source: ~60 of 68 conflicted merges were on `Game.po`/`.locres`; union merges duplicated msgstr lines; 65 commits are literally "chore: recompile localization"; `zh_overrides.csv` + `apply_zh.py` were invented on day 2 as the real source of truth. zh became the bottleneck for merges (Bids 33 rows, meta 45, transforms 38 held for days).

- 第 0 天就做对的：Path/角色键、虚构名只在字符串表（希腊名重键 2 分钟，残留清了 5 小时）；首张表就是双语；`RH.Codex.ShowLayer` 分层审计；长度上限；文本审计 A–F。
- 一开始做错的：中文是从英文翻的（“结账”）；P3 发现**全部**中文描述难懂 → 整体白话重写、推翻了上一次的“澄清”；P6 发现术语难懂 → 关键词注释回填所有表。
- 本地化是最大的合并痛点：68 次冲突合并约 60 次在 `Game.po`/`.locres`；union 合并复制 msgstr 行；65 次提交字面就是“重编译本地化”；第 2 天才发明 `zh_overrides.csv` + `apply_zh.py` 作为真相源。中文成了合并瓶颈（33/45/38 行各卡数天）。

### From zero / 从零开始
1. **Localization pipeline on day 0, designed for merging:** one reviewed CSV per table is the source of truth (en + zh columns + context + glossary ids); `.po`/`.locres` are generated artefacts, ignored by git or regenerated by the gate, never union-merged.
2. **Context-first Chinese from the first string**, with `zh_naming.md` / `zh_glossary.md` written *before* the first card: the glossary (and the keyword-note markup) exists from day 0, because a build game will have terms.
3. **zh is authored by the owning session in the same commit** (provisional allowed), reviewed by a loc pass later; a string without zh fails the gate but never blocks a merge queue for days.
4. **Keep:** Path/role keys, Layer gating, length caps, ≤ 90-string v0.1 narrative scope, "the word god never appears".

1. **第 0 天就建为合并而设计的本地化管线：** 每张表一份审校过的 CSV 是真相（en + zh + 语境 + 术语 id）；`.po`/`.locres` 是生成物，git 忽略或由门槛重生成，永不 union 合并。
2. **第一条字符串起就是“从语境出发”的中文**，`zh_naming.md` / `zh_glossary.md` 在第一张卡之前写好：术语表和关键词注释标记第 0 天就存在，因为构筑游戏必然有术语。
3. **中文由负责该功能的会话在同一提交里写**（允许暂定），稍后统一审校；没有中文的字符串门槛不过，但绝不让合并队列卡几天。
4. **保留：** Path/角色键、分层门控、长度上限、v0.1 约 90 条的叙事范围、“不出现‘神’字”。

---

## 6. Software design / 软件设计

### What happened / 发生了什么
- The TDD architecture held: SoA bullets at fixed 60 Hz with a spatial hash, SoA enemies on ISM + VAT, no GAS, `FRHStatSet` + `URHEffect`, data assets + tags, seeded `FRandomStream`. Budgets were met on the 4080 at the median (GPU 5.6 ms Epic with all art, ≤ 3.9 ms High).
- Fallbacks that were never swapped: bullets are ISM (Niagara array DI never built; a Niagara VFX path lost the A/B by 0.14 ms); 0 `WBP_` assets, 0 `ST_` assets, 0 `SK_` meshes, code-drawn glyphs. The headless-authorable path *was* the architecture.
- Retrofits that cost real time: per-player run state / effect managers / owner-tagged events for co-op (S2a–e) after three arenas of singletons; one RNG stream per system (spawns shared the combat stream, bosses seeded from world time) found by the daily-seed and golden tests; `SaveVersion` never written to disk until the validator was added; capability tags as data done three times.
- Only a 4080 was ever measured; 4070/3060 are proxies (×2.5–3.2). Game-thread p95 reached 8.9–9.9 ms (budget 8) after the behaviours pass and is still the open risk.

- TDD 架构站住了：SoA 子弹 60 Hz 定步 + 空间哈希、SoA 敌人 ISM + VAT、无 GAS、`FRHStatSet` + `URHEffect`、数据资产 + 标签、种子随机流。4080 上中位数达标。
- 从未替换的回退：子弹是 ISM（Niagara 阵列 DI 从未建成；Niagara VFX 路径 A/B 输 0.14 ms）；0 个 `WBP_`、0 个 `ST_`、0 个 `SK_`，字形代码绘制。“可无头编写”其实就是真正的架构。
- 代价真实的回填：合作模式所需的每玩家运行状态 / 效果管理器 / 带所有者的事件（S2a–e）在三张地图的单例之后才做；每系统一条随机流（刷怪共用战斗流、Boss 用世界时间播种）靠日常种子和金测试才发现；存档版本号直到加校验器才真正写盘；能力标签做了三遍。
- 只测过 4080；4070/3060 是代理系数。行为层之后游戏线程 p95 达 8.9–9.9 ms（预算 8），仍是未解风险。

### From zero / 从零开始
1. **Write the architecture for what the agents can author:** code-built UI, code/py-generated materials and meshes, a C++ editor commandlet for any Niagara, CSV string tables. Do not plan `WBP_`/`NS_`/`SK_` swap-outs that nobody will make; log them only if a real owner exists.
2. **Day-0 invariants that are cheap early and expensive late:** per-player state and owner-tagged events (co-op was in the plan); one `FRandomStream` per system, all from the run seed; the whole sim (hero included) on the fixed step; a save version stamped and validated from the first save; capability tags as data; stats/effects event-batched.
3. **Keep ISM bullets as the design, not the fallback;** write the Niagara option as a measured future A/B with a threshold.
4. **Performance:** an agreed proxy factor *or* real 3060 hardware named in §14 on day 0; a p95 budget beside the median; a GPU measurement semaphore that any launcher must take.
5. **Keep:** no GAS, SoA subsystems, data assets + tags with C++ factory fallbacks named as such, seeded runs, RH.* console commands, RH.Bot as a first-class citizen.

1. **按代理能编写的方式写架构：** 代码构建 UI、代码/脚本生成材质与网格、任何 Niagara 用 C++ 编辑器命令行、CSV 字符串表。不要规划没人会做的 `WBP_`/`NS_`/`SK_` 替换；只有存在真实负责人时才记录。
2. **早做便宜、晚做昂贵的第 0 天不变量：** 每玩家状态与带所有者的事件（合作模式本就在计划里）；每系统一条随机流且全由局种子派生；整个模拟（含主角）走定步；首个存档起就写入并校验版本号；能力标签即数据；属性/效果事件批处理。
3. **把 ISM 子弹当设计而非回退；** Niagara 作为带阈值的未来 A/B 项写下。
4. **性能：** 第 0 天在 §14 写明代理系数或真实 3060 硬件；中位数旁边加 p95 预算；任何启动器都必须先拿 GPU 测量信号量。
5. **保留：** 无 GAS、SoA 子系统、数据资产 + 标签 + 命名清楚的 C++ 工厂回退、种子局、RH.* 控制台命令、机器人一等公民。

---

## 7. Development process and the AI pipeline / 开发流程与 AI 管线

### What happened / 发生了什么
- **Era 1 (10-18 → 10-21): up to 9 Claude sessions in worktrees, one owner per file area, a scheduler merging by hand.** 1 294 commits, 529 merges, 68 conflicted merges (36 on 10-20 alone), 101 `fix:` commits. Main broke after merges ≥ 6 times from unity-build name collisions and hidden missing includes; the first non-unity gate was fake; a Shipping compile break was found only at packaging; 84 duplicate commits across worktrees; a 10-message cap repeatedly left instructions undelivered; two PowerShell wait-loop bugs ran two merges at once.
- **Era 2 (10-22 →): Codex implements serially, Claude briefs and reviews, gates before merge** (build with warnings-as-errors, non-unity single-file, localization, text audit, ≥ 500 tests). 91 change jobs, 68 % pass, ≈ $1.24 Claude per job (Codex unmetered), ~16 min per job; `fix:` commits fell to ~12 and conflicts to ~0. New failure modes: 46 Codex rounds "changed nothing" because of the strict stop rule (c-s7, c-s9, c1 burned 2–3 rounds each); the runner never fed Codex's final message to the reviewer; C4458 shadowing ×7; `allowed_files` too narrow; runtime/visual jobs (enemies vanish, gamepad in Shipping) impossible headless; GPU guard blocked jobs while the user played.
- Tests: ~670 automation tests, all `-nullrhi`; rendering, real gamepad and feel untested; some tests "pass trivially".
- Throughput vs stability: Era 1 ≈ 350 commits/half-day with constant breakage; Era 2 ≈ 60 with a clean main. Neither era had a release branch until v0.1b.

- **第一阶段（10-18 → 10-21）：最多 9 个 Claude 会话各占工作树，一个文件区域一个负责人，调度者手动合并。** 1 294 次提交、529 次合并、68 次冲突合并（10-20 一天 36 次）、101 次 `fix:`。主干在合并后至少坏了 6 次（unity 名字冲突、被隐藏的缺失头文件）；第一个非 unity 门槛是假的；Shipping 编译错误到打包才发现；跨工作树 84 次重复提交；10 条消息上限反复让指令送不到；两个 PowerShell 等待循环 bug 让两次合并同时跑。
- **第二阶段（10-22 →）：Codex 串行实现，Claude 写简报与评审，合并前过门槛**（警告即错误、非 unity 单文件、本地化、文本审计、≥ 500 测试）。91 个改动任务，通过率 68 %，每任务约 $1.24（仅 Claude，Codex 未计量），约 16 分钟；`fix:` 降到约 12 次、冲突近 0。新失败模式：46 轮 Codex“什么都没改”，源于过严的停止规则；运行器从未把 Codex 的最终消息喂给评审；C4458 遮蔽 ×7；`allowed_files` 过窄；运行时/视觉类任务无头做不了；用户在玩时 GPU 守卫阻塞任务。
- 测试：约 670 个自动化测试，全部 `-nullrhi`；渲染、真实手柄、手感无测试；部分测试“平凡通过”。
- 吞吐 vs 稳定：第一阶段每半天约 350 次提交但主干不断坏；第二阶段约 60 次但主干干净。直到 v0.1b 才有发布分支。

### From zero / 从零开始
1. **Gates before the first feature, identical locally and in the runner:** build with warnings-as-errors; `-SingleFile` include check on changed files; Shipping compile; headless suite with crash scan and a test-count floor; text/loc audit; **a rendered smoke with golden screenshots**; a packaged-build pad input check. A merge runs the same gates on the merged tree.
2. **Parallel agents, serial merges, one owner per file area** (kept from Era 1), plus: a release branch from the first external build; worktrees pruned after merge; per-worktree DDC; PID-scoped process sweeps; a GPU/interactive-session lock as a file semaphore; adaptive unity off from day 0.
3. **Brief template with tolerances:** "missing wiring inside allowed files → build it; missing structure outside scope → stop as `blocked` after round 1". `done_when` names the test and the assertion. A known-traps list in `AGENTS.md` (C4458 on `Slot`/`World`/`Name`, forward declarations, file-scope `using`, tests inside `WITH_DEV_AUTOMATION_TESTS`).
4. **Runtime-observation work is three jobs, never one:** instrument → graded test job that runs the game → fix. Headless agents never get "find why X vanishes".
5. **Feed the implementer's final message to the reviewer; an empty diff is a stop report.** Start with one fix round and escalate only when gates are close.
6. **Cost and throughput as first-class metrics:** per-job $ cap, Codex tokens metered too, a weekly line "features merged per $100".
7. **Keep the scheduler role but give it fewer humans-in-chat limits:** decision batches via files in a shared folder (no 10-message cap), a dispatch log that is rewritten at every compaction (this worked), and a hard rule "grep the merged code before titling a merge" (lifesteal lesson).

1. **第一个功能之前先有门槛，本地与运行器完全一致：** 警告即错误；改动文件的 `-SingleFile` 头文件检查；Shipping 编译；带崩溃扫描与数量下限的无头测试；文本/本地化审计；**带金图的渲染烟雾测试**；打包版手柄输入检查。合并对合并后的树跑同样的门槛。
2. **代理并行、合并串行、每个文件区域一个负责人**（沿用第一阶段），再加：首个对外版本起就有发布分支；合并后清理工作树；每工作树独立 DDC；按 PID 清进程；GPU/交互会话锁做成文件信号量；第 0 天关闭自适应 unity。
3. **带容差的简报模板：**“允许文件内缺少接线 → 自己补；范围外缺少结构 → 第 1 轮后以 blocked 停止”。`done_when` 必须点名测试与断言。`AGENTS.md` 附已知陷阱清单。
4. **需要运行时观察的工作永远拆三段：** 加埋点 → 跑游戏的评分测试任务 → 修复。无头代理永不接“找出 X 为什么消失”。
5. **把实现者的最终消息喂给评审；空 diff 即停止报告。** 先给 1 轮修复，门槛接近时才升级。
6. **成本与吞吐作为一等指标：** 每任务美元上限、Codex 也计量、每周一行“每 100 美元合并多少功能”。
7. **保留调度者角色，但减少聊天带来的限制：** 决策批次走共享文件夹（无 10 条上限）、每次压缩前重写调度日志（有效）、硬规则“给合并起标题前先 grep 合并后的代码”。

---

## 8. Playtesting and balance / 试玩与平衡

### What happened / 发生了什么
- Six human playtests in three days, every one with the designer as the only tester; the external playtest kit exists but was never run; the pad-only pass was owed from W1 and done at P4.
- Sims went through four stages (stationary hero → RH.Bot → bot v2 with threat map); the first stage produced noise (all mortal runs dead at 16 s, save leaking between runs). Once the bot moved, sims caught real regressions (R2 "same DPS in smaller bullets", the alive-cap paradox) and the 11/11 Warden wins that humans could not match.
- Instrumentation for what humans complained about arrived after the complaint: imbue counters, threat actions/min, nearest-enemy p10, enemy-bullet metrics, heal granted.
- The archetype-band gate (`archetype_bands.md`) was never measured; the ≥ 60 % blessing gate is still unmet; I7 (100 Intercession combinations in ±20 %) is queued.

- 三天六次真人试玩，每次只有设计师一人；外部试玩清单存在但从未执行；手柄专测从第 1 周欠到 P4。
- 模拟经历四个阶段（静止主角 → 机器人 → 带威胁图的机器人 v2）；第一阶段只有噪声。机器人会动之后抓到了真实回归和人类打不出的 11/11 守望者胜率。
- 人类抱怨什么，什么的埋点才出现：附魔计数、威胁动作/分、最近敌人 p10、敌弹指标、治疗量。
- 流派区间门槛从未测过；≥ 60 % 祝福门槛仍未达成；I7（100 种神临组合 ±20 %）还在队列里。

### From zero / 从零开始
1. **Three humans besides the designer, from the first offer screen onward, every 2–3 days;** the "external" kit is a Stage-1 ritual, not a final gate. Pad on the first offer screen.
2. **Day-1 instrumentation list (CSV per minute + JSONL events):** kills/min, swarm TTK, damage by source, dashes, level minutes, threat actions/min, nearest-enemy p10, aimed enemy bullets, proc/FX/audio counters for every visible effect, heal granted, offer shown/picked/declined, run seed. Automatic log export for testers from the first package.
3. **Moving bot before mass sims;** isolated save per run; pinned patron and pool; the skill knob (§1.3); bot numbers reported as "bot (lower bound)".
4. **A number-change log** (one row per constant: date, from, to, evidence [M]/[S], re-run check) so the XP curve and pressure cannot oscillate unnoticed.
5. **Measure the gates that were written:** archetype bands and power split run in the nightly test job from the day the systems exist; a gate that cannot be measured is not a gate.
6. **Reserve machine time:** playtest holds and GPU windows as a scheduler primitive, not a chat message.

1. **除设计师外三位真人，从第一张选卡界面起每 2–3 天一次；**“外部”清单是第一阶段的例行，不是终点门槛。手柄在第一张选卡界面就测。
2. **第 1 天埋点清单（每分钟 CSV + JSONL 事件）：** 击杀/分、小怪 TTK、分来源承伤、冲刺、升级分钟、威胁动作/分、最近敌人 p10、瞄准玩家的敌弹、每个可见效果的触发/特效/音频计数、治疗量、卡牌展示/选取/放弃、局种子。首个打包版就带测试者日志自动导出。
3. **先有会动的机器人再跑批量模拟；** 每局独立存档；固定本尊与卡池；技巧旋钮；机器人数字标注“下界”。
4. **数值变更日志**（每个常数一行：日期、原值、新值、证据 [M]/[S]、复测检查），经验曲线与压力不会再悄悄来回摆。
5. **写下的门槛必须被测：** 流派区间与强度占比从系统存在那天起进入夜间测试任务；测不了的门槛不是门槛。
6. **预留机器时间：** 试玩保持与 GPU 窗口是调度原语，不是聊天消息。

---

## 9. Scope and roadmap mechanics / 范围与路线图机制

### What happened / 发生了什么
- The 6-week calendar roadmap was obsolete within 48 hours; milestone tags m2–m5 were cut in pairs on the same commits. From then on the roadmap was a status log, not a plan.
- "Core now, GATED numbers later" was a good idea that drifted: GATED items accumulated (balance passes, Outfall/Sump winnability, Heat steepness, co-op multipliers) while CORE content kept landing on top of them.
- No scope valve was pulled; the never-cut list was honoured literally (D-48 added minor hands rather than renumber L8/L16).
- Two 19–22 h pauses coincided with the switch of pipeline and with the user's playtime; the autopilot has no notion of "the designer needs to play this before more lands".

- 6 周日历路线图 48 小时内作废；里程碑 m2–m5 成对打在同一提交上。此后路线图是状态日志，不是计划。
- “先做核心、数字 GATED”是好想法但漂移了：GATED 项不断累积，而 CORE 内容继续压在上面。
- 一个缩减阀都没拉；“绝不砍”被字面执行。
- 两次 19–22 小时的停顿分别对应管线切换和用户游玩；自动驾驶没有“设计师必须先玩过这版再继续”的概念。

### From zero / 从零开始
1. **Plan by gates, not by calendar.** Each stage has an entry condition, an exit test with humans, and a list of what may *not* start before the exit. Dates are forecasts, written separately.
2. **A GATED-debt limit:** when more than N gated numbers wait on data, content stops and a playtest happens. Default N = 5.
3. **Valves with triggers and owners;** opening a valve in the other direction (adding scope) requires the system-budget rule (§2.5).
4. **Designer-play checkpoints are stages' exit tests;** the autopilot queues work but does not merge feel-changing systems past an unplayed checkpoint.

1. **按门槛而非日历计划。** 每阶段有进入条件、带真人的退出测试，以及一份“退出前不得开始”的清单。日期是单独写的预测。
2. **GATED 债务上限：** 等数据的 GATED 数字超过 N 个时停止加内容、安排试玩。默认 N = 5。
3. **阀门带触发条件与负责人；** 反向开阀（加范围）必须满足系统预算规则。
4. **设计师游玩检查点就是阶段退出测试；** 自动驾驶可以排队工作，但不把改变手感的系统合并到一个尚未被玩过的检查点之后。

---

## 10. The from-zero roadmap, by gates / 从零开始的路线图（按门槛）

| Stage / 阶段 | Build / 做什么 | Exit test / 退出测试 | Not before exit / 退出前不得开始 |
|---|---|---|---|
| **S0 Foundations 地基** | Repo + LFS + release branch; all gates of §7.1; localization CSV pipeline (§5.1); day-1 instrumentation (§8.2); architecture invariants (§6.2); readability contract + palette on grey-box (§4.1–2); shape-constant table (draft); power-budget tool (§2.1); decision folder; AGENTS traps list; GPU semaphore | Empty arena renders, 5 000 bullets + 400 enemies at budget on named hardware/proxy; golden screenshot gate green; a bilingual test string round-trips | Any content |
| **S1 Core feel slice 核心手感切片** | 1 arena grey-box; 3 Keepers with identity layer (flags, 2 archetypes each, imbue, tiers, 1 Intercession); 5 weapons + ~10 modifiers with capability tags; Swarm/Tank/Rusher/Turret + 1 boss; spawn director with rhythm + drop-ins + kill-horizon; 5-card offer with weights, guaranteed slot, L8 commitment, reroll/banish/lock; HUD with card-kind identity; moving bot v1 | 3 non-designer humans (≥ 1 pad, ≥ 1 zh) play 2 runs each; §3.5 bands hold for a steered build; ≥ 2 can name their 流派; power split within budget; shape constants **locked** | 2nd arena, meta, more Keepers, art-book fidelity |
| **S2 Full Keepers & builds 全司者与构筑** | Keepers 4–6; 10 weapons; 20 modifiers (matrix generated); L16 transformation hand; duo reactions; Intercession variants **as rows of the same table**; elites + affixes; statuses; bot v2 + skill knob; number-change log | Ablation gates measured nightly (Keeper share, archetype bands, Intercession ±20 %); 20-min run winnable by humans and bots; death histogram matches §3.5 | Acts, Bids, fates, Refusal, co-op UI |
| **S3 Loop & meta 闭环与元游戏** | Hub, save (versioned from S0), Ichor, Contracts, Discoveries, Codex layers, narrative §12 scope, full zh pass, settings/accessibility, packaged build with log export | External playtest (3 testers, fresh saves, 3 runs): 2-of-3 go criteria; pad pass in the package; `v0.1_findings.md` written | Full-game numbered specs |
| **S4 Art to the book 美术到位** | Environment, hero, enemies, VFX, UI, weather to the art book; audio signatures; scalability tiers on real 3060 or agreed proxy; p95 budget | Readability sheet 3:1 in both weathers; GPU/GT median and p95 within budget per tier; golden screenshots re-baselined once | — |
| **S5 Breadth 广度** | Arena 2 + 3 new roles + boss; Acts/story state; loadouts; Bids; Heat; each with GATED numbers tied to S3 findings; GATED-debt limit enforced | Each arena winnable at Heat 0 by humans before the next starts; external playtest #2 | Arena 3 before arena 2's exit |
| **S6 Modes & co-op 模式与合作** | Refusal run, fates, ending; local 2-player on the S0 per-player state (S4–S12 shrink to UI/camera/revive/meta); daily seeds; Steam | Couch test with 2 humans; SoloParity golden unchanged; two-bot runs set multipliers | Online co-op |
| **S7 1.0 polish 打磨** | Quota, accessibility, full audio, 3D hub, Deck tier, onboarding (GATED on S5 data) | Ship checklist; perf re-certification on all tiers × arenas | — |

Rule of thumb for calendar: with the throughput seen here (Era 1 ≈ 350 commits/half-day, Era 2 ≈ 60 with a clean main), S0–S3 is about one week *if* the human playtests are scheduled on days 2, 4 and 6; the humans, not the agents, are the critical path.
日历经验：按本项目观察到的吞吐，S0–S3 约一周——前提是真人试玩排在第 2、4、6 天；关键路径是人，不是代理。

---

## 11. What to keep unchanged / 原样保留的做法

- Five docs + CLAUDE.md non-negotiables; "docs win over code"; "stop and say so".
- Path/role keys, fiction only in string tables; bilingual from day 0; Layer gating; context-first zh; TextAudit.
- SoA bullets/enemies, fixed step, no GAS, data assets + tags, seeded runs, ISM + VAT, procedural/CC0 art with a licence log.
- `RH.Stats` + §3.5 bands as acceptance; RH.Bot + LoopCheck + sims as regression; the external-playtest kit (run it).
- One owner per file area; parallel agents, serial gated merges; the scheduler's dispatch log rewritten at every compaction; merge_verify-style gates; the Codex brief/review/gate loop with the §7 fixes.

- 五份文档 + CLAUDE.md 不可谈判条款；“文档胜过代码”；“停下并说明”。
- Path/角色键、虚构名只在字符串表；第 0 天双语；分层门控；从语境出发的中文；文本审计。
- SoA 子弹/敌人、定步、无 GAS、数据资产 + 标签、种子局、ISM + VAT、程序化/CC0 美术 + 许可日志。
- `RH.Stats` + §3.5 区间做验收；机器人 + LoopCheck + 模拟做回归；外部试玩清单（真的去跑）。
- 每个文件区域一个负责人；代理并行、合并串行且带门槛；每次压缩前重写调度日志；merge_verify 式门槛；带 §7 修正的 Codex 简报/评审/门槛循环。

---

## 12. Open items this review did not resolve / 本次复盘未解决的事项

- Playtest item #4 (enemies vanish after a card pick) is not in any `Docs/` file; it exists only in the scheduler's queue. It needs the rendered smoke gate (§4.3) to be reproducible.
- The run-length contradictions (§1) need one sweep: GDD §3.5 columns, Outfall/Sump timelines, Refusal starve curve, identity showcase gate minute, progression v2 decision count, L24.
- Stale GDD sections (§5.4, §5.6) and the Art §8 vs §12 sourcing contradiction.
- The ≥ 60 % Keeper-share gate, the archetype bands and I7 are still unmeasured or queued.
- The external playtest has never happened; `v0.1_findings.md` is the missing input for every GATED number.

- 试玩项 #4（选卡后敌人消失）不在任何 `Docs/` 文件中，只在调度队列里；需要渲染烟雾门槛才能复现。
- 局长矛盾需要一次统一清扫：GDD §3.5 列、外泄区/沉淀池时间线、拒绝局饥饿曲线、流派展示分钟、进阶 v2 决策数、L24。
- GDD 过期章节（§5.4、§5.6）与 Art §8 对 §12 的资产来源矛盾。
- ≥ 60 % 司者占比门槛、流派区间、I7 仍未测或在队列中。
- 外部试玩从未发生；`v0.1_findings.md` 是所有 GATED 数字缺失的输入。
