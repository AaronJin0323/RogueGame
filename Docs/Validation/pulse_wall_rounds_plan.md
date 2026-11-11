# Pulse Wall keeps its rounds: one decision draft

For the scheduler, from the Mechanics module session (D-055 item 4). Written on `plan/pulse-wall-rounds` from `main` at a5134b7. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it.

Build: none read here. Every number comes from run files that exist: the D-061 batches (`d061_hold`, `d061_arm_2`, `d061_arm_8`; `Validation/S2_d061_pulse_wall.md` on `dev/s2` names their builds) and the D-067 arm (`d067_arm`, the build of `s2/d067-pulse-wall` at 1af3eac; `Validation/S2_d067_pulse_wall.md` on `s2/d067-note`). Each is the carry pair `Steered:Storm:Tempest:Tide`, `-Carry Weapon.RingEmitter`, 15 seeds, bot v2 at skill 1.0. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

A constant or a test id that exists only on `dev/s2` is written without its prefix, so the lint on `main` does not look for it.

## What the files say that the two notes did not
The two notes read the time after the Reforging pick as one window. Split at the boss's start, it is two different stories.

How it was read (two scratch scripts, no new run): per run, "before the boss" runs from the `reforge` record to the `boss_start` record, or to the run's end when the run never met the boss; "the boss fight" from `boss_start` to the run's end. Damage is summed from the `hit` records' damage by source inside each window and divided by the window's minutes. Kills per minute come from the whole 60 s `sources` intervals inside the first window. Damage taken comes from the `hero_damaged` records. Medians over the runs: all 15 before the boss, and in the fight the runs that reached it.

| After the pick | Hold | As ported, `dps` 2 | As ported, `dps` 8 | D-067's carry |
|---|---|---|---|---|
| Died before the boss, of 15 | 0 | 8 | 7 | 6 |
| Before the boss: all sources, damage per minute | 16 942 | 13 604 | 15 632 | 15 162 |
| ... against the hold | 1.00 | 0.80 | 0.92 | 0.89 |
| Before the boss: kills per minute | 365 | 319 | 327 | 328 |
| Before the boss: damage taken per minute | 1.8 | 22.3 | 21.6 | 15.9 |
| Reached the boss, of 15 | 15 | 7 | 8 | 9 |
| Boss fight: length, s | 84 | 124 | 134 | 136 |
| Boss fight: all sources, damage per minute | 16 650 | 5 143 | 4 457 | 4 433 |
| ... against the hold | 1.00 | 0.31 | 0.27 | 0.27 |
| Boss fight: damage taken per minute | 11.1 | 23.3 | 30.8 | 18.0 |
| Boss HP left at the run's end | 0.00 | 0.25 | 0.14 | 0.07 |

The boss fight by source, damage per minute:

| Source | Hold | As ported, `dps` 2 | As ported, `dps` 8 | D-067's carry |
|---|---|---|---|---|
| Weapon.RingEmitter (Pulse Wall in the arms) | 5 793 | 170 | 717 | 203 |
| Keeper.Storm | 5 900 | 2 998 | 1 347 | 1 853 |
| Weapon.PulseRifle | 1 001 | 1 179 | 1 221 | 1 047 |
| Keeper.Tide | 620 | 746 | 440 | 544 |
| Weapon.OrbitalDrones | 178 | 331 | 287 | 234 |

What this says:
- Before the boss the build stands at 0.80 to 0.92 of its hold. In the boss fight it stands at 0.27 to 0.31. `evolve_read.py`'s one figure for the whole time after the pick (0.74 to 0.78) is mostly the fight.
- In the fight the evolved weapon deals 170 to 717 a minute where the plain Ring Emitter deals 5 793. The boss stands inside the ring, where the wall strikes nothing, and the wall cannot push it.
- Keeper.Storm falls with it, from 5 900 to between 1 347 and 2 998: the weapon's hits carried it.
- So the fight lasts half again as long, the hero takes two to three times the damage in it, and most runs that reach the boss die there.
- No rule about where the crowd stands reaches this. D-067's carry left the fight where it was (203 against 170).

## Draft 10. A Pulse Wall volley keeps its rounds

- **Class:** RULE (`dev/s2` `reforging.md` R-RFG-12). No number moves
- **Lowest branch:** `dev/s2`. **Before session A:** yes; it is S2 content and no validator of session A meets it
- **Order:** it follows D-067, which was reverted by its own clause. No other RULE record on `reforging.md` waits for a reading

### Context
- The card: "Rings hold as a wall, pushing enemies out." The source: "Rings expand into a slow persistent wall ring (r 600, 3 s) that pushes enemies out."
- The rule as ported, R-RFG-12: "a volley is one ring at the hero". The evolved weapon fires no rounds. "One ring per weapon lives at a time; the cooldown is at least the ring's life."
- What that takes away, from the catalog: at level 6 the plain weapon fires 24 rounds of 11 damage every 0.96 s, reaching 1 350 uu (`weapons.csv`). The evolved weapon opens one ring every 3.6 s and strikes only what stands in its outer 100 uu, for about 5.5 a tick (`reforging.csv`: `dps` 2, `tick_s` 0.25).
- Two readings already taken. D-061: `dps` at 2, 4, 6 and 8 leaves the build at 0.76 to 0.80 of its hold, so the number is not the cause. D-067: carrying out what the ring crosses left the nearest enemy at 232 uu against 500 and the build at 0.74; it was reverted.
- The split above: the loss is the boss fight, and after it the deaths before the boss.
- Every modifier socketed on the Ring Emitter does nothing once the weapon evolves, because there are no rounds for it to act on. In these runs the weapon carried up to two (Scatter Shell, Lingering Field, Split Core, Chain Trigger, Backwash).
- The other evolutions whose rows were read for this draft keep their weapon's own emission and change or add to it: Sweep Beam's beam sweeps, Cyclone's blade spins, Murmuration's rounds persist, and Vortex's rounds curl and their expiry opens a zone (`reforging.csv`). Pulse Wall as ported is the one that removes it. Its own row still lists the capabilities Projectile and Radial.
- How evolutions read on this instrument (`Validation/S2_evolutions.md` on `dev/s2`): the hold takes the Path card Overload where the arm takes the evolution, and the evolutions that note calls working stand at 0.95 to 1.03 of their hold in build damage. Pulse Wall stood at 0.74.
- `reforging.md` R-RFG-18: a transformation changes how a build kills; its output after the pick stays in the source's band, `REFORGE.POWER_MIN` 1.3 to `REFORGE.POWER_MAX` 1.8, a band no evolution reaches on bots ("which may be a field cap").

### Options
- A — The whole disc hurts, and the wall stays the weapon's only emission. Arithmetic, not a reading: every enemy inside takes about 22 a second at level 6, so twenty enemies inside take about 22 000 a minute against the plain weapon's 2 800 before the boss, while a boss alone takes about 1 100 a minute against the plain weapon's 5 793. It overshoots on a crowd and still falls short at the boss, and one `dps` cannot fit both. The modifiers still do nothing.
- B — A true wall for a hero who stands in it: a band no walker crosses, with D-067's carry. It aims at the crowd's place. It leaves the fight at 0.27 to 0.31, because the boss ignores displacement. The bot leaves its own fixed ring in about half the ring lives, so it cannot read it. The carry cost about 0.2 ms of game thread in the S2 scene.
- C — The volley keeps its rounds, and the wall is what the evolution adds. The rounds are what struck the boss and carried Keeper.Storm, so the fight is the hold's again. It moves no crowd beyond what the band pushes today.
- C2 — As C, with the rounds' reach cut to the wall's radius: reach traded for the wall. A second lever on top of C; nothing read so far asks for it.
- D — Leave the rule and hold the card for people, or out of the offers (the scheduler decides that when the S2 kit is built).

### Decision (recommended)
C.

- R-RFG-12 becomes: a Pulse Wall volley fires its rounds as the weapon does, with the weapon's count, damage, speed, reach and pierce and whatever its modifiers do; and unless this weapon has a ring live, the volley also opens one ring at the hero, which grows, holds, strikes its band and pushes exactly as today.
- The weapon's cooldown is its own. The clause "the cooldown is at least the ring's life" goes: "unless this weapon has a ring live" already keeps one ring per weapon.
- A free volley, a kill's (`modifiers.md` R-MOD-16) or a dash's (`keepers.md` R-KEE-34), fires its rounds as a free volley of any weapon does. Its ring clause stays as D-039 and D-040 wrote it: no second ring, the live ring strikes its band once more as that volley's source and at its scale, and with none live it opens one.
- Nothing else moves: the ring's numbers, the band, the push, the two beaded arcs. The card's sentence stays true of the ring that holds.
- Why C and not A: A needs a number at once and cannot make it fit both a crowd and the boss. Why not B: it does not touch the fight, and no bot can read it. Why not D: the fault is now located and the rule that removes it adds no number.
- Why this is still a transformation, which I doubted in Draft 9: the evolved weapon holds ground, with a ring that strikes and pushes whatever stands 500 to 600 uu out for most of every ring life, which the plain weapon never did. That is how the four evolutions named above are built. A transformation that silences the weapon and its modifiers is the odd one, and the readings say what it costs.
- What C may do to output, said now so the reading is not a surprise: the rounds are the hold's, and the band adds to them. As the weapon's only emission the band read 1 960 to 4 068 a minute; with rounds killing first it will add less. The hold takes Overload where the arm takes the evolution, so 1.0 is not given: 0.95 to 1.03 is where working evolutions stand. If the build lands under `REFORGE.POWER_MIN`, as every evolution does on this instrument, a TUNE pass on `dps` upward is a step of its own afterwards; D-061's sweep already shows how the ring's damage follows the cell.
- What C may do to danger: the rounds clear the hero's surroundings as in the hold (1.8 damage a minute before the boss there), and the wall adds to that. Printed, judged nowhere.

### The three points the scheduler asked a next draft to face (board, 2026-11-12 03:27)
- **Which fault it aims at.** The build's output, and inside it the boss fight (0.27 to 0.31 of the hold) and the silenced modifiers. It does not aim at the crowd's place: the rounds are what kept the hold's nearest enemy at 477 uu, and the wall is left as leaky as it is.
- **The perf bound, up front.** No new displacement: the band pushes as today. What is new is that the evolved weapon fires rounds again. The S2 perf scene evolves the Ring Emitter (three rings in each capture window), so its bullet load rises by that weapon's rounds, inside the cap the scene already proves. The job reads the S2 scene twice before and twice after, as D-067 did, and says so in its note; if the game thread moves by more than the spread, the scheduler takes it alone.
- **Whether the carry's gain is worth keeping by itself.** No. Before the boss it cut damage taken from 22.3 to 15.9 a minute, and wins rose from 1 to 4 of 15 (printed), for about 0.2 ms. Six of fifteen runs still died before the boss, and the fight did not move. With the rounds back the hold's own figure is 1.8. `s2/d067-pulse-wall` stays, as the scheduler ruled, in case people find the wall too leaky.

### Hypothesis
We believe letting a Pulse Wall volley keep its rounds moves the build's damage per minute after the pick from 0.74 to 0.78 of its hold to at least 0.95, where the evolutions that read as working stand, and in the boss fight from about 0.3 of its hold to at least 0.9, because the rounds are what struck the boss (5 793 a minute against the wall's 170) and what carried Keeper.Storm; measured by `Tools/BotRuns.ps1 -Carry` on the Ring Emitter pair, 15 seeds, bot v2 at skill 1.0, read by `Tools/evolve_read.py` and by the split of the run files at the boss's start that this note uses, which the job makes a part of `evolve_read.py` with a tools test so that the reading is a tool's and not a scratch script's; reach and wins printed and judged nowhere; reverted, the clause removed, if the build reads under 0.95 of its hold after the pick or under 0.9 of it in the boss fight.

### Impact (dependency sweep)
- Rules replaced: R-RFG-12 (the volley's rounds; the cooldown clause goes) and its test line (rounds fire with the ring; one ring live at a time; the cooldown is the weapon's; a held ring still leaves its inside alone).
- Rules that stay and are read again: the Pulse Wall clauses of `modifiers.md` R-MOD-16 and `keepers.md` R-KEE-34 (D-039, D-040). With rounds back, a free volley is no longer empty-handed while a ring lives; whether the band's one more strike should stay is a question for after the reading, not for this record. D-020's hold on the wall's own numbers stands.
- Constants: none. Catalog: none; the Evolve row's capabilities already say Projectile and Radial.
- Files / tests: `02_Systems/reforging.md`; `Combat/DTWeaponSim.cpp` (the Pulse Wall branch of a volley, and the cooldown floor); the Reforging tests, and the two free-volley tests (`Dusktracer.Modifiers.ChainTrigger.PulseWall`, `Card.Quicksilver.Vector.PulseWall`), where an expectation moves only because a volley now also fires rounds, each move named in the job's note; `Tools/evolve_read.py` and its tools test.
- Text: the card's sentence stays. Whether it should also say that the rounds still fly is a writer's call: one key, en and zh.
- Readings that move: runs that evolve the Ring Emitter, and the S2 perf scene's bullet load. No run of the S2 exit battery evolves it.
- Presentation: the wall draws as today and the rounds as the weapon's. Nothing asked.
- Not shown by any of this: the bot at skill 1.0 kites and does not know the wall is there; whether a person values the wall, and whether it is too safe to stand in, is the first S2 session's.

### Swap-out point
The first S2 session with people.

## Also noted, not drafted
- The same split at the boss's start would say, for the other evolutions that read under their hold on this instrument (Murmuration 0.83, Blade Halo 0.93, Vortex 0.94 in `Validation/S2_evolutions.md`), whether their loss is the fight too. Once `evolve_read.py` holds the split it is one reading of files that exist.
- The harness reading the scheduler allowed (a walker's distance over one ring cycle, by role speed) was not taken: it measures the wall's leak, which this draft does not aim at. The arithmetic of that leak is in `Validation/S2_d067_pulse_wall.md`: the held band stops only Tanks.
