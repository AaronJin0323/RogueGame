# RogueHero — Technical Design (Prototype v0.1)

> Engine: **Unreal Engine 5.8** (latest stable as of 2026-10-18). Language split: **C++ core, Blueprint/DataAsset content**. Solo developer. Target: Windows, 60 fps at 1080p on an **RTX 4070-class GPU at Epic** (RTX 3060-class on the Medium/High scalability tier) — realistic rendering with Lumen + VSM per `05_ArtDirection.md §7`.

---

## 1. Guiding decisions

| Decision | Choice | Why |
|---|---|---|
| Bullets | **C++ data-oriented simulation (struct-of-arrays) in a World Subsystem, rendered by Niagara as emissive tracer ribbons** | Thousands of bullets need to be *data*, not Actors. Niagara renders them for free from an array. Proven path to 5 000+ live bullets. Ribbons (not billboards) because the art direction is realistic tracers (`05_ArtDirection.md §3`). |
| Enemies | **SoA manager for movement/HP/status; swarm rendered as ISM with vertex/procedural animation; skeletal Actors only for elites and the boss** | 300–400 enemies must cost nothing per-instance. The ISM path (Week-1 fallback, now the intended path) makes realistic hard-surface meshes affordable; Actors keep Blueprint-friendliness where behaviour is rich. Upgrade path to Mass if needed, without touching gameplay code. |
| Rendering | **Lumen (software RT) + Virtual Shadow Maps + Nanite for environment statics; TSR** | Realistic art direction (`05_ArtDirection.md`). Enemies are not Nanite (instanced, animated). Scalability groups provide a 3060 tier. |
| Collision | **Custom uniform grid (spatial hash) on the XY plane** | The game is 2D on a plane. Chaos physics is unnecessary overhead for circle-vs-circle tests. |
| Abilities/effects | **No GAS.** Lightweight custom stat set + event hooks | GAS is built for networked ability replication and attribute prediction; here it costs more than it gives. Modifiers are *data flags* on bullets; blessings are small C++/BP effect objects subscribed to batched events. |
| Content | **Primary Data Assets + Gameplay Tags** | Everything a designer tunes lives in DataAssets; code reads them. Tags drive offer weighting, 流派 thresholds, discoveries. |
| UI | **UMG + CommonUI** | Gamepad from day one needs input routing, focus and glyphs that CommonUI provides. |
| Input | **Enhanced Input** (KBM + Gamepad contexts) | Standard in 5.x. |
| Localisation | **String Tables, en + zh-Hans from day one** | FText everywhere; never hard-code strings. |
| Determinism | **`FRandomStream` seeded per run** | Enables daily seeded runs later and reproducible bug reports now. |
| VCS | **Git + Git LFS** | Solo, free, fine for a prototype. |

---

## 2. Project setup (Day 0)

1. Epic Launcher → install **UE 5.8** (with *Starter Content* off, *Editor symbols* optional).
2. Visual Studio 2022 with "Game development with C++" workload + Windows 10/11 SDK + .NET desktop runtime (UE's recommended VS components). Confirm with the UE 5.8 docs page for VS setup.
3. New project: **Games → Blank → C++**, name **`RogueHero`**, location `E:\RogueHero`, Target: Desktop, Quality: Maximum, Starter Content: No, Raytracing: No.
4. Plugins to enable: **CommonUI**, **Niagara** (on by default), **Enhanced Input** (default), **GameplayTags** (built-in), **UMG ViewModel (MVVM)** (optional, recommended for HUD), **EditorScriptingUtilities** (batch data tooling). Disable unused heavy plugins to speed startup (e.g. Bridge/Quixel, Datasmith, OnlineSubsystem* unless needed).
5. Project settings (revised 2026-10-18 for the realistic art direction; the earlier grey-box settings had Lumen/Nanite/VSM off): Lumen GI + reflections **on, software ray tracing**; Nanite **on** (environment statics only); Virtual Shadow Maps **on**; **TSR**; `r.DefaultFeature.MotionBlur=0`, auto-exposure **off** (fixed per arena), bloom on with a high threshold, DOF off, no chromatic aberration; Fixed frame rate **off** (we fixed-step the sim ourselves), **Game Default Map** = `L_Hub`, **Editor Startup Map** = `L_Arena_Test`. Scalability groups: Epic = 4070 target, High/Medium = 3060 target (TSR screen percentage 70–80%).
6. `DefaultInput.ini`: Enhanced Input as default player input/component classes.
7. Create the folder layout (§3), `.gitignore`/`.gitattributes` (repo root), `git lfs install`, first commit.

---

## 3. Folder & naming layout

```
E:\RogueHero\
├─ RogueHero.uproject
├─ Config\                       DefaultEngine/Game/Input/GameplayTags .ini
├─ Docs\                         this folder
├─ Source\RogueHero\
│  ├─ Core\       RHGameInstance, RHGameMode, RHPlayerController, RHCamera, RHTypes.h, RHTags.h, RHLog.h
│  ├─ Player\     RHHero (pawn), RHStatsComponent, RHDashComponent, RHUltimateComponent
│  ├─ Combat\     RHBulletSubsystem, RHBulletTypes.h, RHSpatialHash, RHWeaponInstance, RHWeaponDef
│  ├─ Enemies\    RHEnemySubsystem, RHEnemy (actor), RHEnemyDef, RHAffixDef, RHBoss, RHSpawnDirector, RHSpawnTimeline
│  ├─ Build\      RHRunState, RHOfferGenerator, RHModifierDef, RHBlessingDef, RHGodDef, RHFusionDef, RHEffect (hooks)
│  ├─ Meta\       RHMetaSubsystem, RHSaveGame, RHTreeNodeDef, RHDiscoveryDef, RHContentRegistry
│  ├─ UI\         RHHUD, RHLevelUpScreen, RHHubScreen, view models
│  └─ Debug\      RHCheatManager (console commands), RHStressTest
└─ Content\RH\
   ├─ Maps\        L_Hub, L_Arena_01, L_Arena_Test, L_StressTest
   ├─ Data\        Weapons\ Modifiers\ Blessings\ Gods\ Fusions\ Enemies\ Affixes\ Spawn\ Tree\ Discoveries\
   ├─ Blueprints\  BP_Hero, BP_Enemy_<Role>, BP_Enemy_Boss, BP_RHCamera, effects (BP subclasses of URHEffect)
   ├─ VFX\         NS_Bullet_*, NS_Hit_*, NS_Status_*, NS_Ult_*
   ├─ Materials\   M_GreyBox, MI_* palette, M_BulletSprite, M_HitFlash, M_Telegraph
   ├─ Meshes\      SM_* primitives
   ├─ UI\          WBP_HUD, WBP_LevelUp, WBP_Card, WBP_Hub, WBP_Tree, WBP_RunSummary, glyphs
   ├─ Input\       IA_*, IMC_Gameplay_KBM, IMC_Gameplay_Pad, IMC_Menu
   ├─ Text\        ST_UI, ST_Weapons, ST_Modifiers, ST_Blessings, ST_Keepers, ST_Enemies, ST_Fusions, ST_World, ST_Codex, ST_Tree
   └─ Audio\       placeholders
```

**Naming:** C++ classes prefixed `RH` (`URHBulletSubsystem`, `ARHHero`). Assets: `BP_`, `DA_` (data asset), `NS_` (Niagara), `M_`/`MI_`, `WBP_`, `IA_`/`IMC_`, `ST_` (string table), `SM_`, `T_`, `L_` (level). Data asset instances: `DA_Weapon_PulseRifle`, `DA_Mod_SplitCore`, `DA_Blessing_Storm_Overcharge`, `DA_God_Storm`, `DA_Fusion_Storm_RailLance`, `DA_Enemy_Swarm`.

**Fiction is never a key (binding, see `04_Narrative.md §13`).** Code, tags, asset names and class names use the **Path** for gods (`Storm`, `Forge`, `Tide`, `Shade`, `Radiance`, `Quicksilver`) and the **role** for enemies (`Swarm`, `Tank`, `Rusher`, `Turret`, `Bruiser`, `Support`, `Boss`). Honorifics ("Keeper of Storms / 司雷"), designations (`ATMOS`), Cast-off names ("Scrapwing") and Fusion names live only in string tables as `FText`. `URHEffect_Storm_T1`, not `URHEffect_Zeus_T1`; `BP_Enemy_Swarm`, not `BP_Enemy_Scrapwing`. Renaming the fiction later is a string-table edit, never a redirector hunt.

---

## 4. Runtime architecture

```
GameInstance
 ├─ URHMetaSubsystem        save/load, Ichor, tree, unlocks, discoveries
 └─ URHContentRegistry      all defs loaded via Asset Manager, filtered by unlocks
World (L_Arena_*)
 ├─ ARHGameMode             run start/end, timer, win/lose
 ├─ URHRunState (UObject)   owned weapons/sockets/blessings/favor/commitments/banishes/seed
 ├─ URHBulletSubsystem      bullet SoA sim + Niagara push + hit buffer
 ├─ URHEnemySubsystem       enemy SoA sim (move/HP/status) + actor pool + spatial hash
 ├─ URHSpawnDirector        timeline → spawns, elites, events, boss
 ├─ URHEffectManager        hook dispatch (OnHit/OnKill/OnCrit/OnDash/OnLevelUp/OnTick)
 ├─ URHOfferGenerator       builds 5 cards from RunState + Registry + weights
 ├─ ARHHero (pawn)          input, stats component, dash, ultimate, weapons[4]
 └─ ARHCamera               fixed-angle follow camera with look-ahead and shake
```

**Tick order (one game-thread frame):**
1. Input → hero movement/dash/ultimate.
2. `URHEnemySubsystem::Tick` — rebuild spatial hash from enemy positions; steer (seek player + separation); apply statuses (burn/chill); write actor transforms.
3. Weapons fire → `BulletSubsystem.Spawn(...)` (batched).
4. `URHBulletSubsystem::Tick` — fixed-step sim (60 Hz, sub-steps if dt > 1/30); integrate; behaviours (homing/orbit/boomerang); grid collision → **hit buffer**.
5. Resolve hit buffer: damage (crit, armor, status), bullet on-hit behaviours (split/ricochet/pierce/linger), enemy deaths → **kill buffer**; XP gems; chests.
6. `URHEffectManager` dispatches batched events to active effects (blessings, 流派 tiers, fusions).
7. Push bullet arrays to Niagara; update HUD view model.

---

## 5. Bullet subsystem (the performance core)

### 5.1 Data
```cpp
// RHBulletTypes.h
enum class ERHBulletFlags : uint32 {
  None=0, Pierce=1<<0, Ricochet=1<<1, SplitOnHit=1<<2, Homing=1<<3, Orbit=1<<4,
  Boomerang=1<<5, LingerOnImpact=1<<6, ScatterOnExpiry=1<<7, WaveOnExpiry=1<<8,
  ApplyPatronStatus=1<<9, /* ... */ };

struct FRHBulletSpec {        // composed per weapon from base + up to 3 modifiers; cached
  uint16 ArchetypeId;         // visual archetype (Niagara emitter + sprite + colour + size)
  float  Damage, Speed, Size, Lifetime, CritChance, CritMult, Lifesteal;
  uint8  PierceCount, RicochetCount, SplitCount, BurstCount;
  float  HomingStrength, OrbitTime, ForkAngle; uint8 ForkCount; bool bMirror;
  uint32 Flags;               // ERHBulletFlags
  FGameplayTag StatusTag; float StatusChance;
  uint8  WeaponSlot;          // for per-weapon stats (kills, damage) and Chain Trigger
};

// Subsystem storage: struct-of-arrays, indices reused via a free list
TArray<FVector2f> Pos, Vel;
TArray<float>     Life;        // seconds left
TArray<uint16>    SpecId;      // index into a spec table (shared by all bullets of one weapon config)
TArray<uint8>     PierceLeft, RicochetLeft, HitMask;   // per-bullet mutable state
TArray<int32>     HomingTarget; TArray<float> OrbitPhase;
TArray<uint64>    AlreadyHit;  // small bloom/bitset of enemy ids to avoid multi-hit per frame
TArray<int32>     FreeList;
```
Bullets live on the XY plane at a constant Z. All math is 2D `FVector2f`.

### 5.2 Simulation
- Fixed step 1/60 s; accumulate frame dt, sub-step up to 2× per frame, then clamp (slow-motion for the Quicksilver ultimate just scales dt).
- Behaviour pass per bullet (branch on flags; data-oriented, SIMD-friendly loops where possible).
- Collision: bullets query the **enemy spatial hash** (cell = 200 uu; one `TArray<int32>` per cell rebuilt each frame via counting sort). Circle test `dist² < (rB + rE)²`. On overlap: push `FRHHitEvent{bullet, enemy, pos}` into the hit buffer; mark enemy in the bullet's `AlreadyHit` to prevent re-hits while piercing.
- Expiry behaviours (Scatter/Wave) spawn new bullets into a *deferred spawn list* applied after the loop (never mutate arrays mid-iteration).
- Cap: 6 000 live bullets. When full, the oldest non-fusion bullet is recycled (never drop new spawns silently — log a stat).

### 5.3 Rendering
- One Niagara System per **visual archetype** (~12 in prototype), each with an **Array Data Interface** (`UNiagaraDataInterfaceArrayFloat4`: xyz + size, plus `ArrayColor` where needed). Each frame: `SetNiagaraArrayVector4(Component, "Bullets", Array)` with the compacted list of that archetype's bullets; the emitter spawns/kills particles to match `ArrayLength` and reads position by `ExecIndex`.
- Sprites on a flat plane facing the camera (billboard), additive material with a soft outline; colour per weapon.
- Hits/kills use GPU-sim Niagara bursts triggered through a separate "hit FX" array interface (positions + type) — one system, many events per frame.
- Alternative if arrays become a bottleneck: Niagara **Data Channels** (NDC) — same data, event-style push. Decide at Week 2 profiling.

### 5.4 Weapon → spec composition
`URHWeaponInstance::RebuildSpec()` runs whenever level or sockets change: start from `WeaponDef.BaseSpec[Level]`, apply each socketed `URHModifierDef` (additive/multiplicative fields + flag bits), then apply hero stats (Damage%, Area%, ProjSpeed%, Crit). The result is interned in the subsystem's spec table so bullets store a 16-bit id, not a struct.

Pattern emitters (`ERHFirePattern`): `Aimed`, `Cone`, `Radial`, `Spiral`, `Beam`, `Lob`, `MeleeArc`, `Orbiter`, `Homing`, `Mine`. Each is a small C++ function producing spawn requests; `Fork`/`Mirror`/`Burst` wrap any pattern.

---

## 6. Enemy subsystem

- **SoA:** `Pos`, `Vel`, `HP`, `MaxHP`, `Radius`, `DefId`, `Flags` (elite/shield/etc.), `Statuses` (`BurnTime/BurnDps`, `ChillStacks/ChillTime`, `ShockTime`, `MarkTime`, `BlindTime`), `ActorRef` (weak ptr to the pooled actor). Free list, cap 512.
- **Movement:** seek player + separation (neighbours from the spatial hash, radius 120 uu) + per-type behaviour callbacks (charge, slam, turret). Resolved in the manager; actor transform set with `SetActorLocation(…, /*bSweep*/false, nullptr, ETeleportType::None)`.
- **Actors (`ARHEnemy`):** mesh + hit-flash MID + optional telegraph decal. **No tick**, no collision components. Blueprint subclass per type for visuals and telegraph timings. Elites/boss get real logic in C++ state machines (`ERHBossPhase`) with BP-exposed tunables.
- **Pooling:** pre-spawn 400 actors per type-mix at map load; activate/deactivate (hidden + out of hash) instead of spawn/destroy.
- **Status ticking** in the manager (burn DoT, chill speed mul, freeze at 3 stacks → shatter on kill event, mark/execute check).
- **Death:** push to kill buffer; drop XP gem (gem subsystem shares the same SoA + Niagara approach, magnetised by pickup radius); chance-based chest for elites.
- **Upgrade path** (not in prototype): render swarm types via ISM + Vertex Animation Textures, keep the same SoA; or migrate the swarm to Mass.

---

## 7. Stats, effects, 流派 thresholds

- `FRHStatSet`: fixed array indexed by `ERHStat` with `Base`, `Add`, `Mult` → `Final = (Base + Add) × Mult`. Dirty flag → recompute → broadcast `OnStatsChanged` (weapons rebuild specs, HUD updates).
- Blessing ranks and meta tree nodes write into `Add`/`Mult` via `FRHStatModifier{Stat, Op, Value, SourceTag}` so they can be removed/reapplied cleanly.
- `URHEffect` (UObject, C++ base, **Blueprintable** for content): virtuals `OnActivated/Deactivated`, `OnHits(const TArrayView<FRHHitEvent>&)`, `OnKills(...)`, `OnCrit`, `OnDashStart/End`, `OnLevelUp`, `OnTick(dt)`. Effects are instantiated when a blessing's *signature* behaviour, a 流派 tier, a fusion or an ultimate is taken. Simple numeric blessings are stat modifiers only (no effect object).
- **Favor & thresholds:** `URHRunState::Favor[GodId]`; on change, `URHGodDef::Tiers[]` activate/deactivate `URHEffect` classes at 3/6/9 (patron starts at 2).
- **Statuses** are a `FGameplayTag` (`Status.Burn`, `Status.Chill`, …) → applied through `URHEnemySubsystem::ApplyStatus(enemy, tag, magnitude, duration)`; gods' "apply on hit" blessings are `URHEffect`s listening to `OnHits`.

---

## 8. Offer generator

Input: `URHRunState` (owned weapons + sockets, blessings + ranks, favor, patron, second god, banished tags, locked card, level, luck), `URHContentRegistry` (unlocked defs). Output: 5 `FRHOfferCard`.

```
candidates = []
if free weapon slot:    + new weapons (not owned)
for each owned weapon:  + upgrade card (if level < 8)
                        + modifier→weapon pairs (free socket, not already socketed, not banished)
for each allowed god:   + blessings (new or rank-up < 3)
weight(card) = base[rarity] × godFactor × ownedFactor × synergyFactor × commitFactor
   godFactor     = 1 + 0.30 × Favor[god]                (blessings only)
   ownedFactor   = 1 + 0.25 × weaponLevel                (upgrades / modifiers of owned weapons)
   synergyFactor = 1.5 if card.SynergyTags ∩ run.OwnedTags ≠ ∅
   commitFactor  = 1.5 for the two committed gods after L8 (others already removed)
card[0] = weighted pick from GUARANTEED set (patron/second-god blessings ∪ owned-weapon upgrades)
card[1..4] = weighted sample without replacement from the rest
apply anti-frustration rules (§GDD 5.6), then rarity roll per card (Luck-shifted)
```
RNG: `FRandomStream RunRng(Seed)`; the seed is shown on the run summary for bug reports.

**Commitments:** at L8 the level-up screen is replaced by a 3-card *god* choice (candidates = non-patron gods weighted by favor, min 1 favor-less god for variety). At L16 a *fusion* choice (eligible fusions; fallback Legendary card).

---

## 9. Data assets (Asset Manager primary types)

| Class | Primary type | Key fields |
|---|---|---|
| `URHWeaponDef` | `RHWeapon` | Name/Desc (FText), Icon, Pattern, TargetRule, `Levels[8]` (spec deltas), ArchetypeId, Niagara, Tags, `FusionDefs` |
| `URHModifierDef` | `RHModifier` | FText, Icon, spec deltas + flag bits, `SynergyTags`, Rarity, `UnlockedByDefault` |
| `URHBlessingDef` | `RHBlessing` | God, FText per rank, `StatMods[3]`, optional `EffectClass`, Rarity, `UnlockedByDefault` |
| `URHGodDef` | `RHGod` | `PathTag` (key), Honorific/Designation/Facet (FText), Colour, Element status tag, `Tiers[3]{Threshold, EffectClass, TierLine FText}`, `UltimateClass`, Blessings[] |
| `URHFusionDef` | `RHFusion` | God, BaseWeapon, result weapon def, FText |
| `URHEnemyDef` | `RHEnemy` | `RoleTag` (key), Name (FText), Actor class, HP, Speed, Radius, Damage, XP, Behaviour enum, elite-eligible |
| `URHAffixDef` | `RHAffix` | FText, EffectClass / stat mults |
| `URHSpawnTimeline` | `RHSpawn` | `Blocks[]{Time, Phase (Harvest/Threat/Breath), Mix{EnemyDef,Weight}, Rate, Cap, HPMul, DmgMul}`, `Events[]`, `EliteWaves[]`, `BossTime` — a `Breath` block auto-follows each elite wave/event |
| `URHTreeNodeDef` | `RHTree` | Branch (god/trunk), Cost, Ranks, StatMods or Unlock ref, prerequisites |
| `URHDiscoveryDef` | `RHDiscovery` | Condition (tag + threshold), Unlock refs, FText, `Recollection` (FText) + `StoryLayer` (0–4) |
| `URHCodexEntryDef` | `RHCodex` | Title/Body (FText), `StoryLayer`, unlock source (Discovery / tree node / first win) — Codex sorts by layer |

Gameplay tag families (`Config/Tags/RH.ini`): `God.*` (**Path names**: `God.Storm`, `God.Forge`, `God.Tide`, `God.Shade`, `God.Radiance`, `God.Quicksilver`), `Weapon.*`, `Mod.*`, `Blessing.*`, `Status.*`, `Enemy.*` (**roles**: `Enemy.Swarm`, `Enemy.Tank`, `Enemy.Rusher`, `Enemy.Turret`, `Enemy.Bruiser`, `Enemy.Support`, `Enemy.Boss`), `Affix.*`, `Stat.*`, `Discovery.*`, `Synergy.*` (e.g. `Synergy.Pierce`, `Synergy.Explode`, `Synergy.Crit`).

---

## 10. Meta & save

- `URHSaveGame` (versioned `int32 SaveVersion`): Ichor, `TMap<FPrimaryAssetId,int32> TreeRanks`, `TSet<FPrimaryAssetId> Unlocked`, `TSet<FName> Discoveries`, lifetime stats (`TMap<FName,int64>`), settings (language, audio, bindings), last patron.
- `URHMetaSubsystem`: load on boot, autosave at run end and after tree purchases; migration function per version bump.
- Discoveries evaluated from run stats at run end (`RunStats` counters keyed by tag, e.g. `Stat.Kills.Weapon.ScatterCannon`).

---

## 11. Input & UI

- **Enhanced Input:** `IA_Move` (Axis2D), `IA_Dash`, `IA_Ultimate`, `IA_Pause`, `IA_UI_*`. `IMC_Gameplay_KBM` + `IMC_Gameplay_Pad` added together (both devices live). Dead-zone 0.2 radial for sticks.
- **CommonUI:** `UCommonActivatableWidgetStack` on the HUD layer for Level-Up / Pause / Summary; input routing switches to UI automatically; `UCommonButtonBase` cards with gamepad focus; platform glyphs via `CommonInputBaseControllerData`. **Until glyph textures exist (§17, W5 UI): code-drawn glyph badges (`URHInputPrompt`) resolved from the IMC mappings, switching live on the last-used device.**
- **HUD:** one `WBP_HUD` bound to `URHHUDViewModel` (MVVM): HP, XP, level, timer, six favor bars, weapon icons + modifier pips, dash charges, ultimate progress, reroll/banish/lock counts. Updated via view-model fields, not per-frame polling.
- **Level-up screen:** 5 `WBP_Card` (icon, name, rarity frame, bound weapon, tags, "Favor x→y · Tier n!" line), Reroll/Banish/Lock buttons, gamepad: L/R to move, A to pick, X to lock, Y to banish.
- Damage numbers: pooled world-space widgets *or* a Niagara text sprite sheet — decide in Week 2 by cost. **Decided W2 Sat: neither — one Slate leaf widget painting a fixed pool (§17).**

---

## 12. Localisation

- Every user-facing string is `FText` from a **String Table** (`ST_UI`, `ST_Weapons`, …). In C++ use `NSLOCTEXT("RH", "Key", "English")` only for debug/placeholder; content strings go through tables.
- Localization Dashboard: target `Game`, cultures `en` (native) and `zh-Hans`. Gather → translate (zh table maintained alongside en) → compile each milestone.
- Fonts: a CJK-capable UI font family (e.g. Noto Sans SC fallback) set in the UMG font fallback. **Now: code-built composite font `RHUIStyle::Font` (Roboto + engine DroidSansFallback CJK sub-font, all cultures) — §17 W5 UI.**
- Narrative tables (`04_Narrative.md §13`): `ST_Keepers` (honorific, designation, facet entry, 3 tier lines per Keeper), `ST_Enemies`, `ST_Fusions`, `ST_World`, `ST_Codex`. Keys are English slugs (`Keeper.Storm.Tier2`, `Codex.Recollection.Survive10`). Designations (`ATMOS` …, `ROOT`) are the same string in both cultures. Every `ST_Codex` row carries a `Layer` metadata column; `RH.Codex.ShowLayer n` prints everything a player at layer *n* could have read, to audit truth leaks.

---

## 13. Camera

`ARHCamera`: spring-arm-free, fixed pitch −62°, perspective FOV 50°, height ≈ 2 600 uu (≈ 2 700 uu visible width), follows the hero with a 150 uu look-ahead in the movement direction and 8 Hz smoothing. Shake on crit/kill/ultimate via `UCameraShakeBase` (small). Arena is flat; Z of all gameplay = 0.

---

## 14. Performance budgets & profiling

| Budget | Target |
|---|---|
| Frame | 16.6 ms @ 1080p Epic, RTX 4070-class; 16.6 ms @ 1080p Medium/High (TSR 70–80%), RTX 3060-class |
| Game thread | ≤ 8 ms with 5 000 bullets + 400 enemies |
| GPU | ≤ 12 ms at Epic incl. Lumen + VSM; Lumen ≤ 4 ms, VSM ≤ 2.5 ms |
| Live bullets / enemies / gems | 6 000 / 512 / 2 000 caps |
| Hit events | ≤ 20 000 / s resolved |
| Niagara | ≤ 3 ms GPU (tracer ribbons + impact FX); ≤ 16 systems live; effect LOD culls individual kill FX above 20 kills/s |
| Draw calls | ≤ 1 500 (Nanite environment + ISM enemies) |
| **Steam Deck tier** (D-24, 2026-10-20) | 1280×800, ≥ 40 fps target / 30 fps floor; Lumen off (SSGI or sky light), VSM off, no volumetric cloud (overcast weather only), TSR 60 %, impact-FX density LOD one step earlier, bullet cap unchanged (`Docs/Design/v10_polish_spec.md` §5) |

Tools: `stat unit`, `stat game`, `stat Niagara`, **Unreal Insights** with `-trace=cpu,gpu,frame`, `L_StressTest` map with console commands `RH.Spawn 400`, `RH.Bullets 5000`, `RH.Give Weapon.* / Mod.* / Blessing.*`, `RH.Favor Storm 9`, `RH.Time 19:50`, `RH.Codex.ShowLayer 2`, `RH.Stats` (rolling 60 s: kills/min, swarm TTK, live bullets, dashes/min, damage taken/min — the GDD §3.5 metrics, also written to the run summary and `Saved/Playtests/*.csv`).

---

## 15. Git + LFS

- `.gitignore`: `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`, `.vs/`, `*.sln`, `*.VC.db`, `*.opensdf`, `*.sdf`, `*.suo`, `*.xcodeproj`, `*.xcworkspace`, `Plugins/*/Binaries`, `Plugins/*/Intermediate`.
- `.gitattributes`: `*.uasset`, `*.umap`, `*.png`, `*.jpg`, `*.wav`, `*.ogg`, `*.fbx`, `*.ttf`, `*.otf`, `*.mp4` → LFS.
- Branches: `main` (always opens & plays), `dev/<topic>` short-lived. Commit prefix convention: `feat:`, `fix:`, `content:`, `data:`, `perf:`, `docs:`.
- Tag each milestone: `v0.1-m1` … `v0.1`.

---

## 16. Risks & mitigations

| Risk | Mitigation |
|---|---|
| Niagara array push becomes the bottleneck at 5k+ bullets | Profile Week 2; fallback to Data Channels or GPU-side sim with CPU collision only |
| Lumen + VSM eat the GPU budget with 400 ISM enemies casting shadows | Enemies cast VSM shadows only for elites/boss; swarm uses contact shadows or none; Lumen scene detail reduced for the arena; measured Week 1 Sat on the stress map |
| Realistic scene swallows bullets (readability) | `05_ArtDirection.md §3` contrast rules; `RH.Debug.Contrast` if Week 2 playtest shows problems; bloom threshold tuned so only tracers bloom |
| Actor count for enemies hurts frame time | Enemies never tick; if `SetActorLocation` cost shows, switch swarm types to ISM rendering (same SoA) |
| Build system feels random despite weighting | Tune weights in data, expose `RH.Debug.Offers` to print weights; test with 20-run sims via a headless commandlet that simulates picks |
| Scope: 10 weapons + 20 modifiers + 30 blessings in 6 weeks | Modifiers are **data**, not code; blessings are mostly **stat mods**; only tiers/ultimates/fusions need effect code (≈ 30 small classes) |
| CommonUI learning curve | Start menus in Week 1 with the official CommonUI sample as reference; keep widgets simple |
| Localisation drift | zh table updated at each milestone, gather/compile is part of the milestone checklist |
| Fiction names change after assets exist | Names are FText only; code/tags/assets are keyed by Path and role (§3). A rename is a string-table edit |
| Story truth leaks into early-run text | Every Codex/Recollection row has a `Layer`; `RH.Codex.ShowLayer` audits what a layer-*n* player can see; tier lines are capped at hints (`04_Narrative.md §4.2`) |

---

