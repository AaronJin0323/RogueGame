# Mechanics rules: eight decision drafts and four plans

For the scheduler, from the Mechanics module session (D-055 item 4). Written on `plan/mechanics-rules` from `main` at 4d17faf. Nothing in it is in force: each draft is accepted and numbered by the scheduler, changed, or returned. Nothing was built, launched or measured for it.

Build: none read. Every number is quoted from a note or a file at `main` 4d17faf, `dev/s2` a7137b4 (the S2 exit reread of 146af0a) or `dev/s3` 81c638e; the note is named beside it. "Source" is `Handoff_Dusktracer/game/reference/`, input only (`AGENTS.md`, precedence).

A constant that exists only on a later branch, or is proposed here, is written without its `C.` prefix, so the lint on `main` does not look for it.

## 0. The eight at a glance

| # | Question | Class | Recommended | Lowest branch | Before session A |
|---|---|---|---|---|---|
| 1 | R-KEE-15 and a cast that deals no damage | RULE | The Tide cast strikes what it holds; R-KEE-15 keeps its text. Decided at the S1 gate with draft 2 | `main` | no: `Source`, and power |
| 2 | Tide's early power and boss answer | RULE, then SYSTEM | Draft 1's hit first; then Cracked on the boss as its own record. Nothing tuned on wins | `main` | no |
| 3 | Payday pays for itself (R-KEE-29) | RULE | A payday's own kills add no Debt | `dev/s2` | yes |
| 4 | Pulse Wall strength (R-RFG-12) | TUNE | One pass on `dps`, the value from an uncommitted sweep; restored on a miss | `dev/s2` | yes |
| 5 | The L8 hand's Favor weight; the Keeper hand's seats | text; RULE | R-OFF-08 says the draw is unweighted. A person's all-Keepers run draws every seat; a tool run keeps its seats | `main` (text); `dev/s2` | yes; yes, with the S2 kit |
| 6 | The Trait's two-card prerequisite (R-KEE-04) | text | Keep two cards, the Key counted; `offers.md` says so too. No "Rare", no Discovery | `main` | yes: documents only |
| 7 | Rarity on chest and Reserve cards | RULE | A mark only where it changes the card: the chest lift applies to weapon upgrades | `main` | the Reserve sentence yes; the lift no |
| 8 | A fresh save's pool (D-049's open item) | RULE | Open one archetype's two Amps and Trait per S1 Keeper: Arc Grid, Hammer, Frost | `dev/s3` | yes; needed only before the S3 kit |

**One RULE record per spec at a time (D-050 item 6).** On `dev/s2`'s `keepers.md`: draft 3, then draft 5's seats once draft 3 has its probe reading. On `main`'s `keepers.md`: draft 1, at the gate. On `offers.md`: drafts 5 and 6 change words and no behaviour and can ride in one commit; draft 7's lift is the one RULE and waits for session A. Drafts 3, 4 and 5 and the fit data of `s2/fit-and-unlock` each make the S2 exit reading stale; land them together and reread once.

**Not found.** `STATE.md` names three `offers.md` findings (the Trait's prerequisite, the L8 weight, rarity on chest and Reserve cards). No note or audit section on any branch writes them out. Drafts 5 to 7 state each finding as the texts show it.

## Draft 1. R-KEE-15 and a cast that deals no damage

- **Class:** RULE (`keepers.md` R-KEE-14; R-KEE-15 keeps its text)
- **Lowest branch:** `main` (Tide is an S1 Keeper). **Before session A:** no

### Context
- R-KEE-15, the same on `main` and `dev/s2`: each variant and layer combination of a Keeper deals marginal damage (with the cast minus without, on a refilled field) within `C.INTERCESSION.BAND` of that Keeper's own mean; a part outside it is retuned toward the mean by at most `C.INTERCESSION.RETUNE_STEP_MAX` a pass. T-KEE-15 is a reading, not a gate step.
- The Tide cast displaces and freezes and emits no hit (`Keepers/DTKeeperSim.cpp`, `TidePull`). `Tools/probe.py` puts every row out of band when a Keeper's mean is at or under zero.
- Readings. S1: 15 of 57 in band, before D-019 and after it (Storm 8 of 19, Forge 7 of 18, Tide 0 of 20). Tide's mean: −3 004, then 1 083 after D-019 "set by one row with SD 31 555" (`Validation/S1_tide_rule.md`), then −2 126 in the `dev/s2` reread. S2 exit, reread at 146af0a on 2026-11-11: 45 of 282 (Tide 2 of 47 on a mean of 911, Shade 0 of 48, Quicksilver 2 of 48; `dev/s2` `Validation/S2_exit.md`).
- Shade and Quicksilver do not belong to this question. Shade's cast executes, and its mean (600 282) is Economy's payday loop: draft 3. Quicksilver's cast fires a volley on every dash, and its reading is the probe route and a neutral time scale: plan P1 and the S2 kit. Only Tide's cast strikes nothing.
- The source keeps the damage measure for control casts and bands inside a Keeper for that reason: "Burst and control Intercessions cannot be compared by damage across Keepers", and "amplifier variants are valued by what they add to the hero's own damage (the with-cast minus no-cast metric stays)" (`specs/intercession_variants_spec.md`). Its own Tide cast had hit paths (`balance/intercession_strength.md`: a damage multiplier on "the three Begin Hit paths"). The port's cast has none.
- `Validation/S1_keeper_power.md` §6 listed three candidates. D-019 took the first (Frost and Stillness drop the line). Left: "a Tide cast deals damage of its own" and "R-KEE-15 judges Tide on something other than marginal damage".
- `10_Tuning.md §4`: a probe field cannot credit control.

### Options
- A — The Tide cast deals damage of its own. R-KEE-15 stands as written and applies to every Keeper. A `Source` change and one new number on `main`; it adds power to the weakest Keeper.
- B — R-KEE-15 judges a control cast by control. It needs an instrument that does not exist: a scene where the hero can be hit, read as damage avoided. Until then no tool judges Tide.
- C — R-KEE-15 exempts a cast that deals no damage and asks nothing in its place. True to the tool; Tide's twenty combinations then have no parity check.
- D — Leave it. The row fails for Tide whatever is tuned.

### Decision (recommended)
A, taken at the S1 gate in one record with draft 2, and built after session A. Until then no text moves.

- R-KEE-14 gains: the wave strikes each enemy it takes hold of once, as the hold begins, for `INTERCESSION.TIDE_HIT` (new, GATED(S1)) of the owner's strongest weapon's volley damage, as Keeper damage. A target that refuses the hold is still struck.
- First value, `[G]`: the job reads one probe battery and sets the value so that the base cast with no layer deals half the mean of Forge's no-layer casts on the same scene. Half, because the Tide cast also holds the crowd and Forge's does not. After that only R-KEE-15's own retune moves it.
- Why A and not B. The band was written for casts that all deal damage; the port lost Tide's. The same hit is the lever draft 2 needs: the diagnosed cause of Tide's deaths is that held enemies stay alive. B's instrument is worth having (plan P4) and cannot be the only thing between Tide and a reading.

### Hypothesis
We believe a hit of its own moves Tide's combination mean on the cast scene from at or under zero (1 083 at S1, −2 126 on `dev/s2`) to a positive mean that the hit sets and no single row does, and the kills a Tide cast makes from none to some, because the cast today moves and freezes and strikes nothing; measured by `Tools/probe.py read --check` on the build session B plays (the mean, its largest row's share, rows in band) and by a Tide cast's kills in the bot arms; reverted, the clause and the constant removed, if one row still holds more than half of the sum.

### Impact (dependency sweep)
- Rules replaced: R-KEE-14 gains the clause. `10_Tuning.md §3` loses "A cast that deals no damage of its own has nothing to band". D-019 stands.
- Constants: + `INTERCESSION.TIDE_HIT`.
- Files / tests: `02_Systems/keepers.md`, `constants.yaml`, `08_Constants.md`, `Keepers/DTKeeperSim.cpp`, the Keeper tests, a number-log row, the cast's card line in en and zh-Hans if it says what the cast does.
- Readings that move: seed 9107 on Tide arms, every Tide bot reading, T-KEE-15, the Tide rows of T-KEE-18.

### Swap-out point
Session B's reading of the Tide rows.

## Draft 2. Tide's early power and boss answer

- **Class:** the hit is draft 1's RULE; Cracked is SYSTEM (a status that does not exist)
- **Lowest branch:** `main`. **Before session A:** no

### Context
- Bots at skill 1.0, 15 seeds, reach and wins (`Validation/S1_keeper_power.md` §5, final column): Tide-Frost 10 and 8, Tide-Frost-Storm 11 and 9, Tide-Stillness 6 and 3, Tide-Stillness-Storm 5 and 4, Tide-Whirlpool 6 and 3, Tide-Whirlpool-Storm 5 and 2. Forge-Hammer 14 and 13. So Tide arms die before the boss in 4 to 10 runs of 15 and lose at it in 1 to 3.
- That table is older than the chest restore (D-046). The nineteen arms have been run since (`Validation/S1_d045_sources.md`: 231 of 270 steered runs at the boss, 192 wins), and that note gives no figure per arm here; the reading planned as `S1_d045_sources_3.md` prints reach and wins per arm on the session build.
- Why, on `dev/s2` before the restore (`Validation/S2_trace.md` §3): "Other Keys add damage early, Tide's do not: minute 1 damage 2278 against 2566 ... Held enemies then stay alive, and 2 to 3 times the crowd stands at half the distance. 32 of 45 die before the boss." At the boss, damage per second 164 against Quicksilver's 623.
- With the restore, on `dev/s2` at 146af0a (`Validation/S2_exit.md`, 45 runs a Keeper): Tide wins 15 and reaches the boss in 34 at skill 1.0, up from 5 wins, and is still last (Storm 30, Radiance 35, Forge 42, Shade 42, Quicksilver 45). At skill 0.6 Tide wins 5 of 26 that reach it. Tide lost 19 of 34 boss fights at skill 1.0; Storm lost 1 of 31.
- The boss refuses the kit. R-BOS-11: Freeze is refused and Chill stops at `C.STATUS.BOSS_CHILL_STACK_MAX`. R-BOS-03: every displacement naming the boss is ignored. Card by card, only Stillness's charged-volley multiplier and a shatter of nearby adds reach it (read from the catalog, not measured).
- The source's answer is D-60: control on a control-immune target is "not refused"; it converts to Cracked, 20 % more damage taken for 2 s, with boss time across the showcase builds kept within 20 %. `boss.md` Out of scope puts it in S2, and `dev/s2` did not build it.
- The source's per-archetype answers are numbers this port set lower: Whirlpool's crush at 15 % "its own damage source when played alone" (here 0.10, and 0.05 more on an Amp); Stillness's charged volley at 2.5 (here 2.0).
- `10_Tuning.md §5` item 5 freezes Whirlpool's Key numbers and Frost's shatter damage until people have read them. `Validation/S1_tide_rule.md` tried number moves and kept none; the same tree on two seed ranges read 45 and 30, then 45 and 24, so a move of 2 or 3 on an arm of 15 can be noise.
- The Warden's HP is set from the damage dealt in the first seconds of the fight (`Validation/plans_audit.md` 5.6).
- Session A records each run's Patron and outcome and asks nothing more of Tide. The owner's one run took no Tide card.

### Options
- 1 — Cracked: a refused Freeze on the boss becomes a vulnerability (the source's D-60).
- 2 — The cast strikes what it holds (draft 1).
- 3 — The source's values for crush and for the charged volley: a TUNE on numbers that are frozen until a person has played them.
- 4 — A Patron trait for Tide (the source's Cold Hands): new content, S2 scope.
- 5 — Nothing until a person has played Tide.

### Decision (recommended)
Nothing lands before session A, and nothing is tuned on wins (D-050 item 5). At the gate, in this order.

1. Read what session A shows of Tide: `Tools/session_read.py` prints each run's Patron and end. If no validator took Tide, the gate says so; the reading is then bots only.
2. Take draft 1's hit as the early-power lever. The cause on record is the crowd a Tide build holds and does not kill. A Key number does not reach that, and the frozen numbers may not move.
3. Take Cracked as the boss answer, as a SYSTEM record of its own, after the hit has a reading: a system's rules change at most once in a validation cycle (`Harness/H04 §6`). R-BOS-11 would read: a Freeze on the boss is not refused, it becomes Cracked, with the source's two numbers as `[S]`. Two things first: how the Warden's HP calibration reads damage dealt while Cracked is up (if it counts it, the status cancels itself), and a mark for the status on the boss, which is a Visual file.

A Tide validator who beats the boss at session A, or Tide runs that end for reasons that are not the crowd, would change this.

### Hypothesis (for Cracked, when it is taken)
We believe turning a refused Freeze into Cracked moves a Tide arm's damage per second at the boss from 164 toward the other Keepers' and its losses at the boss down from 1 to 3 of 15, because today the boss refuses every Tide card but one multiplier; measured by damage per second at the boss on the Tide arms and by Tide validators' boss fights at a session; reverted if boss time across the nine S1 archetypes leaves 20 % of its mean, the source's own guard.

### Impact (dependency sweep)
- Rules replaced: `boss.md` R-BOS-11 and its Out of scope line; on `dev/s2`, `keepers.md` R-KEE-25's sentence.
- Constants: two new, from the source. Files: the enemy pool's status code, the boss tests, one text key, a status mark on the boss (Visual).
- Retires: the refusal of Freeze on the boss.

### Swap-out point
The S1 gate decision.

## Draft 3. A payday pays for itself

- **Class:** RULE (`dev/s2` `keepers.md` R-KEE-29)
- **Lowest branch:** `dev/s2`. **Before session A:** yes. First RULE in line on that spec

### Context
- R-KEE-29: "each kill of a Marked enemy adds one Debt"; at `debt_max` the next volley is a payday, one homing round at every enemy in the screen scope. The rule excludes no kill, and the code tests none (`Combat/DTWeaponSimKeepers.cpp`, `ReactKeeperKills`).
- `dev/s2` `Validation/S2_trace.md` §1, the cast scene, two seeds: with the cast 1 948 998 and 1 948 952; with "a payday's kills pay no Debt (a temporary switch, not committed)" 152 441 and 157 067. "The self-pay is 12.7x ... the reading measures the refill, not the cast." At field HP scaled down forty times: 68 927 with the loop, 40 687 without.
- Shade's combination mean on the S2 exit is 600 282 at 146af0a (597 933 before), it reads 0 of 48 in band, and Economy's row with a Vector layer still reads 1 991 364. Its largest row was 3 954 879.
- In bot runs Economy is no outlier: 321 damage a second at minutes 10 to 14 against 261 to 308 for the others.
- The source is silent on whether a payday's kills add Debt. The neighbouring rules are not: R-KEE-33 and R-MOD-13 say their bursts start no reaction.
- What is left without the loop, about 150 000, is still 1.9 times Slam's 77 526: one round at every enemy, each piercing 99 more with the Amp, grows with the square of the crowd.

### Options
- 1 — A payday's own kills add no Debt.
- 2 — Keep the rule; decide that the cast scene cannot judge a screen-clear on a refilled field.
- 3 — The source's shape: the next volley seeks every enemy at a multiplier, not a round for each enemy.
- 4 — Lower `payday_mult`.

### Decision (recommended)
Option 1. R-KEE-29 gains one sentence: a kill made by a payday's own round or by its execute adds no Debt. No number moves: `payday_mult`, `debt_max` and the cast's instant payday stay.

Not 2: the loop also runs in play whenever the screen is Marked, and the scene found it honestly. Not 4: a number would hide a loop. Option 3 is the larger change and is looked at again only if Economy's row still tops Shade's band after this.

### Hypothesis
We believe excluding a payday's own kills from Debt moves Economy's no-layer cast on the cast scene from 1 948 998 to about 150 000 and Shade's combination mean from 600 282 toward the range of the other Keepers' (7 985 to 56 413), because the reading measured a loop on a field that refills; measured by `Tools/probe.py read --check` on `dev/s2`, with Economy's damage per second at minutes 10 to 14 printed beside it and judged nowhere; reverted if the cast row does not fall by at least half.

### Impact (dependency sweep)
- Rules replaced: R-KEE-29 gains the sentence; its card tests gain the case. The duo Pyre Debt is untouched: an ember is not a payday round.
- Constants: none.
- Files / tests: `02_Systems/keepers.md`, `Combat/DTWeaponSimKeepers.cpp` (the kill must carry its source there; not verified that it does), the Economy card tests.
- Readings that move: seed hashes of Shade/Economy arms; the Shade rows of the S2 exit.

### Swap-out point
The S2 exit reread that follows it.

## Draft 4. Pulse Wall strength

- **Class:** TUNE (`dev/s2` `09_Catalog/reforging.csv`, `Evolve.RingEmitter`, `dps`)
- **Lowest branch:** `dev/s2`. **Before session A:** yes. It lifts D-020's hold on this one number

### Context
- The row: `radius_uu=600;grow_s=0.6;hold_s=3;push_uu_s=200;band_uu=100;dps=2;tick_s=0.25`. The source gives the radius, the hold, the growth and the push, and no damage. R-RFG-12: each tick, each enemy in the outer band is struck for `dps` times one round's damage times the tick.
- D-020 settled the unit and kept the value: "`dps` 2 stays a first guess", and a weak ring "is a TUNE of `dps` or a RULE on the band, held for a human reading".
- The one live reading (`dev/s2` `Validation/S2_evolutions.md`, the carry pair, 15 seeds, bot v2 at skill 1.0), evolved against held: boss reach 5 against 14, wins 1 against 10, the weapon's damage per minute 1 891 against 3 603, the build's 13 141 against 17 789. "Build dpm falls by a quarter and the run ends a median 146 s after the pick."
- The arithmetic. The ring deals 0.52 of what the plain weapon dealt, and the build 0.74 of its hold. To bring the build back to its hold's level the ring must add about 4 650 a minute, 3.5 times what it deals: `dps` near 7 if output follows it in a line. The band of R-RFG-18, `REFORGE.POWER_MIN` 1.3 to `REFORGE.POWER_MAX` 1.8, would need about six times.
- `reforging.md` Open: the power band "is unread: output after the pick matches the control, which may be a field cap".
- No run of the 540-run exit battery evolves the Ring Emitter, so the exit does not read this row.

### Options
- 1 — Keep `dps` 2 and hold for people (D-020).
- 2 — A TUNE of `dps`.
- 3 — A RULE on the band: the whole disc hurts while the ring grows; or the ring pushes and deals nothing; or the weapon keeps its rounds beside the wall.
- 4 — Push only, as the source's sentence reads.

### Decision (recommended)
Option 2, one pass by `10_Tuning.md §5`.

- First a sweep that is read and not kept: `dps` 4, 6 and 8 on the carry pair, 15 seeds, in the developer's worktree, reported in the note (as `S1_tide_rule.md` took its first round). Then one committed move to the lowest value whose build ratio is at least 1.0, with the hypothesis line committed before its battery.
- Why lift D-020's hold for this number: the miss is a factor of three on output and not a matter of feel; no exit run reads the row, so nothing else will catch it; and the people who first meet it are two exits away and should judge a wall that works.
- If no value of the sweep brings the build to its hold's level, the number is not the cause. The row stays at 2 and a RULE on the band is the next draft.

### Hypothesis
We believe `dps` at the sweep's value moves a Pulse Wall build's damage per minute from 0.74 of its hold to between 1.0 and 1.8 of it, because the band holds about a third of the enemies the first guess assumed; measured by `Tools/BotRuns.ps1 -Carry` on the Ring Emitter pair, 15 seeds, bot v2 at skill 1.0 (the build's and the weapon's damage per minute; reach and wins printed and judged nowhere); restored to 2 on a miss. The lower edge is 1.0 and not 1.3 because the band's own reading may sit on a field cap.

### Impact (dependency sweep)
- Rules replaced: none. R-RFG-12 names `dps`, not its value.
- Constants: none. One catalog cell, one number-log row, the generated catalog header.
- Readings that move: runs that evolve the Ring Emitter, and no other.

### Swap-out point
The first S2 session with people.

## Draft 5. The L8 hand's weight, and the Keeper hand's seats

- **Class:** a text correction (`offers.md` R-OFF-08, `main`); RULE (`dev/s2` `keepers.md` R-KEE-23)
- **Lowest branch:** `main` for the text, `dev/s2` for the seats. **Before session A:** yes for both; the seats go with the S2 kit

### Context
- R-OFF-08: the L8 hand is "every non-Patron Keeper of the run, up to `C.OFFER.KEEPER_CHOICES`, drawn by Favor weight". Before L8 only the Patron's blessings are granted (R-KEE-05), so every candidate's Favor is zero and every weight is the base weight: the draw is uniform. D-032 already calls it "the same plain draw as the L8 hand, with no Favor to weigh it". The source's "weighted by current favor, min 1 favor-less god" came from a pool where every god's blessings were offered before L8.
- In S1 the L8 hand holds both non-Patron Keepers, so the draw decides only their order.
- `dev/s2` R-KEE-23: "An unlocked Keeper always has a seat: it comes first in the Patron hand and the L8 hand ... **Invented:** the seat rule". With three unlocks the Patron hand is Shade, Radiance, Quicksilver and holds no S1 Keeper (`Validation/plans_audit.md` 5.8).
- The seat serves tools: a bot arm names its Keepers and `Tools/BotRuns.ps1` unlocks only those.
- `dev/s3` has moved on: a tool run keeps the seat; a gated run reserves none but the hub's Patron and draws the last seat unweighted (D-032, D-044). There the hub's Shrine shows all six.
- No person's S2 run is specified: there is no S2 kit, and the launcher passes no unlock.

### Options
- A — Keep the seat for every run. A person's all-Keepers run never offers an S1 Patron.
- B — A person's Patron hand shows every Keeper of the run, six cards. The source's Shrine, and a six-card hand built for the one stage that has no hub.
- C — A person's all-Keepers run reserves no seat: both hands are drawn unweighted. A tool run keeps its seats.
- For the L8 text: say "unweighted"; or give a non-Patron Keeper Favor before L8, which rewrites R-KEE-05.

### Decision (recommended)
- **The text, on `main`.** R-OFF-08 says the L8 hand is drawn unweighted, because no Keeper but the Patron holds Favor before the commitment. No behaviour changes.
- **The seats, on `dev/s2`: C.** R-KEE-23 keeps the seat for a Keeper a tool run's launch names. A run started with every S2 Keeper open for a person (one new launch argument, used by the S2 kit's launcher) reserves none: the Patron hand is `C.OFFER.KEEPER_CHOICES` Keepers drawn unweighted from the six and shown in Path order, and the L8 hand the same number drawn from the five others.
- Why C and not B: the Shrine on `dev/s3` is where a player chooses among six. A six-card hand on `dev/s2` would be built for the S2 exit's three runs and then replaced. C is `dev/s3`'s own draw, one stage earlier.

### Hypothesis
We believe drawing every seat moves the Keepers a person's S2 run can offer as Patron from three of six, all S2, to each of the six in half the hands, because no seat is reserved; measured by an offers test over a thousand seeds (each Keeper's share of Patron hands between 0.45 and 0.55, of L8 hands between 0.55 and 0.65) and by the Patrons offered and taken in the S2 session's logs; reverted to option B if two validators at that session ask for a Keeper they were not offered.

### Impact (dependency sweep)
- Rules replaced: R-OFF-08's "drawn by Favor weight" (`main`); R-KEE-23's seat sentence and its test (`dev/s2`).
- Constants: none. `C.OFFER.KEEPER_CHOICES` stays 3.
- Files / tests: `02_Systems/offers.md`; on `dev/s2` `02_Systems/keepers.md`, `Offers/DTOfferGenerator.cpp` (one block), the launch argument's reader, the offers and Keeper tests.
- Readings that move: none of the tools'. A tool run is unchanged, so no bot hash moves.

### Swap-out point
`dev/s3` landing on `main`: its hub chooses the Patron.

## Draft 6. The Trait's two-card prerequisite

- **Class:** a text correction (`offers.md`); no rule changes
- **Lowest branch:** `main`. **Before session A:** yes, documents only

### Context
- R-KEE-04: "A Trait additionally needs `C.KEEPER.TRAIT_PREREQ_CARDS` owned cards of its archetype, the Key counted." The value is 2, GATED(S1): the Key and one Amp. T-KEE-04 pins it.
- `offers.md` names only the Key: R-OFF-03 (d), R-OFF-07 ("an archetype's amplifiers and trait join the pool when its key is taken") and R-OFF-11 (c). Read alone, `offers.md` says a Trait can be offered on the Key.
- The source: a capstone is "offered only once the player owns 2 cards of that branch"; in another place "at Rare+"; and capstones "unlock through Discoveries" in the meta layer.
- No blessing carries a rarity that changes it (R-OFF-05), so "Rare" has nothing to mean on a Trait. On `dev/s3` no Discovery opens a card; a Trait opens with its archetype's Tree node, marked invented, and `meta.md` Open asks whether capstones go back to a Discovery.

### Options
- 1 — Keep two cards, the Key counted, and make `offers.md` say it.
- 2 — Add "at Rare+": needs a blessing rarity that means something (draft 7 keeps it off).
- 3 — Gate Traits by a Discovery: a meta question, on `dev/s3`.

### Decision (recommended)
Option 1. R-KEE-04 stands. `offers.md` R-OFF-03 (d), R-OFF-07 and R-OFF-11 (c), and the lines of T-OFF-03 and T-OFF-07, name R-KEE-04's count beside the Key. Option 3 stays with `meta.md`'s Open line until people have read the Tree.

The constant is GATED(S1) and has never been read on people. `Tools/session_read.py` should print, for each run, the level and time a Trait was first offered and first taken; the S1 gate then has something to read it by. That line is a follow-up to `s1/session-a-readiness`, not part of this draft.

### Impact (dependency sweep)
- Rules replaced: none. Three sentences and two test lines of `offers.md` gain a clause.
- Constants, code, readings: none.

## Draft 7. Rarity on chest and Reserve cards

- **Class:** RULE (`offers.md` R-OFF-14 against R-OFF-05)
- **Lowest branch:** `main`. **Before session A:** the Reserve sentence yes (text); the lift no (`Source`)

### Context
- R-OFF-05: "In S1 only weapon upgrades carry magnitude by rarity ... other kinds show Common until their spec defines a magnitude." A Rare upgrade gives `C.OFFER.RARE_UPGRADE_LEVELS` levels.
- R-OFF-14: a chest hand has "rarity at least Rare".
- The code follows R-OFF-14 for every kind but two (`Offers/DTOfferGenerator.cpp`, `FinalRarity`): a new weapon, a modifier and a blessing are lifted to Rare in a chest; a Reserve card and a Keeper card are always Common. T-OFF-14's test exempts Reserve cards; no spec line does.
- What a player sees: a chest's modifier, blessing or new weapon shows two pips and "Rare" and the same text as a Common one. A chest's Reserve card shows "Common" in a hand the rule calls at least Rare.
- The source: a chest is "a guaranteed Rare+ card choice"; its balance notes keep blessing and modifier rarity off, "a design call, not balance".

### Options
- 1 — A mark only where it changes the card: in a chest a weapon upgrade is at least Rare; every other kind keeps what R-OFF-05 gives it; a Reserve card is Common everywhere.
- 2 — Give the other kinds a magnitude at Rare (the source's bid draft: about half again). New numbers on every card kind.
- 3 — Keep the mark as it is and write the Reserve exemption into R-OFF-14.

### Decision (recommended)
Option 1. A label that changes nothing teaches a player that labels are noise, and then the upgrade's real "Rare" reads as noise too. R-OFF-05 already says it; R-OFF-14 contradicts it.

- Now, text only: R-OFF-14 says a Reserve card in a chest hand is Common, as the code and the test have it.
- After session A, the lift: R-OFF-14 reads "a weapon upgrade in a chest hand is at least Rare". The other kinds lose the mark in chests. A chest still gives a hand of its own and its restore (D-046).

### Hypothesis
We believe marking only upgrades moves the Rare cards in chest hands whose text equals a Common card's from every one that is not an upgrade to none, because no other kind has a magnitude; measured by the offers test (in a thousand chest hands every Rare or Epic card is a weapon upgrade) and at session B by the kit's unreadable-cards row; reverted for a kind on the day its spec gives it a magnitude by rarity.

### Impact (dependency sweep)
- Rules replaced: R-OFF-14's "rarity at least Rare"; the line of T-OFF-14.
- Constants: none. Files: `02_Systems/offers.md`, `Offers/DTOfferGenerator.cpp` (`FinalRarity`), the offers tests.
- Readings that move: none expected (the roll is unchanged, only its use). Not verified that no bot score reads a card's rarity; seed 9107 says.

### Swap-out point
A spec that defines a magnitude by rarity for another kind.

## Draft 8. What a fresh save offers

- **Class:** RULE (`dev/s3` `meta.md` R-MET-25 and R-MET-22; the Tree catalog)
- **Lowest branch:** `dev/s3`. **Before session A:** it may; it is needed only before the S3 kit is built (D-049)

### Context
- D-049 left open "whether a fresh save should open one archetype's Amps per S1 Keeper", to be decided "before the S3 kit is built, with the reading in `S3_tree.md` in front of it".
- Today a fresh save offers each S1 Keeper its three Keys of twelve cards. The Amps and Traits open at two Tree nodes a Keeper: the first, at 300, opens two archetypes; the second, at 500, the third. Prices are invented; a median death pays 837 to 915.
- `dev/s3` `Validation/S3_tree.md`, nine S1 arms, 90 runs each way, bot v2 at skill 1.0: full pool 62 reach and 47 wins, fresh pool 79 and 69. Keeper share full against fresh: Storm 0.663 and 0.467, Forge 0.803 and 0.540, Tide 0.497 and 0.402. The pillar is `C.POWER.KEEPER_SHARE_L16`. The note does not say which instrument gave the share, and it predates D-047.
- The S3 exit asks three people on fresh saves, three runs each, to name their build (D-049).
- The source's starter pool is the entry card of every branch: Keys only, as today.

### Options
- a — Keep Keys only.
- b — A fresh save also opens the two Amps and the Trait of one archetype for each S1 Keeper.
- c — A fresh save opens the first Amp of every archetype; Traits stay shut.
- d — The first run's end opens the Patron's archetype.

### Decision (recommended)
Option b, with Arc Grid for Storm, Hammer for Forge and Frost for Tide.

- Why open anything. An archetype is a Key, two Amps and a Trait. On a Keys-only pool no archetype can grow past its first card, so the first run, the one that decides whether a person asks for another, cannot show the thing the exit asks them to name.
- Why these three. Each is its Keeper's plainest picture (a chain, a hammer, a freeze), and by the bot's bound none is its Keeper's weakest arm.
- The Tree keeps its shape and its prices: each Keeper's first node then opens one archetype (Tempest, Furnace, Whirlpool) and its second the last (Downpour, Slam, Stillness). Kit question 5 still reads the prices.
- Pillar 5 holds: the pool widens; no number rises.
- The other error is the cheaper one. If b is wrong, the first purchase feels smaller. If a is wrong, the one S3 session is spent on a first run that shows no build.

### Hypothesis
We believe opening one archetype's three cards per S1 Keeper on a fresh save moves the cards of one archetype a first run can hold from one to four, and a fresh pool's Keeper share on those three arms from 0.40 to 0.54 toward the pillar, because the bot's fresh hands hold no Amp to take; measured by `Tools/BotRuns.ps1 -FreshPool` on the nine arms (the share by a named instrument; `-Check T-OFF-15`, which D-024 requires for any change to the first-run list) and at the S3 session by kit question 2 after the first run and question 5; reverted if two of three validators buy no blessing node in three runs and say nothing was worth buying.

### Impact (dependency sweep)
- Rules replaced: the Amp and Trait row of R-MET-25's table; R-MET-22's node lists.
- Constants: none. Files: `09_Catalog/tree.csv` (three `unlocks` cells), the first-run list and its generator, `02_Systems/meta.md`, the Meta pool tests, `Validation/S3_kit.md` (the fresh-save check).
- Readings that move: every fresh-pool reading; MetaCheck's pool windows; the `firstrun` gate step, which wants six of six runs at the boss and may not hold with Amps in the hands. The job reads that step first and stops if it fails.

### Swap-out point
The S3 session.

## Plans (planned, not started)

### P1. A cast-scene route with an aimed dash
- The finding (`dev/s2` `Validation/S2_trace.md` §4): the route now dashes inside the cast, and its dash "runs along the crowd, not into it"; 86 of 943 damage was Keeper damage where bot runs deal 1 400 to 2 900 a minute. The cast's dash window is also Forge's five seconds, whatever the Keeper.
- It is not only Vector's. Slam's variant slams where a dash lands, so the S1 rows read low the same way: the lowest branch is `main`, it touches `Source/Dusktracer/Bot`, and it waits for session A (D-055 item 9).
- The change: a script key `cast_dash_aim` (`crowd` or absent). With it, a dash step inside the cast points the move at the densest group of enemies within the dash's reach. The cast window comes from the casting Keeper's own duration.
- Files: `Bot/DTProbeScript`, `Bot/DTBotBrain`, `Bot/DTBotSubsystem.cpp`, `Tools/probe.py`, `Tools/tools_test.py`, the Bot tests, the line of T-KEE-15, `10_Tuning.md §3`, and a number-log row written before the reading (the scene's parameters are the method).
- Done when: seed 9107 does not move (the route acts only in a probe scene); T-KEE-15 is reread with the old rows beside the new; Vector's no-layer row is compared with the bot's rate in play.

### P2. An S2 kit
- Written on `dev/s2` after the S1 exit, from `S1_kit.md` as `s1/session-a-readiness` leaves it. `12_Production.md`: one sitting, three people, full runs on `dev/s2`'s package with S2 content unlocked.
- It needs first: draft 5's launch argument; and whether a Shipping build can open S2 content at all (on `main` a Shipping validator cannot hold a non-starter weapon, D-053). Not verified on `dev/s2`.
- Its run table adds "first attempt at S2 content" (the death histogram's clauses count first attempts) and the Patron offered and taken.
- Questions it must ask, beyond the S1 list:
  - Time Dilation. The scale stands at 1.0, so the cast slows nothing; 0.3 and 0.7 each failed on marginal damage, a reading that "cannot see" time to read and move. For validators: "When you used that power, what did it do?" For the owner, on the proof build with `dt.Exp.DilationScale` at 1.0, 0.7 and 0.3: which scale reads as time to think.
  - Slipstream Drive's leak (`leak_per_s` 0.15, invented): "Did you notice your speed building? What happened when you stopped?"
  - Reforging (`reforging.md` HUMAN line): at L16, for each of the three cards, what changed on screen within two seconds of the pick.
  - An elite: told apart from its kind, a fight or a wall.
  - A duo reaction: did they notice one, and can they say which two Keepers made it.
- It also lists the HUMAN lines it does not ask, as the S1 kit now does.

### P3. Two questions owed to `S3_kit.md` (`dev/s3`)
- The run-over guard (R-SET-21, one second, invented, "keep unless a human reads it as a lag"). A watch row, since a question would plant it: "At the run-over screen: a key that did nothing, or a remark that it was slow (run, their words)". Then one question at the end: "When a run ended, did the screen respond when you wanted it to?"
- Fresh against full. The kit plays fresh saves only, so a validator cannot compare. Two readings instead: after question 5, for a validator who bought a blessing node, "After you bought it, did your next run feel stronger, weaker or just different?"; and in the owner's rehearsal of the kit, one run on a fresh save and one on a full profile, with the owner's word on which felt stronger. The second goes under "Not proved by this kit" until the rehearsal is held.
- Line 5 of the kit still says the pass rule has "no definition"; D-049 gave it one. Fixed in the same edit.

### P4. Readings the probe cannot take today
- A scene that credits control: the hero can be hit, and a cast is read as damage avoided. It would read Tide's hold, Frost's package gain of 0.79 and Quicksilver's time scale. A tool and `Source/Dusktracer/Bot` job; planned after P1.
- The S2 exit's probe rows with duo reactions on, beside the rows without. `s2/fit-and-unlock` turns duos on in the arm batteries and keeps the probe scenes as they were; turning them on there is a change of method and gets its own number-log row.
