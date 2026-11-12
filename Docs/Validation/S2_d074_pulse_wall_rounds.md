# D-074: a Pulse Wall volley keeps its rounds and opens the disc (2026-11-12)

Branch `s2/d074-pulse-wall-rounds`, from `s2/d070-pulse-wall-disc` (`7694549`), with `dev/s2` `281959d` merged in (`c862713`). Class: RULE (`reforging.md` R-RFG-12). The job stopped once at item 5 (rounds throttled by the disc's chains); the record's rulings 10 to 13 answered it, and the job went on.

## Builds, and what was read on each
| Build | What it is | Read on it |
|---|---|---|
| `2818103` (`s2/d070-note`) | `dev/s2`'s code before the merge: the port's Pulse Wall | the perf "before" pair of step 0 (4.28, 4.40); the port's arms and both holds |
| `61042fd` | this rule, rulings 10 and 11, before the merge | the throttle scene; the moved tests; the rule's two arms; a perf pair under load (decides nothing, below) |
| `281959d` (`dev/s2`, detached) | the base after D-072 landed | the perf base pair of 20:29 and 20:30 |
| `c862713` | this branch merged with `dev/s2` `281959d` | the Tempest hold again; the perf pair of 20:32 and 20:34; the whole gate; the five seed 9107 arms |

The Tempest hold read on `c862713` equals `d074_hold` (read on `2818103`) run for run: all 15 runs' `bot.csv` are byte-equal. D-072's rule moved none of these runs, so the arms read before the merge stand.

## The throttle scene (item 5, ruling 11)
`Dusktracer.Reforging.PulseWallThrottle`, a harness scene: the evolved Ring Emitter at level 8, 48 tough enemies standing inside the disc (three rings of 16 at 150, 300 and 450 uu), three ring lives (10.8 s). It is read as the evolved weapon alone, and with a Keeper's `Chain` composed (Arc Grid's Key: 2 jumps, 350 uu, 0.6).

| | Build | Weapon emissions (`Counters.Spawned`) | Throttled (`Counters.Throttled`) | Rounds spawned (the bullet pool's `Spawned`) |
|---|---|---|---|---|
| No chain | `9c941ca` and `61042fd` | 363 | 0 | 360 |
| With the chain, the strike chaining (the stop) | `9c941ca` | 3 963 | 300 | 192 |
| With the chain, ruling 10 | `61042fd` | 1 035 | 0 | **360** |

- A chain's jump spends one of the weapon's emissions (`ArcJump` calls `TryEmit`), and the cap is one sliding window of `C.WEAPON.SPAWN_RATE_MAX_PER_S` per weapon.
- Ruling 10: the `Chain` case of `ReactToImpact` (`DTWeaponSim.cpp`) skips an impact with `bRingStrike`, the mark Backwash's guard reads. Status stays.
- The chains now ride the rounds only: 1 035 emissions, none refused, 360 rounds as without the chain. The test keeps the expectation that holds: as many rounds with the chain as without.

## The rule, as built
- A volley fires its rounds as the plain weapon does, and opens a ring when none is live. A free volley fires its rounds; with a ring live it strikes what stands inside it once more, and with none it opens one.
- The cooldown clause is gone (`DTReforge.cpp`): the weapon's cooldown is its own.
- The disc is kept from D-070: no band, `dps` 6, `push_uu_s` 73 once a tick, one outer arc.
- A ring's strike carries `bRingStrike`. Backwash's origin pull and the Keeper's `Chain` skip it. Status-apply acts on it.
- `reforging.md` R-RFG-12 says "status" where it said "status and chain", and that a chain rides the volley's rounds, not the strike.

## The fourteen modifiers under this rule (from code)
All fourteen that fit the Ring Emitter stay live and act on the rounds as on the plain weapon. On a ring's strike (`EImpact::MineHit` with `bRingStrike`):

| Modifier | On a ring's strike | Why (`DTWeaponSim.cpp`) |
|---|---|---|
| Split Core | nothing | `Split` fires on a RoundHit, a Detonation or an Expiry |
| Scatter Shell | nothing | the same `Split` case as Split Core |
| Lingering Field | nothing | `Field` places on a RoundHit, an ArcSweep or a Detonation |
| Backwash | nothing | the origin pull skips `bRingStrike` |
| Conduit | the Patron's status, once a struck enemy a tick | `StatusApply` acts on any enemy hit; bounded by the status caps |
| Pierce, Homing, Orbit Lock, Boomerang | nothing | they shape a round's flight; a strike is no round |
| Ricochet | nothing | it bounces a spent RoundHit |
| Spread Wave | nothing | it acts on a round's Expiry |
| Chain Trigger | a strike's kill fires one free volley | one per `min_interval_s` |
| Spillover | a strike's kill jumps its overkill once | one jump per kill |
| Refit | a strike's kill may fit a modifier | once per `interval_s` |

None multiplies without a bound. The Keeper's `Chain` does nothing on a strike (ruling 10).

`Dusktracer.Reforging.PulseWallStrike` binds the sentences on placements and on Backwash with three sockets, since a weapon holds three: Split Core, Lingering Field and Backwash. Scatter Shell goes through the same `Split` case as Split Core, so it is not socketed separately (ruling 13).

## Expectations that moved
| Test | Old (D-070) | New (D-074) | Why |
|---|---|---|---|
| `Dusktracer.Reforging.PulseWall` | "no rounds": 0 bullets after the volley; every hit on the rim and the core is a strike | the volley fires its rounds: `Count` bullets (24); a hit is a strike (`dps` × `Damage` × `tick_s`, 22.5) or a round (`Damage`, 15) | the rounds are back |
| `.PulseWallDisc` | every hit on the near enemy and the boss is a strike; pushes equal hits | a hit is a strike or a round; strikes are counted by their damage, and pushes equal strikes (6 strikes, 0 round hits, 6 pushes, 200 to 309.5 uu) | the rounds are back |
| `.PulseWallGap` | the cooldown equals the ring's life: at most 10 steps of 1 080 with no ring | a ring follows a ring within one cooldown: the longest stretch with none is at most the weapon's cooldown in steps, plus one | the cooldown clause is gone |
| `.PulseWallStrike` | scene 1 placed the enemy between two round lines; scene 2 asked `GetFieldsOpened` for Lingering Field's zone | scene 1 clears the volley's rounds before the enemy stands, so only the ring meets it; scene 2 stands the enemy on a round's line and reads the ring and a zone live together | a round reached the enemy between the lines; a modifier's zone does not count in `GetFieldsOpened` |
| `.PulseWallThrottle` | with the chain: 192 rounds of 360 | 360 of 360 | ruling 10 |

`Dusktracer.Reforging` 29 of 29, `Dusktracer.Modifiers` 20 of 20 and `Card.Quicksilver.Vector` 12 of 12 pass on `61042fd`. `C.GATE.TEST_FLOOR` reads 484: D-070's `PulseWallDisc` plus `PulseWallStrike` and `PulseWallThrottle`.

## What fell away from D-070's branch
- The caps are back to the port's: Projectile, Radial, Area, Expiry. The row's caps line in `number_change_log.csv` is dropped.
- The two `flag_compat.csv` cells are back to none; `matrix.csv` and `flag_compat.csv` equal `dev/s2`'s.
- `weapons.md` R-WPN-08 equals `dev/s2`'s, and T-RFG-01's test is back to `dev/s2`'s form.
- The none filter in `ReactToImpact` is gone.
- The cooldown clause in `DTReforge.cpp` is gone.

## The readings of play
`Tools\BotRuns.ps1 -Arms Steered:Storm:Tempest:Tide` (and `Steered:Storm:ArcGrid:Tide`) `-Carry Weapon.RingEmitter -Seeds 15 -BotVersion 2 -Skill 1.0`. The rule's arms on `61042fd`, the port's arms and both holds on `2818103`. Read with `python -I Tools\evolve_read.py --carry <arm> --hold <hold> --check-windows 0.95 0.9` and the D-061 context script (the crowd is read after the pick). Rates are a minute.

**The Tempest pair, judged: `WINDOWS: PASS (RingEmitter before the boss 1.01 >= 0.95, RingEmitter boss fight 1.12 >= 0.9)`.** The Arc Grid pair, printed: 0.99 and 1.15.

| | Tempest: rule | Tempest: port | Tempest: hold | Arc Grid: rule | Arc Grid: port | Arc Grid: hold |
|---|---|---|---|---|---|---|
| Before the boss: all sources | 17 035 | 13 604 | 16 942 | 16 941 | 15 287 | 17 080 |
| Before the boss: ratio to the hold | **1.01** | 0.80 | | **0.99** | 0.89 | |
| Boss fight: all sources | 18 648 | 5 143 | 16 650 | 17 411 | 3 592 | 15 104 |
| Boss fight: ratio to the hold | **1.12** | 0.31 | | **1.15** | 0.24 | |
| Whole time after the pick (decides nothing) | 1.04 | 0.78 | | 1.03 | 0.77 | |
| Died before the boss | 0 | 8 | 0 | 0 | 3 | 0 |
| Reached the boss, of 15 | 15 | 7 | 15 | 15 | 12 | 15 |
| Wins, of 15 | 15 | 1 | 15 | 15 | 3 | 14 |
| Damage taken before the boss | 3.3 | 22.3 | 1.8 | 0.0 | 13.9 | 1.6 |
| Damage taken in the boss fight | 11.8 | 23.3 | 11.1 | 13.7 | 26.2 | 20.5 |
| Boss fight length, s | 73 | 124 | 84 | 78 | 120 | 80 |
| After the pick: nearest enemy, uu | 487 | 194 | 477 | 523 | 244 | 511 |
| After the pick: live enemies | 14 | 37 | 14 | 12 | 24 | 11 |
| After the pick: damage taken | 2.8 | 24.7 | 4.0 | 2.8 | 17.9 | 6.0 |
| The Ring's own damage before the boss / in the fight | 3 764 / 6 523 | 2 176 / 170 | 2 799 / 5 793 | 3 415 / 6 086 | 1 386 / 181 | 2 731 / 5 240 |

The port's `WINDOWS: FAIL` on both pairs is the baseline this rule is measured against.

### The weapon's throttled count in play (printed, judged nowhere)
The `throttled` field of the run files' `weapon` records for the Ring Emitter (one record a minute, each the count of that minute), summed over each whole run. Seeds 1 to 15:
- Tempest rule: 761, 3 172, 109, 229 766, 226, 125, 0, 60, 663, 243 619, 41 143, 0, 31, 0, 160. Sum 519 835.
- Tempest port: 0, 0, 0, 45 037, 0, 0, 0, 0, 95, 88 660, 0, 0, 0, 0, 0. Sum 133 792.
- Tempest hold: 377, 911, 634, 236 831, 298, 170, 0, 201, 2 012, 237 256, 38 336, 0, 0, 0, 33. Sum 517 059.
- Arc Grid rule: 14, 0, 0, 0, 0, 0, 388, 0, 605, 165 450, 43 833, 0, 0, 0, 0. Sum 210 290.
- Arc Grid port: 0 for every seed but seed 10, 45 533. Sum 45 533.
- Arc Grid hold: 60, 0, 0, 0, 0, 427, 71, 0, 963, 132 902, 28 994, 0, 38 940, 0, 0. Sum 202 357.

The rule's arm is level with the plain weapon's hold on both pairs, in the same long seeds (4, 10, 11, 13): the refusals are the plain weapon's own and not this rule's.

## The perf bound (a bound on a shared machine, not a reading of record)
`& Tools\Perf.ps1 -Scene S2`. The stop line is 0.3 ms of game thread at p95.

| Run | Build | Time | Game thread p50 / p95, ms | `SimTick` mean / p95, ms | GPU p95, ms | Load line (bullets p50, enemies p50) | PERF |
|---|---|---|---|---|---|---|---|
| before 1 | `2818103` | step 0 | 2.92 / 4.28 | 0.86 / 1.80 | 4.25 | 5 801 (cap 6 000), 400 | PASS |
| before 2 | `2818103` | step 0 | 2.97 / 4.40 | 0.91 / 1.84 | 4.22 | 5 805, 400 | PASS |
| after, no base beside | `61042fd` | 20:09 | 3.43 / 5.35 | 1.18 / 2.53 | 9.49 | 5 814, 399 | FAIL (GPU) |
| after, no base beside | `61042fd` | 20:10 | 4.01 / 5.55 | 1.33 / 2.64 | 10.03 | 5 813, 399 | FAIL (GPU) |
| base 1 | `281959d` | 20:29 | 3.51 / 5.34 | 1.12 / 2.46 | 4.59 | 5 814, 399 | PASS |
| base 2 | `281959d` | 20:30 | 5.94 / 8.33 | 1.93 / 3.17 | 4.60 | 5 810, 399 | PASS |
| after 1 | `c862713` | 20:32 | 3.38 / 5.23 | 1.13 / 2.45 | 4.69 | 5 823, 399 | PASS |
| after 2 | `c862713` | 20:34 | 3.86 / 5.74 | 1.24 / 2.64 | 4.68 | 5 816, 399 | PASS |

- The pair read in turn: after 1 against base 1 is 5.23 against 5.34 (−0.11 ms). Base 2 was read under load (p50 5.94); after 2 against base 2 is lower again. No run crosses the stop line against the base read beside it.
- The two 20:09 and 20:10 runs were read under load, with the GPU budget failed and no base beside them. By rule 25 they decide nothing; the GPU read 4.69 and 4.68 on the same code at 20:32 and 20:34.
- The scene is at the bullet cap in every run (5 801 to 5 823 of 6 000): the rounds back on the Pulse Wall add no load the scene can show.

## The gate
`& Tools\Gate.ps1 -Base dev/s2` on `c862713`: `GATE: PASS (c862713; 3 not run; mask FFFFF3FC)`. Not run: pad (no package), scope (no brief in flight), mac (deferred, D-007).

The five seed 9107 arms on `c862713` (rule 24): `& Tools\BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107`. `input_hash`: None `00e0d4ec`, Random `51369747`, Steered:Forge:Hammer `e3293942`, Steered:Storm:Tempest `b046559b`, Steered:Tide:Frost `def51848`. All five equal the expected values.
