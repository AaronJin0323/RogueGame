# Audio — what is heard (S4 wave, the Sound module)

> Written with the plan `Validation/S4_audio_wave1.md` (D-055, its audio item); the plan is accepted as D-056, with its rulings. The jobs of its §8.1 fill each section before their first line of code. The keys are in `constants.yaml` (`C.AUD.*`, job one). A test is planned until its job builds it; the runtime job (job three) built the rules of the seam, the mix's danger priority, scheduling and counts, and the tests that name them.

## Intent
The player hears danger before it lands, hears their own power as a texture that grows richer with the crowd and never shrill, and hears every menu answer. Sound never changes a run: the same seed plays the same with sound on, off or absent. Every sound is made by a script in this repo and is made again byte for byte.

## Rules
### Seam
- **R-AUD-01** Audio reads the sim and never writes it (`04_Presentation` R-PRE-11). No sim file includes a header of `Audio/`. Audio draws its random numbers from its own stream, seeded from the run seed inside `Audio/`, and from no sim stream.
- **R-AUD-02** Audio observes once per sim step, at `UDTSimSubsystem::OnStepEnd` (run sounds), and once per frame (interface sounds), through public delegates, counters that only grow, and the step's own buffers while they still stand. It never calls a take-once queue that has another owner, and never walks the bullet or enemy arrays: it reads counts, and at most `C.AUD.SCAN_MAX` entries of a step buffer. Requests inside a run carry their step, and intervals and a voice's length are counted in steps; interface sounds run on real time. The frame's flush runs at the end of the world's tick, after the sim has stepped, paused or not. A new run resets the run side only; the interface's voices, interval times and waiting requests belong to the world. One observer file per family (`Audio/Observe/`); a family adds a file and edits none.
- **R-AUD-03** A tool run is silent. A tool process is one launched `-unattended`, or one on a desktop that is not the default one (every tool launch goes through `Start-DTHidden`'s private desktop, a packaged check included). A silent process is a tool process, or one running automation tests. In a silent process the engine sink is never made, so no voice is handed to the engine; the scheduler still runs and counts. As a second lock (D-056, its ruling on silence), a tool process sets the application volume to zero at its start, holds it there every frame whatever set it since, and never gives it back; a test run from a person's editor leaves their volume alone. With `dt.Audio.Enabled` at zero when a world is made, the world has no audio runtime at all.

### Mix
- **R-AUD-04** Sound classes: Master over SFX, UI and Music; SFX holds Player and Danger. Master volume is the Settings row (`settings.md` R-SET-07); no other volume row exists. UI sounds play while the world is paused. Music is empty.
- **R-AUD-05** Danger is heard first. A danger voice has kept slots (`C.AUD.VOICES_DANGER`), is never dropped for a player voice, is never delayed or pitch-varied, and reaches the engine in the frame it is asked for, first among that frame's voices and past the frame cap if it must. With no slot free it takes the slot of the oldest player voice (that engine voice ends on its own; the slot is counted as stolen). While one sounds the Player group is lowered by `C.AUD.DUCK_DB`, in within `C.AUD.DUCK_ATTACK_MS` and out over `C.AUD.DUCK_RELEASE_MS`, linear in dB on real time; the scheduler gives the curve, and the engine side applies it. While the heartbeat runs the low-end layer is lowered by `C.AUD.LOWHP_LOWEND_DUCK_DB`.

### Scheduling
- **R-AUD-06** A sound is asked for by id and count. Each id has its least interval, most voices and aggregation gain in `09_Catalog/sounds.csv`. Requests of one id in one step (in one frame for an interface sound) are one voice, louder by one plus the aggregation gain times log2 of their count, and never by more than `C.AUD.AGGREGATE_MAX`. A request inside the id's interval of the voice that sounds merges into it: it is counted and never carried.
- **R-AUD-07** Caps: at most `C.AUD.VOICES_MAX` voices, with `C.AUD.VOICES_DANGER` and `C.AUD.VOICES_UI` kept; a player voice never takes a kept slot; at most `C.AUD.PLAYER_STARTS_PER_S_MAX` player-side starts in any second of sim time and `C.AUD.STARTS_PER_FRAME_MAX` starts a frame, danger first: an interface or player batch past it waits one frame and is then dropped; above `C.AUD.FIRE_DENSE_SHOTS_PER_S` shots a second a weapon sounds at most `C.AUD.FIRE_DENSE_VOICES_PER_S` times a second.
- **R-AUD-08** Density follows the one density tier that eye and ear share (`impact_art.md`, package E; D-056 as ruled). In the per-kill tier every kill sounds; in the crowd tier the crowd layer fades in and single kills thin to `C.AUD.KILL_DENSE_VOICES_PER_S`; in the flood tier the low-end layer joins. Audio holds no rate of its own: the first threshold is `C.PRE.DENSITY_LOD_KILLS_PER_S`, the others are package E's keys, and audio reads the tier from package E's reader. A layer slews at `C.AUD.LAYER_FADE_PER_S`. A kill is never silent.
- **R-AUD-09** Player voices of a grid row start on the next step that is a multiple of `C.AUD.GRID_STEPS`, so overlaps land together. Only the sound waits; play never does, and a danger sound never does.
- **R-AUD-10** Variants play in turn (round robin), so a file never plays twice running. A `key` voice is written on a note of the key (its catalog `note`) and draws one of that note's own five degrees, so every draw stays in the key, up to `C.AUD.KEY_SEMIS_MAX`, and a level within `C.AUD.LEVEL_JITTER_DB`, from audio's own stream; a `fixed` voice and every danger voice never vary. A `ladder` voice plays as written until its family brings its rule.
- **R-AUD-11** Stingers queue: one per `C.AUD.STINGER_GAP_S`, dropped after `C.AUD.STINGER_EXPIRY_S`.
- **R-AUD-12** The heartbeat sounds while a living hero is at or under `C.HUD.LOW_HP_FRAC`, one beat every `C.HUD.LOW_HP_PULSE_S` of sim time, in step with the HP bar's pulse (`hud.md` R-HUD-05). It stops with the sim.

### Sounds
- **R-AUD-13** Every sound is generated by `Tools/audio/` from a recipe in source, with operations IEEE arithmetic defines exactly, and its bytes equal its line in `Tools/audio/sound_manifest.csv`. No sample, loop, recording or library enters the repo, and nothing is copied from another project's generated files (`04_Presentation` R-PRE-10). A family's licence row lands before its first binary.
- **R-AUD-14** One key: every pitched sound is written in A minor pentatonic.
- **R-AUD-15** The danger band `C.AUD.DANGER_BAND_HZ` belongs to warnings. A player-side or interface file has at most `C.AUD.PLAYER_BAND_SHARE_MAX_DB` of its energy there at the highest pitch it can be drawn at (`C.AUD.KEY_SEMIS_MAX` up for a key sound, `C.AUD.LADDER_SEMIS_MAX` for a ladder), and a soft start (`C.AUD.PLAYER_ATTACK_MIN_MS`, `C.AUD.SPIKE_MAX_DB`). A warning has at least `C.AUD.WARN_BAND_SHARE_MIN_DB` there and a hard start (`C.AUD.WARN_ATTACK_MAX_MS`). The heartbeat sits in `C.AUD.LOW_ZONE_HZ`, with at least `C.AUD.LOW_ZONE_SHARE_MIN_DB` of its energy there. No file has more than `C.AUD.AIR_SHARE_MAX_DB` above the air line.
- **R-AUD-16** File hygiene: mono at `C.AUD.SAMPLE_RATE_HZ`; length inside its family's window; peak at most `C.AUD.PEAK_MAX_DBFS` and no sample at full scale; a one-shot starts and ends at zero with its onset inside `C.AUD.LEAD_MAX_MS`; a loop has no seam, in its sample steps or in level (`C.AUD.LOOP_SEAM_LEVEL_MAX_DB`); played loudness inside its family's window; a warning, the heartbeat and every interface sound keep their loudness through a small speaker (`C.AUD.SMALL_SPEAKER_LOSS_MAX_DB`).
- **R-AUD-17** Two sounds a player must tell apart differ by `C.AUD.PAIR_DISTANCE_MIN_DB` in spectrum, or in their count of onsets, or by `C.AUD.PAIR_LENGTH_RATIO_MIN` in length, variant against variant. The sets are named in the catalog.
- **R-AUD-18** Against the loudest player mix the scheduler allows, every warning leads by `C.AUD.DANGER_SMR_MIN_DB` and the heartbeat by `C.AUD.HEARTBEAT_SMR_MIN_DB`, in two bands of its zone at its worst alignment. From the sparsest to the densest mix the player side's share in the danger band holds its limit and its loudness rises by at most `C.AUD.DENSITY_RISE_MAX_DB`. Its crest stays under `C.AUD.MIX_CREST_MAX_DB` in the tiers where a layer is on: a sparse mix is single sounds with silence between, and peak over RMS says nothing there. The crest is judged once no role of the mix is a stand-in, as the median over the model's seeds; until then it is printed and never an error.
  - *The reading as built* (`Tools/audio/mix_model.py`; `Validation/S4_audio_mix.md`). The player side is five roles: fire (one voice stream per weapon: every row of family `Fire`), `Snd.Kills.Hit`, `Snd.Kills.Kill`, `Snd.Kills.LayerCrowd`, `Snd.Kills.LayerLow`; a role with no catalog row plays its stand-in from `Tools/audio/recipes/standin.py` at the levels of the model's own table (`STANDIN_WEAPONS`: rifle `x4`, heavy `x3`, light `x3`), and the output names the roles that were stand-ins. The tiers are `TIERS = (5, 20, 60, 100)` kills a second with `WEAPONS_FIRING = (3, 6, 10, 10)` weapons firing. Each weapon asks `C.AUD.FIRE_DENSE_VOICES_PER_S` voices a second; under `C.PRE.DENSITY_LOD_KILLS_PER_S` every kill asks one, from there `C.AUD.KILL_DENSE_VOICES_PER_S` a second; hits land at `HITS_PER_KILL = 20` a second per kill a second (T-AUD-07's sizes; a model constant, not a key: if the game ever reads it, it becomes a key with a change-log row). Cadences are jittered from one fixed seed. The rules of R-AUD-06, R-AUD-07, R-AUD-09 and R-AUD-10 are applied as the runtime does, per sim step and per frame of `FRAME_HZ = 60`, with the louder candidate first wherever a cap binds; a start with no budget or slot is dropped and counted, and a layer that is on holds `LAYER_SLOTS = 1` (a model constant, as above) of the player side's `C.AUD.VOICES_MAX` − `C.AUD.VOICES_DANGER` − `C.AUD.VOICES_UI` slots. The crowd layer is on from `C.PRE.DENSITY_LOD_KILLS_PER_S`, the low layer from `C.IMP.DENSITY_FLOOD_KILLS_PER_S`, at full level. Each tier is rendered `TIER_SECONDS = 8.0` and read past its first `SKIP_SECONDS = 1.0`: integrated K-weighted loudness, share in the danger band (every tier), crest (printed for every tier on the one seed; judged only when no role is a stand-in, and then in each tier with a layer on, as the median over `CREST_SEEDS = 5` seeds, the run's own first: the extra seeds are rendered only then), the rise from the sparsest tier to the densest. The danger side is read against the densest tier's mix lowered by `C.AUD.DUCK_DB` (the duck holds while the cue sounds; its attack is left out), every variant, the worst kept: a `warn` zone row's audibility in the danger band (the figure with no duck printed beside it); a `low` zone row's in `C.AUD.LOW_ZONE_HZ`, with the low layer lowered by `C.AUD.LOWHP_LOWEND_DUCK_DB` as well; a row of family `Warn` its loudest `lk100` window over the mix's by `C.AUD.WARN_OVER_MIX_MIN_DB`; every danger row the peak of the mix with it on top, at alignments `C.AUD.MASK_WINDOW_MS` apart, at most `C.AUD.LIMIT_DBFS`. The loudest player-side one-shot read as a warning must stay under `C.AUD.DANGER_SMR_MIN_DB`, or the step fails: the model is broken. With no danger-side row in the catalog nothing is read and the verdict says the mix checks are not active yet.
- **R-AUD-19** Each imported asset holds the generated bytes: frames, rate and PCM hash equal the manifest.
- **R-AUD-22** A sound becomes an asset and can play only when its catalog row is `yes` or `delegated`; a row that is `no` or `rejected` holds its id and nothing else: the runtime loads no asset for it, so even a stale local import stays silent. Every new family goes to the owner as audition files before it is wired (`Validation/S4_audio_world.md` §6).

### Counts and cost
- **R-AUD-20** For each sound id audio counts asked, played, merged and dropped (`11_Quality` §3), prints them on request (`DT.Audio.Stats`) and, on `-DTAudioLog`, writes them with the voice log to its own file, never to a telemetry file.
- **R-AUD-21** Everything audio does on the game thread (observing, scheduling, handing voices to the engine) costs at most `C.AUD.GAME_THREAD_MS_MAX` a frame at the p95 of the perf scenes, and audio times its own three parts on every frame with the cycle counter so the figure can be read: p50, p95 and the worst frame per part, on `DT.Audio.Stats` and in one closing log line, over the last frames a fixed ring holds; nothing grows per frame. The worst frame is printed and watched against `C.AUD.FRAME_MS_WATCH`.

## Numbers
Values live in `08_Constants.md` (Audio); every key is presentation-only and `GATED(S4)`.

| Constant | Key | Tag | Dependents |
|---|---|---|---|
| Sample rate | C.AUD.SAMPLE_RATE_HZ | GATED(S4) | R-AUD-16 |
| Danger band, low zone, air line | C.AUD.DANGER_BAND_HZ, C.AUD.LOW_ZONE_HZ, C.AUD.AIR_LINE_HZ | GATED(S4) | R-AUD-15 |
| Band shares | C.AUD.PLAYER_BAND_SHARE_MAX_DB, C.AUD.WARN_BAND_SHARE_MIN_DB, C.AUD.AIR_SHARE_MAX_DB | GATED(S4) | R-AUD-15 |
| Starts | C.AUD.PLAYER_ATTACK_MIN_MS, C.AUD.SPIKE_MAX_DB, C.AUD.WARN_ATTACK_MAX_MS, C.AUD.LEAD_MAX_MS | GATED(S4) | R-AUD-15, R-AUD-16 |
| File limits, small speaker | C.AUD.PEAK_MAX_DBFS, C.AUD.DC_MAX, C.AUD.SMALL_SPEAKER_HZ, C.AUD.SMALL_SPEAKER_LOSS_MAX_DB | GATED(S4) | R-AUD-16 |
| Pair distance | C.AUD.PAIR_DISTANCE_MIN_DB | GATED(S4) | R-AUD-17 |
| Audibility reading and floors | C.AUD.MASK_SPREAD_DB, C.AUD.MASK_WINDOW_MS, C.AUD.DANGER_SMR_MIN_DB, C.AUD.HEARTBEAT_SMR_MIN_DB, C.AUD.WARN_OVER_MIX_MIN_DB | GATED(S4) | R-AUD-18 |
| Mix limits | C.AUD.MIX_CREST_MAX_DB, C.AUD.DENSITY_RISE_MAX_DB, C.AUD.LIMIT_DBFS | GATED(S4) | R-AUD-18, R-AUD-04 |
| Voices | C.AUD.VOICES_MAX, C.AUD.VOICES_DANGER, C.AUD.VOICES_UI | GATED(S4) | R-AUD-07 |
| Start caps | C.AUD.PLAYER_STARTS_PER_S_MAX, C.AUD.STARTS_PER_FRAME_MAX, C.AUD.FIRE_DENSE_SHOTS_PER_S, C.AUD.FIRE_DENSE_VOICES_PER_S | GATED(S4) | R-AUD-07 |
| Aggregation, scan | C.AUD.AGGREGATE_MAX, C.AUD.SCAN_MAX | GATED(S4) | R-AUD-06, R-AUD-02 |
| Layers | C.AUD.LAYER_FADE_PER_S, C.AUD.KILL_DENSE_VOICES_PER_S | GATED(S4) | R-AUD-08 |
| Grid, warnings | C.AUD.GRID_STEPS, C.AUD.WARN_MIN_INTERVAL_MS | GATED(S4) | R-AUD-09, R-AUD-06 |
| Duck | C.AUD.DUCK_DB, C.AUD.DUCK_ATTACK_MS, C.AUD.DUCK_RELEASE_MS, C.AUD.LOWHP_LOWEND_DUCK_DB | GATED(S4) | R-AUD-05 |
| Variation | C.AUD.KEY_SEMIS_MAX, C.AUD.LEVEL_JITTER_DB | GATED(S4) | R-AUD-10 |
| Stingers | C.AUD.STINGER_GAP_S, C.AUD.STINGER_EXPIRY_S | GATED(S4) | R-AUD-11 |
| Cost | C.AUD.GAME_THREAD_MS_MAX, C.AUD.FRAME_MS_WATCH | GATED(S4) | R-AUD-21 |

Read and not owned: `C.PRE.DENSITY_LOD_KILLS_PER_S`, `C.ENM.KILL_RATE_WINDOW_S`, `C.HUD.LOW_HP_FRAC`, `C.HUD.LOW_HP_PULSE_S`.

## Data
| Field | Type | Notes |
|---|---|---|
| `09_Catalog/sounds.csv` | catalog | one row per sound id: family, side, class, recipe, variants, loop, volume, pitch rule, least interval, most voices, aggregation gain, grid, length and loudness windows, confusion set, `note` (the note a `key` sound is written on: A, C, D, E or G; empty for any other pitch rule), and `heard`: `no`, or `yes`, `delegated` or `rejected` with its ISO date (R-AUD-22); the import job reads it and imports `yes` and `delegated` only |
| `Audio/DTSoundsGen.h` | generated | the catalog as a table, with each key sound's written note, whether the row may play, and each sound's longest variant in ms from the manifest; written by `Tools/audio/gen_sounds.py`, never by hand |
| `Audio/DTAudioScheduler`, `Audio/DTStingerQueue` | engine-free | the rules of R-AUD-05 to R-AUD-11 and the counts; they take the table as a view, so tests pass their own rows |
| `Audio/DTAudioObserver`, `Audio/Observe/` | reader | the step and frame observer and one file per family |
| `Audio/DTAudioSubsystem` | world subsystem | owns the three above, resolves assets once, starts voices through a sink, holds both silence locks, the counts and the timer |
| `Tools/audio/sound_manifest.csv` | generated, committed | one line per file: name, frames, SHA-256, its measured numbers |
| `Tools/audio/audio_metrics.py` | source | defines the measures the rules read: the activity line, the attack point, the spike windows, the onset detector, the level-seam window and the spectral distance's floors are named constants there, not keys |
| `Tools/audio/recipes/<family>.py` | source | one pure function per sound over `Tools/audio/dtsynth.py` |
| `Tools/audio/recipes/standin.py` | source | a helper module: drafts of the player side of a fight (three weapons, hit, kill, the two layers), played by `mix_model.py` for a role the catalog has no row for yet; it holds no catalog sound and nothing of it becomes an asset |
| `Tools/audio/mix_model.py` | source | R-AUD-18's off-line mix: the roles, the tiers, the schedule and the readings; its own constants (roles, stand-in levels, tiers, weapons firing, hits per kill, slots a layer holds, render length, seed, `CREST_SEEDS`) are named at its top, its limits are keys. `--render <dir>` writes audition files and plays nothing |
| `Saved/AudioGen/mix.json`, `Saved/AudioGen/standins/` | build output | the mix readings, keyed on the snapshot of the recipes and the core, the catalog, the manifest, the files read, the keys read, `mix_model.py` and `audio_metrics.py`; the rendered stand-ins as exact doubles |
| `Content/DT/Audio/SFX/SW_*` | asset | SoundWaves, imported by `Tools/audio/import_audio.py` |
| `Content/DT/Audio/Mix/*` | asset | the classes, the mix, the Player submix and its presets, made by the same script |
| `Tools/audio/Audio.ps1`, `Tools/audio/AudioRun.ps1` | heavy entry scripts | the import and a real run's voice log; each takes the processor mask and then a heavy-job slot (`07_Technical` R-TEC-18). Every other audio tool is plain Python and takes none |
| `Saved/AudioGen/` | build output | the WAV files; not committed |
| `Saved/Audio/<seed>_<time>.csv` | run output | on `-DTAudioLog`: the voice log (step, time, id, variant, volume, pitch, duck in dB, count), then the counts and the timer |

Interface events (the interface family, `Audio/Observe/DTObserveInterface`). Read once a frame from the menu's, the hub's and the offer subsystem's getters, for the local player (player zero, whose hand the offer screen shows); each line compares this frame with the last. Its `.cpp` is audio's one reader of a `Settings/` header (`settings.md` T-SET-02's list), and stays the only one: the Step sound will read the settings from the same file.

| Sound | Condition |
|---|---|
| `Snd.UI.Open` | the menu screen changes to one at least as deep (none, then pause and run-over, then settings, controls and confirmation), or to the run-over screen; the hub becomes active; the hub leaves its root for a page |
| `Snd.UI.Back` | the menu screen changes to a shallower one or closes; the hub returns to its root |
| `Snd.UI.Focus` | on the same open menu screen the focus row or the controls column moves; on the same hub page the focus moves; within the same open hand the focus moves |
| `Snd.UI.Refuse` | an open menu's status changes to reserved, or to a key of the wrong device |
| `Snd.UI.HandOpen` | an open hand has an id the last frame did not show, and it is not the reroll of the hand the last frame showed |
| `Snd.UI.Pick`, `Snd.UI.Reroll`, `Snd.UI.Banish`, `Snd.UI.Lock` | the offer subsystem's decision, reroll, banish or lock count grew; one request carrying the growth |
| `Snd.UI.Step` | not raised yet: the settings value step belongs to the next job |

Decided in the runtime job, where the plan left it open: interface sounds batch per frame; a voice holds its slot for its file's length (steps for run sounds, real seconds for interface sounds); danger takes a player voice's slot when none is free; the starts-per-second window is a second of sim time; a hand that opens and closes inside one frame (an unattended auto-pick) raises its pick and no hand-open; the interface is the world's, not the run's, and is not reset by a new run.

## Acceptance
Gate step `audio` (`Tools/audio/audio_check.py`) and the `tests` step.

- T-AUD-01 `Dusktracer.Audio.SimUntouched`: one seed with audio on (observing, and asked for sounds) and off ends with equal steps, input hash, hero state and telemetry; no sim file includes an `Audio/` header (`Tools/tools_test.py`, the R-PRE-11 check).
- T-AUD-02 `Dusktracer.Audio.Observer`: each moment of the interface event table raises its request once, on staged views, and an unchanged frame raises none; a fresh world reads as nothing shown; no file of `Audio/` names a take-once queue. `Dusktracer.Audio.NewRun`: a new run drops the run side only; the interface's voice, interval and waiting request stay.
- T-AUD-03 `Dusktracer.Audio.Silent`: the two predicates on staged inputs (unattended; a hidden desktop with no flag; automation alone; a person); in the test process no engine sink exists while voices are scheduled, and in a tool process the volume reads zero straight after the world's start and again after a tick that follows a write.
- T-AUD-04 PLANNED(S4) `Dusktracer.Audio.Classes`: every sound sits in its catalog class; interface sounds carry the paused flag.
- T-AUD-05 `Dusktracer.Audio.DangerFirst`: with every player limit full a warning starts in its own frame, first and off the grid; the duck follows its curve; with no slot free danger takes player slots and no danger voice is dropped.
- T-AUD-06 `Dusktracer.Audio.Aggregate`: a second request inside the interval merges; a batch is louder and capped.
- T-AUD-07 `Dusktracer.Audio.Density`: a staged trace at `C.ENEMIES.CAP` enemies and `C.BULLETS.CAP` rounds (ten weapons, thousands of hits and about a hundred kills a second, on test rows) holds every cap, and no warning is dropped.
- T-AUD-08 PLANNED(S4) `Dusktracer.Audio.Layers`: in the per-kill tier every kill sounds and no layer is on; each layer follows the tier, entering and leaving once per change of tier.
- T-AUD-09 `Dusktracer.Audio.Grid`: player hit and fire voices start on the grid; a danger voice does not wait.
- T-AUD-10 `Dusktracer.Audio.Variation`: for every written note, no variant twice running; every drawn pitch, as the note plus its offset, is a pitch class of A minor pentatonic, and levels are within `C.AUD.LEVEL_JITTER_DB`; a fixed row and a warning never vary; one seed gives one voice log.
- T-AUD-11 `Dusktracer.Audio.Stingers`: the gap, the expiry, and a stinger under full load.
- T-AUD-12 PLANNED(S4) `Dusktracer.Audio.Heartbeat`: it runs exactly while the HUD's low-HP state does, on the same period, and stops with the sim.
- T-AUD-13 PLANNED(S4) the `audio` step: every file's hash equals its manifest line; the generator holds no operation that is not exact; every catalog row has its recipe and its licence row.
- T-AUD-14 PLANNED(S4) the `audio` step: a recipe names no pitch outside the key.
- T-AUD-15 PLANNED(S4) the `audio` step: the band shares and starts of every file, by side.
- T-AUD-16 PLANNED(S4) the `audio` step: format, length, peak, edges, loop seam, loudness, small speaker.
- T-AUD-17 PLANNED(S4) the `audio` step: every pair of every confusion set.
- T-AUD-18 the `audio` step: the audibility floors and the density readings of the off-line mix, with a player-side sound failing as a control (`Tools/audio/audio_test.py`, class `Mix`, on test cues made from `dtsynth` and passed as data): a ping of `E7` and `G7` passes the audibility and over-mix floors and fails both `30 dB` down; a thump of `A1` and `A2` passes or fails the low-zone floor by level; a stand-in with a strong `G7` part fails the band share; the mix raised `12 dB` fails the peak; the rise fails past its key; with a stand-in in the mix a crest over its key is printed and is not an error, with every role given as data it is one in each tier with a layer on and never in a tier with none, judged on the median of the seeds' crests, whose extra renders happen only then; the control stays under the floor and fails the step when a broken measure lifts it; two runs give the same samples; no second of a tier holds more starts than the cap, no frame more than its cap, no instant more voices than the slots; the step reads the mix only with a danger-side row, names the stand-ins in its verdict, and recomputes when a part of its cache key changes and not otherwise.
- T-AUD-19 PLANNED(S4) `Dusktracer.Audio.Assets` (editor): frames, rate and PCM hash of each SoundWave equal the manifest; loops loop.
- T-AUD-22 the `audio` step: every row's `heard` cell is one of its four forms with a real date, and the verdict counts heard, delegated, rejected and not heard (`Tools/audio/audio_test.py`); the import's refusal of a row that is not `yes` or `delegated` is planned with the import job.
- T-AUD-20 `Dusktracer.Audio.Counts`: asked equals played plus merged plus dropped, for every id, once nothing waits, and after a reset drops what waited.
- T-AUD-21 `Dusktracer.Audio.Cost`: the density trace through observer and scheduler stays under ten times their planned share, a blow-up check; the reading of record is the scheduler's perf run before and after a merge, with audio's own timer beside it.
- HUMAN: no tool hears. The listening list of `Validation/S4_audio_wave1.md` §6.3 is the owner's; each family is reported as not heard until it is.

## Telemetry
None in this wave. Counts go to audio's own file (R-AUD-20); a shared effect-count record is a later change to `telemetry.md`.

## Text
No new string. `ST_UI.Settings.Hint.Volume` stops saying the game has no sound; the change is asked of its owner.

## Out of scope
A music bed and ambience; Keeper tier, passed-over and imbued sounds; per-hit position and crit; stereo direction; a spawn arrival cue and a Warden edge cue (D-036); per-Path Intercession casts; status, reaction and Reforging sounds; hub purchases; an echo, timbre tiers, fire beds and a run root; separate SFX and music volume rows. Reasons: `Validation/S4_audio_wave1.md` §4.4.

## Open
| Question | Data that resolves it |
|---|---|
| Do the audibility floors hold on the real worst mix? | the `audio` step once the fire and kill families exist |
| Does a warning make a player look without wincing? | the owner's listening list; a kit question |
| The kills family waits for package E's density-tier reader | package E merged on `dev/s4` |
| Does the engine's off-line renderer write the real mix with no device? | a probe in the evidence job |
