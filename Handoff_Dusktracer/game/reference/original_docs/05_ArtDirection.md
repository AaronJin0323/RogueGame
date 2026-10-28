# RogueHero — Art Direction (Prototype v0.1 → full game)

> Status: **decided 2026-10-18** through a design interview with mock-ups. Supersedes the one-line "neon on marble" mood in GDD §2 and the "grey-box + VFX" assumption in the TDD.
> Direction in one line: **realistic, physically-plausible rendering of an eroded white megastructure at dusk — and every effect on screen is still readable at 2 000 bullets.**
> Binding sections: §3 (readability under realism), §7 (performance contract), §9 (v0.1 scope), §12 (the art book — the binding visual reference, designer decision 2026-10-19). Anything marked [proposal] is Claude's fill-in; change freely.

---

## 1. Decisions (from the interview)

| Question | Decision |
|---|---|
| Render style | **Realistic** (PBR, Lumen GI, real-scale materials), not stylized. References: *The Ascent* (realistic top-down shooter), *Alienation* (realism + swarm density), *NieR:Automata* (white ruins mood). |
| VFX | **Physically plausible realism**: tracers, muzzle flash, sparks, debris, heat distortion, smoke — no cartoon rings or flat glows — **but colour-coded** (player cool, enemy warm, telegraphs as projected light). See §3. |
| The Keep's material language | **White stone megastructure, eroded by time**: marble/ceramic plates and columns, cracks, moss, standing water, dust, half-buried old machines. Six-colour energy visible in seams as plausible light sources (conduits, status lamps, cooling vents). |
| Lighting | **Overcast / dusk, soft light.** No hard sun. Slightly warm horizon, cool sky; emissives read naturally. |
| Hero | **Realistic human, lightly armoured.** |
| Performance target | **RTX 4070-class @ 1080p/60** with Lumen + VSM; RTX 3060-class on a lower scalability tier. Bullet/enemy caps unchanged. |

Why realism works for *this* game specifically: the enemies are **machines** (Cast-offs) — hard-surface assets need no skin, hair or cloth, and kitbash well; the environment is **stone and ruin** — Megascans' strongest category; the hero is small on screen at a 62° top-down pitch. The expensive parts of realism are mostly absent from the design.

---

## 2. The look

**The Keep / 守境.** A Founder-built structure so large its edges are rumour. The arena is one plate of it: a floor of fitted white stone slabs, each a few metres across, worn to grey at the joints; low walls and broken columns of the same stone; shallow pools where the drainage failed; moss and lichen in the north-facing cracks; wind-blown grit in the seams. Beneath and through it, **the Keepers' infrastructure**: conduit trenches under glass, cooling vents, relay masts, status lamps — each carrying its Keeper's colour, dim, steady, *working*. Half-buried in the plate are **older machines**, rusted, of the same family as the Cast-offs; they are set dressing and cover, and they tell the player what the enemies are before the Codex does.

**Variable weather (designer decision 2026-10-19, final).** Two dusk states, both built: **overcast** (as below) and **low sun + volumetric cloud + light shafts** (art book). A weather system picks or blends them per run or per segment, **in the arena (combat) and in the hub, in v0.1**. Each state has its **own GPU budget**, measured against TDD §14 (Epic ≤ 12 ms, 3060 tier), and each must pass the §3 contrast check. In the sun state, telegraphs must still read against long shadows (3:1 against the darkest shadowed slab); if they don't, the shadow length is clamped. The overcast state: The sky is a soft grey-violet dome with a low warm band at the horizon (Keeper of Light's direction [proposal]). Ambient dominates; shadows are soft and short. The scene's own emissives — conduits, lamps, the hero's rig, bullets — are the brightest things on screen without being the *only* things. Exposure is fixed per arena (no auto-exposure swings when a big burst fires).

**Scale and camera.** Slabs ≈ 3 m, hero 1.8 m, Scrapwing ≈ 1 m wingspan, Bulwark ≈ 2.5 m, Breaker ≈ 4 m, the Warden ≈ 12 m. Camera: pitch −62°, FOV 50°, ≈ 26 m height, ≈ 27 m visible width (TDD §13). At this distance a human is ~60 px tall at 1080p, which is why hero fidelity matters less than hero *silhouette* and *rig light*.

**Palette.**

| Role | Colour | Rule |
|---|---|---|
| Ground / walls | warm-neutral greys, bone white in highlights, moss green and rust in accents | desaturated; nothing on the ground competes with bullets |
| Sky / ambient | grey-violet, horizon warm amber | soft |
| Player bullets | **cool**: per-weapon hue in the cyan–white–pale-gold range | saturated, emissive, bloomed; never warm |
| Enemy fire & telegraphs | **warm**: magenta-red tracers, red laser-projected ground markings | the *only* saturated warm elements in combat |
| Keeper colours | **Locked by the art book A1 (designer decision 2026-10-19, §12):** Storm #7FB4FF · Forge #FF9F3A (amber) · Tide #6FE0E8 · Shade #8E62FF · Radiance #FFE28C · Quicksilver #A8F25A (glow / deep values in A1) | used on conduits, blessing VFX, HUD, cards; Forge's warm hue is tuned toward amber to stay distinct from enemy magenta |
| Hero | pale grey armour, one Keeper-coloured rig light (the Patron) | the hero is the brightest *neutral* object |

---

## 3. Readability under realism (binding — these override "realistic" when they conflict)

Realism is the *surface*; the GDD §9 readability rules and the §3.5 anti-fatigue rules are the *contract*. In a realistic scene they are implemented like this:

1. **Player bullets are tracers, not sprites.** Emissive core + short trail + small bloom; hue per weapon within the cool range. Physically plausible (every real tracer glows), always visible against grey stone.
2. **Enemy bullets are warm tracers with a dark core**; enemy muzzle flashes are warm. Player fire is cool, with **one exception (designer decision 2026-10-20, playtest 5, 「尾迹用司色，弹芯保持冷色」)**: **imbued bullets and fusions carry their Keeper's colour in the tail and glow, including Forge amber and Quicksilver green; the core stays cool / near-white.** The tracer *shape* keeps player and enemy fire apart even when hues are close: a player round has a bright core and a coloured tail, an enemy round has a dark core and a magenta rim. Guard rails:
   - **Forge amber** stays at A1 #FF9F3A (hue 32°), far from enemy magenta (338°). It never reaches red or magenta, and it is never used on the tracer head.
   - Keeper-colour tails stay under the density dimming of item 10.
   - The 3:1 contrast check (item 9) includes a warm-tail-vs-enemy-round test in the densest late-game frame: warm enemy fire must still be found in one saccade. If it fails, the tail's saturation and length are reduced; the rule itself stays.
3. **Telegraphs are projected light.** A Breaker's slam is a red laser-projected ring on the ground (its own targeting system); a Courser's charge line is a red light-strip; the Warden's rings are projected arcs. Diegetic, bright, and still "drawn on the ground before a dangerous action".
4. **Enemies carry a rim/status light** in warm magenta (an optical sensor, a reactor glow). Dark machines on grey stone at dusk need it. Elites carry a brighter one; affixes change its pattern.
5. **Kill feedback is physical and layered**: sparks + debris + a short smoke puff per kill; above 20 kills/s the smoke pools into a single ground-level haze and individual sparks are culled (TDD effect LOD). Hit-stop and shake follow GDD §3.5 budgets.
6. **Element statuses are realistic phenomena**: Shock = arc discharge jumping between hulls; Burn = flame licks + embers + heat distortion, smoke on death; Chill = rime frost on hulls, frozen = ice sheath, shatter = ice debris; Mark = a thin violet scan-line on the hull (Shade's "ledger"); Blind = overexposed white flash on the sensor; Quicksilver's trail = heat shimmer + dust.
7. **Ultimates are the one allowed excess**: full-screen but still physical (Thunderhead = real lightning strikes with ground scorch; Forge Nova = expanding fire front with debris; Sun Lance = overexposed beam with lens flare; Closing = violet particulate drawn inward). ≤ 1 per 60 s by construction.
8. **Post-process**: bloom moderate (threshold tuned so only tracers and emissives bloom), no chromatic aberration, no motion blur, DOF off in play, light film grain, dusk colour LUT. Vignette mild. Nothing that smears a bullet.
9. **Contrast check**: a bullet must be ≥ 3:1 luminance against the darkest floor material; an enemy's status light ≥ 3:1 against the brightest. Verified with `RH.Debug.Contrast` [proposal] which tints the frame by luminance band.
10. **Density dims player fire** (designer decision 2026-10-19, R15). Player tracers scale emissive intensity and trail width down as the live player-bullet count rises (mostly the danmaku weapons, GDD §5.2), so a full screen of cool fire reads as a veil and warm enemy fire stays the brightest thing in frame. Impact and kill density follows the **art-book density LOD** (PL.18 / A3, §12; designer decision 2026-10-19: it replaces the lasting-wreckage / decal / shockwave plan of `Docs/Balance/r15_bullet_targets.md §4`).

---

## 4. Enemies: the Cast-offs as hard-surface machines

All six share a family language — **Founder-era industrial hardware**: matte grey-white ceramic armour plates (same material as the Keep, which is the story), exposed dark-metal mechanisms, warm status optics, their origin Keeper's colour on a conduit or lamp, rust and moss where the plates meet. Kitbash-friendly; no organic surfaces.

| Role | Name | Silhouette | Notes |
|---|---|---|---|
| Swarm | Scrapwing | small flat delta drone, two canted rotors / vanes | reads as a flock; one magenta optic |
| Tank | Bulwark | squat tracked frame carrying a tall front plate | the plate is the Keep's stone — literally a wall that walks |
| Rusher | Courser | low six-legged transit unit, headlamp strip | charge telegraph = its headlamp strip going red |
| Turret | Flare Pylon | 3 m heliostat mast with a mirror head | aims the mirror; muzzle = focused light |
| Bruiser | Breaker | 4 m ice-breaking ram on hydraulic legs | slam telegraph projected from its own targeting laser |
| Support | Tallyman | wheeled cabinet with a rotating ledger drum and antenna | its buff aura is a visible data-link beam to allies |
| Boss | the Warden | 12 m, placeholder: a Breaker-class frame scaled up with the other five's parts bolted on | **three forms** across the meta-progression (designer decision 2026-10-19, GDD §6): I as now · II plates torn open, something pale and person-scaled visible inside · III all six Keeper colours in its conduits. One kitbash per form; telegraphs unchanged |

> **Binding reference (designer decision 2026-10-19):** the Cast-off volume of the art book, **弃物卷** (§12.1, PL.C00–C17), supersedes the silhouette column above wherever they differ. The in-game models must be at least that detailed.

Elite affixes: Armored = extra plates; Swift = extra vanes/legs; Volatile = leaking reactor glow; Splitter = carries Scrapwings docked; Vampiric = draining cable to its victims; Shielded = a visible hex-field projector.

---

## 5. The hero: the Reclaimer

A realistic human in **light armour**: ceramic plates over a dark undersuit, no helmet (face is irrelevant at this camera, but the silhouette reads "person" not "mech"), a backpack rig that holds the weapons — the four weapon slots are physically mounted: shoulder emitters, a hip launcher, drones that dock on the rig. One **rig light** in the Patron's colour; it is the player's anchor in every frame. Animation: locomotion blendspace + dash + ultimate cast; weapons auto-fire from the rig, so no aiming animation is needed. Sourcing: a Fab/Marketplace realistic character with retargeted animations, or MetaHuman body + custom armour later; v0.1 uses the best available pack.


**Wear across the whole game** (designer decision 2026-10-19). The Reclaimer's look records how long the body has been in use, the visible twin of Muscle Memory (GDD §4.3): one wear stage per Muscle Memory stage, **on the armour as designed** (designer decision 2026-10-19, final; this overrides art book PL.05's "armour is always new"): 1 scuffed and re-taped plates · 2 mismatched replacement plates and an older rig fitting · 3 a Founder-era rig part that does not match FOUNDRY's work. Material/decal swaps on the same mesh, no new rig. v0.1 (valve 7 withdrawn for art).
---

## 6. UI

HUD and cards follow the "austere, withholding" tone: thin type, few boxes, Keeper colours as accents only, dark translucent panels that do not fight the scene. Cards show a **real render** of the item (bullet VFX loop for modifiers, Keeper sigil for blessings), not icons, where cheap. Hub (Vigil) is a 3D scene of the same materials at the Keep's edge — a room with the Shrine, the Contract wall, the Codex — not a flat menu.

---

## 7. Technical consequences (binding; details in `02_TechnicalDesign.md`)

| Setting | Was | Now |
|---|---|---|
| Lumen GI + reflections | off | **on, software ray tracing** (HWRT optional on 40-series) |
| Virtual Shadow Maps | off | **on** |
| Nanite | off | **on for environment static meshes**; off for enemies (instanced, animated) |
| Anti-aliasing | TSR | TSR, upscale from ~70–80% on the 3060 tier |
| Target | RTX 3060 @ 1080p/60 | **RTX 4070-class @ 1080p/60 (Epic)**, RTX 3060-class @ 1080p/60 (Medium/High via scalability) |
| Budgets | game thread ≤ 8 ms | unchanged; **GPU ≤ 12 ms** at Epic with 5 000 tracers + 400 enemies |
| Bullet rendering | Niagara array DI sprites | same, but **tracer ribbons** (ribbon renderer) instead of billboards; emissive material; effect LOD by density |
| Enemy rendering | SoA + ISM (Week-1 fallback) | same path, now the **intended** path: ISM + vertex-animation or bone-less procedural animation for swarm; skeletal only for boss and elites |

Scalability groups must be set up by Week 6 so the 3060 tier exists in the build.

---

## 8. Asset sourcing (solo-feasible plan)

- **Environment**: Quixel Megascans via Fab (free with UE) — white marble/limestone surfaces, rubble, moss, puddle decals, stone blocks; a few modular sci-fi conduit/vent kits from Fab for the Keepers' infrastructure.
- **Enemies**: Fab hard-surface drone/mech kits + Blender kitbash into the six silhouettes; one shared material set (ceramic plate / dark metal / rust / emissive).
- **Hero**: Fab realistic soldier/operator pack with Epic-skeleton animations; retarget.
- **VFX**: Niagara, built in-house (this is the art the project actually authors); Fab realistic FX packs (sparks, smoke, debris) as ingredients.
- **Audio**: unchanged plan.

---

## 9. v0.1 scope (exactly this)

Realism costs are front-loaded into VFX, which v0.1 needs anyway, and back-loaded into the environment, which can slip.

| Item | When | Must / Can slip |
|---|---|---|
| Project settings: Lumen, VSM, Nanite(env), dusk sky & exposure, post-process | Week 1 Mon (revise Day-0 settings) | must |
| Tracer-style bullets, physical hit/kill FX, warm enemy tracers, projected telegraphs | Weeks 1–4 as each weapon/enemy lands (replaces "unique bullet VFX") | must |
| Element status FX as §3.6 | Week 3 Wed | must |
| Enemy kitbash models ×6 (ISM-ready), shared material | Week 4 Mon–Wed (alongside behaviours) | must for Swarm/Tank/Rusher; others can use scaled primitives |
| Hero model + animations | Week 4 Fri | **must, to art-book detail** (§12; designer decision 2026-10-19, final) |
| Environment art pass: slabs, erosion, water, conduits, buried machines — **to art-book detail (§12)** | Week 6 Mon–Tue (replaces "feedback pass" day, feedback folds into VFX work) | **must** (designer decision 2026-10-19, final: valve 7 withdrawn for art) |
| Hub as a 3D room | — | v0.2; v0.1 hub is a UI over a **rendered** dusk backdrop at art-book detail (PL.23) |
| Scalability tiers (4070 Epic / 3060 Medium) | Week 6 Thu | must |
| Reclaimer wear stages ×3 (§5, designer decision 2026-10-19) | Roadmap "v0.1 additions" | can slip to v0.2 |
| Keeper sound signatures ×6 + restrained music bed (§11) | Week 6 Tue (with audio placeholders) | signatures must; music can slip to v0.2 |
| Warden Forms II–III kitbash (designer decision 2026-10-19) | Roadmap "v0.1 additions" | must for the silhouette change; detail can slip |

~~**Scope valve 7 (roadmap):** ship v0.1 on grey-box geometry.~~ **Withdrawn for art (designer decision 2026-10-19, final)**: every art area meets the art book in v0.1 (§12).

---

## 10. Open items

| Item | State |
|---|---|
| Six Keeper hues (exact values) | **locked 2026-10-19 from the art book A1** (§2, §12) |
| Horizon direction = Keeper of Light | [proposal] |
| Hero asset pack choice | decide Week 4 |
| Warden silhouette | placeholder kitbash by design |
| `RH.Debug.Contrast` | [proposal]; implement if readability problems appear in Week 2 playtest |

---

## 11. Audio direction (designer decision 2026-10-19)

**Epic orchestral, but not loud.** Strings, low brass and choir carry the scale of the Keep; the mix keeps music *under* the combat (−6 dB under SFX during Harvest), because the §3.5 anti-fatigue rules apply to the ear too: dense should sound rich, not shrill. Rules [proposal]:

- **Music follows the rhythm** of GDD §3.5: Harvest = a steady ostinato; Threat = brass and percussion enter; Breath = drop to a held chord or near silence. The Warden gets the full orchestra, and each Form (GDD §6) adds a layer; Form III brings in a six-voice choir.
- **Keeper sound signatures** (accepted idea; sounds are [proposal]): a short motif + texture per Path, heard on the blessing pick, the tier toast, the passed-over line and — subtly, low in the mix — on imbued bullet hits.

| Path | Signature texture | Motif instrument |
|---|---|---|
| Storm | dry electric crackle, distant thunder roll | low brass stab |
| Forge | hammer on anvil, bellows breath | timpani + anvil |
| Tide | deep water, ice creak | low strings, slow |
| Shade | ledger page turn, a stamp | solo cello, muted |
| Radiance | glass ring, high harmonic | high choir / celesta |
| Quicksilver | wind rush, relay clicks | fast woodwind run |

- **Warm vs cool carries over:** enemy fire and telegraphs get the sharp, attention-grabbing sounds; player fire is soft and textural, so danger is heard first.
- The **Reclaimer has no motif** — the empty vessel. (Full game: a motif that assembles slowly from fragments of the six.)
- v0.1 scope: placeholder SFX (Roadmap W6 Tue) + the six signatures; the music bed can slip to v0.2.

---

## 12. The art book — binding visual reference (designer decision 2026-10-19)

**Source:** the designer's art book **RogueHero 美术集 v0.3** (claude.ai/artifact/F4xyLbiWMG8UnfqsbcWbNc), 29 artboards. Quoting the designer: the art book is the art direction, and **the finished in-game version must be more detailed than the art book**.

**Rules**
1. **In-game ≥ art book detail.** Each artboard sets the *floor* for the asset or effect it governs. A4 says the illustrations are references for proportion, shape, colour and light, not final assets; the in-game result must be at least that detailed.
2. **Art §3 (readability) wins any conflict.** Next come the GDD for mechanics, the string tables and the loc session for text, and Narrative for fiction (the gap list explains each).
3. **The v0.3 epic rules** apply to every environment, shrine, hub and menu image:
   - megastructure scale: people are an order of magnitude smaller than the building, with fog layers for depth;
   - inscription and relief on all stone;
   - sky and light: volumetric cloud, the sun in the composition, light shafts;
   - ritual: pilgrims, lanterns, braziers, Keeper banners.

   The v0.3b detail layer comes on top. **Ritual in and around the arena** (designer decision 2026-10-19, final; the scheduler's recommendation, chosen by the designer): pilgrims, lanterns, braziers and banners fill Vigil and the shrines. Around the arena they appear only as **distant, static, low-contrast silhouettes on terraces outside the play bounds**, watching the sealed Breach. They never appear on the playfield: humanoid figures among 400 enemies would break §3 and cost GPU. **The sky rule** (sun, volumetric cloud) is one of the weather states (§2). Sci-fi is about **10 %**: only thin Keeper-colour system light in seams, blade spines and stele edges. The VFX and HUD pages keep the §3 contract unchanged.
4. **Typography carries the two layers of the fiction:** inscription faces and serif = Vigil; monospace = the system (P01, P24).

**Artboard → section it governs**

| Artboard | Governs | Key binding content |
|---|---|---|
| PL.00 Cover, PL.01 Foreword | §1–§2 tone | "Every mark can be read twice": stone inscription (Vigil) over cast relief (facility) over glowing Founder circuitry |
| PL.02 Reclaimer, PL.03 Turnaround, PL.04 Rig, PL.05 Materials | §5 hero | Pilgrim-knight with gold-inlaid ceramic, patron-colour tabard, arched reliquary back rig, **one lamp behind the head**, **hood + rebreather**, 7.5 heads, androgynous. Silhouette tests: the rig is wider than the torso and taller than the head; the lamp shows from every angle; three of four weapon slots sit on the body. Sockets `socket_Rig_EmitterL / R / Top / HipBox / DockL / DockR / Lamp`. PL.05's "armour always new" is **overridden**: armour wears per §5 (designer decision 2026-10-19, final) |
| PL.06 Game scale | §2 scale + camera, TDD §13 | −62°, FOV 50°, 29 m, ≈ 27 m visible, 71 px/m, hero ≈ 60 px (matches TDD §13) |
| PL.07 Sigils | §6 UI, cards, HUD | Six medallions; `T_Sigil_<Path>_Relief / _Card / _HUD`, `M_Sigil_Live` animates only in menus, the Shrine and the Codex |
| PL.08–13 Six shrines | §2 Keeper infrastructure, hub portraits | Storm weather mast on a ziggurat · Forge furnace temple with the empty assembly table · Tide six-tier well · Shade ledger drum with one black row · Radiance heliostat field · Quicksilver viaduct. Tide is told apart from player cyan by frost and stillness. *(The art book's "Forge amber never a bullet colour" and "Quicksilver VFX colourless" are **overridden for bullet tails** (designer decision 2026-10-20, playtest 5): imbued and fusion tails carry the Keeper colour, with a cool core (§3.2). Quicksilver's non-bullet VFX stay colourless: haze, dust, afterimage.)* |
| PL.14 Weapons, PL.15 Fusions, A3 | §3.1, GDD §5.2 / §5.5 | Per-weapon hue table; tracer anatomy; fusion = same core, Keeper-colour tail, one shape change; Crucible's tail is amber with a cool core (§3.2, designer decision 2026-10-20, playtest 5) |
| PL.16 Enemy fire and telegraphs | §3.2–3.3 | Only saturated warm in combat; fill / sequence timings per role |
| PL.17 Status, A3 | §3.6 | One hull, four zones; priority and LOD rule |
| PL.18 Density, A3 | §3.5, GDD §3.5 | Density LOD table (**wins over R15's wreckage plan**, designer decision 2026-10-19, final); shake / hit-stop budget |
| PL.19 Ultimates | §3.7 | Wind-up 0.3 s (lamp white) → Keeper colour → tail; ≤ 1 full-frame excess per 60 s |
| PL.20 HUD, PL.24 Type and panels | §6, TDD §12 | HUD elements ①–⑨ with positions and sizes; fonts; type scale; three panel states; **colour has five duties in UI, and magenta / red never enter UI** |
| PL.21 Level-up, PL.22 Cards | §6, GDD §5.6 | **Two card families (designer decision 2026-10-20, playtest 5):** *weapon, upgrade and modifier* cards use a **high-tech design** (machined metal and glass, a holographic window with the live effect, circuit traces: the Founders' system layer); *blessing, Keeper choice (L8), fusion (L16) and Legendary* cards keep PL.22's **stone tablets** (rarity by stone, kind by plaque, the arched window with the sigil). **Rarity reveal effects are much richer**, in the spirit of a card-pack opening (Hearthstone-like build-up, burst and settle), with **original visuals**: Common a short shimmer; Rare a material sweep + glint; Epic a charged build-up, light burst, rim light and dust; fusion / Legendary a full reveal beat. All of it plays on the paused level-up screen, never in combat, so the §3.5 anti-fatigue and §3 readability rules for combat are untouched; the reveal must be skippable, and A confirms immediately |
| PL.23 Vigil | §8 hub, §9 | UI over a rendered backdrop; Shrine portraits are crops of PL.08–13 |
| A1 Palette | §2 palette, §3.9, §7 | Locked hues and render constants (exposure 0.62, bloom threshold 1.0, grain 0.12, lit stone L ≤ 0.20 so tracer core / lit stone ≥ 3.9:1) |
| A2 Keepers | Narrative §4, GDD §5.4 | Keeper data sheet. Facets stay out of player text |
| A4 Prompts and pipeline | §8 sourcing | How the plates were made; asset keys; Fab picks follow the art book's silhouettes and colour values |

**Enemies and the Warden are now covered** by the Cast-off volume (§12.1, delivered 2026-10-19). Not covered yet: the Warden's Forms II–III (only Form I and its phases P1–P3 are drawn) and the arena layout. Until those pages exist, GDD §6 and §4 are the spec.

**Scope and sourcing (designer decision 2026-10-19, final; supersedes the same-day answers recorded earlier).**
- **The rule *in-game ≥ art book* applies from v0.1, to everything.** That includes the XL items: environment (G16), the Reclaimer's full model (G13b) and the Cast-offs' detail pass (G14). **Scope valve 7 is withdrawn for art.** 29 Nov stays the latest date.
- **We find or create the assets ourselves**, from three kinds of source: procedural content, in-engine authored content, and **clearly licensed CC0 or OFL** sources (e.g. CC0 texture / HDRI / model libraries, the OFL font families of P24). **Each external asset is logged in `Docs/Art/asset_licenses.md`** (source URL, licence, author, date, where used). Nothing with an unclear licence is used. Art-book renders are references, not textures.
- **Fonts:** the P24 families are OFL, so they are imported and logged; `RHUIStyle::Font` gets them.

### 12.1 Cast-off volume — 弃物卷 (designer decision 2026-10-19)

**Source:** **RogueHero 美术集 · 弃物卷** (claude.ai/artifact/TvpEZdPW4CSsmqEtSs8qYk), 18 boards. It is the **binding reference for the Cast-offs and the Warden** (Art §4, GDD §6), under the same rules as §12: in-game ≥ art book detail; §3 wins conflicts; GDD for mechanics and numbers; string tables for text; Narrative for fiction.

| Board | Governs | Binding content |
|---|---|---|
| PL.C00 Cover | tone | The Warden walks out of a breach in the Keep wall; Scrapwings circle it; a red charge band on the ground |
| PL.C01 Family language | §4 family | (1) **Keep ceramic plates** (bone white / kiln grey) with Founder script bands and fret, rivets, access doors, faded stencils; chips show dark primer; rivets bleed rust. (2) **Dark metal mechanisms exposed** (hydraulics, tracks, joints, cables). (3) **Magenta optics**, at least one per unit, always lit. (4) **Origin-Keeper colour only on light guides and lamps**: dark, steady, thin, readable only up close. (5) **Every unit carries an Ichor cell and a seal** (six dots + an entry number). The hull is old; **the seal is new**. Scale row: Reclaimer 1.8 · Scrapwing 1 · Tallyman 1.8 (antenna 3.2) · Courser 2.6 long · Bulwark 2.5 · Pylon 3 · Breaker 4 · Warden 12 m. Swatches: hw #D8D2C5, hwk #A39D92, dark metal #4A4A52, bright metal #9A958E, rust #7A4A2E, optic #FF3B9E / elite #FF7AC2, telegraph #FF2A2A. Top-down first: each unit is 30–200 px at the 62° camera |
| PL.C02–C03 Scrapwing (`Swarm`, ATMOS) | `SM_Enemy_Swarm` | Flat triangular body + two canted ducts; **a "Y" from above**; cup anemometer, wind vane, leading-edge sensor strip; nose optic; three dim Storm-blue guide segments on the spine; broken right wing tip. Swift elite: extra ducts + side fins, fast-flashing light. Render: ISM + VAT |
| PL.C04–C05 Bulwark (`Tank`, FOUNDRY) | `SM_Enemy_Tank` | Tracked chassis carrying **a Keep stele slab taller than itself** (six Keeper names incised, moss climbing). **A horizontal bright edge from above**: the slab's facing *is* the damage-reduction facing. Assembly gantry, four hydraulic clamps, furnace grille, a Forge-orange light guide; a periscope sensor head looks over the slab. Armored elite: side ceramic + rusted iron plates |
| PL.C06–C07 Courser (`Rusher`, RELAY) | `SM_Enemy_Rusher` | Low capsule body + six legs; **a shuttle from above**, the nose is the charge direction. **Telegraph:** the nose light strip turns from magenta to red #FF2A2A for 1 s and projects a red arrowed band on the ground, **width = collision width**. Folded pantograph, Ways-green livery lines, route plate, rear coupler. Volatile elite: cracks leak magenta-white light, it explodes on death |
| PL.C08–C09 Flare Pylon (`Turret`, LUMEN) | `SM_Enemy_Turret` | Tripod + tall mirror mast + honeycomb mirror head; **a hexagonal disc from above**. The mirror's facing is the aim; it flashes once before firing; a 3-round burst every 2 s. Ballast stones, carved turntable, service ladder, a Radiance-gold light guide. Shielded elite: a magenta hex shield that breaks after N hits, with the projector visible |
| PL.C10–C11 Breaker (`Bruiser`, CISTERN) | `SM_Enemy_Bruiser` | Lying boiler + forward ram + four hydraulic legs; **an "I" with a hammer head from above**. **Telegraph:** a red ring #FF2A2A from its aiming laser fills inside → out; ring radius = damage radius; knockback points outward from the centre. Cooling coils, two Tide-cyan guides, frost on the belly, an Ichor tank longer than a person. Vampiric elite: a drain cable with magenta nodes |
| PL.C12–C13 Tallyman (`Support`, LEDGER) | `SM_Enemy_Support` | Wheeled filing cabinet + horizontal ledger drum + tall antenna; **a square with a needle from above**. **Buff = a magenta data link** (thin line + flowing dots) from the antenna to each buffed ally; the link's origin is the priority target. Eight labelled drawers, a black row on the drum (**the same row as on Shade's sigil**), a violet read line, paper tape. Splitter elite: three Scrapwings docked on top |
| PL.C14 Elites | §4 affixes | Elites never swap models; affixes are **attached parts**, auto-placed by bounding box (sides / top / rear). Elite optic #FF7AC2 with a **light pattern per affix**: Swift fast flash · Armored steady + brighter · Volatile irregular · Shielded ring sweep · Vampiric breathing · Splitter double flash |
| PL.C15 Warden (`Boss`) | GDD §6 Form I look | A Breaker-class boiler hunched forward 30°; right arm = a Courser capsule forearm with a breaker-ram fist; left arm = **a whole Keep stele as a tower shield** with the six names; both shoulders = Pylon mirror heads (they fire the P2 rings); back = Tallyman tower + **a sealed stone cage**; four Scrapwings racked on each side; four legs on Bulwark tracks. **Each bolted-on part keeps its origin Keeper's guide colour, so six colours sit on one machine.** Crown of six magenta optics (×1.6 in P3). About 40 % of the screen width. Skeletal mesh `SK_Enemy_Boss` (the only skeletal Cast-off) |
| PL.C16 Warden phases | GDD §6 read test | P1 charge along a hull-width red band, then a 120° ram sweep up close · P2 two staggered radial magenta rings from the shoulders + Bulwark summons · P3 faster rings with spiral drift, an outer red ring shrinking inward, all six optics lit, forearm strip red. Danger is always drawn on the ground or in the air, only in magenta / red. **On death the cage seal stays intact** |
| PL.C17 Spec | §4, §7 | Spec table (all of the above). **Emissive budget:** optics 14 (elite ×1.4), origin guides ≤ 3.2, Ichor cell 2.4, telegraphs 12. **Only optics, enemy rounds and telegraphs bloom**; origin guides must not halo at 1080p. Shared `M_HardSurface` (hw / hwk ceramic, dark metal, bright metal, rust mask, emissive), with rust, moss and chip masks fully on: the same material that is new on the Reclaimer is old on the Cast-offs. Render: Swarm / Tank / Rusher ISM + VAT; Turret / Bruiser / Support ISM + procedural animation; elites skeletal; Warden skeletal |

**How the volume fits the decided design (designer decision 2026-10-19):**
- **The Warden's sealed stone cage** is where the decided direction lives: the Warden guards *the Original's old body* (GDD §6, Narrative §6). The volume's note "what the cage holds is undecided (04 §11)" predates that decision. **Form II** (plates torn open, something pale and person-scaled visible inside) opens or cracks **this cage**; the seal stays intact until Form III.
- **Six colours on one machine** are present at a dim level (≤ 3.2) from Form I. **Form III** makes them the event: each origin guide flares in turn with its Path's pattern (GDD §6). The ≤ 3.2 / no-halo budget is lifted only for that moment, and only on the Warden.
- **Warden numbers follow the game, not the boards** (designer decision 2026-10-19): P1 charge wind-up stays 1.1 s; P2 rings stay R14 (3.2 s / 2.0 s, 3-slot dodge lane). Only the ring origin moves to the shoulder mirror heads. Forms II–III are derived as above until the designer draws them.
- **Names: the glossary and string tables win over board labels** (designer decision 2026-10-19), as with the tracer colour (enemy rounds stay A1 #FF2D8A, not the boards' #FF3B9E). The enemy zh-Hans names stay as shipped: Courser = **驿骑**, Tallyman = **算筹**. The boards' 疾行 / 记账机 are overruled.
- **The seals and Ichor cells** are visible from run 1. They are *evidence without explanation* for Layer 2 (*the Breach is a Yard*, Narrative §10): no text points at them.

**Gap list:** `Docs/Archive/Art/artbook_v0.3_gaps.md` (2026-10-19) compares each area with the game, with sizes and the valve 7 / date flags.
