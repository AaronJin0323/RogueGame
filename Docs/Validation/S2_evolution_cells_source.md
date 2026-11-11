# S2: the ten evolutions, the port's cells against the previous project's code

**Build:** none. A read-only pass, on `s2/evolution-cells-source` from `dev/s2` f041733 (D-070, ruling 9).

**What was read, port side.** On `dev/s2`:
- every `Evolve.*` row of `09_Catalog/reforging.csv` (behaviour, caps, effect cells, the `ref` column);
- `02_Systems/reforging.md` R-RFG-05 to R-RFG-14;
- the level-6 rows of `09_Catalog/weapons.csv`, which give "the row's damage";
- `constants.yaml` `WEAPON.DRONE_ORBIT_RADIUS_UU`.

**What was read, source side.** The previous project's source, read only (`Source/RogueHero/Combat`):
- `RHWeaponDef.cpp`: the plain weapons at 44 to 355, and the evolutions in `MakeEvolution` at 556 to 698;
- `RHSpecComposition.cpp`: the evolution switch at 439 to 466, and the derived hit at 606 to 708;
- `RHBulletSubsystem.cpp` and `RHWeaponInstance.cpp`, where a behaviour is carried out.

A file and line in the tables is the previous project's.

**How to read it.**
- `RHWeaponDef.cpp` 529 to 545 (`EvolveFrom`): an evolution starts as a copy of its plain weapon, level track and all, with its capabilities replaced. A source value not set in `MakeEvolution` is therefore the plain weapon's.
- The derived hit (`FusionHit`) deals `S.Damage * FP.DerivedDamageMult` (`RHSpecComposition.cpp` 610). `S.Damage` is the evolution's composed damage per round.
- "Port: no cell" means the port's row and rule state nothing for it.
- "(arithmetic)" marks a figure I computed; every other figure is read.
- Level 6 is the evolution level (R-RFG-02, EVOLVE_MIN 6).
- The source scales some radii by the hero's Area stat. The port's evolution cells carry no Area term; where it matters, that is named in the row.

No row says which side is right, and nothing here is proposed.

## R-RFG-05 Railstorm (`Evolve.PulseRifle`, `OrbitLaunch`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Blades | `blades` 6 | `Base.Count = 6`, `RHWeaponDef.cpp` 566 | same | |
| Launch rhythm | one every `launch_s` 0.4 | `Base.Cooldown = 2.4f` per blade, "one every 0.4 s", 567 | same | |
| Pierce | `pierce` 3 | `Base.Pierce = 3`, 572 | same | |
| Damage per blade | `dmg_ratio` 0.6 × the row's damage × count: 0.6 × 27 × 3 = 48.6 at level 6 (arithmetic) | `Base.Damage = 16.f`, 568, plus the Pulse Rifle's track (+5 at L2, +8 at L6, 67 and 71): 29 at level 6 (arithmetic) | differs | A blade hits about 1.7 times as hard in the port (arithmetic) |
| Flight | no cell | `Speed` 1500, `Lifetime` 0.9 "out and back", `Range` 800, 569 to 573; boomerang with an out fraction of at least 0.5, `RHSpecComposition.cpp` 445 to 447 | source has a value, the port has no cell | Where the port's blade turns back decides how far out it strikes |
| Orbit | no cell | `OrbitRadius` 130, `OrbitDegPerSec` 200, 574 to 575 | source has a value, the port has no cell | Where the waiting blades stand, and so what they touch |
| Capabilities | Orbit; Projectile; Aimed; Expiry | Projectile, Aimed, Expiry, Orbit, 563 | same | |

## R-RFG-06 Flechette (`Evolve.ScatterCannon`, `EmbedBlast`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Pellets | `pellets` 8 | `Base.Count = 8`, 583; the plain weapon's count steps stay on its track (+4 at L3, 102) | differs at level 6: 8 against 12 (arithmetic, from the track) | More pellets per shot in the source, each a separate strike and blast |
| Pellet damage | the pellets "share the row's total damage": 7 × 16 / 8 = 14 each at level 6 (arithmetic) | not set: the plain pellet's, 3 plus the track's +2 at L2 and L6 = 7 at level 6 (arithmetic) | differs | Two thirds of the pellets in the port, each twice as hard; the port's shot totals a third more (arithmetic: 112 against 84) |
| Embed delay | `embed_s` 0.5 | `EmbedDelay = 0.5f`, 585 | same | |
| Blast radius | `embed_r_uu` 60 | `EmbedBlastRadius = 60.f`, 586, times the hero's Area, `RHSpecComposition.cpp` 651 | same at Area 1 | |
| Blast damage | `blast_dmg` 0.5 of the pellet | `DerivedDamageMult = 0.6f`, 587, of the pellet (`RHSpecComposition.cpp` 610) | differs | Each blast is a sixth weaker in the port (arithmetic: 0.5 against 0.6) |
| Embedding | "a pellet's hit leaves a strike" | the pellet lodges in the enemy: `Defer(Enemies.GetPos(e), ...)`, `RHBulletSubsystem.cpp` 1046 to 1049 | same | |
| Capabilities | Projectile; Area; Aimed; Expiry | Projectile, Aimed, Expiry, Area, 582 | same | |

## R-RFG-07 Blade Halo (`Evolve.OrbitalDrones`, `ContactBlades`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Blades | `blades` 4, no bolts | `Base.Count = 4`, `bNoBolts = true`, 594 and 602; "contact only", `RHWeaponInstance.cpp` 857 to 860 | same | |
| Who strikes, how often | "in turn, each row-cooldown share, one blade strikes every enemy within `contact_r_uu`" | every blade, every `ContactTick` 0.15 s (`RHWeaponDef.cpp` 600), strikes every enemy within its radius: `StepOrbiterContacts`, `RHBulletSubsystem.cpp` 1407 to 1444 | differs | The source strikes with all four blades about 6.7 times a second each (arithmetic); the port with one blade per share of the cooldown. The source strikes far more often |
| Contact radius | `contact_r_uu` 60 | `ContactRadius = 30.f`, 599, times Area (`RHSpecComposition.cpp` 635), plus the enemy's radius (`RHBulletSubsystem.cpp` 1431) | differs | The port's blade reaches twice as far from its centre (before the enemy's own radius, which the port's rule does not name) |
| Damage per strike | `dmg_ratio` 1.5 × the row's damage: 1.5 × 18 = 27 at level 6 (arithmetic) | `Base.Damage = 14.f`, 595, plus the plain track (+4 at L2, +6 at L6, 135 and 139) = 24 at level 6 (arithmetic), × `DerivedDamageMult` 1.0, 601 | differs | About an eighth harder per strike in the port (arithmetic); the rhythm above weighs far more |
| Orbit radius | `C.WEAPON.DRONE_ORBIT_RADIUS_UU` 160 × (1 + `speed_ratio` 0.8 × speed / `speed_ref_uu_s` 600) | `OrbitRadius = 150.f`, 596, × (1 + 0.8 × min(speed / 600, 1.5)), `RHWeaponInstance.cpp` 826 to 829; `MoveSpeedReference` 600, `RHWeaponDef.h` 130 | differs | 160 against 150 at rest. The source caps the widening at 1.5 times the reference speed and the port's rule names no cap |
| Orbit speed | no cell | `OrbitDegPerSec = 260.f`, 597 (the plain weapon's is 120, 133) | source has a value, the port has no cell | How fast a blade sweeps the ring decides how many enemies one pass meets |
| Capabilities | Orbit | Orbit, 593 | same | |

## R-RFG-08 Sweep Beam (`Evolve.RailLance`, `SweepBeam`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Beam time | `beam_s` 1.5 | the tip speed is set for "a 1.5 s, 120 deg sweep", 614 | same | |
| Sweep | `sweep_deg` 120 | `SweepArcDeg = 120.f`, 618 | same | |
| Reach | `reach_uu` 950 | `Base.Size = 950.f` "beam length", 613; `Range` 1100, 617 | same reach | |
| Aim | the densest line (R-WPN-15) | `TargetRule = DensestCluster`, 611 | differs | The source centres the sweep on a cluster, the port on a line |
| Cooldown | at least `beam_s` | `Base.Cooldown = 2.6f`, 615 | differs: the source states 2.6 s | The source rests 1.1 s between sweeps (arithmetic); the port's rest follows the row |
| How it strikes | an arc of `width_deg` 12 every `tick_s` 0.1 | the Arc Blade's sweep motion (`Pattern = MeleeArc`, 610; "the beam already hits all along it", 619) | the source has no tick or width cell | The port strikes what an arc covers every 0.1 s; how often the source's sweep strikes one enemy was not read |
| Damage | `dmg_ratio` 0.6 × the row's damage × count: 0.6 × 57 × 2 = 68.4 per tick at level 6 (arithmetic) | the plain 30 plus its track, with each extra-lance step made +10 damage and each size step +8, 620 to 627: 30 + 12 + 8 + 10 + 15 = 75 at level 6 (arithmetic) | differs in kind | Per tick in the port, per sweep in the source; not comparable without the source's strike rate |
| Count and pierce | through the row's count | `Base.Count = 1`, `Base.Pierce = 0`, and level count and pierce steps zeroed, 616 and 619 to 626 | differs | The port's damage grows with the row's lance count; the source's does not |
| Capabilities | Beam; Continuous; Aimed | Beam, Aimed, 609 | differs | A modifier that needs Continuous fits the port's and not the source's |

## R-RFG-09 Carpet (`Evolve.FlakMortar`, `CarpetLine`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Blasts | `blasts` 5: the landing and 4 strikes | `CarpetSteps = 4`, 634, plus the landing, `RHEvolutions.cpp` 140 | same | |
| Walk | `walk_uu` 600 | `CarpetSpacing = 150.f` × 4 steps, 635, the spacing times Area, `RHSpecComposition.cpp` 451 | same at Area 1 | |
| Step time | `step_s` 0.12 | `CarpetInterval = 0.12f`, 636; the k-th blast after 0.12 × k, `RHBulletSubsystem.cpp` 1840 to 1843 | same | |
| Blast damage | `dmg_ratio` 0.25 of the shell | `DerivedDamageMult = 0.7f`, 637, of the shell | differs | Each walking blast is under half as hard in the port (arithmetic: 0.25 against 0.7) |
| Blast radius | the shell's | `BlastRadius = S.BlastRadius`, `RHSpecComposition.cpp` 651 | same | |
| Capabilities | Lob; Area | Lob, Aimed, Area, Expiry, 632 | differs | Modifiers that need Aimed or Expiry fit the source's and go dormant in the port |

## R-RFG-10 Cyclone (`Evolve.ArcBlade`, `MoveSpin`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Arc | `arc_deg` 360 | `SweepArcDeg = 360.f`, 646 | same | |
| Rate | `hits_s` 2 | `Base.Cooldown = 0.5f` "2 hits / s", 644 | same | |
| Damage | `dmg_ratio` 0.6 × the row's damage × count | not set: the plain blade's damage per swing and its count track (+1 swing at L5, 243) | differs | At level 6, 0.6 × 28 × 2 = 33.6 per sweep in the port against 28 per swing and two swings, 56 a volley, in the source (arithmetic) |
| Only while moving | faster than `move_min_uu_s` 50 | faster than 0.1 × `MoveSpeedReference` 600 = 60 uu/s (arithmetic), `RHWeaponInstance.cpp` 256 | differs | The port spins at a slower walk |
| At rest | keeps its cooldown ready | "ready the moment the hero moves again", `RHWeaponInstance.cpp` 258 | same | |
| Spin speed | no cell | `Base.Speed = 4600.f`, "a 360 deg spin of 220 uu in about 0.3 s", 645 | source has a value, the port has no cell | How long a spin takes to pass round the hero |
| Capabilities | Melee; Area | Melee, Area, 643 | same | |

## R-RFG-11 Murmuration (`Evolve.SeekerSwarm`, `Persist`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Life | `life_s` 6 | `Base.Lifetime = 6.f`, 653 | same | |
| Pierce | `pierce` 200 | `Base.Pierce = 200`, 654 | same | |
| Damage | `dmg_ratio` 0.2 of the row's | not set: the plain missile's (no `DerivedDamageMult`; the missiles are the main rounds, `RHSpecComposition.cpp` 454 to 456) | differs | Each hit is a fifth of the source's in the port (arithmetic) |
| Cooldown | no cell: the row's (0.88 at level 6) | `Base.Cooldown = 1.6f`, 655, with the plain track's ×0.8 at L4 (268): 1.28 at level 6 (arithmetic) | differs | The source launches about two thirds as often (arithmetic) |
| Retarget | a new target after each hit, "never one the round already struck" | a new target after each hit through `FindRicochetTarget` with the round's history, `RHBulletSubsystem.cpp` 1051 to 1055; the history holds 8 enemies, `RHBulletSubsystem.h` 121 | differs | The source may strike an enemy again once eight others came after it; the port never |
| Capabilities | Homing; Projectile; Expiry | Projectile, Homing, 652 | differs | A modifier that needs Expiry fits the port's and not the source's |

## R-RFG-12 Pulse Wall (`Evolve.RingEmitter`, `PulseWall`)
As `dev/s2` stands. D-070 sets three of these cells to the source's and is not built here yet. The full table and its source lines are in `pulse_wall_rounds_plan.md` on `main`.
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Rounds | none: one ring a volley | none: one zone a volley, `RHWeaponInstance.cpp` 269 to 275 | same | |
| Radius | `radius_uu` 600 | `WallRadius = 600.f`, 664, times Area, `RHSpecComposition.cpp` 664 | same at Area 1 | |
| Growth | `grow_s` 0.6, from nothing | 0.6 s, from size 80, `RHSpecComposition.cpp` 659 to 665 | differs at the start | The source's ring starts 80 uu wide |
| Hold | `hold_s` 3 | `WallTime = 3.f`, 665 | same | |
| Tick | `tick_s` 0.25 | `LingerTickInterval = 0.25f`, `RHSpecComposition.cpp` 667 | same | |
| What it strikes | the outer `band_uu` 100 | everything inside its radius: a zone with pierce 255, `RHSpecComposition.cpp` 661 and 670 | differs | Inside the band the port strikes nothing |
| Damage | `dps` 2: 0.5 of a round a tick | `DerivedDamageMult = 1.5f`, 667: 1.5 of a round a tick, `dps` 6 in the port's unit (arithmetic) | differs | The source's tick is three times the port's (arithmetic) |
| Push | `push_uu_s` 200, steady, every step | `WallPush = 220.f`, 666, a radial knock on each tick's hit, `RHSpecComposition.cpp` 668 to 669, `RHBulletSubsystem.cpp` 1021 to 1024 | differs | A kick that decays and is divided by mass in the source (the plan note's arithmetic: about 73 uu a second on a light enemy) |
| Cooldown | at least the ring's life | `Base.Cooldown = 3.4f`, 662, against a life of 3.6 s | differs | The source's rings overlap by 0.2 s (arithmetic) |
| Capabilities | Projectile; Radial; Area; Expiry | Radial, Area, 661 | differs | Seven modifiers that need a round fit the port's and not the source's (D-070) |

## R-RFG-13 Vortex (`Evolve.SpiralProjector`, `Vortex`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| Curl | `curl_deg_s` 70 | `CurveDegPerSec = 70.f`, 675, `RHSpecComposition.cpp` 457 to 458 | same | |
| Reach | `vortex_uu` 500 | no reach cell; "about 500 uu out" in the comment, 672, from the curl and the plain round's speed and life | the source has no cell | The port fixes the reach; in the source it follows the round's flight |
| Zone radius | `zone_r_uu` 260 | `VortexRadius = 260.f`, 676, times Area, `RHSpecComposition.cpp` 678 | same at Area 1 | |
| Zone time | `zone_s` 1.2 | `VortexTime = 1.2f`, 677 | same | |
| Pull | `pull_uu_s` 180 | `VortexPull = 180.f`, 678, as a radial knock of −180 on each tick's hit, `RHSpecComposition.cpp` 681 to 682 | differs in kind | The source's pull is a kick per tick, like its wall's push, and the port's is a speed |
| Zone damage | none named | the zone strikes every 0.15 s (`LingerTickInterval`, `RHSpecComposition.cpp` 680) for `DerivedDamageMult` 0.2 of a round, 679 | the port has no cell | The source's vortex also deals 0.2 of a round about 6.7 times a second to all inside (arithmetic) |
| One zone at a time | none while the weapon has one live | a new one once 0.75 × the zone's life has passed since the last, `RHBulletSubsystem.cpp` 1449 to 1458 | differs | The source opens the next zone 0.3 s before the last ends (arithmetic) |
| Capabilities | Radial; Projectile; Continuous; Expiry | Projectile, Radial, Expiry, Area, 673 | differs | Area fits the source's and not the port's; Continuous the port's and not the source's |

## R-RFG-14 Singularity (`Evolve.GravityMine`, `Merge`)
| Cell or behaviour | Port | Source | | What it changes in play |
|---|---|---|---|---|
| When mines merge | a mine placed while one is live joins it | once 3 of the weapon's mines are on the floor they fold into one at their centre: `SingularityMerge = 3`, 690; `TryMergeMines`, `RHBulletSubsystem.cpp` 1725 to 1755 | differs | The port merges from the second mine on; the source waits for three and places the merged mine at their centre |
| Merged damage | the joined mines' damage added | one mine's damage × `DerivedDamageMult` 3, 694, `RHSpecComposition.cpp` 693 | differs | The port's grows with every mine that joins; the source's is three mines' worth |
| Pull radius | `pull_uu` 900 | `SingularityPullRadius = 900.f`, 691, times Area, `RHSpecComposition.cpp` 699 to 700 | same at Area 1 | |
| Pull time | no cell | `SingularityPullTime = 1.5f`, 692 | source has a value, the port has no cell | How long the crowd is held before the blast |
| Blast | `blast_ratio` 1.5 × the plain mine's area: 1.5 × 212.5 = 319 uu at level 6 (arithmetic, `weapons.csv` `area_r_uu`) | a radius of its own, `SingularityBlastRadius = 380.f`, 693, times Area, `RHSpecComposition.cpp` 702 | differs | The port's blast follows the plain mine's area and its size steps; the source's is 380 at Area 1 whatever the level |
| Lone mines | no cell | `ArmTime = 99.f`, they never trigger on approach, and `MineLife = 8.f`, a lone mine goes off after 8 s, 687 to 688; `Base.Cooldown = 1.4f`, 686 | source has a value, the port has no cell | In the source single mines wait to merge |
| Capabilities | Mine; Area | Mine, Area, 685 | same | |

## Counts
Arithmetic, over the rows above. Of the ten evolutions:
- every one has at least one cell that differs;
- all ten differ in their damage per strike or in what deals it;
- five differ in capabilities: Sweep Beam, Carpet, Murmuration, Pulse Wall, Vortex.

Every `dmg_ratio`, `blast_dmg` and `dps` that the `ref` column marks "first guess" differs from the source, except where the source has no cell at all.
