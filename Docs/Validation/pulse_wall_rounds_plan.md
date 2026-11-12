# Pulse Wall: the source's wall first, its rounds as the fallback (Draft 11)

For the scheduler, from the Mechanics module session (D-055 item 4). Written on `plan/pulse-wall-rounds` from `main` at a5134b7. It replaces Draft 10 in this file, which the scheduler returned on 2026-11-12 with what the source's code says. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it.

Build: none read here. Every number of play comes from run files that exist: the D-061 batches (`d061_hold`, `d061_arm_2`, `d061_arm_8`; `Validation/S2_d061_pulse_wall.md` on `dev/s2` names their builds) and the D-067 arm (`d067_arm`, the build of `s2/d067-pulse-wall` at 1af3eac; `Validation/S2_d067_pulse_wall.md` on `s2/d067-note`). Each is the carry pair `Steered:Storm:Tempest:Tide`, `-Carry Weapon.RingEmitter`, 15 seeds, bot v2 at skill 1.0. **A bot reading is never a human reading.** Reach and wins are printed and judged nowhere (D-050 item 5).

A constant or a test id that exists only on `dev/s2` is written without its prefix, so the lint on `main` does not look for it. Every figure marked "arithmetic" is mine and not a reading; my arithmetic in Draft 9 was wrong once already.

## What the run files say: the loss is the boss fight
The two notes read the time after the Reforging pick as one window. Split at the boss's start, it is two different stories.

How it was read (two scratch scripts, no new run): per run, "before the boss" runs from the `reforge` record to the `boss_start` record, or to the run's end when the run never met the boss; "the boss fight" from `boss_start` to the run's end. Damage is summed from the `hit` records' damage by source inside each window and divided by the window's minutes. Kills per minute come from the whole 60 s `sources` intervals inside the first window. Damage taken comes from the `hero_damaged` records. Medians over the runs: all 15 before the boss, and in the fight the runs that reached it.

| After the pick | Hold | As ported, `dps` 2 | As ported, `dps` 8 | D-067's carry |
|---|---|---|---|---|
| Died before the boss, of 15 | 0 | 8 | 7 | 6 |
| Before the boss: all sources, damage per minute | 16 942 | 13 604 | 15 632 | 15 162 |
| ... against the hold | 1.00 | 0.80 | 0.92 | 0.89 |
| Before the boss: kills per minute | 365 | 319 | 327 | 328 |
| Before the boss: damage taken per minute | 1.8 | 22.3 | 21.6 | 15.9 |
| Reached the boss, of 15 | 15 | 7 | 8 | 9 |
| Boss fight: length, s | 84 | 124 | 134 | 136 |
| Boss fight: all sources, damage per minute | 16 650 | 5 143 | 4 457 | 4 433 |
| ... against the hold | 1.00 | 0.31 | 0.27 | 0.27 |
| Boss fight: damage taken per minute | 11.1 | 23.3 | 30.8 | 18.0 |
| Boss HP left at the run's end | 0.00 | 0.25 | 0.14 | 0.07 |

The boss fight by source, damage per minute:

| Source | Hold | As ported, `dps` 2 | As ported, `dps` 8 | D-067's carry |
|---|---|---|---|---|
| Weapon.RingEmitter (Pulse Wall in the arms) | 5 793 | 170 | 717 | 203 |
| Keeper.Storm | 5 900 | 2 998 | 1 347 | 1 853 |
| Weapon.PulseRifle | 1 001 | 1 179 | 1 221 | 1 047 |
| Keeper.Tide | 620 | 746 | 440 | 544 |
| Weapon.OrbitalDrones | 178 | 331 | 287 | 234 |

- Before the boss the build stands at 0.80 to 0.92 of its hold. In the boss fight it stands at 0.27 to 0.31. `evolve_read.py`'s one figure for the whole time after the pick (0.74 to 0.78) is mostly the fight.
- In the fight the evolved weapon deals 170 to 717 a minute where the plain Ring Emitter deals 5 793. The boss stands inside the ring, where the port's wall strikes nothing.
- Keeper.Storm falls with it, from 5 900 to between 1 347 and 2 998: the weapon's hits carried it.
- Before the boss the hero takes 16 to 22 damage a minute against 1.8, and six to eight of fifteen runs die there: the crowd walks in.

## What the source's code does
The scheduler read the previous project's source (`Source/RogueHero`, outside this repository) and I read the same lines. They are quoted here because the Handoff's documents never state the wall's damage (D-020, Context 2), and the code does.

- `RHWeaponInstance.cpp` 269 to 275: "Pulse Wall (build_depth_spec §1.4): each volley is one wall ring around the hero instead of a ring of orbs." The volley spawns one fusion bullet and returns. The source's evolved weapon fires no rounds.
- `RHSpecComposition.cpp` 656 to 672: "Pulse Wall: a zone that grows to WallRadius, holds WallTime, pushes out". The bullet is a Zone, size 80 growing to `WallRadius` times the hero's Area in 0.6 s, lifetime 0.6 s plus `WallTime`, `LingerTickInterval` 0.25, `Knockback = FP.WallPush`, `bRadialKnock`, pierce 255. Its damage, line 693: `S.Damage * FP.DerivedDamageMult`.
- `RHBulletSubsystem.cpp` 901 to 909 and 957 to 962: "Zones hit everything inside once per tick (first tick on the spawn step); no history needed." The radius of the hit is the bullet's grown size. So the source's wall strikes, and knocks outward, everything inside its whole radius every quarter second. It is a disc.
- `RHWeaponDef.cpp` 658 to 668: "Pulse Wall: each volley is one ring that grows to 600 uu and holds for 3 s, pushing enemies out." Capabilities Radial and Area. Cooldown 3.4 s. `WallRadius` 600, `WallTime` 3, `WallPush` 220, `DerivedDamageMult` 1.5.
- `RHEnemySubsystem.cpp` 60, 683 and 878: a knockback is a speed added to the enemy on the hit, times its inverse mass ("a Bulwark takes a third of a Scrapwing's shove"; bosses take a quarter or less), and it decays at 10 a second. Arithmetic: a kick of 220 carries a light enemy about 18 uu, so four ticks a second move it about 73 uu a second.
- `build_depth_spec.md` §1.4: an evolution keeps its modifiers "when the capability tags allow (§3; incompatible modifiers are refunded as a Reroll)".

### The port's cells beside the source's
| Cell (`reforging.csv`, `Evolve.RingEmitter`) | Port | Source | Where the port's came from |
|---|---|---|---|
| `radius_uu` | 600 | 600, times the hero's Area | the Handoff's row |
| `grow_s` | 0.6, from nothing | 0.6, from size 80 | the Handoff's design log |
| `hold_s` | 3 | 3 | the Handoff's row |
| `tick_s` | 0.25 | 0.25 | the Handoff's zones (D-020, Context 3) |
| `band_uu` | 100: only the outer 100 uu is struck and pushed | none: the whole disc | the port's own, "push band dps first guess" |
| `dps` | 2: 0.5 of one round's damage a tick | 1.5 of one round's damage a tick, which is `dps` 6 in the port's unit | the port's own: D-020 kept 2 as "a first guess" because "the Handoff never states a damage for Pulse Wall" |
| `push_uu_s` | 200, a steady speed, every step, no mass | a kick of 220 a tick that decays and is divided by mass: about 73 uu a second on a light enemy (arithmetic) | the port's own first guess |
| Cooldown | at least the ring's life, one ring per weapon | 3.4 s against a life of 3.6 s, so rings overlap by 0.2 s | R-RFG-12 |
| Capabilities | Projectile, Radial, Area, Expiry | Radial, Area | the port's row |
| Modifiers the new capabilities refuse | dormant, never refunded (R-RFG-04, `modifiers.md` R-MOD-08) | handed back as a Reroll | `reforging.md`, out of scope |

What D-020 held and why: it settled the unit only (`dps` is a ratio of one round's damage, per enemy, per second), from the Handoff's other zones. It left the value at 2 and wrote "whether a ring should deal damage at all, beyond the push the source names, stays an open design point". The source's code closes both: the ring deals damage, to everything inside, at 1.5 of a round a tick.

So "strikes its band" and "what stands inside it is untouched" are the port's own sentences, and they are the two faults the run files show: the crowd walks in, and the boss stands where nothing strikes.

## Draft 11. The wall is the source's disc

- **Class:** RULE (`dev/s2` `reforging.md` R-RFG-12), with three cells of the row set to the source's values in the same step, each cited to the source and none tuned. The scheduler classifies; if the source's code counts as the reference the port missed, it is a port defect
- **Lowest branch:** `dev/s2`. **Before session A:** yes; it is S2 content and no validator of session A meets it
- **Order:** it follows D-067, which was reverted by its own clause. No other RULE record on `reforging.md` waits for a reading

### Options
- **S — the wall as the source has it.** Each `tick_s`, every enemy inside the ring's current radius is struck and pushed radially outward; the boss is struck and not moved. Cells from the source: no band, `dps` 6. The capabilities become Radial and Area, so a modifier that needs a round goes dormant by the port's own rule, which shows it to the player and names it on the card before the pick.
- **C — the volley keeps its rounds** (Draft 10's recommendation). The rounds fire as the plain weapon's, and the port's band wall opens when none is live. It is `[invented]`: the source fires no rounds. No number moves.
- **B — a true wall for a hero who stands in it**: a band no walker crosses, with D-067's carry. It leaves the boss fight where it is.
- **D — hold the card for people**, or out of the offers (the scheduler decides that when the S2 kit is built).
- Draft 10's option A (the whole disc hurts, at the port's `dps`, without a push) is S with two of the source's cells missing; it is not kept as an option of its own.

### S beside C
| | S, the source's disc | C, the rounds back |
|---|---|---|
| Boss fight | The boss is struck four times a second for 1.5 of a round: 66 a second at level 6 while it stands inside, against the 97 a second the plain weapon's rounds dealt it here (arithmetic from 5 793 a minute). Keeper.Storm has four hits a second to ride, against nine. My arithmetic puts the build at 0.45 to 0.6 of its hold | The rounds are the hold's, so the fight should be the hold's: about 1.0 |
| The crowd's place | Answered: whatever enters the disc is struck each tick and pushed out | Left to the rounds, which kept the hold's nearest enemy at 477 uu; the band stays as leaky as it is |
| Output against the band 1.3 to 1.8 | Before the boss the hold already kills what spawns (365 kills a minute against 319 to 328 in the arms), so damage there is capped by what walks in: about 1.0, not a multiple. An overshoot is unlikely; if it happens, the TUNE on `dps` downward is its own step | About 1.0 before the boss, plus the little the band adds |
| Perf | New load on every tick: a strike for every enemy inside, and a push for every enemy inside. D-067's one carry cost about 0.2 ms of game thread at p95 in the S2 scene; this is more | No new displacement. The scene's bullet load rises by the weapon's rounds, inside the cap it already proves |
| What a bot can read | The next ring opens at the hero every 3.6 s and reaches 600 uu, so a kiting bot stands inside one of its discs most of the time: it can read S. How often it stands outside was not read (the D-067 note has only the share of ring lives that end more than 500 uu away: about half) | Everything: the rounds follow the hero |
| What it is | The card's sentence and the source's weapon: a transformation with a cost at the boss | The plain weapon with a wall added: safe, and not the source's |
| Modifiers | The seven that need a round go dormant and are shown so. No refund: the port has none, and adding one is its own decision | The seven that need a round act again |

### Does "one `dps` cannot fit a crowd and a boss" still hold?
No. I argued in Draft 10 that a disc that hurts would deal a crowd of twenty about 22 000 a minute and a lone boss about 1 100, so that no single number could serve both. That multiplied a rate by a crowd that would not be there. A crowd in the disc is pushed out or dies, and before the boss the damage a build can deal is capped by the health that walks in: the hold already takes nearly all of it. So the number has to fit the boss alone, and the crowd's side is capped by supply. That also means a later TUNE on `dps` for the boss fight would not overshoot on the crowd, which the reading below can confirm or refute.

### Decision (recommended)
S first, then C if S misses its hypothesis or costs too much game thread. This is the scheduler's leaning, and I agree with it for three reasons: it is what the card says and what the source does, it goes at both faults with one rule, and the argument I had against it was about a number and was wrong.

- R-RFG-12 becomes: a volley is one ring at the hero, growing to `radius_uu` over `grow_s` and holding for `hold_s`. Each `tick_s`, every enemy inside the ring's current radius is struck for `dps` × the composed `Damage` × `tick_s`, one round's damage and not the weapon's whole output (D-020), as impacts so flags still react, and is pushed radially outward through displacements; an enemy that ignores displacement is struck and stays. One ring per weapon lives at a time; the cooldown is at least the ring's life.
- The free-volley clause of D-039 and D-040 keeps its sense: the live ring strikes once more what stands inside it, as that volley's source and at its scale.
- Cells: `band_uu` goes (the whole disc); `dps` 2 becomes 6, the source's 1.5 of a round a tick. `push_uu_s` stays 200 for the reading, applied once a tick as the source applies its kick, so one displacement per enemy per tick and not one per step: that is the source's rhythm and a fifteenth of the displacement load. On the whole disc 200 is stronger than the source's push (about 73 a second on a light enemy, arithmetic), and a Tank at 165 a second no longer reaches a hero who stands in the disc. Whether that is too safe is the first S2 session's question; if the scheduler wants the source's strength at once, the cell is 73 and I would take it.
- Capabilities: the row's become Radial and Area. Of the fourteen modifiers that fit the Ring Emitter (`matrix.csv`), by the catalog's words seven need a round (Pierce, Ricochet, Homing, Orbit Lock, Boomerang, Scatter Shell, Spread Wave): they go dormant and the HUD and the card say so. Three act on a kill and still act (Chain Trigger, Spillover, Refit). Four react to an impact (Split Core, Lingering Field, Backwash, Conduit), and under S there are far more impacts for them: what each does on a strike that is not a round is the job's first step, a table of the fourteen from code and the harness.
- The drawing: "two beaded arcs, the edges of the band, not a filled disc" was true of a ring whose inside is untouched. Under S the inside hurts. The job draws the one outer arc with the code it has; what a disc that strikes should look like is a question for the Visual session, named here and not decided.
- What is not taken from the source: the radius does not scale with an Area stat, rings do not overlap (one per weapon stays), and nothing is refunded.

### The perf bound comes first, and what stops the job
- Before any reading of play, the job reads the S2 perf scene twice on `dev/s2`'s build and twice on the branch's, as D-067 did. That scene evolves the Ring Emitter and holds 400 enemies, so it is the worst case for this rule.
- The job stops, with nothing more built, if the game thread at p95 rises by more than 0.3 ms over the before pair, or if either after run fails the scene's own budget. It reports the four figures and the scheduler takes the reading alone. 0.3 ms is half again D-067's cost; it is my line, for the scheduler to move.
- If it stops there, C is the next draft to build, and its perf note is the bullet load.

### Hypothesis
We believe that making the wall the source's disc moves the build's damage per minute against its hold in two windows, each judged by itself: before the boss, from 0.80 to at least 0.95, because what walks into the disc is struck and pushed out where today it walks through a band and stands on the hero; and in the boss fight, from about 0.3 to at least 0.5, because the boss is struck four times a second for 1.5 of a round where today the wall deals it 170 a minute, and Keeper.Storm has hits to ride again; measured by `Tools/BotRuns.ps1 -Carry` on the Ring Emitter pair, 15 seeds, bot v2 at skill 1.0, read by the split of the run files at the boss's start that this note uses, which the job makes a part of `Tools/evolve_read.py` with a tools test so that the reading is a tool's and not a scratch script's; the one figure for the whole time after the pick, the nearest enemy and the damage taken are printed beside them and decide nothing; reach and wins printed and judged nowhere; reverted, the rule and its three cells put back, if the build reads under 0.95 of its hold before the boss or under 0.5 of it in the boss fight.

- Why 0.5 and not 0.9 in the fight: 0.9 is parity with a weapon that puts twenty-four rounds into a boss at arm's length, and a zone gives part of that up by what it is. 0.5 is what the source's own cells should give by my arithmetic. If the scheduler holds that the pick must not cost the fight at all, the bar is 0.9, S will probably miss it, and C should be built first.
- What follows a pass that is short of parity: one TUNE pass on `dps` upward, judged on the boss-fight window alone, as a step of its own by `10_Tuning.md §5`. The supply cap above says it cannot overshoot before the boss; the same reading shows whether that is so.
- What follows an overshoot: if the build lands above `REFORGE.POWER_MAX` (1.8) before the boss, one TUNE pass on `dps` downward, as its own step.
- What follows a miss: C, with Draft 10's hypothesis (at least 0.95 before the boss and at least 0.9 in the fight) and its test `Dusktracer.Reforging.PulseWallRounds`.

### The three points of the board's block of 03:27
- **Which fault it aims at.** Both: the crowd's place and the boss fight, with one rule. C aims at the output alone.
- **The perf bound, up front.** Above, with the line that stops the job.
- **Whether D-067's carry is worth keeping by itself.** No. Before the boss it cut damage taken from 22.3 to 15.9 a minute, and wins rose from 1 to 4 of 15 (printed), for about 0.2 ms. Six of fifteen runs still died before the boss, and the fight did not move. Under S every tick pushes what stands inside, which does the carry's work. `s2/d067-pulse-wall` stays, as the scheduler ruled; its tests are the model for this job's.

### Impact (dependency sweep)
- Rules replaced: R-RFG-12 (the disc, the push inside, the drawing sentence) and its test line (every enemy inside is struck each tick and pushed; an enemy that ignores displacement is struck and stays; nothing outside is touched; one ring live; the free volley's one more strike reaches the inside).
- Rules read again: the Pulse Wall clauses of `modifiers.md` R-MOD-16 and `keepers.md` R-KEE-34 (D-039, D-040): "strikes its band" becomes "strikes what stands inside it". D-020: its unit stands; its open point ("whether a ring should deal damage at all") is closed by the source's code.
- Catalog: `reforging.csv`, `Evolve.RingEmitter`: `band_uu` removed, `dps` 6, the caps Radial and Area; a row in the number log for each cell, cited to the source. Constants: none.
- Discoveries, walked by counter on `dev/s3`'s `discoveries.csv`, as D-069 asked of every draft. Fifteen rows, eleven counters:
  - Not reached by this rule: `Best.Level`, `Seconds`, `Best.WeaponLevel`, `Elites`, `Dashes`, `Best.PathsAtTier2`, `Best.Tier`, `BossKills`, and the three per-weapon kill counters, none of which is the Ring Emitter's.
  - `Best.EliteKillMods` (`Elite3Mods`): the job checks in code whether a dormant socket still counts there, and says so; a dormant modifier keeps its socket (R-MOD-08).
  - `Status.Freeze` (`Freeze100`) and `Kills` (`Kills1000`) count faster when a disc strikes everything inside four times a second. Both rows already open on a first run, and neither opens anything an evolved weapon's run does not hold.
  - `Best.Crit` (`Crit500`): a tick is 1.5 of a round of 11 to 15 damage, far under 500 even on a crit.
- Files / tests: `02_Systems/reforging.md`; `Combat/DTWeaponSim.cpp` (`StepForceField`, `StrikeForceField`, the arcs); the Reforging tests and the two free-volley tests, each moved expectation named in the job's note; the table of the fourteen modifiers; `Tools/evolve_read.py` and its tools test.
- Text: the card's sentence stays true. The dormant-socket text on the card and the HUD exists (R-MOD-08).
- Readings that move: runs that evolve the Ring Emitter, and the S2 perf scene. No run of the S2 exit battery evolves it.
- Presentation: the wall's look under S is a question for the Visual session.
- Not shown by any of this: the bot at skill 1.0 kites and does not know the wall is there; whether a person finds the disc too safe to stand in is the first S2 session's.

### Swap-out point
The first S2 session with people.

## Also noted, not drafted
- The same split at the boss's start would say, for the other evolutions that read under their hold on this instrument (Murmuration 0.83, Blade Halo 0.93, Vortex 0.94 in `Validation/S2_evolutions.md`), whether their loss is the fight too. Once `evolve_read.py` holds the split it is one reading of files that exist.
- The other evolutions' cells were ported from the same documents. Whether their own "first guess" cells differ from the source's code in the way this row's did is one read-only pass over ten weapon definitions; I can take it when asked.
- The harness reading the scheduler allowed (a walker's distance over one ring cycle, by role speed) was not taken: it measures the band's leak, and under S there is no band.
