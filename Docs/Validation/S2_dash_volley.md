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
- **Pulse Wall**: the live ring starts again at the hero as the dash volley's, ×`dmg_mult`, with the refund. Never two rings, as Sweep Beam's one beam (R-RFG-08).
- **Singularity**: unchanged. The dash mine's damage joins the live mine at ×`dmg_mult`; the mine keeps the source of the volley that laid it, so a mine the weapon laid earns no refund and a mine the dash laid is Quicksilver's whatever joins it.

**What moves.** Only a run that holds `Flag.VolleyOnDash` with Blade Halo, Cyclone or Pulse Wall. Chain Trigger's free volley is not a dash volley and goes through none of the new branches.

## Reread after both, 2026-11-10, at 8231171
The stale readings named above were reread with the whole S2 exit battery (`S2_exit.md`, the addendum of this date, old against new). The verdicts are the same, 1 PASS, 3 FAIL, 2 NOT READ. 533 of 540 runs keep their `input_hash`; the seven that differ all hold a dash volley with Orbital Drones, on Vector:Forge and Momentum:Storm, and no outcome flips. T-KEE-18's Vector row moves 0.795 to 0.781; 30 of the 31 T-KEE-15 rows with Vector move, none across its band. No run or scene of that battery holds a dash volley with Blade Halo, Cyclone or Pulse Wall, so D-038 is read by its tests only.

## Not verified
- D-038: no human has seen the three weapons under a dash volley. The Blade Halo strike lands on the orbit the dash has widened (R-RFG-07's speed term), read from the code only. Whether a player reads the restarted Pulse Wall as a loss of the wall is the hypothesis D-038 leaves open.
- One seed at one skill was run. The other ten runs that moved were not paired here; the mechanism is the only one the code offers.
- `dev/s3` and `main` were read, not run.
- Whether one more win in 15 is a balance change was not read; the reread calls it inside noise.
- The drones fix: the paired run is one seed at one skill, once per side. The S2 exit battery and the probes have since been rerun (the section above).
- Sweep Beam's dash multiplier is read from the code; the test pins its source only. Sweep Beam with Vector was not run in a bot.
