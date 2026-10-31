# Glossary — key ↔ en ↔ zh-Hans ↔ definition

> Skeleton (S0 day 1). Keys are canonical identifiers (Path and role); fiction lives in string tables only. Chinese is authored from the fiction, not translated (`Handoff_Dusktracer/game/reference/localization/zh_naming.md`). The word "god" never appears in fiction; the only sanctioned 神- words are 神血, 神眷, 神临, 神铸. This file is also the source for `<kw>` keyword notes.

## Product

| Key | en | zh-Hans | Definition |
|---|---|---|---|
| Dusktracer | Dusktracer | 曳光 | The game. The two names share an image; neither translates the other. |
| Tagline | You are the bullet hell. | 弹幕即你。 | |

## Keepers (`Keeper.<Path>` tags; replaces the old `God.*` root)

| Key | Honorific en | zh-Hans | HUD short | Designation | Status applied | Speciality |
|---|---|---|---|---|---|---|
| Keeper.Storm | (string table) | 司雷 | 雷 | ATMOS | Shock 感电 | bullet-hell |
| Keeper.Forge | (string table) | 司炉 | 炉 | FOUNDRY | Burn 灼烧 | melee / burn |
| Keeper.Tide | (string table) | 司潮 | 潮 | CISTERN | Chill 减速 → Freeze 冻结 | control |
| Keeper.Shade | (string table) | 司幽 | 幽 | LEDGER | Mark 标记 | tracking / marks |
| Keeper.Radiance | (string table) | 司曜 | 曜 | LUMEN | Blind 致盲 | burst / crit |
| Keeper.Quicksilver | (string table) | 司驰 | 驰 | RELAY | — | mobility / ricochet |

## Archetypes, S1 (`Archetype.<Path>.<Name>` tags; content in `02_Systems/keepers.md`, `09_Catalog/expression.csv`)

| Key | en | zh-Hans |
|---|---|---|
| Archetype.Storm.Tempest | Tempest | 暴风 |
| Archetype.Storm.ArcGrid | Arc Grid | 电弧网 |
| Archetype.Storm.Downpour | Downpour | 倾盆 |
| Archetype.Forge.Hammer | Hammer | 铁锤 |
| Archetype.Forge.Furnace | Furnace | 熔炉 |
| Archetype.Forge.Slam | Slam | 重砸 |
| Archetype.Tide.Whirlpool | Whirlpool | 漩涡 |
| Archetype.Tide.Frost | Frost | 霜冻 |
| Archetype.Tide.Stillness | Stillness | 止水 |

## Archetypes, S2 (`keepers.md` §9)

| Key | en | zh-Hans |
|---|---|---|
| Archetype.Shade.Hound | Hound | 猎犬 |
| Archetype.Shade.Hunters | Collectors | 收债人 |
| Archetype.Shade.Economy | Ledger | 账簿 |
| Archetype.Radiance.Lance | Lance | 光矛 |
| Archetype.Radiance.Prism | Prism | 棱镜 |
| Archetype.Radiance.Solar | Solar | 烈日 |
| Archetype.Quicksilver.Vector | Vector | 矢量 |
| Archetype.Quicksilver.Momentum | Momentum | 动量 |
| Archetype.Quicksilver.Rebound | Rebound | 弹射 |

## Core terms

| Key | en | zh-Hans | Definition |
|---|---|---|---|
| Intercession | Intercession | 神临 | The one ultimate; one input. Its content varies by archetype, never its key. |
| Blessing | Advance | 预支 | A Keeper card: key, amplifier or trait of an archetype. |
| Reaction | Reaction | 反应 | What happens when two Keepers' effects meet on one enemy (`reactions.md`). |
| Duo | Duo | 双司 | The card that belongs to both of a player's Keepers and strengthens their reaction. |
| Reforging | Reforging | 神铸 | The L16 qualitative transformation offer. |
| Favor | Favor | 神眷 | |
| Ichor | Ichor | 神血 | Meta currency. |
| Reclaimer | the Reclaimer | 回收者 | The player character. |
| Vigil | Vigil | 守夜镇 | Hub town. |

## Rename map (old → new) — see `Handoff_Dusktracer/game/G01_Name.md`
RogueHero/`RH` → Dusktracer/`DT` · `/Game/RH/` → `/Game/DT/` · `God.<Path>` → `Keeper.<Path>` · `URHGodDef` → `UDTKeeperDef` · `DA_God_*` → `DA_Keeper_*`.
