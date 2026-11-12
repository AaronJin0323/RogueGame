# S4 audio: the fire family

Branch `s4/audio-snd-fire`, from `dev/s4` at 42dcd73. It adds ten catalog rows in one family, `Fire`, their recipe file, their forty manifest lines and their rows of the generated table. Every row reads `no` in the catalog's `heard` column, so none becomes an asset and none can play (`02_Systems/audio.md` R-AUD-22). There is no engine code here; the generated table gains ten ids, which changes its shape, so the branch owes a whole gate. The Sound session wrote the recipes and judged them from numbers and pictures. Nobody has heard them.

## 1. Ten weapons, one gesture each

The Reclaimer's weapons are tools of the old machine (D-066). Each is one dry, short gesture, because a weapon is heard many times a second. All ten are written on A or E, the two notes a fight rests on; the heartbeat keeps C, between them. They are low, start softly and end under the danger band. A weapon's four files differ in decay and colour and never in pitch: the game draws the pitch, a degree of the key (R-AUD-10).

| Id | What it is | Written on | Catalog volume | Active ms | Attack ms | Played, LK | Share in the danger band at +5 semitones, dB |
|---|---|---|---|---|---|---|---|
| `Snd.Fire.PulseRifle` | a bolt thrown | A2 | 0.35 | 115 to 116 | 9.1 to 9.2 | -31.5 | -62.7 to -62.1 |
| `Snd.Fire.RingEmitter` | an iron hoop struck flat: a plate, no bell | E2 | 0.40 | 172 to 178 | 19.4 | -30.7 | -72.1 to -71.3 |
| `Snd.Fire.OrbitalDrones` | a relay closes | E3 | 0.25 | 50 | 6.2 | -36.9 | -49.7 to -49.6 |
| `Snd.Fire.ArcBlade` | a blade through air, a low ring behind it | A2 | 0.35 | 188 to 201 | 36.1 to 36.4 | -32.1 | -37.8 to -35.8 |
| `Snd.Fire.GravityMine` | a great weight set down; the floor answers | A1 | 0.45 | 382 to 383 | 18.4 | -28.7 | -78.0 to -77.8 |
| `Snd.Fire.ScatterCannon` | a handful of iron shot thrown | A2 | 0.38 | 138 to 143 | 8.9 to 9.1 | -29.9 | -45.7 to -35.3 |
| `Snd.Fire.RailLance` | a rail struck along its length; it rises as it rings | E3 | 0.40 | 313 | 12.2 | -28.5 to -28.4 | -43.4 to -43.2 |
| `Snd.Fire.FlakMortar` | a breech dropped | A1 | 0.39 | 202 | 18.0 | -30.5 to -30.4 | -77.4 to -75.6 |
| `Snd.Fire.SeekerSwarm` | wires plucked in a quick run: three, each softer | E3 | 0.30 | 187 to 193 | 6.2 | -34.3 to -34.2 | -54.2 to -53.8 |
| `Snd.Fire.SpiralProjector` | a ratchet's tooth, made to run into a purr | A3 | 0.22 | 42 | 4.7 | -39.5 | -58.5 to -57.5 |

The limit of the last column is `C.AUD.PLAYER_BAND_SHARE_MAX_DB`; the least attack allowed is `C.AUD.PLAYER_ATTACK_MIN_MS`. Each is quiet alone on purpose: ten of them sound at once.

The row order is the weapons catalog's, starters first, so the mix model's sparse tier fires the first three. Every row waits 60 ms between its voices but the Spiral Projector, which waits 160 ms: that weapon fires fourteen to twenty-two times a second, and a tooth every sixth of a second is the purr it is meant to be.

One change against the first draft: the Scatter Cannon's small knocks lost half of their upper partial and its low-pass came down from 1600 Hz to 1500. Two of its files read -28.9 and -29.3 dB in the danger band at the highest pitch the game can draw, over the limit of -30; they read -35.3 and -35.7 now. The first file of each of the other nine weapons equals the session's draft byte for byte.

## 2. Told apart

One confusion set, `fire`: forty-five pairs of weapons, every file of one against every file of the other. All forty-five hold by spectral distance alone (`C.AUD.PAIR_DISTANCE_MIN_DB`). The closest:

| Pair | Least spectral distance, dB | It also differs by |
|---|---|---|
| OrbitalDrones, SeekerSwarm | 7.3 | plucks (1, 3), length |
| ArcBlade, ScatterCannon | 7.4 | |
| RailLance, SeekerSwarm | 7.6 | plucks (1, 3) |
| FlakMortar, GravityMine | 8.4 | |
| ArcBlade, SeekerSwarm | 9.0 | plucks (1, 3) |
| PulseRifle, ScatterCannon | 9.2 | |
| every other pair | 10.2 and more | |

## 3. What the mix model reads

`python -I Tools\audio\mix_model.py` on this branch: the fire is the catalog's, and kills and both layers are still stand-ins. Each weapon is asked `C.AUD.FIRE_DENSE_VOICES_PER_S` times a second, which is the worst case: in the game only the Spiral Projector reaches it.

| Kills a second | Weapons | Layers | Loudness, LK | Share in the danger band, dB | Crest, dB (one seed) | Peak, dBFS | Dropped |
|---|---|---|---|---|---|---|---|
| 5 | 3 | none | -27.4 | -49.4 | 16.3 | -9.8 | 0 |
| 20 | 6 | crowd | -22.7 | -48.8 | 12.9 | -8.0 | 0 |
| 60 | 10 | crowd and low | -21.5 | -50.2 | 13.3 | -6.3 | 126 |
| 100 | 10 | crowd and low | -21.5 | -50.7 | 13.0 | -6.6 | 127 |

- The loudness rises 5.9 dB from the sparsest tier to the densest, against `C.AUD.DENSITY_RISE_MAX_DB` (6). It holds by a tenth; with the stand-in fire it read 5.1. The real kills and layers will move it again.
- The crest is printed and not judged, because two roles are stand-ins (R-AUD-18 as ruled). For scale: the three dense tiers read 12.9, 13.3 and 13.0 dB on the one seed, under `C.AUD.MIX_CREST_MAX_DB` (14); with the stand-in fire they read 13.9, 12.7 and 14.1.

The nine danger cues against this fight, each over the densest mix lowered by the duck:

| Cue | Audibility in its zone, dB | The same with no duck | Loudest 100 ms over the mix, dB | Peak with it on top, dBFS |
|---|---|---|---|---|
| `Snd.Warn.Line` | 32.8 | 26.8 | 7.9 | -3.25 |
| `Snd.Warn.Band` | 37.7 | 31.7 | 10.5 | -3.80 |
| `Snd.Warn.Ring` | 41.5 | 35.5 | 8.6 | -3.28 |
| `Snd.Warn.Arc` | 42.8 | 36.8 | 9.0 | -5.82 |
| `Snd.Danger.EnemyFire` | 18.4 | 12.4 | | -4.58 |
| `Snd.Danger.HeroHurt` | | | | -3.16 |
| `Snd.Danger.HeroDown` | | | | -3.74 |
| `Snd.Danger.Heartbeat` | 11.5 | 5.5 | | -3.06 |
| `Snd.Danger.BossPhase` | | | | -3.41 |

All nine hold their floors and the peak limit. The heartbeat keeps its place between the fight's notes: 11.5 dB against `C.AUD.HEARTBEAT_SMR_MIN_DB` (9). It read 12.3 against the stand-in fire; the Ring Emitter's second mode, 1.59 times E2, falls at 131 Hz, which is the heartbeat's octave. The control is now a real weapon: the Rail Lance read as a warning gives -29.6 dB and fails, as it must.

The audio step on this branch: `AUDIO: PASS (29 sounds: 0 heard, 19 delegated, 0 rejected, 10 not heard; 61 files, 71 tool tests; mix: 9 cue(s) over stand-ins for kills, layers; ...)`.

## 4. For the owner

Two folders in the Sound session's worktree, `Saved\S4Report\Audio\`: `2026-11-12_fire_draft\` has each weapon alone, three times, and the ten in turn (its Scatter Cannon is the first draft's, a little brighter than this branch's); `2026-11-13_fire_in_fight\` has the model's fight with this fire in it at four sizes, and the densest with the nine danger sounds on top. Nothing waits on them.

## 5. What is not verified

- Nobody has heard these. The first interface sounds passed every number and were wrong in style.
- Whether ten gestures of one material read as ten weapons to a player, or as one machine at work. The numbers say they differ; they cannot say a player learns them.
- The loudness rise is a tenth under its limit, read with stand-in kills and layers.
- The model asks every weapon six times a second. In the game most fire from one to four times a second, and the heavy ones (Gravity Mine, Rail Lance, Flak Mortar) less than once: the model's fight is thicker in heavy shots than a real one.
- The model does not apply a row's most voices; the runtime does. With these rows that cap can bind only for the long files at the model's rate.
- A weapon's four files are close to one another by design; whether a run of one weapon sounds alive or mechanical is for an ear.
- Nothing raises these sounds. The fire observer is the wiring job's, with the Player group's submix.
