# S2 zh-Hans audit (全量中文)

Branch `dev/s2-zh`, from `dev/s2` at `aef431c`. Scope: the zh-Hans column of the eight `ST_` tables, against `zh_overrides.csv`, `zh_glossary.md`, `zh_naming.md`, `keyword_glossary.md` and `Docs/Glossary.md`. No en text, Context, Kw or row was changed. ST_UI is being added to on another branch, so rows that branch adds are not read here.

## Result

| Reading | Value |
|---|---|
| Rows | 637 in 8 tables |
| Rows with no zh-Hans | 0 |
| Argument mismatches, including repeat counts | 0 |
| Literal `FromStringTable` keys with no row | 0 |
| Character budgets stated in Context, over budget | 0 |
| zh cells changed | 121 |
| `loc` before the fixes, with the new checks | FAIL, 97 violations, some of them false positives since narrowed |
| `loc` after | PASS, 637 rows |

## What changed

| Class | Change | Source |
|---|---|---|
| Wrong name | The Warden 守望者 → 典狱 (3 rows) | zh_glossary §4, zh_naming §5 |
| Retired or drifted term | 冰缓 → 减速, 追猎 → 追踪, 僵直 → 眩晕, 拉拢 and 拉拽 and 拉动 → 吸拢 | zh_naming §5, keyword_glossary Rules |
| One word per mechanic | 弹体 → 子弹, 债务 → 债 (笔), 发薪 → 清账, 过量伤害 → 多余伤害, 火地 → 燃烧地面, 受击 → 被击中, 暴击裂成 → 暴击分裂成, 升起 → 召出 when a Shade is raised | overrides, keyword_glossary, zh_naming §9 |
| Exact en match with the overrides | Blind, Seek, Debt, Momentum notes; Held 已保留; Hone 磨砺; Spare Terms 备用条款; Stored Sky 蓄天; Chasing Storm 追雷; Paused 暂停中 | `zh_overrides.csv`, which zh_glossary names as the truth for its keys. Line breaks were not copied: this card wraps its own lines |
| Meaning | `Duo.ShadeRadiance.Desc` read as damage dealt, now 受到的伤害. `Duo.StormRadiance.Desc` said 风暴, which is not a word for Storm, now 司雷. Split children read as 子弹, now 分出的子弹 | en |
| Layout words | `Lv.{Level}`, `Lv.{From} → Lv.{To}`, 装配到{Weapon}, 装入空武器槽。, 切换 for Change, 「」 quotes, the double ellipsis in 请按一个键…… | zh_glossary §5, §6, §8 |

`Pause.Title` keeps a stale `[invented]` in Context: its zh now comes from the overrides (`HUD.Paused`, 暂停中), while zh_glossary §8 says 已暂停. The overrides win by the glossary's own rule. Context was outside this change.

## Gate check

`Tools/loc_build.py --check` (T-TEC-08) gained these, each failing test seen red first (9 of 56 tool tests, then `loc` FAIL on the real tables):

- Arguments compared as counts, not sets; a broken brace fails.
- `ZH_RETIRED`: a retired word fails in any row, name rows included.
- `ZH_EXACT`: a string that is exactly a glossary term has its one zh.
- `ZH_TERMS`: outside `.Name` rows an en game term needs its zh word.
- Style: untranslated rows unless Context says "do not translate", half-width punctuation next to CJK, a number argument set off from CJK by a space, the zh budget stated in Context.
- `check_code_keys`: each literal `FromStringTable(TEXT("ST_X"), TEXT("Key"))` in Source has its row.

Not caught: keys built at run time, and meaning. `Settings.Graphics.Epic` 极高 and the rarity 史诗 share an en, so `Epic` is left out of `ZH_EXACT`. A row added to ST_UI on another branch that says 守望者 or breaks a term will now fail `loc` at merge, which is the intent.

## Overflow

Off-screen, 1280×720, `-RenderOffScreen`, `-culture=zh-Hans`, Level, Patron, Keeper and Reforging hands and the HUD with the boss bar, at text 100% and 150% (`dt.Offer.AutoPick 0,DT.UI.Set text <pct>,DT.Offer <Hand>,DT.UI.Shot`). Shots are in `Saved/Smoke/zhs_*`, not committed. Read by eye: Level at both sizes; Patron, Keeper, Reforging and the HUD at 150%. The 100% shots of those four were taken and not read, 150% being the harder case.

- Nothing leaves a card, button or panel at either size. The longest rules text seen takes 3 lines at 150%; the 40-character budget needs about 3 lines at card width, and the card body has far more room.
- At 150% the rarity line of a Reforging card sits on its inner frame line (`过载`, 史诗). Only that card kind; legible.
- The keyword note panel covers the dimmed HUD meters at both sizes, by design of the panel's place.
- A number can part from its unit: 分裂成 3 / 发 at 150%. zh_glossary §1 asks for U+00A0 there and these tables use a plain space. Not changed: it touches most rules rows and needs a look at the wrap code first.
- Not shot: the Keeper meters (Debt, Momentum, Shades, Time Dilation), banners, the reaction toast and the victory line. Their strings are short, and these changes did not lengthen them beyond a character or two.

## zh marked invented (29 rows)

- Duo names, "authored here" in Context (15): 弧炉, 风暴潮, 传唤, 致盲电弧, 雷步, 压力釜, 火葬债, 日斑, 加力, 冷藏, 钻石尘, 滑行, 聚光, 速收, 光速.
- `[invented]` (14): `Pause.Title` (stale, above), `Settings.Percent`, `Settings.Window` and its two values, `Settings.Hint.Volume .Graphics .Shake .Flash .DangerOutline`, `Menu.Hint.Pause .Settings .Controls`, `Controls.Fixed`.

Names not marked but absent from every Handoff source (47, reaction names excluded because reactions.md takes them from source Desc lines): 锋面雷, 回击, 滚雷, 长雷, 蓄雷, 重击, 长柄, 精工, 寒引, 深压, 碎点, 冰裂, 退潮, 召雷, 炉鸣, 岔雨, 天应, 环锤, 余烬地, 首尾砸, 霜触, 屏息, 扣押, 寻迹, 托管, 滞纳, 清偿, 焦点, 锋光, 雪盲, 广角, 光痕, 斜击, 连弹, 接力, 回弹, 状态, 冲刺齐射, 镜射, 连锁扳机, 分叉, 跳弹板, 环绕锁定, 扩散波, 拾荒, 补强, 野火.

## For a native reader

1. The new names above, first the Intercessions 召雷 and 炉鸣: zh_naming §1 gave 雷狱, 开炉, 决堤 to differently named en, so whether those three should return is a naming call.
2. 回流 names both Backwash and Backflow. 弹射 names both Rebound and Ricochet (Glossary lists both).
3. 风暴潮, 钻石尘, 压力釜 and 等离子 are close to literal; zh_naming asks for names from the fiction.
4. 债到期 in `Shade.Desc` and 击杀被标记的敌人记债 in `Shade.Economy.Key.Desc`: 债 alone reads short here. The 笔 unit assumes Debt is counted one per kill.
5. 弹幕 for enemy fire in hints and `Transform.Tide.Desc`, against 弹幕即你 for the player.
6. Clumsy but correct rows left alone: 近战重砸少 {n} 次命中就触发, 灼烧跳伤速度, 分裂成扇形的小子弹.
7. Keyword `Reclaimer` and `Intercession` in the Kw column have no `ST_Keywords` row. Whether a note shows for them was not checked; adding rows was out of scope.
