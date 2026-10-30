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

**B4, BUG: Frost's layer does nothing on two Storm variants.** `Flag.StatusApply{status=Status.Chill;stacks=2;on=IntercessionHit}` rides only on `FDTKeeperSim::HitCircle`. Tempest and Downpour casts reshape weapon flags and never call it, so "Intercession hits add 2 Chill" has no hit to ride. Storm/Tempest:Frost reads 8 133 against 8 240 with no layer, and in `Status` the Chill count does not rise.

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
