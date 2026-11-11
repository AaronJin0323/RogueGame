# OPEN — for the owner (D-009: the scheduler decides everything else; D-051: what it decided in your classes is listed last)

Nothing here blocks the agents. The first four items are things only a person can do. The fifth and sixth need your eyes. The last is a list to read.

## O-008 — Three people, two sittings  [GO-NOGO, human-only]
- **Context:** the S1 exit is read in two sessions (D-048): session A reads the people and calibrates, a gate decision sets the pressure bands, session B confirms them. Each is three people who are not you, two runs each, about 40 minutes a person; at least one on a gamepad and one in Chinese. The same three can come back for B, and later for S2 and S3 on fresh saves (D-049). This has been the only thing between the game and its first exit since 2026-10-30.
- **Ask:** pick three people and two rough dates a week or so apart. Name one native Chinese reader among them or beside them: about 135 Chinese rows on the later branches were written by agents and no reader has seen them.
- **The build is ready** (2026-11-11): `Saved\Package\Shipping\Windows` in `E:\Dusktracer`, or the zip `Saved\Packages\Dusktracer_S1_sessionA_e4926d4.zip` beside it, built from `main` e4926d4 after the consolidation list landed, so validators meet the fixes and not the defects. Its run files record what session A is read by, the count of arriving Rushers included. Two earlier packages of the same day were withdrawn and their zips removed; this is the only one to hand out. Copy the folder to the PC and start `Play_en.bat` or `Play_zh-Hans.bat`; `Validation/S1_kit.md` is the sheet. If code lands on `main` before the session, the scheduler rebuilds it and says so in `STATE.md`. Tell the scheduler when a session starts, so automated runs stop.
- **Default if unanswered:** work continues; no exit passes and no further stage starts (D-050).

## O-011 — Two runs of your own, talking aloud  [human-only]
- **Context:** your one run (`Validation/S1_owner_run.md`) lost a whole bar by 563 s where bots lose about 7 % of one, on a path that passed every weapon. One run cannot say whether that was the path or the game, and its files hold none of your words.
- **Ask:** about 40 minutes on `Tools\Play.bat`: one run taking weapons as they come, one passing them again. Say what you see as you play, and say afterwards whether anything hit you from off screen, whether an elite felt like a fight or a wall, and whether you want another run.
- **Default if unanswered:** session A goes ahead without it.

## O-012 — The PC  [human-only]
- **Context:** every build, gate, measurement and package comes from one PC (i9-13900KF, ASUS PRIME Z790-A, BIOS 1820, microcode 0x12F). On 2026-11-11 its random faults were traced to two logical processors, 0 and 10: a ten-line Python loop dies there on most runs, on three different Python builds, and on no other processor in 351 runs (`Validation/S4_py_fault.md`). The sibling threads on the same two cores never fault, so it may be the firmware or a driver and not the silicon; nothing the agents can run tells them apart. Later the same day processor 1, the sibling of 0, faulted once while eight loops ran at once, so the mask now leaves out both whole cores (0, 1, 10, 11). The pattern (processor 10 faults when the machine is idle and not when it is busy; a sibling faults once under load) fits a core that is unstable at its highest turbo speed, which is the known weakness of this CPU family; it is not proved. D-052 keeps every tool run off those processors and adds a check to the gate. That hides the fault from the tools. It does not fix the PC: the game you launch yourself, and anything else you run, still uses all 32. Separately, the S4 exit reads performance per tier and only one GPU has ever been measured.
- **A five-minute test first:** ASUS Armoury Crate, the ASUS AURA lighting service, the ASUS fan control service and Logitech G HUB are running on the PC, and all of them talk to hardware through their own drivers. Close them, then run the ten-line loop of `Validation/S4_py_fault.md §5` pinned to processor 0 twenty times. If it stops dying, one of them is the cause and no hardware is at fault. The agents may not close them for you.
- **Ask:** (1) run Intel Processor Diagnostic Tool; (2) load the BIOS's Intel default power profile, or update the BIOS, and tell the scheduler, so processors 0 and 10 are read again; (3) if they still fault, see which devices interrupt on those two processors (LatencyMon, Device Manager), and if the diagnostic fails, ask Intel about a warranty exchange for this CPU. When S4 nears its exit: a reading on a real 4070, a 3060-class card and a Steam Deck.
- **Default if unanswered:** the mask stays (D-052); packages for people, exit batteries and perf readings run with nothing else alive on the PC (D-050); performance stays proxied and is reported as proxied.

## O-009 — Name the AI service behind the RogueHero hero model  [human-only]
- **Context:** RogueHero's Reclaimer model came from an AI generation service, and this repository is public.
- **Ask:** name the service and whether its terms allow redistribution in a public repo and a shipped game.
- **Default if unanswered:** no AI-sourced mesh, texture or input enters the repo; the hero model is built in Blender from scratch. Nothing waits on this.

## O-014 — Danger on a mid-tone floor  [RULE, needs your eyes]
- **Context:** the telegraph and the enemy round are each built from a bright part and a dark part, and today all seven readable elements clear 3:1 only where the floor is very dark (luminance at most 0.037) or in a lit band (0.106 to 0.16). On any mid-tone both parts of the telegraph fail (`Validation/plans_audit.md` 5.10). The grey-box floor has only those two tones, so the gate passes; real ground will not.
- **Options:** A — make it a rule of the art: the play floor has two tones and no mid-tone under danger. B — give the telegraph a hot core line bright enough to read on any ground, and free the floor.
- **Recommendation:** B, because A binds every floor, light and weather the game will ever have to a window about 0.05 wide. It changes how danger looks, which is yours.
- **Default if unanswered by the first S4 floor merge to `dev/s4`:** A is held as a limit, not a rule: floor and light work stays inside the two windows and reads its worst patch (D-050 item 8).

## O-015 — Two trail colours for players with red-green colour blindness  [RULE, needs your eyes]
- **Context:** simulated for deutan vision, the Forge trail colour (`Pal.Keeper.Forge` #FF9F3A) sits too close to the enemy muzzle flash (a difference of 8.5 where the Settings spec asks 15) and to the danger telegraph. The spec on `dev/s2` records both as debts and calls the fix "an owner call" (`settings.md` R-SET-11 and its Open section there). No person with that vision has looked.
- **Options:** A — change the Forge and Quicksilver trails to the source game's lighter pair (#FFC17A, #B3FB60). B — keep the confirmed palette (D-008) and rely on the danger-outline setting that `dev/s2` already has.
- **Recommendation:** B, on frames the Visual session made on 2026-11-11 (both options, plain and simulated, in a staged fight). Worked out from the palette values, A lifts the Forge trail from 8.5 to 20.3 against the muzzle flash. Measured on the drawn frame it lifts it from 17.0 to 17.6, because a trail is thin and see-through and never shows its palette colour; today's trail already clears both limits there (17.0 where 15 is asked, 31.8 against the telegraph where 20 is asked). A also makes the Forge trail look like the Radiance trail. Danger is told by shape in every frame: an enemy round is a ring, a telegraph a band with a black edge. The scheduler looked at the frames and reads them the same way. No person with that vision has looked, and nothing was seen moving.
- **Default if unanswered:** B: the palette stays as you confirmed it, and the two debts stay written in the spec.

## O-013 — Decided for you  [digest, D-051]
Each stands unless you say no. A line leaves at the next stage gate.

Decided on 2026-11-11, from `Validation/plans_audit.md`:
- **D-045** A drop-in Rusher charges through where you stood. Until now most of them stopped short and could not hit.
- **D-046** Picking up a chest gives back 15 % of your health, six chests a run. The two pressure bands inherited from the previous game are set from session A's readings instead of being assumed.
- **D-047** "Keepers carry at least 60 % of the power" is measured as what a build loses without its blessings.
- **D-048** The S1 exit takes two sessions, as in O-008.
- **D-049** The S3 exit uses your rule of 2026-10-19: two of three hold (two of three testers want another run; two can name their build; you still want to play). It replaces D-034. "Fresh" means a fresh save, so the same three people can return.
- **D-050** S4's first wave continues on `dev/s4`. Nothing further starts early, the S1 build is fixed first, and no number is tuned on bot survival until people calibrate the bots. This answers O-010.

Decided on 2026-11-11, after the reread and the open calls of the handover:
- **D-053** Hammer's Key still changes nothing on Orbital Drones. Its card names only the weapons it converts, and the drones are not among them. Whether its cleave cards should also act on the Arc Blade's sweeps is asked of the people in session A and decided at the S1 gate. Hard Blow, which you took in your run and which did nothing, now knocks enemies back as its card says.
- **D-054** A run played on a PC too slow to keep up is told by its slow share: the part of the time the game ran in slow motion. The limit is set after session A; until then it is printed and nothing is set aside.

Decided on 2026-11-11, on your three messages about working in parallel:
- **D-055** Three module sessions (Mechanics, Visual, Sound) now plan and design their own module on Fable 5.2 extra-high and send out their own Opus 5.5 medium developers; the scheduler alone merges. Sound starts with a plan of its own decisions and a first wave on `dev/s4`. Still held until people have played: a second visual wave, any later stage, new gameplay content, and tuning on bot survival. Say so if you want any of those opened too. One risk is yours to know: four Fable sessions at once can reach the account's Fable limit, and that stops all of them until it resets.

- **D-056** The first sounds. Every sound is made by a script in this repo, none is downloaded or recorded, and all of them sit in one musical key so that no combination clashes, as you ruled for the earlier game. Warnings get a pitch range of their own that your own fire is kept out of, and a warning turns your fire down while it sounds. There is no music in this first wave. Each Keeper gets a short signature when you pick its blessing. Tool runs stay silent; only a game you start yourself makes sound. Nobody has heard any of it: when the first sounds exist you get about ten minutes of listening, as its own item here.

- **D-057** The plan for the next round of art, which is not started: it draws what the game knows and the screen does not yet show (the statuses on enemies, the weapons that are not plain rounds, gems and chests, elite parts sitting on their hulls, a hero that strides) before any surface detail. In it: the menus and cards stay on the drawing code they use today through this stage; Burn is drawn on an enemy in Forge's amber and Blind in Radiance's gold, with a check that neither can be taken for enemy fire. It opens after session A is read. Say so if you want it opened sooner.

Decided earlier by the scheduler, in your classes, and shown to you only in `STATE.md`:
- S2 started on `dev/s2` on 2026-10-31 with no record; S3 started on `dev/s3` (D-022); S4's first wave on `dev/s4` (D-043).
- **D-013** The Patron pick at run start is not one of the 28 decisions. **D-015** Every run starts with the Pulse Rifle.
- **D-023** Meta grants no stats; the old tree's stat ranks were cut. **D-024** A first run holds four weapons and four modifiers; three Tree nodes and every Tree price were invented.
- **D-030** A card states what it takes, per weapon, in the numbers the sim deals.
- **D-036** The camera frames 1500 uu of height and is not a player setting; it reverts to 2000 if one validator reports a hit from off screen.
- **D-041** Killing the Warden on the step you fall is a win.
