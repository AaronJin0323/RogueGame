# S1 telemetry marks, second pass: how many drop-ins landed, and where a fast bot's session closes

Build: base `486998b` (`main`); the change is on `s1/telemetry-marks-2`. Development editor build.

## 1. The arrivals denominator (telemetry only, T-TEL-16)
- **What changed.** `dir_block` gains `dropins_landed_by_role{role: n}`: the drop-ins that landed in the block, by role, counted in `FDTDirectorCore::StepDropIns` where a marker lands and its enemy spawns with `DTEnemyFlags::Arrival`. Each one is an arrival that `hero_damaged.arrival` can name. `dropins` keeps its meaning: the markers published in the block, those cancelled later included. `spawned_by_role` still holds the timeline's spawns and the landed drop-ins together. The definition is in `telemetry.md` (the `dir_block` row and T-TEL-16); `director.md` R-DIR-15, which owns the record, is outside this job's files and does not list the field yet.
- **Test.** T-TEL-16 `Dusktracer.Director.DropInsLanded`: over a whole director run, the blocks' landed counts sum by role to the arrivals the director spawned and to `DropInsLanded`, and never exceed the published markers. Seen failing first with the count not kept ("role 2: landed drop-ins are the arrivals spawned: expected 50, was 0"; the same for Turret, 21). `Dusktracer.Telemetry.Records.Director` pins the field in the record line.
- **Reader.** `Tools/pressure.py` ends each Rusher line with `out of <n> landed Rusher drop-ins`, summed over the run's `dir_block` records, per run, per arm and overall; a run file without the field prints `landed drop-ins not in the file`, and so does a group with any such run. `tools_test.py` `ArrivalHits.test_landed_dropins_are_the_denominator`. No other reader parses the field.
- **A real record**, seed 9107 None arm:
  `{"t":337.5000,"step":20250,"kind":"dir_block","owner":-1,"block":5,"phase":"Harvest","row_start_s":270.0000,"start_s":276.0000,"end_s":337.5000,"ended_by":"beat","spawned_by_role":{"Swarm":132,"Tank":1,"Rusher":6,"Turret":16},"skipped_at_cap":67,"dropins":5,"dropins_landed_by_role":{"Rusher":3,"Turret":2},"threat_actions":89,"kills":115,"kill_horizon_p50_uu":406.5011,"alive_peak":181,"dmg_taken":{"0":0.0000},"dashes":{"0":9}}`
- **Hits out of arrivals**, the same run: `SOURCES: None_s9107\9107_20261111_152154 p0 rusher hits, whole run: arrival charging 0 not charging 1; other charging 0 not charging 1; killed drop-in band 0; out of 22 landed Rusher drop-ins`. Over the five seed 9107 arms: `out of 226 landed Rusher drop-ins`.

## 2. A fast bot's session closed a step or two apart (fixed, T-TEL-17)
- **Which runs.** Comparing `agent-a559188263b0e1f6c\Saved\BotRuns\d045_sources` with `d045_sources_2` run by run, 12 of 285 runs have a different `run_end.steps`, each with the same deciding step in both: Forge:Furnace s13, Forge:Hammer s4, Forge:Slam s12, Storm:Downpour s6, Tide:Frost s6 and s10, Tide:Stillness s1, s12 and s13, Tide:Whirlpool s14, Tide:Whirlpool:Storm s13 and s14. For example Stillness s1 decided at step 58956 and closed at 58957 steps in one battery and 58959 in the other.
- **Cause.** A decided run's session closes on a later tick: the game mode's `EndRunSession` for a death and the boss's `EndTelemetrySession` for a victory are both `SetTimerForNextTick`. The bot's `RunSteps` stops at the decision, but the sim's own `Tick` keeps calling `AdvanceFrame(DeltaTime)` in that frame and the next, and runs as many steps as the wall time owes, up to `C.SIM.MAX_SUBSTEPS`. A fast bot's frames are long and uneven, so the steps that run between the decision and the close, and with them `steps` and the closing records, depend on frame timing. Before the decision the runs are the same because every step is the same step whoever calls it; only the number of steps after it moves.
- **Fix.** `UDTBotSubsystem::HandleStepEnd`: when a fast bot's (`StepsPerFrame` above zero) run is finished, it calls `UDTSimSubsystem::Hold()` at that step boundary. The bot already stops latching input there, so `input_hash` is unchanged. A person's run and a bot at real speed are untouched: they keep running in real time until their session closes. Rule text: `telemetry.md` R-TEL-08.
- **Test.** T-TEL-17 `Dusktracer.Telemetry.Mark.FastBotClose`: one seed played twice by a fast bot to a death, then three frames of one length or another before the close. Before the fix it failed (`steps` 6 against 9); it passes after.
- **`steps` moves, and to the right value.** Every fast bot run now closes with `run_end.steps` equal to the deciding step plus one, where it read plus one to plus three before (plus three on most runs; Stillness s1 read plus one in one battery and plus three in the other). On the five seed 9107 arms: None 38181 to 38179 (decided 38178), Forge:Hammer 58104 to 58102 (58101), ArcGrid 58332 to 58330 (58329), Downpour 58230 to 58228 (58227), Tempest 59332 to 59330 (59329). The deciding step is the last step of the run; the steps after it were wall time, not the run.
- **Proof on a battery run twice.** `Tools\BotRuns.ps1 -Arms Steered:Tide:Stillness -FirstSeed 1 -Seeds 15 -Parallel 3`, twice (`Saved\BotRuns\m2_twice_a`, `m2_twice_b`): 15 of 15 runs identical record for record, wall fields and build stamp left out. That arm held three of the twelve runs that moved; s1 now closes at 58957, s12 at 8737, s13 at 58370, the deciding step plus one each.

## Seed 9107 `input_hash`
Before is `8309b9a`, the code `486998b` holds (`git diff 8309b9a 486998b -- Source Tools` is empty; read in `S1_telemetry_marks.md`); after is this change.

| Arm | Before | After |
|---|---|---|
| None | `fe30ec79` | `fe30ec79` |
| Steered:Storm:Tempest | `32aa9ec9` | `32aa9ec9` |
| Steered:Storm:ArcGrid | `d5bc3fe1` | `d5bc3fe1` |
| Steered:Storm:Downpour | `ae8a4edd` | `ae8a4edd` |
| Steered:Forge:Hammer | `8dd4ffc6` | `8dd4ffc6` |

## Tests added
T-TEL-16 `Dusktracer.Director.DropInsLanded`, T-TEL-17 `Dusktracer.Telemetry.Mark.FastBotClose`; the count rises from 263 to 265.
