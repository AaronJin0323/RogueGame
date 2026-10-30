# S2 arsenal: 10 weapons, 16 modifiers (2026-10-31)

Branch `dev/s2-arsenal`, from `dev/s2`. Bot bounds and tool readings only. **A bot reading is never a human reading**, and no human has played any of this.

## Reproduce
```
& Tools\BotRuns.ps1 -Arms <six arms> -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Unlock S2 -Out Saved\BotRuns\arsenal_v2
& Tools\BotRuns.ps1 -Arms <six arms> -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Out Saved\BotRuns\control_v2
python -I Tools\bot_compare.py control=Saved\BotRuns\control_v2 arsenal=Saved\BotRuns\arsenal_v2
```
Arms: `Steered:Storm:Tempest:Forge`, `Steered:Storm:ArcGrid:Forge`, `Steered:Forge:Hammer:Storm`, `Steered:Forge:Slam:Tide`, `Steered:Tide:Frost:Storm`, `Steered:Tide:Stillness:Storm`.

`-Unlock` is new: the S2 rows are non-starters (`offers.md` R-OFF-07, the pool caps are CORE), so they reach a run only through `DT.Unlock`. `-Unlock S2` passes every `starter=no` row, which also adds Gravity Mine, Fork and Ricochet.

## Failing first
Tests went in before any firing code: `Dusktracer.Arsenal.*`, 12 found, 11 failed (every weapon and modifier); only `Catalog` passed, because the rows were already there. After the work the suite is 257 of 257.

## Bot battery, skill 1.0, 15 seeds per arm
Cell: boss reach, wins, median death minute.

| Arm | control | arsenal |
|---|---|---|
| Forge/Hammer:Storm | 15/15 15 - | 15/15 14 16.4 |
| Forge/Slam:Tide | 7/15 6 13.3 | 4/15 4 9.3 |
| Storm/ArcGrid:Forge | 15/15 13 16.7 | 14/15 10 16.3 |
| Storm/Tempest:Forge | 14/15 14 9.0 | 15/15 13 16.5 |
| Tide/Frost:Storm | 13/15 11 10.6 | 9/15 6 10.3 |
| Tide/Stillness:Storm | 7/15 7 8.7 | 8/15 5 9.3 |
| **All** | 71/90 66 | 65/90 52 |

- Control on the Storm and Slam arms equals the `S2_bot_v2.md` v2 row exactly, so S1 content is unchanged there. The two Tide arms differ from that row; that reading predates D-019 reaching `dev/s2`, which is the likely cause, not proved here.
- With a wider pool the bot wins less (66 to 52). A shift of 2 or 3 on one arm can be noise; 14 over six arms is not likely to be, but it is a bot steering by reach and fit through a pool three times the width of its tuned one. It is not evidence that the new weapons are weak.

### Picks (90 runs)
New weapons taken: Flak 41, Spiral 35, Scatter 27, Mine 29, Rail 23, Seeker 23; against Drones 16, Ring 14, Arc Blade 8. New modifiers: Spread Wave 16, Refit 14, Spillover 10, Homing 10, Boomerang 8, Orbit Lock 8. Every weapon and modifier was picked. Reach data worked with no special case: the Arc Blade fell to 8 picks.

### Damage share (all arms)
Keeper.Storm 49.7, Keeper.Forge 18.6, Pulse Rifle 11.1, Keeper.Tide 3.7, Spiral 2.4, Seeker 2.4, Mine 1.7, Flak 1.7, Scatter 1.6, Drones 1.4, Rail 1.3, Ring 1.1, Scatter Shell 1.1, Lingering Field 0.5, Fork 0.4, Chain Trigger 0.4, Mirror 0.3, Arc Blade 0.2, Ricochet 0.1, Spillover 0.1, Split Core 0.1, Spread Wave under 0.05, Conduit under 0.05. Boomerang, Homing and Orbit Lock add no source: they reshape the weapon's own rounds. Max live bullets across the battery 423, nothing recycled.

## Perf at caps (C.ENEMIES.CAP enemies, C.BULLETS.CAP bullets, RTX 4080, x1.6 proxy)
| Scene | game p50 / p95 ms | GPU p95 ms (raw) | Verdict |
|---|---|---|---|
| default | 2.14 / 2.88 | 2.51 | PASS |
| Rail, Spiral, Scatter, Rifle L8, three modifiers each, Tempest full | 2.17 / 2.79 | 2.55 | PASS |
| Flak, Seeker, Mine, Rifle L8, three modifiers each, ArcGrid full | 2.15 / 2.98 | 2.54 | PASS |

Budget p50 8, p95 10, GPU 12. Earlier runs of the same scenes on a busier machine read game p95 3.7 to 3.8, GPU p95 about 6.1 raw (9.8 proxied), also PASS; other agents' runs share this machine, so read the raw GPU column as a floor. `-Cmds` scenes are not load-checked by `Perf.ps1`; the load printed was 5960+ bullets, 388 enemies. The stress emitter holds the pool at the cap, so weapon spawns recycle stress rounds there and each recycle scans the pool: no cost showed. The fan and chain weapons did not reach the budget.

## What reads wrong (for a human session, none of it tuned)
- Forge/Slam:Tide and Tide/Frost:Storm lose the most with the wider pool. Keeper share is unchanged in shape; the pool dilutes the weapons those arms lean on.
- Spread Wave is close to nothing as a damage source, though the bot picks it most of the six.
- Flak is picked most and sits mid-table for damage; shells choose a cell at launch and do not follow the crowd.
- The Rail Lance, the source's outlier, is 1.3 here against a kiting bot.
- Boomerang gives a lone target one hit per round, not the source's two.
- `ConvertMelee` on the Spiral arcs at the stream's cadence and on the Scatter folds a whole volley: Hammer stayed at 15/15 reach with either in hand, which a human should look at.
