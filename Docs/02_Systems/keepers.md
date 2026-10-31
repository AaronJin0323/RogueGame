# Keepers — S1 identity layer (Storm, Forge, Tide)

## Intent
Keepers drive power (`00_Vision §2`, the third pillar): each Keeper is a speciality, and the blessings a player takes change *how every weapon behaves* (split, chain, cleave, slam, pull, freeze, hold still), not a number on it. S1 builds the complete identity layer for the three Keepers of D-002 so a validator can name their archetype in their own words. Every per-archetype value lives in `09_Catalog/expression.csv` (the expression table, one row per archetype); this page holds the rules that bind it.

## Rules

### 1. Identity and archetypes
| Path | Speciality | Status (enchantment) | Archetypes (`Archetype.<Path>.<Name>`) and key flag |
|---|---|---|---|
| Storm | bullet-hell: fill the screen | `Status.Shock` | `Tempest` `Flag.Split` · `ArcGrid` `Flag.Chain` · `Downpour` `Flag.Echo` |
| Forge | melee and burn: close range, damage over time | `Status.Burn` | `Hammer` `Flag.ConvertMelee` · `Furnace` `Flag.Field` · `Slam` `Flag.Slam` |
| Tide | control: pull, freeze, hold your ground | `Status.Chill` → `Status.Freeze` | `Whirlpool` `Flag.Pull` · `Frost` `Flag.StatusApply` · `Stillness` `Flag.Stillness` |

- **R-KEE-01** S1 offers exactly the Keepers Storm, Forge and Tide. Each has exactly `C.KEEPER.ARCHETYPES_PER_KEEPER` archetypes and each archetype exactly `C.KEEPER.BLESSINGS_PER_ARCHETYPE` blessings with ids `Blessing.<Path>.<Archetype>.<Key|Amp1|Amp2|Trait>`, all loaded from `expression.csv`; no blessing of Shade, Radiance or Quicksilver is ever granted in S1.
- **R-KEE-02** Every Key blessing carries exactly one behaviour flag, its archetype's key flag (table above), and no two archetypes share a key flag. Every Amp and Trait changes a parameter of a `Flag.*` or adds a `Flag.*`; a blessing whose whole effect is a stat delta (damage, rate, crit, armor, HP) is rejected at data load.
- **R-KEE-03** A blessing's flags apply to every weapon the player owns and to every weapon acquired later, exactly once per blessing. Where a weapon's capabilities cannot express a flag natively, the flag's capability fallback (`weapons.md`, `09_Catalog/flag_compat.csv`) applies; a flag is never dropped silently.

### 2. Blessings
- **R-KEE-04** An Amp or Trait is never offered or granted while its archetype's Key is not owned. A Trait additionally needs `C.KEEPER.TRAIT_PREREQ_CARDS` owned cards of its archetype, the Key counted. Owned blessings are never removed during a run.

Flags as blessings use them (the weapon side, caps and fallbacks are `weapons.md`; parameters per archetype are `expression.csv`):

| Flag | What the blessing makes happen |
|---|---|
| `Split` | on hit or expiry a round becomes children (fan); generations above one only from a Trait or an Intercession, capped by `modifiers.md`; `retarget=EliteOrBoss` sends the children at the elites and the boss within their reach, nearest first, before any fan |
| `Chain` | a hit jumps to further enemies in range for a share of its damage; a Shocked target takes `dmg_vs_shocked` more; each jump has `return_same_target` chance to leap back onto the enemy it left; an arc that kills forks into `fork_on_kill` one-jump arcs from the death point, and forks never fork; `kill_shocks_neighbours_uu` Shocks everything in reach of that death |
| `Echo` | a delayed strike falls from the sky on the impact point (not a repeat of the volley); every `every_n`th strike of the player falls on the whole screen around the hero instead |
| `ConvertMelee` | the first rounds of each volley become a cleave around the hero; cool Keeper colours, never red (R-PRE-01); `apply_modifier_on_hit` bounces a live Ricochet off each enemy a cleave strikes (the other live modifiers already reach cleaves; Split is dormant under a conversion) |
| `Field` | the impact (or slam) leaves a zone that ticks damage and applies the `status` it names (R-KEE-22); `trigger=HitOnBurning` needs the struck enemy to Burn when the hit lands; `spread_burn_on_kill_uu` makes a Burning death pass its strongest Burn to everything in reach, at most once per `spread_burn_interval_s` |
| `Slam` | a ground ring around the hero on its triggers (dash landing, every Nth melee hit, hit taken: a hit that cost HP, never a touch inside i-frames or a dash); the first slam after a hit deals × `after_hit_taken_mult`; `embers` rounds of the shared child kind fly out for `ember_dmg` of the slam and apply `ember_status`; `detonate_burning` deals what each Burning enemy's Burn still owes at once, as a Keeper hit, and the Burn runs on |
| `Pull` | the impact draws enemies toward it, each enemy at most once per `C.KEEPER.PULL_INTERVAL_S` (R-KEE-21); pulled enemies whose pulled positions touch take `crush_dmg` of the triggering hit as Keeper damage (the Key carries it); `pulled_vuln` adds that share of every hit on an enemy pulled in the last `pulled_vuln_s`; at `cluster_freeze_min` touching pulled enemies not yet Frozen, they Freeze together and with `shatter` burst once around their centre (a shatter, below); `on=ChargedVolley` is Stillness's Pull: only a charged volley's impact, within `charged_pull_uu` |
| `StatusApply` | hits apply the Keeper status at a chance; for Frost it also changes the Chill → Freeze threshold. A Frozen death with `shatter_on_frozen_kill_uu` is a shatter there; `frozen_hit_chills_neighbours_uu` Chills around a Frozen enemy that is hit; with `frozen_blocks_enemies` other enemies overlapping a Frozen one are pushed back to its rim; `freeze_until_hit` holds up to `held_max` Freezes past their end until a weapon strikes the enemy (a strike ends the hold, it never shortens a Freeze still running) |
| `Stillness` | standing still charges the next volley of every weapon; dashing keeps the charge, walking spends it. A charged volley applies the Key's `status` to what it hits; `charged_freeze_ring_uu` Freezes around the hero as it fires; while any weapon holds a charge the hero pulses `aura_status` within `charged_aura_uu` every `aura_interval_s` |

A Burning enemy that dies explodes for `explode_dmg` × the strongest owned weapon's per-hit damage within `burning_explode_on_death_uu` and Burns what it hits: Hammer's line (on `ConvertMelee`) needs a melee arc's kill, Slam's a slam's kill, Furnace's (on `Field`) any death and leaves that Field. Slam's Intercession variant `dash_cooldown_reset` refills every dash charge on a dash landing during the cast. A shatter is one burst of `shatter_dmg` × the strongest owned weapon's per-hit damage that applies `shatter_status`. Crush, `pulled_vuln`, explosion and shatter hits carry no weapon slot, so no proc feeds on them.

- **R-KEE-21** A `Flag.Pull` moves a given enemy at most once per `C.KEEPER.PULL_INTERVAL_S`, counted in sim steps, whichever player or entry pulls (`PullReadyStep`, `enemies.md` Data). An impact skips an enemy still held: no displacement and nothing that rides on the pull (crush, the `pulled_vuln` mark, `on=Pulled` statuses, the cluster count). A `pulled_vuln` mark already running still pays. Direction and `distance_uu` are unchanged. This covers every `Flag.Pull`, Stillness's and `Mod.Backwash` included; the mine's own pull and the Tide Intercession's line are not `Flag.Pull` and are not held (D-016).
- **R-KEE-22** A field that names a `status` applies it every `C.STATUS.BURN_TICK_S` of sim time to the enemies within its damage reach, once per enemy per source however many of that source's fields overlap, so a Burn stack lives to its first tick. A Burn from a field scales with the emission that left the field (R-KEE-11). The application is not a hit (R-KEE-16) (D-016).

### 3. Primary, second and leading archetype
- **R-KEE-05** The **primary Keeper** (Patron) is the Keeper the player picks in the run-start Patron hand (`offers.md` R-OFF-08); it never changes in the run. Until the L8 commitment (`offers.md`) only primary blessings are granted. At the L8 commitment the player picks one **second Keeper** from the other S1 Keepers; it never changes after. The third S1 Keeper's blessings are never granted to that player.
- **R-KEE-06** A Keeper's **leading archetype** for a player is the archetype with the most owned cards among those whose Key is owned; a tie goes to the archetype of the most recently taken card; with no Key owned there is none. It is re-evaluated on every pick and read by tiers (R-KEE-07), the Intercession (R-KEE-14) and compatibility (R-KEE-16).
- **R-KEE-16** After the commitment, a second-Keeper blessing is offered only if none of its flags is listed in `IncompatFlags` of the primary's leading archetype; with no leading archetype nothing is excluded. The pairs are symmetric and mechanical: `Split`–`ConvertMelee` (converted rounds never fly, so never split), `Chain`–`Field` (field ticks are not hits, so never chain), `Slam`–`Stillness` (dash-in versus hold still). A change of leading archetype never removes owned blessings.

### 4. Favor and tiers
- **R-KEE-07** A Keeper's Favor is its owned blessings × `C.TIER.FAVOR_PER_BLESSING`. Tiers T1, T2 and T3 unlock when Favor reaches `C.TIER.FAVOR_T1`, `C.TIER.FAVOR_T2`, `C.TIER.FAVOR_T3` and are never lost. An unlocked tier applies the tier line of that Keeper's current leading archetype (`expression.csv` `Tier1..Tier3`) and follows it when the leading archetype changes; each tier changes a flag, never only a stat.
- **R-KEE-08** Every tier line fits `C.TIER.LINE_WORDS_EN_MAX` words in en and `C.TIER.LINE_CHARS_ZH_MAX` characters in zh-Hans.

### 5. Enchantment (look + status)
- **R-KEE-09** From the first blessing, every player round carries the primary Keeper's trail colour (`Pal.Keeper.<Path>`) and each player hit applies the primary status with chance `C.ENCHANT.STATUS_CHANCE_PRIMARY`. From the commitment, the second Keeper tints the trail tip only and applies its status with chance `C.ENCHANT.STATUS_CHANCE_SECOND`. No enchantment ever changes the round's core or head colour (R-PRE-01). Status rolls draw only from the `Combat` stream (R-TEC-02).
- **R-KEE-10** Keepers decide *when* a status lands, `enemies.md` owns *what* it does: status state, durations, stacking and effects are R-ENM-21 and R-ENM-22 with `C.STATUS.*`, never redefined here. Every proc (enchantment, `Flag.StatusApply`, a flag's own status, Intercession) is rolled in `EDTSimPhase::Effects` on that step's hits and kills and appends one `FDTStatusApply` (`enemies.md` §Data) with the hit's owner and the granting Keeper's source; it takes effect at the next Enemies phase (`07_Technical §4`).
- **R-KEE-11** A Burn application's damage per second is `C.STATUS.BURN_DPS_RATIO` × the applying hit's damage, raised by the owner's Furnace `burn_tick_rate` (the tick itself is `C.STATUS.BURN_TICK_S`, `enemies.md`); a Furnace stack-cap bonus (`burn_stacks_max`, `expression.csv`) travels in the application's `StackBonus`.
- **R-KEE-12** A Chill application adds its stacks (one, or a flag's `chill_stacks_*`); Frost's lower Freeze threshold (`freeze_stacks`) and longer Freeze (`freeze_duration_scale`) travel in the application's `FreezeAt` and `FreezeScale`; otherwise `C.STATUS.FREEZE_STACKS` and `C.STATUS.FREEZE_S` apply.
- **R-KEE-13** Every hit or tick that exists because of a Keeper (flag children, chain jumps, echo strikes, cleaves, slams, field ticks, crush, status damage, Intercession) carries telemetry source `Keeper.<Path>` of the Keeper that granted it (R-TEL-05); layer hits carry the second Keeper's source.

### 6. Intercession
- **R-KEE-14** `combat_core.md` owns the input, the charge and the activation; this section owns the content. On activation the effect is the primary Keeper's **base**, shaped by the **variant** of the primary's leading archetype (none → base only), plus from the commitment the **layer** of the second Keeper's leading archetype (none → no layer). The three are read at cast time. Where a layer conflicts with a variant, the variant's shape wins and the layer applies to its hits. A primary always exists (the Patron is picked before the first step), so every cast has a base.

| Path | Base (variants and layers: `expression.csv`) |
|---|---|
| Storm | for `C.INTERCESSION.STORM_DURATION_S` every round fired Splits into `C.INTERCESSION.STORM_SPLIT_CHILDREN` and each child Chains once |
| Forge | for `C.INTERCESSION.FORGE_DURATION_S` a fire Slam ring of `C.INTERCESSION.FORGE_RING_RADIUS_UU` every `C.INTERCESSION.FORGE_RING_INTERVAL_S`, knocking back and Burning |
| Tide | every enemy within `C.INTERCESSION.TIDE_RADIUS_UU` is pulled into a line ahead of the hero and Frozen for `C.INTERCESSION.TIDE_FREEZE_S`. A variant that names no `Flag.Pull` (Frost, Stillness) drops the line: the crowd Freezes where it stands (D-019) |

- **R-KEE-15** For each primary Keeper, every variant × layer combination (including no layer) measures marginal damage (with-cast minus no-cast, on a refilled field) within ±`C.INTERCESSION.BAND` of that Keeper's own combination mean. Out-of-band parts are retuned toward the mean, at most `C.INTERCESSION.RETUNE_STEP_MAX` per pass.

### 7. Power budget
- **R-KEE-17** No number anywhere depends on weapon fit: a flag runs at full strength on every weapon, and an off-fit build is weaker only because the mechanic does not fit (fit lists are `weapons.md` data, used for UI marks and offer weights only).
- **R-KEE-18** For a steered build at `C.TEL.POWER_CHECK_LEVEL`, Keepers give at least `C.POWER.KEEPER_SHARE_L16` of the power (T-BAL-01, `Tools/power_split.py`); each archetype's full package on its fit weapons multiplies crowd DPS by `C.KEEPER.PACKAGE_GAIN_BAND` over the same build without it; the nine archetypes sit within ±`C.POWER.ARCHETYPE_BAND` of their mean.

### 8. Per player
- **R-KEE-19** Primary, second, owned blessings, Favor, tiers, leading archetypes and Intercession state are per-player run state keyed by `OwnerId` (R-TEC-01): one player's Keepers never change another player's weapons, statuses' sources or offers. The same seed and inputs give the same status procs.
- **R-KEE-20** `expression.csv` is the only source of archetype content: every row validates (the S1 rows and §9's, known `Flag.*`/`Status.*`/`Archetype.*` tags, every `IncompatFlags` pair symmetric) and generates the Keeper data assets headless; a row that fails stops generation.

### 9. S2 Keepers: Shade, Radiance, Quicksilver
| Path | Speciality | Status (enchantment) | Archetypes (`Archetype.<Path>.<Name>`) and key flag |
|---|---|---|---|
| Shade | tracking | `Status.Mark` | `Hound` `Flag.Seek` · `Hunters` (Collectors) `Flag.Hunter` · `Economy` (Ledger) `Flag.Debt` |
| Radiance | burst | `Status.Blind` | `Lance` `Flag.Charge` · `Prism` `Flag.CritSplit` · `Solar` `Flag.Nova` |
| Quicksilver | mobility | none | `Vector` `Flag.VolleyOnDash` · `Momentum` `Flag.SpeedScale` · `Rebound` `Flag.Ricochet` |

Written from `keeper_identity_spec.md` §3.4 to §3.7, `intercession_variants_spec.md` §2 and §2.1, `keeper_identity_text.md` §3 and §8. Where the source is silent or cannot be built on the S1 seams the rule says so and is marked **invented**; every invented number is in `number_change_log.csv`.

- **R-KEE-23** Rows of `expression.csv` carry a `Stage`; S1 rows come first and stay what a run offers by default, so R-KEE-01 and T-KEE-01 hold unchanged (`DTCat::NumArchetypes` counts them, `DTCat::NumAllArchetypes` every row). An S2 Keeper reaches a run only through `DT.Unlock Keeper.<Path>` or the launch argument `-DTUnlock=Keeper.<Path>[,...]` (D-013's route, until meta). An unlocked Keeper always has a seat: it comes first in the Patron hand and the L8 hand, then the others as R-OFF-08 says, up to `C.OFFER.KEEPER_CHOICES`. Its Keys are starters of that run. **Invented:** the seat rule; the source has six Keepers in every hand.
- **R-KEE-24** `Status.Mark` is per owner, keyed by enemy serial, and lives `C.STATUS.MARK_S` plus the owner's `mark_s`, refreshed by each application; it does nothing by itself: Seek, Hunter, Debt and the Closing read it. Keeper state, because `enemies.md` owns no Mark. The boss can be Marked.
- **R-KEE-25** `Status.Blind` stuns: the application is a Freeze for `C.STATUS.BLIND_S` from the same source and owner, so it moves and shoots nothing and the boss refuses it, plus an owner-keyed Blinded mark for `blind_spread_uu`. Boss control turning into Cracked (D-60) is out of this slice. **Invented:** that Blind is a short Freeze. It lets Freeze's readers see a Blinded enemy (a shatter, a Frozen hit); `enemies.md` should own a real Blind when it next opens.
- **R-KEE-26** `Seek`: a round of a weapon that expresses the flag natively steers at `turn_deg_s` within `seek_range_uu` toward the Marked enemy nearest the hero at launch, or the nearest enemy once its target is gone (`bullets.md` R-BUL-09), with `pierce_add` more pierce. Fallbacks (`flag_compat.csv`): Homing adds `turn_deg_s`; Orbit, Lob and Mine pick a Marked enemy first; Melee and Beam deal `C.KEEPER.SEEK_FALLBACK_DMG` more of the hit to Marked enemies as Keeper damage. `marked_vuln` adds that share of every weapon hit on a Marked enemy as Keeper damage. `execute_below`: a weapon hit on a Marked enemy at or below that share of its HP after the hit kills it, as Keeper damage. Bosses are never executed (`keyword_glossary.md`).
- **R-KEE-27** `Hunter`: a kill named by `source` (`MarkedKill`, `EliteKill`), or any kill at `any_kill_chance`, raises a hunter there, at most `max_live` per player. A hunter lives `life_s` and every cooldown fires the pattern of the owner's highest-load weapon at `dmg` of its damage from its own position; its hits carry that weapon's slot, so modifiers and flags react, and the source `Keeper.Shade`. It walks at hero speed toward the nearest enemy and stops inside the weapon's range. `explode_uu` bursts `explode_dmg` of the strongest weapon when its life ends. `kill_extends_s` adds to every living hunter when a hunter kills. `lasting_every` raises a hunter that never expires at `lasting_dmg` every that many kills, at most `lasting_max`. **Invented:** kills stand in for Souls (no Soul drop exists); `kill_extends_s` stands in for Escrow's heal (heal sources are held, `STATE.md` queue); the walk.
- **R-KEE-28** Hunters are drawn with the drone core; they take no damage and block nothing.
- **R-KEE-29** `Debt` (**invented flag**: the source gives Ledger no flag, R-KEE-02 needs a unique one): each kill of a Marked enemy adds one Debt, `elite_debt` more for an elite. At `debt_max` Debt resets and the owner's next volley of any weapon is a payday: instead of its pattern it fires one homing round at every enemy within `C.ARENA.CAMERA_FRAME_RADIUS_UU`, within the spawn cap (R-WPN-09), for `payday_mult` of the round's damage, pierce `payday_pierce` more, as Keeper damage; `payday_execute_below` then executes Marked enemies at or below that share. Long Arm's gem pull is left out: gems belong to `progression.md`.
- **R-KEE-30** Crit: `crit_add` adds to a weapon's crit chance, `crit_mult` is the crit damage multiplier (the largest; a `+x` tier adds to its Key), under `C.MOD.CRIT_CHANCE_CAP` (R-MOD-05). `crit_pulse_uu` bursts `crit_pulse_dmg` of the hit around a crit; `crit_status` applies its status on a crit. **Invented:** no card names crit damage; the value is a first guess. Keen Edge's "+crit" is a stat, which R-KEE-02 rejects, so it is `charged_crit`, chance on charged shots only.
- **R-KEE-31** `Charge`: every `every_n`th shot of each weapon is charged: `mult` damage, `pierce_add` more pierce, `charged_crit` more crit chance, `always_crit` always a crit, as Keeper damage. Works at every cap (the flag's fallbacks are this same rule). `focal_stack`: each crit on the enemy the owner crit last adds that share of the hit per stack, up to `focal_max`, as Keeper damage; another enemy resets it.
- **R-KEE-32** `CritSplit` (**invented flag**: the source's Prism key is `Split` on a crit; R-KEE-02 needs a unique key flag): a crit by a first-generation emission fans `children` lances for `child_dmg`; lances never split or crit-split. `lance_status`: a lance applies it and runs no other reaction. `pulse_every`: every that many crits bursts `pulse_dmg` of the hit in `pulse_uu`. `blind_spread_uu`: a Blinded death Blinds what is in reach.
- **R-KEE-33** `Nova`: a weapon kill's overkill (the estimate of R-MOD-14) bursts within `radius_uu` for `dmg` of it, as Keeper damage; a burst starts no reaction. Halo Ring: a kill inside `ring_uu` of the hero bursts at `ring_mult`; `sun_share` of weapon damage to elites and the boss is stored and released on the nearest of them every `sun_interval_s`, within `seek` of the screen frame.
- **R-KEE-34** Quicksilver, on hero motion. `VolleyOnDash`: a dash start fires every weapon once, free, along the dash line where the pattern aims and at `dmg_mult`, as Keeper damage; Melee sweeps at the landing. Each kill of such a volley refunds `kill_refund_s` of dash cooldown, at most `refund_max` of it per dash. `dash_cooldown_scale`, `dash_charges` and `free_every` (every that many dashes refill) reach the hero as stats. `tailwind_rate` for `tailwind_s` after a dash speeds fire. `SpeedScale`: moving adds `per_100uu` Momentum per metre, a dash `dash_stacks`, a hit taken loses `hit_loss`, and with no movement it fades `decay_per_s`; each stack gives `per_stack` damage and fire rate, up to `stacks_max`. At the cap `max_pierce` more pierce and a strike at each impact (`max_echo_delay_s`, `max_echo_dmg`). `wave_every_uu` of movement bursts `wave_dmg` of the strongest weapon within `wave_uu`. `Ricochet` from a Keeper adds to the rounds' bounces; a bounce deals `bounce_dmg` more per bounce done, and with no other enemy in `seek_range_uu`, `return_same_target` strikes the same enemy; `kill_free_round` fires one free round at the next enemy from a bounce's kill. Fallbacks: Beam adds its bounces to pierce; Melee and Orbit fire `C.KEEPER.RICOCHET_FALLBACK_RATE` faster per bounce; Lob and Mine strike once more at the nearest enemy. A Quicksilver hit applies no status (the Glossary lists none); `none` is a legal `EnchantStatus`.
- **R-KEE-35** Intercession bases, under R-KEE-14. Shade (*Closing*): for `C.INTERCESSION.SHADE_DURATION_S` every enemy on screen is Marked at the start and rounds Seek at `C.INTERCESSION.SHADE_TURN_DEG_S`; at the end Marked enemies at or below `C.INTERCESSION.SHADE_EXECUTE_BELOW` are executed. Radiance (*Sun Lance*): after `C.INTERCESSION.RADIANCE_CHARGE_S`, for `C.INTERCESSION.RADIANCE_BEAM_S` a beam along the aim hits every `C.INTERCESSION.RADIANCE_BEAM_TICK_S` for `C.INTERCESSION.RADIANCE_BEAM_DMG` of the strongest weapon, width `C.INTERCESSION.RADIANCE_BEAM_WIDTH_UU`, always a crit; a kill's overkill novas by the Path's Nova numbers. Quicksilver (*Time Dilation*): for `C.INTERCESSION.QUICKSILVER_DURATION_S` every dash fires a `VolleyOnDash`. Its slow-motion is **not built**: it is the same `time_scale` as Stillness (Open), and it needs the enemy side. Variants and layers are `expression.csv`: `on_cast` raises hunters; `instant_payday` and `cast_debt` pay now; `beams` runs the beam that many times, each on the densest line; `beam_fan` fans it over `C.INTERCESSION.RADIANCE_FAN_DEG`; `beam_nova_s` novas along it; `grant_stacks` and `no_decay` set Momentum; `dash_cooldown_s` sets the dash cooldown for the cast.
- **R-KEE-36** A Marked enemy shows a cue in `Pal.Keeper.Shade`; a beam is a narrow arc flash. No new palette token. Debt and Momentum have no HUD meter in this slice (`S2_keepers.md`).

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| S2 Keeper numbers | C.STATUS.MARK_S, C.STATUS.BLIND_S, C.KEEPER.SEEK_FALLBACK_DMG, C.KEEPER.RICOCHET_FALLBACK_RATE, C.INTERCESSION.SHADE_* / RADIANCE_* / QUICKSILVER_* | GATED(S2) | §9 |
| Archetypes per Keeper | C.KEEPER.ARCHETYPES_PER_KEEPER | CORE | R-KEE-01, T-KEE-01 |
| Blessings per archetype | C.KEEPER.BLESSINGS_PER_ARCHETYPE | CORE | R-KEE-01, T-KEE-01 |
| Cards before a Trait | C.KEEPER.TRAIT_PREREQ_CARDS | GATED(S1) | R-KEE-04, T-KEE-04 |
| Package gain on fit weapons | C.KEEPER.PACKAGE_GAIN_BAND | GATED(S1) | R-KEE-18, T-KEE-18 |
| Favor per blessing | C.TIER.FAVOR_PER_BLESSING | CORE | R-KEE-07 |
| Tier thresholds | C.TIER.FAVOR_T1 / T2 / T3 | CORE | R-KEE-07, T-KEE-07 |
| Tier line limits | C.TIER.LINE_WORDS_EN_MAX, C.TIER.LINE_CHARS_ZH_MAX | CORE | R-KEE-08, T-KEE-08 |
| Status chance, primary / second | C.ENCHANT.STATUS_CHANCE_PRIMARY / SECOND | GATED(S1) | R-KEE-09 |
| Burn magnitude | C.STATUS.BURN_DPS_RATIO | GATED(S1) | R-KEE-11 (status numbers: `enemies.md`) |
| Default Freeze threshold and length | C.STATUS.FREEZE_STACKS, C.STATUS.FREEZE_S | GATED(S1) | R-KEE-12 |
| Pull interval | C.KEEPER.PULL_INTERVAL_S | GATED(S1) | R-KEE-21 |
| Intercession bases | C.INTERCESSION.STORM_* / FORGE_* / TIDE_* | GATED(S1) | R-KEE-14 |
| Intercession band | C.INTERCESSION.BAND | CORE | R-KEE-15 |
| Retune step per pass | C.INTERCESSION.RETUNE_STEP_MAX | GATED(S1) | R-KEE-15 |
| Power budget | C.POWER.KEEPER_SHARE_L16, C.POWER.ARCHETYPE_BAND | CORE | R-KEE-18 |
| Blessing starter pool | C.POOL.STARTER_BLESSINGS | CORE | `offers.md` R-OFF-07: the `Key` of each archetype is a starter |

Per-archetype parameters (`chance=`, `radius_uu=` …) are `GATED(S1)` starting points from `Handoff_Dusktracer/game/reference/specs/keeper_identity_spec.md §3`, §3.7 and `intercession_variants_spec.md §2`; they live in `expression.csv` and every change is logged in `number_change_log.csv`.

## Data
| Field | Type | Notes |
|---|---|---|
| `expression.csv` row | one per archetype | columns below; generated into `UDTKeeperDef` + `UDTBlessingDef` data assets |
| `Path` / `Archetype` | `Keeper.<Path>` / `Archetype.<Path>.<Name>` tags | |
| `KeyFlag` | `Flag.*` tag | R-KEE-02 |
| `Key` `Amp1` `Amp2` `Trait` | effect spec (every `Key` is a starter card, R-OFF-07) | `Flag.<Name>{param=value;…}`, several joined by ` + `; `+x` adds to the Key's param; blessing id is `Blessing.<Path>.<Archetype>.<column>` |
| Param units | in the spec | `_uu` distance, `_s` time, `dmg` `dps` `crush_dmg` `pulled_vuln` ratios of the triggering hit, `ember_dmg` of the slam, `dmg_ratio` (Slam), `explode_dmg` and `shatter_dmg` ratios of the strongest owned weapon's per-hit damage |
| `Tier1` `Tier2` `Tier3` | effect spec | applied while the archetype leads (R-KEE-07) |
| `EnchantLook` / `EnchantStatus` | palette token / `Status.*` | per Keeper, repeated per row |
| `IntercessionVariant` / `SecondKeeperLayer` | effect spec | variant shapes the primary's base; layer is what this archetype adds as the second Keeper |
| `IncompatFlags` | `Flag.*` list (`|`) | R-KEE-16 |
| `DuoReactions` | `S2` | a marker: reactions live in `09_Catalog/reactions.csv` (`reactions.md`) |
| `Stage` | `S1` / `S2` | R-KEE-23; S1 rows first |
| `TextPrefix` | `ST_Keepers.<Path>.<Archetype>` | keys below |
| `UDTBlessingDef` | data asset | `Id Path Archetype Role{Key,Amp,Trait} Flags[] Requires[] TextKey` |
| `FDTKeeperRunState` | inside `FDTPlayerRunState` (R-TEC-01) | `Primary Second Owned[] Favor[Path] Tier[Path] Leading[Path] CommitDone` |

## Acceptance
- T-KEE-01 `Dusktracer.Keepers.Catalog`: the generated defs hold three Keepers × `C.KEEPER.ARCHETYPES_PER_KEEPER` × `C.KEEPER.BLESSINGS_PER_ARCHETYPE` blessings with the right ids; no other Path is offerable.
- T-KEE-02 `Dusktracer.Keepers.NoStatOnly`: every Key has one flag (its archetype's, unique); a stat-only Amp fixture is rejected at load.
- T-KEE-03 `Dusktracer.Keepers.FlagsOnAllWeapons`: a blessing taken with two weapons applies once to both and to a third acquired later; a non-native weapon gets the fallback.
- T-KEE-04 `Dusktracer.Keepers.Prerequisites`: Amps are ineligible without the Key; the Trait needs `C.KEEPER.TRAIT_PREREQ_CARDS` cards.
- T-KEE-05 `Dusktracer.Keepers.PrimarySecond`: the Patron pick fixes the primary; before commitment only primary blessings are granted; after it only primary and second; the third never.
- T-KEE-06 `Dusktracer.Keepers.LeadingArchetype`: most cards wins, tie goes to the latest card, no Key gives none.
- T-KEE-07 `Dusktracer.Keepers.Tiers`: tiers unlock exactly at the thresholds, follow a leading-archetype change and are never lost.
- T-KEE-08 `Dusktracer.Keepers.TierLineLength`: every tier line in `ST_Keepers` en and zh-Hans is within limits.
- T-KEE-09 `Dusktracer.Keepers.Enchantment`: trail colours as specified, core unchanged; status proc rates over a seeded sample match the chances; same seed, same procs.
- T-KEE-10 `Dusktracer.Keepers.StatusProcs`: procs append one status application in the Effects phase with the hit's owner and the Keeper source, take effect the next step, and use only `C.STATUS.*` behaviour.
- T-KEE-11 `Dusktracer.Keepers.BurnMagnitude`: a Burn application carries the ratio × the hit's damage; Furnace's bonus raises the stack cap.
- T-KEE-12 `Dusktracer.Keepers.FrostThreshold`: Frost's application freezes at its lower threshold and for its scaled length; without Frost the defaults apply.
- T-KEE-13 `Dusktracer.Keepers.Sources`: children, chains, echoes, cleaves, slams, ticks, status and Intercession damage carry the granting Keeper's source; layer hits the second's.
- T-KEE-14 `Dusktracer.Keepers.IntercessionCompose`: base, variant by leading archetype, layer only after commitment, variant shape wins a conflict.
- T-KEE-15 `Tools/probe.py read <dir> --check` (bot probe, `Tools/BotRuns.ps1 -Jobs`; the scene is `Dusktracer.Bot.ProbeScene`): every combination per primary within the band of its mean, on the combinations R-KEE-16 allows, one fixed loadout, a refilled field. A reading, like T-DIR-02, not a gate step until the numbers are tuned against it (`Docs/Validation/S1_probes.md`).
- T-KEE-16 `Dusktracer.Keepers.SecondCompat`: an incompatible second-Keeper blessing is never offered; none excluded without a leading archetype; owned ones stay when the leader changes.
- T-KEE-17 `Dusktracer.Keepers.NoFitMultiplier`: the same blessing set gives identical flag parameters on fit and off-fit weapons.
- T-KEE-18 `Tools/probe.py read <dir> --check` (bot ablation of the L16 snapshots of the nine showcase arms, `bot.csv.L16`): Keeper share, package gain and archetype band hold. A reading, not a gate step yet (`Docs/Validation/S1_probes.md`); `Tools/power_split.py` stays the attribution view of session logs.
- T-KEE-19 `Dusktracer.Keepers.PerPlayer`: two owners with different Keepers never affect each other's weapons, procs or offers.
- T-KEE-21 `Dusktracer.Keepers.Param.*`: Tide's line needs a pull (D-019), pull interval (R-KEE-21), field status (R-KEE-22), Whirlpool's layer, crush, pulled_vuln, arcs (dmg_vs_shocked, return_same_target), fork_on_kill, retarget, HitOnBurning, burning deaths (explosion, spread), dash_cooldown_reset, Frost's shatter, wall, hold and neighbour Chill, Whirlpool's cluster and pull stacks, Stillness's charged status, pull, aura, ring and cast, Slam's after-hit, embers and detonation, Furnace's Burn rate, Hammer's Ricochet, Downpour's screen strike and Arc Grid's kill Shock each change behaviour exactly when their condition holds; the Whirlpool Key has a non-zero Keeper damage share (`Dusktracer.Keepers.Key.Whirlpool`).
- T-KEE-22 `Dusktracer.Keepers.Keywords`: every status and every flag an S1 card or `Kw` column names has `.Name` and `.Desc` in `ST_Keywords`, and "god" never appears (`Tools/loc_build.py`).
- T-KEE-20 `Dusktracer.Keepers.ExpressionCsv`: the generator rejects a missing row, an unknown tag and an asymmetric incompatibility, and builds headless.
- T-KEE-23 `Dusktracer.Keepers.S2.Stage`: the default run offers S1 only; an unlock seats the Keeper first in both hands and opens its Keys.
- T-KEE-24 `Dusktracer.Keepers.S2.Mark`: Mark is per owner, refreshes and expires; `mark_s` lengthens it.
- T-KEE-25 `Dusktracer.Keepers.S2.Blind`: Blind stuns for its length, the boss refuses it, `none` is a legal enchantment.
- T-KEE-26 `Dusktracer.Keepers.S2.Seek`, `.Card.Shade.Hound.*`: rounds steer at Marked enemies, every card and fallback.
- T-KEE-27 `Dusktracer.Keepers.S2.Card.Shade.Hunters.*`: hunters rise, fire, expire, cap; every card.
- T-KEE-28 `Dusktracer.Keepers.S2.HunterCue`: a hunter is gathered as a drone core.
- T-KEE-29 `Dusktracer.Keepers.S2.Card.Shade.Economy.*`: Debt fills, a payday replaces one volley; every card.
- T-KEE-30 `Dusktracer.Keepers.S2.Crit`: `crit_add` under the cap, `crit_mult`, the pulse and the status.
- T-KEE-31 `Dusktracer.Keepers.S2.Card.Radiance.Lance.*`: every Nth shot is charged; every card.
- T-KEE-32 `Dusktracer.Keepers.S2.Card.Radiance.Prism.*`: crits fan lances that never split; every card.
- T-KEE-33 `Dusktracer.Keepers.S2.Card.Radiance.Solar.*`: overkill novas; every card.
- T-KEE-34 `Dusktracer.Keepers.S2.Card.Quicksilver.*`: dash volleys, Momentum, bounces and their fallbacks; every card, each tier.
- T-KEE-35 `Dusktracer.Keepers.S2.Cast.*`: each base, variant and layer, composed and run; layers carry the second's source.
- T-KEE-36 `Dusktracer.Keepers.S2.Cue`, `.Text`: a Marked enemy shows its cue; every S2 row has all its keys, within limits, its args in its spec.
- HUMAN: after one run, ≥ two of three validators name their archetype in their own words (`00_Vision §4`); the trail shows the primary colour from the first blessing and the second only at the tip.

## Telemetry
- `keeper_pick{owner, blessing, path, archetype, role, favor, tier, leading}` per blessing taken.
- `keeper_commit{owner, role, path, primary_leading}` when a Keeper hand is picked: role `Patron` at run start, `Second` at the L8 commitment.
- `status{owner, applied{Status: n}}` per player at each interval end (procs by status).
- `intercession{owner, base, variant, layer, enemies_hit, damage}` per cast; marginal damage is measured offline (R-KEE-15, `Tools/probe.py`).
- Damage, hits and kills by source `Keeper.<Path>` through R-TEL-05.

## Text
- Table `ST_Keepers`, en + zh-Hans from the first string; zh authored from the fiction (`localization/zh_naming.md`), never "god".
- Per row, keys under its `TextPrefix`: `.Name`; `.Key|.Amp1|.Amp2|.Trait` each `.Name` + `.Desc`; `.Tier1|.Tier2|.Tier3`; `.Ult.Desc`; `.Layer.Name` + `.Layer.Desc`. Per Keeper: `ST_Keepers.<Path>.Name` + `.Desc` (the `Keeper.<Path>` card of the Patron and L8 hands), `ST_Keepers.<Path>.Enchant.Desc`, `ST_Keepers.<Path>.Ult.Name`, `ST_Keepers.<Path>.Ult.Desc`. Keyword notes (`<kw>`, used by every card table): `ST_Keywords.Status.<Name>` and `ST_Keywords.Flag.<Name>`, each `.Name` + `.Desc`, owned here. Budget: exactly these keys, no others.
- Numbers in text are format args from data. Tier lines: R-KEE-08. Names and rules follow the card limits of `keeper_identity_text.md` §0.

## Out of scope
- Duo reactions and duo blessings (`reactions.md`); the Stormwall and Breakwater archetypes of the reference; patron traits (their role is the enchantment here): S2.
- Weapon flag expression, caps, fallbacks and `flag_compat.csv` (`weapons.md`); offer weights and the L8 hand (`offers.md`); Intercession input and charge (`combat_core.md`); status behaviour (`enemies.md`). Path transformations for the L16 hand: S2 (the S1 hand is `offers.md` R-OFF-09). Control on the boss converting into a vulnerability: S2 (S1: `boss.md` R-BOS-03, R-BOS-11).

## Open
- All `GATED(S1)` values → resolved by the probes (`Docs/Validation/S1_keeper_power.md`, which reads both bands today and lists the RULE candidates for R-KEE-15 and R-KEE-18) and the S1 validation sessions (O-008).
- Implemented in `Source/Dusktracer/Keepers/` (S1 keepers package). Notes from that build, for the next spec pass:
  - Tier lines and Intercession overlays fold into one flag source per Keeper (`Keeper.<Path>`): a `+x` adds, a plain value replaces. A tier clause with `every_n_volley` and its own `pattern` is a separate per-volley entry (Tempest's ring); the others (Downpour, Whirlpool) change the per-hit entry because the weapon system has no per-volley Echo or Pull.
  - The Intercession bases name mechanics without every number: child damage, chain range and damage, ring damage and knockback, and the Tide line's gap are read from the Path's own cards in `expression.csv` (first card naming the parameter); a screen-wide scope is `C.ARENA.CAMERA_FRAME_RADIUS_UU`.
  - Frost's `frozen_vuln` is expressed by the Keeper layer as an extra `Keeper.Tide` hit on Frozen enemies (no other system owns it).
  - Time Dilation's slow-motion (R-KEE-35) is the same gap as the next line.
  - Stillness's `time_scale` stays unexpressed: a global scale changes the fixed-step clock (R-TEC-03), and an enemy-only scale stretches telegraphs, which never scale (R-DIR-10).
  - Parameters no reader names, on purpose: `counts_still`, `window_s`, `dash_keeps`, `weapons` (Stillness; defaults the code already honours: a walking hero's anchor, `charge_s`, and every weapon carrying the flag). Each is a Keeper parameter, so the owner is this page plus `weapons.md`.
  - `all_volleys_charged` has no reader of its own: the cast's overlay charges in one step, so every volley is charged (`Param.StillnessAllVolleysCharged` pins it). `freeze_until_hit` is a re-applied Freeze because `enemies.md` owns the status; `frozen_blocks_enemies` is the displacement queue, so a wall holds to within a step of movement.
  - `S1_tide_expression.md` holds the arms after every card above was expressed. Pull at full `distance_uu` killed Whirlpool early (enemies near an impact moved toward a point next to the hero several times a second), which R-KEE-21 answers; `S1_pull_rule.md` holds the reading.
  - Intercession state is per player inside the Keeper system (keyed by `OwnerId`), not in `FDTPlayerRunState`; moving it there is a `Core/` change.
  - `ST_Keywords` holds the status notes and the flags S1 cards name (`Status.*`; `Flag.Split Chain Echo ConvertMelee Field Slam Pull StatusApply Stillness Ricochet`); a card's `Kw` column names what its own rules text uses, and the offer screen draws those notes (`offers.md` R-OFF-13).
