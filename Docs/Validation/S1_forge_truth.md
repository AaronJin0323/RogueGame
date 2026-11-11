# S1 forge truth: Hard Blow, the Rusher's touch, Hammer's Key on the weapons it leaves alone, slow share

Build: base `1c670bf` (`main`); commits on `s1/forge-truth` as below. Development editor build, one process at a time, run under the processor mask of D-052. Nothing here is tuned and no catalog number moved (D-050 item 5).

## A. Hard Blow knocks back (BUG, D-053 item 3)
- **Before.** `Forge.Hammer.Amp1.Desc` says cleaves knock enemies back `{knockback_uu_m}` m. No Combat code read `knockback_uu`: the only reader was the Forge Intercession ring, which reads the catalog value with or without the card.
- **Test first.** T-KEE-39 `Dusktracer.Keepers.HardBlowKnockback` was written before the fix and failed on `1c670bf`'s sim: `Expected 'Hard Blow: one knockback per enemy a cleave strikes' to be 15, but it was 0`. It passes after the fix.
- **Path used.** `DTKnockback::Push` (`Source/Dusktracer/Combat/DTKnockback.h`) queues one `FDTDisplacement` per enemy struck, straight away from the strike's centre, falling back to a direction when the enemy stands on the centre. The ring's code in `FDTKeeperSim::HitCircle` was moved into it unchanged and the ring now calls it (the one `Keepers/` edit of this job, to make the path callable from Combat); `FDTWeaponSim::EmitArc` calls it for a strike with `bCleave`, by the composed `ConvertMelee` `knockback_uu`, with the sweep's direction as the fallback. The ring still reads `DTKeepers::PathParam(Forge, ConvertMelee, knockback_uu)` and the variant's value, as before.
- **What the path exempts or scales.** The queue is applied by `FDTEnemyPool::ApplyDisplacements` at the next Enemies phase: a stale serial is ignored, the boss hull is never moved (R-BOS-03), every push is clamped to the arena. No role is scaled: Tanks and elites move the full distance, as under the ring.
- **Found beside it, not changed.** A Hammer-led Intercession folds its variant's `knockback_uu` (200) into the same `ConvertMelee` flag for the cast, so during that cast a cleave pushes 200, with or without Hard Blow; the variant's card says cleaves become rings that knock back. Noted in `keepers.md` Open.
- Arc Blade sweeps are `EmitArc(..., false)` and do not push (D-053 item 2).

## B. The Rusher's touch (BUG and wording, D-045)
- R-ENM-13 and T-ENM-13 read "more than one hero width off the line" (the body clear of the band); the test's comment says the same and its numbers are unchanged.
- The killed-arrival hazard (`FDTEnemyPool::StepOrphans`) compared with `<=`; contact (`AppendContacts`) is strict. The hazard is now strict too. T-ENM-14 `Dusktracer.Enemies.Contact` gains: a body exactly at the reach (the two radii) is not hit and one uu inside is, by contact and by a killed arrival's band. The touch half fails on the old `<=` (`Expected 'touch: killed arrival's band' to be 0, but it was 1`).

## C. Hammer's Key on the drones and the mine (RULE, D-053 items 1, 2, 4)
- `weapons.md` R-WPN-08: the drone and mine cells of `ConvertMelee` state that Hammer's Key leaves the weapon unchanged. Open loses the Hammer's Key item and gains the two questions for the S1 gate. T-WPN-19's line points at the rule. `S1_kit.md` asks question 6.
- **The weapon box, before and after: unchanged, and not as D-053 item 1 expects.** For Orbital Drones the box has no line. `DTOffers::RoundsAfterPick` returns a row only for a weapon whose converted rounds grow (`SpecAfter.ConvertRounds > SpecNow.ConvertRounds`), and T-OFF-13 `Dusktracer.Modifiers.RoundsTable` pins it: "Only what leads with Projectile converts: the Rifle and the Ring. The others lose no round, so the card names none." D-030 wrote two rows, `ST_Weapons.Rounds.None` ("No rounds fly from {Weapon}") and `Rounds.Some` ("{Kept} of {Count} rounds fly from {Weapon}"), both for a weapon the card converts. It has no words for a weapon a card does not touch, so the box cannot say "unchanged" in D-030's words. Silence is what D-030 gives such a weapon, so this is not a BUG against D-030, and the box was not changed. Reported blocked.
- T-WPN-19 still pins that no number moves on the drones; the Arc Blade's behaviour is unchanged.

## D. Slow share (D-054, method)
`10_Tuning.md §2` defines slow share and says how it is read; `telemetry.md` Open names it. No tool changed.

## Seed 9107 `input_hash`
`& Tools\BotRuns.ps1 -Arms None,Steered:Storm:Tempest,Steered:Storm:ArcGrid,Steered:Storm:Downpour,Steered:Forge:Hammer -FirstSeed 9107 -Seeds 1`, skill 1.0, policy All. The gate's `seed` step (`Tools/SeedCheck.ps1`) reads which seed a launch ran, never an `input_hash` (`S1_d045_d046.md`), so these come from the bot record. The Forge:Hammer arm takes Hard Blow at 84.38 s (its second pick), so no further arm was needed.

| Arm | Base `1c670bf` | After A | After B | End (base → after) |
|---|---|---|---|---|
| None | `fe30ec79` | `fe30ec79` | `fe30ec79` | death 636.3167 s, unchanged |
| Steered:Storm:Tempest | `32aa9ec9` | `32aa9ec9` | `32aa9ec9` | victory 988.8333 s, unchanged |
| Steered:Storm:ArcGrid | `d5bc3fe1` | `d5bc3fe1` | `d5bc3fe1` | victory 972.1667 s, unchanged |
| Steered:Storm:Downpour | `ae8a4edd` | `ae8a4edd` | `ae8a4edd` | victory 970.4667 s, unchanged |
| Steered:Forge:Hammer | `af46517a` | `8dd4ffc6` | `8dd4ffc6` | victory 957.1167 s → 968.3667 s |

- The base reads every value `STATE.md` records; Forge:Hammer reads `af46517a` on `1c670bf`, which settles the open line there.
- **A moves Forge:Hammer only, as it should.** It is the one arm holding Hard Blow. Its JSONL equals the base record for record, `wall` fields and build stamp left out, up to record 500 at 91.5 s, seven seconds after the card was taken: a pushed enemy dies elsewhere and a gem is picked up one step apart. No difference before the card.
- **B moves nothing.** No arm's hero stood at exactly the touch distance of a killed arrival's band; the strict comparison changes only a contact within float error of the touch.

## Tests added
T-KEE-39 `Dusktracer.Keepers.HardBlowKnockback` (new test). T-ENM-14 `Dusktracer.Enemies.Contact` (new lines in the existing test). The test count rises by one.
