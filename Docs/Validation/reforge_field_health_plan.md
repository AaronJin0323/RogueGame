# The field the Reforging band is read on: how its health is fixed (Draft 20, an amendment to D-082)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for on 2026-11-13 after the first reading of D-082's equipment. Written on `plan/reforge-field-health` from `main` at f2514a2. Nothing in it is in force until the scheduler accepts and numbers it.

Build: the readings are the equipment's own check (D-082 item 10), on `s2/reforge-band-equipment` at 34c6ae5, before its merge and its gate: the level 16 snapshots of three hold batteries (`-Carry` Orbital Drones, Seeker Swarm and Ring Emitter on `Steered:Storm:Tempest:Tide`, 15 runs each), 180 scenes a stage, five stages. **No card is judged by them.** A bot reading is never a human reading.

A constant or a test id that exists only on `dev/s2` is written without its prefix. Every figure marked "arithmetic" is mine and not a reading.

## What the field is
- 120 Swarm stand round the hero. Each has `ENM.SWARM_HP` times the stage's health scale; "1x" is the health play gives a Swarm at the minute of the hand, and the probe's default is 40x.
- A body that dies is replaced in the same step by a new one on a ring 1 600 uu out, which walks in at 280 uu/s, about 5.6 s.
- So the field has no hard ceiling. A window can kill the 120 that stand and whatever walks in in time; the highest scene of the five stages killed 669. The figure of 160 that D-082's check used is the lower edge of an older reading and is kept only as a flag.
- The window's damage is applied damage (`telemetry.md` R-TEL-11): the health a hit removed, clamped to what the body had left. It holds no overkill.

## The five stages
Lift is the window's damage with the card over the damage without it, same snapshot and seed, the median over builds.

| Health | No pick: kills in the window, median (highest) | Storm's card | Tide's card | Blade Halo | Pulse Wall | Murmuration |
|---|---|---|---|---|---|---|
| 10x | 98 (294) | 1.42 | 1.12 | 1.27 | 1.91 | 1.98 |
| 20x | 27 (229) | 1.41 | 1.17 | 1.36 | 2.03 | 2.38 |
| 40x | 1 (159) | 1.36 | 1.27 | 1.46 | 2.83 | 2.79 |
| 80x | 0 (55) | 1.33 | 1.30 | 1.40 | 3.30 | 4.23 |
| 160x | 0 (0) | 1.32 | 1.33 | 1.40 | 3.28 | 5.05 |
| Builds | 45 | 45 | 45 | 15 | 15 | 15 |

- In play the same builds kill 259 to 262 bodies in the minute that holds the hand, 43 in a window's ten seconds (the holds' own runs, median of 15 each).
- The lift read on kills, where the no-pick build still kills enough to divide by: at 10x Storm 1.63, Tide 1.12, the halo 1.44, the wall 1.76, Murmuration 1.81; at 20x 1.51, 1.25, 1.86, 4.32, 3.40. At 40x the no-pick build's median is one kill and the ratio stops meaning anything (26 for the wall).

## What the stages say
- **For a card that strikes many bodies the lift rises with the field's health**, and it does not settle while anything still dies: the wall 1.91 to 3.3, Murmuration 1.98 to 5.05. The two Path cards and the halo move by a tenth or two.
- **From 80x on nothing dies** for a build without a pick, and the damage is the build's output on the same 120 bodies. That end of the table is a card's throughput on bodies that never die: the best case for a ring that ticks a disc and for missiles that always find a target.
- **At the low end the field empties.** At 10x nine of the 45 no-pick scenes already pass 160 kills and most of the cards' scenes do; the bodies in reach are the ones still walking in, and every strong card is pressed toward the same figure.
- So "raise the health until no scene reaches the ceiling" (D-082 item 2) has no stopping point that is not a choice, and the choice moves the two crowd cards by a factor of two and a half.

## Draft 20. The field is as busy as play

- **Class:** METHOD, an amendment to D-082 items 1 and 2 and to its check (item 10). No rule of play, no number of the game
- **Lowest branch:** `dev/s2` (`Tools/probe.py`, the note of the equipment). **Before session A:** yes; `main` is not touched

### Decision (recommended)
1. **The health is fixed by the rate play kills at.** For a reading, the play rate is the median, over the runs whose snapshots are read, of the kills in the minute that holds the Reforging hand (their own run files), scaled to the probe's window. The field's health is the one at which the no-pick scenes of those snapshots kill that many, at the median. It is found by reading the no-pick scenes on the series 5x, 10x, 20x, 40x and interpolating between the two stages that bracket the play rate, on the logarithm of both; the result is rounded to a whole multiple and that stage is then read whole.
2. **Once for a reading, not for each snapshot.** Every card of one reading stands on the same field, so their lifts can be laid side by side; a health per snapshot would put each build on its own field. A reading on another pair (the other Paths) finds its own health the same way and says it.
3. **Why this point.** Lower, and the field is empty and presses every strong card to one figure. Higher, and the bodies never die, which is a fight the game does not have. At this point the build without a pick is exactly as busy as it is in play: it kills what play sends it. The bodies are tougher than play's (about fifteen times, by these stages), because play's stream is too thin to occupy a level 16 build at all; that is the cost, and the note of every reading says it.
4. **The no-pick build must leave room above it.** If the play rate is over half the standing field (60 of 120), the health is raised until it is not, and the reading says so. Here it is 43.
5. **What is counted: applied damage.** It holds no overkill, it does not blow up where the build without a pick kills little, and it counts a card that wounds many without killing. The lift by kills is printed beside it wherever the no-pick median is twenty kills or more, as a second look and not a verdict.
6. **"At least".** A scene that kills more than the 120 that stand has drawn on the ring; its figure is printed as "at least". The flag of 160 leaves the check.
7. **The far end is printed, never judged:** one stage at a health where the no-pick build kills nothing (160x here), named "bodies that do not die". It says what a card is worth where the game is harder than its stream today, which later stages of the game (more bodies, tougher bodies) will want to know.
8. **D-082's check, item 10, fourth clause** becomes: the reading names its play rate, the stages that bracket it, the health it fixed, and how many scenes read "at least".

### What that field would read, by arithmetic from the two stages that bracket it
The play rate of 43 lies between 10x (98 kills) and 20x (27): the fixed health comes to about 16x. Interpolated the same way, not read: Storm's card about 1.41, Tide's 1.15, the halo 1.33, the wall 1.99, Murmuration 2.23. The stage at 16x is read by the equipment's developer as soon as this is accepted, and those five figures are replaced by the reading; none of them judges a card until then.

### The two sides of a card (a view; the record on what an evolution must give decides)
- A card is read on two sides: against a crowd, on this field; against one body, by the damage dealt to the boss a minute against the arm that declines the hand (D-081 item 1, D-082 item 4).
- My view: **in band means at least `REFORGE.POWER_MIN` on one side and at most `REFORGE.POWER_MAX` on both.** A card for one body is not failed by a crowd's measure, since the boss's side can carry it; a card for crowds is not passed by a field alone if it multiplies a build there, since the upper bound holds on each side by itself.
- The play floors of D-082 item 4 stay as they are, on both sides, for every card.
- What the first reading already suggests, and no more than suggests: the two Path cards sit at the lower edge on a crowd; the halo is at it; the wall and Murmuration are over the upper bound on a crowd at any health read, while by the boss's measure they are level with the build that took the Path's card. If the fixed stage confirms it, they are cards that double a build against a crowd that does not melt and add nothing against one body. In play today that shows as nothing, because play is supply-bound; it would show in any harder mode.

### Cost
- `Tools/probe.py`: the calibration (the no-pick scenes on the series, the interpolation, the fixed stage), the "at least" mark, the far stage, the reader's lines; tools tests. Tools only: the text steps and the tools tests.
- Scenes: the series adds the no-pick scenes of four stages (45 scenes each here) and the fixed and far stages are 180 each; minutes.
- `S2Exit.ps1`'s band step calls the stage with the calibration. No engine code.
- It can ride in the equipment's branch if that has not been gated when this is accepted, or follow it as a Tools branch of its own.

### Not shown by any of this
- The stage at the fixed health: its five figures above are arithmetic.
- Thirteen of the sixteen cards. Three evolutions and two transformations are read.
- The boss's side against no pick, for any card: no arm that declines the hand has been read in play yet.
- Whether a field of tougher bodies stands for a harder game well enough to judge a card by. It is the best field this probe has; people will say what the cards are worth.

### Swap-out point
The record on what an evolution must give.
