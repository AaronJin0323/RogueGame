# S3 narrative v0.1: tables and layer audit

Branch `dev/s3-narrative`, from `dev/s3`. Spec: `Docs/03_Narrative.md`. Sources: Handoff `04_Narrative` §8, §10, §12, §13, the `ST_*` text tables, `zh_overrides.csv`, `zh_glossary.md`, `zh_naming.md`, `G01_Name.md`.

## Rows

| Table | Rows | Holds |
|---|---|---|
| `ST_Codex` (new, Layer + Unlock) | 31 | 6 common-knowledge, `Codex.Root.Title`, 15 Recollections, Form III entry, 7 bestiary, entry title format |
| `ST_KeeperLines` (new, Layer + Unlock) | 42 | 18 tier lines, 6 passed-over lines, 6 × Short, Path, Designation |
| `ST_World` (new, Layer + Unlock) | 42 | 18 proper nouns and mechanic words, 6 hub labels, return line, Quota name + 5 levels, 4 run summary lines, 7 Muscle Memory |
| `ST_Discoveries` (new) | 32 | 15 Discoveries × name + condition, toast, unlock line |
| `ST_Enemies` (added) | 3 | Breaker, Tallyman, the Warden |

150 new rows. Not copied: Facet entries, `.Act2` lines, Bids, remarks, rivals, traits, NPC rows, Outfall and Sump rows, Omens, loadouts, reactions, Refusal rows, Ending rows.

## Layer audit

`loc` (`Tools/loc_build.py --check`, `check_story`) fails on a row above C.NARR.PREWIN_LAYER_MAX whose unlock needs no win.

| Layer | `ST_Codex` | `ST_KeeperLines` | `ST_World` |
|---|---|---|---|
| 0 | 12 | 12 | 41 |
| 1 | 11 | 24 | 0 |
| 2 | 6 | 6 | 1 |
| 3 | 2 | 0 | 0 |

Layer 3 is `Codex.Root.Title` (`Win`) and `Codex.Warden.FormIII` (`Wins3.FormIII`). No Layer 4 row. Deepest layer before the first win: 2, read in the game by `Dusktracer.Core.StoryLayers` and `DT.Codex.ShowLayer`.

What the check cannot read is meaning. It trusts the `Layer` and `Unlock` cells and a short word list. Nothing in the game shows these rows yet, so reachability is proved on the data, not on a screen.

## Retired names

Clean in en, zh, Context and Kw: RogueHero, `RH.*`, `God.*`, the Greek gods and enemies, Underworld, and "god" in en and in the Context of the narrative tables. The zh 神 rule holds (神血, 神眷, 神临 only here). These are now gate failures, and the retired-name list runs on all 12 tables.

Handoff Context notes were rewritten, not copied; they cited the old GDD and a weapon list this build does not have.

## Findings for a reader

1. **`Keeper.Tide.Tier3` may leak Layer 3.** "I kept the part of us that stops. You kept the rest." is Handoff text at Layer 2, reachable in one good run. "Us" with "you" comes close to stating that the Six and the Reclaimer were one, which the Handoff's own §4.2 forbids. Kept verbatim; a call for the writer.
2. **`Keeper.Forge.Tier2`** "You were not always this well made." at Layer 1 is milder, same question.
3. **The Yard has no Keeper line.** Handoff §3 says the word first appears in a Keeper line, then the Codex. None of the 24 lines says it, so it first appears in `Codex.Recollection.Velocity`. `World.Yard` opens with that row.
4. **`Codex.Recollection.Magnify`** says the Six made the Breach machines; §6 has them as relics the Keepers abandoned. Near enough for Vigil, who is "slightly wrong"; left.
5. **Discoveries point at content.** Flak Mortar, Ring Emitter and the like appeared in Handoff notes as unlocks. Unlock targets are not in these rows; `meta.md` owns them. Conditions naming Scatter Cannon, Rail Lance and Orbital Drones match `ST_Weapons`.
6. **Breaker and Tallyman** have names and bestiary lines but no role in the game yet. `Kill.Bruiser` and `Kill.Support` cannot fire until they exist.
7. **Muscle Memory** rows are text with no reader. Stage 3 raises a level cap, a power gain under D-022's rule; flagged there, not decided here.
8. **`Keeper.Radiance.Tier1`** has no row in `zh_overrides.csv`; it uses the §4.2 draft, which predates the clarity pass.

## zh not from the Handoff (20, for a native reader)

Each carries `[invented]` in Context. The bestiary's zh was promised in the art book boards, which the Handoff does not hold.

| Key | en | zh |
|---|---|---|
| `Codex.Enemy.Swarm` | They arrive before the storms. Vigil's children count them to guess whether it will rain tonight. | 它们总在风暴之前飞来。守夜镇的孩子数它们，猜今晚会不会下雨。 |
| `Codex.Enemy.Tank` | A Bulwark is a wall that walks. Do not stand in front of one. | 壁垒是一堵会走的墙。别站在它前面。 |
| `Codex.Enemy.Rusher` | A Courser only runs straight. Watch its lights. | 驿骑只会直着跑。盯住它的灯。 |
| `Codex.Enemy.Turret` | Flare Pylons once carried the sun into the town. Now they send something else. | 耀柱曾把阳光送进镇里。现在它们送来的是别的东西。 |
| `Codex.Enemy.Bruiser` | In winter they broke the ice. No one remembers why they never stopped. | 它们冬天破冰。没人记得它们为什么一直没停下。 |
| `Codex.Enemy.Support` | When a Tallyman is near, the others move faster. Strike it first. | 算筹在附近时，其他弃物动得更快。先打它。 |
| `Codex.Enemy.Boss` | The largest Cast-off waits at the deep end of the Breach. Only the Reclaimer has ever come back from it. | 最大的弃物守在裂口深处。只有回收者从它那里回来过。 |
| `Discovery.ScatterKills500.Name` | Close Accounts | 结清 |
| `Discovery.Elite3Mods.Name` | Fully Fitted | 配齐 |
| `Discovery.Freeze100.Name` | Cold Storage | 冷藏 (also a duo name in `ST_Reactions`) |
| `Discovery.Win.Name` | Breach Cleared | 肃清裂口 |
| `Discovery.Kills1000.Name` | Full Count | 清点完毕 |
| `Discovery.WeaponL8.Name` | Run It Hot | 烧红 |
| `Discovery.Favor9.Name` | Full Measure | 足额 (the override for the same en on an Act II Discovery) |
| `Discovery.ReachL10.Desc` | Reach level 10 in a single run. | 单局升到 10 级。 |
| `Discovery.Freeze100.Desc` | Freeze 100 Cast-offs in a single run. | 单局冻结 100 个弃物。 |
| `Discovery.Kills1000.Desc` | Destroy 1,000 Cast-offs in a single run. | 单局摧毁 1000 个弃物。 |
| `Discovery.WeaponL8.Desc` | Raise a weapon to level 8. | 把一件武器升到 Lv.8。 |
| `Discovery.Elites10.Desc` | Destroy 10 elites in a single run. | 单局摧毁 10 个精英。 |
| `Discovery.Dashes100.Desc` | Dash 100 times in a single run. | 单局冲刺 100 次。 |

Not marked invented: `World.*` and hub labels are in `zh_glossary` §2, `ROOT — [无数据]` is in Handoff §10, Short and Path names are in `zh_glossary` §3.
