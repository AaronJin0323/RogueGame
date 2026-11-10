# Meta — the save, the currency, the run-end autosave and the Tree (S3 row)

## Intent
Every run feeds the next: what a run earns outlives it, in one versioned save that later systems (Tree, Discoveries, Hub, story) write into without a new file. Pillar: meta widens the pool, it never raises a number. A tool run never touches a player's save and always sees the full pool; only a person's run reads what the save unlocked.

Written from `Handoff_Dusktracer/game/G02_Blueprint_DocSet_zh.md` (the meta row and the day-0 list: currency formula, tree of a trunk plus six branches, save with a version from the first write, a validator and a migration table), `reference/original_docs/01_GameDesign.md §8`, `02_TechnicalDesign_s1-16.md §10`, `reference/specs/full_game_specs.md §2.1` and `§5.1`, and `G01_Name.md` (slot `DTMeta.sav`; the currency's name is unchanged). `[invented]` marks what the source does not give; it is this spec's choice and the first thing to revisit.

## Rules
### Names
- **R-MET-01** Fiction is never a key (`AGENTS.md`). Code, save fields and telemetry say `Currency`, `Tree`, `Hub`, `Discovery`; the Path is the key of a Keeper. The fiction names (Ichor, Contracts, Vigil) live only in string tables and in this spec's prose about them.

### Currency
- **R-MET-02** A run's award is the source's formula, floored to a whole number: `C.META.CURRENCY_PER_MINUTE × minutes survived + C.META.CURRENCY_PER_KILL × kills + C.META.CURRENCY_PER_ELITE × elites + C.META.CURRENCY_BOSS_KILL × boss kill`, all times `1 + C.META.CURRENCY_HEAT_BONUS × Heat`. Minutes are fractional, from sim steps. Kills count every kill, elites and the boss included; elites and the boss then add their own term. Heat is `0` until Heat is built. The award is what the source calls retained; the reclaimed and share lines are display-only and not built.
- **R-MET-03** The tally is sim-clocked and read-only: `UDTRunRecorder` runs at the end of each step, counts `FDTWorldView::Kills` and steps, writes nothing any system reads, and stops at the end of the step the run ended on, so the frame rate never moves an award. Sim time is the minutes, so time in a hand or a menu earns nothing.
- **R-MET-04** A run is banked exactly once, however it ends: death, victory, Abandon run, or Restart run and Quit to desktop from a run still going. The source keeps the award on death; the last two keep it too, so no exit forfeits it `[invented]`. The bank hangs off the telemetry session's close, which is already once per run (`settings.md` R-SET-22, R-SET-23, R-SET-25). A run of zero steps (a restart from the first Patron hand) banks nothing and is no run.
- **R-MET-05** Banking adds the award to the balance and to `Runs`. Victory adds to `Wins` and clears `DeathsSinceWin`; a death or an abandoned run adds to `DeathsSinceWin` (GDD §8.6 reads it); a restart or a quit moves neither `[invented]`. The Patron is kept as `LastPatron`, and the run's counts are added to `LifetimeStats`.
- **R-MET-06** The run-over screen shows one line, `Summary.Line`: the award and the balance after it. It is drawn only once the bank has run. A bank the save's lock kept in memory (R-MET-11) also shows `Summary.Waiting` under the panel, where the Discovery toast goes and in its place for that run: the toast's slot shrinks a long line to the panel's width, and whether the run is saved matters more than what it found. No other UI exists here: the source's summary stays out of scope (`settings.md` R-SET-21).

### The file
- **R-MET-07** One file per profile, `DTMeta.sav` under `Saved/SaveGames`, wrapped in the `FDTSaveEnvelope` of R-TEC-04 at `C.SAVE.VERSION`. Settings stay in their own file (`settings.md` R-SET-03); the source's settings fields in the save are dropped, because a save must never carry what R-SET-05 keeps from tool runs.
- **R-MET-08** The payload is canonical: little-endian, fields in the order of §Data, map and set keys sorted, UTF-8 strings with a length. The same profile always gives the same bytes. A reader that runs off the end, finds bytes left over, or meets an empty key treats the file as bad.
- **R-MET-09** The validator runs on every load, after the checksum and any migration: no negative count, `Wins` and `DeathsSinceWin` within `Runs`, `Act` from `C.META.ACT_START` to `C.META.ACT_MAX`, fates in range. A file that fails is bad, even with a good checksum.
- **R-MET-10** Migrations are the envelope's upgrade-only chain, one step per version. Any new field raises `C.SAVE.VERSION` and adds a step that fills its default (a new fate field defaults to Bound, as the source says). The first version holds every field of §Data, so the first system to fill one needs no bump. Unknown tree, unlock and discovery keys load and stay: a removed node never costs a player their record.
- **R-MET-11** Writes are atomic: the bytes go to a sibling `.tmp`, are flushed to disk, then replace the file in one operation (`MoveFileExW` on Windows, `rename` elsewhere). A failed write leaves the old file whole. A leftover `.tmp` is ignored and overwritten. `IFileManager::Move` is not used here: it deletes the target first. The save changes one change at a time (D-042): a bank or a purchase takes the save's lock, rereads the file if another launch wrote it since this session last read or wrote it, makes its change on that profile, writes, and lets the lock go. The lock is the sibling `<file>.lock` opened with no sharing and held for that one change only. The operating system closes it when its process ends, so a crash leaves no lock and no launch cleans one up; a lock file nobody holds blocks nothing. A launch waits up to `C.SAVE.LOCK_WAIT_MS` for it, asking every `C.SAVE.LOCK_RETRY_MS`. When the wait runs out nothing is lost in silence. A bank is made in memory and kept as a change: the session's next save point (its next bank or purchase, or exit) makes it again on the profile the file then holds, and the log and a `save_deferred` record say so. A purchase is refused as `Busy` with nothing spent, said in the log, and can be asked again. A change a failed write left in memory (R-MET-13) is kept the same way and lands on top of what another launch wrote; a kept purchase that no longer passes R-MET-26 there is dropped, its price never taken. A bank still kept at exit with the lock still held is lost, and the log says so as an error. The player is told of a kept bank on the run-over screen (R-MET-06) and of a refused purchase on the Contracts page (`vigil.md` R-VIG-19); a bank lost at exit is in the log alone, because the launch has ended and nothing beside the save tells the next one. Known and left: two games may share a save, and a window that sat idle draws the balance it last read until its own next change.
- **R-MET-12** A bad file is kept: corrupt, too new, a missing or failed migration, or failing R-MET-09, it is moved to `<file>.bad.<n>`, the lowest free number, so no older copy is overwritten; the session starts empty and logs a warning. If the move fails, or the file cannot be read at all, the session is stuck: it plays on an empty profile and writes nothing, so the file stays as it is.
- **R-MET-13** Autosave runs at the end of the bank, on the game thread, never inside a sim step. A write that fails keeps the change in memory and the next save point tries again (R-MET-11). Tree purchases save the same way.

### Tool runs and the sim
- **R-MET-14** Tool runs have no save: a process that is `-unattended`, under automation, a bot (`-DTBot*`) or given `-DTNoSave` neither reads nor writes the file, and banks nothing into its profile; the award is still computed, for the line and for telemetry. `-DTMetaFile=<path>` names an isolated save and overrides this, as `-DTSettingsFile` does (`settings.md` R-SET-05). `Tools/Play.bat` and the packaged `Play_*.bat` are the person's launches and the ones that save. Every launch a tool makes is `-unattended`, but for two checks that make a person's launches on purpose, each on a `-userdir` of its own, so the profile it saves is never the owner's: `Tools/SeedCheck.ps1` (`07_Technical` R-TEC-16) and `Tools/PackageHubCheck.ps1` (`package.md` R-PKG-13). The seed rule reads a narrower tool run, `-unattended` or automation (`07_Technical` R-TEC-16): a `-DTNoSave` launch with no seed skips the hub and the save yet draws a fresh seed, because a launch that falls back to seed `0` unasked is the bug that rule removes (D-028).
- **R-MET-15** Files under `Source/Dusktracer/Meta/` are read by the listed files only (T-MET-15). The sim's readers are the boss (R-MET-17) and the offer system (R-MET-22). A new reader is added to that list on purpose.
- **R-MET-16** Determinism: in a tool run a seed plays exactly as it does with no save, whatever the profile holds but `Wins`: same steps, same input hash, same CSV. The `run_award` record is the only addition to the JSONL, and it holds no save field; `save_deferred` (R-MET-11) needs a second launch on the save, so a tool run alone on its file never writes it. A person's run is the other case: the offer system reads the Tree there (R-MET-22), so an empty profile plays the narrower first-run pool (R-MET-25). That run's seed still replays for that profile; only `-DTPoolGate` lets a tool ask for it (R-MET-23).
- **R-MET-17** Wins feed the form choice (`boss.md` R-BOS-01) at the start of the fight, read once from the session. No save or no game instance reads `0`.

### What later systems build on
- **R-MET-18** The profile carries the Tree (`TreeRanks`, `Unlocked`), Discoveries (`Discoveries`, `LifetimeStats`), the Hub (`LastPatron`, `DeathsSinceWin`) and the story (`Act`, `StoryFlags`, `RecollectionsRead`, `ArenaWins`, `ArenaHeat`, the fate fields). No system opens a second file.
- **R-MET-19** Run-end systems hook `FDTMetaSession::OnBank`, called with the tally and the profile after the award and before the write, so a Discovery lands in the same atomic save. Per-run counters travel in `FDTRunTally::Counters`, keyed by a neutral tag such as `Kills.Weapon.ScatterCannon`, and are added to `LifetimeStats` by name (R-MET-44 for `Best.` keys). A session with no file calls no hook.

### Pillar: the Tree keeps pool and utility nodes only (D-023)
- **R-MET-20** No Tree node may raise a number in a run: not a `FDTStatSet` value, a Favor count, a cooldown, a level or a starting rank. A node either widens the pool or adds a choice. The source's table conflicts; the call on each row:

| Source node (GDD §8.2) | Class | Call | Tree key |
|---|---|---|---|
| Trunk: +Max HP, 5 ranks | stat | cut | none |
| Trunk: Armor | stat | cut | none |
| Trunk: +XP gain | stat | cut: more levels are more power | none |
| Trunk: +Pickup radius | stat | cut: it is XP rate by another name | none |
| Trunk: Starting weapon L2 | stat | cut: a head start in levels | none |
| Trunk: +Luck | stat | held, not built: `offers.md` names it, but it raises rarity; Open | none |
| Trunk: +1 Reroll | utility | kept | `Trunk.Reroll` |
| Trunk: +1 Banish | utility | kept | `Trunk.Banish` |
| Trunk: +1 Lock | utility | kept | `Trunk.Lock` |
| Branch: start with +1 Favor when Patron | stat | cut: reaches a Path tier early | none |
| Branch: Ultimate cooldown −20% | stat | cut | none |
| Branch: unlock locked blessing #4 | pool | kept | `Branch.<Path>.Blessing4` |
| Branch: unlock locked blessing #5 | pool | kept | `Branch.<Path>.Blessing5` |
| Branch: unlock that Keeper's Fusion | pool | kept | `Branch.<Path>.Reforge` |
| Branch: begin the run with one of this Keeper's blessings | stat | cut: a free card is a head start | none |

- **R-MET-21** The catalog is `09_Catalog/tree.csv`: `Trunk.Reroll`, `Trunk.Banish`, `Trunk.Lock`, `Trunk.ScatterCannon`, `Trunk.RailLance`, `Trunk.Workshop` and, per Path, `Blessing4`, `Blessing5`, `Reforge`, each of one rank, generated as `DTCat::TreeNodes`. The last three Trunk nodes have no source row: they are the door of the nine `starter=no` rows no Discovery names (`Trunk.Workshop` holds seven modifiers at one price) `[invented]`, D-024. They are pool nodes, so R-MET-20 binds them. A branch runs in that order: each node needs the one before. A branch of an S2 Keeper needs `Keeper.<Path>` in `Unlocked` first, so nothing is sold for a Keeper the profile cannot meet `[invented]`. `requires` names a node (one rank) or an id of `Unlocked`; `unlocks` names the ids a purchase writes. The generator fails on a class other than `pool` or `utility`, a grant other than the three tools, a Key, an evolution or a `starter=yes` row in `unlocks`, an id two nodes unlock, and an Amp, Trait or Path row no node unlocks.
- **R-MET-22** Blessing #4 and #5 follow `build_depth_spec` §2.2, the source's own re-pointing of those nodes to a Keeper with three archetypes: `Blessing4` opens the Amp1, Amp2 and Trait of the Keeper's first two `expression.csv` rows, `Blessing5` those of its third. The source's capstones through Discoveries are not carried: a Trait opens with its archetype and still needs `C.KEEPER.TRAIT_PREREQ_CARDS` `[invented]`. `Reforge` opens `Transform.<Path>`. An id any node names, or a row of `first_run_locked.csv` (R-MET-25), is Tree-locked: in a gated run it enters the pool only from `Unlocked`, and a shut Path row yields its slot (`offers.md` R-OFF-09). The offer system reads the save once per world, so a purchase counts from the next run. Every id in `Unlocked` goes down the `DT.Unlock` route, so a Discovery or a node only has to add it, and `Mod.Refit`'s draw sees it. A first-run lock the profile has not opened is shut for the run (`ShutStarter`), so the draw does not reach past the gate either.
- **R-MET-23** Only a person's launch is gated. A tool run (R-MET-14) reads the full pool of today: every Tree-locked id in, weapons and modifiers by the `starter` column plus `-DTUnlock`, tool counts at `C.OFFER.*`, whether or not it names a `-DTMetaFile`, so every bot battery and hash stays valid. `-DTPoolGate` gates a tool run on purpose, with the save it names or an empty profile: `Tools/BotRuns.ps1 -FreshPool` reads a first run that way, and `Tools/MetaCheck.ps1` a profile bought back up to a tool run's pool.
- **R-MET-24** Trunk nodes add `C.TREE.TOOL_PER_RANK` to the run's starting count of that tool (`offers.md` R-OFF-10). The grant takes the offer state alone (`DTOffers::ApplyGrant`), so it has no route to a stat. A tool is a choice, not a number on the hero: this is the one thing a run reads from `TreeRanks`.
- **R-MET-25** The first-run pool, on a fresh save:

| Item | Fresh save | How it opens |
|---|---|---|
| Keepers Storm, Forge, Tide | in | always |
| Keepers Shade, Radiance, Quicksilver | out | `Keeper.<Path>` in `Unlocked`, from its door (R-MET-28) |
| The nine S1 archetype Keys | in | always; R-OFF-07 |
| Keys of the S2 archetypes | with their Keeper | always, once that Keeper is met |
| Amp1, Amp2, Trait of every archetype | out | `Branch.<Path>.Blessing4`, `Blessing5` |
| `Transform.<Path>` | out | `Branch.<Path>.Reforge` |
| `Evolve.<Weapon>` | in | always: the weapon in hand is its gate, and the source's Fusion row is the Keeper's `[invented]` |
| `Weapon.PulseRifle`, `Weapon.RingEmitter`, `Weapon.OrbitalDrones`, `Weapon.ArcBlade`; `Mod.SplitCore`, `Mod.Pierce`, `Mod.Mirror`, `Mod.Backwash` | in | always: the `starter=yes` rows `first_run_locked.csv` does not name |
| The four rows of `first_run_locked.csv` | out | their id in `Unlocked`, from the Discovery in `door` |
| `starter=no` weapons and modifiers | out | their id in `Unlocked`, from a Discovery or a Trunk node; no row without one (R-MET-27) |
| `Reaction.Duos` | out | its id in `Unlocked`, from its door (R-MET-28) |
| Reserve cards | in | always; R-OFF-12 |
| Tools | `C.OFFER.REROLLS`, `C.OFFER.BANISH`, `C.OFFER.LOCK` | one more each from the Trunk |

`first_run_locked.csv` holds four of the six Discovery targets (GDD §8.3) that are `starter=yes` here, so their key opens a card `[invented]`, D-024. `Weapon.ArcBlade` and `Weapon.RingEmitter` stay open: shutting them thins a first run past R-OFF-15 (`S3_tree.md`), so `Freeze100` opens no card and of those two `Survive10` opens only `Weapon.SeekerSwarm`. The column stays the tool pool: `starter` is what a tool run reads (R-MET-23), and a gated pool reads this file on top of it. The generator fails on a row that is not `starter=yes` and on `Weapon.PulseRifle` (the start, D-015). The gate step `firstrun` reads a change to this list: the `tests` step's T-OFF-15 reads the tool pool and cannot see it, so `Tools/BotRuns.ps1 -FreshPool -Check T-OFF-15` plays each S1 Keeper on a pair of seeds, a battery of under a minute, and the step fails on any verdict but PASS. A real change to the list still earns `S3_tree.md`'s full battery.

| Id | Door |
|---|---|
| `Mod.ScatterShell` | `ScatterKills500` |
| `Mod.ChainTrigger` | `Elite3Mods` |
| `Mod.Conduit` | `Win` |
| `Mod.LingeringField` | `DroneKills300` |

R-MOD-07 counts every `starter` row, so T-MOD-07 does not see a first run: by caps, of the four open modifiers the Pulse Rifle fits four, the Ring Emitter three, and the Orbital Drones and Arc Blade two each, under `C.MOD.MIN_COMPATIBLE`. The Gravity Mine fits none of the four. It is not in a first run: its door is `Win`, and the same bank opens `Mod.Conduit`, the one modifier it holds until another door adds one that fits it (`Mod.ScatterShell`, `Mod.ChainTrigger`, `Mod.LingeringField`, or `Mod.Fork`, `Mod.Spillover`, `Mod.Refit` from `Trunk.Workshop`); T-MET-27 holds the two facts. Whether one is enough is the same open question as the four above.

- **R-MET-26** A purchase checks in this order: a known node, a session with a writable file, a rank left, `requires`, the balance. It takes the price, adds the rank to `TreeRanks` and the node's ids to `Unlocked`, and autosaves as a bank does (R-MET-13): a failed write keeps the purchase in memory and the next save carries it. The checks and the price are taken under the save's lock, on the file's profile; a lock another launch holds past the wait refuses the purchase as `Busy` (R-MET-11). A tool run has no file and buys nothing. The screen is `FDTTreeTab` (`vigil.md` R-VIG-12): it calls `UDTMetaSubsystem::FindSession`, then `DTTree::GetNode`, `CanBuy` and `Buy`, and takes names from `NodeName` and `NodeDesc`.
- **R-MET-27** Every Discovery has a door on a person's save or is stated out here. A row is shut only for what the table says; nothing is removed. Every "after" row but `Crit500` is a purchase on the Contracts page (`vigil.md` R-VIG-18, R-VIG-19). The generator fails on a `starter=no` row with neither a node nor a Discovery, on a `starter=yes` Discovery target left out of `first_run_locked.csv` other than the two weapons of R-MET-25, on a `door` that does not name its row, on a row stated out that gains a target, and on an `Elites` row and the timeline disagreeing with this table: a timeline that reaches the threshold has to bring the row back in.

| Discovery | State on a person's save | Why |
|---|---|---|
| `ReachL10`, `Survive10`, `Kills1000`, `WeaponL8`, `Dashes100`, `Win` | open on a first run | a level, the clock, kills, the rifle, dashes, the boss |
| `DroneKills300` | open on a first run | `Weapon.OrbitalDrones` is in the first-run pool |
| `Freeze100` | open on a first run | the Keys of Frost and Stillness freeze; it opens no card (R-MET-25) |
| `Elite3Mods` | open on a first run | the Pulse Rifle fits all four open modifiers |
| `Favor9` | after `Branch.<Path>.Blessing4` | Tier 3 takes nine cards of one Keeper; a first run holds three Keys |
| `TwoPaths6` | after `Blessing4` on two Paths | Tier 2 takes six cards of each |
| `ScatterKills500` | after `Trunk.ScatterCannon` | the weapon is `starter=no` |
| `RailKills300` | after `Trunk.RailLance` | the weapon is `starter=no` |
| `Crit500` | after `Keeper.Radiance` and `Win` | no card crits outside Radiance's rows, and it takes a weapon that hits hard: `Weapon.GravityMine`, which `Win` opens, read 656 to 1232 on six of nine gated runs with Radiance's Keys alone (`S3_doors.md`). The Rail Lance read at most 393 there |
| `Elites10` | **stated out** | `timeline_arena1.csv` spawns 2 + 3 + 4 = 9 elites a run and no other source exists. The row's text and its source both say 10 in a single run, so a lifetime count would make the text false, and the spawns stay (`S3_doors.md`). It opens only its Recollection, which stays locked |

- **R-MET-28** Doors of the pool ids no card row lists, `[invented]`: the source gives the three S2 Keepers and the duos no unlock. Each is a target of a row that already earns a Recollection, so no new text and no new price is needed, and a Keeper's door opens on a first run because `Crit500` and that Keeper's three Tree nodes wait on it. Shade and Quicksilver read over R-OFF-15's Reserve cap on the pool without `Weapon.FlakMortar`, so their doors land with it or after it: `ReachL10` writes it and `Keeper.Shade` in one bank, and `Survive10` takes ten minutes, past `ReachL10` on every saved run. Radiance reads under the cap there, so `WeaponL8` needs no order (`S3_doors.md`). A Discovery writes the id at the bank (R-MET-42), the hub reads it (`vigil.md` R-VIG-05), and the branch goes on sale (R-MET-21). The generator fails on one of these ids with no door or two, on a Keeper's door a first run cannot open, on a door of Shade or Quicksilver on any row but those two, on `ReachL10` losing the Mortar, on either of those two rows moved off the counter and threshold the order was read on (`DOOR_ORDER_CELLS` in `Tools/gen_catalog.py`), and on a Tree `requires` no row writes.

| Id | Door |
|---|---|
| `Keeper.Shade` | `ReachL10` |
| `Keeper.Radiance` | `WeaponL8` |
| `Keeper.Quicksilver` | `Survive10` |
| `Reaction.Duos` | `TwoPaths6` |

A door widens a pool and raises no number (D-023): a met Keeper brings its Keys and nothing more, the duos' door brings the duo card and the reactions start only when a run takes it (D-035), and in a gated run it holds no seat of its own: it reaches a Patron hand as the lead or on the last seat's draw, as any Keeper does (`keepers.md` R-KEE-23). The bestiary has no door of its own: `Kill.<Role>` opens on the kills a run banks (R-MET-41), and `Kill.Bruiser` and `Kill.Support` are stated out, because those roles do not exist.

### Discoveries
- **R-MET-40** The catalog is `Docs/09_Catalog/discoveries.csv`, one row per Discovery of `ST_Discoveries`, compiled into `DTCat::Discoveries`: `id` (the `ST_Discoveries` row), `counter` and `threshold` (met at or above), `targets` (pool ids), `recollection` (the `ST_Codex` row) and doc-only `absent` and `ref`. `Tools/gen_catalog.py` fails a row whose text is missing, whose target is not a `weapons.csv` or `modifiers.csv` id or a door of R-MET-28, or whose `ST_Codex` row does not carry `Unlock` `Discovery.<id>`. The source (GDD §8.3) gives seven targets; the other eight come from the `Context` notes of the source `ST_Codex`. Conditions are the `ST_Discoveries` text: all are per run, including the two the GDD left open.
- **R-MET-41** The counters are read-only, like the tally (R-MET-03). Kills by weapon and by role are counted each step from `FDTWorldView::Kills`; a kill names a weapon only through `WeaponSlot`, so a Keeper's own kill counts for no weapon. The rest is read once, at the end of the step the run ended on, or at the close for a run with no end of its own. The enemy pool keeps `FreezesBegun` and `BestCrit` in its counters for this and nothing reads them back.

| Counter | Reads |
|---|---|
| `Kills.Weapon.<Name>`, `Kills.Role.<Role>` | kills by the owner's weapon in that slot, and by role |
| `Dashes` | `FDTHeroState::Dashes` |
| `Status.Freeze` | enemies going from not frozen to frozen, a Blind's short Freeze included (R-KEE-25); a refresh of a frozen enemy is not a freeze `[invented]`: refreshes dwarf real freezes (`S3_discoveries.md`) |
| `Best.Crit` | the largest critical hit after the plate, affixes and Shock, before the enemy's remaining HP caps it `[invented]` |
| `Best.EliteKillMods` | most sockets, dormant ones included, on a weapon that killed an elite |
| `Best.Level`, `Best.WeaponLevel`, `Best.Tier`, `Best.PathsAtTier2` | the run state at the end |

`Seconds`, `Kills`, `Elites` and `BossKills` read the tally's own fields.
- **R-MET-42** Evaluation is the `OnBank` hook, `DTDiscovery::Apply`, bound by `UDTMetaSubsystem`, so a Discovery lands in the bank's atomic save (R-MET-19). A met row adds `<Id>` to `Discoveries`, and to `Unlocked` adds `Discovery.<Id>` and each target. A repeat changes nothing. No run takes a key away.
- **R-MET-43** Key forms in `Unlocked`, one set shared with the Tree: `Discovery.<Id>` is the `Unlock` column of `ST_Codex`, `ST_KeeperLines` and `ST_World`, so a reader shows a row when its `Unlock` is in the set (`World.Yard` opens with `Dashes100`); `Weapon.<Name>` and `Mod.<Name>` are the catalog ids in tag spelling, equal to `FDTOfferCard::ContentId`, so pool gating asks `Unlocked.Contains(ContentId)` (R-MET-22); `Keeper.<Path>` and `Reaction.Duos` are the ids of R-MET-28; the Tree writes `Weapon.*` and `Mod.*` too, along with `Blessing.*` and `Transform.*` (R-MET-21); a node itself is a key of `TreeRanks`, never of `Unlocked`. This system writes the first four, and never reads `Unlocked`. The other story keys need no entry: `Win` is `Wins > 0`, `Kill.<Role>` is `Kills.Role.<Role>` in `LifetimeStats`.
- **R-MET-44** Lifetime stats: every run counter joins `LifetimeStats` by name and adds up (R-MET-19), but a `Best.` key keeps its highest value, since a sum of bests means nothing. The run-over screen shows one toast under its panel, `Discovery.Toast` with the names of what this bank added, joined with ` · `, once the bank has run. A repeat shows nothing; many names shrink to the panel's width. There is no mid-run toast.
- **R-MET-45** A session with no file calls no hook (R-MET-19), so a tool run earns and writes nothing. What a run met still reads: the recorder logs `Meta: counters ...` and `Meta: discoveries met <n> of 15 [...], new [...]` at every banked run. These are log lines only, so R-MET-16 holds: `run_award` stays the JSONL's one addition, and it stays free of save fields. `Tools/BotRuns.ps1` logs carry them with no change.

### The loop, proved
- **R-MET-30** A person's loop is proved on real launches, off-screen, on isolated saves: `Tools/EarnedCheck.ps1`, gate step `earned`. From an empty save a gated run banks real Ichor and a real Discovery, nothing planted; a real key on the Contracts page buys a node that bank can afford; a new process reads the purchase; and the next gated run on that seed offers what the node opened, which the first could not. Then, on a copy of that first save with only a balance planted, keys buy `Reaction.Duos`' two prerequisites and one node of each branch the doors added; keys choose each door's Keeper at the Shrine and begin a gated run on it; each of those Keepers reaches a Patron hand with its bought card in a later hand; an L8 hand holds a met Keeper; and a run earns `TwoPaths6`, whose duo card the next run is offered. A bot stands in for the hands of the played runs; the keys, the file and the pool are real.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Award per minute survived | C.META.CURRENCY_PER_MINUTE | GATED(S3) | R-MET-02 |
| Award per kill | C.META.CURRENCY_PER_KILL | GATED(S3) | R-MET-02 |
| Award per elite | C.META.CURRENCY_PER_ELITE | GATED(S3) | R-MET-02 |
| Award for the boss kill | C.META.CURRENCY_BOSS_KILL | GATED(S3) | R-MET-02 |
| Award bonus per Heat level | C.META.CURRENCY_HEAT_BONUS | GATED(S3) | R-MET-02 |
| Story acts | C.META.ACT_START, C.META.ACT_MAX | CORE | R-MET-09 |
| Save version | C.SAVE.VERSION | CORE | R-MET-07, R-MET-10 |
| The longest wait for the save's lock, and the pause between asks | C.SAVE.LOCK_WAIT_MS, C.SAVE.LOCK_RETRY_MS | CORE | R-MET-11, T-MET-50 |
| Node prices | C.TREE.COST_TOOL, C.TREE.COST_WEAPON, C.TREE.COST_WORKSHOP, C.TREE.COST_BLESSING4, C.TREE.COST_BLESSING5, C.TREE.COST_REFORGE | GATED(S3) | R-MET-21, T-MET-21 |
| Tools per Trunk rank | C.TREE.TOOL_PER_RANK | GATED(S3) | R-MET-24, T-MET-20 |

## Data
`FDTMetaProfile`, in payload order (keys are `FName`, written as strings):

| Field | Type | Notes |
|---|---|---|
| `Currency` | `int64` | the balance |
| `Runs`, `Wins`, `DeathsSinceWin` | `int32` | R-MET-05 |
| `LastPatron` | `FName` | the Path, or empty |
| `TreeRanks` | `TMap<FName,int32>` | node id to rank; `tree.csv` owns the keys and costs |
| `Unlocked` | `TSet<FName>` | pool ids from the Tree and Discoveries: `Blessing.*`, `Transform.*`, `Weapon.*`, `Mod.*`, `Keeper.*`, `Reaction.Duos`; and `Discovery.<Id>`, which a met Discovery writes beside its targets for the readers of an `Unlock` cell (R-MET-42, R-MET-43) |
| `Discoveries` | `TSet<FName>` | earned discoveries; a set, so a repeat is nothing |
| `LifetimeStats` | `TMap<FName,int64>` | `Steps Kills Elites BossKills CurrencyEarned` plus every run counter |
| `Act` | `int32` | `C.META.ACT_START` until the story moves it |
| `StoryFlags` | `TSet<FName>` | |
| `RecollectionsRead` | `int32` | |
| `ArenaWins`, `ArenaHeat` | `TMap<FName,int32>` | by arena key |
| `FateTide`, `FateShade` | enum | Bound, or Returned and OffLedger; irreversible, the source's §5.1 |
| `FateSeen` | `TSet<FName>` | cutaways played |

| Type | Notes |
|---|---|
| `FDTRunTally` | `Steps Kills Elites bBossKilled Heat Patron Counters`: what the recorder hands the bank |
| `FDTRunAward` | `Retained`, `Balance`, whether the bank ran, and `Discovered`, what it added |
| `DTDiscovery` | `Apply`, the `OnBank` hook; `MetBy`, the rows a tally meets, reading no profile |
| `FDTMetaSession` | the profile, its path and the bank; `Change` is one locked change, `Flush` a save point with none; plain C++, so tests drive it on temp files |
| `FDTMetaLock` | the save's lock for one change (R-MET-11) |
| `UDTMetaSubsystem` | game instance subsystem: picks the path and opens one session a launch |
| `UDTRunRecorder` | world subsystem, the tally and the close of the run |
| `FDTTreeNode`, `EDTTreeState`, `EDTTreeBuy` | what a screen reads: id, Path, rank, price, state (Open, Short, Locked, Maxed), what is missing; `Busy` is a purchase the lock refused |
| `FDTPoolGrant` | `bGated Unlocked LastPatron Rerolls Banishes Locks`: what the save hands a run; empty in a tool run |
| `tree.csv` | `id class ranks cost grant grant_per_rank requires unlocks text_key tag ref` |
| `first_run_locked.csv` | `id door tag ref`; compiled into `DTCat::FirstRunLocked` |
| `-DTMetaFile=<path>`, `-DTNoSave` | command line, R-MET-14 |
| `-DTPoolGate` | command line, R-MET-23 |
| `DT.Meta.Wins <n>`, `DT.Meta.Currency <n>`, `DT.Tree.Buy <node\|All\|ToolPool>` | debug, not in Shipping: set the session and buy for `Tools/MetaCheck.ps1`; `ToolPool` buys the tool and branch nodes and writes the first-run locks, which is a tool run's pool |
| `DT.Meta.Plant <key> <amount>` | debug, not in Shipping: adds to a counter of this run's tally, for `Tools/MetaCheck.ps1` |
| `DT.Tree.BuyWhen <node> <file>`, `DT.Meta.Dump` | debug, not in Shipping, for `Tools/TwoWindowsCheck.ps1`: the first waits until the file exists, buys one rank through the same call a node's Accept makes, logs the result and quits; the second logs the session's balance, runs, wins, Tree ranks and unlocks |
| `DT.Meta.BankWhen <prefix> <rounds> <steps>` | debug, not in Shipping, for `Tools/TwoWindowsCheck.ps1 -Race`: each round it waits for the file `<prefix>.<round>`, holds until the instant written in it, and banks a run of that many steps through the call a run's end makes |
| `-DTWinCheck=<dir>` | command line, not in Shipping: T-MET-29's proof, its shots in that folder |

## Acceptance
- T-MET-01 `Dusktracer.Meta.NeutralKeys`: no source file under `Meta/` holds a fiction name.
- T-MET-02 `Dusktracer.Meta.Award`: each term alone, all together, the floor, the fraction of a minute, and the Heat bonus give the formula's values.
- T-MET-03 `Dusktracer.Meta.Tally`: on a played bot run the recorder's kills match the sum of the telemetry `kill` records, steps stop at the run's end, and a boss kill counts once.
- T-MET-04 `Dusktracer.Meta.BankOnce`: ending a session banks once; a second end does nothing; a zero-step run banks nothing and writes no file.
- T-MET-05 `Dusktracer.Meta.BankCounts`: victory, death and leaving move `Runs`, `Wins` and `DeathsSinceWin` as the rule says; the balance grows by the award; stats and counters add up.
- T-MET-06 `Dusktracer.Meta.SummaryLine`: the line resolves with both numbers, reads whole in en, and its `ST_UI` row carries both languages; `Summary.Waiting` is one of the menu's keys, resolves, and its row carries both languages, each in its own words. The test process is an editor, which shows game text in its source language whatever culture is set, so zh-Hans is held by the rows and drawn by a zh-Hans launch, not resolved here.
- T-MET-07 `Dusktracer.Meta.FileHeader`: the written file opens with the envelope at `C.SAVE.VERSION` and the default path ends in `Saved/SaveGames/DTMeta.sav`.
- T-MET-08 `Dusktracer.Meta.Canonical`: every field round-trips; insertion order does not change the bytes; truncated, padded and empty-key payloads fail.
- T-MET-09 `Dusktracer.Meta.Validator`: each broken field fails, and a good-checksum file failing it goes aside. `Dusktracer.Meta.FatesInRange`: each fate reads back in range, a value past it or below zero does not read, and a file holding one goes aside whole.
- T-MET-10 `Dusktracer.Meta.Migration`: an older version runs its step before the validator; a missing step keeps the file aside; unknown keys stay.
- T-MET-11 `Dusktracer.Meta.AtomicWrite`: a blocked temporary file fails the write and leaves the old file whole; a stale `.tmp` is ignored and replaced. `Dusktracer.Meta.AtomicReplace` (Windows): past that guard, with the temporary file written in full and the replace refused because a reader holds the old file, the write fails, the old file is whole byte for byte, the session keeps the change and its next write lands it over the leftover `.tmp`.
- T-MET-12 `Dusktracer.Meta.BadFile`: corrupt, truncated, too-new and bad-magic files each move to a new `.bad.<n>` byte for byte, older copies stay, the session starts empty and its next save makes a fresh file; a locked file sticks the session and leaves the file unchanged. `Dusktracer.Meta.BadFileBranches`: a migration that fails keeps the file aside whole and the session empty, not on what the step left; a file that cannot be read at all (Windows) sticks the session with nothing set aside, writes nothing, and reads again once it can be.
- T-MET-13 `Dusktracer.Meta.Autosave`: a bank on a file session writes the file before it returns, and a reload reads the same bytes; after a failed write the next bank carries both awards.
- T-MET-14 `Dusktracer.Meta.ToolRuns`: unattended, automation and bot processes use no file; an override does; this very run has no active path; a session with no path leaves its profile empty and writes nothing.
- T-MET-15 `Dusktracer.Meta.Readers`: only the listed files include a `Meta/` header.
- T-MET-16 `Dusktracer.Meta.SimUntouched`: two bot runs of one seed, with the recorder off the clock and on it, end with equal steps, input hash, hero state and CSV, and JSONL equal but for `run_award`. `Tools/MetaCheck.ps1` proves it on real launches: no save against an empty isolated save, with equal input hash, steps and CSV.
- T-MET-17 `Dusktracer.Meta.WinsReachForm`: no session reads `0` and form I; a profile at `C.BOSS.FORM_II_WINS` wins reads form II; `Tools/MetaCheck.ps1` starts a real fight on a saved win and reads its wins in the log.
- T-MET-18 `Dusktracer.Meta.ModelFields`: each field of §Data round-trips set to a value, and an all-default profile reads as empty.
- T-MET-19 `Dusktracer.Meta.BankHook`: a hook runs after the award and before the write, and what it adds is in the file; a no-file session calls none.
- T-MET-20 `Dusktracer.Meta.TreeCatalog`: every node of the catalog is `pool` or `utility`, a pool node grants nothing and a utility node only a tool; a full Tree changes the run start's tool counts and nothing else of the player. `FDTStatSet` does not exist as a type, so `Tools/MetaCheck.ps1` proves the rest on a real launch: `DT.Tree.Buy ToolPool`, gate on, plays the same input hash, steps and CSV as a tool run and is dealt its hands card for card (`Tools/pool_read.py`), over the step's `-PoolSeconds` of play, in which the tool run has to show Tree-locked modifiers and blessing cards, or the step fails as unable to tell the pools apart (D-037). The three Trunk pool nodes widen past a tool run's pool, so that launch leaves them out; the catalog check above is what binds them.
- T-MET-21 `Dusktracer.Meta.TreeBuy`: exact money buys and leaves zero; unknown node, prerequisite, maxed, short and no-file are refused and spend nothing; the rank, the unlocks and the balance are in the file before `Buy` returns; the list and the four tile states read true.
- T-MET-22 `Dusktracer.Meta.PoolGate`: a gated pool offers a locked id only from `Unlocked`, each node opens what R-MET-21 and R-MET-22 say, a shut Path row yields its slot with the hand whole, and an ungated pool has everything; this automation process does not gate. `Dusktracer.Arsenal.Refit`: a shut starter stays out of `Mod.Refit`'s draw and an unlocked id comes in.
- T-MET-23 `Dusktracer.Meta.FirstRunPool`: on an empty profile every row of R-MET-25 reads as the table says, across every catalog row: four weapons, four modifiers, the start among them; a Discovery's key opens a first-run lock; an ungated pool holds it.
- T-MET-24 `Dusktracer.Meta.TreeGrant`: a Trunk rank is its tools; a rank above the catalog's reads as the catalog's; an unknown key adds nothing.
- T-MET-25 `Dusktracer.Meta.TreeText`: every node's name and line resolve in en and zh-Hans with no key or brace left. A row missing from `ST_Tree` never prints its key there, so the generator holds that half: `Tools/gen_catalog.py` fails a node whose `text_key` has no `.Name` or `.Desc` row (`Tools/tools_test.py` `test_a_tree_node_without_its_text_fails_the_generator`).
- T-MET-26 `Dusktracer.Meta.TreeBuyWrite`: a purchase through a blocked write stays in memory and the next purchase's file carries both; a stuck session sells nothing.
- T-MET-27 `Tools/tools_test.py` `Doors`: the Id and Door table of R-MET-25 is `first_run_locked.csv`, row for row; the Gravity Mine fits no modifier of a first run and exactly `Mod.Conduit` of what its own door opens (`test_the_gravity_mine_opens_with_its_one_modifier`); R-MET-27 names every Discovery exactly once; every `starter=no` row has a node or a Discovery; every `door` names its row; the generator's lists are this rule's states, and a row stated out is out of reach and opens nothing. The generator holds the catalog cases as well.
- T-MET-28 `Tools/tools_test.py` `Doors`: the table of R-MET-28 is the catalog, a Keeper's door opens on a first run, and a bestiary row names a role or is stated out. A door of Shade or Quicksilver lands with the Flak Mortar or after it, and the two rows that order stands on keep their counter and threshold (`test_the_flak_mortar_order_is_held_by_its_cells`). `Dusktracer.Meta.KeeperDoors`: each door writes its id and a Keeper's branch goes on sale; `Kills1000` alone opens no Keeper; the lead Patron takes the first seat with every Keeper met; on a gated pool an S1 Keeper still draws at L8 and a tool run keeps the seat rule; a met Keeper's Keys are in and its Amps shut; the duo card reaches a gated pool. `Tools/MetaCheck.ps1` step I proves it on real launches; the Amps are read shut over `-PoolSeconds`, in the hands dealt after the run took a Key, against a tool run that chose the Keeper and was dealt them (D-037).
- T-MET-29 `Tools/WinCheck.ps1`, gate step `win`: a bot wins on an empty save with the first-run pool, off-screen, en and zh-Hans; on that one launch the toast the run-over screen draws names `Win`, a second session reads every earned key from the file, and the way back opens the hub, where the Shrine reads the saved Keepers, the five role rows of the bestiary open, `Kill.Bruiser` and `Kill.Support` stay shut, and every Recollection reads as earned or locked. Shots of the toast and the book are kept. A seed the bot loses goes to the next.
- T-MET-30 `Tools/EarnedCheck.ps1`, gate step `earned`, en, about two minutes: every clause of R-MET-30 on real launches. The balance the page opens on is the bank the run logged, and part one's launches carry no plant. A seed that leaves one of the three doors shut goes to the next, because part two starts from that save, and so does a seed that earns no `TwoPaths6`, or one that deals no duo card, read on a fresh copy of the save. Hub scripts `Earned`, `EarnedRead` and `Doors` in `Hub/DTHubCheck.cpp`; each launch's hands stay in `Saved/EarnedCheck/hands_<tag>.txt`.
- T-MET-31 `Dusktracer.Meta.PatronSeats`: for every set of met Keepers and every lead a save can name, on many seeds: the hand is whole at `C.OFFER.KEEPER_CHOICES`, all different Keepers, so meta adds no seat; the lead holds the first seat; Path order holds the next, so no hand is all met Keepers; every Keeper of the run reaches the hand; a run with no met Keeper draws nothing, so its seed does not move; a tool run keeps its seats and draws nothing.
- T-MET-40 `Dusktracer.Meta.DiscoveryCatalog`: fifteen rows, no id twice, each named in `ST_Discoveries`, targets in tag spelling, none met by an empty tally. `Tools/gen_catalog.py` checks the rest (R-MET-40).
- T-MET-41 `Dusktracer.Meta.DiscoveryPlanted`: for every row, a planted counter one short earns and unlocks nothing; at the threshold the row earns itself alone and writes exactly its keys; twice is once.
- T-MET-42 `Dusktracer.Meta.DiscoveryBank`: a Discovery lands in the bank's file with its keys, `Discovered` lists it once and a repeat lists none; a `Best.` stat keeps its highest and a plain one adds up; a no-file session earns and keeps nothing while `MetBy` still reads the tally.
- T-MET-43 `Dusktracer.Meta.RunCounters`: on a played bot run, role kills add up to the kills, weapon kills are no more than the kills, dashes and level equal the hero's and the player's; on a bare pool, only a critical hit moves `BestCrit`, and a refreshed freeze counts once.
- T-MET-44 `Dusktracer.Meta.DiscoveryToast`: no Discovery gives no line; the line resolves with each name.
- T-MET-45 `Tools/MetaCheck.ps1` step F: a tool launch with a planted counter logs the row as met, adds nothing and leaves `DTMeta.sav` alone.
- T-MET-46 `Dusktracer.Meta.BossTrade`: in a game world with the game mode, a run whose Warden and hero fall on one step (`boss.md` R-BOS-13, D-041; both orders) closes as `victory` and banks as one: `run_award` says victory with the boss, the tally holds the boss kill and `Kills.Role.Boss`, the award holds `C.META.CURRENCY_BOSS_KILL`, and the profile gains one win and the `Win` Discovery; a death with the hull standing closes as `death` and banks none of them.
- T-MET-47 `Dusktracer.Meta.PauseFrame`: R-MET-04 when the world pauses on the frame of a run's last step, where a paused world never fires the next-tick close (`settings.md` R-SET-21). On whole frames of a world with a game instance, the world's own game mode, a player controller and the engine's pause: a death and a win, each with a Pause press inside that frame and with a level-up hand queued in that step, close by themselves with the run's own end, open the run-over screen with its title and the summary line, write `run_end` and `run_award` with that end, and leave one run with that end in the save file. The same ends with no pause are the control.
- T-MET-48 `Tools/TwoWindowsCheck.ps1`, by hand, about three minutes: two game processes on one save, each scene in a `-userdir` of its own under `Saved/TwoWindows`, the order forced by the script and the file read afterwards by a third process. A bank over a purchase keeps the purchase and its price; a bank over a bank keeps both runs and both awards; on a balance that pays for one node a second purchase is refused and the first stands; a purchase over a bank keeps the run and its award. `Dusktracer.Meta.TwoWindows` holds the same on two sessions in one process.
- T-MET-49 `Dusktracer.Meta.SaveWindow`: a second session's change forced inside the first one's reread-to-write window, through the bank hook (R-MET-11). A bank made there is kept, said, and in the file after that session's next bank, with every run, award and death of both; a purchase made there is refused as `Busy`, spends nothing, and buys when asked again. `Tools/TwoWindowsCheck.ps1 -Race <trials>`, by hand: real launches held open on one save bank at one instant, trial after trial; no trial may lose a bank, and a last process reads every run and every award from the file.
- T-MET-50 `Dusktracer.Meta.SaveLock`: the lock has one holder, a second ask waits `C.SAVE.LOCK_WAIT_MS` and under a second, and on Windows no lock file is left; a lock file nobody holds blocks nothing. With the lock held by another, a purchase is refused, a bank is kept with the file untouched, and once it is free the kept run lands on what the holder wrote meanwhile. A bank and a purchase a failed write left in memory land on a second window's changes: both runs stand, the node is sold once, and the purchase that cannot be made again is dropped, not charged. `Tools/TwoWindowsCheck.ps1 -Race` ends with the same on real launches, the script holding the lock: a kept bank written by the launch's next bank, a purchase refused, and a bot run that quits with the lock never free, whose log and `save_deferred` record say the bank was kept and whose log says it is lost.
- `Tools/MetaCheck.ps1`, gate step `meta`: real launches off-screen. A tool launch leaves no `DTMeta.sav`; a launch with `-DTMetaFile` banks and writes; a second launch reads it and adds to it; a corrupted file goes aside byte for byte and a fresh one is written; a saved win reaches a real fight; a planted counter earns a Discovery into an isolated save and the next launch holds it, and the same plant with no file earns nothing; a profile at the tool pool under `-DTPoolGate` plays the tool run's seed exactly; `-DTPoolGate` with no save shows Keys and no locked card or first-run lock in any hand. The three pool readings (the tool pool, the first-run pool, a met Keeper's Amps) play `-PoolSeconds` at the bot's full skill and are read from the cards dealt, each against a tool run of the same seed and length that has to have shown the Tree-locked cards in question; the save steps stay at `-Seconds`, two hands (D-037).
- HUMAN: the line on the run-over screen reads well at the largest text size, and the award feels worth a run once there is something to spend it on; a first run reads as a game and not as a locked one (`Docs/Validation/S3_tree.md`).

## Telemetry
`run_award{end retained steps kills elites boss heat}`, once per banked run, before `run_end`. Run-derived only: it holds no balance and no save field, so R-MET-16 holds and R-TEL-08 keeps it deterministic. `end` reads `victory`, `death` or `left`.

`save_deferred{change waiting}`, after `run_award`, only when the save's lock was another launch's past the wait (R-MET-11): `change` reads `bank`, `waiting` is how many changes the session still owes the file. A refused purchase writes none: the hub has no telemetry session, and the log line is its record.

## Text
`ST_UI.Summary.Waiting`: one row, en and zh-Hans, both `[invented]` (D-042): a save in use is no part of the source. `ST_UI.Summary.Line`: one row, en and zh-Hans. The wording comes from `Summary.Retained` and `RunSummary.Balance` of the source tables, joined on one line. Name of the currency: Ichor, 神血 (`G01_Name.md`, `Glossary.md`). The other source `Summary.*` rows wait for their screen.

`ST_Tree`, a new table, en and zh-Hans: the source's `Tree.*` rows for kept nodes and the screen's states, none for a cut node (D-023). A node's `text_key` gives `<key>.Name` and `<key>.Desc`; `Desc` takes the Keeper's name and the tool count. The two blessing lines are rewritten, because a node opens more than one card. The three invented Trunk nodes have invented text, en and zh-Hans. No row carries a story layer: surface text. Invented zh is listed in `Docs/Validation/S3_tree.md`.

## Out of scope
- The return line after a death. The hub, the Codex and the Tree's page are `vigil.md`. The fields exist; the systems are their own specs.
- A mid-run toast, and `Discovery.Unlocks` until a Codex screen draws it.
- Heat, Keeper Bids (Lien), Omens and Off-ledger shares, all of which change the award later; the Currency Reserve card (`offers.md`).
- Cloud saves, several profiles, a save-reset screen, a backup copy beyond R-MET-12.
- Co-op: one profile, the team's tally. A second profile is `coop.md`'s to ask for.

## Open
| Question | Data that resolves it |
|---|---|
| Does Luck stay out? It raises rarity, but `offers.md` lists it as meta | a human reading of the pool once the Tree exists |
| Is the award's scale right? The bound is eight wins for the whole Tree, five for the Trunk and the three S1 Keepers (`S3_tree.md`) | a human's award per run |
| Are the doors of R-MET-28 at the right depth? A bot's first win earns all three Keepers at once; the owner's one run (build `996cd47`, not a validator) reached level 10, was short of the clock and took no weapon upgrade | a human reading on a fresh save; moving a door is one cell of `discoveries.csv`, inside the order R-MET-28 holds |
| `Reaction.Duos` waits on `TwoPaths6`, which waits on two purchases: is a bought depth the right door for a duo? | a writer's call |
| The narrow pool beat the full pool for the bot (69 wins to 47 of 90): do unlocks cost power? | a human reading on a fresh and a full profile (`S3_tree.md`) |
| On a first run the Orbital Drones and the Arc Blade fit two open modifiers each: is that too thin a socket choice? | a human reading on a fresh profile |
| Should `Freeze100` open a card? The Ring Emitter stays in the first pool for R-OFF-15 | a writer's call on the row, or first-run content that lets the weapon be shut |
| Should the source's capstones go back to a Discovery? None of the 15 rows is one | a writer's call on a row; a Trait is then named by that row and taken out of its node's `unlocks` |
| Should a quit forfeit the award, to stop a mid-run exit hiding a death from `DeathsSinceWin`? | a human reading; it only moves the return line |
| `Elites10` is stated out on arena 1: does a later arena or Heat bring a tenth elite, or does a writer change the row? | a writer's call on the row and its text; the generator flags the first timeline that reaches 10 (R-MET-27) |
