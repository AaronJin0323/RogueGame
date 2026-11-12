# S2: D-073, what an evolution refuses is handed back

Branch `s2/d073-evolution-refund`, cut from `dev/s2` at `a200b91` (it holds D-079). Since then `dev/s2` was merged in at `af82843` (the reader's boss-fight block) and at `7b3ebf7` (two validation notes). Class RULE: `reforging.md` R-RFG-04, with `modifiers.md` R-MOD-08 and `offers.md` R-OFF-10 following it.

**The judged half is met on all three pairs.** No socket is dormant from an evolution in any run. In every run the rerolls gained equal the modifiers handed back: Pulse Rifle 1 for 1, Rail Lance 5 for 5, Flak Mortar 0 for 0. No Flak Mortar run evolves with a refused modifier fitted, so on that pair the judged half has nothing to read.

The judged half rests on six hand-backs in forty-five runs, and none of them is Carpet's. The tests hold the cases play did not meet: two and three refused modifiers on one weapon, a modifier the evolved caps accept, and an evolution that refuses nothing.

Build:
- **Base.** The tree's binaries of `40be484`: D-079's gated build, whose code is `a200b91`'s. Read on it: the six batteries of step 0.
- **The rule, `2ec5148`.** The rule is `5360d1e` and the reader `3b44cef`; `2ec5148` adds one moved test line. Read on it: the Reforging, Modifiers, Offers and Telemetry tests, the three arms of step 5, and the Rail Lance hold read again.
- **The gate** is read on the tip below, with the five seed 9107 arms beside it.

## The record's hypothesis
"We believe that handing back what an evolution refuses leaves no dead socket on an evolved weapon, because the modifier leaves at the pick where today it sleeps to the run's end; ... judged: no socket is dormant from an evolution in any run, and the rerolls gained equal the modifiers handed back in every run; printed and judged nowhere: how many freed sockets the bot fits again, the two windows of each pair beside their reading before the rule, reach and wins" (D-073).

## Step 0: what refuses what
Computed with `Tools/gen_matrix.py`'s own `compatible()` over `a200b91`'s catalogs. Each evolution is read with its row's `caps`, against the modifiers that fit its plain weapon:

| Evolution (weapon) | Fits the plain weapon | Refused | Refused modifiers |
|---|---|---|---|
| Railstorm (Pulse Rifle) | 16 | 6 | Split Core, Scatter Shell, Ricochet, Homing, Orbit Lock, Boomerang |
| Sweep Beam (Rail Lance) | 14 | 7 | Split Core, Scatter Shell, Pierce, Backwash, Ricochet, Boomerang, Spread Wave |
| Carpet (Flak Mortar) | 9 | 3 | Scatter Shell, Mirror, Fork |

**Three evolutions refuse a modifier.** Flechette, Blade Halo, Cyclone, Murmuration, Pulse Wall, Vortex and Singularity refuse nothing:
- Pulse Wall keeps its capabilities by D-073 ruling 9.
- Blade Halo keeps the plain weapon's caps by D-079.

The record names five pairs in its Context and Hypothesis and four in ruling 9. The catalogs give three, and this job reads those three.

## Step 1: the rule's text
- **`reforging.md` R-RFG-04, as the record words it.** A refused modifier leaves its socket at the pick, returns to that player's run pool, and gives one reroll, the source's rate. The Reforging card names every modifier it would hand back, and the rerolls, before the pick.
  - T-RFG-04 adds `Dusktracer.Reforging.HandBack`.
  - The telemetry line names `handed_back` and `rerolls_gained`.
- **`modifiers.md` R-MOD-08.**
  - Dormancy now arises from a blessing's conversion only.
  - "Only a blessing or an evolution can silence a socket" loses the evolution and points to R-RFG-04.
  - "Reforge refunds" leaves Out of scope, and the open question narrows to a conversion's dormancy.
  - Text adds `ST_Modifiers.HandBack.*`.
- **`offers.md` R-OFF-10.** An evolution's hand-back is a way a reroll is gained during a run, beside `Reserve.SpareTerms`.
- **`weapons.md` R-WPN-08** is as `dev/s2` has it: `git diff a200b91 -- Docs/02_Systems/weapons.md` is empty. By ruling 9 no exception was written.

## Step 2: the code
- **One place for the hand-back.** It happens in `DTOffers::ApplyPickState`, at the Reforging card's evolution, where the row is set.
  - What it hands back is what the card named: `DTOffers::DormantAfterPick`, the composition on copies, read before the row is set. An evolution's entries now carry `bHandBack`.
  - Each such socket is removed from the weapon, and the player's `RerollsLeft` goes up by one. `FDTPickOutcome` carries `HandedBack` and `RerollsGained`.
- **The run pool.** No modifier is taken out of the run pool by socketing it: a modifier card is drawn for any free socket that does not carry it (`offers.md` R-OFF-11). So a freed modifier is back in the pool with nothing more to do. The test shows the freed socket takes a modifier card, and each handed-back modifier is drawn again for a weapon that expresses it.
- **A blessing's conversion is not touched.** `Dusktracer.Modifiers.Dormant` (T-MOD-08) passes unchanged.
- **The card's box** names each handed-back modifier with `ST_Modifiers.HandBack.Card` ("{Modifier} comes off {Weapon} and back to the pool"). It ends with `ST_Modifiers.HandBack.Rerolls` ("Renegotiate +{Count}"; Renegotiate is the reroll tool's name).
  - The zh lines are first drafts, marked for the native reader in the Context column (D-073 ruling 7).
  - No line after the pick is drawn; whether and where is the Visual session's.
- **The HUD's dormant list** reads the same state, `DTWeaponCompose::DormantSockets`. The HandBack test shows it holds no evolution's socket after the pick.
- **The run file.** The `reforge` record adds `handed_back` (semicolon-joined modifier ids) and `rerolls_gained`. `dormant` stays, and it is now empty after an evolution.
- **No constants.** No constant and no catalog cell is added: one reroll for each modifier is the source's rate, written in R-RFG-04.

## Step 3: the reader
`Tools/evolve_read.py --carry` prints, per batch, after the dormant line:
- the modifiers handed back;
- the rerolls gained against the count handed back;
- the runs with a dormant socket at the pick;
- the runs whose rerolls differ;
- the freed sockets fitted again (the carried weapon's `modifier_socketed` records after the pick, at most one a freed socket);
- then one line a run.

Tools test: `HandBackRead`. This sits on top of the reader's boss-fight block (D-081), merged from `dev/s2` first.

## Step 4: tests
- **New:** `Dusktracer.Reforging.HandBack`, on Sweep Beam.
  - One, two and three refused modifiers socketed: the card names each as handed back, and the reroll line carries the count. At the pick those sockets are freed and as many rerolls are added, and the outcome names them. Chain Trigger, which the evolved caps accept, stays. No socket is dormant, and the HUD's dormant list is empty. The freed socket takes a modifier card, and each handed-back modifier can be drawn again.
  - An evolution that refuses nothing (Murmuration with Pierce and Chain Trigger): the card names nothing, both sockets stay, and no reroll is given.
- **`C.GATE.TEST_FLOOR`** goes from 485 to 486, with a number-log row.
- **Moved expectations:**
  - `Dusktracer.Reforging.Caps`. Old: after the evolution the refused modifier is "still socketed" (1 socket) and "dormant". New: it leaves its socket (0 sockets) and gives one reroll (+1), and the card names it as handed back. The pick now goes through `ApplyPickState`, where the test used to set the row by hand.
  - `Dusktracer.Telemetry.Records.Weapons`. Old: the `reforge` line ended at `"dormant":"Mod.TestRecA;Mod.TestRecB"`. New: it goes on with `"handed_back":"Mod.TestRecC","rerolls_gained":1`.
- **Results on `2ec5148`:** Reforging 31 of 31, Modifiers 20 of 20, Offers 20 of 20, Telemetry 28 of 28. The two new text rows needed `Tools/loc_build.py --build` before the Offers test read them.

## Step 5: the reading
The pair `Steered:Storm:Tempest:Tide`, `-Carry <Weapon>`, 15 seeds, v2, skill 1.0. Each arm is read on the base and on the rule, against the base's hold.

**Judged, exact, each run by itself:**

| Pair | Base: runs dormant at the pick (modifiers) | Rule: runs dormant at the pick | Rule: handed back | Rule: rerolls gained | Runs whose rerolls differ |
|---|---|---|---|---|---|
| Pulse Rifle | 1 (Scatter Shell) | 0 | 1 (Scatter Shell, s15) | 1 | none |
| Rail Lance | 5 (Pierce 3, Split Core 2) | 0 | 5 (Pierce s4, s5, s10; Split Core s9, s13) | 5 | none |
| Flak Mortar | 0 | 0 | 0 | 0 | none |

**Printed, deciding nothing.**

The windows. `--check-windows 0.95 0.9` now judges the fight on the damage dealt to the boss (D-081); the old window is quoted from `--judge-fight-on-old-window`.

| Pair | Read | Before the boss | Fight: damage to the boss, median (pooled) | Fight: old window | Reached the boss, won | Line |
|---|---|---|---|---|---|---|
| Pulse Rifle | base | 1.00 | 1.01 (0.91) | 1.03 | 14, 14 | PASS |
| Pulse Rifle | rule | 1.00 | 1.01 (0.91) | 1.03 | 14, 14 | PASS |
| Rail Lance | base | 0.95 | 1.35 (1.06) | 1.34 | 12, 11 | PASS |
| Rail Lance | rule | 0.95 | 1.25 (1.04) | 1.27 | 12, 11 | PASS |
| Flak Mortar | base | 1.01 | 0.78 (0.92) | 0.78 | 10, 10 | FAIL (fight 0.78 < 0.9) |
| Flak Mortar | rule | 1.01 | 0.78 (0.92) | 0.78 | 10, 10 | FAIL (fight 0.78 < 0.9) |

What the windows show:
- **Freed sockets fitted again:** 0 of 1 on Pulse Rifle, 0 of 5 on Rail Lance. The bot fitted no freed socket again before the run's end.
- **Run for run against the base:**
  - Pulse Rifle's rule arm equals the base 15 of 15. Its one hand-back (s15) changed nothing the run records.
  - Rail Lance differs in one run of its five hand-backs (s5: victory at 989.3 s against 937.4 s).
  - Flak Mortar equals the base 15 of 15.
- **The hold.** The Rail Lance hold read on the rule equals the base's hold 15 of 15 in outcome, `end_s`, `steps` and `input_hash`. A hold never evolves, and the rule moves nothing in it.
- **Ruling 4.** None of the three pairs has a landed rule of its own (they are the ports), so ruling 4's report-before-landing does not arise. The windows are printed all the same.
  - Flak Mortar's fight window fails the same on the base and on the rule. No Flak Mortar run hands anything back, so the rule did not move it.

## Step 6: what the record asks to be counted
- **`Best.EliteKillMods` (`Elite3Mods` on `dev/s3`).** The carry runs of the rule's arms that held three modifiers on the carried weapon before the pick (`modifier_socketed` records on it before the `reforge` record):
  - Pulse Rifle 0, Rail Lance 2, Flak Mortar 0.
  - Of those, 1 run (Rail Lance) handed one or more back.
  - The counter itself is `dev/s3`'s and is read at the carry.
- **Ruling 6, the S2 exit battery.** Its files were read read-only where the exit note says they are: the carry agent's tree, `Saved\BotRuns\s2_exit`, 540 runs of `146af0a`, at `E:\Dusktracer\.claude\worktrees\agent-aa7a88e9be0750f6a`.
  - Evolutions of the three weapons there: Flak Mortar 3, Pulse Rifle 1, Rail Lance 0.
  - Of those, **one run evolves with a refused modifier fitted**: `bots_1.0` `Steered-Radiance-Lance-Storm_s2`, Carpet with `Mod.Fork`. Its `reforge` record's `dormant` field names it.
  - That run's hands after the pick change with this rule, so the battery is read again after this lands, as ruling 6 says.

## Perf
None is owed: the rule acts once, at a pick, on at most three sockets (D-073, The perf bound). Nothing was read.

## Step 7: the gate
`& Tools\Gate.ps1 -Base dev/s2` on `cff6537` (`dev/s2` is `7b3ebf7`, an ancestor):
- `GATE: PASS (cff6537; 3 not run; mask FFFFF3FC)`.
- The tests: 486 run of 486, 0 failed, floor 486.
- `RESTART: PASS` (seed 9107, input_hash `276faf8e` / `276faf8e`, steps 5400 / 5400).
- Not run: pad (no package), scope (no brief in flight), mac (deferred, D-007).

This documents commit on top carries only the gate's line and the five hashes.

The five seed 9107 arms by hand on the gated build: None `00e0d4ec`, Random `51369747`, Steered:Forge:Hammer `e3293942`, Steered:Storm:Tempest `b046559b`, Steered:Tide:Frost `def51848`. All are as expected; none of them evolves a weapon (`reforge=none` in each).

## Not shown
- No person has met a hand-back.
- The bot's use of a reroll, and that it fitted none of the six freed sockets, is its policy, not a player's.
- Whether a player sees what the evolution handed back and what they do with the reroll is the S2 kit's question.
