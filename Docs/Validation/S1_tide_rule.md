# S1 Tide rule and the round 1 numbers (2026-10-31)

Order: D-019 (RULE), then the six numbers of `S1_keeper_power.md` §4 round 1 (TUNE). Everything is Bot v1 at skill 1.0, headless: a bound, not a human reading (O-008). Nothing here is called balanced. T-KEE-15, T-KEE-18, `C.INTERCESSION.BAND`, `C.POWER.*` and `C.KEEPER.PACKAGE_GAIN_BAND` are unchanged. Hypotheses are in `number_change_log.csv`, written before each measurement.

Before = `S1_keeper_power.md` §5 final tree, re-run here: the 19 arms reproduce that table exactly, T-DIR-01 588.6 s, T-OFF-15 195/196 (`Saved/BotRuns/control`). After = this branch (`Saved/BotRuns/rule`, `Saved/Probes/ik_rule`, `ab_rule`, 40× field).

## 1. D-019: Frost and Stillness drop the line
**Source check:** confirmed. Handoff `intercession_variants_spec.md` §2, Handoff `ST_Ultimates` and `ST_Keepers` give Frost and Stillness no pull; Whirlpool's text is the only one with one. Citations in D-019.

**Verdict: applied, one logged clause missed.** Test `Dusktracer.Keepers.Param.TideLineNeedsAPull`, seen failing first.

| Logged clause | Line | Reading | Met |
|---|---|---|---|
| Boss reach | no Tide arm down more than 2 of 15 | worst −1 | yes |
| 13 Frost and Stillness combinations, mean | ≥ −1 500 (from −8 506) | −2 203 | **no** |

The cast line was taken from the earlier trial's −392, and that trial was not this rule. Frost's seven rows equal the trial exactly; Stillness's six do not. Taking the screen Freeze off Stillness as well reproduces all six to the unit, so the trial had done that too. The line stays as logged and stays missed. The rule stands on the scheduler's rejection test, the boss clause; D-019 says how to undo it.

| Form | 13 combinations, mean | Six Tide arms, boss / wins | Tide-Frost-Storm |
|---|---|---|---|
| Control | −8 506 | 43 / 29 | 11 / 9 |
| **No line (D-019)** | −2 203 | 45 / 30 | 11 / 9 |
| No line, no Stillness screen Freeze (not adopted) | −392 | 40 / 30 | 9 / 8 |

### Bot, 15 seeds, 19 arms
Reached boss / wins.

| Arm | Before | After |
|---|---|---|
| None | 0 / 0 | 0 / 0 |
| Forge-Furnace | 14 / 10 | 14 / 10 |
| Forge-Furnace-Tide | 6 / 6 | 6 / 6 |
| Forge-Hammer | 14 / 13 | 14 / 13 |
| Forge-Hammer-Tide | 13 / 10 | 13 / 10 |
| Forge-Slam | 14 / 11 | 14 / 11 |
| Forge-Slam-Tide | 10 / 3 | 10 / 3 |
| Storm-ArcGrid | 12 / 11 | 12 / 11 |
| Storm-ArcGrid-Forge | 15 / 12 | 15 / 12 |
| Storm-Downpour | 15 / 14 | 15 / 14 |
| Storm-Downpour-Forge | 15 / 14 | 15 / 14 |
| Storm-Tempest | 10 / 6 | 10 / 6 |
| Storm-Tempest-Forge | 15 / 14 | 15 / 14 |
| Tide-Frost | 10 / 8 | 10 / 7 |
| Tide-Frost-Storm | 11 / 9 | 11 / 9 |
| Tide-Stillness | 6 / 3 | 9 / 3 |
| Tide-Stillness-Storm | 5 / 4 | 6 / 6 |
| Tide-Whirlpool | 6 / 3 | 5 / 3 |
| Tide-Whirlpool-Storm | 5 / 2 | 4 / 2 |

Steered: at the boss 196 → 198, wins 153 → 154. T-DIR-01 PASS 588.6 s, unchanged. T-OFF-15 FAIL 195/196 → 197/198, one miss both times. The line's protection did not show as lost reach. The Whirlpool arms move because a build can lead with Frost or Stillness for part of a run, and they hold those Keys in 7 of 16 builds.

### T-KEE-15 and T-KEE-18
**T-KEE-15: FAIL, 15/57 → 15/57 in band.** Casts that lose damage 17 → 13, 12 of them Tide. Storm and Forge rows do not move.

| Keeper | Mean before | In band before | Mean after | In band after | Range after |
|---|---|---|---|---|---|
| Storm | 37 920 | 8/19 | 37 920 | 8/19 | unchanged |
| Forge | 48 762 | 7/18 | 48 762 | 7/18 | unchanged |
| Tide | −3 004 | 0/20 (no band) | 1 083 | 0/20 | −10 376 to 40 169 |

The Tide mean is now positive, so a band exists, and it is ±217 around a mean that one row with SD 31 555 sets (Whirlpool:Furnace, `S1_keeper_power.md` §7).

**T-KEE-18: FAIL, gain 1/9 → 1/9, share 4/9 → 5/9, archetype band 5/9 → 5/9.** Mean share 0.573 → 0.589. The Storm and Forge arms read as before.

| Arm | Builds | Full DPS | Share | Loss without blessings | Gain before | Gain after | Band |
|---|---|---|---|---|---|---|---|
| Tide/Frost | 12 | 6 283 | 0.653 PASS | 0.737 | 0.90 | 0.86 | IN |
| Tide/Stillness | 11 | 6 232 | 0.632 PASS | 0.786 | 1.26 | 1.13 | IN |
| Tide/Whirlpool | 8 | 7 493 | 0.907 PASS | 0.860 | 1.23 | 0.95 | OUT |

## 2. Round 1 numbers: none kept
Tree: D-019. Six Tide arms plus None, 15 seeds. Six numbers took five runs. The Forge and Storm numbers were paired on arms that cannot hold both, because a Tide arm's second Keeper is fixed: Forge's numbers read on the three Forge-second arms, Storm's on the three Storm-second arms. A Frost arm can hold Whirlpool's Key (6 of 12 builds), so that pair needed splitting.

Line for each: its named arms sum boss and wins both ≥ +2, no Tide arm −2 or worse; only then the 19 arms. None reached the 19-arm step.

| Number | Named arms | Boss | Wins | Verdict |
|---|---|---|---|---|
| Downpour Key `dmg` 0.50 → 0.35 | Frost-Storm, Stillness-Storm, Whirlpool-Storm | 21 → 25 | 17 → 16 | reverted |
| Tempest Key `child_dmg` 0.45 → 0.65 | the same three | 21 → 23 | 17 → 18 | reverted; T-DIR-01 607.4 s |
| Slam Key `n` 6 → 4 | Frost, Whirlpool | 15 → 15 | 10 → 6 | reverted |
| Furnace Key `dps` 0.36 → 0.45 | Frost, Stillness, Whirlpool | 24 → 21 | 13 → 10 | reverted |
| Frost Trait `shatter_dmg` 0.50 → 1.00 | Frost, Frost-Storm | 21 → 19 | 16 → 10 | reverted |
| Whirlpool Key `crush_dmg` 0.10 → 0.25 | Whirlpool, Whirlpool-Storm | 9 → 10 | 5 → 6 | reverted |

Shatter and crush as a pair read best, 49/35 on the six arms, though neither did alone, so the pair got its own line and fresh seeds.

| Six Tide arms, boss / wins | D-019 tree | Round 1 change | Result |
|---|---|---|---|
| Pair, seeds 1–15 | 45 / 30 | 49 / 35 | the reading that raised it |
| Pair, seeds 16–30 | 45 / 24 | 43 / 32 | **line not met:** boss −2, Tide-Stillness 8 → 5; reverted |
| All six, seeds 1–15 | 45 / 30 | 51 / 32 | Frost 9/6, Stillness 11/6, Stillness-Storm 7/6 |

**Which number did it: none on its own.** The set lifts boss reach here by less than round 1 reported on a different tree, wins barely move, and no part of the lift can be pinned on one number.

The same tree on two seed ranges reads 45/30 and 45/24, and one arm reads 5/3 and 3/1. Any change to a number reshuffles a run, so moves of 2–3 on an arm of 15 can be noise. Every line above sits in that range, including the ones that were close. Round 1's 213/162 should not be read as evidence that its numbers worked.

## 3. What reads wrong
- D-019 stands with a logged clause missed. The miss comes from a bad target, not a small effect, but the scheduler should know it was the implementer's line and it failed.
- Stillness keeps a screen Freeze for 3 s that no text names, in a 6 s pool. Taking it off is the trial, reads better on casts and worse at the boss. That is a separate RULE.
- `Tide-Stillness` gained 3 at the boss and 0 wins; `Tide-Frost` lost a win. D-019 helps runs reach the Warden, not beat it.
- Gain on all three Tide arms fell or held (0.86, 1.13, 0.95), still far from 2.0–2.4. Tide/Whirlpool's sample changed, 10 → 8 builds, so its 1.23 → 0.95 is partly a different set of builds.
- Frost:Forge rows stay the worst casts (−5 992, −10 375), Stillness:Furnace too. Not traced.
- 15 seeds cannot separate a change of 2 on an arm. A TUNE line at that size is reading noise; the fresh-seed check is what caught the pair.
- All of this is a bound at skill 1.0 and no number here has met a human (O-008).
