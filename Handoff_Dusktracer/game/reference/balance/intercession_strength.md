# Intercession strength measurement (I7 / D-66)

Run `RogueHero.Balance.IntercessionStrength.Measure` to write
`Saved/Balance/intercession_strength.csv`, then run
`python Tools/Balance/intercession_strength.py [csv_path]`.
The CSV columns are
`Patron,Variant,Second,TotalDamage,RatioToPatronMean,RatioToGlobalMean,AttributedUltimate`.
`TotalDamage` is marginal damage (with cast minus no cast) on the refilled field.
`RatioToPatronMean` divides it by that patron's combination mean;
`RatioToGlobalMean` uses all 100 combinations for information only.
The six base rows receive both ratios but are excluded from every mean, minimum,
maximum and outside-band count. `AttributedUltimate` remains diagnostic only.
The reporting tool groups by patron, lists outliers and per-variant/per-second
mean ratios, and writes to stdout unless an explicit `--out` is supplied.
`--log-path <gate_log>` optionally includes the exact global gate summary.

**D-66, designer decision 2026-10-26:** each patron's variants x its five second
Keepers must lie within +/-20% of that patron's own mean [GATED]. Burst and
control Intercessions cannot be compared by damage across Keepers; the supplied
base-branch spread is about 1300x. No band assertion is added. The existing
pairwise non-identity assertions and 0.5% weak-layer findings remain.

## Per-patron results (2026-10-26)

**gate run, round 2** (`Saved/Logs/RogueHero.log`, 2026-10-26 11:19:00 UTC).
These are historical results before this round's fixes. Build, non-unity and
TextAudit passed; automation was **720 passed / 2 failed**. Wall time was
**5.661 seconds**, below the **180-second** limit.

```text
IntercessionStrength Storm mean=23013.6 min=Storm.Downpour+Tide 0.1274 max=Storm.ArcGrid+Radiance 2.8101 outside20=18
IntercessionStrength Forge mean=190820.3 min=Forge.Slam+Quicksilver 0.0121 max=Forge.Hammer+Shade 3.1414 outside20=15
IntercessionStrength Tide mean=2945.6 min=Tide.Stillness+Shade -0.0656 max=Tide.Frost+Storm 2.1230 outside20=11
IntercessionStrength Shade mean=11196.1 min=Shade.Hound+Quicksilver -0.0480 max=Shade.Economy+Tide 3.0352 outside20=15
IntercessionStrength Radiance mean=17034.8 min=Radiance.Solar+Forge 0.4601 max=Radiance.Solar+Quicksilver 1.6588 outside20=6
IntercessionStrength Quicksilver mean=35296.8 min=Quicksilver.Vector+Shade 0.0723 max=Quicksilver.Rebound+Storm 2.6710 outside20=15
IntercessionStrength mean=43344.1 min=Shade.Hound+Quicksilver -0.0124 max=Forge.Hammer+Shade 13.8297 outside20=95 nonpositive=5 weaklayer=8
IntercessionStrength wall_seconds=5.661
```

Rows affected by this round: **Quicksilver.Rebound+Tide**, **Quicksilver.Rebound+Shade**,
**Tide.Stillness+Shade**, and **Tide.Stillness+Quicksilver**. Hammer+Shade's generation
fix already affected round 2; this round adds its missing regression test without
retuning it. Restoring once-per-spawn Tide Marks also corrects other Tide+Shade
variants if a Mark is removed. The phantom retry can affect Vector+Shade and
Momentum+Shade only when their shared weapon spawn budget is exhausted. The
corresponding patron/global summaries must be remeasured by the cowork gates.
Stillness+Radiance retains regression coverage; its gameplay is unchanged.

### Round-2 assertion and mechanic audit

Every failing assertion in the log was read. Measure had exactly two failures:

- Stillness+Radiance and Stillness+Quicksilver were identical at
  **450.80001068115234** (relative difference must exceed 1e-4).
- Rebound+Tide and Rebound+Shade were identical at **94174.982862472534**.

Hammer+Shade versus +Quicksilver passed round 2, as did the previously added five
VariantOutput tests and the hunter pair comparison. TideLayers.Shade failed
`only weak Marked line member executes` (2 instead of 1) and
`unmarked line member survives`: the round-2 Mark refresh undid explicit removal.
Mark registration now remembers enemy index plus existing spawn generation, once
per enemy, including outgoing pool contacts. Beam preference is explicitly
registered by Stillness+Shade only, selects only that registration's list, and is
removed on thaw, Finish and destruction. Generic Seek registrations do not
change Beam TargetRule.

Rebound's Tide diagnostic had frozen=5/shatters=0. The old dash-only list did not
follow ricochet recipients, and retained indices could refer to dead/replaced
enemies. Surviving bounced contacts from this hero's current weapon graph now
receive Freeze through Status; existing end shatter uses Hit on matching spawn
identities only. The new test requires a physical bounce outside any hero dash,
replacement eligibility, and end-shatter HP damage.

Rebound's Shade diagnostic had phantoms=3/volleys=19 with no extra source damage.
The volley counter records attempted fire: SpawnInternal drops capped spawns,
while Rebound's recurring ricochet output can spend the shared budget after each
bullet step, before phantom cadence runs. Phantom copies now get one bounded
retry after the next step replenishes that same budget. Copies preserve the
ricochet graph, damage, source and fixed origin; queued graph references survive
retirement. Neither the 400/s per-weapon cap nor the 6000 global cap is bypassed.
The new deterministic regression exhausts the budget, proves no immediate spawn,
then requires a retried physical ricochet and HP damage. Runtime confirmation
that this separates the measured pair belongs to the next gate run.

Stillness's outgoing guaranteed-Freeze volley now carries Surf during the active
hero dash window, including lethal contacts that never acquire a surviving
Freeze status in ApplyHits. A bounded shatter burst at the contact uses Hit and
layer Params (`QuicksilverPoolShatterRadius`, 120 uu, GATED; the existing shatter
damage is unchanged). Contacts are deduplicated by spawn identity and burst
victims by batch. This adapts Surf to the pool's output shape (spec 1.3), while
other Tide shapes retain their geometric dash sweep. A regression places a lethal
contact outside both disc and hero sweep and requires neighbouring shatter damage.
The earlier remote-survivor dash test remains intact with its missing pre-fix
condition assertion added.

Hunter tests now first resolve an otherwise identical attribution-only contact
and assert the recipient survives without a hunter execution hook; Quicksilver
also asserts the contact lies outside the hero sweep. The Hammer test kills with a
ring, refills the same index before the delayed kill check, proves the alive-only
condition would hide the death, and requires seeker release and damage. Existing
Measure assertions, fixture, thresholds, seeds, windows and SoloParity golden
are untouched. No band tuning or band assertion is introduced. TextAudit passes locally with zero violations. This round does
not run editor/tests; the cowork gates must verify the changes.

### Negative Hound + Quicksilver

Round 1 recorded withcast=4819.0, nocast=5356.3, marginal damage about **-537.3**:

| Source | with | no | difference |
|---|---:|---:|---:|
| Weapon (0) | 1530.10001 | 3351.1 | -1820.99999 |
| Blessing (2) | 1894.20015 | 2005.20018 | -111.00003 |
| Ultimate (5) | 1373.69999 | 0 | +1373.69999 |
| Ultimate execute | 21.0000043 | 0 | +21.0000043 |

Sources 1, 3, 4 and 6 were zero in both windows. The sum is approximately
-537.30003: -0.0125 times the global mean, or -0.0479 times Shade's mean.
The diagnostic had three closing executions, **collections=0 refunds=0**.
Thus this is not a negative-damage collection proc. `URHUltimate_Shade::Begin`
grants Hound Seek and two pierces; `RHSpecComposition::Compose` attributes that
paired cadence shape to Ultimate, while `SteerHoming` changes contacts.
`FField::Refill` replaces casualties after each full step, changing later contact
and replacement opportunities. Weapon damage lost (1821) plus Blessing damage
lost (111) exceeded the positive Ultimate and execute contribution (1394.7).
Reclassification alone cannot change the all-source sum. These source deltas
identify the measured deficit; the log does not isolate individual missed contacts.
This row is not tuned.

## Mid-run fixture (2026-10-26)

Status: **GATED fixture data**, prerequisite for intercession_variants_spec section 1.4 (D-66). No band assertion or gameplay tuning is introduced.

The old single L1 PulseRifle under-represented amplifying kits: Shade executes, Quicksilver dash kits and Tide holds read near zero; base Shade read 36 and base Quicksilver 188 versus base Storm 22,924. These historical readings are not results for the new fixture.

Each variant uses the first weapon in the code-order `RHOfferArchetypes::GetFitWeapons` list at L5 and the complete `RHBranches::GetCards()` package: key rank 2, each amplifier rank 2, capstone rank 1. All 20 code fit lists agree with keeper_identity_spec section 3.7's D-59 table. All roles resolve; there are no unresolved roles.

| Archetype key | Weapon at level | Key | Amplifiers | Capstone |
|---|---|---|---|---|
| `Storm.Tempest` | `Weapon.ScatterCannon` L5 | `Blessing.Storm.ForkedPath` x2 | `Blessing.Storm.Overcharge` x2 | `Blessing.Storm.Stormfront` x1 |
| `Storm.Stormwall` | `Weapon.RailLance` L5 | `Blessing.Storm.StaticFocus` x2 | `Blessing.Storm.LowCeiling` x2 | `Blessing.Storm.GridLock` x1 |
| `Storm.ArcGrid` | `Weapon.OrbitalDrones` L5 | `Blessing.Storm.LiveWire` x2 | `Blessing.Storm.GroundFault` x2 | `Blessing.Storm.Supercell` x1 |
| `Storm.Downpour` | `Weapon.FlakMortar` L5 | `Blessing.Storm.Thunderclap` x2 | `Blessing.Storm.RollingThunder` x2 | `Blessing.Storm.Capacitor` x1 |
| `Forge.Hammer` | `Weapon.ScatterCannon` L5 | `Blessing.Forge.Bellows` x2 | `Blessing.Forge.TemperedPlating` x2, `Blessing.Forge.BlastFurnace` x2 | `Blessing.Forge.Masterwork` x1 |
| `Forge.Furnace` | `Weapon.GravityMine` L5 | `Blessing.Forge.Kindling` x2 | `Blessing.Forge.BankedHeat` x2, `Blessing.Forge.SlowBurn` x2 | `Blessing.Forge.Conflagration` x1 |
| `Forge.Slam` | `Weapon.ArcBlade` L5 | `Blessing.Forge.EmberTrail` x2 | `Blessing.Forge.Quench` x2, `Blessing.Forge.SparkShower` x2 | `Blessing.Forge.CinderStorm` x1 |
| `Tide.Whirlpool` | `Weapon.FlakMortar` L5 | `Blessing.Tide.Undertow` x2 | `Blessing.Tide.DeepCold` x2 | `Blessing.Tide.Shatterpoint` x1 |
| `Tide.Breakwater` | `Weapon.RailLance` L5 | `Blessing.Tide.ColdFront` x2 | `Blessing.Tide.Riptide` x2 | `Blessing.Tide.KingTide` x1 |
| `Tide.Frost` | `Weapon.OrbitalDrones` L5 | `Blessing.Tide.Brittle` x2 | `Blessing.Tide.Breakwater` x2 | `Blessing.Tide.DeepReserve` x1 |
| `Tide.Stillness` | `Weapon.RailLance` L5 | `Blessing.Tide.HighWater` x2 | `Blessing.Tide.Wellspring` x2 | `Blessing.Tide.SpringTide` x1 |
| `Shade.Hound` | `Weapon.RingEmitter` L5 | `Blessing.Shade.Lien` x2 | `Blessing.Shade.Foreclosure` x2, `Blessing.Shade.Garnish` x2 | `Blessing.Shade.FinalNotice` x1 |
| `Shade.Hunters` | `Weapon.SeekerSwarm` L5 | `Blessing.Shade.Dividend` x2 | `Blessing.Shade.Escrow` x2, `Blessing.Shade.ShadeBoost` x2 | `Blessing.Shade.Arrears` x1 |
| `Shade.Economy` | `Weapon.SpiralProjector` L5 | `Blessing.Shade.Interest` x2 | `Blessing.Shade.LongArm` x2, `Blessing.Shade.Compound` x2 | `Blessing.Shade.Audit` x1 |
| `Radiance.Lance` | `Weapon.RailLance` L5 | `Blessing.Radiance.PerfectSight` x2 | `Blessing.Radiance.KeenEdge` x2, `Blessing.Radiance.FocalPoint` x2 | `Blessing.Radiance.Glint` x1 |
| `Radiance.Prism` | `Weapon.ScatterCannon` L5 | `Blessing.Radiance.Flashpoint` x2 | `Blessing.Radiance.Afterimage` x2, `Blessing.Radiance.Corona` x2 | `Blessing.Radiance.Whiteout` x1 |
| `Radiance.Solar` | `Weapon.FlakMortar` L5 | `Blessing.Radiance.Halo` x2 | `Blessing.Radiance.WideAperture` x2, `Blessing.Radiance.RadiantWake` x2 | `Blessing.Radiance.HaloRing` x1 |
| `Quicksilver.Vector` | `Weapon.ScatterCannon` L5 | `Blessing.Quicksilver.BlinkStrike` x2 | `Blessing.Quicksilver.LightStep` x2, `Blessing.Quicksilver.Tailwind` x2 | `Blessing.Quicksilver.SecondWind` x1 |
| `Quicksilver.Momentum` | `Weapon.ArcBlade` L5 | `Blessing.Quicksilver.Fleet` x2 | `Blessing.Quicksilver.Drafting` x2, `Blessing.Quicksilver.Wayfare` x2 | `Blessing.Quicksilver.TerminalVelocity` x1 |
| `Quicksilver.Rebound` | `Weapon.PulseRifle` L5 | `Blessing.Quicksilver.Slipstream` x2 | `Blessing.Quicksilver.BankShot` x2, `Blessing.Quicksilver.Carom` x2 | `Blessing.Quicksilver.RicochetKill` x1 |

The six base rows use the weapon of their patron's first variant in `GetAll()` order at L5, with **no archetype cards** and no second Keeper. This preserves the no-patron-key base selection rule (spec section 1.2).

Each variant retains the L8 second-Keeper GodChoice. Its build is identical across all five second-Keeper rows and both windows; only the GodChoice changes. Grants use `AddBlessingRank` after hero/status binding and retain their natural Favor and tier effects, as in `archetype_bands.py`. The scheduler supplied ranks 2/2/1: that script still has `PACKAGE_RANKS` values set to `None` and requires `--ranks`.

The metric remains total damage with cast minus total damage without cast on the same deterministic, refilled field, with the existing fixed dash schedule and re-seeded RNG. The control runs for the cast window's length. Build damage is present in both arms, so the subtraction measures marginal Intercession value, including its interactions with the build. Warmup, patrol, tail, `Attributed`, 106 rows and existing assertions are unchanged; the two ratio columns replace RatioToMean.

Bare-world adaptation: initialize only `URHWeaponRegistry` with a transient game-instance holder (the existing RHCleaveOfferTests pattern), then use the same tag lookup, `Hero->AddWeapon`, `SetLevel` and `GetLevel` route as `RH.Give`. No Debug helper is required; the public level API is reusable. Weapon count/level and each exposed blessing rank are asserted through the running automation test. Each window deinitializes and releases its registry and transient holder before world destruction.

Economy uses `Blessing.Shade.Interest` as its key. The old separate grant is removed so its rank stays 2; both setup and post-warmup debt resets remain 10. No other role/fit fallback is needed.

Build, non-unity build, full automation, text/localization gates and the wall-time target (<180 seconds) await the cowork runner; no editor/test execution was performed for this edit.

## Field and prerequisites

The field uses `RHBalanceProbe::CrowdSize`, `CrowdSeed` and `CrowdHPMult` with
`RandomCrowdSpot` around the origin. Swarms have zero speed, contact damage and XP,
and are not elite eligible. Casualties refill at their original slots after each
complete simulation step, including the tail. `WindUp` is overridden to 0. The
measured window includes a 2 s tail, with surviving hunters continuing to step.

Each variant gets its code-first fit weapon at L5, key x2, each amplifier x2 and
capstone x1. The L8 second Keeper is committed before hero/status binding; package
grants follow binding and precede warmup. Economy debt is set to 10 at setup and
again after warmup. Base rows get their patron's first-variant fit weapon at L5,
no archetype cards and no second Keeper. Weapon count, level and each package
rank are recorded as test assertions for both windows, with combo, expected and
actual values.

Warmup lasts half the weapon's base cooldown, rounded up to simulation steps.
The patrol axis is selected from 32 directions through the densest initial strip;
the hero reverses at the crowd's outer radius. Economy pursues the nearest
naturally wounded enemy below half HP, without injected HP or status. Dash
requests begin at step 30; Storm requests every 120 steps thereafter, other
patrons request each step, and Economy requests only when it has a wounded
target. Actual charges and cooldowns govern acceptance. Both windows retain the
same input policy and seeded start.

## Historical fixture job: round-2 failure and round-3 repair (2026-10-26)

`Saved/Logs/RogueHero.log:2778` records the actual failure:
`Expected 'Shade.Collectors exactly one fixture build' to be 1, but it was 0.`
The early integrity return prevented any row measurement. The runtime variant
uses `Shade.Collectors`, while the canonical D-59/package key is `Shade.Hunters`.
The literal 20-entry table is correct and stays unchanged. The harness now uses
`RHIntercessionVariants::GetArchetypeTag` to obtain the canonical package key for
integrity matching and fixture lookup. It retains the runtime ID for
`SetForcedVariant`, row IDs and the exact resolved-variant assertion; package
grants cannot override that forced selection. This is a harness compatibility
adaptation until the runtime variant IDs are canonicalized by their owner, not a
weapon/card correction or a numeric difference from D-59.

Integrity failures now include expected/actual weapon and card-role lists and
continue into measurement unless a build or weapon definition is missing.
Post-build checks report automation failures instead of crashing. Each field
calls the registry's inherited `Deinitialize()` and resets both strong pointers
before destroying the world; the transient game-instance holder was never
initialized, so it needs no shutdown call. The integrity preflight registry has
the same explicit cleanup.

Round-2 gate summary supplied with the job: build, non-unity and text audit
passed; automation **716 passed / 1 failed**, the failure above. This log has no
`IntercessionStrength mean=...` or `IntercessionStrength wall_seconds=...` line.
That earlier fixture run predates the I7d2 round-2 results above; the current
log now includes L5 measurements and a 5.661-second wall time. No measured gameplay finding can be inferred from this early return.
No editor, builds or tests were run for the round-3 edit; the cowork gates must
verify all existing assertions, Accounting and unchanged SoloParity.

## Metric and assertions

`TotalDamage = WithCast.AllDelta - NoCast.AllDelta`.

Each window's AllDelta is the change since the cast point in the sum of all
ERHDamageSource totals plus Ultimate execute value. Snapshots precede Start, so
immediate cast output counts. Each row runs with cast first, from a fresh seeded
world. Its window lasts from cast until 2 s after the Intercession's active window
ends (the existing EndStep rule, TailSteps=120), capped at MaxSteps=1800 (30 s),
with the existing cap warning. A second fresh world starts identically but never
casts and runs exactly that many steps with identical dash indices. No controls
are cached or shared across rows.

This measures the marginal contribution of reshaped/amplified hero rounds,
executions, reactions and kill effects without changing their creator attribution.
Execute value records remaining HP once outside source totals and power-split
shares; source damage includes overkill. Ultimate DamageDealt remains diagnostic.

Existing measurement assertions include:

- 106 rows: the 100 combinations followed by six base references;
- TotalDamage > 0 for the six base rows only (scheduler-authorized narrowing);
- row 0's with-cast AllDelta repeats exactly with `==` in a fresh world;
- row 0's no-cast AllDelta repeats exactly over the same Steps.

Combination rows <= 0 are AddInfo balance findings, counted in `nonpositive`;
they are not assertion failures. No band assertion is added: `outside20` still
reports the spec section 1.4 [GATED] band. Each patron mean uses its combination rows, including non-positive ones.
The informational global mean uses all 100 marginal values. `FRHIntercessionAccounting` remains untouched.

The per-row format is unchanged:
`IntercessionStrength <combo> TotalDamage=%.1f ult=%.1f execute=%.1f steps=%d DamageDealt=%.1f weapon=%.1f dashserial=%u weapons=%d ulthits=%d`.

Only TotalDamage's meaning changes; the other fields come from the cast window.
A separate line adds paired-run diagnostics:
`IntercessionStrengthDiag <combo> withcast=%.1f nocast=%.1f attributed=%.1f steps=%d`.
Both determinism pairs print on one line at %.17g. The summary retains its format
and appends ` nonpositive=%d`. CSV non-positive findings use rounded values; the
optional gate-log count uses the harness's full-precision classification.

## Deterministic start

Immediately after creating the world context, before StartRun or any spawn,
ReseedAll resets HitRng, FMath Rand/SRand, enemy EliteRng and effect-manager
LocalRng to RunSeed 67. StartRun seeds all run streams, including Combat, through
DeriveStreamSeed; Combat is not separately re-seeded. Ultimate GameRng delegates
to the bound effect manager, which uses Combat (LocalRng is its fallback).
Weapon Init already seeds its RNG with 0x5EED0000 + slot; hunters use that
weapon stream and detached phantom volley RNGs copy it. Hunters and reactions own no independent streams.
The local crowd stream retains CrowdSeed; local ultimate shape streams derive
from deterministic gameplay seeds/positions. Cosmetic FxRng, SparkRng and
impact-FX RNG are untouched. Both worlds therefore have identical seeded starts,
although the cast may change later RNG consumption and field evolution.

## Attribution

Closing grants Seek to every Shade variant, including Economy. The grant reaches
composition through the normal weapon behaviour version/rebuild path. Economy's
instant payday adds a timed Damage modifier and blessing-owned Seek; it does not
create a separate direct damage instance. The merged Seek behaviour retains
Ult.Shade provenance, while its main rounds keep their creator attribution.

Historical i7a3 attribution change (superseded by the paired Hound shape described above): the Seek-to-Ultimate main-round override was reverted: RHRunStats.h requires
attribution by creator, never amplifier. Main rounds retain their creator even
when Ultimate-granted Seek reshapes them. Only the assertion "Ultimate hunting
rounds count as Ultimate" in FRHWbUltimateSeekAccountingTest is removed; all
other assertions remain. TagUltimate remains for derived instances. Gameplay
damage, trajectory, cadence and the remaining attribution plumbing are unchanged.
The marginal metric captures these rounds' contribution without retagging them.

Start/Advance/Finish retain restoring Ultimate scopes; nested reactions retain
Reaction. Direct/layer Hit output and deferred novas use the existing attribution
route. Explicit Ultimate executes record pre-kill HP through the existing helper.
Manager-applied Burn retains latest-applier ownership, including Ultimate Status
calls, with no stack, duration or damage changes. Derived split/scatter/echo/charge/
dash specs, Collector main rounds and Phantom Gunner graphs retain their existing
PowerSplitSource routing. Blended Ultimate/blessing kinds use Ultimate accounting.

Retained limits: Collector modifier children keep their original creators;
bullet-applied Burn keeps its Modifier path; enemy-owned crush and automatic Mark
executions keep existing accounting. The optional I2 ShatterEnemy API is absent;
current layers use Hit, and a future implementation needs its own attribution.
Probe/Batch behaviour and SoloParity's existing golden are unchanged.

## Historical results (before the L5 package fixture)

Prior-fixture measurements from the earlier gate log (not validation of the L5 package fixture)
(`Saved/Logs/RogueHero.log`, `RogueHero.Balance.IntercessionStrength.Measure`):

- Mean: **11207.2**.
- Minimum: **Shade.Economy+Quicksilver**, ratio **0.0051**.
- Maximum: **Storm.Tempest+Radiance**, ratio **4.1736**.
- Outside +/-20% of the mean: **outside20=92**.
- Non-positive combination rows: **nonpositive=0** (balance findings, not failures).
- Row-0 with-cast AllDelta: first **29396.293718338013**, repeat **29396.293718338013**.
- Row-0 no-cast AllDelta: first **285.60000610351562**, repeat **285.60000610351562**.

Both row-0 determinism pairs match exactly. The six base-row positivity assertions
passed in the round-1 gate run. These historical numbers must not be used as results for the new fixture.

Double-run wall time (`IntercessionStrength wall_seconds=`): **2.932 s**.
The measured **2.932 s is under the 120 s double-run budget**, including the two
row-0 repeat windows.

Round-1 gates: build **0 warnings**; non-unity **PASS (18 files)**; full RogueHero
suite (including Accounting, ShadeLayers and SoloParity) **714 passed / 0 failed**;
TextAudit **0 violations**. No editor or automation tests are rerun for this
documentation-only fix round. String tables are untouched.

## Pass-1 tuning (D-66 per Keeper, main b82e12be)

All values remain **GATED**. The rule is factor = 1 / the mean ratio within the patron; the existing value is multiplied by that factor. The scheduler supplied the factors below.

| Patron | Combination/variant | Param | Factor | Old | New |
|---|---|---|---:|---:|---:|
| Storm | Radiance | LayerBoltDamageMult | 0.539 | 0.7 | 0.3773 |
| Storm | Quicksilver | LayerBoltDamageMult | 1.365 | 0.7 | 0.9555 |
| Storm | Tide | LayerBoltDamageMult | 1.478 | 0.65 | 0.9607 |
| Forge | Tide | LayerSlamDamageMult | 1.549 | 0.9 | 1.3941 |
| Tide | Storm | LayerWaveDamageMult | 0.726 | 0.7 | 0.5082 |
| Tide | Radiance | LayerWaveDamageMult | 1.356 | 0.7 | 0.9492 |
| Tide | Shade | LayerWaveDamageMult | 1.346 | 0.7 | 0.9422 |
| Radiance | Storm | LayerBeamDamageMult | 1.281 | 0.7 | 0.8967 |
| Radiance | Shade | LayerBeamDamageMult | 0.796 | 0.9 | 0.7164 |
| Storm | ArcGrid (variant) | NetDamage | 0.570 | 30 | 17.1 |
| Storm | Downpour (variant) | Damage | 2.0 (capped) | 45 | 90 |
| Tide | Frost (variant) | VariantDamageMult | 0.741 | 1.0 | 0.741 |
| Tide | Whirlpool (variant) | VariantDamageMult | 0.795 | 1.0 | 0.795 |

Downpour is capped at x2.0 and may remain below the band.

ArcGrid has no variant-specific sky-strike damage key: strikes use the shared Thunderhead Damage fallback, so only NetDamage was scaled. Tide reads VariantDamageMult through Param with default 1.0; it multiplies the three Begin Hit paths (Frost, Breakwater, base/Whirlpool) alongside LayerWaveDamageMult, plus TickLayer Storm current and Quicksilver dash shatter, ThawLayer Forge steam, and OnPoolRounds shatter. Tide Status calls only apply Freeze/Chill/Mark, with no direct status damage; status magnitudes/durations, execute rules and weapon lens amplification remain unchanged. No authored DA_IntercessionLayer_* or DA_IntercessionVariant_* assets exist under Content/RH/Data; code-fallback parity is complete, with no asset parity outstanding. No SoloParity golden was re-recorded.

Untouched out-of-range variants are design questions for the user: Forge Hammer, Furnace and Slam; Shade Economy, Hound and Collectors; Quicksilver Rebound, Momentum and Vector; Tide Stillness.

Results must be remeasured by the cowork gates, including pairwise non-identity checks. No measurement fixture, seeds, windows, thresholds or band assertion changed.

Local validation: TextAudit passed with 0 violations; scoped diff whitespace checks passed. Static comparison against HEAD confirmed exactly nine layer values and four variant Params changed, with all other Params preserved. Build.bat and all four requested non-unity compilation attempts failed with UnrealBuildTool exit -532462766; no successful compilation or automation result is claimed. Full automation, SoloParity, pairwise non-identity and strength remeasurement remain for the cowork gates. String tables and goldens are unchanged.

## Pass-2 tuning (main b31b2310)

Designer decision 2026-10-26: amplifier variants are valued by what they add to
the hero's own damage (the with-cast minus no-cast metric stays). Weak variants
are retuned toward their Keeper's mean in steps of at most x2 per pass.
All values below remain **GATED** in the `_CodeFallback` definitions. No authored
`DA_IntercessionVariant_*` or `DA_IntercessionLayer_*` assets were found under
`Content/RH/Data`; no assets were created.

| Patron | Variant | Param | Measured ratio | Factor | Old -> new |
|---|---|---|---:|---:|---|
| Forge | Slam | MeteorDamage | 0.06 | 2.0 | 110 -> 220 |
| Forge | Furnace | GroundDamage | 0.20 | 2.0 | 8 -> 16 |
| Forge | Furnace | SlamDamage | 0.20 | 2.0 | 28 -> 56 |
| Quicksilver | Momentum | PressureWaveDamage | 0.25 | 2.0 | 35 -> 70 |
| Shade | Collectors | VariantDamageMult | 0.03 | 2.0 | absent (1.0) -> 2.0 |
| Shade | Hound | VariantDamageMult | 0.06 | 2.0 | absent (1.0) -> 2.0 |
| Quicksilver | Vector | VariantDamageMult | 0.09 | 2.0 | absent (1.0) -> 2.0 |
| Tide | Stillness | VariantDamageMult | 0.32 | 2.0 | absent (1.0) -> 2.0 |

`URHUltimate_Forge::Meteor` reads `MeteorDamage`, `Tick` reads `GroundDamage`,
`Slam` reads `SlamDamage`, and `URHUltimate_Quicksilver::Tick` reads
`PressureWaveDamage`; no field substitution was needed. Counts, durations,
radii, cooldowns, chances, time scales and every layer Param are unchanged.

New default-1.0 Param readers are `URHUltimate_Shade::GetVariantDamageMult` and
`URHUltimate_Quicksilver::GetVariantDamageMult` in `RHUltimate_Paths.h`.
In `RHUltimate_Paths.cpp`, Shade `Begin` supplies the multiplier to the paired
Hound Pierce grant and each Ultimate hunter spawn. `RHSpecComposition::Compose`
applies that grant's DamageMult only to the existing paired Ultimate Hound main
round. `URHHunterSubsystem::Spawn` stores a per-hunter multiplier; `Step` applies
it to copied-round and detached-hit damage, retaining it for the hunter's lifetime.
Ordinary hunters and creator-attributed hero rounds are not scaled.
Shade `MarkScreen` scales its Forge Burn; in `RHUltimate_ShadeLayers.cpp`,
`ExecutionLayer` scales Forge blasts and `RadianceClose` scales lance damage.
`TickLayer` novas inherit the scaled lance once, without a second multiplication.

Quicksilver `GrantQuicksilverWeapons` scales dash volley damage alongside the
existing `LayerVolleyDamageMult`, and `Tick` does the same for pressure waves.
In `RHUltimate_QuicksilverLayers.cpp`, `TickLayer` scales Forge rings/Burn and
phantom copies; `ReleaseLayer` scales Storm static hits, Tide shatter and Radiance
lances. Existing attribution and spawn caps are unchanged.

Stillness in `URHUltimate_Tide::Tick` uses effective charge multiplier
`1 + (StillChargeMult - 1) * VariantDamageMult`: 1.6 -> 2.2, doubling only the
0.6 charge bonus to 1.2. `StillChargeMult` remains 1.6. Existing Tide direct and
layer damage readers already use VariantDamageMult.

### Pairing audit and fix-round regressions

Round-3 evidence (2026-10-26): `Saved/Logs/RogueHero.log`, round-2
Measure rows/Diag at lines 3237-3350 and 3385-3530. Values below are
with-cast / no-cast per `ERHDamageSource` (0 Weapon, 2 Blessing, 5 Ultimate).
Modifier, Transform, Reaction and Other are **0 / 0 in every row**; execute
is also **0 / 0**. No new diagnostic line is needed.

| Combination | Marginal damage | / patron mean | Weapon with / no | Blessing with / no | Ultimate with / no | ulthits |
|---|---:|---:|---:|---:|---:|---:|
| Forge.Hammer+Storm | 578229.8 | 3.2629 | 4,549.2 / 6,125.0 | 580,654.0 / 25,139.2 | 24,290.8 / 0.0 | 1025 |
| Forge.Hammer+Tide | 49087.2 | 0.2770 | 5,460.2 / 6,125.0 | 23,766.8 / 25,139.2 | 51,124.4 / 0.0 | 1146 |
| Forge.Hammer+Shade | 599436.2 | 3.3825 | 4,685.2 / 6,125.0 | 595,548.0 / 25,139.2 | 30,467.2 / 0.0 | 1530 |
| Forge.Hammer+Radiance | 548271.0 | 3.0938 | 5,410.2 / 6,125.0 | 544,846.4 / 25,139.2 | 29,278.6 / 0.0 | 1244 |
| Forge.Hammer+Quicksilver | 593025.8 | 3.3464 | 4,041.2 / 6,125.0 | 583,456.4 / 25,139.2 | 36,792.4 / 0.0 | 2321 |
| Storm.Downpour+Forge | 10458.9 | 0.4590 | 592.2 / 2,496.5 | 4,118.0 / 5,651.0 | 13,896.2 / 0.0 | 600 |
| Storm.Downpour+Tide | 9340.1 | 0.4099 | 1,445.4 / 2,496.5 | 4,828.0 / 5,651.0 | 11,214.3 / 0.0 | 127 |
| Storm.Downpour+Shade | 8631.5 | 0.3788 | 1,445.4 / 2,496.5 | 4,828.0 / 5,651.0 | 10,505.7 / 0.0 | 127 |
| Storm.Downpour+Radiance | 32580.3 | 1.4298 | 1,402.2 / 2,496.5 | 4,861.4 / 5,651.0 | 34,464.2 / 0.0 | 168 |
| Storm.Downpour+Quicksilver | 9944.5 | 0.4364 | 1,341.8 / 2,496.5 | 4,453.0 / 5,651.0 | 12,297.3 / 0.0 | 140 |

The patron summaries (lines 6305-6306) give Storm mean **22,786.7**,
min Downpour+Shade **0.3788**, max ArcGrid+Radiance **2.3260** (13 outside);
Forge mean **177,214.6**, min Slam+Quicksilver **0.0201**, max Hammer+Shade
**3.3825** (15 outside). Thus Hammer+Tide is **0.2770** and
Downpour+Radiance **1.4298** of their respective means after pass 2.

- **Forge.Hammer + Tide: fixed (2026-10-26).** `ResolveRing` replaced fire
  push/Burn with pull/Chill on frost rings. That omitted Burn, so those
  deaths could not pass `URHEffect_Forge_T3::OnStatusDeaths`' Burn-bit gate
  and chain explosions; Blessing damage fell from 580,654 to 23,767.
  Following spec §1.3, every Hammer ring now retains its push and Burn,
  and would-be frost rings add Chill without pull. Other Forge variants
  keep their alternation. `RogueHero.Ultimates.ForgeLayers.HammerTide`
  failed in round 1: "victim subsequently dies from Burn" and the dependent
  queued-death/T3/neighbour assertions failed because the single 0.1 s step
  preceded Burn's 0.5 s tick. The corrected test derives surviving HP from
  Slam's Burn Params (20 DPS, 3 s defaults) and steps by RH::SimStep up to
  the Burn duration. It verifies the sole victim's queued death and Burn bit
  before dispatch, then checks T3 damage on a neighbour spawned after the
  ring/death within the Param-derived explosion radius. Furnace remains the
  guard, using its actual ring radius/cadence and an explicit frost-ring count.
  All cadence, radius, damage, push, Burn, Chill, death-chain and non-Hammer
  pull assertions remain. This is test setup only; the corrected test awaits
  cowork gates.
- **Storm.Downpour + Radiance: fixed (2026-10-26).** The release applied
  12 × 60 = 720 per target to 41 targets, contributing 29,520 raw damage.
  The prior log's 168 hits and 33,832.5 raw damage are consistent with
  127 impact-target hits at 90 × 0.3773 plus those 41 giant-target hits;
  this remains an inference from the recorded counters, not a new measurement.
  `RadianceRelease` now splits one budget, 12 × min(stored, 60), evenly
  across all hit targets. Per spec §2.1, bolt count scales the budget below
  saturation; at saturation the total is 720 (17.56 each for 41 targets).
  Empty queries consume the release without damage or division by zero.
  `RogueHero.Ultimates.StormLayers.RadianceBudget` checks actual HP loss
  for 41 clean targets and one clean target within ±1 % of the total budget.
  Existing release tests update only old per-target formula expectations;
  all Params, density selection, radius, flash and storage logic are unchanged.
- **RadianceBolts saturation:** retained only as storage hardening in
  `RHUltimate_Paths.cpp::Strike` and `LightningNet`, **not a pairing fix**.
  Release still clamps the stored count. Existing DownpourRadianceCap storage
  saturation assertions remain unchanged; only its old per-target formula
  expectation changes to the shared budget.
- **Shade execute paths:** Joint Liability and Collection Run have no damage
  term to scale: status application, thresholds and `TryUltimateExecute`
  only. Eligibility and execute accounting are unchanged.

The round-1 `Saved/Logs/RogueHero.log` identifies all seven failed tests:
`QuicksilverLayers.Forge`, `.Radiance`, `.Shade`, `.Storm`, `.Tide`,
`ShadeLayers.Forge`, and `VariantOutput.Pass2DamageRouting` (all under
`RogueHero.Ultimates`). Quicksilver's shared fixture includes Vector and
Momentum: expected dash/layer damage now uses `GetVariantDamageMult()`;
expected Momentum waves read `PressureWaveDamage` instead of the obsolete 35.
The Hound-forced Shade Forge assertion similarly uses its multiplier.
The routing test compared Hound's 28 damage with 19.6 because removing the
ultimate changes the two-weapon DPS rank and slot factor (1 -> 0.7);
its expectation now accounts for both `GetSlotFactor()` values and the
variant accessor, retaining the comparison and tolerance. Fixtures, variants,
assertions and tolerances are preserved. The shared Quicksilver test also
asserts default/unlisted multipliers remain 1.0.

Review of round-1 `RHSpecComposition::Compose` and `RHHunterSubsystem::Spawn/Step`
confirmed damage-only changes: existing `PowerSplitSource`, damage scopes and
RHRunStats routing are unchanged. No attribution code was reverted.
`RogueHero.Balance.IntercessionStrength.Measure` and `RogueHero.Coop.SoloParity`
both passed in that log; no golden was re-recorded.

Results await gate remeasurement. Build, non-unity and automation were not run
in this fix round, as instructed. No measurement fixture, seeds, windows,
thresholds, accounting or layer/variant Params changed this round.

## Pass-3 tuning (main aab942f7)

Designer decision 2026-10-27: over-strong variants are tuned down toward their
Keeper's mean by at most x0.5 per pass; weak variants rise by at most x2
(2026-10-26). The marginal-damage metric and per-patron +/-20% target remain
GATED. These are requested tuning inputs, not newly measured results.

| Variant / layer | Param | Measured ratio | Factor | Old -> new |
|---|---|---:|---:|---|
| Forge.Hammer | SlamDamage | 2.67 | 0.5 | 32 -> 16 |
| Forge.Hammer | BurnDps | 2.67 | 0.5 | absent (20 fallback) -> 10 |
| Shade.Economy | PaydayMult | 2.56 | 0.5 | 3 -> 1.5 |
| Quicksilver.Rebound | VariantDamageMult | 2.44 | 0.5 | absent (1) -> 0.5 |
| Storm.ArcGrid | NetDamage | 1.59 | 0.63 | 17.1 -> 10.773 |
| Tide.Breakwater | VariantDamageMult | 1.30 | 0.77 | absent (1) -> 0.77 |
| Storm.Downpour | Damage | 0.47 | 2 | 90 -> 180 |
| Forge.Furnace | GroundDamage | 0.25 | 2 | 16 -> 32 |
| Forge.Furnace | SlamDamage | 0.25 | 2 | 56 -> 112 |
| Tide.Stillness | VariantDamageMult | 0.48 | 2 | 2 -> 4 |
| Shade.Collectors | VariantDamageMult | 0.09 | 2 | 2 -> 4 |
| Shade.Hound | VariantDamageMult | 0.34 | 2 | 2 -> 4 |
| Quicksilver.Momentum | PressureWaveDamage | 0.44 | 2 | 70 -> 140 |
| Storm -> Radiance | RadianceDamagePerBolt | - | 3 | 12 -> 36 |

All values are in `RHIntercessionVariant.cpp`'s GATED `_CodeFallback` definitions.
`RHUltimate_Paths.cpp::URHUltimate_Forge::Slam` reads `BurnDps` and passes it to
`RHUltimate_ForgeLayers.cpp::ResolveRing`; Hammer now supplies 10 instead of the
shared 20 DPS fallback. `BurnSeconds` stays 3. No substitute field was needed;
Forge T3 and all blessing damage remain unchanged.
`RHUltimate_Paths.cpp::URHUltimate_Quicksilver::GrantQuicksilverWeapons` supplies
`GetVariantDamageMult()` to the Rebound Chain grant's `DamageMult`.
`Combat/RHSpecComposition.cpp::Compose` already reads that field into
`BounceDamageMult` (and the derived cleave bounce); no composition or attribution
rewrite is needed. Existing layer/dash readers also scale Rebound by 0.5.
Vector and Momentum have no Chain grant. Creator attribution, PowerSplitSource,
400/s per-weapon and 6000 global caps remain unchanged.

Stillness's effective charge is now `1 + (1.6 - 1) * 4 = 3.4`.
ArcGrid sky strikes still use the shared Thunderhead Damage fallback.
Storm/Radiance releases one shared budget `36 * min(stored, 60)`: saturation
rises from 720 to 2160, across **every Storm variant + Radiance**. The count 60
and LayerBoltDamageMult 0.3773 stay unchanged. No other layer Param changes.
No authored `DA_IntercessionVariant_*` / `DA_IntercessionLayer_*` assets exist
under `Content/RH/Data`; no assets were created.

### Hammer + Tide root cause and fix

The pass-2 ring fix retained push/Burn and added Chill, but did not cover the
reaction-enabled path. `RHStatus.cpp` allows Burn alongside Chill/Freeze.
`URHReaction_ForgeTide::Fire` explicitly removes Burn and Chill **before** its
blast. This strips the Burn bit from blast deaths and removes the later DoT;
`URHEffect_Forge_T3::OnStatusDeaths` requires that bit. Merely reapplying Burn
after dispatch cannot repair a death that already happened.

Following spec section 1.3, Hammer+Tide `ResolveRing` registers successful ring
Burn with the caster's effect manager, tied to enemy spawn generation and the
original (status-scaled) Burn duration. Forge/Tide skips only consumption of
that registered Burn; it still consumes Chill/Freeze, blasts, staggers and runs
its duo output. No Burn magnitude, duration, death attribution, global status
rule or reaction definition changes. Registrations expire, are pruned on ring
registration, reset with the run, and cannot transfer to a recycled enemy slot.
Other Forge variants keep fire/frost alternation; Hammer never adds frost pull.
This narrow shape-preservation exception is recorded in TDD section 17.

`ForgeLayers.HammerTideChainParity` uses identical seed-67 fields and steps,
forced Hammer, real enemy statuses, enabled pair reactions and the existing
T3 `Explosions` counter. It requires Storm > 0, Tide >= half Storm, and a real
Forge/Tide reaction fire. Existing HammerTide and Furnace guards remain.
Tuned test expectations read Params with their original tolerances; base scale
1.0 and tuned Rebound/Breakwater scales remain explicitly checked.

First round: SoloParity golden untouched. TextAudit passed with 0 violations.
Build, non-unity, the full automation suite, strength remeasurement and pairwise
checks are reserved for cowork gates as instructed. No string tables changed.
The restored chains can change Forge/Tide strength; no layer retuning is included.


### Fix round 1 (2026-10-27; results await gates)

The supplied `Saved/Logs/RogueHero.log` reports 730 passed and three failed.
The failing assertions are:

- Line 6653, `Balance.IntercessionStrength.Measure`:
  `Expected 'pairwise Shade.Hound+Storm=6090.1000509262085 vs Shade.Hound+Quicksilver=6090.1000509262085 (not identical, rel > 1e-4)' to be true.`
  Both diagnostic rows have zero executions, arcs and collections. This is
  pairwise identity, not a determinism failure. Hound's amplified weapon contacts
  were absent from the variant-output execution callback: registration and
  filtering accepted only Collectors' hunters. Storm waited for Closing and
  Quicksilver only swept the hero's dash; neither layer reached Hound's remote
  rounds. `URHUltimate_Shade::ApplyLayerEffect` now also registers Hound with the
  existing `OnHunterRounds` callback. It accepts the caster's actual equipped
  main/derived spec IDs (`GetSpecId`, `GetDerivedIds`), not attribution-only pins
  or unrelated summons. Those contacts use the existing Storm execution/arc
  path, or the existing Quicksilver collection/refund path only during a dash.
  Alive, active-duration, Mark and execute-threshold checks remain. No damage,
  execute threshold or attribution routing changed. Measure's fixture, seeds,
  windows, accounting, row count, exact repeats and pairwise threshold are untouched.
- Line 37490, `ForgeLayers.HammerTideChainParity`:
  `Expected 'Forge/Tide reactions still fire on Hammer layer hits' to be true.`
  The fixture called `Bind` after granting blessings; `Bind` clears active pairs
  and does not call `Refresh`. Added `Refresh` after binding to actually enable
  the pair. Victim HP now uses each cast's `LayerSlamDamageMult` and Hammer's
  resolved `BurnDps`/`BurnSeconds`, with a fraction of one Burn tick above ring
  damage. Both runs retain seed 67, eight enemies, identical positions and
  steps, and forced `Forge.Hammer`; only the second Keeper and consequently
  its derived survival HP differ. Forge advances every fixed step throughout
  the Burn window. `GetKillStatusMask(Index)` is already exactly the passing
  HammerTide accessor: its implementation accepts a kill-buffer index and
  subtracts `LastKillBase`. Kept that index and documented the contract; an
  enemy-slot substitution would be incorrect in this checkout. All three
  acceptance assertions, HammerTide and the Furnace guard remain unchanged.
- Line 38565, `StormLayers.RadianceBudget`:
  `Expected 'target survives its share' to be true.`
  The fixed spawn HP multiplier was sized for the old budget and failed the
  single-target survival check after the release budget tripled. Cast first to
  resolve Params, then size each target to twice its saturated-budget share,
  converting HP to the `Spawn` HP multiplier with the Swarm definition's HP.
  Expectations use `RadianceDamagePerBolt * min(RadianceBolts, RadianceMaxBolts)`
  divided by hit count, retaining every assertion and the same 1% tolerance.
  Rebuild the spatial hash after spawning. `DownpourRadianceCap` already derives
  its budget and share from Params; it contains no stale 720 / 17.56 expectation
  and needs no change.

Hammer+Tide re-trace: the gameplay cause remains Forge/Tide consuming Hammer's
ring Burn before the reaction blast, erasing its death bit and later DoT.
The existing spec section 1.3 exception remains narrowly scoped to registered
Hammer+Tide Burn, with generation and expiry checks on the caster's manager.
`FRHEnemyStatuses::Tick` thaws Freeze independently; `ClearKinds` only clears
Burn when its own bit is requested. Consuming Chill clears Chill/Freeze, not
Burn. The other reaction `RemoveStatus(Burn)` is Forge/Radiance's detonation,
not the Forge/Tide pair in this fixture. `GetStatusTarget()` falls back to the
world's enemy subsystem in the real path, so the identity guard accepts it.
`ForgeHammerBurns` is instance state and `ResetAll` clears it; fresh worlds do
not share it. No additional gameplay Burn exception was indicated by this
trace. Corrected parity should now exercise the existing fix, including Burn
bits on reaction-blast deaths; its numerical result still requires the gates.

Both edited Economy payday comments now say ASCII `x1.5`. All pass-3 tuning,
Rebound wiring, Slam/Vector and SoloParity goldens remain unchanged. No editor,
build or automation was run in this fix round, as instructed. New measurements,
parity, non-unity, text/localization audits and the complete suite await cowork
gates; no success or new strength numbers are claimed.

## Redesign candidates

Both are **flagged for the designer**, with no pass-3 tuning.
`Saved/Balance/intercession_strength.csv` and `Saved/Logs` are absent from this
worktree. Consequently the requested per-row Ult / weapon / blessing / execute
breakdowns (including withcast/nocast deltas) cannot be recovered here; no source
shares are invented. `AttributedUltimate` alone would not identify all shares.

| Candidate | Ult share | Weapon share | Blessing share | Execute share |
|---|---|---|---|---|
| Forge.Slam | unavailable | unavailable | unavailable | unavailable |
| Quicksilver.Vector | unavailable | unavailable | unavailable | unavailable |

Forge.Slam improved only from 0.06 to 0.08 of its patron mean after x2.
`RHUltimate_Paths.cpp::Begin/Tick` reads `SlamInterval = 99`, stopping rhythm
rings after the first; its recurring direct damage comes only from dash meteors
through `Meteor`. `MeteorDamage` stays 220. Increasing that damage does not
create more dash opportunities or restore ring coverage.
**First-order linear estimate** from the supplied ratios: at the pre-pass-2
setting the Param-scaled share was about 0.02 and the fixed share about 0.04.
After pass 2 the scaled share is about 0.04, so `(1 - 0.04) / 0.04 = 24` times
the current damage would be needed to reach the old reference mean. This is an
estimate, not a measured source decomposition; a moving mean, overkill and
kill-chain effects invalidate strict extrapolation.

Quicksilver.Vector improved from 0.09 to 0.12 after x2. Its only variant damage
lever is `VariantDamageMult` (still 2), alongside `VectorDashCooldown` (still
0.2). `Begin` changes dash availability; `GrantQuicksilverWeapons` scales dash
volleys, and layer readers scale their outputs. It grants no ricochet output.
Scaling damage cannot create missing dash contacts or additional damage shapes.
**First-order linear estimate:** the pre-pass-2 scaled/fixed shares were about
0.03/0.06, making the current scaled share 0.06; `(1 - 0.06) / 0.06 = 15.67`,
roughly x16 more, would be needed at the old mean. This is likewise an estimate,
not an Ult/weapon/blessing/execute breakdown. Neither candidate can approach the
mean in one allowed x2 pass under that linear model; redesign is a designer call.

## Pass-4 tuning (main a1a34d4b)

Designer decisions 2026-10-26/27: weak values rise by at most x2 and strong
values fall by at most x0.5 per pass, toward each patron's own mean marginal
damage on the refilled field (spec section 1.4). Results await gate
remeasurement. No new measurements are claimed. Ratios below are only those
supplied in the job brief, measured before this fix round.

| Variant / layer | Param | Measured ratio | Factor | Old -> new |
|---|---|---:|---:|---|
| Forge.Hammer | BurnSeconds | 2.63 | 0.5 | absent (3 fallback) -> 1.5 |
| Forge.Hammer | SlamDamage | 2.63 | 2 (pass-3 revert) | 16 -> 32 |
| Forge.Hammer | BurnDps | 2.63 | 2 (pass-3 revert) | 10 -> absent (20 fallback) |
| Shade.Economy | PaydayMult | 2.00 | 0.5 | 1.5 -> 0.75 |
| Quicksilver.Rebound | VariantDamageMult | 2.02 | 0.5 | 0.5 -> 0.25 |
| Storm.ArcGrid | NetDamage | 1.38 | 0.72 | 10.773 -> 7.7566 |
| Forge.Furnace | GroundDamage | 0.30 | 2 | 32 -> 64 |
| Forge.Furnace | SlamDamage | 0.30 | 2 | 112 -> 224 |
| Shade.Collectors | VariantDamageMult | 0.24 | 2 | 4 -> 8 |
| Shade.Hound | VariantDamageMult | 0.75 | 1.33 | 4 -> 5.32 |
| Storm -> Radiance | LayerBoltDamageMult | 0.65 | 1.54 | 0.3773 -> 0.5810 |
| Tide -> Storm | LayerWaveDamageMult | 1.30 | 0.77 | 0.5082 -> 0.3913 |
| Tide -> Radiance | LayerWaveDamageMult | 0.77 | 1.30 | 0.9492 -> 1.2340 |
| Tide -> Shade | LayerWaveDamageMult | 0.77 | 1.30 | 0.9422 -> 1.2249 |
| Radiance -> Shade | LayerBeamDamageMult | 0.76 | 1.32 | 0.7164 -> 0.9456 |

Values are the requested rounded targets in the GATED `_CodeFallback`
`Defs()` / `LayerDefs()` in `Build/Ultimates/RHIntercessionVariant.cpp`.
A recursive inventory confirms no authored `DA_IntercessionVariant_*` or
`DA_IntercessionLayer_*` assets under `Content/RH/Data`; none were created.
Downpour, Stillness, Momentum, Slam, Vector and all other layers are unchanged.

### Hammer trace

The single halved Hammer-specific lever is **BurnSeconds**, resolved by
`URHUltimate_Forge::Slam` in `Build/Ultimates/RHUltimate_Paths.cpp` and passed
to `ResolveRing` in `Build/Ultimates/RHUltimate_ForgeLayers.cpp`. The ring hits,
then applies Burn to survivors through `URHUltimate::Status` and
`URHEffectManager::ApplyStatus`. `Enemies/RHStatus.cpp::FRHEnemyStatuses::Apply`
refreshes duration and keeps the stronger DPS; Burn does not stack.
`RebuildMask` sets the Burn bit while `BurnTime > 0`.
`URHEnemySubsystem::KillWithEvent` captures that mask before retiring the enemy;
`URHEffectManager::DispatchStep` forwards it to
`Build/Effects/RHTierEffects.cpp::URHEffect_Forge_T3::OnStatusDeaths`.
T3 requires the Burn bit, then uses its own explosion damage and chain Burn
Params, independent of Hammer's SlamDamage and BurnDps.

Thus ring Burn duration controls the eligibility window for subsequent deaths
to seed T3 chains, including weapon kills after a ring contact. Hammer+Tide's
existing `PreserveForgeHammerBurn` registration also takes this duration (plus
unchanged status DurationAdd), preserving the same shortened window through
reaction consumption. The new Hammer-only override halves the base ring Burn
window from 3 to 1.5 seconds; it does not halve any shared blessing duration.
Refreshes, other Burns and status bonuses can extend actual eligibility.

Pass 3's reported 2.67 -> 2.63 ratio is consistent with reducing direct hit/DoT
damage without reducing the death-bit window or T3's independent explosion
payload. That trace explains why those Params do not directly scale chain
damage; it is not a measured source decomposition or a guarantee of a linear
response. Restore SlamDamage 16 -> 32 and remove BurnDps 10 (20 fallback).
Forge T3, all blessing data, ring radius, cadence and counts are untouched.
The actual effect of the shorter eligibility window awaits gate remeasurement.

### Radiance wiring result and regression coverage

**Already wired; no runtime wiring was added.**
`RHUltimate.cpp::ApplySecondKeeperLayer` copies layer Params after Begin (or
wind-up), respecting variant overrides. `RHUltimate_Paths.cpp::URHUltimate_Radiance::Tick`
reads `Param("LayerBeamDamageMult", 1.f)` into TickDamage, sends it through
`Hit(e, TickDamage)`, and scales Solar SunDamage and overkill novas too.
`RHUltimate_RadianceLayers.cpp::TickRadianceLayer` also reads the same budget.
The unchanged measured ratios in the brief (Storm 1.19, Shade 0.76) therefore
do not establish a missing multiplier on this checkout.

The five existing pass-1 layer values (Storm / Forge / Tide / Shade /
Quicksilver) were already active: 0.8967 / 0.7 / 0.8 / 0.7164 / 0.7.
Only Shade changes to 0.9456; there is no new activation of the other four.
`RadianceLayers.BeamDamageParam` compares actual enemy HP loss with the
layer's authored multiplier versus half, using seed 67, the same stationary
field and steps, forced Radiance.Prism and each committed second Keeper.
The test changes the definition before cast resolution and restores it with
RAII. No attribution, caps or beam shape changes are involved.

Rebound's remaining fixed expectation now reads Params. Hammer Burn HP sizing,
HammerTideChainParity and Economy already read their Params and retain their
assertions and tolerances. The measurement fixture, pairwise relative-difference
threshold > 1e-4 and SoloParity golden are untouched; no golden was re-recorded.
Build, non-unity, automation and text/localization gates are left to cowork as
instructed. No new gate results are claimed.

### Fix round (2026-10-27)

The supplied round-2 gate log identifies the following exact failing assertion
lines. These are historical failures, not new results. Log line numbers refer
to `Saved/Logs/RogueHero.log`; each copied line also includes its source location.

**RogueHero.Balance.IntercessionStrength.Measure**

Log line 6655:

```text
[2026.10.26-13.50.34:823][649]LogAutomationController: Error: Expected 'pairwise Shade.Collectors+Storm=460.40007209777832 vs Shade.Collectors+Quicksilver=460.40007209777832 (not identical, rel > 1e-4)' to be true. [E:\RogueHero-cowork\i7i-ult-tuning-pass4\Source\RogueHero\Build\Tests\RHIntercessionStrengthTests.cpp(671)]
```

**RogueHero.Ultimates.Gating.LayerTable**

Log line 37671:

```text
[2026.10.26-13.51.20:966][ 88]LogAutomationController: Error: Expected 'Tide wave budget authored' to be true. [E:\RogueHero-cowork\i7i-ult-tuning-pass4\Source\RogueHero\Build\Tests\RHIntercessionGatingTests.cpp(148)]
```

Log line 37672:

```text
[2026.10.26-13.51.20:966][ 88]LogAutomationController: Error: Expected 'Tide wave budget authored' to be true. [E:\RogueHero-cowork\i7i-ult-tuning-pass4\Source\RogueHero\Build\Tests\RHIntercessionGatingTests.cpp(148)]
```

**RogueHero.Ultimates.VariantOutput.HuntersQuicksilver**

Log line 38971:

```text
[2026.10.26-13.51.22:028][169]LogAutomationController: Error: Expected 'hunter output collects during dash' to be true. [E:\RogueHero-cowork\i7i-ult-tuning-pass4\Source\RogueHero\Build\Tests\RHVariantOutputLayerTests.cpp(210)]
```

Log line 38972:

```text
[2026.10.26-13.51.22:028][169]LogAutomationController: Error: Expected 'collection refunds the hero dash' to be true. [E:\RogueHero-cowork\i7i-ult-tuning-pass4\Source\RogueHero\Build\Tests\RHVariantOutputLayerTests.cpp(217)]
```

**RogueHero.Ultimates.VariantOutput.HuntersStorm**

Log line 38992:

```text
[2026.10.26-13.51.22:053][172]LogAutomationController: Error: Expected 'hunter execution arcs' to be true. [E:\RogueHero-cowork\i7i-ult-tuning-pass4\Source\RogueHero\Build\Tests\RHVariantOutputLayerTests.cpp(215)]
```

Log line 38993:

```text
[2026.10.26-13.51.22:053][172]LogAutomationController: Error: Expected 'arc applies Shock through Status' to be true. [E:\RogueHero-cowork\i7i-ult-tuning-pass4\Source\RogueHero\Build\Tests\RHVariantOutputLayerTests.cpp(217)]
```

Root causes and changes:

- **Measure:** Collectors+Storm and Collectors+Quicksilver both report
  `TotalDamage=460.40007209777832`. Diagnostics at log lines 4702 and 4894
  both show `withcast=6342.8`, `nocast=5882.4`, `attributed=751.8`,
  `steps=480`, `executes=0`, `arcs=0`, `collections=0`, `refunds=0`.
  This is pairwise identity, not a base-row or determinism failure.
  `ApplyHits` kills lethal recipients before broadcasting `OnResolvedRounds`;
  `OnHunterRounds` then discards dead recipients. At the pass-4 multiplier,
  the variant kills outright and both layers lose their contact output.
  Spec section 1.3 shape preservation: `ApplyHits` now separately batches the
  actual lethal non-boss contacts, retaining the killing spec and position
  before slot reuse. Shade subscribes only for Collectors/Hound and checks
  the same explicit hunter provenance / equipped Hound spec IDs. These
  contacts carry Storm arcs or dash-gated Quicksilver collection/refund once,
  through the existing layer routes. Surviving recipients still require the
  existing threshold execution. A lethal recipient is not executed again and
  contributes no extra execution damage credit. Bosses, skipped duplicate
  contacts and unrelated killing specs cannot acquire this layer output.
  Both delegates are removed on End. No fixture, accounting, seeds, windows,
  row count, thresholds or Params in Measure changed.
- **HuntersStorm / HuntersQuicksilver:** the attribution-only survival and
  provenance assertions passed. The next, genuine hunter contact killed the
  200-HP recipient before the callback, so arcs/Shock and collection/refund
  were absent. The same gameplay fix restores these lethal-contact paths.
  All original assertions and the original lethal fixture remain. Each test
  also runs a survivor fixture: cast first, derive maximum HP from the
  resolved variant multiplier, convert to a Spawn multiplier using the
  registered Swarm definition HP, and wound to 20%. Added assertions prove
  survival of copied-round damage, the resolved threshold precondition,
  actual remaining-HP execution, no repeated layer output from a dead
  recipient, rejection of an attribution-only lethal hit followed by a stale
  hunter contact, and removal of both delegates on Finish. There was no hard-coded pass-3 damage expectation to replace.
- **LayerTable:** its old `LayerWaveDamageMult <= 1` budget assumption rejects
  the approved Tide/Radiance 1.2340 and Tide/Shade 1.2249 values. This is not
  an L8 gating failure. The assertion retains positivity and now checks exact
  equality of a committed Tide cast's resolved Param with
  `FindLayer(...)->Params`, with no added tolerance. Existing finite-value,
  collision, key and gating assertions remain. Radiance wiring and its
  BeamDamageParam regression are unchanged; the supplied Radiance/Storm
  wiring-note ratio remains 1.19.

All pass-4 values are unchanged. The table above now includes every supplied
ratio, including both Furnace rows. SoloParity's golden and all string tables
are untouched. No editor, build or automation was run. Build, non-unity, the
full RogueHero suite, TextAudit and localization results await the cowork
gates; no new measured results or green gates are claimed.
