# Murmuration: the missile keeps its damage, as the source's does (Draft 13)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for on the board on 2026-11-12 04:41. Written on `plan/murmuration-blade-halo` from `main` at cdbce7b. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it. It is built, if accepted, after D-070 and after Draft 12 (Blade Halo) have their readings, one at a time.

Build: none read here. The numbers of play come from run files that exist: the batches `ev_arm` and `ev_hold` of `Validation/S2_evolutions.md` (`dev/s2`), read by `Tools/evolve_read.py` as it stands on `dev/s2` at 73e9c3c. The pair is `Steered:Storm:Tempest:Tide` with `-Carry Weapon.SeekerSwarm`, 15 seeds, bot v2 at skill 1.0; the hold takes the Path card Overload where the arm takes the evolution. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

A constant or a test id that exists only on `dev/s2` is written without its prefix. Every figure marked "arithmetic" is mine and not a reading. The source's lines are quoted as `Validation/S2_evolution_cells_source.md` (`dev/s2`) read them; I did not read these lines of the source myself.

## What the run files say: level before the boss, a third in the fight
| After the pick | Hold | Murmuration |
|---|---|---|
| Died before the boss, of 15 | 2 | 0 |
| Before the boss: all sources, damage per minute | 18 168 | 17 816 |
| ... against the hold | 1.00 | 0.98 |
| Before the boss: the weapon's own damage per minute | 4 278 | 5 597 |
| Before the boss: Keeper.Storm | 9 258 | 6 580 |
| Before the boss: damage taken per minute | 3.4 | 0.0 |
| Reached the boss, of 15 | 13 | 15 |
| Boss fight: length, s | 54 | 103 |
| Boss fight: all sources, damage per minute | 16 895 | 6 176 |
| ... against the hold | 1.00 | 0.37 |
| Boss fight: the weapon's own damage per minute | 7 149 | 1 835 |
| Boss fight: Keeper.Storm | 5 254 | 1 736 |
| Boss fight: damage taken per minute | 11.9 | 17.8 |
| Boss HP left at the run's end | 0.00 | 0.00 |

- Before the boss Murmuration is the better weapon: it deals more than the plain missiles, the hero takes nothing, and no run dies. `S2_evolutions.md` calls it the evolution with the best record, and that stands.
- In the boss fight the evolved weapon deals a quarter of the plain one's damage, Keeper.Storm a third, and the fight lasts twice as long. The runs still end with the boss dead.
- Why, from the rule: a Murmuration missile deals `dmg_ratio` 0.2 of the row's damage and "takes a new target after each hit, never one the round already struck". A crowd gives it many targets, so a fifth of the damage many times over is more than one full hit. A lone boss gives it one hit, at a fifth, and then nothing to turn to. What rides the hit is a fifth as well, which is where Keeper.Storm goes.

## What the source does, beside the port
| Cell or behaviour | Port (`reforging.md` R-RFG-11, `reforging.csv`) | Source (`RHWeaponDef.cpp` 652 to 656 and the lines named) |
|---|---|---|
| Damage per hit | `dmg_ratio` 0.2 of the row's, "first guess" | the plain missile's: no multiplier is set, and the missiles are the weapon's main rounds (`RHSpecComposition.cpp` 454 to 456) |
| Cooldown | no cell: the row's, 0.88 s at level 6 | `Base.Cooldown` 1.6 s, with the plain track's step at level 4: 1.28 s at level 6 (arithmetic) |
| Retarget | a new target after each hit, "never one the round already struck" | a new target after each hit through the round's history, which holds 8 enemies (`RHBulletSubsystem.cpp` 1051 to 1055, `RHBulletSubsystem.h` 121): an enemy may be struck again once eight others came after it |
| Life, pierce | `life_s` 6, `pierce` 200 | the same |
| Capabilities | Homing, Projectile, Expiry | Projectile, Homing |

- The port cut the hit to a fifth and kept the plain cadence. The source keeps the hit whole and launches about two thirds as often. In a crowd the two come out near each other only if a missile finds about three targets in its six seconds; the files say it finds more. On a lone target they are far apart: arithmetic at level 6, eight missiles of 16 a volley, the port deals 29 a second and the source 100, against the plain weapon's 145.

## Draft 13. A Murmuration missile deals the row's damage and launches on the source's cooldown

- **Class:** RULE (`dev/s2` `reforging.md` R-RFG-11), with the row's cells set to the source's values in the same step, each cited to the source and none tuned
- **Lowest branch:** `dev/s2`. **Before session A:** yes; S2 content
- **Order:** after D-070's reading and after Draft 12's. It is the least urgent of the three: this evolution wins its runs today

### Options
- **S — the source's missile.** `dmg_ratio` 0.2 becomes 1 (the row's damage); the evolved weapon's cooldown becomes the source's (a new cell, `cooldown_mult`, the source's 1.6 over the plain 1.1 at the same level, read from the source's rows when the job starts); a round may strike an enemy again once `rehit_after` others came after it (a new cell, 8).
- **H — the hit alone.** `dmg_ratio` becomes 1 and the cadence stays the port's. One cell. The evolved weapon is then the plain one with six seconds of life and pierce 200 added at no cost: stronger than the source's in every case.
- **D — leave it.** It has the best record of the ten, and a fight that lasts 103 s is a cost a person may accept.

### Decision (recommended)
S. H is one cell and would read well, but it removes the trade the source makes (a persistent cloud that launches less often), and the port would then hold a third version of this weapon that neither the source nor its own first guess is. D is defensible; the reason to act is that the same fault (a hit cut to a fifth) is what sends Keeper.Storm down with it, for every Keeper that rides a hit.

- R-RFG-11 becomes: missiles live `life_s`, with `pierce`, for `dmg_ratio` of the row's damage, launched at the row's cooldown times `cooldown_mult`, and take a new target after each hit; a round does not strike an enemy again until `rehit_after` other enemies came after it.
- Cells, each with a number-log row cited to the source: `dmg_ratio` 0.2 to 1; `cooldown_mult` (new); `rehit_after` 8 (new). The capabilities stay as they are: dropping Expiry would send Scatter Shell and Spread Wave dormant on this weapon, which is the refund record's question and not this one's.
- Cell or behaviour, as the scheduler asked: `dmg_ratio` is a cell. The cooldown is a cell that composition must learn to read (one line of code and a test). The re-strike is a behaviour: a history per round, a test, and the perf look below.
- What S may do before the boss, said now: a hit five times as hard at two thirds of the cadence is more than three times the damage a missile can deal. Before the boss the build already takes what walks in (kills 368 a minute against the hold's 364), so the damage there is capped by supply and should stay near 1.0; the weapon's share of it will rise and Keeper.Storm's will come back with the hit. If the build lands above `REFORGE.POWER_MAX` there, one TUNE pass on `dmg_ratio` downward is its own step.

### The perf bound comes first, and what stops the job
- Fewer rounds, not more: at two thirds of the cadence there are about 37 live missiles where today there are about 55 (arithmetic). The history adds eight serials to each live round. Hits per minute should fall, since a full hit kills what five fifths did.
- The job still reads the S2 perf scene twice before and twice after, says whether that scene holds a Murmuration, and stops on D-070's line (more than 0.3 ms of game thread at p95, or a failed budget).

### Hypothesis
We believe that giving the Murmuration missile the row's damage on the source's cooldown moves the build's damage per minute in the boss fight from 0.37 of its hold to at least 0.6, because a missile then deals a lone boss its whole hit where today it deals a fifth (100 a second against 29 by arithmetic, the plain weapon 145), and what rides the hit comes back with it; and that before the boss the build stays at 0.95 of its hold or more, where it reads 0.98 today; measured by `Tools/BotRuns.ps1 -Carry` on the Seeker Swarm pair, 15 seeds, bot v2 at skill 1.0, read by `Tools/evolve_read.py --check-windows 0.95 0.6`; the whole-time figure, hits per minute, damage taken and the fight's length are printed and decide nothing; reach and wins printed and judged nowhere; reverted, the rule and its cells put back, if the build reads under 0.95 of its hold before the boss or under 0.6 of it in the boss fight.

- Why 0.6 and not 0.9 in the fight: the source's missile launches two thirds as often as the plain one and cannot strike a lone boss twice, so its cells give about two thirds of the plain weapon on one target by arithmetic. A pass short of parity is followed by one TUNE pass, judged on the fight alone and aiming at 0.9; `rehit_after` is the lever I would try first, since a lower number lets a missile return to a boss, and the cooldown second.
- What a bot can and cannot read: all of it. The missiles find their own targets, so the bot's kiting does not matter here.

### Impact (dependency sweep)
- Rules replaced: R-RFG-11 and its test line (a missile deals the row's damage; the evolved cooldown; a round returns to an enemy only after `rehit_after` others).
- Catalog: `reforging.csv`, `Evolve.SeekerSwarm`: the three cells. Constants: none.
- Modifiers: the capabilities do not change, so no socket changes state.
- Discoveries, walked by counter on `dev/s3`'s `discoveries.csv`: no per-weapon counter is the Seeker Swarm's. `Kills` and `Status.Freeze` may count a little faster and open nothing new. `Best.Crit`: a full hit of 16 to about 22 is far under 500. No other counter is reached.
- Files / tests: `02_Systems/reforging.md`; `Combat/DTWeaponSim.cpp` (the persisting round's retarget) and the composition's reading of the row; the Reforging tests; `reforging.csv` and the number log.
- Readings that move: runs that evolve the Seeker Swarm; the S2 perf scene if it holds one. No run of the S2 exit battery evolves it.
- Presentation: none asked. Fewer, harder missiles draw as today's.
- Not shown by any of this: no person has played a Murmuration.

### Swap-out point
The first S2 session with people.
