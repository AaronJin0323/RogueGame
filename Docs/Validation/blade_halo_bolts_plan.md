# Blade Halo keeps its bolts (Draft 17)

For the scheduler, from the Mechanics module session (D-055 item 4): the recommendation on Blade Halo's form that D-075 asks for once its arm is read. Written on `plan/blade-halo-bolts` from `main` at 2797d6d. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it.

Build: none read here. The numbers are D-071's job (`s2/d071-blade-halo`, tip 5cf4387, kept and not landed) and D-075's job (`s2/d075-bot-reach-arm`, tip 45da24e, kept and not handed over; its note is on `s2/d075-note`). Pair `Steered:Storm:Tempest:Tide` with `-Carry Weapon.OrbitalDrones`, 15 seeds, bot v2 at skill 1.0, the source's halo merged onto `dev/s2` 281959d. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

A constant or a test id that exists only on `dev/s2` is written without its prefix. Every figure marked "arithmetic" is mine and not a reading.

## What D-075 read
The arm missed its own clause twice and is out (D-075 ruling 3). No third build was made.

| After the pick, the source's halo | Standard bot | Arm, first build (crowd and kite terms) | Arm, second build (and the body term) | Hold (plain drones) |
|---|---|---|---|---|
| Runs that evolved, of 15 | 15 | 14 | 11 (4 died before the hand) | 0 |
| The weapon's hits a minute (bar 180) | 61 | 69 | 71 | 247 |
| Nearest enemy, median (p25, p75), uu | 201 (174, 253) | 187 (163, 276) | 176 (144, 256) | 434 (352, 475) |
| Damage taken a minute before the boss | 22.0 | 34.1 | 63.0 | 1.7 |
| Died before the boss, of those evolved | 7 | 9 | 8 | 0 of 15 |
| Reached the boss, won | 8, 4 | 5, 5 | 3, 3 | 15, 15 |
| Before the boss, against the hold | 0.89 | 0.78 | 0.71 | |
| Boss fight, against the hold (runs) | 0.55 (8) | 0.64 (5) | 0.59 (3) | |

- My draft named two terms of the bot's standoff. There are three: the one that sets the ring is the body term of the path's threat, which looks 0.6 s ahead (`Bot/DTBotBrain.cpp`, `ThreatAlong`). The first build left it whole. The second build shortened it to the bot's own dash threshold.
- Each step closer cost more than it gave. From the standard bot to the second build the nearest body came 25 uu closer, the halo landed 10 more hits a minute, and the damage taken nearly tripled.
- The blades reach a Swarm at 212 uu from the hero (orbit 150, contact 30, its radius 32). In both builds the nearest body's median is inside that, and the hits still stay near 70. Why is not traced.
- By the record the arm reads nothing about the halo, because it never made the bot use the weapon. What the two columns say as printed evidence: a bot that stops walking away does not make a contact weapon work, and it dies of trying. A bot that walks into its crowd is a new behaviour and a wider job, and I do not propose it.
- So there is still no reading, by any bot, in which the source's halo holds a run. D-074's item 7 then stands as written: an evolution that deletes its weapon's emission is not built again without one. D-075's ruling 4 names what follows: the additive form.

## Draft 17. The drones keep firing and gain the blades

- **Class:** RULE (`dev/s2` `reforging.md` R-RFG-07 and the row `Evolve.OrbitalDrones`). The blades' cells are the source's, as D-071 read them; no new number
- **Lowest branch:** `dev/s2`. **Before session A:** yes; S2 content, and `main` is not touched
- **Order:** after D-074 (landed). Before D-073, which is built last on a base that holds this rule

### Options
- **A — the additive form.** The evolved weapon fires its bolts as the plain drones do, and four blades turn about the hero on the source's rhythm and strike what they touch.
- **B — the source's form, held for people.** D-071's branch as built, its cost on the card, asked in the S2 kit. Until people play it, every battery that evolves the drones reads a pick that loses the run, and the offers would have to hold the card back.
- **C — as today on `dev/s2`.** The port's halo: 0.80 before the boss and 0.67 in the fight on the same base.

### Decision (recommended)
A.
- R-RFG-07 becomes: a Blade Halo volley fires its bolts as the weapon does, with the weapon's count, damage, cooldown and whatever its modifiers do; and `blades` blades turn about the hero at `orbit_deg_s` on an orbit that rests at `orbit_r_uu` and widens with the hero's speed up to `speed_cap`; each `contact_tick_s` every blade strikes every enemy within `contact_r_uu` of it, the enemy's radius counted, for `dmg_ratio` of the row's damage.
- The row's caps become the plain weapon's, `Orbit` and `Projectile`. No socket goes dormant at the pick: Pierce and Backwash stay live on the bolts.
- A blade's strike carries what the Pulse Wall's ring strike carries (R-RFG-12, D-074): the Patron's status, and nothing a modifier places or moves. A strike's kill still counts for the modifiers that act on a kill.
- The chain. On D-071's branch a blade's strike starts a Keeper's chain, and that is why the Arc Grid pair stood at its bars. With the bolts back, a chain's jump spends one of the weapon's emissions, as it did on the wall. The job reads a throttle scene first (level 8, bodies standing in the blades, Arc Grid's Key composed): if the bolts spawned are the same with and without the chain, the strike keeps the chain; if any bolt is refused, a blade's strike carries status and not the chain, as D-074's ruling 10 decided for the ring, and the scene is read again. Either way the outcome is written in R-RFG-07 and held by a test.
- A free volley, a kill's or a dash's, fires its bolts as any weapon's free volley does, and every blade strikes once where it is, as today (D-038, D-039).
- Honestly, against R-RFG-18's "a transformation changes how a build kills": this form changes little. It is the drones with a guard ring that this bot seldom touches. It is the safe form, as the wall's was, and of the three it is the least like the source's card. What makes it more than the plain weapon is for people to say: whether the blades are worth standing near.

### Hypothesis
We believe that a Blade Halo that keeps its bolts holds its build, because the evolved weapon then lands what the plain drones land and the blades add to it; measured by `Tools/BotRuns.ps1 -Carry Weapon.OrbitalDrones` on `Steered:Storm:Tempest:Tide`, 15 seeds, bot v2 at skill 1.0, read by `Tools/evolve_read.py --check-windows 0.95 0.9` against the hold on the same base and seeds, with the port's arm on that base beside it; judged: the build after the pick reads at least 0.95 of the hold before the boss and at least 0.9 in the boss fight, and the weapon's own hits a minute are at least 0.95 of the hold's drones; printed and judged nowhere: the second pair `Steered:Storm:ArcGrid:Tide`, the blades' share of the weapon's damage, the weapon's throttled count on both pairs, damage taken, deaths before the boss, reach and wins; reverted if a window or the hits clause is missed, and then nothing else is built for the halo without a new record.

- By arithmetic the blades add about 1 200 to 1 400 a minute before the boss to the hold's build, a few parts in a hundred. The bars are the wall's, which the same form met at 1.01 and 1.12.

### The perf bound
A before and after pair on `Tools/Perf.ps1 -Scene S2`, read in turn on one evening, with the scene's option that puts the Orbital Drones in the Spiral Projector's place (granted by the scheduler as way (1) and put on hold until a halo rule stood to be landed; it comes off hold with this record). The stop line is 0.3 ms of game thread at p95 against the base read beside it.

### Impact (dependency sweep)
- Rules replaced: `reforging.md` R-RFG-07 and T-RFG-07; one sentence each in `keepers.md` R-KEE-34 and `modifiers.md` R-MOD-16 (the free volley now fires bolts as well).
- Catalog cells, each with its number-log row and the source's lines as D-071's note cites them: `caps` Orbit to Orbit and Projectile; `dmg_ratio` 1.5 to 1.33; `contact_r_uu` 60 to 30; new `contact_tick_s` 0.15, `orbit_deg_s` 260, `orbit_r_uu` 150, `speed_cap` 1.5. `matrix.csv` is generated again and its diff shown: only this row may move.
- Constants: none.
- Code: the blades' rhythm and orbit come from `s2/d071-blade-halo`; the job is cut from `dev/s2` 41d510b, takes that branch's blade code and cells, and restores the bolts. The developer says how (a merge or a pick) before building.
- Tests: `Dusktracer.Reforging.BladeHalo` (bolts fire; blades strike on the tick), the two free-volley tests named in D-071's T-RFG-07, one throttle scene. `C.GATE.TEST_FLOOR` follows the count.
- D-073 (the refund): Blade Halo no longer refuses a modifier, so its table loses that row and its battery reads four pairs, not five.
- Discoveries, walked by counter on `dev/s3`'s `discoveries.csv`: `DroneKills300` counts the weapon's kills; with the bolts kept an evolved weapon counts as the plain one does or faster. No other counter reads this weapon.
- Text: the card's line says "Drones become blades that cut what they touch." It becomes a line that says the drones keep firing and gain blades, en and zh, the zh a first draft. What a halo with bolts looks like is the Visual session's, asked when this is accepted.
- Readings that move: every run that evolves the drones. The five seed 9107 arms are read by hand on the branch's build and reported, as every `dev/s2` hand-over is.
- Branches kept and not landed: `s2/d071-blade-halo` (the source's form, for option B at the first S2 session if people ask for it) and `s2/d075-bot-reach-arm` (the arm).
- The S2 kit: two questions. Did you stand near enemies to use the blades? Did the blades matter?
- Not shown by any of this: how a person plays it; why the source's blades land so few hits with a body inside their reach.

### Swap-out point
The first S2 session with people.
