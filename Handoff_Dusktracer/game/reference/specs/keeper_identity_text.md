# Keeper identity — card text, EN-first (draft 2026-10-20)

This is the text for `keeper_identity_spec.md`. Rules follow `act2_text.md` §0:
- names ≤ 18 chars EN / ≤ 6 ZH;
- rules ≤ 90 chars EN / ≤ 40 ZH;
- numbers are format args from data;
- zh is written by the loc session from context.

**Key policy:** existing keys are kept (D-39). Where an effect changed so much that the old name misleads, the **display name changes** and the key does not. Those renames are marked ✎.

**Owners:**
- balance: blessing / trait / tier / imbue defs.
- statuses: the transformation reworks.
- meta-ui: UI strings.
- loc-zh: zh.

**Count:**

| Section | Strings |
|---|---|
| §1 Archetype names | 20 |
| §2 Traits | 12 |
| §3 Reworked blessings | 49 descs + 5 names |
| §4 Intercessions | 6 descs |
| §5 Imbue | 4 lines |
| §6 Transformation reworks | 5 |
| §7 UI | 6 |
| **Total** | **≈ 107 EN strings** |

---

## 1. Archetype names (20) — `ST_Blessings`, `Branch.<Path>.<Archetype>.Name`
These replace build_depth's branch names. The keys take the archetype id; old `Branch.*` keys are retired.

| Path | Archetypes (key id → name) |
|---|---|
| Storm | `Tempest` Tempest · `Stormwall` Stormwall · `ArcGrid` Arc Grid · `Downpour` Downpour |
| Forge | `Hammer` Hammer · `Furnace` Furnace · `Slam` Slam |
| Tide | `Whirlpool` Whirlpool · `Breakwater` Breakwater · `Frost` Frost · `Stillness` Stillness |
| Shade | `Hound` Hound · `Hunters` Collectors (key `Branch.Shade.Hunters`: `RogueHero.Text.FictionNotKeys` rejects "Collectors" because it is part of a fiction name; balance a703c97. The display FText stays "Collectors") · `Economy` Ledger (key `Branch.Shade.Economy`: **LEDGER is Shade's designation and may not be a key**; `RogueHero.Text.FictionNotKeys` rejects it. The display FText stays "Ledger" / 账簿) |
| Radiance | `Lance` Lance · `Prism` Prism · `Solar` Solar |
| Quicksilver | `Vector` Vector · `Momentum` Momentum · `Rebound` Rebound |

## 2. Patron traits (6 × name + desc) — `ST_Keepers`, `Keeper.<Path>.Trait.Name/.Desc`
Shown on the Shrine portrait and in the build screen while the Keeper is the patron.

| Path | Name | Desc |
|---|---|---|
| Storm | Static Sky | Every {0}th volley of each weapon fires twice. |
| Forge | Close Work | +{0}% damage to enemies within {1} m. |
| Tide | Cold Hands | Hits slow by {0}%, up to {1}%. Stand still 1 s for +{2}% damage. |
| Shade | Bookkeeping | Your first weapon's shots bend toward Marked enemies, and {0}% of its hits Mark. |
| Radiance | Clarity | Every {0}th shot of each weapon is charged for ×{1} damage. |
| Quicksilver | Restless | +{0}% move and attack speed while moving. −{1}% after standing still. |

## 3. Reworked blessings — `ST_Blessings` `.Desc` (and `.Name` where ✎)
Kept blessings (spec §8, *Keep*) keep their current text.

### Storm (12)

| Key | Name | Desc |
|---|---|---|
| ForkedPath | Forked Path | {0}% of shots split into 3 on hit or at the end of their flight. |
| Overcharge | Overcharge | Split shots make {0} more child. Children deal +{1}%. |
| Stormfront | Stormfront | Children split once more. |
| StaticFocus ✎ | **Eye Wall** | Your shots circle you for {0} s, then burst outward. |
| LowCeiling | Low Ceiling | {0} more shots join the circle each volley. |
| GridLock | Grid Lock | Circling shots arc to enemies that cross the ring. |
| LiveWire | Live Wire | Hits arc to {0} more enemies for {1}% damage. |
| GroundFault | Ground Fault | Arcs Shock. Shocked enemies take +{0}% from arcs. |
| Supercell | Supercell | An arc that kills forks into 2 new arcs. |
| Thunderclap | Thunderclap | Each volley falls again from the sky as bolts, {0} s later, for {1}%. |
| RollingThunder | Rolling Thunder | Sky bolts leave a field that Shocks for 1 s. |
| Capacitor | Capacitor | Every {0}th sky volley strikes the whole screen. |

### Forge (9 reworked)

| Key | Name | Desc |
|---|---|---|
| Bellows ✎ | **Hammer Hand** | The first {0} shots of each volley become a cleave around you for ×{1} damage. |
| TemperedPlating | Tempered Plating | +{0} Armor. Your cleaves knock enemies back. |
| BlastFurnace | Blast Furnace | Cleaves and explosions are {0}% larger. |
| Masterwork | Masterwork | Your first weapon gains a 4th socket. Modifiers on cleaving weapons are {0}% stronger. |
| Kindling | Kindling | {0}% of hits Burn. Close hits leave burning ground. |
| EmberTrail | Ember Trail | Your dash ends in a slam that hits everything within {0} m, and leaves fire behind. |
| Quench | Quench | When you are hit, your next slam deals double damage. |
| SparkShower | Spark Shower | Slams throw {0} embers that Burn what they hit. |
| CinderStorm | Cinder Storm | Slams set off every Burning enemy in range. |

### Tide (9 reworked)

| Key | Name | Desc |
|---|---|---|
| Undertow | Undertow | Hits pull enemies {0} m toward the impact and Chill them. |
| Shatterpoint | Shatterpoint | When {0} or more pulled enemies touch, they Freeze together and shatter. |
| ColdFront | Cold Front | Every {0}th volley becomes a wave that pushes enemies back. |
| Riptide | Riptide | Every hit pushes enemies back {0} m. |
| Brittle | Brittle | Two Chills Freeze. Frozen enemies take +{0}% damage. |
| Breakwater | Breakwater | Frozen enemies block other enemies like walls. |
| DeepReserve ✎ | **Permafrost** | Freeze lasts until the enemy is hit, up to {0} enemies at once. |
| HighWater ✎ | **Deep Calm** | Stand still for 1 s: your next volley deals ×{0} damage and Freezes. Breaks when you move. |
| Wellspring | Wellspring | Regenerate {0} HP per second, tripled while you stand still. |

Spring Tide gains a line: `Blessing.Tide.SpringTide.Desc` = "Healing past full HP becomes a shield, up to {0}. When it breaks, it Freezes nearby enemies."

### Shade (8 reworked)

| Key | Name | Desc |
|---|---|---|
| Lien | Lien | Your shots bend toward Marked enemies, or the nearest if none. Hits Mark. |
| Foreclosure | Foreclosure | Shots bend harder toward Marked enemies. Marks last {0} s longer. |
| FinalNotice | Final Notice | Bending shots execute Marked enemies {0}% sooner. |
| Dividend | Dividend | Marked enemies that die rise as Shades for {0} s, firing your first weapon at {1}%. |
| Interest | Interest | Each Marked kill adds 1 Debt. At {0} Debt, your next volley hunts every enemy on screen. |
| LongArm | Long Arm | XP gems fly to you. Elites add 1 more Debt. |
| Compound | Compound | Payday volleys pierce every enemy. |
| Audit | Audit | Payday also executes Marked enemies below {0}% HP. |

### Radiance (5 reworked)

| Key | Name | Desc |
|---|---|---|
| PerfectSight | Perfect Sight | Every {0}th shot of each weapon is a charged lance: ×{1} damage, pierces everything. |
| Glint | Glint | Charged shots always crit. |
| Flashpoint | Flashpoint | Crits split into 3 lances of light for {0}% damage. |
| Halo ✎ | **Radiant Burst** | Overkill damage bursts in light around the target, r {0} m. |
| HaloRing | Halo Ring | A ring of light surrounds you. Enemies dying inside it burst twice as hard. |

### Quicksilver (6 reworked)

| Key | Name | Desc |
|---|---|---|
| BlinkStrike | Blink Strike | Dashing fires every weapon along the dash line. |
| SecondWind | Second Wind | Each kill by a dash volley refunds {0}% of your dash cooldown. |
| Fleet | Fleet | +{0}% damage for every 10% bonus move speed. |
| Drafting | Drafting | Bonus move speed also raises fire rate by {0}% per 10%. |
| TerminalVelocity | Terminal Velocity | Above {0}% move speed, shots pierce one more enemy and fire twice. |
| Slipstream | Slipstream | Shots ricochet {0} more time, and bounces find a new target. |

## 4. Intercessions (6 descs) — `ST_Ultimates` `Ult.<Path>.Desc`
Names are kept.

| Path | Desc |
|---|---|
| Storm | For {0} s every shot you fire splits into 3, each child arcs, and bolts strike across the screen. |
| Forge | For {0} s you are the hammer: a ring of fire slams out around you every half second. |
| Tide | A wave pulls every enemy on screen into a line before you and Freezes them for {0} s. |
| Shade | For {0} s every enemy is Marked and every shot hunts. Then the weak are executed. |
| Radiance | Charge, then a beam for {0} s that always crits and bursts on overkill. |
| Quicksilver | For {0} s the world slows to 30%, and every dash fires all your weapons. |

## 5. Imbue lines (4 reworked) — `ST_Blessings` `Imbue.<Path>.Rules`
Radiance and Quicksilver keep theirs. The Storm and Tide lines were shortened in the build (integration b4b5cbc) so imbue + card text stay within the **140-character card limit** (e.g. Grid Lock, King Tide); this doc matches the build.

| Path | Rules |
|---|---|
| Storm | All shots: {0}% split in 2 (+{1}%/Favor); some hits arc. |
| Forge | All shots: close hits cleave for {0}%; {1}% of hits Burn. |
| Tide | All shots: {0}% of hits Chill (+{1}%/Favor) and push slightly. |
| Shade | All shots: {0}% of hits Mark (+{1}%/Favor), and shots lean toward Marked enemies. |

## 6. Path transformation reworks (D-41) — `ST_Fusions`

| Key | Name | Desc |
|---|---|---|
| `Transform.Forge.Desc` | Kiln (kept) | Enemies that die Burning become slag husks for {0} s that cleave anything near them. |
| `Transform.Radiance.Name` | **Zenith** ✎ | (renamed from Prism) |
| `Transform.Radiance.Desc` | — | Charged shots split into 3 charged lances, and every overkill bursts in light. |
| `Transform.Quicksilver.Name` | **Slipstream Drive** ✎ | (renamed from Vector) |
| `Transform.Quicksilver.Desc` | — | +1 dash charge. Dashes fire all weapons, and your power keeps rising with speed, without a cap. |

## 7. UI (6) — `ST_UI`

| Key | EN | Context |
|---|---|---|
| `Card.Archetype` | {0} · {1} | Plaque: Keeper short · archetype (replaces `Card.Branch`) |
| `Card.Key` | Key | Mark on archetype key cards ("this changes how you fight") |
| `Card.NeedsKey` | Needs {0} | Tooltip on amplifiers whose key isn't owned (amplifiers are not offered without it; shown only in the Codex) |
| `Card.BreaksOnMove` | Breaks when you move | Warning line on Stillness cards |
| `Build.Load` | Load {0}× | Build-screen readout: the weapon slot factor total (D-42) |
| `Shrine.Trait` | Patron trait: {0} | Shrine detail line |

---

## 8. Archetype balance pass — card text changes (EN; 2026-10-21, spec §3.7; owner balance for EN, loc-zh for zh)
Only the cards whose rules changed. Numbers are format args (GATED).

| Key | Name | New rules text |
|---|---|---|
| `Blessing.Storm.StaticFocus` | Eye Wall | Your shots circle you for {0} s, then burst outward with +{1}% damage and 1 more pierce. |
| `Blessing.Storm.LiveWire` | Live Wire | Hits arc to {0} more enemies for {1}%. With none near, the arc strikes the same target again. |
| `Blessing.Storm.Thunderclap` | Thunderclap | {0} s after each hit, a bolt strikes the same spot for {1}%. |
| `Blessing.Forge.Masterwork` | Masterwork | Your first weapon gains a 4th socket. Cleaves carry every socketed modifier's on-hit effect. |
| `Blessing.Forge.Kindling` | Kindling | {0}% of hits Burn. Close hits leave burning ground, where Burn ticks faster. |
| `Blessing.Forge.Conflagration` | Conflagration | Each second, and on each kill, Burning enemies spread Burn to a neighbour. |
| `Blessing.Forge.EmberTrail` | Ember Trail | Your dash landing, every 6th melee hit and every hit you take set off a slam within {0} m. |
| `Blessing.Tide.Undertow` | Undertow | Hits pull enemies {0} m toward the impact and Chill them. Enemies pulled into each other take crush damage. |
| `Blessing.Tide.ColdFront` | Cold Front | Every 3rd volley becomes a wave that herds enemies into a front. |
| `Blessing.Tide.Riptide` | Riptide | Pushed enemies that hit a wall, ice or each other take {0}% crush damage. |
| `Blessing.Tide.HighWater` | Deep Calm | Hold almost still for 0.6 s: every weapon's next volley deals ×{0} and Freezes. Dashing keeps the charge. |
| `Blessing.Shade.Lien` | Lien | Your shots bend toward Marked enemies, pierce one more, and Mark what they hit. |
| `Blessing.Shade.Dividend` | Dividend | Marked kills and elite kills raise a Shade that fires your best weapon at {0}%, modifiers included. |
| `Blessing.Shade.Arrears` | Arrears | Every {0} Souls raise a lasting Shade that fires at {1}%. |
| `Blessing.Radiance.Flashpoint` | Flashpoint | Crits split into 3 lances of light for {0}%. Lances don't split again. |
| `Blessing.Radiance.HaloRing` | Halo Ring | A ring of light surrounds you. Damage to elites and bosses charges a sun that bursts on them every 3 s. |
| `Blessing.Quicksilver.BlinkStrike` | Blink Strike | Dashing fires every weapon along the dash line at ×{0}. Each kill refunds {1} s of dash cooldown. |
| `Blessing.Quicksilver.SecondWind` | Second Wind | Every 3rd dash costs no charge. |
| `Blessing.Quicksilver.Fleet` | Fleet | Moving builds Momentum, up to 30. Each point: +{0}% damage and fire rate. Getting hit loses 10. |
| `Blessing.Quicksilver.Drafting` | Drafting | Dashing adds {0} Momentum. Momentum fades slower. |
| `Blessing.Quicksilver.TerminalVelocity` | Terminal Velocity | At 30 Momentum, shots pierce one more enemy and echo once. |
| `Blessing.Quicksilver.Slipstream` | Slipstream | Shots ricochet {0} more time and find a new target, or return to the last one. |

**New status term: Cracked / 脆化.** Bosses can't be Frozen or Blinded; those effects crack them instead (+20 % damage taken for 2 s). It needs a status-name row (statuses + loc-zh) and the `Cracked` keyword (`keyword_notes_spec.md`). **New keyword `Momentum`** (54 in total).
