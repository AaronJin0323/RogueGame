# S2 every evolution in a run (2026-10-31)

Class: tool (`bot.md` R-BOT-18, off by default) and one BUG (Pulse Wall). Layer: `Bot/`, `Tools/`, one function in `Combat/`. Replaces: nothing; seven evolutions had tests and no run (`S2_reforging.md`). Every number is bot v2, skill 1.0, S1 Keepers: **a bound, never a human reading.**

## Reproduce
```
& Tools\BotRuns.ps1 -Arms "Steered:Storm:Tempest:Tide" -Carry All -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Out Saved\BotRuns\ev_arm
& Tools\BotRuns.ps1 <the same> -CarryHold -Out Saved\BotRuns\ev_hold
python -I Tools\evolve_read.py --carry Saved\BotRuns\ev_arm --hold Saved\BotRuns\ev_hold
```
150 runs a batch, seeds 1..15, on `dev/s2` at 79abddd plus this branch's arm and fix (game code of 9eaeb1e). The hold is the same run, except that the Reforging hand takes its first Path card, Overload, instead of the evolution.

## How the arm came to grant levels
The first version used offers alone, on Tide/Frost: 33 of 150 runs took their evolution, because the carried weapon's upgrade showed in 13 to 26% of hands and 66 of 150 runs died before the hand. That batch, `carry_a`, is thrown away. The arm now hands over the weapon at the Patron pick and raises it to the level two levels before the Reforging hand, as a probe scene does (R-BOT-18, the one exception to R-BOT-02). Storm/Tempest:Tide replaced Tide/Frost: Storm is the strongest Path card, and with the second Keeper Tide every weapon fits a Keeper.

**What that costs the reading.** Median 0 to 3 levels are free, the Rifle stays at level 1 until the pick, and one Keeper pair is read. Boss reach of the arm is not comparable with `S2_reforging.md`. The column to read is arm against hold on the same seed. The hold is itself a card, so a loss here means "worse than Overload", and Overload is the strongest Path row of the batteries.

## Arm off changes nothing
`Steered:Storm:Tempest:Tide`, seeds 1..15, arm off on this branch against a build of `dev/s2` at 9e98177: 15 of 15 `input_hash` the same, 13/15 at the boss, 9 wins.

## Each evolution
Runs are the 15 seeds of that weapon. `Hand` is runs that reached the Reforging hand; the evolution card was in every one of them and was taken in every one. Boss and wins are on those seeds, arm / hold. Damage per minute (dpm) is of the carried weapon, in the intervals after the pick, median, arm / hold. Build dpm the same, all sources. Rank is seeds that did better / worse than their hold (win over boss over neither).

| Evolution (weapon) | Hand | Granted | Boss | Wins | Weapon dpm | Build dpm | Weapon hits/min | Rank | Read |
|---|---|---|---|---|---|---|---|---|---|
| Railstorm (Pulse Rifle) | 15 | 1 | 11 / 13 | 8 / 10 | 6558 / 4715 | 18673 / 18710 | 177 / 248 | +1 -4 | works |
| Pulse Wall (Ring Emitter) | 14 | 2 | 5 / 14 | 1 / 10 | 1891 / 3603 | 13141 / 17789 | 340 / 308 | +0 -13 | **BUG fixed; still reads wrong** |
| Blade Halo (Orbital Drones) | 15 | 0 | 4 / 15 | 2 / 13 | 1262 / 3057 | 14884 / 16029 | 63 / 226 | +0 -13 | **reads wrong, no bug** |
| Cyclone (Arc Blade) | 13 | 2 | 4 / 6 | 1 / 2 | 1248 / 778 | 14228 / 14745 | 62 / 43 | +1 -3 | weak weapon both ways |
| Singularity (Gravity Mine) | 7 | 2 | 3 / 6 | 3 / 3 | 4715 / 951 | 16991 / 17075 | 689 / 125 | +1 -3 | works; few runs |
| Flechette (Scatter Cannon) | 15 | 3 | 13 / 13 | 7 / 5 | 2021 / 1874 | 15905 / 15454 | 190 / 315 | +4 -3 | works |
| Sweep Beam (Rail Lance) | 14 | 1 | 10 / 11 | 7 / 9 | 2159 / 3012 | 17738 / 18657 | 62 / 73 | +1 -4 | loses a modifier half the time |
| Carpet (Flak Mortar) | 10 | 3 | 8 / 10 | 7 / 8 | 3187 / 1443 | 18115 / 17551 | 221 / 54 | +1 -3 | works |
| Murmuration (Seeker Swarm) | 15 | 0 | 15 / 13 | 12 / 10 | 4694 / 4933 | 15458 / 18593 | 1394 / 350 | +5 -2 | works |
| Vortex (Spiral Projector) | 13 | 1 | 9 / 13 | 8 / 12 | 1862 / 4807 | 15809 / 16901 | 362 / 771 | +0 -5 | **reads wrong, no bug** |

All 150 of each batch: boss 82 / 114, wins 56 / 82. 121 evolutions were taken, all ten kinds.

**Crash, ensure, dead behaviour.** 300 runs: no crash, no ensure, no `LogDT` warning or error, no missing record, no timeout, no kill. No evolution read zero weapon damage after the pick in any run. Dead behaviour found: the Pulse Wall volley below, and nothing else.

## Pulse Wall: a lost volley, fixed (BUG)
A ring's life equals its cooldown (R-RFG-12), so each new volley came due in the step before the old ring ended. `OpenForceField` refused it for "one ring per weapon", and the volley still spent its cooldown: every second ring never opened. `Dusktracer.Reforging.PulseWallGap` on the unfixed code: 430 of 1080 steps with no ring. After: green. A refused volley now keeps the cooldown ready, as Cyclone at rest does. This is a line in `Combat/DTWeaponSim.cpp`, outside this brief's folders, because that is where the evolution lives; no number or rule moved.

Before the fix, same batch: boss 4, wins 0, weapon dpm 1107, 189 hits/min. Only Pulse Wall's numbers moved; the nine other rows repeat exactly.

## Static crowd
A throwaway test, not committed: level 6, 24 unkillable enemies on one ring around a hero who stands still, 10 s, plain -> evolved damage per second. It separates the rule from the bot's kiting. Fields and pull are not applied to the dummies' positions. Cyclone reads 0 here by its rule (at rest), and Pulse Wall's figures predate the fix.

| Weapon | r200 | r300 | r450 | r550 | r800 |
|---|---|---|---|---|---|
| Orbital Drones | 155 -> 1196 | 155 -> 0 | 153 -> 0 | 151 -> 0 | 0 -> 0 |
| Ring Emitter | 290 -> 26 | 264 -> 0 | 264 -> 26 | 264 -> 277 | 238 -> 0 |
| Spiral Projector | 260 -> 260 | 254 -> 254 | 184 -> 224 | 144 -> 0 | 98 -> 0 |
| Seeker Swarm | 154 -> 1021 | 154 -> 59 | 141 -> 28 | 141 -> 56 | 141 -> 28 |
| Rail Lance | 182 -> 561 | 91 -> 561 | 91 -> 561 | 91 -> 561 | 91 -> 561 |
| Scatter Cannon | 235 -> 571 | 235 -> 571 | 206 -> 260 | 147 -> 174 | 0 -> 0 |
| Pulse Rifle | 202 -> 233 | 202 -> 238 | 202 -> 238 | 202 -> 238 | 194 -> 233 |
| Gravity Mine | 1157 -> 2139 | 579 -> 1134 | 446 -> 1000 | 379 -> 667 | 0 -> 0 |
| Flak Mortar | 634 -> 734 | 346 -> 389 | 173 -> 216 | 173 -> 202 | 115 -> 144 |

## Why Blade Halo loses and Railstorm does not
Earlier, under `-PrimaryFirst`, Blade Halo won 3 of 11 and Railstorm 71 of 84. Here, on paired seeds, it is 2 wins against 13 for its own hold, with 13 of 15 seeds worse and none better. Railstorm keeps its reach and adds damage. **Blade Halo trades reach for contact, and the bot does not stand in contact.** It gives up bolts that reach 600 from the drones for strikes 60 uu around four points on an orbit of 160 to 288 uu. Its strike is strong: eight times the plain weapon at r200. At r300 and beyond a hero at rest deals nothing. In runs its hits fall from 226 to 63 a minute against the hold, and the median kill distance falls from 673 to 521 uu, so the crowd gets closer. The bot keeps enemies at a distance (nearest enemy 214 to 407 uu in S1, `DTBotTuning::ReachPriorUU`), and closing to melee is out of its scope (`bot.md`). The reading therefore cannot say what a player who walks into the crowd gets. It says that the card is a large loss to a player who kites. Rule problem, reported; nothing changed.

## Number and rule problems, unchanged
- **Pulse Wall, still worst with Blade Halo.** Build dpm falls by a quarter and the run ends a median 146 s after the pick. Two things to decide. The band acts only from 500 to 600 uu, and what it pushes out stays out of reach of everything the ring gave up. And the damage: `dps` multiplies the composed damage per round, so level 6 does 22 per second per enemy. R-RFG-12 says "`dps` × the composed damage per second", which can also mean the weapon's whole output, 275 per second, or 550 with `dps` 2. The code takes the first reading. A test cannot settle this one way or the other, so it is reported, not classed as a bug.
- **Vortex's reach.** `vortex_uu` 500 replaces the Spiral's range of 1350, so the evolved weapon stops at 500 uu and its hits halve. R-RFG-13 says this and the source's "at 500 uu" agrees. Wins 8 against 12.
- **Sweep Beam's modifiers.** Its caps drop Projectile, so 7 of 14 picks put a modifier to sleep (Pierce 5, Split Core 2). R-RFG-04 refunds none.
- **Murmuration.** Hits rise fourfold and damage stays level, at a quarter of the damage per hit. It does well in crowds and badly on a thin field: 59 against the plain 154 at r300. Build dpm is 17% under its hold and yet it has the best record here.
- **Gravity Mine and Flak Mortar reach few hands** (7 and 10 of 15), and Cyclone's weapon has 62 hits a minute: these arms die early with the Rifle at level 1, so the count is about the arm, not the evolution.

## Not read
- A human, on any of this. Which Keeper suits which evolution: one pair was read. Screen, readability, perf: no renderer, and `Tools\Perf.ps1` was not run. Skills under 1.0. The power band of R-RFG-18.

## Gate
See the commit after this one's report.
