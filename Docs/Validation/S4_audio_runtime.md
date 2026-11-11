# S4 audio runtime: the C++ side of audio wave 1

Job 3 of `Validation/S4_audio_wave1.md` §8.1 (D-056). Branch `s4/audio-runtime` from `s4/audio-snd-ui` at cb996a5, with `dev/s4` a155ace merged. The spec is `02_Systems/audio.md`; this note says what landed, what proves it, and what is not verified.

**No voice has reached the engine in any run made for this job.** Every run was unattended (the automation tests, the gate, the bot runs), so the engine sink was never made, and the application volume was held at zero. Nothing here has been heard.

## 1. What landed, and what proves it

| Item | Files | Proof |
|---|---|---|
| The scheduler: per-id rules from the generated table, taken as a view; run sounds on sim steps, interface sounds on real seconds; one voice per id and step, louder by the aggregation rule and capped; merging inside the interval; the voice caps with kept danger and interface slots; player starts per second of sim time; starts per frame, danger first, a waiting batch dropped after one frame; danger never waits or varies and takes a player slot when none is free; the duck curve; the grid; variants in turn; key degrees and level jitter from audio's own stream; the counts | `Audio/DTAudioScheduler.h/.cpp`, `Audio/DTAudioTypes.h` | `Dusktracer.Audio.DangerFirst` (T-AUD-05), `.Aggregate` (T-AUD-06), `.Density` (T-AUD-07), `.Grid` (T-AUD-09), `.Variation` (T-AUD-10), `.Counts` (T-AUD-20) |
| The stinger queue, ported from RogueHero's; no reader yet | `Audio/DTStingerQueue.h/.cpp` | `Dusktracer.Audio.Stingers` (T-AUD-11) |
| The observer: bound to `OnStepEnd` for run families and polled each frame; one file per family; the interface family | `Audio/DTAudioObserver.h/.cpp`, `Audio/Observe/DTObserveInterface.h/.cpp` | `Dusktracer.Audio.Observer` (T-AUD-02): each interface moment raises once on staged views, and no file of `Audio/` names a take-once queue |
| The world subsystem: assets resolved once by convention, a missing asset silent, counted and logged once; the engine sink (`PlaySound2D`, UI flag for the UI class); both silence locks; `dt.Audio.Enabled`; `DT.Audio.Stats`; `-DTAudioLog`; the timer of three parts | `Audio/DTAudioSubsystem.h/.cpp` | `Dusktracer.Audio.Silent` (T-AUD-03), `.SimUntouched` (T-AUD-01), `.Cost` (T-AUD-21) |
| The generated table carries each sound's longest variant in ms, from the manifest; the header is written, and the `audio` step holds it | `Tools/audio/gen_sounds.py`, `Audio/DTSoundsGen.h` | `audio_test.py` `Header` tests; the step's verdict names only the mix checks as not active |
| `C.AUD.LEVEL_JITTER_DB` 1; `C.GATE.TEST_FLOOR` 573 to 584 | `constants.yaml`, `08_Constants.md`, `number_change_log.csv`, `Core/DTConstantsGen.h` | `gen_constants.py --check`, `docs_lint.py` |

The interface event table and the decisions the spec did not settle are in `02_Systems/audio.md` (Data).

One file outside `Audio/` changed, under the Sound session's grant (D-056 ruling 3, moved to this job): `Tests/Settings/DTSettingsTests.cpp`, where T-SET-02's list of settings readers gains `Audio/Observe/DTObserveInterface.cpp`. The interface observer must include the menu subsystem's header, a `Settings/` header. Audio stays at exactly one reader: its header takes the menu's types through `Hub/DTHubState.h`, and the Step sound of the next job will read the settings from this same file.

The ten interface sounds this branch carries (cb996a5) were heard by the owner and rejected for style (`S4_audio_ui.md` §0). They stay as placeholders that give the runtime its ten ids; no asset exists and nothing plays them.

## 2. The sim proof (`S4_wave1.md` §2 item 1)

The five seed 9107 arms were run one at a time with `Tools\BotRuns.ps1 -Arms <arm> -FirstSeed 9107 -Seeds 1` on ce2d5d1. Each run launches the game unattended with audio on: it observed, scheduled and counted (one run's closing line: 102 frames, observe p95 0.0731 ms at 600 steps a frame). Before is the baseline of record, `S4_p0.md` §8, second table, which `S4_e0_tap.md` read again after the tap.

| Arm, seed 9107 | `input_hash` before / after | `steps` before / after | `bot.csv` bytes before / after | SHA-256 (first 16) before / after |
|---|---|---|---|---|
| None | `00e0d4ec` / `00e0d4ec` | 44137 / 44137 | 535 / 535 | `b6a4878cb23adae7` / `b6a4878cb23adae7` |
| Random | `51369747` / `51369747` | 59380 / 59380 | 663 / 663 | `485ed85694ad5c92` / `485ed85694ad5c92` |
| Steered:Forge:Hammer | `e3293942` / `e3293942` | 57749 / 57749 | 679 / 679 | `b3cda2804c6a4664` / `b3cda2804c6a4664` |
| Steered:Storm:Tempest | `b046559b` / `b046559b` | 60231 / 60231 | 680 / 680 | `cab7cae12d42f789` / `cab7cae12d42f789` |
| Steered:Tide:Frost | `def51848` / `def51848` | 48598 / 48598 | 563 / 563 | `dc02a5d3f44c2d11` / `dc02a5d3f44c2d11` |

Restart and meta: §4.

## 3. The timer

| Run | Frames | p95 ms observe / schedule / hand | Worst ms |
|---|---|---|---|
| `Dusktracer.Audio.SimUntouched`, audio on (45 s of sim, one frame a second) | 46 | 0.0031 / 0.0009 / 0.0001 | 0.0038 / 0.0016 / 0.0055 |
| Bot run Steered:Storm:Tempest, seed 9107 (600 steps a frame) | 102 | 0.0731 / 0.0014 / 0.0005 | 0.0882 / 0.0020 / 0.0220 |
| `Dusktracer.Audio.Cost`, the density trace through the interface observer and the scheduler | 1200 | 0.0009 ms a frame on average, against the blow-up line of 0.1 | |

None of these is a reading of record: they share the machine with the gate or run hundreds of steps a frame. The reading of record is the scheduler's perf run (D-056 ruling 6).

## 4. The gate

## 5. What is not verified

- No voice has been handed to the engine and nobody has heard the game through this runtime: every run was unattended. The engine sink, the UI flag while paused and the volumes the engine plays are read only by the owner's first attended launch.
- No asset exists yet (`Content/DT/Audio/SFX/` is job 4's): every voice is counted as missing and silent.
- The duck's curve is computed and logged per voice; nothing applies it to the Player group until job 4 makes the classes and the mix.
- The run families (danger, kills, fire, pickups, Keepers), the ladder pitch rule, the density layers, fire density and the heartbeat have seams and no behaviour.
- The Step sound (a settings value moved) is not raised; job 4 has the settings grant.
- A hand that opens and closes in one frame (the unattended auto-pick) raises its pick and no hand-open: in a person's run the hand stays open for frames.
