# Dusktracer / 曳光 — START HERE for the rebuilding agent

> You are starting **Dusktracer** from zero: a new repository, a new document set, a new build — under the **Gatework Harness** in `../harness/`, using the previous game (RogueHero, 2026-10-18 → 10-28) only as reference material. Read this file, then `../harness/H00_README.md`, then the files in the order given in §4. Do not start writing code until stage S0 of the harness is complete.
> 你从零开始做《曳光》：新仓库、新文档集、新构建，遵循 `../harness/` 的门槛工作法；前作 RogueHero 只是参考材料。先读本文件，再读 `../harness/H00_README.md`，然后按 §4 的顺序读。S0 完成之前不写代码。

## 1. The mission (one paragraph)
Build a top-down survivor-like where **the player is the bullet hell**: auto-firing weapons, move / dash / one Intercession as the only verbs, a 15-minute session that ends in a boss, a five-card offer at every level, six Keepers (Paths: Storm, Forge, Tide, Shade, Radiance, Quicksilver) whose blessings **change weapon behaviour** and carry ≥ 60 % of the build's power by level 16, a second Keeper at level 8 and a qualitative transformation at level 16. Realistic eroded-white megastructure at dusk; cool player tracers, warm enemy fire; every threat a skill check. English and Simplified Chinese from the first string. Local two-player in 1.0, architected from day 0.

## 2. What this folder is
- `../harness/` — **binding** method: document system, gates, change and context management, pipeline, self-audit. Project-agnostic. Changes to it go through a decision record.
- `game/` (this folder) — everything about *this* game:
  - `G01_Name.md` — the name, identifiers, rename map. **Binding.**
  - `G02_Blueprint_DocSet_zh.md` — the plan for the new document set and the core design decisions (parts 2, 3, 5 are game-specific; parts 1, 4, 6, 7 were generalised into the harness). **Binding as a plan**; the documents you write from it become the source of truth and this file becomes history.
  - `G03_Retrospective_EN_ZH.md` — what happened in the previous project and the from-zero plan. **Reference.**
  - `G04_Reflection_EN_ZH.md` — the previous agents' own account of their mistakes. **Reference**; read §5 and §6.
  - `G05_Cleanup_Findings_zh.md` — what turned out dead or unwired in the old code base. **Reference** (a list of things not to repeat: unwired text, unset hooks, always-true guards).
  - `reference/` — the old project's documents, specs, text tables, tags, balance notes, playtest notes, tools. **Reference and history, never copied blindly.** See the status table in §6.

## 3. Rules of engagement
1. The harness is binding. Where the old reference material and the harness disagree, the harness wins; where the old GDD and `G02` disagree, `G02` wins; where `G02` and the owner disagree, the owner wins and a decision record is filed.
2. Every proper noun follows the naming rules: Path and role are the keys; fiction lives in string tables only; Chinese is authored from the fiction, not translated; the word "god" never appears in fiction. (`reference/original_docs/04_Narrative.md §13`, `reference/localization/zh_naming.md`.)
3. The text tables in `reference/text_tables/` are authored content (≈ 1 380 rows, en + zh) and may be **reused row by row** after the new key scheme exists — but only rows whose system exists in the new design; never import a table wholesale.
4. Nothing from `reference/tools/` is run as-is; they are reference implementations of gates (text audit, zh overrides, per-file compile check, smoke, RC gate, the job runner's README). Rewrite them under the new names and the harness's gate list.
5. Keep the three questions on every change: what class, what layer, what does it replace.

## 4. Reading order
1. `G01_Name.md` (5 min) → 2. `../harness/H00`, `H01` (15 min) → 3. `G02` parts 2, 3, 5 (the document plan and the core decisions; 40 min) → 4. `../harness/H02`, `H03`, `H07` (how to write the docs, how to stage, what gates) → 5. `G03 §1–§3, §10` and `G04 §5–§6` (why those decisions) → 6. Only then the old GDD / TDD / Narrative / Art for depth, by section, as `G02` points to them → 7. `../harness/H04`–`H06`, `H08`, `H09` before the first brief is written.

## 5. What to keep, what to change, what to drop (from the previous game)

**Keep unchanged (proven):** five-doc split and binding sections · Path/role keys, fiction in string tables, bilingual from day 0, Layer gating, length caps · the offer system (5 cards, weights, guaranteed slot, L8 second Keeper, L16, reroll/banish/lock, anti-frustration) · struct-of-arrays bullets at a fixed 60 Hz with a spatial hash, SoA enemies on instanced meshes + VAT, no GAS, data assets + tags, seeded runs · the readability contract (cool player core, warm/magenta danger, red projected telegraphs, 3:1 contrast, density LOD, no motion blur) · procedural / CC0 / OFL assets with a licence log · `Stats` with the core-feel bands as acceptance · bot + loop checks + sims as regression · the external playtest kit (run it this time).

**Change (decided in `G02` part 5):** session 15:00 and ≈ 28 decisions locked at S1 · Keeper identity (behaviour-flag layer) is the core system from S1; stat-only blessings never exist · 6 Keepers × 3 archetypes = 18, one expression table per Keeper (transformation, Intercession variant and duo reaction are columns of the same table) · 16 modifiers, all specialisers; compatibility matrix generated from capability tags; no numeric "fit" bonus · one Intercession key, no second ultimate input · decision log as files, docs reference instead of restate, lint · art-book fidelity after the fun gate; palette rules fixed once before the first tracer · rendered smoke + goldens and a packaged pad check in the merge gates · ≥ 3 non-owner humans every 2–3 days from the first offer screen · per-player state, per-system RNG streams, whole-sim fixed step and save versioning as day-0 invariants.

**Do not carry over:** the 20-archetype layout, the second ultimate input, the `God.*` tag root, the three overlapping modifier-matrix generations, the union-merged `.po` pipeline, hand-written `MapsToCook` lists, code-only `WBP_/NS_/SK_` "swap-out" promises, the 250k-token decision log, any text row whose system does not exist yet (`G05 §B.5, §C.1–C.2` list the unwired ones).

## 6. Status of the reference files

| Path | Status | Use |
|---|---|---|
| `reference/original_docs/01_GameDesign.md` | history; §3.5 and §5.6 still the best statements of the core feel and the offer rules | source for `00_Vision`, `02_Systems/offers.md`, `director.md` |
| `reference/original_docs/02_TechnicalDesign_s1-16.md` | reference architecture (keep the shape; add the day-0 invariants) | source for `07_Technical` |
| `reference/original_docs/02_TechnicalDesign_s17_decision_log_HISTORY.md` | history only (580 entries, duplicates) | search when you need to know *why* something was done |
| `reference/original_docs/03_Roadmap.md` | history (a calendar plan executed in 48 h) | do not reuse the format; see `H03` |
| `reference/original_docs/04_Narrative.md`, `05_ArtDirection.md` | reference; §13 naming and Art §3 readability are near-final | source for `03_Content`, `04_Presentation` |
| `reference/original_docs/CLAUDE.md`, `AGENTS.md`, `README.md` | reference for the new `AGENTS.md` (rewrite under the harness; keep the non-negotiables as rule IDs) | |
| `reference/specs/*.md` | design specs of the previous game; `keeper_identity_spec.md` and `build_depth_spec.md` §1/§3/§4 are the richest sources for the expression tables; `v04_coop_spec.md` for per-player state | mine, do not copy |
| `reference/localization/*` | binding in spirit: zh naming and glossary conventions | carry forward |
| `reference/text_tables/*.csv` | authored content, reuse row by row after the key scheme exists | |
| `reference/tags/*.ini` | reference for tag families (rename `God.*` → `Keeper.*`; drop the dead v1 branch tags listed in `G05 §B.3`) | |
| `reference/balance/*` | method (model, sims, power split, pressure proposal, R15 bullet targets) and measured values to seed `GATED` constants | source for `10_Tuning` |
| `reference/playtests/*` | the six human sessions and the external kit; what humans actually said | source for the validation protocol and the first hypothesis lines |
| `reference/art/asset_licenses.md` | format for the new licence log | |
| `reference/tools/*` | reference implementations of gates | rewrite |

## 7. The first week (S0 → S1 entry)
- **Day 1:** `00_Vision`, shape-constant draft (from `G02` part 3 §08), `Glossary` skeleton with the rename map; owner confirms line by line in the window. Repo, LFS, release branch, `AGENTS.md` with traps and tolerances.
- **Day 2:** gates (build 0-warnings, per-file compile, Shipping compile, headless tests + floor, text/loc audit, rendered smoke + goldens, packaged pad check) green on an empty project; localisation CSV pipeline; `Stats` instrumentation list implemented against an empty arena; GPU semaphore; state block; `OPEN.md`.
- **Day 3:** `07_Technical` invariants 1–10 implemented as the skeleton (per-player run state, RNG streams, fixed step, save version, catalog → data generation); readability contract + palette tokens on grey boxes; value-budget ablation tool stub.
- **Days 4–7:** `02_Systems/keepers.md` expression tables for **three** Keepers (recommended: Storm = bullet-hell, Forge = melee/burn, Tide = control — the most distinct trio), `offers.md`, `director.md`, `weapons.md`/`modifiers.md` with capability tags; then the S1 slice. **Book the three S1 validation sessions on day 1.**
- S1 exit: three non-owner humans, pad and KBM, en and zh; core bands hold; they can name their build; value split within budget; shape constants locked.

## 8. Decisions the owner should confirm before day 1 (put them in `OPEN.md`)
1. Session length 15:00 + boss ≤ 2:00 (as `G02`), or re-open.
2. The three S1 Keepers (recommendation above).
3. Engine / platform: UE 5.8.x, Win64 first; Deck tier at 1.0.
4. Local co-op in 1.0 (architecture from day 0) — yes/no.
5. Whether any Act II / Act III content (second and third arenas, fates, Refusal) is in scope before S5 — recommendation: sketches only until `findings_S3.md` exists.

## 9. Kickoff prompt for the new agent (copy-paste)
> You are the scheduler for **Dusktracer / 曳光**, a new UE 5.8 project. The folder `Handoff_Dusktracer/` is your only input: `harness/` is the binding method, `game/` the design and reference material. Start with `game/G00_START_HERE.md`. Do not write code before harness stage S0 is complete and the owner has confirmed the shape constants in `OPEN.md`. Keep the state block rewritten, the must-read set under budget, and every change classified. Report the S0 exit test, not progress.

---
### 中文摘要
任务：从零重做《曳光》；`harness/` 是约束性方法，`game/` 是本游戏的设计与参考。规则：方法 > `G02` > 旧文档；键名用 Path/角色，虚构名只在字符串表；旧文本表可逐行复用但不整表导入；旧工具只作门槛的参考实现。保留经过验证的部分（选卡系统、SoA 架构、可读性契约、双语、埋点、机器人回归、外部试玩包），按 `G02` 第五部分改掉十件事，不要带走 20 流派布局、第二神临键、`God.*` 标签根、三代矩阵、union 合并的 `.po`、25 万 token 的决策日志和未接线的文本。第一周按 S0 → S1 推进，第 1 天就预约三次真人验证；§8 列出需要负责人先确认的五个决定；§9 是给新代理的启动提示词。
