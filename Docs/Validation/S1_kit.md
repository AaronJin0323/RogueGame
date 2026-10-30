# S1 validation kit — core slice (O-008)

Exit test (`12_Production §2`, S1): **3 validators × 2 runs**, none of them the owner; ≥ 1 on a gamepad, ≥ 1 in zh-Hans. The scheduler prepares the build; the owner hosts and watches. **Watch, don't explain.**

## Before each tester
- Tell the scheduler "S1 session starting": automated runs stop (holds in `STATE.md`) so frame times are clean.
- Build: `main` at the hash the scheduler gives (gate green). Run `Tools\Build.bat` once.
- Launch: `Tools\Play.bat en` or `Tools\Play.bat zh-Hans` (full-screen arena, director on, bot off). Pad: plug it in before launch; keyboard/mouse and pad both work at any time.
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
- Send the scheduler: this sheet, and the whole `Saved\Playtests\` folder (one CSV + JSONL per run). Nothing else is needed: per-minute damage taken, dashes, decisions and the damage split by source are in the logs.
- The scheduler fills `Harness/templates/validation_note.md` as `Docs/Validation/S1_session.md` and triages every finding (class, layer, owner).

## Pass rule (written before the sessions; `12_Production §2`, `00_Vision §4`)
S1 exits when **all** hold:
1. 3 non-owner validators played 2 runs each (≥ 1 pad, ≥ 1 zh-Hans).
2. ≥ 2 of 3 name their archetype in their own words.
3. Core bands hold for steered play in minutes 10–13: damage taken per minute in `C.DMG_TAKEN.BAND_MIN10_13`, dashes per minute in `C.DASHES.BAND_MIN10_13` (from the logs; median across runs that reached minute 13).
4. Value split: Keeper share at L16 ≥ `C.POWER.KEEPER_SHARE_L16`, archetypes within `C.POWER.ARCHETYPE_BAND` (`Tools/power_split.py` on the session logs, cross-checked against the bot baseline).
5. The owner still wants to play after the session.
Then the shape constants lock (`08_Constants §1`). A fail routes findings through `Harness/H04`; it never adds content.
