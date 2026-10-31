# S1 validation kit — core slice (O-008)

Exit test (`12_Production §2`, S1): **3 validators × 2 runs**, none of them the owner; ≥ 1 on a gamepad, ≥ 1 in zh-Hans. The scheduler prepares the build; the owner hosts and watches. **Watch, don't explain.**

## Before each tester
- Tell the scheduler "S1 session starting": automated runs stop (holds in `STATE.md`) so frame times are clean.
- Build: the package the scheduler gives, a folder from `Tools\Package.ps1` (`Saved\Package\Shipping\Windows`, Shipping, en + zh-Hans). Copy that folder to the PC; no editor or engine is needed. If the game reports a missing runtime, run `Engine\Extras\Redist\en-usc_redist.x64.exe` from it once.
- Launch: double-click `Play_en.bat` or `Play_zh-Hans.bat` (full-screen arena, director on, bot off, a fresh seed each launch). Pad: plug it in before launch; keyboard/mouse and pad both work at any time. The session must have one tester on a pad: `pad` in the gate only passes from that run.
- Between runs: after death or victory the screen offers Restart run and Quit to desktop, keyboard or pad; a restart takes a fresh seed and writes new run files. Pause (`Esc`, pad Menu) offers the same plus Abandon run, each with a confirmation. Alt+F4 mid-run is safe too; the run file closes with reason `quit`.
- Have this sheet and a clock. Note the run timer (HUD) at each key moment.

## The one sentence (say only this)
> "You are the bullet hell. Survive fifteen minutes and beat the boss; every level you pick one card. I won't help — think out loud if you like."
>
> 「弹幕即你。撑过十五分钟、击败首领；每升一级选一张卡。我不会提示——想到什么可以说出来。」

Do not explain Keepers, archetypes, flags, Intercession or the card kinds.

## Observe (one column per tester; two runs each)
| | Tester 1 | Tester 2 | Tester 3 |
|---|---|---|---|
| Experience (novice / genre veteran), input (KBM / pad), language | | | |
| Patron chosen, and why (their words) | | | |
| Second Keeper at L8, and why | | | |
| First dash (time); dashed under pressure without prompting? | | | |
| First Intercession (time); did they find the input alone? | | | |
| Moments: laughed, swore, leaned in, went quiet or bored (time → words) | | | |
| Anything unreadable: telegraphs, enemy fire, cards, HUD, text | | | |
| Run outcome per run: death time + cause, or boss win | | | |
| Bugs, crashes, stutters (time) | | | |
| **Asked for another run unprompted?** (Y/N) | | | |

## Ask after each tester (in this order, record exact words)
1. "When did your build click?" — do not say "archetype", "流派" or a Keeper name first.
2. "What would you call your build?" → **can they name their archetype in their own words?** (Y/N; the scheduler maps the words to `Archetype.*`).
3. "Was there a card you were glad to see?"
4. "Did anything feel tiring or unfair?"
5. "What would make you play again?"

## After the session
- Hand over: double-click `ExportTelemetry.bat`. It makes `Dusktracer_Telemetry_<time>.zip` on the desktop (one CSV + JSONL per run, from `%LOCALAPPDATA%\Dusktracer\Saved\Playtests`, and a `MANIFEST.txt`) and prints the path. Nothing leaves the PC by itself: you carry the zip. Send the scheduler the zip and this sheet; nothing else is needed: per-minute damage taken, dashes, decisions and the damage split by source are in the logs.
- What the zip holds: gameplay only (seed, build hash, culture, last input device, combat numbers, cards offered and picked, Keeper and weapon choices, the local start time). No name, account, machine name, path or hardware. This build writes no engine log; `-IncludeLogs` adds log and crash folders if any exist, and those can name the Windows user, so it is off by default.
- Crash: note the time on the sheet. The run's file has no `run_end`; the seed in its file name replays the start on the proof build.
- Sessions on one PC share the folder: the zip holds every run so far, so tell runs apart by the start time noted on this sheet.
- The scheduler fills `Harness/templates/validation_note.md` as `Docs/Validation/S1_session.md` and triages every finding (class, layer, owner).

## Pass rule (written before the sessions; `12_Production §2`, `00_Vision §4`)
S1 exits when **all** hold:
1. 3 non-owner validators played 2 runs each (≥ 1 pad, ≥ 1 zh-Hans).
2. ≥ 2 of 3 name their archetype in their own words.
3. Core bands hold for steered play in minutes 10–13: damage taken per minute in `C.DMG_TAKEN.BAND_MIN10_13`, dashes per minute in `C.DASHES.BAND_MIN10_13` (from the logs; median across runs that reached minute 13).
4. Value split: Keeper share at L16 ≥ `C.POWER.KEEPER_SHARE_L16`, archetypes within `C.POWER.ARCHETYPE_BAND` (`Tools/power_split.py` on the session logs, cross-checked against the bot baseline).
5. The owner still wants to play after the session.
Then the shape constants lock (`08_Constants §1`). A fail routes findings through `Harness/H04`; it never adds content.
