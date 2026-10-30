# 04 — Presentation

> The readability contract (§2), the palette tokens (§3), the render settings (§4) and the asset-source rules (§5) are **binding** and fixed before the first tracer is drawn; art sessions implement them and never reinterpret them. §6 is reference material (non-binding). Tunable thresholds are `C.PRE.*` keys in `08_Constants.md`; hex colours live only in the token table of §3. Readability beats realism wherever the two conflict.

## 1. Direction

- An **eroded white megastructure at dusk**: fitted white stone slabs worn grey at the joints, moss, standing water, half-buried Founder machines; the Keepers' infrastructure glows dimly in the seams in the six Path colours.
- **Realistic hard-surface rendering**: PBR stone and ceramic, Lumen GI, soft overcast or low-sun light; enemies are machines, so no skin, hair or cloth.
- Two dusk weathers, both built and both held to this contract: **overcast** (soft, short shadows) and **low sun** (volumetric cloud, light shafts, long shadows).
- **Readability beats realism.** The hero, every bullet, every telegraph and every enemy must be found in one glance at `C.BULLETS.CAP` live bullets.

## 2. Readability contract (binding)

Every rule is checked on the rendered frame at gameplay camera, not on swatches. `PLANNED(<stage>)` marks a test that does not exist yet.

- **R-PRE-01** Player fire is a tracer: a bright cool emissive core (`Pal.Player.Core`) plus a short trail in the leading Keeper's colour (`Pal.Keeper.<Path>`; `Pal.Player.TrailNeutral` before any Keeper is held). No Keeper, effect or modifier ever recolours the core. A second Keeper only tints the trail (an accent toward the trail's tip); it never changes the core, the head, or the first Keeper's trail colour.
  - T-PRE-01 PLANNED(S1) `Dusktracer.Pre.PlayerTracer`: for every weapon × Path × second-Path combination the core colour equals `Pal.Player.Core`; the trail equals the leading Path's token; the second Path changes only the trail accent.
- **R-PRE-02** Enemy fire is warm/magenta with a **dark core**: a `Pal.Enemy.BulletCore` centre inside a `Pal.Enemy.Bullet` rim; enemy muzzle flashes use `Pal.Enemy.Muzzle`. Shape separates the two sides even when hues are close: player rounds have a bright core and a coloured tail, enemy rounds a dark core and a bright rim. Player trails never use red or magenta; `Pal.Keeper.Forge` amber stays on the trail and never reaches the head.
  - T-PRE-02 PLANNED(S1) `Dusktracer.Pre.EnemyRound`: enemy round core and rim equal their tokens for every enemy role; no player-side token falls in the enemy hue band (`Tools/palette_lint.py` hue check).
- **R-PRE-03** Every enemy attack is announced by a **red projected telegraph** (`Pal.Telegraph`) before its damage can land: a ground ring, band or arc projected by the enemy's own targeting light, or an aim line for a projectile volley. The telegraph's extent equals the damage area. Telegraphs are the only red element in combat.
  - T-PRE-03 PLANNED(S1) `Dusktracer.Pre.TelegraphCoverage`: every attack row in `09_Catalog` (enemies, boss) names a telegraph shape; the spawned telegraph's footprint matches the attack's damage shape; telegraph colour equals `Pal.Telegraph`.
- **R-PRE-04** Every enemy carries at least one always-lit status light (`Pal.Enemy.Optic`). Elites carry a brighter `Pal.Elite.Light` whose blink pattern names the affix (one pattern per affix; the most dangerous affix decides). Elites never swap models; affixes are attached parts.
  - T-PRE-04 PLANNED(S1) `Dusktracer.Pre.EliteLight`: every enemy instance has a lit optic; every affix maps to a distinct pattern; elite light colour equals `Pal.Elite.Light`.
- **R-PRE-05** Every gameplay-relevant element (hero, player tracer core, enemy round, telegraph, enemy optic, elite light, pickups) reaches a luminance contrast of at least `C.PRE.CONTRAST_MIN` against **both** `Pal.Ground.Shadow` and `Pal.Ground.Bright`, in **both weathers**, in the densest late-run frame. A dual-tone element (dark core + bright rim) passes when either tone meets the ratio against that ground. If a weather fails, the scene changes (shadow length, ground albedo, emissive), never the rule.
  - **Telegraph edge.** A telegraph holds its true red, whose luminance alone cannot reach the ratio on a mid-tone floor, so its contrast is structure. Floor up, each layer `C.PRE.TELEGRAPH_LAYER_UU` over the last from `C.ENEMIES.TELEGRAPH_Z_UU`: a dark band in `Pal.Telegraph.Edge`, the shape grown by `C.PRE.TELEGRAPH_EDGE_UU`, outside the footprint so the red still equals the damage area (R-PRE-03); the rim, the footprint itself, always at full emissive; the fill, the shape shrunk by `C.PRE.TELEGRAPH_RIM_UU`, its emissive ramping with `Progress` from `C.PRE.TELEGRAPH_FILL_MIN` of `C.ENEMIES.TELEGRAPH_EMISSIVE` to all of it as the attack lands. A shape too thin to shrink (an aim line) is band and rim. All bands sit under all red, so overlapping telegraphs merge into one red mass with one dark outline and a neighbour's band never crosses red (R-PRE-06). Control cues draw flat, without layers (R-PRE-11).
  - T-PRE-05 PLANNED(S0) `smoke` gate contrast step (`Tools/Smoke.ps1`, `11_Quality §1`): per-element contrast table from captures of the stress arena in each weather; fails below `C.PRE.CONTRAST_MIN`.
- **R-PRE-06** Density LOD: above `C.PRE.DENSITY_LOD_KILLS_PER_S` kills per second, per-kill effects aggregate (individual sparks and debris are culled; smoke pools into one ground-level haze). Player tracer emissive and trail width scale down with the live player-bullet count so that enemy fire and telegraphs stay the brightest things in frame. Density LOD never dims enemy rounds, telegraphs or optics.
  - T-PRE-06 PLANNED(S1) `Dusktracer.Pre.DensityLod`: at a kill rate above the threshold the live kill-effect count is bounded and aggregated; enemy-side emissive is unchanged at every density.
- **R-PRE-07** Nothing smears or hides a bullet: no motion blur, no chromatic aberration, no depth of field in play, no lens flare, fixed exposure per weather (no eye adaptation). Bloom is thresholded so that only emissives bloom. Ground and sky are desaturated so that nothing on the floor competes with fire.
  - T-PRE-07 PLANNED(S0) `smoke` gate settings step: the render settings of §4 are present in `Config/DefaultEngine.ini` and in the arena post-process volume; a captured burst frame shows no exposure change.
- **R-PRE-08** UI card kinds are distinguishable by **border, icon, label and rarity independently**: each of the four channels alone identifies the kind (or the rarity, for the rarity channel), so no channel — colour least of all — is the only carrier. `Pal.Enemy.*` and `Pal.Telegraph` never appear in UI.
  - T-PRE-08 PLANNED(S1) `Dusktracer.Pre.CardIdentity`: for every card kind × rarity, border, icon and label keys are unique per kind and the rarity marker is unique per rarity; no UI style reads an enemy or telegraph token.
- **R-PRE-09** Colour comes only from the tokens of §3: code, materials and UI styles read a token by name; a literal colour outside the token source is a lint error. Changing a token value is a PRESENT change with a before/after capture.
  - T-PRE-09 `Tools/gen_constants.py --check` (gate step `constants`): `Source/Dusktracer/Core/DTPaletteGen.h` is generated from the §3 table and matches it; no hex colour literal (`FromHex`, `#RRGGBB`/`RRGGBB` string) appears in `Source/` outside generated files.

- **R-PRE-11** Control the player holds is visible around what it holds, in cool tokens only, and presentation reads sim state, never writes it. Hues are judged on the rendered frame, so flat marks that must read as their token (telegraphs, cues) draw unlit on `M_CueColor`: a lit quad takes the warm sun, and the tonemapper then slides it off its token. Every cue shows what the sim is doing now:
  - **Stillness dial** (any owner of `Flag.Stillness`, Trait or not): `Pal.Keeper.Tide`, one closed ring per charge held, the first of outer radius `C.PRE.CHARGE_RING_RATIO` of the hero's radius, each `C.PRE.CHARGE_RING_UU` thick, the next `C.PRE.CHARGE_RING_GAP_UU` further out; the charge being gathered shows as up to `C.PRE.CHARGE_TICKS` ticks round the next ring, each `C.PRE.CHARGE_TICK_FILL` of its share of the circle. Walking clears it; dashing keeps the rings.
  - **Stillness aura** (`charged_aura_uu`): a ring in `Pal.Keeper.Tide` whose outer edge is `charged_aura_uu` while a charge is held, `C.PRE.AURA_RING_UU` thick plus up to `C.PRE.AURA_PULSE_UU` inward as it pulses.
  - **Frozen wall** (`frozen_blocks_enemies`): a flat `Pal.Keeper.Tide` plate of `C.PRE.WALL_PLATE_RATIO` of the enemy's radius under each enemy that player Froze.
  - **Frozen hold** (`freeze_until_hit`): a square frame in `Pal.Player.Core`, half-side `C.PRE.HOLD_FRAME_RATIO` of the radius, `C.PRE.HOLD_FRAME_UU` thick, around each held enemy.
  - **Plain Frozen** (every other Frozen enemy): a diamond in `Pal.Player.TrailNeutral`, half-diagonal `C.PRE.FROZEN_CUE_RATIO` of the radius, `C.PRE.FROZEN_CUE_UU` thick. An enemy with a wall or a hold shows those instead.

  Draw order, floor up: zones (fields, mine zones, the wall plate) at `C.PRE.ZONE_Z_UU`, `C.PRE.ZONE_HALF_UU` to either side, so a disc never hides a cue, a telegraph or a hero's legs; dial and aura at `C.PRE.CUE_Z_UU`, under telegraphs (R-PRE-03); frame and diamond at `C.SIM.PLANE_Z_UU`, around the hull's own middle, so the camera's tilt cannot slide them off it. Dial, aura and plate draw at `C.PRE.CUE_HUE_EMISSIVE`, frame and diamond at `C.PRE.CUE_EMISSIVE`.
  - T-PRE-11 `Dusktracer.Pre.ControlCues`: each cue appears exactly when the sim is doing what it shows; footprints match; the draw order holds; cue tokens reach `C.PRE.CONTRAST_MIN` against both ground references at the drawn emissive, stay in the cool hues and are never an enemy or telegraph token.

## 3. Palette tokens (binding)

Y = relative luminance of the sRGB value (WCAG, linear). Swatches are base colours; contrast is judged on the rendered frame (R-PRE-05), where emissives sit at their intensity. Rows marked **owner (D-008)** were set for this project; the rest come from the previous project's locked art book (A1, Cast-off volume).

| Token | sRGB | Y | Use | Contrast requirement | Status |
|---|---|---|---|---|---|
| `Pal.Player.Core` | #EEF7FF | 0.92 | core of every player round | vs both ground refs, both weathers | from art book (A3 near-white core) |
| `Pal.Player.TrailNeutral` | #9CD2FF | 0.58 | player trail while no Keeper is held; plain Frozen cue (R-PRE-11) | trail is not a carrier; core carries | owner (D-008) |
| `Pal.Hero.Armour` | #D9DDE2 | 0.72 | hero armour; brightest neutral object | vs both ground refs, both weathers | owner (D-008) |
| `Pal.Keeper.Storm` | #7FB4FF | 0.44 | trail, rig light, conduits, HUD, cards | rendered emissive vs `Pal.Ground.Bright` | art book A1 |
| `Pal.Keeper.Forge` | #FF9F3A | 0.46 | as Storm; trail only, never the head | as Storm; hue stays clear of the enemy band | art book A1 |
| `Pal.Keeper.Tide` | #6FE0E8 | 0.63 | as Storm | as Storm | art book A1 |
| `Pal.Keeper.Shade` | #8E62FF | 0.22 | as Storm; needs the highest emissive | as Storm; UI may lighten it to pass on `Pal.UI.Panel` | art book A1 |
| `Pal.Keeper.Radiance` | #FFE28C | 0.78 | as Storm | as Storm | art book A1 |
| `Pal.Keeper.Quicksilver` | #A8F25A | 0.73 | as Storm | as Storm | art book A1 |
| `Pal.Enemy.Bullet` | #FF2D8A | 0.25 | enemy round rim and trail | rim vs `Pal.Ground.Shadow` | art book A1 |
| `Pal.Enemy.BulletCore` | #2A0A16 | 0.01 | dark centre of every enemy round | core vs `Pal.Ground.Bright` | art book PL.16 |
| `Pal.Enemy.Muzzle` | #FF7A3A | 0.36 | enemy muzzle flash, hit-on-hero flash | brief flash; not measured | art book PL.16 |
| `Pal.Enemy.Body` | #1C1E24 | 0.01 | swarm hull (dark gunmetal); readability is carried by the optic, not the hull | none (hull is a silhouette; `Pal.Enemy.Optic` meets R-PRE-05) | scheduler (D-010) |
| `Pal.Enemy.Optic` | #FF3B9E | 0.27 | always-lit enemy status light | rendered emissive vs both ground refs | Cast-off volume C01 |
| `Pal.Elite.Light` | #FF7AC2 | 0.39 | elite status light, per-affix blink | vs both ground refs, brighter than `Pal.Enemy.Optic` | Cast-off volume C14 |
| `Pal.Telegraph` | #FF2A2A | 0.23 | projected rings, bands, arcs, aim lines | rendered vs both ground refs, both weathers | Cast-off volume C01 |
| `Pal.Telegraph.Edge` | #000000 | 0.00 | dark band around every telegraph (R-PRE-05) | against `Pal.Ground.Bright`, and against the red it borders | scheduler (telegraph contrast) |
| `Pal.Ground.Bright` | #7E7970 | 0.19 | brightest ground reference (sunlit slab) | reference tone | owner (D-008) |
| `Pal.Ground.Lit` | #66625B | 0.12 | typical lit slab; target for ground albedo | reference tone (art book: lit stone Y ≤ 0.20) | owner (D-008) |
| `Pal.Ground.Shadow` | #34322F | 0.03 | darkest ground reference (shadowed slab, low sun) | reference tone | owner (D-008) |
| `Pal.Rarity.Common` | #B8B2A7 | 0.45 | rarity marker, Common | vs `Pal.UI.Panel` | owner (D-008) |
| `Pal.Rarity.Rare` | #5FA8E0 | 0.36 | rarity marker, Rare | vs `Pal.UI.Panel` | owner (D-008) |
| `Pal.Rarity.Epic` | #8A7CFF | 0.27 | rarity marker, Epic | vs `Pal.UI.Panel` | G02 (≈ #8A7CFF) |
| `Pal.Pickup.Gem` | #5CF2A0 | 0.68 | XP gems and the HUD XP bar fill (cool green, clear of the enemy band and the Keeper hues) | vs both ground refs, both weathers | scheduler (progression.md) |
| `Pal.Pickup.Chest` | #FFD866 | 0.71 | chests on the floor | vs both ground refs, both weathers | scheduler (progression.md) |

| `Pal.Card.Tech` | #9DB2C6 | 0.43 | card border and icon of machined-tech kinds: weapon, upgrade, modifier (R-PRE-08) | vs `Pal.UI.Panel` (≈ 8:1) | scheduler (offers package) |
| `Pal.Card.Stone` | #C8B07E | 0.45 | card border and icon of Keeper-stone kinds: blessing, Keeper, Reforging (R-PRE-08) | vs `Pal.UI.Panel` (≈ 8.5:1) | scheduler (offers package) |
| `Pal.Card.Plain` | #8E8A82 | 0.26 | card border and icon of plain neutral stock: Reserve (R-PRE-08) | vs `Pal.UI.Panel` (≈ 5:1) | scheduler (offers package) |
| `Pal.UI.Panel` | #15171C | 0.01 | dark translucent panel base | reference tone for UI | owner (D-008) |
| `Pal.UI.Text` | #E8E4DC | 0.78 | main UI text (bone) | vs `Pal.UI.Panel` | art book PL.24 |

The rarity ladder covers the tiers `02_Systems/offers.md` defines; a tier added there gets a row here before it is drawn.

## 4. Render settings (binding)

Already set in `Config/DefaultEngine.ini`:

| Setting | Value in config | Meaning |
|---|---|---|
| `r.DynamicGlobalIlluminationMethod`, `r.ReflectionMethod` | Lumen | Lumen GI and reflections |
| `r.Lumen.HardwareRayTracing`, `r.RayTracing` | False | software Lumen only |
| `r.GenerateMeshDistanceFields` | True | required by software Lumen |
| `r.AllowStaticLighting` | False | fully dynamic lighting |
| `r.Shadow.Virtual.Enable` | on | Virtual Shadow Maps |
| `r.Nanite.ProjectEnabled` | True | Nanite available; used for **environment statics only** |
| `r.AntiAliasingMethod` | TSR | Temporal Super Resolution |
| `r.DefaultFeature.MotionBlur`, `r.MotionBlurQuality` | off | no motion blur |
| `r.SceneColorFringeQuality` | off | no chromatic aberration |
| `r.DefaultFeature.AutoExposure` | False | fixed exposure |
| `r.DefaultFeature.LensFlare` | False | no lens flare |

Binding but not yet set (owned by the arena post-process volume, S0): depth of field off in play; exposure fixed per weather; bloom threshold so that only emissives bloom. Enemies, bullets and other instanced or animated meshes never use Nanite. Lighting stays dynamic (no baked lighting) so the two weathers can blend.

## 5. Asset sources and the licence log (binding)

- **R-PRE-10** Every asset is one of: procedural (generated by a script in this repo), authored in-engine, **CC0**, or **OFL** (fonts). Engine content is referenced from `/Engine/` and never copied into `Content/`. The repo is public (D-006): nothing whose licence forbids redistribution — marketplace or Fab content, Megascans, assets from AI services with restrictive terms, anything with an unclear licence — is committed. Every asset that ships is logged in `Docs/09_Catalog/licenses.csv` before it is committed.
  - T-PRE-10 PLANNED(S1) licence check in `Tools/catalog_lint.py`: every file under `Content/DT/` and `SourceArt/` matches a `licenses.csv` row by path or glob; every row's licence is in the allowed set; no binary is committed without a row.

`Docs/09_Catalog/licenses.csv` columns (one row per asset or per glob of a generated family):

| Column | Content |
|---|---|
| `asset_path` | path or glob in the repo (`Content/DT/Meshes/SM_Env_*`) |
| `kind` | mesh, texture, material, font, audio, vfx, map |
| `origin` | `procedural`, `in_engine`, `external` |
| `source` | generator script path for procedural / in-engine, URL for external |
| `author` | person or organisation; `Dusktracer` for own work |
| `licence` | `own`, `CC0-1.0`, `OFL-1.1`, `UE-EULA-engine` (referenced `/Engine/` content only) |
| `licence_file` | path of the licence text shipped with the asset (OFL requires it); empty for `own` |
| `date_added` | ISO date |
| `used_for` | what it is in the game |
| `added_by` | brief id or commit |

## 6. Reference material (non-binding)

- The previous project's art book (Keep volume and Cast-off volume) is the look reference for silhouettes, materials, scale and light. Art-book fidelity is scheduled after the fun gate (`12_Production`, S4); until then grey boxes carry the tokens of §3 and pass §2.
- Previous direction document: `Handoff_Dusktracer/game/reference/original_docs/05_ArtDirection.md` (§2 look, §4 Cast-off family language, §12.1 per-role silhouettes and affix parts). Mine it; this document wins.
- Camera, scale and per-role silhouettes belong to `06_UX.md` and the enemy specs; their telegraph shapes must satisfy R-PRE-03.

## 7. Open

| Question | Data that resolves it |
|---|---|
| Enemy optic `#FF3B9E` and enemy round `#FF2D8A` are two near-identical hues; unify into one danger hue? | side-by-side capture at gameplay camera |
| Exposure value per weather, bloom threshold | first S0 stress-arena captures; become `C.PRE.*` keys when measured |
| Per-weapon hue variation inside the cool range, or one core for all weapons | S1 playtest: can players tell weapons apart by shape alone |
| `Pal.Telegraph` at its true red is low in luminance: rendered Y 0.21 (#F32F27) against the rendered floor's 0.10 is 1.8:1, and the token against `Pal.Ground.Bright` is 1.2:1, under `C.PRE.CONTRAST_MIN` (R-PRE-05). The coral it replaced reached 2.6:1 by leaving its hue. Change the token, the ground reference, or the rule for hue-carried elements? | T-PRE-05 on captures in both weathers; a human reading of the red on the floor |
| `Pal.Keeper.Tide` renders at its hue (182, token 183) but paler (saturation 0.34, token 0.52): the tonemapper desaturates, and a lower emissive darkens it below the cue contrast of T-PRE-11. Accept, or grade the tonemapper for all colours? | T-PRE-05 and a human reading; a grade changes every token |
