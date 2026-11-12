# S4 audio danger wiring: the Warn and Danger observers, the heartbeat's two forms

Job 5 of `Validation/S4_audio_wave1.md` §8.1 (D-056), its second half. Branch `s4/audio-danger` from e910f6b, with `s4/audio-snd-danger-ok` 42dcd73 merged (the nine rows `delegated 2026-11-12`, playable, their eleven SoundWaves); `dev/s4` was at the same commit. The spec is `02_Systems/audio.md`; the first half is `Validation/S4_audio_danger.md`.

**No run has played a voice aloud.** Every run made for this job was a tool process (the automation tests, the gate, the bot runs, the perf scene): no engine sink was made and the application volume was held at zero. The danger sounds are raised, scheduled and given their assets; nobody has heard them through the game.

## 1. The owner's first question: the heartbeat's dip, in numbers

Computed in `Dusktracer.Audio.Heartbeat` and `Dusktracer.Audio.LowHpHold` from the scheduler's own duck curve, sampled once a millisecond over one beat in a steady low stretch (the real table's heartbeat row: a voice of 400 ms; one beat every `C.HUD.LOW_HP_PULSE_S` = 0.8 s; the duck's attack 20 ms, depth `C.AUD.DUCK_DB` = -6 dB, release 250 ms). Not estimated.

| Form | How it plays | Depth | Below full, per beat | Average level over a beat |
|---|---|---|---|---|
| a (the default; Shipping) | each beat ducks the Player group like any danger voice | -6.00 dB | 650 ms of every 800 (the 400 ms voice, then the 250 ms release) | -3.86 dB averaged in dB; -3.49 dB averaged as gain |
| b (`dt.Audio.LowHpHoldDb` below zero) | the Player group is held at the set depth for as long as health is low; the beat adds no duck of its own | the set depth (-4.00 dB at -4) | all of it, level: no movement at a beat | the set depth |

So form a pumps: the Player group drops 6 dB at every beat and is back at full for only 150 ms before the next one. Form b sits still. The mix model reads the heartbeat 6.3 dB over the mix with no duck, against a floor of 9 dB; a hold of -3 would give 9.3 and -4 gives 10.3. **The suggestion is to try `dt.Audio.LowHpHoldDb -4`** in a development build (the console, or `-ExecCmds="dt.Audio.LowHpHoldDb -4"`), get low and listen against `0`. Under form b a warning or another danger voice still takes the group to `C.AUD.DUCK_DB` and back to the hold.

## 2. What landed, and what proves it

| Item | Files | Proof |
|---|---|---|
| The Warn family: at each step's end, the step's public telegraph view, at most `C.AUD.SCAN_MAX` entries; each telegraph new this step raises its shape's warning: band → `Snd.Warn.Band`; arc → `Snd.Warn.Arc`; ring, and the boss's ring with a gap, spiral lane and hazard ring → `Snd.Warn.Ring`; aim line → `Snd.Warn.Line`. Several new ones of a shape in one step are one request carrying the count. No take-once queue, no walk of bullets or enemies | `Audio/Observe/DTObserveWarn.h/.cpp` | `Dusktracer.Audio.Observer` (T-AUD-02) |
| How "new" is known: the view has no ids, so the observer keeps last step's copy (at most the scan cap) and matches. A telegraph is the one of last step when that one has the same shape, a `Progress` no greater, and an origin within 150 uu (`DTWarnEvents::MatchUU`: an origin moves at most that far in a step); each of last step's is matched once; what is left over is new. A tell that stands for many steps is new once; a tell that starts again (its `Progress` drops) is new again | the same | Observer: a band standing a hundred steps, `Progress` growing and its origin drifting, raises exactly one warning; the same view twice raises none; two new rings in a step are one request of two; a restarted tell and a ring moved past the match distance are new |
| The Danger family: enemy fire by the growth of the enemy rounds' spawned counter (one request carrying the step's growth; a counter that goes down is a new baseline); the local hero's hurt and down, from `UDTHeroSubsystem::OnHeroEvent` (bound at the observer's bind, removed at its unbind), one request each carrying the step's count; the boss's change of phase while it stands both steps | `Audio/Observe/DTObserveDanger.h/.cpp` | Observer: each moment once, an unchanged step none, the boss's first phase and its leaving none |
| The heartbeat (R-AUD-12): the observer reads the HUD's own condition, `DTHud::LowHp(Hp, MaxHp, sim seconds, ...)`.bLow for player zero, on the HUD's own clock (the sim's step index over `C.SIM.FIXED_HZ`, as `ADTHUD` passes it), and beats when the step crosses into a new pulse (`floor(sim seconds / C.HUD.LOW_HP_PULSE_S)` changes), the moment the HP bar's fill starts full again. It needs no HUD or sim file edited: the pure function and the step index are public | the same | `Dusktracer.Audio.Heartbeat` (T-AUD-12) |
| Heartbeat, form b: one console variable, `dt.Audio.LowHpHoldDb` (float dB, default 0, not compiled in Shipping, never above zero). Below zero, while the low-health state holds, the observer asks the scheduler for a held depth and marks the heartbeat's request as one that does not duck. The scheduler's duck is now two segments with the same slopes, the danger voices' and the hold, and the lower wins; the hold enters by the attack and leaves by the release. Outside tests, 75 lines added and 29 removed in the scheduler, the observer interface and the subsystem's requests (most of it the old curve rewritten as a segment), and the variable's dozen in the Danger observer; voices are counted as before | `Audio/DTAudioScheduler.h/.cpp`, `Audio/DTAudioObserver.h`, `Audio/DTAudioSubsystem.h`, `Audio/Observe/DTObserveDanger.cpp` | `Dusktracer.Audio.LowHpHold` |
| The observer interface: a run request may say it does not duck; the requests take a held depth; a family has `Begin` and `End` around its bind. The three families are registered in `FDTAudioObserver`'s constructor, one line each | `Audio/DTAudioObserver.h/.cpp` | every observer test |
| The four Warn rows wait `C.AUD.WARN_MIN_INTERVAL_MS` between voices | `Tools/audio/audio_test.py` | `Catalog.test_the_warnings_keep_the_warning_interval`, on the real `sounds.csv` |
| T-AUD-12 `Dusktracer.Audio.Heartbeat`: a staged run of two thousand steps (full, low, full, low, down) through the observer and the scheduler on the real table, with a five-second stop of the sim inside it; a beat only while the HUD shows low HP, one for each pulse the HUD starts while low, none above the line or down, none while the sim stands, the gaps a pulse's period to the step and on average to a tenth; a new run's beats fall on its first step and then its own pulses; form a's numbers of §1 | `Tests/Audio/DTAudioTests.cpp` | the test |
| `Dusktracer.Audio.LowHpHold`: with a hold of -4 the curve goes down once when health turns low (within the attack), stays level across the beats with no send, goes to `C.AUD.DUCK_DB` for a warning and back to the hold, returns to zero within the release on recovery and when down; with the hold at zero every frame's curve and every voice equal those of the runtime without form b, sample for sample; the variable exists, is off by default, is read and is never above zero | the same | the test |
| `Dusktracer.Audio.DangerPlayable` (editor): each danger moment raised by its family's rule, through a recording sink, gets a non-null sound for each of the nine ids, missing none | the same | the test |
| `Dusktracer.Audio.SimUntouched` with the danger observers on: the bot run is armed with two turrets and two rushers at its start (both arms alike: the opening flock neither shoots nor tells), runs a frame after every step, and asks for warnings and enemy fire; steps, input hash, hero state and telemetry equal the run with audio off | the same | the test |
| Floor 597 to 600 | constants, `08_Constants.md`, the change log, the regenerated header | the gate's `tests` step |

Where the brief left a choice, decided here: a blow that downs the hero raises both its hurt and its down (each is a public event; the scheduler orders and caps them); a director drop-in marker is not a telegraph of the view and raises nothing; the beat falls on the first step after the pulse's top, because the top falls between steps (the fill there is within one step's fall of full: 0.9976).

## 3. Per-id counts from a run with enemies

From `Dusktracer.Audio.SimUntouched`'s armed bot run (seed 31, Steered Storm Tempest, 45 s of sim time, a frame after every step):

| Id | Asked | Played | Merged | Dropped |
|---|---|---|---|---|
| `Snd.Warn.Line` | 9 | 8 | 1 | 0 |
| `Snd.Warn.Band` | 2 | 1 | 1 | 0 |
| `Snd.Warn.Ring` | 0 | 0 | 0 | 0 |
| `Snd.Warn.Arc` | 0 | 0 | 0 | 0 |
| `Snd.Danger.EnemyFire` | 24 | 21 | 3 | 0 |
| `Snd.Danger.HeroHurt` | 0 | 0 | 0 | 0 |
| `Snd.Danger.HeroDown` | 0 | 0 | 0 | 0 |
| `Snd.Danger.Heartbeat` | 0 | 0 | 0 | 0 |
| `Snd.Danger.BossPhase` | 0 | 0 | 0 | 0 |

Merged is the interval at work (two rushers' bands inside `C.AUD.WARN_MIN_INTERVAL_MS`; turret volleys inside enemy fire's). The bot was not hit in those 45 s, met no ring or arc and no boss: those five ids are proven by the staged tests of §2, not by this run.

## 4. The gate

- The gate of record, on 38d9a5b (`s4/audio-danger` with `s4/audio-snd-danger-ok` 42dcd73 merged, which is `dev/s4`, and `git lfs checkout` run): `GATE: PASS (38d9a5b; 3 not run; mask FFFFF3FC)`, tests `600 run of 600 found, 0 failed, floor 600`, restart `276faf8e / 276faf8e`, meta `ba2a3ac8 / ba2a3ac8`, `AUDIO: PASS (19 sounds: 0 heard, 19 delegated, 0 rejected, 0 not heard; 21 files, 72 tool tests; mix: 9 cue(s) over stand-ins for fire, kills, layers; ...)`, `TOOLS: PASS (416 tests)`; build, shipping and devgame at 0 warnings; not run: pad, scope, mac. The commit after it is this note; the text steps were run on the final tip.
- Silence with the danger assets present, from that gate's `Saved\Logs\Smoke_combat_staged_en.log`: `Audio: start: tool process; no engine sink; volume held at 0; 21 of 21 wave(s) resolved; duck mix found` and, at the staged fight's end, `Audio: end: asked 143, voices started 49, handed to the engine 0, missing 0` (the import job's same scene read `asked 1`: the danger observers now ask).
- The five seed 9107 arms, each alone on 38d9a5b (`Tools\BotRuns.ps1 -Arms <arm> -FirstSeed 9107 -Seeds 1`), match `S4_p0.md` section 8 byte for byte: None `00e0d4ec` 44137 steps, 535 bytes, `b6a4878cb23adae7`; Random `51369747` 59380, 663, `485ed85694ad5c92`; Steered:Forge:Hammer `e3293942` 57749, 679, `b3cda2804c6a4664`; Steered:Storm:Tempest `b046559b` 60231, 680, `cab7cae12d42f789`; Steered:Tide:Frost `def51848` 48598, 563, `dc02a5d3f44c2d11`. The Storm run's audio lines: `Audio: end: asked 5571, voices started 465, handed to the engine 0, missing 0`.

## 5. Cost

`& Tools\Perf.ps1 -Scene S4` on 38d9a5b: `PERF: PASS (1920x1080, game p50 4.15 / p95 6.58 ms, GPU p95 4.21 ms, load 5808.00 bullets / 400.00 enemies, 1800 frames)`, and its closing audio line `Audio: 5063 frame(s); p95 ms observe 0.0061 schedule 0.0013 hand 0.0001; worst 0.0390 0.0459 0.0009`. The three p95 add up to 0.0075 ms, under `C.AUD.GAME_THREAD_MS_MAX` (0.05 ms); the scene asked 131 sounds and started 53. The observe part now holds the Warn scan and the Danger reads every step (the runtime job's figure, with only the interface family, was the runtime standing). A bot run at 600 steps a frame reads higher per frame (the Storm arm: observe p95 0.1194 ms) because one frame there carries 600 steps' observing; it is not a game frame.

## 6. What is not verified

- **Nothing has been heard.** The owner's hearing of the danger family, and of the heartbeat's two forms, is §1's question.
- The heartbeat in a real run: no bot run of this job went low on health, so the observer's reading of the hero's state in a world is proven by the build and the staged tests, not by a run that beat.
- Hurt, down, ring, arc and the boss's phase in a real run (§3).
- Form b at the engine: the held depth reaches the engine as the same Player-group curve, through the same sender; the engine sink exists only in a person's launch.
- The match distance (150 uu a step) is read from the enemies' and the boss's movement, not measured over every tell; a telegraph that jumps further in one step would warn twice.

## 7. Not in this job: the last fifth of a tell

A mark in the last fifth of a telegraph needs the lead's length, which is not known at its first step: the view publishes only `Progress`. It could be read from the second step on (the step's growth of `Progress` gives the lead in steps), or from a public getter of the lead's length, which would be an edit to the enemies' and the boss's files.
