# S4 audio: the kills family, the two beds and the win

Branch `s4/audio-snd-kills`, on `s4/audio-snd-fire` (which is from `dev/s4` at 42dcd73). It adds six catalog rows in one family, `Kills`, their recipe file, their thirteen manifest lines and their rows of the generated table. As first committed every row read `no` in the catalog's `heard` column (`02_Systems/audio.md` R-AUD-22). On 2026-11-13 the scheduler delegated the family on these readings, with the whole wave's thirty rows (D-056's Result on `main`): the rows read `delegated 2026-11-13`, and the family's files are SoundWaves in `Content/DT/Audio/SFX/`, imported by `Tools\audio\Audio.ps1` under the licence row that covers `SW_*`. Nothing raises them yet, so the game sounds as before. There is no engine code here; the generated table gains six ids on top of the fire family's ten, which changes its shape, so the branch owes a whole gate. The Sound session wrote the recipes and judged them from numbers and pictures. Nobody has heard them.

With this family the mix model plays no stand-in: every part of the fight it reads is a catalog row, and the crest is judged for the first time.

## 1. Six sounds

The Keep's machines wear fired clay (D-066). A round that lands and does not break it is a dull tap; a machine that falls is a vessel breaking. Hit is written on E, kill and the great jar on A, the two notes a fight rests on.

| Id | What it is | Meant for | Catalog volume | Active ms | Attack ms | Played, LK | Share in the danger band at its top pitch, dB |
|---|---|---|---|---|---|---|---|
| `Snd.Kills.Hit`, four files | clay tapped and holding: a dull tock on E4 | a round lands | 0.28 | 57 to 58 | 6.3 | -35.9 | -73.3 to -71.1 |
| `Snd.Kills.Kill`, four files | clay cracks on A3, five shards fall | a machine breaks | 0.40 | 157 to 193 | 5.7 | -29.7 to -29.6 | -39.8 to -34.8 |
| `Snd.Kills.Elite`, two files | a great jar on A2: nine heavier shards, a short answer from the room | an elite falls | 0.70 | 476 to 494 | 11.3 | -23.6 | -61.9 to -61.3 |
| `Snd.Kills.LayerCrowd`, a loop of 4 s | many vessels breaking further off, no single fall in front | the crowd tier's bed | 0.42 | | | -30.0 | -76.5, as written |
| `Snd.Kills.LayerLow`, a loop of 4 s | the hall under a flood: a slow low roll | the flood tier's bed | 0.42 | | | -28.3 | -89.8, as written |
| `Snd.Kills.BossDefeated` | the Warden's mechanism runs down and stops, a last latch falls, three bronze bowls answer, rising: A2, E3, A3, the last left to the hall | the run is won | 0.80 | 5316 | | -17.9 | -81.9, as written |

- Hit, kill and the two beds are the recipes the mix model played as stand-ins since `dev/s4` 2aadc0d, now catalog rows; the stand-in module stays for a catalog that lacks a role. The great jar and the win are new.
- Bronze is this world's sound for what is granted, so the win ends in bronze and rises. The Reclaimer's fall, `Snd.Danger.HeroDown`, is one bell whose pitch sinks: the two are opposites on purpose.
- The great jar is rare, a few a minute, and is set 6 dB over a kill so that it is heard over a fight. It is still 7.5 dB under the hurt.
- The low bed's partials all start together, where the wave is steepest; as written the step across the loop's seam was larger than the file's ordinary steps. The loop now begins at its half, where the detuned partials pull against the others: the seam's step is 45 against an ordinary 80, and the level is the same on both sides.
- Two volumes moved against the stand-ins: hit 0.25 to 0.28 and kill 0.35 to 0.40. With the stand-ins' values the loudness rise read 6.0 dB against a limit of 6. Both sound as often in a small fight as in a flood, so raising them fills the small fight more than the large one.

## 2. Told apart

The set `kills` holds hit, kill and the great jar. Beside it, the pairs a player might take for one another across families, which no rule binds:

| Pair | Least spectral distance, dB | Length ratio |
|---|---|---|
| Hit, Kill | 14.0 | 2.7 |
| Kill, Elite | 13.3 | 2.5 |
| Hit, Elite | 18.0 | 8.1 |
| Hit, the Orbital Drones' relay | 14.7 | 1.1 |
| Kill, the Scatter Cannon | 12.4 | 1.1 |
| Elite, the hurt | 12.5 | 2.2 |
| the win, the fall | 8.5 | 2.7 |
| the win, the Warden's phase | 12.1 | 2.7 |
| the win, the hand opening | 12.0 | 2.2 |

The limits are `C.AUD.PAIR_DISTANCE_MIN_DB` (6) and `C.AUD.PAIR_LENGTH_RATIO_MIN` (2). The win and the fall are the closest of these: both are low bronze in a hall. They differ by 8.5 dB of spectrum, by length, and by gesture: six onsets against one, rising against sinking.

## 3. What the mix model reads

`python -I Tools\audio\mix_model.py` on this branch, `stand-ins: none`.

| Kills a second | Weapons | Layers | Loudness, LK | Share in the danger band, dB | Crest, dB: median of five seeds | Peak, dBFS | Dropped |
|---|---|---|---|---|---|---|---|
| 5 | 3 | none | -26.9 | -49.7 | not judged (16.3 on one seed) | -9.4 | 0 |
| 20 | 6 | crowd | -22.4 | -49.0 | 13.6 | -7.7 | 0 |
| 60 | 10 | crowd and low | -21.4 | -50.5 | 13.1 | -6.0 | 126 |
| 100 | 10 | crowd and low | -21.3 | -50.7 | 13.8 | -5.6 | 127 |

- The crest holds in the three tiers where it is judged, against `C.AUD.MIX_CREST_MAX_DB` (14): by 0.4, 0.9 and 0.2 dB.
- The loudness rises 5.6 dB from the sparsest tier to the densest, against `C.AUD.DENSITY_RISE_MAX_DB` (6).
- Both hold with little room, and they pull against each other. Which level moves which, for the next tuning:
  - **Hit and kill up** (measured on this branch: hit 0.25 to 0.28 and kill 0.35 to 0.40, about 1 dB each): the sparsest tier gains 0.5 dB, the tier of 20 gains 0.3 and the two dense tiers 0.1, so the rise falls by 0.4 dB (6.0 to 5.6); the crest's medians grow by 0.1, 0.3 and 0.4 dB (13.5, 12.8 and 13.4 to 13.6, 13.1 and 13.8). They sound as often in a small fight as in a flood, so they fill the small fight; they are short and struck, so they stand over the beds.
  - **The beds up** (reasoned, not measured): they sound only in the dense tiers and are steady, so a louder bed raises the rise and lowers the crest; a quieter one does the reverse.
  - **The heavy weapons down** (measured by a session script with the model's own functions, not on a branch: Gravity Mine, Rail Lance and Flak Mortar 1 dB and 2 dB lower): the rise falls by 0.3 and 0.6 dB (5.6 to 5.3 and 5.0); the crest does not move (13.6, 13.1, 13.8 to 13.7, 12.9, 13.8 and to 13.8, 13.2, 13.7). They carry as much of the dense tiers' level as of their peaks, so both fall together. It was reasoned beforehand that they would lower the crest; they do not.
  - **The first three weapons up** (reasoned, not measured): they are the whole of the sparsest tier's fire, so raising them lowers the rise.
  - So a rise over its limit is met with hit and kill up, the heavy weapons down or the beds down; the first and the last at the crest's cost. A crest over its limit has one lever here, the beds up, at the rise's cost; the heavy weapons down can pay that cost back.

The nine danger cues against this fight, each over the densest mix lowered by the duck:

| Cue | Audibility in its zone, dB | The same with no duck | Loudest 100 ms over the mix, dB | Peak with it on top, dBFS |
|---|---|---|---|---|
| `Snd.Warn.Line` | 32.4 | 26.4 | 7.9 | -3.41 |
| `Snd.Warn.Band` | 37.3 | 31.3 | 10.5 | -3.84 |
| `Snd.Warn.Ring` | 41.1 | 35.1 | 8.5 | -3.04 |
| `Snd.Warn.Arc` | 42.4 | 36.4 | 9.0 | -5.33 |
| `Snd.Danger.EnemyFire` | 18.0 | 12.0 | | -4.29 |
| `Snd.Danger.HeroHurt` | | | | -2.81 |
| `Snd.Danger.HeroDown` | | | | -3.36 |
| `Snd.Danger.Heartbeat` | 11.6 | 5.6 | | -3.17 |
| `Snd.Danger.BossPhase` | | | | -3.49 |

All nine hold their floors and the peak limit. The heartbeat is read here with the duck of a danger voice, 11.6 dB; under the form the game will play, a hold of 4 dB while health is low, the same reading is 9.6 dB against `C.AUD.HEARTBEAT_SMR_MIN_DB` (9). The model takes that form with the fire family's wiring.

The win is not in the model. Read by a session script with the model's own functions: alone it peaks -6.25 dBFS; over the densest fight with nothing lowered its first three and a half seconds peak -2.52 dBFS. In a run the fight stops as the Warden falls, and the bowls sound over silence.

The audio step on this branch: `AUDIO: PASS (35 sounds: 0 heard, 19 delegated, 0 rejected, 16 not heard; 74 files, 71 tool tests; mix: 9 cue(s) over the catalog's player side; ...)`.

## 4. For the owner

`Saved\S4Report\Audio\2026-11-13_kills_in_fight\` in the Sound session's worktree: the hit, the kill and the great jar alone; the win alone, and after a heavy fight; the model's fight at four sizes with every part a real draft; the flood with the nine danger sounds on top. Nothing waits on it.

## 5. What is not verified

- Nobody has heard these.
- The win is the one long, composed sound of the run, and the one most a matter of taste: three bowls on an open fifth. It is simple on purpose. Whether it is grand enough for the end of a run is for an ear.
- Whether the crowd bed reads as "many breaking, further off" or as a noise floor. It is made of two hundred and sixty small falls with none in front; the numbers say it has no hole and no seam, not that it sounds like a crowd.
- The low bed loses 11.4 dB on a small speaker: on a laptop the flood has no low roll. No rule binds it.
- The crest and the rise hold by 0.2 and 0.4 dB, on a model whose every weapon fires six times a second.
- Nothing raises these sounds. The kills observer reads `FDTWeaponCounters::Impacts`, the step's kills and elite deaths, the density tier and `OnBossDefeated`; it is a wiring job's.
