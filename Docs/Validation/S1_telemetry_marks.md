# S1 telemetry marks: which hits came from a drop-in, and which runs a bot ran fast

Build: base `bd2651f` (`main`); the change is on `s1/telemetry-marks`. Development editor build. No sim behaviour changes: the marks are written, never read by the sim.

## What each run file now says
- **`hero_damaged.arrival`** (R-TEL-10, T-TEL-14 `Dusktracer.Telemetry.Mark.Arrival`). True when the hitter carries `DTEnemyFlags::Arrival`, a director drop-in, by its body (`FDTEnemyPool::AppendContacts`) or by the band of one killed in its wind-up (`FDTEnemyPool::StepOrphans`, the hazard path); false for every other hit, rounds and the boss included. It is written on every `hero_damaged`, as a bool, the way `intercession.complete` is: a missing field then means a file from before the mark. `hit_kind` is unchanged: a drop-in's live charge is a `Contact` hit (R-ENM-14), the band of a killed one a `Charge` hit.
  - **What the mark covers.** The flag is set at spawn and never cleared, so a drop-in Rusher's contact after its first charge, when it walks like any Rusher, is still marked. A Turret drop-in's contact is marked; its rounds are not (a round carries no enemy flag).
  - **`hero_damaged.charging`**, added after the first review because `hit_kind` cannot tell a drop-in's charge from its walking into the hero later: true when the hitter is a Rusher, drop-in or not, that moved under its charge on the step it touched the hero (`FDTEnemyAux::LastChargeStep` equals the step, set in the Charge state of `StepRoles`, so the last charge step counts though the Rusher is already in Recover when `AppendContacts` runs). False for every other hit, and false for the band of a drop-in killed in its wind-up: the band has no live hitter, and its `hit_kind` Charge already names it. Written on every record, like `arrival`. D-045's question, how many drop-ins' charges hit, is the count of `arrival` and `charging` both true, plus the killed drop-ins' bands.
  - The marks travel as `FDTHeroHit::bArrival` and `bCharging` (`Core/DTSimTypes.h`) and the same two on `FDTHeroEvent` (`Player/DTHeroTypes.h`), all telemetry only, like the hit's `Position`. `Core/DTSimTypes.h` holds the hit struct; the two fields are the only change there.
- **`run_start.bot_fast`** (R-TEL-08, T-TEL-15 `Dusktracer.Telemetry.Mark.BotFast`). The steps per frame an active bot runs past the clock (`UDTBotSubsystem::GetStepsPerFrame`), zero for a person's run, for a bot at real speed and for the plain core's default. Written on every `run_start`. The bot sets it at world begin play, before `ADTGameMode` starts the session. No name, path or machine fact.

## Tests
Both tests were seen failing first: built with the struct fields but without the two writers, `Dusktracer.Telemetry.Mark.Arrival` failed on "an arrival's contact is marked", "a Rusher that walked in is not" and "and carries the mark", and `Dusktracer.Telemetry.Mark.BotFast` failed on all four of its lines. Both pass with the writers. The `charging` assertions went under T-TEL-14, the mark's home: with the struct field but no writer they failed on "charging is the charge step's" (the drop-in's charge steps read false), and they pass with it. `Dusktracer.Telemetry.Records.Combat` pins the hero_damaged line and now reads `"arrival":false,"charging":false` in it. `Tools/tools_test.py` gains `SlowShare.test_run_start_bot_fast_wins_over_the_folder` and `ArrivalHits.test_marked_beside_unmarked`.

## Readers
- `Tools/pressure.py`: after each run's damage `SOURCES:` line, `SOURCES: <run> p<n> rusher hits, whole run: arrival charging <a> not charging <b>; other charging <c> not charging <d>; killed drop-in band <e>` (plus `unmarked contact <x> charge <y>` for a file without the two fields), then one such line per bot arm and one `overall`. Counted over the whole run, not the band window: drop-ins start at 270 s. The `SLOW:` line reads `run_start.bot_fast`; a run file without it falls back to the folder rule and says "no bot_fast in run_start, told by its folder".
- `Tools/bot_summary.py` and `Tools/owner_read.py` read records with `dict.get` and take the new fields unchanged; no other reader needed a change.

## Seed 9107 `input_hash`
`& Tools\BotRuns.ps1 -Arms None,Steered:Storm:Tempest,Steered:Storm:ArcGrid,Steered:Storm:Downpour,Steered:Forge:Hammer -FirstSeed 9107 -Seeds 1`, skill 1.0, policy All.

| Arm | Before (`bd2651f`) | After | End |
|---|---|---|---|
| None | `fe30ec79` | `fe30ec79` | death 636.3167 s |
| Steered:Storm:Tempest | `32aa9ec9` | `32aa9ec9` | victory 988.8333 s |
| Steered:Storm:ArcGrid | `d5bc3fe1` | `d5bc3fe1` | victory 972.1667 s |
| Steered:Storm:Downpour | `ae8a4edd` | `ae8a4edd` | victory 970.4667 s |
| Steered:Forge:Hammer | `8dd4ffc6` | `8dd4ffc6` | victory 968.3667 s |

All five are the values `main` records, and none moved: read on the first change and again with `charging` (`Saved\BotRuns\marks_charging`), the same five both times.

## Real records
- A drop-in's hit, seed 9107 None arm, taken on the first change, before `charging` existed (one of its two marked hits; the other is a Turret drop-in's contact):
  `{"t":391.4667,"step":23488,"kind":"hero_damaged","owner":0,"amount":19.9942,"hp_after":80.0058,"max_hp":100.0000,"hit_kind":"Contact","role":"Rusher","attack":null,"arrival":true,"hero_x":2271,"hero_y":-1807,"src_x":2343,"src_y":-1839}`
- A bot's fast run, the same run: `"bot_fast":600` in `{"t":0.0000,"step":0,"kind":"run_start","owner":-1,"seed":9107,"players":1,"culture":"en","build":"bd2651f+dirty","interval_s":60.000,"step_s":0.016667,"bot_fast":600,"wall":"2026-11-11T02:28:55.839Z"}`. `pressure.py` prints `SLOW: ... not read (bot at -DTBotFast=600; 90 dropped steps)`.
- A person's launch, `Tools/SeedCheck.ps1` launch 6 (`Tools\Play.bat`, a fresh seed, no `-unattended`): `{"t":0.0000,"step":0,"kind":"run_start","owner":-1,"seed":1822879877,"players":1,"culture":"en","build":"bd2651f+dirty","interval_s":60.000,"step_s":0.016667,"bot_fast":0,"wall":"2026-11-11T02:36:09.932Z"}`.

- A drop-in's charge, both marks true. None of the five seed 9107 runs has one (the None arm reads `arrival charging 0 not charging 1; other charging 0 not charging 1; killed drop-in band 0`). A battery of the None arm at skill 0.5, seeds 9108 to 9117, reads `arrival charging 1 not charging 4; other charging 2 not charging 2; killed drop-in band 0`; the one hit, seed 9116, is the fatal one:
  `{"t":585.9667,"step":35158,"kind":"hero_damaged","owner":0,"amount":21.8974,"hp_after":-18.1681,"max_hp":100.0000,"hit_kind":"Contact","role":"Rusher","attack":null,"arrival":true,"charging":true,"hero_x":-3776,"hero_y":3960,"src_x":-3832,"src_y":3908}`

## Not changed
`package.md`'s Data line now names the two marks. That line says `MANIFEST.txt` and `Validation/S1_kit.md` "change with this list"; both describe the files as gameplay and combat numbers, which covers the marks, and neither is in this job's files.
