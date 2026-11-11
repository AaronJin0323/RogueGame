# S3: D-064, a fresh save opens one archetype for each S1 Keeper, and D-069, the duos' door earned by play

Branch `s3/d064-fresh-pool`, cut from `s3/kit-questions` at `d1dfec0` (then `dev/s3`), with `dev/s3` merged in at `892c617`, `00332f5` and `edcb6e5`. Class RULE, layer the first-run pool (`meta.md` R-MET-25, R-MET-22) and the Tree catalog. It replaces R-MET-25's row "Amp1, Amp2, Trait of every archetype: out" and R-MET-22's node lists. Nothing is tuned: no price, weight or card number moved.

Build:
- Base reference: `d1dfec0`, Development editor build, the first battery on 2026-11-12.
- Branch batteries of D-064 and the `firstrun` step: `47540b9` (the branch with `dev/s3` at `892c617` merged in).
- The first whole gate, which failed at `earned`: `7de3e01` (`dev/s3` at `00332f5` merged in).
- D-069's readings (the three item 4 batteries, PlantSave) and EarnedCheck alone: the build of `f6866d1`; `1c4f52b` merged `dev/s3` at `edcb6e5`, Tools and documents only, and `ba20cb3` changed comments and one spec sentence, so that build stands for them.
- The whole gate after D-069: `ba20cb3`. The base's second seed range: `d1dfec0` rebuilt, then the worktree switched back to the branch and rebuilt.

## The data change
- `Docs/09_Catalog/tree.csv`: the `unlocks` cell of `Branch.Storm.Blessing4`, `Branch.Forge.Blessing4` and `Branch.Tide.Blessing4` drops the Amp1, Amp2 and Trait of Arc Grid, Hammer and Frost, and keeps those of Tempest, Furnace and Whirlpool. The same three rows take the text key `Tree.Branch.Advance4One`. `Blessing5` and every price and `requires` cell stay.
- A card is out on a fresh save when a node names it or `first_run_locked.csv` holds it (`DTOffers::IsTreeLocked`). No node names the nine cards now, so a fresh pool holds them. No C++ changed; `DTCatalogGen.h` is regenerated.
- `Tools/gen_catalog.py` failed on an Amp, Trait or Path row no node unlocks. `FIRST_RUN_ARCHETYPES` names the three, and `tree_cover_faults` fails both ways: a gated card with no node, and a node naming one of the nine (`tools_test.py` `test_the_first_run_archetypes_are_open_and_the_rest_have_a_node`).
- `ST_Tree`: `Tree.Branch.Advance4` is shared by all six Paths and still true of the S2 Keepers' first node, so the S1 nodes have their own key.

## Invented zh, for a native reader
| Key | en | zh-Hans |
|---|---|---|
| `Tree.Branch.Advance4One.Name` | Further Advance | 追加预支 (the same as `Tree.Branch.Advance4.Name`) |
| `Tree.Branch.Advance4One.Desc` | {0} offers the later advances of one more archetype. | {0}再开放一个流派的后续预支。 |

Both are marked `[invented en and zh]` in Context, as the table marks the other Advance rows.

## The two batteries, side by side
`& Tools\BotRuns.ps1 -BotVersion 2 -FirstSeed 9100 -Seeds 10 -FreshPool -Check T-OFF-15 -Parallel 6`, the nine S1 archetypes as Steered arms with no second Keeper named, as `S3_tree.md` ran it, then the same on `-FirstSeed 9110`. Base on `d1dfec0`, branch on `47540b9`, one worktree rebuilt between them.

The Keeper share is `Tools/bot_summary.py`'s `keeper_share`: the whole run's attribution, the "Keeper share" column of the summary's first table, not `keeper_share_l16`. It is the instrument `S3_tree.md`'s fresh-pool table used: on the base it reads Storm 0.467 (the note: 0.467), Forge 0.528 (0.540) and Tide 0.393 (0.402). The note did not name it. The L16 share is given beside it.

| Seeds 9100 to 9109, 30 runs a Keeper | Base reach / wins | Branch reach / wins | Base share | Branch share | Base share at L16 | Branch share at L16 |
|---|---|---|---|---|---|---|
| Storm | 29 / 28 | 30 / 29 | 0.467 | 0.556 | 0.421 | 0.478 |
| Forge | 29 / 29 | 30 / 28 | 0.528 | 0.631 | 0.498 | 0.597 |
| Tide | 26 / 24 | 28 / 27 | 0.393 | 0.513 | 0.365 | 0.434 |
| All 90 | 84 / 81 | 88 / 84 | 0.463 | 0.567 | 0.429 | 0.504 |

| Seeds 9110 to 9119 | Base reach / wins | Branch reach / wins | Base share | Branch share |
|---|---|---|---|---|
| Storm | 30 / 28 | 29 / 29 | 0.457 | 0.536 |
| Forge | 29 / 29 | 30 / 30 | 0.519 | 0.629 |
| Tide | 28 / 26 | 28 / 26 | 0.367 | 0.480 |
| All 90 | 87 / 83 | 87 / 85 | 0.448 | 0.548 |

The three opened archetypes' own arms (`keeper_share`):

| Arm | Base 9100+ | Branch 9100+ | Base 9110+ | Branch 9110+ |
|---|---|---|---|---|
| Storm / Arc Grid | 0.467 | 0.581 | 0.464 | 0.569 |
| Forge / Hammer | 0.521 | 0.681 | 0.539 | 0.648 |
| Tide / Frost | 0.396 | 0.552 | 0.373 | 0.558 |

Picks by kind, seeds 9100 to 9109, 90 runs (the base on seeds 9110 to 9119: Blessing 529, WeaponUpgrade 1240, Mod 447, Weapon 204, Reforging 77, Reserve 9): base Blessing 530, WeaponUpgrade 1222, Mod 474, Weapon 199, Reforging 73, Reserve 19; branch Blessing 1051, WeaponUpgrade 941, Mod 260, Weapon 187, Reforging 27, Reserve 25. The branch's Amp and Trait picks of the opened archetypes: Arc Grid 178, Hammer 177, Frost 178; the base has none. Seeds 9110 to 9119 on the branch: Blessing 1042, WeaponUpgrade 940, Mod 250, Weapon 187, Reforging 39, Reserve 16.

Reach and wins are printed and judged nowhere (D-050 item 5). As a bot bound only: the wins did not fall with the Amps open (81 to 84 of 90 on the first range), the opposite of what `S3_tree.md`'s full-against-fresh comparison suggested (47 against 69 wins), where more open cards went with fewer wins. The base's own figures have moved since that note (79 reach and 69 wins then, 84 and 81 now), because the tree under it has.

## T-OFF-15 on both ranges
| | Base | Branch |
|---|---|---|
| Seeds 9100 to 9109 | `PASS (84/84 runs at the boss ...; mean decisions 29.321)` | `FAIL (87/88 runs at the boss ...; mean decisions 28.955)` |
| Seeds 9110 to 9119 | `PASS (87/87 runs at the boss ...; mean decisions 29.414)` | `PASS (87/87 runs at the boss ...; mean decisions 29.023)` |
| Runs at the boss, of 180 | 171 | 175 |
| Runs inside the band, of those | 171 | 174 |
| Runs outside the band | 0 | 1 |

Every run outside the band, or short of the boss:
- Branch, 9102 `Steered:Tide:Stillness`: at the boss, 22 decisions, level 20, died at 907 s. **The only run outside the band.**
- Base, short of the boss (deaths): Forge/Furnace 9105 at 873 s, Storm/Downpour 9107 at 759 s, Tide/Frost 9109 at 898 s, Tide/Whirlpool 9105 at 556 s, 9107 at 779 s and 9109 at 724 s.
- Branch, short of the boss (deaths): Tide/Stillness 9107 at 780 s, Tide/Whirlpool 9102 at 455 s; on the second range Storm/Tempest 9117 at 789 s (L20), Tide/Stillness 9112 at 269 s (L6), Tide/Whirlpool 9116 at 420 s (L9).
- Base, second range, short of the boss (deaths): Forge/Slam 9111 at 563 s (L16), Tide/Stillness 9112 at 269 s (L6, the same run as the branch's: it dies before any D-064 card can differ) and 9115 at 483 s (L12).

Each side has 180 runs: the base 0 runs outside the band, the branch 1 (Tide/Stillness 9102). That is all the evidence there is on the rate: one miss in 175 runs at the boss against none in 171, on two seed ranges of one bot.

## The one miss, traced
Read from the two runs' files, with no new run. `Steered:Tide:Stillness`, seed 9102.
1. A decision is counted on each pick, the Patron hand left out (`Telemetry/DTTelemetryRecords.cpp`, `run_end.per_player.decisions`, which `bot_summary.py` reads). The branch run's 22 are 17 Level hands, 3 Chest, the Keeper hand and the Reforging hand; every hand opened was picked, and none was open or queued at the end: the last level-up, L20 at 857 s, was picked, and the run died at the boss at 907 s. The base run of the same arm and seed made 29: 23 level-ups and 6 chests, L24, a win at 953 s.
2. The two runs are hand for hand the same through L6. At L7 the branch hand holds `Blessing.Tide.Frost.Amp2`, a D-064 card, and the bot takes it where the base, whose hand has no Amp, takes the Pulse Rifle's upgrade. From L7 to L15 the branch takes a blessing at every Level hand (Frost Amp1, Amp2, Trait; Hammer's Key, Amps and Trait; Forge's other Keys) and only one weapon upgrade, from a chest at 368 s. Its kill rate falls to 147 a minute against about 205 for the field, its pickup to 0.60 of the XP spawned against about 0.9, and it is at level 11 at 630 s where every other run at the boss is at 16 or more. It levels less, so it meets fewer hands.
3. Once the pool ran dry: at the L16 Reforging hand it owned every open blessing of both its Keepers, six of Tide and six of Forge, and no evolution was eligible, so the hand was three Reserve cards. Those are the only Reserve cards offered in the run (3 offered, 1 taken, `Reserve.Restore`; Reserve share 0.029). No Level or Chest hand ran short of legal cards.
4. The same run met `TwoPaths6` on the fresh pool: `Meta: discoveries met 6 of 15 [... TwoPaths6 ...]`, `Best.PathsAtTier2=2`. It is a tool run with no file, so nothing was banked.
5. The check's known tail: `10_Tuning.md` §1 says it "fails by one run in about two hundred" with no bug found. The misses written up before are on the same arm family and short of XP the same way: Tide/Stillness:Storm seed 6 at 24 decisions, L19 (`S1_boss_after_fields.md`), and Tide/Stillness:Storm seed 7 at 24 (`S2_reforging.md`). This one has that shape, and its count, 22, is the lowest yet.

An effect of this change on this seed, of the same shape as the check's known Tide/Stillness tail: the opened Amps draw the Tide/Stillness bot into blessings ahead of weapon upgrades from L7, and the run falls short of XP; how often the change makes that happen is not decided by these files.

For a person, not a bot: the bot that fell behind took every open blessing of its Path ahead of weapon upgrades from L7 to L15; a person picks by what they want, and the kit's question 2 after the first run reads whether the opened archetype helps them name a build.

## The hypothesis
"We believe opening one archetype's three cards per S1 Keeper on a fresh save moves the cards of one archetype a first run can hold from one to four, and a fresh pool's Keeper share on those three arms from 0.40 to 0.54 toward the pillar ..."
- Cards of one archetype a first run can hold: from one (its Key) to four (Key, Amp1, Amp2, Trait) for Arc Grid, Hammer and Frost, by the data (`Meta.FirstRunPool`).
- The share on those three arms, `keeper_share`: base 0.461 and branch 0.605 on the first range (the mean of the three arms), base 0.459 and branch 0.592 on the second. The record's 0.40 was the base before; it reads 0.46 here, and the branch passes 0.54 on both ranges. The pillar, `C.POWER.KEEPER_SHARE_L16`, is read at L16; that column is in the first table.
- `-Check T-OFF-15`, which D-024 asks of any change to the first-run list: one run outside the band in 180 on the branch, against 0 on the base (above).
- The people's half is the S3 session's: kit question 2 after the first run and question 5.

## The `firstrun` gate step, alone
Run first, as `Tools/Gate.ps1` runs it, on `47540b9`: `-BotVersion 2 -FreshPool -Arms "Steered:Storm:Tempest,Steered:Forge:Hammer,Steered:Tide:Frost" -FirstSeed 9100 -Seeds 2 -Check T-OFF-15 -ReadAll -Parallel 6 -Stagger 1.5`: `BOTRUNS: PASS (T-OFF-15: PASS (6/6 runs at the boss with decisions 28+-3 and Reserve share <= 0.1; mean decisions 28.167))`. Two of its three arms, Forge:Hammer and Tide:Frost, are now first-run archetypes, so the step reads a fresh pool that holds its own arms' Amps.

| Run | Base (the base battery's same run) | Branch |
|---|---|---|
| Forge:Hammer 9100 | victory 981 s, 29 decisions | victory 951 s, 28 |
| Forge:Hammer 9101 | victory 952 s, 29 | victory 945 s, 29 |
| Storm:Tempest 9100 | victory 939 s, 29 | victory 950 s, 28 |
| Storm:Tempest 9101 | victory 961 s, 29 | victory 1026 s, 29 |
| Tide:Frost 9100 | victory 967 s, 29 | victory 981 s, 29 |
| Tide:Frost 9101 | victory 955 s, 30 | victory 962 s, 26 |

## `TwoPaths6` and D-069: the duos' door is earned by play
`TwoPaths6` (`Best.PathsAtTier2` at 2) opens `Mod.SpreadWave` (`starter=no`, an S2 row) and `Reaction.Duos`, the door of the duos (R-MET-28), with `Codex.Recollection.SpreadWave`. Tier 2 is `C.TIER.FAVOR_T2` cards of a Keeper, and a fresh save now holds exactly that many of each S1 Keeper, so a first run earns it by taking every Key and all three cards of the open archetype of both its Keepers: twelve blessing picks, the second Keeper's after the L8 commitment. For the bot that is the common case, not a rare one: `TwoPaths6` met in 0 of 90 fresh runs on the base and in 67 and 66 of 90 on the branch's two seed ranges (133 of 180; Storm/Tempest 19 of 20). The traced Tide/Stillness run above is one of them.

The first whole gate on the branch found it: `GATE: FAIL at earned (7de3e01; mask FFFFF3FC)`, every other step passing (547 tests, `restart` 276faf8e and `meta` ba2a3ac8 unmoved, `firstrun` six of six). EarnedCheck's `Doors` asserted "the duos are not earned" after run one, and its earning run had banked `TwoPaths6`; and the card it read from `Branch.Forge.Blessing4` was Hammer's, which a fresh save now holds (fixed in `bd7a3c7`: Furnace's). I had written that no first-run check could be tripped by it; EarnedCheck could. That finding is D-069: option A, the door is earned by play, and no cell or number moves.

### What D-069 changed here
- `meta.md`: R-MET-27's `TwoPaths6` row (a door a person's first runs open, with the bot's 133 of 180 as a bound and this note as its source), R-MET-28's sentence (earned by play, no purchase before it; `Blessing4` no longer leads to the duos), R-MET-30 and T-MET-30 (the duo leg below); the Open row "`Reaction.Duos` waits on `TwoPaths6`, which waits on two purchases" is closed and removed.
- `S3_kit.md`: the run at which each validator's duos opened, and "What did that card do?" after the first duo card they took.

### D-069 item 4: T-OFF-15 on the pool a person holds entering their second run
The route is a real save per run, made by the door's own writer: `Tools/PlantSave.ps1` plays one gated run with `DT.Meta.Plant` on the counters and lets its bank's Discoveries write their targets (nothing writes `Unlocked` by hand), then gives each Keeper a copy with that Keeper as its last Patron (`DT.Meta.LastPatron`, on a launch that plays no step and banks nothing). `Tools/BotRuns.ps1 -SaveFrom` gives every run its own copy through `-DTMetaFile`, gated. A met Keeper's run takes its seat by the rule as it stands, `keepers.md` R-KEE-23: "One seat stays, the first of the Patron hand, for the Patron chosen at the hub or, on a Restart, the save's `LastPatron`." No map option is passed, so `vigil.md` R-VIG-02 holds as written. On a person's save the duo reactions start only when a duo card is taken (D-035); the tool route `-DTUnlock` starts them at run start, which is another game, so it is not used.

- Pool 1, the first-run pool with the duos and Spread Wave: a 20 s run with `Best.PathsAtTier2 2` banked `new [TwoPaths6]`; the save holds `Discovery.TwoPaths6 Mod.SpreadWave Reaction.Duos`.
- Pool 2, that and what a first run's three Keeper doors write: a 610 s run (Survive10 reads the clock) with `Best.PathsAtTier2 2`, `Best.WeaponLevel 8` and `Best.Level 10` banked `new [Freeze100 Kills1000 ReachL10 Survive10 TwoPaths6 WeaponL8]`; each save holds 15 ids: the six `Discovery.*`, `Keeper.Shade`, `Keeper.Radiance`, `Keeper.Quicksilver`, `Mod.SpreadWave`, `Reaction.Duos`, `Weapon.FlakMortar`, `Weapon.SeekerSwarm`, and the starters `Weapon.ArcBlade` and `Weapon.RingEmitter`, already open. `Freeze100` and `Kills1000` came with that run's own play and open nothing shut. One copy per Keeper, each with that Keeper as its last Patron.

| Battery | Verdict | Pool line, every run | Duo card offered | Spread Wave offered | Flak Mortar / Seeker Swarm offered |
|---|---|---|---|---|---|
| Pool 1, nine S1 arms, seeds 9100 to 9109 | `T-OFF-15: PASS (85/85 runs at the boss with decisions 28+-3 and Reserve share <= 0.1; mean decisions 29.012)` | `Offers: pool gated, 3 unlocked`, 90 of 90 | 88 runs, taken in 88; the reactions started in those 88 | 90 | not in this pool |
| Pool 2, nine S1 arms, seeds 9100 to 9109 | `T-OFF-15: PASS (84/84 runs at the boss with decisions 28+-3 and Reserve share <= 0.1; mean decisions 28.929)` | `Offers: pool gated, 15 unlocked`, 90 of 90 | 89 | 90 | 89 / 89 |
| Pool 2, nine S2 arms (`-ArmSet S2Keepers`), seeds 9100 to 9103 | `T-OFF-15: PASS (36/36 runs at the boss with decisions 28+-3 and Reserve share <= 0.1; mean decisions 28.917)` | `Offers: pool gated, 15 unlocked`, 36 of 36 | 36 | 36 | 35 / 36 |

On Pool 2 every run's Patron hand held its arm's Keeper in the first seat, and the bot took it: 90 of 90 and 36 of 36. The last seat of that hand is a draw, as the rule says. `S3_doors.md` does not print the seed count of its S2 batteries; its 32 runs at the boss over nine arms need at least four seeds, so four.

### EarnedCheck, the duo leg from both sides
`TwoPaths6` is earned when and only when a run held Tier 2 on two Paths, read from that run's own `Meta: counters` line (`Best.PathsAtTier2`, the run's tally at its bank, `Tools/duo_read.py`), never from the save or the Discovery list.
- "When": the earning run, seed 9100 on an empty save, read `Best.PathsAtTier2=2`; its own hands held no duo card; `Doors` read the save it left: `the three Keepers are met, and the run that held Tier 2 on two Paths left the duos, TwoPaths6 and Spread Wave: ok`; the next gated run on that save, seed 9230, was offered a duo card.
- "Only when": `quiet`, seed 9100 on another empty save, Storm/Arc Grid taking blessings only, at the first of a stated list of lengths that reads exactly one Path: 400 s and 340 s read 0, 460 s read `Best.PathsAtTier2=1 Best.Tier=2`. Its picks: five Storm cards (the Arc Grid Key, Amps and Trait, the Downpour Key; the Tempest Key was never offered), then after the L8 hand at 231 s six Tide cards by 446 s, so its one Path is its second Keeper. `DuosShut` read: `a run with one Path at Tier 2 left neither the duos, TwoPaths6 nor Spread Wave: ok`.
- The purchase leg is as it was; the two `Blessing4` purchases that stood before the duos are gone, and `Doors` buys one node of each new branch.

`EARNED: PASS (seed 9100 banked 1493, [Freeze100 Kills1000 ReachL10 Survive10 TwoPaths6 WeaponL8 Win], key spent it to 1193, new process read it, next run's hands hold the bought cards; keys bought three nodes on a planted balance; Shrine chain=True, branch cards=True, L8 holds a met Keeper=True; duos: earned with 2 Paths at Tier 2 (seed 9100), not earned with 1 (seed 9100), duo card on the next run (seed 9230); 162 s)`

### Notes that were read before D-069
Not edited here; read them with this:
- `S3_doors.md` lines 27, 54 and 58: "the one door that does not open today on a person's save", two `Blessing4` purchases before `TwoPaths6`, and "nothing in a first run reaches a duo" no longer hold since D-064 and D-069.
- `S3_earned.md` lines 65 to 76: the duo proof on two bought nodes.
- `S3_tree.md` lines 54 and 105, and `S3_vigil.md` line 101: `TwoPaths6` out of a first run's reach, or reached through the page.

### The people's half
D-069's hypothesis is read at the S3 session: the run at which each validator's duos opened, and whether they can say what the first duo card they took did (the two kit lines above). None of it is read here.

## The gate
`GATE: PASS (ba20cb3; 3 not run; mask FFFFF3FC)` (`& Tools\Gate.ps1 -Base dev/s3`, `dev/s3` at `edcb6e5` merged in). `tests: 547 run of 547 found, 0 failed, floor 547`, so no test function was added and `C.GATE.TEST_FLOOR` stays; `tools: TOOLS: PASS (352 tests)`. `restart: RESTART: PASS (seed 9107, input_hash 276faf8e / 276faf8e ...)` and `meta: META: PASS (input_hash ba2a3ac8 / ba2a3ac8 ... first-run pool=True ...)`: neither hash moved, so no run of the tool pool moved. `firstrun` 6/6, `vigil`, `win` and `earned` PASS (the `earned` line is the one quoted above, 212 s). `pad`, `scope` and `mac` not run: no package, no brief in flight, D-007. The first whole gate on the branch, `GATE: FAIL at earned (7de3e01; mask FFFFF3FC)`, is the finding that led to D-069.

## What goes stale
- `S3_tree.md`'s fresh-pool table ("A bot on a fresh-save pool") and its 69-to-47 reading, which `S3_kit.md` now marks as read before D-064.
- `Tools/MetaCheck.ps1`: step G's `DT.Tree.Buy ToolPool` now unlocks 25 ids (it was 34), and step H's fresh hands may show the three archetypes' cards; both are changed on this branch. Its pool windows (`-PoolSeconds`, `-PoolHands`, `-KeyHands`) are unchanged and read on the gate above.
- Any note that lists the first-run pool as Keys only: `S3_tree.md`, `S3_doors.md`'s first-run readings.
