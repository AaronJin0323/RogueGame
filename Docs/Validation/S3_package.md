# S3 in the package: a person's path

Branch `dev/s3-realpath`, from `dev/s3` at `c6f02d2`. Package: `Tools/Package.ps1` at `1a848be`, clean tree, Shipping and Development, 961 MB. Check: `Tools/PackageHubCheck.ps1` (`02_Systems/package.md` R-PKG-13, T-PKG-13), `-RenderOffScreen -nosplash`, 1280x720, no window, no `Play.bat`, no launcher. A machine reading, not a validator session.

Before this, `Tools/VigilCheck.ps1` proved the hub in the editor, on a tool run forced through `-DTHub`. No launch had gone down the person's branch of `IsToolRun`, and nothing of S3 had been read in a package.

## Result

`PKGHUB: PASS (same cook=True, 13 tables read in en and zh-Hans, Shipping csv same as the walk=True, seed 9107 hash 276faf8e, owner's save unchanged=True, windows 0)`, 301 s.

The path works in the package. No cook, staging, text or save failure was found. The walk found one real failure, in the seed (below); it is fixed. Shipping could not be walked, and what stands in for that is listed under Shipping.

## Which build proved what

| Claim | Build | Reading |
|---|---|---|
| Every table is staged | Both | 13 of 13 `DT/Text/Generated/ST_*.csv` in each `.pak`, by `UnrealPak -List`, with `Game.locres` for en and zh-Hans |
| Same cook in both builds | Both | all 4 `.utoc`/`.ucas` SHA-256 equal |
| Every table shows its CSV, in both languages | Development | 849 keys, 0 showing nothing; table below |
| A person's launch has no tool flag | Development, Shipping (D) | `Start-Game` refuses `-unattended`, `-nullrhi`, `-DTBot*`, `-DTHub`, `-DTNoSave`, `-DTMetaFile`, `-DTSettingsFile`, `-DTPoolGate`; each starts from the package's `Dusktracer.exe` |
| Boot lands in the hub | Development | no run, step or pawn; `Hub: open, balance 0, runs 0, last patron 'None'` |
| Every page opens, in both languages | Development | Contracts, Codex, Ledger-stone, Settings, Shrine: `PATHCHECK ...: ok`, shots read below |
| The Shrine's pick reaches the run | Development | en Storm, zh-Hans Forge; `run_start`, first `offer_pick` and the run all agree |
| A hand waits for a person | Development | `IsAutoPick` false; open a second past its guard; picked by Enter |
| A first run is gated | Development | `Offers: pool gated, 0 unlocked` in every world; 73 ids shut, none in a hand. Thin: 2 hands per run |
| A real death reaches the run-over screen | Development | en seed 9107, step 1512; zh-Hans step 1611; `run_end` and `run_award` read `death` |
| The way back leads | Development | `Return to Vigil` on focus, past the guard, Accept lands on Begin with no `DTSeed` on the map |
| The bank | Development | en `Meta: banked 8` then 3, hub reads 8 then 11; zh-Hans 9 then 3, hub reads 9 then 12; each equals the run file's `run_award` |
| The save lands at the default path | Development | no `-DTMetaFile`: `<userdir>/Saved/SaveGames/DTMeta.sav`, 449 bytes |
| A second run | Development | keeps the Patron, takes a fresh seed, `run_end` `abandoned` |
| Quit to desktop | Development | exit code 0 on all 6 launches |
| The save outlives the process | Development | a new process reads 11 runs 2 Storm, and 12 runs 2 Forge, on Begin |
| Language switches live from the hub | Development | Settings, Right, Back: the hub redraws in the other language; `DTSettings.ini` holds it at the default path |
| No crash, no warning | Development | 0 lines of `: Warning: ` or `: Error: ` in 6 logs; no `Saved/Crashes` |
| A person's launch reads the default path | Shipping | bad file planted there, boot: `DTMeta.sav.bad.1` byte for byte, no new save. A tool run leaves the same file alone |
| The hub starts no run | Shipping | alive at 20 s, 0 run files. The runs in the next rows wrote theirs at 1.3 and 2.0 s; a check fails if that nears the wait |
| Shipping plays what the walk played | Shipping | same seed, first pool, no input: CSV byte-equal to the en walk's first run, SHA-256 `917786BA...`; JSONL 137 lines each, 4 differ, in wall fields only |
| Shipping's bank reads | Shipping, Development | Shipping banks 8; a person's launch of Development reads 8, 1 run, Storm |
| Shipping banks on a Development save | Shipping, Development | 12 runs 2, plus 8 in zh-Hans; Development reads 20, 3 runs, Storm; Ledger-stone shows 20 |
| The owner's folder | none touched | no `SaveGames`, no `DTSettings.ini`, 142 run files and none of this check's, before and after. On the pass with no other track running, its newest file stayed 2026-10-31 23:25:43 |

## What the walk found

**Two runs of one launch played one seed.** `Tools/PackageHubCheck.ps1`'s first walk read `Hub: begin DTSeed=9107?DTPatron=Forge` twice in one process. `Play_*.bat` gives one `-DTSeed` and R-VIG-07 used it for every Begin, so a person's second run replayed the first: same spawns, same offers for the same picks. R-PKG-04 exists to stop exactly that. Class BUG. Replaced: `-DTSeed` names a launch's first run, every later run takes a fresh seed. Reading after: `DTSeed=9107` then `DTSeed=378613971`. Launchers untouched. **This is the one change under `Source/Dusktracer/Hub`:** `BeginRun` in `DTHubSubsystem.cpp`, no page. It holds whatever the launchers end up passing; with no `-DTSeed` the hub already took a fresh seed.

## Traps a tool will meet

- **A temporary `LOCALAPPDATA` does not move a Shipping save.** The engine asks Windows (`SHGetKnownFolderPath`, `WindowsPlatformProcess.cpp`), not the environment, so it would have written into the owner's folder. `-userdir=<dir>` moves the save, the settings, the run files and the engine's `Saved/Config` together, and is what the check uses. Read in the engine source; deliberately not tried against the real folder.
- **Shipping ignores a map on its command line** (`UGameInstance::StartGameInstance`, `UE_ALLOW_MAP_OVERRIDE_IN_SHIPPING`), so no `?DTSeed=...?DTPatron=...` can start a person's run there. The `$Map` that `PadCheck` and `PackageRestartCheck` pass to Shipping does nothing; `-DTSeed` is what they rely on.
- `Hub: open` logs an empty `LastPatron` as `None`. An abandoned run's `run_award` reads `end` `death`, as R-MET-05 counts it; `run_end` reads `abandoned`.
- A dirty Development build stamps `build` `<hash>+dirty`, which breaks the byte compare against a clean Shipping. Only a clean package reads E.

## Shipping

Shipping has no log and no `-DTPathCheck`, an off-screen run has no window for a key, and a map cannot be named. So nothing leaves its hub, and no hook was added: R-PKG-02 keeps them out. What stands in:

| Launch | Flags | Proves |
|---|---|---|
| D boot | a person's: `-culture=en -DTSeed=9107` | the person's branch, the default path, no run |
| E run | `-unattended -DTPoolGate -DTMetaFile=<default path> -culture=en -DTSeed=9107` | play and bank, equal to the walk; a tool run by necessity |
| G run | as E, zh-Hans, on the zh-Hans walk's save | Shipping reads a Development save and adds to it |

The Development source differs from Shipping's by `#if !UE_BUILD_SHIPPING` blocks only, the cook is the same, and the run the two played was the same. That is a bridge, not the act.

## Text in the package

Keys and rows differing from en, as shown by the running game; each equals its CSV. en shows 0 localized in every table.

| Table | Keys | zh-Hans differs |
|---|---|---|
| ST_UI | 158 | 152 |
| ST_Weapons | 20 | 20 |
| ST_Modifiers | 32 | 32 |
| ST_Keepers | 300 | 300 |
| ST_Keywords | 52 | 52 |
| ST_Reactions | 62 | 62 |
| ST_Enemies | 14 | 13 |
| ST_Reforging | 32 | 32 |
| ST_Codex | 31 | 30 |
| ST_KeeperLines | 42 | 36 |
| ST_World | 45 | 45 |
| ST_Discoveries | 32 | 32 |
| ST_Tree | 29 | 29 |

The 13 tables were registered by hand one at a time, and the gather follows that macro, so a table left out would have no zh-Hans and no test would say so. `Tools/loc_build.py --check` now fails one (`Story` in `Tools/tools_test.py`), in the gate at no cost.

## The shots

`Saved/PackageHub/<culture>/shots/<page>.png`, 40 in all, not committed; each passes `Tools/image_check.py`. Read one by one: en `root_fresh`, `run`, `hand`, `runover_death`, `ledger_banked`, `codex`, `contracts`, `root_switched`; zh-Hans `hand`, `shrine`, `codex`, `settings`, `runover_death`, `ledger_banked`, and the Ledger-stone after Shipping's bank. Nothing is missing, no box glyph, no raw key. What a reader may want to weigh: the card screen's keyword note sits over the HP label in zh-Hans (`hand`, a hand's own layout). Hub pages match `S3_vigil.md`, which read them in the editor.

## Tool runs did not move

| Reading, seed 9107 | `dev/s3` (`c6f02d2`), built before any edit | `dev/s3-realpath` (`1a848be`) |
|---|---|---|
| Editor, `Tools/RestartCheck.ps1`, `input_hash` | 276faf8e | 276faf8e |
| Editor, steps | 5400 | 5400 |
| Package, Development, bot, 90 s | not read | 276faf8e |
| Package, Shipping, bot, 90 s | not read | 276faf8e |

## Readings worth keeping

- A Reclaimer who never moves is dead at 24.6, 25.2 and 26.9 s in this pass, banking 8 or 9 Ichor; earlier passes read 26.4, 27.2 and 49.3 s, the last banking 19. One run per seed, no bot, no human: not a tuning note, and no number was touched.
- `LoadMap` on a hub path, boot and reloads: 0.017 to 0.341 s. Whole check: 301 s, 146 of them the two walks.

## Decisions

- `-userdir`, not a temporary `LOCALAPPDATA`, isolates a launch (R-PKG-13).
- `-DTSeed` names a launch's first run (R-VIG-07, R-PKG-04).
- The driver is `Source/Dusktracer/Settings/DTPathCheck.*`, ticked from the menu like `DTRestartCheck`, so nothing of it sits under `Hub`, where the Contracts track works. It reads the save through log lines and includes no `Meta/` header.
- Not a gate step: 301 s on top of a package the gate does not build, so it would read `NOT RUN` on nearly every branch. A hand check after `Tools/Package.ps1`, named in R-PKG-13, as `PackageRestartCheck` is.
- No hook in Shipping. It stays a bridge.
- A walk stands still. Real keys drive the pages and hands; the hero never moves, so the run is seeded and comparable.

## Not proved

- Shipping past its hub: a key on any hub page, Begin, a person's run, the bank by a person, the way back. Needs real hands (`package.md` HUMAN).
- That the hub draws in Shipping: D reads no run, which is not a frame.
- A real key through the operating system, a real window, Fullscreen, a real pad: the proof sends key events into the player controller.
- `Play_en.bat` and `Play_zh-Hans.bat` themselves: each opens a full-screen window, so they were not launched; their arguments were copied with `-fullscreen` swapped for off-screen. `-DTSeed=%RANDOM%%RANDOM%` overflows to 2147483647, the owner's run's seed; the seed agent has that, and launchers were not touched.
- Shipping's real save under `%LOCALAPPDATA%`: never aimed there, on purpose.
- A win through the hub, a Discovery on a person's save, a Codex row opening by play: a hero who stands still reaches none.
- Buying anything: the Contracts page has no factory on this base.
- The gated pool in depth: 2 hands a run. `Dusktracer.Meta.FirstRunPool` is the proof of the list; this is one real launch agreeing with it.
- Restart run from the run-over screen, and the other Settings rows, in the package.
- Editor against package on any seed but 9107.
