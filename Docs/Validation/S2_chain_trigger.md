# S2 Chain Trigger on the evolved weapons: the readings behind D-039

**Verdict: RULE, decided by D-039 on `fix/chain-trigger-rule` (cut from `dev/s2` at 6a5afe3). A kill's free volley did nothing on Blade Halo and Pulse Wall and cost Sweep Beam kills. It now acts on all three, each in its own shape, and never starts a live beam or ring again. Cyclone at rest and Singularity keep their weapon rules. No catalog number and no card text moved.**

Class RULE, rule layer, then sim. It replaces Blade Halo's and Pulse Wall's early returns for this volley and the Chain Trigger half of R-RFG-08's restart. A harness reading is not a bot reading, and neither is a human reading.

## What the game says and offers
- Card: "A kill by this weapon fires one free volley." / "这把武器击杀敌人时，免费再齐射一次。" (`ST_Modifiers.csv`).
- `modifiers.csv`: no required cap, no blocked cap, no flag, one number, `min_interval_s`. `matrix.csv` reads `yes` for all ten weapons; `flag_compat.csv` does not enter, the modifier carries no flag.
- An evolution's caps never silence it: on all twenty forms the socket takes it and it is not dormant (`.EveryWeapon`). So R-MOD-08 shows nothing and the offer pool keeps the card.
- It is offered in play. The S2 exit batch at 8231171, `bots_0.6/Steered-Tide-Whirlpool_s11`: `Evolve.RailLance` at 550.95 s, and the level 20 hand at 699.20 s holds `Mod.ChainTrigger` for `Weapon.RailLance`. The bot passed it.

## Read before any change
Seven tests written first, run on 6a5afe3 with nothing else touched:
```
& Tools\RunTests.ps1 -Filter Dusktracer.Modifiers.ChainTrigger
```
`TESTS: FAIL (7 run of 7 found, 5 failed)`, on nine lines:

| Test line | Expected | Read at 6a5afe3 |
|---|---|---|
| `.BladeHalo`: every blade strikes once as `Modifier.ChainTrigger` | at least 4 hits | 0 |
| `.PulseWall`: the band is struck once more | 1 hit | 0 |
| one kill is one strike | 1 | 0 |
| `.SweepBeam`: one tick along the densest line | 1 hit | 2, the restarted beam's |
| the weapon's sweep runs on to its end | 2 hits of the weapon's | 0 |
| `.EveryWeapon`, Orbital Drones evolved: a kill's free volley adds damage | more | 27000 against 27000 |
| Ring Emitter evolved | more | 4140 against 4140 |
| `.March`, Rail Lance evolved, 60 hp: no fewer kills with the modifier | at least 164 | 116 |
| Rail Lance evolved, 240 hp | at least 114 | 69 |
| `.Cyclone`, `.Singularity`: every line | pass | pass (they pin the rules that stand) |

The tests were shaped on that tree before the sim was touched, and three scenes were corrected for their own faults, not the game's: `.EveryWeapon` hands its kill in at one second, when a ring holds its band over the outer crowd, not at half a second, when the growing band stands between two rings of enemies; `.BladeHalo`'s "once" counts one touch of every blade from the weapon's own touches; `.SweepBeam` hands its kill in before the live beam reaches the enemy, since the tick is on the volley's aim (below).

## The scene
`.March`: the weapon at level 8, alone, hero at the origin (walking back and forth for Cyclone). Six enemies a second from 750 uu on a golden-angle turn, walking in at 100 uu/s, thirty seconds, kills resolved by the pool and pushes applied. Each reading is the same scene without and with the modifier, at 60 and 240 health. Breaches are enemy-steps inside 150 uu of the hero.

| Form | Kills at 6a5afe3, 60 hp | 240 hp | Kills after, 60 hp | 240 hp |
|---|---|---|---|---|
| Blade Halo | 144 → 144 | 0 → 0 | 144 → 145 | 0 → 0 |
| Pulse Wall | 150 → 150 | 75 → 75 | 150 → 157 | 75 → 120 |
| Sweep Beam | 164 → 116 | 114 → 69 | 164 → 171 | 114 → 126 |
| Cyclone, moving | 152 → 155 | 134 → 143 | same | same |
| Singularity | 162 → 168 | 161 → 170 | same | same |
| Railstorm | 139 → 147 | 64 → 122 | same | same |
| Flechette | 142 → 162 | 135 → 138 | same | same |
| Carpet | 157 → 164 | 137 → 153 | same | same |
| Murmuration | 149 → 175 | 129 → 143 | same | same |
| Vortex | 140 → 145 | 46 → 49 | same | same |

The ten plain weapons gain in all twenty readings and do not move with the change.
- **Sweep Beam at 6a5afe3**: breaches 197 → 33148 and 38670 → 61404, damage 11218 → 7934. R-RFG-08 let any free volley start the beam again from its edge; with a kill every `min_interval_s` the beam never crosses the line it was aimed at.
- **Blade Halo after**: breaches 1137 → 207 at 60 hp. At 240 hp the blades kill nothing in thirty seconds with or without the modifier, so there is no kill to trigger on and that reading shows no gain.
- **Pulse Wall after**: damage 9585 → 9840 and 29340 → 34980; breaches 0 in every reading.

## The restart, tried and put back
D-038 lets a dash volley start Pulse Wall's live ring again, and R-RFG-08 did the same for any free volley on Sweep Beam. The same branch for a kill's free volley on Pulse Wall (one line, `bVolleyForced || bFree`, built, run, reverted):

| | Without | With, under a restart |
|---|---|---|
| Kills, 60 hp | 150 | 70 |
| Kills, 240 hp | 75 | 16 |
| Breaches, 60 hp | 0 | 45572 |
| Breaches, 240 hp | 0 | 35456 |

A ring that starts again at every kill never reaches its radius. That is the reason D-039 does not follow D-038 here. D-040 has since read the restart at a dash's rate and taken it out for the dash volley too (`S2_dash_volley.md`).

The first shape tried for Sweep Beam was one tick where the live beam points. At 60 hp it read 164 → 164 with no hit of the modifier's: the tick fell on the arc the beam had just emptied. The tick along the volley's own aim, the densest line, reads 164 → 171.

## The call, per weapon (`13_Decisions/D-039.md` has the reasons)
- **Blade Halo**: every blade strikes once where it is, at the row's damage, as `Modifier.ChainTrigger`. No bolts; no blade's turn is spent.
- **Pulse Wall**: the live ring strikes its band once more where it stands, for one tick of the row. Never two rings, never a restart. With none live the free volley opens one as the modifier's.
- **Sweep Beam**: one tick of the beam along the densest line; the live beam sweeps on. With none live the free volley starts one as the modifier's.
- **Cyclone**: unchanged. At rest nothing sweeps; moving, the free sweep lands.
- **Singularity**: unchanged. The free volley's mines join the live mine, which keeps its source.
- **Everything else**: unchanged.

After the change `TESTS: PASS (7 run of 7 found, 0 failed)`.

## What moves
Only a run that holds `Mod.ChainTrigger` socketed on Blade Halo, Pulse Wall or Sweep Beam.
- Seed 9107, on the changed build, all unchanged: None `7bd32faf`, Random `f5270196`, Steered:Forge:Hammer `c6573c69`, Steered:Storm:Tempest `d7c8b43c`, Steered:Tide:Frost `209a3446` (`Tools\BotRuns.ps1`, defaults, one process); RestartCheck `276faf8e`. None of the five runs evolves a weapon.
- **Stale S2 exit readings: none.** Counted from the batch at 8231171 (`S2_exit.md`, the addendum of 2026-11-10): of 540 runs ten take an evolution, and one of them is one of the three, `Evolve.RailLance` on Tide:Whirlpool seed 11 at skill 0.6; no run evolves Orbital Drones or Ring Emitter. That run was offered Chain Trigger for the Sweep Beam and never took it; it never took `Mod.Refit` either, and none of its five `modifier_socketed` records is Chain Trigger. So 0 of 540 runs hold the modifier on a changed weapon. The ablation builds are level 16 snapshots taken before the Reforging hand, and no cast scene names an evolution.
- `S2_exit.md` says of that batch "Reforging picks: one `Evolve.GravityMine`". The records hold ten evolution picks: Flak Mortar three, Pulse Rifle two, and one each of Gravity Mine, Rail Lance, Scatter Cannon, Seeker Swarm and Spiral Projector. Its conclusion about D-038 stands, since none is Orbital Drones, Arc Blade or Ring Emitter; the sentence was corrected on `fix/dash-ring-shape`.
- `main` has no evolutions. `dev/s3` carries the same sim code and takes this by merge.

## Not verified
- No bot run and no human session holds any of the three pairs. The readings are a harness scene: enemies walk straight in and never fire, the hero stands still, the weapon is alone at level 8.
- None of the three has a look of its own. Whether a tick on the densest line reads as a second beam, and whether a ring's extra strike reads at all, takes a person.
- The card's "one free volley" against one tick on Pulse Wall and Sweep Beam: left for a person to read (`modifiers.md` Open).
- The three forms with the modifier were not read against `C.REFORGE.POWER_MAX`, and `min_interval_s` is still a first guess.
- `dev/s3` and `main` were read, not built or run.
