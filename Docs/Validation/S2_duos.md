# S2 duo reactions and duo blessings (2026-10-31)

Branch `dev/s2-duos`, from `dev/s2`. Class: SYSTEM. Layer: a Keeper layer on the weapon seams (`reactions.md`). Replaces nothing: no Keeper number and no T-KEE criterion moved. Bot and tool readings only. **A bot reading is never a human reading**, and no human has played any of this.

## Reproduce
```
$Arms = "Steered:Storm:ArcGrid:Forge,Steered:Storm:Downpour:Tide,Steered:Shade:Hound:Storm,Steered:Radiance:Lance:Storm,Steered:Quicksilver:Rebound:Storm,Steered:Forge:Furnace:Tide,Steered:Shade:Hunters:Forge,Steered:Radiance:Solar:Forge,Steered:Quicksilver:Momentum:Forge,Steered:Shade:Economy:Tide,Steered:Radiance:Solar:Tide,Steered:Quicksilver:Rebound:Tide,Steered:Radiance:Prism:Shade,Steered:Shade:Hound:Quicksilver,Steered:Radiance:Lance:Quicksilver"
& Tools\BotRuns.ps1 -Arms $Arms -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Stagger 0.4 -Unlock Reaction.Duos -Out Saved\BotRuns\duos_v2
& Tools\BotRuns.ps1 -Arms $Arms -Seeds 15 -BotVersion 2 -Skill 1.0 -Parallel 12 -Stagger 0.4 -Out Saved\BotRuns\duos_control
python -I Tools\reaction_read.py Saved\BotRuns\duos_v2 Saved\BotRuns\duos_control
```

## Failing first
`Dusktracer.Reactions.*` went in over stubs (commit daffb63): 39 of 41 red, no crash, every test reached. The two green were `Live` (a pure function, not stubbed) and `Stage` (a negative: nothing fires). The first green passes found test faults, not game faults: a scene past the rifle's reach was clamped onto the arena edge, and an enemy index went stale after a swap-removal. One game fault came from the battery: `-DTUnlock=a,b` stopped at the comma, so an arm of two S2 Keepers lost its second (fixed; that also made the first battery's arms wrong, and it was thrown away).

## S1 did not move
Six S1 arms, 15 seeds, v2 at skill 1.0, nothing unlocked, this branch against `dev/s2` at 00289bd built and run the same way: every cell equal (60 wins of 90 on both). That is not the 66 of `S2_arsenal.md`: `dev/s2` itself moved after that reading.

## Bot battery, skill 1.0, 15 seeds per arm
Fifteen arms, one per pair. Each with `Reaction.Duos` against the same arm without it.

| Arm | Pair | Wins | Control wins | Median death | Card held | Fires/run | Reaction damage share | First fire s |
|---|---|---|---|---|---|---|---|---|
| Forge/Furnace:Tide | ForgeTide | 3/15 | 5/15 | 11.4 | 12 | 3510 | 0.104 | 281 |
| Quicksilver/Momentum:Forge | ForgeQuicksilver | 13/15 | 15/15 | 14.6 | 15 | 1 | 0.017 | 292 |
| Quicksilver/Rebound:Storm | StormQuicksilver | 15/15 | 14/15 | - | 15 | 598 | 0.005 | 350 |
| Quicksilver/Rebound:Tide | TideQuicksilver | 13/15 | 15/15 | 14.0 | 12 | 563 | 0.000 | 339 |
| Radiance/Lance:Quicksilver | RadianceQuicksilver | 15/15 | 15/15 | - | 15 | 64 | 0.000 | 483 |
| Radiance/Lance:Storm | StormRadiance | 14/15 | 15/15 | 16.9 | 14 | 183 | 0.024 | 289 |
| Radiance/Prism:Shade | ShadeRadiance | 15/15 | 15/15 | - | 14 | 1497 | 0.123 | 297 |
| Radiance/Solar:Forge | ForgeRadiance | 14/15 | 14/15 | 16.5 | 15 | 345 | 0.022 | 288 |
| Radiance/Solar:Tide | TideRadiance | 7/15 | 8/15 | 11.3 | 12 | 445 | 0.012 | 326 |
| Shade/Economy:Tide | TideShade | 14/15 | 13/15 | 6.2 | 15 | 440 | 0.032 | 350 |
| Shade/Hound:Quicksilver | ShadeQuicksilver | 15/15 | 15/15 | - | 15 | 0 | 0.000 | 607 |
| Shade/Hound:Storm | StormShade | 14/15 | 15/15 | 16.3 | 15 | 1351 | 0.047 | 348 |
| Shade/Hunters:Forge | ForgeShade | 14/15 | 14/15 | 15.2 | 14 | 1102 | 0.020 | 302 |
| Storm/ArcGrid:Forge | StormForge | 13/15 | 13/15 | 12.9 | 13 | 963 | 0.042 | 290 |
| Storm/Downpour:Tide | StormTide | 13/15 | 10/15 | 16.0 | 15 | 1601 | 0.014 | 331 |
| **All** | | 192 of 225 | 196 of 225 | | | | | |

Every pair fired in at least one run. Every number above is at skill 1.0, a bound; a shift of 2 or 3 on 15 seeds can be noise. Damage share counts `Keeper.Duo.<Pair>` sources; a reaction's status and control do not show in it.

T-KEE-15 and T-KEE-18 were not reread: their probes do not unlock reactions, and the job changes no pass criterion.

## Hypotheses of the number log
- "No single reaction above 15% of its arm's damage": held; the highest is Exposure at 12.3%.
- "Writ, Flash Arc, Live Rail fire on their arms; Refraction and the ember hit": all fired. Hit rates of embers and shards were not read apart.
- Cold Storage's Frozen share and Lightning Step's field count: not read; nothing in the battery isolates them.

## Perf at caps
| Scene | Fires in the run | Game p50 / p95 ms | GPU p95 raw (proxied) | Verdict |
|---|---|---|---|---|
| Default, nothing unlocked | none | 2.05 / 2.51 | 3.60 (5.75) | PASS |
| Caps with a live pair: 5975 bullets, 389 enemies | StormForge 3 | 2.22 / 2.76 | 3.90 (6.24) | PASS |
| Weapons only: 70 bullets, 398 enemies | StormForge 41 | 1.31 / 1.63 | 1.59 (2.55) | PASS |

The commands are in `Saved/Perf/perf.csv`; `Tools/Perf.ps1` prints a scene's fires from its telemetry. Scene three runs 17 s at about the rate the battery shows on a typical arm (2 to 3 fires a second). **The stress emitter kills the crowd before weapons reach it**, so at the bullet cap only 3 reactions fired: caps and reaction load were not met in one scene. The worst arm (Steam, about 6 a second over a run) and a field-heavy Arc Furnace are staged in `S2_perf.md`.

## What the source lacked, and what was invented
All marked in `reactions.md`; numbers in `number_change_log.csv`.
- The stage gate `Reaction.Duos`: the source has reactions always on. It keeps S1 runs and every T-KEE and T-OFF reading unchanged.
- Damage for Writ, Flash Arc, Live Rail, the ember and Deep Ledger's shatter, and every reach but Conduction's, Plasma's, Steam's and Exposure's.
- Bonus speed as Momentum: nothing else raises speed here.
- Deep Ledger needs no execute of its own; it would never fire off Hound.
- Summons' second half: a Writ bolt only strikes Marked enemies, so "Mark what they hit" did nothing.
- The toast's place, under the director's banner: no Keeper-line slot exists. The cue is two rings for all fifteen, not the source's fifteen Looks.
- Lightspeed's refund limit.
- All zh names of duo cards; the zh toasts of Steam, Flare, Wildfire and Afterglow (the source has none).
- Left out: Souls (Deep Ledger, Cold Storage), Wildfire's ember trail, Glide's first half, the card text's "Boosts X and Y" (no rule behind it, and Breakwater does not exist), cross cards, the second duo set.

## What reads wrong (for a human session, none of it tuned)
- **Duos add no wins**: 192 against 196 of 225. Against an arm that already wins, a 2 to 12 percent damage layer may not show, so this says little; but no arm was lifted either.
- **Steam spends what the pair built.** Forge/Furnace:Tide fires 3510 times a run and wins 3 of 15 (5 without). Consuming Freeze thaws the crowd and consuming Burn stops the Furnace's fields (`HitOnBurning`). It is the source's rule and it may fight the arm.
- **Repossession does not fire**: 15 runs, median 0 fires, the first at 607 s. The bot dodges bodies and a dash needs a low Marked enemy on its line. This is the bot; a human reading decides.
- **Wildfire is either always on or never**: a few Momentum stacks pass `speed_over`, so it starts once and stays lit (1 fire a run, 1.7% of damage). Without Momentum and with no field it needs Afterburner.
- **Quicksilver reactions carry no damage**: Black Ice and Afterglow read 0.000. Their worth is control and crit, which this table cannot see. Rebound:Tide lost two wins to its control and Lance:Quicksilver none, within noise.
- **Exposure is the largest at 12.3%** and is a state, so it runs on every hit of a stunned Marked enemy, past the per-enemy cooldown. Blinding enemies at 15% of hits makes that common on Radiance arms.
- **Fields and arcs add emission load** through `TryEmit`: Arc Furnace and Lightning Step spend a weapon's spawn cap (R-WPN-09), so a duo shares its rifle's cap in a dense fight. Measured in `S2_perf.md`: the field loses, not the rifle.
- **A Tide or Forge Patron with an S2 second is not reachable by a bot arm**: the Patron hand holds the unlocked Keeper first and three cards, so the arm's Patron may be absent and the bot takes another. Arms here use the S2 Keeper as Patron for that reason. A human sees the same hand.
