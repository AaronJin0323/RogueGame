# S3 Doors: the content a person could not reach

Spec: `02_Systems/meta.md` R-MET-25, R-MET-27, R-MET-28, T-MET-27 to T-MET-29; `keepers.md` R-KEE-23. Every bot number here is a bound (bot v2, skill 1.0), not a human reading. No human has played any of this. The owner's run (build `996cd47`, one designer run, not a validator session) is cited only where it says something a bot cannot.

**H04.** Class SYSTEM. Layer: content (four cells of `discoveries.csv`) and one rule (the seat rule in a gated run). Replaces: "no writer yet" in R-MET-25, the `unreachable` state of `Crit500` and `Elites10` in R-MET-27, the seat rule on a person's save, and the hand-run `-FreshPool` read of R-MET-25. The review's two fixes are class BUG, same layers: `Keeper.Shade`'s cell on `ReachL10` replaces its cell on `Kills1000`, and Path order in a gated Patron hand replaces the met Keepers' seats.

**Hypothesis.** We believe four door cells move the rows a person can never reach from 9 Tree nodes, 3 Keepers, the duos and `Crit500` to none, because each door is a row a bot already earns, with no number moved; measured at the external S3 session on a fresh profile; reverted if a first run's new Keepers read as noise or a door sits too shallow.

## Tool runs are unchanged
`-DTBotVersion 2`, seed 9107, base `c6f02d2` against this branch. `bot.csv` is byte-identical on all five arms, read again after the review's fixes with the same hashes.

| Arm | Base input_hash | Doors input_hash | Base steps | Doors steps |
|---|---|---|---|---|
| None | 7bd32faf | 7bd32faf | 30114 | 30114 |
| Steered:Storm:Tempest | b5fe3b6c | b5fe3b6c | 58812 | 58812 |
| Steered:Tide:Frost:Forge | dd5bf564 | dd5bf564 | 48394 | 48394 |
| Steered:Shade:Hound:Storm | bdba5258 | bdba5258 | 58084 | 58084 |
| Steered:Storm:Tempest, skill 0.6 | db7d94d6 | db7d94d6 | 34734 | 34733 |

The last row's step is the death tail `S2_package.md` and `S3_discoveries.md` already note: this branch read 34734 and 34733 on two launches, with an equal hash and an equal `bot.csv`. The other direction is `Tools/MetaCheck.ps1` step G, on every gate: over 600 s since D-037 (`S3_tree.md`), over 20 s and two hands before it.

## Decided here
- **The doors are Discovery targets, not Tree rows.** Existing rows need no new text and no invented price, and they land with no Contracts page. `Keeper.Shade` on `ReachL10`, `Keeper.Radiance` on `WeaponL8`, `Keeper.Quicksilver` on `Survive10`, `Reaction.Duos` on `TwoPaths6`, each marked `[invented]` in the catalog's `ref`.
- **`Kills1000` is no door.** It was Shade's until the review of this branch: a person can bank it without level 10, and that pool fails R-OFF-15. See below.
- **`Dashes100` is no door.** It is Quicksilver's row in the fiction, but it fired once in 90 bot runs and the owner dashed 17 times in 9:23, so a Keeper behind it stays dark for most people. `Survive10` went instead; the owner's run stopped 37 s short of it.
- **`Win` is no door.** It holds three targets, the row's limit, and a Keeper behind a win shuts a person who cannot win yet.
- **`Reaction.Duos` waits on `TwoPaths6`,** the one row about two Keepers. That puts it behind two purchases and the Contracts page, like `Favor9`, and it is the one door that does not open today on a person's save. Held in `meta.md` Open.
- **A Keeper's door opens on a first run.** `Crit500` and that Keeper's three nodes wait on it, so the generator fails a Keeper door on a row that waits on a purchase.
- **`Crit500` needed no change.** 285 was the best crit of arms that carried nothing heavy. See below.
- **`Elites10` is stated out, no row changed.** Both the text ("Destroy 10 elites in a single run.") and the source (`ST_Codex` context, "Destroy 10 elites in one run") say per run, and R-MET-40 says all rows are, so a lifetime count would make the text false: the brief's condition fails. Rejected with it: the Warden as a tenth elite (a boss is no `bElite`, R-ELT-01, and R-MET-02 counts it apart), a threshold of 9 (the text says 10), and more spawns (held). The row opens only `Codex.Recollection.Burst`, so one Recollection stays locked. The generator now reads the timeline: a row stated out that a timeline can reach fails until R-MET-27 changes, and it cannot gain a target.
- **A gated run drops the seat rule in both hands, and the lead Patron holds the one seat left.** Found by opening the doors, see below. Tool runs keep R-KEE-23 as it was.
- **`firstrun` and `win` are gate steps,** because they are cheap: 18 to 37 s and 42 s for both cultures. Since D-037 `firstrun` needs all six of its runs read at the boss; before it, one was enough.

## What the doors broke, and the fix
R-KEE-23's seat rule was written for one forced Keeper. Three met Keepers took all three seats of the Patron hand, so at the Shrine a person who chose Tide with one Keeper met, or any S1 Keeper with three, got a hand without it and no run on that Patron. On a Restart the same save read no S1 Keeper at all. The L8 hand had the same shape: no S1 Keeper could be a second Keeper on a full profile, which is meta narrowing a pool.

| Hand | Before | Now, gated | Tool run |
|---|---|---|---|
| Patron, chosen at the Shrine | could lose the choice | the choice leads, then Path order, the last seat drawn past three Keepers (`S3_earned.md`) | no option, as it was |
| Patron, Restart | met Keepers took the seats | `LastPatron` leads, then Path order, the last seat drawn past three Keepers (`S3_earned.md`) | as it was |
| L8 | every met Keeper seated | R-OFF-08's plain draw | as it was |

Three seats cannot hold six Keepers, so someone waits for the Shrine, and the call is who. No met Keeper holds a seat of its own: that is the widening. The first pass of this branch kept the met Keepers in the Patron hand behind the lead, and the review of it read `Shade Radiance Quicksilver` on a real Restart. With `LastPatron` on a met Keeper one S1 Keeper still dropped, Tide, to the seat the person's own last run earned. `S3_earned.md` settles that hand with a drawn last seat, so a met Keeper can now take the seat Tide held: a recorded hand reads `Storm Forge Radiance`, and a Shrine pick on Radiance read `Radiance Storm Tide`, past Forge. R-MET-28 said here that a met Keeper "takes no seat from another Keeper"; that wording went with the draw, and R-MET-28 now reads "holds no seat of its own". The hands read below predate the draw.

On eight saves past the door, with nobody named, all eight L8 hands read whole and mixed (`Quicksilver Shade Forge`, `Forge Quicksilver Radiance`, `Quicksilver Tide Shade`, `Forge Shade Quicksilver`), and all eight launches started on the bot's Storm. Four of those saves were last played on Shade and read `Shade Storm Forge`, the others `Storm Forge Tide`. The save the review read, a first win with all three doors and `LastPatron` on Shade, read `Shade Storm Forge` on a Restart after the fix, and the bot took Storm. `Tools/MetaCheck.ps1` step I proves the lead on real launches, on a Keeper Path order could not reach.

A bot in a hub world crashed on the empty run; `UDTBotSubsystem::Tick` now does nothing with no run. No person's launch reaches it.

## Keeper and duo doors, played
| Door | What was read | Result |
|---|---|---|
| `ReachL10`, `WeaponL8`, `Survive10` | a bot's first win on an empty gated save, `Tools/WinCheck.ps1`, en and zh-Hans | all three in the file; the Shrine opens Shade, Radiance and Quicksilver |
| `Survive10`, `ReachL10`, `WeaponL8` rates | the fresh battery of `S3_tree.md`, no pool change since | 86, 90 and 86 of 90 |
| `TwoPaths6` | two `Blessing4` purchases on a real save, then one played run, seeds 9100 to 9107 | 8 of 8 earned it, and `Reaction.Duos` is in the file |
| Duo cards | one more launch on each of those eight saves | a `Blessing.Duo.*` card reached a hand and was taken in 8 of 8, on the pair each run held |
| Keys of a met Keeper, Amps shut | `Tools/MetaCheck.ps1` step I, every gate. Until D-037 its 20 s dealt two hands, where no pool deals an Amp, so only the Keys were shown. Now 600 s on a copy of the door's save: 3 Radiance Keys taken, 21 hands, 17 after the first Key, no Amp, Trait or Path row of Radiance; the tool run that chose Radiance was dealt 6 of those cards and its Path row | pass |

With no purchase, `Best.Tier` stayed 1, as `S3_tree.md` read, so nothing in a first run reaches a duo.

## Crit500 is earnable
Radiance's Keys alone, gated pool, three archetypes, seeds 9100 to 9102, `-Carry` the weapon. `Weapon.GravityMine` comes from `Win`.

| Weapon | Runs at or over 500 | Best.Crit, high to low |
|---|---|---|
| Gravity Mine | 6 of 9 | 1232, 1072, 1072, 938, 938, 656, 402, 312, 67 |
| Rail Lance | 0 of 9 | 393, 342 eight times |

On the full pool the Rail Lance reaches 684 and the Gravity Mine 1232, and the Flak Mortar stays under at 276.

On real saves, nothing planted: the file a first win leaves, with `Keeper.Radiance` and the mine, three launches on Prism carrying the mine. All three read `new [Crit500 ...]`, at 1072, 1072 and 857. A person has to draw and keep the mine: a carrying bot is the bound.

## A win on one launch
`Tools/WinCheck.ps1`, gate step `win`, seed 9100, both cultures, an empty save, 42 s. One process plays the win on the first-run pool, then reads what a person's screens read. The toast has now been seen, en and zh-Hans, six names on one line under the panel at 1280x720: `Recollection recovered: Cold Storage · Full Count · Deep Work · Long Shift · Run It Hot · Breach Cleared`. Shots: `Saved\WinCheck\<culture>\runover.png`, `codex.png` (not committed).

| Step | Read |
|---|---|
| Toast | the award the screen draws; `Win` in the bank; the name in the line |
| Unlocked | a second session opens the file: every earned key and target, 16 ids |
| Way back | the first row, past the guard, reloads into the hub |
| Shrine | all six open, each by its rule |
| Bestiary | `Swarm Tank Rusher Turret Boss` open on the kills banked; `Bruiser Support` shut |
| Recollections | all 15 rows read as earned or locked; the book's count reads 18 of 30 |

`Kill.Bruiser` and `Kill.Support` stay stated out in R-MET-28, and `Tools/tools_test.py` fails a bestiary row that names neither a role nor a stated-out cell.

## first_run_locked.csv is in the gate
`firstrun`: three S1 Keepers, seeds 9100 and 9101, `-FreshPool -Check T-OFF-15`, 37 s under load and 18 s idle. With `-ReadAll` (D-037) it reads 6/6 at the boss, mean 29.5 decisions, in 21 s.

| List | T-OFF-15 | Reserve share, per run |
|---|---|---|
| the four modifiers (kept) | PASS, 6 of 6 | 0.007 to 0.031 |
| plus Arc Blade and Ring Emitter, thrown away | FAIL, 0 of 6 | 0.279 to 0.346 |

The second row is the list D-024 turned down, built to see the step fail, then restored. It fails in 18 s.

## A met Keeper on the bare first pool is thin
Nine S2 arms with `-FreshPool` read T-OFF-15 FAIL, 16 of 18: `Rebound` 0.152 and `Economy` 0.129 on one seed each, against `C.OFFER.RESERVE_SHARE_MAX` 0.1. With the Flak Mortar, which `ReachL10` opens, the same arms read PASS, 32 of 32, worst 0.023, decisions 28 to 31. The `firstrun` step plays S1 Keepers and cannot read this, so the order of the doors has to hold it, and the generator holds the order. These arms were read on the first pass, when a met Keeper held a Patron seat, so every run played its own Keeper; a hand with no seat for it does not do that, and `S3_earned.md` has what the same arms read now.

| Door | Why that pool cannot be on a save |
|---|---|
| `Keeper.Shade` on `ReachL10` | the row writes the Keeper and the Mortar in one bank |
| `Keeper.Quicksilver` on `Survive10` | ten minutes: `Survive10` without `ReachL10` on 0 of 116 saved runs, read from each run's own log, and the lowest level at 600 s is 14 on the 105 runs that got there with a telemetry file of their own |
| `Keeper.Radiance` on `WeaponL8` | it can hold that pool, and reads under the cap there: worst 0.036, 6 of 6. `WeaponL8` without `ReachL10` on 0 of 116 |

`Keeper.Shade` began on `Kills1000`, on the claim that the row comes after level 10. It does not. Level 10 takes 1012 XP and a kill pays about one, so the two rows land together. The no-pick arm banks `Kills1000` alone, and always did: seed 9107, level 9, 1005 kills, dead at 501.9 s, `met 1 of 15 [Kills1000]`. On arms that pick, the 1000th kill came first on 23 of 114 saved runs, by at most 5.2 s. The owner's run went the other way by 37 XP: level 10 at 329.2 s on 938 kills, the 1000th kill at 339.1 s on 1049 XP, at a pickup of 0.89 gems, and `xp_curve.csv`'s reference reaches level 10 at 388 s. A person who dies in that gap would have banked the Keeper without the Mortar. `ReachL10` is the row that fits: `Survive10` and `Win` already hold three targets, and the two rows sit at one depth, ten seconds apart on the owner's run, so no number moved.

The generator fails a door of Shade or Quicksilver on any other row, and `ReachL10` losing the Mortar. A door of Radiance moves freely. All of these are bounds: a bot's Reserve share, and one owner run.

## For a native reader, held, nothing changed
| Key | Layer | en | zh | Question |
|---|---|---|---|---|
| `Keeper.Tide.Tier3` | 2 | I kept the part of us that stops. You kept the rest. | 能停下的那部分在我这。其余在你那。 | may leak Layer 3: "us" with "you" comes close to saying the Six and the Reclaimer were one (`S3_narrative.md`) |
| `Keeper.Forge.Tier2` | 1 | You were not always this well made. | 你以前可没造得这么好。 | the same question, milder |

The doors reach neither row early: Tier 3 still needs a purchase, and every S2 Keeper line sits at Layer 2 or under, `C.NARR.PREWIN_LAYER_MAX`. No door raises a number (D-023): `Dusktracer.Meta.KeeperDoors` holds a met Keeper to its Keys.

## Not verified
- A person's launch was not run: every launch here is off-screen or headless, and the bot stands in for the player. `-DTPoolGate` takes the same branch as a person's run.
- The toast at the largest text size: read at the default only.
- `Elite3Mods` and `Dashes100` as doors of nothing: unchanged, and still bot-thin.
- Whether a Keeper arriving this early is welcome. A bot's first win opens all three at once, and the owner's run reached level 10 but took no weapon upgrade, on a full pool that is not a person's first.
- A person banking `Kills1000` alone: the no-pick bot shows the case, and the generator and `Dusktracer.Meta.KeeperDoors` close it. No person has played it.
- Which Keepers a person wants on a Restart: the hand is a rule, and the Shrine stays the way to a met Keeper.
- `Reaction.Duos` and the three new branches on a person's save: `DTHubTabs::SetTreeFactory` had no factory on this branch, so the purchases ran through `DT.Tree.Buy`. The Contracts page arrived at integration (`S3_vigil.md`), and those rows were not bought on it here. `S3_earned.md` buys them there.
- S2 Keeper balance on a person's pool: the numbers above are budget and reach, not power. T-KEE-15 and T-KEE-18 are untouched and still FAIL on them.
