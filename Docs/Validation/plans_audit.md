# Plans audit (2026-11-10)

An audit of every plan in the repository, software and game, asked for by the owner. Read-only: nothing was built, launched, tuned or changed. It is one reading on one day; the branches moved while it ran (section 8).

Class: none (a reading). Layer: the plans themselves. Replaces: nothing.

## 1. Verdict

The plans are well made as documents and out of order as a schedule.

- **As documents they hold up.** Constants and YAML mirror exactly, every rule has a same-numbered test, hypotheses were logged before their readings, failed tunes were really reverted, and almost every note says what it did not verify. Section 7 lists this.
- **As a schedule they have not followed their own first rule.** The method says humans are the critical path and are booked before content (`Harness/H01` P4). Four stages now stand on no outside player and one owner run. Each early start was authorised on the record and none waives an exit, but the exits are no closer.
- **The gate everything waits behind cannot pass as written.** Two of the five clauses of the S1 pass rule fail by construction, whatever the validators do (section 3). Booking O-008 today would buy a FAIL that says nothing about the game.

The cheapest useful move is small: fix the S1 exit rule so it can be read, then put people in front of `main`.

## 2. How this was read

Seven slice audits ran in parallel (core design specs, balance evidence, S2, S3 and later stages, presentation and UX, the technical plan against the code, decision records); the lead read the method, the roadmap, STATE and the stage decisions. Each slice cited both sides of a claim by file and line. Items tagged `[R]` were rechecked by the lead against the files; the rest stand on a slice's citations.

Severity: **critical** = the plan cannot work as written, or would mislead a decision. **major** = a real contradiction, gap or stale claim someone will trip on. Minor items are in section 6.

Branch names qualify a path that is not on `main` (`dev/s2:`, `dev/s3:`, `dev/s4:`).

## 3. The schedule and the method

**3.1 critical. Four stages rest on no human reading.** `[R]`
- S0 passed 2026-10-30 (D-012). S1 was feature-complete the same day. The first `dev/s2` commit is 2026-10-31 06:50 and no decision records the S2 start. S3 started that day (`dev/s3:Docs/13_Decisions/D-022.md`). S4 started 2026-11-10 22:36 (`dev/s4:Docs/13_Decisions/D-043.md`), while this audit ran.
- O-008 (book three people) has been open and unchanged since 2026-10-30. Its context line still says nothing playable exists.
- `12_Production §2` lists "meta, more Keepers, art fidelity" under S1's not-before-exit. All three have now been crossed, each by its own record.
- The handoff calls the previous project's roadmap "a calendar plan executed in 48 h" (`Handoff_Dusktracer/game/G00_START_HERE.md` §6). This one has done the same in five working days: 143 commits on 10-30, 195 on 10-31.
- Rework exposure grows with each stage. D-043 names it honestly: one human reading that moves the camera frame, a palette token or a readability rule rebinds the art above it.

**3.2 critical. The S1 exit cannot pass as written.** `[R]`
- Clause 3, damage band. `C.DMG_TAKEN.BAND_MIN10_13` is 30 to 50 % of max HP per minute over 180 s, so 90 to 150 % of `C.HERO.MAX_HP`, and S1 has no heal. A run alive at minute 13 has lost under 100 % in total. `02_Systems/director.md:119` says the band and reaching the boss "cannot both hold"; `Validation/S1_kit.md:51` still requires it.
- Clause 4, Keeper share. `C.POWER.KEEPER_SHARE_L16` has two instruments that disagree arm by arm. Ablation (the test line in `keepers.md:133`) reads Forge/Furnace 0.791 and Storm/Downpour 0.461; attribution (the rule in `keepers.md:70` and the kit at `S1_kit.md:52`) reads 0.45 and 0.59 on the same runs (`Validation/S1_probes.md:39-49`). `Validation/S1_keeper_power.md:170` says the formula cannot pass Storm. The gate tool ends in `return 0  # stub` (`Tools/power_split.py:135`).
- The measurement has no home. `10_Tuning.md` is the named dependent of three CORE shape constants (`08_Constants.md:25,28,29`) and exists on no branch. `Harness/H07 §5` wants each measurement defined there first.
- `C.TTK.SWARM_STEERED_MAX` is a Vision success criterion with no test and no kit clause, and its only reading fails on all three winning arms (0.383, 0.700, 1.333 s against 0.3; `Validation/S1_run_telemetry.md:39-47`).
- The kit and the tool compute clause 3 differently: a median over runs that reached minute 13 (`S1_kit.md:51`) against a per-run mean over time alive (`Tools/pressure.py:5-8`).
- The heal that would resolve clause 3 is a SYSTEM candidate named in notes only; `13_Decisions/OPEN.md` has no entry for it.

**3.3 critical. The owner window is gone, and owner-territory calls were made without it.** `[R]` for D-009, D-022, D-034, D-043
- The owner made 7 of 43 decisions; the last is D-017 on 2026-10-30. D-009 "replaces the daily owner window"; `Harness/H08 §2` says a full-auto mandate does not remove it, and that file is unchanged.
- Calls the harness keeps with the owner, made by the scheduler and shown only in STATE prose: the S2, S3 and S4 starts; the S3 pass rule (D-034); the camera frame (D-036); what counts as a win (D-041); cutting stat progression from meta (D-023); first-run locks and invented prices (D-024); what cards say (D-030); the run-start Patron pick outside `C.DECISIONS.PER_RUN` (D-013).
- D-034 narrows a rule the designer wrote. The source reads "continue … if 2 of 3 hold": two testers ask for another run, two can name their build, the designer still wants to play (`Handoff_Dusktracer/game/reference/original_docs/03_Roadmap.md:150`). D-034 makes both tester rows mandatory and drops the designer's, and its context says the docs list no criteria.
- Fourteen applied decisions can only be confirmed or reverted by a human session that is not booked.
- Thirteen "held for a human reading" items and two CORE targets recorded as unreachable sit in STATE with no ID, default or date.

**3.4 major. The method's own limits are not being read.**
- GATED debt is about 250 constants on `main` and 321 on `dev/s3` against `C.GATED.DEBT_MAX` 5. `[R]`
- `Dashboard/complexity_dashboard.csv` is a header with no rows; the weekly self-audit (`Harness/H09 §4`) has never run. `[R]`
- The system-budget caps `Harness/H04 §4` puts in the constants file do not exist (`08_Constants.md:84-91` holds four harness caps, none for systems, strings or UI kinds). `[R]`
- The two-speed rule (`H04 §6`) allows one rule change per system per validation cycle. With no cycle yet, `keepers.md` rules were changed by seven decisions, and one Quicksilver rule three times between 14:43 and 17:55 on 2026-11-10 (D-038, D-039, D-040).
- Nine RULE or SYSTEM records carry no hypothesis line. Missed hypotheses were kept: D-019 ("applied, with the logged cast clause not met"), three rows on `dev/s2`, and Time Dilation, which failed twice, was set to scale 1.0 and still ships its rule, test, constant and cvar with no decision record. `[R]` for D-019 and Time Dilation
- No stage has a budget or a consolidation window, though `12_Production §3` reserves 15 % of each.

**3.5 major. Bots are the only oracle and they are not calibrated.**
- About 35 of 43 tuning hypotheses hinge on a bot survival clause. On the owner's own path the bot loses about 7 % HP; the owner lost 104.2 % and died at 563 s (`Validation/S1_owner_run.md`).
- The number log's evidence tags mean the reverse of `Harness/H07 §6`: `[M]` marks bot readings, `[H]` marks hand guesses, `[G]` is undefined. No row on any branch carries human evidence. `[R]`
- Eight constants have three or more rows and no oscillation review. Downpour Key `dmg` moved seven times in both directions for two clauses of one test.
- One place the human reading supports the bots: pickup. The owner read 0.733 and the bots 0.756 against `C.XP.REFERENCE_PICKUP_RATIO` 0.51, a value measured on the previous game.

**3.6 major. Four stacked branches with no landing plan.** `[R]` for sizes and counts
- `main` to `dev/s2` is 207 files and about 25,800 added lines; `dev/s2` to `dev/s3` is 167 files and about 16,700 more. No document says when or how either lands. Landing `dev/s2` changes S1 runs: elite affixes are on by default.
- No branch holds every decision: `main` has 28 files, `dev/s2` 32, `dev/s3` 42, and D-043 is on `dev/s4` only. STATE on `main` cites records `main` does not have. The "next free number" line on `dev/s2` and `dev/s3` says D-041 while D-041 exists there.
- Fixes do not flow down. Gate hardening is on `dev/s3` only (section 4.3). The zh audit is on `dev/s2` and up. Two S2 sim defects were fixed on `dev/s3` only, so the S2 exit battery reads a build with both.
- Some IDs mean different things by branch: the tests numbered 10 for modifiers and 5 for weapons name different tests on `main` and `dev/s2`.
- `dev/s4` was cut four commits before the save lock (D-042) reached `dev/s3`.
- STATE on `main` contradicts itself: line 4 says four stages run; queue item 5 says S4 "queues behind the exit tests". `[R]`

## 4. The technical plan against the code

**4.1 critical. One machine, and its CPU is known to fault.**
STATE says the CPU "threw random faults under heavy parallel load; rerun an odd failure once". Every build, gate, bot battery, perf reading and validator package comes from that PC. A wrong PASS or a miscompiled package cannot be told from a real one, and "rerun once" trains agents to discount real intermittent bugs. `07_Technical §8` does not list it. Only the owner can fix the hardware.

**4.2 major. The test floor does not guard the suite.** `[R]`
`C.GATE.TEST_FLOOR` is 30 on `main` against 257 tests, and 30 on `dev/s2` against several hundred; `dev/s3` holds 526. `11_Quality §1` says the floor rises in the commit that adds tests. On two branches a merge could lose most of the suite and pass.

**4.3 major. The gate is weaker than the plan says.** `[R]` for the first three
- On `main` a missing Python gate script reads NOT RUN and the gate passes (`Tools/Gate.ps1:33`); `dev/s3` fails it.
- The `scope` step is hard-coded NOT RUN (`Tools/Gate.ps1:165`) while agents are in flight.
- There is no pipeline and no scheduled job. `Harness/H07 §1` says "identical locally and in the pipeline"; `12_Production §2` says "ablation gates nightly".
- Determinism is a hand reading: no golden `input_hash` is committed, the Shipping-against-Development check is not a gate step, and nothing pins the compiler.
- `07_Technical §8` says the secret scan runs before every push; the only pre-push hook is the LFS one.
- `11_Quality §1` lists neither `catalog` nor `locbuild`, which run, and still says `smoke` is NOT RUN.

**4.4 major. The architecture doc and two non-negotiables describe code that does not exist.** `[R]`
`UDTEffect`, `UDTEffectManager`, `UDTOfferGenerator`, `UDTKeeperDef`, any data asset, and UMG or CommonUI appear in no source file; `FDTStatSet` appears in comments only. `07_Technical §3` names folders the tree does not have. `AGENTS.md` non-negotiables 3 and 4 still name them. Under "docs win over code" an agent is pointed at a design D-014 replaced.

**4.5 major. Per-player state holds in the sim and nowhere above it.** `[R]` for the first two
- The game mode starts every run with one player (`Source/Dusktracer/Game/DTGameMode.cpp:66`) and the camera serves player 0 (`:165`). A slice counted about 20 solo shortcuts on `main` and 45 on `dev/s3`, mostly UI, hub and menus.
- The rule's test checks struct routing; no test runs two players to the boss.
- `coop.md` is cited by six specs and was never written. S3 chose one shared profile for a team against the reference's per-player design, with no decision.
- The S6 exit names a `SoloParity` golden that exists nowhere.

**4.6 major. Performance is one GPU number on one machine.** `[R]` for the lock
- Game thread and GPU are read on an i9-13900KF and an RTX 4080; the 4070 budget is that reading times a proxy factor from raster benchmarks. There is no 3060 or Deck budget, no render-thread reading and no hitch metric, and S4 ("perf tiers") has started.
- STATE's figures were "read on a busy machine"; `11_Quality §5` discards such readings. The lock cannot see contention: `gpu.lock` sits in each worktree's own `Saved` (`Tools/Perf.ps1:30,55`), so 75 worktrees hold 75 locks.
- A slow PC plays in slow motion and nothing records it. `C.SIM.MAX_SUBSTEPS` is 2, dropped steps are counted (`Source/Dusktracer/Core/DTFixedStep.h:25`) and telemetry never writes the count. A validator on weak hardware would read normal while playing an easier game.

**4.7 major. Saves (`dev/s3`): right fundamentals, thin proof, silent failure.**
- Version, checksum, validator, atomic replace and set-aside are all present.
- Every failure is a log line only (`dev/s3:Source/Dusktracer/Meta/DTMetaSession.cpp:105,123,128,142`), and Shipping writes no log. A corrupt save starts the player empty with no message. `[R]`
- Not proved: the atomic path, a kill mid-write, any real migration (the version is 1 and no step exists), Shipping's real save path, a person's bank.
- No plan for a backup generation, downgrade, reset or cloud sync.

## 5. The game design

**5.1 critical. The damage band and a game with no heal.** Section 3.2, clause 3. This is a shape-constant call: add a heal source, or re-derive the band for a no-heal game. It is the owner's.

**5.2 critical. Drop-in Rushers mostly cannot reach the hero.** `[R]`
An arriving Rusher winds up where it lands and charges 1500 uu/s for 0.65 s: 975 uu (`08_Constants.md:127-128`). The director lands it 1000 to 1200 uu from the hero (`:320-321`). With both radii that reaches about 1055 uu, so about a quarter of landings can touch a hero who stands still. Rushers are 70 % of drop-ins (`09_Catalog/timeline_arena1.csv:6`). The kill-horizon guarantee counts each as a threat action, and a balance note read the zero damage as the skill check working. The owner's run took no Rusher damage.

**5.3 critical. Pillar 3 has no agreed measure, and part of the Intercession band is unpassable.** Section 3.2, clause 4. In addition Tide's cast deals no damage, so it "has no damage band" (`Validation/S1_keeper_power.md:169`); on `dev/s2` the Intercession row reads 46 of 282 in band, and Shade and Quicksilver fail it by construction. The rule fixes are listed as "candidates, not taken" and parked for a human reading that cannot move a bot probe.

**5.4 major. "Stat-only blessings never exist" holds by naming only.** `[R]`
The loader rejects hero and weapon stat names. At least six of the 18 Amp cards are plain increments on a flag's number (`09_Catalog/expression.csv`): Whirlpool Amp2 `crush_dmg=+0.05`, Hammer Amp2 `radius_scale=+0.30`, Furnace Amp1 `burn_tick_rate=+0.25`.

**5.5 major. Threats that are not skill checks.**
- Three of the six S1 threat beats are elites with HP times five and no added check, against pillar 1. Affixes fix this on `dev/s2` only; validators play `main`.
- Hammer's Key does nothing on Orbital Drones: its fallback is "+drone contact area" (`09_Catalog/flag_compat.csv:5`) and drones deal no contact damage (`02_Systems/weapons.md:35`). `[R]`
- The dash band has no specified demand before the boss; the owner dashed 1.81 times a minute.

**5.6 major. The boss.** `[R]` for the rule text
- The Warden's HP is set once from damage dealt in the first seconds of the fight (`02_Systems/boss.md:15`). Holding fire lowers it; an Intercession in the window inflates it.
- `C.RUN.BOSS_MAX` is called the fight cap, and no rule ends or bounds the fight there.
- Neither `director.md` nor `boss.md` owns the field left alive at boss time. Up to the enemy cap persists, and auto-aim shoots it during the calibration window.

**5.7 major. Run shape.**
- The level anchors pass a test that reads the previous game's income. On S1's own timeline the model misses L16 by 82 s, and real pickup is about 0.74 against the 0.51 the curve assumes; the two errors cancel by accident.
- The pause limit is written for a bot run, and bots read 0. The owner read 20.6 s per sim minute against `C.OFFER.PAUSED_S_PER_MIN_MAX` 15. Twenty-eight five-card hands make a "15:00" run about 20 minutes of wall time before the boss.
- The decision count fails by one run in about 200, because the test is a universal whose tolerance equals the current spread.

**5.8 major. S2 as built (`dev/s2`).** `[R]` for the first four
- Keepers 4 to 6 have no fit data. The `fits` columns name only Storm, Forge and Tide, so with only S2 Keepers owned no weapon is weighted up and every modifier is weighted down (`dev/s2:Source/Dusktracer/Offers/DTOfferGenerator.cpp:507,524`). Every S2 bot reading carries this.
- "All Keepers" never offers all six. `C.OFFER.KEEPER_CHOICES` stays 3 and an unlocked Keeper always takes a seat first, a rule the spec marks invented; three unlocks remove every S1 Patron from the hand.
- The exit battery does not exercise three of the eight build items: `-Unlock S2` expands to weapons and modifiers only, so duo reactions are never on, and elites and Reforging are barely touched.
- `reforging.md` says a card changes how a build kills, "not how much", and is "never a stat line"; its rule 18 then requires output 1.3 to 1.8 times after the pick. The reading is 1.00.
- The exit reads 1 PASS, 3 FAIL, 2 NOT READ. The three FAIL rows have no route to PASS (5.3). The two human rows point at O-008, which plays `main`, where no S2 content exists. There is no S2 kit and the launcher passes no unlock.

**5.9 major. S3 as built (`dev/s3`).** `[R]` for the first three
- A fresh save shuts every Amp and Trait. The S3 exit asks fresh validators to name their archetype on the thinnest pool the game has, with Keeper share at 0.40 to 0.54 against the 0.60 pillar (`dev/s3:Docs/Validation/S3_tree.md:61-66`).
- Unlocking makes the bot weaker: 69 of 90 wins on the first-run pool against 47 on the full pool. The note faces it; D-023 and D-024 still cite it as support.
- Pillar 5 says meta never raises numbers. Three Trunk nodes raise reroll, banish and lock by one each (`dev/s3:Docs/09_Catalog/tree.csv:2-4`). D-032 rejected a fourth Patron seat on exactly that ground.
- "Narrative v0.1" is a Codex page. The 24 Keeper lines the spec names as the channel for two story layers have no reader, and a player is never shown what opens a Discovery.
- A validator's run cannot be replayed from what the kit collects: `run_start` records no pool, unlock or tool state.
- The review record (56 fixed, 30 refuted, 51 deferred) overstates its rechecking. Six sampled refutations are contradicted by the code, and the deferred list has no owner or stage; three deferred items should precede a kit build.
- `findings_S3.md` is an empty template and says so plainly. D-005's wording ("until it exists") is met by the letter.

**5.10 major. Presentation and UX.**
- **critical, arithmetic `[R]`.** The contrast design passes only on a two-tone floor. From the repo's own tones the seven elements all clear `C.PRE.CONTRAST_MIN` only where ground luminance is at most 0.037 or between 0.106 and 0.16; any mid-tone fails the telegraph both ways. The gate reads a flat grey-box floor and cannot see it. Margins today are 3.27 and 3.18. S4's exit needs both weathers; one exists, and the other is defined as soft shadow.
- Audio has no plan beyond the words "audio signatures": no spec, constant, budget or generator, and the blueprint's full-audio line fell off the S7 row. Every human reading for S1 to S3 will be taken silent.
- Seven tests of the binding readability contract are still PLANNED. The contrast rule and its test measure different things; against the bright-ground token three elements read under 3.
- `06_UX.md` is a camera page. The HUD hero block, run timer and boss bar have no rule. There is no onboarding: the HUD labels Dash and Intercession without a key (`Content/DT/Text/ST_UI.csv:6-7`), and validators get one sentence. `[R]`
- The S1 zh validator will play un-audited Chinese. The audit fixed 121 cells from `dev/s2` up; `main` still uses the retired name for the Warden in three rows. `[R]`
- D-036 reverts the camera "if any" validator reports a hit from an unseen source, and the kit asks nothing about it. Its three held items have no owner.
- About 135 zh rows wait for a native reader nobody has named.

**5.11 major. Later stages.**
- S4's size, "about 370 session-days", is a floor in a unit that converts to no cost or calendar; the human path is named critical and given no figure; the reference bar is undefined and the art book is outside source control. D-043's first wave is 53 of 212 rows.
- S5, S6 and S7 are one table row each. They are narrower than the binding blueprint (acts, loadouts, Steam, full audio, a 3D hub) and no decision records the cuts. `coop.md` and `modes.md`, the named homes of S6, were never written.
- Later-content rules already ship as text on `dev/s3`, one of them (survive a lethal hit once a run) against D-023.

## 6. Minor

- 15 `PLANNED` markers on `main` have outlived their stage (2 for S0, 12 for S1); the lint treats PLANNED as a warning for ever. `[R]`
- No gameplay code has compiled on macOS since 2026-10-28, while `AGENTS.md` says the code is kept portable.
- `11_Quality §3` promises telemetry the code does not write: per-effect counts, healing, a declined hand.
- The golden-image gate is a whole-frame mean; the camera move scored 1.08 against a limit of 12 and passed.
- No licence log exists though five binary assets are tracked; no font is chosen, so every zh layout proof is on the engine fallback face.
- Manual aim is promised as a saved option and cannot be set in a Shipping build.
- The Glossary is still the day-1 skeleton.
- STATE meets its 100-line cap with lines up to 1,900 characters that carry history, against `Harness/H05` C1.
- 75 worktrees and about 160 local branches are not pruned; the gate has no disk preflight.
- Stale Open items: `offers.md:128`, `weapons.md:118`, `boss.md:92,94`, `bullets.md:74,77`, `enemies.md:139`.

Two public-repository hygiene items were found and reported to the owner outside this note.

## 7. What is sound

- `08_Constants.md` and `constants.yaml` mirror exactly, 319 keys, and no S1 number differs between branches.
- Every rule has a same-numbered test in every spec.
- Hypotheses were committed "pending" before their verdicts. Failed tunes were reverted and the catalog matches the log's net state. No criterion was loosened to pass.
- Every bot note says a bot reading is a bound; the owner run is labelled n = 1; the exit tools refuse to judge human rows.
- No early start waives an exit, and D-022's guardrail holds in git: no meta or hub code is on `main` or `dev/s2`.
- RNG discipline, no actor per bullet or enemy, no run-state singleton, an owner on every event, debug commands out of Shipping.
- The seed rule, the hidden-desktop launcher and the no-window gate are careful work.
- Decision records from D-032 on have options, a full hypothesis, a sweep and a "Not verified" list.
- D-018 was reverted the right way: rule, constant, test and text all gone.

## 8. Not verified

- Nothing was run. Every PASS, count and reading above is quoted from the docs or read from source.
- The exports were `dev/s2` at f7e9a98 and `dev/s3` at 9fafa8f. During the audit `main` moved to 550eb38 (O-010), `dev/s4` was created, and D-042 landed on `dev/s3`.
- The art book and the previous project's folder are outside the repo.
- Chinese wording was counted, not reviewed.
- What the owner was told or agreed to in chat is not in the repo and was not read.
- Items without `[R]` were not rechecked by the lead.

## 9. What each reader can do with this

**Only the owner:**
1. Answer O-010: does S4 continue ahead of three open exits.
2. Decide the heal against the damage band (5.1).
3. Review the owner-territory decisions of 3.3, D-034 first.
4. Play two runs of `main` talking aloud, one taking weapons and one passing them. About 40 minutes; it settles whether 104 % HP lost is the path or the pilot, and it reads D-036's held items.
5. Book O-008 once 3.2 is fixed, and name a native zh reader.
6. The CPU (4.1); real 4070, 3060 and Deck hardware (4.6); O-009.

**The scheduler, in this order, none of it new content:**
1. Make the S1 exit decidable: one Keeper-share instrument, one statistic in the kit and the tool, a clause for swarm kill time, a short `10_Tuning.md`.
2. Bring `main` up to what validators need: the gate hardening and test floor (4.2, 4.3), the zh cells, key labels on the HUD, a kit question for D-036, the dropped-step count in `run_end`, a decision on S1 elites and on the Rusher landing ring.
3. Put every decision file on `main`, post one OPEN digest of the calls in 3.3, and give the held list IDs.
4. Stop tuning on a bot survival clause until a skill row is calibrated against people; fix the evidence tags.
5. Reconcile `07_Technical`, `AGENTS.md` and `11_Quality §1` with the code and the gate.
6. S2: fit data, the seat rule, the exit battery's coverage, an S2 kit. S3: the fresh-pool question, a save-failure line on the hub, an owner rehearsal of the kit build.
