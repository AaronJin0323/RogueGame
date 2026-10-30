# S2 Keepers: Shade, Radiance, Quicksilver (2026-10-31)

Branch `dev/s2-keepers`, from `dev/s2`. Class: SYSTEM. Layer: Keeper content and the weapon flags under it (`keepers.md` §9). Replaces nothing: S1 rows and numbers are untouched. Bot and tool readings only. **A bot reading is never a human reading**, and no human has played any of this.

## Reproduce
```
& Tools\BotRuns.ps1 -ArmSet S2Keepers -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 10 -Stagger 0.4 -Out Saved\BotRuns\s2k_v2
python -I Tools\probe.py intercession Saved\Probes\s2k_ik --seeds 2 --stage S2
& Tools\BotRuns.ps1 -Jobs Saved\Probes\s2k_ik\jobs.csv -Parallel 10 -Stagger 0.4 -Out Saved\Probes\s2k_ik\runs
python -I Tools\probe.py ablation Saved\Probes\s2k_ab --builds Saved\BotRuns\s2k_v2 --stage S2
```
The ablation job list was cut to builds 1 to 5 per arm (258 runs of 792); Solar has 3 complete builds.

## Failing first
`Dusktracer.Keepers.S2.*` went in over stubs (commit 1a445f3): 19 of 20 reached were red, then a test crash stopped the batch, so the later tests (the casts among them) were not seen red. Several test faults were found on the way to green, in the tests, not the game. One game fault was found by them: a hunter fanned a rifle's two rounds across a cone and missed its target.

## S1 did not move
Six S1 arms, 15 seeds, v2 at skill 1.0, this branch with nothing unlocked: every cell equals the `control` column of `S2_arsenal.md` (66 wins of 90). The row walk, the Stage column and the two new statuses changed no S1 run at this reading.

## Bot battery, skill 1.0, 15 seeds per arm
Cell: boss reach, wins, median death minute.

| Arm | Reach, wins, death | Keeper share at L16 | Dmg share/min 600-780 s |
|---|---|---|---|
| Shade/Hound:Storm | 15/15 15 - | 0.71 | 0.013 |
| Shade/Hunters:Forge | 15/15 14 15.5 | 0.77 | 0.032 |
| Shade/Economy:Storm | 15/15 11 16.9 | 0.80 | 0.022 |
| Radiance/Lance:Storm | 15/15 15 - | 0.77 | 0.009 |
| Radiance/Prism:Storm | 15/15 15 - | 0.70 | 0.010 |
| Radiance/Solar:Tide | 10/15 8 12.1 | 0.58 | 0.131 |
| Quicksilver/Vector:Forge | 14/15 13 11.3 | 0.57 | 0.018 |
| Quicksilver/Momentum:Storm | 15/15 15 - | 0.58 | 0.005 |
| Quicksilver/Rebound:Storm | 15/15 15 - | 0.66 | 0.002 |
| **All** | 129/135 121 | | |

Damage share by source over the runs, top Keepers: Hound Storm 38 Shade 33; Hunters Shade 64 Forge 13; Economy Shade 40 Storm 39; Lance Storm 49 Radiance 28; Prism Storm 48 Radiance 22; Solar Radiance 44 Tide 15; Vector Forge 38 Quicksilver 23; Momentum Storm 48 Quicksilver 9; Rebound Storm 42 Quicksilver 24.

## T-KEE-15, marginal damage of a cast: FAIL
22 of 144 combinations in band (Quicksilver 9/48, Radiance 12/48, Shade 1/48), 2 seeds. As with S1, these are readings; nothing was tuned against them.

| Variant | No layer | Min | Max | Mean |
|---|---|---|---|---|
| Shade/Hound | 7740 | -23656 | 46268 | 11857 |
| Shade/Hunters | 5201 | -29300 | 48404 | 9991 |
| Shade/Economy | 4569 | -28216 | 453426 | 57768 |
| Radiance/Lance | 17418 | -17488 | 31278 | 14994 |
| Radiance/Prism | 11259 | -13375 | 46367 | 19280 |
| Radiance/Solar | 11420 | -21427 | 65502 | 19663 |
| Quicksilver/Vector | 5086 | -28484 | 47140 | 9386 |
| Quicksilver/Rebound | 10847 | -24444 | 51400 | 14985 |
| Quicksilver/Momentum | 23175 | -13416 | 71838 | 25354 |

## T-KEE-18, ablation at level 16: FAIL
Share 8 of 9 arms, gain 3 of 9, archetype band 6 of 9; mean share 0.676. Five builds an arm (Solar 3), one seed, so a thin reading.

| Arm | Full DPS | Share | Package gain |
|---|---|---|---|
| Quicksilver/Momentum | 9203 | 0.606 | 2.97 |
| Quicksilver/Rebound | 6073 | 0.648 | 1.76 |
| Quicksilver/Vector | 11773 | 0.855 | 0.94 |
| Radiance/Lance | 12484 | 0.749 | 3.73 |
| Radiance/Prism | 7967 | 0.620 | 2.20 |
| Radiance/Solar | 11265 | 0.618 | 2.36 |
| Shade/Economy | 6641 | 0.522 | 1.11 |
| Shade/Hound | 3822 | 0.617 | 2.03 |
| Shade/Hunters | 10777 | 0.852 | 0.96 |

## Perf at caps
Default scene: game p50 2.26, p95 3.05 ms, GPU p95 2.64 raw (4.23 proxied), 5975 bullets, 388 enemies: PASS. A second scene with `DT.Bless` of six S2 cards read the same (game p95 3.06), but `DT.Give` may not be a command and the scene has no Patron or proof that a hunter, nova or Momentum ran: **it says nothing about S2 cost**. The real cost is unmeasured.

## What the source lacked, and what was invented
- Ledger has no flag and Prism's is `Split`, which R-KEE-02 forbids sharing: `Flag.Debt` and `Flag.CritSplit` are new.
- No crit damage number, no crit source but cards; Keen Edge is a stat R-KEE-02 rejects, so it is `charged_crit`.
- Per-archetype tiers: the source has per-Keeper tiers; all 27 lines are drawn from those or invented.
- Layers: the source has 30 per Keeper pair; the build has one per archetype, drawn from them. So a layer does not vary with the patron.
- Souls do not exist (kills stand in); Escrow's heal is held (`kill_extends_s` stands in); Long Arm's gem pull is left out; Radiant Wake is a field at impacts, not a trail.
- Blind is a short Freeze because `Source/Dusktracer/Enemies` was closed to this job.
- The seat rule for unlocked Keepers. Patron traits and the 12 + 12 text of the old imbue are not built, as for S1.

## Not built
- Time Dilation's slow-motion (same gap as Stillness's `time_scale`): the cast is only dash volleys.
- Cracked: control on the boss is refused, not converted (D-60). Frozen and Blinded boss answers are missing, so Lance and Prism lose a line against it.
- HUD meters for Debt and Momentum. The cards are generic and show; the stacks do not.
- Radiance's crit reactions do not run on mine hits (mine impacts carry no crit).
- Seek picks a Marked target at launch only; a round whose target dies takes the nearest, Marked or not.
- Beam fallbacks are unreachable: the Rail lists Projectile first, no weapon resolves Beam.
- `modifier_socketed` still has no production caller: no Keeper reads it, so it stays as the arsenal report left it.

## What reads wrong (for a human session, none of it tuned)
- **Eight of nine arms take almost no damage** (0.002 to 0.032 share/min against a band of 0.3 to 0.5) and 121 of 135 win, against 66 of 90 for the control. Second Keeper Storm is in five of the six cleanest arms and deals about half their damage, so this may be Storm more than the new Keepers; the arms were not rerun with other seconds.
- Shade/Economy's best cast is 453426 against a mean of 57768: an instant or layered payday reads out of all scale on the 40× field. Its low `none` shows the layer does it.
- Every primary has casts of about -25000. Keeper Second Marks or stuns change who the build shoots; same shape as Tide's negative casts in S1, not traced here.
- Hunters and Vector show package gains near 1 with the highest shares: their second Keeper Forge may be doing the work, or the probe's route does not raise hunters or dash where they matter.
- Solar:Tide applied about 1.0M Blinds and 2.8M Freezes in 15 runs, ten times the other Radiance arms: Radiant Wake's field ticks are hits with a slot, so each rolls the enchantment. Probably a lock on the crowd, and the only arm at a real damage rate is this one, so it does not read as a benefit either.
- Relay Chain's free round can kill and fire again; a dense weak crowd may chain to the spawn cap.
- Momentum shows little as a source (9) because it multiplies the weapon's own rounds; its 15/15 cannot be separated from Storm's.
