# D-061: Pulse Wall's `dps`, the sweep (2026-11-12)

Branch `s2/d061-pulse-wall`, on `s2/d060-payday` at `ad664d7`. Class: TUNE (`09_Catalog/reforging.csv`, `Evolve.RingEmitter`, `dps`), by `10_Tuning.md` section 5. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5, D-061 ruling 2).

Build:
- `dps` 2 and the hold: the build of `372dfc6` (run stamp `372dfc6`; its code equals `ad664d7`, whose later commits are documents).
- `dps` 4, 6 and 8: `ad664d7` with the one cell edited and the catalog regenerated, built each time (`Tools\Build.bat`: `Result: Succeeded` three times; run stamp `ad664d7+dirty`). The cell was put back to 2 and regenerated after the sweep; no swept value was committed.

## The measure
The one of `S2_evolutions.md`, for the Ring Emitter pair only: the carry arm and its hold, 15 seeds, bot v2 at skill 1.0.
```
& Tools\BotRuns.ps1 -Arms Steered:Storm:Tempest:Tide -Carry Weapon.RingEmitter -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 6 -Stagger 0.4 -Out Saved\BotRuns\d061_arm_<dps>
& Tools\BotRuns.ps1 <the same> -CarryHold -Out Saved\BotRuns\d061_hold
python -I Tools\evolve_read.py --carry Saved\BotRuns\d061_arm_<dps> --hold Saved\BotRuns\d061_hold
```
`-Parallel` changes no reading (a run is deterministic per seed). The hold never takes the evolution, so the `dps` cell cannot reach it: it was run once, on the `dps` 2 build. "Weapon dpm" and "build dpm" are `evolve_read.py`'s medians over the intervals after the pick (the weapon's own damage, and every source), evolved against hold; the ratio is evolved over hold.

## Sweep (read, not kept)
| `dps` | Weapon dpm, evolved / hold | Build dpm, evolved / hold | Build ratio | Weapon ratio | Reach the boss, evolved / hold | Wins, evolved / hold |
|---|---|---|---|---|---|---|
| 2 (today) | 2 238 / 3 420 | 13 320 / 17 124 | 0.78 | 0.65 | 7 / 15 | 1 / 15 |
| 4 | 2 508 / 3 420 | 13 711 / 17 124 | 0.80 | 0.73 | 9 / 15 | 1 / 15 |
| 6 | 3 512 / 3 420 | 13 307 / 17 124 | 0.78 | 1.03 | 8 / 15 | 2 / 15 |
| 8 | 4 068 / 3 420 | 12 992 / 17 124 | 0.76 | 1.19 | 8 / 15 | 3 / 15 |

All 15 runs of each batch reached the Reforging hand and took the evolution; no fault, no zero reading. Today's row is close to `S2_evolutions.md`'s on its older tree (weapon 1 891 / 3 603, build 13 141 / 17 789, build ratio 0.74).

## Result
**No swept value brings the build to its hold's level.** The build ratio stays at 0.76 to 0.80 while the ring's own damage per minute nearly doubles from 2 to 8, passing the plain weapon's at 6. So the number is not the cause, as D-061's ruling 3 foresaw: the wall's damage rises and the build's does not, and the run still ends early. Nothing is committed to the catalog: the row stays at `dps` 2, no number-log row is written, and the next step is a RULE on the band, which the session drafts.

What a person at the first S2 session should still judge: whether a Pulse Wall build feels weaker than the rings it replaced, and what the wall's push does to the crowd the rest of the build is aimed at.
