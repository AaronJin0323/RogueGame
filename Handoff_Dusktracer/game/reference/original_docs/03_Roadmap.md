# RogueHero — 6-Week Prototype Roadmap

> Start: **Sun 2026-10-18** (Day 0). Prototype v0.1: **Sun 2026-11-29**. Solo developer. Each week ends with a playable build tagged in Git and a 15-minute self-playtest with notes in `Docs/Playtests/`.

Principle: **fun check first, content second, meta third, polish last.** Every milestone must *open and play* — never leave `main` broken over a weekend.

---

## Day 0 — Setup (today / Mon 19 Oct)
- [x] Install UE 5.8 (5.8.3) + MSVC Build Tools (VS 18); IDE is JetBrains Rider (see `02_TechnicalDesign.md §17`).
- [x] Create `RogueHero` Blank C++ project in `E:\RogueHero`; enable CommonUI, MVVM; disable unused plugins.
- [x] Apply project settings (Lumen/Nanite off, TSR, Enhanced Input defaults) — see `02_TechnicalDesign.md §2`.
- [x] Folder layout + `.gitignore` + `.gitattributes`.
- [x] `git lfs install`; first commit `chore: project skeleton` (local repo only — no GitHub remote, see TDD §17).
- [x] Gameplay tag families in `Config/Tags/RH.ini`.
- [x] **Re-key tags and code to the narrative decision** (`04_Narrative.md §13`, `02_TechnicalDesign.md §3`): `God.*` by **Path** (`God.Storm`, `God.Forge`, `God.Tide`, `God.Shade`, `God.Radiance`, `God.Quicksilver`), `Enemy.*` by **role** (`Enemy.Swarm` … `Enemy.Boss`); rename `MakeHarpy` → `MakeSwarm` and any other Greek identifiers in `Source/`; fiction names live only in string tables.
- [x] String tables `ST_UI` (en + zh-Hans) and Localization Dashboard target. (CSV tables in `Content/RH/Text/`, target `Game` in `Config/Localization/`, run with `Tools\Localize.bat` — TDD §17.)
- [x] Decide the [proposal] names in `04_Narrative.md §14` that are needed early: six honorifics (HUD) and Cast-off names. (Decided as proposed, 2026-10-18; seeded in `ST_Keepers` / `ST_Enemies`.)
> Note (2026-10-18): git is local-only (no remote, TDD §17). Maps are created by `Tools\CreateMaps.bat`.
- **Done when:** project opens, compiles, `L_Arena_Test` loads a flat 6000×6000 floor, commit pushed.

---

## Week 1 (19–25 Oct) — "Shoot and kill"  → tag `v0.1-m1`
**Goal:** hero moves, dashes, one weapon fires via the bullet subsystem, Scrapwings swarm and die.

| Day | Work |
|---|---|
| Mon | Hero pawn + Enhanced Input (KBM + pad) + fixed camera with look-ahead; **revise project settings for the realistic direction** (Lumen SW, VSM, Nanite env, TSR, fixed exposure, post-process — TDD §2) and set up the dusk sky/light rig in `L_Arena_Test` |
| Tue | `URHBulletSubsystem`: SoA arrays, fixed-step sim, free list; Niagara array-DI rendering of one archetype as an **emissive tracer ribbon** |
| Wed | Spatial hash; `URHEnemySubsystem` SoA + pooled `ARHEnemy`; Scrapwing (`Enemy.Swarm`) seek + separation |
| Thu | Hit buffer → damage → death; hit-flash; physical hit/kill FX (sparks, debris, smoke puff) with density LOD; XP gem SoA + magnet |
| Fri | `URHWeaponInstance` + `URHWeaponDef` (Pulse Rifle, Aimed pattern, 8 levels); dash with i-frames |
| Sat | Stress test map + `RH.*` console commands; profile 5 000 bullets / 400 enemies **with Lumen + VSM on** (GPU ≤ 12 ms Epic); decide swarm shadow policy; fix hot spots |
| Sun | Playtest, notes, tag |

**Done when:** 60 fps with 3 000 bullets + 300 Scrapwings on the dev machine; dash passes through enemies; XP collected.

---

## Week 2 (26 Oct–1 Nov) — "A 5-minute run"  → `v0.1-m2`
**Goal:** level-ups with 5 cards, 4 weapons, spawn timeline, HUD. The loop exists.

| Day | Work |
|---|---|
| Mon | `FRHStatSet` + `URHStatsComponent`; XP → level curve; `URHRunState` |
| Tue | `URHOfferGenerator` v1 (new weapon / upgrade cards only, weights, guaranteed slot, no-duplicates) |
| Wed | CommonUI stack; `WBP_LevelUp` with 5 `WBP_Card`, gamepad focus, Reroll button; pause |
| Thu | Weapons 2–4: Scatter Cannon (Cone), Orbital Drones (Orbiter), Rail Lance (Beam/Pierce) + their Niagara archetypes |
| Fri | `URHSpawnDirector` + `URHSpawnTimeline` DA (0:00–5:00 block); Bulwark (`Tank`, frontal shield) + Courser (`Rusher`, charge telegraph) |
| Sat | `WBP_HUD` + view model (HP, XP, timer, weapon icons); damage numbers (pick cheapest approach) |
| Sun | Playtest 3 runs of 5 min; first `RH.Stats` readout (kills/min, swarm TTK, live bullets, dashes/min, dmg taken/min — GDD §3.5); tag |

**Done when:** a 5-minute run with ~6 level-ups is playable start to finish with pad only.

> Status 2026-10-19 (scheduler): **met → `v0.1-m2`.** Human playtest 2026-10-19 (KBM, 20-min run; `Docs/Playtests/2026-10-19_human.md`) plus scripted pad paths (RH.UI.Press: hub → Settings → language → Back; level-up picks) after the shared-UI accept / virtual-pointer / stacked-menu fixes. A human pad-only pass is still owed at the next playtest.

---

## Week 3 (2–8 Nov) — "Builds exist"  → `v0.1-m3`
**Goal:** modifiers, gods, favor thresholds, statuses, steering tools. A run converges into a 流派.

| Day | Work |
|---|---|
| Mon | `URHModifierDef` + spec composition; flags: Pierce, Ricochet, Split, Fork, Mirror, Magnify, Overclock, Heavy Slug (8 starting modifiers) |
| Tue | Modifier→weapon pre-bound offer cards; sockets on HUD; remaining 12 modifier flags (Homing, Orbit, Boomerang, Chain Trigger, Burst, Spread Wave, Linger, Crit Lens, Velocity, Scatter Shell, Lifeline, Divine Conduit) |
| Wed | Status system on enemies (Burn, Chill/Freeze, Shock, Mark, Blind) + status VFX; `URHGodDef`, `URHBlessingDef`, favor counter + HUD bars |
| Thu | Blessings: 6 gods × 5 (mostly stat mods; signature effects as `URHEffect`) in **en + zh**; **narrative text pass 1**: `ST_Keepers` (honorific + designation ×6, 18 tier lines) and all proper nouns from `04_Narrative.md §12` |
| Fri | 流派 tiers T1/T2/T3 as `URHEffect` classes for Storm, Forge, Tide; tier-line toast on threshold |
| Sat | Tiers for Shade (Souls, Shades), Radiance, Quicksilver (dash charges/trail); Banish + Lock; offer weights tuned; `RH.Debug.Offers` |
| Sun | Playtest: can you reach Tier 2 of your patron by minute 8 reliably? Tag |

> Note 2026-10-18: Mon is done and so is the flag half of Tue — all 20 modifier flags work on all 4 weapons (`RogueHero.Modifiers.AllWeapons`; TDD §17). Tue stays open for the pre-bound modifier offer cards and the HUD sockets.

> Note 2026-10-18: pass-1 text is written ahead of schedule — `ST_Keepers` (designations, Path names, 18 tier lines with `Layer`), `ST_World` (proper nouns), `ST_Fusions`. The Thu row stays open until blessing text and the tier-line toast exist.
> Note 2026-10-18: `URHGodDef`, `URHBlessingDef`, favor tiers, `URHEffect`/`URHEffectManager` and all 30 blessings (data + 12 effect classes, en + zh `ST_Blessings`) are in (TDD §17). Still open: the enemy status system (Wed) — status/effect-damage blessings are stubbed against `IRHStatusTarget` until it lands — favor HUD bars, and the tier-line toast.
> Note 2026-10-18: the enemy status system (Wed) is in — Burn, Chill/Freeze, Shock, Mark, Blind on the enemy SoA with ISM status VFX; `URHEnemySubsystem` is the effect manager's `IRHStatusTarget`, so status and effect-damage blessings now work (TDD §17). Wed stays open for the favor HUD bars.

> Note 2026-10-18: the 18 流派 tier effects (Fri + the tier half of Sat) and the tier-line toast event are in (`URHEffect_<Path>_T<N>`, TDD §17); `RH.Favor <Path> 9` reaches Tier 3 for every Path (`RH.Debug.Tiers`). Hit/kill-driven tiers go live when the game mode calls `URHEffectManager::DispatchStep` and the enemy status system implements `IRHStatusTarget`; Banish + Lock / offer tuning / `RH.Debug.Offers` stay with the offer work.

> Note 2026-10-18: game-mode integration pass done (TDD §17): spawn director, hit outcomes, effect dispatch with crits, meta run start/end, run-state time/kills, full offer pool (modifier + blessing cards, always 5) and the level-up flow are wired in `ARHGameMode`; `RogueHero.Wiring.*` covers it. Open: patron pick (hub), win condition (boss), fusions.

> Note 2026-10-18: integration pass 2 (TDD §17): single run start in the game mode, `URHRunLaunch` patron entry point, run end → run-summary screen, Time Dilation on enemy Dt, HUD tier toast, status deaths from enemy kill bits, meta ult-cooldown / start-blessing bonuses, `RH.Bot` autopilot. The no-pick bot survives all 20 min: the §3.5 "overrun by ~12:00" contract needs the timeline's events / late blocks.

**Done when:** all 20 modifiers work on all 4 weapons; every god reaches Tier 3 in a debug run; favor-weighting is visibly steering offers.

> Status 2026-10-19 (scheduler): **met → `v0.1-m3`.** All 20 modifiers × all weapons and fusions are covered by automation (120 fusion pairs + RHModifierTests); tiers T1–T3 for all six Paths are tested (RHTierTests); favor steering is measured by the offer depth audit (`Tools/offer_audit.py`, TDD §17).

---

## Week 4 (9–15 Nov) — "The full 20-minute run"  → `v0.1-m4`
**Goal:** all content for a run: 10 weapons, 6 enemies + elites, commitments, fusions, ultimates, boss.

| Day | Work |
|---|---|
| Mon | Weapons 5–7: Flak Mortar (Lob), Arc Blade (MeleeArc), Seeker Swarm (Homing); **enemy kitbash models**: Scrapwing, Bulwark, Courser (Fab hard-surface kits, shared ceramic/metal/rust/emissive material, ISM-ready) |
| Tue | Weapons 8–10: Ring Emitter (Radial), Spiral Projector (Spiral), Gravity Mine (Mine) + archetypes; kitbash Flare Pylon, Breaker, Tallyman (scaled primitives acceptable if short on time) |
| Wed | ✅ Flare Pylon (`Turret`, warm tracers), Breaker (`Bruiser`, laser-projected slam ring), Tallyman (`Support`, visible data-link beam); elite affixes ×6 with visible parts + chests (3-card Rare+ offer) — chest → offer wiring and the Breaker knockback on the hero are integration hooks (TDD §17) |
| Thu | L8 second-god commitment screen; pool narrowing; L16 fusion screen; 6 fusion weapons (reuse patterns + tier effects) |
| Fri | Patron selection (temporary debug menu) + 6 ultimates as `URHEffect`/Niagara (physical: lightning strikes, fire front, overexposed beam…) — **ultimates done 2026-10-18**: six Intercessions, hero wiring (Q / LB), HUD meter; until the patron menu exists the ultimate is the run's first Path, debug patron `RH.Ult.Path <Path>` (TDD §17); full 20-min timeline with events & elite waves authored as Harvest / Threat / Breath blocks (GDD §3.5, §7); scaling; **hero model + animations** from a Fab pack with Patron rig light (capsule + light if it slips) |
| Sat | Boss **the Warden** (`Enemy.Boss`, placeholder fiction): 3-phase state machine, charge, radial rings, spiral, adds, arena shrink; win/lose + run summary (seed shown as "Filed under:") |
| Sun | Two full 20-min playtests (different patrons); balance notes; tag |

> Note 2026-10-18: Fri's **full 20-min timeline** is in (Harvest / Threat / Breath blocks, 5:00 / 10:00 / 15:00 elite waves, 7:30 ring / 12:30 stampede / 17:30 Pylon nest, scaling, `DA_SpawnTimeline_Main` + `DA_Enemy_*` assets) with a boss call site at 20:00 for Sat (TDD §17). The rest of the Fri row (patron selection menu, hero model) is not part of it.

**Done when:** a 20-minute run can be won; each patron's ultimate and at least 3 fusions have been seen in play.

> Status 2026-10-19 (scheduler): **met → `v0.1-m4`.** LoopCheck W4 with RH.Bot v2 and a fusion-chasing picker, on main after balance R10: 5 of 6 twenty-minute runs won (Warden defeated in 32–455 s), 6/6 patron ultimates fired, all 6 fusions seen (TDD §17). With the plain steered bot policy only 1 fusion is seen, because the policy doesn't collect fusion base weapons; that's a bot limit, not the game. The human two-run playtest (Sun row) is still owed at the next playtest.

> Note 2026-10-18: the weapon half of Mon + Tue is done — weapons 5–10 with their patterns, target rules, 8-level tracks, archetypes and ST_Weapons rows; all 20 modifiers work on all 10 weapons (`RogueHero.Modifiers.AllWeapons`; TDD §17). The kitbash enemy models of Mon / Tue (Fab assets) stay open.

---

## Week 5 (16–22 Nov) — "The loop closes"  → `v0.1-m5`
**Goal:** hub, Ichor, blessing tree, discoveries, save/load, full localisation pass.

| Day | Work |
|---|---|
| Mon | `URHSaveGame` + `URHMetaSubsystem` (versioned); Ichor formula; run-end autosave |
| Tue | `L_Hub` + `WBP_Hub` (**Vigil**): patron select (the Shrine: 6 portraits, locked hints), Start Run, Contracts, Codex, Settings (language toggle en/中文); hub labels from `ST_World` |
| Wed | Blessing Tree data (trunk + 6 branches) + `WBP_Tree` (gamepad-navigable radial/grid) + purchase flow |
| Thu | `URHContentRegistry` unlock filtering; `URHDiscoveryDef` ×15 + run-stat counters + unlock toasts; **narrative text pass 2**: 15 Recollections + 7 Codex entries (`ST_Codex`, layer-tagged, Codex sorts by layer), `ROOT — [no data]` on first win |
| Fri | Localisation: gather, fill zh-Hans for all tables, compile; CJK font fallback; pad glyphs |
| Sat | Run summary → Ichor → tree → next run (summary shows `reclaimed / the Six's share / retained`, display-only); new-player pool verified (4 weapons / 8 mods / 18 blessings); `RH.Codex.ShowLayer` audit: no Layer ≥ 3 text reachable before the first win |
| Sun | Fresh-save playtest: 3 consecutive runs; tag |

> Note 2026-10-18: the UI half of **Fri** (CJK font fallback, pad glyphs) and the **Settings screen** of **Tue** (language toggle en ⇄ 中文, live + persisted; master volume placeholder) are done ahead of schedule (TDD §17, "W5 UI pulled forward"). Settings is reached from the pause screen for now. Tue stays open for `L_Hub` / `WBP_Hub` (which must reuse `URHSettingsScreen`); Fri stays open for the full gather / zh-Hans fill of all tables.

> Note 2026-10-18: pass-2 text is written ahead of schedule — `ST_Codex` (15 Recollections, 6 Codex Keeper entries + `Codex.Root.Title`, `Layer` column) and `ST_World` (hub labels, run-summary lines). The Thu/Tue/Sat rows stay open until the Codex/hub UI and `URHDiscoveryDef` consume it.

> Note 2026-10-18: Mon/Wed/Thu **systems** pulled forward (TDD §17 "Week 5 meta core"): `URHSaveGame` (v2, migration) + `URHMetaSubsystem` (Ichor formula, run-end autosave entry point `OnRunEnded`), `URHContentRegistry` (new-player pool 4 / 8 / 18), 15 `URHDiscoveryDef` + `URHRunCounters` + `OnDiscoveryUnlocked` event, Contracts data (45 `URHTreeNodeDef`, purchase API), `RH.Codex.ShowLayer` audit (pre-win max layer 2), `ST_Discoveries` / `ST_Tree`. Rows stay open: Mon needs the game-mode run-end hook wired; Wed needs `WBP_Tree`; Thu needs the toast UI, the counter producers and the offer flow reading the registry; Sat's audit passes but the summary screen is not built.

**Done when:** delete the save, play 3 runs, unlock ≥ 2 discoveries and ≥ 4 tree nodes without touching the console.

> Status 2026-10-19 (scheduler): **met → `v0.1-m5`.** LoopCheck W5 with RH.Bot v2 from a deleted save: 3 runs (3 wins), 13 discoveries, 8 tree nodes, all bought through the Contracts screen. The human fresh-save playtest (Sun row) is still owed.

---

## Week 6 (23–29 Nov) — "Polish, balance, ship v0.1"  → `v0.1`
**Goal:** it *feels* good; it runs well; someone else can play it.

| Day | Work |
|---|---|
| Mon | **Environment art pass** (`05_ArtDirection.md §2`): Megascans stone slabs, erosion, moss, puddles, conduit seams in Keeper colours, half-buried machines; feedback polish (hit-stop budget, shake budget, aggregated damage numbers) folded into the same day |
| Tue | Environment pass cont. + audio placeholders per `05_ArtDirection.md §11` (restrained epic orchestral; **six Keeper sound signatures** — designer decision 2026-10-19) (fire per weapon class, hit, kill, level-up, ultimate, boss); UI SFX; crowd-kill audio layer at high density. **Scope valve 7:** if Monday ran over, stop the environment here and ship grey-box geometry with realistic lighting + VFX |
| Wed | Balance against GDD §3.5 targets: XP curve, timeline caps, swarm TTK ≤ 0.3 s for a steered build, elite HP, weapon DPS table (spreadsheet in `Docs/Balance/`), rarity & luck; death-by-minute histogram from 10 runs |
| Thu | Performance: Insights capture at 15:00 & boss; fix top 3 hot spots; **scalability groups** (Epic = 4070, Medium/High = 3060 with TSR 70–80%); verify 60 fps on both tiers |
| Fri | Bug bash from playtest notes; crash-free 5 runs; package **Windows Shipping** build |
| Sat | External playtest (2–3 friends): watch, don't explain; record which 流派 they *name*. **Gate (designer decision 2026-10-19): only after "blessings imbue bullets", the context-first Chinese rewrite, the passed-over Keeper line and Muscle Memory stage 1 are on main**, so testers see the final feel and text. Heat and Warden Forms II–III may land after the playtest (they need wins) |
| Sun | Write `Docs/Playtests/v0.1_findings.md`; decide full-game direction. **Go criteria (designer decision 2026-10-19): continue to the full game if 2 of 3 hold — ≥ 2 of 3 testers ask for "one more run" unprompted; ≥ 2 testers can name their 流派; the designer still wants to play after the playtest. If all testers say it felt tiring, or none can name a 流派, fix the core feel before adding content**; tag `v0.1` |

**Done when:** success criteria from GDD §10 hold for at least 2 of 3 external testers.

> External testers (designer decision 2026-10-19): mostly **KBM**, **bilingual** (both en and zh-Hans must be ready; have at least one tester play in each language), novices and veterans, **mostly veterans** — read the §3.5 difficulty targets (written for first-time players) per tester experience. The human pad-only pass is therefore not a playtest gate, but it is still owed before the `v0.1` tag; **the designer will borrow a gamepad** for it.

> External playtest focus (designer decision 2026-10-19): the two moments testers should talk about afterwards are **the moment a 流派 comes together** and **a fusion card**. Record for each tester: the minute their Path reached Tier 2 and Tier 3, whether they noticed it (ask "when did your build click?"), whether a fusion was offered at L16, and what they said when they saw it.

### v0.1 additions (designer decision 2026-10-19)
New v0.1 scope from the planning session. ~9–11 dev-days in total. **Tag `v0.1` as soon as this block, the external playtest and the pad pass are done** (designer decision 2026-10-19) — 29 Nov is the latest date, not the target. Specs: GDD §4.3, §5.6, §6, §8.5; Narrative §4.3, §12; Art §4, §9.

| Work | Spec | Est. |
|---|---|---|
| Passed-over Keeper line at L8 (6 lines en + zh, toast) | GDD §5.6, Narrative §4.3 | 0.5 d |
| Muscle Memory: 3 stages from cumulative progress, save fields, build-screen / Codex display, unlock toast | GDD §4.3 | 2 d |
| Heat 1–5: Shrine selector, unlock chain, 5 skill-check changes in timeline / elite / boss tuning, Ichor multiplier, per-Heat stats | GDD §8.5 | 2–3 d |
| Warden Forms II–III: form by wins, II build-counter + retreat, III phase 4 + dash-synced ring. **Structure + mechanics now; the final look follows the designer's art-book pages, expected ~2026-10-20** (designer decision 2026-10-19, final) | GDD §6, Art §4 / §12 | 2–3 d + art later |
| Return-after-death system line in the hub (cold, precise; full game: reforging animation) | GDD §8.6 | 0.25 d |
| Warden Form III first kill → Layer 3 Codex entry | GDD §6, Narrative §10 | 0.25 d |
| Reclaimer wear stages ×3 on the armour (as designed; material/decal, tied to Muscle Memory) | Art §5 | 0.5–1 d |
| **Art book v0.1 bar** (`05_ArtDirection.md §12`, gap list `Docs/Archive/Art/artbook_v0.3_gaps.md`): G1–G13, G16 environment, G13b hero model, G14 enemies, G17 weather (overcast + sun / volumetric), assets procedural / in-engine / CC0 / OFL logged in `Docs/Art/asset_licenses.md` | Art §12 | **≈ 5–7 weeks solo-equivalent; parallel sessions needed for 29 Nov** |
| Codex bestiary: 7 Vigil common-knowledge entries (Narrative §12) | Art §12.1, Narrative §12 | 0.5 d |
| Text for the above (~24 strings en + zh-Hans, context-first) | Narrative §12 | 0.5 d |


---

## Milestone summary

| Tag | Date | One-line test |
|---|---|---|
| `v0.1-m1` | 25 Oct | Can shoot and kill at 60 fps |
| `v0.1-m2` | 1 Nov | 5-minute run with level-ups, pad only |
| `v0.1-m3` | 8 Nov | A run converges into a named 流派 |
| `v0.1-m4` | 15 Nov | 20-minute run can be won |
| `v0.1-m5` | 22 Nov | Meta loop closes across 3 runs |
| `v0.1` | 29 Nov | External testers want "one more run" |

---

## Scope valves (pull these if behind)
1. Fusions: ship 3 instead of 6 (one per "committed" pair most players pick).
2. Enemies: drop Tallyman; keep 5 types.
3. Discoveries: 8 instead of 15 (and 8 Recollections).
4. Tree: trunk + 3 god branches populated, others "coming soon".
5. zh-Hans: UI + card names only, descriptions follow in v0.2.
6. Narrative: Recollections and Codex entries to v0.2; keep names + tier lines (they are the HUD).
7. ~~Art: environment stone pass to v0.2; ship grey-box geometry …~~ **Withdrawn for art (designer decision 2026-10-19, final)**: the art book (`05_ArtDirection.md §12`) is the v0.1 bar for environment, hero and enemies too.

Never cut: the 5-card offer, weights + guaranteed slot, L8 commitment, favor thresholds, dash, boss.

---

## After v0.1 (full-game backlog — now scheduled below in "v0.2 → 1.0")
Starting loadouts (replaces "multiple heroes") · co-op (every player a Reclaimer) · more Keepers → **post-release DLC**, not the full game · cross-god fusions · 2nd and 3rd arena, downstream of the Breach (the Outfall, the Sump [proposal]; `04_Narrative.md §11`) · the Refusal run (harder, lonelier, emptier; win condition undecided) · Vigil NPCs cast (`04_Narrative.md §8`) · story Acts II–III and Vigil NPCs (`04_Narrative.md §11`) · difficulty heat · daily seeded runs · Steam (achievements, cloud saves, Deck controls) · full art pass (3D Vigil hub, Warden model, hero armour, remaining environment per `05_ArtDirection.md`) · audio · accessibility · co-op evaluation.

---

## v0.2 → 1.0 — full-game roadmap (draft 2026-10-20)

> Next stage, by the designer (2026-10-20): *"before the v0.1 playtest is done, you can go for next-stage development. Do the core part first; leave the work that needs playtest feedback for now."* D-1 … D-10 are **decided as recommended** (scheduler decision 2026-10-20 (designer's full-auto mandate); `Docs/Design/full_game_specs.md` §8): the Outfall is a separate run; art is derived now; the Floodgate; weapons stay at 10; the Act II→III condition; fate order Shade → Tides; Tidal Wave as a second Intercession; Refusal = **Starve the Yard**; role keys `Lurker` / `Tether` / `Artillery`; local co-op for 1.0, online after.
>
> **CORE** = buildable now, no playtest data needed. **GATED** = needs v0.1 feedback (balance, difficulty, offer weights, onboarding, Heat steepness …); only build its structure until the data is in.
>
> Specs: `Docs/Design/full_game_specs.md` (Outfall, acts, loadouts, Bids, fates, Refusal) · `Docs/Archive/Design/coop_evaluation.md`. The v0.1 rules all carry over: Art §12 *in-game ≥ art book*, Art §3 readability, fiction never a key, en + zh-Hans.

### Keeper identity redesign — NEW TOP PRIORITY (2026-10-20; outranks everything below)
Designer feedback (2026-10-20): blessings must drive the 流派 and the power; 爽 comes from mechanics combining; each Keeper gets one speciality with 3–5 流派.
- **Spec:** `Docs/Design/keeper_identity_spec.md`. **Text:** `keeper_identity_text.md` (~107 strings). Supersedes build_depth §2 (branches) where they conflict.
- **Steps (§9), ≈ 37 d, parallel across 5 sessions:**
  1. Behaviour flags (weapons).
  2. Weapon load + level reshape (weapons / meta-ui).
  3. 72 blessings as archetypes + traits / tiers / imbue (balance).
  4. Intercessions (balance / statuses).
  5. Enemy-side pull / push / freeze-hold / seek classes (statuses).
  6. Modifier flatten + synergy (balance / weapons).
  7. Offers O1–O6 + power-split telemetry (tiers).
  8. Reactions / duos / transformation reworks (hub / statuses).
  9. UI (meta-ui).
  10. zh (loc-zh).
  11. The bot release gate: blessings ≥ 60 % of the L16 power (balance, GATED).
- **Status 2026-10-20 (scheduler):**
  - Steps 1, 2, 4, 5, 6, 7, 8 (hub part) and 9 are **done on main**.
  - Step 3 is **in progress**: balance re-authors the 72 blessings and the numbers; integration does the traits, tiers and imbue.
  - Step 10 (zh) follows the text.
  - Step 11 is **GATED**: the release gate is the offline ablation in `Docs/Balance/power_split.md`, and the §11 baseline is pending from balance.
  - The owner changes since the spec was written are recorded in spec §9: step 3 is split between balance and integration; step 4's effects moved to weapons.

### Build depth — TOP PRIORITY (2026-10-20; outranks v0.2 content, v0.4 and 1.0)
Designer feedback (2026-10-20): L16 should be a qualitative change; runs repeat; melee weapons get flight modifiers; the patron and the second Keeper don't interact.
- **Spec:** `Docs/Design/build_depth_spec.md`. **Text:** `Docs/Design/build_depth_text.md`.
- **Steps (all CORE, numbers GATED), ≈ 33 d total:**
  1. Capability tags + modifier matrix (1.5 d).
  2. Transformation framework + the L16 hand (2 d).
  3. 7 Path transformations (5 d).
  4. 10 weapon evolutions (6 d).
  5. 3 branches × 12 blessings per Keeper, 42 new (6 d).
  6. Offer extensions + 12 omens (3 d).
  7. 15 duo reactions (5 d).
  8. 15 duo blessings (3 d).
  9. Text + zh (2 d).
  10. The bot balance pass is **GATED**.

### v0.2 — "Downstream" (Act II)

| Priority | Work | Spec | Tag | Est. |
|---|---|---|---|---|
| 1 | ✅ **Story state** in the save (Act, flags, per-arena wins / Heat), act transitions, per-act layer audit, `RH.Story.*` | specs §2.1–2.3 | CORE | 2 d |
| 2 | ✅ **The Outfall**, structure: `L_Arena_Outfall` (basin, three current channels, spillway grate, terrace with distant pilgrims), current physics on hero / enemies / gems / enemy bullets, Shrine arena selector, per-arena Heat | specs §1.2–1.3 | CORE | 5–7 d |
| 3 | ✅ **Three new roles**: `Lurker` (Dredge), `Tether` (Linesman), `Artillery` (Slagger): behaviour, telegraphs, drop-in swap, models to the 弃物卷 family language | specs §1.4 | CORE (behaviour) · GATED (HP / damage / rates) | 6–8 d |
| 4 | ✅ **The Floodgate** (Outfall boss): pumps → rising water → the gate cracks; dodge-lane rule; win opens the grate | specs §1.5 | CORE (phases) · GATED (numbers) | 5–6 d |
| 5 | ✅ **`DA_SpawnTimeline_Outfall`** with the events (sluice flush, flood tide, Linesman net) | specs §1.6 | CORE (structure) · GATED (caps, scaling, Ichor bonus) | 2 d |
| 6 | ✅ **Outfall art** to the art book bar: Founder ruins (a second inscription family), water, sluices (Art §12; CC0 / OFL or procedural, logged) | Art §12 | CORE | 8–12 d (parallel) |
| 7 | ✅ **Keeper Bids**: generator injection, three Terms, Bid card (stone + contract seal), call-in at L8, telemetry, tests | specs §4 | CORE (system) · GATED (chance, term numbers) | 4 d |
| 8 | ✅ **Starting loadouts** (6): data, Shrine picker, rig traits / drawbacks, unlock Discoveries | specs §3 | CORE (system) · GATED (trait numbers) | 3–4 d |
| 9 | ✅ **Act II text** (~83 strings × 2): 15 Recollections L3, Tides / Dead Act II tier lines, Act II passed-over set, Bid lines, Dead's `[not filed]`, Vigil NPC remarks, Outfall names / Codex | specs §2.4, §1.8 | CORE (writing) · GATED (onboarding placement) | 5 d |
| 10 | ✅ **Vigil NPC remarks in the hub** (5 NPCs as labels per place, by story state; no dialogue UI) | specs §2.4, Narrative §8 | CORE | 1–2 d |
| 11 | ✅ Outfall Discoveries (+6) + 2 modifiers (*Backwash*, *Spillover*) + Tide Legendaries | specs §1.7 | CORE · GATED (weights) | 3 d |
| — | Balance pass on all of the above against v0.1 findings | — | **GATED** | after the v0.1 playtest |

> Status 2026-10-20 (scheduler): rows 1–11 are **done on main**. Still open from *Done when*: **the Outfall is not yet winnable** (balance is fixing it). The GATED balance pass waits for the v0.1 playtest.

**Done when:** from a save with one Breach win, the Outfall is selectable and winnable; Act II text and Bids appear; every new system has automated tests; `RH.Codex.ShowLayer` shows max layer 3 in Act II.

### v0.3 — "Refusal" (Act III)

> **Started 2026-10-20 (scheduler):**
> - environment: the Sump map;
> - meta-ui: the fates and ending UI;
> - weapons: the *Refit* modifier;
> - hub: an E2E skeleton for the Act III flow.

| Priority | Work | Spec | Tag |
|---|---|---|---|
| 1 | ✅ (winnability open) **The Sump** (arena 3): seeding works, conveyors, pylons, roles `Carrier` / `Siphon`, boss **the Font** | `Docs/Design/v03_refusal_spec.md` §1 | CORE (structure) · GATED (numbers) |
| 2 | ✅ **Keeper fates** as meta-state: Shade OffLedger → Tides Returned, the Reclaimer card frame, Recollection tiers, second Intercession, patron fallback | `v03_refusal_spec.md` §2 | CORE |
| 3 | ✅ **The Refusal run, Starve the Yard**: 3 + 2 temptation offers, conversion on taking one, Muscle Memory stage 4, starve curve, presentation switches | `v03_refusal_spec.md` §3 | CORE (rules) · GATED (curve, bonuses) |
| 4 | ✅ **Act III text** (`Docs/Design/act3_text.md`, 59 strings) and **the ending sequence** (7 shots, frozen-dusk sky, epilogue state) | `v03_refusal_spec.md` §4 | CORE |
| 5 | ✅ Keeper personalities as mechanics (GDD §11): design approved (D-52 … D-56) and **implemented** (integration b78dd9e / da84a76 / 0efcd3f, Fifty a8df6cc, Settled f78f5f5; meta-ui remarks toggle; loc-zh 24 lines) → `Docs/Design/keeper_personality_spec.md` (Disposition, Bid temperaments, remarks, rivals; no combat power) | GDD §11 | CORE (design) · GATED (tuning) |
| 6 | Superseded by [build_depth_spec.md §4](Design/build_depth_spec.md#4-patron--second-keeper-duo-reactions-15-and-duo-blessings--core-rules-gated-numbers). | build_depth_spec §4 | done |

> **Status 2026-10-21 (planning, against main 107c772):**
> - **Row 1:** everything is done, including the **4 Sump Discoveries and SumpWin → Refit** (meta-ui d7a4733). **Still open: Sump winnability only.** Work in flight:
>   - hub's before / after probe;
>   - balance's levers 1–3: turret share / cap (dffd9ac), integration's bot dodge (01a0b75), statuses' Turret wind-up glint.
> - **Rows 2, 3, 4:** done. The 4 Sump Recollections landed with d7a4733.
> - **Row 5:** implemented.
> - **Row 6:** superseded (built in v0.2 build depth).
> - **v0.3 waits only on Sump winnability**, so v0.4 co-op **S3+ is open to integration as of 2026-10-21** (scheduler).

### v0.4 — "Two vessels" (co-op, local)

> **Opened 2026-10-21 (scheduler):** steps S3+ are open to integration, since v0.3 now waits only on Sump winnability.
Spec: `Docs/Design/v04_coop_spec.md` (D-18 … D-23). About **5 weeks** of steps S1–S12:
- multi-hero ownership;
- split run state into shared + per-player;
- owner-tagged events;
- nearest-hero targeting;
- centroid spawns;
- shared camera with a leash and zoom;
- non-pausing level-up;
- local players and input;
- two-player HUD and hands;
- downed / revive;
- meta for two (guest vessel or second profile);
- bots and tests.

The steps are **CORE**. Co-op multipliers, the level-up window and revive numbers are **GATED** on two-bot runs and a couch test.

### 1.0
Spec: `Docs/Design/v10_polish_spec.md` (D-24 … D-30): Quota 6–10, the audio plan (layered score per arena, ambience, sourcing), the 3D hub as a camera-rail diorama, the Steam ship checklist, the 1.0 performance targets including a new **Deck** tier.

**CORE:**
- Heat beyond 5. **Done 2026-10-27:** Quota 6-10 as `v10_polish_spec.md` §1 (numbers GATED, `URHQuotaTuning`; TDD §17).
- Daily seeded runs (`FRandomStream`, TDD §1).
- Steam: achievements, cloud saves, Deck controls.
- Accessibility (remap, text size, a colour-blind check of the A1 palette against Art §3).
- Full audio (Art §11, the music bed).
- 3D Vigil hub (Art §9).
- Performance re-certification on all scalability tiers with three arenas.

**GATED:**
- Final balance across the three arenas and Heat.
- Onboarding (first-run tutorial beats).
- Difficulty tiers.

**After 1.0:**
- Online co-op (option B).
- More Keepers as DLC (Narrative §11: reconcile the six facets first).

- [ ] **D-71 step 2 (2026-10-23):** All three shipped timeline assets rewritten with BossTime 900; factory clock/slopes, gem XP x1.2 and credited XP counter, Refusal 18:00 fail-safe, comments and lifetime test corrected. Full RogueHero suite 570/570 PASS, SoloParity unchanged; text gates PASS. Direct single-file/unity compilation and relink pass without warnings; fresh Build.bat/BuildNonUnity.bat script passes remain blocked by UBT user-profile trace access (round-2 review passed both). Weather migration outside allowed paths and the 22-level/36-decision acceptance measurement remain with the scheduler; details in TDD section 17.
