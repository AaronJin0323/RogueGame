# S2: D-075, a measuring arm that holds the bot's crowd where the carried weapon strikes

Branch `s2/d075-bot-reach-arm`, cut from `dev/s2` at `281959d`. Class SYSTEM (the bot, a measuring arm). **The arm missed its own test on both builds and is taken out.** With it on, the source's Blade Halo landed 69 and then 71 hits a minute; the bar is 180. As the brief says, nothing of the arm is handed over but this note:
- no `bot.md` rule (R-BOT-19 does not land);
- no test, and no change to the test floor.

The arm's commits stay on `s2/d075-bot-reach-arm` (`d358fd1`, `7b089de`, and the second build `45da24e`), not handed over. No gate was run, and a note owes none.

Builds:
- **Base** (`dev/s2` `281959d`): the binaries already in the tree. They were built at `3059d82` (the D-072 merge), whose Source equals `281959d`'s: `git diff 3059d82 281959d -- Source Content Config` is empty. The hold battery's base read.
- **Build 1** (`7b089de`): the crowd term and the kite lean. Read with the bot tests, the hold battery and the five seed 9107 arms.
- **Build 2** (`45da24e`): build 1 plus the body term, and a held body counts its own radius. Read with the bot tests, the hold battery and the five seed 9107 arms.
- **The reading branches:** a throwaway local merge of `s2/d071-blade-halo` (the source's halo cells, D-071) onto each build, built for step 4 only. Both were never gated, and both were deleted.

## The keep clause, said as two
- **The record's (D-075):** the weapon's hits a minute rise from 61 to at least 180, and the nearest enemy's median after the pick falls inside the blades' reach. That reach is the centre distance at which a blade touches a Swarm: orbit 150 plus contact 30 plus the Swarm's radius 32, 212 uu.
- **The brief's, stricter:** the median falls between the band's edges, 120 to 180 uu on the source's cells.

By the brief's clause build 1 missed on both counts (187 uu). By the record's clause it was inside the reach and missed on hits alone. Build 2 is inside on both clauses (176 uu) and still misses on hits.

## Step 1: where the standoff comes from (`Source/Dusktracer/Bot/DTBotBrain.cpp` on `281959d`)
**My first read was wrong.** It named the crowd term and the kite lean and said the body term reaches only about 50 uu past touching. That left out the body's own velocity.

A second read, by the other developer (written to the session's scratchpad as `d075_standoff.md`), named the body term as the one that sets the ring. I checked it against the code, and it holds. Bot v2 at skill 1.0 uses the profile's defaults: `MarginUU` 18, `LeaveUU` 0, `HorizonS` 0.6 s and `ImminentS` 0.22 s (`DTBotPerception.h`).

The three terms that keep a body away:
- **The body term of the path's threat, which sets the ring.**
  - Code: `ThreatAlong`'s enemy loop (lines 321 to 333), called from `ScoreMove` with `Profile.HorizonS`.
  - How it scores: each body's closest approach on the candidate path over 0.6 s (`ClosestApproach`, lines 113 to 118) scores `PathThreat(BodyWeight, ...)`. `BodyWeight` is 400 (line 15); `PathThreat` is at lines 126 to 138.
  - What it means: standing still is a threat as soon as a chaser would reach contact plus `MarginUU` within 0.6 s, at about 258 uu for a Swarm, 242 for a Rusher and 215 for a Tank.
  - Its weight: it outweighs the other two terms by two orders. A Swarm walking straight in from 200 uu scores about 350 on the stand-still candidate.
  - This is the ring of 215 to 258 uu. D-071's median of 201 uu sits at its inner edge.
- **The crowd term.**
  - Code: `DangerAt` (lines 368 to 371). Every enemy within `CrowdRadiusUU` 650 (line 21) adds `CrowdWeight` 14 (line 22) times W², where W = (650 − D) / 650.
  - Where it is read: `ScoreMove` reads it at two look-ahead points, 0.3 s and 0.6 s (lines 393 to 395). Each enemy is carried forward by its own velocity to the look-ahead time (line 366 on the base).
  - Effect: a body walking in is read where it will be, not where it stands.
- **The kite lean.**
  - Code: `ChooseMove`. The centroid of every enemy within `KiteRadiusUU` 1300 (line 24, loop at line 481) gives a direction away, and each candidate scores `KiteWeight` 6 (line 25) times how far it points that way (line 539).
  - Effect: with equal danger, walking away always beats standing.

The change needed neither the sim, nor the hero's input rules, nor a walk toward enemies, and no number of its own.

**A finding: R-BOT-09's `WeaponReach` (line 813) ignores `Evolve`.** It reads the plain weapon's level row, `range_uu` (or `radius_uu` for a melee arc), so for a Blade Halo it would give the plain drones' bolt range. The arm read the evolution's own row through `DTReforge::Param` instead.

## The arm as built
- **The switch.** `-DTBotCarryReach`, read only beside `-DTBotCarry`; without a carried weapon it does nothing.
  - `Tools/BotRuns.ps1 -CarryReach` passes it. The run folder's label ends in `-Reach`, and the `BOTRUNS:` line says `carry_reach=1, the D-075 measuring arm`.
  - The bot record's config says `carry_reach=1`, and `Tools/evolve_read.py` prints the labels it read.
  - These tool changes stay on the arm's branch with the rest.
- **The band, by the coordinator's instruction.** The brief asked for one reach. The coordinator instructed a band with an inner and an outer edge, because a halo strikes on a ring, not on a disc: a body inside the ring is as far from the blades as one outside it. Both edges are read from the evolution's own row:
  - Blade Halo: the ring at rest less `contact_r_uu` to the ring at rest plus `contact_r_uu`. That is 120 to 180 uu on the source's cells and 100 to 220 on the port's row. The ring widens with the hero's speed (`reforging.md` R-RFG-07); the edges keep the rest radius and do not follow it.
  - Pulse Wall: from the hero out to `radius_uu`.
  - Any other weapon: from the hero out to R-BOT-09's reach.
- **Build 1.** With the arm on, a body between the edges, and further off than the body term's clearance (contact plus `MarginUU`), adds nothing to the crowd term and nothing to the kite lean. Rounds, telegraphs, hazards, the wall band, the dash and the body term are as before.
- **Build 2**, by the coordinator's instruction, after step 4 of build 1:
  - With the arm on, `ThreatAlong`'s body loop takes each body's closest approach over `min(Horizon, Profile.ImminentS)`, the bot's own dash threshold, and passes that horizon to `PathThreat`.
  - A body counts as held when its disc overlaps the band (`D − r ≤ outer` and `D + r ≥ inner`) and it is still outside the clearance.
  - The dash already reads `ThreatAlong` over `ImminentS`, so it is unchanged.
- **The arm is on from the run's start.** Before the evolution, the band is the plain drones', from the hero out to R-BOT-09's reach, so the bot holds its crowd close with the plain weapon too. Build 2 lost 4 of 15 runs before the Reforging hand.

## Step 3: with the arm off nothing moves, on both builds
- **The hold battery.** The Orbital Drones pair: `Steered:Storm:Tempest:Tide -Carry Weapon.OrbitalDrones -CarryHold`, 15 seeds, v2, skill 1.0.
  - It was read on the base binaries, on build 1 and on build 2.
  - All three reads are 15 of 15 equal in outcome, `end_s`, `steps` and `input_hash`. Seed 1 reads victory, 1063.8 s, 63828 steps, `77081527`.
- **The five seed 9107 arms**, on build 1 and on build 2: None `00e0d4ec`, Random `51369747`, Steered:Forge:Hammer `e3293942`, Steered:Storm:Tempest `b046559b`, Steered:Tide:Frost `def51848`. All are as expected on both builds.
- **The bot tests.** `Dusktracer.Bot`: 26 of 26 pass on build 1. On build 2, 25 of 26 pass; the one failure is the `CarryReach` assertion below.

## Build 2's failing assertion
`Dusktracer.Bot.CarryReach`, "with the arm, the bot gets out of the way of a body about to touch".
- **The fixed world:** a static body of radius 14 at 63 uu, inside the clearance of 72, with the band running from the hero out to 220.
- **What happens:** with the arm on, the bot moves (0.38, 0.92), slightly toward the body. The standard bot walks straight away.
- **The cause** is `PathThreat`'s time weight, 1 − T / (H + 0.1):
  - At H = 0.22 the discount on a later approach is steep. A diagonal that grazes the body at T ≈ 0.06 s scores about 0.97 of the weight.
  - Walking away scores about 1.125, because its closest approach is at T = 0.
  - At H = 0.6 the walk away still won. This is the effect the comment at `PathThreat` describes: read plainly, a path that walks off a body inside the clearance scores worse than standing to wait for it.
- **In play,** the arm-on bot of build 2 cuts across bodies it already touches. The build allowed one attempt, so this was not fixed.
- **The other new tests of build 2 pass,** on band 120 to 180:
  - With the arm on, a Swarm walking in from 200 uu is not walked off from; the bot steps diagonally, (0.38, 0.92).
  - From 121 uu, where it would touch within `ImminentS`, the bot steps aside, (0, 1).
  - With the arm off, both are walked straight away from.

## Step 4: the arm judges itself
The source's halo: `orbit_r_uu` 150, `contact_r_uu` 30, band 120 to 180, reach 212. The Tempest pair `Steered:Storm:Tempest:Tide -Carry Weapon.OrbitalDrones`, 15 seeds, v2, skill 1.0.

| | standard bot | build 1 (crowd + kite) | build 2 (+ body term at ImminentS, disc overlap) | hold |
|---|---|---|---|---|
| read on | build 1 + `s2/d071-blade-halo` | build 1 + `s2/d071-blade-halo` | build 2 + `s2/d071-blade-halo` | build 1 and build 2 (equal run for run) |
| runs evolved | 15 | 14 | 11 (4 died before the hand) | none |
| weapon hits a minute after the pick | 61 | 69 | 71 | 247 (the plain drones) |
| nearest enemy median (p25, p75), uu | 201 (174, 253) | 187 (163, 276) | 176 (144, 256) | 434 (352, 475) |
| intervals within 212 uu | 40 of 70 | 38 of 63 | 37 of 59 | 0 of 90 |
| intervals in the band, 100 to 220 uu | 46 of 70 | 40 of 63 | 36 of 59 | 0 of 90 |
| died before the boss (of the evolved) | 7 of 15 | 9 of 14 | 8 of 11 | 0 of 15 |
| damage taken a minute before the boss | 22.0 | 34.1 | 63.0 | 1.7 |
| reached the boss, won | 8, 4 | 5, 5 | 3, 3 | 15, 15 |
| before the boss against the hold | 0.89 | 0.78 | 0.71 | |
| boss fight against the hold | 0.55 (8 runs) | 0.64 (5 runs) | 0.59 (3 runs) | |

The standard column reads with the arm off on build 1. With the arm off, build 1 equals the base run for run (step 3), so this column is D-071's reading again (61 hits a minute, median 201 uu).

**The verdict: taken out on both builds.**
- Neither reaches 180 hits a minute.
- The arm closes the nearest enemy by 14 and then 25 uu.
- It raises the hits by about 10 a minute.
- It multiplies the damage taken by one and a half, then by nearly three.

## Not traced
- **Why the hits stay near 70.** The nearest body is inside the reach in most intervals after the pick: 37 of 59 on build 2. Yet the weapon lands about a third of the hits the plain drones do in the hold.
- **How many enemies stand within 212 uu after the pick** (median per interval, on the standard column and on build 2): not read. The run files hold no enemy positions; the `hit` records carry counts and damage by source only, and the interval CSV carries only the nearest enemy.

## What the arm cannot show
How a person plays. A person may stand in a halo's ring on purpose, or walk through a crowd the bot will not. The arm models no player. It was a probe of whether a weapon that gives up its rounds holds a run when it is used, and on this reading the bot does not use it that way.
