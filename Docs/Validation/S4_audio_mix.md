# S4 audio: the mix model

> The off-line mix of `02_Systems/audio.md` R-AUD-18 and T-AUD-18 as built on `s4/audio-mix`: `Tools/audio/mix_model.py`, the stand-ins in `Tools/audio/recipes/standin.py`, and the readings' place in the `audio` step. The plan is `S4_audio_wave1.md` §5 item 3 and its density readings; the method was the Sound session's working sketch.

## 1. What it proves, and how

"Danger is heard first", as numbers. The model builds the loudest player mix the scheduler allows and reads every danger sound of the catalog against it.

1. **Roles.** The player side of a fight is five roles: fire (one voice stream per weapon), hit, kill, the crowd layer, the low layer. A catalog row takes a role by family (`Fire`: each row is one weapon) or by id (`Snd.Kills.Hit`, `Snd.Kills.Kill`, `Snd.Kills.LayerCrowd`, `Snd.Kills.LayerLow`). None of these rows exists yet. A role with no row plays its stand-in, at the levels of the model's table, and the output always names which roles were stand-ins.
2. **Tiers.** 5, 20, 60 and 100 kills a second, with 3, 6, 10 and 10 weapons firing. Ten stand-in weapons: rifle x4, heavy x3, light x3, interleaved so the three of the sparsest tier are one of each.
3. **Schedule.** Each weapon asks `C.AUD.FIRE_DENSE_VOICES_PER_S` voices a second. Kills: one voice each under `C.PRE.DENSITY_LOD_KILLS_PER_S`, then `C.AUD.KILL_DENSE_VOICES_PER_S` a second. Hits land at 20 a second per kill a second (`HITS_PER_KILL`, from T-AUD-07's sizes: thousands of hits at about a hundred kills a second). A stream's requests are counted per sim step at `C.SIM.FIXED_HZ`, with its cadence jittered (owed requests grow by rate x step x (0.4 + 1.2u)) so the mix is not a clock. The requests of one step are one voice, louder by 1 + its aggregation gain x log2(count), capped at `C.AUD.AGGREGATE_MAX`; a request inside its least interval merges, counted. Grid rows start on the next multiple of `C.AUD.GRID_STEPS` steps. Per 1/60 s frame the candidates go loudest first: at most `C.AUD.STARTS_PER_FRAME_MAX` start, the rest wait one frame and are then dropped; a start past `C.AUD.PLAYER_STARTS_PER_S_MAX` in the second up to the frame, or with no free slot of `C.AUD.VOICES_MAX` - `C.AUD.VOICES_DANGER` - `C.AUD.VOICES_UI`, is dropped and counted. A layer that is on holds one slot (`LAYER_SLOTS`: the runtime's layer is one looping voice). Both are model constants, not keys, confirmed by the scheduler: the game reads neither, and if it ever does, each becomes a key with a change-log row. Variants play in turn per stream; a key voice draws one of its written note's five degrees by resampling, and a level within `C.AUD.LEVEL_JITTER_DB`. The crowd layer is on from `C.PRE.DENSITY_LOD_KILLS_PER_S`, the low layer from `C.IMP.DENSITY_FLOOD_KILLS_PER_S`, both at full level. One seed, through `dtsynth.Rng`: two runs give the same samples.
4. **Render.** Mono floats at `C.AUD.SAMPLE_RATE_HZ`, 8 s a tier, read past the first second.
5. **Readings.** Per tier: integrated K-weighted loudness, the share in `C.AUD.DANGER_BAND_HZ` (at most `C.AUD.PLAYER_BAND_SHARE_MAX_DB`), crest (printed for every tier on the one seed; judged against `C.AUD.MIX_CREST_MAX_DB` only as ruled in §2), and the rise from 5 to 100 (at most `C.AUD.DENSITY_RISE_MAX_DB`). Then the danger side against the 100 tier's mix lowered by `C.AUD.DUCK_DB` (the duck holds while a cue sounds; its attack is left out), every variant, the worst kept:
   - a `warn` zone row: `audio_metrics.audibility` in the danger band, at least `C.AUD.DANGER_SMR_MIN_DB`; the figure with no duck is printed beside it (the same reading shifted by the duck: band powers scale with the gain squared);
   - a `low` zone row (the heartbeat): audibility in `C.AUD.LOW_ZONE_HZ` against the mix with its low layer lowered by `C.AUD.LOWHP_LOWEND_DUCK_DB` as well, at least `C.AUD.HEARTBEAT_SMR_MIN_DB`;
   - a row of family `Warn`: its played loudest 100 ms over the ducked mix's, at least `C.AUD.WARN_OVER_MIX_MIN_DB`;
   - every danger row: the ducked mix with it on top, at alignments `C.AUD.MASK_WINDOW_MS` apart, peaks at most at `C.AUD.LIMIT_DBFS`;
   - the control: the loudest player-side one-shot, read as a warning, must not reach `C.AUD.DANGER_SMR_MIN_DB`. If it does, the step fails: the model is broken.
6. **In the step.** With no danger-side row in the catalog there is nothing to read; the verdict says `not active yet: the mix checks (R-AUD-18)`, as before. With one, the readings join the `audio` step and the verdict names them, for example `mix: 9 cue(s) over stand-ins for fire, kills, layers`. They are cached in `Saved/AudioGen/mix.json` under a key over the recipes and the core (the build's snapshot hash), the catalog, the manifest, the bytes of every file the model reads, the keys it reads, `mix_model.py` and `audio_metrics.py`. The rendered stand-ins are kept as exact doubles in `Saved/AudioGen/standins/`, keyed on `dtsynth.py`, `recipes/world.py`, `recipes/standin.py` and the sample rate.
7. **Audition.** `python -I Tools/audio/mix_model.py --render <dir>` writes 16-bit mono WAV at played level: each tier's mix, and the 100 tier's mix with every danger sound on top in turn, two seconds apart, with the duck's curve on the player side (down within `C.AUD.DUCK_ATTACK_MS`, held while the sound lasts, back over `C.AUD.DUCK_RELEASE_MS`, linear in dB) and the low layer's own duck under a `low` zone cue. It plays nothing.

## 2. The stand-in mix, by tier

All five roles are stand-ins (the catalog has no `Fire` or `Kills` row). Seed 1, 8 s a tier read past the first second. The table as `mix_model.py` prints it now: the crest of every tier, and no median, because no tier is judged while a role is a stand-in.

| Kills/s | Weapons | Layers | Asked | Merged | Started | Dropped | Most starts in 1 s | Most voices | LK integrated | Danger band dB | Crest dB | Median | Peak dBFS |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 5 | 3 | none | 966 | 705 | 259 | 0 | 35 | 7 | -27.3 | -53.3 | 17.3 | - | -8.5 |
| 20 | 6 | crowd | 3622 | 3194 | 428 | 0 | 57 | 12 | -23.6 | -54.8 | 13.9 | - | -8.2 |
| 60 | 10 | crowd, low | 10249 | 9634 | 474 | 139 | 60 | 17 | -22.1 | -55.6 | 12.7 | - | -7.5 |
| 100 | 10 | crowd, low | 16651 | 16034 | 478 | 138 | 60 | 17 | -22.2 | -54.5 | 14.1 | - | -6.2 |

Under it the model prints `crest not judged: stand-ins for fire, kills, layers (printed on one seed)`.

Loudness rise, 5 to 100: 5.1 dB (limit 6). The band share holds by more than 20 dB in every tier. The start cap binds in the two densest tiers (60 starts in a second, 138 to 139 dropped in 8 s), the voice cap never (17 of 22). The control, `standin.fire_heavy` variant 2, reads -66.7 dB as a warning, far under the floor.

**Crest: the ruling (2026-11-12, option A with the median).** The crest is printed for every tier and judged against `C.AUD.MIX_CREST_MAX_DB` only where both hold: no role of the mix is a stand-in, and a layer is on in that tier. A sparse mix is single sounds with silence between, and peak over RMS says nothing there. Where it is judged, the reading is the median over `CREST_SEEDS` = 5 seeds, the run's own seed first: one seed's draw moves the crest by more than 1 dB and the limit sits inside that, so one seed over the line by a tenth is not a finding. The count is odd, so the median is one seed's own reading (an even count would take the mean of the two middle ones). The extra seeds are rendered only when the crest is judged, so today's run with stand-ins costs no more than before. The other readings stay on the one seed. The key is unchanged.

For scale only, since nothing judges it: read as if no role were a stand-in, the stand-in mix's five seeds give 13.9 to 16.9 dB at 20 kills a second (median 14.1), 12.7 to 15.3 at 60 (median 15.0) and 14.1 to 14.8 at 100 (median 14.4). Those extra seeds cost about 2.7 s.

**What follows when the first real family replaces a stand-in.** Its role leaves the stand-in list and the verdict names one role fewer, but the crest stays unjudged until the last stand-in has gone. Every other reading moves with the new family at once: band share, rise, audibility, over-mix, peak and control. When `Fire` and `Kills` both land with their four ids, the crest is judged in the 20, 60 and 100 tiers on the median of five seeds. A median over the key then fails the step, and the fix is in the family's levels or the ruling, not in the seed.

**Test cues.** A ping of E7 and G7 at 0.25 reads 36.3 dB of audibility (30.3 with no duck) and 6.4 dB over the mix; a thump of A1 and A2 at 0.6 reads 13.8 dB in the low zone. These are fixtures of the tests, not sounds of the game.

**Time.** The step's added cost with nothing changed is its tests (about 5 s of the step's 9 to 12 s on this PC while other sessions run; 4.8 s before). A recompute on a changed key, with nine cues, takes about 18 s on top, and about 3 s more once the crest is judged.

## 3. What the model is not

- **Not the engine's mixer.** It sums the files with the scheduler's gains; the engine's own summing, its limiter and the device do not enter. The last step, from a voice handed to the engine to air, is the owner's to hear (`S4_audio_wave1.md` §6.3).
- **Band energy is a proxy for hearing.** Third-octave powers with an upward spread of 5 dB per band stand in for masking; "leads in two bands" stands in for "is heard".
- **The stand-ins are drafts nobody has heard.** Their numbers say the method works and where the limits sit; they say nothing about how a fight sounds. Each family that replaces a stand-in goes to the owner by ear first, and the model's readings move with it.
- **A model of the worst case, not of a run.** The rates, the hits per kill and the full-level layers are chosen to load every cap; a real run's voice log (`-DTAudioLog`) is the evidence of what a run does.
