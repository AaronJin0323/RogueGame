# S4 audio tools: the generator's tools and the gate step `audio`

Job 1 of `Validation/S4_audio_wave1.md` §8.1 (D-056), branch `s4/audio-tools` from `dev/s4` at 0f8a973. Python standard library only; no C++, no engine code, no sound played. Nothing here makes a sound for the game yet: the catalog and the manifest are header-only, and the Sound session writes the recipes (job 2).

## 1. What landed, and what proves it

| Item | Files | Proof |
|---|---|---|
| The generator core, ported from the session's S2 sketch with the same names and signatures; the sample rate's one source is `C.AUD.SAMPLE_RATE_HZ` | `Tools/audio/dtsynth.py` | **Port proof** (`audio_test.py` `PortProof`): the sketch's `recipes.py`, kept as a fixture inside the test with its code unchanged, run on this core, writes the twelve files of the sketch's `out\SW_*.wav` byte for byte (SHA-256 of each, read from the sketch's folder on 2026-11-11). Two processes write the same bytes |
| Exact operations only (D-056 item 3): an AST lint over the core and every recipe: no `random`, `cmath`, numpy, scipy, `statistics`, `decimal`, `fractions`; no `math` function but `floor sqrt frexp ldexp`; no `**` or `**=`; no `pow() round() hash()`; no `import math as`. In a recipe: no bare number (or expression of numbers alone) as an oscillator frequency, and no `hz()` note outside A C D E G | `Tools/audio/audio_check.py` (`lint_source`) | `Lint` tests: the core, the sketch recipes and the fixture recipes pass; ten planted operations and five planted pitches fail, each with one error; a planted `math.sin` in a recipe file fails the step |
| The measures of the sketch's `analyze.py`: third-octave bands, share, centroid, `lk100` (BS.1770 K-weighting), active length, lead, attack, onset spike, peak, crest, DC, small-speaker loss, onset count, spectral distance, upward spread, audibility. Band edges, the air line, the small-speaker line and the masking spread and window come from `C.AUD.*` | `Tools/audio/audio_metrics.py` | `test_the_measures_read_a_planted_signal`; the per-file fault tests below read through it. The fixture's kill sound reads the sketch's figures (attack 4.8 and 6.4 ms, `lk100` -19.3, band share at +4 semitones -40.8 dB for variant 1) |
| The catalog `sounds.csv` (header only), every cell validated | `Docs/09_Catalog/sounds.csv`, `audio_build.py` (`validate`) | `Catalog` tests: eleven planted bad cells, a duplicate id, a warning with a drawn pitch, a danger sound on the grid, a danger sound outside the Danger class; a recipe or recipe module that does not exist |
| Build into `Saved/AudioGen/` with a stamp (the hash of `dtsynth.py`, `audio_build.py`, every recipe module and the sample rate, plus each file's hash): an unchanged file is reused. `--update` rewrites the manifest (name, frames, SHA-256, thirteen measures rounded to 0.1). Measures are cached by file hash in `measures.json` | `Tools/audio/audio_build.py`, `Tools/audio/sound_manifest.csv` (header only) | `test_the_fixture_catalog_passes` runs the step twice: the second run rebuilds nothing; `test_a_changed_byte_against_the_manifest_fails`; catalog and manifest naming different files fails both ways |
| The gate step: tool tests, lint, catalog, licence rows, manifest names, build and hashes, per-file rules, variants, confusion sets; the generated table once it exists | `Tools/audio/audio_check.py`, `Tools/Gate.ps1` | `Files` tests, each fault planted alone in a fixture tree that otherwise passes: a clipped file; a body file with energy in the band at +4 semitones; a warning with a slow attack; a loop seam; byte-identical variants; a confusion pair too close (and the same pair against a warning passes); a family with no licence row |
| `DTSoundsGen.h` text: namespace `DTCat`, enums of side, class and pitch, an enum of ids, a constexpr table, zero rows handled | `Tools/audio/gen_sounds.py` | `Header` tests on fixtures: no rows, two rows, `--check` and `--write`, a hand edit fails both `--check` and the step. The header is not written into `Source/` in this job |
| The pictures and the audition files | `Tools/audio/audio_view.py` | Run by hand on the sketch's twelve files (a sheet of six and a third-octave chart, read as images) and an audition of two families; no automated test |
| 48 keys `C.AUD.*`, `GATED(S4)`, presentation only, each with `02_Systems/audio.md` and its test id as dependents | `Docs/constants.yaml`, `08_Constants.md` (Audio), `number_change_log.csv`, `Core/DTConstantsGen.h` regenerated | `gen_constants.py --check`, `docs_lint.py` |
| The licence lint's AI-service pattern: `udio` becomes `\budio\b` (D-056 ruling 3) | `Tools/catalog_lint.py`, `Tools/tools_test.py` | `test_a_row_naming_the_audio_tools_passes`, `test_a_row_naming_the_audio_service_fails` |
| Two licence rows, before any binary (R-PRE-10): `Content/DT/Audio/SFX/SW_*` (procedural) and `Content/DT/Audio/Mix/*` (in engine) | `Docs/09_Catalog/licenses.csv` | `catalog_lint.py` passes; the step requires a matching audio row for every file a catalog row writes |
| The R-PRE-11 include check treats `Audio/` as presentation (R-AUD-01) | `Tools/tools_test.py` | `test_a_sim_file_that_includes_audio_is_found`; the tree scan still finds only the known renderer owners |

## 2. The per-file rules of the step

All files: mono 16-bit at `C.AUD.SAMPLE_RATE_HZ`; peak at most `C.AUD.PEAK_MAX_DBFS`; no sample at plus or minus 32767; mean offset under `C.AUD.DC_MAX`; share above `C.AUD.AIR_LINE_HZ` at most `C.AUD.AIR_SHARE_MAX_DB`; `lk100` plus 20 log10 of the catalog volume inside the row's loudness window; active length (first to last sample over -40 dB of the peak) inside the row's length window. One-shots: first and last sample zero, lead at most `C.AUD.LEAD_MAX_MS`. Loops: the seam step (last sample to first) no larger than the 99th percentile of the file's own sample steps. Zone `body`: share in `C.AUD.DANGER_BAND_HZ` at most `C.AUD.PLAYER_BAND_SHARE_MAX_DB`, read after resampling 4 semitones up for pitch `key`, 12 for `ladder`, none for `fixed`; attack at least `C.AUD.PLAYER_ATTACK_MIN_MS`; spike at most `C.AUD.SPIKE_MAX_DB`. Zone `warn`: share at least `C.AUD.WARN_BAND_SHARE_MIN_DB`, attack at most `C.AUD.WARN_ATTACK_MAX_MS`, pitch `fixed` (a catalog rule). Zone `low`: at least half its energy in `C.AUD.LOW_ZONE_HZ`. Zones `warn` and `low` and side `ui`: small-speaker loss at most `C.AUD.SMALL_SPEAKER_LOSS_MAX_DB`. Variants of one row are not byte-identical. Every pair inside a confusion set: spectral distance at least `C.AUD.PAIR_DISTANCE_MIN_DB`, or a different onset count, or active lengths a factor of two apart.

Catalog rules beyond the schema, from the plan's §4.2 and `audio.md` R-AUD-05 and R-AUD-09: the danger side is the Danger class and only it; an interface sound is in the UI class; a `warn` zone sound is on the danger side; danger and interface sounds have pitch `fixed`; a danger sound has grid 0.

## 3. Commands

```
. Tools\Machine.ps1; $null = Set-DTAffinity; python -I Tools\audio\audio_check.py      # the gate step
python -I Tools\audio\audio_test.py                                                       # the tool tests alone
python -I Tools\audio\audio_build.py [--update] [--force]                                 # build; --update rewrites the manifest
python -I Tools\audio\gen_sounds.py --check | --write                                     # the runtime job writes the header
python -I Tools\audio\audio_view.py --family UI | <a.wav ...> | --bands <a.wav ...> | --audition [--out <dir>]
```

Readings on this PC, 2026-11-11: `AUDIO: PASS (0 sounds, 0 files, 22 tool tests; ...)` in 2.6 to 2.9 s whether or not `Saved/AudioGen` exists (the tool tests are the cost); the twelve sketch files built, measured and written to a manifest from nothing in 1.3 s, and checked again unchanged in under 0.1 s. `tools_test.py` 351 tests pass; `docs_lint`, `secret_scan`, `catalog_lint` and `gen_constants --check` pass. The whole gate on `dev/s4`: §5.

## 4. What is not verified

- No sound of the game exists: the step passes on an empty catalog. Its rules are proved on fixtures only, built from the sketch's recipes.
- The port proof holds on one PC and one Python (3.14.4). That the bytes hold on another machine follows from the construction (exact operations, integer random source) and was not tried.
- The mix checks (R-AUD-18: audibility floors, density readings) are not active and are named so in the verdict; `audio_metrics.audibility` and `spread` are ported but nothing calls them yet. The generated table check activates by itself once `Source/Dusktracer/Audio/DTSoundsGen.h` exists.
- The measures are read from the 16-bit file, not the floats before quantisation as the sketch did: the air share of the sketch's kill reads -80 dB here against -110 there (the quantisation floor); the figures compared by hand (attack, `lk100`, active length, band share) agree with the sketch's report to its rounding.
- `audio_view.py` has no automated test; its pictures were read by eye once.
- No sound was heard by anyone.

## 5. The gate
