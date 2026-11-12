# What an evolution refuses is handed back (Draft 14)

For the scheduler, from the Mechanics module session (D-055 item 4), asked for by D-070's rulings 7 and 11. Written on `plan/evolution-refund` from `main` at f5d7334. Nothing in it is in force until the scheduler accepts and numbers it. Nothing was built for it.

Build: none read here. The table of refused modifiers is computed, not played: the repository's own compatibility function (`Tools/gen_matrix.py`, the one offers, sockets and composition share by `modifiers.md` R-MOD-02) run by a scratch script over `dev/s2`'s catalogs at d5d4a81, with the Pulse Wall row read as D-070 will leave it (capabilities Radial and Area; the two flags of its ruling 10 native on Area).

A constant or a test id that exists only on `dev/s2` is written without its prefix.

## What the source does
- `Handoff_Dusktracer/game/reference/specs/build_depth_spec.md` §1.4, on all ten evolutions: "They swap the weapon def (like a fusion) and keep its modifiers when the capability tags allow (§3; incompatible modifiers are refunded as a Reroll)." Its §3: "Evolutions re-check the matrix with their new caps (§1.4). For example, Blade Halo drops Projectile, so its bolt modifiers are refunded."
- The source's decision log (`reference/original_docs/02_TechnicalDesign_s17_decision_log_HISTORY.md`): "level, slot and compatible modifiers are kept; refused modifiers are removed, returned, and paid back as one Reroll each (`URHRunState::AddTools`). The toast is `ST_UI` `Transform.Refund`, shown by UI." Its test: "Railstorm drops Homing, Blade Halo and Sweep Beam drop Pierce".

## What the port does
- `reforging.md` R-RFG-04: "a modifier the new caps cannot express goes dormant (R-MOD-08), never refunded".
- `modifiers.md` R-MOD-08: a dormant modifier "keeps its socket, is shown as dormant, and contributes nothing until the weapon can express it again"; the HUD lists it, and the card names it before the pick. "Only a blessing or an evolution can silence a socket."
- `modifiers.md` holds the question open: "Dormant modifiers may need a refund (a reroll) to avoid feeling like a lost pick → decided with `offers.md` after the S1 validation sessions."
- A weapon holds at most three modifiers (`MOD.SOCKETS_PER_WEAPON`). A run starts with two rerolls (`OFFER.REROLLS`).

So on the port an evolution's capabilities are fixed for the rest of the run, a modifier they refuse can never wake, and it keeps one of three sockets until the run ends.

## Which evolutions refuse what
| Evolution (weapon) | Modifiers that fit the plain weapon | Refused by the evolved capabilities |
|---|---|---|
| Railstorm (Pulse Rifle) | 16 | 6: Split Core, Scatter Shell, Ricochet, Homing, Orbit Lock, Boomerang |
| Flechette (Scatter Cannon) | 16 | none |
| Blade Halo (Orbital Drones) | 9 | 2: Pierce, Backwash |
| Sweep Beam (Rail Lance) | 14 | 7: Split Core, Scatter Shell, Pierce, Backwash, Ricochet, Boomerang, Spread Wave |
| Carpet (Flak Mortar) | 9 | 3: Scatter Shell, Mirror, Fork |
| Cyclone (Arc Blade) | 7 | none |
| Murmuration (Seeker Swarm) | 10 | none; Scatter Shell and Spread Wave newly fit |
| Pulse Wall (Ring Emitter), as D-070 leaves it | 14 | 10: Split Core, Scatter Shell, Pierce, Lingering Field, Backwash, Ricochet, Homing, Orbit Lock, Boomerang, Spread Wave |
| Vortex (Spiral Projector) | 14 | none |
| Singularity (Gravity Mine) | 7 | none |

- Five of ten evolutions can silence a socket. For Sweep Beam and Pulse Wall it is half or more of what the plain weapon could carry, so a weapon with three modifiers will usually lose at least one. `Validation/S2_evolutions.md` saw it in play: Sweep Beam "loses a modifier half the time".
- The port refuses more than the source's test names (the source's Railstorm drops Homing; the port's also drops five others), because the port's flags and blocked capabilities are its own.

## Draft 14. A modifier an evolution refuses leaves its socket and comes back as a reroll

- **Class:** RULE (`dev/s2` `reforging.md` R-RFG-04, with `modifiers.md` R-MOD-08 and `offers.md` R-OFF-10 following it). One number is used and none is new: one reroll for each modifier handed back, the source's
- **Lowest branch:** `dev/s2`. **Before session A:** yes for this draft, because it is scoped to evolutions, which are S2 content. Dormancy from a blessing's conversion is S1 play and is left exactly as it is: the open question in `modifiers.md` keeps its own timing, after the S1 validation sessions
- **Order:** its own record, before the S2 kit is built (D-070 ruling 7). It touches `reforging.md`, where D-070 is in flight and Drafts 12 and 13 wait; the scheduler places it in that line

### Options
- **A — the source's refund.** At the pick, each modifier the evolved capabilities refuse leaves its socket, goes back into that player's run pool, and gives one reroll. The card names each one before the pick, in the box that names a dormant socket today.
- **B — the socket back, no reroll.** As A without the tool. The pick still costs the player the level they spent on that modifier.
- **C — as today.** The modifier sleeps in its socket to the end of the run.
- **D — a swap.** The modifier stays dormant, and a later modifier card for that weapon may take a dormant socket. No tool moves, but the player has to find and spend another card to undo what the evolution did.

### Decision (recommended)
A.

- R-RFG-04 becomes: an evolved weapon keeps its id, level, slot, load rank, source and its sockets; its declared caps become the row's `caps`; a modifier the new caps cannot express leaves its socket at the pick, returns to that player's run pool, and gives the player one reroll. The Reforging card names every modifier it would hand back, and the rerolls, before the pick.
- R-MOD-08 keeps dormancy for the one case left, a blessing's conversion, where a later change can still wake the socket. Its sentence "Only a blessing or an evolution can silence a socket" loses the evolution. The line "Reforge refunds" leaves `modifiers.md`'s out-of-scope list, and its open question narrows to conversions.
- `offers.md` R-OFF-10 names the refund as a way a reroll is gained during a run, beside the lock it already hands back.
- Why A and not B: the reroll is what pays for the pick the player spent, and it is the tool that helps find something for the socket that came back. Why not C: a socket is a third of what a weapon can carry, and the evolution is the one event in a run that can never give it back. Why not D: it charges a second card for the game's own change.
- What A may do to the hands, said now: a weapon can hand back at most three modifiers, so one pick can add three rerolls to a run that began with two. That is the source's rate. If people or the bot's hands show it is too many, the lever is the count per pick, as a step of its own.
- D-070's stated exception in `weapons.md` R-WPN-08 is read again here, as its ruling 11 asked, and it stays. It says that on the Pulse Wall row the flags a ring's strike cannot carry resolve to nothing. Those are a Keeper's flags as well as a modifier's, and no refund touches a Keeper's card. With this rule the exception's sentence stops naming a refund as its end; it ends when some rule gives a zone weapon a resolution for those flags, if one ever does.

### Hypothesis
We believe that handing back what an evolution refuses leaves no dead socket on an evolved weapon, because the modifier leaves at the pick where today it sleeps to the run's end; measured by `Tools/BotRuns.ps1 -Carry` on the five pairs whose evolution can refuse a modifier (Pulse Rifle, Orbital Drones, Rail Lance, Flak Mortar, Ring Emitter), 15 seeds each, bot v2 at skill 1.0, read by `Tools/evolve_read.py`, which prints dormant sockets today and to which the job adds the modifiers handed back, the rerolls gained and the sockets fitted again after the pick; judged: no socket is dormant from an evolution in any run, and the rerolls gained equal the modifiers handed back in every run; printed and judged nowhere: how many freed sockets the bot fits again, the two windows of each pair beside their reading before the rule, reach and wins; reverted if a socket is still dormant from an evolution, or the rerolls do not match.

- This is a rule about what the game does with a pick, so its judged half is exact and small. Whether a player sees the hand-back, understands it and uses the reroll is the first S2 session's, and the S2 kit asks it.
- If the two windows of a pair fall against their reading before the rule, that is printed and reported, not judged: a freed socket changes the hands after the pick, and one battery cannot tell a worse build from a different draw.

### The perf bound
None is owed: the rule acts once, at a pick, on at most three sockets. The job says so in its note and reads nothing.

### Impact (dependency sweep)
- Rules replaced: `reforging.md` R-RFG-04 and its test line; `modifiers.md` R-MOD-08, its out-of-scope line and its open question; `offers.md` R-OFF-10; `weapons.md` R-WPN-08's exception sentence.
- Constants and catalog cells: none. The table above follows the catalogs by itself.
- Discoveries, walked by counter on `dev/s3`'s `discoveries.csv`: `Best.EliteKillMods` (`Elite3Mods`) counts a weapon's sockets at an elite kill, dormant ones included (`Meta/DTRunRecorder.cpp`). After a hand-back the weapon holds fewer until it is fitted again, so an evolved weapon can count lower than today. That row opens on a first run and an evolution comes at the Reforging hand, so it is normally met long before; the job says in its note whether any carry run met it only after the pick. No other counter reads sockets or tools.
- Files / tests: the three specs; the Reforging pick in `Combat` and the offer state's tool count and run pool; the dormant list the HUD reads, which no longer holds an evolution's sockets; the Reforging and Modifier tests; `Tools/evolve_read.py` and its tools test.
- Text: a line for the Reforging card and one for after the pick, en and zh, the zh a first draft: the card says which modifiers come back and how many rerolls. The card's box exists (`04_Presentation` R-PRE-08); whether a line after the pick is drawn, and where, is the Visual session's, asked when this rule is accepted.
- Readings that move: any run that evolves one of the five weapons with a refused modifier fitted deals different hands after the pick. The S2 exit battery is read again after this lands if any of its runs does; the job counts them from the battery's own files.
- The S2 kit: one question, whether the validator saw what the evolution handed back and what they did with the reroll.
- Not shown by any of this: no person has met a hand-back; the bot's use of a reroll is its policy, not a player's.

### Swap-out point
The first S2 session with people.
