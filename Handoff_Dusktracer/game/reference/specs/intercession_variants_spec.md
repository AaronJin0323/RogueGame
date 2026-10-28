# Intercession variants — spec — FINAL (2026-10-21, scheduler decision: no designer input arrived from the mechanics session, so the ⟨M⟩ points stand as drafted, which is how they are implemented)

Source: playtest 6 (`Docs/Playtests/2026-10-20_human_6.md`) #5, quoting the designer: *the archetype should also change the Intercession, and the second Keeper should affect it too.* Scheduler decision #5: CORE.

- Owners: **weapons** (effects), **statuses** (enemy side). Text: **loc-zh**, **meta-ui** (HUD).
- Builds on `keeper_identity_spec.md` §3, where each Keeper's base Intercession is reworked toward its identity.
- **Status: final; revised 2026-10-21 by mechanics decision list #2** (three layers, 30 pair layers, ±20 % of the mean). It is final only after the mechanics session's decision list (via the scheduler). Points that list may change are marked ⟨M⟩.

## 1. Rules — CORE
1. **One variant per archetype** (20). The **patron Keeper's** archetypes pick the variant: the Intercession is always the patron's.
2. **Which variant** (confirmed as drafted): the patron archetype with the **most cards owned, key required**. Ties go to the most recent card; no top-two combining. With no patron key owned, the **base** Intercession plays. It is re-evaluated on each pick.
   - **HUD (D-68):** the ult ring shows **"{Ult} · {Archetype} + {Layer}"**, with a hover note listing the three layers.
   - **VFX (D-68):** the second Keeper's hue is a second tail layer (Art §3.2), never red or magenta.
3. **Three layers (D-64, designer-confirmed 2026-10-21, mechanics decision list #2):**
   - **Base** (6; `keeper_identity_spec.md` §3);
   - **archetype variant** (20, changes the shape; §2);
   - **second-Keeper layer** (30, **one unique mechanic per ordered pair** patron → second; §2.1).

   20 variants × 5 seconds = **100 distinct combinations** from 50 authored parts.
   - **The layer** (D-65) is active from the **L8 commitment**; before L8, only the base + variant. It replaces the draft rule "every hit carries the second Keeper's status".
   - It acts on the **variant's output shape** (e.g. Tide · Whirlpool's single point + a Storm second = current runs through the frozen ball).
   - If a layer conflicts with a variant, **the variant's shape wins** and the layer applies on top.
   - Duo reactions still fire normally on the layer's hits.
4. **Strength (D-66; designer decision 2026-10-26):** variants and layers may differ, but every combination of a patron (its archetype variants × its 5 second Keepers) stays within **±20 % of that patron's own mean**. The metric is **marginal damage: with-cast minus no-cast, on a refilled field** [GATED]. Burst and control Intercessions cannot be compared by damage across Keepers; the measured spread is about 1300×. This replaces the draft's "±15 % of base, same total damage". The ≤ 1 full-frame excess per 60 s rule (GDD §3.5, Art §3) is unchanged.
   - Designer decision 2026-10-26: amplifier variants are valued by what they add to the hero's own damage (the with-cast minus no-cast metric stays). Weak variants are retuned toward their Keeper's mean in steps of at most x2 per pass.
   - Designer decision 2026-10-27: over-strong variants are also tuned down toward their Keeper's mean, at most x0.5 per pass (weak ones up at most x2 per pass, 2026-10-26).
5. **Refusal run:** only Tidal Wave, the Reclaimer's own (`v03_refusal_spec.md` §3). It uses the base Tidal Wave with no variant and no second Keeper.
6. **Data:** `URHIntercessionVariantDef` (`Archetype` tag, effect class or `FRHWeaponBehaviourMod` overrides, VFX overrides). The base Intercession effect reads its variant at cast time. Tests: variant selection (most cards, tie → latest), a duo reaction fires during the Intercession with the second Keeper committed, every combination's marginal damage (with-cast minus no-cast) within ±20 % of its patron's own combination mean on a refilled field (D-66; designer decision 2026-10-26) [GATED], the 5 second-Keeper layers of each variant measure pairwise not identical totals (relative difference > 1e-4). Pairs under 0.5 % are reported as weak-layer balance findings.

## 2. The 20 variants — CORE (shape), GATED (numbers)
Base Intercessions are from `keeper_identity_spec.md` §3.

| Keeper · base | Archetype | Variant |
|---|---|---|
| **Storm · Thunderhead** (6 s: rounds split in 3 and arc; bolts across the screen) | Tempest | Rounds split into **5-round fans, and the children split again** (two generations) |
| | Stormwall | All rounds fired in the 6 s **orbit you as a storm ring**, then the ring **bursts outward at once** at the end |
| | Arc Grid | Every 1 s a **lightning net** links every enemy on screen to its 3 nearest (chain arcs) |
| | Downpour | Every impact point is struck **twice more from the sky** (Echo as an impact strike, D-63) |
| **Forge · Forge Nova** (5 s: a fire slam ring every 0.5 s) | Hammer | Each ring is a **360° cleave** with double reach, knocking back |
| | Furnace | Rings leave **burning ground for 8 s**, and Burn stacks to its cap |
| | Slam | Dash cooldown is reset and **every dash lands a meteor slam** (r 500) |
| **Tide · Tidal Wave** (pull every enemy into a line, Freeze 3 s) | Whirlpool | Everything is pulled into **one point** (a vortex), then Frozen |
| | Breakwater | The wave **herds** every enemy into a dense wall front ahead of you; collisions crush (D-63 herding); then Chill |
| | Frost | **Everything on screen Freezes 4 s**; Frozen enemies take +50 % |
| | Stillness | Plants a **tide pool** (r 500) around you for 6 s. Inside it you count as still even while moving, every volley is charged and Freezes, and time slows to 70 % |
| **Shade · Closing** (6 s: all Marked, rounds hunt, the weak executed) | Hound | Rounds seek with **unlimited turn rate and pierce 2** |
| | Collectors | **6 hunters** rise at once with double life, copying your **highest-level** weapon (D-63) |
| | Economy (Ledger) | **Instant payday ×3** from current Debt, and the execute threshold is 40 % |
| **Radiance · Sun Lance** (charge, then a 2 s beam that always crits and novas on overkill) | Lance | **Three lances in succession**, each aimed at the densest line |
| | Prism | The beam **splits into a fan of 5** |
| | Solar | The beam **detonates a sun nova every 0.4 s** along its length |
| **Quicksilver · Time Dilation** (4 s slow-mo, dashes fire volleys) | Vector | Dash cooldown 0.2 s: **dashes are unlimited** |
| | Momentum | Momentum stacks jump to **max (30)** and don't decay for the duration; every 100 uu you move releases a pressure wave |
| | Rebound | Every round **ricochets 3 times, seeking** |

### 2.1 The 30 second-Keeper layers (D-65; designer-confirmed 2026-10-21, mechanics decision list #2)
Rows are the patron's Intercession, columns the second Keeper. **The names are text** (FText; working names from the mechanics session, five renamed by planning where they clashed with existing names, marked ✎). Numbers are GATED.

| Patron · base | Second | Layer | Mechanic |
|---|---|---|---|
| Storm · Thunderhead | Forge | **Thunder Anvil** | Each bolt lands a fire slam ring |
| | Tide | **Ice Lattice** | Bolts freeze their spot; arcs jump between all Frozen enemies |
| | Shade | **Debt Bolt** | Bolts strike Marked enemies first; bolt kills pass the Mark to neighbours |
| | Radiance | **Stored Sky** | Every bolt charges the sky; at the end one giant bolt strikes the densest point, scaled by the bolt count |
| | Quicksilver | **Chasing Storm** | Each dash calls a line of bolts along its path; dash charges refill |
| Forge · Forge Nova | Storm | **Live Ring** | Ring hits chain to 2 enemies beyond the ring |
| | Tide | **Fire and Frost** ✎ (was "Bellows", a blessing key) | Rings alternate fire (push out) and frost (pull in), hammering the crowd |
| | Shade | **Cinder Hounds** | Each ring kill releases a cinder hound that runs at the nearest enemy and explodes |
| | Radiance | **White Heat** | Every 3rd ring is white-hot: always crits, overkill novas |
| | Quicksilver | **Walking Anvil** | Rings follow you; dash start and landing each burst a ring; the ring interval halves while moving |
| Tide · Tidal Wave | Storm | **Conductor** | Current runs the length of the frozen line every 1 s |
| | Forge | **Thermal Shock** ✎ (was "Quench", a blessing name) | When it thaws, the whole line bursts in a steam explosion |
| | Shade | **Drowning List** | The line is Marked, rounds seek it, and anything below 30 % at thaw is executed |
| | Radiance | **Ice Lens** | Your rounds passing through the line become charged (×2, pierce) |
| | Quicksilver | **Surf** | Dashing along the line shatters every Frozen enemy you pass; every 10 shattered refunds a dash |
| Shade · Closing | Storm | **Joint Liability** | Each execution arcs to the 3 nearest (Mark + Shock), chaining executions |
| | Forge | **Burning Ledger** | Marked enemies Burn and explode when executed |
| | Tide | **Custody** | Marked enemies are slowly dragged toward you and Chilled; execute threshold +10 % on Frozen |
| | Radiance | **Audit Light** | At the end each Marked enemy takes a lance equal to its missing HP (always crits, overkill nova) instead of the threshold execute |
| | Quicksilver | **Collection Run** ✎ (was "Repossession", a duo reaction) | Dashing through a Marked enemy below 50 % executes it and refunds the dash |
| Radiance · Sun Lance | Storm | **Branching** | Lightning branches off the beam every 0.25 s |
| | Forge | **Molten Trench** | The beam leaves a 6 s burning trench |
| | Tide | **Cold Light** | The beam freezes as it passes and shatters everything frozen at its end |
| | Shade | **Bending Light** | The beam bends toward Marked enemies and elites |
| | Quicksilver | **Carried Sun** | The beam's origin moves with you (sweep it by moving); dashes extend its duration |
| Quicksilver · Time Dilation | Storm | **Static Wake** | Each dash leaves static; all of it discharges when the slow-mo ends |
| | Forge | **Drop Hammer** | Each dash landing slams a fire ring |
| | Tide | **Freeze Frame** | Enemies the dash path touches freeze, and all of them shatter when the slow-mo ends |
| | Shade | **Phantom Gunners** ✎ (was "Afterimages", clashes with Radiance's *Afterimage*) | Each dash leaves an afterimage that fires your weapons for the rest of the Intercession |
| | Radiance | **Long Exposure** ✎ (was "Exposure", a duo reaction) | At the end every dash line replays as a light lance |

## 3. Text (EN; zh by loc-zh)
- **Names are composed:** `Ult.Variant.Format` = "{0} · {1} + {2}" (Ult · Archetype + Layer). Before L8 it is "{0} · {1}".
- **20 variant one-liners:** `Ult.<Path>.<Archetype>.Desc`, ≤ 90 chars EN / ≤ 40 ZH. The draft lines are §2's "Variant" column.
- **30 layers:** `Ult.<Path>.Layer.<SecondPath>.Name` (the §2.1 name) and `.Desc` (the §2.1 mechanic, shortened to ≤ 90 EN / ≤ 40 ZH by loc-zh). That is 60 strings.
- **The HUD hover note:** `Ult.LayersNote` = "Base: {0}\nShape: {1}\nSecond Keeper: {2}".

## 4. Routing by file ownership (each file in one row; D-64 … D-68)

| # | Work | Owner | Files |
|---|---|---|---|
| I1 | Variant selection (as drafted), the 5 variant fixes (D-67), the 30 layer effects, layer gating at L8, conflict rule (the variant's shape wins) | weapons | `Build/Ultimates/*` (`RHIntercessionVariant.*`, `RHUltimate_Paths.*`, `RHUltimate.*`, `RHUltimateFX.*`, `RHUltimateSubsystem.*`) |
| I2 | Enemy side: Custody drag, crush on Breakwater herding (shares D-63's crush), Freeze Frame, the frozen line for Conductor / Surf / Ice Lens queries, the tide-pool slow | statuses | `Enemies/*` |
| I3 | Momentum variant: stacks to max and no decay; Economy payday hooks | balance | `Build/Effects/Branches/RHBranches.*` (`RHBranchHooks`) |
| I4 | Duo reactions keep firing on layer hits (verification + any trigger tag the layers need) | hub | `Build/Reactions/*` |
| I5 | The ult ring label "{Ult} · {Archetype} + {Layer}" and its hover note | meta-ui | `UI/RHUltimateMeter.*` |
| I6 | Text: 60 layer strings + 20 variant lines + format strings, en + zh | loc-zh | `Content/Localization/zh_overrides.csv` + the EN rows in `Content/RH/Text/ST_Ultimates.csv` (loc-zh writes both, from §2 / §2.1) |
| I7 | Strength gate: each patron's combinations within ±20 % of that patron's own mean (D-66) on a refilled field | balance | `Tools/Balance/*`, `Docs/Balance/*` |
