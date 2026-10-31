# S2 exit readings (2026-10-31)

Branch `dev/s2-exit`, from `dev/s2` at 0747e3c. Class: SYSTEM (a tool) plus one spec rule. Layer: the measuring tools. Replaces nothing; no game number or behaviour changed, nothing was tuned, T-KEE criteria are unchanged. **A bot reading is never a human reading.** No human has played S2. This is a reading, not an exit: S2 has not passed.

## Reproduce
```
& Tools\S2Exit.ps1 -Out Saved\BotRuns\s2_exit -Parallel 12
& Tools\S2Exit.ps1 -Out Saved\BotRuns\s2_exit -ReadOnly
```
By hand, about an hour: the S2 row says nightly, nothing schedules it. 18 arms (nine S1 showcase with no second, the nine of `-ArmSet S2Keepers`), 15 seeds, bot v2 at skill 1.0 and 0.6, `-Unlock S2` so the full arsenal is in the pool, 540 runs. T-KEE-18 is builds 1 to 5 of the skill 1.0 batch, one seed, 480 scenes. T-KEE-15 is all 282 combinations of six Keepers, 2 seeds, 1128 scenes. Probes are v1, skill 1.0 (R-BOT-01), on the 40x field. The build stamp reads 0747e3c+dirty: the tree was these tools and constants over 0747e3c, no game source.

## Exit readings

| Reading | Verdict | Value | Pass line |
|---|---|---|---|
| Keeper share | **FAIL** | 10/18 arms pass, mean 0.598 | `C.POWER.KEEPER_SHARE_L16` per arm |
| Build bands | **FAIL** | 11/18 arms in band | `C.POWER.ARCHETYPE_BAND` of the arm mean |
| Intercession | **FAIL** | 50/282 in band | `C.INTERCESSION.BAND` of the Keeper's mean |
| Bots win a full run | PASS, a bound | wins of 45: Storm 24, Forge 33, Tide 8, Shade 41, Radiance 34, Quicksilver 44 | at least one win per Keeper at skill 1.0 |
| Humans win a full run | NOT READ | human only | O-008 |
| Death histogram | NOT READ | human only; the bot's is below | `bot.md` R-BOT-17 |

"Bots win" is the row's "can win" read literally, the weakest honest line: it says nothing about balance.

`S2EXIT: 1 PASS, 3 FAIL, 2 NOT READ`.

## T-KEE-18 across six Keepers: FAIL
Share below on 8 arms, gain outside on 16, outside the archetype band on 7. Builds are few where the bot rarely reaches level 16 (Whirlpool 2, ArcGrid and Frost 3).

| Arm | Builds | Full DPS | Share | Package gain | Share | Gain | Band |
|---|---|---|---|---|---|---|---|
| Forge/Furnace | 5 | 10188 | 0.618 | 1.37 | PASS | FAIL | IN |
| Forge/Hammer | 5 | 13350 | 0.659 | 2.82 | PASS | FAIL | IN |
| Forge/Slam | 5 | 8796 | 0.554 | 1.01 | FAIL | FAIL | IN |
| Quicksilver/Momentum | 5 | 14282 | 0.520 | 2.17 | FAIL | PASS | IN |
| Quicksilver/Rebound | 5 | 10874 | 0.542 | 1.95 | FAIL | FAIL | IN |
| Quicksilver/Vector | 5 | 12512 | 0.724 | 0.98 | PASS | FAIL | IN |
| Radiance/Lance | 5 | 12393 | 0.737 | 2.14 | PASS | PASS | IN |
| Radiance/Prism | 5 | 10782 | 0.624 | 1.69 | PASS | FAIL | IN |
| Radiance/Solar | 5 | 6715 | 0.749 | 1.88 | PASS | FAIL | OUT |
| Shade/Economy | 5 | 10950 | 0.413 | 1.69 | FAIL | FAIL | OUT |
| Shade/Hound | 5 | 6177 | 0.485 | 1.59 | FAIL | FAIL | IN |
| Shade/Hunters | 5 | 9352 | 0.636 | 1.00 | PASS | FAIL | IN |
| Storm/ArcGrid | 3 | 7026 | 0.394 | 3.03 | FAIL | FAIL | OUT |
| Storm/Downpour | 4 | 7755 | 0.449 | 3.56 | FAIL | FAIL | IN |
| Storm/Tempest | 4 | 5554 | 0.312 | 1.11 | FAIL | FAIL | OUT |
| Tide/Frost | 3 | 8668 | 0.751 | 0.90 | PASS | FAIL | OUT |
| Tide/Stillness | 4 | 8109 | 0.792 | 1.09 | PASS | FAIL | OUT |
| Tide/Whirlpool | 2 | 9137 | 0.812 | 0.95 | PASS | FAIL | OUT |

All three Storm arms miss share, as `STATE` already says of the formula. These builds took S2 cards, so this table is not the no-unlock table of `S2_keepers.md`: its S2 arms read lower here (Hound 0.485 against 0.580, on other builds).

## T-KEE-15 across six Keepers: FAIL

| Keeper | In band | Min | Max | Mean |
|---|---|---|---|---|
| Storm | 8/46 | -2995 | 209354 | 46253 |
| Forge | 17/45 | 3620 | 80552 | 51727 |
| Tide | 0/47 | -10718 | 28688 | 1374 |
| Shade | 2/48 | -22714 | 2297951 | 429237 |
| Radiance | 21/48 | -15640 | 80156 | 27279 |
| Quicksilver | 2/48 | -18132 | 38384 | 8847 |

Tide has no band: its cast deals no damage (`S1_tide_rule.md`). Shade's mean is Economy's payday (`S2_keepers.md`).

## A second probe fault, found and fixed here
The first T-KEE-15 reading here was void. `BotRuns -Jobs` unlocked the scene's second Keeper too; an unlocked Keeper leads the hand (R-KEE-23), so an S2 second pushed Tide out of the Patron hand and the bot took the S2 Keeper as Patron: 27 Tide combinations, 108 runs. The first reading said Tide 4/47, total 54/282.

- Fix: only the Patron is unlocked; the scene seats its second itself. `Tools/BotRuns.ps1`.
- Guard: `probe.wrong_patrons` reads each log's first pick against the scene, and `Tools/s2_exit.py` makes a reading with a wrong Patron NOT READ, so this cannot pass silently again. It would have caught the Storm fault too.
- Control: full rerun, 255 of 282 combinations identical to the unit, the 27 faulted ones moved. No run in the final reading took a wrong Patron.
- Reach: this hits S1 Patrons with S2 seconds only. `S2_keepers.md`'s tables have S2 Patrons and all 258 logs picked them; T-KEE-18 here picked every Patron, and no arm there has an S2 second on Tide.

## Death histogram: the source gives a shape, the bots do not read it
The source states one: `01_GameDesign.md` §3.5, Difficulty targets, which `G03_Retrospective_EN_ZH.md`'s S2 row names ("death histogram matches §3.5"). It is now `bot.md` R-BOT-17, keys `C.DEATH.*`.

- A first-time player dies before 10:00 on roughly half of first attempts.
- A player who has learned to steer a Path wins about 30 to 40 percent.
- The boss kills about a third of those who reach it.

Each clause is about humans, so the verdict stays NOT READ until sessions exist. Only the win band is a real band: "roughly half" and "about a third" have no tolerance in the source and none was invented. The 10:00 was not scaled when the source's clock moved to a 15-minute run; it is written as stated and left open in `bot.md`. The source also says if nobody dies the threats are HP walls, and if everyone dies in harvest the swarm leaks damage: no number, not read.

Printed, not judged, all 18 arms:

| Bot skill | Runs | Deaths | Dead before 600 s | Wins | Boss kills of reachers | Deaths by minute |
|---|---|---|---|---|---|---|
| 1.0 | 270 | 86 | 0.13 | 0.68 | 0.10 of 205 | 2:1 3:4 4:2 5:3 6:5 7:6 8:9 9:6 10:5 11:9 12:5 13:1 14:9 15:9 16:11 17:1 |
| 0.6 | 270 | 176 | 0.14 | 0.35 | 0.53 of 198 | 4:5 5:4 6:6 7:9 8:5 9:9 10:8 11:7 12:6 13:9 14:4 15:86 16:17 17:1 |

**Skill 0.6's wins sit inside the band, and that means nothing:** no skill is a player (R-BOT-13), and it was not picked to land there. Its 86 deaths in minute 15 are the boss (81 last hits): 0.53 of reachers die to it against the stated third, the first thing a human session should check. Skill 1.0 is a ceiling, and the source reads a strong dodger's wins as an upper bound for humans.

## Misses to read, none tuned
- Tide is weakest by wins: 8 of 45 at skill 1.0, 1 of 45 at 0.6, while its share of a build reads highest.
- Solar:Tide 6 wins of 15, Tempest 5, ArcGrid 8; every other S2 arm has 13 or more.
- Quicksilver 44 of 45 and Shade 41 of 45 at skill 1.0, against Storm's 24, with Storm as the second in most of those arms.
- Gain misses on 16 of 18 arms; `C.KEEPER.PACKAGE_GAIN_BAND` may not suit a build with a second Keeper, not decided here.

## Gate
`& Tools\Gate.ps1 -Base dev/s2` at d821859: GATE: PASS, 403 tests of 403, tools 40, 3 not run (pad, scope, mac). The only Source change is the regenerated constants header, which has no `.cpp`, so nonunity read 0 files.

## Human only
- Humans win a full run; the three clauses of R-BOT-17; O-008 sessions cover both, with first-attempt runs recorded as first attempts.
- Whether the 10:00 cutoff stands; what tolerance "roughly half" and "about a third" carry.
