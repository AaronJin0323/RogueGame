# The name / 游戏名

## Decision: **Dusktracer** / **《曳光》**

**English — Dusktracer.** A *tracer* is the round that draws a line of light so the gunner can see where the fire goes; the game's single most recognisable image is thousands of cool emissive tracer ribbons from the player. *Dusk* is the setting — an eroded white megastructure at the end of the day. And *to trace* is to follow a line back to its source: the Reclaimer, a vessel re-made run after run, is tracing what the Original left behind. One word, two syllables that are easy to say in any accent, no article, no colon, and no established game carries it.

**中文 —《曳光》。** 取自“曳光弹”：拖着光迹飞行、让射手看见弹道的子弹。玩家的标志性画面正是成千上万条冷色的曳光拖尾。“曳”是拖曳、牵引，也是“摇曳”的曳——黄昏巨构里的光在摇曳；“光”既是子弹，也是六位司者各自的颜色，也是残存在容器里的那一点原初之光。两个字，古意与军械感并存，读作 yè guāng，念起来干净，市面上没有同名游戏。

The two names are **not translations of each other**; they share the image (light that trails behind a shot at dusk) and each is the strongest phrase in its own language. That is the standard for every proper noun in this project (see the naming rules in `reference/original_docs/04_Narrative.md §13` and `reference/localization/zh_naming.md`).

**Tagline:** *You are the bullet hell.* / **弹幕即你。**

## Identifiers (bind these in `07_Technical` on day 0)

| Item | Value | Note |
|---|---|---|
| Project / module | `Dusktracer` | UE module name; `Dusktracer.uproject` |
| Class prefix | `DT` | `ADTHero`, `UDTBulletSubsystem`, `FDTStatSet`, `UDTEffect` |
| Content root | `/Game/DT/` | `Content/DT/...` |
| Console / cvar prefix | `DT.*` / `dt.*` | `DT.Stats`, `DT.Bot`, `dt.Exp.*` (experiments, with expiry) |
| Gameplay-tag roots | `Keeper.<Path>`, `Enemy.<Role>`, `Cap.*`, `Flag.*`, `Status.*` | `Keeper.*` replaces the old `God.*` root: the word never appears in fiction, so it should not be the code key either |
| String tables | `ST_<Table>` unchanged | same table set and key style |
| Save slot / app id placeholders | `DTMeta.sav`, Steam app id TBD | |
| Repo | `Dusktracer/`; branches `dev/<topic>`, `release/<ver>` | |

## Alternatives considered (keep for the record)

| EN / ZH | Why it was attractive | Why not |
|---|---|---|
| Gloaming / 薄暮 | both elegant words for dusk; rare | "gloaming" is obscure for many players; neither word carries the bullets |
| Halflight / 半明 | dusk, half-alive vessel, the Six's half share | too close to "Half-Life" |
| Vesper / 晚祷 | evening + prayer fits the Keeper shrines | an existing game title (2022) |
| Afterglow / 余晖 | the Reclaimer as afterglow of the Original | peripheral brand; crowded |
| Dusktide / 暮潮 | dusk + tide of enemies/bullets; 暮潮 is a real poetic compound | too close to "Darktide" |

A trademark / store search for *Dusktracer* and 曳光 has **not** been done; do it before any public use.

## Rename map (old → new) for anything carried over from the reference material

| Old | New |
|---|---|
| RogueHero, `RH`, `/Game/RH/`, `RH.*` commands | Dusktracer, `DT`, `/Game/DT/`, `DT.*` |
| `God.<Path>` tags, `URHGodDef`, `DA_God_*` | `Keeper.<Path>`, `UDTKeeperDef`, `DA_Keeper_*` |
| Everything else in fiction (Keeper honorifics, Reclaimer, Vigil / 守夜镇, the Breach, Ichor / 神血, Cast-offs, the Warden) | unchanged unless the new narrative pass decides otherwise; they are string-table content, not keys |
