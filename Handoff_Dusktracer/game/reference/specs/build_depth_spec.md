# Build depth — spec (2026-10-20, top priority)

Source: designer feedback 2026-10-20, relayed by the scheduler. The scheduler set the direction under the designer's full-auto mandate.

| # | Problem | Answer here |
|---|---|---|
| 1 | The L16 Legendary should be a qualitative change to how the build works, one of the main sources of 爽 | §1 **Transformations** |
| 2 | Runs repeat; too few Keeper blessings, so every Path converges | §2 **Branches** (5 → 12 blessings per Keeper) + offer extensions |
| 3 | Why does a melee / orbiting weapon get "bullets fly to the nearest enemy"? | §3 **Capability tags** + the 23 × 10 matrix |
| 4 | Patron and second Keeper don't interact | §4 **Duo reactions** (15 pairs) + **duo blessings** |

Also: GDD §11's open question on cross-Keeper elemental reactions is **settled: yes** (§4).

**Conventions:**
- **CORE** = buildable now. **GATED** = numbers need playtest / bot data. Every number here is a starting value unless it is called a rule.
- Text is in `build_depth_text.md` (EN-first, keys, limits; the loc session writes zh).
- **GDD §5.6 is extended, never reduced.** All five offer rules and every anti-frustration rule stay.

**Global 爽 / readability rules for everything below** (GDD §3.5, Art §3):
- **(R1) Visible within 2 s.** Every transformation and reaction changes what the screen shows within 2 s of taking it or triggering it.
- **(R2) Player-side colours only.** Cool cores; Keeper-colour tails / glows (Art §3.2). Reactions mix the *two* Keepers' hues. **No warm red or magenta, ever**; Forge amber stays on the status and the tail.
- **(R3) Danger stays readable.** Nothing player-made may hide a red telegraph. Persistent player objects (pillars, fields, minions) render ≤ 60 % opacity over telegraphs, and telegraphs draw on top.
- **(R4) Density.** All new FX go through the density LOD (Art §12 PL.18). Reactions have a per-enemy cooldown (1 s) and a global cap (≤ 30 reaction FX/s; above it, aggregated numbers and one shared burst per cluster). The bullet cap (6 000) holds.
- **(R5) Power budget.** A transformation may change *how* a build kills, not multiply its output. Target: a steered build's DPS after its L16 pick rises **×1.3–1.8** (GATED, bot-measured). Swarm TTK ≤ 0.3 s holds before and after.
- **(R6) No new verbs.** Dash stays the only active verb (§3.5). "Dash becomes a weapon" changes what dash *does*, not how it is pressed.

---

## 1. L16 Transformations — CORE (behaviour), GATED (numbers)

### 1.1 What changes
- The L16 commitment becomes a **choice of 3 transformation cards**, not "one fusion or one Legendary".
- **Three kinds:**
  - **(a) Fusions:** the existing 6, Path × weapon (GDD §5.5).
  - **(b) Path transformations:** 6 new, one per Keeper, plus Tide's *Slack Water* from the Outfall (7). They **replace the stat Legendaries ("Final Advance")**.
  - **(c) Weapon evolutions:** 10 new, one per weapon.
  - **23 in total.**
- **Each one changes how a weapon or a Path behaves.**

### 1.2 The L16 hand (extends GDD §5.6 rule 3)
**Candidates:**
- Fusions where the player owns the weapon and has ≥ 6 Favor with its Keeper (as now).
- The **patron's and the second Keeper's** Path transformations (any Favor).
- **Weapon evolutions** for owned weapons at **level ≥ 6**.

**Deal 3:**
- If any fusion qualifies, one card is a fusion.
- At least one card is tied to the patron or the second Keeper.
- The rest are weighted by: owned weapon level, Favor, and **not picked in the last 3 runs** (×0.6).

**Sealed fusion:** if a fusion would qualify but is still locked in Contracts, the hand shows it as a **sealed, unpickable 4th card** ("Unlock in Contracts"). This keeps the 2026-10-19 intent (feel the missing fusion) without making L16 a non-choice ⟨D-31⟩.

**Second transformation at L24** (runs reach L23–L29): a second hand of 3, built the same way, excluding the first pick's kind for the same weapon ⟨D-32⟩. GATED on bot level data.

**Data:**
- `URHTransformDef` (primary type `RHTransform`): `Kind` {Fusion, Path, Weapon}, `Tag` `Transform.<Path>` / `Transform.<Weapon>`, an effect class, an optional weapon-def swap (fusions and evolutions), and VFX overrides.
- Fusions keep `URHFusionDef` and are wrapped by a `Transform` entry.

### 1.3 Path transformations (7)
They replace `Blessing.<Path>.Legendary` (Final Advance). Each applies to **all weapons**.

| Path | Name | Mechanic | Why it is 爽 | Readability |
|---|---|---|---|---|
| Storm | **Overload** | Shock **stacks** (max 3). At 3 stacks the enemy **discharges**: a chain bolt to 4 more enemies that applies 1 stack each. **Every kill on a Shocked enemy chains.** | Crowds pop in cascades | Blue-white chain bolts, cool; one discharge flash per enemy (R4 cooldown) |
| Forge | **Kiln** | Enemies that die Burning become **slag turrets** for 4 s: an allied, rooted husk that spits cinder bolts at the nearest enemy (20 % of your hit). Max 8 alive | Your kills fight for you; the field fills with your fire | Husks render dark with an amber Burn glow (status colour), ≤ 60 % opacity over telegraphs (R3) |
| Tide | **Glacier** | Frozen enemies become **ice pillars** for 3 s. Pillars block enemies **and enemy bullets**, then shatter (AoE + Chill) when hit or when they expire | You build walls from the crowd | Pale cyan, translucent; telegraphs draw on top; max 12 pillars |
| Tide (Outfall) | **Slack Water** | Chill never wears off on enemies standing in water; Frozen enemies in water shatter for double damage (already specced, `act2_text.md` §12.2) | Water becomes your weapon | Frost on the water's surface |
| Shade | **Collection** | Marked enemies that die rise as **Shades** that orbit you and fire your *first* weapon's base pattern at 25 % damage. Max 10; oldest replaced | An army that copies your gun | Violet translucent wisps, cool |
| Radiance | **Prism** | Every crit **splits** the bullet into 3 refracted light lances (short pierce, 40 % each) | Crits fan into light | Gold-white lances (Radiance tail), short-lived |
| Quicksilver | **Vector** | **Dash becomes a weapon:** each dash fires every weapon's volley along the dash line and leaves a blade trail (1 s, 60 % of Pulse Rifle damage per tick). +1 dash charge | Every dodge is an attack | Colourless afterimage blades (Quicksilver VFX are colourless) |

### 1.4 Weapon evolutions (10)
They swap the weapon def (like a fusion) and keep its modifiers when the capability tags allow (§3; incompatible modifiers are refunded as a Reroll).

| Weapon | Evolution | Mechanic | Capability change |
|---|---|---|---|
| Pulse Rifle | **Railstorm** | The bolt is replaced by **6 orbiting blades** that orbit you and launch at the nearest enemy every 0.4 s, then return | Projectile → +Orbit |
| Scatter Cannon | **Flechette** | Pellets **embed**, then detonate after 0.5 s (small AoE each); 8 pellets | + Area |
| Orbital Drones | **Blade Halo** | Drones become **4 orbiting blades** whose radius grows with move speed; contact damage, no bolts | Orbit only (Projectile removed) |
| Rail Lance | **Sweep Beam** | The lance becomes a **continuous 1.5 s beam that sweeps 120°** toward the densest side | Beam (continuous) |
| Flak Mortar | **Carpet** | Each shell becomes a **line of 5 impacts** walking 600 uu forward | Lob + Area |
| Arc Blade | **Cyclone** | The sweep becomes a **360° spin while you move**, 2 hits/s | Melee |
| Seeker Swarm | **Murmuration** | Missiles **persist** as a hunting cloud for 6 s, retargeting after each kill | Homing + persistent |
| Ring Emitter | **Pulse Wall** | Rings expand into a slow **persistent wall ring** (r 600, 3 s) that pushes enemies out | Radial + Area |
| Spiral Projector | **Vortex** | The spiral **curves inward**, forming a vortex at 500 uu that pulls enemies in | Radial + pull |
| Gravity Mine | **Singularity** | All live mines **merge** into one singularity that pulls everything within 900 uu, then collapses (big AoE) | Mine + Area |

Example from the brief, "bullets become orbiting blades": **Railstorm**.

### 1.5 Fusions (6, existing)
These are unchanged, and they now count as transformations. Their visual rule stays: the core stays cool, the tail takes the Keeper colour, and the shape changes once.

---

## 2. Variety: Keeper branches, 5 → 12 blessings — CORE (structure + cards), GATED (weights)

### 2.1 Structure
Each Keeper has **3 branches × 4 blessings**:
- 1 **entry**;
- 2 **core**;
- 1 **capstone** (offered only once the player owns 2 cards of that branch).

The 5 existing blessings are placed into branches; **7 are new per Keeper (42 total)**. Tag: `Branch.<Path>.<Branch>`. Tiers (T1–T3 at Favor 3 / 6 / 9) and the imbue stay as they are; branches decide *what kind* of Storm / Forge … build you are playing.

| Path | Branch A | Branch B | Branch C |
|---|---|---|---|
| Storm | **Arc** (chaining): Live Wire*, Ground Fault*, +2 | **Thunder** (bolts from above): Thunderclap*, +3 | **Charge** (crit / overcharge): Overcharge*, Static Focus*, +2 |
| Forge | **Burn**: Kindling*, Banked Heat*, +2 | **Ember** (explosions, dash fire): Ember Trail*, +3 | **Anvil** (armour, attack speed, sturdiness): Tempered Plating*, Bellows*, +2 |
| Tide | **Frost** (Freeze / shatter): Undertow*, Deep Cold*, +2 | **Wellspring** (sustain): Deep Reserve*, Wellspring*, +2 | **Current** (push, waves): Cold Front*, +3 |
| Shade | **Mark** (execute): Lien*, Foreclosure*, +2 | **Souls** (minions): Dividend*, +3 | **Ledger** (economy): Interest*, Long Arm*, +2 |
| Radiance | **Crit**: Glint*, +3 | **Blind** (light pulses): Flashpoint*, Afterimage*, Corona*, +1 | **Halo** (area, size): Halo*, +3 |
| Quicksilver | **Dash**: Light Step*, Tailwind*, +2 | **Momentum** (speed scales power): Fleet*, Wayfare*, +2 | **Ricochet** (bullets): Slipstream*, +3 |

\* = existing blessing. The new cards (names and rules) are in `build_depth_text.md` §2.

### 2.2 Pool and meta
**Starter pool:** the **entry card of every branch**, 6 × 3 = **18 blessings**. That is the same count GDD §5.6.5 promises, so new players are not swamped.

**Contracts** branch nodes (GDD §8.2) are re-pointed:
- Node (3) "unlock blessing #4" → **unlock the core cards of this Keeper's branches A + B**.
- Node (4) "unlock blessing #5" → **unlock branch C's core cards**.
- **Capstones** unlock through Discoveries: one per Keeper, "reach Tier 3 with 3 cards from one branch".

### 2.3 Offer extensions (GDD §5.6, added; nothing removed)
- **E1 Branch momentum.** A blessing's weight × (1 + **0.4** × cards owned in its branch). Once the player owns ≥ 2 cards in one branch of a Keeper, that Keeper's *other* branches get × **0.8**. A build leans into a branch without being locked. [GATED]
- **E2 Capstones** are offered only when 2 cards of their branch are owned, at Rare+.
- **E3 Run-start omens.** Every run rolls **1 omen** from 12, shown at the Shrine and on the run-start banner (fiction: the Six's posted terms for the day). It changes weights or rules for the run, usually pushing one random branch (×2) or one weapon. List in `build_depth_text.md` §5. A Discovery can unlock re-rolling the omen once (Contracts trunk).
- **E4 Cross cards.**
  - From L10, each level-up has a **10 %** chance [GATED] that one of cards 2–5 is a **cross card**: a Rare+ blessing from a Keeper **outside** the committed pair.
  - Taking it gives that Keeper Favor as normal and can switch on a reaction (§4).
  - At most 1 per 3 levels; never card 1; it does not count toward "2 of 5 from the chosen gods".
  - This extends the L8 rule "the other 4 leave the pool" with a rare exception.
- **E5 Duo cards.** After L8, the committed pair's **duo blessing** (§4.3) enters the pool at Rare+, weight × 1.5, once per run.
- **E6 Run memory.** Blessings taken in each of the last 3 runs **with the same patron** get × **0.8** [GATED].
- **E7 Weapon-start variety.** Loadouts (`full_game_specs.md` §3) already vary the start weapon. Omens may also seed a second weapon.

---

## 3. Modifier capability tags — CORE

### 3.1 Data
- **Weapons** carry `Cap.*` tags (`URHWeaponDef.Capabilities`): `Projectile` (free-flying rounds), `Aimed`, `Radial`, `Lob`, `Beam`, `Melee`, `Orbit`, `Homing`, `Mine`, `Expiry` (rounds end in flight / on landing), `Area`, `Continuous` (a stream, not discrete shots; added 2026-10-20 from the weapons session's implementation).
- **Modifiers** carry `RequiredCaps` (any-of) and `BlockedCaps` (none-of) (`URHModifierDef`).
- **The rules:**
  - The offer generator offers `Modifier → weapon` only when the weapon's caps pass.
  - Evolutions that change caps refund incompatible modifiers as a Reroll.
  - Test `RogueHero.Modifiers.Compatibility` asserts the matrix below.
- **Path bullet effects follow the same caps** (the imbue and Quicksilver T3 ricochet): on weapons without `Projectile`, Quicksilver's ricochet becomes **+10 % sweep / orbit speed**, and Storm's arc starts from the hit point as before.

### 3.2 Weapon caps

| Weapon | Caps |
|---|---|
| Pulse Rifle | Projectile, Aimed, Expiry |
| Scatter Cannon | Projectile, Aimed, Expiry |
| Orbital Drones | **Orbit**, Projectile *(short drone bolts)* |
| Rail Lance | Projectile, Aimed, Beam, Expiry |
| Flak Mortar | Lob, Aimed, Area, Expiry |
| Arc Blade | **Melee**, Area |
| Seeker Swarm | Projectile, Homing |
| Ring Emitter | Projectile, Radial, Expiry |
| Spiral Projector | Projectile, Radial, Expiry, **Continuous** |
| Gravity Mine | **Mine**, Area |

### 3.3 Matrix (✔ offered · — never offered)
Columns: Pulse · Scatter · Drones · Rail · Flak · Arc · Seeker · Ring · Spiral · Mine.

| # | Modifier | Rule | Pu | Sc | Dr | Ra | Fl | Ar | Se | Ri | Sp | Mi |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 | Split Core | Projectile; not Lob | ✔ | ✔ | ✔ | ✔ | — | — | ✔ | ✔ | ✔ | — |
| 2 | Ricochet Plate | Projectile | ✔ | ✔ | ✔ | ✔ | — | — | ✔ | ✔ | ✔ | — |
| 3 | Pierce Rounds | Projectile | ✔ | ✔ | ✔ | ✔ | — | — | ✔ | ✔ | ✔ | — |
| 4 | **Homing Gyro** | Projectile; **not Orbit, Homing, Beam** | ✔ | ✔ | — | — | — | — | — | ✔ | ✔ | — |
| 5 | Orbit Lock | Projectile; not Orbit, Homing, Beam | ✔ | ✔ | — | — | — | — | — | ✔ | ✔ | — |
| 6 | Boomerang | Projectile + Expiry; not Orbit, Homing | ✔ | ✔ | — | ✔ | — | — | — | ✔ | ✔ | — |
| 7 | Fork | Aimed, Orbit, Homing or Mine (more rounds / mines) | ✔ | ✔ | ✔ | ✔ | ✔ | — | ✔ | — | — | ✔ |
| 8 | Mirror | Aimed or Melee; not Homing | ✔ | ✔ | — | ✔ | ✔ | ✔ | — | — | — | — |
| 9 | Magnify | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 10 | Overclock | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 11 | Heavy Slug | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 12 | Chain Trigger | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 13 | Burst Capacitor | not Mine, not Continuous | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | — | — |
| 14 | Spread Wave | Expiry or Orbit; not Lob | ✔ | ✔ | ✔ | ✔ | — | — | — | ✔ | ✔ | — |
| 15 | Lingering Field | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 16 | Crit Lens | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 17 | Velocity Coil | Projectile or Lob | ✔ | ✔ | ✔ | ✔ | ✔ | — | ✔ | ✔ | ✔ | — |
| 18 | Scatter Shell | Expiry or Mine | ✔ | ✔ | — | ✔ | ✔ | — | — | ✔ | ✔ | ✔ |
| 19 | Lifeline | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 20 | Divine Conduit | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 21 | Backwash (v0.2) | Projectile or Lob | ✔ | ✔ | ✔ | ✔ | ✔ | — | ✔ | ✔ | ✔ | — |
| 22 | Spillover (v0.2) | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |
| 23 | Refit (v0.3) | any | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ | ✔ |

**Per weapon** (of the 20 v0.1 modifiers): Pulse 20 · Scatter 20 · Drones 15 · Rail 18 · Flak 13 · Arc 10 · Seeker 14 · Ring 18 · Spiral 17 · Mine 10.

Every weapon keeps ≥ 10, enough for 3 sockets with choice.

**Implementation (weapons session, d02e8e2):** the test asserts every cell and the per-weapon totals. The audit table is `Docs/Design/modifier_weapon_matrix.md`. Two rule wordings were corrected to produce this matrix:
- **Spread Wave** reads "Expiry or Orbit; not Lob", because drone bolts have no Expiry. Giving drones Expiry would also switch on Scatter Shell.
- **Burst Capacitor** reads "not Mine, not Continuous", with `Cap.Continuous` on the Spiral Projector. Blocking `Weapon.SpiralProjector` by its weapon tag gives the same result; the cap is the preferred long-term form.

**The designer's case:**
- **Homing on Orbital Drones is gone.** Drones keep the modifiers that act on their short bolts (Split, Ricochet, Pierce, Fork, Spread Wave …) but none that change free flight (Homing, Orbit Lock, Boomerang, Mirror).
- **Arc Blade (melee) gets no flight modifiers at all.**

Evolutions re-check the matrix with their new caps (§1.4). For example, Blade Halo drops Projectile, so its bolt modifiers are refunded.

---

## 4. Patron × second Keeper: duo reactions (15) and duo blessings — CORE (rules), GATED (numbers)

### 4.1 When a reaction fires
- **Requirement:** the player holds at least one blessing **or** the imbue from **both** Keepers of a pair. That is normally the patron + the second Keeper after L8; cross cards (§2.3 E4) can add a third.
- **Trigger:** a reaction fires when **both Keepers' effects meet on one enemy.** One Keeper's status (or crit / dash / ricochet for Radiance / Quicksilver) is on the enemy, and the other Keeper's effect hits it.
- **Limits** (R4): 1 s per-enemy cooldown per reaction; global FX cap; the damage numbers aggregate.
- **First trigger in a run:** a one-time toast with the reaction name (`build_depth_text.md` §4), shown in the HUD Keeper-line slot.

### 4.2 The 15 reactions

| Pair | Reaction | Meets when | Effect [GATED numbers] | Look (R2) |
|---|---|---|---|---|
| Storm + Forge | **Plasma** | an arc hits a Burning enemy | Plasma burst r 150 (60 % of the arc); Burn spreads to every arc target | Blue-white burst with an amber rim |
| Storm + Tide | **Conduction** | an arc hits a Chilled / Frozen enemy | The arc jumps to **every** Chilled enemy within 300 uu (cap 6), dealing full arc damage each | Cyan-white lines along the frost |
| Storm + Shade | **Writ** | a Shocked + Marked enemy dies | A bolt strikes the 2 nearest Marked enemies | Blue bolt, violet strike ring |
| Storm + Radiance | **Flash Arc** | a crit lands on a Shocked enemy | Arcs to 3 targets and Blinds them 0.3 s | Gold-white arcs |
| Storm + Quicksilver | **Live Rail** | a ricochet or a dash passes a Shocked enemy | The ricochet gains +1 bounce and arcs; a dash through it discharges Shock (AoE r 180) | Colourless streak + blue spark |
| Forge + Tide | **Steam** | Burn meets Chill / Freeze, either way round | Both statuses are consumed: a steam blast r 200 (120 % of the hit) + 0.4 s stagger | Pale steam cloud, cool white |
| Forge + Shade | **Cremation** | a Burning + Marked enemy dies | It leaves a soul-ember that seeks the nearest enemy and Burns it; execute threshold +5 % on Burning enemies | Violet ember with an amber core |
| Forge + Radiance | **Flare** | a crit lands on a Burning enemy | The remaining Burn detonates at once, ×1.5 | Gold flash, amber flicker |
| Forge + Quicksilver | **Wildfire** | a dash crosses burning ground, or you move > 110 % speed | Your bullets ignite (Burn) for 2 s; the ember trail spreads to anything crossing it | Colourless afterimages with cinders |
| Tide + Shade | **Deep Ledger** | a Frozen enemy is Marked, or a Marked one Freezes | The execute threshold is 25 % (not 15 %); executed Frozen enemies shatter into a Soul | Violet line through ice |
| Tide + Radiance | **Refraction** | a crit lands on a Frozen enemy | It refracts into 3 light shards (pierce, 40 %) | Gold-white shards from ice |
| Tide + Quicksilver | **Black Ice** | a dash through a Chilled enemy, or a ricochet off a Frozen one | The dash Freezes Chilled enemies on its path; a ricochet off a Frozen enemy doesn't use a bounce | Frost streak |
| Shade + Radiance | **Exposure** | a Marked enemy is Blinded | It takes +40 % damage; its death Marks the neighbours within 200 uu | Violet + gold outline |
| Shade + Quicksilver | **Repossession** | a dash through a Marked enemy under 30 % HP | Executes it; each execution refunds 20 % of the dash cooldown | Colourless slash, violet stamp |
| Radiance + Quicksilver | **Afterglow** | a ricochet crits, or you move fast | +1 % crit chance per 10 % bonus move speed; crit ricochets flash Blind 0.3 s | Gold glint on afterimages |

Example from the brief: Storm + Tide = **Conduction**.

**The six T3 tier effects still work alone.** Reactions sit on top and need no tier.

### 4.3 Duo blessings (15, one per pair; a second set is GATED for v0.3)
**Offer rule** (§2.3 E5): offered after L8 when the pair is committed, Rare+, once per run. It counts as a blessing of **both** Keepers (+1 Favor each). Each one **amplifies its pair's reaction**:

| Pair | Duo blessing | Effect |
|---|---|---|
| Storm + Forge | **Arc Furnace** | Plasma bursts leave burning ground for 2 s |
| Storm + Tide | **Storm Surge** | Conduction has no target cap; every 3rd conducted jump Freezes |
| Storm + Shade | **Summons** | Writ bolts Mark what they hit |
| Storm + Radiance | **Blinding Arc** | Flash Arc's Blind lasts 1 s and Blinded enemies take +20 % from arcs |
| Storm + Quicksilver | **Lightning Step** | Every dash leaves a Shocking trail; Live Rail discharges chain once |
| Forge + Tide | **Pressure Cooker** | Steam blasts also Chill and Burn anything they hit (chains Steam) |
| Forge + Shade | **Pyre Debt** | Cremation embers count as Marked kills (Souls, Shades) |
| Forge + Radiance | **Sunspot** | Flare detonations crit |
| Forge + Quicksilver | **Afterburner** | Wildfire triggers at any speed while dashing; +10 % move speed while Burning enemies are near |
| Tide + Shade | **Cold Storage** | Deep Ledger executions drop 2 Souls and Freeze neighbours |
| Tide + Radiance | **Diamond Dust** | Refraction shards Chill; shards from crits Blind |
| Tide + Quicksilver | **Glide** | You are not slowed by water or ice; Black Ice also works with ricochets off Chilled enemies |
| Shade + Radiance | **Spotlight** | Exposure applies to Blinded enemies whether Marked or not, at +20 % |
| Shade + Quicksilver | **Fast Collection** | Repossession works at 40 % HP and grants +1 dash charge for 3 s |
| Radiance + Quicksilver | **Lightspeed** | Afterglow's crit bonus doubles, and crits refund 2 % of the dash cooldown |

---

## 5. GDD and data changes (summary)
- **GDD §5.3:** the modifier compatibility rule (capability tags) + a pointer to §3.
- **GDD §5.4:** 12 blessings per Keeper in 3 branches + duo reactions + duo blessings (pointers).
- **GDD §5.5:** the L16 hand of 3 transformations + the sealed fusion + L24 (D-32); the stat Legendaries are replaced by Path transformations.
- **GDD §5.6:** extensions E1–E7 (none removed).
- **GDD §11:** cross-Keeper reactions are **decided: yes**.
- **New data:** `URHTransformDef`, `URHReactionDef` (pair tags, trigger, effect class, VFX), `URHOmenDef`, `Branch.*` and `Cap.*` tags, plus `RequiredCaps` / `BlockedCaps` on `URHModifierDef`.
- **New tests:**
  - the capability matrix;
  - branch weights;
  - capstone gating;
  - cross-card limits;
  - duo-card timing;
  - every reaction fires once per enemy per second;
  - the L16 hand composition (fusion present when it qualifies, a patron / second card always present, a sealed card when locked).

## 6. Order of work (all CORE; numbers GATED)

Owners (scheduler 2026-10-20):
- weapons: steps 1, 4;
- tiers: steps 2, 6;
- statuses: step 3;
- balance: step 5;
- hub: steps 7, 8;
- meta-ui: the Contracts re-point + the L16 hand / sealed card / omen UI;
- loc-zh: zh.

The key-range table is `build_depth_text.md` §7.

| Step | Work | Est. |
|---|---|---|
| 1 | Capability tags + matrix + offer filter + test (fixes complaint 3 at once) | 1.5 d |
| 2 | Transformation framework + the L16 hand of 3 + sealed fusion | 2 d |
| 3 | 7 Path transformations | 5 d |
| 4 | 10 weapon evolutions | 6 d |
| 5 | Branch tags + 42 new blessings (effects) + Contracts re-point | 6 d |
| 6 | Offer extensions E1–E6 + omens (12) | 3 d |
| 7 | Reaction framework + 15 reactions + their VFX | 5 d |
| 8 | 15 duo blessings | 3 d |
| 9 | Text (`build_depth_text.md`) + zh | 2 d + loc |
| 10 | Bot balance pass (R5 budget; branch divergence measured as the share of distinct branch sets across 20 runs, target ≥ 60 % unique) | GATED |

## 7. Decisions — all decided as recommended (scheduler decision 2026-10-20, full-auto; D-32 / D-35 / D-36 stay GATED on numbers)

| # | Decision | Decided |
|---|---|---|
| D-31 | The L16 hand shows a **sealed** fusion card when one is eligible but locked (this supersedes the 2026-10-19 "single Legendary is intentional", which the new feedback overrides) | Yes |
| D-32 | A second transformation hand at **L24** | Yes, GATED on bot level data |
| D-33 | Path transformations **replace** the stat Legendaries (Final Advance retired) | Yes |
| D-34 | The starter pool = the entry card of every branch (18); Contracts nodes (3) / (4) unlock branch cores; capstones by Discovery | Yes |
| D-35 | Cross cards from L10 at 10 %, ≤ 1 per 3 levels | Yes, GATED |
| D-36 | Run memory: blessings taken in each of the last 3 runs with the same patron get × 0.8 | Yes, GATED |
| D-37 | Reactions need a blessing **or the imbue** of both Keepers (so a cross card can switch one on) | Yes |
