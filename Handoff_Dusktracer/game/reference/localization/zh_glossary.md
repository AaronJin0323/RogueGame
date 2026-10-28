# zh-Hans glossary / 中文术语表

> Status: **2026-10-19**, first full zh-Hans quality pass (after the 2026-10-19 human playtest: "the Chinese translation is not good").
> Scope: every recurring term in player-facing text. Proper nouns follow `04_Narrative.md` (§4, §6, §7, §9, glossary); design terms follow the `01_GameDesign.md` glossary. When this file and a narrative doc disagree, the narrative doc wins: change the doc first, then this file.
> **Names:** every Chinese name is authored from the fiction, not translated. See `zh_naming.md` (2026-10-19, designer-directed) for each name's source and reasoning. A new name gets a row there first.
> Pipeline: the reviewed Chinese for every string we changed is in `Content/Localization/zh_overrides.csv` (`Key,Table,zh,en`). `python -I Tools/apply_zh.py` writes it into `Content/Localization/Game/zh-Hans/Game.po`, then `Tools\Localize.bat` compiles the `.locres` (TDD §12, §17 2026-10-19). **The overrides CSV is the source of truth for every key it lists.** Don't hand-edit those msgstr lines in Game.po: the next apply_zh run overwrites them. New strings still get their first zh in Game.po. Move a string into the overrides only when it is revised after review.

---

## 1. Style rules

> **Designer decision 2026-10-19: "只改说明，名字不动"** (change only the descriptions, leave the names). Every description, rules line, tier line, toast, hint and menu text is plain, direct game Chinese (大白话): say what it does first, give numbers explicitly, no literary compression. **Proper nouns stay exactly as listed in §2–§4 and `zh_naming.md`, with no added glosses**: 司雷 / 司炉 / 司潮 / 司幽 / 司曜 / 司驰, 典狱 and the Cast-offs, 加征, 神眷, 身忆, 神血, 神临, 神铸, 预支, 守境, 守夜镇, 回收者, the advance / weapon / modifier / reforging names, Contracts node names, Discovery names. Keeper lines keep ≤ 20 characters and never use 神 for a Keeper. This supersedes the earlier clarity note below wherever that note renamed something (预支 → 祝福, 身忆 → 身体记忆, 加征（难度）, 标准 are restored to 预支, 身忆, 加征, 常例; the marker 逾期, the Shrine state 封存, the Recollection verb 寻回 and the run-summary Ichor lines 回收 / 六司所取 / 留存 are restored too). The status words 标记 and 减速 stay; the scheduler is raising them with the designer.
>
> **Clarity first (designer, playtest 3, 2026-10-19: "中文文本很难懂").** Plain words come first and fiction second. Rules text, status words, card labels and toasts use the common word a player already knows. Fiction lives in **names** (Keepers, advances, weapons, places), not in what a line tells you to do. Status words are mechanical: 感电 · 灼烧 · 减速 → 冻结 · 标记 · 致盲. Earlier fiction words for statuses (勾名, 冰缓) are retired. A rules line says the condition, then who, then the effect, then the number (`命中时有 X% 几率让敌人…`), and the duration is joined by a no-break space. **Card layout:** a card rules line with more than one clause is laid out with explicit line breaks (`\n` in the msgstr) at phrase boundaries. Each line stays within ~10 CJK characters at 1080p, so the text reads as a short list (condition / effect / number) and never strands a character. Check with `RH.UI.CardSheet 1|2` under `-culture=zh-Hans`.

| Rule | Do | Don't |
|---|---|---|
| Register | Short, plain game Chinese. Card text is rules: stat name + signed number. | Literal MT word order (`你不会被允许…`, `…的命中使目标…`). |
| Stat lines | `伤害 +{0}%`, `暴击率 +15%`, `生命上限 +{0}`, `射速 −30%` | `提高{0}%伤害`, `+{0}%伤害` (pick one shape: **name, space, signed value**) |
| Chance | `命中时有 {0}% 几率让敌人感电 3 秒。`, `暴击时有 {0}% 几率让敌人致盲。` (say who is affected) | `{0}% 的命中使目标…`; dropping 有 / 敌人 to save width (clarity pass 2026-10-19: plain beats short) |
| Duration bonus | `灼烧持续时间 +{0} 秒`; after the status is already named in the sentence, `持续 +2 秒` | `持续时间延长 {0} 秒` (mixes two shapes) |
| Card wrap | A card rules line holds ~9–10 CJK characters plus its numbers at 1080p. Put the condition first and the number last in a clause (`命中时 {0}% 几率灼烧 4 秒，每秒 {1} 点伤害。`); on a card `攻速` may stand for 攻击速度 when the full word forces a wrap (`冲刺后 {1} 秒内攻速 +{0}%。`), so a wrap never leaves 2–3 characters alone on a line. Check with `RH.UI.CardSheet 1` / `2` under `-culture=zh-Hans`. | |
| No-break unit | In card rules text, a number and its unit are joined by a **no-break space U+00A0** (`3 秒`), not a normal space, so a wrap never leaves `秒。` alone on a line. ICU line breaking keeps NBSP-joined runs together and breaks at the normal space before the number instead. It looks identical. Used so far in `Blessing.Forge.Kindling/Storm.LiveWire/Shade.Lien.Desc`; add it wherever `RH.UI.CardSheet` shows a number split from its unit. | a normal space there |
| Upgrade stats | `伤害 {0} → {1}`, `弹数 {0} → {1}`, `穿透 {0} → {1}`, `射速 +{0}%`, `尺寸 +{0}%`, `流派：{0}` | `大小` |
| Spacing | A half-width space between CJK and a number or `{arg}`: `持续 {0} 秒`, `16 级`. No space after `+`/`−`. No space between `{arg}` and a CJK name that follows a possessive: `{0}的神临`. | `持续{0}秒` next to `持续 4 秒` on the same card |
| Punctuation | Full-width `，。；：（）——、` in sentences. Keep `·`, `/`, `→`, `×`, `±`, `%`, `°` and `‹ ›` as in EN. Minus is `−` (U+2212), as in EN. | Half-width `,` `;` `:` between CJK clauses; `-` as minus |
| Lists | `Meta.ListSeparator` / `RunSummary.ListSeparator` = `、` | `, ` |
| Format args | Every `{n}` of the EN must survive; reordering is fine (`{1} 秒内…{0}%`). `apply_zh.py` refuses an override that drops or adds one. | |
| Lengths | Keeper tier line ≤ 20 characters, punctuation counted (Narrative §2.4). Card kind / rarity / marker ≤ 4 characters. Hub and prompt labels ≤ 4 characters. | |
| "god" | The word "god" never appears in fiction (Narrative §2.5): no `神明`, `众神`, `神祇`, no bare `神` for a Keeper. A Keeper is `司者`, collectively `六司`. The only sanctioned 神- words are the four fixed terms the narrative docs define: **神血** (Ichor), **神眷** (Favor), **神临** (Intercession), **神铸** (Reforging/Fusion). Nothing else gets 神 (see `Mod.DivineConduit` below). | `第二位神`, `神的祝福` |
| Pronouns | The Reclaimer is `它` in Vigil and Keeper text (Narrative §2.6); the player is `你` in rules text. The Keepers say `我们`. | `他` before a late truth-layer decision |
| Designations | Same string as EN (`ATMOS`, `FOUNDRY`, `CISTERN`, `LEDGER`, `LUMEN`, `RELAY`, `ROOT`). Never translated, never glossed in UI. | `根`, `大气` |

## 2. World and fiction (Narrative)

| EN | zh-Hans | Note |
|---|---|---|
| the Keep | 守境 | the world |
| Vigil | 守夜镇 | the hub town |
| the Breach | 裂口 | what Vigil calls the arena |
| the Yard | 清理场 | the Keepers' word for the arena (a clue, keep distinct) |
| the Keepers | 司者 | collective; also the generic "a Keeper" in running text: `一位司者`, `第二位司者` |
| the Six | 六司 | |
| Patron | 主司 | the Keeper chosen for the run |
| the Reclaimer | 回收者 | |
| the asset | 资产 | Keepers' word for the Reclaimer |
| the Cast-offs | 弃物 | |
| Ichor | 神血 | sanctioned 神- word; meta currency |
| Favor (stake) | 神眷 | the only word for Favor, cards included (`每点神眷 +{1}%`); never 眷顾 |
| Advance (= blessing) | 预支 | a name: the card kind label, the run summary header and the Contracts node names (designer 2026-10-19: names unchanged) |
| Intercession (= Ultimate) | 神临 | |
| Reforging (= Fusion) | 神铸 | card kind; the card body verb is `重铸{0}` |
| Contracts (meta tree) | 契约 | `签约` = buy a node; `长期契约` = a branch |
| Recollections | 忆痕 | recovered = **寻回** (`寻回忆痕：{0}`, `已寻回 {0} / {1}`) |
| Codex | 典册 | |
| Discovery (design word) | 发现 | not shown as a label in v0.1 |
| Renegotiate / Refuse / Hold | 重议 / 拒约 / 保留 | Reroll / Banish / Lock; counts are `次数` |
| Founders | 奠基者 | |
| Shrine / Ledger-stone / Begin | 祠堂 / 账碑 / 启程 | hub labels |
| Filed under | 归档编号 | run summary seed |
| Reclaimed / the Six's share / Retained | 回收 / 六司所取 / 留存 | run summary Ichor lines |
| Commitment (L8 second Keeper) | 立约 | card kind |
| Heat / Quota | 加征 (`加征 {0}`); Heat 0 = 常例 | the Six raise the levy (zh_naming §7.3) |
| Muscle Memory | 身忆 · 惊觉 / 本能 / 架势 | the body's memory, paired with 忆痕 |

## 3. Keepers and Paths (Narrative §4, §14)

| Path key | Honorific | HUD short | Path name (流派) | Designation | Status |
|---|---|---|---|---|---|
| Storm | 司雷 | 雷 | 雷霆 | ATMOS | Shock 感电 |
| Forge | 司炉 | 炉 | 炉火 | FOUNDRY | Burn 灼烧 |
| Tide | 司潮 | 潮 | 寒潮 | CISTERN | Chill 减速 → Freeze 冻结 |
| Shade | 司幽 | 幽 | 幽影 | LEDGER | Mark 标记 |
| Radiance | 司曜 | 曜 | 曜光 | LUMEN | Blind 致盲 |
| Quicksilver | 司驰 | 驰 | 驰驿 | RELAY | — |

Codex titles render `司雷 · ATMOS` (`Codex.EntryTitle`).

## 4. Cast-offs and affixes (Narrative §6, GDD §6)

| EN | zh-Hans | | EN affix | zh-Hans |
|---|---|---|---|---|
| Scrapwing | 碎翼 | | Armored | 重甲 |
| Bulwark | 壁垒 | | Swift | 迅捷 |
| Courser | 驿骑 | | Volatile | 易爆 (explodes on death) |
| Flare Pylon | 耀柱 | | Splitter | 分裂 |
| Breaker | 破冰机 | | Vampiric | 吸血 |
| Tallyman | 算筹 | | Shielded | 护盾 |
| the Warden | 典狱 | | elite | 精英 |

## 5. Progression words — keep these three apart

| EN | zh-Hans | Used for | Example |
|---|---|---|---|
| Level | 等级 / `Lv.` | hero level, weapon level | `Lv.3 → Lv.4`, `{0} Lv.{1}`, `等级 {0}` (level-up title) |
| Rank | 级 | blessing rank, Contract node rank | `1 级 → 2 级`, `新获得 · 1 级`, `{0}/{1} 级`, `每级护甲 +{1}` |
| Tier | 阶 | Path tier at Favor 3/6/9 only (nothing else is a "tier") | `雷霆  2 阶`, `任一流派达到 3 阶` |
| run | 局 | one run | `单局`, `每局`, `本局`, `再来一局` |
| MAX | 满级 | weapon at max level, Contract node maxed (`已满级`) | |

## 6. Cards (level-up screen)

| Slot | EN | zh-Hans |
|---|---|---|
| Kind | New weapon / Upgrade / Modifier / Advance / Reforging / Legendary / Commitment | 新武器 / 武器升级 / 模块 / 预支 / 神铸 / 传奇 / 立约 |
| Rarity | Common / Rare / Epic | 普通 / 稀有 / 史诗 |
| Marker | Guaranteed / Held / Synergy / Overdue | 保底 / 已保留 / 协同 / 逾期 |
| Badge glyphs / rarity pips | + ▲ ◆ ◈ ★ / ● ○ | same symbols (not translated) |
| Body | Mount in a free slot. | 装入空武器槽。 |
| | Level {0} → {1} | Lv.{0} → Lv.{1} |
| | Socket into {0} | 装配到{0} |
| | Rank {0} → {1} / New · Rank {0} | {0} 级 → {1} 级 / 新获得 · {0} 级 |
| | Reforges {0} | 重铸{0} |
| | From the Keeper with the highest stake. | 来自神眷最高的司者。 |
| | A second Keeper takes a stake. | 第二位司者入股。 |
| Title / subtitle | Level {0} / Choose one | 等级 {0} / 选择一项 |

"Modifier" is **模块** (GDD glossary), not 改装. A weapon upgrade says `武器升级` so it can't be confused with a blessing rank-up, which reads `x 级 → y 级` under the kind `预支`.

## 7. Stats and statuses

| EN | zh-Hans | | EN | zh-Hans |
|---|---|---|---|---|
| damage | 伤害 | | HP | 生命 (HUD `生命 {0}/{1}`) |
| attack speed | 攻击速度 | | max HP | 生命上限 |
| fire rate | 射速 | | regenerate / heal | 恢复生命 |
| crit chance | 暴击率 | | armor | 护甲 |
| crit damage | 暴击伤害 | | move speed | 移动速度 |
| crit (noun / verb) | 暴击 | | projectile / bullet speed | 弹速 |
| area | 范围 | | pickup range / radius | 拾取范围 |
| range | 射程 | | XP | 经验 |
| knockback | 击退 | | Luck | 幸运 |
| pierce | 穿透 | | cooldown | 冷却时间 |
| bounce / ricochet | 弹射 | | charge (ultimate) | 充能 |
| Burn | 灼烧 (status) · 燃烧地面 (ground) | | Shock | 感电 |
| Chill / Freeze | 减速 / 冻结 (`叠加 3 层就会冻结`) | | Mark | 标记 (`被标记的敌人`, `几率标记敌人 5 秒`) |
| Blind | 致盲 | | XP gem | 经验晶体 |
| Dash | 冲刺 | | Ready | 就绪 |

## 8. HUD, menus and UI verbs

| EN | zh-Hans | | EN | zh-Hans |
|---|---|---|---|---|
| Kills | 击杀 | | Resume | 继续 |
| Survived / Time survived | 存活 / 存活时间 | | Settings | 设置 |
| PAUSED | 已暂停 | | Back | 返回 |
| DEFEATED — press R / (A) to restart | 已倒下——按 R / (A) 重新开始 | | Choose / Select | 选择 / 确认 |
| VICTORY / DEFEAT | 胜利 / 败北 | | Change | 切换 |
| Run again / Return to {0} | 再来一局 / 返回{0} | | Move / Page / Branch | 移动 / 翻页 / 分支 |
| No Keeper yet | 尚无主司 | | Contract (prompt) | 签约 |
| Graphics: Low/Medium/High/Epic/Custom | 画质：低/中/高/极高/自定义 | | Master volume / Language | 主音量 / 语言 |
| Locked / Complete / Not enough Ichor | 未解锁 / 已满级 / 神血不足 | | Requires {0} | 前置：{0} |
| Sealed (shrine) | 封存 | | Contract signed: {0} | 已签约：{0} |

## 9. Names chosen in this pass (where the old zh was unclear or clashed)

Superseded by [zh_naming.md](zh_naming.md#zh-hans-naming--authored-from-the-fiction).
