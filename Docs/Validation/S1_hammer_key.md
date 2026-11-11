# S1 Hammer's Key on fallback weapons

**Verdict: BUG fixed on Arc Blade; Gravity Mine and Orbital Drones wait for a rule. With `Blessing.Forge.Hammer.Key` an Arc Blade's sweep now has the card's reach and deals the card's multiplier; before, it had the Key's arc and nothing else. On the mine and the drones the Key still moves no number: what its card can mean on a blast and on drones is not written anywhere, so it is listed below with options and not decided here. No number was added or changed. Seed 9107 `input_hash` is unchanged on all five arms read, because no arm holds an Arc Blade.**

Class BUG, weapon composition layer (`Combat/DTWeaponCompose.cpp`, `Combat/DTWeaponSim.cpp`). It replaces the `Melee` fallback that read `arc_deg` and `radius_scale` only. Queue item 5; `weapons.md` Open named the weapons; `plans_audit.md` 5.5.

## The card and the three weapons
The Key's card reads: each volley's first `rounds` rounds stop flying and merge into one `radius_uu` cleave for × `{cleave_mult}` damage, where `{cleave_mult}` is `dmg_mult` × `C.WEAPON.CONVERT_MELEE_DAMAGE_RATIO` (`keepers.md` Text). On the Pulse Rifle and the Ring Emitter that is what happens (R-WPN-10, T-WPN-10, T-OFF-13). On the three weapons where `Flag.ConvertMelee` resolves to a fallback (R-WPN-08):

| Weapon | Before | After |
|---|---|---|
| Arc Blade | arc widened to the Key's `arc_deg`; reach and damage as without the Key | the sweep is the cleave: the wider arc, the longer of the blade's reach and the Key's `radius_uu`, × `{cleave_mult}` on the first `rounds` sweeps; Long Haft scales that reach as it scales a native cleave's |
| Gravity Mine | no number moves | the same: open |
| Orbital Drones | no number moves | the same: open |

Arc Blade is the plain case: its own strike is already an arc from the hero that hits every enemy inside it, so every number on the card has a place on its shape and nothing has to be invented. The sweeps are not merged (a level five blade's two sweeps face opposite ways, R-WPN-06), they stay `Weapon.ArcBlade` hits, and the blade takes no conversion, so no socket goes dormant.

## Readings
`Dusktracer.Weapons.HammerKeyFallback` (T-WPN-19), the real Key and the real Long Haft from the catalog, Arc Blade at each of its eight levels, three enemies (near in front, in front past the blade's own reach, behind).

| Run | Result |
|---|---|
| Before the fix (test added, code of the commit under this one) | FAIL, 24 assertions: at every level the reach is the row's and not the Key's, the far enemy is not hit, and Long Haft scales the row's reach. The damage line was not reached, since it reads the hits of a sweep that never got there; the composed spec had no damage factor at all. The lines that pin the mine and the drones passed: the Key moved no number on either |
| After | PASS. Every sweep deals the row's damage × `{cleave_mult}` under `Weapon.ArcBlade`, at the Key's reach |
| Whole suite after | `TESTS: PASS (259 run of 259 found, 0 failed)` |

Seed 9107, `Tools/BotRuns.ps1`, one run per arm, serial. Before is the commit under this one (the run files stamp it `f157c5c`, its hash before a one-word doc fixup was folded into the commit below it), after is this change.

| Arm | Before | After | Moved |
|---|---|---|---|
| None | `1c738e97` | `1c738e97` | no: takes no card |
| Steered:Storm:Tempest | `e96dc0a8` | `e96dc0a8` | no: holds an Arc Blade and no Forge blessing |
| Steered:Storm:ArcGrid | `f8a90efe` | `f8a90efe` | no: holds no Forge blessing |
| Steered:Storm:Downpour | `5fa00d86` | `5fa00d86` | no: holds no Forge blessing |
| Steered:Forge:Hammer | `9385c307` | `9385c307` | no: it takes the Key at 36 s and holds the Pulse Rifle, the Ring Emitter and, from 613 s, Orbital Drones. It never holds an Arc Blade, and the drones are one of the two open weapons |

So no bot run has read the fix: it is read by its test only.

## Needs a rule (not decided here)
One RULE record per spec at a time (D-050 item 6), and each of these changes a cell of the R-WPN-08 table.

1. **Gravity Mine.** The cell says "larger blast"; the Key carries no blast number (`radius_uu` is a reach from the hero, `radius_scale` is on Long Haft).
   - A: the detonation deals × `{cleave_mult}`, size unchanged. The smallest change; the card's "cleave" becomes "a stronger blast".
   - B: A, and the blast radius is at least the Key's `radius_uu`. That is 170 to 380 uu at level one on the weapon `weapons.md` Open already calls an order of magnitude over the others in a crowd, and the pull radius follows it.
   - C: the Key is not offered, or says on its card that it does nothing, while the mine is the only weapon held.
2. **Orbital Drones.** The cell says "larger drone contact area" and drones deal no contact damage (R-WPN-06), so the cell is void.
   - A: the bolts deal × `{cleave_mult}`. Nothing stops flying, so the card's first half is false on drones.
   - B: the first `rounds` drones stop firing and cleave around themselves at the Key's numbers. Closest to the card; a new strike shape, and the drones lead with `Orbit`, so the cap walk of R-WPN-08 has to change for it.
   - C: as 1C.
3. **Arc Blade's sweeps as cleaves.** They are not `bCleave` hits, so Hammer's first tier line (Burn on a cleave) and Masterwork do not act on them, and they count for the weapon and not for Forge in telemetry. Marking them cleaves makes those cards work on the blade and raises Forge's damage share by the blade's whole output, which moves the readings D-047 defines. Slam's every-nth-cleave line and the third tier's explosion already count blade sweeps.
4. **`{cleave_mult}` or `dmg_mult` on the blade.** The fix deals the number the card prints, `dmg_mult` × the fold ratio. Nothing folds on a blade, so `dmg_mult` alone is the other reading; the card would then understate the blade.

## Found on the way, not fixed
- Hard Blow (`Blessing.Forge.Hammer.Amp1`, "Cleaves knock enemies back") is read only by the Forge Intercession ring (`Keepers/DTKeeperSim.cpp`); nothing in `Combat/` reads `knockback_uu`, so no weapon cleave knocks back on any weapon. Found by reading the code; no test was written for it.

## Not verified
- No person has seen a blade sweep at the Key's reach; the arc flash draws at the composed reach and arc (`FArcFlash`), not looked at on a frame.
- No bot arm on any seed read here holds an Arc Blade with the Key.
