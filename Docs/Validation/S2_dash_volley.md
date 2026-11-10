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

## Open, not fixed here
An Orbital Drones dash volley does not carry `Keeper.Quicksilver`. `FireVolley` hands an Orbiter to `StepDrones`, whose free branch was written for Chain Trigger: it takes the Chain Trigger source if that modifier is socketed and the weapon's source otherwise, and never the source the dash passed. So those bolts are not Keeper damage and their kills refund nothing, against R-KEE-34. Read from the code only, older than D-027, class BUG. Fixing it moves every run that holds Drones with Vector, so it wants its own brief.

## Not verified
- One seed at one skill was run. The other ten runs that moved were not paired here; the mechanism is the only one the code offers.
- `dev/s3` and `main` were read, not run.
- Whether one more win in 15 is a balance change was not read; the reread calls it inside noise.
