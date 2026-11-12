# S4 audio: the warnings and the danger family, as drafts

Branch `s4/audio-snd-danger`, on `s4/audio-snd-ui-stone` at 60c4f28, with the mix model's branch `s4/audio-mix` and the import job's branch `s4/audio-import` merged, so that it gates once on the tree `dev/s4` will be. It adds nine catalog rows in two families, their recipes, their manifest lines and their rows of the generated table. Every row reads `no` in the catalog's `heard` column, so none becomes an asset and none can play (`02_Systems/audio.md` R-AUD-22). There is no engine code here; the generated table gains nine ids, which changes its shape, so the branch owes a whole gate. The Sound session wrote the recipes and judged them from numbers and pictures.

## 0. Why the rows read `no`

- A danger row stays `no` until its branch carries the mix model and shows a warning over a fight; then it becomes `delegated` on the scheduler's word, unless the owner has spoken. The model is here now and §3 holds its readings; the word is the scheduler's.
- The owner has had the nine as audition files since 2026-11-12 (`Saved\S4Report\Audio\2026-11-12_danger_draft\` of the session's worktree), and a model of a fight with them on top (`Saved\S4Report\Audio\2026-11-12_fight_draft\`). Nothing waits on them and no answer has come.
- Two things in those folders differ from these rows. The warnings play 2 dB louder there: the model showed a warning on top of the densest mix peaking over the limiter's line, so the catalog volumes came down. And the heartbeat there is an earlier one, on A; §1 says why it moved.

## 1. Two families, nine sounds

`Warn` holds the four warnings of an attack to come. `Danger` holds what else must be heard first: enemy fire, the hero struck and fallen, the heartbeat, the Warden's phase. Both are on the danger side and in the Danger class: kept slots, never varied, heard in the frame they are asked for, and the player's side steps back while one sounds (R-AUD-05). They are two families because the loudness floor of R-AUD-18 is for a warning, and enemy fire, which is heard many times a second, is kept quiet on purpose.

A warning belongs to the Keep by its material and stands apart by its size: a small bronze thing struck hard rings inside the danger band, where nothing else of this world lives. The four differ by gesture, so no ear for pitch is needed.

| Id | What it is | Meant for | Zone | Active ms | Attack ms | Loudest 100 ms, LK | Played, LK | Share in the danger band, dB | Small-speaker loss, dB | Onsets |
|---|---|---|---|---|---|---|---|---|---|---|
| `Snd.Warn.Line` | one damped strike | an aim line: a volley is coming | warn | 63.0 | 1.5 | -12.9 | -16.8 | -0.5 | 0.0 | 1 |
| `Snd.Warn.Band` | two strikes, the second lower | a lane on the floor: leave it | warn | 173.7 | 1.3 | -10.3 | -14.2 | -0.3 | 0.0 | 2 |
| `Snd.Warn.Ring` | three quick damped strikes | a ring, a spiral, a hazard edge | warn | 175.4 | 1.7 | -12.3 | -16.2 | -0.3 | 0.0 | 3 |
| `Snd.Warn.Arc` | one strike left to ring, with a shimmer | a sweep close by | warn | 319.3 | 1.7 | -11.8 | -15.7 | -0.3 | 0.0 | 1 |
| `Snd.Danger.EnemyFire`, two files | dark iron, a dry tick | enemy rounds leave a barrel | warn | 40.7, 43.9 | 1.3, 1.7 | -20.4, -18.4 | -24.8, -22.8 | -0.1, -0.3 | 0.0 | 1 |
| `Snd.Danger.HeroHurt`, two files | a heavy damped plate and a crack over it | the vessel is struck | free | 219.9, 210.3 | 3.8, 9.7 | -13.2, -13.3 | -16.1, -16.2 | -57.8 | 4.6 | 1 |
| `Snd.Danger.HeroDown` | a great bell struck once; its pitch sinks as it dies | the vessel falls | free | 1959.2 | 18.2 | -15.9 | -16.8 | -77.0 | 4.5 | 1 |
| `Snd.Danger.Heartbeat` | the vessel's own pulse: two low beats on C, dry | low health, every beat | low | 355.7 | 15.6 | -16.9 | -17.8 | -90.1 | 4.3 | 2 |
| `Snd.Danger.BossPhase` | a mechanism turns over under a low bell | the Warden changes phase | free | 1961.1 | 133.4 | -15.4 | -16.3 | -79.7 | 6.5 | 1 |

All are fixed in pitch. The warnings ring on E7, G7 and D7 with partials that are notes of the key. The helper `clunk` (a heavy latch of the old machine) joins `Tools/audio/recipes/world.py`; the ten interface files did not move by a byte.

**The heartbeat sits between the fight's notes.** A fight in this world rests on A and E (the stand-ins' fire on A1, A2 and E3, the low layer on A and E). The first heartbeat was on A, in the same third-octave bands as the fire, and the model read it at 9.3 dB against a floor of 9. It now beats on C, with its weight on the note, its octave and its double octave: the bands at 63, 125 and 250 Hz, which the fight's A and E leave open. The model reads 12.3 dB, and it lost less on a small speaker (4.3 dB against 4.9).

The catalog's least interval of a warning is 120 ms, the value of `C.AUD.WARN_MIN_INTERVAL_MS`. Nothing ties the cell to the key yet; the wiring job adds that check or drops the key.

## 2. Told apart

Two confusion sets. The warnings' distances are the session's reading of the same bytes (the built files equal the judged drafts, by hash); the heartbeat's are read from the built file.

| Pair | Spectral distance, dB | It holds by |
|---|---|---|
| Line, Band | 6.6 | distance, strikes (1, 2), length |
| Line, Ring | 9.2 | distance, strikes (1, 3), length |
| Band, Ring | 5.3 | strikes (2, 3) |
| Arc, Line | 1.1 | length alone (319 ms against 63) |
| Arc, Band | 8.1 | distance, strikes |
| Arc, Ring | 9.8 | distance, strikes |
| EnemyFire against the four | 7.8 to 16.5 | distance |
| HeroDown, HeroHurt | 8.3 | distance, length |
| Heartbeat, HeroHurt | 10.3 | distance, beats (2, 1) |
| Heartbeat, HeroDown | 7.2 | distance, beats, length |
| BossPhase, HeroHurt | 10.8 | distance, length |
| BossPhase, HeroDown | 6.5 | distance alone |
| BossPhase, Heartbeat | 7.3 | distance, beats, length |

## 3. What the mix model reads

`python -I Tools\audio\mix_model.py`, on this branch: the nine rows against a player side made of stand-ins for fire, kills and both layers (`Validation/S4_audio_mix.md` has the method and the tiers). Each cue is read against the densest mix, lowered by the duck as it would be while the cue sounds.

| Cue | Audibility in its zone, dB | The same with no duck | Loudest 100 ms over the mix, dB | Peak with it on top, dBFS |
|---|---|---|---|---|
| `Snd.Warn.Line` | 32.9 | 26.9 | 7.7 | -3.56 |
| `Snd.Warn.Band` | 37.9 | 31.9 | 10.2 | -4.23 |
| `Snd.Warn.Ring` | 41.6 | 35.6 | 8.3 | -2.37 |
| `Snd.Warn.Arc` | 42.9 | 36.9 | 8.8 | -5.26 |
| `Snd.Danger.EnemyFire` | 18.6 | 12.6 | | -4.58 |
| `Snd.Danger.HeroHurt` | | | | -3.57 |
| `Snd.Danger.HeroDown` | | | | -3.82 |
| `Snd.Danger.Heartbeat` | 12.3 | 6.3 | | -3.77 |
| `Snd.Danger.BossPhase` | | | | -3.83 |

Floors and limits: `C.AUD.DANGER_SMR_MIN_DB` for the five of zone warn, `C.AUD.HEARTBEAT_SMR_MIN_DB` for the heartbeat in the low zone, `C.AUD.WARN_OVER_MIX_MIN_DB` for the four of family `Warn`, `C.AUD.LIMIT_DBFS` for every peak. All nine hold. The control, a stand-in's fire read as a warning, reads -66.7 dB and fails, as it must.

The audio step on this branch: `AUDIO: PASS (19 sounds: 0 heard, 10 delegated, 0 rejected, 9 not heard; 21 files, 71 tool tests; mix: 9 cue(s) over stand-ins for fire, kills, layers; 0 rebuilt, 20.8 s)`. The import's selection on it: `SELECT: PASS (10 asset(s) of 19 row(s))`: the nine rows that read `no` yield no asset.

The test `Dusktracer.Audio.Playable` is narrowed here to the interface's rows. As the import job wrote it, it expected every playable row to be raised by an interface moment, which holds only while every playable row is an interface sound; a danger row accepted before its observer exists would have failed it for a reason it is not about.

**The crest is printed and not judged.** Before the scheduler's ruling of 2026-11-12 this branch failed on one line that is not about these sounds: the stand-in mix's crest at 100 kills a second, 14.1 dB against `C.AUD.MIX_CREST_MAX_DB`. By the ruling (R-AUD-18 as it now reads) the crest is judged only where no part of the mix is a stand-in and a layer is on, as a median over the model's seeds. It is worth knowing now that the stand-in mix would not pass that reading: its medians are 14.1, 15.0 and 14.4 dB in the three dense tiers. The first real family is read against the limit before it is tuned.

## 4. What is not verified

- Nobody has heard these in a fight. The first interface sounds passed every number and were wrong in style.
- Two pairs hold by one leg. Arc and Line are the same strike, damped or left to ring: they differ in length alone. BossPhase and HeroDown are both long, low and of bronze: they differ by 6.5 dB of spectrum against a floor of 6. Each wants an ear; the second can be moved apart at no cost if the owner hears them as one.
- The heartbeat reaches its floor only with the duck (6.3 dB without it): its audibility rests on the heartbeat being a danger voice that lowers the player's side on every beat. Enemy fire holds with no duck by 0.6 dB and by 6.6 with it.
- The readings are against stand-ins. The real fire and kills families will sit on other notes and levels; the heartbeat's place between A and E holds only while the fight keeps to them.
- Whether a turret's fire, sounding many times a second and lowering the player's side each time, makes a fight pump. The model can draw the duck; it cannot hear it.
- BossPhase loses 6.5 dB on a small speaker. The rule binds warnings, the heartbeat and the interface, so it passes; on a laptop the Warden's phase is a faint sound.
- Nothing raises these sounds. The danger observers are the wiring job's.
