# S3 validation kit — the external test

Exit test (`12_Production §2`, S3): **3 validators on fresh saves × 3 runs**, none of them the owner; a validator who played an earlier build may return, and the sheet records what they played (D-049); ≥ 1 on a gamepad and ≥ 1 in zh-Hans (`§1`); **passes when two of three hold**: ≥ 2 ask for another run unprompted, ≥ 2 name their build, the owner still wants to play (D-049); a packaged pad pass; `findings_S3.md` written. The scheduler prepares the build; the owner hosts and watches. **Watch, don't explain.**

No session has been played. D-049 settled the pass rule, which `Harness/H07 §4` wants written before the session: two of three hold (≥ 2 validators ask for another run unprompted, ≥ 2 name their build in their own words, the owner still wants to play), with its stop line. It also settled "fresh": a fresh save, and a validator who played an earlier build may return. Those are calls 1 and 2 in `findings_S3.md`. What counts as a run and how much play makes a gamepad or a zh-Hans validator still have no definition in the docs: settle call 3 in `findings_S3.md` first. Book one validator for a whole session in zh-Hans and one for a whole session on a pad: those read `PASS` however call 8 lands.

## What a validator gets
- A folder: the package from `Tools\Package.ps1`, `Saved\Package\Shipping\Windows`, Shipping, en + zh-Hans. Copy it to the PC; no editor or engine is needed. If the game reports a missing runtime, run `Engine\Extras\Redist\en-us\vc_redist.x64.exe` from it once.
- A PC or a Windows account of their own (next section), with a pad plugged in before launch if they are the pad validator.
- The one sentence, and nothing else. Never the Development folder: it has a console (`package.md` R-PKG-02).

The build must be clean: `Saved\Package\package_stamp.json` reads `"dirty": false`. Its `commit` is the kit's build. Write it in `findings_S3.md`.

## Before the first validator: the machine half of the pad row
`Packaged pad pass` needs `Tools\PadCheck.ps1` on this package as well as a real pad, and PadCheck judges only a package current with the tree. So the scheduler runs it right after `Tools\Package.ps1`, before the tree moves, and keeps its line in the session folder:
```
& Tools\PadCheck.ps1 -Keep <session folder>\PadCheck.txt
```
It must end `PADCHECK: NOT RUN (package <hash> ok, ...`. That is the good line: NOT RUN there says only that no real pad has played on this account. `FAIL` or any other `NOT RUN` means the package is not ready, and no validator gets it.

No window opens. Its launches stay under `Saved\PadCheck`, so it writes nothing to `%LOCALAPPDATA%\Dusktracer` and is safe on the owner's account. `S3Exit` finds `PadCheck.txt` in the session folder by itself. A later run that finds the package stale or gone leaves the kept line alone; a later run that judges the same package replaces it.

If the line is lost, check out the build's commit in a clean worktree and run `Tools\Package.ps1` and PadCheck there. Without a line the row stays NOT READ, and `Saved\Logs\Gate_pad.log` will not do: every gate overwrites it.

## A fresh save for each validator
The save and the run files belong to the Windows account, not to the folder: `%LOCALAPPDATA%\Dusktracer\Saved\SaveGames\DTMeta.sav` and `Saved\Playtests` beside it. Copying the package again does not make a new save.

So each validator plays on one of these:
1. Their own PC, which has never run the game.
2. A new Windows account made for them on a shared PC, one per validator.

**Never on the owner's Windows account.** The owner's run files are there, and `Export_Logs.bat` would put every one of them in the validator's zip. **Never delete, rename or move `%LOCALAPPDATA%\Dusktracer` to make a save fresh:** those files are the owner's evidence, and they do not come back.

A rehearsal by the host goes on a spare account of its own, and its zip stays out of the session folder: a rehearsal on the kit's build reads exactly like a validator's run, and no tool can tell them apart.

## Telling a validator's save from the owner's
The logs name no person and no save (`meta.md` R-MET-16), so a save is told by looking, before the first run:

| Look at | A fresh save | Anything else |
|---|---|---|
| The first screen | Focus rests on Shrine 祠堂: no Patron chosen | Focus rests on Begin 启程: this save has played. Stop |
| Ledger-stone 账碑 | Every row 0: Ichor held, Ichor earned, Runs, Wins | Any other number. Stop |
| Shrine | Storm, Forge, Tide open, three Keepers Sealed 封存 | A fourth Keeper open. Stop |

Write the Ledger-stone reading in `findings_S3.md` before the first run. A save that reads clean can still sit on the owner's account, so the account is checked first.

After the session the zip checks it again:
- It holds that validator's runs and no others. More files than runs played means the account was shared.
- `S3Exit` refuses any run in two validators' zips (`under two validators`): they shared a folder, so they shared a save.
- `S3Exit` reads `Fresh` as FAIL when a validator's first run showed a card no fresh save of that build can hold. It only works one way: a clean hand proves nothing.

The owner's own runs read differently too: `Tools\Play.bat` runs say `config` `Development`, and anything from before this kit has no `driver`. Both are refused.

## Before each validator
- Tell the scheduler "S3 session starting": automated runs stop (holds in `STATE.md`), so frame times are clean.
- Do the checks above. Have the sheet and a clock. Note the run timer (HUD) at each key moment.

## The one sentence (say only this)
> "You are the bullet hell. Survive fifteen minutes and beat the boss; every level you pick one card. I won't help — think out loud if you like."
>
> 「弹幕即你。撑过十五分钟、击败首领；每升一级选一张卡。我不会提示——想到什么可以说出来。」

Do not explain Vigil, Keepers, Contracts, Ichor, archetypes or the card kinds.

## What a validator does
1. Double-click `Play_en.bat` or `Play_zh-Hans.bat`. The game opens full screen in **Vigil 守夜镇**, the hub, not in a fight. A fresh seed is drawn for every run.
2. The hub is a list: Shrine, Contracts, Codex, Ledger-stone, Settings, Begin, Quit to desktop. **It takes keys, not the mouse:** arrows or `W S` with `Enter` and `Esc`; on a pad the D-pad or left stick with `A` and `B`. Someone clicking and getting nothing is a finding: note it. If they are stuck, say only "arrow keys and Enter".
3. At the Shrine they choose a Patron, then Begin starts a run.
4. A run ends in a death or a win. The run-over screen shows what the run earned, in Ichor, and offers Return to Vigil, Restart run and Quit to desktop.
5. Back in Vigil, Contracts spends Ichor. A purchase takes one press and cannot be undone. Three runs, in whatever way they find.

Pause (`Esc`, pad Menu) offers Restart run, Abandon run and Quit to desktop, each with a confirmation. A run is banked once however the game ends it, so no exit loses what it earned; a crash does.

**Language.** The launcher sets the language at launch: `Play_zh-Hans.bat` for the zh-Hans validator, for every run. Settings 设置 → Language 语言 switches it live, and each run records only the language it started in, so a switch in mid-run is never read. Any validator may change it, but `zh-Hans` reads `PASS` only on a validator with ≥ 3 runs ended by a death or a win, all started in zh-Hans. A mix reads NOT READ until call 8 is made.

**Pad.** The pad validator plugs the pad in before launch and uses it for every run. Each CSV row reads the last device at that interval's end, and a moved mouse or a pressed key reads `kbm`. `Packaged pad pass` takes only a validator with ≥ 3 runs ended by a death or a win, each reading `pad` in some interval and `kbm` in none; anything less reads NOT READ until call 8 is made, and the row names who played what.

**Between sittings.** The save keeps the Ichor, the Contracts and the Patron. Start the same launcher on the same account. Never a new account for the same validator: that is a second save.

## Observe (one column per validator; three runs each)
| | V1 | V2 | V3 |
|---|---|---|---|
| Experience (novice / genre veteran), input (KBM / pad), language | | | |
| PC or account; Ledger-stone read 0 before the first run | | | |
| Found Begin alone? How long in Vigil before the first run? | | | |
| Patron chosen, and why (their words) | | | |
| Second Keeper at L8, and why | | | |
| First dash and first Intercession (time); did they find the input alone? | | | |
| Moments: laughed, swore, leaned in, went quiet or bored (time → words) | | | |
| Anything unreadable: telegraphs, enemy fire, cards, HUD, hub, text | | | |
| Run outcome per run: death time + cause, or boss win | | | |
| Bugs, crashes, stutters (time) | | | |
| What they looked at in Contracts and the Codex; what they bought | | | |
| At the run-over screen: a key that did nothing, or a remark that it was slow to respond (run → their words) | | | |
| **Asked for another run unprompted?** (Y/N) | | | |

## Ask after each validator (in this order, record exact words)
1. "When did your build click?" — do not say "archetype", "流派" or a Keeper name first.
2. "What would you call your build?" → **can they name their archetype in their own words?** (Y/N).
3. "Was there a card you were glad to see?"
4. "Did your first run feel like the whole game, or a locked part of it?" (D-024)
5. "Was there anything you wanted to buy?" (D-023)
6. "Did anything feel tiring or unfair?"
7. "What would make you play again?"
8. Only if they bought a blessing node: "After you bought it, did your next run feel stronger, weaker or just different?"
   「你买了那个之后，下一局感觉更强、更弱，还是只是不一样？」
9. "When a run ended, did the screen respond when you wanted it to?" (R-SET-21)
   「一局结束的时候，结算界面在你想按的时候有反应吗？」

The zh wording is a first draft; the host may reword it and writes down what was asked.

## What they send back
With the game closed, the validator double-clicks `Export_Logs.bat` in the package folder. It makes one file on the desktop, `Dusktracer_Logs_<date>_<time>.zip`, and prints where. Nothing leaves the PC by itself: they hand that one file to the host. A second export is fine: it holds every run so far.

What the zip holds: gameplay only. Seed, build hash and build type, who drove the run (a person, a tool or a bot), culture, last input device, combat numbers, where the hero was every few seconds and where each hit on the hero came from, cards offered and picked, Keeper and weapon choices, how long each card took to pick and their total per run, and the start time, UTC inside each file and local in its name, so the two show the time zone. No name, account, machine name, path or hardware. Shipping writes no game log, so a zip holds run files and a `MANIFEST.txt` saying this. The save and the settings file stay on the PC.

Crash: note the time. The run's file has no `run_end`.

## What the host reads
Put each zip in a folder named for its validator, and never a real name: the repo is public (D-006).
```
<session folder>\PadCheck.txt
<session folder>\V1\Dusktracer_Logs_....zip
<session folder>\V2\Dusktracer_Logs_....zip
<session folder>\V3\Dusktracer_Logs_....zip
```
```
& Tools\S3Exit.ps1 -Logs <session folder> [-Build <hash>]
```
It starts no game and writes nothing. It prints one line per validator, the runs it refused, one `EXIT` line per row, then `S3EXIT: <p> PASS, <f> FAIL, <n> NOT READ`. With no `-Build` it takes the clean commit in `package_stamp.json`; with no build named, no run counts. Name `-Build` once a later package has replaced that stamp. It reads the first-run list from that commit, not from the checkout, so run it in a clone that holds the commit.

**A run counts only if its `run_start` says a person played the kit's stamped Shipping build.** Every other run is refused, and the line says why:

| Line | What it means |
|---|---|
| `bot run` | `driver` is `bot`, or the run ended `bot_timeout` |
| `tool run` | `driver` is `tool`: `-unattended`, a named seed or any other `-DT` switch |
| `designer's build` | `config` is not `Shipping`: `Tools\Play.bat` or the proof build |
| `unstamped build` | `build` is `unknown` or ends `+dirty` |
| `another build` | a clean Shipping build, not the one named |
| `no build named to read it against` | pass `-Build` |
| `older than driver and config` | a file from before R-TEL-13 |
| `no run_start` | a file torn at its start, or not a run file |
| `under two validators` | the same run in two zips: it counts for neither |

Of the accepted runs, one ended by a death or a win is **counted**. `quit`, `restart`, `abandoned` and a file with no `run_end` (a crash or a killed process) are shown beside them and do not count (call 3).

| `EXIT` row | Reads | PASS | FAIL | NOT READ |
|---|---|---|---|---|
| `Validators` | the logs | ≥ 3 folders with ≥ 3 counted runs | some accepted runs, and fewer than 3 folders with ≥ 3 of them | no accepted run, or only the runs they left decide it: call 3 |
| `Fresh` | a human | never: only the host knows | the first run of a folder shows a card no fresh save of that build can | otherwise, and whenever a refused or torn file in that folder could be older |
| `zh-Hans` | the logs | a validator with ≥ 3 counted runs, all started in zh-Hans | `Validators` passed and no accepted run in zh-Hans | otherwise: call 8 |
| `Packaged pad pass` | the logs and `Tools\PadCheck.ps1` | a validator with ≥ 3 counted runs, each reading `pad` and no interval `kbm`, and PadCheck's line passes this package | `Validators` passed and no accepted run reads `pad` | otherwise: call 8, or the line is missing or names another package |
| `Pass rule` | a human, from `findings_S3.md` (D-049) | never here | never here | always: a person reads whether two of the three hold (another run asked for by ≥ 2, a build named by ≥ 2, the owner still wants to play) |
| `findings_S3.md` | the file | every row read, each with a name | never | otherwise; `NOT READ` with a reason behind it, in any case, is no reading |

**A verdict on an open call** comes only where every reading of the call agrees: `PASS` on its strict side, `FAIL` where even its loosest reading misses, and `NOT READ` between.

**NOT READ is not a soft pass.** It means no human evidence reached this row. A row that needs a human stays NOT READ here whatever the logs say, and no number from a bot stands in for one (R-BOT-13). `S3Exit` cannot say the stage is over: a person reads the output and fills `findings_S3.md`, with their name in each row.

Then triage every finding (class, layer, owner) in `findings_S3.md`.

## Not proved by this kit
- **Nobody has played Shipping past its hub.** A real key from Vigil through a run, the bank by a person, Alt+F4 in full screen, `Export_Logs.bat` by double-click, the look of zh-Hans cards on a real display: all are `package.md` HUMAN rows, so the first session is the first time. A rehearsal on a spare account finds these early.
- **`driver` `person` on Shipping.** Shipping cannot reach a run with no hands, so a launcher's arguments read `person` on the editor build only. Read the first zip the day it comes back: a wrong reading here would refuse every run, and it costs a session to find out late.
- A real pad through `S3Exit`, and a first hand from a real fresh save through `Fresh`: both are held on fixtures only.
- The real path of the save and the run files under `%LOCALAPPDATA%` on Shipping: no check aims there, on purpose (`S3_package.md`). `PadCheck` last read a run there on package `53d3547` (`S3_logs.md`), before it took a `-userdir`, so the first zip is the next reading. The Ledger-stone is what the host trusts.
- That a person reads this sheet as written: nobody has hosted from it.
- **Fresh against full.** The kit plays fresh saves only, so it cannot say whether opening the pool makes a run stronger or weaker for a person (the bot won 69 of 90 on the first-run pool against 47 on the full pool, `S3_tree.md`). That is read in the owner's rehearsal of the kit, one run on a fresh save and one on a full profile, with the owner's word on which felt stronger.

## Readings of the kit
Class **SYSTEM**, layer the measuring tools. It replaces `S1_kit.md` as the kit the package's `README.txt` names on S3 builds, sorting runs by a noted start time with a zip per validator, and `PadCheck`'s shared folder and PID bookkeeping with a `-userdir` of its own. No rule of play moved and no number was tuned.

We believe a host can read the S3 exit from three zips with no run of a bot or a designer's build counted, because `run_start` now says what made each run (`telemetry.md` R-TEL-13); measured at the first S3 session; revisited if a real run is refused or a tool's run is accepted.

Machine readings, none of a person. No session was simulated, and no exit row read `PASS` from any of them. Every launch was off-screen; Shipping ran on a `-userdir` under this worktree's `Saved`.

| Reading | Tool | Result |
|---|---|---|
| Each launcher's real arguments, and a bare launch | `Tools/SeedCheck.ps1`, editor build | `driver` `person` on all 4; `tool` on all 4 named or unattended launches. `Tools\Play.bat` reads `person` too, which is why `config` has to refuse it |
| The same on the package | `Tools/SeedCheck.ps1 -Package`, package of `d978116`, through PadCheck | `SEEDCHECK: PASS (3 human launches on fresh seeds, 2 exact, 2 rejected)`: `person` on both `Play_*.bat` and a bare launch on Development, `tool` on a named seed on both builds |
| The machine half of the pad row | `Tools/PadCheck.ps1 -Keep`, package of `d978116`, 195 s, every launch on a `-userdir` under `Saved\PadCheck` | `PADCHECK: NOT RUN (package d978116 ok, en+zh-Hans, 0 windows; real pad needs a human, ...)`: both cultures end `bot_timeout` on the stamped build, the export reads, both goldens pass. This shows the step works; the kit's own build gets its own line |
| That line and that check's real zip in a session folder | `S3Exit`, folder named with a trailing backslash | `REFUSED V1: 2 bot run`, `S3EXIT: 0 PASS, 0 FAIL, 6 NOT READ` |
| The owner's folder, across that package and that check | a listing of `%LOCALAPPDATA%\Dusktracer` before and after | 477 entries, none new, changed or gone |
| A bot | `Tools/BotRuns.ps1`, seed 9107, editor build | `driver` `bot`, `config` `Development` |
| A bot, zh-Hans | Shipping at `51d7249` | `config` `Shipping`, `driver` `bot`, `build` equal to the stamp, `run_end` `bot_timeout` |
| `-unattended`, no bot, killed | Shipping at `51d7249` | `config` `Shipping`, `driver` `tool`, `run_start` alone, no CSV |
| Those two through the package's own `Export_Logs.ps1`, then `S3Exit` | a real zip in a `V1` folder | `REFUSED V1: 1 bot run`, `REFUSED V1: 1 tool run`, `S3EXIT: 0 PASS, 0 FAIL, 6 NOT READ` |
| The same bot file from the editor in a `V1` folder | `S3Exit` | refused, `S3EXIT: 0 PASS, 0 FAIL, 6 NOT READ` |
| Seed 9107, `input_hash` and steps | `Tools/RestartCheck.ps1` | `276faf8e`, 5400 on the base `5ccf82e` rebuilt from its own `Source`, and the same on this branch: the sim did not move |
| The owner's folder | a listing before and after the Shipping launches | 307 files, none new, changed or gone |

What a real run file holds, field by field on a bot's JSONL: the kinds in `telemetry.md` §Data, no string but ids and `run_start.wall`, and no trace of the user name, the machine name or a path. `hero_damaged` did not fire in that run, so its fields are read from the code. D-026 had already made "hero positions" too narrow: a run also holds the point each hit came from and a round's line, and how long each card took. The manifest, `package.md` and `S1_kit.md` said neither, and now do.

Found while proving it:
- A real zip's member names use backslashes on Windows PowerShell 5.1. The reader takes either, and a test holds it.
- A run killed inside its first minute has no CSV, so its device is unread. `S3Exit` counts these as `no csv`.
- `Tools/PadCheck.ps1` launched Shipping with no `-userdir`, so it wrote to `%LOCALAPPDATA%\Dusktracer` and the kit could not ask for it on the owner's account (`S3_package.md`). Every launch now stays under `Saved\PadCheck`. `Tools/ExportLogsCheck.ps1` and `Tools/PackageRestartCheck.ps1` still launch Shipping with no `-userdir`, so neither was run for this kit.
- A review of the kit found three things only a host would have met. `zh-Hans` and `Packaged pad pass` read `PASS` on one run and one interval, which no doc holds: both now take the strict side of call 8. The kit never scheduled PadCheck, so that row's machine half had no way to be read after the tree moved: the step and its kept line are above. `Fresh` could read `FAIL` on a fresh save when a real first run was refused, or when the shut list had grown since the package: it now reads the build's own list, and only a run no file in the folder can predate.
- `S1_kit.md` still offered `-IncludeLogs`, which `Export_Logs.ps1` no longer has, and its redist path held a vertical tab.
