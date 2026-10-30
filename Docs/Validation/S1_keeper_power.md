# S1 Keeper power: why T-KEE-15 and T-KEE-18 fail (2026-10-31)

Order: diagnosis (§1, committed before any behaviour changed), then bugs, then probe, then numbers. Everything is Bot v1 at skill 1.0, headless: a bound, not a human reading (O-008). Baseline probes: `Saved/Probes/ik`, `Saved/Probes/ab` (`S1_probes.md` §2).

## 1. Diagnosis

Telemetry reads used here: `hit.damage_by_source` per step and `kill` per step, binned over the 12 s window; plus two re-runs of the unchanged build at 40× field HP instead of 10× (`Saved/Probes/ik40`, `Saved/Probes/ab40`; `probe.py --hp-mult`), which separates a limit of the field from a limit of the build.

### 1.1 Causes that cut across the groups

**B1, BUG: the hit event rounds each hit, and sub-0.5 hits vanish.** `FDTEnemyPool::ApplyHits` sends `FMath::RoundToInt32(Dealt)`. A Field ticks `dps × Dt` every step, 0.1–0.25 at these weapons, so every tick rounds to 0. Forge/Furnace with no layer, no cast, 12 s: 367 716 hits, 121 kills, reported damage 6 248 and only 183 of it `Keeper.Forge`. 121 kills at 268 HP is about 32 400 dealt. `Modifier.LingeringField` reads 0 on every arm for the same reason. Enemy HP is right (`Hp[Index] -= Damage` uses the float); only the event is wrong, so this is a telemetry fault, not a damage fault. It sits under T-KEE-15, T-KEE-18, `bot_summary.py` Keeper share and `power_split.py`.

**B2, BUG: `HitTaken` fires on every touch, not on a hit taken.** `FDTWeaponSim::StepSlamTriggers` reads `View.HeroHits`, the touches the enemies published. The hero then refuses most of them (`bProtected` in `FDTHeroSim`: i-frames, dash, `bNoDamage`). So a Slam fires on each contact, in a probe every few steps because the probe hero is `bNoDamage` and has no i-frames to spend. Forge/Slam at 40×: damage steps of 30–45 hits arrive about 8 times a second, while the Arc Blade's `EveryNMelee` allows about 0.45 slams a second. `S1_probes.md` §1 says this trigger never fires in the probe; it is the reverse. It fires far more than in any run, and the real game has a smaller form of the same fault: a touch inside i-frames slams.

What B2 explains:
- **All 6 Slam variants, Tempest with a Slam layer, and the worst Tide layers.** Forge/Slam plain is 75 050 in the window, of which `Keeper.Forge` 72 592. At 40× it becomes 291 916 with the same kills (about 300), so this is a burst limited by the crowd, not by HP. A cast loses because anything that stops contact stops the slams: Forge's rings knock back (120 uu every 0.5 s, 240 uu/s against 280 uu/s of walking), Tide's line and Freeze hold the crowd away. Tide/Frost:Slam at 40×: 42 405 and 51 202 in the first 5 s plain, 3 040 and 919 with the cast. Storm casts do not push, and Storm/Downpour:Slam still goes from +57 207 to −926 at 40×, because `Keeper.Forge` is 258 377 plain, and a Storm cast's extra kills thin the crowd that was feeding it.
- **Most of the T-KEE-18 "second Keeper carries it" reading.** The Keeper cards a Frost, Whirlpool or Stillness build holds include Forge's Slam Key. `Keeper.Forge` is 2 522 of Frost's 4 041 DPS and 3 376 of Whirlpool's 3 976. Removing Frost's own cards leaves the Slam Key running, so gain sits at 1.

**A1, probe artefact: the field has a ceiling.** Field HP is 268 and kills sit at 200–222 in 10 s on every strong arm, so the crowd's arrival caps DPS near 6 000. Every Forge arm and two Tide arms read 4 000–6 000 full DPS. At 40× full DPS goes to 11 300–22 900 with kills still 141–210. A build at the ceiling cannot show a cast's gain and cannot show a package's gain, and it inflates the share: weapon-level and modifier drops vanish when the ceiling holds without them.

**A2, probe artefact: the route walks away from a crowd the cast moved.** The hero keeps its circle whatever the crowd does. `Weapon.ArcBlade` (reach 220–275 uu) is the largest weapon line in this loadout. Forge is unaffected; on Tide it falls to 0 for 2–5 s: Frost 3 930 → 1 845, Stillness 3 714 → 718, and on Whirlpool it gets 3 920 in the first 2 s, the whole ball inside the arc, then 0 for 3 s as the hero leaves. Rifle and drones hold their numbers. A player walks to what they froze; a fixed route cannot.

**A3, probe artefact: one dash in a 5 s cast.** The route dashes every 6 s, so Slam's variant, whose whole content is "every dash landing slams and refills", lands once in its cast.

### 1.2 T-KEE-15, per group

| Group | Reading | Cause | Class |
|---|---|---|---|
| Slam (6) | −14 036 to −4 222 | B2: the cast stops the contact that drives the slams. A3 hides the variant. The rings' 11 210 are real | (b), (a) |
| Tempest + Slam layer | −1 341 | B2 | (b) |
| Frost (7) | −27 285 to −1 403 | B2 on Forge layers; A2 for the rest. **B3 below** | (b), (a) |
| Stillness (5 of 6) | −7 909 to −1 958 | A2. Charged volleys do report: `Weapon.PulseRifle` moves to `Keeper.Tide` as R-WPN-07 says, so no fire is suppressed | (a) |
| Whirlpool (4 of 7) | −13 254 to −1 745 | B2 on Slam; A2 for the rest | (b), (a) |
| Tide mean −4 868 | | the sum of the above. The cast also deals no damage of its own, so what remains after the fixes is (c) or a design reason | see §5 |
| Furnace 28 000–33 300 | over the Forge mean | real: rings for 8 s against 5, burning to the cap. B1 understates it and its control | (c), B1 |
| Forge layers on Storm | Downpour 6 471–10 732 against 29 866 | A1 and B2: the plain run is already at the ceiling | (a), (b) |

**B3, BUG: Frost's cast never pays its `frozen_vuln`.** The variant is `Flag.StatusApply{status=Status.Freeze;scope=Screen;duration_s=4.0;frozen_vuln=0.50}`. `TidePull` reads only `duration_s`; nothing folds the clause into the flags, so the card's "Frozen take +50 %" (`ST_Keepers`, Tide.Frost.Ult.Desc) stays the Key's 30 %. `scope=Screen` needs no reader: `C.INTERCESSION.TIDE_RADIUS_UU` equals the camera frame radius.

**B4, BUG: Frost's layer does nothing on two Storm variants.** `Flag.StatusApply{status=Status.Chill;stacks=2;on=IntercessionHit}` rides only on `FDTKeeperSim::HitCircle`. Tempest and Downpour casts reshape weapon flags and never call it, so "Intercession hits add 2 Chill" has no hit to ride. Storm/Tempest:Frost reads 8 133 against 8 240 with no layer, and its Chill applications are the enchantment's alone (739 with the cast, 420 without; 2 893 with the fix).

### 1.3 T-KEE-18, per group

| Group | Reading | Cause | Class |
|---|---|---|---|
| Furnace gain 0.90 | full 4 545, without package 5 053 | B1 hides the Field, the package's main line (hits 85 636 against 6 446). A1 and B2: the Forge Key cards left in the build reach the ceiling alone. At 40× still 0.96 for the second reason | (b), (a) |
| Frost gain 1.03 | 4 041, 4 506 | B2: `Keeper.Forge` from the Slam Key carries it. Freezing the crowd costs those slams (2 522 with the package, 4 153 without) | (b) |
| Whirlpool gain 0.85 | 3 976, 4 556 | as Frost, 3 376 against 4 077; rests on 7 builds | (b) |
| ArcGrid, Downpour, Tempest share 0.41–0.46 | gain 2.97, 5.10, 1.33 | not weakness in the first two: without blessings they lose 76–83 %. The share formula divides by the drops of levels and extra weapons too, and a Storm flag multiplies a weapon's hits, so removing a weapon removes its flags. Those drops are large because Keepers are strong, not weak. Tempest is real: gain 1.33, 57 % lost, Full DPS 2 025 | design, (c) |
| Stillness share 0.52, gain 1.13 | Full DPS 2 140 | real: `Keeper.Tide` 588. A ×2 on the next volley of a 19-damage rifle is small, and `mult` was tried (`S1_balance_pass2.md` §6) | (c) |
| Forge, Frost, Whirlpool share 0.79–0.97 | PASS | inflated by A1 and B2: the ceiling hides the other drops. These passes must not be trusted until re-read | (a), (b) |
| Downpour gain 5.10 over the band | | real strength, and not B2: `Keeper.Storm` 4 650 | (c) |

### 1.4 What is not at fault
- `RebuildFlags` at cast start and end does not reset cooldowns or `MeleeActivations`: Rifle and drones hold their cadence through every cast.
- The emission cap (R-WPN-09, 400 a second per weapon) is not the cause of the Slam loss: the fall follows the crowd's distance, with Tide and with Forge's knockback, not the cast's emission count.
- The Storm base overlay replaces Arc Grid's `jumps` and `dmg` for the window. That lowers the cards' numbers in the cast, as the spec's "each child Chains once" says, and the variant's net more than pays it.
- Tide's pull is base behaviour, also under Frost and Stillness. Whether the Frost and Stillness variants should drop the line (their card text does not mention it, the Handoff's wording suggests they do) is a RULE question and is left as a candidate unless the readings force it.

### 1.5 Plan, in this order
1. B1, B2, B3, B4 with failing-first tests.
2. A2 and A3 in the probe route; A1 by reading whether B2 alone lifts the ceiling, and raising the field HP if not.
3. Re-read both probes, then TUNE only what is still (c).

Prediction, to be checked rather than assumed: after B2 Slam's cast turns positive, Frost and Whirlpool stop being carried by slams, and Forge's T-KEE-18 passes fall. B2 also weakens every real run holding Slam, so boss reach may fall on those arms.

**Read back after the fixes:** the prediction held for Slam and for Frost and Whirlpool's carry. B2 was the largest single fault: every Slam number in `S1_probes.md` carried it. A1 did not lift with B2 (below), so the field moved to 40×.

## 2. Bugs fixed
Class BUG, each seen failing first. Replaces the fault; no new mechanism.

| Bug | Fix | Test |
|---|---|---|
| B1 | `ApplyHits` reports whole points and carries the fraction per owner and source | `Dusktracer.Core.Hit.FractionsAddUp` |
| B2 | the hero publishes `View.HeroHurt` when a hit costs HP; `StepSlamTriggers` reads that, not the touches | `Dusktracer.Combat.Hero.HurtPublished`, `Dusktracer.Keepers.Param.HitTakenNeedsAHit` (`AfterHitTakenMult` moved to the new field) |
| B3 | a Frost cast folds its `frozen_vuln` into the Tide flags for the window | `Dusktracer.Keepers.Param.FrostCastVuln` |
| B4 | on a Storm cast the weapons' hits carry the layer's status | `Dusktracer.Keepers.Param.FrostLayerOnStormCast` |

Probe: A2, a walking leg with no enemy within 300 uu heads for the nearest one (plain runs barely change, the crowd stays in reach); A3, `cast_dash_s 1` on Slam's cast scenes only; `Dusktracer.Bot.ProbeRouteFollowsCrowd`. A1, below.

Effect of the bug fixes alone on the same 15 seeds and 19 arms: steered runs at the boss 201 → 192, wins 164 → 143. This is mostly B2, which handed every Slam holder free slams. T-DIR-01 PASS 588.6 s unchanged, T-OFF-15 191/192.

## 3. The field: 10× still has a ceiling
With B2 gone, at 10× full DPS still reads 2 156–5 346 with 160–176 kills in 10 s on the strong arms, and round 1's numbers moved no gain (below). The same builds at 40× read 2 620–11 626 with kills down to 1–88, and the gains spread: Furnace 1.45 → 1.91, Hammer 1.77 → 2.89, Downpour 4.11 → 4.84, Frost 1.20 → 0.87. At 10× Forge read 16 of 18 in band on T-KEE-15 only because the ceiling pressed its combinations together; at 40× it is 8 of 18. **`FIELD_HP_MULT` is now 40.** Kill-driven effects (shatter, explosion, Chain forks) count for less at 40× than in a run, as they did at 10× than at 1×.

## 4. Numbers (TUNE), three rounds
One hypothesis per change, written to `number_change_log.csv` before measuring. Verdicts follow the line as written, boss clause included.

| Round | Field | Change | Target | Reading | Verdict |
|---|---|---|---|---|---|
| 1 | 10× | Downpour Key `dmg` 0.50 → 0.35 | gain ≤ 2.8 | 3.50 | reverted |
| 1 | 10× | Tempest Key `child_dmg` 0.45 → 0.65 | gain ≥ 1.9 | 1.48 | reverted |
| 1 | 10× | Slam Key `n` 6 → 4 | gain ≥ 1.5 | 1.20 | reverted |
| 1 | 10× | Furnace Key `dps` 0.30 → 0.45 | gain ≥ 1.8 | 1.45 | reverted |
| 1 | 10× | Frost Trait `shatter_dmg` 0.50 → 1.00 | gain ≥ 1.5 | 1.18 | reverted |
| 1 | 10× | Whirlpool Key `crush_dmg` 0.10 → 0.25 | gain ≥ 1.3 | 0.98 | reverted |
| 2 | 40× | Downpour Key `dmg` 0.50 → 0.30 | gain ≤ 3.2, casts ≤ 36 000, boss ≥ 13 | 4.36, 39 500–46 400, 12 | reverted |
| 2 | 40× | Tempest Amp1 `child_dmg` +0.10 → +0.30 | gain ≥ 1.7, boss ≥ 10 | 1.72, 9 | reverted |
| 2 | 40× | Slam Key `n` 6 → 4 | gain ≥ 1.5, boss ≥ 14 | 1.29, 12 | reverted |
| 2 | 40× | Furnace Key `dps` 0.30 → 0.36 | gain 2.0–2.4, boss ≥ 11 | 2.21, 12 | **kept** |
| 2 | 40× | Hammer Key `dmg_mult` 1.6 → 1.3 | gain ≤ 2.4, boss ≥ 13 | 3.58, 13 | reverted |
| 2 | 40× | Tempest variant `children` 5 → 8 | casts ≥ 16 000 | 10 100–11 200, no movement; not traced | reverted |
| 2 | 40× | Arc Grid variant `links_per_enemy` 3 → 5 | casts ≥ 26 000 | 35 400–38 500 | **kept** |
| 2 | 40× | Slam variant `radius_uu` 500 → 350 | casts ≤ 61 000 | 61 500–69 700 | reverted |
| 3 | 40× | Downpour variant `count` 2 → 1 | casts ≤ 42 000, boss stays 15 | 21 800–37 800, 13, wins 15 → 11 | reverted |
| 3 | 40× | Tempest variant `child_dmg` new 0.90 | casts ≥ 18 000 | 27 200–36 000 on five of seven layers | **kept** |
| 3 | 40× | Furnace variant `duration_s` 8 → 10 | +15 % on most layers | +39–46 % on three Tide layers, ≤ +12 % on the other three | reverted |

Round 1 was read on the ceiling, which §3 found afterwards; its two re-tried levers (Slam `n`, Furnace `dps`) were re-read at 40× and that is said here rather than hidden. Round 1's Tempest `child_dmg` also moved the no-pick arm (588.6 → 607.4 s, T-DIR-01 still PASS), because the Storm base reads the Key's value and every arm has a Patron; the later Tempest changes stay off it. Note that round 1 as a set read better on the arms than anything kept: 213 at the boss, 162 wins, Frost 13/10, Stillness 10/7, Stillness:Storm 8/8. No hypothesis named that, so it is not kept, and which number did it is unknown. It is the first thing to try next.

## 5. Results
Before = `Saved/Probes/ik`, `ab` and `after_probes`. After = final tree, 40× field (`Saved/Probes/ik_final`, `ab_final`, `Saved/BotRuns/final`, in the worktree).

**T-KEE-15: FAIL, 3/57 → 15/57 in band.** Casts that lose damage 23 → 17, 16 of them Tide.

| Keeper | Mean before | In band before | Mean after | In band after | Range after |
|---|---|---|---|---|---|
| Storm | 15 538 | 3/19 | 37 920 | 8/19 | −1 454 to 62 524 |
| Forge | 13 983 | 0/18 | 48 762 | 7/18 | 18 969 to 77 105 |
| Tide | −4 868 | 0/20 | −3 004 | 0/20 (no band) | −30 987 to 40 169 |

- All Slam casts are positive, 42 748–77 105, and now over the band by 1.45–1.58 on five of six.
- Downpour stays 1.37–1.65. Cutting it costs real runs (round 3), so it stands.
- Forge with Tide layers on Furnace is 0.39–0.50, and two combinations sit at zero, Arc Grid:Hammer 3 937 (SD 5 369) and Tempest:Furnace −1 454. Not traced.

**T-KEE-18: FAIL, gain 0/9 → 1/9, share 5/9 → 4/9, archetype band 2/9 → 5/9.**

| Arm | Builds | Full DPS | Share | Loss without blessings | Gain before | Gain after | Band |
|---|---|---|---|---|---|---|---|
| Forge/Furnace | 15 | 10 288 | 0.601 PASS | 0.862 | 0.90 | **2.05 PASS** | IN |
| Forge/Hammer | 14 | 12 629 | 0.800 PASS | 0.933 | 1.49 | 2.88 | OUT |
| Forge/Slam | 14 | 8 470 | 0.554 | 0.853 | 1.99 | 1.20 | IN |
| Storm/ArcGrid | 14 | 4 277 | 0.389 | 0.788 | 2.97 | 3.30 | OUT |
| Storm/Downpour | 15 | 7 498 | 0.443 | 0.857 | 5.10 | 4.86 | IN |
| Storm/Tempest | 13 | 2 782 | 0.320 | 0.580 | 1.33 | 1.37 | OUT |
| Tide/Frost | 12 | 6 102 | 0.579 | 0.681 | 1.03 | 0.90 | IN |
| Tide/Stillness | 9 | 6 838 | 0.680 PASS | 0.862 | 1.13 | 1.26 | IN |
| Tide/Whirlpool | 10 | 6 368 | 0.792 PASS | 0.827 | 0.85 | 1.23 | OUT |

Mean share 0.683 → 0.573: the old figure was inflated by A1 and B2, as §1.3 said. Forge/Slam's pass and its 1.99 were the HitTaken fault.

**Bot, 15 seeds, 19 arms.** Reached boss / wins / median end s / Keeper share at L16.

| Arm | Control (`after_probes`) | Final |
|---|---|---|
| None | 0 / 0 / 589 / — | 0 / 0 / 589 / — |
| Forge-Furnace | 13 / 11 / 986 / 0.45 | 14 / 10 / 985 / 0.58 |
| Forge-Furnace-Tide | 8 / 3 / 920 / 0.37 | 6 / 6 / 815 / 0.55 |
| Forge-Hammer | 14 / 13 / 978 / 0.59 | 14 / 13 / 963 / 0.62 |
| Forge-Hammer-Tide | 13 / 7 / 974 / 0.59 | 13 / 10 / 968 / 0.64 |
| Forge-Slam | 14 / 12 / 994 / 0.48 | 14 / 11 / 977 / 0.47 |
| Forge-Slam-Tide | 10 / 7 / 950 / 0.44 | 10 / 3 / 943 / 0.48 |
| Storm-ArcGrid | 13 / 12 / 970 / 0.51 | 12 / 11 / 979 / 0.51 |
| Storm-ArcGrid-Forge | 15 / 13 / 987 / 0.58 | 15 / 12 / 977 / 0.58 |
| Storm-Downpour | 15 / 15 / 994 / 0.59 | 15 / 14 / 974 / 0.59 |
| Storm-Downpour-Forge | 15 / 15 / 965 / 0.61 | 15 / 14 / 991 / 0.62 |
| Storm-Tempest | 13 / 10 / 989 / 0.41 | 10 / 6 / 982 / 0.43 |
| Storm-Tempest-Forge | 15 / 13 / 990 / 0.42 | 15 / 14 / 987 / 0.46 |
| Tide-Frost | 11 / 9 / 956 / 0.39 | 10 / 8 / 960 / 0.40 |
| Tide-Frost-Storm | 11 / 10 / 954 / 0.37 | 11 / 9 / 969 / 0.37 |
| Tide-Stillness | 7 / 5 / 875 / 0.25 | 6 / 3 / 833 / 0.32 |
| Tide-Stillness-Storm | 6 / 5 / 564 / 0.23 | 5 / 4 / 575 / 0.23 |
| Tide-Whirlpool | 4 / 1 / 580 / 0.34 | 6 / 3 / 864 / 0.36 |
| Tide-Whirlpool-Storm | 4 / 3 / 556 / 0.27 | 5 / 2 / 529 / 0.27 |

Steered: at the boss 201 → 196, wins 164 → 153. T-DIR-01 PASS 588.6 s, unchanged. T-OFF-15 FAIL 195/196, the same single miss as the control (Tide/Stillness:Storm seed 6, 24 decisions). `Tools/pressure.py --check` FAIL 0/19, unchanged. Keeper share by attribution rises on Furnace because B1 now counts its Field.

## 6. What cannot be forced
- **Tide's cast has no damage band, and that is a design reason.** The base deals nothing: it moves and freezes. A probe where no hit reaches the hero cannot credit that, and the cast's freeze costs the build's own reach. With the crowd followed, the mean is still −3 004. A trial with no line on Frost and Stillness (built, measured, not kept: `Saved/Probes/ik_rule`) moved those 13 combinations to mean −392, 0 of 13 in band, so the line accounts for most of the loss and not for the missing band. R-KEE-15's "retune toward the mean" has nothing to aim at. **RULE candidates, not taken:** (1) Frost and Stillness drop the line, as their card text and the Handoff read; (2) a Tide cast deals damage of its own; (3) R-KEE-15 judges Tide on something other than marginal damage.
- **The share formula cannot pass Storm.** `share` divides by the drops of levels and extra weapons, and a Storm flag multiplies a weapon's hits, so those drops rise with the Keeper's own power. Arc Grid and Downpour lose 79–86 % without blessings and read 0.39–0.44. Gain 3.30 and 4.86 say the same builds are over their band. No number satisfies both: lifting the Keeper raises the drops with it, and rounds 1–2 moved share by at most 0.04. **RULE candidate:** read R-KEE-18's share on `loss` for arms whose flags multiply weapons.
- **Package gain on Tide.** Frost 0.90, Whirlpool 1.23, Stillness 1.26. Control spends the crowd's reach on safety, so a no-damage field reads it as a loss. Round 1 moved gain by at most 0.07 while the arms rose. The band may not suit a control package.
- **Slam and Tempest are real weakness left unsolved.** Three levers tried on each; two would lift gain and cost boss reach. Tempest's `chance` stays out (`S1_balance_pass2.md` §6).

## 7. What reads wrong
- The boss numbers fell for a good reason, but they fell: Slam arms in particular lose wins (Forge-Slam-Tide 7 → 3).
- Tempest `child_dmg` 0.90 in the variant is a number the card text does not show; it only raises what the cast's fan deals.
- `C.INTERCESSION.RETUNE_STEP_MAX` is ×2: Tempest's variant `child_dmg` 0.45 → 0.90 sits at that limit, Arc Grid's links under it.
- `Tide/Whirlpool:Furnace` reads 40 169 with SD 31 555, a chain of Burn deaths on three seeds. Do not read a mean into it.
- All of this is a bound at skill 1.0 and no number here has met a human (O-008).
