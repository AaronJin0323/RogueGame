# Elites — affixes, placement, readable parts (S2)

## Intent
An elite is a named skill check inside a Threat beat: the same role the player already reads, more HP, and one or two affixes that each ask for a different answer (focus it, step out of the ring, break the cable, strip the shield with something other than density). Power with teeth, never an HP wall (`00_Vision §2.1`): every affix either changes what the elite does or changes what kills it, and each is visible before it matters. `enemies.md` owns the pool and the roles; this spec owns what an elite adds to them. `director.md` owns when elites come.

## Rules
### What an elite is
- **R-ELT-01** An elite is an enemy with `bElite`: `C.ROLE.ELITE_HP_MULT` times its scaled HP (`enemies.md` R-ENM-09) and an affix mask kept for life (`FDTEnemySpawn::AffixMask`, one bit per row of `09_Catalog/elites.csv`). An affix on a non-elite spawn is ignored. Otherwise it is its role: same radius, same state machine, same telegraphs, same attacks.
- **R-ELT-02** Rolling: an `EliteWave` row draws `event_affixes` distinct affixes per elite, weighted, from the stream `EDTRngStream::Elite`, appended so no earlier stream moves (R-TEC-02). Each affix weighs one; a row marked `favored` in `elites.csv` weighs the wave's `event_favor` instead (empty cell: one). A role listed in an affix's `no_roles` cannot draw it (Swift on a Turret would be a light that promises nothing).
- **R-ELT-03** Armored: every hit on it, from any source, is multiplied by `C.AFFIX.ARMORED_DAMAGE_TAKEN` in `ResolveHits`, after a Tank's plate and before Shock.
- **R-ELT-04** Swift: its move speed is `C.AFFIX.SWIFT_SPEED_MULT` times its role's. A Rusher's charge keeps its role speed, so its band stays the charge (R-ENM-11).
- **R-ELT-05** Volatile: on death, whatever killed it, a filled `Ring` telegraph of radius `C.AFFIX.VOLATILE_RADIUS_UU` stands at the death position for `C.AFFIX.VOLATILE_LEAD_S`, then every hero whose body touches it takes `C.AFFIX.VOLATILE_DAMAGE`, unscaled (`EDTHeroHitKind::Blast`, attack `Affix.Volatile`; i-frames and dash protect as for any hit, `combat_core.md`). The telegraph is one threat action. It resolves even if the arena moves on; `DT.Spawn.Clear` removes it.
- **R-ELT-06** Splitter: on death, `C.AFFIX.SPLITTER_COUNT` `Swarm` enemies appear around the death position at `C.ENM.SEPARATION_RADIUS_UU`, evenly spaced, at the start of the next Enemies phase, so no later hit of the step lands on a reused slot and telemetry's slot mirror sees them as spawns. Each has the elite's `HpScale` and `DmgScale`, is not elite and has no beat tag. It keeps the elite's `Director` flag, so debug elites still drop nothing. At the cap they are refused and counted (R-ENM-01).
- **R-ELT-07** Vampiric: with a living hero within `C.AFFIX.VAMPIRIC_RANGE_UU` of its centre, a cable (`AimLine` telegraph from the elite to that hero, width `C.AFFIX.VAMPIRIC_CABLE_UU`) connects for `C.AFFIX.VAMPIRIC_LINK_S`, one threat action; the hero leaving range or the target changing resets it. Then, at the link's end and every `C.AFFIX.VAMPIRIC_TICK_S` while the hero stays in range, one drain appends `EDTHeroHitKind::Drain` (attack `Affix.Vampiric`) of `C.AFFIX.VAMPIRIC_DRAIN_DPS` times the tick times `DmgScale`, and heals the elite `C.AFFIX.VAMPIRIC_HEAL_RATIO` of its max HP, never above it. A hero flagged intangible neither takes the drain nor feeds the heal. A frozen elite holds its timers (R-ENM-22). The cable stays on the ground for as long as it drains.
- **R-ELT-08** Shielded: the first `C.AFFIX.SHIELD_HITS` hits from a weapon (`FDTHit::WeaponSlot` set) deal nothing and are counted, one each however large; then the shield is gone for good. Burn, fields and other hits with no weapon slot pass through it from the start (density alone does not kill it; a status does). An absorbed hit still reports its `Hit` event, of zero, so telemetry's hit order holds (R-ENM-05).
- **R-ELT-09** Everything an affix does is deterministic on the fixed step and uses no randomness outside R-ELT-02.

### Reading it
- **R-ELT-10** Light (`04_Presentation` R-PRE-04): an elite's optic is `Pal.Elite.Light`, in the same place as the role's optic, larger with the hull. Its emissive runs between `C.ENEMIES.OPTIC_EMISSIVE` and `C.ELITE.LIGHT_EMISSIVE` along the `light` pattern of its lead affix, one digit per `C.ELITE.BLINK_STEP_S`: it dims, it never goes out, and density never dims it. The lead affix is the one with the highest `danger`. An elite with no affix holds the top of the range.
- **R-ELT-11** Parts: an elite is drawn at `C.ELITE.DRAW_SCALE` of its role's size (its hit circle is unchanged: a body that looks larger than it hits errs toward the player) and carries one attached part set per affix, never a swapped model, sized from `C.ELITE.PART_RATIO`. Armored: a plate on each side. Swift: two swept vanes at the rear. Splitter: `C.AFFIX.SPLITTER_COUNT` docked drones at the rear. These three are `Pal.Enemy.Body`: they change the silhouette. Volatile: a reactor at the rear. Vampiric: a spike at the nose. Shielded: up to `C.AFFIX.SHIELD_PLATES` plates orbiting at `C.ELITE.SHIELD_ORBIT_DEG_S`, the count following the hits left. These three are lit `Pal.Elite.Light`, at the elite's light emissive.
- **R-ELT-12** Label: at most `C.ELITE.LABELS_MAX` elites on screen carry their affix names under the hull, lead affix first, in `Pal.UI.Text` from `ST_Enemies` (FText only). The label is for learning: the light and the parts carry the read in a fight, and the label carries no number.

### Measuring it
- **R-ELT-13** One `elite` record per elite death (§Telemetry). `hero_damaged` already names `Blast` and `Drain`.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| HP factor | C.ROLE.ELITE_HP_MULT | MEASURED | R-ELT-01 |
| XP factor | C.DROP.XP_ELITE_MULT | GATED(S1) | `director.md` R-DIR-12 |
| Armored, Swift | C.AFFIX.ARMORED_DAMAGE_TAKEN, C.AFFIX.SWIFT_SPEED_MULT | GATED(S2) | R-ELT-03, R-ELT-04 |
| Volatile | C.AFFIX.VOLATILE_RADIUS_UU, C.AFFIX.VOLATILE_LEAD_S, C.AFFIX.VOLATILE_DAMAGE | GATED(S2) | R-ELT-05, T-ELT-05 |
| Splitter | C.AFFIX.SPLITTER_COUNT | GATED(S2) | R-ELT-06, T-ELT-06 |
| Vampiric | C.AFFIX.VAMPIRIC_RANGE_UU, C.AFFIX.VAMPIRIC_LINK_S, C.AFFIX.VAMPIRIC_TICK_S, C.AFFIX.VAMPIRIC_DRAIN_DPS, C.AFFIX.VAMPIRIC_HEAL_RATIO, C.AFFIX.VAMPIRIC_CABLE_UU | GATED(S2) | R-ELT-07, T-ELT-07 |
| Shielded | C.AFFIX.SHIELD_HITS, C.AFFIX.SHIELD_PLATES | GATED(S2) | R-ELT-08, R-ELT-11 |
| Favoured weight | C.ELITE.FAVORED_WEIGHT | GATED(S2) | R-ELT-02, `timeline_arena1.csv` |
| Look | C.ELITE.DRAW_SCALE, C.ELITE.LIGHT_EMISSIVE, C.ELITE.BLINK_STEP_S, C.ELITE.PART_RATIO, C.ELITE.SHIELD_ORBIT_DEG_S, C.ELITE.LABELS_MAX | GATED(S2) | R-ELT-10…12 |

Sources: `Handoff_Dusktracer/game/reference` `balance/model.md`, `balance/recommendations.md` R8a, `original_docs/02_TechnicalDesign_s17_decision_log_HISTORY.md` (the affix entry, the readability audit, the late-lever nudge). Numbers the source does not hold are in `Docs/number_change_log.csv` with a hypothesis: `C.AFFIX.VAMPIRIC_TICK_S`, the `danger` order and `light` strings of `elites.csv`, and every `C.ELITE.*` look number except `C.ELITE.DRAW_SCALE`.

## Data
`09_Catalog/elites.csv`, one row per affix, in `EDTAffix` order:

| Column | Type | Notes |
|---|---|---|
| `id` | key | `Armored Swift Volatile Splitter Vampiric Shielded` |
| `tag` | tag | `Enemy.Affix.<id>`: the root stays `Enemy` (AGENTS non-negotiable 6) |
| `name_key` | text key | `ST_Enemies.Affix.<id>.Name` |
| `attack`, `telegraph`, `lead_s` | key, key, seconds | attack id and shape of an affix that hits the hero; else `none` |
| `danger` | int | the highest in the mask leads the light and the label (R-PRE-04) |
| `favored` | yes/no | R-ELT-02 |
| `no_roles` | roles | `|`-separated roles that cannot draw it |
| `light` | digits | brightness per `C.ELITE.BLINK_STEP_S`, nine is the top of the range; a distinct string per affix |

`timeline_<arena>.csv` gains `event_affixes` (int) and `event_favor` (float or `C.` key), read on `EliteWave` rows.

Pool: `FDTEnemyAux` gains `AffixMask ShieldHitsLeft ShieldAbsorbed DrainTarget DrainLinkLeft DrainTickLeft Drained Healed AliveSeconds`. `FDTEliteBlast{Origin Left Role EnemySerial}` lives in the pool like an orphan attack; `FDTEliteDeath` is published per `ResolveHits`. Presentation reads the pool through `DTElite::BuildParts`; it never writes the sim.

Debug (not in Shipping, R-TEC-12): the `elite` option of `DT.Spawn` and `DT.Spawn.Role` rolls `C.ELITE.DEBUG_AFFIXES` affix on the Elite stream; `elite=Armored+Swift` names them, `elite=none` gives the bare S1 elite. Debug elites stay non-Director: they drop nothing.

Elite waves in the shipped timeline: the first two roll one affix, the last rolls two with the favoured rows weighted (the source's three waves, at their D-71 positions).

## Acceptance
- T-ELT-01 `Dusktracer.Elites.Spawn`: an elite keeps its mask through swap-removal; a mask on a non-elite spawn is dropped; each role keeps its radius and attack.
- T-ELT-02 `Dusktracer.Elites.Roll`: the count is exact and distinct; no role draws a `no_roles` affix; a favoured weight shifts the draw by the stated factor within a band over many rolls; the Director and Spawn streams are untouched by rolls; the shipped `EliteWave` rows carry the affix counts of §Data.
- T-ELT-03 `Dusktracer.Elites.Armored`: a hit deals the stated share, from a weapon and from Burn alike; the plate and Shock multiply with it.
- T-ELT-04 `Dusktracer.Elites.Swift`: the role speed cap is multiplied; a Rusher's charge length still equals its band.
- T-ELT-05 `Dusktracer.Elites.Volatile`: death by weapon and by Burn each publish one filled ring at the death position for the full lead; a hero inside at landing takes one `Blast` of the unscaled damage, one outside none; the attack row's lead is at least the minimum (R-ENM-13).
- T-ELT-06 `Dusktracer.Elites.Splitter`: children appear on the next Enemies phase only, with the elite's scales, no elite flag, no beat tag; a beat ends when its elites die whatever their children do; at the cap they are refused and counted; a debug parent's children are not `Director`.
- T-ELT-07 `Dusktracer.Elites.Vampiric`: no drain before the full link, with the cable published for all of it; leaving range resets; ticks are spaced as stated, scale by `DmgScale` and heal up to max HP only; an intangible hero feeds nothing; frozen holds the timers.
- T-ELT-08 `Dusktracer.Elites.Shielded`: weapon hits are absorbed up to the count whatever their damage; Burn and slotless hits pass through at once; after the count, weapon hits deal full damage; absorbed hits still report one `Hit`.
- T-ELT-09 `Dusktracer.Elites.Deterministic`: the same seed and inputs give identical rolls, blasts, drains and children.
- T-ELT-10 `Dusktracer.Pre.EliteLight`: T-PRE-04: every affix maps to a distinct pattern; the lead affix is the highest `danger`; the light equals `Pal.Elite.Light`, never drops below the optic's emissive, and reaches `C.ELITE.LIGHT_EMISSIVE`.
- T-ELT-11 `Dusktracer.Elites.Parts`: an elite is drawn scaled and its optic moves to the elite parts (non-elites unchanged); every affix adds its parts in its token; shield plates fall with the hits left; every colour is a token.
- T-ELT-12 `Dusktracer.Elites.Label`: every affix has its `ST_Enemies` key in both languages' source CSV; the label is ordered lead first and capped.
- T-ELT-13 `Dusktracer.Elites.Telemetry`: an elite's death writes one `elite` record with its role, affixes, time alive, absorbed hits, drained and healed amounts and owner; a non-elite writes none.
- HUMAN: from gameplay camera, with `DT.Spawn.Role Tank 6 elite`, a validator tells two elites apart before reading a label, steps out of a Volatile ring they saw appear, and says what breaks a cable.

## Telemetry
- `elite{serial role affixes[] owner source alive_s shield_absorbed shield_left drained healed beat}` per death (R-ELT-13). An elite alive at run end writes none.
- `hero_damaged` carries `hit_kind` `Blast` or `Drain` and `attack_id`; `ThreatAction` carries the affix attack id.

## Text
`ST_Enemies`, en + zh-Hans, table created here with the role names `enemies.md` §Text promises: `Enemy.<Role>.Name` (one row per role), `Affix.<id>.Name` (one row per affix), `Affix.Join` (joins two names on a label). Terms follow `Handoff_Dusktracer/game/reference/localization/zh_glossary.md §4`.

## Out of scope
- Heat rules (an extra affix, a third affix, shield regeneration), `Hub.Quota.*`, Omens and Discoveries about elites: S5 and later.
- Escort elites, Bruiser and Support roles; boss adds that carry affixes.
- Art-book affix meshes (S4; grey boxes carry the tokens until then).
- A chest per elite: the source dropped one on every elite death; `director.md` R-DIR-16 holds one chest per beat.

## Open
- Label read against crowd density is unmeasured → first S2 session.
- Whether a drain the hero's hurt window refuses should still heal: today it does, only a dash stops the heal; the enemy system cannot see the window in its own phase → first human reading of Vampiric.
- `C.AFFIX.SHIELD_HITS` is the source's value after R8a, measured there on other weapons → S2 bot reading, `Docs/Validation/S2_elites.md`.
- Splitter children add a small Swarm gem income above the XP model → `Tools/bot_summary.py` reads it as a bound.
