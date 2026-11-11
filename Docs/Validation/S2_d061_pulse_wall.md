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

## Where the build's damage goes (from the sweep's run files)
Read by a scratch script over the run files of `d061_hold`, `d061_arm_2` and `d061_arm_8` (no new run). Per run, after the Reforging pick (the evolution in the arm, the Path card in the hold; the pick falls at a median 567 s in all three): damage by source and kills from the `sources` records whose 60 s interval starts at or after the pick, per minute of those intervals; XP from the `xp` records after the pick; `nearest_enemy_uu` and `live_enemies` from the run CSV's intervals that start at or after the pick; damage taken from `hero_damaged` after the pick, per minute to the run's end, by the attacker's role. Medians over the 15 seeds. These whole-interval figures differ a little from `evolve_read.py`'s (its build dpm 13 320 at `dps` 2 against 12 088 here), so compare inside this table only.

| After the pick | Hold | `dps` 2 | `dps` 8 |
|---|---|---|---|
| Keeper.Storm | 8 519 | 7 496 | 6 685 |
| Weapon.RingEmitter (Pulse Wall in the arm) | 3 191 | 1 960 | 3 223 |
| Keeper.Tide | 1 399 | 1 426 | 1 011 |
| Weapon.PulseRifle | 996 | 1 244 | 1 361 |
| Weapon.OrbitalDrones | 220 | 596 | 636 |
| Modifier sources (Chain Trigger, Lingering Field, Scatter Shell, Split Core) | 0 | 0 | 0 |
| All sources, damage per minute | 16 450 | 12 088 | 11 890 |
| Kills per minute | 264 | 248 | 242 |
| XP per minute | 351 | 344 | 345 |
| Level at the pick / at the end | 16 / 24 | 16 / 22 | 16 / 23 |
| Whole minutes after the pick | 7 | 5 | 7 |
| Nearest enemy, uu | 477 | 194 | 199 |
| Live enemies | 14 | 37 | 28 |
| Damage taken per minute | 4.0 | 24.7 | 17.1 |
| ... by role (Rusher, Tank, Swarm, Turret, Boss) | 0, 0, 0, 0, 2.4 | 8.2, 8.4, 1.4, 1.4, 0 | 8.7, 2.8, 0, 1.0, 2.0 |
| How the runs end | 15 victories | 1 victory; 14 deaths (Boss 6, Tank 5, Swarm 2, Rusher 1), minutes 10.7 to 17.4 | 3 victories; 12 deaths (Boss 5, Rusher 3, Tank 2, Turret 1, Swarm 1), minutes 10.5 to 17.4 |

Against the hold, at `dps` 2 the build loses about 4 360 a minute. Of what the medians show by source: Pulse Wall loses 1 231 against the plain rings (about 28 %), Keeper.Storm 1 023 (about 23 %); the rifle (+248) and the drones (+376) gain a little and Tide stands still. At `dps` 8 the build loses about 4 560: the wall now deals what the rings did (+32), and Keeper.Storm loses 1 834 (about 40 %) and Tide 388 (about 9 %), with the rifle (+365) and the drones (+416) again a little up. (Medians by source do not add up to the median total; the shares are of the total's difference.)

The build at the pick is the same in every run of all three batches: Pulse Rifle, Ring Emitter and Orbital Drones, with up to two modifiers, most often on the Ring Emitter (Scatter Shell, Lingering Field, Split Core, Chain Trigger, Backwash). Base reach from `weapons.csv`: the Pulse Rifle 900 uu and the plain Ring Emitter's rounds 1 350 uu reach an enemy held at 500 to 600 uu; the Orbital Drones' bolts reach 600 uu from an orbit of 160 uu (`C.WEAPON.DRONE_ORBIT_RADIUS_UU`), so they reach the band's inner edge and no further; Pulse Wall itself strikes only its outer band, 500 to 600 uu (`radius_uu` 600, `band_uu` 100).

What the numbers say: the wall does not keep the crowd at its band. After the pick the nearest enemy stands at about 195 uu against 477 in the hold, two to three times as many enemies are alive, and the hero takes four to six times the damage, most of it from Rushers and Tanks. At `dps` 2 the build's loss is split between the wall's own damage and Keeper.Storm's; raising `dps` to 8 gives the wall back the rings' damage and Keeper.Storm loses more, so the build stands where it was. These files do not show by which mechanism Keeper.Storm's damage falls.

## Result
**No swept value brings the build to its hold's level.** The build ratio stays at 0.76 to 0.80 while the ring's own damage per minute nearly doubles from 2 to 8, passing the plain weapon's at 6. So the number is not the cause, as D-061's ruling 3 foresaw: the wall's damage rises and the build's does not, and the run still ends early. Nothing is committed to the catalog: the row stays at `dps` 2, no number-log row is written, and the next step is a RULE on the band, which the session drafts.

What a person at the first S2 session should still judge: whether a Pulse Wall build feels weaker than the rings it replaced, and what the wall's push does to the crowd the rest of the build is aimed at.
