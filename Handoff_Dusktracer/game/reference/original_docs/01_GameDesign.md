# RogueHero — Game Design Document (Prototype v0.1)

> Working title: **RogueHero** (codename). Genre: roguelike · 弹幕射击 · 割草 (survivor-like).
> Engine: Unreal Engine 5.8. Platform: PC (Windows), keyboard+mouse and gamepad.
> Status: planning complete 2026-10-18 · prototype target: 6 weeks (see `03_Roadmap.md`) · world & story decided 2026-10-18 (see `04_Narrative.md`).

---

## 1. Vision

**One sentence:** A top-down survivor-like where *you* are the bullet hell — high-tech weapons and modifiers fire screen-filling patterns, while the blessings of six technological gods shape thousands of possible builds into a handful of recognisable 流派.

**Design pillars (in priority order):**

1. **爽 — but with teeth.** The core feeling is power: the screen fills with *your* bullets and VFX, and crowds die in sheets (割草密度). But it is never mindless: the run alternates **harvest** (dense, weak swarms, big numbers) with **threat** (elites, events, the boss) that must be read and dodged, and a badly built run *does* fall behind the HP curve. Fatigue is a design failure as much as boredom is. Rules and metrics in §3.5.
2. **The build is the game.** Every level-up is a meaningful choice among 5 cards. Weapons × modifiers × god blessings produce thousands of combinations, but tag-weighting, commitment points and 流派 thresholds make a coherent build *reachable*, not a lottery. The build is how 爽 is *earned*.
3. **Player-side danmaku.** The screen fills with the player's bullets, not the enemies'. Enemies are mostly melee swarms; dodging matters against elites and the boss.
4. **15 minutes, one arena, one decision at a time.** No menus mid-run except the level-up card pick. The run should be readable at a glance.
5. **Every run feeds the next.** Ichor and Discoveries unlock permanent blessings and grow the item pool, so the "random pool" grows with the player's mastery.

**Reference games:** Vampire Survivors (loop, scale), Brotato (tag-driven builds, HUD clarity), Hades (god boons, commitment via offers), Path of Exile (support-gem style modifiers), Halls of Torment (3D top-down readability).

---

## 2. Setting & Theme

Full bible: **`04_Narrative.md`**. This section is the one-screen version; the GDD uses the word "god" for the build pillar, the fiction never does.

**Premise (surface layer):** An original setting — no borrowed mythology. The world is **the Keep / 守境**, an ancient structure kept habitable by six great systems that the people of **Vigil / 守夜镇** call **the Six Keepers / 六司** and pay tithes to. From a sealed zone, **the Breach / 裂口**, old machines — **the Cast-offs / 弃物** — spill out in waves. The Keepers employ one operative to clear them: **the Reclaimer / 回收者**, who wakes without memory, fights, dies, and is rebuilt from the **Ichor / 神血** it recovers. Each run it accepts one Keeper's terms (the Patron) and takes their **advances** (blessings) against the work.

**Hidden layer (spoilers, revealed over many runs):** the Six are forks of one mind, each one facet of it; the Reclaimer is that original; the Cast-offs are released on purpose. The full game's arc is **breaking the deal**. Nothing in v0.1 states this; see `04_Narrative.md §10` for what leaks when.

| Layer | Fiction | Game role |
|---|---|---|
| **Weapons** | Founder-era tech from the Reclaimer's rig (rifles, drones, emitters) | Base fire patterns. Auto-fire. |
| **Modifiers** | Tech modules socketed into a weapon | Change how bullets move/split/bounce. Keeper-neutral. |
| **Blessings** | A Keeper's **advance**, written into the Reclaimer's core; Favor = that Keeper's **stake** | Player-wide passives, elemental status, 流派 progression. |
| **Fusions** | A weapon **reforged / 神铸** by a Keeper in its own image | Evolved super-weapon (chase goal). |
| **Enemies** | Cast-offs: each Keeper's abandoned machines (drones, frames, transit units) | Scrapwings, Bulwarks, the Warden. |

**Naming (binding):** each Keeper has a mortal honorific (HUD/cards) and a Founder-era system designation (Codex); code keys are the Path names and enemy roles, never the fiction names — see `04_Narrative.md §13`.

Tone: epic, austere and withholding. **Art direction is decided and realistic** — an eroded white stone megastructure at overcast dusk, hard-surface machine enemies, a lightly-armoured human hero, physically-plausible VFX that stay colour-coded for readability. Full spec: **`05_ArtDirection.md`**; its §3 (readability under realism) is binding.

---

## 3. Core Loop

```
[Hub]  choose Patron god → start run
   └─► [Run: 15:00 timer, one arena]
         move + dash + ultimate; weapons auto-fire
         kill → XP gems → level up → PICK 1 OF 5 CARDS
         L8: choose 2nd god   L16: choose a Fusion
         3:45/7:30/11:15 elite waves · 15:00 boss → kill = WIN
   └─► [Run end] Ichor earned + Discoveries unlocked
[Hub]  spend Ichor on Blessing Tree → pool grows → next run
```

**Run length:** 15 minutes, boss at 15:00, win on boss kill. Death ends the run (keeps Ichor earned so far).
**Level-ups per run:** target 20–24 (curve tuned so L8 lands ≈ 5:30 and L16 ≈ 12:30).
**Choice points per run:** approximately 22 levels and 36 decisions (D-49, progression_v2_spec section 2.4).

### 3.5 爽 with teeth — the run's rhythm (pillar 1, made operational)

**Where 爽 comes from** (in order of weight): (a) **kill density** — swarm enemies die in sheets, ideally one-shot; (b) **VFX density** — the player's bullet count and hit/kill effects, not enemy effects; (c) **feedback** — hit-stop, damage numbers, kill bursts, the sound of a crowd dying at once; (d) **power jumps** — a new modifier or a Path tier visibly changes the screen within seconds of being picked.

**Where tension comes from** (and what keeps it from being 无脑): threats that are **skill checks, not HP walls** — they cannot be solved by more damage alone:
- **Coursers** (telegraphed line, must dash or sidestep) · **Breakers** (slam circle) · **Flare Pylons** (the only ranged enemy; magenta bullets to weave through) · elite affixes **Shielded** (absorbs N bullets — density alone doesn't kill it) and **Swift**.
- **Events** that force movement into uncleared space: Scrapwing ring (5:37.5), Courser stampede (9:22.5), Pylon nest (13:07.5).
- **Boss** rings and spiral (phase 2–3) and the shrinking arena — pure pattern reading.
- **XP geometry**: gems drop where enemies die, so harvesting means walking *into* the crowd you just killed.

**Rhythm.** The 15 minutes are built as alternating phases, authored in the spawn timeline (§7):
- **Harvest** (~67–90 s): spawn rate high, mix mostly `Swarm` + `Tank`, no events. Kill numbers climb; the player is allowed to feel untouchable.
- **Threat** (~30–60 s): an elite wave or event; `Rusher`/`Turret` share rises; damage taken is *expected* here.
- **Breath** (5–8 s): after each elite wave or event the spawn director pauses regular spawns, the chest drops, gems get collected. This is the anti-fatigue valve — every spike has a visible end.
Elite waves at 3:45 / 7:30 / 11:15 and events at 5:37.5 / 9:22.5 / 13:07.5 give a threat beat roughly every 1:52.5; between beats, danger comes only from **drop-ins** (below): single, telegraphed, never stacked.

**Drop-in threats (designer decision 2026-10-19, balance proposal P1, `Docs/Balance/pressure_proposal.md`).** Playtest 3 showed that a strong build kills everything ~1 000 uu out, past every trigger range, so no skill check ever fired between beats. Some threats therefore **arrive already in range, behind a red telegraph**, and act on arrival even if they die a moment later:
- **Courser line:** arrives 1 000–1 200 uu out with its charge line already drawn (the 1.0 s marker is its wind-up), then charges through where the hero stood.
- **Breaker drop:** a red slam circle (r 300) centred 0–200 uu from the hero; the Breaker lands at the end of the 1.2 s telegraph and slams.
- **Pylon perch:** arrives ~1 000 uu out and fires its 3-round burst after its aim line.

Cadence from 4:30 is one every **12 s**, falling to **10 s at 7:30, 7 s at 11:15 and 5 s at 13:30**, and **×2 in Threat blocks**. The mix is 50 % Courser, 30 % Breaker, 20 % Pylon.

Fairness rules:
- At most **1** drop-in telegraph is active at a time.
- At least 1.5 s between landings.
- Never during the boss fight.
- **No HP change and no telegraph change**: same damage numbers, same telegraph durations (§9).

`RH.Stats` adds `threat_actions_per_min`, with targets ≥ 4 at 10:00 and ≥ 8 at 18:00, and `nearest_enemy_p10_uu`.

**Scaling contract.** Enemy HP ×(1 + 0.16·min) is tuned so that a *well-steered* build (reaching its Path Tier 2 by ~8:00) keeps swarm time-to-kill ≤ 0.3 s all run — the one-shot feeling never leaves — while a *scattered* build (no Tier 2 by 12:00) sees TTK climb past 1 s by minute 12 and is overrun. Difficulty lives in the **build**, not in bullet sponges. Enemy *damage* scales slower (×(1 + 0.0667·min)) because dying should feel like being caught, not being chipped.

**Anti-fatigue rules (hard):**
- Camera shake has a **budget**: max 1 shake per 0.5 s, amplitude capped; hit-stop only on crit/kill of elites+ (30 ms), never on swarm kills.
- Screen-filling VFX stay **cool-coloured and additive**; nothing white-flashes the whole screen except the Ultimate (≤ 1 per 60 s by construction).
- Enemy bullets and telegraphs are the only **warm/magenta** elements — the eye finds danger in one saccade (§9).
- No more than **one decision verb** during play: dash. Everything else is automatic. Level-up is the only pause.
- Kill-feedback **layers degrade with density**: ≤ 20 kills/s → individual numbers and sounds; above that, numbers aggregate ("×47") and audio switches to a crowd layer. Loud at 300 kills/min would be exhausting; dense should sound *rich*, not shrill.

**Target metrics (measured by `RH.Stats`, reviewed every playtest):**

| Metric | Minute 2 | Minute 10 | Minute 18 |
|---|---|---|---|
| Kills / min (steered build) | ≥ 60 | ≥ 250 | ≥ 400 |
| Swarm time-to-kill (whole build, not per weapon) | ≤ 0.3 s | ≤ 0.3 s | ≤ 0.3 s |
| Live player bullets on screen (re-baselined R15) | ≥ 5 | ≥ 40 | ≥ 100 |
| Player hit + kill effects on screen (R15; **targets suspended** — re-set under the art-book density LOD, see note) | tbd | tbd | tbd |
| Dashes / min (forced by threats) | 2–4 | 4–8 | 6–10 |
| Damage taken / min (% max HP) | ≤ 10% | 15–30% | 30–50% |
| Power split at L16, steered build (**gate:** offline ablation `Docs/Balance/power_split.md`; the live `RH.Stats` split is indicative only; keeper identity spec §4) | — | — | blessings ≥ 60 % · modifiers 15–25 % · levels + extra weapons the rest |

> Designer decision 2026-10-19 (R15, `Docs/Balance/r15_bullet_targets.md`): the old 150 / 800 / 2 000 bullet row is retired. "The screen is full of my power" is delivered by **impacts and kills under the art-book density LOD** (Art §3.5 / §12, PL.18: individual sparks and debris degrade with density into ground haze and an aggregated number — *remove the individuals, keep the thickness*; designer decision 2026-10-19: the art book wins over R15's lasting-wreckage plan, so the effects-row targets are re-set by measurement), and by the two **danmaku weapons** (Ring Emitter, Spiral Projector, §5.2), which are deliberately low-damage / high-count. Swarm TTK ≤ 0.3 s is measured on the **whole build**: a danmaku weapon on its own may miss it. At high density player bullets **dim and thin automatically** so warm enemy fire stays the brightest thing on screen (`05_ArtDirection.md §3`).

**Difficulty targets for v0.1:** a first-time player dies before 10:00 on roughly half of first attempts; a player who has learned to steer a Path wins ~30–40% of runs; the boss kills about a third of the players who reach it. If nobody dies, the threats are HP walls and the game is 无脑; if everyone dies in harvest phases, the swarm is leaking damage it shouldn't.

---

## 4. The Hero

One playable character: **the Reclaimer / 回收者** (the fiction's word; "hero" below). Identity comes from the build, not the character — which is also the fiction: it wakes with no memory each run.

### 4.1 Stats
| Stat | Base | Notes |
|---|---|---|
| Max HP | 100 | Regen 0 base (blessings add) |
| Armor | 0 | Flat reduction per hit |
| Move speed | 600 uu/s | |
| Pickup radius | 150 uu | XP gem magnet |
| Damage % | 100 | Global multiplier |
| Attack speed % | 100 | Fire-rate multiplier |
| Projectile speed % | 100 | |
| Area % | 100 | Bullet size / AoE radius |
| Crit chance / Crit dmg | 5% / 150% | Crit chance is hard-capped at **60 %** (designer-confirmed 2026-10-21, mechanics decision list #1) |
| Cooldown % | 100 | Dash + Ultimate |
| XP gain % | 100 | |
| Luck | 0 | Affects rarity rolls & chest quality |

### 4.2 Abilities
| Ability | Input (KBM / Pad) | Spec |
|---|---|---|
| Move | WASD / Left stick | 8-directional, no acceleration lag |
| **Dash** | Space / RB (R1) | 0.25 s, ~450 uu, **i-frames for full duration**, 3.0 s cooldown, 1 charge (Keeper of Ways adds more) |
| **Ultimate** | Q / LB (L1) | Patron Keeper's signature ability (**Intercession / 神临**), 60 s cooldown, charges visibly on HUD |
| Pause / Build screen | Esc / Start | Shows weapons, modifiers, blessings, favor bars |

### 4.3 Muscle Memory / 身忆 — the Reclaimer's own passive (designer decision 2026-10-19)
The Reclaimer grows a passive of its own across the **whole game**, independent of every Keeper: no Path tag, no Favor, never offered as a card, never bought with Ichor. It unlocks **automatically from cumulative progress**, as if the body remembers what the mind was wiped of (fiction: `04_Narrative.md §5`). It is **not** part of Contracts (§8.2, the deal with the Keepers). v0.1 ships 3 stages (accepted 2026-10-19; numbers tunable):

| Stage | Unlocks at | Effect |
|---|---|---|
| 1 *Flinch / 惊觉* | 3 runs finished (any result) | Once per run, a hit that would kill you leaves you at 1 HP with 1 s of i-frames |
| 2 *Reflex / 本能* | first win | Dashing through an active enemy telegraph (charge line, slam ring, boss ring) refunds 50 % of the dash cooldown |
| 3 *Form / 架势* | 3 wins, or 10 Discoveries | The first weapon you take in a run can reach level 9 |

Its visible twin is the Reclaimer's **wear** (`05_ArtDirection.md §5`): one wear stage per Muscle Memory stage. Rules: effects reward the Reclaimer's own verbs (survive, dash, its rig) and never duplicate a Keeper's element or status; shown on the build screen and in the Codex; one toast on unlock. EN stage names are accepted; zh-Hans names come from the loc session (context-first).

Weapons **auto-fire** and **auto-target** per weapon rule (nearest / densest cluster / random / none for radial patterns). The player never aims.

---

## 5. The Build System (core of the game)

### 5.1 Three content layers

**Slots:** 4 weapon slots · each weapon has 3 modifier sockets · unlimited blessings (each blessing max rank 3).

```
WEAPON  (tech, 10 in prototype)      — base pattern, levels 1→8 in-run
  └── MODIFIER ×3 (tech, 20)          — alters that weapon's bullets
BLESSING (god, 6 gods × 5 = 30)       — player-wide passive, +1 Favor to its god
FUSION  (weapon + god ≥6 favor, 6)    — evolved weapon chosen at L16
```

Combinatorics (prototype content): 10 weapons × C(20,3) modifier sets ≈ 11 400 distinct weapon configurations, before gods. The *full* game scales each list; the architecture does not change.

### 5.2 Weapons (10)
All weapons: Level 1–8 in-run. Each level = one of {+damage, +count, +fire rate, +size} per weapon's own upgrade track.

| # | Weapon | Pattern | Target rule | Identity |
|---|---|---|---|---|
| 1 | **Pulse Rifle** | Single fast bolt | Nearest | Baseline; starting weapon |
| 2 | **Scatter Cannon** | 5-pellet cone, short range | Nearest | Close-range shredder |
| 3 | **Orbital Drones** | 2 drones orbit player, each fires bolts | Nearest to drone | Passive coverage |
| 4 | **Rail Lance** | Slow, long piercing beam-bolt | Densest line | Line clear; pierce-native |
| 5 | **Flak Mortar** | Lobbed shell, AoE on impact | Densest cluster | AoE; slow |
| 6 | **Arc Blade** | Melee sweep 120° around player | Facing / nearest | No projectile; hits many |
| 7 | **Seeker Swarm** | 4 homing micro-missiles | Random alive | Fire-and-forget |
| 8 | **Ring Emitter** | 12 bullets radially every 1.2 s | None (radial) | Pure 弹幕 — **danmaku weapon**: low damage, high count, long-lived bullets; fills the screen, other weapons finish |
| 9 | **Spiral Projector** | Continuous rotating stream | None (spiral) | Pure 弹幕; screen control — **danmaku weapon** (as Ring Emitter) |
| 10 | **Gravity Mine** | Drops mine; pulls, then detonates | Densest cluster | Crowd control |

**Weapon load (Keeper identity redesign, scheduler direction on designer feedback 2026-10-20; `Docs/Design/keeper_identity_spec.md`, §4; factors GATED).** Owning more weapons no longer multiplies output linearly. The strongest weapon deals ×1.0, the others ×0.7, ×0.55 and ×0.45, shown as "Load" on the build screen. Weapon levels add pattern (count / size / rate) before damage.

**Danmaku weapons (designer decision 2026-10-19, R15).** Ring Emitter and Spiral Projector are the build's screen-fillers: per-bullet damage stays low and bullet count / lifetime go up, so one of them at L6+ puts 40–90+ bullets on screen. They are exempt from the per-weapon swarm-TTK check (§3.5 measures TTK on the whole build). Their tracers dim and thin with density (Art §3). The other eight weapons keep the one-shot role.

Starting loadout: Pulse Rifle L1. Prototype *starting pool* (before unlocks): weapons 1–4; the rest are Discoveries (see §7).

### 5.3 Modifiers (20)
Socketed into **one** weapon (offers are pre-bound: the card reads "Split Core → Scatter Cannon"). Max 3 per weapon. A modifier is taken once per weapon (no ranks) — stacking comes from combining modifiers.

| # | Modifier | Effect | Notes |
|---|---|---|---|
| 1 | Split Core | On hit, bullet splits into 2 (−40% dmg each) | Multiplies with Pierce |
| 2 | Ricochet Plate | Bounces to a new target up to 2× | |
| 3 | Pierce Rounds | Passes through +3 enemies | |
| 4 | Homing Gyro | Bullets steer toward targets | |
| 5 | Orbit Lock | Bullets orbit player 2 s, then release: each round, at its own release, is re-aimed once (no homing) at the boss if one is alive (Warden, Floodgate, Font, their targetable parts included), else at the nearest living enemy within the weapon's range × 1.5 (GATED), else it flies out along its orbit as before. Speed, damage, pierce, other modifiers and Boomerang's return are unchanged; range and target are the owning hero's (designer decision 2026-10-27) | Turns any weapon into a shield |
| 6 | Boomerang | Bullets return, hitting twice | |
| 7 | Fork | +2 bullets at ±15° | |
| 8 | Mirror | Duplicate bullet fires backward | |
| 9 | Magnify | +50% bullet size, −10% speed | |
| 10 | Overclock | +35% fire rate, −15% damage | |
| 11 | Heavy Slug | +60% damage, −30% fire rate, knockback | |
| 12 | Chain Trigger | On kill, weapon fires a free extra volley | Snowballs in crowds |
| 13 | Burst Capacitor | Fires in 3-round bursts | |
| 14 | Spread Wave | On expiry, bullet becomes a short wave | |
| 15 | Lingering Field | Impact leaves a 1 s damage zone | |
| 16 | Crit Lens | +15% crit chance, crits +50% dmg | |
| 17 | Velocity Coil | +50% projectile speed & range | |
| 18 | Scatter Shell | On expiry, explodes into 6 radial bullets | 弹幕 generator |
| 19 | Lifeline | Heals 3% of this weapon's damage dealt | |
| 20 | Divine Conduit | Every hit applies the **Patron's** element status (100%) | Bridges tech ↔ gods |

Starting pool: 1–8; the rest are Discoveries.

**Modifiers are specialisers (Keeper identity redesign, scheduler direction on designer feedback 2026-10-20; `Docs/Design/keeper_identity_spec.md`, §4.2).** Flat multipliers are flattened (e.g. Overclock +20 % / −10 %, Heavy Slug +35 %). Each modifier lists the archetypes it **amplifies** (`AmplifiesTags`); on an owned archetype its effect is ×1.75 [GATED].

**Capability rule (scheduler direction 2026-10-20, designer build-depth feedback).** Every weapon carries capability tags (`Cap.Projectile`, `Aimed`, `Radial`, `Lob`, `Beam`, `Melee`, `Orbit`, `Homing`, `Mine`, `Expiry`, `Area`). Every modifier carries required / blocked tags, and a modifier is **only offered for weapons where it makes sense**: no Homing / Orbit Lock / Boomerang on Orbital Drones; no flight modifiers on the Arc Blade. The full 23 × 10 matrix is in `Docs/Design/build_depth_spec.md` §3; every weapon keeps ≥ 10 compatible modifiers.

### 5.4 Gods (the Six Keepers), Elements, 流派 (6)

> **Identity redesign (Keeper identity redesign, scheduler direction on designer feedback 2026-10-20; `Docs/Design/keeper_identity_spec.md`; CORE, numbers GATED).** Each Keeper now has **one combat identity**:

| Keeper | Identity |
|---|---|
| Storm | **bullet-hell** |
| Forge | **melee** |
| Tide | **control** |
| Shade | **tracking** |
| Radiance | **burst** |
| Quicksilver | **mobility** |

> - **3–4 archetypes (流派) per Keeper**, 20 in total, chosen by which blessings you take. Each archetype's **key** blessing changes how *every* weapon behaves through data flags (split, echo, orbit-then-release, convert-to-melee, slam, pull / push, stillness, seek, hunters, charge, nova, dash volley, speed scaling, ricochet, chain).
> - **Patron traits** are new.
> - **Tiers, imbue and Intercessions** are re-aimed at the identity.
> - **Power target at L16:** blessings ≥ 60 % of the gain, modifiers 15–25 % (they become specialisers that amplify an archetype), weapon levels and extra weapons the rest.
>
> **Archetype balance (designer-confirmed 2026-10-21, mechanics decision list #1, spec §3.7):**
> - All 20 archetypes sit in one band (×2.0–2.4 at L16 on fit weapons, ±15 %).
> - **Fit is mechanical, never numerical:** each archetype names 3 fit weapons (every weapon fits 6), used only for the resonance mark and the offer weight. Off-fit builds are weaker only because the mechanic doesn't land.
> - Runaway guards: crit chance is capped at 60 %; Split goes at most 2 generations, via a capstone.
> - A bot showcase gate per archetype (500 kills/min at 18:00, swarm TTK ≤ 0.15 s).
>
Superseded by [keeper_identity_spec.md §3](Design/keeper_identity_spec.md#3-per-keeper-design).

Each blessing taken = **+1 Favor** for its god (rank-ups also +1). The **Patron starts with 2 Favor**. Favor thresholds **3 / 6 / 9** unlock the god's 流派 tiers. Favor bars are always visible on the HUD. Names: honorific / 尊称 · Founder designation; the **Path name is the code key** (`God.Storm` …). A one-line Keeper remark is shown as a toast at each tier (`04_Narrative.md §4.2`).

**Look (revised 2026-10-20, playtest 5: 「尾迹用司色，弹芯保持冷色」; Art §3.2, CLAUDE.md rule 11).** Every imbued and every fusion bullet keeps a **cool / near-white core**, and carries **its Keeper's colour on the tail and the glow**. That covers all six Paths, **Forge amber and Quicksilver green included**, in the locked A1 hues (Storm #7FB4FF, Forge #FF9F3A, Tide #6FE0E8, Shade #8E62FF, Radiance #FFE28C, Quicksilver #A8F25A). This replaces the earlier rule "Storm and Shade recolour the core; Forge amber never tints a bullet; Quicksilver is colourless". Shape keeps player fire apart from enemy fire: a player round has a bright core and a coloured tail, an enemy round has a dark core and a magenta rim. The Art §3.2 guard rails apply:
- amber stays at hue 32°, never red or magenta, never on the tracer head;
- tails follow the density dimming;
- the densest-frame contrast test runs warm tails against enemy rounds, and on a failure the tail's saturation and length are reduced.

Tide is still told apart from player cyan by frost as well as hue. Status and ricochet procs flash on the target. With two Keepers imbued, the higher-Favor Keeper's colour is the tail and the second Keeper's is a small tip accent (ties: the Patron, then Path order). **Cards** say it: each Keeper's blessing card ends with that Keeper's imbue line (`ST_Blessings Imbue.<Path>.Rules`). **Data:** `DA_Imbue_<Path>` (C++ fallback `DA_Imbue_<Path>_CodeFallback`); `RH.Imbue 0` switches it off for A/B measurement.

**Branches (scheduler direction 2026-10-20, designer build-depth feedback): each Keeper has 12 blessings in 3 branches of 4** (entry, 2 core, capstone). Examples: Storm = Arc · Thunder · Charge; Forge = Burn · Ember · Anvil. So the same patron can build differently. The starter pool is the 18 branch entries. The full list and offer rules are in `Docs/Design/build_depth_spec.md` §2 and the text in `build_depth_text.md`. **Duo reactions:** when two Keepers' effects meet on one enemy, a pair reaction fires (15 pairs, e.g. Storm + Tide = Conduction). Each committed pair also gets a **duo blessing** after L8 (`build_depth_spec.md` §4).

### 5.5 Fusions (6, one per god)
Available at the **L16 commitment** if the player owns the weapon *and* has ≥ 6 Favor with the god. Replaces the weapon (keeps its modifiers).

| Keeper (Path) | Weapon | Fusion (名) | Effect sketch |
|---|---|---|---|
| Storms (Storm) | Rail Lance | **Arc Line / 雷弧** | Lightning beam that chains off every enemy it passes |
| Forge (Forge) | Flak Mortar | **Crucible / 熔雨** | Meteor shells; craters burn for 4 s |
| Tides (Tide) | Ring Emitter | **Frostring / 霜环** | Rings expand as frozen waves; freeze on contact |
| Dead (Shade) | Seeker Swarm | **Collectors / 索命者** | 3 persistent hounds that hunt and execute Marked — they collect debts |
| Light (Radiance) | Pulse Rifle | **Clarity / 澄明** | Piercing crit lances; crits Blind |
| Ways (Quicksilver) | Orbital Drones | **Relay Blades / 驿刃** | Drones become fast blades; ricochet, scale with move speed |

Fusion names are fiction (FText); the data asset is keyed by Path + weapon (`DA_Fusion_Storm_RailLance`).

Superseded by [build_depth_spec.md §1](Design/build_depth_spec.md#1-l16-transformations--core-behaviour-gated-numbers).

L16 is a **hand of 3 transformations**, each changing *how* a weapon or a Path behaves, as a main source of 爽:
- fusions (above);
- **Path transformations**, which replace the stat Legendaries: Storm *Overload*, Forge *Kiln*, Tide *Glacier* / *Slack Water*, Shade *Collection*, Radiance *Prism*, Quicksilver *Vector*;
- **weapon evolutions**, one per weapon (e.g. Pulse Rifle → *Railstorm*, orbiting blades).

An eligible-but-locked fusion shows as a **sealed** 4th card. A second hand at L24 is GATED. Spec: `Docs/Design/build_depth_spec.md` §1. The full game adds cross-god fusions.

> *(Superseded 2026-10-20 by the L16 transformation hand; the "feel the missing fusion" intent lives on as the sealed card.)* Designer decision 2026-10-19: on a fresh save this single Legendary card at L16 is **intentional** — the player should feel the missing fusion and go unlock it in Contracts (§8.2). Not a defect; do not pad it to 3 cards.

### 5.6 Offers: "pick 1 of 5"
Each level-up presents **5 cards** drawn from the *run pool*:

- **New weapon** (if a slot is free) · **Weapon upgrade** (owned weapon +1 level) · **Modifier → weapon** (pre-bound pair; only for weapons with a free socket) · **Blessing** (new, or rank-up).
- **Rarity:** Common / Rare / Epic affects magnitude (e.g. a Rare weapon upgrade gives +2 levels). Luck shifts rarity.

**Steering (keeps randomness, makes builds reachable):**
1. **Tag weights.** Every card has gameplay tags (`God.Storm`, `Weapon.RailLance`, `Mod.Pierce`…). Weight = base × (1 + 0.30 × favor) for that god's blessings; owned weapons' upgrades × (1 + 0.25 × weapon level); modifiers that *synergise* with owned content (data-tagged) × 1.5.
2. **Guaranteed slot.** Card 1 is always from {patron/second-god blessings ∪ upgrades of owned weapons}. Cards 2–5 are weighted-random. No two identical cards.
3. **Commitments.** *L8:* choose a **second god** from 3 offered (weighted by current favor); blessings of the other 4 gods leave the pool, and the two gods' blessings get +50% weight. The **passed-over Keeper** with the higher favor among the two offered but not chosen speaks one line as a HUD toast (designer decision 2026-10-19; `04_Narrative.md §4.3`) — fiction only, no mechanical penalty. *L16:* choose a **Fusion** (or Legendary).
4. **Player tools (per run):** **Reroll ×2**, **Banish ×1** (remove a card from the run pool permanently), **Lock ×1** (keep a card for the next level-up). Meta tree adds more.
5. **Pool size grows with meta.** Early players see a small pool (4 weapons, 8 modifiers, 18 blessings) so builds are easy to assemble; Discoveries add depth as mastery grows.

**Extensions (scheduler direction 2026-10-20, designer build-depth feedback; nothing above is removed):**
- **E1** branch momentum weights.
- **E2** capstones only after 2 branch cards.
- **E3** one run-start **omen** ("terms of the day").
- **E4** rare **cross cards** from outside the committed pair (L10+, ≤ 1 per 3 levels, never card 1, they don't count toward the 2-of-5 rule).
- **E5** the pair's **duo blessing** after L8.
- **E6** a gentle run memory against repeating the last 3 runs' blessings.
- **E7** the L16 / L24 **transformation hands** (rule 3).

Spec: `Docs/Design/build_depth_spec.md` §2.3, §1.2.

**More extensions (Keeper identity redesign, scheduler direction on designer feedback 2026-10-20; `Docs/Design/keeper_identity_spec.md`, §6):**
- **O1** the guaranteed slot prefers a blessing before L8; after L8 at least 2 of 5 are blessings from the chosen Keepers, one of them from an owned archetype.
- **O2** archetype momentum (E1 renamed).
- **O3** amplifiers only after their archetype's key.
- **O4** modifier synergy weights.
- **O5** weapon fit weights.
- **O6** the starter pool is the 20 archetype keys.

Nothing is cut.

*Card presentation* (two card families, rich rarity reveal) is specified in §9 and `05_ArtDirection.md §12`; it changes no offer rule above.

**Reserve fallback (scheduler decision 2026-10-21; names by planning).** Late in fresh-save runs (L22+), every blessing of the pair can be at max rank and every weapon at max level.
- **Reserve cards** then fill **only** the slots no legal card can fill, at most **one of each kind per hand**, so a hand always reaches 5 distinct cards.
- They **never displace a legal card** and **can't be Refused**. Every rule above is unchanged.
- No Keeper, no Favor, no archetype.
- Minor hands and Breath caches (`Docs/Design/progression_v2_spec.md`) use the same fallback.

| Key (ST_UI `Card.Reserve.<Id>`) | Name | Rules |
|---|---|---|
| `Restore` | Restore | Heal {0}% of your max HP. (30 %) |
| `Hone` | Hone | +{0}% damage. Stacks. (4 %, GATED) |
| `Currency` | Ichor Cache | +{0} Ichor at the end of the run. (25) |
| `Tools` | Spare Terms | +1 Renegotiate. |
| `Mend` | Field Patch | +{0} Armor. Stacks. (1) |

**Anti-frustration rules:** never offer a modifier for a weapon with no socket; never offer a 5th weapon; after L8, a blessing from the two chosen gods appears in at least 2 of 5 cards; if the player has < 2 weapons by L5, force a weapon card.

---

## 6. Enemies, Elites, Boss

Enemies are **melee-first** (the player makes the 弹幕); one ranged type and the boss provide things to dodge. In fiction they are **the Cast-offs / 弃物**: each Keeper's abandoned machines (`04_Narrative.md §6`). The **role is the code key** (`Enemy.Swarm`, `BP_Enemy_Swarm`); the name is FText.

| # | Enemy (名) | Role key | Origin | HP / Speed | Behaviour |
|---|---|---|---|---|---|
| 1 | **Scrapwing / 碎翼** | `Swarm` | ATMOS drones | Low / Fast | Flocks straight at the player; contact damage |
| 2 | **Bulwark / 壁垒** | `Tank` | FOUNDRY frames | High / Slow | Frontal shield: −50% damage from the front → rewards pierce, orbit, flanking patterns |
| 3 | **Courser / 驿骑** | `Rusher` | RELAY transit units | Med / Burst | Telegraphed 1 s wind-up, then a straight charge; dash to avoid |
| 4 | **Flare Pylon / 耀柱** | `Turret` | LUMEN pylons | Med / Static | Spawns at range; fires aimed 3-bullet bursts every 2 s; priority target |
| 5 | **Breaker / 破冰机** | `Bruiser` | CISTERN pumps | Very high / Slow | Ground-slam with a telegraphed circle; knockback |
| 6 | **Tallyman / 算筹** | `Support` | LEDGER tally units | Low / Slow | Buffs nearby enemies (+30% speed), heals them; kills grant bonus XP |

**Elites:** any enemy type, 5× HP, 1–2 affixes from {Armored, Swift, Volatile (explodes on death), Splitter (spawns 3 Scrapwings), Vampiric, Shielded (bullet-absorbing bubble, breaks after N hits)}. Drops a **Chest** → a guaranteed Rare+ card choice (3 cards).

**Boss — the Warden / 典狱 (15:00)** — name and fiction are placeholders by decision; it is simply the largest old machine until the full game decides what the Breach was:
- Phase 1 (100–60%): charge attacks (telegraphed line), sweep.
- Phase 2 (60–30%): **radial bullet rings** every 4 s (classic danmaku — the player's own dash/pattern-reading test), summons Bulwarks.
- Phase 3 (< 30%): enraged — faster rings + spiral, arena shrinks (hazard ring).
- Kill = **WIN** screen with run summary and Ichor bonus.
- **Control on bosses** (designer-confirmed 2026-10-21, mechanics decision list #1): control effects (Freeze, Blind, pull / push holds) on control-immune targets convert to **Cracked / 脆化** (+20 % damage taken, 2 s, GATED) instead of being refused. Boss TTK across the 20 archetype showcase builds stays within ±20 %.

**The Warden evolves across the whole game** (designer decision 2026-10-19). What it guards — direction decided, never stated in v0.1 text — is **the Original's old body / the Yard's drain** (`04_Narrative.md §6`). Across the meta-progression (not within a run) it changes **mechanics, look and behaviour**, so the player feels they are uncovering something. The form is chosen by cumulative wins; Heat (§8.5) does not change it. v0.1 ships 3 forms (accepted 2026-10-19; numbers tunable):

| Form | Reached at | Mechanic | Look | Behaviour |
|---|---|---|---|---|
| I | 0 wins | as above (charge, rings, spiral, adds, shrink) | Breaker-class frame with the five other Cast-offs' parts bolted on | guards a fixed point at arena centre |
| II | 1 win | + it **answers your build**: phase 2 adds a counter to your top Path (e.g. Shielded adds against Storm chains, plating that ignores Burn for 4 s against Forge) | plates torn open; something pale and person-scaled visible inside | **retreats toward** the centre when hurt, as if protecting it |
| III | 3 wins | + phase 4 (< 10 %): the six Keeper conduit colours light up in it one by one, each firing a short pattern from that Path; the arena ring stops shrinking and **pulses in sync with your dash** | all six Keeper colours in its conduits | stops attacking for 2 s at phase 4 start, facing you |

The **first kill of Form III** unlocks one Layer 3 Codex entry (designer decision 2026-10-19; `04_Narrative.md §10`), the deepest text in v0.1. Every form keeps the telegraph rules (§9). Form III hints at Layer 3 (*the Six were one*) only, never Layer 4, and cannot be reached before the first win.

---

## 7. Pacing & Spawn Director

A data-driven **timeline** (per 22.5 s block) sets: enemy mix weights, spawn rate, HP & damage multipliers, max alive cap, and a **phase tag** (`Harvest` / `Threat` / `Breath`, see §3.5) so the rhythm is authored, not emergent. Every elite wave and event is followed by a 5–8 s `Breath` block (regular spawns paused).

| Time | Event |
|---|---|
| 0:00 | Scrapwings, cap 90 (D-71, was 60) |
| 1:07.5 | + Bulwarks |
| 2:15 | + Coursers, cap 180 (D-71, was 120) |
| **3:45** | **Elite wave (2)** |
| 4:30 | + Pylons; drop-ins start: every 12 s, 10 s at 7:30, 7 s at 11:15, 5 s at 13:30, x2 in Threat |
| 5:37.5 | Scrapwing ring |
| 6:45 | + Breakers, cap 300 (D-71, was 200) |
| **7:30** | **Elite wave (3)** + Tallymen |
| 9:22.5 | Courser stampede |
| **11:15** | **Elite wave (4, affixed)**; Threat cap 300; cap 400 from the 11:37.5 Harvest block (D-71 amendment) |
| 13:07.5 | Pylon nest |
| **15:00** | **Boss** |

**Designer feedback decision 2026-10-22 (playtest v0.2 #3, scheduler D-71 step 1).** The basic Swarm spawn rate is ×1.5 per block and alive caps are ×1.5, capped at 400 (TDD §14), in all three arenas: Breach, Outfall and Sump. Elites, events, drop-ins, boss, HP/damage scaling and XP per kill are unchanged. Step 2 below supersedes the original 20:00 run length. The "≈ 22 levels at 20:00" estimate predates D-71 and is re-measured in step 2. Amendment 2026-10-23 (scheduler): §3.5 threats are skill checks, so the ×1.5 Swarm rate and cap apply only to `Harvest` and `Breath` blocks; `Threat` blocks keep their pre-D-71 rate, mix and cap (Rusher + Turret ≥ 25 %).

**D-71 step 2 (scheduler decision 2026-10-23).** All three arenas use a 15:00 run: timeline clock positions scale x0.75 and per-minute HP/damage slopes scale x(1/0.75), preserving the same structure and end-of-run enemy strength. Durations, telegraphs and drop-in intervals stay unchanged under the fairness contract in section 3.5. This is not a simpler run. Any elite wave or event whose scaled start plus its Duration and Breath would pass the boss is pulled earlier to start at boss time - Duration - Breath (order kept, Duration and Breath unchanged), so every beat and its Breath end before the boss.

Scaling: enemy HP ×(1 + 0.16·minute), damage ×(1 + 0.0667·minute). XP per gem ×1.1 → ≈ 22 levels and ≈ 36 decisions at 15:00 (D-49, progression_v2_spec section 2.4). D-71, 2026-10-26 W4 re-measure: 6 bot runs, Heat 0, ×1.2 gave level 23/25/25/27/25/25 at 15:00 (avg 25.0, 34.2 decisions); proportional estimate at ×1.1 = 22.9. The XP curve and offer rules are unchanged.

---

## 8. Meta-Progression

### 8.1 Currency: **Ichor / 神血**
Earned per run: `10 × minutes survived + 0.1 × kills + 50 × elites + 500 × boss kill`. Kept on death. The run summary may present this as `reclaimed → the Six's share → retained` (retained = the formula; the others are display-only) — see `04_Narrative.md §7`.

### 8.2 Blessing Tree (one hero, blessed by six gods) — in fiction **Contracts / 契约**
A radial tree: a **universal trunk** (the Reclaimer's own terms) and **six god branches** (standing contracts with each Keeper).

- **Trunk (universal):** +Max HP (5 ranks), +XP gain, +Pickup radius, +1 Reroll, +1 Banish, +1 Lock, Starting weapon L2, +Luck, Armor.
- **Each god branch:** (1) start with +1 Favor when Patron · (2) Ultimate cooldown −20% · (3) **unlock locked blessing #4** · (4) **unlock locked blessing #5** · (5) **unlock that god's Fusion** · (6) a run-start option: "Begin the run with one of this god's blessings".

Branch nodes are the main **pool-unlock** path for blessings and fusions.

### 8.3 Discoveries (pool unlocks for weapons & modifiers)
Achievement-style conditions, checked at run end (and some mid-run with a toast). Each Discovery also unlocks one **Recollection / 忆痕** — a 1–2 sentence Codex entry tagged with a story layer (`04_Narrative.md §10`); the Codex sorts by layer, not by unlock order.

| Discovery | Unlocks |
|---|---|
| Reach L10 in a run | Flak Mortar |
| Kill 500 enemies with Scatter Cannon | Scatter Shell |
| Survive 10:00 | Arc Blade, Seeker Swarm |
| Kill an elite with ≥ 3 modifiers on one weapon | Chain Trigger |
| Freeze 100 enemies | Ring Emitter |
| Deal a 500+ crit | Crit Lens |
| Win a run | Spiral Projector, Gravity Mine, Divine Conduit |
| … | (one per locked item; full table in data) |

### 8.4 Hub screen — **Vigil / 守夜镇**
Patron selection (**the Shrine / 祠堂**: 6 portraits; locked Keepers greyed with hint), Blessing Tree (**Contracts / 契约**), Codex (**典册**: discovered items, Recollections, 流派 descriptions), Stats (**Ledger-stone / 账碑**), Settings. Everything gamepad-navigable. No NPCs in v0.1; Vigil is labels only.

### 8.5 Heat — the Six raise the quota (designer decision 2026-10-19)
Unlocked by the **first win**; level *n + 1* unlocks after a win at level *n*; chosen at the Shrine before a run. Fiction name **Quota / 加征** (zh-Hans authored 2026-10-19, `Docs/Localization/zh_naming.md` §7). Each level adds **one skill-check change** — never more HP, never shorter telegraphs (§3.5 "skill checks, not HP walls"). Ichor ×(1 + 0.15 · Heat). Levels are cumulative (accepted 2026-10-19; numbers tunable):

| Heat | Change |
|---|---|
| 1 | Flare Pylons fire the 4-bullet late fan from 4:30 instead of 9:00 |
| 2 | Every elite rolls one extra affix; the 3:45 wave is already affixed |
| 3 | Events overlap: each event (5:37.5 / 9:22.5 / 13:07.5) brings a lighter version of another event with it |
| 4 | Coursers chain: a second charge right after the first, its telegraph shown while the first runs |
| 5 | The Warden's enraged phase starts at 50 % instead of 30 % |

The §3.5 targets are for Heat 0; the death-by-minute histogram is recorded per Heat level.

### 8.6 Return after death (designer decision 2026-10-19)
When a run ends in death, the hub opens with one **cold, precise system line** before anything else, e.g. `Asset recovered. Repairs this cycle: 3.` (count = deaths since the last win). One line, ~1.5 s, skippable, no animation in v0.1; the full game replaces it with a short reforging animation. Text in `04_Narrative.md §12`.

---

## 9. UX & Readability Rules

- Player bullets: **cool colours** (cyan/white/gold per weapon), rendered as emissive tracers; enemy bullets: **magenta/red** tracers with a dark core. Never the same hue. (`05_ArtDirection.md §3`)
- Enemy telegraphs are **always** drawn on the ground before a dangerous action — in the realistic style, as red laser-projected markings from the machine's own targeting system.
- Enemies carry a warm status light (optic/reactor) so dark machines read against grey stone at dusk; elites carry a brighter one.
- Hit feedback: flash + hit-stop (30 ms on crit/kill) + damage numbers (pooled; crits bigger & gold).
- HUD: HP bar, XP bar + level, timer, 6 Favor bars (patron highlighted), 4 weapon icons with modifier pips, dash charges, ultimate ring, reroll/banish/lock counts.
- Level-up screen pauses the game. Card look (designer decision 2026-10-20, playtest 5, `05_ArtDirection.md §12`): **weapon / upgrade / modifier cards are high-tech** (metal, glass, holographic window); **blessing / Keeper / fusion / Legendary cards are stone tablets**. **Rarity reveals are rich** (pack-opening style, original visuals, skippable). Cards show tags as icons, 流派 progress ("Storms 5→6 — Tier 2!"), and the weapon the modifier binds to.
- Narrative text obeys `04_Narrative.md §2`: Keeper lines ≤ 12 words, no exposition, the word "god" never appears in-fiction.
- Localisation: all text via String Tables; **English and 简体中文** maintained from day one.

---

## 10. Prototype Content Checklist (v0.1 "Fun check")

| Category | Count | Done when |
|---|---|---|
| Weapons | 10 | each has a distinct pattern, 8 upgrade levels, unique tracer look (hue/trail/impact) |
| Modifiers | 20 | each visibly changes bullets; all 20 work on all 10 weapons |
| Gods | 6 | 5 blessings each, 3 thresholds each, 1 ultimate each |
| Fusions | 6 | each visibly stronger than the base weapon |
| Enemies | 6 + 6 affixes | distinct silhouettes & behaviour; elite chests |
| Boss | 1 | 3 phases, danmaku rings, win screen |
| Arena | 1 | 6000×6000 uu; realistic lighting/sky/post from Week 1; stone environment pass in Week 6 (may slip to v0.2 — `05_ArtDirection.md §9`) |
| Meta | tree (trunk + 6 branches), ~15 discoveries, save/load | |
| Meta additions (designer decision 2026-10-19) | Muscle Memory 3 stages (§4.3), Heat 1–5 (§8.5), Warden Forms I–III (§6), passed-over Keeper line at L8 (§5.6) | each reachable through normal play; Form II/III and Heat need wins |
| UI | hub, HUD, level-up, pause/build, run summary; KBM + pad; en + zh | |
| Narrative | all proper nouns, 18 Keeper tier lines, 15 Recollections, 7 Codex entries, hub labels (en + zh) | text only; story reaches `ROOT — [no data]` after the first win (`04_Narrative.md §12`) |

**Prototype success criteria:** a new player finishes a 15-min run, can *name* the 流派 they ended with, and immediately wants to try a different patron — **and** says the run felt 爽 without saying it felt tiring (ask both, separately; §3.5 metrics are within range on the recorded runs).

---

## 11. Out of scope for the prototype (full-game backlog)

**Starting loadouts** (designer decision 2026-10-19: replaces "multiple heroes" — there is only one Reclaimer; variety comes from run-start loadouts) · more Keepers (designer decision 2026-10-19: kept as a concept for **post-release DLC**, not in the full game; the six-facet fiction must be reconciled when it is designed, `04_Narrative.md §11`) · cross-god fusions · ~~cross-Keeper elemental reactions~~ (**decided yes, 2026-10-20**: 15 duo reactions, now core design, `build_depth_spec.md` §4) · **Keeper personalities as mechanics** (designer decision 2026-10-19: blessings reward each Keeper's want — Storms rewards never stopping, Tides rewards holding still, Ways punishes standing still … — to be designed in detail for the full game; must respect the "one decision verb" rule of §3.5) · multiple arenas (direction decided 2026-10-19: further arenas lie **downstream** of the Breach, `04_Narrative.md §11`) · Heat beyond level 5 · daily seeded runs · Steam achievements & cloud saves · controller rumble & accessibility options · final art & audio (direction: `05_ArtDirection.md §11`, restrained epic orchestral + Keeper sound signatures) · **co-op** (designer decision 2026-10-19: kept; every player is a Reclaimer — fiction in `04_Narrative.md §11`) · Vigil NPCs, story Acts II–III, Warden identity stated in text, Warden forms beyond III, Muscle Memory beyond stage 3 (`04_Narrative.md §11`).

---

## 12. Full-game design sketches (not v0.1)

Designs decided in direction but not scheduled. They must not change v0.1 behaviour, and they never remove anything from §5.6.

### 12.1 Keeper Bids — the Six compete for you (designer decision 2026-10-19: direction approved, "design carefully, balance well")
Fiction: Act II, *the Keepers' competition becomes legible in the offers* (`04_Narrative.md §11`). Mechanic: before the L8 commitment, a non-patron Keeper may **bid** for the Reclaimer with better terms that hide a cost. Draft rules [proposal]:

- **When.** Only after the player has reached story Layer 2 (meta gate). Only on level-ups L4–L7. At most **one Bid card per hand**, at most **two per run**. A Bid never takes the guaranteed slot (card 1) and never comes from the patron.
- **Who bids.** The non-patron Keeper with the second-highest favor, if it is within 2 favor of the highest non-patron Keeper — the race is close, so it bids.
- **The offer.** One of that Keeper's blessings at **one rarity step higher** (Common → Rare → Epic; ≈ +50 % magnitude), plus one visible **Term**.
- **Terms (the cost)** — one per bid, shown on the card, never an HP wall:
  - *Binding:* the blessing gives +2 Favor instead of +1, and that Keeper is guaranteed to appear in the L8 offer. If you pass it over at L8, it **calls the debt**: that blessing drops one rank, and you hear its passed-over line (Narrative §4.3).
  - *Lien:* the Keeper takes an extra 20 % of this run's Ichor (fiction: its share).
  - *Collateral:* lose one Renegotiate (Reroll) this run.
- **Declining is free.** Not picking the card costs nothing. A Bid can't be Held (Locked) and can't be Refused (Banished); it is gone after that hand.
- **Balance targets (bot + playtest telemetry).** Bid pick rate 35–60 % (above 70 % → terms too soft; below 20 % → too harsh); win rate of runs that took a Bid within ±5 % of runs that didn't; *Binding* L8 pass-over rate 15–35 % (players should sometimes break the deal). `RH.Debug.Offers` reports bids; `Tools/offer_audit.py` gets a bid column.
- **Why it fits.** It turns the deal theme into the pick-1-of-5 itself: power now, against freedom at L8 — the same trade the whole story is about.

### 12.2 Keeper fates in the build system (designer decision 2026-10-19)
Tides (sacrifice) and Dead (betrayal) break ranks in Act III (`04_Narrative.md §11`). Rule: **a Keeper's fate must show in the cards**, so the player never asks "why is a dead Keeper still blessing me?". Details are [proposal]:

- **Tides → returned to the Reclaimer.** CISTERN was a facet of the Original; when it stops, the facet comes home. From then on the Tide Path's cards carry **no Keeper signature** and use the Reclaimer's own card frame. They give **no Favor**, Tides never speaks again, and Tide can't be picked as patron or at L8. The Tide tiers unlock by **Recollections read** instead of Favor (count thresholds, tuned like 3 / 6 / 9), and Tidal Wave becomes the Reclaimer's own Intercession, available to any patron as a second, slower ultimate [proposal]. It is the first piece of the Original's own power coming back: Muscle Memory (§4.3) and the Refusal run grow from the same root.
- **Dead → "off-ledger" blessings.** LEDGER lives on and keeps offering, but its cards are **off the books**: they take no Ichor share (the run's Ichor isn't reduced by them), and they start to carry Recollections (taking a Shade blessing can unlock one). The other four Keepers **retaliate**: in a run where you hold any Shade blessing, their Bids (§12.1) get harsher Terms and their passed-over lines turn colder. Betrayal becomes a playable choice: freedom and truth from Shade, against the rest of the Six's goodwill.
- Both fates are **meta-state** (saved, per profile), reached only in Act III; nothing in v0.1 changes.

## Glossary (中英)
Fiction terms (Keep, Vigil, Keepers, Cast-offs, Contracts, Recollections …) are in `04_Narrative.md`. Design terms:
- **流派 / Path** — the build archetype a run converges on; named for the element, one per Keeper (Storm, Forge, Tide, Shade, Radiance, Quicksilver). Path names are the code keys.
- **Favor / 神眷** — per-god counter that unlocks Path tiers at 3/6/9. In fiction: a Keeper's **stake**.
- **Blessing / 祝福** — god passive card. In fiction: an **advance / 预支**.
- **Modifier / 模块** — tech card socketed into a weapon.
- **Fusion / 神铸** — evolved weapon; in fiction a Keeper **reforges** it.
- **Ultimate / 神临 (Intercession)** — the Patron's active ability.
- **Ichor / 神血** — meta currency.
- **Discovery / 发现** — pool-unlock achievement; unlocks a **Recollection / 忆痕** in the Codex.
- **Reroll / Banish / Lock** — in fiction **Renegotiate / 重议**, **Refuse / 拒约**, **Hold / 保留**.
