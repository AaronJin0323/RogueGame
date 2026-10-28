# RogueHero — instructions for development agents

You are working on **RogueHero**, a UE 5.8 top-down roguelike 弹幕射击·割草 (survivor-like). The design is finished and lives in `Docs/`. **Your job is to implement those documents, not to redesign them.** Read them before touching anything, in this order:

| Read | Why |
|---|---|
| `Docs/03_Roadmap.md` | What to build **this week and today**. Work the current day's row; do not skip ahead. |
| `Docs/02_TechnicalDesign.md` | Architecture, naming, data assets, budgets. §3 (naming) and §17 (decision log) are binding. |
| `Docs/01_GameDesign.md` | Rules and numbers. §3.5 (爽 with teeth) and §5.6 (offers) are the heart of the game. |
| `Docs/04_Narrative.md` | Names and text. §13 (naming rules) and §12 (what text ships in v0.1) are binding. |
| `Docs/05_ArtDirection.md` | The look: realistic eroded white megastructure at dusk, hard-surface machine enemies, physically-plausible VFX. §3 (readability under realism), §7 (render settings & budgets) and §9 (v0.1 scope) are binding. |

When the docs and the code disagree, the docs win. When you must deviate (an engine limit, a measured performance problem), **write the deviation into `02_TechnicalDesign.md §17 Decision log` in the same commit**, dated, with the swap-out point if it is temporary. Undocumented deviations are bugs.

## Non-negotiables (never "simplify" these away)

1. **The core feeling is 爽 with teeth** (GDD §3.5). Swarm time-to-kill ≤ 0.3 s for a well-steered build; threats are skill checks (telegraphs, ranged, Shielded, events, boss rings), never HP walls. Follow the anti-fatigue rules: camera-shake budget, hit-stop on elite+ kills only, cool-coloured player VFX, warm/magenta only for danger, aggregated damage numbers above 20 kills/s. `RH.Stats` must report the §3.5 metrics.
2. **Bullets are data, not Actors** (TDD §5): struct-of-arrays in `URHBulletSubsystem`, fixed-step 60 Hz, spatial-hash collision, Niagara array DI (ISM is the accepted Week-1 fallback). Cap 6 000. Never spawn an Actor per bullet.
3. **No GAS** (TDD §1). Stats are `FRHStatSet`; effects are `URHEffect` subscribed to batched events.
4. **Everything a designer tunes is a Primary Data Asset + Gameplay Tags** (TDD §9). Modifiers are data flags, not code. C++ factory fallbacks are allowed only until the `DA_` asset exists and must be named as such.
5. **The offer system is sacred** (GDD §5.6): 5 cards, tag weights, guaranteed slot, L8 second-god commitment, L16 fusion, reroll/banish/lock, anti-frustration rules. Never cut any of it (Roadmap "Never cut").
6. **Fiction is never a key** (TDD §3, Narrative §13). Tags, assets, classes and identifiers use the **Path** (`Storm`, `Forge`, `Tide`, `Shade`, `Radiance`, `Quicksilver`) and the enemy **role** (`Swarm`, `Tank`, `Rusher`, `Turret`, `Bruiser`, `Support`, `Boss`; v0.2 adds `Lurker`, `Tether`, `Artillery`; v0.3 adds `Carrier`, `Siphon`). Honorifics (Keeper of Storms / 司雷), designations (`ATMOS`), Cast-off names (Scrapwing), Fusion names — **FText in string tables only**. No Greek names anywhere: Zeus/Harpy/Asterion etc. are retired; `Underworld` is now `Shade`.
7. **All user-facing text is `FText` from String Tables, en + zh-Hans** (TDD §12). No hard-coded strings in UI. Keeper lines ≤ 12 words EN / ≤ 20 chars ZH; the word "god" never appears in-fiction (Narrative §2).
8. **Narrative scope for v0.1 is exactly Narrative §12** — names, 18 tier lines, 15 Recollections, 7 Codex entries, hub labels. No dialogue system, no NPCs, no Layer ≥ 3 story text reachable before the first win (`RH.Codex.ShowLayer` audits it).
9. **Performance budgets** (TDD §14): 60 fps @ 1080p on an RTX 4070-class GPU at Epic (Lumen SW + VSM + Nanite env on) with 5 000 bullets + 400 enemies; game thread ≤ 8 ms, GPU ≤ 12 ms; a 3060 scalability tier must exist by Week 6. Profile before optimising; log measurements in §17.
11. **The art is realistic, the readability is not negotiable** (Art §3). Player bullets are cool emissive tracer ribbons (exception, designer decision 2026-10-20: imbued and fusion bullets carry their Keeper's colour — Forge amber and Quicksilver green included — on the tail and glow only, while the core stays cool/near-white; Art §3.2); enemy fire is warm; telegraphs are red laser-projected ground markings; enemies carry a warm status light; bloom only on emissives; no motion blur, no chromatic aberration, fixed exposure. Never ship a "realistic" effect that fails the 3:1 contrast rule. The art book (`05_ArtDirection.md §12`) is the visual reference and the in-game result must be at least as detailed, from v0.1, for everything: environment, hero, enemies, VFX, UI and weather (designer decision 2026-10-19; scope valve 7 is withdrawn for art). Assets are procedural/in-engine or CC0/OFL, each logged in `Docs/Art/asset_licenses.md`.
10. **`main` always opens and plays.** Never leave it broken. Topic branches `dev/<topic>`. Commit prefixes `feat: fix: content: data: perf: docs: chore:`. Tag milestones `v0.1-m1` … `v0.1`.

## How to work a task

1. Find today's row in `Docs/03_Roadmap.md`. Check the week's **Done when** line — that is the acceptance test.
2. Check `02_TechnicalDesign.md §17` for fallbacks already in place so you extend them rather than fight them.
3. Build with `Tools\Build.bat`; run with `Tools\Play.bat` or the editor (`L_Arena_Test`). Debug via the `RH.*` console commands (README).
4. When done: update the roadmap checkbox or row if scope changed, add a §17 line for any deviation, and — on playtest days — write notes to `Docs/Playtests/`.
5. If a task cannot be done as specified, **stop and say so** with the doc section it conflicts with. Do not quietly substitute a different design. The scope valves in `03_Roadmap.md` are the only approved cuts.

## Migration status

The Greek → Path/role re-key is **done** (2026-10-18, TDD §17). Do not reintroduce Greek names anywhere.

## Conventions summary

- C++ prefix `RH` (`URHBulletSubsystem`, `ARHHero`). Assets `BP_ DA_ NS_ M_/MI_ WBP_ IA_/IMC_ ST_ SM_ T_ L_`. Data assets `DA_God_Storm`, `DA_Enemy_Swarm`, `DA_Fusion_Storm_RailLance`.
- Folder layout: TDD §3. Content under `Content/RH/`. Docs under `Docs/`.
- Language: code comments and docs in English; proper nouns carry `EN / 中文`; all player-facing text bilingual via string tables.
- Toolchain: UE 5.8.3 at `E:\UE5\UE_5.8`, MSVC Build Tools, Rider. Git/LFS setup is the user's; do not create a repo or push.
