# S2 outlier trace (2026-10-31)

Branch `dev/s2-trace`, from `dev/s2` at 38c49ae. Class: BUG, three fixes; the rest are readings. Layer: Keeper hit capture, the payday, the probe. Replaces: the enchantment roll on field ticks, the payday outside the volley. **No number or rule changed; T-KEE criteria are unchanged. A bot reading is never a human reading**, and no human has played S2.

"Before" is the `S2_exit.md` battery, "after" is the same battery on this branch (build stamp f2ef7ed).

## Verdicts

| Outlier | Verdict | Mechanism in one line |
|---|---|---|
| 1 Economy's cast, about 1.3M | **Working as written; the rule needs a decision.** One BUG found beside it, fixed, and it did not lower the reading | A payday's own kills pay for the next payday, on a field that refills |
| 2 Solar and Lance DPS fell | **Not a fall.** The baseline was void | The "was" column ran with Storm as Patron |
| 2 Solar:Tide Blinds | **BUG, fixed** | Each 60 Hz field tick rolled the enchantment |
| 3 Tide 8 of 45 | **Working as specified, no bug found.** Needs a rule or number decision | The share reads the second Keeper; Tide's own cards add no DPS |
| 4 Vector's cast near zero | **BUG in the probe, fixed; the low reading is as specified** | The route never dashed in the cast, and at a neutral scale the cast only adds dashes |

## Fixes, each red first
Failing tests: commit b60b7c2. Fixes: f2ef7ed.

| Test | Red | Rule |
|---|---|---|
| `Dusktracer.Keepers.Param.FieldTick` | 3008 rolls for 16 rifle hits; 233 Chills on an enemy only fields touched | R-KEE-16 "field ticks are not hits", R-KEE-09 |
| `Dusktracer.Keepers.S2.Card.Shade.Economy.Volley` | a payday fired between volleys | R-KEE-29, T-KEE-29 |
| `tools_test` `test_a_cast_that_acts_on_dashes_is_dashed` | no `cast_dash_s` in a Quicksilver scene | R-KEE-15, R-KEE-35 |

The Volley test first read a cooldown before any step, a test fault fixed before f2ef7ed; its first red assertion stood. `Keepers.Keywords` also failed in that red run: generated text was not yet built in the worktree, nothing to do with these tests.

- **Field tick.** `FDTHit::bFieldTick`; `FDTKeeperSim::CaptureHits` skips it. So a tick also stops reaching `frozen_vuln` and the layer status, which read the same capture and say "hit". Status a field names (R-KEE-22) is unchanged, as are force fields and mine ticks, which `weapons.md` calls hits. **This changes S1 arms on `dev/s2`.**
- **Payday.** It fired at the head of the step, and the weapon's own volley fired too. It now waits for the next volley of any weapon, replaces it and takes its cooldown. A cast still pays that step (`PayNow`, R-KEE-35 "pay now"). An Orbiter has no volley, so a build of only Orbiters would pay only by cast; every run starts on the Rifle.
- **Probe.** `CAST_DASH_S` already covered Slam; it now covers Quicksilver, in the cast arm only. The dash window is still Forge's 5 s, so the last dash is the route's own at 12 s and none lands after it.

## 1. Economy's cast
Scene `IK-Shade-Economy-none`, seeds 17001 and 17002, 12 s window. Enemy HP is about 1072 (40x field). On the 1x rows HP is scaled down 40 times; those two rows are small and noisy.

| Scene (after the fix) | Damage | Kills |
|---|---|---|
| No cast, 40x | 7796, 8552 | 0 |
| Cast, 40x | 1948998, 1948952 | 1785, 1791 |
| Cast, 40x, a payday's kills pay no Debt (a temporary switch, not committed) | 152441, 157067 | 134, 132 |
| No cast, 1x | 33300, 18821 | 1266, 702 |
| Cast, 1x | 68927, 50633 | 2579, 1881 |
| Cast, 1x, no self-pay | 40687, 26870 | 1538, 998 |
| No cast, 1x, no self-pay | 22735, 9010 | 871, 325 |

- Step by step: the cast Marks the screen (R-KEE-35) and forces a payday. A payday round is 57 here (rifle 19 x `payday_mult` 3.0). One round goes at each of 120 enemies with `payday_pierce` 99, so before the fix the first burst alone was about 2100 hits, about 120000 damage, close to the field's 128000 HP. That is why it grows with the square of the crowd.
- Kills of Marked enemies add Debt, `debt_max` is 13 with these cards, so a payday pays for the next. Damage runs flat at about 140000 per second to the window's end, 6 s past the cast. Damage equals kills times HP (1440 x 1072 = 1.54M before the fix): the reading measures the refill, not the cast.
- **Split:** the self-pay is 12.7x, the rest (one payday, square of the crowd, 40x HP) still leaves about 146000, 1.9 times Slam's 77526, the largest no-layer cast elsewhere, and about 25 times Hunters' 5898. At 1x HP the cast adds roughly 34000 with the loop and 18000 without.
- R-KEE-29 says "each kill of a Marked enemy" and excludes nothing, so this is the rule as written. R-KEE-33 and R-MOD-13 do say their bursts start no reaction; R-KEE-29 does not. **Not changed: this is a rule.**
- The volley fix did not lower it: variant mean 1296433 before, 1782817 after, no-layer 1620916 to 1940801. Debt now fills at the same rate and rounds still reach everything; the reason it rose was not traced.
- In real runs it is no outlier: Economy deals 321 per second at minutes 10 to 14, others 261 to 308, 14 of 15 wins before and after at skill 1.0. Skill 0.6 went 9 to 3 of 15, the largest move of the fixes on any arm; not traced, and 15 seeds.

## 2. Solar and Lance
Same 8 build scenes, same seeds, mean full DPS:

| Reading | Solar | Lance |
|---|---|---|
| `S2_keepers.md` first reading, Patron Storm by fault | 11265 | 12484 |
| Today, no `-DTUnlock`, Patron Storm | 10448 | 12925 |
| Today, unlocked, Patron Radiance | 7165 | 10518 |
| `S2_keepers.md` rerun | 7165 | 10518 |

Today's build equals the rerun to the unit, and Storm as Patron restores the old level, so the game did not fall: the "was" column was the void reading. Patron Storm rolls Shock at the primary chance and Radiance rolls Blind, so the drop is the build's real Patron. Tree drift between the two is -7 percent on Solar, +4 on Lance. Solar's Blinds were then 709 on one build under Storm and 66213 under Radiance.

**Blinds.** A field ticks every step for `dps x Dt` and each tick had a slot, so `Enchant` rolled 15 percent 60 times a second, per enemy, per field. Solar:Tide over 15 runs at skill 1.0:

| Status | Before | After |
|---|---|---|
| Blind | 653349 | 38258 |
| Chill | 240042 | 53290 |
| Freeze | 1844653 | 1207306 |

`S2_keepers.md`'s 1.0M was the no-unlock battery; this table is the exit battery. Blind dropped 94 percent. **Freeze is still 1.2M, unknown:** Frost's hold is re-applied every two steps (`DTWeaponSim.cpp`, `freeze_until_hit`), which fits, not proved on this arm. It did not change the arm: 6 of 15 at skill 1.0 both times, and 5 to 1 at 0.6. Solar's full DPS in the exit table is 6715 before and 7188 after, Lance's 12393 and 13312.

## 3. Tide
No bug found. The field tick was the suspect and removing it did not help: 8 to 5 of 45 at skill 1.0, 1 to 1 at 0.6, a shift of that size is noise here. So the rolls were not what held Tide down.

| Skill 1.0, after | Reach | Wins | Live enemies, minutes 1, 8, 13 | Nearest uu, minute 8 | Damage taken/min, minute 8 | Boss DPS |
|---|---|---|---|---|---|---|
| Tide | 13 | 5 | 32, 58, 63 | 227 | 0.15 | 164 |
| Quicksilver | 43 | 42 | 18, 23, 21 | 443 | 0.03 | 623 |

- **The share is not Tide's.** `no_keepers` removes both Keepers, and every Tide arm's second is Forge (45 of 45). Tide/Frost at minute 12: Keeper.Forge 7163, Keeper.Tide 1876. Package gain, which is the archetype's own cards, is 0.97 to 1.08 on all three arms against shares of 0.83 to 0.90. Share reads high and gain reads nothing: the formula is working, and what it says is that Forge is doing the damage.
- **The crowd.** Other Keys add damage early, Tide's do not: minute 1 damage 2278 against 2566. That is 10 kills a minute, 17 fewer by the end of the second minute, which is the whole difference in the crowd (32 against 15 to 18). It starts before any status: 8 Chills in minute 1. Held enemies then stay alive, and 2 to 3 times the crowd stands at half the distance. 32 of 45 die before the boss.
- **The boss.** R-BOS-11 refuses Freeze and caps Chill, the Cracked answer is out of slice (D-60), and Tide's cast deals no damage (D-019). Boss DPS 164 against 623.
- Quicksilver's 42 is second Storm on 30 of 45 runs plus Rebound and Momentum, which work on one target: 761 and 669 boss DPS before.
- Probe, field and hero that takes no damage: T-KEE-18 cannot see any of this. **Anything that moves Tide is a rule or a number, reported, not made.**

## 4. Vector's cast
No layer, cast and plain were identical to the unit on both seeds (7872 and 9076), so 0 was exact. The route dashes every 6 s: at 6, 12, 18. The cast runs 8 to 12 and the dash at 12 lands as it ends. `R-KEE-35` acts on dashes only, so it never acted. The same hid the dash half of Rebound and Momentum.

Dashed: seed 23401 reads 943, Rebound 1034, Momentum 18174. In the full reading the no-layer value is Vector 75, Rebound 592, Momentum 15942, on its own seeds; variant means 841, 2482, 20832.

So Vector still reads near zero, as specified. `dash_cooldown_s` buys dashes, the volleys are the Key's, and the scale that would give the cast its meaning stands neutral (`S2_keepers.md`: both tries failed). Only 86 of 943 was Keeper damage: the route's dash runs along the crowd, not into it, and in bot runs Keeper.Quicksilver deals 1400 to 2900 a minute, so the probe still under-reads a dash aimed by a player. **Not changed:** the route, and the scale. `Slam`'s reading is unchanged by this.

## S2 exit, rerun
`& Tools\S2Exit.ps1 -Out Saved\BotRuns\s2_exit -Parallel 12` at f2ef7ed, 540 runs, T-KEE-18 and T-KEE-15 complete. `S2EXIT: 1 PASS, 3 FAIL, 2 NOT READ`, the same verdicts.

| Reading | Before | After |
|---|---|---|
| Keeper share | 10/18, mean 0.598 | 8/18, mean 0.607 |
| Build bands | 11/18 | 9/18 |
| Intercession | 50/282 | 46/282 |
| Wins of 45 at skill 1.0 | Storm 24, Forge 33, Tide 8, Shade 41, Radiance 34, Quicksilver 44 | Storm 24, Forge 41, Tide 5, Shade 39, Radiance 35, Quicksilver 42 |
| Boss kills of reachers, skill 0.6 | 0.53 of 198 | 0.52 of 192 |

Forge gains 8 at skill 1.0 (Slam 9 to 13, Furnace 11 to 14), not traced; a shift of 2 or 3 on an arm can be noise. `S2_exit.md` stands as the reading at its commit.

## For a decision, none made
1. May a payday's kills fill Debt? A rule (R-KEE-29). Then whether T-KEE-15 can judge a screen-clear on a refilled 40x field at all.
2. What Tide's Key gives before the first boss, and the boss answer (D-60). The share formula credits the second Keeper to the arm.
3. Time Dilation's scale, for hands.
4. T-KEE-15's route for an aimed dash.

## Gate
`& Tools\Gate.ps1 -Base dev/s2` at 243267c: GATE: PASS, 405 tests of 405, tools 41, 3 not run (pad, scope, mac). This line is the only change after it.
