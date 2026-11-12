# Blade Halo after D-071: what follows (Draft 15)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for on the board on 2026-11-12 after D-071's reading. Written on `plan/blade-halo-next` from `main` at 033bf71. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it.

Build: none read here. Every number is from D-071's job (`Validation/S2_d071_blade_halo.md` on `s2/d071-blade-halo`, tip 5cf4387, kept and not landed): the pair `Steered:Storm:Tempest:Tide` with `-Carry Weapon.OrbitalDrones`, 15 seeds, bot v2 at skill 1.0, three arms on one base (`dev/s2` d5d4a81) and the same seeds, read by `Tools/evolve_read.py`. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

A constant or a test id that exists only on `dev/s2` is written without its prefix. Every figure marked "arithmetic" is mine and not a reading.

## What D-071 read
| After the pick | Hold | The port's arm | D-071's arm (the source's cells) |
|---|---|---|---|
| Before the boss, against the hold | 1.00 | 0.80 | 0.89 |
| Boss fight, against the hold (runs that reached it) | 1.00 (15) | 0.67 (6) | 0.55 (8) |
| The weapon's hits per minute | 247 | 60 | 61 |
| The halo's own damage per minute before the boss | 3 541 | 1 424 | 1 182 |
| Damage taken per minute before the boss | 1.7 | 33.5 | 22.0 |
| Died before the boss, of 15 | 0 | 9 | 7 |
| Reach and wins, of 15 | 15 and 15 | 6 and 4 | 8 and 4 |
| Nearest enemy after the pick, median | 434 uu | 205 uu | 201 uu |
| Intervals with the nearest enemy 100 to 220 uu out | 0 of 90 | 34 of 62 | 46 of 70 |

- The bar was 0.9 before the boss; the rule read 0.89. The fight is not judged: eight runs reached it and the record asks for ten.
- The mechanism the hypothesis named did not happen. Every blade strikes every 0.15 s on an orbit at 260 degrees a second, and the weapon lands 61 hits a minute where the port lands 60. My arithmetic in Draft 12 was wrong.
- Against the port on the same base the rule did lift the build from 0.80 to 0.89 and cut damage taken by a third. With no more hits, that came from elsewhere in the build's play and not from the halo striking more.

### The second pair, printed only
`Steered:Storm:ArcGrid:Tide`, D-071's arm against its own hold: 0.93 before the boss and 0.90 in the boss fight, which twelve of fifteen runs reached (the tool reads the fight just under 0.9). Three runs died before the boss; six won. No arm of the port was read on that pair.

So on a build whose Keeper rides any hit, the source's halo stands at its bars. On the Tempest pair it does not, and part of that is Tempest: a halo fires no bolts, so Tempest's Split children are gone, as with the wall.

### Where the crowd stands: one step outside the blades
- The source's blades reach 150 (the orbit at rest) plus 30 (the contact) plus the enemy's own radius: about 200 uu from the hero, a little more while the hero moves.
- The nearest enemy after the pick stands at a median 201 uu, and in two intervals of three it is between 100 and 220 uu.
- So the bot holds its crowd at the outer edge of the weapon. It lets the crowd come that close because nothing kills it further out, and it does not step in the last few tens of uu, because every body is a threat it walks away from (`Bot/DTBotBrain.cpp`: the body term of the path's threat, and the kite term that leans away from anything inside 1 300 uu).
- A person who picks a contact weapon steps into the crowd. This bot never does. That is the sentence `Validation/S2_evolutions.md` already wrote: "Blade Halo trades reach for contact, and the bot does not stand in contact."

## The four things the scheduler put on the table

### 1. The additive form, and what it does to R-RFG-18
- The form: the drones keep their bolts and gain the contact blades. By D-074's item 7 it is the standing form for an evolution: it keeps its weapon's own emission and changes it or adds to it.
- What it would read, by arithmetic: the hold's build plus what the blades land, about 1 200 to 1 400 a minute before the boss, so a little over 1.0 in both windows and the hold's safety.
- Honestly, against R-RFG-18 ("a transformation changes how a build kills"): it changes nothing in how the build already kills. It is the plain drones with a contact ring that this bot barely uses. It is safe and it is thin: of the four ways on this page it is the least like the card, which is a halo of blades and no bolts.
- Its place: the fallback, as C with the port's band was for the wall.

### 2. The source's form kept, its cost on the card, held for people
- The form is D-071's branch as built. The card already says there are no bolts; it would say the cost in the same line (blades strike what they touch, nothing at range).
- What the S2 kit would ask: whether the validator stepped into the crowd with the halo; whether it killed what it touched; whether losing the bolts was worth it.
- Its weakness: by D-074's item 7 an evolution that deletes its emission is not built again "without a reading, by a bot that uses the weapon or by people, that shows it holds a run". Holding it for people is waiting for that reading. Until then every bot battery that evolves the drones reads a pick that loses the run, and the offers would have to hold the card back.

### 3. What the second pairs show
- The wall's second pair missed the fight too (0.36), so there the fault was the weapon's.
- The halo's second pair stands at its bars (0.93 and 0.90). That is not a pass, and it is printed only; but it says the source's halo is not far off on a build that suits it, where the Tempest pair is the hardest case twice over: no Split children, and a bot that holds the crowd outside the blades.
- Whether a rule may be judged on a pair other than Tempest is the method question the scheduler kept for itself. What this page adds to it: for a contact weapon the instrument's larger fault is not the pair, it is the bot's standoff, which is the same on every pair.

### 4. The smallest bot change that lets a bot use the weapon
- What: a measuring arm, off by default, beside `-DTBotCarry` (`bot.md` R-BOT-18) and `-DTBotPrimaryFirst` (R-BOT-16), which "model no player". With it on, the bot treats the carried weapon's reach as the distance it holds its crowd at: bodies inside that reach are not walked away from until they are close enough to touch, and the lean away from the crowd stops at the weapon's reach where today it starts at 1 300 uu. For the halo that is about 200 uu; for a weapon that fires rounds the reach is long and the arm changes nothing.
- Where: `Source/Dusktracer/Bot/DTBotBrain.cpp` (the two terms named above) and `Docs/02_Systems/bot.md` (a new rule beside R-BOT-18, with its test line). The bot already reads a weapon's reach from the catalog for its card choice (R-BOT-09), so no new data is needed.
- Where it lands: `dev/s2`, where the carry arm and the evolutions are. With the arm off every battery, every hash and `main`'s bot are as they are, so session A is not touched. The job's first step is to read how the bot's standoff arises in code, read-only, and say whether the two terms are the whole of it; if the change is wider than one option in those two places, it stops and reports.
- What it could show: whether the source's halo holds a run when the weapon is used. That is the reading D-074's item 7 names.
- What it cannot show: that a person plays that way. It is a bound from the other side: a bot that always steps in, where the standard bot never does.

## Draft 15. Read the halo with a bot that steps in, before choosing its form

- **Class:** SYSTEM, layer the measuring tools (a bot arm, off by default). It changes no rule of play and no number. It is followed by a ruling on Blade Halo's form, which this draft does not make
- **Lowest branch:** `dev/s2`. **Before session A:** yes; with the arm off nothing moves, and `main` is not touched
- **Order:** it needs no other rule. D-071's branch is kept and is one of the arms it reads

### Recommendation
Build the measuring arm of item 4, then read three arms on one base and the same seeds, on the Tempest pair and the Arc Grid pair, each with the standard bot and with the arm on:
- the hold;
- the port's halo as it stands on `dev/s2`;
- the source's halo (D-071's branch, merged onto the base that holds the arm).

Then the scheduler rules on the form with those columns in hand:
- if the source's halo holds a run when the bot steps in (the two windows at or over 0.9 on the arm-on columns), option 2 has the reading item 7 asks for, and the halo can land as the source's with its cost on the card and the S2 kit's questions;
- if it does not, the additive form of item 1 is built, as the wall's was.

Why not build the additive form now: it would land a weapon that is the plain one with a ring this bot does not use, on the strength of a fault that is the instrument's as much as the weapon's. One small arm tells the two apart, and the same arm will serve the next contact or zone weapon.

Why not hold for people now: nothing reads the card until the first S2 session, and the standard batteries would keep reading a pick that loses the run.

### Hypothesis
We believe that a bot that holds its crowd at the carried weapon's reach lands the halo's blades on what it fights, because today the nearest enemy stands at a median 201 uu, the outer edge of blades that reach about 200, and the bot never closes the last step; measured by `Tools/BotRuns.ps1 -Carry Weapon.OrbitalDrones` with the arm on, 15 seeds, bot v2 at skill 1.0, on the source's halo: the weapon's hits per minute rise from 61 to at least 180, three times as many, and the nearest enemy's median falls inside the blades' reach; the arm is kept if both are met and taken out if either is missed, since an arm that does not make the bot use the weapon reads nothing. The halo's two windows, damage taken, deaths, reach and wins are printed on every column and decide nothing here: they are what the scheduler rules the form on.

- 180 is a floor for "the bot uses the weapon", not a target for the weapon: an enemy standing on the orbit is passed by a blade about three times a second (arithmetic), and the crowd holds a dozen or more.
- The arm judges itself and nothing else. No rule of play is judged on arm-on columns unless the scheduler's method decision says so.

### The perf bound
The arm changes what the bot weighs, not the sim: no bound is owed for it. The halo's own bound stays owed for whichever form lands, read with the perf scene's option the scheduler granted and put on hold (the Orbital Drones in the Spiral Projector's place when the option is on).

### Impact (dependency sweep)
- Rules: `bot.md`, one new rule and its test line beside R-BOT-18. No rule of play.
- Constants and catalog cells: none. If the arm needs a number of its own (how close is close enough to touch), it is a named constant of the bot with a number-log row, and the job says so before it adds one.
- Discoveries, walked by counter: none is reached; a measuring arm banks nothing on a person's save and a tool run banks nothing at all.
- Files / tests: `Bot/DTBotBrain.cpp`, the bot's launch arguments and `Tools/BotRuns.ps1` (one switch that passes the arm), the bot tests, `bot.md`.
- Readings that move: none with the arm off; the job shows it by one standard battery's hashes before and after.
- Not shown by any of this: how a person plays a contact weapon.

### Swap-out point
The first S2 session with people, for the halo's form. The arm itself stays as a measuring tool.
