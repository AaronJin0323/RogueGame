# How a boss fight is read after an evolution (Draft 18)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for on 2026-11-13 after D-079's reading. Written on `plan/boss-fight-measure` from `main` at 8dccac5. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it.

Build: no run was made for this note. Every figure is read from run files that exist: the carry batteries of D-072 (`dev/s2` 281959d's base and its rule), D-074 (2818103 and 61042fd) and D-079 (268174f and df753ed), 15 seeds each, bot v2 at skill 1.0, by a scratch script over the `boss_calibrated`, `boss_end`, `hit` and `hero_damaged` records. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

A constant or a test id that exists only on `dev/s2` is written without its prefix.

## What I said, and what the records say
In my interim on D-079 I wrote that the evolved Blade Halo kills the boss more slowly than the build that took its Path card, because its fight lasts 128 s against 93 s while the fight window reads 1.03, and that the window could not see it. I had not opened the boss's own records. Both halves were wrong.

- The Warden's health is not fixed. `boss.md` R-BOS-09 sets its effective max once, from the damage it takes in the first `BOSS.CALIBRATE_S` (8 s) of the fight times `BOSS.TARGET_FIGHT_S` (80 s), clamped between 9 600 and 64 000. A build that hits the boss harder at the start meets a boss with more health.
- So the length of a fight is not a measure of a build. In D-079's Tempest pair the hold's fights sit on the floor of 9 600 in 8 of 15 runs; the rule's sit there in 3 of 13 and their median boss has 14 871.
- By the damage the boss itself takes, the evolved build is not weaker: 1.04 of the hold by the median of runs, 1.15 pooled.
- The window was not hiding a slower kill. On every rule and hold arm read here the boss takes 0.86 to 0.96 of the fight's damage by the hit records (the ports' arms 0.73 to 0.91), and the window and the boss's own measure agree to within 0.03 when both are pooled and to within 0.08 by the median.

## The three rules read again
Damage dealt to the boss a minute is the effective max times the share taken, over the fight's seconds, from `boss_calibrated` and `boss_end`. "Median" is the median over the runs that reached the boss, as the tool's window is; "pooled" is all damage over all seconds.

| Rule, pair | The fight window as judged (median) | Boss damage a minute, rule against hold: median | pooled | Bar | Fight, median s: rule, hold | Effective max, median: rule, hold | Runs on the 9 600 floor: rule, hold |
|---|---|---|---|---|---|---|---|
| D-072 Murmuration, Tempest | 0.91 | 0.99 | 0.91 | 0.6 | 85, 79 | 18 571, 18 542 | 0 of 15, 2 of 15 |
| D-074 Pulse Wall, Tempest | 1.12 | 1.07 | 1.13 | 0.9 | 73, 84 | 21 359, 21 976 | 1 of 15, 1 of 15 |
| D-074 Pulse Wall, Arc Grid (printed) | 1.15 | 1.17 | 1.16 | | 78, 80 | 21 784, 20 571 | 1 of 15, 0 of 15 |
| D-079 Blade Halo, Tempest | 1.03 | 1.04 | 1.15 | 0.9 | 128, 93 | 14 871, 9 600 | 3 of 13, 8 of 15 |
| D-079 Blade Halo, Arc Grid (printed) | 1.35 | 1.35 | 1.37 | | 94, 121 | 10 035, 9 600 | 6 of 15, 8 of 15 |

- **Neither D-072 nor D-074 would have missed its bar by damage to the boss.** D-079 is met by it too.
- The ports, by the same measure, pooled: Murmuration 0.38; Pulse Wall 0.24 and 0.19; Blade Halo 0.69 and 0.82. They fail by either measure.
- The hero's damage taken a minute in the fight, rule against hold, pooled over the whole fight: Murmuration 11.0 against 13.4; Pulse Wall 12.3 against 10.4 and 12.0 against 16.5; Blade Halo 21.7 against 15.3 and 14.8 against 17.0. The tool prints medians, which read 25.1 against 11.6 for the halo's Tempest pair. That one stands: the evolved halo's Tempest fights are longer and cost the hero more, and all thirteen that reach the boss win.
- The median and the pooled figure part where some runs sit on the health floor and some do not (D-079's Tempest pair: 1.04 and 1.15). A clause must say which it is judged on.

## What the fight's length does show
- With the health set from the first eight seconds, a steady build's fight lasts about `BOSS.TARGET_FIGHT_S`. A fight much longer than that means one of two things: the build's damage to the boss fell after its opening, or the boss sat on the health floor and the build was too weak to clear even that in the target time.
- The halo's Tempest rule is the first case: its opening earns a boss of 14 871, and over the whole fight it deals 0.62 of its opening rate (arithmetic from the two medians, 14 871 over 80 s against 14 871 over 128 s). The hold on the same pair is the second: on the floor, 93 s.
- `boss.md` already names a cause it has not decided: enemies left alive when the fight starts take fire during the calibration window, and that lowers the effective max. The hold, which stands far from its crowd, and an evolved build that stands nearer may differ in exactly that. Not traced here.

## Draft 18. A boss-fight clause is judged on the damage the boss takes

- **Class:** METHOD (how a reading is judged), with one printed block in a tool and one pair of fields in a run record. No rule of play and no number
- **Lowest branch:** `dev/s2`, where `Tools/evolve_read.py` and the evolutions are. **Before session A:** yes; `main` is not touched
- **Order:** the tool's block before the S2 exit battery, as the scheduler asked; the fields with it or after it

### Decision (recommended)
1. **The judged measure**, from the next rule on: the damage dealt to the boss a minute, effective max times the share taken over the fight's seconds, as the median over the runs that reached the boss, rule against hold on the same base and seeds. A clause names its bar on that and still asks that ten runs reach the boss.
2. **Why change a measure that agreed five times out of five:** it is the thing the clause means; it is exact, from the boss's own health and not from a sum of hits; and it cannot be moved by what else is alive in the fight. The old window stays as a printed line, so the three landed rules can still be compared with what follows.
3. **The fight's length is never printed alone.** Beside it: the median effective max and how many runs sat on a clamp. A reading that names a longer fight says which of the two cases it is.
4. **Printed with every fight** (the block the scheduler granted): the old window; the fight's median length with the effective max and the clamp count; the damage the hero takes a minute in the fight; boss health left in the runs that lost.
5. **The three landed rules are not re-judged.** Their Results gain nothing but a pointer to this note. Their figures by the new measure are the table above.
6. **Blades and bolts.** Not a second source id: a source id is what a kill is credited to, so it would move `Kills.Weapon.OrbitalDrones` (a Discovery counter on `dev/s3`), the Keeper share of the power split and the dash refund's "a kill of such a volley". Instead the `weapon` record gains two fields, `strike_hits` and `strike_damage`: the hits and damage of the interval that came from an evolution's own strikes, the impacts D-074 already marks (`bRingStrike`: the Pulse Wall's ring and, by D-079, the halo's blades). Telemetry only; no sim state reads them, so no hash moves. The reader then prints the added part's share of the weapon's damage, which answers for bots what the S2 kit asks people: did the blades matter.

### What it costs
- `Tools/evolve_read.py` and its tools test: Tools only, the text steps and the tools tests as its gate.
- The two fields: `Combat` (where the weapon's interval counters are kept), `telemetry.md` and `weapons.md` (the record's schema), one test. Engine code, so a whole gate; the five seed 9107 arms must not move. It can ride with the next rule's branch on `dev/s2` (D-073) to save a gate, or go alone.
- Readings that move: none. Run files made before the fields simply lack them, and the reader prints "not read" for the share.

### Not shown by any of this
- Why the halo's Tempest build falls to 0.62 of its opening rate on the boss. A guess I have not checked: the blades reach the boss while it closes and not while it stands off.
- Whether the calibration window is a fair read of a build that stands near its crowd against one that stands far. That is `boss.md`'s open line, and it is S1's.
- How a person's fight goes.

### Swap-out point
The record on what an evolution must give, after D-073, which will use these lines for all ten.
