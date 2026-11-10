# S2 dash volley under a conversion: the trace of D-027 on Vector with Forge

**Verdict: RULE, already decided. D-027's code line also covers Quicksilver's dash volley, so under Hammer's conversion the volley's cleave is `Keeper.Quicksilver`, not `Keeper.Forge`, and its kills refund dash cooldown. That is what R-KEE-34 and R-WPN-07 require. No game code and no number changed on `fix/d027-dash-volley`: the specs now name the case, one test pins it, and D-027's impact note is corrected. No new decision.**

Class RULE, rule layer. It replaces the sentences of R-KEE-13, R-KEE-34 and R-WPN-07 that named only a modifier's free volley, and D-027's claim that the input stream is unchanged. A bot reading is never a human reading.

## The finding
The S2 exit reread (`S2_exit.md`, 2026-11-10) paired 540 bot runs by `input_hash` against the batch at f2ef7ed. 529 are equal; the 11 that differ are all Quicksilver/Vector with Forge, where wins went 12 to 13 of 15 at skill 1.0 and 4 to 5 at 0.6. D-027 had called its change attribution-only, checked on Forge/Hammer.

## Mechanism
- `FDTWeaponSim::FireVolley` folds a volley's converted rounds into one cleave. Before D-027 (`b4baf41`, on `dev/s2` since the merge `0d9b694`) a Keeper `ConvertMelee` always gave that cleave its own source. Since D-027 a free volley (`bFree`) keeps the source it was fired with.
- Two things fire a free volley: `Mod.ChainTrigger` on a kill, which D-027 was written for, and `VolleyOnDash`, which passes `Keeper.Quicksilver` (`DTWeaponSimKeepers.cpp`, the dash step). Only Forge/Hammer grants a Keeper `ConvertMelee`.
- So with Vector and Hammer both held, the dash volley's cleave moved from `Keeper.Forge` to `Keeper.Quicksilver`. The cleave itself is the same: same damage, arc, reach, targets and on-hit effects.
- One rule reads that source. `ReactKeeperKills` refunds `kill_refund_s` (0.15 s) of dash cooldown for each weapon kill whose source is the `VolleyOnDash` flag's, up to `refund_max` (0.50) of the cooldown per dash. The cleave's kills earned nothing while they read `Keeper.Forge`; they earn the refund now.
- The records also show later hits one point apart (6 for 7). That is the event's rounding: the whole-point carry is kept per owner and source (`FDTEnemyPool`, R-TEL-05), so a relabelled hit moves a fraction between two sources. Enemy health is a float and does not see it, and nothing in play reads `DamageDealt`.

## Paired run
Seed 13, skill 1.0, the arm and switches of the exit battery:
```
& Tools\BotRuns.ps1 -Arms Steered:Quicksilver:Vector:Forge -FirstSeed 13 -Seeds 1 -BotVersion 2 -Skill 1.0 -Unlock S2 -Out Saved\BotRuns\d027_after
```
After is 4fd1971 as built. Before is the same tree with the one line of D-027 put back (`Conversion = Spec.FindFlag(...)` without `bFree`), not the commit before the merge, because that merge also carried D-025 and D-026. Each side ran once.

| | Before | After |
|---|---|---|
| `input_hash` | `5c599c9b` | `4027996c` |
| Outcome, end | victory, 966.4000 s | victory, 961.3167 s |
| Dash presses | 93 | 86 |
| The exit batch it equals | f2ef7ed, hash and end time | d827a58, hash and end time |

The one line is the whole difference between the two batches on this run. Every record is equal up to 371.1334 s, 18.5 s after the bot took `Blessing.Forge.Hammer.Key`:

| Step | Record | Before | After |
|---|---|---|---|
| 22268 | dash, then hit: 4 hits, 78 damage | `Keeper.Forge` 78 | `Keeper.Quicksilver` 78 |
| 22268 | kill: 2 kills | `Keeper.Forge` 2 | `Keeper.Quicksilver` 2 |
| 22270 | hit, `Keeper.Forge` | 7 | 6 (rounding carry) |
| 22397 | the next dash, `charges_after` | 0 | 1 |

Two kills refund 0.30 s, so the dash charge is back before the next press on one side only. From there the bot's dashes, and with them its inputs, differ.

## Which behaviour the spec requires
The new one. R-WPN-07 gives an emission its creator's source and lets a Keeper conversion take only the weapon's own rounds; D-027 clause 1 bounds D-015 the same way. A dash volley is an extra emission that Quicksilver creates. R-KEE-34 says the volley is Keeper damage and that each kill of such a volley refunds. Before D-027 the code broke R-KEE-34 for the converted rounds, and nobody had read it. The specs only spelled out the modifier's case, so the fix is in their text:
- `keepers.md` R-KEE-13 and R-KEE-34, `weapons.md` R-WPN-07: a Keeper's free volley under a second Keeper's conversion stays the firing Keeper's, refund included.
- T-KEE-34 `Dusktracer.Keepers.S2.Card.Quicksilver.Vector.Key` gains the case. With the old line put back it fails, 1 of 4.
- `13_Decisions/D-027.md`: the impact line and the `dev/s2` section say what moved.

## Other free volleys and branches
- **Chain Trigger's free volley**, every branch: source `Keeper.Forge` to `Modifier.ChainTrigger`. No rule reads either from a hit or a kill, so it stays attribution-only. Read from the code, and by the reread's 529 equal runs.
- **`main`**: has no dash volley, no duo and no rule that reads a kill's or a hit's source; `Kill.SourceId` is written and never read by play. No arm's sim moved there. Read from the code at `main`; run-confirmed by D-027 on Forge/Hammer only.
- **`dev/s3`**: the same code in `DTWeaponSim`, `DTWeaponSimKeepers` and `DTWeaponSimDuos`, so the same change on Vector with Forge and nothing else. Not run there.
- **Duo reactions** read a hit's Path for Storm and Quicksilver only, and one pair is live per player (R-REA-01). With Forge and Quicksilver the pair is `ForgeQuicksilver`, which reads no Path. Not touched.
- **Echo, Hunters, Ricochet's free round, Momentum's wave**: none goes through `FireVolley`, so the line does not reach them. Hunter and Ricochet read their own Keeper's source on a kill, which a Forge cleave never carried.
- **Stillness and charge** skip a free volley by the same `bFree`, older than D-027; not changed by it.

## Drones and Sweep Beam: closed on `fix/drones-dash-volley`
The item left open here was real, and a second weapon had it. Class BUG, sim layer. It replaces the two places where a free volley's emission took the weapon's source instead of its creator's. No number moved and no rule was written: R-KEE-34 and R-WPN-07 already said it.

**The fault.** `FireVolley` builds one shot from the source and the multiplier it was called with, and every pattern it emits itself uses that shot. Two weapons leave it for a step of their own, and both dropped what the caller passed:
- **Orbital Drones.** `StepDrones`' free branch took Chain Trigger's source if socketed and the weapon's otherwise, and never scaled the bolt. A dash volley's bolts were `Weapon.OrbitalDrones` at ×1.0: not Keeper damage, not at `dmg_mult` (1.5), no refund per kill.
- **Sweep Beam** (evolved Rail Lance). The volley only starts the beam; `StepBeam` made every tick from the weapon's source at ×1.0. A dash volley's beam, and Chain Trigger's, read as `Weapon.RailLance`.

**Reproduced before any fix.** Two tests written first, run on the tree at 342f90e, 6 failures:
```
& Tools\RunTests.ps1 -Filter Dusktracer.Keepers.S2.Card.Quicksilver.Vector
```
| Test line | Expected | Read at 342f90e |
|---|---|---|
| `Vector.Drones`: a drone's dash bolt is `Keeper.Quicksilver` | at least 1 hit | 0 |
| the weapon takes none of the dash volley | 0 | 2 |
| at the Key's multiplier | 12 (1.5 × 8) | no such hit |
| its kill refunds cooldown | 0.15 s | 0 |
| `Vector.EveryWeapon`, Rail Lance evolved: hits as `Keeper.Quicksilver` | at least 1 | 0 |
| Rail Lance evolved: none of it is the weapon's | 0 | 10 |

With the fix all six pass. A paired bot run shows it in telemetry, on the exit battery's arm at a seed that holds Drones from 15 s and the Vector Key from 117 s:
```
& Tools\BotRuns.ps1 -Arms Steered:Quicksilver:Vector:Forge -FirstSeed 9 -Seeds 1 -BotVersion 2 -Skill 1.0 -Unlock S2 -Out Saved\BotRuns\drones_after
```
| | Before (342f90e as built) | After |
|---|---|---|
| `input_hash` | `23469e3f`, the reread batch's | `5c1d6704` |
| Outcome, end | victory, 937.0667 s | victory, 963.9834 s |
| Dash presses | 156 | 154 |
| Step 8733, the hit after the first dash with the Key (step 8731) | `Weapon.OrbitalDrones` 4 | `Keeper.Quicksilver` 7 |
| Step 8751 | `Weapon.OrbitalDrones` 2 | `Keeper.Quicksilver` 3 |

Every record before step 8733 is equal. Each side ran once; the before side equals the reread's record for record.

**The fix.** `StepDrones` takes the source `FireVolley` was called with, so a Chain Trigger volley keeps Chain Trigger's (the same value as before) and a dash volley Quicksilver's; a dash volley's bolt is scaled by `dmg_mult` where `FireVolley` scales its rounds. The beam remembers the source and scale of the free volley that started it, and its ticks use them; the weapon's own beam is as it was. `reforging.md` R-RFG-08 now says one beam lives per weapon and a free volley's beam is that volley's.

**Tests.** T-KEE-34 gains `Dusktracer.Keepers.S2.Card.Quicksilver.Vector.Drones` (source, multiplier, refund) and `Vector.EveryWeapon` (the other nine weapons, plain and evolved: a dash volley hits as `Keeper.Quicksilver` and no hit of it is the weapon's).

**Every weapon's path, read and run.** `Vector.EveryWeapon` passed for all but Sweep Beam before the fix:
- Emitted by `FireVolley` from the one shot: Pulse Rifle, Scatter Cannon, Rail Lance, Seeker Swarm, Ring Emitter, Spiral Projector, Arc Blade, and the evolutions that only change the spec or the round (Railstorm, Flechette, Murmuration, Vortex, Cyclone).
- Handed on with the shot's source kept: Flak Mortar's shells and Carpet's strikes, Gravity Mine's mines, Pulse Wall's ring.
- Handed on with the source dropped: Orbital Drones and Sweep Beam, the two fixed here.
- Not a fault of source, left as they are: Blade Halo fires no bolts, so its dash volley is nothing (R-RFG-07); Cyclone sweeps only while the hero moves (R-RFG-10); Pulse Wall opens no second ring while one lives (R-RFG-12); Singularity adds a mine's damage to the live mine, whose source it then is (R-RFG-14). Under these a dash volley could do nothing or lose its label by the weapon's own rule. Decided since by D-038 (the section below): the first three now act, Singularity's merge stands.

**What moved.** Only a run that holds a Vector card with Orbital Drones, or with Sweep Beam: its drones' dash bolts gain ×1.5 and the refund, so its dashes and inputs change. Sweep Beam under Chain Trigger moves in attribution only.
- Seed 9107, on the fixed build, all unchanged: None `7bd32faf`, Random `f5270196`, Steered:Forge:Hammer `c6573c69`, Steered:Storm:Tempest `d7c8b43c`, Steered:Tide:Frost `209a3446` (`Tools\BotRuns.ps1`, defaults); RestartCheck `276faf8e` on the gate. None of these arms holds a Vector card.
- **Stale S2 exit readings** (`S2_exit.md`, read at d827a58): the three Quicksilver arms, at both skills: Vector:Forge, Momentum:Storm, Rebound:Storm. Counted from that batch's picks, 8 of 540 runs hold a Vector card with Drones: at skill 1.0 Vector:Forge seeds 2, 6 and 9 and one Momentum run; at 0.6 two Vector:Forge runs, one Momentum, one Rebound. No run there holds Sweep Beam with Vector. The other 15 arms cannot be offered a Quicksilver card and none took one.
- **Stale probe readings**: the probe loadout holds Orbital Drones at level 3 and the route dashes every 6 s (`Tools/probe.py`, `FDTBotBrain::ProbeInput`), so every T-KEE-15 combination with a Vector card moves, and every T-KEE-18 scene whose build has a Vector card and Drones. Not rerun here.
- `main` has no dash volley and no Sweep Beam, so nothing moves there. `dev/s3` takes this fix by merge.

## The four weapon rules: decided by D-038 on `fix/dash-volley-rule`
Class RULE, rule layer, then sim. It replaces the three early returns under which a dash volley did nothing on Blade Halo, Cyclone and Pulse Wall; R-RFG-14 keeps Singularity. No catalog number was added or moved, and no card text: with the call, the Vector Key is true of every weapon in en and zh-Hans.

**Read before any change.** Four tests written first, run on e40943c with nothing else touched:
```
& Tools\RunTests.ps1 -Filter Dusktracer.Keepers.S2.Card.Quicksilver.Vector
```
| Test line | Expected | Read at e40943c |
|---|---|---|
| `Vector.BladeHalo`: every blade strikes once as `Keeper.Quicksilver` | at least 4 hits | 0 |
| at the Key's multiplier | 40.5 (1.5 × 27) | no such hit |
| its kills refund cooldown | at least 0.15 s | 0 |
| `Vector.Cyclone`: the landing sweep on a hero who stops | 1 hit | 0 |
| at the Key's multiplier | 50.4 (1.5 × 33.6) | no such hit |
| `Vector.PulseWall`: the dash volley's ring strikes as `Keeper.Quicksilver` | at least 1 hit | 0 |
| its kill refunds cooldown | 0.15 s | 0 |
| `Vector.Singularity`: all 11 lines | pass | pass (it pins the rule that stands) |

Two lines of that first run failed for the tests' own reasons and were rewritten, not the game. Cyclone's own turn does sweep once inside the dash step, because the dash moves the hero, so "no own hit" became "no own hit after the landing". Pulse Wall's first target stood on the rim, where the ring pushes an enemy out before a tick lands; it now stands where the growing band is at the second tick. With the change all ten `Vector.*` tests pass.

**The call, per weapon** (`13_Decisions/D-038.md` has the reasons):
- **Blade Halo**: every blade strikes once where it is, at ×`dmg_mult`, as `Keeper.Quicksilver`, with the refund. Still no bolts; no blade's turn is spent.
- **Cyclone**: the landing sweep no longer asks the hero's speed in that step. The weapon's own turning keeps its gate.
- **Pulse Wall**: the live ring starts again at the hero as the dash volley's, ×`dmg_mult`, with the refund. Never two rings, as Sweep Beam's one beam (R-RFG-08). Replaced since by D-040 (the section after the reread): the ring is not started again.
- **Singularity**: unchanged. The dash mine's damage joins the live mine at ×`dmg_mult`; the mine keeps the source of the volley that laid it, so a mine the weapon laid earns no refund and a mine the dash laid is Quicksilver's whatever joins it.

**What moves.** Only a run that holds `Flag.VolleyOnDash` with Blade Halo, Cyclone or Pulse Wall. Chain Trigger's free volley is not a dash volley and goes through none of the new branches. What it does on those weapons was decided after this, by D-039 (`S2_chain_trigger.md`).

## Reread after both, 2026-11-10, at 8231171
The stale readings named above were reread with the whole S2 exit battery (`S2_exit.md`, the addendum of this date, old against new). The verdicts are the same, 1 PASS, 3 FAIL, 2 NOT READ. 533 of 540 runs keep their `input_hash`; the seven that differ all hold a dash volley with Orbital Drones, on Vector:Forge and Momentum:Storm, and no outcome flips. T-KEE-18's Vector row moves 0.795 to 0.781; 30 of the 31 T-KEE-15 rows with Vector move, none across its band. No run or scene of that battery holds a dash volley with Blade Halo, Cyclone or Pulse Wall, so D-038 is read by its tests only.

## The restart measured: decided by D-040 on `fix/dash-ring-shape`
Class RULE, rule layer, then sim. It replaces D-038's restart of Pulse Wall's live ring and R-RFG-08's restart of Sweep Beam's live beam under a dash volley. A dash volley now does on those two weapons what D-039 gave a kill's free volley, at `dmg_mult` and with the refund: one strike of the ring's band, one tick of the beam along the dash line, and what lives is left alone. No catalog number and no card text moved. A harness reading is not a bot reading, and neither is a human reading.

**The question.** D-039 found the restart erased the wall when a kill triggered it, as often as every 0.2 s. A dash is rarer. Nobody had read the restart at a dash's rate.

**The scene.** `Vector.DashMarch` is `S2_chain_trigger.md`'s `.March` with a hero who dashes: the evolved weapon alone at level 8, the Vector Key, six enemies a second walking in at 100 uu/s, kills resolved, pushes applied, two minutes. A dash is 0.25 s intangible. Each reading is the sum of four runs whose first dash falls at 20.0, 20.9, 21.8 and 22.7 s, across one ring's life.
- **Rates.** One dash every 33 s: the owner's run, 1.81 a minute (`S1_owner_run.md`). Every 9 s: the bot's, Vector:Forge at skill 1.0 presses 104.5 times in about 970 s (`S2_exit.md`). Every 0.9 s: the least the Vector column allows, Amp1's cooldown of 2.4 s less the Key's full refund of 1.5 s.
- **Scenes.** Stand: the dash moves nothing, the crowd starts 750 uu out all round, as `.March`. In the other three each dash carries the hero 450 uu and the crowd starts on the spawn ring, 1600 uu from the hero: a new way each dash (Roam), away from a crowd that comes from one side (Flee), into it (Charge). The hero stands between dashes.
- **Shapes.** No Key; the restart (846ceb1 with only the tests added); one strike (after the change).
- Breaches are enemy-steps within 150 uu of the hero.

```
& Tools\RunTests.ps1 -Filter Dusktracer.Keepers.S2.Card.Quicksilver.Vector
```
Each cell reads no Key → restart → one strike. Damage and refund are sums over the four runs, which hold 14, 46 and 438 dashes at the three rates.

| Weapon | Scene | A dash every | hp | Kills | Breaches | Damage | `Keeper.Quicksilver` damage, restart / strike | Refund, s, restart / strike |
|---|---|---|---|---|---|---|---|---|
| Pulse Wall | Stand | 33 s | 60 | 2740 → 2753 → 2744 | 0 → 0 → 0 | 166920 → 169868 → 167974 | 22388 / 2216 | 19.20 / 3.60 |
| Pulse Wall | Stand | 33 s | 240 | 2436 → 2452 → 2445 | 0 → 0 → 0 | 631920 → 638816 → 636292 | 102589 / 9472 | 19.35 / 4.35 |
| Pulse Wall | Stand | 9 s | 60 | 2740 → 2753 → 2744 | 0 → 0 → 0 | 166920 → 174975 → 169991 | 72878 / 8156 | 66.60 / 14.40 |
| Pulse Wall | Stand | 9 s | 240 | 2436 → 2505 → 2458 | 0 → 0 → 0 | 631920 → 648431 → 639319 | 294131 / 32209 | 66.90 / 22.95 |
| Pulse Wall | Stand | 0.9 s | 60 | 2740 → 2748 → 2756 | 0 → 0 → 0 | 166920 → 184556 → 173490 | 158951 / 42818 | 354.75 / 120.60 |
| Pulse Wall | Stand | 0.9 s | 240 | 2436 → 314 → 2532 | 0 → 5858352 → 0 | 631920 → 324840 → 648536 | 258278 / 166399 | 33.45 / 99.90 |
| Pulse Wall | Roam | 33 s | 60 | 2360 → 2374 → 2373 | 632502 → 584436 → 609772 | 147698 → 150360 → 149134 | 19462 / 2351 | 19.20 / 3.75 |
| Pulse Wall | Roam | 33 s | 240 | 1725 → 1693 → 1734 | 1757284 → 1875833 → 1739810 | 510712 → 514620 → 516821 | 76500 / 8809 | 14.70 / 1.65 |
| Pulse Wall | Roam | 9 s | 60 | 1726 → 1697 → 1765 | 1740464 → 1662960 → 1676411 | 124732 → 125168 → 127969 | 57195 / 6334 | 66.45 / 11.70 |
| Pulse Wall | Roam | 9 s | 240 | 232 → 362 → 266 | 4140017 → 4004273 → 4087857 | 213142 → 239708 → 229564 | 138038 / 13579 | 37.95 / 1.80 |
| Pulse Wall | Flee | 33 s | 60 | 2533 → 2538 → 2538 | 0 → 0 → 0 | 154792 → 157155 → 155670 | 20205 / 1912 | 18.45 / 3.60 |
| Pulse Wall | Flee | 33 s | 240 | 2237 → 2242 → 2242 | 0 → 0 → 0 | 583208 → 587332 → 586609 | 77692 / 7301 | 13.95 / 3.00 |
| Pulse Wall | Flee | 9 s | 60 | 2333 → 2386 → 2348 | 0 → 0 → 0 | 144825 → 156720 → 150278 | 106132 / 14310 | 66.90 / 19.80 |
| Pulse Wall | Flee | 9 s | 240 | 1763 → 1971 → 1844 | 0 → 0 → 0 | 511680 → 545692 → 525911 | 397822 / 48341 | 62.70 / 23.40 |
| Pulse Wall | Charge | 33 s | 60 | 2061 → 2109 → 2089 | 1702987 → 1477851 → 1643894 | 135840 → 139699 → 137966 | 20554 / 2711 | 19.35 / 4.35 |
| Pulse Wall | Charge | 33 s | 240 | 946 → 944 → 972 | 4486848 → 4487792 → 4428398 | 364612 → 381334 → 377048 | 76106 / 8145 | 15.60 / 2.55 |
| Pulse Wall | Charge | 9 s | 60 | 1753 → 1504 → 1781 | 2163162 → 2404988 → 2108600 | 135360 → 114158 → 137929 | 43942 / 5186 | 62.25 / 13.95 |
| Pulse Wall | Charge | 9 s | 240 | 186 → 34 → 193 | 4213707 → 4285286 → 4212701 | 192922 → 135221 → 200914 | 70549 / 9214 | 3.90 / 1.50 |
| Sweep Beam | Stand | 33 s | 60 | 2824 → 2823 → 2826 | 12504 → 12804 → 9768 | 193162 → 194940 → 193675 | 5540 / 1129 | 8.10 / 1.65 |
| Sweep Beam | Stand | 33 s | 240 | 2632 → 2605 → 2613 | 847920 → 859682 → 834866 | 742824 → 745526 → 742003 | 27599 / 3078 | 9.75 / 0.30 |
| Sweep Beam | Stand | 9 s | 60 | 2824 → 2822 → 2826 | 12504 → 20013 → 12042 | 193162 → 198839 → 194290 | 17442 / 2975 | 24.90 / 4.35 |
| Sweep Beam | Stand | 9 s | 240 | 2632 → 2623 → 2609 | 847920 → 849263 → 858856 | 742824 → 755581 → 744979 | 84029 / 9337 | 33.45 / 2.10 |
| Sweep Beam | Stand | 0.9 s | 60 | 2824 → 2489 → 2828 | 12504 → 1084212 → 13804 | 193162 → 188818 → 196034 | 55712 / 7798 | 81.45 / 11.40 |
| Sweep Beam | Stand | 0.9 s | 240 | 2632 → 2010 → 2620 | 847920 → 3049851 → 820795 | 742824 → 629930 → 742585 | 187040 / 24727 | 92.25 / 12.15 |
| Sweep Beam | Roam | 33 s | 60 | 2659 → 2671 → 2663 | 5096 → 2608 → 5041 | 181876 → 184235 → 182389 | 4617 / 718 | 6.30 / 1.05 |
| Sweep Beam | Roam | 33 s | 240 | 2473 → 2479 → 2458 | 433725 → 422297 → 436796 | 702673 → 703870 → 702023 | 9747 / 2975 | 1.20 / 0.75 |
| Sweep Beam | Roam | 9 s | 60 | 2644 → 2647 → 2663 | 7993 → 6611 → 7833 | 180850 → 187621 → 183312 | 19699 / 3488 | 27.60 / 5.10 |
| Sweep Beam | Roam | 9 s | 240 | 2484 → 2482 → 2487 | 306388 → 298227 → 302728 | 696722 → 706196 → 700826 | 34781 / 7798 | 6.15 / 1.20 |
| Sweep Beam | Flee | 33 s | 60 | 2696 → 2697 → 2696 | 0 → 0 → 0 | 184406 → 184475 → 184406 | 0 / 0 | 0.00 / 0.00 |
| Sweep Beam | Flee | 33 s | 240 | 2582 → 2602 → 2582 | 16557 → 14771 → 16557 | 721688 → 725245 → 721688 | 0 / 0 | 0.00 / 0.00 |
| Sweep Beam | Flee | 9 s | 60 | 2629 → 2635 → 2629 | 0 → 0 → 0 | 179824 → 180234 → 179824 | 0 / 0 | 0.00 / 0.00 |
| Sweep Beam | Flee | 9 s | 240 | 2528 → 2529 → 2528 | 716 → 716 → 716 | 707051 → 706709 → 707051 | 0 / 0 | 0.00 / 0.00 |
| Sweep Beam | Charge | 33 s | 60 | 2704 → 2695 → 2698 | 0 → 21 → 0 | 184954 → 187929 → 184851 | 10773 / 923 | 11.25 / 1.35 |
| Sweep Beam | Charge | 33 s | 240 | 2586 → 2583 → 2593 | 198125 → 151806 → 178771 | 724288 → 729794 → 725895 | 22469 / 4001 | 4.80 / 2.10 |
| Sweep Beam | Charge | 9 s | 60 | 2766 → 2765 → 2758 | 7019 → 6317 → 6374 | 189194 → 200104 → 189639 | 32935 / 2975 | 32.70 / 4.35 |
| Sweep Beam | Charge | 9 s | 240 | 2679 → 2685 → 2679 | 289233 → 276710 → 276386 | 742687 → 756025 → 745936 | 55814 / 8516 | 6.15 / 1.20 |

**What it reads.**
- **Standing, at the owner's rate and the bot's, the restart does no harm.** Pulse Wall has no breach in any of those readings and the restart is the larger gain: kills +0.5 to +2.8 % against +0.1 to +0.9 % for one strike. Sweep Beam moves by about 1 % or less under either shape, and not always up: that is the scene's scatter.
- **Chained, it is D-039's fault again.** At a dash every 0.9 s Pulse Wall at 240 hp keeps 314 of 2436 kills and the wall is gone (5858352 breaches from 0); at 60 hp the ring still kills inside its short life and nothing is lost. Sweep Beam loses 12 % and 24 % of its kills: the beam sweeps about the dash line for all of `beam_s`, every dash, and never aims itself. One strike loses nothing at that rate.
- **The restart feeds the rate.** Under it the ring is the dash volley's for its whole life, so its kills refund: 1.37 to 1.45 s per dash standing at the two ordinary rates, of the 1.5 s the Key allows, against 0.26 to 0.50 s for one strike.
- **The label.** Standing at the bot's rate 42 % and 45 % of Pulse Wall's damage reads `Keeper.Quicksilver` under the restart, and the total is up 4.8 % and 2.6 %: most of it is the weapon's own ring under the Key's name. Under one strike 4.8 % and 5.0 % of the damage is the Key's.
- **A dash that moves the hero.** A dash volley fires at the dash start, so the restarted ring sits where the hero was and holds that spot for a whole ring life. At the bot's rate that costs kills dashing into the crowd (1753 → 1504 and 186 → 34; one strike 1781 and 193) and gains them dashing away (2333 → 2386 and 1763 → 1971; one strike 2348 and 1844). At the owner's rate no scene moves by more than 3 % under either shape. The readings at 240 hp in Roam and Charge at the bot's rate are a hero already overrun without the Key (232 and 186 kills of some 2800 enemies).
- **Sweep Beam dashing away** reads no `Keeper.Quicksilver` hit under either shape: the beam, or the tick, goes along the dash, away from the crowd. The restart still moves kills a little there (2582 → 2602 at most); why was not traced.

**Read once on a build that was not kept** (a switch in the sim to hold all three shapes in one binary, removed before the commit; standing, the same four starts):

| Reading | Pulse Wall kills, 60 hp | 240 hp | Sweep Beam kills, 60 hp | 240 hp |
|---|---|---|---|---|
| A dash whenever the 3 s cooldown, less its refunds, allows | 2740 → 2770 → 2740 | 2436 → 2569 → 2493 | 2824 → 2798 → 2828 | 2632 → 2602 → 2614 |
| the dashes that makes | 133 → 243 → 147 | 133 → 245 → 149 | 133 → 152 → 136 | 133 → 156 → 135 |
| A dash every 1.5 s | 2740 → 2773 → 2749 | 2436 → 2583 → 2522 | 2824 → 2679 → 2828 | 2632 → 2452 → 2614 |
| A dash every 1.2 s | 2740 → 2762 → 2754 | 2436 → 2525 → 2543 | 2824 → 2770 → 2828 | 2632 → 2426 → 2618 |
| Sixteen dashes in 4 s, every 30 s (the Intercession's `dash_cooldown_s`) | 2740 → 2753 → 2740 | 2436 → 2488 → 2453 | 2824 → 2823 → 2824 | 2632 → 2624 → 2620 |

- With the Key alone a hero who dashes on every cooldown is not hurt by the restart on Pulse Wall, and dashes nearly twice as often because of it. The wall goes between a dash every 1.2 s and every 0.9 s; the beam already loses 5 to 7 % at 1.5 s.
- The cast's burst loses nothing standing: four seconds is too short for the crowd to cross the ring.
- One tick of the beam along the densest line and not the dash line was read too: kills within 0.3 % of the dash line's at the two ordinary rates, three to seven times the `Keeper.Quicksilver` damage when dashes are chained. D-040 keeps the dash line, which is what the Key says.

**A fault of the scene, not the game.** The first moving scenes started the crowd 750 uu from the hero, and Flee read 0 → 114519 breaches under the restart at the bot's rate. That was the scene: 750 uu from a hero who has just dashed 450 uu is inside the ring left behind, and a ring leaves alone what stands inside it. No enemy spawns that close in play. The moving scenes start the crowd on the spawn ring, and Flee has no breach under any shape.

**Tests, before and after.** On 846ceb1 with only the tests added, `TESTS: FAIL (12 run of 12 found, 3 failed)`, on 12 lines:

| Test line | Expected | Read at 846ceb1 |
|---|---|---|
| `Vector.PulseWall`: the band is struck once more, as `Keeper.Quicksilver` | 1 hit in the two steps of the dash | 0 |
| for one tick of the row at the Key's multiplier | 11.25 (1.5 × 7.5) | no such hit |
| one dash is one strike | 1 | 2, the restarted ring's |
| the ring did not start again: what it had passed is left alone | 0 hits | 1 |
| the ring's own ticks are not spent | 4 | 0 |
| `Vector.SweepBeam`: one tick along the dash line | 1 hit | 2, the restarted beam's |
| the weapon's sweep runs on to its end | 2 hits of the weapon's | 0 |
| after a dash across the line the weapon's sweep still crosses the enemy | at least 1 hit | 0 |
| `Vector.DashMarch`, Pulse Wall, standing, 0.9 s, 240 hp: no more than one kill in twenty lost | at least 2315 | 314 |
| the wall holds | 0 breaches | 5858352 |
| Sweep Beam, 0.9 s, 60 hp | at least 2683 | 2489 |
| Sweep Beam, 0.9 s, 240 hp | at least 2501 | 2010 |

After it `TESTS: PASS (12 run of 12 found, 0 failed)`, and `Dusktracer.Modifiers.ChainTrigger` still passes, 7 of 7. `Vector.PulseWall` was D-038's test of the restart and is rewritten for the rule that replaced it; D-038's other three (`BladeHalo`, `Cyclone`, `Singularity`) are untouched and pass. `Vector.DashMarch` asserts the standing readings only; the moving scenes are printed.

**The fix.** `FireVolley`'s two branches for a kill's free volley on Sweep Beam and Pulse Wall take any free volley, so a dash volley's tick and strike carry `Keeper.Quicksilver` and `dmg_mult` as its rounds do. `OpenForceField` loses its restart branch and is as it was before D-038.

**What moves.** Only a run that holds a dash volley with Pulse Wall or Sweep Beam.
- **Stale S2 exit readings: none.** Counted from the batch at 8231171 (`bots_1.0` and `bots_0.6`, 540 runs): ten runs take an evolution, none evolves Ring Emitter, and the one that evolves Rail Lance (Tide:Whirlpool seed 11 at 0.6, Forge second) picks no Quicksilver card and no Slipstream Drive. Three of the ten are on Quicksilver arms (Momentum:Storm seed 2 at 1.0 with Flak Mortar, Rebound:Storm seed 2 at 0.6 with Pulse Rifle, Vector:Forge seed 14 at 0.6 with Spiral Projector), none with either weapon. So 0 of 540 runs hold a dash volley with Pulse Wall or Sweep Beam. The ablation builds and the cast scenes name neither evolution.
- Seed 9107, on the changed build (d5b7d73), all unchanged: None `7bd32faf`, Random `f5270196`, Steered:Forge:Hammer `c6573c69`, Steered:Storm:Tempest `d7c8b43c`, Steered:Tide:Frost `209a3446` (`Tools\BotRuns.ps1`, defaults, one process); RestartCheck `276faf8e` on the gate, which read `GATE: PASS (d5b7d73; 3 not run)`. None of the five runs evolves a weapon.
- `main` has no evolutions. `dev/s3` carries the same sim code and takes this by merge.

## Not verified
- D-038: no human has seen the three weapons under a dash volley. The Blade Halo strike lands on the orbit the dash has widened (R-RFG-07's speed term), read from the code only. Whether a player reads the restarted Pulse Wall as a loss of the wall was the hypothesis D-038 left open; D-040 measured it in a harness and took the restart out.
- D-040: no bot run and no human session holds a dash volley with Pulse Wall or Sweep Beam. The readings are a harness scene: enemies walk straight in and never fire, the hero stands between dashes, the weapon is alone at level 8 and only the Key is held. The 0.9 s rate is the catalog's arithmetic dashed by a script, not the hero sim with Amp1 held, and no person or bot has been seen to dash that often. The readings on a build that was not kept were each taken once. Whether a player can tell the one strike happened takes a person: it has no look of its own. The Vector layer and Slipstream Drive go through the same branch at their own `dmg_mult` and were not run.
- One seed at one skill was run. The other ten runs that moved were not paired here; the mechanism is the only one the code offers.
- `dev/s3` and `main` were read, not run.
- Whether one more win in 15 is a balance change was not read; the reread calls it inside noise.
- The drones fix: the paired run is one seed at one skill, once per side. The S2 exit battery and the probes have since been rerun (the section above).
- Sweep Beam's dash multiplier is read from the code; the test pins its source only. Sweep Beam with Vector was not run in a bot.
