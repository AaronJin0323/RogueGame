# S1 session A: readiness of the reading tools

**Build:** no session build read. The tools are on `s1/session-a-readiness` (from `main` 4d17faf); the dry read below read the two run files of build `e4926d4` that the scheduler's package check exported.

## What this is

Session A is three outside players, two runs each, on the Shipping package. Each PC hands back a zip and a paper sheet. After it the S1 gate decision sets the two pressure bands, names a bot skill row and judges the revert clauses of D-036, D-045, D-046, D-053 and D-054. The game already writes every field those need; the gaps were in the kit sheet and the reading tools. This note says what now reads each one, and what stays unread whatever is played.

One command now reads a session:

```
python -I Tools/session_read.py Saved\Sessions\A --validators Saved\Sessions\A\validators.csv
```

It copies the returned zips into `Saved\SessionRead\A\`, takes only the run files and the manifest out of the copies, and prints every S1 gate item or `NOT READ (why)`. What only the paper holds prints as `SHEET:` with the kit's question number.

## The audit's gaps

The session A audit (read on `main` ee38eb1, package `e4926d4`) named sixteen gaps. Each, with what now reads or states it; tests are in `Tools/tools_test.py`.

- **G1, not closed** (it needs a game change, which this branch does not make; both fields are optional). `run_start` has no `config` field, so a Shipping file cannot be told from the Development proof build by its content. And `boss_start` holds no health. `session_read.py` D-046 derives the health at the boss from the last `hp_after` instead (`SessionRead.test_d046_reads_each_second_run_at_the_boss`).
- **G2, closed.** `session_read.py` MINUTES prints the per-minute damage and dash tables. BANDS prints the middle half, the quarter-of-median floor and both bounds, and ends `NOT SET` (`test_the_minute_table_marks_the_partial_last_minute`, `test_bands_print_not_set_under_three_runs`, `test_the_band_arithmetic_on_four_hand_checked_runs`).
- **G3, closed.** `owner_read.py run --t <s>` prints `HP lost by <s> s` with the lower-bound wording; `session_read.py` HP600 uses the same function (`OwnerRead.test_run_honours_t_with_the_lower_bound_of_a_run_that_ended_before`, `SessionRead.test_hp600_with_a_run_down_at_480_s_reads_at_least`).
- **G4, closed.** The `pressure.py` group SOURCES lines and `session_read.py` D-045 read hits and landed drop-ins over the runs that carry the count, and name the rest (`PressureStatistic.test_a_group_reads_the_landed_count_over_the_runs_that_carry_it`, `SessionRead.test_d045_reads_drop_in_hits_over_the_runs_that_carry_the_count`).
- **G5, closed.** The pickup ratio of a run that ended before 300 s reads `unread` in `owner_read.py` and `NOT READ` in `session_read.py` READS (`OwnerRead.test_the_pickup_ratio_of_a_run_that_ended_before_300_s_is_unread`, `SessionRead.test_slow_shares_sorted_a_crash_unread_and_the_pickup_ratio_unread_under_300_s`).
- **G6, closed.** The `owner_read.py` timeline marks hits `[drop-in]` and `[charging]` and lists restores. `session_read.py --timeline` adds `off frame`, and its header maps the HUD timer to `t` (`OwnerRead.test_the_timeline_marks_drop_ins_charges_and_restores`, `SessionRead.test_a_bullet_on_frame_at_landing_and_off_at_the_fire_time_estimate`).
- **G7, closed.** `session_read.py` D-036 and OFFFRAME test every source against the frame derived from the camera constants, a round also at its fire time (`test_the_frame_comes_from_the_constants_and_tests_each_edge`, `test_a_bullet_on_frame_at_landing_and_off_at_the_fire_time_estimate`).
- **G8, half closed.** `session_read.py` RUN lists the input device of every run in a returned zip, and ROW 1 counts only `pad` (`test_row_one_needs_a_pad_run_and_kbm_never_counts`). The gate's `pad` step still reads only the development PC's folder (`Tools/PadCheck.ps1`), so a pad run from another PC never turns it to PASS. `package.md` Open records that; closing it is a PadCheck change outside this branch.
- **G9, half closed.** `power_split.py` reads a folder (`Attribution.test_a_folder_reads_every_run_and_one_file_as_before`). `Tools/probe.py` still cannot take a person's build: it rebuilds bot snapshots only, so pass rule row 7 stays a probe reading on the session build (`ROW 7:`).
- **G10, closed.** `pressure.py --check T-DIR-02` prints `NOT SET` while `DRAFT_BANDS` holds it, and a test ties that list to `10_Tuning.md` §1 (`PressureStatistic.test_draft_bands_print_not_set_with_the_same_medians`, `test_the_draft_list_follows_the_tuning_table`).
- **G11, closed.** The kit's run table has one row per run: tester, first or second run, launch time, input, language, end and seed. The scheduler types it into `validators.csv` (`run,validator,order`), and `session_read.py --list` prints the rows to fill (`test_list_prints_only_the_csv_to_fill`, `test_a_run_not_on_the_sheet_is_left_out_and_no_sheet_reads_not_read`).
- **G12, closed in the kit.** Questions 8 and 14 and the drop-in row cover D-045 ("no time to get away", unprompted and prompted kept apart). Question 7 covers D-046 ("what came closest to killing you"). Question 13 and the slow-motion row cover D-054. Questions 10, 11, 12 and their rows cover D-036's held items. The tool prints each one as a `SHEET:` line.
- **G13, closed in the kit.** "The owner, after the session" is session A row 3. The row "Took Hammer Hand while holding the Arc Blade or the Orbital Drones" leads to question 6, and `session_read.py` D-053 reads the same thing from the files (`test_d053_names_who_held_the_key_with_the_blade_before_or_after_the_pick`).
- **G14, closed in the kit.** Hand-over: the owner puts the zip and the sheet in `Saved\Sessions\A\<tester>\` and says so in the scheduler's session.
- **G15, closed in the kit.** The pass rule names the two rows D-048 added: **Unseen hit** and **Elite**.
- **G16, closed.** The manifest text in `Tools/PackageFiles/ExportTelemetry.ps1` now names every hit's source and positions, the drop-in and charging marks, restores, the drop-in count, whether a test bot ran the run at fast speed (`run_start.bot_fast` says no more than that) and the five clock readings of R-TEL-08. The kit's "What the zip holds" and `package.md` §Data say the same (`Manifest.test_the_manifest_names_the_five_clock_readings_and_the_two_marks`). From this commit the built package is stale (`Tools/PadCheck.ps1` compares `Tools/PackageFiles`), so it is rebuilt before session A.

The audit's kit mismatches are gone in the kit as given. It names the runtime path `Engine\Extras\Redist\en-us\vc_redist.x64.exe` and says what is true only on the development PC (slowdown, the faulty processors, the owner's own runs in the export). It ties the `pad` step to that PC.

What the tool reads for each S1 gate item:

| Item | Section of `session_read.py` | Test |
|---|---|---|
| Returned zips are data: copy, then read only run files and the manifest, by base name | (copy step), `NOTE:` | `test_a_hostile_zip_leaves_nothing_outside_the_work_folder_and_gives_only_run_files` |
| One run once across zips | (copy step), `NOTE:` | `test_the_same_run_in_two_zips_counts_once_and_a_differing_copy_keeps_the_ended_one` |
| Who played which run, first or second | the sheet, `--list` | `test_a_run_not_on_the_sheet_is_left_out_and_no_sheet_reads_not_read`, `test_list_prints_only_the_csv_to_fill` |
| Row 1 (three validators, two runs each, a pad, zh-Hans) | `ROW 1:` | `test_row_one_needs_a_pad_run_and_kbm_never_counts` |
| Damage taken and dashes in every minute (D-048) | `MINUTES:` | `test_the_minute_table_marks_the_partial_last_minute` |
| The band arithmetic (D-046, `10_Tuning.md §2`) | `WINDOW:`, `BANDS:` | `test_bands_print_not_set_under_three_runs`, `test_the_band_arithmetic_on_four_hand_checked_runs` |
| HP lost by ten minutes, and the bot row that brackets it | `HP600:`, `BRACKET:` | `test_hp600_with_a_run_down_at_480_s_reads_at_least`, `test_the_bracket_names_the_rows_under_and_over_and_says_when_none_brackets` |
| D-045 drop-in hits | `D-045:` | as G4 |
| D-046 runs alive at 600 s, restores, second run at the boss | `D-046:` | `test_d046_reads_each_second_run_at_the_boss` |
| D-036 sources off the frame | `D-036:`, `OFFFRAME:` | `test_the_frame_comes_from_the_constants_and_tests_each_edge`, `test_a_bullet_on_frame_at_landing_and_off_at_the_fire_time_estimate` |
| D-053 the Key with the blade or the drones | `D-053:` | `test_d053_names_who_held_the_key_with_the_blade_before_or_after_the_pick` |
| D-054 slow share | `D-054:` | `test_slow_shares_sorted_a_crash_unread_and_the_pickup_ratio_unread_under_300_s` |
| Kill time, pause, L8 and L16, pickup ratio, attribution | `READS:` | as G5 |
| HUD times on the sheet against the files | `TIMELINE:` (its header) | `test_a_bullet_on_frame_at_landing_and_off_at_the_fire_time_estimate` |
| The first Trait card offered and taken (`C.KEEPER.TRAIT_PREREQ_CARDS`) | `READS:` | `test_the_first_trait_card_offered_and_taken` |
| Bot skill rows before any session | `BRACKET:` | `test_bots_read_without_a_session` |

## Dry read

PadCheck's export (`Saved\PadCheck\export`, one zip, two Shipping bot runs of 65 s on build `e4926d4`, en and zh-Hans) was copied into `Saved\DryRead\A\T1\` and read with a two-line sheet that names both runs as `T1`, first and second:

```
run,validator,order
770077_20261111_155519,T1,1
770077_20261111_155626,T1,2
```

`python -I Tools\session_read.py Saved\DryRead\A --validators Saved\DryRead\sheet.csv --work Saved\DryRead\work --timeline 770077_20261111_155519` printed:

```text
NOTE: work folder Saved\DryRead\work
SESSION: folder Saved\DryRead\A; 1 zip(s), 0 loose run file(s), 1 manifest(s); 2 run(s)
SESSION: builds seen e4926d4 x 2
SESSION: 2 run(s) on the sheet from 1 validator(s) (T1); 0 run(s) not on the sheet
SESSION: player 0 only (solo runs)
RUN: 770077_20261111_155519 | validator T1 | order 1 | local 2026-11-11 15:55:19 | utc 2026-11-11T04:55:19.171Z | build e4926d4 | culture en | input kbm (the default value: proves nothing) | end bot_timeout at 1:05 (65.0 s) | level 3 | keepers Patron Storm | slow share 0.0000 (0 dropped of 3900 steps) | seen in T1
RUN: 770077_20261111_155626 | validator T1 | order 2 | local 2026-11-11 15:56:26 | utc 2026-11-11T04:56:26.945Z | build e4926d4 | culture zh-Hans | input kbm (the default value: proves nothing) | end bot_timeout at 1:05 (65.0 s) | level 3 | keepers Patron Storm | slow share 0.0000 (0 dropped of 3900 steps) | seen in T1
ROW 1: validators T1 2 run(s); pad not seen; zh-Hans seen in 770077_20261111_155626: does not hold (1 validator(s) with two runs, need 3; no pad run (kbm never counts))
SHEET: none of the validators is the owner (row 1, the run table)
ROW 2: SHEET (question 2)
ROW 3: SHEET (the owner's line)
MINUTES: sim minutes, k = 60k <= t < 60(k+1) s of sim time; hands pause the clock, so a minute here is a minute of play; `-` after the run ended; `*` marks the last, partial minute and holds its total
MINUTES: damage taken per minute, the sum of hero_damaged amount / max_hp
| run | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 770077_20261111_155519 | 0.000 | 0.000* | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| 770077_20261111_155626 | 0.000 | 0.000* | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
MINUTES: dashes per minute
| run | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 | 15 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 770077_20261111_155519 | 0 | 0* | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
| 770077_20261111_155626 | 0 | 0* | - | - | - | - | - | - | - | - | - | - | - | - | - | - |
MINUTES: 770077_20261111_155519 restores by minute (share of max HP): none
MINUTES: 770077_20261111_155626 restores by minute (share of max HP): none
WINDOW: 770077_20261111_155519 damage taken -/min, dashes -/min, alive 0.00 of 3.00 min in 600-780 s: ended inside or before the window: not judged
WINDOW: 770077_20261111_155626 damage taken -/min, dashes -/min, alive 0.00 of 3.00 min in 600-780 s: ended inside or before the window: not judged
BANDS: NOT SET (0 run(s) alive through 600-780 s from 0 validator(s); need 3 and 2; session B becomes a second calibration)
HP600: 770077_20261111_155519 HP lost by 600 s 0.000 of max HP (restores by then 0.000, never subtracted), dashes 0.00/min over the first ten minutes alive; ended at 65.0 s (bot_timeout): a lower bound
HP600: 770077_20261111_155626 HP lost by 600 s 0.000 of max HP (restores by then 0.000, never subtracted), dashes 0.00/min over the first ten minutes alive; ended at 65.0 s (bot_timeout): a lower bound
HP600: session NOT READ (2 sheet run(s), need 3)
BRACKET: NOT READ (no bot battery given: --bots)
D-045: 770077_20261111_155519 rusher hits, whole run: arrival charging 0 not charging 0; other charging 0 not charging 0; killed drop-in band 0; out of 0 landed Rusher drop-ins
D-045: 770077_20261111_155626 rusher hits, whole run: arrival charging 0 not charging 0; other charging 0 not charging 0; killed drop-in band 0; out of 0 landed Rusher drop-ins
D-045: session 0 hit(s) by an arrival's own attack (live charge + killed drop-in band) out of 0 landed Rusher drop-ins, over the 2 run(s) that carry the count
D-045: hit by an arrival's own attack: no validator
SHEET: D-045 revert clause (questions 8 and 14, the drop-in row): two validators call an arrival a hit they had no time to answer
D-046: runs alive at 600 s: 0 of 2 (hypothesis: at least half of the six)
D-046: 770077_20261111_155519 restores none
D-046: 770077_20261111_155626 restores none
D-046: T1 second run 770077_20261111_155626: boss not reached (bot_timeout at 1:05 (65.0 s))
D-046: 0 validator(s) reached the boss above half health on their second run
SHEET: D-046 revert clause (question 7): at least two of three validators reach the boss above half HP on their second run and name nothing that threatened them
D-036: frame at 16:9: down 1504.25, up 2041.16, side 2666.67 + 0.23315 x dx uu (C.ARENA.CAMERA_PITCH_DEG, C.ARENA.CAMERA_FOV_DEG, C.ARENA.CAMERA_FRAME_RADIUS_UU; up-screen is world +X)
2026-11-12: D-080 raised the frame after this dry run; the tool now prints down 1402.37, up 1571.52, side 2269.58 + 0.23315 x dx.
D-036: 0 of 0 hits with a source off a 16:9 frame
D-036: the tester's screen shape is not in the files, so the sides are exact for 16:9 only (a wider screen shows more at the sides, never less height); a body shows up to its radius past an edge; a Bullet's source is where it was fired
D-036: 0 of 2 run(s) reached the boss (question 12 is asked of those)
SHEET: D-036 revert clause (question 9, the unseen-hit row): one report reverts the frame
SHEET: D-036 held items (questions 10, 11, 12)
D-053: 770077_20261111_155519 Key not picked
D-053: 770077_20261111_155626 Key not picked
D-053: no validator held the Key with the blade or the drones, so the clause is unread
SHEET: D-053 revert clause (question 6): two of the validators who held the Key with the drones or the blade say the card seemed broken there
D-054: 770077_20261111_155519 share 0.0000 (0 dropped of 3900 steps)
D-054: 770077_20261111_155626 share 0.0000 (0 dropped of 3900 steps)
D-054: 2 run(s) read, 0 not read; no threshold exists; the S1 gate decision sets one
SHEET: D-054 revert clause (question 13, the slow-motion row) against these shares
READS: with no verdict (D-048); L8 against C.LEVEL.L8_MIN 300 s, L16 against C.LEVEL.L16_MIN 630 s
READS: 770077_20261111_155519 swarm kill time 0.092 s over 2 cells, whole run (not alive through the window); pause 0.0 s per sim minute, 0.0 without the Patron hand; L8 not reached, L16 not reached; pickup ratio (XP of model at 300 s) NOT READ (ended at 65.0 s, before 300 s); attribution n/a (level 3, under L16)
READS: 770077_20261111_155519 Trait card (C.KEEPER.TRAIT_PREREQ_CARDS 2): none offered
READS: 770077_20261111_155626 swarm kill time 0.092 s over 2 cells, whole run (not alive through the window); pause 0.0 s per sim minute, 0.0 without the Patron hand; L8 not reached, L16 not reached; pickup ratio (XP of model at 300 s) NOT READ (ended at 65.0 s, before 300 s); attribution n/a (level 3, under L16)
READS: 770077_20261111_155626 Trait card (C.KEEPER.TRAIT_PREREQ_CARDS 2): none offered
ROW 7: NOT READ here (the probe on the session build: Tools/probe.py; the note that holds it is named in the session note)
TIMELINE: 770077_20261111_155519: the HUD's run timer (top right, Game/DTHUD.cpp) shows MM:SS of whole sim seconds since the run started, the step count over C.SIM.FIXED_HZ with the fraction dropped; it stops while a hand is open, as t does, and starts on the step the run file starts (Game/DTGameMode.cpp starts both together). HUD m:ss is t from m*60+ss to m*60+ss+1
TIMELINE: 770077_20261111_155519 0:00 0.0 Patron Keeper Storm
TIMELINE: 770077_20261111_155519 0:00 0.0 weapon Weapon.PulseRifle
TIMELINE: 770077_20261111_155519 0:00 0.0 Patron hand: Keeper.Storm
TIMELINE: 770077_20261111_155519 0:11 11.4 L2
TIMELINE: 770077_20261111_155519 0:11 11.4 Level hand: Blessing.Storm.Downpour.Key
TIMELINE: 770077_20261111_155519 0:32 32.6 L3
TIMELINE: 770077_20261111_155519 0:32 32.6 weapon Weapon.OrbitalDrones
TIMELINE: 770077_20261111_155519 0:32 32.6 Level hand: Weapon.OrbitalDrones
TIMELINE: 770077_20261111_155519 1:05 65.0 run_end bot_timeout
```

What a real 65-second bot run can show: that the copy step finds both runs in one zip and its manifest; the build, culture, UTC start and local start of each file; that `input_device` reads `kbm` (the default, so it proves no device); the end reason and time; the Patron, the Pulse Rifle given at the Patron pick, the first picks; the slow share; a minute table two minutes long; the frame numbers from the constants; and the HUD clock mapping. What it cannot show: anything in the band window (600 to 780 s), so no window value, band arithmetic, HP lost by 600 s that is not a lower bound, or bracket; no hit, so no off-frame test and no drop-in ratio (drop-ins start at 270 s); no boss, no Key, no L8 or L16, no pickup ratio (it ends before 300 s), no attribution. Those are read by the synthetic cases listed above.

## Unread at session A, whatever is played

- The tester's screen shape: the side edges of the off-frame test are exact for 16:9 only.
- Whether a validator is the owner, what they named their build, what they said: the sheet (`SHEET:` lines).
- A pad run's evidence for the gate's `pad` step: `Tools/PadCheck.ps1` reads only the development PC's run folder (`package.md` Open); `session_read.py` lists the input device of every returned run, and that list is the session's pad evidence.
- Whether a run with `bot_fast` 0 was a person's: only the hand-over and the sheet say so.
- A crash's pause, slow share and end: its file has no `run_end`.
- Row 7 (Keeper share and archetype band by loss): `Tools/probe.py` on the session build, never the session logs.
- The two bands themselves: the tool proposes and prints `BANDS: NOT SET`; the S1 gate decision writes them (D-046).
