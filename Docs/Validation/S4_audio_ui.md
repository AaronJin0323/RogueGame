# S4 audio: the interface family

Job 2 of `Validation/S4_audio_wave1.md` §8.1 (D-056), branch `s4/audio-snd-ui` on `s4/audio-tools` at 8792462. Recipes, catalog rows and manifest lines only: no engine code, no asset, and nothing in the game plays them yet. The Sound session wrote the recipes and judged them from numbers and pictures. Nobody has heard them.

## 1. The ten sounds

All are in the UI class, zone body, fixed pitch, in A minor pentatonic, with attacks of 6 ms or slower and a low-pass under the danger band. Each has its own gesture, so any two differ in spectrum, in their count of onsets or in length.

| Id | Gesture | Meant for | Active ms | Attack ms | Loudest 100 ms, LK | Played, LK | Share in the danger band, dB | Small-speaker loss, dB | Onsets |
|---|---|---|---|---|---|---|---|---|---|
| `Snd.UI.Focus` | one small tick on A5 | the cursor moved | 37.2 | 5.2 | -24.9 | -30.9 | -46.1 | 0.0 | 1 |
| `Snd.UI.Step` | one lower tick on E5 with a soft click | a value moved; on the volume row it plays at the new level | 47.4 | 5.4 | -24.6 | -29.8 | -40.4 | 0.0 | 1 |
| `Snd.UI.Open` | two notes up a fifth, A4 then E5 | a screen opened | 207.5 | 5.9 | -20.3 | -24.7 | -39.9 | 0.0 | 2 |
| `Snd.UI.Back` | one glide down, E5 to A4 | a screen closed | 127.6 | 5.5 | -21.5 | -25.9 | -42.1 | 0.0 | 1 |
| `Snd.UI.Refuse` | two low thuds on A3 with C4 over them | not allowed | 177.5 | 4.7 | -20.6 | -25.0 | -60.2 | 0.9 | 2 |
| `Snd.UI.HandOpen` | a rolled open chord, A3 E4 A4 C5 | an offer hand opens | 897.1 | 84.8 | -21.8 | -26.2 | -50.6 | 0.2 | 1 |
| `Snd.UI.Pick` | a fourth up and held, E4 then A4 | a card taken | 372.0 | 80.7 | -20.4 | -24.1 | -56.1 | 0.1 | 2 |
| `Snd.UI.Reroll` | three quick notes up, D5 E5 G5 | the hand is dealt again | 156.5 | 5.3 | -21.2 | -25.6 | -38.1 | 0.0 | 3 |
| `Snd.UI.Banish` | a falling breath and a low note on D4 | a card sent away | 157.0 | 66.2 | -22.6 | -27.0 | -57.2 | 0.3 | 1 |
| `Snd.UI.Lock` | a low clunk on C4 and a tick on A5 | a card held | 111.2 | 5.8 | -22.5 | -26.9 | -49.1 | 0.4 | 2 |

The figures are the manifest's (`Tools/audio/sound_manifest.csv`); "played" adds the catalog volume. The limits they are read against: `C.AUD.PLAYER_BAND_SHARE_MAX_DB`, `C.AUD.PLAYER_ATTACK_MIN_MS`, `C.AUD.SPIKE_MAX_DB`, `C.AUD.SMALL_SPEAKER_LOSS_MAX_DB`, `C.AUD.PEAK_MAX_DBFS`, `C.AUD.LEAD_MAX_MS`, and each row's own length and loudness windows. Focus is the quietest on purpose: it is the sound heard most.

## 2. Told apart

The ten are one confusion set, `ui`: 45 pairs. Every pair holds by spectral distance alone, against `C.AUD.PAIR_DISTANCE_MIN_DB`; the closest is HandOpen against Banish at 10.5 dB, and that pair also differs by more than a factor of five in length. Thirty-nine of the 45 pairs differ in onset count or length as well. The distances are the sketch's reading of the same bytes: the ten files this branch builds are byte-identical to the sketch's.

## 3. What proves it

- `python -I Tools\audio\audio_check.py`: `AUDIO: PASS (10 sounds, 10 files, 50 tool tests; not active yet: the mix checks (R-AUD-18) and the generated C++ table DTSoundsGen.h; 0 rebuilt, 3.5 s)`.
- `python -I Tools\audio\audio_view.py --family UI` draws the family's sheet into `Saved\AudioGen\sheets\sheet_UI.png`: the session read it and it shows the ten gestures as the table names them. The picture is build output and is not committed.
- Two margins are thin and are watched: Refuse's attack reads 4.7 ms against a floor of 4, and Banish's lead 1.3 ms against a ceiling of 2.

## 4. What is not verified

- Nobody has heard any of these. Whether a sound is pleasant, whether Refuse reads as "no", and whether Focus is too quiet or too frequent are the owner's to judge (`Validation/S4_audio_wave1.md` §6.3).
- The band rule is proved per file. The mix checks are not active: no danger sound and no player mix exist yet.
- Nothing plays them: the runtime and the import are the next two jobs. The events they are meant for are named in the plan's §7.1 and are wired in job 4.
- The catalog's intervals and voice counts are RogueHero's starting values and have no reading here.
