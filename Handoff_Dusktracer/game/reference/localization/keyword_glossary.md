# Keyword glossary: hover notes for card terms (2026-10-20)

Playtest 6, item 7: the designer found 横斩 confusing. Every game term that appears on cards, toasts, plaques, bids and Intercession text gets a short plain-language note, shown under the card on hover or pad focus. **Final list: 58 terms** (planning, `Docs/Design/keyword_notes_spec.md` §2, which adopts this file as the source for the rows). loc-zh owns the zh; meta-ui builds the UI.

## Rules

- **One mechanic, one word.** The zh column is the only word for that mechanic on cards, toasts and Codex lines. A synonym is a bug (the 2026-10-20 pass replaced 受寒 / 冰冻 / 处决 / 弹跳 / 带电 / 追猎 / 之敌 in the Duo cards, Reaction toasts and Closing). Plain descriptive words may stay in a sentence (Echo is 回响; 天雷 / 落雷 still describe the bolts).
- **Notes are plain and number-free** wherever a blessing changes the number. Fixed facts stay (Chill: 3 stacks Freeze; Favor: 3 / 6 / 9; Transformation: level 16).
- **Length:** EN ≤ 20 words, zh ≤ 40 characters (spec §2, meta-ui's panel). The word "god" / 神 never appears outside 神眷 / 神临 / 神血 / 神铸.
- **Keys:** table `ST_Keywords`; `Keyword.<Id>.Name` is the term as shown, `Keyword.<Id>.Def` the note. `<Id>` is the mechanic's code name, never fiction. Rows with en + zh: shared notes `zh_rows_keywords.csv`.
- **Marking (spec §1):** explicit rich-text markup, `<kw id="Cleave">横斩</kw>`, first occurrence only, at most 3 per card, EN and zh mark the same id set. Matching this file's EN / zh words is the retrofit tool that proposes the markup (`python -I Tools/kw_markup.py <Ids>`, `--apply` to write; only offer-card strings, because other screens do not strip markup yet); it is not a runtime rule. `apply_zh.py` refuses a zh override whose `kw` id set differs from the EN. Done 2026-10-20: Cleave, Volley, Execute (9 strings); Split, Slam, Freeze, Mark, Chill, Shock, Blind, Burn, Ricochet, Pierce (58 strings; the 20 cards balance is rewriting in keeper identity passes 2-3 are skipped with `--skip=` until they land). Keeper traits and Intercession text wait for meta-ui's ee6c1b3 (plain-text stripping) on main.
- **Marking done 2026-10-23:** Crush and Herd (3 offer-card strings: Undertow, Riptide, Cold Front), first occurrence in EN and zh with matching ids; no card reached the 3-mark limit.
- **The Keeper group** (Renegotiate, Hold, Refuse, Binding, Levy, Collateral …) also shows on hover / focus of the offer buttons and bid plaques, from the same table.

## Terms

| Id | Group | EN | zh | EN note | zh note | Appears on |
|---|---|---|---|---|---|---|
| `Burn` | Status | Burn | 灼烧 | Deals damage every second for a few seconds. | 持续几秒，每秒受到伤害。 | Forge blessings, imbue, Duo cards |
| `Shock` | Status | Shock | 感电 | A Shocked enemy takes more damage from everything. | 感电的敌人受到的所有伤害提高。 | Storm blessings, Arc Grid, Duo cards |
| `Chill` | Status | Chill | 减速 | Slows the enemy. Chill stacks: 3 stacks Freeze it. | 让敌人变慢。可以叠加，叠满 3 层就会冻结。 | Tide blessings, imbue, Duo cards |
| `Freeze` | Status | Freeze | 冻结 | The enemy can't move or attack for a short time. Bosses can't be Frozen. | 敌人短时间内不能移动或攻击。首领不会被冻结。 | Tide blessings, Tidal Wave, transforms |
| `Shatter` | Status | Shatter | 碎裂 | When a Frozen enemy dies it shatters, hurting and Chilling enemies around it. | 被冻结的敌人死亡时碎裂，伤害周围的敌人并让它们减速。 | Tide T3, Shatterpoint, Tide transforms |
| `Blind` | Status | Blind | 致盲 | Stuns the enemy briefly: it can't move or shoot. Bosses can't be Blinded. | 让敌人短暂眩晕，不能移动也不能射击。首领不会被致盲。 | Radiance blessings, Duo cards |
| `Mark` | Status | Mark | 标记 | Marks an enemy for the Keeper of the Dead. Tracking shots, hunters and executions go after Marked enemies first. | 为司幽记下这个敌人。追踪的子弹、亡影和斩杀都会优先找上被标记的敌人。 | Shade blessings, imbue, Closing |
| `Execute` | Status | Execute | 斩杀 | Kills an enemy outright once its HP drops below a threshold. Bosses are never executed. | 敌人生命低于一定比例时，直接将其击杀。首领不会被斩杀。 | Shade T3, Final Notice, Audit, Closing |
| `Volley` | Shot | Volley | 齐射 | One firing of a weapon: all the bullets it shoots at once. "Every 4th volley" counts firings, not bullets. | 武器开火一次射出的全部子弹。「每第 4 次齐射」数的是开火次数。 | Storm trait, Cold Front, Capacitor |
| `Cleave` | Shot | Cleave | 横斩 | A sweeping arc in front of you that hits every enemy inside it, instead of a bullet that flies. | 在你面前横扫出一道弧形斩击，击中范围内的所有敌人，而不是射出子弹。 | Forge imbue, Hammer, Forge transform |
| `Slam` | Shot | Slam | 重砸 | A ring that bursts out of the ground around you, hitting and knocking back everything near you. | 以你为中心，从地面砸出一圈冲击，击中并击退身边的一切。 | Forge T2, Ember Trail, Forge Nova |
| `Split` | Shot | Split | 分裂 | A bullet breaks into smaller bullets when it hits or ends its flight. They deal less damage. | 子弹命中或飞到尽头时，分成几发小子弹。分出的子弹伤害较低。 | Storm imbue, Tempest, Thunderhead, Split Core |
| `Echo` | Shot | Echo | 回响 | A delayed strike on the same impact point. | 回响会在同一命中点延迟打出一次攻击。 | Downpour (Thunderclap, Capacitor), Terminal Velocity; 天雷 / 落雷 stay as plain words in the sentence |
| `ChargedShot` | Shot | Charged shot | 蓄能弹 | Every few shots a weapon fires one charged shot that deals multiplied damage. | 每隔几发，武器会射出一发蓄能弹，伤害成倍提高。 | Radiance trait, Lance, Radiance transform |
| `Arc` | Shot | Arc | 电弧 | Lightning that jumps from a hit to a nearby enemy for part of the damage. | 命中时，闪电跳到附近的敌人身上，造成部分伤害。 | Storm imbue, Arc Grid, Duo cards |
| `Chain` | Shot | Chain | 连锁 | An arc that keeps jumping from enemy to enemy, several times. | 电弧在敌人之间连续跳跃好几次。 | Live Wire, Storm transform |
| `Ricochet` | Shot | Ricochet | 弹射 | After a hit, the bullet bounces on to another enemy. | 子弹命中后，弹向另一个敌人。 | Quicksilver imbue, Rebound, Ricochet |
| `Pierce` | Shot | Pierce | 穿透 | The bullet passes through enemies and keeps flying. The number is how many more enemies it can hit. | 子弹穿过敌人继续飞行。数字是它还能多击中几个敌人。 | Pierce, Terminal Velocity, Lance |
| `Seek` | Shot | Tracking | 追踪 | Bullets bend in flight toward their target, Marked enemies first. | 子弹在飞行中拐向目标，优先追向被标记的敌人。 | Shade trait, imbue, Hound, Closing |
| `Orbit` | Shot | Orbit | 环绕 | Bullets circle you for a moment, then fly outward. | 子弹先绕着你转一会儿，再向外飞出。 | Stormwall, Orbit Lock |
| `Overkill` | Shot | Overkill | 多余伤害 | Damage beyond what it took to kill the enemy. Some effects make it burst onto nearby enemies. | 击杀敌人后溢出的那部分伤害。有些效果会让它炸向附近的敌人。 | Solar (Halo), Sun Lance, Spillover |
| `Push` | Shot | Knockback | 击退 | Shoves enemies away from you. | 把敌人推离你。 | Breakwater, Riptide, Heavy Slug |
| `Pull` | Shot | Pull | 吸拢 | Drags enemies toward a point, bunching them together. | 把敌人拉向一点，聚成一团。 | Whirlpool, Gravity Mine, Tidal Wave |
| `Field` | Shot | Damage zone | 伤害区域 | A patch of ground that hurts enemies standing in it for a short time. | 一片地面，在短时间内伤害站在里面的敌人。 | Lingering Field, Rolling Thunder, burning ground |
| `Stillness` | Shot | Stand still | 站定 | Not moving. Some effects charge up while you stand still and fire with your next volley. | 原地不动。有些效果在你站定时蓄力，随下一次齐射打出。 | Stillness archetype (High Water) |
| `Crit` | Shot | Crit | 暴击 | A hit that deals extra damage. Crit chance is how often; crit damage is how much extra. | 造成额外伤害的命中。暴击率决定多常出现，暴击伤害决定多打多少。 | Radiance, Crit Lens |
| `Souls` | Drop | Soul | 魂 | Dropped by kills for the Keeper of the Dead. Picking one up heals you and gives XP. | 击杀后为司幽留下的东西。拾取后回复生命并获得经验。 | Shade T1, Arrears, Escrow |
| `Shades` | Drop | Shade | 亡影 | An allied ghost raised from a kill. It fights beside you for a few seconds, firing your weapon. | 从被击杀的敌人身上召出的友方影子。它在你身边作战几秒，用你的武器射击。 | Shade T2, Collectors, Shade transform |
| `Embers` | Drop | Embers | 余烬 | Burning sparks thrown out by a death or a Slam. They Burn what they hit. | 敌人死亡或重砸时抛出的火星，会让命中的目标灼烧。 | Spark Shower, Cremation, Forge imbue |
| `Shield` | Hero | Shield | 护盾 | Takes damage before your HP does. | 先于生命承受伤害。 | Spring Tide |
| `Armor` | Hero | Armor | 护甲 | Reduces the damage of every hit you take. | 减少你受到的每次伤害。 | Tempered Plating, Contracts |
| `Favor` | Keeper | Favor | 神眷 | Your standing with a Keeper. Each of its advances adds one; 3, 6 and 9 open its Tiers. | 你在一位司者那里的分量。每拿它一项预支加 1 点；3、6、9 点开启它的阶。 | HUD, cards, Imbue rules |
| `Tier` | Keeper | Tier | 阶 | A Keeper's big reward, unlocked at 3, 6 and 9 Favor. | 司者的大额奖励，在神眷达到 3、6、9 点时开启。 | HUD.KeeperTier, tier toasts |
| `Patron` | Keeper | Patron | 主司 | The Keeper whose terms you accepted for this run. It gives your Intercession. | 本局你接受其条款的司者。你的神临由它给予。 | Shrine, Contracts |
| `Intercession` | Keeper | Intercession | 神临 | Your Patron's ultimate. It charges as you fight; press to unleash it. | 主司赐予的终极技能。战斗中逐渐充能，充满后按键释放。 | HUD, Light Step, Contracts |
| `Advance` | Keeper | Advance | 预支 | A Keeper's blessing, taken on credit. The Keepers collect later. | 司者的祝福，先拿后还。司者日后会来收取。 | Card kind |
| `Renegotiate` | Keeper | Renegotiate | 重议 | Redraw the whole hand of offers. | 重新抽一手选项。 | Offer buttons, Audit, Contracts |
| `Hold` | Keeper | Hold | 保留 | Keep one offer for the next level-up. | 把一张选项留到下一次升级。 | Offer buttons, Contracts |
| `Refuse` | Keeper | Refuse | 拒约 | Turn an offer down for the rest of the run: it will not be offered again. | 本局拒绝这张选项，它不会再出现。 | Offer buttons, Contracts |
| `Debt` | Drop | Debt | 债 | Counts Marked kills. When the count is full, your next volley hunts every enemy on screen. | 记下标记击杀的数目。记满时，你的下一次齐射会追踪全屏的敌人。 | Ledger archetype (Interest, Long Arm, Audit) |
| `Imbue` | Keeper | Imbue | 灌注 | Holding any advance from a Keeper gives all your bullets a little of its effect. It grows with Favor. | 只要持有一位司者的预支，你的所有子弹都会带上它的一点效果，并随神眷增强。 | Imbue rules lines |
| `Archetype` | Keeper | Archetype | 流派 | A set of a Keeper's advances. Its key card changes how your weapons fire. | 一位司者的一组预支。它的核心卡会改变你武器的射击方式。 | Card.Rules.Path, build screen |
| `Key` | Keeper | Key card | 核心 | The archetype card that changes how your weapons fire. Its other cards only come after it. | 流派中改变武器射击方式的那张卡。拿到它之后，同流派的其他卡才会出现。 | Card.Key, Card.KeyTaken |
| `Capstone` | Keeper | Capstone | 终章 | An archetype's strongest card, offered once you hold two of its cards. | 流派中最强的一张卡，持有该流派两张卡之后才会出现。 | Card.Capstone |
| `Duo` | Keeper | Duo | 双司 | An advance that belongs to both of your Keepers and strengthens their reaction. | 同时属于你两位司者的预支，会强化它们的反应。 | Duo cards, Omen Old Alliance |
| `Reaction` | Keeper | Reaction | 反应 | When two Keepers' effects meet on one enemy, something new happens. | 两位司者的效果落在同一个敌人身上时，会产生新的效果。 | Reaction toasts, Duo cards |
| `Transformation` | Keeper | Transformation | 质变 | The level-16 choice. It changes how a weapon or a Keeper's power works. | 16 级时的选择，改变一件武器或一位司者力量的运作方式。 | L16 hand, transform cards |
| `Socket` | Shot | Socket | 插槽 | A slot on a weapon for one modifier. | 武器上安装一个模块的位置。 | Masterwork, modifier cards |
| `Load` | Shot | Load | 负载 | Each weapon after your strongest fires a little weaker, so more weapons spread your power. | 除最强的一件外，每多一件武器火力就略弱一些：武器越多，力量越分散。 | Build.Load |
| `Binding` | Keeper | Binding | 绑定 | A bid term: that Keeper will be offered at level 8. Pass it over and the advance loses a rank. | 出价条款：Lv.8 时必定出现这位司者。不选它，这项预支降 1 级。 | Bid cards |
| `Levy` | Keeper | Levy | 抽成 | A bid term: the Keeper takes more of this run's Ichor. | 出价条款：这位司者多拿走本局的神血。 | Bid cards, run summary |
| `Collateral` | Keeper | Collateral | 抵押 | A bid term: you lose one Renegotiate now. | 出价条款：立即失去 1 次重议。 | Bid cards |
| `Cracked` | Status | Cracked | 脆化 | Bosses can't be Frozen or Blinded; those effects crack them instead: they take more damage for a moment. | 首领不会被冻结或致盲；这些效果改为让它脆化：短时间内受到的伤害提高。 | Tide Whirlpool / Breakwater / Frost; Freeze and Blind on bosses (D-60) |
| `MomentumStack` | Hero | Momentum | 动量 | Built by moving and dashing; each point adds damage and fire rate. Getting hit loses some. | 移动和冲刺时积累，每点都提高伤害和射速。被击中会失去一部分。 | Fleet, Drafting, Terminal Velocity |
| `Crush` | Status | Crush | 挤压 | Pushed or pulled enemies hurt each other when they collide. | 被推到墙、冰或其他敌人身上时受到的额外伤害。 | Undertow, Riptide |
| `Herd` | Shot | Herd | 驱赶 | Pushes enemies into a dense wall in front of you instead of scattering them. | 把敌人驱赶到你面前聚成密集的一堵墙，而不是让它们散开。 | Cold Front |
| `SunCharge` | Shot | Sun charge | 日光蓄能 | Damage to elites and bosses is stored and bursts on them as light every few seconds. | 对精英和首领造成的伤害会被储存，每隔几秒化作光在它们身上爆发。 | card / plaque / bid text (spec §2) |
| `Resonance` | Keeper | Resonance | 共鸣 | This weapon suits the archetype. A display hint only: it changes no numbers. | 这件武器适合该流派。仅为显示提示，不改变任何数值。 | card / plaque / bid text (spec §2) |
