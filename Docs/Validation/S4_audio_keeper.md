# S4 audio: the six Keepers' signatures

Branch `s4/audio-snd-keeper`, on `s4/audio-snd-hero` (which is on `s4/audio-snd-kills` and `s4/audio-snd-fire`, from `dev/s4` at 42dcd73). It adds six catalog rows in one family, `Keeper`, their recipe file, their six manifest lines and their rows of the generated table. Every row reads `no` in the catalog's `heard` column, so none becomes an asset and none can play (`02_Systems/audio.md` R-AUD-22). There is no engine code here; the generated table gains six ids on top of the twenty-four of fire, kills, pickups and the hero, so the branch owes a whole gate. The Sound session wrote the recipes and judged them from numbers and pictures. Nobody has heard them.

With this family every sound of the first wave's plan (`Validation/S4_audio_wave1.md` §4.3) has a recipe and a row.

## 1. Six cells

The art direction's audio section (`Handoff_Dusktracer/game/reference/original_docs/05_ArtDirection.md` §11) gives each Path a texture and a motif instrument. D-066 keeps the voices and the horn for the Six. So each cell is one of them speaking, with the thing its Path is made of; and each Path has its own notes of the one key, so the six are told apart by pitch as well as by material. A cell is one gesture under a second, then the hall. They are interface sounds: a blessing is taken in the hand, while the world stands, and they go through the stinger queue (R-AUD-11).

| Id | The art direction's words | What it is here | Its notes | Active ms | Played, LK | Small-speaker loss, dB |
|---|---|---|---|---|---|---|
| `Snd.Keeper.Storm.Pick` | dry electric crackle, distant thunder; a low brass stab | five dry ticks into a low horn, and a roll far off | A2 and E3, struck | 774 | -23.3 | 2.5 |
| `Snd.Keeper.Forge.Pick` | hammer on anvil, bellows; timpani and anvil | an iron bar struck over a great drum; the bellows breathe after | E4 over A2 | 774 | -23.1 | 1.7 |
| `Snd.Keeper.Tide.Pick` | deep water, ice creak; low strings, slow | low voices swell over a horn, and ice creaks once | D3 and A3 over D2 | 1066 | -23.5 | 4.4 |
| `Snd.Keeper.Shade.Pick` | a ledger page, a stamp; a muted solo cello | a page turned, a stamp, one low voice holding | G3, alone | 950 | -22.7 | 1.0 |
| `Snd.Keeper.Radiance.Pick` | glass ring, high harmonic; high choir or celesta | glass rings twice, rising a fourth, over a high still voice | E5 then A5, over E4 | 1242 | -22.1 | 0.0 |
| `Snd.Keeper.Quicksilver.Pick` | wind rush, relay clicks; a fast woodwind run | wind, two relays, three quick notes running up | E4, G4, A4 | 1158 | -22.6 | 0.0 |

- All six are fixed: a signature is learned. Catalog volume 0.70 each; a card taken (`Snd.UI.Pick`) plays at -24.1 LK, and its Keeper answers at about the same level when the stinger gap has passed.
- The instruments of an orchestra are not in this world, and nothing is sampled (R-AUD-13). "Low brass" is the horn of the Six; "low strings" and "choir" are their voices; "timpani" is a drum whose pitch sinks a little; "celesta" is struck glass holding only its note and its octave; "woodwind" is a three-partial reed. Radiance and Quicksilver are the highest sounds of the interface and the player's side, and still read -47.5 dB or lower in the danger band, against `C.AUD.PLAYER_BAND_SHARE_MAX_DB` (-30).
- An interface sound must keep its loudness on a small speaker (`C.AUD.SMALL_SPEAKER_LOSS_MAX_DB`, 6). Storm and Tide are the low ones and hold by their upper partials.
- Two cells begin with something soft, a page and a swell of water. Each has a faint first contact so that it starts in the frame it is asked for (`C.AUD.LEAD_MAX_MS`).

## 2. Told apart

The set `keeper` holds the six: fifteen pairs. All hold by spectral distance alone, against `C.AUD.PAIR_DISTANCE_MIN_DB` (6).

| Pair | Distance, dB |
|---|---|
| Storm, Shade | 9.5 |
| Storm, Tide | 11.4 |
| Tide, Shade | 11.6 |
| Shade, Quicksilver | 13.1 |
| Storm, Forge | 13.2 |
| Storm, Quicksilver | 13.5 |
| Tide, Quicksilver | 13.5 |
| Forge, Shade | 15.0 |
| Forge, Quicksilver | 15.7 |
| Radiance, Quicksilver | 17.4 |
| Forge, Tide | 19.2 |
| Shade, Radiance | 22.0 |
| Forge, Radiance | 24.7 |
| Tide, Radiance | 25.5 |
| Storm, Radiance | 26.7 |

Beside the set, against what sounds near them in a run (no rule binds these):

| Pair | Distance, dB |
|---|---|
| Storm, the Intercession charged | 6.4 |
| Shade, a card taken | 7.4 |
| Quicksilver, the level's two bells | 8.6 |
| Storm, a card taken | 9.4 |

Storm's horn and the waiting voices of the Intercession are the same two notes in nearly the same voice. One is a stab with a crackle, heard in the hand; the other a swell, heard in a fight.

The audio step on this branch: `AUDIO: PASS (49 sounds: 0 heard, 19 delegated, 0 rejected, 30 not heard; 92 files, 71 tool tests; mix: 9 cue(s) over the catalog's player side; ...)`.

## 3. For the owner

`Saved\S4Report\Audio\2026-11-13_keeper_signatures\` in the Sound session's worktree: the six alone, the six in turn, and one as it falls in a run (the card's tablet and bell, then its Keeper). Nothing waits on it.

## 4. What is not verified

- Nobody has heard these, and of everything made so far they are the most a matter of taste: six composed gestures, each standing for an instrument this world does not have.
- Whether a player learns which Keeper is which from them. The numbers say the six differ; they cannot say they are remembered.
- The voices and the horn are three detuned tones through two formants and a low-pass. Whether they read as the Six or as an organ stop is for an ear.
- Tier, the passed-over line and the imbued hit, which the art direction also names, are not made: the first two wait for their HUD lines, the third for a second wave (`Validation/S4_audio_wave1.md`).
- Nothing raises these sounds. Their observer reads `OnBlessingTaken` and `OnKeeperChosen` and asks through the stinger queue; it is a wiring job's.
