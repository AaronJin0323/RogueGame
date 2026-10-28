# Build depth — card text, EN-first (2026-10-20)

This is the text for `build_depth_spec.md`. Format and rules follow `act2_text.md` §0.
- Card **rules** text is plain rules (Narrative §2.4: rules, not flavour). Numbers are format args (`{0}`, `{1}`) that come from data per rank and rarity.
- Names ≤ 18 chars EN / ≤ 6 ZH. Rules ≤ 90 chars EN / ≤ 40 ZH. Toasts ≤ 60 chars EN / ≤ 26 ZH.
- zh-Hans is written by the loc session from context, not translated.
- **No name repeats an existing name.** I checked against `ST_Blessings`, `ST_Modifiers`, `ST_Ultimates`, `ST_Fusions` and the Act II / III docs.
- **One rename:** the Bid Term **Lien** clashed with Shade's blessing *Lien*, so the Term is now **Levy** (`Bid.Term.Levy.*`; the change is in `act2_text.md` §7 and `full_game_specs.md` §4).

**Owners:**
- integration: transform hand, offer extensions, reactions, omens.
- weapons / content: effects, `DA_*`.
- meta-ui: cards, hand titles, toasts, the Shrine omen line.
- loc-zh: zh.

**Count:**

| Section | Strings |
|---|---|
| §1 Transformations | 32 + 6 UI |
| §2 Branches and new blessings | 18 + 84 |
| §3 Duo blessings | 30 + 1 |
| §4 Reactions | 30 |
| §5 Omens | 24 + 2 |
| §6 UI | 6 |
| **Total** | **≈ 233 EN strings** |

---

## 1. Transformations — `ST_Fusions` (`Transform.*`)

### 1.1 Path transformations (6 new; Tide's *Slack Water* is in `act2_text.md` §12.2)
They replace `Blessing.<Path>.Legendary.*` (Final Advance is retired, D-33).

| Key prefix | Name | Desc |
|---|---|---|
| `Transform.Storm` | Overload | Shock stacks to 3. At 3, the enemy discharges a chain bolt to {0} more enemies. |
| `Transform.Forge` | Kiln | Enemies that die Burning become slag turrets for {0} s that spit cinders at nearby enemies. |
| `Transform.Tide` | Glacier | Frozen enemies become ice pillars for {0} s that block enemies and their shots, then shatter. |
| `Transform.Shade` | Collection | Marked enemies that die rise as Shades that orbit you and fire your first weapon at {0}% damage. |
| `Transform.Radiance` | Prism | Crits split the bullet into 3 light lances that pierce, each dealing {0}% damage. |
| `Transform.Quicksilver` | Vector | +1 dash charge. Every dash fires all your weapons along its line and leaves a blade trail. |

### 1.2 Weapon evolutions (10)

| Key prefix | Name | Desc |
|---|---|---|
| `Evolve.PulseRifle` | Railstorm | Becomes {0} orbiting blades that launch at the nearest enemy and return. |
| `Evolve.ScatterCannon` | Flechette | Pellets embed in enemies, then explode after 0.5 s. |
| `Evolve.OrbitalDrones` | Blade Halo | Drones become {0} blades. Their orbit widens the faster you move. |
| `Evolve.RailLance` | Sweep Beam | Fires a beam for {0} s that sweeps across the densest side. |
| `Evolve.FlakMortar` | Carpet | Each shell lands as a line of {0} blasts walking forward. |
| `Evolve.ArcBlade` | Cyclone | Spins in a full circle while you move, hitting {0} times per second. |
| `Evolve.SeekerSwarm` | Murmuration | Missiles stay as a hunting swarm for {0} s and pick new targets after each kill. |
| `Evolve.RingEmitter` | Pulse Wall | Rings hold as a wall around you for {0} s, pushing enemies out. |
| `Evolve.SpiralProjector` | Vortex | The spiral curls inward into a vortex that pulls enemies together. |
| `Evolve.GravityMine` | Singularity | All your mines merge into one that pulls everything in, then collapses. |

### 1.3 Hand UI (6) — `ST_UI`

| Key | EN | Context |
|---|---|---|
| `Transform.Hand.Title` | Choose a transformation | L16 / L24 hand header |
| `Transform.Kind.Path` | Path | Card plaque |
| `Transform.Kind.Weapon` | Evolution | Card plaque |
| `Transform.Kind.Fusion` | Fusion | Card plaque |
| `Transform.Sealed` | Sealed. Unlock it in Contracts. | The sealed 4th card (D-31) |
| `Transform.Refund` | {0} doesn't fit {1}. Refunded as a Renegotiate. | Toast when an evolution drops an incompatible modifier ({0} modifier, {1} weapon) |

## 2. Branches (18 names) and new blessings (42 × name + desc) — `ST_Blessings`
- Branch names: `Branch.<Path>.<Branch>.Name`.
- Blessings: `Blessing.<Path>.<Key>.Name` / `.Desc`.
- Roles: **E** entry · **C** core · **K** capstone (offered only after 2 cards of that branch).

### Storm — Arc · Thunder · Charge

| Key | Branch | Role | Name | Desc |
|---|---|---|---|---|
| `Branch.Storm.Arc` | | | **Arc** | |
| `Blessing.Storm.ForkedPath` | Arc | C | Forked Path | Arcs reach {0} more enemies. |
| `Blessing.Storm.GridLock` | Arc | K | Grid Lock | Shocked enemies near each other arc between themselves for {0} damage every second. |
| `Branch.Storm.Thunder` | | | **Thunder** | |
| `Blessing.Storm.LowCeiling` | Thunder | C | Low Ceiling | Thunderclap strikes {0} enemies at once. |
| `Blessing.Storm.RollingThunder` | Thunder | C | Rolling Thunder | Bolts leave a crackling field for 1 s that Shocks anything in it. |
| `Blessing.Storm.Stormfront` | Thunder | K | Stormfront | Every {0} kills, bolts strike 5 enemies. |
| `Branch.Storm.Charge` | | | **Charge** | |
| `Blessing.Storm.Capacitor` | Charge | C | Capacitor | Every {0} hits, your next hit deals +{1}% damage and arcs. |
| `Blessing.Storm.Supercell` | Charge | K | Supercell | Crits Shock. Shocked enemies take +{0}% crit damage. |

Existing cards: Live Wire (Arc, E), Ground Fault (Arc, C), Thunderclap (Thunder, E), Overcharge (Charge, E), Static Focus (Charge, C).

### Forge — Burn · Ember · Anvil

| Key | Branch | Role | Name | Desc |
|---|---|---|---|---|
| `Branch.Forge.Burn` | | | **Burn** | |
| `Blessing.Forge.SlowBurn` | Burn | C | Slow Burn | Burn stacks up to {0} times. |
| `Blessing.Forge.Conflagration` | Burn | K | Conflagration | Every second, each Burning enemy spreads Burn to one neighbour. |
| `Branch.Forge.Ember` | | | **Ember** | |
| `Blessing.Forge.SparkShower` | Ember | C | Spark Shower | Burning enemies throw {0} embers when they die. Embers Burn what they hit. |
| `Blessing.Forge.BlastFurnace` | Ember | C | Blast Furnace | Your explosions are {0}% larger. |
| `Blessing.Forge.CinderStorm` | Ember | K | Cinder Storm | Dashing sets off every Burning enemy within {0} m for its remaining Burn. |
| `Branch.Forge.Anvil` | | | **Anvil** | |
| `Blessing.Forge.Quench` | Anvil | C | Quench | When you are hit, gain +{0}% attack speed for 3 s. |
| `Blessing.Forge.Masterwork` | Anvil | K | Masterwork | Your first weapon gains a 4th modifier socket. |

Existing cards: Kindling (Burn, E), Banked Heat (Burn, C), Ember Trail (Ember, E), Tempered Plating (Anvil, E), Bellows (Anvil, C).

### Tide — Frost · Wellspring · Current

| Key | Branch | Role | Name | Desc |
|---|---|---|---|---|
| `Branch.Tide.Frost` | | | **Frost** | |
| `Blessing.Tide.Brittle` | Frost | C | Brittle | Frozen enemies take +{0}% damage. |
| `Blessing.Tide.Shatterpoint` | Frost | K | Shatterpoint | When a Frozen enemy shatters, enemies within {0} m Freeze. |
| `Branch.Tide.Wellspring` | | | **Wellspring** | Branch shares its name with its entry card (by design: it *is* the spring) |
| `Blessing.Tide.HighWater` | Wellspring | C | High Water | Above 80% HP, deal +{0}% damage. |
| `Blessing.Tide.SpringTide` | Wellspring | K | Spring Tide | Healing past full HP becomes a shield, up to {0}. |
| `Branch.Tide.Current` | | | **Current** | |
| `Blessing.Tide.Riptide` | Current | C | Riptide | Your hits push enemies back {0} m. |
| `Blessing.Tide.Breakwater` | Current | C | Breakwater | Cold Front also pushes enemies away. |
| `Blessing.Tide.KingTide` | Current | K | King Tide | Every {0} s, a wave rolls out from you, pushing back and Chilling everything. |

Existing cards: Undertow (Frost, E), Deep Cold (Frost, C), Wellspring (Wellspring, E), Deep Reserve (Wellspring, C), Cold Front (Current, E).

### Shade — Mark · Souls · Ledger

| Key | Branch | Role | Name | Desc |
|---|---|---|---|---|
| `Branch.Shade.Mark` | | | **Mark** | |
| `Blessing.Shade.Garnish` | Mark | C | Garnish | Marked enemies take +{0}% damage. |
| `Blessing.Shade.FinalNotice` | Mark | K | Final Notice | Marked enemies are executed {0}% sooner. |
| `Branch.Shade.Souls` | | | **Souls** | |
| `Blessing.Shade.Escrow` | Souls | C | Escrow | Souls heal {0} HP more. |
| `Blessing.Shade.DebtCollectors` | Souls | C | Debt Collectors | Shades last {0} s longer and deal +{1}% damage. |
| `Blessing.Shade.Arrears` | Souls | K | Arrears | Every {0} Souls you collect raise a Shade. |
| `Branch.Shade.Economy` | | | **Ledger** | |
| `Blessing.Shade.Compound` | Ledger | C | Compound | XP gems are worth +{0}% for each Path at Tier 1 or higher. |
| `Blessing.Shade.Audit` | Ledger | K | Audit | Every {0} levels, gain a Renegotiate. |

Existing cards: Lien (Mark, E), Foreclosure (Mark, C), Dividend (Souls, E), Interest (Ledger, E), Long Arm (Ledger, C).

### Radiance — Crit · Blind · Halo

| Key | Branch | Role | Name | Desc |
|---|---|---|---|---|
| `Branch.Radiance.Crit` | | | **Crit** | UI may show it as "Sight" (`Branch.Radiance.Crit.Name` = "Sight") |
| `Blessing.Radiance.KeenEdge` | Crit | C | Keen Edge | +{0}% crit chance. |
| `Blessing.Radiance.FocalPoint` | Crit | C | Focal Point | Each crit on the same enemy adds +{0}% crit damage against it, up to 5 times. |
| `Blessing.Radiance.PerfectSight` | Crit | K | Perfect Sight | Every {0}th shot of each weapon is a sure crit. |
| `Branch.Radiance.Blind` | | | **Glare** | |
| `Blessing.Radiance.Whiteout` | Blind | K | Whiteout | Blinded enemies Blind their neighbours when they die. |
| `Branch.Radiance.Halo` | | | **Halo** | |
| `Blessing.Radiance.WideAperture` | Halo | C | Wide Aperture | Bullets are {0}% larger. |
| `Blessing.Radiance.RadiantWake` | Halo | C | Radiant Wake | Bullets leave a fading trail of light that deals {0}% of their damage. |
| `Blessing.Radiance.HaloRing` | Halo | K | Halo Ring | A ring of light around you burns enemies {0} times per second. |

- Existing cards: Glint (Crit, E), Flashpoint (Blind, E), Afterimage (Blind, C), Corona (Blind, C), Halo (Halo, E).
- Branch display names: Crit → **Sight**, Blind → **Glare**, Halo → **Halo**.
- "Burns" in *Halo Ring* means damage, not the Burn status. The loc session must keep the zh distinct from 燃烧.

### Quicksilver — Dash · Momentum · Ricochet

| Key | Branch | Role | Name | Desc |
|---|---|---|---|---|
| `Branch.Quicksilver.Dash` | | | **Dash** | |
| `Blessing.Quicksilver.SecondWind` | Dash | C | Second Wind | Dashing heals {0} HP. |
| `Blessing.Quicksilver.BlinkStrike` | Dash | K | Blink Strike | Dashing through enemies deals {0}× your strongest weapon's hit. |
| `Branch.Quicksilver.Momentum` | | | **Momentum** | |
| `Blessing.Quicksilver.Drafting` | Momentum | C | Drafting | +{0}% damage for every 10% bonus move speed. |
| `Blessing.Quicksilver.TerminalVelocity` | Momentum | K | Terminal Velocity | Above {0}% move speed, bullets pierce one more enemy. |
| `Branch.Quicksilver.Ricochet` | | | **Rebound** | |
| `Blessing.Quicksilver.BankShot` | Ricochet | C | Bank Shot | Bullets ricochet {0} more time. |
| `Blessing.Quicksilver.Carom` | Ricochet | C | Carom | Each ricochet deals +{0}% damage. |
| `Blessing.Quicksilver.RelayChain` | Ricochet | K | Relay Chain | When a ricochet kills, it fires a free bullet at the next enemy. |

- Existing cards: Light Step (Dash, E), Tailwind (Dash, C), Fleet (Momentum, E), Wayfare (Momentum, C), Slipstream (Ricochet, E).
- Branch display names: Dash, Momentum, **Rebound**.
- On weapons without `Cap.Projectile`, Rebound cards read as +orbit / sweep speed (spec §3.1). The card shows that line when the player owns no projectile weapon.

## 3. Duo blessings (15 × name + desc, 1 plaque) — `ST_Blessings`
- Key: `Blessing.Duo.<PathA><PathB>`, in the order Storm, Forge, Tide, Shade, Radiance, Quicksilver.
- Plaque: `Duo.Plaque` = "Duo · {0} + {1}" (Keeper short names).

| Key | Name | Desc |
|---|---|---|
| `Blessing.Duo.StormForge` | Arc Furnace | Plasma bursts leave burning ground for {0} s. |
| `Blessing.Duo.StormTide` | Storm Surge | Conduction has no limit, and every 3rd jump Freezes. |
| `Blessing.Duo.StormShade` | Summons | Writ bolts Mark what they strike. |
| `Blessing.Duo.StormRadiance` | Blinding Arc | Flash Arc Blinds for {0} s. Blinded enemies take +{1}% from arcs. |
| `Blessing.Duo.StormQuicksilver` | Lightning Step | Dashing leaves a Shocking trail. Live Rail chains once more. |
| `Blessing.Duo.ForgeTide` | Pressure Cooker | Steam blasts Chill and Burn what they hit, so they set off more Steam. |
| `Blessing.Duo.ForgeShade` | Pyre Debt | Cremation embers count as Marked kills. |
| `Blessing.Duo.ForgeRadiance` | Sunspot | Flare detonations crit. |
| `Blessing.Duo.ForgeQuicksilver` | Afterburner | Wildfire works at any speed while you dash. +{0}% move speed near Burning enemies. |
| `Blessing.Duo.TideShade` | Cold Storage | Deep Ledger executions drop 2 Souls and Freeze enemies nearby. |
| `Blessing.Duo.TideRadiance` | Diamond Dust | Refraction shards Chill. Shards from crits Blind. |
| `Blessing.Duo.TideQuicksilver` | Glide | Water and ice never slow you. Black Ice also works off Chilled enemies. |
| `Blessing.Duo.ShadeRadiance` | Spotlight | Exposure works on every Blinded enemy, Marked or not, at +{0}%. |
| `Blessing.Duo.ShadeQuicksilver` | Fast Collection | Repossession works below {0}% HP and gives +1 dash charge for 3 s. |
| `Blessing.Duo.RadianceQuicksilver` | Lightspeed | Afterglow's crit bonus doubles. Crits refund {0}% of your dash cooldown. |

## 4. Reactions (15 × name + toast) — `ST_World` (`Reaction.<PathA><PathB>.*`)
`.Name` is shown on the first trigger in a run, in the Keeper-line slot, as "{Name}: {Toast}". The toast is the rule in a few words.

| Key prefix | Name | Toast |
|---|---|---|
| `Reaction.StormForge` | Plasma | Arcs ignite burning enemies. |
| `Reaction.StormTide` | Conduction | Arcs run through everything chilled. |
| `Reaction.StormShade` | Writ | Shocked marks call down bolts when they die. |
| `Reaction.StormRadiance` | Flash Arc | Crits on shocked enemies arc and blind. |
| `Reaction.StormQuicksilver` | Live Rail | Ricochets and dashes discharge Shock. |
| `Reaction.ForgeTide` | Steam | Fire meets ice and bursts. |
| `Reaction.ForgeShade` | Cremation | Marked burning enemies leave seeking embers. |
| `Reaction.ForgeRadiance` | Flare | Crits set off Burn at once. |
| `Reaction.ForgeQuicksilver` | Wildfire | Speed sets your bullets alight. |
| `Reaction.TideShade` | Deep Ledger | Frozen marks are executed sooner. |
| `Reaction.TideRadiance` | Refraction | Crits on ice split into shards. |
| `Reaction.TideQuicksilver` | Black Ice | Dashing through the chilled freezes them. |
| `Reaction.ShadeRadiance` | Exposure | Blinded marks take more and spread the mark. |
| `Reaction.ShadeQuicksilver` | Repossession | Dash through weak marked enemies to execute them. |
| `Reaction.RadianceQuicksilver` | Afterglow | Speed sharpens your crits. |

## 5. Omens (12 × name + desc, 2 UI) — `ST_World` (`Omen.<Id>.*`)
- Fiction: the terms the Six post for the day. Vigil reads them at the Shrine.
- UI:
  - `Omen.Label` = "Terms of the day" (Shrine row label).
  - `Omen.Banner` = "Terms of the day: {0}" (run-start banner, 3 s).

| Key prefix | Name | Desc |
|---|---|---|
| `Omen.FavoredBranch` | Favoured Work | {0} blessings are offered twice as often. ({0} = Keeper + branch, e.g. "Storm · Thunder") |
| `Omen.SecondWeapon` | Spare Fitting | Start with a second weapon: {0}. |
| `Omen.FineParts` | Fine Parts | Modifier cards are always Rare or better. |
| `Omen.OpenTerms` | Open Terms | +2 Renegotiate, −1 Hold. |
| `Omen.Bounty` | Bounty | Elite chests offer 4 cards. |
| `Omen.CrossedLines` | Crossed Lines | Blessings from outside your pair appear from level 6, more often. |
| `Omen.HeavyArms` | Heavy Arms | Weapon upgrades are Rare or better. Move speed −10%. |
| `Omen.DrySpell` | Dry Spell | XP −10%. Ichor +25%. |
| `Omen.GlassEdge` | Glass Edge | Damage +20%. Max HP −20. |
| `Omen.QuietKeepers` | Quiet Keepers | No bids this run. After level 8, blessings are one rarity higher. |
| `Omen.SwarmDay` | Swarm Day | Scrapwings +30%. XP +15%. |
| `Omen.OldAlliance` | Old Alliance | Your pair's duo blessing is offered right after level 8. |

**Rule check:** no omen reduces the 5-card hand or any GDD §5.6 rule. *Open Terms* trades a Hold for Renegotiates and keeps both tools. *Quiet Keepers* only matters in Act II (Bids).

## 6. Card UI (6) — `ST_UI`

| Key | EN | Context |
|---|---|---|
| `Card.Branch` | {0} · {1} | Branch line on blessing cards ({0} Keeper short, {1} branch name) |
| `Card.Capstone` | Capstone | Plaque add-on on capstones |
| `Card.Capstone.Needs` | Needs two {0} cards | Tooltip (not shown in offers; capstones only appear when eligible) |
| `Cross.Plaque` | Cross · {0} | Plaque on cross cards (§2.3 E4) |
| `Cross.Hint` | Outside your pair. It can wake a reaction. | Tooltip on cross cards |
| `Duo.Plaque` | Duo · {0} + {1} | Plaque on duo blessings |

---

## 7. Owner table — who wires which keys (scheduler assignment 2026-10-20)
Spec steps are from `build_depth_spec.md` §6.

| Owner (session) | Spec steps | Key ranges (this doc) | Also owns |
|---|---|---|---|
| **weapons** | 1 (capability tags + matrix + filter), 4 (weapon evolutions) | `Evolve.<Weapon>.Name/.Desc` (10 × 2), `Transform.Refund` | `Cap.*` tags on `URHWeaponDef`, `RequiredCaps` / `BlockedCaps` on `URHModifierDef`, `RogueHero.Modifiers.Compatibility`; the evolution weapon defs; the Rebound line variant on non-projectile weapons (§2, spec §3.1) |
| **tiers** (integration) | 2 (transformation framework + L16 / L24 hands + sealed card logic), 6 (offer extensions E1–E6 + omens) | `Omen.<Id>.Name/.Desc` (12 × 2), the logic behind `Cross.*` and `Transform.Kind.*` | `URHTransformDef`, `URHOmenDef`, offer-generator weights (branch momentum, capstone gate, cross cards, duo-card entry, run memory) |
| **statuses** | 3 (Path transformations) | `Transform.<Path>.Name/.Desc` (6 × 2) and the Slack Water keys (`Blessing.Tide.Legendary2.*`, `act2_text.md` §12.2; to be re-keyed as `Transform.Tide2.*`, see the rename note below) | Shock stacking (Overload), slag turrets (Kiln), ice pillars + enemy-bullet blocking (Glacier), orbiting Shades (Collection), refracted lances (Prism), dash volleys (Vector) |
| **balance** | 5 (branches + 42 new blessings) | `Branch.<Path>.<Branch>.Name` (18), `Blessing.<Path>.<NewKey>.Name/.Desc` (42 × 2) | The branch tags on all 72 blessings (30 existing + 42 new), entry / core / capstone roles, the capstone Discoveries (6), the bot measurement of branch divergence (spec §6 step 10) |
| **hub** | 7 (duo reactions), 8 (duo blessings) | `Reaction.<PathA><PathB>.Name/.Toast` (15 × 2), `Blessing.Duo.<PathA><PathB>.Name/.Desc` (15 × 2), the logic behind `Duo.Plaque` | `URHReactionDef`, the reaction effects + VFX (R2 colours, R4 limits), the first-trigger toast event |
| **meta-ui** | Contracts re-point (spec §2.2), L16 hand UI, sealed card, omen UI | `Transform.Hand.Title`, `Transform.Kind.Path/.Weapon/.Fusion`, `Transform.Sealed`, `Omen.Label`, `Omen.Banner`, `Card.Branch`, `Card.Capstone`, `Card.Capstone.Needs`, `Cross.Plaque`, `Cross.Hint`, `Duo.Plaque` | Contracts nodes (3) / (4) → branch cores (`ST_Tree` text update); hand layout for 3 transformation cards + the sealed 4th; the Shrine omen row and run-start banner; the branch line and plaques on cards |
| **loc-zh** | 9 | Every key in this doc | Keep 燃烧 (the Burn status) distinct from *Halo Ring*'s "burns" (§2 note); branch display names; the omen fiction ("terms of the day") |

**Rename note for statuses / tiers:** with D-33 (Path transformations replace the stat Legendaries), *Slack Water* is a **Tide Path transformation**, not a Legendary. New keys: `Transform.Tide2.Name` / `.Desc`; the L16 subtitle `Blessing.Tide.Legendary2.Choice` is retired, because the hand already shows a choice of 3. Tide's two transformations both qualify for its hand; at most one of them is dealt per hand. The old `Blessing.<Path>.Legendary.*` keys are kept until the transformation hand lands (fallback), then removed.

**Carried over from v0.2 (not build depth):** the Bid Term rename **Lien → Levy** (`Bid.Term.Levy.*`, `Summary.Levy`) belongs to whoever wires Bids (v0.2 step 7; tiers by default).
