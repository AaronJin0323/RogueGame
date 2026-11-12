# S2: D-079, Blade Halo keeps its bolts and gains the source's blades

Branch `s2/blade-halo-bolts`, cut from `dev/s2` at `1106820` (41d510b plus the D-075 note). Class RULE: `reforging.md` R-RFG-07 and the row `Evolve.OrbitalDrones`. **The hypothesis is met on all three clauses** (Tempest, the judged pair): before the boss 0.99 of the hold, the boss fight 1.03, and the weapon's own hits a minute 269 against the hold's drones' 247 (1.09).

Build:
- **Base, `268174f`.** This is `dev/s2` plus the perf scene's `Drones` option alone, committed first. Read on it: the perf "before" pair, the scene of record beside it, and the four base batteries (Tempest and Arc Grid, each port arm and hold).
- **The rule, `54a64dc`.** The blades are picked from `s2/d071-blade-halo` as `a807540` and `1c7e20e`; the rule itself is `54a64dc`. Read on it: the first throttle scene (with the chain kept) and the Reforging, Modifiers, Keepers and Weapons tests.
- **The tip, `df753ed`.** One mark (`bRingStrike`) for a blade's strike; the chain is dropped from it. Read on it: the throttle scene again, the Reforging tests, the Tempest hold, the rule's arms, the perf "after" pair, the gate and the five seed 9107 arms.

## The record's hypothesis
"We believe that a Blade Halo that keeps its bolts holds its build, because the evolved weapon then lands what the plain drones land and the blades add to it; ... judged: the build after the pick reads at least 0.95 of the hold before the boss and at least 0.9 in the boss fight, and the weapon's own hits a minute are at least 0.95 of the hold's drones" (D-079).

## Step 0: the perf scene's option
`DT.PerfScene S2 Drones` puts an evolved Orbital Drones in the Spiral Projector's place. It keeps the same three modifiers (Pierce, Backwash, Spillover) and is off by default. Without the option the scene of record is built as before: same build line, same load line.

The game log of an option-on run shows it took:
- `Weapon.OrbitalDrones result 0`, and its three modifiers socketed, each with result 0;
- `Evolve.OrbitalDrones owned`;
- the line "Drones: Weapon.OrbitalDrones evolved in the Spiral Projector's place".

No sim file and no file of `Tools/Perf.ps1` is touched; `-Cmds "DT.PerfScene S2 Drones"` selects the option.

## Step 1: the source's blades
- **What was picked.** `39035cd` and `277f918` from `s2/d071-blade-halo`: the blade code, the six cells and their number-log rows, and the tests.
- **What was not brought.** D-071's note (`5cf4387`); that branch is kept for the first S2 session.
- **The number log** took both sides. Its six rows now cite D-079 and the `--check-windows 0.95 0.9` reading.
- **`DTCatalogGen.h`** was regenerated after the pick, not taken from the auto-merge.

## Step 2: the rule
- **The bolts are the plain weapon's.** `ApplyEvolution` no longer sets the count or the damage for `ContactBlades`. The drones keep their constant orbit and turn, and fire as the plain weapon does.
- **The blades run beside them** on their own angle (`BladeAngleDeg`, at `orbit_deg_s`) and their own contact tick (`BladeTimer`).
  - There are `blades` of them.
  - Each strikes for `dmg_ratio` of the weapon's composed damage.
- **A free volley** fires the bolts and strikes every blade once (`StrikeBlades`); a dash volley does both at its multiplier.
- **The visuals** gather both: the drones on their constant ring and the blades on theirs. How the blades look is the Visual session's (D-079 item 7).
- **The row's caps** are now `Orbit;Projectile`, with a number-log row.
  - `matrix.csv` does not move on regeneration. It holds plain weapons only, so an evolution's caps never appear in it.
  - The proof that no socket goes dormant is the BladeHalo test: Pierce and Backwash, socketed on the plain drones, stay live at the pick.
- **The card**, en and zh: "The drones keep firing and gain blades that cut what they touch. The blades' orbit widens the faster you move." The zh line is a first draft for the native reader, marked so in the text table's Context column.
- **R-KEE-34 and R-MOD-16** each take one clause: Blade Halo fires its bolts and its blades each strike once.

## Step 3: what a blade's strike carries
**The mark.** A blade's strike carries D-074's own mark, `bRingStrike`, set on the impacts `StrikeBlades` makes. A second mark that kept the chain was used for the scene's first read only, and was then taken out: nothing reads it.

**The throttle scene.** `Dusktracer.Reforging.BladeHaloThrottle`: level 8, 72 tough bodies standing in the blades for 5 s (three rings at the rest orbit and 25 uu either side), and a Keeper's Chain composed as Arc Grid's Key (2 jumps, 350 uu, 0.6).

| Read | Chain | Emissions | Throttled | Bolts | Blade strikes |
|---|---|---|---|---|---|
| chain kept on a blade's strike (`54a64dc`) | without | 76 | 0 | 76 | 1252 |
| | with | 2003 | 361 | 66 | 1252 |
| chain dropped (`df753ed`) | without | 76 | 0 | 76 | 1252 |
| | with | 228 | 0 | 76 | 1252 |

With the chain kept, bolts were refused. So a blade's strike carries the Patron's status and not the chain, as D-074's ruling 10 rules for the ring. R-RFG-07 says so, and the scene stands as the test: equal bolts, nothing refused, equal strikes.

**The nine modifiers that fit the drones, from code** (`DTWeaponSim.cpp` `ReactToImpact`; a blade's strike is a `MineHit` impact with `bRingStrike`):

| Modifier | On the bolts | On a blade's strike |
|---|---|---|
| Pierce | `pierce_add` more pierce, as on the plain weapon | nothing; a strike spends no pierce |
| ChainTrigger | a kill fires one free volley: the bolts, and every blade once | a strike's kill counts |
| LingeringField | a bolt's impact leaves a zone | nothing; a `MineHit` is not an impact that places |
| Backwash | a bolt's hit pulls toward the origin | nothing; `bRingStrike` |
| Conduit | every hit applies the Patron's status | every strike applies it |
| Fork | side bolts each volley | nothing |
| SpreadWave | a bolt's end becomes a short wave | nothing |
| Spillover | a kill's overkill jumps to the nearest enemy | a strike's kill spills too (`SpillOverkill` reads the slot's kills) |
| Refit | a kill fits a modifier, once an interval | a strike's kill counts |

## Step 4: tests
New: `Dusktracer.Reforging.BladeHaloThrottle`. `C.GATE.TEST_FLOOR` goes from 484 to 485, with a number-log row.

Moved expectations, old and new:
- **`Reforging.BladeHalo`.**
  - Old: "no bolts" (0 bullets), and "the blade count" equal to the spec's count (4).
  - New: "the bolts are the plain weapon's": count, damage and cooldown are the plain spec's, and in 5 s on one enemy at 600 uu the plain and the evolved weapon each spawn 75 bolts and land 71 hits.
  - Blade strikes are now told from bolts by their damage (`dmg_ratio` of the weapon's). The blades' turn is read from `GetBladeAngle`, and the evolved weapon's drones turn at their constant.
  - It also gains the sockets check (Pierce and Backwash stay live).
- **`Reforging.Caps`.** Old: the Blade Halo was the evolution that silences a socket. New: Sweep Beam (`Weapon.RailLance`), which loses `Projectile` and `Expiry`; the Halo loses no cap now.
- **`Modifiers.ChainTrigger.BladeHalo`.** Old: "and still no bolts", with every chain hit at the blade's damage. New: every chain hit is a blade's strike or a bolt; the blades strike once each as the modifier's (at least `blades`, at most `blades` × the most per touch), and the modifier's bolts fire.
- **`Keepers.S2.Card.Quicksilver.Vector.BladeHalo`.** Old: "and still no bolts", with the strikes counted against the spec's count. New: every blade strikes once at the Key's multiplier, and the bolts fire, a bolt at the Key's multiplier.

## Perf
`Tools/Perf.ps1 -Scene S2`, 1920x1080, 1800 frames. The game thread's p50 and p95 are in ms, the GPU's p95 in ms, and the load is in bullets and enemies. All lines are from the same evening.

| Read | Build | Scene | Game thread p50 / p95 | GPU p95 | Load | Line |
|---|---|---|---|---|---|---|
| before 1 | `268174f` | `Drones` | 3.02 / 4.42 | 4.10 | 5780 / 399 | PASS |
| before 2 | `268174f` | `Drones` | 2.92 / 4.47 | 4.09 | 5771 / 399 | PASS |
| scene of record | `268174f` | (none) | 3.07 / 4.82 | 4.26 | 5815 / 399 | PASS |
| check | `268174f` | `Drones` | 3.99 / 6.02 | 8.99 | 5789 / 399 | FAIL |
| after 1 | `df753ed` | `Drones` | 2.92 / 4.37 | 4.10 | 5775 / 399 | PASS |
| after 2 | `df753ed` | `Drones` | 2.91 / 4.37 | 4.19 | 5774 / 399 | PASS |

- **The scene of record beside D-072's lines.** D-072's two scene-of-record reads earlier tonight, on its own build, were p95 4.39 and 4.68, with loads of 5798 and 5805.
- **The check run.** It was read only to keep the game log that shows the option took, and is no part of either pair. Its GPU and game thread both rose together, which reads as the PC's own load.
- **The game log of the "after" runs** shows the same option lines as the "before" check.
- **The stop line** is 0.3 ms of game thread at p95 against the base. After minus before is 4.37 − 4.42 and 4.37 − 4.47, so −0.05 and −0.10 ms: under it.

## Step 5: the reading
**The hold.**
- The Tempest hold read on the tip equals step 0's hold 15 of 15 in outcome, `end_s`, `steps` and `input_hash`. The rule moves nothing in a run that does not evolve the drones, so step 0's holds serve both pairs.
- Step 0's Tempest hold also equals D-075's (`d075_hold_branch`, read on `281959d`) 15 of 15: D-074's landing did not move it.

**The fight window.** `Tools/evolve_read.py` (`split_windows`) counts every source's damage per minute, from every `hit` record of player 0, to everything alive in the window, not damage to the boss. A fight window over 1.0 therefore does not say the boss dies faster. On Tempest the evolved build kills the boss more slowly than the hold (a median fight of 128 s against 93 s), although its fight window reads 1.03.

| | Tempest hold | Tempest port | Tempest rule | Arc Grid hold | Arc Grid port | Arc Grid rule |
|---|---|---|---|---|---|---|
| before the boss against the hold | | 0.80 | **0.99** | | 0.94 | 1.00 |
| boss fight against the hold | | 0.67 (6 runs) | **1.03** (13 runs) | | 0.89 (13 runs) | 1.35 (15 runs) |
| weapon hits a minute after the pick | 247 | 60 | **269** | 168 | 30 | 206 |
| weapon damage a minute after the pick | 3262 | 1412 | 3865 | 2460 | 730 | 3096 |
| weapon's share of the build after the pick | | 0.09 | 0.25 | | 0.05 | 0.20 |
| weapon throttled (of spawned) | 0 of 325109 | 0 of 133591 | 68 of 353104 (one run) | 0 of 224058 | 0 of 107341 | 0 of 254271 |
| nearest enemy after the pick, before the boss: median (p25, p75), uu | 434 (352, 475) | 205 (172, 259) | 388 (329, 461) | 456 (421, 495) | 242 (212, 290) | 424 (384, 467) |
| live enemies after the pick, median | 23 | 32 | 26 | 19 | 27 | 22 |
| damage taken a minute before the boss | 1.7 | 33.5 | 1.5 | 1.7 | 10.3 | 0.0 |
| damage taken a minute in the fight | 11.6 | 26.2 | 25.1 | 20.6 | 23.4 | 15.2 |
| the fight's length, median s | 93 | 102 | 128 | 121 | 119 | 94 |
| died before the boss | 0 | 9 | 2 | 0 | 2 | 0 |
| reached the boss, won | 15, 15 | 6, 4 | 13, 13 | 15, 15 | 13, 9 | 15, 15 |
| a socket dormant | | Mod.Pierce once | none | | Mod.Pierce once | none |

How the table was read:
- **The holds' columns.** Their "after the pick" is after their Path card at the same hand.
- **The blades' share of the weapon's damage** is not read. The blades and the bolts carry one source, `Weapon.OrbitalDrones`, so the run files do not tell them apart.
- **The nearest enemy.** With the bolts back, the bot holds its crowd off as with the plain drones (388 and 424 uu): a body seldom stands where the blades reach.

**The two Tempest runs that die before the boss.** The hold wins both seeds.
- Seed 13: picks the halo at 564.9 s and dies at 676.4 s, 111.5 s after the pick. Last hit Contact/Rusher; hero level 18; the drones at level 6. At that hand the hold took `Transform.Storm` and won at 965.6 s.
- Seed 3: picks the halo at 554.6 s and dies at 876.8 s, 322.2 s after the pick. Last hit Contact/Swarm; hero level 23; the drones at level 7. At that hand the hold took `Transform.Storm` and won at 964.3 s.

On Arc Grid no run of the rule dies before the boss.

Nothing above is judged but the three clauses (D-050); nothing was tuned on it.

## Step 6: the gate
GATE

## Not shown
The blades' share of the weapon's damage: blades and bolts carry one source, `Weapon.OrbitalDrones`. Telling them apart would need a source of its own for a blade's strike, and this job did not add one.

How a person plays it. Whether a player stands near enemies to use the blades, and whether the blades matter, are the S2 kit's questions (D-079 item 9).
