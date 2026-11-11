# S4 audio wave 1: the audio decisions

For the scheduler. A plan, written by the Sound module session on `plan/s4-audio` from `dev/s4` at fc5000b (D-055 item 8). It starts nothing: it holds a decision draft (§2) and the scope that draft points at. Nothing in the repo was built, launched or changed for it. One thing was run outside the repo: a sketch of the generator and its checks in the session's `Saved\Session\audio_sketch\` (Python standard library, no engine), reported in §9.

A new constant is written here without its prefix, as `AUD.NAME`, until the first job adds the key: the docs lint fails a key that does not exist. Values are proposals, all `GATED(S4)`, all presentation-only.

## 1. What is there today

| Fact | Source |
|---|---|
| No sound path in `Source`, no audio asset, no audio module in `Dusktracer.Build.cs`, no `[Audio]` section in either ini | read on fc5000b |
| The Master volume row already works: `UDTSettingsSubsystem::ApplyVolume` calls `FApp::SetVolumeMultiplier`, so any sound obeys it (R-SET-07, T-SET-07). A tool run reads no settings file and so plays at full volume, not at none | `Settings/DTSettingsSubsystem.cpp` |
| The sim broadcasts `UDTSimSubsystem::OnStepEnd` after every step, on both step paths. At that moment the step's kills, elite deaths, threat actions, telegraphs and status applies are still readable; only `View.Hits` (per-hit position and crit) is already emptied | `Core/DTSimSubsystem.cpp`, `Enemies/DTEnemyPool.cpp` |
| Public delegates exist for hero damaged, down, healed, dash and denied; Intercession cast; level-up; blessing taken and Keeper chosen; boss defeated; session ending | §7.1 |
| Counters that only grow exist for weapon rounds spawned and impacts per slot, kills, gems and chests collected, enemy rounds spawned, offer decisions, rerolls, banishes and locks | §7.1 |
| The kill rate is `UDTEnemySubsystem::GetKillsPerSecond()` over `C.ENM.KILL_RATE_WINDOW_S`. `C.PRE.DENSITY_LOD_KILLS_PER_S` has no reader in code | `Enemies/DTEnemyPool.h` |
| `/Game/DT` is always cooked, so `Content/DT/Audio` ships with no packaging change | `Config/DefaultGame.ini` |
| Only five Tools scripts pass `-nosound`. Smoke, RunTests, Perf, RestartCheck, SeedCheck, MetaCheck, VigilCheck and the package checks do not | `Tools/*.ps1` |
| The licence lint fails any row whose `source`, `used_for`, `author`, `added_by` or `licence_file` holds the letters of the word audio: its AI-service pattern lists `udio` with no word boundary | `Tools/catalog_lint.py:53` |
| This PC has Python 3.14.4 with no numpy, scipy or Pillow. RogueHero's offline mix model (`Tools/sfx_bus.py`) needs numpy and scipy and so cannot run here | probe, 2026-11-11 |
| RogueHero (`E:\RogueHero`, the owner's, MIT) was readable. Its runtime is 2D, engine-free in its scheduler, and has no danger priority, no ducking except hits halved under the crowd layer, no heartbeat and no layer called low end | read at 282df7da |

The owner's taste is on record from RogueHero's playtest of 2026-10-20, after two rounds of fire sounds judged "刺耳" and "单调" (harsh, monotonous): one key, soft sources, a low-pass on every voice, events on a grid, and "保证音效不管如何组合都不能刺耳难听" (no combination may be harsh or ugly). `E:\RogueHero\Docs\Audio\SfxAudition.md` holds the measures that came of it. This plan carries those principles and their measured gates; it copies no generated file.

## 2. Decision draft

In the form of `13_Decisions/`. The scheduler numbers it or returns it.

### D-nnn — Audio wave 1: a ported scheduler, generated sounds in one key, and a band kept for danger

- **Date:** 2026-11-11
- **Decider:** scheduler (delegated, D-009), on a draft by the Sound module session (D-055 item 4)
- **Class:** SYSTEM (presentation; no sim rule or number moves), with the scope calls of the wave
- **Status:** open

#### Context
D-055 item 8 opens audio with a first wave on `dev/s4` and asks for these decisions first. The gap note sizes audio at three rows (§2.3: the runtime, the density layers, the 24 signatures) and the heartbeat of §2.4, and leaves two calls open in its §6.3: whether RogueHero's Art §11 is what "audio signatures" means, and whether a music bed is in scope. The source's rule for density is one sound per kill, a crowd layer fading in from 20 to 60 kills a second, crowd plus low end above 60. The source's rule for sides is that enemy fire and telegraphs get the sharp sounds and player fire is soft, "so danger is heard first" (`reference/original_docs/05_ArtDirection.md` §11). Wave 1's four limits bind (D-043 Option C). §1 above holds the facts found.

#### Options
- A — Port RogueHero's audio whole: runtime, the 330 generated assets' recipes, the submix chain, the echo, tiers and beds. Fastest to a full sound, but its events are RogueHero's, its mix has no rule that keeps a warning above the player's fire, its offline model cannot run on this PC, and most of its size (three timbre tiers, imbue layers, ten fire beds) was tuned by ear over three playtest rounds that Dusktracer has not had.
- B — Engine-native audio: MetaSounds graphs and Quartz. Sample-accurate and flexible, but the sounds become editor-authored binaries that no script regenerates byte for byte, and a new module dependency.
- C — Port the engine-free core of RogueHero's runtime, add what readability needs, generate every sound from recipes in this repo with checks that run without a person, and open the wave family by family.

#### Decision
C, in fourteen items.

1. **Runtime.** A world subsystem in `Source/Dusktracer/Audio/` with an engine-free scheduler and stinger queue ported from RogueHero under DT names. Playback is 2D through the Engine module; no MetaSounds, no Quartz, no change to `Dusktracer.Build.cs` (§3).
2. **Mix.** Sound classes Master over SFX, UI and Music; SFX holds two groups, Player and Danger. One Master volume row, as R-SET-07 has it. Music exists as a class and stays empty (§3.2).
3. **Sounds are generated.** A standard-library Python generator under `Tools/audio/` writes every sound from a recipe in source. The code that makes sound bytes uses only operations IEEE 754 defines exactly, so the bytes do not depend on the C library, the CPU or the Python version. WAV files are build output and are not committed; a text manifest of their hashes is, and so are the imported `.uasset` files, each family after its licence row (§4.1).
4. **One key.** Every pitched sound is written in A minor pentatonic, and voices that vary in pitch draw a degree of it, so no two sounds clash (the owner's RogueHero rule).
5. **A band kept for danger.** The band `AUD.DANGER_BAND_HZ` (2 to 5 kHz) belongs to warnings, as the enemy hue band belongs to enemy fire (R-PRE-02). Player-side and UI sounds stay out of it; a warning has most of its energy in it, a hard onset, no pitch variation and no delay. This is the ear's form of the readability contract (§5).
6. **Danger outranks the player's own sound.** A danger voice is never dropped for a player voice, is handed to the engine in the frame it is asked for, and lowers the Player group by `AUD.DUCK_DB` while it sounds. The low-HP heartbeat lowers the low-end layer further (§5).
7. **Rate limits.** Sound is asked for by count, never per bullet or per enemy: per-sound interval, voice and aggregation rules from the catalog, a cap on voices, and a cap on player-side starts per second. The source's density rule is kept with `C.PRE.DENSITY_LOD_KILLS_PER_S` as its first threshold (§3.4, §3.5).
8. **The first wave** is six families: interface, danger, kills and density, weapon fire, pickups and hero actions, the six Keeper pick signatures. About 86 files from about 47 recipes (§4.3). What is left out is listed with its reason (§4.4).
9. **No music bed in this wave.** The source lets it slip, RogueHero's first arena has none, and a bed is judged by ear alone.
10. **Art §11 is the design for Keeper signatures.** Wave 1 builds the Pick variant of each Path. Tier and Passed-over wait for their HUD lines, which package F held; Imbued waits for a second wave.
11. **Tool runs are silent.** An unattended process hands no sound to the engine: the scheduler still runs and counts, so density is proved without a speaker. A person's launch is the only one that sounds.
12. **The seam.** Audio reads public delegates and public state at `OnStepEnd` and edits no sim file. Its random draws come from its own stream, seeded from the run seed inside `Audio/`. The hit and kill tap of package E is needed only for per-hit position and crit, which wave 1 leaves out (§7).
13. **Counts.** Per sound id: asked, played, merged, dropped. Audio writes them to its own file on request and to no telemetry file in this wave (§7.2).
14. **Checks.** A gate step `audio` (Python, no engine) and automation tests in the `tests` step. What only ears can judge is a listening list for the owner and questions for a later kit (§6).

#### Hypothesis
We believe a first wave of generated sound makes danger heard before it lands and makes density sound rich and not shrill, because warnings own a band the player's fire is kept out of and every player-side sound is asked for by count under a cap; measured now by the gate (each warning stands at least `AUD.DANGER_SMR_MIN_DB` above the loudest player mix the scheduler allows; player-side starts stay at or under `AUD.PLAYER_STARTS_PER_S_MAX` with `C.ENEMIES.CAP` enemies and `C.BULLETS.CAP` rounds), and at the owner's first listening and the first kit that carries sound by two questions: was any hit not heard coming, and did anyone lower the volume or mute. Reverted, by leaving the families unmerged or by shipping the Master row at zero, if any seed 9107 hash moves, if audio's game thread cost in the S4 scene reads over `AUD.GAME_THREAD_MS_MAX` after its caps are lowered (§7.3), or if the owner's listening finds a family harsh after one recipe pass.

#### Impact (dependency sweep)
- Rules replaced: none. `settings.md` R-SET-07 stands. D-043's audio hold was already replaced by D-055 item 8.
- New: `Docs/02_Systems/audio.md` (R-AUD and T-AUD ids, free on all four branches), `Docs/09_Catalog/sounds.csv`, an `AUD` block in `constants.yaml`, `08_Constants.md` and `number_change_log.csv`, audio rows in `licenses.csv`.
- Files of other modules, each the smallest change, for the scheduler to grant or dispatch: §8.3.
- Sim: no file. Proof on every merge as `S4_wave1.md` §2 item 1 has it.
- Retires: the gap note's two open calls on audio (§6.3 there), and the line "the game has no sound yet" in `settings.md` and `ST_UI.csv`.

#### Swap-out point
The first owner listening on `dev/s4`, and the S1 gate decision after session A (D-055's own swap-out), which rereads what audio goes on.

## 3. The runtime

### 3.1 Structure

| Part | What it is | From RogueHero |
|---|---|---|
| `FDTAudioScheduler` | Engine-free. Takes requests (sound id, count, step), applies the catalog rules and the caps, returns voices to start with volume and pitch, runs the density layers and the duck, keeps the counts | Port of `FRHAudioScheduler`, plus danger priority, the duck, the low-end layer, the start cap, step time |
| `FDTStingerQueue` | Engine-free. One stinger per `AUD.STINGER_GAP_S`, entries expire after `AUD.STINGER_EXPIRY_S` | Port of `FRHKeeperStingerQueue` |
| `FDTAudioObserver` | Bound to `OnStepEnd`. Reads counters, step buffers and delegates and turns their changes into requests. Reads only | New: RogueHero's listener was one of its effects |
| `UDTAudioSubsystem` | A tickable world subsystem for game worlds, ticking while paused. Owns the three above, starts voices with `PlaySound2D`, holds one `UAudioComponent` per loop, applies class volumes through one sound mix | Port, without the per-frame `LoadObject` probing: every asset path comes from the generated table and is resolved once |
| `DTSoundsGen.h` | The catalog as a constexpr table, generated from `sounds.csv` | New |

Options weighed for how a sound is asked for: (A) call sites in each system post sounds (RogueHero's adapters; it edits other modules' files and lost events when the listener was re-added); (B) audio subscribes to telemetry records (the sink is a single pointer and its output is take-once); (C) audio observes public state and delegates at step end. C is chosen: it needs no hook for wave 1, it runs once per sim step on both step paths, and it makes a run's requests a function of the seed.

Time is sim time for everything in a run: a request carries its step, and intervals count steps. So a fast bot and a person give the same requests for the same seed, and a voice log can be compared byte for byte. UI sounds run on real time, since the world is paused under them.

### 3.2 Mix

| Class | Holds | Dynamics | Plays while paused |
|---|---|---|---|
| Master | everything | a limiter at `AUD.LIMIT_DBFS`, which the gate expects to stay idle | |
| SFX / Player | fire, hits, kills, both layers, pickups, hero actions | a compressor, RogueHero's `SMX_SFX` settings as the start | no |
| SFX / Danger | warnings, enemy fire, hero hurt and down, heartbeat, boss phase | none: a warning is never turned down by the player's own loudness | no |
| UI | menus, hands, level-up, Keeper stingers | none | yes |
| Music | nothing in wave 1 | | |

Options for the duck: (A) a sidechain compressor on the Player submix keyed by Danger, sample-accurate but hard to model off line; (B) a class volume override driven by the scheduler, with a fade in and out. B is chosen: its gain over time is known to the scheduler, so the off-line check and the voice log hold the same curve the engine is given.

The classes, the mix and the Player submix with its two presets are assets made by the import script, not by hand.

### 3.3 Volume
One Master row, already wired (R-SET-07). No reader is added for it. Audio does read the settings header for one thing, to tick when a value steps, so the player hears the new volume as they set it. Separate SFX and music rows stay out of scope as `settings.md` has it: with no music there is nothing to balance.

### 3.4 Rate limits

| Limit | Proposed | Why |
|---|---|---|
| Per sound: least interval, most voices, aggregation gain | catalog columns; RogueHero's table as the start (fire 70 ms and 2 voices, hit 35 ms and 4, kill 45 ms and 5) | tuned by the owner's ear on RogueHero |
| A batch inside one interval is one voice, louder by `gain x log2(count)`, capped at `AUD.AGGREGATE_MAX` 1.6 | as RogueHero | a hundred hits in a step is one tick, not a hundred |
| `AUD.VOICES_MAX` 32, of which `AUD.VOICES_DANGER` 6 and `AUD.VOICES_UI` 4 are kept | RogueHero held 24 with no reserve | a player voice is stolen first; a danger voice never waits |
| `AUD.PLAYER_STARTS_PER_S_MAX` 60 | new | the bound on "noise": a number the density test reads |
| `AUD.STARTS_PER_FRAME_MAX` 4, danger first; a player voice past it waits a frame | RogueHero's four combat starts a frame | the bound on the game thread's worst frame (§7.3) |
| Fire density: above `AUD.FIRE_DENSE_SHOTS_PER_S` 8 a weapon sounds at most `AUD.FIRE_DENSE_VOICES_PER_S` 6 times a second | as RogueHero, without the beds | the owner's second round |
| Player hit and fire voices wait for a grid of `AUD.GRID_STEPS` 3 sim steps (50 ms) | RogueHero's 44.6 ms grid, made a whole number of steps | overlaps land together; only the sound waits, and never a danger sound |
| Warnings: one per shape per `AUD.WARN_MIN_INTERVAL_MS` 120; more in the interval raise its gain by the aggregation rule | new | ten telegraphs at once are one louder warning |

Audio never walks the bullet or enemy arrays. It reads counts, and at most `AUD.SCAN_MAX` 64 entries of a step buffer.

### 3.5 Density layers

| Kill rate | What is heard |
|---|---|
| up to `C.PRE.DENSITY_LOD_KILLS_PER_S` (20) | one sound per kill |
| above it, until `AUD.LOWEND_ON_KILLS_PER_S` 60 | the crowd loop fades in, reaching full at 60; single kills thin to `AUD.KILL_DENSE_VOICES_PER_S` 8 a second |
| above 60 | crowd and the low-end loop; single kills stay at the thinned rate, so a kill is never silent |

Each layer leaves at a lower rate than it enters (`AUD.CROWD_OFF_KILLS_PER_S` 16, `AUD.LOWEND_OFF_KILLS_PER_S` 50, RogueHero's exits for its visual tiers) and slews at `AUD.LAYER_FADE_PER_S` 3. The rate is the sim's own counter; no sim number is added. Package E will need the same tiers for the eye. One reader should serve both: this plan reads the counter itself and moves to Visual's reader when it exists (§8.3 item 9).

### 3.6 What is ported and what is not

| Ported | Not ported, and why |
|---|---|
| The scheduler's request rules, aggregation and tests | Per-frame `LoadObject` probing for variants: paths come from the table |
| The stinger queue | The echo submix: judged by ear only, and its tail reaches the danger band |
| The crowd layer's hysteresis and slew | Three timbre tiers per weapon level, imbue layers, ten fire beds: about 190 of its 330 files, tuned over three listening rounds |
| Round-robin variants with no repeat, a pentatonic degree and 1 dB per voice | The run root transposition: one more axis to check for no gain a machine can read |
| The Player bus compressor settings, as a start | `sfx_bus.py`: needs numpy and scipy; a standard-library model replaces it |
| The measured gates: 2 to 5 kHz share, onset spike, stream crest | Every generated asset: sounds are regenerated here from Dusktracer recipes |

## 4. The sounds

### 4.1 How they are made

Options: (A) recipes as Python functions over a small synth core, as RogueHero has them; (B) recipes as data read by an interpreter; (C) synthesis in C++ at game start, with no asset. A is chosen. B becomes a language of its own. C would leave the session judging one implementation while the game plays another.

- **Core** (`Tools/audio/dtsynth.py`): oscillators with glide, noise, envelopes, RBJ filters, a soft clip, level and edge finishing, a 16-bit mono WAV writer at `AUD.SAMPLE_RATE_HZ` 48000.
- **Exact operations only.** Sine, exponential and logarithm are written in the core from `+ - * /`, `sqrt`, `floor`, `frexp` and `ldexp`; the random source is an integer xorshift; the soft clip is `s / sqrt(1 + s*s)`. A lint walks the core and every recipe and fails on `random`, on any other `math` function and on `**`. The C library's sine differs in the last bit between CPUs; a 16-bit sample can turn on that bit.
- **Recipes** (`Tools/audio/recipes/<family>.py`): a pure function of the variant index. Variants change decay, transient colour, glide and tail, never pitch.
- **Build** (`Tools/audio/audio_build.py`): writes `Saved/AudioGen/SW_*.wav` and compares each hash with `Tools/audio/sound_manifest.csv`. A changed recipe is committed with its new manifest line, so a diff shows which sounds moved.
- **Import** (`Tools/audio/import_audio.py`, run by `Tools\EditorPython.bat`): SoundWaves to `/Game/DT/Audio/SFX`, classes and mix to `/Game/DT/Audio/Mix`. An editor-only test hashes each SoundWave's PCM against the manifest, so the committed asset is proved to hold the generated bytes.

Committed binaries are the `.uasset` files only (LFS, as every `.uasset` is). The sources of 86 files come to about 3 MB and are rebuilt in seconds (the sketch of §9 built and measured twelve in 10 s), so storing them would add binaries and prove nothing the manifest does not.

### 4.2 Zones

| Side | Where it sits | How it starts | Pitch |
|---|---|---|---|
| Player (fire, hits, kills, layers, pickups, hero actions) | under the danger band: share of energy in it at most `AUD.PLAYER_BAND_SHARE_MAX_DB` -30 dB, read at the highest pitch the catalog lets the engine draw | soft: at least `AUD.PLAYER_ATTACK_MIN_MS` 4 ms, onset spike at most `AUD.SPIKE_MAX_DB` 3 dB | a degree of the key per voice |
| UI | as Player | soft | fixed |
| Warnings | inside the band: share at least `AUD.WARN_BAND_SHARE_MIN_DB` -3 dB | hard: at most `AUD.WARN_ATTACK_MAX_MS` 3 ms, no leading silence | fixed, always: a warning is learned |
| Heartbeat | `AUD.LOW_ZONE_HZ` (40 to 300 Hz), told by its rhythm | two thumps | fixed |
| All | nothing above 6.3 kHz over `AUD.AIR_SHARE_MAX_DB` -30 dB: no hiss | | in key |

Warnings are in key too. An out-of-key alarm was weighed and dropped: the owner ruled on RogueHero that no combination may be ugly, and a warning in a band nothing else uses does not need dissonance to be found.

### 4.3 The first wave, by family

Asset `SW_<Family>_<Name>[_n]`; catalog id `Snd.<Family>.<Name>`. "v" is the number of round-robin files.

| Family (job) | Sounds | Read from |
|---|---|---|
| **Interface** (UI class) | Focus; Step (a settings value moved; on the volume row it plays at the new level); Open; Back; Refuse; HandOpen; Pick; Tool (reroll, banish and lock, as three degrees of one recipe) | changes of `UDTMenuSubsystem::GetState()`, `UDTHubSubsystem::GetState()`, `DTSettings::Live()`, the offer subsystem's open hand, focus and counters |
| **Danger** | Warn.Band, Warn.Arc, Warn.Ring (also the boss ring, spiral and hazard shapes), Warn.Line: four gestures told apart by pip count and contour; Enemy.Fire v2; Hero.Hurt v2; Hero.Down; Hero.Heartbeat; Boss.Phase | new entries of `View.Telegraphs` and the boss's attacks; `FDTEnemyRoundCounters::Spawned`; `OnHeroEvent`; hero HP against `C.HUD.LOW_HP_FRAC`, one beat every `C.HUD.LOW_HP_PULSE_S` of sim time, so the ear and the HP bar pulse together; the boss phase |
| **Kills and density** | Hit v4; Kill v4; Kill.Elite; Layer.Crowd (loop); Layer.LowEnd (loop); Boss.Defeated | `FDTWeaponCounters::Impacts`; `View.Kills` and `GetStepEliteDeaths()` at step end; the kill rate; `OnBossDefeated` |
| **Weapon fire** | one sound for each of the ten `Weapon.*` rows, v4 each; a Reforging form sounds as its base weapon | `FDTWeaponCounters::Spawned` per slot |
| **Pickups and hero actions** | Pickup.Gem (its degree climbs with a streak); Pickup.Chest; Hero.Heal; Hero.LevelUp; Hero.Dash v2; Hero.DashReady; Hero.IntercessionReady; Hero.IntercessionCast (one for all Paths) | `FDTPlayerRunState::Pickups`, chests collected, `OnHeroEvent`, `OnLevelUp`, `DashCharges`, `Intercession01`, `OnIntercession` |
| **Keeper signatures** | Keeper.`<Path>`.Pick for the six Paths: Art §11's motif and texture, as a cell of at most 0.8 s, through the stinger queue | `OnBlessingTaken` (the Path is in the blessing id), `OnKeeperChosen` |

The order is §8.1: interface first because it proves the whole path and can mask nothing; danger before any player-side family, so the game never has fire without warnings.

### 4.4 Left out, and why

| Left out | Why |
|---|---|
| A music bed; ambience | Decision item 9; a bed under everything spends the mask margin and is judged by ear alone |
| Keeper Tier and Passed-over | Their HUD lines are held (package F); a tier is readable as state, so the sound can follow the line when it lands |
| Keeper Imbued ticks | "Low in the mix" is an ear's call; it adds a voice to every Keeper build |
| Crit tick; a hit or kill placed by its position | Needs the tap's per-hit data (package E) |
| Stereo direction for warnings | Needs a listener design and a person to say it helps; all wave 1 sound is centred, as RogueHero's is |
| A spawn arrival cue; a Warden edge cue | D-036's held items: a sound would settle by ear what is waiting for a reading |
| Per-Path Intercession casts; Time Dilation | Every Intercession look is held (D-043); the scale is open |
| Status and reaction sounds; Reforging's own sounds | Wave 1 holds their effects too (`S4_wave1.md` §4) |
| Hub purchase and refusal | The result is private in `Meta/DTTreeTab.h`: it needs an accessor in another module's file |
| Elite spawn, attack landing with no hit, drop-in landing | Each is one more danger-side sound; the wave keeps warnings few so each is learned |
| The echo, timbre tiers, fire beds, run root transposition | §3.6 |
| Separate SFX and music rows in Settings | `settings.md` Out of scope |

## 5. Readability comes first

A sound that warns must never be masked by the player's own fire. Three things hold that, and each has a number.

1. **The band** (per file, static). Player-side and UI files stay out of `AUD.DANGER_BAND_HZ`; warnings live in it (§4.2). The gate reads every file's share of energy in the band, player files at their highest drawn pitch.
2. **Priority and the duck** (runtime, engine-free, tested). Danger voices have kept slots, start in the frame they are asked for, and lower the Player group by `AUD.DUCK_DB` -6 dB within `AUD.DUCK_ATTACK_MS` 20, for the cue's length, released over `AUD.DUCK_RELEASE_MS` 250. While the heartbeat runs the low-end layer is lowered by `AUD.LOWHP_LOWEND_DUCK_DB` -12 dB.
3. **Audibility against the worst mix** (off line, a gate number). `Tools/audio/mix_model.py` runs the scheduler's rules on a trace with every player-side limit full (ten weapons firing, hits and kills past every cap, both layers at full level), renders that mix from the generated files with the catalog volumes and the duck, and reads each danger cue against it: third-octave bands of the cue's zone, a 100 ms window, every alignment 100 ms apart, with the masker spread upward at 5 dB per third octave, which is wider than the ear's at any level a person plays at. A cue is heard through a window when it leads the mix in two bands; the figure is the worst window. Floors: `AUD.DANGER_SMR_MIN_DB` 12 dB for warnings, `AUD.HEARTBEAT_SMR_MIN_DB` 9 dB for the heartbeat in the low zone. A player-side sound read the same way must fail, as a control.

Density has its own reading from the same render, at 5, 20, 60 and 100 kills a second: the player mix's share in the danger band stays under the per-file limit, its crest stays at or under `AUD.MIX_CREST_MAX_DB` 14 dB, its loudness rises by at most `AUD.DENSITY_RISE_MAX_DB` 6 dB from the sparsest to the densest, and its peak with a warning on top stays under `AUD.LIMIT_DBFS`. That is "rich, not shrill" as numbers.

What this does not prove: the model sums files with the scheduler's gains; it is not the engine's mixer, and band energy is a proxy for hearing. And no agent can make the game sound: a launch that is not unattended would play through the owner's speakers, which the standing rule forbids. So the last step, from a voice handed to the engine to air, is first checked by the owner. §6.3 lists what ears must confirm.

## 6. Checking a sound without a person

### 6.1 Gate numbers

Step `audio` (`Tools/audio/audio_check.py`, prints `AUDIO: PASS` or `AUDIO: FAIL`). It generates into `Saved/AudioGen` when the files are missing or a generator file has changed since they were written (a stamp beside them holds the generator's hash), and otherwise reads what is there, in about a second.

| Check | Limit |
|---|---|
| Same bytes | every file's SHA-256 equals its manifest line; the exact-operations lint passes |
| Format | mono, 16-bit, `AUD.SAMPLE_RATE_HZ` |
| Length | inside the family's window in `sounds.csv` (a combat one-shot at most 150 ms, a warning at most 250 ms, a Keeper pick at most 800 ms) |
| Peak and clipping | peak at most `AUD.PEAK_MAX_DBFS` -1 dBFS; no sample at full scale; mean offset under `AUD.DC_MAX` |
| Edges | a one-shot starts and ends at zero; onset within `AUD.LEAD_MAX_MS` 2 ms of the file's start; a loop's seam is no larger than its ordinary sample steps |
| Loudness | the loudest 100 ms K-weighted window (ITU-R BS.1770 weighting), times the catalog volume, inside the family's window; a warning at least `AUD.WARN_OVER_MIX_MIN_DB` 6 dB over the worst player mix |
| Spectrum | the zone rules of §4.2: band share, attack, spike, air |
| Small speaker | a warning, the heartbeat and every UI sound lose at most `AUD.SMALL_SPEAKER_LOSS_MAX_DB` 6 dB of loudness through a 150 Hz high-pass |
| Told apart | for every pair in a confusion set (the four warnings; the UI set; hurt against kill; level-up, chest and pick; the six Keeper picks; heartbeat against the low-end layer): spectral distance at least `AUD.PAIR_DISTANCE_MIN_DB` 6 dB over third-octave spectra, or a different count of onsets, or lengths more than a factor of two apart. A variant differs from its sound by less than the pair limit and by more than zero |
| The mix | §5 item 3 and the density readings |
| Catalog | every `sounds.csv` row has its recipe, its manifest lines and its licence row; `DTSoundsGen.h` matches the CSV |

In the `tests` step (headless, engine-free unless said):

| Test | Reads |
|---|---|
| `Dusktracer.Audio.Density` | a staged trace at `C.ENEMIES.CAP` enemies and `C.BULLETS.CAP` rounds (thousands of hits and a hundred kills a second, ten weapons): voices at most `AUD.VOICES_MAX`, player starts a second at most the cap, each layer switching once, no warning dropped |
| `Dusktracer.Audio.OnePerKill` | under the first threshold every kill sounds and no layer is on |
| `Dusktracer.Audio.DangerFirst` | with every player limit full, a warning starts in its own frame, the duck follows its curve, and no danger voice is stolen |
| `Dusktracer.Audio.Silent` | in an unattended world no voice reaches the engine, and the counts still run |
| `Dusktracer.Audio.SimUntouched` | one seed with audio on and off: equal steps, input hash, hero state and telemetry |
| `Dusktracer.Audio.Cost` | the observer and scheduler under the density trace: a blow-up check at ten times their planned share; the reading itself is §7.3 |
| `Dusktracer.Audio.Assets` (editor only) | every catalog row resolves; frames, rate and PCM hash equal the manifest; loops loop; classes are set |
| `Dusktracer.Audio.Readers` | no sim file includes an `Audio/` header (or the tools test of §8.3 item 3) |

### 6.2 How the session judges a sound it cannot hear
`Tools/audio/audio_view.py` draws, per sound, its waveform and a log-frequency spectrogram with the danger band marked and its numbers printed, a sheet per family, and a chart of third-octave levels of any cues over the worst mix. The session reads those images and the tables before a recipe goes on. §9 shows one. It also writes an audition file per family (each sound alone, then in the mix as the scheduler would play it) for the owner; no tool plays it.

### 6.3 What only ears can judge
A listening list for the owner, about ten minutes on headphones and once on laptop speakers, from the audition files and one run:

0. Do you hear the game at all, and does the Master volume row change it? (No agent has heard it: tool runs are silent by rule.)
1. Is any sound unpleasant, alone or in the busiest minute?
2. Does a warning make you look, without making you wince? Can you tell the four apart after one run?
3. Does your own fire fade into a texture, or does it nag?
4. At a crowd of kills, does it sound richer or only louder?
5. Do hits and kills sound joined to what the screen shows, or late?
6. Is the game's level right beside your other programs at the default volume?
7. Does each Keeper pick sound like its Keeper?
8. Does the heartbeat help, or annoy?

Questions for a later kit: was there a hit you did not hear coming; did you lower the volume or mute, and when; which sound do you remember; could you tell a level-up from a chest with your eyes on the hero.

## 7. The seam

### 7.1 Events

| Moment | Read today, with no hook | Needs |
|---|---|---|
| Weapon fired; rounds hit | `FDTWeaponCounters::Spawned` and `Impacts`, by slot | nothing |
| Kill, elite kill, kill rate | `View.Kills`, `GetStepEliteDeaths()`, `GetKillsPerSecond()` at `OnStepEnd` | nothing |
| Per-hit position and crit | emptied in ResolveHits | the tap of package E; not in wave 1 |
| Telegraph starts | `View.Telegraphs` entries with `Progress` zero, by shape; the boss's `GetAttacks()` | nothing; a developer confirms the zero-progress reading on the first step |
| Enemy rounds fired; threat actions | `FDTEnemyRoundCounters::Spawned`; `GetStepThreatActions()` | nothing |
| Hero hurt, down, healed, dash, denied | `OnHeroEvent` | nothing |
| Low HP; dash ready; Intercession ready | hero state | nothing |
| Intercession cast; level-up; boss defeated; run end | `OnIntercession`, `OnLevelUp`, `OnBossDefeated`, `OnSessionEnding` | nothing |
| Blessing or Keeper picked | `OnBlessingTaken`, `OnKeeperChosen` | nothing |
| Gem, chest | counters | nothing |
| Hand open, focus, pick, reroll, banish, lock | the offer subsystem's state and counters | nothing |
| Menu and hub focus, screen, refused binding | `GetState()` of each | nothing |
| A settings value stepped | `DTSettings::Live()` | one entry in T-SET-02's list of readers |
| Keeper tier line, passed-over line | no runtime code | their HUD lines |
| Hub purchase result | private | an accessor; not in wave 1 |

Never called by audio: `TakeEmissions`, `TakeReactionEvents`, `TakeStartedCasts`, `TakeFinishedCasts`, telemetry's `TakeOutput`. Each has one owner, and a second caller would take its data.

### 7.2 Counts
`11_Quality` §3 asks for trigger, VFX and audio counts per visible effect. Audio's half: for each sound id, asked (the trigger), played, merged into another voice, dropped by a limit. `DT.Audio.Stats` prints them; `-DTAudioLog` writes them and the voice log (step, id, variant, volume, pitch, duck) to `Saved/Audio/<seed>_<time>.csv`. Nothing is added to the telemetry files in this wave: a new record kind is a `telemetry.md` change and would move the byte-identical `bot.csv` proof, and the trigger and VFX halves are package E's. A shared record is a later job for the three modules.

### 7.3 Budget
**What there is to spend.** The perf reading of record (`Validation/S4_perf_1080p.md`, fc5000b, alone on the machine, 1920x1080) puts game thread p95 at 2.45 to 2.53 ms at caps and at 4.76 to 4.78 ms in the S4 scene, against the wave's ceiling of 5.0 ms (`S4_wave1.md` §5 item 5). The S4 scene is the tight one: 0.22 to 0.24 ms are left there, and package E is not built and will spend there too.

**What audio plans to spend.** `AUD.GAME_THREAD_MS_MAX` 0.05 ms p95 per frame in the S4 scene, a quarter of what is left, for everything audio does on the game thread:

| Part | What it does | Planned | Bounded by |
|---|---|---|---|
| Observe | per sim step: a few hundred reads and compares of counters, hero state and at most `AUD.SCAN_MAX` buffer entries; no allocation | under 0.01 ms a frame for both together, at the two steps `C.SIM.MAX_SUBSTEPS` allows | R-AUD-02: counts, never the bullet or enemy arrays |
| Schedule | per step: a few dozen requests against fixed tables sized at start | | the catalog's size |
| Hand to the engine | per frame: one `PlaySound2D` per voice started, volume updates on the loops | the rest | `AUD.STARTS_PER_FRAME_MAX` 4: danger first, a player voice past the cap waits a frame |

RogueHero's whole tick, play calls included, read 0.0099 ms on average and 0.496 ms on its worst single frame. A frame like that does not move a p95 and is still a hitch, so the worst frame is printed beside the p95 and watched against `AUD.FRAME_MS_WATCH` 0.2 ms; the per-frame start cap is what bounds it. If a reading is over, the lever is that cap and the starts-per-second cap. Audio asks for no more of the ceiling.

**How it is measured.**

1. **Audio's own timer**, always on: cycle counts around each of the three parts, every frame. `DT.Audio.Stats` prints p50, p95 and the worst frame per part over the run; a tool run writes the same lines to the audio log, and a person's run writes one closing line to the game log. This is the sharp instrument: it reads microseconds.
2. **`Dusktracer.Audio.Cost`**, in the `tests` step: the density trace of `Dusktracer.Audio.Density` through observer and scheduler. A gate run shares the PC, so it is no reading; it fails at ten times the planned share and catches a blow-up.
3. **The reading of record** is the scheduler's, alone on the machine, as `S4_wave1.md` §2 item 6 has it: `Tools\Perf.ps1 -Scene S4` and `-Scene S2` (the scene that holds a kill rate), three runs each, on the tree before the merge and on the merged tree. Two figures per scene: the timer's p95, and the move of the scene's game thread p95. The second is the cross-check that nothing outside the timer moved. The S4 scene's three runs spread 0.02 ms today, so the before and after resolves about that much: a move inside the spread is reported as inside the spread, not as zero. It is read at the runtime's merge and again at the fire family's, when the load is real.
4. **The play calls are the part no tool run reads.** A tool run is silent (decision item 11), so part three costs nothing there, and the scenes above measure observe and schedule only. Two ways to close that: (A) the start cap bounds it by construction, and its real cost is read from the owner's first listening run, whose closing line holds the timer; (B) a muted perf mode, in which a tool run hands voices to the engine with the Master class and the application volume both at zero, so `Perf.ps1` reads the play calls too. A is planned. B hands voices to the engine in a tool run, so it needs the scheduler's yes; it is worth asking for only if the owner's run reads the play calls near the budget.

Mixing itself is the engine's audio thread, bounded by `AUD.VOICES_MAX`; whether it moves the game thread is what the cross-check of item 3 is for.

### 7.4 What a Shipping package carries
`Content/DT/Audio` cooks with `/Game/DT`: about 86 SoundWaves, the classes, the mix, the submix and its presets. No ini change is planned; if the import shows the engine's default compression or load settings to be wrong for 100 ms one-shots, the job reports it and the change to `Config/` is asked for by name. `Tools/Package.ps1` is not edited. The package check that proves it: the cooked build's `Dusktracer.Audio.Assets` equivalent, run by a later job through `PadCheck`'s launch path.

## 8. The jobs

### 8.1 Order

| # | Branch | Who | Owns | Engine | Done when |
|---|---|---|---|---|---|
| 0 | dispatched by the scheduler | | §8.3 items 1 to 3 | no | the licence lint passes a row that names `Tools/audio/`; the gate has its `audio` step |
| 1 | `s4/audio-tools` | developer | `Tools/audio/` (`dtsynth.py`, `audio_build.py`, `audio_check.py`, `audio_view.py`, `mix_model.py`, `gen_sounds.py`, `audio_test.py`, `recipes/`), `Docs/09_Catalog/sounds.csv`, `sound_manifest.csv`, the `AUD` block, the audio rows of `licenses.csv`, `audio.md` Numbers and Data | no | `AUDIO: PASS` on the interface family; planted faults fail (a clipped file, a player file in the band, a changed byte, a non-exact operation); two processes write the same bytes |
| 2 | `s4/audio-snd-ui` | session | `recipes/ui.py`, its catalog and manifest rows | no | the family passes the step; its sheet is in the note |
| 3 | `s4/audio-runtime` | developer | `Source/Dusktracer/Audio/`, `Source/Dusktracer/Tests/Audio/`, `audio.md` Rules and Acceptance | yes | the tests of §6.1 but Assets; the whole gate; the sim proof |
| 4 | `s4/audio-import` | developer | `Tools/audio/import_audio.py`, `Tools/audio/Audio.ps1`, `Content/DT/Audio/` | yes | `Dusktracer.Audio.Assets`; a tool run with the assets present hands no voice to the engine; the voices a person's launch would start are read through a recording sink; `Validation/S4_audio_ui.md` |
| 5 | `s4/audio-snd-danger`, then `s4/audio-danger` | session, then developer | `recipes/danger.py`; the danger observer file; its assets | recipes no, wiring yes | §5 item 3 passes with warnings alone against a stand-in mix; `DangerFirst` |
| 6 | `s4/audio-snd-kills`, `s4/audio-kills` | as 5 | kills and both layers | as 5 | `Density`, `OnePerKill`; the density readings |
| 7 | `s4/audio-snd-fire`, `s4/audio-fire` | as 5 | ten weapons | as 5 | §5 item 3 on the real worst mix; the scheduler's perf reading |
| 8 | `s4/audio-snd-pickups`, `s4/audio-pickups` | as 5 | pickups and hero actions | as 5 | the family's checks |
| 9 | `s4/audio-snd-keepers`, `s4/audio-keepers` | as 5 | six picks, the stinger queue's wiring | as 5 | the six told apart pairwise |
| 10 | `s4/audio-evidence` | developer | `Tools/audio/AudioRun.ps1`, `Validation/S4_audio_mix.md` | yes | a real run's voice log rendered off line and read as §5; the owner's audition files in `Saved\S4Report\Audio` |

### 8.2 What runs side by side
Jobs 1 and 2 need no engine and can run while `alone.txt` stands. Every recipe job (the session's, one at a time) needs no engine and overlaps any developer job. Jobs 3 and 4 own different files and take turns at the one heavy slot; one built worktree serves both, and the same developer is kept for the wiring jobs. Each family adds one observer file and its own block of catalog rows, so two families do not edit one file.

### 8.3 Asked of the scheduler

| # | File (owner) | The smallest change | Blocks |
|---|---|---|---|
| 1 | `Tools/catalog_lint.py`, `Tools/tools_test.py` | `udio` becomes `\budio\b` in `AI_SOURCED`, with a test that a row naming `Tools/audio/x.py` passes and one naming the service fails | the first licence row |
| 2 | `Tools/Gate.ps1`, `Tools/tools_test.py` (`PY_STEPS`), `11_Quality.md` §1 | one line after `palette`: `Invoke-PyStep "audio" "Tools\audio\audio_check.py" "AUDIO"`, its header comment, its stand-in entry, its table row | the gate step |
| 3 | `Tools/tools_test.py` | the R-PRE-11 include check also treats `Audio/` as presentation: no sim file includes an `Audio/` header | job 3's proof |
| 4 | `Tools/tools_test.py` (`ENTRY_PS`) | `audio/Audio.ps1` and `audio/AudioRun.ps1` join the scripts held to the mask rule | jobs 4 and 10 |
| 5 | `Content/DT/Text/ST_UI.csv`, `settings.md` (Mechanics) | `Settings.Hint.Volume` in en and zh-Hans, and the Master volume row's "the game has no sound yet" | job 4; text for a native reader |
| 6 | `Tests/Settings/DTSettingsTests.cpp` (Mechanics) | the one audio file that reads the settings header joins T-SET-02's readers | job 4 |
| 7 | `AGENTS.md` | asset prefixes `SW_` (wave), `SC_` (class), `SMX_` (mix and submix) | none; naming |
| 8 | the Tools launchers without `-nosound` | add it, as a second lock behind decision item 11. The scheduler's call: the flag also removes the audio device, which item 11 does not | none |
| 9 | Visual session | whether package E builds one density-tier reader that audio then reads | none; §3.5 |
| 10 | Visual session | the tap's event shape, when it lands | a second wave only |

`Tools/gen_catalog.py` is not asked for: `Tools/audio/gen_sounds.py` reads `sounds.csv` with that tool's own `read_csv` and writes `Audio/DTSoundsGen.h`, checked by the `audio` step, so no shared generator and no shared generated header is edited. If the scheduler would rather have `DTCat::Sounds` in `DTCatalogGen.h`, that is one function there and this plan changes by one file.

## 9. The method, shown (S2)

A sketch of the core, six recipes, the checks and the pictures: about 700 lines of standard-library Python in the session's `Saved\Session\audio_sketch\`, run under the processor mask. Nothing of it is committed. A whole run takes 10 s.

- **Same bytes.** Two processes wrote 12 of 12 files byte-identical. The exact-operations lint passes on the core and the recipes. This is one PC and one Python; that the bytes hold on another machine follows from the construction and was not tried.
- **Per file** (variant 0; the picture is `out\sheet.png` beside the sketch):

| Sound | Active ms | Attack ms | Peak dBFS | Loudest 100 ms, LK | Share in 2 to 5 kHz, dB | Small-speaker loss, dB | Onsets |
|---|---|---|---|---|---|---|---|
| Fire (Pulse Rifle), 4 variants | 119 to 130 | 7.1 to 7.2 | -8.3 to -7.2 | -19.5 | -54.3 to -52.0 | 0.5 | 1 |
| Kill, 4 variants | 107 to 108 | 4.8 to 7.2 | -8.8 to -8.2 | -19.3 | -52.0 to -49.8 | 0.0 | 1 |
| UI accept | 208 | 5.9 | -12.3 | -20.4 | -34.8 | 0.0 | 2 |
| Telegraph warning | 146 | 1.9 | -2.0 | -10.6 | -0.4 | 0.0 | 2 |
| Heartbeat | 365 | 18.6 | -4.8 | -18.5 | -110.3 | 8.6 | 2 |
| Low-end loop | 2000 | | -7.0 | -17.7 | -80.5 | 5.3 | |

- **The band rule at the highest drawn pitch** (four semitones up): fire -40.4 dB, kill -34.5 dB, UI accept -35.1 dB, against a limit of -30.
- **Told apart.** Spectral distance between any two of the six is 14.8 to 29.7 dB, but for heartbeat against the low-end loop at 5.6 dB: they share a zone and differ by rhythm, which is why the pair rule has three legs. Fire variants differ from each other by 1.4 to 5.6 dB.
- **The mix.** A player mix of 20 fire voices and 20 kills a second at the catalog volumes reads -21.3 LK with -38.0 dB of its energy in the danger band. The warning stands +25.8 dB over it in its worst window with no duck and +31.8 dB with the 6 dB duck (floor 12). The heartbeat stands +7.3 dB over fire, kills and the low-end loop with no duck and +13.3 dB with 6 dB (floor 9). The control reads -43.9 dB: a UI sound cannot be heard through the mix in the warning band, as it should not.
- **The checks changed two recipes**, which is the point of them. The first kill sat 11.6 dB from the fire sound; moved up an octave it sits 24.0 dB away. The first heartbeat lost 11.6 dB on a small speaker; with stronger second to fourth harmonics it loses 8.6 dB, which still fails the proposed 6 and needs another pass.
- **What the picture shows.** The fire and kill sounds are falling tones under 700 Hz with almost nothing above 1.5 kHz; the warning is two pips at 3.1 and 2.6 kHz with a body an octave under each; the heartbeat is two thumps under 250 Hz. The fire sound is darker than it need be: its limit leaves it 10 dB more top than it uses.

## 10. What this plan does not know

- No sketch sound was heard by anyone. Every claim about character is from numbers and a picture.
- The worst mix of §9 holds two of the six families. The floors of §5 are set from it and may move when the real mix exists; each move is a `number_change_log.csv` row.
- Whether `View.Telegraphs` shows a new telegraph at zero progress on its first step was read by a subagent in code, not run.
- Engine behaviour not tried: how a class volume override fades on voices already playing, what the default compression does to a 100 ms one-shot, whether an unattended run with a live audio device logs per frame as RogueHero's map reloads did.
- Whether the engine's off-line renderer can write the real mix to a file with no device. If it can, job 10 reads the engine's own output; it is a probe, not a promise.
- The sizes: no job here is sized in days. RogueHero's decision log dates its audio layer from 2026-10-18 to 2026-10-20, with three listening rounds by the owner inside those days.
- `Docs/Validation/S4_artbook_gaps.md` §2.3 names a Flare Pylon; the roles in code are Swarm, Tank, Rusher, Turret and Boss, so enemy fire is one sound for the Turret's rounds.
