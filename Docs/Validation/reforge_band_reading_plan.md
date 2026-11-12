# How the Reforging band is read (Draft 19)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for on 2026-11-13 after D-081's supply reading. Written on `plan/reforge-band-reading` from `main` at d733b86. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it and no run was made.

A constant or a test id that exists only on `dev/s2` is written without its prefix. **A bot reading is never a human reading.**

## What the band is, and what has read it
- The source (`Handoff_Dusktracer/game/reference/specs/build_depth_spec.md`, R5): "A transformation may change *how* a build kills, not multiply its output. Target: a steered build's DPS after its L16 pick rises ×1.3–1.8 (GATED, bot-measured). Swarm TTK ≤ 0.3 s holds before and after."
- The port: `reforging.md` R-RFG-18 keeps it as `REFORGE.POWER_MIN` 1.3 and `REFORGE.POWER_MAX` 1.8. The number log's row says how it was read: damage a minute after the pick over before it, on the same runs. It read 1.00, "may be a field cap", and the spec has carried the open line since.
- D-081's supply reading: it is a field cap. Between the pick and the boss every arm of every battery kills what the timeline sends at the rate it sends it.
- The carry batteries of D-061 to D-079 compare an evolution with its hold, and the hold takes its Path's card at that hand (`bot.md` R-BOT-18). They read a floor: an arm under about 0.95 is failing to clear what is sent.
- So nothing has read the band: no reading has its denominator (the build without the pick), and none is on a measure a full-clear build cannot saturate. That holds for the six Path cards as much as for the ten evolutions.

## What the hand offers
- At `OFFER.REFORGING_LEVEL` (16) one milestone hand holds the Reforging cards: the Patron's transformation, the second Keeper's, and an evolution for each weapon at `OFFER.REFORGE_EVOLVE_MIN_LEVEL` (6) or more. The player takes one.
- A player cannot take nothing: declining a hand exists only for a bot's control arm (`offers.md`).
- So "over nothing" is the band's measure and never a player's choice. The choice a player has is between cards, and that is the design question behind the band: is each card a real lift, and is one of them so far ahead that the hand is decided before it opens.

## Three instruments
1. **A no-pick arm.** The carry arm gains a second control beside its hold: the same run, and the Reforging hand is declined. It models no player; it is the band's denominator. The offer system's `Decline` exists; the bot needs a switch that uses it at that one hand, off by default.
2. **The boss fight, by D-081.** Damage dealt to the boss a minute is not supply-bound: the boss has health to spare. With the no-pick arm it reads a pick's lift in the fight directly, the calibrated health beside it.
3. **The probe's fixed field.** `Tools/probe.py` already rebuilds each run's level 16 build, the build that reaches this hand, and reads its damage in a window on a standing, refilled field whose health is raised until a level 16 build no longer reaches the refill's ceiling (T-KEE-18 is read this way). The same field read three ways (no pick, the Path's card, the evolution) is a build's power against a crowd, with and without the pick, and nothing else different.

## Draft 19. The band is read on the probe's field; play gives the floors

- **Class:** METHOD, with measuring equipment: a switch of the bot, one field of the probe's scene script, a stage of `Tools/probe.py`. One sentence of `reforging.md` R-RFG-18 changes, the one that says what the band is measured on. No rule of play and no number
- **Lowest branch:** `dev/s2`. **Before session A:** yes; `main` is not touched
- **Order:** before the S2 exit battery, so that the battery reads the band once

### Decision (recommended)
1. **The band is judged against a crowd, on the probe's field.** For each Reforging card: the builds that reach the hand (each run's level 16 snapshot, from the carry battery of the card's weapon or Path), each read with the card and without it on the same field and seed. The card's lift is the damage in the window with it over the damage without it, as the median over its builds. The card is in band when that is from `REFORGE.POWER_MIN` to `REFORGE.POWER_MAX`.
2. **Why there, and not in play.** It is the source's own sentence, a build's damage after its pick over before it. It changes one thing and holds the build, the field and the seed. It cannot be saturated if the field is kept under its ceiling, and the job proves that for every scene before a figure is used.
3. **One band, not two.** The boss gets a floor and not the band, for two reasons. After the hand the three arms are different runs: over five minutes of different hands lie between the pick and the boss, so a ratio there carries their noise. And some cards are crowd tools by design (a wall, a pull); a band on boss damage would ask every card to be a boss weapon.
4. **The floors, in play.** `Tools/BotRuns.ps1 -Carry` with three arms on the same seeds: the card, the Path's card (today's hold), no pick. Judged against the no-pick arm, each by itself: damage a minute before the boss at least 0.95 (the window; it says the build still clears what is sent), and damage dealt to the boss a minute at least 0.95 as the median over the runs that reached it, ten reaching it (D-081 item 1). Under either, the card costs the run more than taking nothing, and it does not land.
5. **Always printed beside a card's figures: its Path's card on the same three measures.** That is the hand's real choice. If the Path's card reads 1.5 and the evolution 1.2, the hand is decided for the player; the record on what an evolution must give says what gap is too wide. This draft sets no number for it.
6. **The source's second clause** (the swarm's time to kill stays under `TTK.SWARM_STEERED_MAX` before and after) is printed from the play arms; the run files hold it.
7. **What the field cannot show, said with every reading:** it reads power when the weapon is used. The probe's hero takes no damage and walks its route inside the crowd, so a contact weapon reads at its best there. Whether a build survives using it is the play arms': deaths before the boss and damage taken, printed per arm.
8. **R-RFG-18's sentence becomes:** "Its power is a reading on the build that reaches the hand: on the probe's fixed field the build's damage with the card is `REFORGE.POWER_MIN` to `REFORGE.POWER_MAX` times its damage without it; in play the card costs neither the run before the boss nor the boss fight against a control that declines the hand."

### The equipment's own check
It is kept if all of this holds, and reported if any does not:
- With the bot's switch off, the five seed 9107 arms and one hold battery read as before, run for run.
- With it on, the run file holds no Reforging pick, and the run is the hold's run to the step of that hand.
- A scene with no pick gives the same window damage, to the unit, as T-KEE-18's full configuration of the same snapshot.
- No scene reaches the field's refill ceiling. If one does, the stage's field health is raised for every scene of the stage, and the note says by how much and why.
- The six Path cards and the ten evolutions each have at least ten builds read.

### Cost, and what must exist before the exit battery
| Piece | Kind | Before the exit battery |
|---|---|---|
| The switch that declines the Reforging hand; `Tools/BotRuns.ps1` passes it; a rule and a test in `bot.md` | engine code (`Bot`), a whole gate, every hash unmoved with it off | yes |
| A Reforging pick in a probe scene's build (the scene script has none today) | engine code (`Bot/DTProbeScript`), the same gate; scenes of record unchanged without it | yes |
| `Tools/probe.py`: a stage that writes the three scenes per snapshot, and the reader's lines (lift, in band or not, the Path's card beside the evolution) | tools | yes |
| The carry batteries with three arms for the ten weapons | runs: thirty batteries of fifteen, about two hours alone at tonight's pace; the scenes are minutes | they are the exit battery's new step |
| `strike_hits` and `strike_damage` (D-081 item 6) | engine code | can follow; they say whether an evolution's own strikes mattered, not whether it is in band |

- **Not in D-073's branch.** D-073 is a rule with a hypothesis of its own; its gate should prove that rule and nothing else, and equipment the exit battery needs should not wait on a rule that can be reverted. One branch for the equipment, its own gate. For the same reason I would move the two strike fields out of D-073's branch and into this one; D-081 item 6 put them with D-073, so that is the scheduler's to change.
- The two branches touch different files (`Bot` and `Tools/probe.py` here; the Reforging pick in `Combat` and the offers there), so two developers can build them side by side.

### Order
D-079's hand-over; the reader's block of D-081; D-073; this equipment beside it; the carry to the later branches; then the exit battery with the band's step in it.

### Not shown by any of this
- What any of the sixteen cards reads. I expect nothing and predict nothing here.
- How a person values a card. The band is a bot's bound, as the source says of it.
- Whether 1.3 to 1.8 is the right band for this game; it is the source's, taken as it is.

### Swap-out point
The record on what an evolution must give, which this makes writable: for each hand, whether each card is in band over nothing, and whether one card decides the hand.
