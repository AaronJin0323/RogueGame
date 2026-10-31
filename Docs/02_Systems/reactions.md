# Reactions — duo reactions and duo blessings (S2)

## Intent
The second Keeper should change the first. A reaction is what happens when the two Keepers' effects meet on one enemy: something neither does alone, on the screen within a moment, with no input and no tier needed. A duo blessing is the one card that belongs to both Keepers and strengthens their reaction. Fifteen pairs of six Keepers, one reaction and one blessing each. Numbers live in `09_Catalog/reactions.csv`.

Written from `build_depth_spec.md` §4 (the fifteen, their triggers, limits and blessings), `keeper_identity_spec.md` §5.2 (the triggers re-read with flags), `build_depth_text.md` §3 and §4, `keyword_glossary.md`. Where the source is silent, or names something the build does not have, the rule says so and is marked **invented**; every invented number is in `number_change_log.csv`.

## Rules
### When a reaction fires
- **R-REA-01** A reaction of a pair is live for a player who owns at least one blessing of each of its two Keepers (`DTReactions::ActiveMask`). That is the Patron and the second Keeper, so at most one pair per player in this slice. **Invented:** the source's "or the imbue" falls away, because the enchantment here starts with the first blessing.
- **R-REA-02** S2 stage: a run has reactions and duo cards only with `Reaction.Duos` in its pool (`DT.Unlock`, `-DTUnlock`, as `keepers.md` R-KEE-23). Without it nothing here runs and no stream is drawn, so an S1 run is unchanged. **Invented:** the source has them always on.
- **R-REA-03** Who hits. A **Keeper hit** is a hit whose source is `Keeper.<Path>`: split children, echoes, arcs, hunters, slams and every other flag hit count as their Keeper's (`keeper_identity_spec.md` §5.2). A plain weapon round is no Keeper's hit, but any of the owner's hits can be a crit, and a crit is Radiance's effect (the source's "crit / dash / ricochet for Radiance / Quicksilver"). A reaction's own hits carry `Keeper.Duo.<Pair>` and no weapon slot: they feed no proc and no reaction. Cremation's ember and Refraction's shards are rounds, so they keep a slot as a hunter's do (R-KEE-27); they start no reaction either.
- **R-REA-04** A Pull landing on an enemy counts as Chill contact for that step (`keeper_identity_spec.md` §5.2): Steam and Conduction read it as Chill.
- **R-REA-05** One reaction fires at most once per enemy per `C.REACTION.ENEMY_COOLDOWN_S`, per owner, counted in sim steps. Exposure's added damage is a state and runs on every hit; its cue and count obey the limit.
- **R-REA-06** Rolls: reactions draw nothing from any stream. The same seed and inputs give the same reactions (R-TEC-02).

### The fifteen
Every number is a `reactions.csv` parameter of the pair (`Reaction` column); duo parameters (`Duo` column) are in the next table. "Hit" is the triggering hit's damage, "strongest" the strongest owned weapon's per-hit damage.

| Pair | Reaction | Meets when (re-read) | Effect |
|---|---|---|---|
| Storm + Forge | Plasma | a Storm hit lands on a Burning enemy | burst around it within `radius_uu` for `dmg` of the hit; the burst Burns what it strikes |
| Storm + Tide | Conduction | a Storm hit lands on a Chilled or Frozen enemy | up to `max` other Chilled or Frozen enemies within `radius_uu` take `dmg` of the hit |
| Storm + Shade | Writ | a Shocked and Marked enemy dies | a bolt strikes each of the `bolts` nearest Marked enemies within `range_uu` for `dmg` of strongest |
| Storm + Radiance | Flash Arc | a crit lands on a Shocked enemy | arcs to `targets` others within `range_uu` for `dmg` of the hit; they are Blinded `blind_s` |
| Storm + Quicksilver | Live Rail | a ricochet off a Shocked enemy, or a dash through one | the round gains `bounces` and arcs to the nearest for `arc_dmg` within `arc_range_uu`; a dash discharges: its Shock is spent for a burst within `radius_uu` of `dmg` of strongest |
| Forge + Tide | Steam | a hit lands on an enemy holding Burn and Chill or Freeze, or a Pull lands on a Burning one | Burn and Chill and Freeze are spent; a blast within `radius_uu` of `dmg` of the hit; what it strikes is staggered `stagger_s` |
| Forge + Shade | Cremation | a Burning and Marked enemy dies | an ember seeks the nearest enemy within `range_uu` for `ember_dmg` of strongest and Burns it. Every execute threshold is `execute_add` higher on a Burning enemy |
| Forge + Radiance | Flare | a crit lands on a Burning enemy | what its Burn still owes lands at once, × `mult`, and the Burn is spent |
| Forge + Quicksilver | Wildfire | a dash crosses the owner's field, or bonus speed reaches `speed_over` | for `burn_s` every weapon hit Burns |
| Tide + Shade | Deep Ledger | any hit on a Frozen and Marked enemy at or below `execute_below` of its HP | it is executed and shatters: `shatter_dmg` of strongest within `shatter_uu`. Bosses are never executed |
| Tide + Radiance | Refraction | a crit lands on a Frozen enemy | `shards` shards fan over `fan_deg` from it, `dmg` of the hit, pierce `pierce` |
| Tide + Quicksilver | Black Ice | a dash through a Chilled enemy, or a ricochet off a Frozen one | the dash Freezes it; the ricochet spends no bounce |
| Shade + Radiance | Exposure | a hit on a Marked and Blinded enemy | it takes `vuln` more; its death Marks everything within `mark_uu` |
| Shade + Quicksilver | Repossession | a dash through a Marked enemy at or below `below` of its HP | it is executed; `refund` of the dash cooldown comes back |
| Radiance + Quicksilver | Afterglow | a ricochet crits, or bonus speed | `crit_per_10pct` crit chance per tenth of bonus speed, under `C.MOD.CRIT_CHANCE_CAP`; a ricochet's crit Blinds `blind_s` |

- **R-REA-07** Each row above does what it says, on the seams named here.
  - A status spent is an application with `bConsume` (`enemies.md` owns the state; the application clears it next Enemies phase). Blinded for a length of its own is the Blind of `keepers.md` R-KEE-25 with that length.
  - **Bonus speed** is the hero's move speed over the base plus Momentum × `per_stack`. **Invented:** the build has no speed source but Momentum; the source's "bonus move speed" had one.
  - A dash goes **through** an enemy while the hero is intangible and within the two radii of it.
  - **Invented, damage where the source names none:** Writ's bolt, Flash Arc's arcs, Live Rail's arc and burst, the Cremation ember, Deep Ledger's shatter. Reaches: `range_uu` of Writ, Flash Arc and Cremation, `arc_range_uu`, `shatter_uu`.
  - Plasma's "Burn spreads to every arc target" is read as the burst Burning what it strikes (**invented:** an arc's targets are not known to a burst). Cremation's soul-ember is a homing round. Wildfire's ember trail is left out: no ember trail exists. Deep Ledger's Soul is left out: Souls do not exist (R-KEE-27), and Deep Ledger needs no execute of its own to fire, because a pair without Hound would never see it.

### Duo blessings
- **R-REA-08** After the commitment the committed pair's card `Blessing.Duo.<Pair>` joins that player's candidates, once per run, while `Reaction.Duos` is in the pool. Its weight is `C.OFFER.WEIGHT_BASE` × `C.OFFER.WEIGHT_DUO`, its rarity at least Rare; banish takes it as any card. It needs no Key.
- **R-REA-09** A duo blessing counts as a blessing of both Keepers: +`C.TIER.FAVOR_PER_BLESSING` Favor each, tiers follow. It belongs to no archetype and never moves a leading archetype.
- **R-REA-10** Each does what its row says (`Duo` column), only for its owner.

| Pair | Duo blessing | Effect |
|---|---|---|
| Storm + Forge | Arc Furnace | a Plasma burst leaves a field for `field_s` at `field_dps` of the burst that Burns |
| Storm + Tide | Storm Surge | Conduction has no target limit; every `every_n`th jump Freezes |
| Storm + Shade | Summons | Writ bolts Mark what they strike, and with no Marked enemy left strike the nearest unmarked |
| Storm + Radiance | Blinding Arc | Flash Arc's Blind lasts `blind_s`; Blinded enemies take `arc_vuln` more from Storm hits |
| Storm + Quicksilver | Lightning Step | a dash leaves Shocking fields every `trail_uu` for `trail_s`; a discharge also arcs to the nearest |
| Forge + Tide | Pressure Cooker | Steam blasts Chill and Burn what they strike |
| Forge + Shade | Pyre Debt | kills by Cremation embers count as Marked kills for Debt and hunters |
| Forge + Radiance | Sunspot | Flare's detonation is a crit, × the best crit multiplier |
| Forge + Quicksilver | Afterburner | every dash starts Wildfire; `speed` more move speed with a Burning enemy within `near_uu` |
| Tide + Shade | Cold Storage | a Deep Ledger execution Freezes everything within `freeze_uu` |
| Tide + Radiance | Diamond Dust | Refraction shards Chill; a shard that crits Blinds |
| Tide + Quicksilver | Glide | Black Ice's ricochet works off Chilled enemies too |
| Shade + Radiance | Spotlight | a Blinded enemy that is not Marked takes `vuln` more |
| Shade + Quicksilver | Fast Collection | Repossession works at `below`; an execution adds `charges` dash charge for `charge_s` |
| Radiance + Quicksilver | Lightspeed | Afterglow's crit bonus × `crit_scale`; a crit refunds `refund` of the dash cooldown, at most once per `C.REACTION.ENEMY_COOLDOWN_S` |

**Invented in that table:** `field_dps`, `trail_uu`, `trail_s`, `near_uu`, `freeze_uu`; the second half of Summons (a Writ bolt only strikes Marked enemies, so "Mark what they hit" did nothing). Left out: Cold Storage's two Souls; Glide's "not slowed by water or ice" (nothing slows the hero); Lightspeed's refund limit is added, because every crit would otherwise refund. The second-set duo blessings, the Stormwall and Breakwater archetypes and cross cards are not built.

### Reading it
- **R-REA-11** Each fire shows a two-ring cue at the enemy within `C.REACTION.CUE_S`: an outer ring in one Keeper's `Pal.Keeper.<Path>`, an inner one in the other's (the source's "mixes the two Keepers' hues"). At most `C.REACTION.FX_PER_S_MAX` fires per second show one, across players; the effect still happens. No warm red, no new palette token. The Look column of the source is grey-boxed to this one cue.
- **R-REA-12** The first fire of a reaction in a run shows its name and toast for `C.HUD.BANNER_S`, under the director's banner. **Invented:** the place. The Keeper-line slot of the source does not exist. Only one toast at a time; a new one replaces it.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Per-enemy cooldown | C.REACTION.ENEMY_COOLDOWN_S | GATED(S2) | R-REA-05, T-REA-05 |
| Cue cap | C.REACTION.FX_PER_S_MAX | GATED(S2) | R-REA-11, T-REA-11 |
| Cue length | C.REACTION.CUE_S | GATED(S2) | R-REA-11, T-REA-11 |
| Duo card weight | C.OFFER.WEIGHT_DUO | GATED(S2) | R-REA-08, T-REA-08 |

Per-pair numbers are `GATED(S2)` starting values in `09_Catalog/reactions.csv`, each from `build_depth_spec.md` §4 unless logged in `number_change_log.csv`.

## Data
| Field | Type | Notes |
|---|---|---|
| `reactions.csv` row | one per pair, Path order | `Pair PathA PathB Reaction Duo Source`; generated as `DTCat::Reactions` |
| `Reaction` / `Duo` | `name=value;...` | numbers only, at most `FReactionRow` holds; R-REA-07, R-REA-10 |
| `Reaction.Duos` | pool id | R-REA-02 |
| `Blessing.Duo.<Pair>` | card id | in `FDTKeeperRunState::Owned`, `EDTCardKind::Blessing`; `DTOffers::FindBlessing` does not know it, `DTReactions::FindDuo` does |
| `Keeper.Duo.<Pair>` | telemetry source | under `Keeper.*`, so tools count it as Keeper power (`00_Vision §2`, R-KEE-18) |
| `FDTReactionCounters` | per owner, per pair | `Fires Damage`; `DT.Reactions.Stats` |
| `FDTStatusApply::bConsume` | bool | R-REA-07 |

## Acceptance
- T-REA-01 `Dusktracer.Reactions.Live`: live only with a blessing of both Keepers; never for the third.
- T-REA-02 `Dusktracer.Reactions.Stage`: without `Reaction.Duos` no reaction fires and no card is a candidate; the Combat stream is untouched.
- T-REA-03 `Dusktracer.Reactions.Sources`: a flag hit fires, a plain weapon hit does not; a reaction's hit carries `Keeper.Duo.<Pair>`, no slot, and starts nothing.
- T-REA-04 `Dusktracer.Reactions.PullContact`: a Pull landing reads as Chill for Steam and Conduction.
- T-REA-05 `Dusktracer.Reactions.Cooldown`: one fire per enemy per cooldown, per reaction, per owner; another enemy fires at once.
- T-REA-06 `Dusktracer.Reactions.Deterministic`: the same seed gives the same fires and damage.
- T-REA-07 `Dusktracer.Reactions.Pair.<Pair>`: each of the fifteen, firing exactly when it meets and with its effect; `bConsume` clears what R-REA-07 names.
- T-REA-08 `Dusktracer.Reactions.Offer`: the card shows only after the commitment, for the pair, once, Rare or better, weighted, bannable.
- T-REA-09 `Dusktracer.Reactions.Favor`: +Favor to both, tiers follow, no leading archetype moves.
- T-REA-10 `Dusktracer.Reactions.Duo.<Pair>`: each of the fifteen duo blessings changes its reaction exactly as its row says.
- T-REA-11 `Dusktracer.Reactions.Cue`: a fire gives both Keepers' rings; past the cap the effect runs with no cue.
- T-REA-12 `Dusktracer.Reactions.Toast`, `.Text`, `.Telemetry`: the first fire toasts once a run; every row has its keys in en and zh-Hans with its args in its parameters; a `reaction` record carries fires and damage.
- HUMAN: after a run with a pair, a validator can say what their two Keepers do together.

## Telemetry
- `reaction{fires{Pair: n}, damage{Pair: x}}` per player at each interval end, only when one fired.
- `reaction_first{pair}` on a reaction's first fire in a run.
- `keeper_pick` with role `Duo`, `path` the pair's first Keeper, for a duo card.
- Damage, hits and kills by source `Keeper.Duo.<Pair>` through R-TEL-05.

## Text
- Table `ST_Reactions`, en + zh-Hans. Per pair: `<Pair>.Name`, `<Pair>.Toast` (reaction), `Duo.<Pair>.Name`, `Duo.<Pair>.Desc` (card; args are `Duo` parameters, `_pct` × hundred). `Toast.Format`, `Duo.Plaque`. Keyword notes `Duo` and `Reaction` in `ST_Keywords` (`keyword_glossary.md`).
- The source's "Boosts X and Y" clause is dropped: no rule stands behind it and two of its names do not exist here. zh names of reactions come from the source's own Desc lines; zh names of duo blessings are authored here.

## Out of scope
- Cross cards (E4), run memory, omens, the second duo set. Per-reaction VFX of the source's Look column: S4.
- A reaction needing a status `enemies.md` does not have: none.

## Open
- All per-pair numbers: read by bots only (`Docs/Validation/S2_duos.md`), no human reading. R-KEE-15 and R-KEE-18 are not reread here; `keepers.md` keeps its criteria.
- Afterglow and Wildfire are thin without Momentum, since bonus speed has no other source.
- Bot v2 scores a duo card `DTBotTuning::DuoScore`, a tool parameter, not a rule of `bot.md`.
- Blind is still a short Freeze (R-KEE-25), so Exposure and Flash Arc inherit its limits on the boss.
