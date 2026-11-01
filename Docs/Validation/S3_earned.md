# S3 Earned: a person's loop, end to end

Branch `dev/s3-earned-path`, from `dev/s3` at `5ccf82e`. Spec: `02_Systems/meta.md` R-MET-30, T-MET-30, T-MET-31; `keepers.md` R-KEE-23. Proof: `Tools/EarnedCheck.ps1`, gate step `earned`, off-screen, en. Every played run here is a bot (bot v2, skill 1.0), so every award and hand is a bound, not a human reading. No human has played any of this. No balance number moved. Every launch of the proof is `-RenderOffScreen`, none through `Play.bat`.

**H04.** Class SYSTEM (a proof) with one RULE (the Patron hand's last seat). Layer: tools, and a seat rule in the offer system. Replaces: the planted balance as the only evidence of a purchase (`S3_vigil.md`), `DT.Tree.Buy` as the only buyer of the doors' rows (`S3_doors.md`), and the Open row on Restart's hand in `meta.md`.

**Hypothesis.** We believe one repeatable proof moves "no launch earned its own purchase" to a gate step, and one drawn seat moves a met Keeper's reach of a Restart hand from "only when played last" to every Keeper of the run, because the page and the doors first met at integration and Path order never reached past Tide; measured at the external S3 session on a fresh and a full profile; reverted if a person reads the drawn seat as noise or misses an S1 Keeper it drew past.

## Tool runs are unchanged
Seed 9107, base `5ccf82e` built before any edit, against this branch.

| Reading | Base input_hash | Branch input_hash | Base steps | Branch steps | `bot.csv` |
|---|---|---|---|---|---|
| `Tools/RestartCheck.ps1` launch A | 276faf8e | 276faf8e | 5400 | 5400 | |
| None | 7bd32faf | 7bd32faf | 30114 | 30114 | same bytes |
| Steered:Storm:Tempest | b5fe3b6c | b5fe3b6c | 58812 | 58812 | same bytes |
| Steered:Tide:Frost:Forge | dd5bf564 | dd5bf564 | 48394 | 48394 | same bytes |
| Steered:Shade:Hound:Storm | bdba5258 | bdba5258 | 58084 | 58084 | same bytes |
| Steered:Storm:Tempest, skill 0.6 | db7d94d6 | db7d94d6 | 34734 | 34734 | same bytes |

`Tools/MetaCheck.ps1` step G holds the other direction on every gate: a gated profile at the tool pool has no met Keeper, draws nothing, and plays the tool run's seed.

## The hand, decided
**For a D-number.** In a gated run with more Keepers than `C.OFFER.KEEPER_CHOICES`, the last seat of the Patron hand is an unweighted draw over every Keeper not yet seated. The lead keeps the first seat, Path order the seats between. Rule text: `keepers.md` R-KEE-23, `offers.md` R-OFF-08; test: T-MET-31.

The brief describes this hand as "LastPatron then met Keepers". That was the first pass of `dev/s3-doors`, and the review of it removed it (`S3_doors.md`): at `5ccf82e` the code and `meta.md` Open read `LastPatron`, then Path order. So S1 Keepers were no longer shut out wholesale; what was left was the other half. `Shade Storm Forge` drops Tide for every run on a met Keeper, `Storm Forge Tide` holds no met Keeper at all, and a met Keeper not played last could never reach this hand. The decision answers that hand, and keeps what the brief asked to keep: a met Keeper has no seat of its own, as R-KEE-23 says for the L8 hand, and no S1 Keeper is shut out by a save.

| Option | Call | Why |
|---|---|---|
| A fourth seat, or every Keeper in the hand | no | raises `C.OFFER.KEEPER_CHOICES` for a save: a number meta moves (D-023) |
| A seat for each met Keeper | no | the first pass; reads `Shade Radiance Quicksilver`; the review's bug |
| A fixed seat for the first met Keeper | no | takes Tide's seat for good on every save and leaves two met Keepers out |
| Path order as it stood | no | a door opens a Keeper this hand can never show, and Tide drops on every met lead |
| Cycling the seat by `Runs` | no | a new field in the grant to do what one draw does |
| **The last seat drawn** | **kept** | one seat, not one for each; the same plain draw as the L8 hand, with no Favor to weigh it |

What it costs: on a save with met Keepers, Forge and Tide lose a certain seat. With three met and a lead, the last seat reads each of four Keepers one hand in four; with one met, one in two. The stream moves by one draw on a gated run with a met Keeper, a hub run included, though no person sees that hand. A seed still replays for its profile (R-MET-16). A run with no met Keeper draws nothing, so a first run and every tool run read as they did.

Read on real launches, Patron hands in `Saved/EarnedCheck/hands_<tag>.txt`: on named seeds `Shade Storm Forge`, `Radiance Storm Tide`, `Quicksilver Storm Shade` and `Storm Forge Radiance`; with no met Keeper `Storm Forge Tide`. The Shrine chain's runs take fresh seeds: on one pass its hands led by Quicksilver, the last a real Restart, read `Quicksilver Storm Tide` and `Quicksilver Storm Shade`.

T-MET-31 walks all eight sets of met Keepers under every lead, on 256 seeds a hand. `Tools/MetaCheck.ps1` step I now reads the lead and Storm, and leaves the last seat to the draw. D-023 holds: the hand is the same size, and nothing here touches a Favor, a stat or a cost.

## Part one: Ichor earned, spent, and the pool changed
From an empty save, seed 9100, Storm Tempest, nothing planted. No launch here carries `-ExecCmds` or `-DTUnlock`.

| Launch | What it read |
|---|---|
| `earn`, a gated bot run | `Offers: pool gated, 0 unlocked`; victory; `Meta: banked 1500, balance 1500, runs 1, wins 1`; new `Freeze100 Kills1000 ReachL10 Survive10 WeaponL8 Win`. Thirty hands, none holding an Amp or a Trait |
| `Earned`, a second process, real keys | `Hub: open, balance 1500`; Contracts; six presses of Down reach `Branch.Storm.Blessing4`, state Open; Enter; `Tree: bought Branch.Storm.Blessing4, balance 1200`; rank and cards in the session |
| `EarnedRead`, a third process | `Meta: opened (2)`, balance 1200; the rank and the cards, and only those; the page reads `Complete` |
| `next`, the same seed, gated | `Offers: pool gated, 22 unlocked`; thirty hands, six holding a bought Storm card, the first at level 4, all six taken; `Meta: banked 1501, balance 2701, runs 2, wins 2`; new `Favor9`, the row R-MET-27 says waits on this node |

The same seed, the same bot, one purchase apart: no Forge or Tide Amp reached a hand, and the balance adds up across all four launches. Shots `earned_open`, `earned_bought`, `earned_after` read `Ichor held: 1,200` and `Complete` on the page.

## Part two: the doors' rows on the page
On a copy of the save `earn` left, so `Keeper.Shade`, `Keeper.Radiance` and `Keeper.Quicksilver` are doors a run opened. Only the balance is planted, with `DT.Meta.Currency`, as `Tools/VigilCheck.ps1` does: five nodes at `C.TREE.COST_BLESSING4`.

| Step | What it read |
|---|---|
| `Doors`, keys | `Branch.Storm.Blessing4`, `Branch.Forge.Blessing4`, `Branch.Shade.Blessing4`, `Branch.Radiance.Blessing4`, `Branch.Quicksilver.Blessing4` bought by Enter, exact money to 0. Shot `doors_bought` |
| `Doors`, keys, gated | each new Keeper chosen at the Shrine, Begin, a run on it with no hand left, Abandon, back, `LastPatron` saved; then a real Restart: a run with no `DTPatron`, on Quicksilver, the Keeper last played |
| Shade, seed 9121, named | `Shade Storm Forge`; L8 `Quicksilver Radiance Forge`; three of ten hands hold a bought Shade card, from level 4 |
| Radiance, seed 9122, named | `Radiance Storm Tide`; L8 `Tide Quicksilver Forge`; three of ten, from level 4 |
| Quicksilver, seed 9123, nobody named | `Quicksilver Storm Shade`, the hand a Restart reads; three of eight, from level 3 |
| Storm, seed 9130 | L8 `Tide Forge Shade`, so a Keeper 4 to 6 in an L8 hand; a death, 856 banked; new `DroneKills300 Favor9 TwoPaths6`, so `Reaction.Duos` is in the file. No duo card in its hands |
| Storm, seed 9230, the next run | `Offers: pool gated, 52 unlocked`; `Blessing.Duo.StormForge` offered at level 11 and taken; victory |

An L8 hand on this save cannot miss: with Storm the Patron, two S1 Keepers and three met Keepers are left for three seats.

## Can the proof fail
| Mutant, thrown away after | Result |
|---|---|
| `next` plays a copy of the save with no purchase | `EARNED: FAIL (the loop on seed 9100: page=True read=True next=False, banked 1500)` |
| Met Keepers seated, the first pass's rule | tests 2 failed of 503: `Dusktracer.Meta.PatronSeats` and `Dusktracer.Meta.KeeperDoors`, on the seat and on every Keeper reaching the hand |

## Cost, and why it is a gate step
The first cut, eleven launches, read 129 s at low load, its bot-free launches 10 s to 19 s and its bot launches about 10 s each. Folding the Shrine chain into `Doors` took one engine start off it, leaving ten, three of them at once. With six engines of other sessions running the script read 224 s and `Earned` 41 s against 14 s. The biggest wait in those logs, up to 25 s, sits at engine start before `UBT AutoSDK ReturnCode: 0`, a stretch every off-screen launch of the gate shares. It stays a gate step: about two minutes at low load, past three on a busy machine.

## Not verified
- A person's launch: every launch is `-unattended`, `-DTHub` opens the door, auto-pick takes the Patron hand, and a bot plays. `-DTPoolGate` takes the branch a person's run takes.
- A real device: keys are simulated events through the player controller, keyboard but for one pad Back. `Tools/VigilCheck.ps1` reads both devices on the page, on no real device either.
- zh-Hans: this proof is en. The page's rows in zh are `S3_vigil.md`'s.
- A package or Shipping: nothing here ran there. The gate's `pad` step reads NOT RUN with no package in this worktree.
- A person's first purchase: this bot banks 1500 on a win. The owner's run gives 284 by R-MET-02 against the cheapest node at 300 (`S3_vigil.md`), and the award's scale stays held in `meta.md` Open.
- Trunk nodes, `Blessing5`, `Reforge` and `Branch.Tide.*` were not bought here on any money. `Branch.Storm.Blessing4` is the only node bought on earned Ichor.
- How the drawn seat reads to a person: whether losing a certain Forge or Tide on Restart feels like a loss, and whether a Keeper seen there tempts anyone. The Shrine stays the way to choose exactly.
- Balance on a met save: no band was read, and T-KEE-15 and T-KEE-18 were not rerun. `S3_doors.md` carries them as FAIL on Keepers 4 to 6.

## Noted, not changed
- `Tools/BotRuns.ps1 -Parallel` with several arms on one seed can land in one second and share a telemetry name: `pressure.py` then raised a `ValueError` on the mixed CSV and the battery read FAIL. Its per-run `bot.csv` stayed exact, and the same arms read PASS serially. My own call hit it; distinct seeds do not.
- A bot on a save with met Keepers and no `DTPatron` can find its `-DTBotPath` out of the hand now, and takes the lead, which scores the same as the rest. Nothing in `Tools/` does this today: `BotRuns.ps1 -FreshPool` has no met Keeper and `MetaCheck.ps1` names the Keeper or plays the lead. A future battery on a met save should name its Patron.
