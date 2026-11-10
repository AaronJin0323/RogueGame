# S1 validation kit — core slice (O-008)

Exit test (`12_Production §2`, S1; D-048): **3 validators × 2 runs, at each of two sessions**, none of them the owner; ≥ 1 on a gamepad, ≥ 1 in zh-Hans. This sheet serves both sessions; the pass rule at the end says which rows each one judges. The scheduler prepares the build; the owner hosts and watches. **Watch, don't explain.**

## Before each tester
- Tell the scheduler "S1 session starting": automated runs stop (holds in `STATE.md`) so frame times are clean.
- Build: the package the scheduler gives, a folder from `Tools\Package.ps1` (`Saved\Package\Shipping\Windows`, Shipping, en + zh-Hans). Copy that folder to the PC; no editor or engine is needed. If the game reports a missing runtime, run `Engine\Extras\Redist\en-usc_redist.x64.exe` from it once.
- Launch: double-click `Play_en.bat` or `Play_zh-Hans.bat` (full-screen arena, director on, bot off, a fresh seed each launch). Pad: plug it in before launch; keyboard/mouse and pad both work at any time. The session must have one tester on a pad: `pad` in the gate only passes from that run.
- Between runs: the game has no restart. After the death or victory screen press Alt+F4 (the host does it for a pad tester), then launch again. Alt+F4 mid-run is safe; the run file closes with reason `quit`.
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
| Hit by something they did not see coming, or from off screen (time → what; D-036 reverts on one such report) | | | |
| An elite: a fight or a wall (their words) | | | |
| Chests: did they go for one; did they notice health come back | | | |
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
- What the zip holds: gameplay only (seed, build hash, culture, last input device, combat numbers, hero positions on the arena, cards offered and picked, Keeper and weapon choices, the start time, UTC inside each file and local in its name, so the two show the time zone). No name, account, machine name, path or hardware. This build writes no engine log; `-IncludeLogs` adds log and crash folders if any exist, and those can name the Windows user, so it is off by default.
- Crash: note the time on the sheet. The run's file has no `run_end`; the seed in its file name replays the start on the proof build.
- Sessions on one PC share the folder: the zip holds every run so far, so tell runs apart by the start time noted on this sheet.
- The scheduler fills `Harness/templates/validation_note.md` as `Docs/Validation/S1_session_A.md`, then `S1_session_B.md`, each with a `Build:` line, and triages every finding (class, layer, owner).

## Pass rule (written before the sessions; D-048, `12_Production §2`, `00_Vision §4`)
The S1 exit is read in two sessions. The same three people may come back for the second.

**Session A** judges three rows:
1. 3 non-owner validators played 2 runs each (≥ 1 pad, ≥ 1 zh-Hans).
2. ≥ 2 of 3 name their build in their own words.
3. The owner still wants to play after the session.

Session A also reads, with no verdict: damage taken and dashes for every minute of every run, swarm kill time, pause seconds per minute, the L8 and L16 times, the pickup ratio, restores collected, the Keeper attribution of any run that reached L16, and the two new rows of the sheet above. "Asked for another run unprompted" is recorded and reported at both sessions; it gates the S3 exit (D-049).

**The gate decision** between the sessions sets the two pressure bands from A's readings (D-046, method in `10_Tuning.md §2`), names the bot skill row that brackets the validators, and keeps, changes or removes the chest restore.

**Session B** judges, on runs alive through `C.DIR.BAND_WINDOW_S` (the median over those runs; fewer than three such runs reads NOT READ):
4. Damage taken and dashes per minute land in the bands the gate decision wrote.
5. Swarm kill time is within `C.TTK.SWARM_STEERED_MAX`.
6. Hands pause the run for at most `C.OFFER.PAUSED_S_PER_MIN_MAX`, the Patron hand left out.

**The tool row**, read on each session's build hash and required at B:
7. Keeper share is at least `C.POWER.KEEPER_SHARE_L16` and the archetypes sit within `C.POWER.ARCHETYPE_BAND`, both by loss (`Tools/probe.py`, D-047, `10_Tuning.md §3`). `Tools/power_split.py` on the session logs is printed beside it and never judged.

S1 exits when rows 1 to 3 have held at either session and rows 4 to 7 hold at B. Then the shape constants lock (`08_Constants §1`). A fail routes findings through `Harness/H04`; it never adds content.
