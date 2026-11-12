# S4 audio: pickups and the hero's acts

Branch `s4/audio-snd-hero`, on `s4/audio-snd-kills` (which is on `s4/audio-snd-fire`, from `dev/s4` at 42dcd73). It adds eight catalog rows in two families, `Pickup` and `Hero`, their two recipe files, their twelve manifest lines and their rows of the generated table, and three helpers in `Tools/audio/recipes/world.py` (`stack`, `voices`, `horn`: the voices of the Six). Every row reads `no` in the catalog's `heard` column, so none becomes an asset and none can play (`02_Systems/audio.md` R-AUD-22). There is no engine code here; the generated table gains eight ids on top of the sixteen of fire and kills, so the branch owes a whole gate. The Sound session wrote the recipes and judged them from numbers and pictures. Nobody has heard them.

## 1. Eight sounds, three materials

Each material is used for what it means in this world (D-066).

| Id | What it is | Material | Meant for | Catalog volume | Active ms | Played, LK |
|---|---|---|---|---|---|---|
| `Snd.Pickup.Gem`, four files | a bronze bead dropped into a bowl, on E4 | bronze: what is granted | an XP gem taken | 0.25 | 68 | -36.8 |
| `Snd.Pickup.Chest` | a casket's latch falls open, the bowl inside is struck on E3; the hall answers | bronze, and a latch | a chest taken | 0.70 | 1413 | -22.9 |
| `Snd.Hero.Heal` | a bowl made to sing on A3: a swell, no strike | bronze | health restored | 0.60 | 885 | -26.5 |
| `Snd.Hero.LevelUp` | two small bells, A3 then E4 | bronze, rising | a level gained | 0.70 | 1599 | -22.5 |
| `Snd.Hero.Dash`, two files | a piston's thrust: air, and a weight let go, on A2 | the old machine | the dash | 0.45 | 127 to 133 | -29.6 |
| `Snd.Hero.DashReady` | a latch re-arms: two soft ticks, E3 then A3 | the old machine | the dash is back | 0.30 | 129 | -34.1 |
| `Snd.Hero.IntercessionReady` | the Six, mouths closed, on A2 and E3 | the voices | the Intercession is charged | 0.60 | 829 | -25.7 |
| `Snd.Hero.IntercessionCast` | the Six as one chord, A2, E3 and A3, a horn under it, and the hall | the voices | the Intercession is cast | 0.70 | 1973 | -20.6 |

- **Rising bronze means "gained".** A gem's streak climbs the key, a level is two bells rising a fifth, and the win of a run (`Snd.Kills.BossDefeated`) is three bowls rising. The level's bells sit over the hand's great bowl on A2, which opens with it: its octave and the fifth above that, no third.
- **The voices appear nowhere else in a run.** The interface is stone and bronze, the weapons are the old machine. A choir is the one sound that means a Keeper is acting, so it is kept for the Intercession; the six Keepers' own signatures, a later family, are the same material.
- **The gem is a ladder** (`pitch` `ladder`): its pitch climbs with a streak, up to `C.AUD.LADDER_SEMIS_MAX` (an octave). It holds only a hum, the note, its fifth and its octave, so an octave up it is still under the danger band: it reads -70.4 to -68.5 dB there at the top, against `C.AUD.PLAYER_BAND_SHARE_MAX_DB` (-30). By R-AUD-10 a ladder plays as written until its family brings its rule; the streak's rule (the degrees of the key from E, how long a streak lasts) is the wiring job's.
- **A swell still starts on time.** The healing and the waiting voices have no strike, and as first written their sound crossed the line of activity 5 ms into the file, against `C.AUD.LEAD_MAX_MS` (2). Each now begins with a faint contact (a mallet laid on the rim, a breath drawn), which is in the file as a short soft noise well under the swell.
- The dash loses 6.2 dB on a small speaker: on a laptop it is mostly air. No rule binds it.

## 2. Told apart

Two sets: `gain` (gem, chest, healing, level) and `act` (dash, dash ready, the two Intercession sounds). Beside them, the pairs a player might take for one another across families, which no rule binds. Least spectral distance over the files, against `C.AUD.PAIR_DISTANCE_MIN_DB` (6):

| Pair | Distance, dB | It also differs by |
|---|---|---|
| Gem, the hit | 14.4 | |
| Gem, the Orbital Drones' relay | 10.1 | |
| Chest, the Warden's phase | 15.0 | strikes (2, 1) |
| Chest, a card taken | 12.9 | |
| Chest, the level | 13.2 | |
| Level, a card taken | 11.6 | |
| Level, the win | 14.0 | strikes (2, 6), length |
| Dash, the Arc Blade | 11.5 | |
| Dash, the hurt | 8.9 | 13.5 dB of level |
| Dash ready, the relay | 11.8 | ticks (2, 1), length |
| Intercession ready, cast | 10.9 | length |
| the cast, the hand opening | 8.8 | a swell against a strike |
| the cast, the win | 8.3 | a swell against six strikes, length |
| the cast, the fall | 9.9 | a swell against a strike |

## 3. Against the fight

These families are not in the mix model: it reads fire, kills and the two beds. Read by a session script with the model's own functions, each sound's peak with it on top of the densest fight, nothing lowered (the fight alone peaks -5.59 dBFS):

| Sound | Peak on the fight, dBFS |
|---|---|
| Gem | -4.61 |
| Chest | -3.43 |
| Heal | -4.51 |
| LevelUp | -2.81 |
| Dash | -4.33 |
| DashReady | -4.93 |
| IntercessionReady | -4.83 |
| IntercessionCast | -1.48 |

The cast is the loudest sound of the player's side, and it falls in the thick of a fight. At a catalog volume of 0.85 it read -0.72 dBFS on the flood, over `C.AUD.LIMIT_DBFS` (-1), so it is 0.70, where it reads -1.48.

The audio step on this branch: `AUDIO: PASS (43 sounds: 0 heard, 19 delegated, 0 rejected, 24 not heard; 86 files, 71 tool tests; mix: 9 cue(s) over the catalog's player side; ...)`.

## 4. For the owner

`Saved\S4Report\Audio\2026-11-13_pickups_and_hero\` in the Sound session's worktree: each sound alone at its played level, the gem as two streaks climbing the key, and a level as it falls in a run (the two bells, then the hand's great bowl). Nothing waits on it.

## 5. What is not verified

- Nobody has heard these.
- The voices are synthesis: three detuned tones a note through two vowel formants. The numbers say they are dark and in the key. Whether they read as a choir or as an organ is for an ear; of everything in this family they are the most likely to be wrong in style.
- A gem streak at speed. The model has no gems; whether sixty beads a second climbing an octave is a pleasure or a nuisance depends on the streak's rule, which does not exist yet.
- Whether the level's two bells and the hand's bowl, heard together twenty times a run, stay welcome.
- The cast is not ducked under and ducks nothing: it is a player-side sound among the others. Whether the Six should be heard over everything is a design question for its wiring.
- Nothing raises these sounds. Their observer reads the run state's pickups and chests, `OnHeroEvent`, `OnLevelUp`, the dash charges, `Intercession01` and `OnIntercession`; it is a wiring job's.
