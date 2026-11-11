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

## 1a. What a recipe branch can change in `DTSoundsGen.h`

The scheduler asked whether the generated table can be checked without a compile that proves anything new when only recipes change. It can. `audio_test.py` `test_a_data_cell_changes_values_never_the_shape` holds the table's shape: for one set of ids, a change of any data cell (volume, interval, voices, aggregation gain, grid, loop, variants, pitch rule, side and class, the lengths read from the manifest) changes only initialiser values on that id's row of `DTCat::Sounds`, never a type, an enum, an enum member, a field or their order. The cells the table does not emit (the length and loudness windows, zone, recipe, confusion set) leave the text byte for byte as it was. Since the review, the table also carries a key sound's written `note` and `bPlayable` (from `heard`), so a change of `heard` or `note` is a data change too: a value in its row, never the shape. So a recipe branch that keeps its ids changes, at most, the numbers, booleans and enum values inside existing rows; `gen_sounds.py --check` in the `audio` step proves the header matches the catalog, and C++ that compiled against the old header compiles against the new one, since every name it can use is unchanged. What it cannot do without a compile: add, remove, rename or reorder an id (that changes `ESoundId`'s members and `SoundCount`, which the observers name), or add a column the table emits (a new field of `FSoundRow`). Either changes the shape, and the test shows a new id or a new order does. The behaviour those values drive is still the runtime's tests' to prove; a new value inside a row needs no new compile proof, only the tests that read it.

## 1b. The review (eleven changes)

- **Silence.** Two pure predicates, driven by staged inputs in `Dusktracer.Audio.Silent`: `IsToolProcess` (`-unattended`, or a desktop that is not `Default`, read with `GetThreadDesktop` and `GetUserObjectInformationW` under `PLATFORM_WINDOWS`) and `IsSilent` (a tool process, or automation testing). Lock 1 follows `IsSilent`, lock 2 follows `IsToolProcess` only and never gives the volume back. A packaged check started with no `-unattended` sits on `Start-DTHidden`'s private desktop and is now a tool process.
- **The key.** `sounds.csv` has a `note` column; each written note draws its own five degrees (A -5 -2 0 +3 +5, C -5 -3 0 +2 +4, D -5 -2 0 +2 +5, E -4 -2 0 +3 +5, G -5 -3 0 +2 +5); `C.AUD.KEY_SEMIS_MAX` is 5, held by a `static_assert`. The Variation test checks every draw against the scale's pitch classes, for every note.
- **R-AUD-22 at runtime.** The table carries `bPlayable`; `ResolveAssets` loads nothing for a row that may not play.
- **Order.** The frame's flush is bound to `FWorldDelegates::OnWorldTickEnd`. Verified by reading `Engine/Source/Runtime/Engine/Private/LevelTick.cpp` of 5.8: `UWorld::Tick` runs `FTickableGameObject::TickObjects` (every tickable world subsystem, the sim's included) at line 1821, and broadcasts `OnWorldTickEnd` at line 2061, at the end of the function, paused or not. So a warning asked for at a step's end starts in that frame whatever order the subsystems tick in. The subsystem's own `Tick` keeps only the volume lock and the observer's binding.
- **A new run** resets the run side only (`FDTAudioScheduler::ResetRun`); `Dusktracer.Audio.NewRun` holds it.
- **dt.Audio.Enabled 0** at world creation: no audio subsystem; SimUntouched compares a run with the runtime against one without it.
- **Tests that now hold their rule:** DangerFirst has players of six ids and warnings competing for the frame cap (fails if danger-first ordering goes); Density asserts the player voice limit, the player starts in a second and the frame cap are each reached; Stingers asserts the dropped counts.
- **Cost.** The timer is a fixed ring of 8192 frames per part; `Frame()` reuses member arrays.
- `FDTStingerQueue::NumPaths` is `EDTPath::Count`.

## 2. The sim proof (`S4_wave1.md` §2 item 1)

The five seed 9107 arms were run one at a time with `Tools\BotRuns.ps1 -Arms <arm> -FirstSeed 9107 -Seeds 1`, on ce2d5d1 and again on e340553, the tip the last gate built; both readings are the after column below. Each run launches the game unattended with audio on: it observed, scheduled and counted (one run's closing line: 102 frames, observe p95 0.0731 ms at 600 steps a frame). Before is the baseline of record, `S4_p0.md` §8, second table, which `S4_e0_tap.md` read again after the tap.

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

- On e340553 (`dev/s4` a155ace merged), the gate of record: `GATE: PASS (e340553; 3 not run; mask FFFFF3FC)`, with tests `584 run of 584 found, 0 failed, floor 584`, restart `276faf8e / 276faf8e` (5400 steps, csv same), meta `ba2a3ac8 / ba2a3ac8` (1200 steps, csv same), `AUDIO: PASS (10 sounds: 0 heard, 0 delegated, 10 rejected, 0 not heard; 10 files, 53 tool tests; not active yet: the mix checks (R-AUD-18); 0 rebuilt, 3.5 s)`, `TOOLS: PASS (363 tests)`, build, shipping and devgame at 0 warnings. Not run: pad (no package), scope, mac (D-007). The gate waited for a machine slot first (the machine was held alone) and then ran.
- Earlier, on ce2d5d1: `GATE: FAIL at tests`, 584 run of 584, 1 failed: `Dusktracer.Settings.Readers` (T-SET-02), because the interface observer includes a `Settings/` header. Fixed by the one-reader change of §1. A later run on c0c153b was stopped with the machine and is void.
- The smoke scenes ran the runtime unattended; their closing lines read, for example, 4591 frames of `arena_empty` at p95 0.0022 / 0.0008 / 0.0001 ms (observe, schedule, hand), worst 0.0093 / 0.0194 / 0.0005.

## 5. What is not verified

- No voice has been handed to the engine and nobody has heard the game through this runtime: every run was unattended. The engine sink, the UI flag while paused and the volumes the engine plays are read only by the owner's first attended launch.
- No asset exists yet (`Content/DT/Audio/SFX/` is job 4's): every voice is counted as missing and silent.
- The duck's curve is computed and logged per voice; nothing applies it to the Player group until job 4 makes the classes and the mix.
- The run families (danger, kills, fire, pickups, Keepers), the ladder pitch rule, the density layers, fire density and the heartbeat have seams and no behaviour. The duck is computed and not applied (the import job applies it through the sound mix). The run seed is read through the sim's non-const accessor, since it has no const one; audio only reads it.
- The Step sound (a settings value moved) is not raised; job 4 has the settings grant.
- A hand that opens and closes in one frame (the unattended auto-pick) raises its pick and no hand-open: in a person's run the hand stays open for frames.
