# S3 Discoveries: the 15, what a bot earns, and the sim check

Branch `dev/s3-discoveries`, from `dev/s3-base` at `364e264`. Spec: `Docs/02_Systems/meta.md` R-MET-40 to R-MET-45. Catalog: `Docs/09_Catalog/discoveries.csv`.

## Reading

A bot at skill 1.0 earned **10 of 15 on the default pool** and **13 of 15 once S2 content is unlocked and a weapon is carried**. No single run earned more than 8 on the default pool or 9 with unlocks. Two rows were never met: `Elites10` cannot be, on arena 1, and `Crit500` was not, in 135 runs.

- Arm A: bot v2, skill 1.0, the 18 archetypes, seeds 1 to 5, the default pool: 90 runs (`BotRuns -ArmSet S2Keepers`; S2 Keepers are unlocked for their own arms, as the harness does).
- Arm B: bot v2, skill 1.0, `-Unlock S2`, Tempest, Slam and Frost carrying each of Scatter Cannon, Rail Lance and Orbital Drones, seeds 1 to 5: 45 runs.

| Discovery | Condition | Arm A, of 90 | Arm B, of 45 |
|---|---|---|---|
| `ReachL10` | level 10 | 90 | 45 |
| `Kills1000` | 1,000 kills | 90 | 45 |
| `Survive10` | 10:00 | 84 | 40 |
| `Favor9` | Tier 3 on a Path | 81 | 34 |
| `TwoPaths6` | Tier 2 on two Paths | 78 | 26 |
| `Win` | win | 71 | 23 |
| `Freeze100` | 100 freezes | 44 | 30 |
| `DroneKills300` | 300 with Orbital Drones | 30 | 16 |
| `WeaponL8` | weapon at level 8 | 3 | 13 |
| `Dashes100` | 100 dashes | 2 | 0 |
| `RailKills300` | 300 with Rail Lance | 0 | 17 |
| `ScatterKills500` | 500 with Scatter Cannon | 0 | 15 |
| `Elite3Mods` | elite killed by a weapon with 3 modifiers | 0 | 8 |
| `Crit500` | a 500+ crit | 0 | 0 |
| `Elites10` | 10 elites | 0 | 0 |

These are bot counts: the bot skips primary upgrades unless it carries (`S2_bot_v2.md`), which is why `WeaponL8` and `Elite3Mods` move so much with a carry. The bot's dashes are a poor stand-in for a person's.

## Unreachable or blocked

| Row | State | Why |
|---|---|---|
| `Elites10` | **unreachable** | `timeline_arena1.csv` spawns 2 + 3 + 4 = 9 elites a run; 103 of 135 runs read exactly 9, none above. The row is kept at 10 because its text says 10 |
| `Crit500` | not earned | best crit 285, on a Radiance arm; only Radiance arms crit at all. Not proved unreachable |
| `ScatterKills500`, `RailKills300` | blocked on the default pool | both weapons are `starter = no`, and no Discovery target opens them. Reachable only if the Tree or a gating call does |

**Roles and items that do not exist.** None of the 15 conditions names a missing role: Breaker and Tallyman appear only in the bestiary rows `Codex.Enemy.Bruiser` and `Codex.Enemy.Support`, whose `Kill.Bruiser` and `Kill.Support` stay unreachable until those roles exist. What is missing is seven source targets, modifiers this catalog does not hold. Those rows still earn and still open their Recollection, and write no pool key:

| Discovery | Source target (no catalog row) |
|---|---|
| `Crit500` | Crit Lens |
| `Kills1000` | Magnify |
| `WeaponL8` | Overclock |
| `RailKills300` | Heavy Slug |
| `Elites10` | Burst Capacitor |
| `Dashes100` | Velocity Coil |
| `Favor9` | Lifeline |

The source's Divine Conduit is `Mod.Conduit` here, the same rule under a shorter name.

## For the pool gating work

- Written keys: `Discovery.<Id>` for each earned row, and targets as catalog ids: `Weapon.FlakMortar Weapon.ArcBlade Weapon.SeekerSwarm Weapon.RingEmitter Weapon.SpiralProjector Weapon.GravityMine Mod.ScatterShell Mod.ChainTrigger Mod.Conduit Mod.SpreadWave Mod.LingeringField` (R-MET-43).
- Six targets are `starter = yes` today, so their key opens nothing until gating takes them out of the start pool: `Weapon.ArcBlade`, `Weapon.RingEmitter`, `Mod.ScatterShell`, `Mod.ChainTrigger`, `Mod.Conduit`, `Mod.LingeringField`.
- Nine `starter = no` rows have no Discovery opening them: `Weapon.ScatterCannon`, `Weapon.RailLance`, `Mod.Fork`, `Mod.Ricochet`, `Mod.Homing`, `Mod.OrbitLock`, `Mod.Boomerang`, `Mod.Spillover`, `Mod.Refit`.
- `Freeze100` opens the Ring Emitter from a Keeper's status, and `ScatterKills500` opens a modifier from a weapon that is itself locked. Both are the source's; neither was changed.

## The sim is untouched

Seed 9107, bot v2, skill 0.6, Storm Tempest, headless, base build against this branch's:

| Run | `dev/s3-base` | `dev/s3-discoveries` |
|---|---|---|
| 90 s | `276faf8e`, 5400 steps | `276faf8e`, 5400 steps |
| to its end (death at 578.9 s) | `db7d94d6`, 34732 steps | `db7d94d6`, 34732 steps |
| to its end, through `-DTMetaFile` | | `db7d94d6`, 34732 steps |

A second launch on that save read `db7d94d6` with `steps` 34733, one apart with an equal hash: the death-tail reading already noted in `S2_package.md`. T-MET-16 still holds, and the only pool change is two counters nothing in the sim reads.

Counting every Freeze application instead read 55,969 on this run against 547 real freezes, which is why `Status.Freeze` counts the change of state (R-MET-41).

## One real run through a save

Same seed, `-DTMetaFile` on an empty isolated file, no planted counter. Launch 1: `banked 485`, `Meta: discoveries met 3 of 15 [ReachL10 Freeze100 Kills1000], new [Freeze100 Kills1000 ReachL10]`. Launch 2 on the same file: `opened (2) ... balance 485 ... discoveries 3`, then `new []`, the balance 970, and no second toast. The default `DTMeta.sav` was not created.

Planted counters: T-MET-41 walks every row one short and at its threshold, and `Tools/MetaCheck.ps1` plants `Dashes 100` on a real launch, with a file and without.

## Not verified

- The toast has not been seen: tool runs are `-nullrhi`, and a no-file bot run adds none, so no screenshot exists. T-MET-44 proves the text, not the layout, at any text size. HUMAN.
- `Elite3Mods` counts dormant sockets. No run separates the two readings.
- A Blind's short Freeze counts toward `Freeze100` by the rule; the arms' split between it and a true Tide freeze is unread.
