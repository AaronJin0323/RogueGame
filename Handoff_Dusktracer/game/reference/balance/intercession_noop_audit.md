# Intercession no-op audit (2026-10-26)

## Evidence

Numbers below are historical, from [the round-2 gate capture](intercession_noop_round2_diagnostics.log) and the preceding audit. That gate reported 714 passes and one failure, `RogueHero.Balance.IntercessionStrength.Measure`; build, non-unity and TextAudit passed. No editor, build or automation was run for this revision. Implementation dispositions below are source changes, not new measured results.

The scheduler corrected spec section 1.6: within a variant, every pair must have `abs(left-right) > 1e-4 * max(abs(left),abs(right))`. Two zeros fail. Passing pairs at or below 0.5% produce `AddInfo` and increment `weaklayer`; they do not fail. D-66's outside20 count remains informational.

## Measured findings and dispositions

| Combo / pair | Recorded number | Cause supported by the evidence | Outcome |
|---|---|---|---|
| Storm.ArcGrid+Tide / +Quicksilver | 25837.099666595459 / 25962.549774169922 | Distinct totals rejected by the former 0.5% assertion. | Weak-layer finding. Reverted the later pre-impact Freeze/arc ordering change. |
| Radiance.Prism+Storm / +Tide | 13793.499974727631 / 13797.799999237061 | Distinct totals rejected by the former 0.5% assertion; this is not evidence of missing Branching dispatch. | Weak-layer finding. Reverted the slot-cache change. |
| Quicksilver.Momentum+Tide / +Shade | 4956.8400039672852 / 4955.1600040197372; five freezes, zero surviving shatters; two phantoms, eight volleys | Both mechanics ran. End shatter requires surviving Frozen targets. | Weak-layer finding. Reverted Tide-before-wave ordering and slot-cache changes; retained phantom behavior. |
| Quicksilver.Rebound+Forge / +Radiance | 836.60000228881836 / 832.75999927520752; two rings and two lances | Both shapes were emitted; their small difference did not establish a dispatch failure. | Weak-layer finding. Reverted the real-dash completion guard and targeting guards; counters only observe live hits. |
| Shade.Economy+Storm / +Quicksilver | Both 173.60000610351562; two executions each, six Storm arcs, zero collections | Storm arcs apply Mark/Shock and re-test execution; Shock is vulnerability. The recorded patrol produced no qualifying Collection contact. | Still an identical historical pair under the corrected rule. Reverted added arc damage and Collection timing/departure changes. Retained the harness's identical HP-only pursuit policy in cast/control windows, using naturally wounded hulls. No new separation is claimed. |
| Storm.ArcGrid+Forge / +Tide / +Radiance | With-cast 36110.9 / 26173.1 / 57045.6; six nets; Radiance stored 585 bolts and released one giant | Main's net route bypasses bolt-layer dispatch. The retained net-node dispatch supplies Forge rings, Tide Freeze/arcs, Shade Mark propagation and Radiance charge without extra sky strikes. | Fixed in code; new `RogueHero.Ultimates.StormLayers.ArcGridDispatch` asserts net-only damage, statuses, charge/release, and Mark transfer. The later pre-lethal ordering change is reverted. |
| Shade.Economy+Radiance | 104 lances; Ultimate source 42.0 | The old 32-lance cap truncates the specified lance for each Marked enemy. | Retained 512-cap fallback; `RogueHero.Ultimates.ShadeLayers.AccountingRegression` asserts all 40 fixture hulls receive lances beyond the old cap. |
| Storm.ArcGrid+Quicksilver | With-cast 26298.5, no-cast 336.0 | These historical diagnostics predate dashbolts/refills fields and cannot establish a zero bolt count. The preceding audit changed departure sampling, landing wait and refills without a measured justification for changing the contract. | Chasing Storm restored to main: `HeroPos()` on new serial, authored delay, no IsDashing wait, one activation refill. `RogueHero.Ultimates.StormLayers.Quicksilver` retains its whole-line and once-only refill assertions. Harness retains `Init(Hero)`, bound real movement and position updates; Storm now uses fixed dash request times in both windows. Counters only count. |
| Tide.Stillness+Shade / +Quicksilver | With-cast 414.4 / 438.2, no-cast 336.0; one thaw execution / zero shatters | Real movement and pool exit distinguish these historical windows; the data does not prove a slot-retirement fix is required. | Reverted branch-only Mark retirement; retained bound real movement in the harness. |

## Complete gameplay disposition against main

Only two gameplay changes remain: ArcGrid node-layer dispatch (`RHUltimate_Paths.cpp`, its `ForgeBoltRing` declaration in `.h`) and Shade's every-Marked-hull lance cap (`RHIntercessionVariant.cpp`). Their no-ops and regression tests are named above.

Other branch changes were restored to main: Breakwater crush and its Param/helpers, Tide current cold-membership filtering, Tide Mark retirement, Surf's shatter-route replacement, Shade Storm arc damage and Collection departure/timing, Storm pre-impact Freeze ordering, Radiance Freeze slot caching, Quicksilver Tide sampling order/cache, Forge/Quicksilver landing waits, and Quicksilver live-target guards. These changes had no regression assertion proving the specific alleged fix. Chasing Storm's departure/wait/refill changes were also restored as explicitly required by the scheduler.

Unrelated main differences were restored: XP tuning and its complete assertions, boss arrival logging, and RunStats damage-per-minute reporting/test. No golden was changed and no existing assertion was removed or loosened except the authorized pairwise threshold.

## Accounting and harness

`TotalDamage = With.AllDelta - NoCast.AllDelta`.

`AttributedUltimate = (Ultimate source + execute value)_with - (Ultimate source + execute value)_no-cast`.

Both use the same post-warmup window and tail. The CSV remains `Patron,Variant,Second,TotalDamage,RatioToMean,AttributedUltimate`.

- Downpour's recorded Weapon 336 to 112 (delta -224) follows direct sky strikes resolving before buffered weapon hits; casualty preemption is not source reassignment.
- Hound+Radiance's recorded Weapon difference +564.2 and Ultimate 630 expose the seeking/piercing cadence rounds previously left as Weapon. Retained accounting-only composition override and ordinary-hunter override clearing; gameplay sources, damage and cadence stay unchanged. The existing weapon-behavior accounting assertions cover ordinary, Hound and mixed grants.
- Intercession executes retain the scoped exclusion from generic Blessing accounting and record remaining HP once as execute value. `RogueHero.Ultimates.ShadeLayers.AccountingRegression` and `RogueHero.Balance.IntercessionStrength.Accounting` cover the routes. This does not change execution eligibility or kills.
- Ultimate mechanic counters remain observational. The harness retains its existing warmup, Debt prerequisite, crowd, loadout, refill, seeds, MaxSteps and TailSteps. Storm's fixed dash schedule and bound moving hero are used identically in cast and no-cast windows; no HP or status is injected. No new runtime totals are claimed for that schedule.
