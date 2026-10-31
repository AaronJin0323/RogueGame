# S3 Tree: the first-run pool and the tool-run proof

Spec: `02_Systems/meta.md` R-MET-20 to R-MET-26. Every bot number here is a bound (bot v2, skill 1.0), not a human reading. No human has played the Tree, and it has no screen yet.

## Tool runs are unchanged
`Tools/BotRuns.ps1 -BotVersion 2 -FirstSeed 9107 -Seeds 1`, the same four arms on a build of `dev/s3-base` and on this branch:

| Arm | Base input_hash | Tree input_hash | Base steps | Tree steps |
|---|---|---|---|---|
| None | 7bd32faf | 7bd32faf | 30114 | 30114 |
| Steered:Storm:Tempest | b5fe3b6c | b5fe3b6c | 58812 | 58812 |
| Steered:Tide:Frost:Forge | dd5bf564 | dd5bf564 | 48394 | 48394 |
| Steered:Shade:Hound:Storm (S2 Keeper, `-DTUnlock`) | bdba5258 | bdba5258 | 58084 | 58084 |

`Tools/MetaCheck.ps1` adds the other direction on every gate: a full Tree with the gate on plays a tool run's seed with the same hash, steps and CSV, so no node reached a number. The bot never uses a tool, so the Trunk's extra counts are not exercised there; T-MET-20 and T-MET-24 cover them.

## A bot on a fresh-save pool
`Tools/BotRuns.ps1 -BotVersion 2 -FirstSeed 9100 -Seeds 10`, the nine S1 archetypes as Steered arms with no second Keeper named, 90 runs each way. `-FreshPool` runs carry the empty profile. No hand of the 90 fresh runs holds an Amp, a Trait or a Path row.

| Keeper (30 runs) | Full reach / wins | Fresh reach / wins | Full Keeper share | Fresh Keeper share |
|---|---|---|---|---|
| Storm | 25 / 18 | 30 / 28 | 0.663 | 0.445 |
| Forge | 30 / 26 | 30 / 27 | 0.803 | 0.494 |
| Tide | 7 / 3 | 21 / 19 | 0.497 | 0.364 |
| All | 62 / 47 | 81 / 74 | | |

| Read | Full | Fresh |
|---|---|---|
| Decisions, runs that reach the boss | 29.0, 62 of 62 within tolerance | 29.2, 81 of 81 within tolerance |
| Reserve share, mean and worst run at the boss | 0 and 0 | 0.023 and 0.081, under `C.OFFER.RESERVE_SHARE_MAX` 0.1 |
| Picks: Blessing, Weapon upgrade, Modifier | 1663, 208, 118 | 514, 1073, 549 |
| Reforging picks | 78 | 37, evolutions only |

**The narrow pool beats the full pool.** The bound runs the wrong way from what a tree implies: with no Amps to take, the bot's hands turn to weapon upgrades and modifiers, and those carry it further than blessings do. A locked pool is no wall to a first run, the budget holds, and Reserve stays small. It also means, for this bot, every blessing node bought lowers its win rate, and Keeper share falls well away from the pillar that Keepers drive power. Nothing was tuned on this. Two reasons it may be the bot's shape and not the game's: skill 1.0 dodges well enough that raw weapon levels pay, and its steering takes blessings first once it has them. The same gap in a human reading would be a design problem: a player who unlocks would get weaker. It stays open in `meta.md` and needs a human on a fresh and a full profile. D-022 keeps anything from this off `dev/s2` and `main`.

The fresh pool is narrower than anything the S1 and S2 readings saw, so none of their bands speak for a person's first run; they speak for a full profile, as they did.

## Cost table against the award
Awards on the 90 full runs: median win 1499, median death 837. Fresh runs: 1496 and 858.

| Span | Price | Bound |
|---|---|---|
| First branch step | 300 | one death |
| One S1 branch | 1600 | about one win or two deaths |
| Trunk | 1200 | about one win |
| Trunk and three S1 branches | 6000 | four wins or about seven deaths |
| Whole Tree | 10800 | about seven wins; three branches are shut (below) |

Prices are invented, marked in `number_change_log.csv`. A human earns less per run than this bot, so these are the short end.

## Decided here
- Blessing #4 and #5 follow `build_depth_spec` §2.2, because a Keeper has twelve cards here and not five. A Trait opens with its archetype instead of through a capstone Discovery, because `ST_Discoveries` has no such row.
- `Evolve.<Weapon>` rows stay in on a fresh save, so L16 still offers a real card when the primary weapon qualifies; only `Transform.<Path>` is the Keeper's to give.
- Gating follows the process, not the file: a bot with `-DTMetaFile` is still a tool run with the full pool, which keeps the `meta` gate step's hash comparison true. `-DTPoolGate` is the one way a tool asks for a person's pool.
- A branch of an S2 Keeper needs `Keeper.<Path>` first.

## Not verified
- A person's launch was not run: every launch here is headless. The gate path was read through `-DTPoolGate`, which takes the same branch.
- Nobody writes `Keeper.<Path>` or `Reaction.Duos` yet, so on a person's save the three S2 branches cannot be bought and S2 Keepers cannot be met. This matches today's reach but leaves nine nodes dark.
- Until the screen lands, a person on this branch has the first-run pool and no way to spend. `DT.Tree.Buy` works in non-Shipping builds.
- The Trunk's tools in a played hand: covered by T-MET-20 and T-OFF-10's counts, no bot reads them.

## Invented zh (no source row; for a native reader)
| Key | zh-Hans |
|---|---|
| `Tree.Branch.Trunk` | 你自己的条款 |
| `Tree.Cost` | {0} 神血 |
| `Tree.State.Short` | 神血不足 |
| `Tree.Trunk.Reroll.Name` | 重议条款 |
| `Tree.Trunk.Banish.Name` | 拒约条款 |
| `Tree.Trunk.Lock.Name` | 保留条款 |
| `Tree.Branch.Advance4.Desc` | {0}开放前两个流派的后续预支。 (en invented too) |
| `Tree.Branch.Advance5.Desc` | {0}开放最后一个流派的后续预支。 (en invented too) |
| `Tree.Branch.Reforging.Name` | 神铸权 |
| `Tree.Branch.Reforging.Desc` | 升到 16 级时，{0}可以为你神铸。 (source line, shortened) |

The Advance rows use 流派 for archetype, which has no glossary row yet. `Tree.Branch.Advance5.Name` keeps the source's "Fifth Advance", 第五项预支, though the node now opens three cards: a name to re-read with the screen.
