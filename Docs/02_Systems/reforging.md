# Reforging — the L16 transformations (S2)

## Intent
At `C.OFFER.REFORGING_LEVEL` the build changes kind (`00_Vision §2`; `offers.md` Intent). `offers.md` R-OFF-09 owns the hand; this spec owns what the cards are and what they do. A transformation changes *how* a build kills, not how much (source: `build_depth_spec` R5); a Reforging card is never a stat line. Everything here runs on the player's existing weapons, rounds, hits, fields and displacements: no new verb, no Actor, no new art.

## Rules
**Catalog and hand**
- **R-RFG-01** Every Reforging card is one row of `09_Catalog/reforging.csv`: `Transform.<Path>` (kind `Path`) or `Evolve.<Weapon>` (kind `Evolve`). Parameters live in the `effect` column; code holds none of them. `expression.csv` has no transformation column, and a row in this catalog stands for it (`keepers.md` Out of scope).
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
- **R-RFG-12** Pulse Wall (`PulseWall`): a volley is one ring at the hero, growing to `radius_uu` over `grow_s`, holding for `hold_s`. Each `tick_s`, enemies in its outer `band_uu` are struck for `dps` × the composed damage per second, as impacts so flags still react, and every step they are pushed out at `push_uu_s` through displacements. One ring per weapon lives at a time; the cooldown is at least the ring's life.
- **R-RFG-13** Vortex (`Vortex`): rounds curl at `curl_deg_s` and reach `vortex_uu`; a round's expiry opens a pull zone of `zone_r_uu` for `zone_s` at `pull_uu_s`, unless the weapon has one live.
- **R-RFG-14** Singularity (`Merge`): a mine placed while one is live adds its damage to it instead of placing a second; the live mine pulls within `pull_uu` and blasts at `blast_ratio` × the area.

**Path transformations** (the S1 Keepers; a Keeper's row arrives with that Keeper)
- **R-RFG-15** Overload (`Overload`): each Shock this owner applies adds a stack on that enemy, up to `stacks`. At `stacks` the enemy discharges and its stacks clear: `jumps` other enemies within `range_uu`, nearest first, each take `dmg` × the owner's strongest per-hit damage (R-KEE-13 source) and one Shock. A kill on a Shocked enemy discharges from where it died. One enemy discharges at most once per `cooldown_s`.
- **R-RFG-16** Kiln (`Kiln`): an enemy that dies Burning leaves a husk for `husk_s`, at most `husks_max` live; a new husk at the cap is not made. Each `cleave_s` a husk strikes enemies within `cleave_r_uu` for `dmg` × the owner's strongest per-hit damage.
- **R-RFG-17** Glacier (`Glacier`): an enemy this owner freezes becomes a pillar, at most `pillars_max`. Its Freeze holds for `pillar_s` from then. While it holds, enemies are pushed out of it and enemy rounds touching it are removed. When any of the owner's hits strike it, or its time ends, it shatters: `shatter_dmg` × the strongest per-hit damage and one Chill to enemies within `shatter_uu`. The enemy lives on unless that kills it.

**Budget**
- **R-RFG-18** A Reforging pick is one decision and one milestone hand (R-OFF-15). It adds no hand and no XP. Its power is a reading: a steered build's output after the pick stays inside the source's band, `C.REFORGE.POWER_MIN` to `C.REFORGE.POWER_MAX`, and T-OFF-15 reads no worse than a control without it. Income above about 0.85 of the model breaks T-OFF-15 (D-018), so a row that lifts XP income that far is cut, and the hand budget is not the thing that moves.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Evolution eligibility | C.OFFER.REFORGE_EVOLVE_MIN_LEVEL | GATED(S2) | R-RFG-02, T-RFG-02 |
| Power band after the pick | C.REFORGE.POWER_MIN, C.REFORGE.POWER_MAX | GATED(S2) | R-RFG-18 |
| Row parameters | `09_Catalog/reforging.csv` `effect` | GATED(S2) | R-RFG-05…17 |

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
- T-RFG-10 `Dusktracer.Reforging.Cyclone`: hits all round while moving, none at rest.
- T-RFG-11 `Dusktracer.Reforging.Murmuration`: a missile outlives a plain one, moves on after a hit and hits several enemies once each.
- T-RFG-12 `Dusktracer.Reforging.PulseWall`: one ring; it grows, hurts its band, and pushes out through displacements only.
- T-RFG-13 `Dusktracer.Reforging.Vortex`: rounds turn; an expiry opens one pull zone.
- T-RFG-14 `Dusktracer.Reforging.Singularity`: one live mine, summed damage, the longer pull.
- T-RFG-15 `Dusktracer.Reforging.Overload`: stacks count, discharge at the limit, the cooldown holds, a Shocked kill discharges, the source is the Keeper.
- T-RFG-16 `Dusktracer.Reforging.Kiln`: a Burning death leaves a husk, a plain death none; the cap holds; husks strike and expire.
- T-RFG-17 `Dusktracer.Reforging.Glacier`: Freeze holds past its length; a strike shatters; enemy rounds and bodies stop at a pillar; the cap holds.
- T-RFG-18 `Dusktracer.Reforging.Determinism`: the same seed and picks give the same hits; `Tools/BotRuns.ps1 -Check T-OFF-15` plus `Docs/Validation/S2_reforging.md` are the budget reading (a bot bound).
- HUMAN: at L16 a validator can say, in their own words, what changed on screen within two seconds of the pick, for each of the three cards they were offered.

## Telemetry
- `reforge{kind, id, target, weapon_level, dormant[]}` (`owner` = the player) on a Reforging pick, after the `offer_pick`; `dormant` lists modifiers the pick put to sleep. Filler blessings are a `keeper_pick`, not a `reforge`.
- The bot record gains `reforge=<id>` (`none` when the hand gave only filler or the run ended first) so a batch reads per card.

## Text
- Table `ST_Reforging`: `<Id>.Name`, `<Id>.Desc` per row (`Transform.Storm.Name`), en + zh-Hans, with `Kw`. Names and most rules from `reference/text_tables/ST_Fusions.csv` and `zh_overrides.csv`. Lines name no catalog number (`weapons.md` Text). `ST_UI`: `Offer.Line.Evolves` (binding line, `{Weapon}`), `Offer.Line.PathReforge` (binding line, `{Keeper}`).

## Out of scope
- Fusions (R-OFF-09 names three sources; fusions are not one), the sealed card and a second hand at L24 (`offers.md` Out of scope, S3+), Slack Water (needs water, arena two), refunds for dormant modifiers.
- Transformation art, audio and density LOD (S4): evolved weapons draw with the arc, field, drone and mine visuals that exist.
- The Path rows of Keepers four to six: they arrive with those Keepers.

## Open
- Hit source of an evolved weapon stays `Weapon.<Name>`, so `Tools/power_split.py` and T-KEE-18 count it outside the Keeper share → read once a human reading exists.
- Cyclone asks for motion and Stillness asks for rest; a Stillness build can take Cyclone and fight itself → a dead pairing to read, not yet forbidden.
- A pillar shatters on the owner's next hit, and auto-fire hits what is near, so pillars close to the hero may not last → read in `Validation/S2_reforging.md`.
- Path rows are eligible at any Favor (source), so a Path whose status the build never applies is a weak card → the hand has two others; read it.
