# S4 audio: the warnings and the danger family, as drafts

Branch `s4/audio-snd-danger`, on `s4/audio-snd-ui-stone` at 60c4f28. It adds nine catalog rows in two families, their recipes, their manifest lines and their rows of the generated table. Every row reads `no` in the catalog's `heard` column, so none becomes an asset and none can play (`02_Systems/audio.md` R-AUD-22). There is no engine code here; the generated table gains nine ids, which changes its shape, so the branch owes a whole gate. The Sound session wrote the recipes and judged them from numbers and pictures.

## 0. Why the rows read `no`

- A danger row stays `no` until its branch carries the mix model and shows a warning over a fight; then it becomes `delegated` on the scheduler's word, unless the owner has spoken. The mix model is its own job (`s4/audio-mix`). This note gains its readings when the two are merged.
- The owner has had the nine as audition files since 2026-11-12 (`Saved\S4Report\Audio\2026-11-12_danger_draft\` of the session's worktree), and a model of a fight with them on top (`Saved\S4Report\Audio\2026-11-12_fight_draft\`). Nothing waits on them and no answer has come.
- The warnings in the first folder play 2 dB louder than these rows: the model of the fight showed a warning on top of the densest mix peaking over the limiter's line, so the catalog volumes came down (§3).

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
| `Snd.Danger.Heartbeat` | the vessel's own pulse: two low thumps, dry | low health, every beat | low | 355.7 | 18.8 | -17.5 | -18.4 | -89.0 | 4.9 | 2 |
| `Snd.Danger.BossPhase` | a mechanism turns over under a low bell | the Warden changes phase | free | 1961.1 | 133.4 | -15.4 | -16.3 | -79.7 | 6.5 | 1 |

All are fixed in pitch. The warnings ring on E7, G7 and D7 with partials that are notes of the key. The helper `clunk` (a heavy latch of the old machine) joins `Tools/audio/recipes/world.py`; the ten interface files did not move by a byte.

The catalog's least interval of a warning is 120 ms, the value of `C.AUD.WARN_MIN_INTERVAL_MS`. Nothing ties the cell to the key yet; the wiring job adds that check or drops the key.

## 2. Told apart

Two confusion sets. Distances are the session's reading of the same bytes (the built files equal the judged drafts, by hash).

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
| Heartbeat, HeroHurt | 13.5 | distance, strikes |
| Heartbeat, HeroDown | 6.9 | distance, strikes, length |
| BossPhase, HeroHurt | 10.8 | distance, length |
| BossPhase, HeroDown | 6.5 | distance alone |
| BossPhase, Heartbeat | 6.8 | distance, strikes, length |

## 3. What proves it, and the levels

- `python -I Tools\audio\audio_check.py`: `AUDIO: PASS (19 sounds: 0 heard, 10 delegated, 0 rejected, 9 not heard; 21 files, 54 tool tests; not active yet: the mix checks (R-AUD-18); 0 rebuilt, 5.6 s)`. Per file that is: a warning's share in the danger band at least -3 dB and its attack at most 3 ms; the heartbeat's share in the low zone; lengths, levels, peaks, edges; the small speaker for the warnings and the heartbeat; every pair above.
- The levels come from a model of the fight in the session's sketch (not committed; it is being ported as `Tools/audio/mix_model.py`): stand-in sounds for the player's side, mixed by the scheduler's caps. With these rows' volumes over the densest mix it read: each warning ahead by 25 to 35 dB in the danger band with no duck (floor `C.AUD.DANGER_SMR_MIN_DB` 12); enemy fire by 10.6 dB, and 16.6 with the duck; the heartbeat by 7.3 dB in the low zone, and 13.3 with the duck (floor 9); a player sound read as a warning at -52, failing as it must; the peak with one danger sound on top at -2.5 dBFS (limit -1). At the first drafts' volumes that peak read +0.4 dBFS, which is why the warnings are at 0.64 and not 0.8.
- These are a sketch's figures. The figures of record are the mix model's, on the merged tree.

## 4. What is not verified

- Nobody has heard these in a fight. The first interface sounds passed every number and were wrong in style.
- Two pairs hold by one leg. Arc and Line are the same strike, damped or left to ring: they differ in length alone. BossPhase and HeroDown are both long, low and of bronze: they differ by 6.5 dB of spectrum against a floor of 6. Each wants an ear; the second can be moved apart at no cost if the owner hears them as one.
- Enemy fire and the heartbeat reach their floors only with the duck. That is by design for enemy fire (it sounds many times a second). For the heartbeat it means its audibility rests on the heartbeat being a danger voice that lowers the player's side on every beat.
- Whether a turret's fire, sounding many times a second and lowering the player's side each time, makes a fight pump. The model can draw the duck; it cannot hear it.
- BossPhase loses 6.5 dB on a small speaker. The rule binds warnings, the heartbeat and the interface, so it passes; on a laptop the Warden's phase is a faint sound.
- Nothing raises these sounds. The danger observers are the wiring job's.
