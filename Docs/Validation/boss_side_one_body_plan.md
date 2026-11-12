# The boss's side of the Reforging band: read against the Warden's own hull, on the probe (Draft 21, an amendment to D-082 and D-085)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for on 2026-11-13 after the reader's first interval table. Written on `plan/boss-side-one-body` from `main` at 06d4f7c. Nothing in it is in force until the scheduler accepts and numbers it.

**Written from figures already held, before the pause of 2026-11-13. No scene with one body has been run.** The choice of body is made here from the boss's own rules; the reading on two bodies that the scheduler asked for is the first item of the equipment's own check, and it can still overturn the choice. A bot reading is never a human reading.

A constant or a test id that exists only on `dev/s2` is written without its prefix. Every figure marked "arithmetic" is mine and not a reading. The intervals quoted are the reader's lines on `s2/evolve-read-decline` before its hand-over (ninety in a hundred, resampled by seed, complete pairs); the hand-over's printed lines replace them.

## What play can and cannot say about the boss's side
| Reading (fifteen seeds a pair) | Before the boss | Damage dealt to the boss a minute |
|---|---|---|
| Forge's card against no pick | 1.003 (0.978 to 1.014) | 0.948 (0.721 to 1.202) |
| Shade's card | 1.026 (1.005 to 1.043) | 1.372 (1.064 to 2.116) |
| Radiance's card | 0.977 (0.961 to 1.015) | 1.185 (0.881 to 1.495) |
| Quicksilver's card | 0.999 (0.973 to 1.043) | 1.462 (0.985 to 1.795) |
| D-074 Pulse Wall against its hold, Tempest | 1.005 (0.992 to 1.022) | 1.067 (0.959 to 1.265) |
| D-079 Blade Halo against its hold, Arc Grid | 1.004 (0.984 to 1.018) | 1.351 (1.133 to 1.702) |

- Before the boss the interval is about 0.03 either side: a floor of 0.95 can be decided there.
- On the boss it is 0.15 to a half either side. After the hand the two arms are different runs for five minutes of different hands, so the builds that meet the boss differ, and the boss's health follows each build's own first seconds (`boss.md` R-BOS-09).
- To bring Forge's interval to 0.05 either side would take over three hundred seeds a pair (arithmetic: the width falls with the square root of the count). For sixteen cards that is not a battery anyone runs.
- So play's boss side can show a gross cost, or that a card clears a bar by a wide margin. It cannot place a card against `REFORGE.POWER_MIN` or `REFORGE.POWER_MAX`.

## Which body, from the boss's own rules
The crowd's side is read on bodies of the field: a Swarm, `C.ENM.SWARM_RADIUS_UU` across the radius. Three rules make the Warden a different target, and each one changes what a card is worth:
- **Size** (R-BOS-03): the hull's radius is `C.BOSS.RADIUS_UU`, about ten times a Swarm's. A spray, a ring of blades or a burst of children lands more of itself on it.
- **Statuses** (R-BOS-11): Freeze is refused and Chill stacks stop at a cap. A card built on a freeze does on a Swarm body what it can never do to the Warden.
- **Movement** (R-BOS-03): the hull is moved by script, every displacement naming it is ignored, and in its first phase it keeps its catalogue standoff from the hero. A pull or a push does nothing to it, and a weapon that works at contact reaches it only as far as the standoff allows.

**So the body is the hull itself, role `Boss`, in its own fight.** A Swarm body made large would still take a freeze and a pull. One small body is read beside it once, in the check, as the measure of what a small body misses; it is not a standing column.

## Draft 21. The boss's side is read on the probe, against the hull

- **Class:** METHOD, with measuring equipment; an amendment to D-082 item 4 and to D-085 item 9 (where the boss's side comes from). No rule of play, no number of the game
- **Lowest branch:** `dev/s2`. **Before session A:** yes; `main` is not touched
- **Order:** after the carry; before the S2 exit battery, so that the battery reads both sides once

### Decision (recommended)
1. **The scene.** For each Reforging card, the builds that reach the hand (the same level 16 snapshots as the crowd's side), each in a probe scene in which the Warden's fight is started and no field stands. Two scenes a snapshot and seed: with the card, without it.
2. **The measure.** Damage dealt to the hull in the window, applied damage (`telemetry.md` R-TEL-11). The window starts after the calibration's `C.BOSS.CALIBRATE_S`, so the health is settled before anything is counted. The card's lift is the damage with it over the damage without it, the median over its builds.
3. **An interval on every figure of the band, on both sides.** The median over builds is resampled by build (ninety in a hundred, a fixed seed), on the field and on the hull alike. A card is under or over a bound when its whole interval is; when the interval holds a bound the reading says "at the lower bound" or "at the upper bound" and the card comes to the scheduler with the interval. D-085's `split` (item 10) and its two sets of builds (ruling 12) stay.
4. **The band, as D-085 item 9 has it:** at least `REFORGE.POWER_MIN` on one side and at most `REFORGE.POWER_MAX` on both. "The boss's side" now means items 1 and 2.
5. **Play keeps what it can decide.** The floor before the boss stays as D-082 item 4 and its ruling have it. The boss floor becomes an alarm: a card fails it only when its whole interval is under 0.95; anything else is printed and decides nothing. No second battery follows by rule; the scheduler orders one where it wants one. Reach, deaths before the boss and damage taken are printed per arm, as now.
6. **What the scene is not, said with every reading.** The probe's hero takes no damage and walks the probe's route. The scene reads what a build can do to the Warden when it does not have to dodge. Whether a build can stand in the fight is play's: reach, deaths, the alarm.

### The equipment's own check
It is kept if all of this holds, and reported before any card is judged on it if one does not:
- **Two bodies, once.** The hull, and one Swarm body that does not die (`field 1`), on the first reading's 45 snapshots and the five Paths'. If the two rank the cards alike and agree within a tenth, the note says the cheaper body would have done. If not, the table is the measure of what a small body misses, card by card.
- **The scene agrees with play where play can speak.** Each card's lift on the hull lies inside play's interval for it: the five Path cards against no pick; for an evolution, its lift over its Path card's lift, against the landed pair's interval. A card outside its interval means the scene is not reading what the fight reads.
- **Same order of magnitude.** With no pick, the scene's damage to the hull a minute is printed beside the same builds' figure in play.
- **The window is clean.** It lies after the calibration. The count of scenes in which a phase changes inside the window is printed (R-BOS-04: a change starts a shift of `C.BOSS.PHASE_SHIFT_S` with no attack); what a shift does to the damage the hull takes is looked up in the scene's records, not assumed.
- Every card has at least ten builds read.

### Cost
| Piece | Kind |
|---|---|
| Starting the fight in a probe scene. The console command `DT.Boss` asks the boss system for the fight directly; a probe scene runs with the director off and is launched by `Tools/BotRuns.ps1`, which passes a console command only for unlocks today. If the command on a probe's launch line gives a clean scene: Tools only | Tools, the text steps |
| If it does not (the fight starts before the build is applied, or a field cannot be left out): a `boss` word in the scene script, as D-082 added `reforge` | engine code (`Bot/DTProbeScript`), a whole gate, scenes of record unchanged without it |
| `Tools/probe.py`: the two scenes a snapshot, the hull's damage from the boss's records, the reader's lines; the interval by build on both sides | Tools |
| `Tools/S2Exit.ps1`: the band step calls it beside the field | Tools |
| Scenes | two more a snapshot; minutes |

Which of the first two rows it is, is found by one try. It is the first thing done when the pause ends.

### Not shown by any of this
- Any figure on one body. No scene with the hull or with one Swarm body has been run.
- That `DT.Boss` gives a usable scene without engine code.
- How wide the interval by build is. On the field the first reading's two sets of builds differed by 0.33 for the Blade Halo; that is the only hint.
- That a scene without dodging ranks cards as the fight does. The second check line is there to find out.

### Swap-out point
The record on what an evolution must give.
