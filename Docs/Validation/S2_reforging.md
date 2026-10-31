# S2 L16 Reforging (2026-10-31)

Class: SYSTEM. Layer: `Combat/` (evolutions, Path transformations, one enemy-round removal), the offer hand and card text, telemetry, a bot card rule. Replaces: the S1 hand, which was always filler blessings; on this branch filler remains the fallback. Spec: `Docs/02_Systems/reforging.md`. Every number below is a bot on the S1 Keepers, version 2, skill 1.0: **a bound, never a human reading.**

## Reproduce
19 arms (no-pick plus 18 Steered), seeds 1..15, 285 runs per batch, sim cap 1100 s.
```
& Tools\BotRuns.ps1 -Arms <the 19 arms> -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Check T-OFF-15 -Out Saved\BotRuns\rfg2
python -I Tools\bot_compare.py control=<control> reforge=Saved\BotRuns\rfg2
```
Control: the same battery on a build of `dev/s2` at 584c0e5, before this branch. The first battery, `rfg1`, was stopped part way by the scheduler because runs showed Unreal's splash window; it was thrown away whole and is not read here. `rfg2` ran with `-nosplash`.

The batch ran on the game code of aaebc2a. Commits after it changed tests, docs, tools and the name of one telemetry field (`kind` to `reforge_kind`: it repeated the record's own key), so no sim number moved.

## Failing first
ef3254c holds the tests on neutral stubs. Nine failed (CardReforge, BladeHalo, Caps, Carpet, Catalog, Cyclone, Flechette, Glacier, Hand), Determinism passed with nothing evolved, and the run then crashed at the Kiln test on a stale enemy index in the test itself, so the other eight were seen unfinished, not seen failing one by one.

## Battery

| Steered (270 runs) | control | reforge |
|---|---|---|
| At the boss | 192 | 191 |
| Wins | 152 | 147 |
| Median death minute | 12.0 | 11.7 |
| T-OFF-15 | PASS 192/192 | PASS 191/191 |
| Mean decisions at the boss | 28.73 | 28.77 |
| Runs at the upper end, 30 decisions | 31 | 40 |
| Spawned XP / model | 0.7621 | 0.7622 |
| Collected XP / model | 0.706 | 0.706 |

Deaths by minute:
- control: 2:2 3:6 4:2 5:1 6:4 7:8 8:5 9:10 10:13 11:8 12:5 13:8 14:6 15:20 16:19 17:1
- reforge: 2:2 3:6 4:2 5:1 6:4 7:8 8:5 9:11 10:13 11:14 12:6 13:2 14:5 15:10 16:27 17:7

The budget did not worsen: no run left the band, and spawned XP stays well under the D-018 line. Its top edge did fill: nine more runs sit at 30, one short of the limit. A shift of a few wins or boss runs over 15-seed arms can be noise, so the five lost wins are not a finding on their own. Deaths move later inside the run, from minutes 11 to 15 toward 16 and 17; that is a reading to repeat, not a result.

## By card
`rel` is damage per minute in the minute after the pick over the minute before, divided by the same ratio on the same arm and seed in the control.

| Card taken | Runs | At the boss | Wins | rel median |
|---|---|---|---|---|
| Transform.Storm | 80 | 76 | 62 | 1.00 |
| Transform.Forge | 78 | 65 | 46 | 1.00 |
| Transform.Tide | 63 | 46 | 30 | 1.01 |
| Evolve.PulseRifle | 9 | 9 | 8 | 1.00 |
| Evolve.RingEmitter | 1 | 1 | 0 | 0.99 |
| Evolve.OrbitalDrones | 1 | 1 | 1 | 0.94 |
| none (hand never reached) | 53 | 0 | 0 | |

Card rows are not comparable as strength: Path cards follow the arms that own those Keepers, and R-BOT-15 assigns the card by seed.

## What the readings say
- **R-RFG-18 power band: missed low.** The median is 1.00 against `C.REFORGE.POWER_MIN`. Either the transformations add almost no output, or damage per minute cannot show it, because a build that clears its field is capped by what spawns. This reading does not separate the two.
- **Evolution slot: missed.** 15 of 232 hands held an evolution card, against a hypothesis of over half. Eleven evolutions were taken, on three weapons. The other seven weapons' evolutions, and Railstorm beyond those nine runs, are covered by tests only. Traced in the second pass below: the bot's card scores.
- No number was changed in answer. Raising power spends the top of the T-OFF-15 band, and a bot median at a field cap is weak evidence to spend it on.

## Not read
- A human, on any card. The HUMAN line of `reforging.md` is open.
- Screen readability, the two-second rule, density and perf: the battery has no renderer, and `Tools\Perf.ps1` was not run on this branch. The new visuals reuse arc, field, drone and mine; a Pulse Wall draws as a filled disc, not a ring.
- How long pillars last, whether Overload discharges often, and husk counts: no counter wrote them here; the second pass reads them.

# Second pass: three Path rows and evolution reach (2026-10-31)

Branch `dev/s2-reforge2`, from `dev/s2` c8b7a5d. Class: SYSTEM for the rows, BUG for Path hits and the Pulse Wall drawing, no change for evolution reach (no number or rule moved). Every number is bot v2, skill 1.0: **a bound, never a human reading.**

## Reproduce
28 arms: no-pick, the 18 Steered S1 arms, `-ArmSet S2Keepers`; seeds 1..15; 420 runs a batch.
```
& Tools\BotRuns.ps1 -Arms <the 19 arms> -ArmSet S2Keepers -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Check T-OFF-15 -Out Saved\BotRuns\r2_base
& Tools\BotRuns.ps1 <the same> -PrimaryFirst -Out Saved\BotRuns\r2_pf
python -I Tools\evolve_read.py Saved\BotRuns\r2_base Saved\BotRuns\r2_pf
python -I Tools\bot_compare.py control=<control> base=Saved\BotRuns\r2_base pf=Saved\BotRuns\r2_pf
```
Control: the same battery on a build of `dev/s2` at c8b7a5d. The batteries ran on game code of 010d3e5.

## Failing first
ab7615a holds the tests on stubs. Red: Catalog, Collection, Glacier, Kiln, Overload, PulseWallLook, Slipstream; Zenith stopped the run on a null flag in the test. CycloneStillness and GlacierNeighbour passed before any change: they pin behaviour that was already right. Catalog also failed its keyword line for every Path row until `Tools\Localize.bat` ran in the fresh worktree; that part was an unbuilt table, not the rows.

## Why evolution cards rarely reach a hand
**The cause is the bot's card scores.** The offers supply the upgrades, the conditions are not too narrow, and no bug is involved. A replay of the picks gives the primary's level at the hand, and matches the counted hands exactly.

| Steered runs, 405 | control | this branch | PrimaryFirst arm (R-BOT-16) |
|---|---|---|---|
| Reforging hands | 365 | 365 | 374 |
| Hands with an evolution card | 25 (0.07) | 25 (0.07) | 287 (0.77) |
| Mean primary level at the hand | 2.88 | 2.88 | 6.13 |
| Hands before L16 showing the primary's upgrade | 1552 | 1552 | 1987 |
| ... taken | 486 (31%) | 486 | 1506 (76%) |
| Upgrades among picks before L16 | 587 | 587 | 1594 |
| Blessings among picks before L16 | 4494 | 4494 | 3769 |
| At the boss / wins | 318 / 268 | 310 / 265 | 336 / 298 |
| T-OFF-15 | 318/318 | 309/310 | 336/336 |

- **Primaries stay under level 6: yes.** 340 of 365 primaries stand at 1 to 5 at the hand, most at 2.
- **The card's conditions are too narrow: no.** 77% of hands qualify with one card rule changed and the game untouched.
- **The bot's picks: yes.** The primary's upgrade was on the table 4.25 times a run and was passed over 1066 times, for a blessing 840 times and a new weapon 220. An upgrade scores 60 less reach; an own-archetype blessing scores up to 100 and the second Keeper's up to 94.

No number or rule moved: `C.OFFER.REFORGE_EVOLVE_MIN_LEVEL` stays at the source's 6, and no T-KEE criterion was touched. The hypothesis of over half stays missed for the plain bot and holds for a run that wants the card. Whether a human takes upgrades is unread, and that is the number this turns on. A batch that should read evolutions passes `-PrimaryFirst`.

The arm reads better than the plain bot (18 more at the boss, 30 more wins over 15-seed arms): the bot may undervalue upgrades. Its card scores were not changed, because every S1 and S2 reading stands on them.

Even so, evolution cards reach three weapons only (Railstorm 268, Blade Halo 11, Pulse Wall 8): the Rifle starts first and wins level ties. Seven evolutions have tests and no run.

## The three rows
The source had text for all three: `ST_Fusions`, `zh_overrides.csv`, D-41 for the reworks. Names and rules are not invented; the `{0}%`, the "3" and the "+1" were taken out to meet `weapons.md` Text.

| Card taken | Runs | Wins | rel median | Boss s, against control |
|---|---|---|---|---|
| Transform.Shade (Collection) | 29 | 25 | 1.02 | 61 / 60 |
| Transform.Radiance (Zenith) | 28 | 24 | 1.00 | 52 / 68 |
| Transform.Quicksilver (Slipstream Drive) | 27 | 27 | 1.01 | 64 / 61 |
| Transform.Storm | 110 | 93 | 1.00 | 73 / 75 |
| Transform.Forge | 88 | 55 | 1.00 | 78 / 84 |
| Transform.Tide | 68 | 28 | 1.00 | 73 / 82 |

`rel` and the card caveat as above. 38806 Shades rose in the battery, all in the runs that took the card, so Collection runs. R-RFG-18's band stays **missed low**, with the same two explanations, which this reading does not separate.

**Invented** (`reforging.md`, `number_change_log.csv`): Zenith giving a build with no Charge or Nova card those flags, and Slipstream Drive the dash volley and Momentum (D-41 calls it a combo and gives no numbers); the fan at the muzzle; `fan_deg`, `lance_pierce`; `leak_per_s`. Adapted: Collection fires the composed pattern, not a base one; lances are 0.3 by the source's R26, not the earlier 40 %. Dropped by D-41: Vector's blade trail.

## Budget
Spawned XP over model 0.789 against 0.789; runs at 30 decisions or more 58 against 72; mean decisions 28.81 against 28.81. T-OFF-15 reads 309/310 against 318/318: the miss is Tide/Stillness:Storm seed 7, 24 decisions, which stands on the floor in the control too (25) and died 86 s sooner. One run, the class of `STATE.md`'s seed 6, but R-RFG-18 says no worse than the control, so it is reported as missed. Wins 265 against 268 and boss 310 against 318 are within what 15 seeds move.

## Leads
- **Pulse Wall: a bug, fixed.** Every field drew as a disc, so a ring that touches only its outer band read as filling the circle. It now draws the band's two edges with the arc beads. No renderer was run: the visual list is tested, the look is not.
- **Path hits claimed weapon slot 0: a bug, fixed.** Found on the way to Glacier. Discharge, cleave and shatter hits reported slot 0, against `keepers.md`'s line that such hits carry none (T-RFG-22). This moves S1 arms a little.
- **Glacier: no bug.** A shatter leaves the next pillar standing. 60044 pillars rose; those that ended themselves lived 1.38 s on average against `pillar_s` 3, 24785 struck and 10164 run out (71% struck); the others died first. That is R-RFG-17. Whether it is a wall is a human reading. Discharges 257375, husks 43379.
- **Cyclone with Stillness: no bug.** The charge gathered at rest is carried by the first spin (T-RFG-10). At rest the weapon is silent, which is the card. No battery run took this pair.

## Perf and gate
`Tools\Perf.ps1`: PASS, game p50 2.24, p95 2.75 ms, GPU p95 4.90 raw (7.84 proxied, budget 12), 5975 bullets, 388 enemies, 1800 frames. The scene owns no Reforging card.
`Tools\Gate.ps1 -Base dev/s2`: PASS, 390 tests run, 0 failed, 0 warnings, smoke en and zh-Hans, contrast 7/7; pad, scope and mac not run.

## What still reads wrong
- Under the arm Blade Halo won 3 of 11 runs, Railstorm 71 of 84. Eleven runs, unsorted by arm, but the first number that card has.
- Under the arm Quicksilver/Rebound:Storm fell from 15 wins to 12 and Radiance/Lance:Storm to 13: evolving a Rifle may strip what their Keeper cards need. Not traced.
- Against the control Radiance/Solar:Tide fell from 11 at the boss to 8, Whirlpool from 5 to 2, Shade/Economy from 15 to 13. 15 seeds, not separated from noise; these arms lean on Keeper hits, so the slot fix may matter there.
- Zenith's boss time drops by a quarter on 28 runs: the one row that shows in any number.
- The perf scene owns no Reforging card: Shades, lances and rings are unmeasured at caps.
- A human, on any of this.
