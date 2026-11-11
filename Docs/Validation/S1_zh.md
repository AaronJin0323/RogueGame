# S1 zh-Hans: the S2 audit carried down to `main`

**Verdict: the 59 zh cells the `dev/s2` audit corrected in rows that exist on `main` now read as they do on `dev/s2`, and `loc` carries the audit's checks. Before: the new checks FAIL `main`'s tables with 37 violations. After: `LOC: PASS (5 tables, 267 rows)`. No native reader has read any of it (O-008).**

Class PRESENT, text layer. It replaces the 59 cells below and nothing else: no en text, key, Context, Kw or row changed, and no S2 row came down. Queue item 5, `plans_audit.md` 5.10.

## What was carried
- Source: `dev/s2` commit `954d67c` (the audit; its note `S2_zh.md` is on `dev/s2` and comes to `main` when S2 lands) and the later zh edits `dev/s2` made to the same rows. Every `main` row was compared with its `dev/s2` row by key. The en column is identical in all 267, so each differing zh cell is a translation of the same en; each was replaced by the `dev/s2` line, byte for byte, so the carry of `main` into `dev/s2` meets identical text.
- One difference was left: `Offer.Kind.Reforging` Context says "(S2 sources)" on `main` and not on `dev/s2`. It is a translator note and it is true on `main`.
- `Tools/loc_build.py` is now byte-identical to `dev/s2`. Its term lists name S2 words (`Debt`, `Shade`, `Momentum`); they match no row on `main` and cost nothing. `Tools/tools_test.py` gains the audit's `Loc` class (13 tests, 80 to 93), and `Gate.ps1` shows the `ST_` and `Source/` lines of a failing step.
- The check that every literal `FromStringTable(TEXT("ST_X"), TEXT("Key"))` in `Source` has its row found nothing missing on `main`.

## Not done, not verified
- No frame was shot for overflow on `main`. The longest changed cell grows by 12 characters (`Forge.Slam.Trait.Desc`); `S2_zh.md` read the same strings on cards at 100 % and 150 % on `dev/s2` and found nothing leaving a card.
- The audit's open questions for a native reader (`S2_zh.md`, "For a native reader") stand for these rows too: 召雷 and 炉鸣, 弹幕 for enemy fire, the clumsy but correct 近战重砸少 {n} 次命中就触发.

## Cells (old, new)
| Table | Key | Before | After |
|---|---|---|---|
| `ST_Keepers` | `Storm.Desc` | 弹体分裂，电弧在敌人间跳跃，天雷落下。命中可让敌人感电。 | 子弹分裂，电弧在敌人间跳跃，天雷落下。命中可让敌人感电。 |
| `ST_Keepers` | `Forge.Desc` | 弹体化作横斩，火地蔓延，重砸震地。命中可让敌人灼烧。 | 子弹化作横斩，燃烧地面蔓延，重砸震地。命中可让敌人灼烧。 |
| `ST_Keepers` | `Tide.Desc` | 命中会拉拢并减速敌人，减速叠满即冻结；站定可以蓄力。 | 命中会吸拢并减速敌人，减速叠满即冻结；站定可以蓄力齐射。 |
| `ST_Keepers` | `Storm.Tempest.Amp1.Desc` | 分裂多 {children} 发，子弹伤害 +{child_dmg_pct}%。 | 分裂时多分出 {children} 发，分出的子弹伤害 +{child_dmg_pct}%。 |
| `ST_Keepers` | `Storm.Tempest.Trait.Desc` | 子弹会再分裂一次。 | 分出的子弹会再分裂一次。 |
| `ST_Keepers` | `Storm.ArcGrid.Key.Desc` | 命中后电弧跳向 {range_uu_m} 米内 {jumps} 个敌人，造成 {dmg_pct}% 伤害。 | 命中后电弧再跳向 {range_uu_m} 米内 {jumps} 个敌人，造成 {dmg_pct}% 伤害。 |
| `ST_Keepers` | `Storm.ArcGrid.Amp1.Desc` | 电弧让敌人感电。感电的敌人受电弧伤害 +{dmg_vs_shocked_pct}%。 | 电弧会让敌人感电。感电的敌人受到的电弧伤害 +{dmg_vs_shocked_pct}%。 |
| `ST_Keepers` | `Storm.ArcGrid.Trait.Desc` | 电弧击杀敌人时分出 {fork_on_kill} 道新电弧。 | 电弧击杀敌人时，分成 {fork_on_kill} 道新电弧。 |
| `ST_Keepers` | `Forge.Furnace.Key.Desc` | 命中时 {chance_pct}% 几率灼烧；命中灼烧的敌人留下火地 {duration_s} 秒。 | 命中时 {chance_pct}% 几率灼烧；命中灼烧的敌人留下燃烧地面 {duration_s} 秒。 |
| `ST_Keepers` | `Forge.Furnace.Amp2.Desc` | 火地持续 +{duration_s} 秒，灼烧层数上限 +{burn_stacks_max}。 | 燃烧地面持续 +{duration_s} 秒，灼烧层数上限 +{burn_stacks_max}。 |
| `ST_Keepers` | `Forge.Slam.Key.Desc` | 冲刺落地、每第 {n} 次近战命中和受击时，在 {radius_uu_m} 米内重砸。 | 冲刺落地、每第 {n} 次近战命中和每次被击中时，都会在 {radius_uu_m} 米内重砸一次。 |
| `ST_Keepers` | `Forge.Slam.Amp1.Desc` | 受击后，下一次重砸伤害 ×{after_hit_taken_mult}。 | 被击中后，下一次重砸伤害 ×{after_hit_taken_mult}。 |
| `ST_Keepers` | `Forge.Slam.Amp2.Desc` | 重砸溅出 {embers} 颗火星，命中即灼烧。 | 重砸抛出 {embers} 颗余烬，命中的目标会灼烧。 |
| `ST_Keepers` | `Forge.Slam.Trait.Desc` | 重砸引爆范围内所有灼烧的敌人。 | 重砸时，范围内所有灼烧中的敌人立刻受到剩余的灼烧伤害。 |
| `ST_Keepers` | `Tide.Whirlpool.Key.Desc` | 命中把 {radius_uu_m} 米内的敌人拉向落点，{chance_pct}% 几率减速，相撞受 {crush_dmg_pct}% 伤害。 | 命中把 {radius_uu_m} 米内的敌人拉向落点，{chance_pct}% 几率减速，相撞受到 {crush_dmg_pct}% 挤压伤害。 |
| `ST_Keepers` | `Tide.Whirlpool.Amp1.Desc` | 被拉动的敌人获得 {chill_stacks_on_pull} 层减速。 | 被吸拢的敌人获得 {chill_stacks_on_pull} 层减速。 |
| `ST_Keepers` | `Tide.Whirlpool.Amp2.Desc` | 相撞伤害 +{crush_dmg_pct}%。 | 挤压伤害 +{crush_dmg_pct}%。 |
| `ST_Keepers` | `Tide.Whirlpool.Trait.Desc` | {cluster_freeze_min} 个以上被拉动的敌人挤在一起时，一同冻结并碎裂。 | {cluster_freeze_min} 个以上被吸拢的敌人挤在一起时，一同冻结并碎裂。 |
| `ST_Keepers` | `Tide.Frost.Key.Desc` | 命中时 {chance_pct}% 几率减速，{freeze_stacks} 层冻结；冻结的敌人受伤 +{frozen_vuln_pct}%。 | 命中时 {chance_pct}% 几率减速，{freeze_stacks} 层冻结；冻结的敌人受到的伤害 +{frozen_vuln_pct}%。 |
| `ST_Keepers` | `Tide.Frost.Amp1.Desc` | 冻结的敌人像墙一样挡住其他敌人。 | 被冻结的敌人会像墙一样挡住其他敌人。 |
| `ST_Keepers` | `Tide.Frost.Amp2.Desc` | 冻结持续到敌人被命中，最多同时 {held_max} 个。 | 冻结会一直持续到敌人被击中，最多同时冻住 {held_max} 个敌人。 |
| `ST_Keepers` | `Storm.Enchant.Desc` | 弹体拖出雷蓝尾迹，命中有 {chance_pct}% 几率感电。 | 子弹拖出雷蓝尾迹，命中有 {chance_pct}% 几率感电。 |
| `ST_Keepers` | `Storm.Ult.Desc` | {duration_s} 秒内每发弹体分裂成 {children} 发，每发再跳一次电弧。 | {duration_s} 秒内每发子弹分裂成 {children} 发，每发再跳一次电弧。 |
| `ST_Keepers` | `Forge.Enchant.Desc` | 弹体拖出炉火尾迹，命中有 {chance_pct}% 几率灼烧。 | 子弹拖出炉火尾迹，命中有 {chance_pct}% 几率灼烧。 |
| `ST_Keepers` | `Forge.Ult.Desc` | {duration_s} 秒内每 {interval_s} 秒震出火环，击退并灼烧敌人。 | {duration_s} 秒内每 {interval_s} 秒重砸出火环，击退并灼烧敌人。 |
| `ST_Keepers` | `Tide.Enchant.Desc` | 弹体拖出潮青尾迹，命中有 {chance_pct}% 几率减速。 | 子弹拖出潮青尾迹，命中有 {chance_pct}% 几率减速。 |
| `ST_Keepers` | `Storm.Tempest.Tier2` | 分裂多出 {children} 发。 | 分裂时多分出 {children} 发。 |
| `ST_Keepers` | `Storm.Tempest.Ult.Desc` | 神临时弹体扇形分裂成 {children} 发，子弹再分裂一次。 | 神临时子弹扇形分裂成 {children} 发，分出的子弹再分裂一次。 |
| `ST_Keepers` | `Storm.Tempest.Layer.Desc` | 作为第二位司者：神临时弹体也分裂成 {children} 发。 | 作为第二位司者：神临时子弹也分裂成 {children} 发。 |
| `ST_Keepers` | `Storm.ArcGrid.Layer.Desc` | 作为第二位司者：神临时命中跳向 {jumps} 个敌人并感电。 | 作为第二位司者：神临时命中的电弧跳向 {jumps} 个敌人并感电。 |
| `ST_Keepers` | `Forge.Furnace.Tier1` | 火地范围 +{radius_uu_m} 米。 | 燃烧地面范围 +{radius_uu_m} 米。 |
| `ST_Keepers` | `Forge.Furnace.Tier2` | 火地每秒伤害 +{dps_pct}%。 | 燃烧地面每秒伤害 +{dps_pct}%。 |
| `ST_Keepers` | `Forge.Furnace.Layer.Desc` | 作为第二位司者：神临时落点留下火地 {duration_s} 秒。 | 作为第二位司者：神临时落点留下燃烧地面 {duration_s} 秒。 |
| `ST_Keepers` | `Forge.Slam.Tier2` | 近战重砸提前 {n} 次触发。 | 近战重砸少 {n} 次命中就触发。 |
| `ST_Keepers` | `Tide.Whirlpool.Tier1` | 拉拽距离 +{distance_uu_m} 米。 | 吸拢距离 +{distance_uu_m} 米。 |
| `ST_Keepers` | `Tide.Whirlpool.Tier2` | 被拉动的敌人受伤 +{pulled_vuln_pct}%。 | 被吸拢的敌人受到的伤害 +{pulled_vuln_pct}%。 |
| `ST_Keepers` | `Tide.Whirlpool.Tier3` | 每第 {every_n_volley} 次齐射从 {radius_uu_m} 米外拉拽。 | 每第 {every_n_volley} 次齐射从 {radius_uu_m} 米外吸拢。 |
| `ST_Keepers` | `Tide.Frost.Ult.Desc` | 神临冻结全屏 {duration_s} 秒，冻结的敌人受伤 +{frozen_vuln_pct}%。 | 神临冻结全屏 {duration_s} 秒，冻结的敌人受到的伤害 +{frozen_vuln_pct}%。 |
| `ST_Modifiers` | `SplitCore.Desc` | 命中时，弹体分裂成一扇小弹。 | 命中时，子弹分裂成扇形的小子弹。 |
| `ST_Modifiers` | `ScatterShell.Desc` | 弹体飞完或引爆时，炸成一圈子弹。 | 子弹飞完或引爆时，炸成一圈子弹。 |
| `ST_Modifiers` | `Pierce.Desc` | 弹体可以穿透更多敌人。 | 子弹可以穿透更多敌人。 |
| `ST_Modifiers` | `LingeringField.Desc` | 命中、横扫和爆炸处留下伤害区。 | 命中、横扫和爆炸处留下伤害区域。 |
| `ST_Modifiers` | `Backwash.Desc` | 命中时把敌人拉向弹体射来的方向。 | 命中时把敌人拉向子弹射来的方向。 |
| `ST_Modifiers` | `Fork.Desc` | 每次齐射在两侧加射弹体。 | 每次齐射在两侧加射子弹。 |
| `ST_Modifiers` | `Ricochet.Desc` | 命中的弹体会弹向新的敌人。 | 命中的子弹会弹射到新的敌人身上。 |
| `ST_UI` | `Level.Label` | 等级 {Level} | Lv.{Level} |
| `ST_UI` | `Offer.Marker.Held` | 保留 | 已保留 |
| `ST_UI` | `Offer.Line.NewWeapon` | 装入空余武器槽。 | 装入空武器槽。 |
| `ST_UI` | `Offer.Line.Upgrade` | 等级 {From} → {To} | Lv.{From} → Lv.{To} |
| `ST_UI` | `Offer.Line.Target` | 装入{Weapon} | 装配到{Weapon} |
| `ST_UI` | `Offer.Reserve.Hone.Name` | 打磨 | 磨砺 |
| `ST_UI` | `Offer.Reserve.SpareTerms.Name` | 余约 | 备用条款 |
| `ST_UI` | `Hud.BossName` | 守望者 | 典狱 |
| `ST_UI` | `Hud.Victory` | 守望者倒下了。本局胜利。 | 典狱倒下了。本局胜利。 |
| `ST_UI` | `Hud.BossIncoming` | 守望者逼近 | 典狱逼近 |
| `ST_Weapons` | `RingEmitter.Desc` | 向四面八方射出一圈弹体。 | 向四面八方射出一圈子弹。 |
| `ST_Weapons` | `OrbitalDrones.Desc` | 浮游炮环绕你，射击靠近的敌人。 | 浮游炮绕着你转，射击靠近的敌人。 |
| `ST_Weapons` | `GravityMine.Desc` | 在敌群中布雷：先把敌人吸过来，再引爆。 | 向敌群投下地雷，先把敌人吸拢，再引爆。 |
| `ST_Weapons` | `Rounds.None` | {Weapon}不再有弹体飞出 | {Weapon}不再有子弹飞出 |
