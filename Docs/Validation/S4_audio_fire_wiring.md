# S4 audio fire wiring: the fire observer, the limiter, the heartbeat's form b

The fire family's wiring job (`Validation/S4_audio_wave1.md` §8.1, D-056 and its Result of 2026-11-13). Branch `s4/audio-fire` from `s4/audio-snd-keeper` 1cb689b, with `s4/audio-danger`, then `s4/audio-snd-keeper` 8032b0e (the wave's rows `delegated 2026-11-13`, their SoundWaves) and `dev/s4` 1618624 merged in. The spec is `02_Systems/audio.md`; the model is `S4_audio_mix.md`.

**Nothing has been heard.** Every run of this job was a tool process: no engine sink was made and the application volume was held at zero. No ear has heard a heartbeat under fire; the owner's first sitting with fire is the reading that counts.

## 1. What landed, and what proves it

| Item | Files | Proof |
|---|---|---|
| The heartbeat's form b is the game's: `C.AUD.LOWHP_HOLD_DB` (-4 dB); in Shipping the key, elsewhere `dt.Audio.LowHpHoldDb` defaults to it and 0 still gives form a (a duck per beat) | `constants.yaml`, `08_Constants.md`, the change log, `Audio/Observe/DTObserveDanger.cpp`, R-AUD-12 | `Dusktracer.Audio.Heartbeat`, `Dusktracer.Audio.LowHpHold` (form a at 0 equal, sample for sample, to the runtime without the hold) |
| The limiter: `SMX_Game`, one submix for the game, holding `SMX_GameLimiter`, a dynamics processor as a limiter at `C.AUD.LIMIT_DBFS`; every class of R-AUD-04's tree names it as its Default Submix (the engine hands a class's submix to no child) | `Tools/audio/import_audio.py`, `audio_select.py`, `Content/DT/Audio/Mix/SMX_Game*`, the six `SC_*`; R-AUD-04 | `Dusktracer.Audio.Classes` (the submix, its one effect, type Limiter, threshold, hard knee, all six classes routed) |
| The fire observer: per slot of the local player, the growth of `FDTWeaponSim::GetCounters(0, Slot).Spawned` (through `UDTWeaponSubsystem::GetSim() const`; the slot's weapon from `GetLastSpec`, which an evolved weapon keeps) raises `Snd.Fire.<Name>`, one request a firing with a count of one (its sound is its whole volley; the fire rows' `agg_gain` is 0) | `Audio/Observe/DTObserveFire.h/.cpp`, `Audio/DTAudioObserver.cpp`, `sounds.csv`, `DTSoundsGen.h` | `Dusktracer.Audio.Observer` (Fire moments, a count of one whatever the growth, baselines, every table weapon and every evolve row resolving), `Catalog.test_every_weapon_has_its_fire_row_and_every_fire_row_its_weapon` |
| R-AUD-07's last clause: a weapon sounds at most `C.AUD.FIRE_DENSE_VOICES_PER_S` times a second, at any rate (the threshold key is removed: a rule where a faster weapon sounded less often than a slower one was a fault); a withheld firing is counted as asked and merged | `FDTFireDensity`, `FDTAudioScheduler::CountMerged`, `IDTAudioRequests::Merged` | `Dusktracer.Audio.FireDensity` (7 to 22 firings a second: at most 6 voices in any second, evenly spread; 2 and 6 a second: every firing; the counts add up through the scheduler) |
| The sim untouched with the fire observer on | — | `Dusktracer.Audio.SimUntouched`: the armed bot run asks for fire; steps, input hash, hero state and telemetry equal the run with audio off |
| Every fire row playable and given its asset | — | `Dusktracer.Audio.FirePlayable` (editor): ten of ten |
| The mix model: the fullest build at the cap; a batched cue read at its cap; the pairs; the crest judged on the flood tier; the heartbeat on two lines | `Tools/audio/mix_model.py`, `audio_check.py`, `audio_test.py` (class `Mix`); R-AUD-18; `S4_audio_mix.md` §3 | the tool tests; the `audio` step |
| Floor 601 to 603 (FireDensity, FirePlayable) | constants, `08_Constants.md`, the change log | the gate's `tests` step |

## 2. The limiter, and how often it would act

`SMX_GameLimiter`: type Limiter, threshold `C.AUD.LIMIT_DBFS` (-1 dBFS), peak detection, hard knee (0 dB: the engine's default 10 dB knee would start lowering 5 dB under the line), attack 1 ms (the engine's least) behind a 3 ms look-ahead, release 100 ms. Each is a named constant of `import_audio.py`; the threshold is the key. The import's first run saved the eight assets (`SMX_Game`, `SMX_GameLimiter`, the six classes); a second run saved nothing.

On the model's densest fight (the flood tier, the fullest build), it would never act on the fight alone (its peak -4.6 dBFS) nor under one danger cue on top (the loudest, enemy fire at its batch cap, -1.70 dBFS). It acts when two danger sounds start in one frame: of the 35 pairs that can, 5 pass the line with nothing under them and 22 on the ducked densest fight. Worst bare: Ring with enemy fire at its cap +0.20 dBFS; worst on the mix: HeroHurt with BossPhase +1.10, then Ring with enemy fire +1.07 and Band with Ring +0.94.

Enemy fire, read at `C.AUD.AGGREGATE_MAX` (+4.1 dB, the count the game asks it with is the step's rounds), peaks at -1.70 dBFS on the ducked densest fight: 0.70 dB under the limit for the danger wiring that has already landed, with the limiter behind it. It plays -18.8 LK at the cap.

## 3. The model's readings on this tree

Fire: the fullest build a run holds, `C.WEAPON.SLOTS` = 4 weapons, the loudest fire rows by played level, each at the cap of 6 a second: Rail Lance (-28.4 LK), Gravity Mine (-28.7), Scatter Cannon (-29.9), Flak Mortar (-30.4); 24 voices a second. The cap is the bound because no catalog row bounds the fire-rate multiplier (`Transform.Quicksilver`, Slipstream, takes Flag.SpeedScale's cap away). No role is a stand-in. These are the baseline a later change is read against.

| Kills/s | Weapons | Beds | Asked | Merged | Started | Dropped | LK int | Danger band dB | Crest dB (first seed) | Median | Peak dBFS |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | 3 | none | 966 | 705 | 259 | 0 | -24.3 | -46.7 | 14.0 | — | -8.4 |
| 20 | 4 | crowd | 3452 | 3117 | 335 | 0 | -22.2 | -50.6 | 13.1 | 13.3 (printed) | -7.4 |
| 60 | 4 | crowd, low | 9712 | 9377 | 334 | 0 | -21.6 | -50.5 | 13.4 | 13.0 (printed) | -6.2 |
| 100 | 4 | crowd, low | 16199 | 15864 | 334 | 0 | -21.9 | -50.2 | 15.4 | 14.0 (judged) | -4.6 |

- The flood's crest reads 14.0 against 14, with no room; it is read again when the gem becomes a role of the model.
- The rise, 5 to 100 kills a second: 2.4 dB (limit 6).
- The cues over the densest fight (audibility ducked / with no duck; over the mix; peak on top):

| Cue | SMR | No duck | Over mix | Peak dBFS |
|---|---|---|---|---|
| `Snd.Warn.Line` | 30.6 | 24.6 | 8.4 | -3.38 |
| `Snd.Warn.Band` | 35.6 | 29.6 | 11.0 | -3.74 |
| `Snd.Warn.Ring` | 39.3 | 33.3 | 9.0 | -3.39 |
| `Snd.Warn.Arc` | 40.6 | 34.6 | 9.5 | -5.04 |
| `Snd.Danger.EnemyFire` | 16.3 | 10.3 | — | -1.70 (at its cap) |
| `Snd.Danger.HeroHurt` | — | — | — | -3.33 |
| `Snd.Danger.HeroDown` | — | — | — | -3.84 |
| `Snd.Danger.Heartbeat` (held, median of five seeds) | 9.4 | 5.4 | — | -3.51 |
| `Snd.Danger.BossPhase` | — | — | — | -3.37 |

- The control, the loudest fire file read as a warning: -31.7 dB, far under the floor.

## 4. The heartbeat under form b, in numbers

At the hold of -4 dB, over the flood tier's five seeds: 9.4, 9.4, 9.4, 7.1 and 7.4 dB of audibility in the low zone. The scheduler's ruling (2026-11-13, 04:08) judges it on two lines: the median, 9.4 against `C.AUD.HEARTBEAT_SMR_MIN_DB` (9); the least draw, 7.1 against `C.AUD.WARN_OVER_MIX_MIN_DB` (6), the floor a warning must clear (3.1 with nothing lowered on that draw). Both pass. Judged on the least draw alone against 9, as the first draft of this job had it, it failed at 7.1.

The hold was not deepened. A hold of -7 dB would take 7 dB from every player's fire in every low-health stretch, and put a 7 dB step on a health line with no hold-off, to cover two draws of five on a build only an unbounded fire rate reaches. If the least draw ever falls under 6, the answer is not a deeper hold for everyone: it goes to the scheduler as a draft of a hold that deepens only in the flood tier, with a hold-off on leaving low health. Not built.

In the runtime (`Dusktracer.Audio.LowHpHold`): the Player group goes down to -4 dB once, within the duck's attack, when health turns low, stays level across the beats with no send to the engine, goes to `C.AUD.DUCK_DB` and back to the hold for a warning, and returns to zero within the release.

## 5. Per-id counts from a run

From `Dusktracer.Audio.SimUntouched`'s armed bot run (seed 31, Steered Storm Tempest, 45 s of sim time, a frame after every step), in the gate: asked 270 in all.

| Id | Asked | Played | Merged | Dropped |
|---|---|---|---|---|
| `Snd.Fire.PulseRifle` | 87 | 87 | 0 | 0 |
| `Snd.Fire.RingEmitter` | 13 | 13 | 0 | 0 |
| `Snd.Fire.OrbitalDrones` | 133 | 133 | 0 | 0 |
| the other seven fire ids | 0 | 0 | 0 | 0 |
| `Snd.Warn.Line` | 9 | 8 | 1 | 0 |
| `Snd.Warn.Band` | 2 | 1 | 1 | 0 |
| `Snd.Danger.EnemyFire` | 24 | 21 | 3 | 0 |
| Ring, Arc, HeroHurt, HeroDown, Heartbeat, BossPhase | 0 | 0 | 0 | 0 |

The bot held three weapons; none fired past the cap, so nothing of the fire was withheld. The density's withholding is proven by `Dusktracer.Audio.FireDensity`, not by this run.

Silence in the gate's staged fight (`Saved\Logs\Smoke_combat_staged_en.log`): `Audio: start: tool process; no engine sink; volume held at 0; 92 of 92 wave(s) resolved; duck mix found` and `Audio: end: asked 173, voices started 72, handed to the engine 0, missing 0`.

## 6. The gate

`GATE: PASS (9425e1d; 3 not run; mask FFFFF3FC)` with `dev/s4` 1618624 merged: tests `603 run of 603 found, 0 failed, floor 603, 0 crash marker(s)`; restart `276faf8e / 276faf8e`; meta `ba2a3ac8 / ba2a3ac8`; `AUDIO: PASS (49 sounds: 0 heard, 49 delegated, 0 rejected, 0 not heard; 92 files, 79 tool tests; mix: 9 cue(s) over the catalog's player side (35 pair(s): 5 bare and 22 on the mix over C.AUD.LIMIT_DBFS, for the limiter); ...)`; `LICENCE: PASS (199 file(s) against 22 row(s))`; `TOOLS: PASS (424 tests)`; build, shipping and devgame at 0 warnings; not run: pad (no package), scope (no brief in flight), mac (deferred, D-007). The commit after it is this note.

## 7. Cost

`& Tools\Perf.ps1 -Scene S4` on 9425e1d: `PERF: PASS (1920x1080, game p50 3.32 / p95 4.79 ms, GPU p95 3.71 ms, load 5816.00 bullets / 400.00 enemies, 1800 frames)`, and audio's own timer line `Audio: 6103 frame(s); p95 ms observe 0.0052 schedule 0.0018 hand 0.0003; worst 0.0261 0.0125 0.0025`. The three p95 add up to 0.0073 ms, under `C.AUD.GAME_THREAD_MS_MAX` (0.05 ms); the scene asked 3408 sounds and started 487 (the danger wiring's reading was 0.0075 ms, asked 131: the fire family adds no cost that this reading resolves).

## 8. What is not verified

- **Nothing has been heard.** No ear has heard a heartbeat under fire, the fire family in a fight, or the limiter acting; the owner's first sitting with fire is the reading that counts.
- The limiter in the engine: it is made, routed and read back by the test, but only a person's launch hands a voice to the engine, so no tool run has passed a sound through it. Its acting is a model reading (§2).
- The fire in a real run of a full build: the bot held three weapons and none passed the cap; the four-weapon build at the cap is the model's, not a run's.
- The heartbeat in a real run: no run of this job went low on health.
- The flood's crest at 14.0 has no room and is read again with the gem in the model.
