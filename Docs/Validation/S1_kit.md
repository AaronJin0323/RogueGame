# S1 validation kit — core slice (O-008)

Exit test (`12_Production §2`, S1; D-048): **3 validators × 2 runs, at each of two sessions**, none of them the owner; ≥ 1 on a gamepad, ≥ 1 in zh-Hans. This sheet serves both sessions; the pass rule at the end says which rows each one judges. The scheduler prepares the build; the owner hosts and watches, or hands this sheet to a host. **Watch, don't explain.**

## Before the session
- Announce it. The owner says "S1 session A starting" in the scheduler's session (Central Project Scheduler), and "S1 session A over" when the last tester has left. A host who is not the owner tells the owner. Automated runs on the development PC stop in between (holds in `STATE.md`).
- Build: the package the scheduler gives, a folder from `Tools\Package.ps1` (`Saved\Package\Shipping\Windows`, Shipping, en + zh-Hans). Copy that folder to the PC; no editor or engine is needed. If the game reports a missing runtime, run `Engine\Extras\Redist\en-us\vc_redist.x64.exe` from it once.
- Launch: double-click `Play_en.bat` or `Play_zh-Hans.bat` (full-screen arena, director on, bot off, a fresh seed each launch). Pad: plug it in before launch; keyboard/mouse and pad both work at any time. The session must have one tester who plays on a pad.
- Between runs: the game has no restart. After the death or victory screen press Alt+F4 (the host does it for a pad tester), then launch again. Alt+F4 mid-run is safe; the run file closes with reason `quit`.
- Have this sheet and a clock. Write the clock time each time you launch, and the run timer (HUD) at each key moment.

### Only if the session is on the development PC
Three things are true there and nowhere else:
- Other work on that PC slows the game. That is why the session is announced: the scheduler stops every automated run first. Shipping writes no frame times; a slowed run shows in its file as a slow share (D-054).
- That PC has faulty processors (D-052, O-012), and a game started by double-click runs on all of them. A crash or freeze there may be the PC and not the game: note the run and the time, as for any crash.
- The export takes every run in the PC's folder, the owner's own earlier runs included. The clock times in the run table below are what tells a tester's runs from the rest.

The gate's own `pad` step reads only the development PC's folder, so it passes only from a pad run played there. A pad run on another PC is still the session's pad evidence: the reading tool lists the input device of every run in the zip.

## The one sentence (say only this)
> "You are the bullet hell. Survive fifteen minutes and beat the boss; every level you pick one card. I won't help — think out loud if you like."
>
> 「弹幕即你。撑过十五分钟、击败首领；每升一级选一张卡。我不会提示——想到什么可以说出来。」

Do not explain Keepers, archetypes, flags, Intercession or the card kinds.

## Runs (one row per run, filled as it happens)
The files hold no names. The clock time you write here is the only thing that ties a file to a person, and "1st / 2nd" is how the second run of each person is found (D-046 reads it).

| # | Tester (1, 2, 3) | Their 1st or 2nd run | Clock time at launch (HH:MM) | Input (KBM / pad) | Language | How it ended: HUD time, and death + what killed them, boss win, quit, or crash | Seed (after the export) |
|---|---|---|---|---|---|---|---|
| 1 | | | | | | | |
| 2 | | | | | | | |
| 3 | | | | | | | |
| 4 | | | | | | | |
| 5 | | | | | | | |
| 6 | | | | | | | |

The seed is not shown in the game. After the export each run's two files are named `<seed>_<date>_<time>`, in the PC's local time: find the file whose time matches the row and copy its seed.

## Observe (one column per tester; two runs each; write "run 1" or "run 2" beside a time)
| | Tester 1 | Tester 2 | Tester 3 |
|---|---|---|---|
| Experience (novice / genre veteran) | | | |
| Patron chosen, and why (their words) | | | |
| Second Keeper at L8, and why | | | |
| First dash (time); dashed under pressure without prompting? | | | |
| First Intercession (time); did they find the input alone? | | | |
| Moments: laughed, swore, leaned in, went quiet or bored (time → words) | | | |
| Anything unreadable: telegraphs, enemy fire, cards, HUD, text. For the zh-Hans tester also any wording that read wrong or unnatural (which card or screen → their words) | | | |
| **Unseen hit:** hit by something they did not see coming, or from off screen (time → what; D-036 reverts on one such report) | | | |
| **Drop-in charge:** a red marker lands, an enemy drops in on it and charges along a red line. Each time one of these hit them: time → what they said or did (D-045) | | | |
| Enemies appearing out of nothing where they could see it: did they react? (time → words) | | | |
| A whole-screen power that left enemies standing near the left or right edge, or a remark on how far it reached (time → words) | | | |
| The boss, if reached: did they lose track of it, did it attack from outside the screen? (time → words) | | | |
| **Elite:** a fight or a wall (their words) | | | |
| Chests: did they go for one; did they notice health come back | | | |
| Took Hammer Hand (锤手) while holding the Arc Blade or the Orbital Drones? (Y/N, which weapon, time; if Y ask question 6) | | | |
| Slow motion (the whole game moves slower for a while) or a stutter (a short hitch): run, time, which of the two | | | |
| Bugs, crashes (run, time) | | | |
| **Asked for another run unprompted?** (Y/N) | | | |

**The owner, after the session** (session A row 3; the owner's own answer, in the owner's words): do you still want to play this? Y / N — ______

## Ask after each tester (after their second run, in this order, record exact words)
Questions 1 to 5 are open on purpose. Do not name an enemy, a card or a Keeper before the tester does. Question 14 is the only one that describes something, and it comes last.

1. "When did your build click?" — do not say "archetype", "流派" or a Keeper name first.
   「你的这套打法是什么时候成型的？」
2. "What would you call your build?" → **can they name their archetype in their own words?** (Y/N; the scheduler maps the words to `Archetype.*`).
   「你会怎么称呼你这套打法？」
3. "Was there a card you were glad to see?"
   「有没有哪张卡，你一看到就很高兴？」
4. "Did anything feel tiring or unfair?"
   「有没有什么让你觉得累，或者不公平？」
5. "What would make you play again?"
   「什么会让你想再玩一局？」
6. Only if they took Hammer Hand (锤手) while holding the Arc Blade or the Orbital Drones: "You took Hammer Hand with the blade (or the drones). What did you expect it to do to that weapon?" — do not say "cleave" or "横斩" first; record whether they say it seemed broken there (D-053 item 4 and its revert).
   「你拿着光刃（或无人机）的时候选了锤手。你当时觉得它会对那把武器做什么？」
7. About the second run only: "In that last run, what came closest to killing you?" — their words, or "nothing".
   「刚才那一局，什么最接近把你打死？」
8. "Was there a hit you felt you had no time to get away from? What was it?" — do not describe any enemy. If they describe an enemy that dropped in and charged, write "drop-in, unprompted".
   「有没有哪一下，你觉得根本来不及躲？是什么打的？」
9. "Did anything hit you that you could not see on the screen?"
   「有没有被屏幕上看不到的东西打中？」
10. "Did enemies ever appear out of nowhere right in front of you?"
    「有没有敌人凭空出现在你眼前？」
11. "Some of your powers hit the whole screen. Did one ever seem to miss enemies you could see, or stop short of the edge of the screen?"
    「有些能力是打全屏的。有没有哪次看起来漏掉了你看得见的敌人，或者没打到屏幕边上？」
12. Only if they reached the boss: "Could you always tell where the boss was? Did it ever attack you from outside the screen?"
    「你一直知道首领在哪吗？它有没有从屏幕外面打你？」
13. "Did the game ever seem to slow down, like slow motion?" — which run, about when.
    「游戏有没有变慢过，像慢动作那样？」
14. Last, and only now describe it: "Some enemies drop in at a red marker and charge along a red line. Did one of those hit you? Did you have time to move?" — Y/N to each; write "prompted".
    「有的敌人会落在红色标记上，然后沿着红线冲过来。你被这种敌人撞到过吗？来得及躲吗？」

The zh wording is a first draft; the host may reword it and writes on the sheet what was actually asked.

What each answer is for (the scheduler's use):

| Answer | Read by |
|---|---|
| Question 2 | Pass rule row 2 |
| Question 4, with the runs alive at 600 s | D-046's hypothesis |
| Question 6 | D-053: reverted if two who held the Key with the blade or the drones say the card seemed broken there |
| Question 7, with the second run's health at the boss | D-046: the restore is removed if two of three reach the boss above half health on their second run and name nothing |
| Questions 8 and 14, the drop-in row | D-045: reverted if two validators call an arrival a hit they had no time to answer. Unprompted and prompted answers are kept apart |
| Question 9, the unseen-hit row | D-036: the frame goes back to 2000 on one report of a hit from an unseen source |
| Questions 10, 11, 12 and their rows | D-036's three held items: enemies appearing in view, whole-screen effects against the frame, the Warden off screen |
| Question 13, the slow-motion row | D-054: reverted if a validator reports slow motion on a run whose slow share read near zero |
| The owner's line | Pass rule row 3 |

## After the session
- Export: double-click `ExportTelemetry.bat`. It makes `Dusktracer_Telemetry_<time>.zip` on the desktop (one CSV + JSONL per run, from `%LOCALAPPDATA%\Dusktracer\Saved\Playtests`, and a `MANIFEST.txt`) and prints the path. Nothing leaves the PC by itself: you carry the zip. Export after each tester or once at the end; a later zip holds the earlier runs too.
- Hand over: the host gives the zip and the filled sheet (the paper, or a photo of every page) to the owner. The owner puts them in `E:\Dusktracer\Saved\Sessions\A\<tester>\` on the development PC, one folder per tester named `T1`, `T2`, `T3` as on this sheet and never by a person's name, and says so in the scheduler's session. Session B goes to `Saved\Sessions\B\` the same way. Nothing else is needed.
- What the zip holds: gameplay only (seed, build hash, culture, last input device, combat numbers, every hit the hero took with its source and where both stood, whether that source dropped in or was charging, health restored, how many drop-ins landed, hero positions on the arena, cards offered and picked, Keeper and weapon choices, whether a bot played the run, and the clock: the start time, UTC inside each file and local in its name, so the two show the time zone, the seconds each hand took, the seconds paused, the run's length on the clock and the steps the PC could not keep up with). No name, account, machine name, path or hardware. This build writes no engine log; `-IncludeLogs` adds log and crash folders if any exist, and those can name the Windows user, so it is off by default.
- Crash: note the run and the time on the sheet. The run's file has no `run_end`, so its pause, its slow share and its end are not read; the seed in its file name replays the start on the proof build.
- A returned zip is data. The reading tool copies it into its own folder (`Saved\SessionRead\`) before it opens it, takes only run files and the manifest out of it, and runs nothing from it.
- Reading: the scheduler types the run table into `Saved\Sessions\A\validators.csv` (`python -I Tools\session_read.py Saved\Sessions\A --list` prints the rows to fill, one per run found, with each file's start time) and runs `python -I Tools\session_read.py Saved\Sessions\A --validators Saved\Sessions\A\validators.csv`. It prints every reading the pass rule and the decisions above name, or `NOT READ` with the reason. What only this sheet holds is printed as `SHEET:` with the question's number.
- The scheduler fills `Harness/templates/validation_note.md` as `Docs/Validation/S1_session_A.md`, then `S1_session_B.md`, each with a `Build:` line, and triages every finding (class, layer, owner).

## Not asked at session A
These `HUMAN` lines of the specs are not put to a tester by this sheet, so a session note does not count them as read: naming a card's kind at a glance (`offers.md`), saying when a Threat started and ended (`director.md`), saying how a modifier changed the Keeper (`modifiers.md`), saying what each weapon does (`weapons.md`), the gem and level lines (`progression.md`), the Warden's skill checks by name (`boss.md`). The watch rows above catch some of it; none of it gates the S1 exit.

## Pass rule (written before the sessions; D-048, `12_Production §2`, `00_Vision §4`)
The S1 exit is read in two sessions. The same three people may come back for the second.

**Session A** judges three rows:
1. 3 non-owner validators played 2 runs each (≥ 1 pad, ≥ 1 zh-Hans).
2. ≥ 2 of 3 name their build in their own words.
3. The owner still wants to play after the session.

Session A also reads, with no verdict: damage taken and dashes for every minute of every run, swarm kill time, pause seconds per minute, the L8 and L16 times, the pickup ratio, restores collected, the Keeper attribution of any run that reached L16, and the two rows D-048 added to the Observe table, **Unseen hit** and **Elite**. It reads for the decisions taken since: each run's slow share (D-054), the hits by drop-ins out of the Rusher drop-ins that landed (D-045), the runs alive at 600 s and each second run's health at the boss (D-046), and who held Hammer's Key with the blade or the drones (D-053). The two draft pressure bands are judged at no session until the gate decision sets them (D-046): at session A the tool prints the values and `NOT SET`. "Asked for another run unprompted" is recorded and reported at both sessions; it gates the S3 exit (D-049).

**The gate decision** between the sessions sets the two pressure bands from A's readings (D-046, method in `10_Tuning.md §2`), names the bot skill row that brackets the validators, and keeps, changes or removes the chest restore.

**Session B** judges, on runs alive through `C.DIR.BAND_WINDOW_S` (the median over those runs; fewer than three such runs reads NOT READ):
4. Damage taken and dashes per minute land in the bands the gate decision wrote.
5. Swarm kill time is within `C.TTK.SWARM_STEERED_MAX`.
6. Hands pause the run for at most `C.OFFER.PAUSED_S_PER_MIN_MAX`, the Patron hand left out.

**The tool row**, read on each session's build hash and required at B:
7. Keeper share is at least `C.POWER.KEEPER_SHARE_L16` and the archetypes sit within `C.POWER.ARCHETYPE_BAND`, both by loss (`Tools/probe.py`, D-047, `10_Tuning.md §3`). `Tools/power_split.py` on the session logs is printed beside it and never judged.

S1 exits when rows 1 to 3 have held at either session and rows 4 to 7 hold at B. Then the shape constants lock (`08_Constants §1`). A fail routes findings through `Harness/H04`; it never adds content.
