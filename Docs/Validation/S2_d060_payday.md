# D-060: a payday's own kills add no Debt (2026-11-11)

Branch `s2/d060-payday`, cut from `s2/fit-and-unlock` at `8affa4e` (on `dev/s2` `fe107d7`). Class: RULE (`keepers.md` R-KEE-29; D-060 and its ruling 4). Layer: the Debt rule, with one bool carried from a payday's round to the kill. Replaces: "each kill of a Marked enemy adds one Debt" with no exception. No number moved. **A bot reading is never a human reading.**

Build:
- Before: the build of `ed8afd8` (run stamp `ed8afd8`). Its `Source` equals the step-1 tip `8affa4e` (`git diff ed8afd8 8affa4e -- Source Content Config` is empty).
- After: the build of `372dfc6` (run stamp `372dfc6`; the code is `abc3538`, `372dfc6` adds the rule's wording). `Tools\Build.bat`: `Result: Succeeded`. `Tools\RunTests.ps1 -Filter Dusktracer.Keepers`: `TESTS: PASS (134 run of 134 found, 0 failed, floor 0, 0 crash marker(s))`; `-Filter Dusktracer.Reactions` (the Pyre Debt test among them): `TESTS: PASS (44 run of 44 found, 0 failed, floor 0, 0 crash marker(s))`.
- The whole gate runs once on the top of the stack (`s2/d061-pulse-wall`).

## What changed
- `bPayday`, default false, on `FShot`, on the bullet (bit 1 of the pool's existing per-bullet `Crit` byte, bit 0 still `bCrit`: no new array, nothing more at `C.BULLETS.CAP`), on `FDTHit` and on `FDTKill`.
- Set by `FirePayday`'s rounds (the volley and the cast's instant payday) and by the execute a payday calls (`ExecuteMarked(..., bPayday)`).
- Carried to what a payday round's impact sets off: while that impact reacts, every round it spawns is flagged (`SpawnRound`), and every hit, echo, arc mark and impact it appends is stamped (`StepEffects`); an echo's strikes and a chain's forks stamp their hits from the flag they carry; a round's expiry takes its flag. So a split, a chain, an echo, a ricochet, a Seek bonus or execute and a Pull's extra hit set off by a payday round are the payday's (D-060 ruling 4).
- Not carried, as the rule says: a status it applied (a Burn tick's kill adds Debt), a field it left, a nova from its kills, and the duo reactions (they run after the impacts, outside the scope).
- Copied when the hit is written (`DTBulletPool.cpp`) and when the kill is made (`DTEnemyPool.cpp`, one line); read only by `ReactKeeperKills`. No `SourceId` and no telemetry field moved.
- Test `Dusktracer.Keepers.S2.Card.Shade.Economy.PaydayKills` (T-KEE-29): an ordinary Marked kill adds one; a payday whose rounds kill five Marked enemies leaves Debt where it was and readies no second payday, and its hits carry the bool; the audit's execute adds none; a hunter's Marked kill adds one; Hound's own execute adds one, as before; a run with no Debt entry has no hit or kill marked. `C.GATE.TEST_FLOOR` 480 to 481 (`number_change_log.csv`).

## Readings
Cast scenes as the exit builds them: `python -I Tools\probe.py intercession Saved\Probe\d060_before --seeds 2 --stage all` (1128 scenes), the 192 whose cast Keeper is Shade kept in `jobs_shade.csv`, then `& Tools\BotRuns.ps1 -Jobs Saved\Probe\d060_before\jobs_shade.csv -Unlock S2Arsenal -Parallel 6 -Stagger 0.4 -Out <dir>`, which runs `probe.py read`. The same job file before and after.

| Shade cast row (marginal damage) | Before | After |
|---|---|---|
| Economy, no layer | 1 940 801 | 146 580 |
| Economy, Vector layer | 1 991 364 | 144 886 |
| Economy, largest row | 3 954 879 (Momentum) | 293 464 (Solar) |
| Economy, smallest row | 722 608 (Downpour) | 129 454 (Lance) |
| Shade mean (48 combinations) | 600 282 | 59 294 |
| Rows within +-0.2 of the mean | 0 of 48 | 0 of 48 |
| T-KEE-15 line | `FAIL (0/48 ...; Shade mean 600282, 0/48 in band)` | `FAIL (0/48 ...; Shade mean 59294, 0/48 in band)` |

The before row confirms the exit at `146af0a`: Shade mean 600 282, Economy with a Vector layer 1 991 364. The record's 1 948 998 is one of the two seeds of `S2_trace.md`'s cast scene (1 948 998 and 1 948 952); this exit-built row is the mean of its own two seeds. All 32 Hound and Hunters rows read exactly the same before and after.

Printed beside it and judged nowhere (bot v2, skill 1.0, `-Unlock S2`, the exit's arm `Steered:Shade:Economy:Storm`):

| | Before | After |
|---|---|---|
| Seed 9107: outcome, end, `input_hash` | victory, 976.7 s, `bdc8038a` | victory, 955.3 s, `242cd291` |
| Seeds 1 to 15: damage per second at minutes 10 to 14 (`sources` records 600 to 840 s, mean of runs alive at 840 s) | 311.2 (15 of 15) | 309.9 (15 of 15) |
| Seeds 1 to 15: wins | 15 | 15 |

The damage per second is read by a scratch script over the runs' `sources` records (not a Tools script); the record's 321 came from another tree and options, so only this table's before and after compare.

## Hypothesis
D-060: "moves Economy's no-layer cast on the cast scene from 1 948 998 to about 150 000 and Shade's combination mean from 600 282 toward the range of the other Keepers' ... reverted if the cast row does not fall by at least half". **Met**: the no-layer row falls from 1 940 801 to 146 580 (to 0.076 of it), and Shade's mean from 600 282 to 59 294. Shade still has no combination in its band: its mean is now near the top of the other Keepers' range (7 985 to 56 413), and Economy's rows still stand about 2.4 times that mean.

## Finding: Economy still tops Shade's band (not a tune)
With the loop gone, Shade's mean is 59 294. Economy's rows stand at about 2.4 times it (no layer 146 580; the largest, Solar, 293 464), and Hound's and Hunters' rows sit far under it (the largest of them 23 690), so still 0 of 48 combinations are within `C.INTERCESSION.BAND` of the mean (R-KEE-15). What is left in Economy's cast is one round at every enemy, each piercing on with the Amp: it grows with the square of the crowd. D-060's own Decision named this case: "Option 3 is the larger change and is looked at again only if Economy's row still tops Shade's band after this." It does, so the next look is the source's shape of a payday (one volley that seeks every enemy at a multiplier) or a retune under R-KEE-15, as a decision of its own. Nothing is tuned here.
## What goes stale
The Shade rows of the S2 exit (T-KEE-15 and T-KEE-18 scenes holding Economy) and every bot run with an Economy card: seed hashes of Shade/Economy arms moved (seed 9107 above). Arms that hold no Debt card are untouched by the rule; the five-arm seed 9107 check on the top of the stack shows it.

## Whole gate on the top of the stack, `fa1acde`, 2026-11-12
`s2/d061-pulse-wall` at `fa1acde`: `s2/fit-and-unlock` and `s2/d060-payday` beneath it, `dev/s2` `63c4e51` merged at `1dcde7b`. `Tools\Build.bat` (`Result: Succeeded`), `Tools\Localize.bat` (`LOCALIZE: PASS`), then `& Tools\Gate.ps1 -Base dev/s2`; it waited about 13 minutes for the machine, which the scheduler held for a perf reading, then ran. Nothing was rerun.

```ncpu:        CPU: PASS (28 processors, mask FFFFF3FC, 600 rounds)
secrets:    SECRETS: PASS (574 files, 0 hit(s))
docs:       DOCS: PASS (0 error(s), 6 warning(s), 321 rules, must-read ~19748 tokens)
constants:  CONSTANTS: PASS (constants + palette)
catalog:    CATALOG: PASS (10 catalogs, 22 C. cells resolved, 98 tags)
matrix:     MATRIX: PASS (16 modifiers)
loc:        LOC: PASS (8 tables, 655 rows)
tools:      TOOLS: PASS (214 tests)
build:      rc 0, 0 warning(s)
locbuild:   LOCALIZE: PASS (Content\Localization\Game\<culture>\Game.locres)
nonunity:   5 file(s), 0 failed
shipping:   rc 0, 0 warning(s)
devgame:    rc 0, 0 warning(s)
tests:      481 run of 481 found, 0 failed, floor 481, 0 crash marker(s)
smoke:      SMOKE: PASS (arena_empty_en, run start=True, crash=0, ensure=0, LogDT errors=0, killed=False, loc ok (en), 
keys ok (Dash  [Space] | Intercession  [Q]), image PASS (1280x720, luma stddev 11.74, mean diff vs golden 0.29 (max 
12.0)), mask FFFFF3FC, 84 sample(s), 0 outside)
smoke:      SMOKE: PASS (arena_empty_zh-Hans, run start=True, crash=0, ensure=0, LogDT errors=0, killed=False, loc ok 
(zh-Hans), keys ok (冲刺  [空格键] | 神临  [Q]), image PASS (1280x720, luma stddev 11.52, mean diff vs golden 0.36 (max 
12.0)), mask FFFFF3FC, 90 sample(s), 0 outside)
smoke:      SMOKE: PASS (burst_en, run start=True, crash=0, ensure=0, LogDT errors=0, killed=False, loc ok (en), keys 
ok (Dash  [Space] | Intercession  [Q]), RENDER: PASS (PostProcess_Presentation: 5 settings as 04_Presentation s4 
says), image PASS (1280x720, luma stddev 36.61, exposure shift +1.72 levels against the calm frame (max 12.0)), mask 
FFFFF3FC, 275 sample(s), 0 outside)
restart:    RESTART: PASS (seed 9107, input_hash 276faf8e / 276faf8e, steps 5400 / 5400, csv same=True, old run reads 
restart=True, input check=True)
seed:       PASS (4 human launches on fresh seeds, 4 exact, 2 rejected)
contrast:   CONTRAST: PASS (7/7 elements at or above 3.0), mask FFFFF3FC, 147 sample(s), 0 outside)
contrast:   CONTRAST: PASS (dormant_line 13.62 on its panel, need 3.0), mask FFFFF3FC, 279 sample(s), 0 outside)
windows:    PASS (0 shown by bot, smoke and test runs across 92 processes; probe caught; bypass caught; 10 lost; 39 s)
pad:        NOT RUN (no package; Tools\Package.ps1)
scope:      NOT RUN (no brief in flight)
mac:        NOT RUN (deferred, D-007)
GATE: PASS (fa1acde; 3 not run; mask FFFFF3FC)
```

Then the five-arm seed 9107 check on that build (`& Tools\BotRuns.ps1 -Arms None,Random,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -FirstSeed 9107`, `BOTRUNS: PASS`): `input_hash` None `00e0d4ec`, Random `51369747`, Steered:Forge:Hammer `e3293942`, Steered:Storm:Tempest `b046559b`, Steered:Tide:Frost `def51848`, the same five as before, with the same `steps`.
