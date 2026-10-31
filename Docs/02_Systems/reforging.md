# Reforging — the L16 transformations (S2)

## Intent
At `C.OFFER.REFORGING_LEVEL` the build changes kind (`00_Vision §2`; `offers.md` Intent). `offers.md` R-OFF-09 owns the hand; this spec owns what the cards are and what they do. A transformation changes *how* a build kills, not how much (source: `build_depth_spec` R5); a Reforging card is never a stat line. Everything here runs on the player's existing weapons, rounds, hits, fields and displacements: no new verb, no Actor, no new art.

## Rules
**Catalog and hand**
- **R-RFG-01** Every Keeper of the run has its Path row. Every Reforging card is one row of `09_Catalog/reforging.csv`: `Transform.<Path>` (kind `Path`) or `Evolve.<Weapon>` (kind `Evolve`). Parameters live in the `effect` column; code holds none of them. `expression.csv` has no transformation column, and a row in this catalog stands for it (`keepers.md` Out of scope).
- **R-RFG-02** Sources of the hand (R-OFF-09), in slot order: the Patron's Path row, the second Keeper's Path row, the evolution of the primary weapon. A Path row is eligible at any Favor (source). The primary weapon is the owned weapon of load rank one (R-WPN-03); its evolution is eligible at `C.OFFER.REFORGE_EVOLVE_MIN_LEVEL` or above, while it is not already evolved. A Keeper without a row, or a row already taken, yields its slot (R-OFF-09). Reforging cards are Epic, carry weight zero, and refuse tools (R-OFF-10). The card's `TargetWeapon` names the evolved weapon, and none for a Path row.
- **R-RFG-03** A Path pick is added to `FDTPlayerRunState::PathReforges` and applies to every weapon the player owns, now or later. An Evolve pick sets `FDTWeaponInstance::Evolve` on that weapon. A pick grants no Favor and no tier: it is not a blessing. Hits a Path row causes carry `Keeper.<Path>` (R-KEE-13); hits an evolved weapon causes carry `Weapon.<Name>` (R-WPN-07).
- **R-RFG-04** An evolved weapon keeps its id, level, slot, load rank, source and sockets, and takes upgrades as before; its level row still gives the base numbers, and composition applies the row's `effect` between step one and step two of R-WPN-09. Its declared caps become the row's `caps`, so flags resolve on them (R-WPN-08), and a modifier the new caps cannot express goes dormant (R-MOD-08), never refunded (`modifiers.md` Out of scope). The offer pool reads these caps (R-OFF-03).

**Evolutions**
- **R-RFG-05** Railstorm (`OrbitLaunch`): `blades` blades are drawn around the hero; one launches at the nearest enemy every `launch_s` as a boomerang round with `pierce`, for `dmg_ratio` of the row's damage times count.
- **R-RFG-06** Flechette (`EmbedBlast`): `pellets` pellets share the row's total damage; a pellet's hit leaves a strike that lands `embed_s` later within `embed_r_uu` for `blast_dmg` of the pellet.
- **R-RFG-07** Blade Halo (`ContactBlades`): `blades` blades and no bolts. In turn, each row-cooldown share, one blade strikes every enemy within `contact_r_uu` for `dmg_ratio` of the row's damage. Orbit radius is `C.WEAPON.DRONE_ORBIT_RADIUS_UU` × (one + `speed_ratio` × hero speed / `speed_ref_uu_s`).
- **R-RFG-08** Sweep Beam (`SweepBeam`): aimed at the densest line (R-WPN-15), a beam of `reach_uu` crosses `sweep_deg` centred on it over `beam_s`, as an arc of `width_deg` every `tick_s` for `dmg_ratio` of the row's damage times count. The cooldown is at least `beam_s`.
- **R-RFG-09** Carpet (`CarpetLine`): a landing adds `blasts` minus one strikes along the shell's flight line, spread over `walk_uu`, one every `step_s`, for `dmg_ratio` of the shell. They are strikes, not landings, so they do not walk again.
- **R-RFG-10** Cyclone (`MoveSpin`): a sweep of `arc_deg`, `hits_s` times a second, for `dmg_ratio` of the row's damage times count, only while the hero moves faster than `move_min_uu_s`; standing still it keeps its cooldown ready.
- **R-RFG-11** Murmuration (`Persist`): missiles live `life_s`, with `pierce`, for `dmg_ratio` of the row's damage, and take a new target after each hit, never one the round already struck.
- **R-RFG-12** Pulse Wall (`PulseWall`): a volley is one ring at the hero, growing to `radius_uu` over `grow_s`, holding for `hold_s`. Each `tick_s`, each enemy in its outer `band_uu` is struck for `dps` × the composed `Damage` × `tick_s`, one round's damage and not the weapon's whole output (D-020), as impacts so flags still react, and every step they are pushed out at `push_uu_s` through displacements. One ring per weapon lives at a time; the cooldown is at least the ring's life. It draws as two beaded arcs, the edges of the band, not a filled disc: what stands inside it is untouched.
- **R-RFG-13** Vortex (`Vortex`): rounds curl at `curl_deg_s` and reach `vortex_uu`; a round's expiry opens a pull zone of `zone_r_uu` for `zone_s` at `pull_uu_s`, unless the weapon has one live.
- **R-RFG-14** Singularity (`Merge`): a mine placed while one is live adds its damage to it instead of placing a second; the live mine pulls within `pull_uu` and blasts at `blast_ratio` × the area.

**Path transformations** (the S1 Keepers)
- **R-RFG-15** Overload (`Overload`): each Shock this owner applies adds a stack on that enemy, up to `stacks`. At `stacks` the enemy discharges and its stacks clear: `jumps` other enemies within `range_uu`, nearest first, each take `dmg` × the owner's strongest per-hit damage (R-KEE-13 source) and one Shock. A kill on a Shocked enemy discharges from where it died. One enemy discharges at most once per `cooldown_s`.
- **R-RFG-16** Kiln (`Kiln`): an enemy that dies Burning leaves a husk for `husk_s`, at most `husks_max` live; a new husk at the cap is not made. Each `cleave_s` a husk strikes enemies within `cleave_r_uu` for `dmg` × the owner's strongest per-hit damage.
- **R-RFG-17** Glacier (`Glacier`): an enemy this owner freezes becomes a pillar, at most `pillars_max`. Its Freeze holds for `pillar_s` from then. While it holds, enemies are pushed out of it and enemy rounds touching it are removed. When any of the owner's hits strike it, or its time ends, it shatters: `shatter_dmg` × the strongest per-hit damage and one Chill to enemies within `shatter_uu`. The enemy lives on unless that kills it.

**Path transformations of the S2 Keepers** (`build_depth_spec` as reworked by D-41 in `keeper_identity_spec`; they run only when a run unlocks that Keeper, `keepers.md` R-KEE-23)
- **R-RFG-19** Collection (`Collection`): a kill of an enemy this owner Marked raises a Shade, at most `shades_max`; at the cap the oldest goes, and the Shade that replaces it takes its seat and that seat's cooldown. A Shade sits on the drone orbit around the hero, and when an enemy is in the range of the owner's first weapon (acquisition order, the source's word) it fires that weapon's pattern at `dmg` of its damage on that weapon's cooldown, from its seat, with no walk and no end. Its hits carry that weapon's slot, so modifiers and flags react, and the source `Keeper.Shade`, as a hunter's do (R-KEE-27, R-KEE-28). A Shade counts against no hunter cap. **Adapted:** the source says "base pattern"; the composed pattern is what the hunters already fire.
- **R-RFG-20** Zenith (`Zenith`): an owner with no `Flag.Charge` card gains that flag at `every_n` and `mult`, and one with no `Flag.Nova` card gains that flag at `nova_uu` and `nova_dmg`; a card's own flag wins, so its numbers stand. Each charged shot also fans `lances` lances along its line over `fan_deg`, from where it left, each for `lance_dmg` of the charged shot with `lance_pierce` pierce, generation one, running no reaction (R-KEE-32). **Invented:** a build with no Charge card gets one, or the card reads dead in Prism and Solar; the fan at the muzzle rather than at the hit, so it works at every cap and a piercing shot splits once; `fan_deg`, `lance_pierce`.
- **R-RFG-21** Slipstream Drive (`Slipstream`): `dash_charges` more dash charges. An owner with no `Flag.VolleyOnDash` card gains it at `dmg_mult`, one with no `Flag.SpeedScale` card gains it at the row's numbers; a card's own flag wins. Momentum has no cap while this row is owned: it leaks `leak_per_s` of what is held each second, so a level stands where speed balances the leak, and a faster hero stands higher. A card's `stacks_max` still names when its at-the-cap lines run. **Invented:** the leak. "Without a cap" taken flat gains stacks at walking speed for a whole run, and each stack raises damage and rate together; the leak keeps the words and bounds the walk.
- **R-RFG-22** A Path row's hits are Keeper hits: they carry no weapon slot, so no proc feeds on them (`keepers.md` §2, R-KEE-13). Collection's are the exception R-RFG-19 names.

**Budget**
- **R-RFG-18** A Reforging pick is one decision and one milestone hand (R-OFF-15). It adds no hand and no XP. Its power is a reading: a steered build's output after the pick stays inside the source's band, `C.REFORGE.POWER_MIN` to `C.REFORGE.POWER_MAX`, and T-OFF-15 reads no worse than a control without it. XP income past the line D-018 measured breaks T-OFF-15, so a row that lifts XP income that far is cut, and the hand budget is not the thing that moves.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Evolution eligibility | C.OFFER.REFORGE_EVOLVE_MIN_LEVEL | GATED(S2) | R-RFG-02, T-RFG-02 |
| Power band after the pick | C.REFORGE.POWER_MIN, C.REFORGE.POWER_MAX | GATED(S2) | R-RFG-18 |
| Row parameters | `09_Catalog/reforging.csv` `effect` | GATED(S2) | R-RFG-05…17, R-RFG-19, R-RFG-20, R-RFG-21 |

Named-in-source values and first guesses are told apart in each row's `ref` and in `number_change_log.csv`.

## Data
| Field | Type | Notes |
|---|---|---|
| `reforging.csv` | catalog | `id kind source behaviour caps effect tag ref`; generated as `DTCat::Reforges` |
| `FDTWeaponInstance::Evolve` | `int32` | row, `INDEX_NONE` when not evolved |
| `FDTPlayerRunState::PathReforges` | array of `int32` | rows taken |
| `FDTWeaponSpec::Evolve` | `FDTEvolveSpec` | the row's behaviour and parameters, composed |
| `EDTEvolve`, `EDTPathReforge` | enum | one value per `behaviour` word |

## Acceptance
- T-RFG-01 `Dusktracer.Reforging.Catalog`: one row per catalog weapon and one per Keeper of the run; every evolved cap list resolves every flag native or fallback; every row has a name, a rules line and a keyword list that resolves in both cultures.
- T-RFG-02 `Dusktracer.Reforging.Hand`: all sources present give Patron, second, evolution, Epic, unbanishable; under the level the evolution slot takes a blessing; the primary is the rank-one weapon; taken rows leave; no row at all still gives the card count (T-OFF-09).
- T-RFG-03 `Dusktracer.Reforging.Pick`: a Path pick grants no Favor and no tier; an Evolve pick marks only that weapon; upgrades still apply.
- T-RFG-04 `Dusktracer.Reforging.Caps`: an evolved weapon's effective caps are the row's; a modifier it cannot express goes dormant and stays socketed; the offer pool stops showing such modifiers.
- T-RFG-05 `Dusktracer.Reforging.Railstorm`: one boomerang per `launch_s`, at the stated pierce and damage.
- T-RFG-06 `Dusktracer.Reforging.Flechette`: the pellet count, the summed damage, and a strike that lands after `embed_s`.
- T-RFG-07 `Dusktracer.Reforging.BladeHalo`: no bolts; blades strike what they touch; the radius grows with speed.
- T-RFG-08 `Dusktracer.Reforging.SweepBeam`: the beam crosses the stated angle in the stated time; enemies outside the sweep take nothing.
- T-RFG-09 `Dusktracer.Reforging.Carpet`: one landing makes the stated strikes along the line, in order, and no strike walks.
- T-RFG-10 `Dusktracer.Reforging.Cyclone`, `.CycloneStillness`: hits all round while moving, none at rest; a Stillness charge gathered at rest is carried by the first spin, not lost.
- T-RFG-11 `Dusktracer.Reforging.Murmuration`: a missile outlives a plain one, moves on after a hit and hits several enemies once each.
- T-RFG-12 `Dusktracer.Reforging.PulseWall`, `.PulseWallGap`, `.PulseWallLook`: one ring; it grows, hurts its band at `dps` × `Damage` × `tick_s` a tick, and pushes out through displacements only; a ring follows a ring with no volley lost; it draws both edges of the band and no disc.
- T-RFG-13 `Dusktracer.Reforging.Vortex`: rounds turn; an expiry opens one pull zone.
- T-RFG-14 `Dusktracer.Reforging.Singularity`: one live mine, summed damage, the longer pull.
- T-RFG-15 `Dusktracer.Reforging.Overload`: stacks count, discharge at the limit, the cooldown holds, a Shocked kill discharges, the source is the Keeper, no weapon slot.
- T-RFG-16 `Dusktracer.Reforging.Kiln`: a Burning death leaves a husk, a plain death none; the cap holds; husks strike and expire, with no weapon slot.
- T-RFG-17 `Dusktracer.Reforging.Glacier`, `.GlacierNeighbour`: Freeze holds past its length; a strike shatters, with no weapon slot; enemy rounds and bodies stop at a pillar; the cap holds; a shatter leaves the next pillar standing.
- T-RFG-18 `Dusktracer.Reforging.Determinism`: the same seed and picks give the same hits; `Tools/BotRuns.ps1 -Check T-OFF-15` plus `Docs/Validation/S2_reforging.md` are the budget reading (a bot bound).
- T-RFG-19 `Dusktracer.Reforging.Collection`: a Marked death raises a Shade, a plain death none; the cap holds, the oldest replaced; it is no hunter; it rides the orbit and fires the first weapon at the stated share. `.CollectionCooldown`: a Marked kill on every step at the cap leaves a full orbit's hits unchanged.
- T-RFG-20 `Dusktracer.Reforging.Zenith`: a build with no Charge card charges and fans lances, on a round weapon and a melee one; lances never split; a card's Charge wins; an overkill bursts.
- T-RFG-21 `Dusktracer.Reforging.Slipstream`: a dash fires the weapons; one more charge; Momentum passes any card's cap, reaches the weapons, stands higher when faster, and stops under the leak's balance.
- T-RFG-22 `Dusktracer.Reforging.Overload`, `.Kiln`, `.Glacier`: discharges, cleaves and shatters report no weapon slot.
- HUMAN: at L16 a validator can say, in their own words, what changed on screen within two seconds of the pick, for each of the three cards they were offered.

## Telemetry
- `reforge{reforge_kind, id, target, weapon_level, dormant}` (`owner` = the player) on a Reforging pick, after the `offer_pick`; `dormant` joins with semicolons the modifiers the pick put to sleep. A filler blessing writes the blessing records and no `reforge`.
- The bot record gains `reforge=<id>` (`none` when the hand gave only filler or the run ended first) so a batch reads per card, and `reforge_read=pillars:n;pillar_s:s;by_hit:n;by_time:n;discharges:n;husks:n;shades:n`, what the Path rows did in the run.

## Text
- Table `ST_Reforging`: `<Id>.Name`, `<Id>.Desc` per row (`Transform.Storm.Name`), en + zh-Hans, with `Kw`. Names and most rules from `reference/text_tables/ST_Fusions.csv` and `zh_overrides.csv`; Collection, Zenith and Slipstream Drive keep the D-41 names there, with their numbers taken out. Lines name no catalog number (`weapons.md` Text). `ST_UI`: `Offer.Line.Evolves` (binding line, `{Weapon}`), `Offer.Line.PathReforge` (binding line, `{Keeper}`).

## Out of scope
- Fusions (R-OFF-09 names three sources; fusions are not one), the sealed card and a second hand at L24 (`offers.md` Out of scope, S3+), Slack Water (needs water, arena two), refunds for dormant modifiers.
- Transformation art, audio and density LOD (S4): evolved weapons draw with the arc, field, drone and mine visuals that exist.

## Open
- Hit source of an evolved weapon stays `Weapon.<Name>`, so `Tools/power_split.py` and T-KEE-18 count it outside the Keeper share → read once a human reading exists.
- Cyclone asks for motion and Stillness asks for rest: the first spin carries a charge gathered at rest (T-RFG-10), but at rest the weapon is silent → a poor pairing, no bug; forbidden only by a human reading.
- A pillar shatters on the owner's next hit, and most end that way, well short of `pillar_s` (`Validation/S2_reforging.md`) → what the rule says; a human reading decides if it is a wall.
- Few steered primaries reach `C.OFFER.REFORGE_EVOLVE_MIN_LEVEL` by the hand: the cause is the bot's card scores, not the offers (`Validation/S2_reforging.md`) → human weapon levels at the hand are still unread.
- The power band is unread: output after the pick matches the control, which may be a field cap → a measure that a full-clear build cannot saturate, before any row moves.
- Fusions are named by `build_depth_spec` and absent from R-OFF-09 → stays out unless a decision adds a fourth source.
- Path rows are eligible at any Favor (source), so a Path whose status the build never applies is a weak card → the hand has two others; read it. Collection needs Marks, and a second-Keeper Shade Marks at the second's chance only.
