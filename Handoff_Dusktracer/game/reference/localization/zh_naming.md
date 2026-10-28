# zh-Hans naming — authored from the fiction

> Status: **2026-10-19, designer-directed** (`Docs/Playtests/2026-10-19_human_2.md` item 6). The designer's note: "结账 as the ultimate of one Keeper is exactly wrong. The Chinese should not be a simple translation from English; the Chinese texts, names and everything should come from the whole game's context."
> Method: each name is written as if the game had been written in Chinese first. It starts from the owner's place in the fiction (`04_Narrative.md` §3–§9, `05_ArtDirection.md` §2/§4) and not from the English word. The English stays as it is. The two languages name the same thing from their own traditions.
> Binding rules kept: no 神 meaning "god" except the sanctioned compounds 神血 / 神眷 / 神临 / 神铸 (Narrative §2.5, §13). Designations (ATMOS …) and honorifics (司雷 …) are unchanged. No borrowed mythology (GDD §2): Chinese *words* with weight are welcome, named figures from folk religion are not.
> **Clarity pass 2026-10-19** (designer: "中文文本很难懂"): names below stand, but rules text, status words and toasts were made plain. Statuses: Mark 勾名 → **标记**, Chill 冰缓 → **减速**. (This pass first renamed 预支 → 祝福 and 身忆 → 身体记忆, and changed Heat to 加征（难度） with Heat 0 = 标准. The designer's decision "只改说明，名字不动" reverted all four, so every name in this file stands unchanged.) Tier and passed-over toasts were reworded into plain speech (see `zh_overrides.csv`). §5's 勾名 row is superseded by this note.
> Format: **fiction source → Chinese → why**. Review in one sitting; to change a name, edit `Content/Localization/zh_overrides.csv` and this file together.

## 0. The six vocabularies

The Keepers are six *offices* (司). Chinese has an old tradition of offices and their working words, and each Keeper's names come from its own office's vocabulary:

| Keeper | Office / domain (Narrative §4) | Word field the names come from |
|---|---|---|
| 司雷 · ATMOS | sky and weather regulation; facet **anger**; terse, impatient | 天时 and the 节气 calendar (weather is scheduled), 雷霆, 霹雳, 怒 |
| 司炉 · FOUNDRY | fabrication and repair; facet **craft**; talks tolerances | the smith's working verbs: 鼓风, 淬, 封火, 开炉, 熔 |
| 司潮 · CISTERN | water, cooling, the deep; facet **grief**; slow, says the least | the reservoir: 蓄, 涌, 堤, 寒, 息 |
| 司幽 · LEDGER | the record of the living and the dead; debts; speaks in entries | the register: 簿, 勾 (the brush tick that strikes a name), 收讫, 宽限, 生息, 清算 |
| 司曜 · LUMEN | light and power; facet **clarity**; answers exactly | 曜, 炫, 冕, 贯日 |
| 司驰 · RELAY | transit and relay; facet **restlessness**; always moving | the 驿 relay-post system: 驿程, 换马, 急递, 驰驿 |

## 1. Intercessions (ultimates, `ST_Ultimates Ult.<Path>.Name`)

| Path | EN | Old zh | **New zh** | Why |
|---|---|---|---|---|
| Shade | Closing | 结账 | **清算** | LEDGER holds the debts of the living and the dead. Its intercession pulls in everything owed (every XP gem), then drains and marks the debtors. 结账 is what a till does at closing time. 清算 is the final settling of *all* accounts, and in Chinese it also carries "a reckoning", which is the weight the designer asked for. 了账 / 销账 were considered: 销账 is clerical, and 了账 means "done" with no menace. |
| Storm | Thunderhead | 雷云 | **雷狱** | The effect is a cage of strikes all around the Reclaimer for 6 s. ATMOS is the angriest facet. 雷云 is only weather; 雷狱, a prison of thunder, is what its anger does to anything standing near. |
| Forge | Forge Nova | 炉火新星 | **开炉** | 炉火新星 was a word-for-word "nova". When a smith opens the furnace door a ring of heat rolls out. 开炉 is also the ceremonial start of a forging, the moment FOUNDRY begins work on you. |
| Tide | Tidal Wave | 怒潮 | **决堤** | CISTERN is a reservoir, and the one Keeper "that could stop". Its single act of violence is the dam giving way: the stored water breaks out along a line, pushes and freezes. 怒潮 gave the quiet, grieving Keeper an anger that belongs to 司雷. |
| Radiance | Sun Lance | 日枪 | **贯日** | A giant beam through the densest line. 白虹贯日 is the classical image of light piercing straight through. 日枪 read as a mistranslated weapon name. |
| Quicksilver | Time Dilation | 时滞 | **急递** | RELAY carries everything in the Keep. 急递 is the urgent dispatch of the relay posts: the road clears, the world slows, and the courier goes through. 时滞 is a physics word for "lag". |

## 2. Paths (`Keeper.<Path>.Path`, shown as `X流派`)

| Path | Old | **New** | Why |
|---|---|---|---|
| Storm | 雷暴 | **雷霆** | 雷霆 carries authority (雷霆之怒, 雷霆手段) and fits a Keeper that commands. 雷暴 is a weather report. |
| Quicksilver | 迅驰 | **驰驿** | 迅驰 is a famous laptop-chip brand (Centrino), so a Chinese player reads an ad. 驰驿 is classical: travelling by relay-post horses, RELAY's own word. |
| Forge / Tide / Shade / Radiance | 炉火 / 寒潮 / 幽影 / 曜光 | kept | 炉火 recalls 炉火纯青 (mastery of craft); 寒潮 is cold water on the move; 幽影 names the Shades the Path raises; 曜光 is LUMEN's own light. |

## 3. Advances (blessings, `ST_Blessings`)

| Keeper | EN | Old | **New** | Why |
|---|---|---|---|---|
| 司雷 | Overcharge (+damage) | 过载 | **雷怒** | The anger facet, said directly: the blessing *is* ATMOS's temper lent to you. |
| 司雷 | Static Focus (+crit) | 静电聚焦 | **引雷** | Drawing lightning to one point, as a lightning rod does. That is precision, which is what crit chance is. |
| 司雷 | Live Wire (hits Shock) | 火线 | **惊蛰** | The solar term when the first thunder wakes the insects. ATMOS keeps the weather calendar, and its first shock wakes everything it touches. |
| 司雷 | Ground Fault (Shock longer, shocked take more) | 接地故障 | **雷痕** | The scar a strike leaves; the marked enemy stays hurt for longer. An electrician's fault code was the wrong register. |
| 司雷 | Thunderclap | 霹雳 | kept | already the native word |
| 司炉 | Bellows (+attack speed) | 风箱 | **鼓风** | Name the act, not the tool: working the bellows makes the fire run faster. |
| 司炉 | Tempered Plating (+armor) | 回火护甲 | **淬甲** | 淬 (quench) is the smith's hardening word and is shorter. |
| 司炉 | Kindling | 引火 | kept | |
| 司炉 | Banked Heat (Burn longer & stronger) | 余温 | **封火** | "Banking a fire" is exactly 封火: covering the coals so the heat lasts. A craft term from FOUNDRY's own work. |
| 司炉 | Ember Trail (dash leaves fire) | 余烬足迹 | **踏火** | Short and active: you walk on fire. |
| 司潮 | Undertow (hits Chill) | 暗流 | **暗涌** | 暗涌 is the surge you don't see and also "hidden feeling", the grief facet. |
| 司潮 | Cold Front | 冷锋 | **寒息** | 冷锋 is a weather-map word and belongs to 司雷's sky. CISTERN breathes cold from the deep. |
| 司潮 | Deep Reserve (+max HP) | 深层储备 | **深蓄** | A reservoir's virtue is how much it holds. 深层储备 sounded like a bank. |
| 司潮 | Deep Cold, Wellspring | 深寒, 涌泉 | kept | |
| 司幽 | Dividend (heal every N kills) | 分红 | **收讫** | The ledger's stamp, "received in full": every N debts collected, you are paid. 分红 is a shareholder word. |
| 司幽 | Foreclosure (Mark longer) | 止赎 | **宽限** | LEDGER grants the debtor more time, so the Mark (勾名) lasts longer. 止赎 is opaque legal jargon in Chinese. |
| 司幽 | Interest (+XP) | 利息 | **生息** | Interest accrues, and 生 is also "life": what LEDGER keeps is a record of the living. |
| 司幽 | Lien (hits Mark) | 留置权 | **入簿** | To be entered in the book. The hit writes the enemy into LEDGER's register. |
| 司幽 | Long Arm (+pickup) | 长臂 | **追讨** | Collecting what is owed from further away. |
| 司曜 | Flashpoint (crits Blind) | 闪点 | **炫目** | 闪点 is chemistry (the flash point of a fuel). 炫目 is what the light does to the eye. |
| 司曜 | Glint, Halo, Afterimage, Corona | 锋芒, 光晕, 残像, 日冕 | kept | already light words |
| 司驰 | Fleet (+move) | 迅足 | **健步** | the natural word for a good walker |
| 司驰 | Light Step (dash/ult cooldown) | 轻步 | **换马** | At each relay post the courier changes horses and goes on fresh. That is what cooldown reduction is for RELAY. |
| 司驰 | Wayfare (shockwave every N m) | 行旅 | **驿程** | the leg between two relay posts; every leg completed, something happens |
| 司驰 | Slipstream, Tailwind | 尾流, 顺风 | kept | |
| all | Final Advance (L16 Legendary) | 终极预支 | **倾囊** | The highest-stake Keeper empties its purse: everything it can still advance, at once. 终极预支 read as a loan advert. |

## 4. Weapons, modifiers, reforgings (`ST_Weapons`, `ST_Modifiers`, `ST_Fusions`)

The Reclaimer's rig is Founder-era hardware, so these names are **military-technical Chinese** (Art §4) rather than Keeper vocabulary. The Keeper vocabulary begins when a Keeper reforges the weapon.

| EN | Old | **New** | Why |
|---|---|---|---|
| Orbital Drones | 环绕无人机 | **浮游炮** | the established Chinese term for orbiting remote guns; 无人机 reads as a consumer drone |
| Gravity Mine | 重力地雷 | **引力雷** | it *pulls* (引力) and then blows; shorter |
| Ring Emitter | 环形发射器 | **环射器** | the pair 环射器 / 旋射器 reads as one weapon family |
| Spiral Projector | 螺旋投射器 | **旋射器** | 投射器 is a slide projector |
| Seeker Swarm | 追猎蜂群 | **猎蜂** | two characters, the image intact |
| Pulse Rifle, Scatter Cannon, Rail Lance, Flak Mortar, Arc Blade | 脉冲步枪, 霰弹炮, 磁轨长枪, 破片迫击炮, 弧刃 | kept | already the native technical terms |
| Crit Lens | 暴击透镜 | **聚焦透镜** | a part is named for what it is; "crit" is the effect, not the object |
| Homing Gyro | 追踪陀螺 | **寻的陀螺** | 寻的 is the military term for homing guidance |
| Magnify | 放大 | **扩径** | a gun part widens the bore (径); 放大 is a camera button |
| Scatter Shell | 霰弹壳 | **子母弹** | the exact Chinese term for a shell that bursts into sub-munitions |
| Split Core | 分裂核心 | **裂芯** | short, physical |
| Lingering Field | 残留力场 | **滞留场** | |
| Crucible (Forge × Flak Mortar) | 坩埚 | **熔雨** | 坩埚 is lab glassware. FOUNDRY's reforging makes the shells fall like molten rain. |
| Collectors (Shade × Seeker Swarm) | 收账者 | **索命者** | LEDGER's hounds hunt the marked down and take the debt in full. 索命 (to come for a life that is owed) has the weight; 收账者 was a debt-agency clerk. Deliberately not 勾魂使, a named figure of folk religion (no borrowed mythology). |
| Relay Blades (Quicksilver × Orbital Drones) | 中继刃 | **驿刃** | 中继 is a network repeater; 驿 is RELAY's own word |
| Arc Line, Frostring, Clarity | 雷弧, 霜环, 澄明 | kept | |

## 5. Cast-offs and statuses

| EN | Old | **New** | Why |
|---|---|---|---|
| Courser (RELAY transit unit, runs old straight routes) | 疾行 | **驿骑** | It is a relay-post runner still running its route. 疾行 is an adjective, not a name. |
| Tallyman (LEDGER tally unit, rotating ledger drum) | 记账机 | **算筹** | A cash-register word again. 算筹 are the ancient counting rods, the oldest Chinese tally, and LEDGER's tool. The Codex line "算筹倒下时仍在计数" now reads literally. |
| Mark (Shade status) | 标记 | **勾名** | When LEDGER marks you, it ticks your name in its book (勾 is the brush stroke that strikes a name off a register). Rules text: `几率勾名 5 秒`, `被勾名的敌人`. |
| Scrapwing, Bulwark, Flare Pylon, Breaker, the Warden | 碎翼, 壁垒, 耀柱, 破冰机, 典狱 | kept | Narrative §6 names that already come from the fiction (the Bulwark is literally a wall that walks) |
| Shock, Burn, Chill/Freeze, Blind | 感电, 灼烧, 冰缓/冻结, 致盲 | kept | mechanical clarity wins for the other statuses |

> **Designer decision 2026-10-19 (final):** the shipped names stay: **驿骑** (Courser) and **算筹** (Tallyman). The Cast-off art book boards (弃物卷 PL.C06 / C12) still print the earlier drafts 疾行 / 记账机; the in-game text, the Codex bestiary (`Codex.Enemy.Rusher/Support`) and every other string use 驿骑 / 算筹, and the boards follow the game.

## 6. Places, world and UI words (checked, mostly kept)

| EN | zh | Verdict |
|---|---|---|
| the Keep / Vigil / the Breach / the Yard | 守境 / 守夜镇 / 裂口 / 清理场 | kept. 守夜 (the night watch) is right for a town built against the Breach wall. |
| the Reclaimer | 回收者 | **kept on purpose.** A Chinese reader hears "recycling" (废品回收), and on the truth layer that is exactly what the Keepers do to it every run (Narrative §5). The surface reading is mundane and the truth reading is grim, which is Narrative §0's double reading. |
| Shrine / Contracts / Codex / Ledger-stone / Begin | 祠堂 / 契约 / 典册 / 账碑 / 启程 | kept |
| Ichor / Favor / Intercession / Reforging / Advance | 神血 / 神眷 / 神临 / 神铸 / 预支 | kept (Narrative-sanctioned) |
| Renegotiate / Refuse / Hold | 重议 / 拒约 / 保留 | kept: contract verbs |
| Contracts trunk: Fair Odds (+Luck) | 公平赔率 → **公道** | the trunk is "your own terms"; 公道 (a fair deal) is contract language, and 赔率 is a betting shop |
| Contracts trunk: Warm Start | 热启动 → **预热** | 热启动 is a computer reboot |
| Discovery: Long Shift (survive 10:00) | 漫长班次 → **长夜班** | the job is done for a town called the Night Watch |

## 7. Batch 2 (designer decisions 2026-10-19): passed-over lines, Muscle Memory, Heat, Warden Forms

### 7.1 Passed-over lines (`ST_Keepers Keeper.<Path>.PassedOver`, Narrative §4.3)

The Keeper you did not choose at L8 speaks once. Each line uses that Keeper's register from §0, stays ≤ 20 characters and never explains. The zh is in `zh_overrides.csv` already. `apply_zh.py` reports these keys as "unknown" until the implementing session adds the EN rows, and then applies them automatically.

| Keeper | EN draft | **zh** | Why |
|---|---|---|---|
| 司雷 | Slow choice. I will remember it. | **选得太慢。我会记住。** | ATMOS's two complaints in four characters each: slowness, then a grudge. No softening particle; the anger is in the bareness. |
| 司炉 | A second-rate fitting. It will hold, for now. | **次等配件。暂且堪用。** | A workshop verdict on a part. 堪用 is the inspector's word for "passes, barely". Pride shows as contempt for the fit, not for you. |
| 司潮 | Good. Go with them. I will wait. | **也好。随他们去。我会等。** | 也好 is resignation, not approval. CISTERN's line is the only one with warmth, and the warmth is in 我会等. |
| 司幽 | Noted. Your account with me stays open. | **已记录。你我之间的账，未结。** | LEDGER speaks in entries: 已记录, the same word as its Codex line on the Warden. 未结 sets up its intercession 清算. |
| 司曜 | You looked, and still chose wrong. | **你看过了，仍然选错。** | LUMEN answers exactly and only: one clause of fact, one of verdict. |
| 司驰 | Fine. I carry you either way. | **无妨。横竖都是我送你。** | RELAY carries everything. 送 is a courier's word and also "to see someone off". Truth-layer readers will hear the second meaning, which is Narrative §0's double reading. |

### 7.2 Muscle Memory → **身忆** (GDD §4.3)

The Reclaimer's own passive, "as if the body remembers what the mind was wiped of". The mind's recovered memories are **忆痕** (Recollections), so the body's are **身忆**, and the pair reads as a set. It is deliberately not 肌肉记忆, which is a sports-science phrase. The stages follow how a body learns, from jolt to instinct to stance:

| Stage | EN | **zh name** | zh description (draft for the row) | Why |
|---|---|---|---|---|
| 1 | Flinch | **惊觉** | 每局一次，致命一击会让你留下 1 点生命，并获得 1 秒无敌。 | the body jolts awake at the edge of death before the mind does |
| 2 | Reflex | **本能** | 冲刺穿过敌人正在显示的攻击预警时，返还一半冲刺冷却。 | what the body now does without being told |
| 3 | Form | **架势** | 每局拿到的第一件武器可升至 Lv.9。 | 架势 is a fighter's settled stance; the body has a shape of its own again |
| toast | `MuscleMemory.Toast` Muscle memory: {0} | **身体记起了：{0}** | | the body, not the mind; mirrors 寻回忆痕：{0} |
| locked | `MuscleMemory.Locked` Not yet remembered. | **身体尚未记起。** | | pairs with the toast; Recollections say 尚未寻回 |

### 7.3 Heat / Quota → **加征** (GDD §8.5)

The Six raise the quota after each win. 加征 is a levy raised on top of the usual tax, and Chinese history gives it the right weight (each added levy is a turn of the screw on Vigil). It reads at once as "more is demanded of you". Levels are shown as `加征 {0}` (a digit, matching the HUD). Heat 0 is **常例**: the standing levy, the baseline that 加征 adds to. Settled 2026-10-19 over the provisional 定额 (hub ca8b056): a fixed quota has no pressure, while 加征 says the Six are asking for more. Considered and rejected: 定额 is neutral and has no pressure, 指标 is a modern office target, and 课额 is too obscure.

| Level | zh description (draft for the row) |
|---|---|
| 1 | 耀柱自 6:00 起就发射四连扇形弹（原为 12:00）。 |
| 2 | 每个精英多带一个词缀；5:00 的精英波次也带词缀。 |
| 3 | 事件叠加：每次事件都会附带另一事件的弱化版。 |
| 4 | 驿骑连冲：第一次冲锋后紧接第二次，预警同时显示。 |
| 5 | 典狱的狂暴阶段在生命 50% 时开始（原为 30%）。 |

### 7.4 Warden Forms (GDD §6; only if the UI names them)

Layer 3 at most: a hint that *the Six were one*, never that the Warden guards the Original (Layer 4).

| Form | **zh** | Why |
|---|---|---|
| I | **典狱 · 封守** | it holds a fixed point: a sealed guard |
| II | **典狱 · 护持** | it retreats to protect what is inside. 护持 says it protects without saying what. |
| III | **典狱 · 六合** | the six Keeper colours light up together. 六合 (the six directions joined, "all under heaven") hints that the six are one whole without naming it. |

### 7.5 Batch 4: return line and the Form III Codex entry

| Key | EN draft | **zh** | Why |
|---|---|---|---|
| `ST_World Hub.Recovered` (GDD §8.6) | Asset recovered. Repairs this cycle: {Count}. | **资产已回收。本周期返修：{Count} 次。** | This is the Keep's own system voice, not a Keeper's, so it reads like a work-order log: 资产 is the Keepers' word for the Reclaimer (Narrative §5), and 返修 means "sent back for repair", what FOUNDRY does on schedule. 已回收 echoes 回收者: Vigil hears "reclaimed", a truth-layer reader hears "recycled". No particles, no warmth. |
| `ST_Codex Codex.Warden.FormIII` (Narrative §10, Layer 3) | Six conduits, one housing. It was built to carry all of them at once. | **六道导管，共一具机身。它被造出来，就是要同时承载这六者。** | Vigil's plain record of what it saw. 共一具机身 hints that the Six share one origin (Layer 3). It never says what is inside or whose body (no Layer 4). Two sentences, as the Codex rule requires. |

Both are in `zh_overrides.csv`. They show as "unknown" until the EN rows land, so add the EN exactly as above.

### 7.6 Imbue lines and Favor (weapons b702807)

Blessings imbue every bullet with the Keeper's feature (`ST_Blessings Imbue.<Path>.Rules`). The lines use the glossary's card shape: `所有子弹：命中时 {0}% 几率<status>（每点神眷 +{1}%）。`. Each status is its glossary word: 跳出电弧, 灼烧, 冰缓, **勾名** (the provisional line said 标记), 光脉冲 (as on Corona), 弹射.
**Favor stays 神眷**, the Narrative-sanctioned term already on `World.Favor` and in the Contracts text. The provisional 眷顾 is replaced: two words for one stat would confuse the card and the Contracts screen.

## 8. v0.2: the Outfall (full_game_specs §1; names are FText, keys are Arena.Depth2 / roles)

| EN | **zh** | Why |
|---|---|---|
| the Outfall (arena 2; Vigil's name) | **泄口** | It pairs with 裂口 (the Breach): the crack the Cast-offs come out of, and the outlet the Yard drains into. One plain place word, like 裂口. |
| the Floodgate (arena boss, a sluice machine in the basin wall) | **泄洪闸** | Says exactly what it is (a flood-release gate) and shares 泄 with 泄口. Three characters, like a machine name; 典狱 stays the only two-character boss. |
| Dredge (`Lurker`, CISTERN dredge pump; swims submerged, surfaces under you) | **浚泵** | 浚 is the dredging verb (疏浚), 泵 the pump: a machine name in the 碎翼 / 壁垒 pattern (what it does + what it is). |
| Linesman (`Tether`, RELAY cable layer; pairs hold a beam) | **缆工** | The worker who lays cable. 工 keeps it a working machine, not a monster, as Narrative §6 asks of every Cast-off. |
| Slagger (`Artillery`, FOUNDRY slag thrower) | **渣炮** | It throws slag: 渣 (slag) + 炮 (what lobs shells). Short and readable over a red landing circle. |
| Keeper Bids (v0.2, GDD §12.1): {0} bids / Binding / Lien / Collateral | **{0} 出价** / **绑定** / **抽成** / **抵押** | Plain trade words. Lien is **抽成** (a cut), not 留置/入簿: 入簿 is already Shade's blessing Lien (§3). |

## 9. v0.2: Keeper identity (keeper_identity_text.md, 2026-10-20)

Rows in `side-session-notes/zh_rows_keeper_identity_5f751ce.csv` (108, zh_overrides form; each owner appends with their EN rows).

| Term (EN) | zh | Note |
|---|---|---|
| Archetypes, Storm / Forge / Tide | 暴风 · 风暴墙 · 电弧网 · 倾盆 / 铁锤 · 熔炉 · 重砸 / 漩涡 · 防波堤 · 霜冻 · 止水 | key cards carry `Card.Key` 核心 |
| Archetypes, Shade / Radiance / Quicksilver | 猎犬 · 收债人 · 账簿 / 光矛 · 棱镜 · 烈日 / 矢量 · 动量 · 弹射 | 收债人 (archetype) is distinct from the blessing 讨债人 |
| Patron traits | 静电天幕 · 贴身活 · 冷手 · 记账 · 清明 · 不歇 | `Shrine.Trait` = 主司特性：{0} |
| Renamed blessings | Eye Wall 风眼墙 · Hammer Hand 锤手 · Permafrost 永冻 · Deep Calm 静渊 · Radiant Burst 光爆 | keys unchanged (D-39) |
| Renamed transformations | Zenith 天顶 · Slipstream Drive 滑流 | |
| Mechanic words | volley 齐射 · cleave 横斩 · slam 重砸 · charged shot / lance 蓄能弹 / 蓄能光矛 · sky bolt 天雷 · split / child 分裂 / 分出的子弹 · Debt 债 (笔) · Payday 清账 · Shades 亡影 · execute 斩杀 · Load 负载 | plain words; no new 神 |

## 10. v0.3: the Sump and the Refusal (act3_text.md, 2026-10-20)

| EN (role / key) | zh | Why |
|---|---|---|
| the Sump (arena 3, `Arena.Depth3`) | **沉井** | A sunk shaft: where the Yard's water and freight finally settle. Pairs with 裂口 / 泄口 as a plain place word. |
| Ferry (`Carrier`, RELAY freight drone that drops a pack of Scrapwings) | **驿驮** | RELAY's 驿 family (the Courser is 驿骑); 驮 is a pack load on the back, which is exactly what it drops. Not 渡舟: a boat collides with the Ferryman (摆渡人) and the Sump has no water. |
| Tapper (`Siphon`, LEDGER collection unit that takes XP gems) | **抽筹** | LEDGER's counting-rod words (the Tallyman is 算筹): it draws off the tally chips you leave on the ground, and echoes the Clerk's 抽走了一份. Not 汲手: drawing water is CISTERN's vocabulary. Distinct from the Bid term 抽成 (§8). |
| the Font (`Enemy.Boss`, the Sump's seeding engine; makes Cast-offs) | **母机** | 工作母机 is the machine tool that makes machines, which is what the Font is; 母 also reads as birth. Two characters by exception: it is the Warden's housing shape, so the two housings carry the only two-character boss names (典狱 / 母机). Not 源泉 (too close to Wellspring 涌泉, and soft). |
| Own (Card.Family.Own: the plaque on Tide's cards in the Reclaimer frame after its fate) | **归己** | "Come home to its own": it echoes Codex.Fate.Tide (都回到了回收者身上). The first draft 自有 read as corporate "self-owned". |
| Drafting (Quicksilver amplifier, keeper identity pass 2) | **借势** | Its old 尾流 duplicated Slipstream's name in the same pool; 借势 (borrowing momentum) fits the Momentum archetype. |
| Refusal (run mode, Muscle Memory stage 4, Shrine row) | **拒约** / **无约** | The glossary verb (Refuse = 拒约). Stage 4 keeps it although stages 1-3 are body words (惊觉 / 本能 / 架势): the stage is the mode. |
| ROOT, FOUNDRY, LEDGER … in Act III text | kept in Latin | designations are never localised (Codex.Root.Title, Ending.Ledger). |
| Tamsin's rhyme, finished (Ending.Rhyme) | **六根手指，一只手，缺的那只手……那只是你的。** | Finishes her Act II phrasing (六根手指，一只手，那只手是……) in the same counting-song rhythm. |

## 11. Keyword hover notes (playtest 6 item 7)

The plain definitions behind the card keywords (横斩, 分裂, 齐射 …) live in `Docs/Localization/keyword_glossary.md`. That file is also the term list: a card, toast or Codex line that uses one of its mechanics uses its zh word, never a synonym (no 受寒 / 冰冻 / 处决 / 弹跳 / 带电 / 追猎 / "之敌"; fixed 2026-10-20).
