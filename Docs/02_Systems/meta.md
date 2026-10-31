# Meta — the save, the currency and the run-end autosave (S3 row)

## Intent
Every run feeds the next: what a run earns outlives it, in one versioned save that later systems (Tree, Discoveries, Hub, story) write into without a new file. Pillar: meta widens the pool, it never raises a number. Nothing in the save reaches the sim while the save is empty, and no tool run ever touches a player's save.

Written from `Handoff_Dusktracer/game/G02_Blueprint_DocSet_zh.md` (the meta row and the day-0 list: currency formula, tree of a trunk plus six branches, save with a version from the first write, a validator and a migration table), `reference/original_docs/01_GameDesign.md §8`, `02_TechnicalDesign_s1-16.md §10`, `reference/specs/full_game_specs.md §2.1` and `§5.1`, and `G01_Name.md` (slot `DTMeta.sav`; the currency's name is unchanged). `[invented]` marks what the source does not give; it is this spec's choice and the first thing to revisit.

## Rules
### Names
- **R-MET-01** Fiction is never a key (`AGENTS.md`). Code, save fields and telemetry say `Currency`, `Tree`, `Hub`, `Discovery`; the Path is the key of a Keeper. The fiction names (Ichor, Contracts, Vigil) live only in string tables and in this spec's prose about them.

### Currency
- **R-MET-02** A run's award is the source's formula, floored to a whole number: `C.META.CURRENCY_PER_MINUTE × minutes survived + C.META.CURRENCY_PER_KILL × kills + C.META.CURRENCY_PER_ELITE × elites + C.META.CURRENCY_BOSS_KILL × boss kill`, all times `1 + C.META.CURRENCY_HEAT_BONUS × Heat`. Minutes are fractional, from sim steps. Kills count every kill, elites and the boss included; elites and the boss then add their own term. Heat is `0` until Heat is built. The award is what the source calls retained; the reclaimed and share lines are display-only and not built.
- **R-MET-03** The tally is sim-clocked and read-only: `UDTRunRecorder` runs at the end of each step, counts `FDTWorldView::Kills` and steps, writes nothing any system reads, and stops at the end of the step the run ended on, so the frame rate never moves an award. Sim time is the minutes, so time in a hand or a menu earns nothing.
- **R-MET-04** A run is banked exactly once, however it ends: death, victory, Abandon run, or Restart run and Quit to desktop from a run still going. The source keeps the award on death; the last two keep it too, so no exit forfeits it `[invented]`. The bank hangs off the telemetry session's close, which is already once per run (`settings.md` R-SET-22, R-SET-23, R-SET-25). A run of zero steps (a restart from the first Patron hand) banks nothing and is no run.
- **R-MET-05** Banking adds the award to the balance and to `Runs`. Victory adds to `Wins` and clears `DeathsSinceWin`; a death or an abandoned run adds to `DeathsSinceWin` (GDD §8.6 reads it); a restart or a quit moves neither `[invented]`. The Patron is kept as `LastPatron`, and the run's counts are added to `LifetimeStats`.
- **R-MET-06** The run-over screen shows one line, `Summary.Line`: the award and the balance after it. It is drawn only once the bank has run. No other UI exists here: the source's summary stays out of scope (`settings.md` R-SET-21).

### The file
- **R-MET-07** One file per profile, `DTMeta.sav` under `Saved/SaveGames`, wrapped in the `FDTSaveEnvelope` of R-TEC-04 at `C.SAVE.VERSION`. Settings stay in their own file (`settings.md` R-SET-03); the source's settings fields in the save are dropped, because a save must never carry what R-SET-05 keeps from tool runs.
- **R-MET-08** The payload is canonical: little-endian, fields in the order of §Data, map and set keys sorted, UTF-8 strings with a length. The same profile always gives the same bytes. A reader that runs off the end, finds bytes left over, or meets an empty key treats the file as bad.
- **R-MET-09** The validator runs on every load, after the checksum and any migration: no negative count, `Wins` and `DeathsSinceWin` within `Runs`, `Act` from `C.META.ACT_START` to `C.META.ACT_MAX`, fates in range. A file that fails is bad, even with a good checksum.
- **R-MET-10** Migrations are the envelope's upgrade-only chain, one step per version. Any new field raises `C.SAVE.VERSION` and adds a step that fills its default (a new fate field defaults to Bound, as the source says). The first version holds every field of §Data, so the first system to fill one needs no bump. Unknown tree, unlock and discovery keys load and stay: a removed node never costs a player their record.
- **R-MET-11** Writes are atomic: the bytes go to a sibling `.tmp`, are flushed to disk, then replace the file in one operation (`MoveFileExW` on Windows, `rename` elsewhere). A failed write leaves the old file whole. A leftover `.tmp` is ignored and overwritten. `IFileManager::Move` is not used here: it deletes the target first.
- **R-MET-12** A bad file is kept: corrupt, too new, a missing or failed migration, or failing R-MET-09, it is moved to `<file>.bad.<n>`, the lowest free number, so no older copy is overwritten; the session starts empty and logs a warning. If the move fails, or the file cannot be read at all, the session is stuck: it plays on an empty profile and writes nothing, so the file stays as it is.
- **R-MET-13** Autosave runs at the end of the bank, on the game thread, never inside a sim step. A write that fails keeps the profile in memory and the next bank tries again. Tree purchases save the same way when the Tree exists.

### Tool runs and the sim
- **R-MET-14** Tool runs have no save: a process that is `-unattended`, under automation, a bot (`-DTBot*`) or given `-DTNoSave` neither reads nor writes the file, and banks nothing into its profile; the award is still computed, for the line and for telemetry. `-DTMetaFile=<path>` names an isolated save and overrides this, as `-DTSettingsFile` does (`settings.md` R-SET-05). Every launcher in `Tools/` but `Play.bat` is `-unattended`; `Play.bat` is the person's launch and the one that saves.
- **R-MET-15** Files under `Source/Dusktracer/Meta/` are read by the listed files only (T-MET-15), and the one sim reader is the boss. A new reader is added to that list on purpose.
- **R-MET-16** Determinism: with an empty profile a seed plays exactly as it does with no save: same steps, same input hash, same CSV. The `run_award` record is the only addition to the JSONL, and it holds no save field. This holds for any profile with `Wins` at `0`, because nothing else of the save reaches a system until a later spec says so; such a spec restates this rule for its reader.
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

### Discoveries
The rule numbers leave a gap so the Tree's rules can follow R-MET-20.

- **R-MET-40** The catalog is `Docs/09_Catalog/discoveries.csv`, one row per Discovery of `ST_Discoveries`, compiled into `DTCat::Discoveries`: `id` (the `ST_Discoveries` row), `counter` and `threshold` (met at or above), `targets` (pool ids), `recollection` (the `ST_Codex` row) and doc-only `absent` and `ref`. `Tools/gen_catalog.py` fails a row whose text is missing, whose target is not a `weapons.csv` or `modifiers.csv` id, or whose `ST_Codex` row does not carry `Unlock` `Discovery.<id>`. The source (GDD §8.3) gives seven targets; the other eight come from the `Context` notes of the source `ST_Codex`. Conditions are the `ST_Discoveries` text: all are per run, including the two the GDD left open.
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
- **R-MET-43** Key forms in `Unlocked`, one set shared with the Tree: `Discovery.<Id>` is the `Unlock` column of `ST_Codex`, `ST_KeeperLines` and `ST_World`, so a reader shows a row when its `Unlock` is in the set (`World.Yard` opens with `Dashes100`); `Weapon.<Name>` and `Mod.<Name>` are the catalog ids in tag spelling, equal to `FDTOfferCard::ContentId`, so pool gating asks `Unlocked.Contains(ContentId)`; `Trunk.*` and `Branch.*` are the Tree's (R-MET-20). This system writes only the first two, and never reads `Unlocked`. The other story keys need no entry: `Win` is `Wins > 0`, `Kill.<Role>` is `Kills.Role.<Role>` in `LifetimeStats`.
- **R-MET-44** Lifetime stats: every run counter joins `LifetimeStats` by name and adds up (R-MET-19), but a `Best.` key keeps its highest value, since a sum of bests means nothing. The run-over screen shows one toast under its panel, `Discovery.Toast` with the names of what this bank added, joined with ` · `, once the bank has run. A repeat shows nothing; many names shrink to the panel's width. There is no mid-run toast.
- **R-MET-45** A session with no file calls no hook (R-MET-19), so a tool run earns and writes nothing. What a run met still reads: the recorder logs `Meta: counters ...` and `Meta: discoveries met <n> of 15 [...], new [...]` at every banked run. These are log lines only, so R-MET-16 holds: `run_award` stays the JSONL's one addition, and it stays free of save fields. `Tools/BotRuns.ps1` logs carry them with no change.

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

## Data
`FDTMetaProfile`, in payload order (keys are `FName`, written as strings):

| Field | Type | Notes |
|---|---|---|
| `Currency` | `int64` | the balance |
| `Runs`, `Wins`, `DeathsSinceWin` | `int32` | R-MET-05 |
| `LastPatron` | `FName` | the Path, or empty |
| `TreeRanks` | `TMap<FName,int32>` | Tree key to rank; the Tree spec owns the keys and costs |
| `Unlocked` | `TSet<FName>` | pool unlocks from the Tree and Discoveries |
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
| `FDTMetaSession` | the profile, its path and the bank; plain C++, so tests drive it on temp files |
| `UDTMetaSubsystem` | game instance subsystem: picks the path and opens one session a launch |
| `UDTRunRecorder` | world subsystem, the tally and the close of the run |
| `-DTMetaFile=<path>`, `-DTNoSave` | command line, R-MET-14 |
| `DT.Meta.Wins <n>` | debug, not in Shipping: sets the session's wins for `Tools/MetaCheck.ps1` |
| `DT.Meta.Plant <key> <amount>` | debug, not in Shipping: adds to a counter of this run's tally, for `Tools/MetaCheck.ps1` |

## Acceptance
- T-MET-01 `Dusktracer.Meta.NeutralKeys`: no source file under `Meta/` holds a fiction name.
- T-MET-02 `Dusktracer.Meta.Award`: each term alone, all together, the floor, the fraction of a minute, and the Heat bonus give the formula's values.
- T-MET-03 `Dusktracer.Meta.Tally`: on a played bot run the recorder's kills match the sum of the telemetry `kill` records, steps stop at the run's end, and a boss kill counts once.
- T-MET-04 `Dusktracer.Meta.BankOnce`: ending a session banks once; a second end does nothing; a zero-step run banks nothing and writes no file.
- T-MET-05 `Dusktracer.Meta.BankCounts`: victory, death and leaving move `Runs`, `Wins` and `DeathsSinceWin` as the rule says; the balance grows by the award; stats and counters add up.
- T-MET-06 `Dusktracer.Meta.SummaryLine`: the line resolves in en and zh-Hans with both numbers.
- T-MET-07 `Dusktracer.Meta.FileHeader`: the written file opens with the envelope at `C.SAVE.VERSION` and the default path ends in `Saved/SaveGames/DTMeta.sav`.
- T-MET-08 `Dusktracer.Meta.Canonical`: every field round-trips; insertion order does not change the bytes; truncated, padded and empty-key payloads fail.
- T-MET-09 `Dusktracer.Meta.Validator`: each broken field fails, and a good-checksum file failing it goes aside.
- T-MET-10 `Dusktracer.Meta.Migration`: an older version runs its step before the validator; a missing step keeps the file aside; unknown keys stay.
- T-MET-11 `Dusktracer.Meta.AtomicWrite`: a blocked temporary file fails the write and leaves the old file whole; a stale `.tmp` is ignored and replaced.
- T-MET-12 `Dusktracer.Meta.BadFile`: corrupt, truncated, too-new and bad-magic files each move to a new `.bad.<n>` byte for byte, older copies stay, the session starts empty and its next save makes a fresh file; a locked file sticks the session and leaves the file unchanged.
- T-MET-13 `Dusktracer.Meta.Autosave`: a bank on a file session writes the file before it returns, and a reload reads the same bytes; after a failed write the next bank carries both awards.
- T-MET-14 `Dusktracer.Meta.ToolRuns`: unattended, automation and bot processes use no file; an override does; this very run has no active path; a session with no path leaves its profile empty and writes nothing.
- T-MET-15 `Dusktracer.Meta.Readers`: only the listed files include a `Meta/` header.
- T-MET-16 `Dusktracer.Meta.SimUntouched`: two bot runs of one seed, with the recorder off the clock and on it, end with equal steps, input hash, hero state and CSV, and JSONL equal but for `run_award`. `Tools/MetaCheck.ps1` proves it on real launches: no save against an empty isolated save, with equal input hash, steps and CSV.
- T-MET-17 `Dusktracer.Meta.WinsReachForm`: no session reads `0` and form I; a profile at `C.BOSS.FORM_II_WINS` wins reads form II; `Tools/MetaCheck.ps1` starts a real fight on a saved win and reads its wins in the log.
- T-MET-18 `Dusktracer.Meta.ModelFields`: each field of §Data round-trips set to a value, and an all-default profile reads as empty.
- T-MET-19 `Dusktracer.Meta.BankHook`: a hook runs after the award and before the write, and what it adds is in the file; a no-file session calls none.
- T-MET-20 PLANNED(S3) with the Tree: every node of the catalog is `pool` or `utility`, and a full Tree changes no `FDTStatSet` value at run start.
- T-MET-40 `Dusktracer.Meta.DiscoveryCatalog`: fifteen rows, no id twice, each named in `ST_Discoveries`, targets in tag spelling, none met by an empty tally. `Tools/gen_catalog.py` checks the rest (R-MET-40).
- T-MET-41 `Dusktracer.Meta.DiscoveryPlanted`: for every row, a planted counter one short earns and unlocks nothing; at the threshold the row earns itself alone and writes exactly its keys; twice is once.
- T-MET-42 `Dusktracer.Meta.DiscoveryBank`: a Discovery lands in the bank's file with its keys, `Discovered` lists it once and a repeat lists none; a `Best.` stat keeps its highest and a plain one adds up; a no-file session earns and keeps nothing while `MetBy` still reads the tally.
- T-MET-43 `Dusktracer.Meta.RunCounters`: on a played bot run, role kills add up to the kills, weapon kills are no more than the kills, dashes and level equal the hero's and the player's; on a bare pool, only a critical hit moves `BestCrit`, and a refreshed freeze counts once.
- T-MET-44 `Dusktracer.Meta.DiscoveryToast`: no Discovery gives no line; the line resolves with each name.
- T-MET-45 `Tools/MetaCheck.ps1` step F: a tool launch with a planted counter logs the row as met, adds nothing and leaves `DTMeta.sav` alone.
- `Tools/MetaCheck.ps1`, gate step `meta`: real launches off-screen. A tool launch leaves no `DTMeta.sav`; a launch with `-DTMetaFile` banks and writes; a second launch reads it and adds to it; a corrupted file goes aside byte for byte and a fresh one is written; a saved win reaches a real fight; a planted counter earns a Discovery into an isolated save and the next launch holds it, and the same plant with no file earns nothing.
- HUMAN: the line on the run-over screen reads well at the largest text size, and the award feels worth a run once there is something to spend it on.

## Telemetry
`run_award{end retained steps kills elites boss heat}`, once per banked run, before `run_end`. Run-derived only: it holds no balance and no save field, so R-MET-16 holds and R-TEL-08 keeps it deterministic. `end` reads `victory`, `death` or `left`.

## Text
`ST_UI.Summary.Line`: one row, en and zh-Hans. The wording comes from `Summary.Retained` and `RunSummary.Balance` of the source tables, joined on one line. Name of the currency: Ichor, 神血 (`G01_Name.md`, `Glossary.md`). Every other source row with Ichor in it (`Tree.*`, `Summary.*`) waits for its screen.

## Out of scope
- The Tree, its costs and screen; pool gating, which reads `Unlocked`; the Hub; the Codex screen, which reads `Unlock`; the return line after a death. The fields exist; the systems are their own specs.
- A mid-run toast, and `Discovery.Unlocks` until a Codex screen draws it.
- Heat, Keeper Bids (Lien), Omens and Off-ledger shares, all of which change the award later; the Currency Reserve card (`offers.md`).
- Cloud saves, several profiles, a save-reset screen, a backup copy beyond R-MET-12.
- Co-op: one profile, the team's tally. A second profile is `coop.md`'s to ask for.

## Open
| Question | Data that resolves it |
|---|---|
| Does Luck stay out? It raises rarity, but `offers.md` lists it as meta | a human reading of the pool once the Tree exists |
| Is the award's scale right? The source's costs are unknown here | the Tree spec's cost table against a bot run's award |
| Should a quit forfeit the award, to stop a mid-run exit hiding a death from `DeathsSinceWin`? | a human reading; it only moves the return line |
| `Elites10` cannot fire: arena 1 spawns 2 + 3 + 4 = 9 elites a run. Does its threshold drop, or do elites grow? | a writer's call on the row; the text says 10, so the number stays until then (`S3_discoveries.md`) |
| Scatter Cannon and Rail Lance are `starter = no`, and no Discovery target opens them, so their kill rows need the Tree or another row to open them | the pool gating spec |
