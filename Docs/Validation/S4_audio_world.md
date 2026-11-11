# S4 audio: the sound world

For the scheduler and the owner. Written by the Sound module session on `plan/s4-audio-world` from `dev/s4` at 2edf5b7, after the first hearing any sound of this game has had. It changes no rule of D-056: the method stands (generated from recipes, one key, the danger band, the checks). It says what the sounds are made of, which the first ten did not ask.

## 1. The hearing

On 2026-11-11 the owner heard the ten interface sounds of `s4/audio-snd-ui` and answered: "风格不对" (the style is wrong), "全部" (all ten), and "音效应该符合游戏的故事背景和设定，具有史诗感宏大感的同时尽量简单，不刺耳，不惹人厌烦" (the sounds should fit the game's story and setting, feel epic and grand while staying as simple as possible, not harsh, not annoying).

Every number passed on those ten. They were soft sine tones: a system's beeps. A number can keep a sound out of the danger band and under a level; it cannot say what world a sound is from. That is the lesson, and §6 turns it into a check.

## 2. What the Keep is made of, and so what it sounds like

The Keep is a vast old structure of fitted white stone at dusk, whose six systems still run and are revered: a town pays them tithes and calls them the Six (`03_Narrative` §1). The art book draws cards as carved tablets and Cast-offs as bone ceramic over dark metal, the HUD marks the boss with a brass token, and the Reclaimer is an empty vessel with no motif of its own (`reference/original_docs/05_ArtDirection.md` §11). So the world is a temple that is a machine.

| What is there | What it gives the ear | Used for |
|---|---|---|
| Stone: slabs, tablets, steles | a dull knock, a scrape; it does not ring | the smallest, most frequent sounds; refusals; things placed |
| Bronze and brass: bowls, bells, seals, tokens | a struck tone that rings long, with a hum under it | what is granted: a hand, a pick, a level, a Keeper's sign |
| The hall: a very large stone room | a dark tail after a sound, seconds long | scale, on rare sounds only |
| Voices of the Six, and a horn | a low choir on a dark vowel, never a word | the Keepers when they act |
| The old systems: relays, latches, lines, turbines | heavy closures, a low hum that rises and steadies | the Keep's own acts: mechanisms, the Reclaimer's tools |
| Ceramic over metal | a crack, a dry shatter, a rattle | Cast-offs breaking: kills (a later family) |

**Register.** Low. Bodies sit from A1 to A3 (55 to 220 Hz) with their partials up to about 1.5 kHz. A small speaker still plays them because every sound keeps partials above 150 Hz (`C.AUD.SMALL_SPEAKER_LOSS_MAX_DB`).
**Intervals.** Open fifths, fourths and octaves, in A minor pentatonic. No thirds held as chords: a third is sweet, and this world is old and plain. A bell here is tuned so that all eight of its partials are notes of the key (hum, prime, minor third, fifth, octave, the fourth above, twelfth, double octave).
**Gestures.** One per sound: a strike, or a swell, or two notes. Never a tune.
**Tails follow rarity.** What is heard a hundred times a minute is dry and a blink long. What is heard once a minute may ring for seconds.

| How often | Example | Body | Tail |
|---|---|---|---|
| many a second | cursor, hits, fire | under 100 ms | none, or a breath of room at the edge of hearing |
| a few a minute | refuse, a tool, a pickup | under 300 ms | under half a second |
| once a minute or rarer | a hand opens, a card taken, a level, a Keeper, the boss | up to 2 s | 2 to 3 s of hall |

**What grand means.** Material, low register and the size of the room; never loudness, never length where length would tire. For a 60 ms cursor tick it means a small thing touched in a very large place: a pebble on a slab with the faintest answer from the walls. For a hand of cards it means one large object struck softly, and the hall holding it.
**What keeps it from tiring.** The most frequent sounds are the smallest and the driest; nothing sits in the band the ear is most sensitive to (`C.AUD.DANGER_BAND_HZ`); attacks are padded (`C.AUD.PLAYER_ATTACK_MIN_MS`), as a felt mallet is; variants and a drawn degree of the key stop a stream from repeating one take.

## 3. How it is made, with the standard library and exact operations

| Source | Method | In the sketch |
|---|---|---|
| A struck body (bell, bowl, slab, rail, latch) | modal synthesis: a sum of decaying sine partials at the body's ratios, each with its own decay, a padded attack, a short noise for the contact | `struck`, `bell`, `stone`, `clunk` |
| A voice or a horn | a harmonic stack, three detuned singers a note, two formant band-passes on a dark vowel (450 and 800 Hz), a slow attack | `voices`, `horn` |
| Breath, scrape, air | noise through two band-passes with a slow envelope | `breath` |
| The hall | six damped comb filters in parallel and two all-passes in series, the wet side kept under 1.5 kHz. Delays, sums and products only, so it is exact like the rest of the generator | `hall` |

**The tail is baked into the file.** D-056 left the echo out of the runtime, and this page asks for nothing new there. A baked tail is the same bytes every time and is read by the same checks as the body. Its costs are known: a rare sound's file is two to five seconds long, so the scheduler counts it as a voice for that long (`C.AUD.VOICES_MAX` has room: rare sounds are rare); and tails are per sound, so two tails overlap as two rooms would not. If the owner's ear finds that wrong, a shared reverb on a submix is the alternative, and it would be a decision record.
**The danger band survives.** Everything of this world ends in a low-pass at or under 1.8 kHz, tails included; the twelve sketch files read -48 dB or lower in the band against a limit of `C.AUD.PLAYER_BAND_SHARE_MAX_DB`. A warning then belongs to the same world by its material and stands apart by its size: a small bronze or iron thing struck hard is high and urgent, and the 2 to 5 kHz band is where it rings. The danger draft is redone on that idea once a direction is chosen.
**The key survives.** Every pitched source above takes its notes from the key, and the in-key bell is the proof that a struck sound need not leave it.

## 4. Three directions for the owner to choose from

The same four sounds in each: the cursor moves, a hand of cards opens, a card is taken, a refusal. Twelve files and three that play a direction's four in a row, in `Saved\S4Report\Audio\2026-11-11_ui_v2\` of the session's worktree, at the level the game would play them. No tool played them.

| | A. Stone and bronze | B. The voices | C. The old machine |
|---|---|---|---|
| In plain words | what Vigil's people strike in a great stone hall | the Six speaking as one | the Keep's own systems, old and huge |
| Cursor | a pebble on a slab | one low hum, a blink long | a relay closes in the wall |
| A hand opens | a great bronze bowl, struck softly | an open fifth and its octave on a dark vowel, a horn under it | a latch, a low line that rises and steadies, a second latch |
| A card taken | a tablet set down, then a smaller bell | a horn rises a fourth and the voices take the upper note | a heavy switch, then a fifth that rings like a struck rail |
| A refusal | a slab that does not move, and the scrape of its return | low voices, mouths closed, sinking a tone | a lock that will not turn: two short low stops |
| Closest pair of its four, dB | 14.0 | 13.5 | 8.6 |

All twelve hold every rule the audio step has for an interface sound (band share, padded attack, onset spike, lead, peak, small speaker), and each direction's four are told apart by spectrum alone against `C.AUD.PAIR_DISTANCE_MIN_DB`. None of that says which one is the Keep. The directions can be mixed: the materials of §2 are one world, and the owner may want A's bell for a hand and C's relay for the cursor.

## 5. What changes in the plan

- `s4/audio-snd-ui` cb996a5 is not landed. The interface family is written again in the chosen direction, as a new recipe branch.
- The danger draft and every later family take the same world. Kills are ceramic breaking; weapons are the old systems' tools; the Keepers' signatures are voices and their own material (Art §11's table already names an anvil, water, a page, glass).
- The catalog's length windows grow for rare sounds; nothing else in the catalog, the constants or the runtime moves. The tools, the gate step and the runtime job go on as they are.
- The first ten are kept in the session's sketch folder as the example of what the numbers let through.

## 6. A check the first hearing added

A machine cannot judge style. So every new family goes to the owner as audition files before it is wired into the game: three to five sounds at a time, each named for what it is for, at played level, with one question. A family is recorded as not heard until it is, and a recipe branch says in its note whether its sounds were heard and what was said. The numbers still do what they can: they keep a sound in its zone, at its level, out of the danger band and apart from its neighbours.

## 7. What is not verified

- Nobody has heard the three directions. Every word about their character above is from recipes, numbers and pictures, which is exactly what failed the first time.
- The hall is a simple one (six combs, two all-passes). Whether it reads as a great stone room or as a metallic ring is an ear's call.
- The voices are a harmonic stack through two formants. Whether they read as a choir or as an organ is an ear's call.
- Baked tails were not tried under density: no mix exists yet.
- The sketch is not committed. Its twelve files were built with the session's sketch core, which the landed core reproduces byte for byte on the twelve reference files of `S4_audio_tools.md`; the hall is new code and is not in the landed tools.
