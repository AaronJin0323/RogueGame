# Blade Halo: every blade strikes, as the source's do (Draft 12)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for on the board on 2026-11-12 04:41. Written on `plan/murmuration-blade-halo` from `main` at cdbce7b. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it. It is built, if accepted, after D-070 has its reading, and before the Murmuration draft beside it.

Build: none read here. The numbers of play come from run files that exist: the batches `ev_arm` and `ev_hold` of `Validation/S2_evolutions.md` (`dev/s2`), read by `Tools/evolve_read.py` as it stands on `dev/s2` at 73e9c3c, which splits the time after the Reforging pick at the boss's start. The pair is `Steered:Storm:Tempest:Tide` with `-Carry Weapon.OrbitalDrones`, 15 seeds, bot v2 at skill 1.0; the hold takes the Path card Overload where the arm takes the evolution. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

A constant or a test id that exists only on `dev/s2` is written without its prefix. Every figure marked "arithmetic" is mine and not a reading. The source's lines are quoted as `Validation/S2_evolution_cells_source.md` (`dev/s2`) read them; I did not read these lines of the source myself.

## What the run files say: the loss is before the boss
| After the pick | Hold | Blade Halo |
|---|---|---|
| Died before the boss, of 15 | 0 | 11 |
| Before the boss: all sources, damage per minute | 17 526 | 14 555 |
| ... against the hold | 1.00 | 0.83 |
| Before the boss: the weapon's own damage per minute | 3 514 | 1 326 |
| Before the boss: kills per minute | 354 | 326 |
| Before the boss: damage taken per minute | 1.6 | 18.1 |
| Reached the boss, of 15 | 15 | 4 |
| Boss fight: all sources, damage per minute | 8 663 | 11 943 (4 runs) |
| Boss fight: the weapon's own damage per minute | 1 885 | 1 180 (4 runs) |
| Boss fight: damage taken per minute | 11.3 | 43.3 (4 runs) |
| Boss HP left at the run's end | 0.00 | 0.40 (4 runs) |

- The evolved weapon deals 0.38 of what the plain drones deal before the boss, the hero takes eleven times the damage, and eleven of fifteen runs die there. The fight's 1.38 rests on four runs that then died in it.
- `S2_evolutions.md` already said why in words: "Blade Halo trades reach for contact, and the bot does not stand in contact." Its hits fall from 226 to 63 a minute.

## What the source does, beside the port
| Cell or behaviour | Port (`reforging.md` R-RFG-07, `reforging.csv`) | Source (`RHWeaponDef.cpp` 593 to 602 and the lines named) |
|---|---|---|
| Who strikes, how often | "In turn, each row-cooldown share, one blade strikes every enemy within `contact_r_uu`". In the code (`DTWeaponSim.cpp`, `StepBlades`) one blade strikes every cooldown over blades: at level 6 a strike every 0.08 s, so each blade strikes every 0.32 s | every blade, every `ContactTick` 0.15 s, strikes every enemy within its radius (`StepOrbiterContacts`, `RHBulletSubsystem.cpp` 1407 to 1444) |
| Contact radius | `contact_r_uu` 60 | `ContactRadius` 30, times the hero's Area, plus the enemy's own radius |
| Damage per strike | `dmg_ratio` 1.5 of the row's: 27 at level 6 (arithmetic) | 24 at level 6 (arithmetic from `Base.Damage` 14 and the plain track) |
| Orbit speed | no cell: the blades turn at the plain drones' 120 degrees a second | `OrbitDegPerSec` 260, where the plain drones turn at 120 |
| Orbit radius | 160 at rest, widening with the hero's speed, no cap named | 150 at rest, widening capped at 1.5 times the reference speed (`RHWeaponInstance.cpp` 826 to 829) |
| Blades, bolts, capabilities | 4 blades, no bolts, Orbit | the same |

- Arithmetic, to size the difference, for an enemy that crosses the orbit on its way to the hero (about half a second in a blade's reach of the ring). On the port the blades pass a point of the ring 1.3 times a second and a blade over the enemy strikes it about once: about 0.6 strikes a crossing, 16 damage. In the source the blades pass 2.9 times a second and strike every 0.15 s: about 1.4 strikes a crossing, 34 damage. So the source's halo deals an enemy that crosses it about twice what the port's does, not many times.
- The port's own row says "`dmg_ratio` first guess". The rhythm and the orbit's speed are not marked as guesses, but no document of the Handoff states them; the source's code does.

## Draft 12. Every blade strikes what it touches, on the source's rhythm

- **Class:** RULE (`dev/s2` `reforging.md` R-RFG-07), with the row's cells set to the source's values in the same step, each cited to the source and none tuned, as D-070 did for the wall
- **Lowest branch:** `dev/s2`. **Before session A:** yes; S2 content
- **Order:** after D-070's reading (one RULE on `reforging.md` at a time), and before Draft 13 (Murmuration), because this evolution is the one that loses the run before the boss

### Options
- **S — the source's halo.** Every blade strikes, every `contact_tick_s` (a new cell, 0.15), each enemy within `contact_r_uu` plus the enemy's own radius; `contact_r_uu` 30; `dmg_ratio` 1.33 (24 at level 6); the blades turn at `orbit_deg_s` 260 (a new cell); the orbit is 150 at rest and its widening stops at 1.5 times the reference speed.
- **R — the rhythm alone.** Every blade strikes every `contact_tick_s`; every other cell stays the port's. One behaviour and one new cell. It leaves a blade that reaches twice as far and hits an eighth harder than the source's, on an orbit that turns at the plain drones' speed.
- **D — hold the card for people.**

### Decision (recommended)
S, with a plain warning: by my arithmetic it lifts the weapon to about twice its damage on what crosses the orbit, which is about 0.8 of the plain drones' and a build at about 0.9 of its hold before the boss. That is better than 0.83 and short of 0.95, where working evolutions read. The source's halo is the same trade of reach for contact; its cells make the trade cheaper and do not remove it.

Why S and not R: the port's other cells were guessed around its own rhythm (a wide, hard blade on a slow orbit), and keeping them under the source's rhythm would be reading numbers we expect to change, which D-070's ruling 2 declined for the wall's push. The orbit's speed is half of the gain: a blade that turns at 260 degrees a second meets nearly everything that crosses the ring.

- R-RFG-07 becomes: `blades` blades and no bolts. Each `contact_tick_s`, every blade strikes every enemy within `contact_r_uu` of it, the enemy's own radius counted, for `dmg_ratio` of the row's damage. The blades turn at `orbit_deg_s`. The orbit radius is its rest radius times (one + `speed_ratio` × hero speed / `speed_ref_uu_s`), the speed counted no higher than 1.5 times `speed_ref_uu_s`.
- The free-volley clause stays as D-038 and D-039 wrote it: every blade strikes once where it is, as the volley's source and at its scale. "No blade's turn is spent" loses its meaning, since blades no longer take turns, and goes.
- Cells, each with a number-log row cited to the source: `contact_tick_s` 0.15 (new), `contact_r_uu` 60 to 30, `dmg_ratio` 1.5 to 1.33, `orbit_deg_s` 260 (new), the rest radius 160 to 150 for this evolution alone (a cell of the row, not the plain drones' constant).
- Cell or behaviour, as the scheduler asked: the rhythm, the enemy's radius in the contact test, the orbit speed and the widening's cap are behaviours (code, a test each, the perf look below). `contact_r_uu` and `dmg_ratio` are cells.
- Not taken from the source: the contact radius does not scale with an Area stat.

### The perf bound comes first, and what stops the job
- New load: a contact test for each of four blades every 0.15 s, and a strike for every enemy a blade touches. Today's weapon makes 63 hits a minute in these runs; the source's rhythm will make many times that.
- The job reads the S2 perf scene twice on `dev/s2`'s build and twice on the branch's. It first says whether that scene's build holds a Blade Halo; if it does not, it reads the scene's variant that does, if one exists without a file changed, and says so if none exists.
- It stops, with nothing more built, if the game thread at p95 rises by more than 0.3 ms over the before pair or an after run fails the scene's budget, D-070's line. The scheduler then takes the reading alone.

### Hypothesis
We believe that letting every blade strike on the source's rhythm, on an orbit that turns at the source's speed, moves the build's damage per minute before the boss from 0.83 of its hold to at least 0.9, because an enemy that crosses the orbit is then struck about 1.4 times where today it is struck about 0.6 times, and the weapon's own damage there stands at 0.38 of the plain drones'; and that in the boss fight the build reads at least 0.9 of its hold; measured by `Tools/BotRuns.ps1 -Carry` on the Orbital Drones pair, 15 seeds, bot v2 at skill 1.0, read by `Tools/evolve_read.py --check-windows 0.9 0.9`; the fight's window is judged only if at least ten of the fifteen evolved runs reach the boss, since today's fight figure rests on four, and it is printed with its count otherwise; the whole-time figure, the hits per minute, the damage taken and the nearest enemy are printed and decide nothing; reach and wins printed and judged nowhere; reverted, the rule and its cells put back, if the build reads under 0.9 of its hold before the boss, or under 0.9 in a fight that ten runs reached.

- Why 0.9 before the boss and not 0.95: 0.9 is what the source's cells should give by my arithmetic, and a bar the rule is expected to miss would revert a rule that helps. It is not where this ends: a pass is followed by one TUNE pass on `dmg_ratio` upward by `10_Tuning.md §5`, judged on the window before the boss, whose aim is 0.95 or more. That step has its own hypothesis and its own revert. If the scheduler holds that the rule itself must reach 0.95, the bar is 0.95 and I expect a miss.
- Why the fight bar is parity here and was 0.5 for the wall: the halo gives up no single-target damage by what it is. A boss that stands on the hero is inside the orbit, and whether a blade reaches it turns on the enemy's own radius in the contact test, which this rule adds. If the reading shows the boss untouched, that is a finding about the orbit and the boss's size, and the answer is a step of its own.
- What follows an overshoot: above `REFORGE.POWER_MAX` before the boss, one TUNE pass on `dmg_ratio` downward, as its own step.
- What a bot can and cannot read: the bot kites and does not seek contact, as the evolutions note says. Enemies that chase it cross the orbit, so the bot can read whether they die there; it cannot read a player who steers the halo into a crowd.

### Impact (dependency sweep)
- Rules replaced: R-RFG-07 and its test line (every blade strikes on the tick; the enemy's radius counts; the orbit turns at its own speed; the widening stops at the cap; the free volley's strike is unchanged).
- Rules read again: `keepers.md` R-KEE-34 and `modifiers.md` R-MOD-16 for the free volley (D-038, D-039): unchanged in sense.
- Catalog: `reforging.csv`, `Evolve.OrbitalDrones`: the cells above. Constants: none; the plain drones' orbit constant is not touched.
- Modifiers: the capabilities stay Orbit, so no socket changes state.
- Discoveries, walked by counter on `dev/s3`'s `discoveries.csv`: `Kills.Weapon.OrbitalDrones` (`DroneKills300`, which opens Lingering Field) counts the evolved weapon's kills, and it will count faster; that row already opens on a first run with the plain drones, long before a Reforging hand. `Kills` and `Status.Freeze` count faster and open nothing new. `Best.Crit`: a strike of 24 is far under 500. No other counter is reached.
- Files / tests: `02_Systems/reforging.md`; `Combat/DTWeaponSim.cpp` (the contact blades' step); the Reforging tests; `reforging.csv` and the number log.
- Readings that move: runs that evolve the Orbital Drones; the S2 perf scene if it holds one. No run of the S2 exit battery evolves it.
- Presentation: blades that turn at 260 degrees a second where they turned at 120 will look different. The look is the Visual session's; the job changes the speed and nothing of the drawing.
- Not shown by any of this: no person has played a Blade Halo.

### Swap-out point
The first S2 session with people.
