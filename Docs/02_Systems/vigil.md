# Vigil — the hub screen (S3 row)

## Intent
A person's launch lands in a place, not a fight: a code-built page of labels where they pick a Patron, read what they have earned and start a run, and every run comes back to it. No NPCs, no town to walk, no asset. Nothing here reaches a tool run: the hub is a door in front of the sim, and with the door skipped everything plays as it did.

Written from `reference/original_docs/01_GameDesign.md §8.4`, `04_Narrative.md §8`, `Docs/03_Narrative.md` (§4, R-NAR-03, §7 replace points), `meta.md` and `settings.md`. Code says `Hub`, as `meta.md` R-MET-01 asks; the fiction names (Vigil, Shrine, Ledger-stone, Contracts) live in string tables and in this prose. `[invented]` marks what the source does not give; it is this spec's choice and the first thing to revisit.

## Rules
### The door
- **R-VIG-01** A map load opens the hub when the process has one (R-VIG-02) and the map came with no `DTSeed` option. The hub world is the same map with no run: `ADTGameMode` holds the sim before begin play, so no run starts and no step runs, no hero spawns, no telemetry session opens and no file is written. The whole screen is the hub. `[invented]` the same map: a second map is a binary asset for a page that draws nothing of it.
- **R-VIG-02** Tool runs skip the hub. A process that `meta.md` R-MET-14 calls a tool run (`-unattended`, automation, `-DTBot*`, `-DTNoSave`) starts its run as it did before the hub existed. `-DTHub` overrides this for `Tools/VigilCheck.ps1` alone; nothing else passes it. R-MET-16 holds: the hub adds no reader of the save to any sim system, and the map options of R-VIG-07 are absent from every tool run, so input hash, steps and CSV of a seed do not move.

### The page
- **R-VIG-03** Root rows, in order: Shrine, Contracts, Codex, Ledger-stone, Settings, Begin, Quit to desktop. `[invented]` the order and the Quit row: the source lists pages, and a hub with no way out is a dead end. The root opens on Begin when a Patron is chosen, else on the Shrine.
- **R-VIG-04** Navigation is `settings.md` R-SET-17 with the same fixed keys: up and down move focus and wrap, Accept acts, Back returns one level and lands on the row that opened the page. Back at the root moves focus to Quit and does nothing else, so a press never leaves the game. Left and right page a long list by one window. There is no mouse (`settings.md` Out of scope).

### The Shrine
- **R-VIG-05** Every Keeper is a row, in Path order. A Keeper is open when it is a run's Keeper (`keepers.md` R-KEE-23), or `Keeper.<Path>` is in the profile's `Unlocked`, or this launch's pool holds it (`-DTUnlock`). The others are sealed: greyed, marked `Hub.Shrine.Locked`, and on focus the detail line is `Hub.Shrine.LockedHint`. Accept on a sealed Keeper does nothing. An open Keeper's detail is its `<Path>.Desc`.
- **R-VIG-06** `LastPatron` (`meta.md` R-MET-05) is the chosen Patron when the root opens, unless it is sealed. The Shrine opens on the chosen Keeper. The hub never writes it: the bank does, at the end of the run that used it.
- **R-VIG-07** Begin with no Patron opens the Shrine. With one, the run starts by a map reload with the options `DTSeed`, `DTPatron` and, for a saved Keeper unlock, `DTUnlock`. The seed is `-DTSeed` when given, else a fresh one as R-SET-26 says. The Patron hand still opens and is still picked, through the same `Pick` as any card, before the first step, so its telemetry and its Rifle (D-015) are the hand's; the player sees no hand. The key that chose Begin must come up first (R-SET-23). `[invented]` the options as the channel: a reload is the clean reset R-SET-24 asks for.

### The Codex
- **R-VIG-08** The page lists `ST_Codex` by `Layer`, then key (`03_Narrative` §4). A row deeper than the profile's reach is not listed: reach is `C.NARR.PREWIN_LAYER_MAX` before the first win and `C.NARR.LAYER_MAX` after. `Codex.EntryTitle` is a format and no entry. A listed row not unlocked shows `Codex.Locked` as its line and nothing of its title or text. An unlocked row's title comes from its subject (a Keeper's name, an enemy's name, the Discovery's name); `Codex.Root.Title` is its own title and has no body. The header is `Codex.Count`.
- **R-VIG-09** `Unlock` is read against the profile, and an unknown cell is locked:

| Cell | Open when |
|---|---|
| `Start` | always |
| `Win` | `Wins` above zero |
| `Wins3.FormIII` | `Wins` above `C.BOSS.FORM_III_WINS`: the kill is one win past the wins that open the form |
| `Kill.Boss` | `LifetimeStats.BossKills` above zero |
| `Kill.<Role>` | `LifetimeStats.Kills.Role.<Role>` above zero |
| `Discovery.<Id>` | `<Id>` or `Discovery.<Id>` in `Discoveries` |

### The rest
- **R-VIG-10** The Ledger-stone shows the balance, then lifetime Ichor earned, `Runs`, `Wins`, `DeathsSinceWin`, time survived as hours, minutes and seconds of sim time, kills, elites and boss kills. It reads only; focus does not move. `[invented]` the rows; the source names the page.
- **R-VIG-11** Settings is the existing screen (`settings.md`), opened over the hub. Back from it, and Cancel or Back from the Quit confirmation (R-SET-20), close it and land on the row that opened it. The press that closed it never reaches the hub.
- **R-VIG-12** Contracts is a slot: a page that shows rows from an `IDTHubTab`, built by the factory given to `DTHubTabs::SetTreeFactory`. The hub draws, moves focus and calls `Accept`; the tab owns its data and its writes. With no factory the page opens with `Hub.Tree.Empty`. This spec holds no Tree logic.

### Coming back
- **R-VIG-13** When the process has a hub, the run-over screen leads with `RunSummary.ReturnTo`, on focus, and the rows of R-SET-21 follow unchanged. It reloads the map with no options, which is R-VIG-01. The run is already banked and closed. `Restart run` is as it was (R-SET-23): a reload onto a fresh seed and a run, with the Patron hand, on the same screens. With no hub the screen has exactly the rows it had. `[invented]` the way back as a row, not an automatic reload: the summary line needs a moment.

### Limits
- **R-VIG-14** The hub reads the save and writes nothing: `UDTMetaSubsystem::ReadProfile` is its only way in. Hub files never hold a session.
- **R-VIG-15** Grey box on the HUD canvas, no binary UI asset (R-TEC-06). Colour comes only from `Pal.UI.*` tokens; a sealed or locked row says so in words as well as in grey, and focus is a frame. All text is tables, en and zh-Hans, and follows `settings.md` R-SET-12.
- **R-VIG-16** The root, the Shrine and the Ledger-stone fit without scrolling at `C.SETTINGS.TEXT_MAX_PCT`; the Codex scrolls with focus.
- **R-VIG-17** The whole door is proved on real launches, off-screen, with real key events of both devices: `Tools/VigilCheck.ps1`, gate step `vigil`.

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Layers before and after the first win | C.NARR.PREWIN_LAYER_MAX, C.NARR.LAYER_MAX | CORE | R-VIG-08 |
| Wins that open Form III | C.BOSS.FORM_III_WINS | GATED(S2) | R-VIG-09 |
| Largest text | C.SETTINGS.TEXT_MAX_PCT | CORE | R-VIG-16 |
| Run-over guard | C.SETTINGS.RUN_OVER_GUARD_S | GATED(S3) | R-VIG-13 |

Layout sizes are reference pixels in `DTHubState.cpp`, presentation as in the menu; nothing here is balance.

## Data
| Field | Type | Notes |
|---|---|---|
| `FDTHubState` | struct | page, focus, scroll, Patrons, the choice; pure, driven by commands |
| `DTHubData` | functions | profile and tables to rows; `IsUnlocked`, `BuildCodex`, `Patrons`, `Stats`, `RunOptions` |
| `DTHubFlow` | functions | whether a map load is the hub; no save or settings type in its header |
| `IDTHubTab`, `DTHubTabs` | interface | R-VIG-12 |
| `UDTHubSubsystem` | world subsystem | runs the hub: keys, pages, the start of a run |
| `UDTSimSubsystem::HoldForHub` | call | R-VIG-01 |
| `DTPatron`, `DTUnlock` | map options | R-VIG-07 |
| `-DTHub`, `-DTHubCheck=<First or Reboot>`, `-DTHubShots=<dir>` | command line | proofs; the last two not in Shipping |

## Acceptance
- T-VIG-01 `Dusktracer.Hub.Boot`: the rule's four cases; `Tools/VigilCheck.ps1` reads a world with no run, no step and no pawn on a real boot.
- T-VIG-02 `Dusktracer.Hub.ToolRuns`: a tool run skips the hub; this very run has none; the run-over rows are as they were. The gate's `restart` step reads the seed's `input_hash` and steps, and `S3_vigil.md` holds them against `dev/s3-base`.
- T-VIG-03 `Dusktracer.Hub.RootRows`: the rows in order, and what each opens or asks for.
- T-VIG-04 `Dusktracer.Hub.Navigation`: focus wraps; Back lands on the opening row from every page and moves to Quit at the root; a long page scrolls and pages.
- T-VIG-05 `Dusktracer.Hub.Shrine`: open and sealed rows by rule, Accept on a sealed row, the hint, both ways a sealed Keeper opens.
- T-VIG-06 `Dusktracer.Hub.LastPatron`: names read back, the root opens on it, the Shrine on it, a sealed one is not chosen. `Tools/VigilCheck.ps1` proves it across two processes.
- T-VIG-07 `Dusktracer.Hub.Begin`: no Patron opens the Shrine; the pick lands on Begin; the options, with only Keeper keys travelling. `Tools/VigilCheck.ps1` plays a run on a Patron that is not the first card, under auto-pick.
- T-VIG-08 `Dusktracer.Hub.CodexRows`: sort, reach, a locked row shows nothing of itself, every row of the table reads with a title and text in the running culture.
- T-VIG-09 `Dusktracer.Hub.Unlock`: every cell of the table, each side of its line, and an unknown cell.
- T-VIG-10 `Dusktracer.Hub.Stats`: each row's value and label.
- T-VIG-11 `Dusktracer.Hub.Settings`: Back from Settings and from the Quit confirmation closes; Controls still backs to Settings; the confirm row quits.
- T-VIG-12 `Dusktracer.Hub.TreeSlot`: no factory, an empty page; with one, the tab is built and Accept reaches it with the row.
- T-VIG-13 `Dusktracer.Hub.RunEnd`: the rows, the way back first; Restart and Quit answer as before; Back still does nothing.
- T-VIG-14 `Dusktracer.Hub.ReadOnly`: no Hub file names a session or a write.
- T-VIG-15 `Dusktracer.Hub.TextKeys`: every key resolves; the screen holds tokens and no colour of its own.
- T-VIG-16 `Dusktracer.Hub.Layout`: the rows fit at the largest text.
- T-VIG-17 `Tools/VigilCheck.ps1`: en and zh-Hans, boot through every page, the Shrine, a run, Abandon and back, then a second process on the same save, with shots of every page.
- HUMAN: a pad in the hand; the largest text on real screens; whether a sealed Keeper's hint and a book of locked lines read as promise or as a wall; whether Begin feels like leaving town.

## Telemetry
None. The hub has no run, so no record. A started run reads as any run, and its Patron pick is the hand's usual record, with a very short decide time.

## Text
`ST_World`: `World.Vigil`, `Hub.Shrine`, `Hub.Contracts`, `Hub.Codex`, `Hub.LedgerStone`, `Hub.Settings`, `Hub.Begin`, `Hub.Shrine.Prompt`, `Hub.Shrine.Locked`, `Hub.Shrine.LockedHint`. `ST_UI`: `Hub.Balance`, `Hub.Begin.NeedPatron`, `Hub.Tree.Empty`, `Hub.Hint.Read`, `Hub.Stats.*`, `RunSummary.Time`, `RunSummary.ReturnTo`, `Codex.Count`, `Codex.Locked`. Source wording is kept where it exists; every invented zh cell is marked in Context and listed in `Docs/Validation/S3_vigil.md`.

## Out of scope
- NPCs, portraits, a town to walk, ambient art.
- The Tree's nodes and costs; the return-after-death line (`Hub.Recovered`, GDD §8.6); Quota, Descent and Rig rows on the Shrine; the Codex's marker for a new line; the run summary.
- Mouse on these pages.
- Saving a Patron chosen but not played.

## Open
| Question | Data that resolves it |
|---|---|
| Which unlock opens Keepers 4 to 6? `Keeper.<Path>` is read here and no node or Discovery writes it | a new Discovery or Tree row, by decision (`meta.md` Open) |
| Should Restart keep the chosen Patron rather than open the hand, and carry `DTUnlock`? | a human reading; the brief kept it as it was |
| Should the run-over screen return by itself after a time? | a human reading |
