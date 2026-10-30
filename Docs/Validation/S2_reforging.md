# S2 L16 Reforging (2026-10-31)

Class: SYSTEM. Layer: `Combat/` (evolutions, Path transformations, one enemy-round removal), the offer hand and card text, telemetry, a bot card rule. Replaces: the S1 hand, which was always filler blessings; on this branch filler remains the fallback. Spec: `Docs/02_Systems/reforging.md`. Every number below is a bot on the S1 Keepers, version 2, skill 1.0: **a bound, never a human reading.**

## Reproduce
19 arms (no-pick plus 18 Steered), seeds 1..15, 285 runs per batch, sim cap 1100 s.
```
& Tools\BotRuns.ps1 -Arms <the 19 arms> -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Check T-OFF-15 -Out Saved\BotRuns\rfg2
python -I Tools\bot_compare.py control=<control> reforge=Saved\BotRuns\rfg2
```
Control: the same battery on a build of `dev/s2` at 5bcac1c, before this branch. The first battery, `rfg1`, was stopped part way by the scheduler because runs showed Unreal's splash window; it was thrown away whole and is not read here. `rfg2` ran with `-nosplash`.

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
- **Evolution slot: missed.** 15 of 232 hands held an evolution card, against a hypothesis of over half. Eleven evolutions were taken, on three weapons. The other seven weapons' evolutions, and Railstorm beyond those nine runs, are covered by tests only. Why primaries sit under the level was not traced; the bot does not read rerolls and takes blessings first, so it may be the bot and not a player.
- No number was changed in answer. Raising power spends the top of the T-OFF-15 band, and a bot median at a field cap is weak evidence to spend it on.

## Not read
- A human, on any card. The HUMAN line of `reforging.md` is open.
- Screen readability, the two-second rule, density and perf: the battery has no renderer, and `Tools\Perf.ps1` was not run on this branch. The new visuals reuse arc, field, drone and mine; a Pulse Wall draws as a filled disc, not a ring.
- How long pillars last, whether Overload discharges often, and husk counts: no counter writes them.
