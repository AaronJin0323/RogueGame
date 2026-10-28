# Keeper identity redesign — spec (draft 2026-10-20, top priority)

Source: designer feedback (2026-10-20), verbatim in the scheduler's brief:
1. Blessings don't drive power. Modifiers do, but a 流派's shape and its qualitative change should come from the Keepers.
2. 爽 should come from mechanics combining (blessing × weapon, blessing × blessing, blessing × modifier), not from picking a weapon or modifier that doubles DPS.
3. Each Keeper needs a speciality (tracking, mobility, melee, burst, bullet-hell, …), with 3–5 流派 per Keeper decided by the blessings taken. This may overturn the combat setup: blessings, traits and ultimates.

The scheduler set the constraints (full auto, 2026-10-20).

**Status:**
- **CORE** = the structure, flags, cards and mechanics, buildable now.
- **GATED** = every number, tuned on bot sims (balance session).
- Open points are **⟨D-n⟩**, listed in §10.
- This spec **supersedes `build_depth_spec.md` §2 (branches) wherever they conflict.** The *framework* is reused: branches become **archetypes**, and the `Branch.*` tags, momentum weights (E1) and capstone gate (E2) stay. Transformations (§1), the capability tags (§3), reactions (§4) and omens all stay.

**Unchanged:**
- GDD §5.6 in full: 5 cards, the guaranteed slot, the L8 commitment, the L16 transformation hand, reroll / banish / lock, anti-frustration.
- The fiction-key rules.
- The en / zh text limits.
- The GDD §3.5 targets (swarm TTK ≤ 0.3 s, the threat cadence).

---

## 1. The six identities

| Keeper (Path) | Identity | Why it fits the fiction (Narrative §4) | It changes weapons by … |
|---|---|---|---|
| **Storm** (ATMOS, anger) | **Bullet-hell** (满屏弹幕) | Anger is indiscriminate: the sky does not aim, it fills. "The sky does not wait for you." | splitting, echoing and ringing your shots until the screen is full |
| **Forge** (FOUNDRY, craft) | **Melee** (近战) | The hammer and the anvil; close tolerances; FOUNDRY works with its hands | turning shots into cleaving arcs, slams and close-range fire |
| **Tide** (CISTERN, grief, *the part that could stop*) | **Control** (控场), the identity added to the designer's list | Cold slows everything; it wants stillness; "fewer runs, longer intervals" | pulling, pushing, freezing, and rewarding standing still |
| **Shade** (LEDGER, memory) | **Tracking** (追踪) | Every debt is collected; nothing escapes the ledger | bending shots onto Marked targets, sending hunters, collecting |
| **Radiance** (LUMEN, clarity) | **Burst** (高爆发) | One exact answer to the exact question: a focused, overexposed strike | charging shots, crit-splitting, overkill novas |
| **Quicksilver** (RELAY, restlessness) | **Mobility** (灵活) | Standing still is how they count you | firing on the dash, scaling with speed, ricochets |

Mobility ≠ Control: Quicksilver rewards **moving**, Tide rewards **holding still**. The two Keepers' fictional opposition is now a playstyle opposition.

---

## 2. The behaviour layer — blessing → weapon flags (TDD §9, no per-weapon code) — CORE
Owner: **weapons** (aligned with the weapons session's generic hook layer; their names win where they already exist, and these are additions).

`FRHBehaviourFlags` lives on blessing / tier / trait / transformation defs and is composed into every owned weapon's spec. It is applied **after** weapon level and **after** modifiers, so modifiers can *amplify* a flag (§5).
- Flags from several sources **add their params** up to a per-flag cap.
- Every flag has a **capability fallback** (build_depth §3 caps), so it means something on every weapon.

| Flag | Params | Does | Applies to (caps) | Fallback on other caps |
|---|---|---|---|---|
| `Split` | chance, children, pattern {Fan, Ring, Spiral}, child dmg %, generations | On hit or expiry a round becomes N children | Projectile, Expiry | Melee / Orbit / Mine: every Nth activation also emits a small ring of children from the hero |
| `Echo` | delay, dmg %, source {Sky, Origin} | The volley repeats; Sky = as bolts dropping on the impact points | all | — |
| `ConvertOrbit` | orbit time, count | Rounds circle the hero, then release outward | Projectile, not Orbit | Orbit: +1 drone / blade |
| `ConvertMelee` | arc deg, radius, dmg ×, every N rounds | Rounds within the convert budget become a cleaving arc around the hero (no flight) | Projectile, Lob | Melee: +size; Mine / Orbit: +area |
| `Slam` | radius, dmg, trigger {Dash, Hit taken, Every N melee hits} | A ground slam ring | all | — |
| `Field` | radius, duration, dps %, status | The impact (or the slam) leaves a zone | all | — |
| `Pull` / `Push` | distance, radius | Impact pulls enemies toward it / pushes them away | all | — |
| `Stillness` | charge time, mult | Standing still charges the next volley (× mult, + status) | all | — |
| `Seek` | turn rate, target {Marked, Nearest, Elite} | Rounds bend toward the target class | Projectile, not Homing | Homing: + turn rate; Melee / Mine: + damage vs the target class |
| `Hunter` | count, life, dmg %, source {Marked kill, Every N kills} | Spawns allied hunters that fire the weapon's base pattern | all | — |
| `Charge` | every N, mult, pierce | Every Nth shot per weapon is a charged shot | all | Melee: every Nth sweep ×mult |
| `Nova` | radius, % of overkill | Overkill damage bursts as an AoE | all | — |
| `VolleyOnDash` | weapons {All, First}, line | A dash fires the weapons along the dash line | all | Melee: a sweep at landing |
| `SpeedScale` | dmg % per 10 % bonus speed, fire rate % per 10 % | Power scales with move speed | all | — |
| `Ricochet` | bounces, seek on bounce | As now (build_depth §3) | Projectile | Melee / Orbit: + sweep / orbit speed |
| `Chain` | jumps, range, dmg % | On hit, the damage jumps to N enemies (lightning) | all | — |
| `StatusApply` | status, chance | As the imbue does today | all | — |

- **Caps and density:** every flag respects the 6 000 bullet cap. `Split` / `Echo` / `Hunter` children count against it, and the oldest-first recycle applies. Art §3 R1–R4 from build_depth hold (cool cores, Keeper-colour tails, telegraphs on top, density LOD).
- **Readability:** `ConvertMelee` arcs and `Slam` rings are **cool / Keeper-tail coloured, never red** (red = enemy telegraphs).
- **Data:** a blessing def gains `Archetype` (`Branch.<Path>.<Archetype>` tag) and `Flags`. `URHEffect` subclasses remain only for event logic that flags can't express.

---

## 3. Per-Keeper design
Each Keeper keeps **12 blessings** (the 72 keys stay; most are reworked, §8).
- **Archetypes:** 3–4 per Keeper, each = 1 **key** card (grants the flag, *the* qualitative change) + amplifiers + 1 **capstone** (offered after 2 cards of the archetype, build_depth E2).
- **Patron trait** (new): active only when the Keeper is the **patron**; a small version of the identity from minute 0.
- **Tiers** T1–T3 (Favor 3 / 6 / 9) are re-aimed at the identity.
- **The imbue** is the identity in miniature.
- **The Intercession** is reworked to express the identity.

The text for every card is in `keeper_identity_text.md`. The numbers below are starting points, **GATED**.

### 3.1 Storm — Bullet-hell

| | |
|---|---|
| **Trait** *Static Sky* | Every 6th volley of each weapon fires twice |
| **Imbue** | 5 → 15 % of rounds Split into 2 (fan); arcs stay at the old imbue strength |
| **T1 / T2 / T3** | T1: +1 child on every Split. T2: Split children trigger on-fire effects as new shots. T3: every 5th volley also bursts as a 24-round ring (the old T3 thunderbolt becomes the ring) |
| **Intercession** *Thunderhead* (kept name) | 6 s: every round you fire Splits into 3 and each child arcs once; bolts strike across the screen (kept) |

| Archetype | Key (flag) | Amplifiers | Capstone | Best weapons | Modifiers that amplify it |
|---|---|---|---|---|---|
| **Tempest** (split fans) | *Forked Path*: `Split` 25 % → fan of 3, children 45 % | *Overcharge*: children +1 and +dmg | *Stormfront*: children split once more (2 generations) | Pulse, Scatter, Ring, Spiral | Split Core, Fork, Scatter Shell, Spread Wave |
| **Stormwall** (orbit, then burst) | *Static Focus*: `ConvertOrbit` 1.2 s, then release outward | *Low Ceiling*: +2 orbiting rounds per volley | *Grid Lock*: orbiting rounds arc to enemies crossing the ring | Pulse, Rail, Ring, Spiral | Orbit Lock, Magnify, Velocity Coil |
| **Arc Grid** (chain lightning) | *Live Wire*: `Chain` 2 jumps, 60 % | *Ground Fault*: Shock + arcs +dmg on Shocked | *Supercell*: an arc that kills forks into 2 new arcs | Drones, Scatter, Seeker | Ricochet Plate, Chain Trigger, Pierce |
| **Downpour** (sky echo) | *Thunderclap*: `Echo` from the Sky, 0.4 s, 50 % | *Rolling Thunder*: Echo bolts leave a 1 s Shock `Field` | *Capacitor*: every 8th Echo is a screen-wide bolt volley | Flak, Rail, Pulse | Burst Capacitor, Lingering Field, Heavy Slug |

### 3.2 Forge — Melee

| | |
|---|---|
| **Trait** *Close Work* | +25 % damage to enemies within 400 uu |
| **Imbue** | Hits within 350 uu also cleave a 90° arc for 30 %; Burn stays at the old imbue strength |
| **T1 / T2 / T3** | T1: Burn on 25 % of hits (kept). T2: every 4th melee hit (arc or slam) `Slam`s r 300. T3: Burning enemies explode on death, and explosions Burn (the old T2 + T3 merged) |
| **Intercession** *Forge Nova* (kept name) | 5 s: you are the hammer. Every 0.5 s a fire `Slam` ring r 600 around you knocks back and Burns |

| Archetype | Key (flag) | Amplifiers | Capstone | Best weapons | Modifiers that amplify it |
|---|---|---|---|---|---|
| **Hammer** (shots become arcs) | *Bellows*: `ConvertMelee`: the first 2 rounds of each volley become a 140° cleave r 380 at ×1.6 | *Tempered Plating*: +Armor, cleaves knock back · *Blast Furnace*: cleave radius +30 % | *Masterwork*: 4th socket; modifiers on converted weapons work at ×1.5 | Arc Blade, Scatter, Pulse, Drones | Heavy Slug, Magnify, Mirror, Lifeline, Chain Trigger |
| **Furnace** (fire zones) | *Kindling*: Burn + melee hits leave a burning `Field` r 120 | *Banked Heat* · *Slow Burn* (Burn stacks) | *Conflagration*: Burning enemies spread Burn each second | Arc, Mine, Flak | Lingering Field, Magnify |
| **Slam** (dash and retaliation) | *Ember Trail*: `Slam` on dash landing r 400, and the dash leaves fire | *Quench*: when hit, the next Slam ×2 · *Spark Shower*: Slams throw 6 embers | *Cinder Storm*: Slams detonate every Burning enemy in range | any | Heavy Slug, Lingering Field |

### 3.3 Tide — Control

| | |
|---|---|
| **Trait** *Cold Hands* | Hits slow 8 % (stacking to 24 %); standing still for 1 s gives +15 % damage until you move |
| **Imbue** | Chill (kept) + a small `Push` 30 uu on hit |
| **T1 / T2 / T3** | Kept: T1 Chill on 25 % of hits · T2 Chilled take +25 % · T3 Frozen shatter (AoE + Chill) |
| **Intercession** *Tidal Wave* (kept name and its role as the Reclaimer's second Intercession after Tides' fate) | A wave that **pulls** every enemy on screen into a line in front of you and Freezes them for 3 s |

| Archetype | Key (flag) | Amplifiers | Capstone | Best weapons | Modifiers that amplify it |
|---|---|---|---|---|---|
| **Whirlpool** (pull and cluster) | *Undertow*: `Pull` 120 uu toward the impact + Chill | *Deep Cold*: pulled enemies Chill twice as fast | *Shatterpoint*: when 5+ pulled enemies touch, they Freeze together and shatter | Ring, Spiral, Mine, Flak | Backwash, Lingering Field, Magnify |
| **Breakwater** (push and waves) | *Cold Front*: every 4th volley becomes a pushing wave (`Push` 200, line) | *Riptide*: every hit `Push`es 60 uu | *King Tide*: every 6 s a wave rolls out from you, pushing and Chilling | Rail, Scatter, Arc, Pulse | Spread Wave, Heavy Slug, Velocity Coil |
| **Frost** (stop them) | *Brittle*: Freeze at 2 Chill stacks (not 3); Frozen take +30 % | *Breakwater*: Frozen enemies block other enemies (walls) | *Deep Reserve*: Freeze lasts until the enemy is hit (max 8 held) | Drones, Seeker, Pulse | Crit Lens, Pierce |
| **Stillness** (hold your ground) | *High Water*: `Stillness`: standing still 1 s charges the next volley ×2 and Freezes what it hits | *Wellspring*: regen ×3 while still | *Spring Tide*: overheal becomes a shield; the shield bursts as a Freeze ring when it breaks | Spiral, Ring, Rail, Flak | Burst Capacitor, Magnify, Lifeline |

### 3.4 Shade — Tracking

| | |
|---|---|
| **Trait** *Bookkeeping* | Your first weapon's rounds `Seek` Marked enemies (10°/s), and its hits Mark 20 % |
| **Imbue** | Mark (kept) + light `Seek` toward Marked (5 → 15°/s) |
| **T1 / T2 / T3** | Kept: T1 Souls · T2 Shades rise (10 %) · T3 Marked below 15 % are executed; Shades explode |
| **Intercession** *Closing* (kept name) | 6 s: every enemy on screen is Marked and every round `Seek`s hard (90°/s); at the end, Marked enemies below 30 % are executed |

| Archetype | Key (flag) | Amplifiers | Capstone | Best weapons | Modifiers that amplify it |
|---|---|---|---|---|---|
| **Hound** (every shot hunts) | *Lien*: `Seek` 60°/s toward Marked (Nearest if none) + Mark on hit | *Foreclosure*: +turn rate, Marks last longer · *Garnish*: Marked take +dmg | *Final Notice*: execute threshold +10 % for Seeking hits | Pulse, Rail, Scatter, Ring | Homing Gyro, Pierce, Velocity Coil, Crit Lens |
| **Collectors** (hunters; key `Branch.Shade.Hunters`, display name "Collectors") | *Dividend*: `Hunter`: Marked kills raise a Shade hunter (6 s, fires your first weapon's base pattern at 25 %) | *Escrow*: hunters heal you on kill · *Debt Collectors*: +life, +dmg | *Arrears*: every 20 Souls raise a permanent hunter (max 3) | Seeker, Drones, Pulse | Chain Trigger, Lifeline, Split Core |
| **Ledger** (debt, then payday; key `Branch.Shade.Economy`, display name "Ledger" / 账簿) | *Interest*: Marked kills add 1 Debt; at 20 Debt the next volley `Seek`s every enemy on screen at ×2 | *Long Arm*: gems seek you; Debt +1 per elite · *Compound*: payday volleys pierce | *Audit*: payday also executes Marked below 25 % | Spiral, Ring, Seeker | Burst Capacitor, Fork, Scatter Shell |

### 3.5 Radiance — Burst

| | |
|---|---|
| **Trait** *Clarity* | Every 10th shot of each weapon is `Charge`d ×2 |
| **Imbue** | Kept (crit chance, crit flash) |
| **T1 / T2 / T3** | Kept: T1 +10 % crit · T2 crits pulse · T3 crits Blind, crit dmg +50 % |
| **Intercession** *Sun Lance* (kept) | A 0.5 s charge (the lamp turns white), then a 2 s overexposed beam that **always crits** and `Nova`s on overkill |

| Archetype | Key (flag) | Amplifiers | Capstone | Best weapons | Modifiers that amplify it |
|---|---|---|---|---|---|
| **Lance** (charged shots) | *Perfect Sight*: `Charge` every 5th shot ×3, pierce all | *Keen Edge*: +crit · *Focal Point*: crits stack on the same target | *Glint*: charged shots always crit | Rail, Pulse, Flak, Seeker | Heavy Slug, Velocity Coil, Pierce, Crit Lens |
| **Prism** (crits split into light) | *Flashpoint*: crits `Split` into 3 lances (40 %) | *Afterimage*: lances Blind · *Corona*: every 5 crits a pulse | *Whiteout*: Blinded enemies Blind their neighbours on death | Scatter, Pulse, Drones, Ring | Crit Lens, Split Core, Fork |
| **Solar** (overkill novas) | *Halo*: `Nova`: overkill bursts r 200 at 100 % | *Wide Aperture*: +size · *Radiant Wake*: rounds leave a light trail | *Halo Ring*: a ring of light around you; enemies dying in it `Nova` at ×2 | Flak, Rail, Arc, Mine | Magnify, Heavy Slug, Lingering Field |

### 3.6 Quicksilver — Mobility

| | |
|---|---|
| **Trait** *Restless* | +10 % move and attack speed while moving; −10 % after 1 s still |
| **Imbue** | Kept (bullet speed, ricochet) |
| **T1 / T2 / T3** | T1: +10 % move / attack speed (kept). T2: +1 dash charge, dash trail (kept). T3: `SpeedScale` +3 % damage per 10 % bonus speed (replaces the old T3; ricochet moves to the Rebound archetype) |
| **Intercession** *Time Dilation* (kept) | 4 s slow-mo (kept) + every dash during it fires a `VolleyOnDash` |

| Archetype | Key (flag) | Amplifiers | Capstone | Best weapons | Modifiers that amplify it |
|---|---|---|---|---|---|
| **Vector** (fire on the dash) | *Blink Strike*: `VolleyOnDash`: all weapons fire along the dash line | *Light Step*: dash cooldown −X % · *Tailwind*: after a dash, fire rate ×1.5 for 1 s | *Second Wind*: dash-volley kills refund 10 % of the dash cooldown each | Scatter, Rail, Pulse, Arc | Fork, Mirror, Overclock |
| **Momentum** (speed is power) | *Fleet*: `SpeedScale` +4 % damage per 10 % bonus speed | *Drafting*: also fire rate · *Wayfare*: pressure waves on distance | *Terminal Velocity*: above 130 % speed, rounds pierce +1 and `Echo` once | any | Velocity Coil, Overclock |
| **Rebound** (ricochet) | *Slipstream*: `Ricochet` +1, bounces `Seek` | *Bank Shot*: +1 bounce · *Carom*: +dmg per bounce | *Relay Chain*: a ricochet kill fires a free round at the next enemy | Pulse, Scatter, Drones, Seeker | Ricochet Plate, Pierce, Split Core |

**Keys:** archetype tags are `Branch.<Path>.<Archetype id>` with the ids of `keeper_identity_text.md` §1. Shade's second archetype is **`Shade.Hunters`** (display "Collectors"; "Collectors" is a fiction name). Shade's third archetype is **`Shade.Economy`** (LEDGER is a designation; fiction never keys, CLAUDE.md 6); its display name is "Ledger".

**Count:** Storm 4 + Forge 3 + Tide 4 + Shade 3 + Radiance 3 + Quicksilver 3 = **20 archetypes**, 72 blessings.

---

### 3.7 Archetype balance pass (designer-confirmed 2026-10-21, mechanics decision list #1; source: shared notes `mechanics_archetype_balance.md`)
Designer brief, verbatim: 「每一种流派都应该有相似的强度，在各种机制配合下都应该有无敌爽的最终构筑。但是如果选择的武器和流派之间没有配合，那么强度当然应该偏弱。」 Every archetype should be about equally strong, with an unbeatable, 爽 end build when its mechanics combine. If the chosen weapons don't fit the archetype, it is naturally weaker.

This section **supersedes the per-archetype "Best weapons" columns of §3.1–§3.6 and §5.4**. Numbers are GATED.

**Global rules:**
- **D-57 Equal bands.** Each archetype package (key + 2 amplifiers + capstone) on its **fit** weapons at L16, steered, gives crowd DPS **×2.0–2.4** over the same build without it. All 20 sit in the same band **±15 %**.
- **D-58 Fit is mechanical, never numerical.** The designer, 2026-10-21: 「非契合武器不应该有数值上的惩罚，是机制上的不契合导致打不过敌人，而不是数值。」 An off-fit weapon gets no numerical penalty; it loses because the mechanic doesn't fit, not because of a number.
  - Each archetype names **3 fit weapons** (`FitWeapons`; today `RHOfferArchetypes::GetFitWeapons`). The list is used **only** for the UI resonance mark (cards, build screen) and the O5 offer weight.
  - **No multiplier anywhere depends on fit.** Flags and capability fallbacks run at full strength on every weapon. The gap must come from the mechanic itself: an Echo lands on a spot whose enemy is already dead, Pull without AoE has nothing to cluster for, Seek on a weapon that already hits gains nothing.
  - If a bot sim shows an archetype equally strong off-fit, **the mechanic is redesigned**; no penalty is added.
- **D-59 Coverage.** 20 × 3 = 60 fit slots; **every weapon fits exactly 6 archetypes** (the table below).
- **D-60 Boss answer.** Control effects on **control-immune** targets (bosses; the Freeze / Blind ward) are **not refused**. They convert to a **vulnerability status** (+20 % damage taken, 2 s, GATED), zh **脆化**. **Boss TTK across the 20 showcase builds stays within ±20 %.**
  - The mechanics notes call the status "Brittle", but that is already the Tide Frost key card's name. **Its EN display name is decided as "Cracked"** (key `Status.Cracked`; keyword `Cracked`, `keyword_notes_spec.md`).
- **D-61 Runaway guards:**
  - a hard crit-chance cap of **60 %**;
  - Split recursion beyond 1 generation **only from a capstone or a timed Intercession grant** (Thunderhead · Tempest, intercession_variants_spec §2), **max 2 generations**; every other source is forced to 1 (scheduler clarification 2026-10-21, on weapons' R6 question);
  - the per-weapon spawn cap (400/s) and the 6 000 global cap hold;
  - the modifier ×1.75 synergy (D-43) applies to the modifier's *effect* and **never** to a crit-chance modifier or to a damage multiplier above +35 %.
- **D-62 Showcase gate (bot):** each archetype's showcase build (table below) must reach the 无敌爽 bar:
  - kills/min **≥ 500 at 18:00**;
  - whole-screen swarm TTK **≤ 0.15 s**;
  - boss TTK in band.

  The same archetype on 3 **off-fit** weapons must fall to the *scattered* band (TTK > 0.6 s by 12:00) **through mechanics alone** (D-58).

**D-63 Per-archetype changes:**

| Archetype | Estimated before | Change | Boss answer |
|---|---|---|---|
| Storm **Tempest** | A | Keep. Children from a hit on an elite / boss **re-target that target** (the fan converges) | converging fans |
| Storm **Stormwall** | B | Released rounds **+30 % damage, +1 pierce** (pays for the 1.2 s delay); Low Ceiling +2 orbiting rounds | the pierce release at the boss |
| Storm **Arc Grid** | A | Keep. With no other target in range, the arc **returns to the same target at 50 %** (ground return) | ground return |
| Storm **Downpour** | A (too universal) | Echo becomes a **delayed strike on the impact point** (0.4 s), not a repeat of the volley. On AoE / lingering / mine weapons it doubles the blast area-time; on a single-target round whose enemy already died it lands on empty ground. Same number on every weapon | an echo on the impact point (the boss stays put) |
| Forge **Hammer** | A / S | Masterwork keeps the 4th socket; its **×1.5 modifier bonus is removed**. Instead, **cleaves apply every socketed modifier's on-hit effect** | the ×1.6 cleave is strong single-target |
| Forge **Furnace** | B | Burn ticks **25 % faster inside fields**; Conflagration also spreads **on kill** | Burn stacks on the boss |
| Forge **Slam** | B− | Trigger no longer dash-only: **dash landing + every 6th melee hit (cleave / blade / drone contact) + every hit taken** | slams hit the boss in melee |
| Tide **Whirlpool** | A (combo) / B alone | Pulled enemies that **collide take 15 % crush damage** (its own damage source when played alone) | Cracked |
| Tide **Breakwater** | C+ | **Herding**: pushed enemies bunch into a wave front instead of scattering. Collisions (enemy, wall, Frozen enemy, boss) deal **crush damage at 30 % of the hit**. Cold Front fires **every 3rd volley** (was 4th) | crush into the boss + Cracked |
| Tide **Frost** | B+ | Keep; Frozen +30 % kept | Cracked (it had none before) |
| Tide **Stillness** | B− | "Still" = moved **< 50 uu in 0.6 s** (micro-steps allowed). Charge time 0.6 s. **Dashing keeps the charge; walking spends it.** The charge applies to the next volley of **every** weapon | a charged volley ×2.5 |
| Shade **Hound** | B+ | Seeking rounds **+1 pierce** (on the key) | seek always hits |
| Shade **Collectors** (key `Branch.Shade.Hunters`) | C+ | Hunters fire your **highest-level** weapon's pattern at **40 %** (was the first weapon at 25 %), **inherit its modifiers**, and every **elite kill raises one**. Arrears' permanent hunters fire at **60 %** | hunters focus elites / the boss |
| Shade **Ledger** (key `Shade.Economy`) | B | Debt **20 → 15**; payday volley **×2.5** | payday ×2.5 on the boss |
| Radiance **Lance** | A | Keep | already a boss killer |
| Radiance **Prism** | S | Lances **40 % → 30 %**; lances never crit-split again (1 generation); crit cap (D-61) | lances on crits |
| Radiance **Solar** | A | Capstone adds a **Sun charge**: 20 % of the damage dealt to elites / the boss is stored and released as a nova on them every 3 s | Sun charge |
| Quicksilver **Vector** | B− | The kill refund moves to the key (**0.15 s per dash-volley kill**, ≤ 50 % of the cooldown); dash volley **×1.5** damage; capstone: **every 3rd dash costs no charge** | volley ×1.5 |
| Quicksilver **Momentum** | C+ | **Rework to Momentum stacks.** +1 per 100 uu moved (max 30), dash +5, decays while still, a hit taken −10. Each stack gives **+2 % damage and fire rate** (max +60 %). Capstone at 30 stacks: +1 pierce and Echo once | the stacks apply to every target |
| Quicksilver **Rebound** | B+ | Keep; Carom's damage per bounce slightly higher. With no other target, a bounce **returns to the same target** (as Arc Grid) | bounce-back |

**D-59 Fit table** (every weapon exactly 6):

| Archetype | Fit weapons |
|---|---|
| Tempest | Scatter, Ring, Spiral |
| Stormwall | Rail, Seeker, Pulse |
| Arc Grid | Drones, Seeker, Arc Blade |
| Downpour | Flak, Mine, Rail |
| Hammer | Scatter, Arc Blade, Pulse |
| Furnace | Mine, Flak, Spiral |
| Slam | Arc Blade, Drones, Ring |
| Whirlpool | Flak, Mine, Ring |
| Breakwater | Rail, Spiral, Scatter |
| Frost | Drones, Seeker, Pulse |
| Stillness | Rail, Flak, Mine |
| Hound | Ring, Spiral, Scatter |
| Collectors | Seeker, Drones, Mine |
| Ledger | Spiral, Ring, Seeker |
| Lance | Rail, Pulse, Flak |
| Prism | Scatter, Drones, Arc Blade |
| Solar | Flak, Rail, Mine |
| Vector | Scatter, Arc Blade, Pulse |
| Momentum | Arc Blade, Drones, Spiral |
| Rebound | Pulse, Seeker, Ring |

**D-62 Showcase builds** (the 无敌爽 endpoint of each archetype; the bot gate builds):

| Archetype | Weapons | Modifiers | L16 pick | 2nd Keeper | On screen |
|---|---|---|---|---|---|
| Tempest | Spiral, Scatter, Ring | Split Core, Fork, Scatter Shell | Spiral → Vortex | Shade (Hound) | a vortex of homing fans |
| Stormwall | Rail, Seeker, Pulse | Orbit Lock, Velocity Coil, Magnify | Arc Line | Radiance (Lance) | a ring that fires out as charged lances |
| Arc Grid | Drones, Seeker, Arc Blade | Ricochet Plate, Chain Trigger, Pierce | Overload | Tide | chain cascades across the field |
| Downpour | Flak, Mine, Rail | Burst Capacitor, Lingering Field, Heavy Slug | Flak → Carpet | Forge | carpet lines echoing from the sky |
| Hammer | Scatter, Arc Blade, Pulse | Heavy Slug, Magnify, Lifeline | Arc Blade → Cyclone | Quicksilver (Vector) | a dash-cleaving whirlwind |
| Furnace | Mine, Flak, Spiral | Lingering Field, Magnify | Crucible | Tide (Whirlpool) | clustered crowds in fire lakes |
| Slam | Arc Blade, Drones, Ring | Heavy Slug, Lingering Field | Kiln | Quicksilver | slam rings + cleaving slag turrets |
| Whirlpool | Flak, Mine, Ring | Backwash, Lingering Field, Magnify | Mine → Singularity | Radiance (Solar) | the screen collapses into novas |
| Breakwater | Rail, Spiral, Scatter | Spread Wave, Heavy Slug, Velocity Coil | Glacier | Storm | waves crush crowds into ice walls |
| Frost | Drones, Seeker, Pulse | Crit Lens, Pierce | Glacier | Radiance | a frozen field shattering on crits |
| Stillness | Rail, Flak, Mine | Burst Capacitor, Magnify, Lifeline | Slack Water / Glacier | Radiance (Lance) | stand still → one screen-clearing freeze volley |
| Hound | Ring, Spiral, Scatter | Pierce, Velocity Coil, Crit Lens | Ring → Pulse Wall | Storm (Tempest) | every radial round hunts |
| Collectors | Seeker, Drones, Mine | Chain Trigger, Lifeline, Split Core | Collectors fusion / Collection | Forge | an army copying your guns |
| Ledger | Spiral, Ring, Seeker | Burst Capacitor, Fork, Scatter Shell | Collection | Radiance | periodic screen-wide homing paydays |
| Lance | Rail, Pulse, Flak | Heavy Slug, Velocity Coil, Pierce | Clarity / Zenith | Shade (Hound) | homing charged lances |
| Prism | Scatter, Drones, Arc Blade | Crit Lens, Split Core, Fork | Zenith | Storm | light fans that chain |
| Solar | Flak, Rail, Mine | Magnify, Heavy Slug, Lingering Field | Zenith / Singularity | Tide (Whirlpool) | nova chains through clusters |
| Vector | Scatter, Arc Blade, Pulse | Fork, Mirror, Overclock | Slipstream Drive | Forge (Slam) | every dash is a barrage |
| Momentum | Arc Blade, Drones, Spiral | Velocity Coil, Overclock | Drones → Blade Halo | Storm | a blade storm that grows with speed |
| Rebound | Pulse, Seeker, Ring | Ricochet Plate, Pierce, Split Core | Pulse → Railstorm | Shade | bouncing rounds that never stop |

## 4. Power budget — CORE (levers), GATED (numbers)
**Target at L16, steered build** (scheduler): the gain over the L1 starting kit comes ≥ **60 %** from blessings (cards, tiers, imbue, trait), **15–25 %** from modifiers, and the rest from weapon levels and extra weapons. Baseline: today's split, as measured by balance (§11).

**Levers:**
1. **Archetype keys carry the power.** Each key is a qualitative change worth roughly **+25–40 %** effective DPS in its fitting build. Amplifiers add +8–15 % each; capstones +20–30 %.
2. **Modifiers become specialisers.**
   - Their flat multipliers are flattened. Starting points [GATED]:
     - Overclock: +35 % rate / −15 % dmg → **+20 % / −10 %**.
     - Heavy Slug: +60 % → **+35 %**.
     - Split Core children: −40 % → **−55 %**.
     - Fork side rounds: −35 % each.
     - Crit Lens: 15 % → **10 %**.
   - In exchange, each modifier carries **synergy tags** (the per-archetype "amplify it" column in §3). When the owned blessings include that archetype's key, the modifier's effect is **×1.75** [GATED]. A modifier alone is a modest specialiser; a modifier on its archetype is a combo.
3. **No more "a second weapon doubles DPS".** Weapon slot damage factors: the strongest weapon ×1.0, then **×0.7, ×0.55, ×0.45** (total ×2.7 for four, not ×4) [GATED]. Shown on the build screen as "Load: 2.25×". Weapon levels give **pattern** (count / size / rate) at ~+8 % DPS per level instead of ~+15 % [GATED].
4. **Telemetry and the release gate** (scheduler wording, 2026-10-20):
   - **The gate** is balance's **offline ablation**: `Tools/Balance/power_split.py` writes `Docs/Balance/power_split.md` with **L8 / L16 / T20 rows**. Each group (blessings + tiers + imbue + traits, modifiers, weapon levels, extra weapons, fusion) is removed in turn on a refilled swarm field (`RH.Balance.Probe`); share = the group's DPS drop over the sum of drops.
   - **`RH.Stats` and the run-stats CSV** carry a **live, per-hit-tagged power split**, labelled **"indicative, not the gate"**. It is for playtest logs only, because per-hit tagging misattributes multiplicative sources.

**Equal bands, guards and showcase gate:** §3.7 (D-57 equal bands ×2.0–2.4 ±15 %; D-61 runaway guards: crit cap 60 %, Split max 2 generations via a capstone, ×1.75 never on crit chance or damage multipliers above +35 %; D-62 bot showcase gate).

**爽 checks** (GDD §3.5) still bind: swarm TTK ≤ 0.3 s for a steered build all run, kills/min targets, the threat cadence. The redesign moves *where* power comes from; it must not lower it.

---

## 5. Synergy matrix — CORE
### 5.1 Blessing × blessing, same Keeper (across archetypes)
**Every pair of archetypes of a Keeper has one named interaction** (the flags compose). These need no new code beyond flag composition; listed so designers and testers can find them:

| Keeper | Pair | Result |
|---|---|---|
| Storm | Tempest × Arc Grid | Split children chain: a storm of arcs |
| Storm | Tempest × Downpour | Echo bolts split on landing |
| Storm | Stormwall × Arc Grid | The orbiting ring is a live wire |
| Storm | Stormwall × Downpour | Released rings echo from the sky |
| Storm | Tempest × Stormwall | Orbiting rounds release as fans |
| Storm | Arc Grid × Downpour | Echo bolts chain |
| Forge | Hammer × Furnace | Cleaves leave fire zones |
| Forge | Hammer × Slam | Every 4th cleave slams |
| Forge | Furnace × Slam | Slams ignite their ring |
| Tide | Whirlpool × Frost | Pulled clusters freeze together |
| Tide | Whirlpool × Breakwater | Pull, then push: herding |
| Tide | Frost × Stillness | A still charged volley Freezes in a cone |
| Tide | Breakwater × Stillness | A still volley becomes a wave |
| Tide | Whirlpool × Stillness | A still volley pulls the whole screen inward |
| Tide | Frost × Breakwater | Pushed into frozen walls: crush damage |
| Shade | Hound × Collectors | Hunters' rounds seek |
| Shade | Hound × Ledger | Payday volleys use the Hound turn rate |
| Shade | Collectors × Ledger | Hunters add Debt |
| Radiance | Lance × Prism | Charged shots split into charged lances |
| Radiance | Lance × Solar | Charged-shot overkill novas ×2 |
| Radiance | Prism × Solar | Light lances nova |
| Quicksilver | Vector × Momentum | Dash volleys scale with speed |
| Quicksilver | Vector × Rebound | Dash-volley rounds ricochet |
| Quicksilver | Momentum × Rebound | Bounces gain speed damage |

### 5.2 Blessing × blessing across Keepers
- **The 15 duo reactions stay** (build_depth §4). Their triggers are re-read with the new flags (owner: hub):
  - Split / Echo / Hunter children count as hits of their Keeper.
  - A `Slam` counts as a Forge hit.
  - A `Pull` landing counts as Chill contact for Tide reactions.
- **On top of the reactions, flag combos across Keepers** (examples; they emerge from composition, no new code):
  - Forge Hammer × Quicksilver Vector = **dash slams with cleaves**.
  - Shade Hound × Storm Tempest = **homing fans**.
  - Radiance Lance × Shade Hound = **homing charged lances**.
  - Tide Whirlpool × Radiance Solar = **pulled clusters nova**.
  - Tide Stillness × Quicksilver = **anti-synergy by design** (moving vs still). Card text warns ("Breaks when you move").
- **Duo blessings** (15) are re-aimed to amplify the pair's **archetype keys** where the reaction alone is thin (text pass, `keeper_identity_text.md` §6).

### 5.3 Blessing × modifier
This is the "amplify it" column of §3. In data, `URHModifierDef.AmplifiesTags` lists `Branch.<Path>.<Archetype>` tags. The offer generator weights a modifier ×2 when its amplified archetype is owned (§6).

### 5.4 Blessing × weapon

Superseded by [keeper_identity_spec.md §3.7](#37-archetype-balance-pass-designer-confirmed-2026-10-21-mechanics-decision-list-1-source-shared-notes-mechanics_archetype_balancemd).

---

## 6. Offer changes (extend GDD §5.6, cut nothing) — CORE (rules), GATED (numbers)
- **O1 Blessing density.** Before L8, card 1's guaranteed slot prefers a blessing (70 %) over a weapon upgrade. After L8, "≥ 2 of 5 from the chosen Keepers" becomes **≥ 2 of 5 blessings from the chosen Keepers, at least one from an owned archetype** [GATED].
- **O2 Archetype momentum** = build_depth E1 renamed: × (1 + 0.5 × cards owned in the archetype); the other archetypes of that Keeper get × 0.8 after 2 cards.
- **O3 Keys first.** An archetype's amplifiers are offered only once its key is owned (amplifiers without a key are dead cards). Capstones need 2 cards (E2).
- **O4 Modifier synergy.** A modifier that amplifies an owned archetype ×2; a modifier that amplifies nothing owned ×0.7.
- **O5 Weapon fit.** New weapons that fit an owned archetype ×1.5.
- **O6 Starter pool:** the **archetype keys** (20) replace the 18 branch entries as the starter pool (D-34 updated, ⟨D-40⟩).

---

## 7. GDD changes (to apply with this spec)
- **§5.2:**
  - Weapon slot damage factors (§4.3).
  - Levels give pattern before damage.
- **§5.3:**
  - Modifiers are specialisers: flattened multipliers + synergy tags (§4.2, §5.3).
  - The capability matrix stays.
- **§5.4:**
  - Rewritten around the identity table (§1).
  - Traits, tiers, imbue and Intercessions as §3.
  - 12 blessings in 3–4 archetypes per Keeper.
  - The flag vocabulary (§2) is referenced.
  - The *Branches* paragraph from build_depth is replaced by archetypes.
- **§5.5:** unchanged (transformation hand). Path transformations are re-aimed (§8).
- **§5.6:** extensions O1–O6.
- **§3.5:** add the power-split targets to the `RH.Stats` table.
- **TDD §9:**
  - `FRHBehaviourFlags` on defs.
  - `URHModifierDef.AmplifiesTags`.
  - `URHBlessingDef.Archetype`.
  - The weapon slot factor in `URHRunTuning`.

## 8. Migration table — what happens to today's content
- *Keep* = unchanged effect.
- *Rework* = same key, new effect / role (the name may change, see the text file).
- *Retire* = removed.

**Blessings (72):**

| Keeper | Keep | Rework (→ new role) | Retire |
|---|---|---|---|
| Storm | — | Forked Path (Tempest key) · Overcharge (Tempest amp) · Stormfront (Tempest cap) · Static Focus (Stormwall key) · Low Ceiling (Stormwall amp) · Grid Lock (Stormwall cap) · Live Wire (Arc Grid key) · Ground Fault (Arc Grid amp) · Supercell (Arc Grid cap) · Thunderclap (Downpour key) · Rolling Thunder (Downpour amp) · Capacitor (Downpour cap) | — |
| Forge | Banked Heat, Slow Burn, Conflagration (Furnace) | Bellows (Hammer key) · Tempered Plating, Blast Furnace (Hammer amps) · Masterwork (Hammer cap) · Kindling (Furnace key, + Field) · Ember Trail (Slam key) · Quench, Spark Shower (Slam amps) · Cinder Storm (Slam cap) | — |
| Tide | Deep Cold (amp), King Tide (cap), Spring Tide (cap, + Freeze ring) | Undertow (Whirlpool key: Pull) · Shatterpoint (Whirlpool cap) · Cold Front (Breakwater key: waves) · Riptide (Breakwater amp) · Brittle (Frost key) · Breakwater (Frost amp: walls) · Deep Reserve (Frost cap) · High Water (Stillness key) · Wellspring (Stillness amp) | — |
| Shade | Garnish, Escrow, Debt Collectors, Arrears | Lien (Hound key: Seek) · Foreclosure (Hound amp) · Final Notice (Hound cap) · Dividend (Collectors key: Hunter) · Interest (Ledger key: Debt) · Long Arm, Compound (Ledger amps) · Audit (Ledger cap) | — |
| Radiance | Keen Edge, Focal Point, Afterimage, Corona, Whiteout, Wide Aperture, Radiant Wake | Perfect Sight (Lance key) · Glint (Lance cap) · Flashpoint (Prism key: crit Split) · Halo (Solar key: Nova) · Halo Ring (Solar cap) | — |
| Quicksilver | Light Step, Tailwind, Wayfare, Bank Shot, Carom, Relay Chain | Blink Strike (Vector key) · Second Wind (Vector cap) · Fleet (Momentum key: SpeedScale) · Drafting (Momentum amp) · Terminal Velocity (Momentum cap) · Slipstream (Rebound key) | — |

**Retirements:** none; every key is reused. Stat-only blessings (+X % damage, +crit and the like) are gone as *stat* cards; each became an archetype role.

**Other content:**

| Item | Decision |
|---|---|
| Duo blessings (15) | **Keep**, with a text re-aim pass toward the archetype keys (§5.2) |
| Duo reactions (15) | **Keep**; triggers re-read with flags (§5.2) |
| Path transformations | Storm *Overload*: keep (Arc Grid's peak). Forge *Kiln*: **rework**, the slag turrets cleave instead of shoot (melee identity). Tide *Glacier*: keep (Frost). Tide *Slack Water*: keep. Shade *Collection*: keep (Collectors). Radiance *Prism*: **rework** into *Zenith*: charged shots split into lances and nova (Prism is now an archetype). Quicksilver *Vector*: **rework** into *Slipstream Drive*, a permanent dash-volley + speed-scale combo (Vector is now an archetype key) ⟨D-41⟩ |
| Weapon evolutions (10), fusions (6) | **Keep** |
| Tiers T1–T3 | Storm: **rework** (split-based) · Forge: **rework** (T2 slam; old T2 + T3 merged into T3) · Tide: keep · Shade: keep · Radiance: keep · Quicksilver: **rework** T3 |
| Imbue (6) | Storm, Forge, Tide, Shade: **rework** (identity added, old status kept) · Radiance, Quicksilver: keep |
| Patron traits (6) | **New** |
| Intercessions (6) | Names kept. Thunderhead, Forge Nova, Tidal Wave, Closing: **rework**. Sun Lance, Time Dilation: **extended** |
| build_depth §2 branches | **Superseded by archetypes.** The framework is reused: `Branch.*` tags are now archetype tags; build_depth's branch names (Arc / Thunder / Charge …) are replaced by the archetype names here |

---

## 9. Ordered steps and owners

| # | Step | Owner | Tag | Est. |
|---|---|---|---|---|
| 1 | Behaviour layer: **done as `ERHWeaponBehaviour` / `FRHWeaponBehaviourMod` (d73f343, §12.0)**; still to do: **D-47 kinds `StatusApply` (+ `StatusTag`) and `Pull` / `Push`**, capability fallbacks per kind, the per-weapon spawn cap, tests | **weapons** | CORE | 6 d (part done) |
| 2 | Weapon slot damage factors + level track reshape (pattern over damage) + build-screen "Load" readout | **weapons** (factors), **meta-ui** (readout) | CORE (mechanism), GATED (factors) | 2 d |
| 3 | Re-author the 72 blessings as archetype key / amp / capstone defs with flags (and all numbers) · patron traits (D-44), Path tier reworks and imbue reworks | **balance** (72 blessings + numbers) · **integration (tiers)** (traits, tiers, imbue; owner change by the scheduler 2026-10-20) | CORE (data), GATED (numbers) | 6 d |
| 4 | Intercession reworks (4) and extensions (2), incl. D-45 Tidal Wave pull-then-freeze | **weapons** (effects; owner change by the scheduler 2026-10-20) + **statuses** (enemy-side: Pull lines, Freeze holds, execute) | CORE | 4 d |
| 5 | Enemy-side support: `Pull` / `Push` on the enemy SoA with mass by role; Freeze-until-hit (cap 8); Frozen-as-obstacle; Seek target classes (Marked, Elite); execute rules; slag-turret cleave (Kiln); **Hunters** (D-47: extend the Shade minion system, data-only minions firing through `FireVolleyFrom`, ≤ 6 alive) | **statuses** | CORE | 6 d |
| 6 | Modifier flattening + `AmplifiesTags` + the ×1.75 synergy rule | **balance** (numbers) + **weapons** (rule) | CORE (rule), GATED (numbers) | 2 d |
| 7 | Offer changes O1–O6; starter pool = 20 keys; telemetry: power-split ablation in `RH.Stats` | **integration (tiers)** | CORE (rules), GATED (weights) | 3 d |
| 8 | Duo reaction trigger re-read with flags; duo blessing re-aim; Path transformation reworks (Kiln, Zenith, Slipstream Drive) | **hub** (reactions, duos), **statuses** (transformations) | CORE | 4 d |
| 9 | UI: archetype on cards (plaque "Storm · Tempest", key / capstone marks), build screen archetype progress, trait line on the Shrine, "Breaks when you move" warnings | **meta-ui** | CORE | 3 d |
| 10 | Text (`keeper_identity_text.md`) + zh | **loc-zh** | CORE | 2 d + loc |
| 11 | Bot sims: the power split ≥ 60 % blessings, TTK and kills/min held, archetype divergence (distinct archetype sets ≥ 60 % over 20 runs) | **balance** | **GATED** release gate | — |

Started 2026-10-20 by the scheduler: step 1 (D-47 kinds), step 5 (Hunters), step 7 (offers O1–O6, behind flags), step 10 (zh). Steps 1 and 5 can start at once; 3 needs 1's flag names; 7 needs 3's tags. About 37 d total, parallel across 5 sessions.

### 9.1 Routing for the archetype balance pass (§3.7, 2026-10-21)
Files are checked against the code on main and the scheduler's ownership map:
- **weapons:** `Combat/*`, `Build/RHModifierDef.*`.
- **balance:** `Build/Blessings/*`, `Build/Effects/Branches/*`, `Build/RHEffect_Blessings.*`, the `ST_Blessings` EN rows, `Tools/Balance`, `Docs/Balance`.
- **statuses:** `Enemies/*`, `Build/Hunters/*`.
- **tiers:** `Build/RHOfferArchetypes.*`, `Player/*`, `Debug/RHBot.*`, `UI/RHHUDWidget.*`.
- **meta-ui:** `UI/*` except the HUD.
- **loc-zh:** `zh_overrides.csv`.

⚑ `Build/RHEffect_Blessings.*` is listed for **weapons** in the old map (the imbue work), but balance's step-3 passes (a703c97, 33c8ca0, e2784cc) are the recent edits. Here it is **balance's**; scheduler, please confirm.

**A. One row per change:**

| # | Change | Owner | Files |
|---|---|---|---|
| R1 | D-57 equal bands: all archetype numbers | balance | `Build/Blessings/RHBlessingBehaviourData.*`, `Build/Effects/Branches/RHBranches.*`, `Build/RHEffect_Blessings.*` |
| R2 | D-58 / D-59 fit table (§3.7) into `GetFitWeapons`; audit that no multiplier reads fit | tiers | `Build/RHOfferArchetypes.*` |
| R3 | D-58 resonance mark on cards and the build screen | meta-ui | `UI/RHCardStyle.*` + the build-screen widget (UI, not the HUD) |
| R4 | D-60 control on control-immune → **Cracked** (+20 %, 2 s) instead of refused; its status look | statuses | `Enemies/RHStatus.*`, `Enemies/RHStatusLook.*` |
| R5 | D-61 crit-chance cap 60 % | tiers | `Player/RHStats.*` |
| R6 | D-61 Split recursion only from a capstone flag, max 2 generations | weapons | `Combat/RHWeaponBehaviour.*`, `Combat/RHSpecComposition.*` |
| R7 | D-61 the ×1.75 synergy never on crit chance or on damage multipliers above +35 % | weapons | `Build/RHModifierDef.*` |
| R8 | D-62 showcase build policy for the bot (20 builds + the off-fit variant) | tiers | `Debug/RHBot.*` |
| R9 | D-62 showcase gate run + report (500 kills/min @ 18:00, TTK ≤ 0.15 s, off-fit > 0.6 s @ 12:00, boss TTK ±20 %) | balance | `Tools/Balance/*`, `Docs/Balance/*` |
| R10 | Tempest: children of a hit on an elite / boss re-target it | weapons | `Combat/RHWeaponBehaviour.*`, `Combat/RHSpecComposition.*` |
| R11 | Stormwall: released rounds +30 % / +1 pierce; Low Ceiling +2 | balance | R1's files |
| R12 | Arc Grid ground return (no target → the same target at 50 %) | weapons | `Combat/*` (Chain) |
| R13 | Downpour: Echo = a delayed strike on the impact point (0.4 s) | weapons | `Combat/*` (Echo) |
| R14 | Hammer: cleaves apply every socketed modifier's on-hit effect | weapons | `Combat/RHSpecComposition.*` |
| R15 | Hammer: Masterwork's ×1.5 removed | balance | R1's files |
| R16 | Furnace: Burn ticks 25 % faster inside player fields | statuses | `Enemies/RHStatus.*` |
| R17 | Furnace: Conflagration also spreads on kill | balance | R1's files |
| R18 | Slam: triggers = dash landing + every 6th melee hit + hit taken | balance | `Build/Effects/Branches/RHBranches.*` |
| R19 | Whirlpool: crush damage on pulled-enemy collisions (15 %) | statuses | `Enemies/RHEnemySubsystem.*` |
| R20 | Breakwater: herding (a wave front) + collision crush 30 % (enemy, wall, Frozen, boss) | statuses | `Enemies/RHEnemySubsystem.*` |
| R21 | Breakwater: Cold Front every 3rd volley | balance | R1's files |
| R22 | Stillness rework (< 50 uu / 0.6 s; dash keeps the charge; walking spends it; every weapon's next volley) | balance | `Build/Effects/Branches/RHBranches.*` |
| R23 | Hound: +1 pierce on the key | balance | `Build/Blessings/RHBlessingBehaviourData.*` |
| R24 | Collectors: highest-level weapon at 40 % with its modifiers; every elite kill raises one; Arrears at 60 % | statuses | `Build/Hunters/*` |
| R25 | Ledger: Debt 15, payday ×2.5 | balance | R1's files |
| R26 | Prism: lances 30 %, one generation | balance | R1's files (the generation cap itself is R6) |
| R27 | Solar: Sun-charge capstone (20 % of elite / boss damage → a nova every 3 s) | balance | `Build/RHEffect_Blessings.*` |
| R28 | Vector: refund on the key (0.15 s per kill, ≤ 50 %), volley ×1.5, capstone every 3rd dash free | balance | `Build/Effects/Branches/RHBranches.*` |
| R29 | Momentum: the stacks rework (effect) | balance | `Build/Effects/Branches/RHBranches.*` |
| R30 | Momentum: the stack counter on the HUD | tiers | `UI/RHHUDWidget.*` |
| R31 | Rebound: a bounce returns to the same target when alone | weapons | `Combat/RHBulletSubsystem.*` |
| R32 | Rebound: Carom's number | balance | R1's files |
| R33 | Card text EN for the reworked cards (`keeper_identity_text.md` §8) | balance | `Content/RH/Text/ST_Blessings.csv` |
| R34 | zh for R33 + the `Cracked` and `Momentum` keywords (`keyword_notes_spec.md`) | loc-zh | `Content/Localization/zh_overrides.csv` (+ `ST_Keywords` rows handed to meta-ui) |

**B. File → single owner** (no file has two owners):

| Owner | Files touched by this pass |
|---|---|
| weapons | `Combat/RHWeaponBehaviour.*`, `Combat/RHSpecComposition.*`, `Combat/RHBulletSubsystem.*`, `Build/RHModifierDef.*` |
| balance | `Build/Blessings/RHBlessingBehaviourData.*`, `Build/Effects/Branches/RHBranches.*`, `Build/RHEffect_Blessings.*`, `Content/RH/Text/ST_Blessings.csv` (EN), `Tools/Balance/*`, `Docs/Balance/*` |
| statuses | `Enemies/RHStatus.*`, `Enemies/RHStatusLook.*`, `Enemies/RHEnemySubsystem.*`, `Build/Hunters/*` |
| tiers | `Build/RHOfferArchetypes.*`, `Player/RHStats.*`, `Debug/RHBot.*`, `UI/RHHUDWidget.*` |
| meta-ui | `UI/RHCardStyle.*`, the build-screen widget, `ST_Keywords` (table registration) |
| loc-zh | `Content/Localization/zh_overrides.csv` |

**Order:**
- R4, R6, R7 and R2 first: the guards and rules. R1 tunes against them.
- R9 runs last as the gate.

## 10. Decisions — all approved as recommended (scheduler decision 2026-10-20; D-42 / D-43 / D-46 stay GATED on numbers)

| # | Decision | Decided |
|---|---|---|
| D-38 | Identity mapping: Storm bullet-hell · Forge melee · Tide control (the added identity) · Shade tracking · Radiance burst · Quicksilver mobility | Yes |
| D-39 | Archetype counts 4 / 3 / 4 / 3 / 3 / 3 = 20, 12 blessings per Keeper (no new keys) | Yes |
| D-40 | Starter pool = the 20 archetype keys (updates D-34's 18 branch entries) | Yes |
| D-41 | Path transformation reworks: Kiln cleaves; Prism → *Zenith*; Vector → *Slipstream Drive* (their old roles became archetypes) | Yes |
| D-42 | Weapon slot damage factors 1.0 / 0.7 / 0.55 / 0.45, shown as "Load" | Yes, GATED |
| D-43 | Modifier flattening as §4.2 + ×1.75 when amplifying an owned archetype | Yes, GATED |
| D-44 | Patron traits are new, patron-only passives (no Favor) | Yes |
| D-45 | Tidal Wave keeps its name and its fate role (the Reclaimer's second Intercession) with the new pull-then-freeze effect | Yes |
| D-46 | O1: the guaranteed slot prefers a blessing (70 %) before L8 | Yes, GATED |
| D-57 … D-63 | Archetype balance pass (§3.7): equal bands, fit is mechanical (display + O5 only), the fit table (6 per weapon), Cracked conversion on control-immune targets, runaway guards, the showcase gate, 20 per-archetype changes | **Designer-confirmed 2026-10-21** (mechanics list #1). D-60's EN name "Cracked" (not "Brittle", which is a card name) is planning's naming call |
| D-47 | Weapons adds `StatusApply` (+ `StatusTag`) and `Pull` / `Push` as behaviour kinds; Pierce and Field stay modifier-like; Hunters are owned by statuses (Shade minion system) | Yes |

## 11. Baseline from balance — measured 2026-10-20
**Source:** `Docs/Balance/power_split.md` (balance e90d6af on `claude/jovial-satoshi-0eb5d9`; raw data `power_split_raw.json`).

**Setup:**
- main 065edd4; six steered bot runs, one per patron; omens off; fresh save, so only the 18 branch-entry cards were available.
- Each build is rebuilt per configuration and ablated on a refilled swarm field at 10× swarm HP.
- **Pre-redesign:** today's blessings and modifiers.

**Mean share of the DPS gain:**

| | Blessings (+ tiers, imbue) | Modifiers | Weapon levels | New weapons | Transformation | Duo reactions |
|---|---|---|---|---|---|---|
| L8 | 16 % | 1 % | 20 % | **62 %** | – | 2 % |
| **L16** | **16 %** | 19 % | 21 % | **34 %** | 7 % | 2 % |
| 20:00 | 19 % | 22 % | 24 % | 22 % | 8 % | 5 % |

Gain over the bare starter kit: L8 ×4–8 · L16 ×9–100 · 20:00 ×24–559.

**What it says (it confirms the designer's complaint):**
1. **Blessings are 16 % at L16 against the ≥ 60 % target.** One more blessing pick is worth **+3 %** (−7 … +14 %), one more modifier **+1 … +51 %**, and one more new weapon **+7 … +121 %**. A blessing is the weakest pick of the three.
2. **New weapons dominate early.** They are 62 % at L8 and 34 % at L16. The 2nd weapon multiplies DPS by **×2.3 (median; ×0.9 … ×10.9)**, the 3rd by ×1.0 … ×4.9. This is the "a second weapon doubles DPS" problem, measured.
3. **Modifier outliers.** Radiance crit builds (Crit Lens + Split Core + Fork): **×10.7 at 20:00**, ×5.3 at L16; Forge ×3.5 at L16; Quicksilver ×2.8 at 20:00. The modifier *mean* (19 %) is inside the 15–25 % band, but its variance is not.
4. **The Keeper's own power is mostly interaction.** Removing the patron Keeper loses 12–69 %, mostly through other systems (Storm L16 60 %, Radiance 20:00 69 %). The identity layer should make that interaction explicit (behaviour flags) instead of incidental.
5. **Blessing power sits in 1–2 branches per run.** The bots held entry cards plus duo blessings and the L16 Path transformation. The build-depth cards barely appeared, because of the fresh-save pool.

**Targets set against it (§4; numbers GATED):**

| Group at L16 | Baseline | Target | Lever |
|---|---|---|---|
| Blessings (cards + tiers + imbue + traits) | 16 % | **≥ 60 %** | Archetype keys at +25–40 % each (a pick worth ~10× today's +3 %); traits; identity tiers (§3, D-39 / D-44) |
| Modifiers | 19 % (high variance) | 15–25 %, **no single modifier set above ×2.5** | Flattening + `AmplifiesTags` ×1.75 (D-43): the power moves *into* archetype combos; cap the crit stack (Crit Lens + Split Core + Fork) |
| New weapons | 34 % (2nd weapon ×2.3) | ≤ 15 % (2nd weapon ≤ ×1.7) | Load factors 1 / 0.7 / 0.55 / 0.45 (D-42) |
| Weapon levels | 21 % | ≤ 15 % | Levels add pattern before damage (§4.3) |
| Transformation + reactions | 9 % | counted with blessings (they are Keeper power) | — |

**Measurement notes** (balance):
- 10× HP under-weights kill-chain effects. At 1× the strong builds saturate the field (Storm L16 wipes it every frame).
- One 10 s window per configuration gives about ±10–20 %, so read the mean rows.
- **Re-run the same probe** after steps 3 and 6 land, with the full archetype pool unlocked (not a fresh save), and add a 1×-HP row for kill chains. That re-run is the release gate (§4.4, step 11).

## 12. Alignment with the weapons hook layer — agreed with the weapons session (2026-10-20)
The spec text uses **concept names**; the code uses these. One accumulator serves modifiers and blessings.

**Where it plugs in.**
- Blessing / tier / trait / transformation behaviour is a **behaviour def**: the same field names as `URHModifierDef`. It is passed to `RHSpecComposition::Compose` as a second input, **applied after the modifiers and before the hero stats**, so ExtraPierce-style passes stay in one place.
- Caps stay per weapon, inside Compose; `RHModCompat::IsCompatible` checks them.
- Behaviours are `ERHBulletFlags` on `FRHBulletSpec`; params are the existing additive / multiplier / largest-wins fields.

### 12.0 Committed API (weapons d73f343, `Combat/RHWeaponBehaviour.h`) — these names are binding
- **`ERHWeaponBehaviour`**: `Seek`, `Curve`, `Split` (Trigger `OnHit` / `OnExpiry`; Shape `Fan` / `Ring` / `Spiral`), `Chain` (pierce → chain), `Spread`, `Density`, `Orbit`, `ConvertMelee`, `Echo`, `Charge`, `VolleyOnDash`.
- **`FRHWeaponBehaviourMod`** has generic fields `Magnitude`, `Count`, `AngleDeg`, `Range`, `Time`, `DamageMult`, `Scale`, plus optional `RequiredCaps` / `BlockedCaps`. The header defines each field per kind.
- **Granted by:** a blessing through `FRHBlessingRank::WeaponBehaviours`; an effect (tier, trait, transformation, Intercession) through `URHEffectManager::SetWeaponBehaviours(Source, Mods)`.
- **Stacking:** `Magnitude` / `Count` / `AngleDeg` add; `Range` / `Time` take the largest; `DamageMult` / `Scale` multiply. Then a per-weapon clamp.

**How the spec's concepts map onto it:**

| Spec concept | Granted as |
|---|---|
| Seek, Split, Orbit (ConvertOrbit), ConvertMelee, Echo, Charge, VolleyOnDash, Chain | the matching `ERHWeaponBehaviour` kind |
| Pierce, Field (Linger), StatusApply, Push (Knockback), Pull (Backwash / PullToImpact) | a **modifier-like entry** on the blessing (the `URHModifierDef` fields), as weapons proposed |
| Slam | `VolleyOnDash` with `ConvertMelee` (dash slam), or an effect firing a MeleeArc ring through `FireVolleyFrom` (hit-taken / every-N slams) |
| Stillness, SpeedScale, Nova | **blessing effects** (`URHEffect`, balance): Stillness and SpeedScale set a `DamageMult` / fire-rate behaviour through `SetWeaponBehaviours` while their condition holds; Nova is an on-kill effect |
| Hunter (summons) | **not the weapons layer.** Extends the existing Shade minion system (Shade T2 Shades / the *Collection* transformation). **Owner: statuses.** Hunters fire through `FireVolleyFrom` |

- **`Curve`, `Spread`, `Density`:** weapons kinds the spec doesn't use yet. Free for balance to use as amplifier effects (e.g. Tempest amplifiers could use `Density`).
- **Decided for weapons ⟨D-47⟩:** add **`StatusApply` (with `StatusTag`)** and **`Pull` / `Push`** as `ERHWeaponBehaviour` kinds. Control (Tide) and tracking (Shade) lean on them, and stacking / clamping through one path is safer than modifier-like entries. Pierce and Field stay modifier-like.

### 12.1 Earlier alignment (concept → existing modifier fields; superseded by 12.0 where it differs)

**Concept → code:**

| Spec concept (§2) | Code (existing, or **new**) |
|---|---|
| `Seek` | `Homing` + `HomingTurnDegPerSec`, `HomingRange`; **new param** `HomingTarget` {Nearest, Marked, Elite} |
| `Split` (on hit, fan) | `SplitOnHit` + `SplitAdd`, `SplitAngleDeg` |
| `Split` (on expiry, ring) / (fan) | `ScatterOnExpiry` / `WaveOnExpiry` |
| `Split` pattern Spiral | **new** child pattern |
| `Pierce` | `Pierce` / `PierceAdd` |
| `Chain` | `Ricochet` (bounce to N) or `ChainArc` (fusion bolt jumps). Not `ChainOnKill`, which is the free volley on kill |
| `Field` | `LingerOnImpact` (`Linger*`) |
| `Push` | `Knockback` (`KnockbackAdd`) |
| `Pull` | `BackwashPull` (toward the shooter), or **new** `PullToImpact` for Whirlpool (impact-centred) |
| `StatusApply` | `ApplyPatronStatus` + `StatusChance`; **new param** `StatusTag` so non-patron statuses can be named |
| `ConvertOrbit` | `Orbit` flag (`OrbitTime`, `OrbitRadius`); Orbit Lock already orbits then releases |
| `Echo` | **new** `EchoVolley` (delay, dmg %, source): a delayed repeat through `URHWeaponInstance::FireVolleyFrom` |
| `VolleyOnDash` | `FireVolleyFrom(Origin, Aim, Bullets, SpecOverride)` (exists) + **new** dash hook |
| `Charge` | **new** per-weapon shot counter / idle timer in the weapon instance + a derived ×K spec |
| `Hunter` | **new**: minions as data (not Actors) firing via `FireVolleyFrom` from their positions |
| `ConvertMelee` | **new** `PatternOverride` = MeleeArc on the composed weapon |
| `Slam`, `Stillness`, `Nova`, `SpeedScale` | **new** (Slam = a MeleeArc ring through `FireVolleyFrom`; the others are hero-side or derived specs) |

**Rules agreed:**
1. **`PatternOverride`** may only switch to patterns that exist as archetypes (MeleeArc, Orbit, Beam, Lob, Radial, Spiral).
   - At most **one** override per weapon: the highest-tier source wins (capstone > key > tier > trait), and overrides never stack.
   - After an override, the **cap check runs again**. For example, ConvertMelee drops Projectile, so Pierce / Homing stop applying.
   - Incompatible modifiers are refunded as a Renegotiate (build_depth §3).
2. **Count → damage conversion** when a high-count pattern becomes melee (Scatter, Ring, Spiral): the cleave's damage = the pattern's per-volley damage × 0.8. It is one arc, never N arcs (as Sweep Beam did).
3. **Spawn cap per weapon:** max bullets spawned per second per weapon (start at 400/s [GATED]). It applies to the *product* of Echo × Burst × Split × Chain × Hunters, not per flag. On top, the global 6 000 cap and oldest-first recycle hold.
4. **Hunters** are few: max 6 alive per player [GATED]. Their volleys count against the spawn cap.
5. **Push vs Pull on the same hit:** the larger magnitude wins; on a tie, **Pull** wins. The control identity prefers clustering.

Owner: weapons (step 1). The flag names in §2 / §3 stay as concept names for designers; the data uses the code names above.
