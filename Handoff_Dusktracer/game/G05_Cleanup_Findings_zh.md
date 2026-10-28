# 代码库清理清单 / Cleanup candidates — 2026-10-26

> 评审会话只读扫描，基准 `main` 26d6842b。五个并行扫描：Source 死代码、Tools/Config、Content/Saved、字符串表、分支/工作树/文档。**本会话没有删除任何东西。** 执行必须走调度者 + 门槛（构建、非 unity、测试、烟雾），下文已按“不需要构建 / 需要构建 / 需要设计决定”分层，并拆成可直接派发的任务。
> 每项带证据与置信度：High = 安全删，Medium = 删后要过门槛确认，Low/决定 = 先问人。

## 0. 一眼看清

| 层 | 内容 | 可回收 | 风险 |
|---|---|---|---|
| **A. 不需要构建** | 105 个已合并的工作树、约 130 个已合并分支、旧打包与暂存目录、旧日志、.git 临时对象、笔记文件夹里无引用的截图 | **≈ 396 GB 工作树 + ≈ 3 GB Saved + ≈ 0.7 GB 杂项** | 极低（全部已合并、无未提交改动；两个例外见 A.3） |
| **B. 需要构建 + 测试** | 约 350 行 High / 450 行 Medium 死代码；12 个无用标签；2 个过时脚本 + 1 个误入库文件；28 行字符串；1 个模板残留 ini 节 | 代码整洁度，不是体积 | 低—中（门槛可证） |
| **C. 需要设计决定** | 36 行“可能是没接线而不是死”的文本、3 个可能未完成的功能钩子、1 个旧 Boss 网格、2 个未合并分支、1 个工作树里的 2 个未跟踪资产、OfferCloseTrace 调查工具 | — | 决定错了会丢功能 |
| **D. 文档** | §17 中 29 行逐字重复、过期的 NEXT_playtest、已完成的计划文档、标注 superseded 的章节 | 可读性 | 低 |

**结论：** 内容（Content/）其实很干净（真正候选 < 0.2 MB）；代码没有整文件可删，死的是函数级；真正臃肿的是**工作树与构建产物（≈ 400 GB）**和**文档的重复/过期段落**。

---

## A. 不需要构建的删除（调度者可直接执行，顺序重要）

### A.1 工作树（≈ 396 GB）
- `E:\RogueHero-cowork\*`：88 个，全部分支已合并进 main（`merge-base --is-ancestor` 逐个核过），`status -uno` 无未提交改动。每个约 3.2 GB，几乎全是 `Intermediate/`。
- `E:\RogueHero\.claude\worktrees\*`：17 个注册工作树中 16 个已合并；`agent-a60559ea1a55589b6` 的分支未合并，但其功能已完整以 16c12ac3 上线（司者音色签名），可删。
- 例外（见 C.5）：`magical-kare-33c248` 里有 2 个未跟踪资产。
- `release-v01` 工作树实际停在 **v0.3** 的提交（d1824817），名字误导，可删；分支 `release/v0.1` 本身保留。
- 非工作树残留：`.claude\worktrees` 下 53 个零散文件（39 个 .log、`bosstests.bak`、`fixinc.py`、`sweep_*.txt`、两份规格副本、`rc/`、`tracers-2d60a3/`），约 41–89 MB，可删。

顺序：`git worktree remove <path>` → `git branch -d <branch>` → `git worktree prune`。前提：确认 cowork 自动驾驶不在运行、没有 UE 进程持有这些目录。

### A.2 分支（134 个 → 保留 4 个）
- **127 个已合并**：全部安全。其中 104 个被工作树占用（先删工作树），23 个现在就能删（`backup/env-g16-revert-of-revert`、15 个 `claude/*`、`data/zh-hub-pl23`、`data/zh-lifetime`、`dev/codex-i1a2-collectors`、`sched/int`、`sched/offer-hooks`、`sched/pt5a-reland`、`worktree-agent-aa49d82800d6bcf01`）。
- **未合并 6 个**：
  - `dev/codex-b7b-resonance-finish` — 同一工作以 7b6377ec（b7c）上线 → 删。
  - `zh/metaui-432bdfe` — 45/45 条 zh 已在 main → 删。
  - `worktree-agent-a60559ea1a55589b6` — 被 16c12ac3 取代 → 删。
  - `release/v0.1` — **保留**（发布引用；v0.1 从未打标签，这是一个待补的缺口）。
  - `dev/codex-d2-gamepad` — 见 C.6。
  - `wip/skeletal-m1` — 见 C.7。

### A.3 Saved/（gitignore，≈ 3 GB High + 1 GB Medium）
| 路径 | 大小 | 判定 |
|---|---|---|
| `Saved/Packages/RogueHero_v0.1b_playtest_9d36133.zip`、`v0.2-preview_dd336f9.zip`、`v0.2_90ebeff.zip` | 1 036 MB | 删（提交/标签仍在，可重建） |
| `Saved/Packages/RogueHero_v0.3_playtest_d1824817/`（解压目录） | 495 MB | 删（旁边有同一版本的 zip） |
| `Saved/Packages/RogueHero_v0.3_playtest_d1824817.zip` | 351 MB | **保留**（最新试玩包） |
| `Saved/Packages/Windows/`、`Saved/StagedBuilds/Windows` | 484 + 484 MB | 删（每次打包重生成） |
| `Saved/Logs/` 10-23 之前的 409 个文件（含 82 MB 的 `OfferCloseTrace_20261022*.log`） | 309 MB | 删，保留最新 |
| `Saved/Cooked`、`Saved/Shaders`、`Saved/Crashes`、`Saved/SigilPreview`、`Saved/*.log`、`rel_*` 标记 | ≈ 70 MB | 删 |
| `Saved/Cowork/<id>/` 121 个任务产物目录 | 613 MB | Medium：删（`*.md` 报告与 `log.md` **保留**；`e5-xp-warden-measure` 目录无报告无日志行，是孤儿） |
| `Saved/Screenshots/WindowsEditor` | 106 MB | Medium：删（合成图已在 Docs/） |
| `Saved/Playtests`、`Saved/Balance` | 6 MB | **保留** |

### A.4 .git 与 LFS
- `.git/objects/*/tmp_obj_*` 4 个孤儿对象（10-21 中断留下），95 MB → 删（High）。
- `.git/lfs` 510 MB 对应 197 MB 检出，约 313 MB 旧对象 → `git lfs prune`（Medium：工作树共享此存储，先删工作树）。
- `Intermediate/` 4.8 GB、`DerivedDataCache` 2 MB、`Tools/**/__pycache__` → 可删，代价是一次完整重建。`Binaries/` 保留。

### A.5 共享笔记文件夹（691 MB，450 张 PNG）
- 48 张（97 MB）被 TDD/笔记引用 → 保留（如 `g11b/g11c_sigils_vs_pl07.png`、`g3_fusions_before_after.png`、`vfx_ults_*.png`、`pl18_density_check.png`）。
- 其余 **591 MB 无任何引用** → 删或归档（`env/` 228 MB、`ui_g9` 54、`ui_audit` 44、`ui_audit_after` 25、`weapons24_400enemies.png`、`steam_deck/`、`rc_health/`、`hud_xp/` 等）。

---

## B. 需要构建 + 测试的删除（拆成 Codex 任务）

### B.1 代码：High 置信（≈ 350 行，约 20 个文件）
**从未被调用的函数**（仓库内仅声明 + 定义 2 处）：
`FRHStatSet::ClearModifiers`（Player/RHStats）、`URHWeaponRegistry::FindFusionFor`、`URHInputPrompt::GetShownKeys`、`URHEffect_Radiance_HaloRing::GetSunStored`、`URHAudioSubsystem::IsMenuContext` / `SetCategoryVolume`、`URHOfferGenerator::IsReserveId`、`URHMetaTile::SetTileSize`、`RHNiagaraAuthor::AddUserFloat/AddUserInt`（编辑器模块）、`RHEvolutions::IncompatibleModifiers`、`RHStoryLink::SetRefusalUnlockedOverride`（cvar 仍在，只删 setter）、`URHWeaponInstance::bSuppressNormalCadenceForTest + TickUltimateTriggersForTest`、`URHUltimateComponent::PullPickups` 及 `Counters.Pulls`（连同 `RH.Ult` 打印列）。

**从未写入的状态 / 从未设置的钩子**：
- `RHBranchHooks::SetSpeedScaleMult/ClearSpeedScaleMult` + `RHBranchSpeedScale::Map`：0 个调用者，`GetSpeedScaleMult` 恒为 1；删掉三处 `×1`（RHTierEffects.cpp:1019-1021、RHBranchEffects.cpp:1651/1743）行为不变。
- `URHHUDWidget::SetUltimateSource/UltimateSource`：setter 从未调用，HUD 永远走回退路径。

**恒真的集成守卫**（目标头文件都已存在，`#else` 永不编译）：`RH_STORY_LINK_HAS_STORY_STATE`、`RH_GM_HAS_ENDING`、`RH_OFFERFLOW_HAS_EVOLUTIONS`、`RH_TELEGRAPHS_HAS_SUMP`、`RH_OMENS_HAS_BRANCHES`、`RH_BRANCH_FX_REMARKS`、`RH_ENEMIES_HAS_KEEPER_REMARKS`、`RH_SPLIT_CHANCE_SUPPORTED`、`RH_BRANCH_HOOKS_MOMENTUM(_HOLD)` —— 约 70 行。

**死类型**：`enum class ERHBulletFaction`（RHBulletTypes.h:53）、`UENUM ERHMuscleStage`（RHMuscleMemory.h:9，段数用的是 int）。

**重复实现**：`RHCardTech.cpp` 与 `RHCardSlab.cpp` 的 `Hash/Smooth/ValueNoise/Fbm`（常数相同，Hash 逐字节一致）→ 合并到一个共享头（≈ 35 行）。**不要**合并 `RHSigil.cpp` 的变体（常数不同，会改变印记纹理）。

### B.2 代码：Medium 置信（≈ 450 行，需两个模块构建 + 全套测试）
- `URHUltimateMeter` 的 **widget 半边**（NativeOnInitialized/Tick/Mouse*/FindSource/Apply/RefreshNote/SetSource/GetShown，cpp 130–300 行）：类从未被创建；只有静态 `MakeState/StatusText/NameText/NoteText` 被 RHHUDWidget 使用 → 保留静态部分为命名空间。
- 约 70 个从未调用的单行内联访问器（`ARHHero::GetAnimInput/GetKnockVelocity/GetLampLevel/...`、`URHEnemySubsystem::GetChestPos/GetCourserChain/...`、`URHBulletSubsystem::NumHitSparks/...` 等）：安全但价值低。
- `RHGodCommands.h`：只被自己的 .cpp 使用 → 删头文件，函数改 static。
- 测试里两份 `SafeHue` 副本（RHCardFidelityTests、RHCardTextContrastTest）vs `RHCardStyle.cpp:242`：可共用导出函数，但测试副本也是独立 oracle，可不动。
- `URHTreeScreen::PurchaseFocused/GetFocusedNodeId`：无人调用。

### B.3 Config / 标签
- `Config/Tags/RHBuildDepth.ini`：18 个 v1 分支标签已被 `RHBranches.cpp` 的现行表取代。**12 个**在 Source/Content（含二进制）无任何出现 → 删；3 个只被测试使用（`Storm.Arc`、`Storm.Thunder`、`Forge.Ember`，RHOfferExtensionTests / RHOfferIdentityTests）→ 先把测试迁到现行流派标签再删；3 个在 `RHBranches.ini` 重复定义（`Tide.Frost`、`Shade.Economy`、`Quicksilver.Momentum`）→ 删重复。
- `DefaultEngine.ini` `[/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings]`：模板残留（含 SecurityToken），Win64 项目无引用 → 删。`ActiveGameNameRedirects TP_Blank` 无害，可顺手删。
- `DefaultGame.ini`：`MapsToCook` 只列 L_Hub/L_Arena_Test；Outfall/Sump 靠 `DirectoriesToAlwaysCook=/Game/RH` 带入，同时把 `L_StressTest` 和 5 张 `L_UnitLineup*` 评审图也烤进了 Shipping → 显式加两张竞技场、排除评审图（这是修正不是删除，但属于同一次清理）。

### B.4 Tools
- 删：`Tools/Balance/patch_outfall_timeline.py`、`patch_sump_timeline.py`（TDD 10-23 明确被 `RH.Spawn.WriteFactoryAssets` 取代，且硬编码 D-71 之前的数值，再跑会回退 D-71）；`.claude/errs.py`（误入库的一次性解析器）。
- 合并：`Tools/Balance/RunTests.ps1` → `RCGate.ps1`（三处相同的测试启动命令）；`create_status_materials.py` + `.bat` → `build_vfx_materials.py`。
- 过时需修：`pressure.py`（假设 20 分钟、minute 18）、`balance_model.py`（常数来自已被 DA 取代的 C++ 回退，归档）、`RunSims.ps1` 默认 `StopMinute=20`、`ShipCheck.ps1 -Boss` 的 17:00 假设、`Package.bat` 的 `-map=` 缺两张竞技场；注释里指向不存在的 `Tools/build_boss_forms.py`/`BuildBossForms.bat`（build_enemy_models.py:6、build_unit_models.py:959、RHBossSubsystem.cpp、g14 计划文档）与 `Tools/ShipSteam.bat`（DefaultGame.ini 注释、v10 规格）。
- 重复启动器代码：约 12 个 .ps1 各自复制 UE_ROOT / Start-Process / PID taskkill / 日志轮询 → 一个共享 `.psm1`（整洁度任务，非删除）。
- 三个“延时执行”命令做同一件事：`RH.Later`、`RH.Debug.After`（仅 CardShots 用）、`RH.E2E.Later` → CardShots 改用 `RH.Later`，删 `RH.Debug.After`。

### B.5 字符串表（删 CSV 行 → 删 zh_overrides 行 → `Tools\Localize.bat`）
总计 1 379 行，**64 行孤儿（4.6 %）**，无重复行，无空文本，司者行均在长度上限内。High 置信可删 **28 行 + 14 条覆盖**：
- `ST_World`：`World.{Keep,Six,Yard,Reclaimer,Asset,Contracts,Advance,Reforging}`（8，无查找的词汇表条目；同组 Patron/Vigil/… 在用）。
- `ST_UI`：`HUD.Defeated`、`HUD.DefeatSummary`（旧结算覆盖层）、`HUD.Kills`、`HUD.Tools`、`HUD.WeaponLineMax`（HUD 重构残留）、`HUD.UltLine`、`Card.Marker.Join`、`Card.NeedsKey`（8）。
- `ST_Keywords`：`Keyword.{Tier,Transformation,Socket,MomentumStack,SunCharge,Resonance}.{Name,Def}`（12）——没有任何 `<kw id>` 标记引用；DevNotes 承诺的 D-70 卡面标记从未加。**二选一：加标记或删行**（见 C.1）。
附带发现：`TextAudit.py`/`apply_zh.py` 的 `KEEPER_LINE` 正则锚定 `$`，`.Act2` 变体不受长度上限检查——修正则。

---

## C. 需要设计决定（删错会丢功能）

| # | 项 | 证据 | 决定项 |
|---|---|---|---|
| C.1 | `Ult.<Patron>.Layer.<Second>.Desc` 30 行（ST_Ultimates）+ 其 30 条覆盖 | 只有 `.Name` 被构造（RHUltimateMeter.cpp:101）；层定义不设 Description。文本已写好。删掉会连带孤立 `Keyword.{Chain,ChargedShot,Shatter}` 6 行 | **接线**（神临面板显示第二司者层的描述）还是删？建议接线：P6 反馈正是“第二司者应影响神临”，描述是让玩家感知它的最便宜手段 |
| C.2 | 拒绝局文本 `Refusal.Intro`、`Summary.Refused`、`Refusal.Offer`、`Memory.Stage4.Name/.Desc`、`Summary.FiledUnder.NotFiled` 6 行 | 在 v03/act3 规格与代码注释里被点名，但从未查找；`NotFiledEvent` 有触发却只显示 `Summary.FiledUnder` | 这是**漏接线的 bug**的可能性大于死文本。建议：开一个接线任务，而不是删 |
| C.3 | `URHOfferFlow::HasMinorHand`、`URHRunLaunch::SetStartBlessingKeeper`（GameMode:802 永远拿到空标签） | 注释说明用途（HUD 小手牌刻度 / 起手祝福选择），但从未接上 | 未完成功能：完成或删，二选一 |
| C.4 | `FRHEnemyShotPool`（Enemies/RHEnemyShots，83 行）与 `OfferCloseTrace`（Debug/RHOfferCloseTrace 283 行 + `Tools/OfferCloseTrace.ps1` + `RH.Debug.OfferCloseTrace/UIPickLoop`） | 弹池的替换点已完成（`SetHostileShotSink`），只在无子弹子系统时运行，单元测试可能依赖；OfferCloseTrace 是试玩项 #4 的调查工具 | 弹池：跑测试后决定。OfferCloseTrace：**#4 关闭之前保留** |
| C.5 | 工作树 `magical-kare-33c248` 的 2 个未跟踪资产 `Content/RH/Models/SM_VFX_SmokePuff.uasset`、`Content/RH/VFX/Status/M_VFX_Smoke.uasset` | main 上没有这两条路径（main 的 M_VFX_Smoke 在 Materials/VFX） | 看一眼是否是更新的版本；否则随工作树删 |
| C.6 | 分支 `dev/codex-d2-gamepad` | 修复已经由 d3 上线；分支上独有的是 `Tests/RHGamepadTests.cpp`（84 行）| 救回这个测试文件（cherry-pick）再删分支，或直接删 |
| C.7 | 分支 `wip/skeletal-m1` | 骨骼网格管线试点（URHSkeletalBuild、SK_Enemy_Rusher，33 个文件）；模块骨架已上线，其余是停放的 v0.2 工作 | 1.0 之前要不要骨骼管线？不要就删，要就留分支并在 §17 记替换点 |
| C.8 | `SM_Enemy_Boss` + `SM_Enemy_Boss_Vane`（97 KB）及只被它们引用的 `MI_Unit_Ceramic/Rust`、无人使用的 `MI_Unit_Headlamp` | RHBossDef.cpp:8 说 G15 之前的旧 Boss“留在 lineup 里，这里不用”；`BossVaneMesh` 声明未用 | 删，同时从 `build_unit_models.py` 移除生成，否则下次生成又回来 |
| C.9 | 7 个未在任何文档/脚本出现的调试命令（`RH.BossOutfall.*`、`RH.BossSump.KillNozzle`、`RH.Sump`、`RH.Stress.Elites`） | 刻意的调试工具 | 写进 README 而不是删 |

---

## D. 文档清理（不需要构建，但建议走一次 TextAudit/doc 检查）

- `Docs/02_TechnicalDesign.md` §17：**28 条逐字重复、29 行冗余**。行号对：833/952/1054（同一条三次）；整块重复 977–985 = 1079–1087（9 条）、1093–1095 = 1110–1112；成对：407/413、408/414、440/446、449/469、467/514、557/565、558/566、559/567、561/568、562/569、599/613、951/1053、1223/1301、1231/1309、1245/1315；近似重复 556/564（Warden Forms）、726/727（Tech cards）→ 删第二份。
- `Docs/Playtests/NEXT_playtest.md`：指向 91b65a9（落后 978 次提交），所要求的手柄专测已在 f00052b5 通过 → 删或重写为下一次（外部）试玩。
- `Docs/Art/g14_enemy_models_plan.md`（状态仍是“plan”，G14 已完成）→ 归档；`artbook_v0.3_gaps.md` 关掉 G9 后归档。
- 标注 superseded 的章节：`keeper_identity_spec.md §12.1` 与第 353 行契合表、`build_depth_spec.md §2`（被身份规格取代，§1/§3/§4 仍活）、`zh_glossary.md §9`、`Balance/sims.md §0b/§0c`、`03_Roadmap.md` v0.3 第 6 行 → 删章节，正文替换为链接。
- `coop_evaluation.md`：决策已做、v04 规格已实现 → 归档并修 3 处链接（Roadmap:214、full_game_specs:269、v04:3）。
- `01_GameDesign.md` §5.4“pre-redesign state; where they differ, the spec wins”、§5.5 删除线段落 → 用规格链接替换（这是第六部分“替换不叠加”的第一个实例）。
- README 仍写“20-minute … boss at 20:00”→ 改为 15。
- 缺口：**v0.1 从未打标签**；`release/v0.1` 分支尖端 9d361339 可补 `git tag v0.1`（决定项，非删除）。

---

## E. 建议的派发顺序（给调度者）

| 任务 | 类型 | allowed_files | done_when |
|---|---|---|---|
| **X0 空间回收** | 调度者脚本，非 Codex | 工作树目录、分支、Saved/、.git tmp、笔记 PNG | `git worktree list` 只剩 main（+ 保留项）；`git branch` ≤ 5；Saved/Packages 只剩 v0.3 zip；`git fsck` 干净；记录回收 GB 数 |
| **X1 死代码 High** | Codex change | B.1 列出的约 20 个文件 | Build 0 警告；BuildNonUnity 通过；全套测试数不降（≥ 680）；`rg` 对每个删除符号 0 命中 |
| **X2 标签 + ini + 工具** | Codex change | `Config/Tags/RHBuildDepth.ini`、`DefaultEngine.ini`、`DefaultGame.ini`、两个 patch 脚本、`.claude/errs.py`、两个测试文件 | 3 个测试改用现行标签后通过；Shipping 编译通过；`RH.Spawn.WriteFactoryAssets` 文档行补齐 |
| **X3 文本 28 行** | Codex change | 3 张 CSV、`zh_overrides.csv`、`TextAudit.py`（正则） | Localize 重生成；TextAudit A–F 通过；`.Act2` 行纳入长度检查 |
| **X4 死代码 Medium** | Codex change（X1 之后） | B.2 文件 | 同 X1，加 `RogueHeroEditor` 模块构建 |
| **X5 文档去重** | Codex docs | §17、NEXT_playtest、superseded 章节、README | `doc` 检查：§17 无逐字重复；无 “superseded / spec wins” 字样留在正文 |
| **C 组** | 先进 `13_Decisions/OPEN.md`（或 TDD §17 决定行） | — | 设计师逐条决定后再派 |

总计：A 组约 **400 GB**；B 组约 800 行代码 + 12 个标签 + 28 行文本 + 3 个文件；C 组 9 项待决定。
