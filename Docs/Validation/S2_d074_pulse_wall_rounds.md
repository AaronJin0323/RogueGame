# D-074: a Pulse Wall volley keeps its rounds and opens the disc (2026-11-12): stopped at item 5

Branch `s2/d074-pulse-wall-rounds`, from `s2/d070-pulse-wall-disc` (`7694549`). Class: RULE (`reforging.md` R-RFG-12). **Stopped before any reading of play**, as the record's item 5 says: the weapon's rounds are throttled by the chains the disc's strikes start. Nothing was tuned, nothing reverted, no gate.

Build: the branch at `9c941ca` (the rule), then `9c941ca` plus the throttle test's round count (this note's commit). `Tools\Build.bat`: `Result: Succeeded` both times.

## The throttling check (item 5)
`Dusktracer.Reforging.PulseWallThrottle`, a harness scene: the evolved Ring Emitter at level 8, 48 tough enemies standing inside the disc (three rings of 16 at 150, 300 and 450 uu), three ring lives (10.8 s). It is read twice: as the evolved weapon alone, and with a Keeper's `Chain` composed (Arc Grid's Key: 2 jumps, 350 uu, 0.6).

| | Weapon emissions (`Counters.Spawned`) | Throttled (`Counters.Throttled`) | Rounds spawned (the bullet pool's `Spawned`) |
|---|---|---|---|
| No chain | 363 | 0 | 360 |
| With the chain | 3 963 | 300 | **192** |

- A chain's jump spends one of the weapon's emissions (`ArcJump` calls `TryEmit`). The spawn-rate cap is one sliding window of `C.WEAPON.SPAWN_RATE_MAX_PER_S` per weapon (`DTWeaponSim.cpp`, `TryEmit`), shared by rounds and jumps.
- With 48 enemies inside the disc and four strikes a second, the jumps fill that window and the volleys' rounds are refused: 192 of 360, so 168 rounds are lost (47 %).
- So the record's stop applies: "If rounds are throttled, it stops and reports; it tunes nothing round it." By item 9, C as drafted (the port's band) is the fallback.

## The port's arms (the base, step 0)
Built on `s2/d070-note` `2818103` (`dev/s2`'s code, the port's Pulse Wall). `Tools\BotRuns.ps1 -Arms Steered:Storm:Tempest:Tide` (and `Steered:Storm:ArcGrid:Tide`) `-Carry Weapon.RingEmitter -Seeds 15 -BotVersion 2 -Skill 1.0`, each against its `-CarryHold`. Read with `python -I Tools\evolve_read.py --carry <arm> --hold <hold> --check-windows 0.95 0.9` and the D-061 context script (the crowd is read after the pick). Rates are a minute.

| | Tempest: port | Tempest: hold | Arc Grid: port | Arc Grid: hold |
|---|---|---|---|---|
| Before the boss: all sources | 13 604 | 16 942 | 15 287 | 17 080 |
| Before the boss: ratio to the hold | **0.80** | | **0.89** | |
| Boss fight: all sources | 5 143 | 16 650 | 3 592 | 15 104 |
| Boss fight: ratio to the hold | **0.31** | | **0.24** | |
| Whole time after the pick (decides nothing) | 0.78 | | 0.77 | |
| Died before the boss | 8 | 0 | 3 | 0 |
| Reached the boss, of 15 | 7 | 15 | 12 | 15 |
| Wins, of 15 | 1 | 15 | 3 | 14 |
| Damage taken before the boss | 22.3 | 1.8 | 13.9 | 1.6 |
| Damage taken in the boss fight | 23.3 | 11.1 | 26.2 | 20.5 |
| Boss fight length, s | 124 | 84 | 120 | 80 |
| After the pick: nearest enemy, uu | 194 | 477 | 244 | 511 |
| After the pick: live enemies | 37 | 14 | 24 | 11 |
| After the pick: damage taken | 24.7 | 4.0 | 17.9 | 6.0 |
| The Ring's own damage before the boss / in the fight | 2 176 / 170 | 2 799 / 5 793 | 1 386 / 181 | 2 731 / 5 240 |
| The Ring's throttled emissions in play, summed over 15 runs | 133 792 | 517 059 | 45 533 | 202 357 |

`WINDOWS: FAIL` on both pairs: the port's wall is the baseline this job is measured against, not a reading of it.

The throttled count is the `throttled` field of the run files' `weapon` records (one a minute, a count for that minute) for the Ring Emitter, summed per run. Printed, judged nowhere. Per run (seeds 1 to 15):
- Tempest port: 0, 0, 0, 45 037, 0, 0, 0, 0, 95, 88 660, 0, 0, 0, 0, 0.
- Tempest hold: 377, 911, 634, 236 831, 298, 170, 0, 201, 2 012, 237 256, 38 336, 0, 0, 0, 33.
- Arc Grid port: 0 for every seed except seed 10, 45 533.
- Arc Grid hold: 60, 0, 0, 0, 0, 427, 71, 0, 963, 132 902, 28 994, 0, 38 940, 0, 0.

The plain weapon (the holds) throttles too, in the long runs (seeds 4, 10, 11 and 13). So the cap binds in play before this job; the count is read per run, not as a rate.

## What else was built before the stop (on the branch, read only by tests)
- From D-070's branch, put back as the record's item 3 says:
  - The caps are back to the port's: Projectile, Radial, Area, Expiry.
  - The two `flag_compat.csv` cells are back to none. `matrix.csv` and `flag_compat.csv` equal `dev/s2`'s (no diff).
  - `weapons.md` R-WPN-08 equals `dev/s2`'s, and T-RFG-01's test is back to `dev/s2`'s form.
  - The none filter in `ReactToImpact` is gone.
- The disc is kept: no band, `dps` 6, `push_uu_s` 73 once a tick, the one outer arc.
- The rule:
  - A volley fires its rounds as the plain weapon does, and opens a ring when none is live. A free volley fires its rounds; with a ring live it strikes what stands inside it once more, and with none it opens one.
  - The cooldown clause is gone (`DTReforge.cpp`).
  - A ring's strike carries `bRingStrike` on its impact, and Backwash skips it (`DTWeaponSim.cpp`, the Pull case's origin branch). Placements already do not fire on it: Split fires on a RoundHit, a Detonation or an Expiry; Lingering Field only on a placing impact.
- Tests on the `9c941ca` build: `Dusktracer.Modifiers` 20 of 20 and `Card.Quicksilver.Vector` 12 of 12 pass. `Dusktracer.Reforging` fails 5 of 29:
  - `.PulseWall`, `.PulseWallDisc`, `.PulseWallGap`: their D-070 expectations count every hit as a ring strike, assert no rounds, and expect a cooldown as long as the ring's life. With the rounds back these must move.
  - `.PulseWallStrike`: its scene does not yet keep the volley's rounds off the enemy it reads.
  - `.PulseWallThrottle`: the stop above.
  - None of the first four was adjusted, since the job stops here.
- `Docs/02_Systems/reforging.md` R-RFG-12 and its test line carry the record's text. `modifiers.md` R-MOD-16 and `keepers.md` R-KEE-34 carry the free volley's rounds.
- `C.GATE.TEST_FLOOR` reads 484 on the branch: D-070's `PulseWallDisc` plus `PulseWallStrike` and `PulseWallThrottle`.

## Not done
- The table of the fourteen under this rule, the perf "after", the two arms, the reading and the gate.
- The base's readings were taken (step 0, on `s2/d070-note` `2818103`, `dev/s2`'s code): the perf "before" pair, and the port's arms with their holds in `Saved\BotRuns\d074_port_arm`, `d074_port_arm_arcgrid`, `d074_hold` and `d074_hold_arcgrid`.
  - Perf before, game thread p95: 4.28 and 4.40 ms, both PASS.
  - The holds equal tonight's D-070 holds run for run.
