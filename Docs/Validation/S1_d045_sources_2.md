# S1 — D-045 sources, second battery: the marks split the Rusher hits (2026-11-11)

**Reading: on `main` at `7f75ada`, with the `arrival` and `charging` marks, steered bots at skill 1.0 take 216 Rusher hits in 270 runs over the whole run. Of these, 35 are drop-ins hitting with their charge (in 28 runs) and 13 are bands of drop-ins killed in their wind-up. 24 are drop-ins touching the hero after their charge. 87 are charges of Rushers that walked into range, and 57 are those Rushers' contact outside a charge. So 48 hits, 10.75 of 47.10 max HP of Rusher damage, come from an arrival's attack. The number of drop-ins that were Rushers cannot be counted from the run files (§4), so "out of how many arrivals" is not answered. Run for run the sim equals the first battery (`S1_d045_sources.md`): same outcome, end time and `input_hash` in 285 of 285 runs. At skill 0.5 the same split on four arms gives more arrival hits per run on Storm/Tempest and about the same elsewhere. A bot is not a person at either skill.**

A correction to the first note, which stays as written. In the log a live charge that touches the hero is a `Contact` hit (R-ENM-14). `Charge` is only the band of a drop-in killed in its wind-up. The first note's "13 charge hits" are those 13 bands, and its "Rusher contact" includes live charges.

## 1. Build and runs
- Branch `s1/d045-sources-2` from `7f75ada` (`main`). `Tools\Build.bat` (DusktracerEditor Development): `Result: Succeeded`, 0 warnings. Runs read `run_start.build` `7f75ada`, a clean tree, and `bot_fast` 600.
- The battery is the first note's: the no-pick arm and the eighteen steered arms of `S1_keeper_power.md` §5. Fifteen seeds from seed 1, skill 1.0, policy All, 600 steps a frame, `-MaxSeconds` 1100.
- The skill 0.5 battery takes the no-pick arm and one archetype per Keeper: Forge/Hammer, Storm/Tempest and Tide/Frost. Fifteen seeds from seed 1, the rest the same.
- The two batteries ran one after the other into folders of their own. The Tools applied the processor mask (D-052). A carry agent was building and gating in another worktree at the same time; the sim is seeded and fixed-step, so that changes no result.

```
Tools\Build.bat
& Tools\BotRuns.ps1 -Arms None,Steered:Forge:Furnace,Steered:Forge:Furnace:Tide,Steered:Forge:Hammer,Steered:Forge:Hammer:Tide,Steered:Forge:Slam,Steered:Forge:Slam:Tide,Steered:Storm:ArcGrid,Steered:Storm:ArcGrid:Forge,Steered:Storm:Downpour,Steered:Storm:Downpour:Forge,Steered:Storm:Tempest,Steered:Storm:Tempest:Forge,Steered:Tide:Frost,Steered:Tide:Frost:Storm,Steered:Tide:Stillness,Steered:Tide:Stillness:Storm,Steered:Tide:Whirlpool,Steered:Tide:Whirlpool:Storm -Seeds 15 -Parallel 14 -Out Saved\BotRuns\d045_sources_2
& Tools\BotRuns.ps1 -Arms None,Steered:Forge:Hammer,Steered:Storm:Tempest,Steered:Tide:Frost -Seeds 15 -Skill 0.5 -Parallel 14 -Out Saved\BotRuns\d045_sources_2_s05
python -I Tools\pressure.py Saved\BotRuns\d045_sources_2 --check
python -I Tools\pressure.py Saved\BotRuns\d045_sources_2_s05 --check
```

| Battery | Asked | Ran | Failed | Rerun | Outcomes |
|---|---|---|---|---|---|
| Skill 1.0 | 285 | 285 | 0 | 0 | 192 victories, 91 deaths, 2 at the 1100 s cap; steered at the boss 231, wins 192 |
| Skill 0.5 | 60 | 60 | 0 | 0 | 20 victories, 40 deaths |

- No run was killed on the timeout. Both batteries printed `BOTRUNS: PASS`.
- No `LogDT` error or warning, no fatal error and no access violation. Every log ends with `LogExit: Exiting`. Every run wrote its bot record and `run_end`.
- Every `hero_damaged` record carries both marks: 0 unmarked records in either battery. One `arrival` hit is a Turret drop-in's contact, not a Rusher's, and is left out of the tables.
- `pressure.py --check` prints `T-DIR-02: FAIL (0/19 group(s) pass ...)` and `T-DIR-18: FAIL (0/19 ...)` at skill 1.0, and `T-DIR-02: FAIL (0/4 ...)` and `T-DIR-18: FAIL (0/4 ...)` at skill 0.5, with None not read. These are printed here and not judged by this note.

## 2. The same sim as the first battery
Bot records (`bot.csv.txt`) compared run for run with `Saved\BotRuns\d045_sources`:
- Outcome, `end_s`, level at 630 s, dash and Intercession presses, picks, declines, `input_hash` and config are equal in 285 of 285 runs.
- `steps` differs in 12 runs, by 1 or 2.

The JSONL agrees with that. Leaving out the marks, `bot_fast`, the build stamp and the wall fields, every record up to `hero_down` or `boss_end` is equal in all 285 runs. In those 12 runs the last few records (the closing `dir_block`, `sources`, `status`, `weapon` and `run_end`) sit 1 or 2 steps later or earlier. So the session closes a step or two after the run is decided, at a step that is not fixed at `-DTBotFast` 600. This is a finding about the close, not about the sim; no reading here uses those records. Sample hashes on `7f75ada`, equal to the first battery:

| Run | `input_hash` | Outcome | End s |
|---|---|---|---|
| None seed 1 | `a8e97fe3` | death | 691.7000 |
| None seed 4 | `a5c2f615` | at the 1100 s cap | 1100.0000 |
| Storm/Tempest seed 7 | `d9769b10` | victory | 1035.9833 |
| Tide/Whirlpool:Storm seed 3 | `c546d931` | at the 1100 s cap | 1100.0000 |

## 3. Rusher hits, five ways (skill 1.0)
**How the tables are made.** `pressure.py` prints the split per bot arm and overall, over the whole run. Its overall line, None included:

`SOURCES: overall rusher hits, whole run: arrival charging 36 not charging 29; other charging 89 not charging 62; killed drop-in band 13`

A scratch script (`split5.py`, in the session scratchpad) counts the same split from the JSONL by the same rule, per arm and also inside the band window, 600–780 s. Its counts equal `pressure.py`'s on every arm.

Each cell gives hits, then the HP they took as a share of max HP. The columns:
- **Arr. charge:** `arrival` and `charging` both true.
- **Arr. other:** `arrival` true, `charging` false.
- **Walked charge:** `arrival` false, `charging` true.
- **Walked other:** both false.
- **Band:** `hit_kind` Charge, the band of a drop-in killed in its wind-up.

| Arm | Arr. charge | Arr. other | Walked charge | Walked other | Band | Runs with an arrival charge hit |
|---|---|---|---|---|---|---|
| None | 1 (0.21) | 5 (1.15) | 2 (0.35) | 5 (0.99) | 0 | 1 |
| Forge/Furnace | 2 (0.47) | 0 | 7 (1.53) | 0 | 0 | 2 |
| Forge/Furnace:Tide | 0 | 2 (0.45) | 5 (1.08) | 3 (0.61) | 2 (0.48) | 0 |
| Forge/Hammer | 1 (0.21) | 0 | 2 (0.44) | 1 (0.19) | 0 | 1 |
| Forge/Hammer:Tide | 3 (0.71) | 0 | 6 (1.39) | 0 | 2 (0.39) | 2 |
| Forge/Slam | 3 (0.60) | 3 (0.73) | 4 (0.78) | 2 (0.38) | 2 (0.53) | 2 |
| Forge/Slam:Tide | 5 (1.10) | 1 (0.25) | 4 (0.85) | 6 (1.41) | 0 | 3 |
| Storm/ArcGrid | 1 (0.21) | 2 (0.52) | 4 (0.87) | 1 (0.27) | 0 | 1 |
| Storm/ArcGrid:Forge | 0 | 0 | 1 (0.25) | 0 | 3 (0.74) | 0 |
| Storm/Downpour | 0 | 0 | 3 (0.65) | 0 | 1 (0.20) | 0 |
| Storm/Downpour:Forge | 0 | 0 | 2 (0.45) | 0 | 0 | 0 |
| Storm/Tempest | 4 (0.98) | 1 (0.19) | 6 (1.40) | 9 (2.08) | 0 | 3 |
| Storm/Tempest:Forge | 0 | 0 | 1 (0.25) | 3 (0.62) | 0 | 0 |
| Tide/Frost | 2 (0.45) | 1 (0.25) | 2 (0.45) | 8 (1.70) | 1 (0.26) | 2 |
| Tide/Frost:Storm | 3 (0.64) | 5 (1.09) | 3 (0.67) | 3 (0.59) | 1 (0.23) | 2 |
| Tide/Stillness | 4 (0.86) | 3 (0.68) | 5 (1.13) | 3 (0.64) | 0 | 3 |
| Tide/Stillness:Storm | 2 (0.43) | 3 (0.69) | 6 (1.30) | 6 (1.18) | 1 (0.21) | 2 |
| Tide/Whirlpool | 2 (0.39) | 1 (0.21) | 14 (2.93) | 5 (0.98) | 0 | 2 |
| Tide/Whirlpool:Storm | 3 (0.66) | 2 (0.38) | 12 (2.53) | 7 (1.32) | 0 | 3 |
| **Steered, 270 runs** | **35 (7.71)** | **24 (5.44)** | **87 (18.95)** | **57 (11.96)** | **13 (3.04)** | **28** |

**Band window, 600–780 s, steered:** arrival charge 6 (1.48), arrival other 7 (1.75), walked charge 20 (4.89), walked other 9 (2.13), band 2 (0.50). Per arm, the window holds 0 to 7 Rusher hits; the per-arm window table is in `Saved\Logs\split5_d045_sources_2.txt`.

**Beside the first battery** (steered, whole run): 216 Rusher hits for 47.10 max HP in both; 129 runs with a Rusher hit in both. The first note's 13 "Charge" hits are the 13 bands here. Its 203 "Contact" hits are 35 + 24 + 87 + 57 = 203: 122 of them under a charge and 81 not. In the window the first note's 44 Rusher hits are 6 + 7 + 20 + 9 + 2 = 44.

## 4. How many arrivals: the denominator
It cannot be counted from the run files:
- `dir_block.dropins` counts the drop-in markers published in a block, every role, including markers cancelled before they land.
- `dir_block.spawned_by_role` mixes drop-ins with the timeline's own spawns.
- No record names the role of a drop-in, or says that one landed.
- The steered runs published 19924 drop-ins of every role, 4036 of them in blocks that lie inside the window. The catalogue weight of a Rusher drop-in is 0.70 on every row (`timeline_arena1.csv`), so about 13 900 of the 19924 would be Rushers. That is an expectation from the weight, not a count.

**The field that would answer it:** a per-role count of landed drop-ins on `dir_block` (for example `dropins_by_role`, counted where `StepDropIns` lands a marker, `DTDirectorCore.cpp`), or one record per landing. With it, "how many arrivals' charges hit, out of how many arrivals" is arrival charge plus band hits over landed Rusher drop-ins. With what exists, the numerator is 48 hits (35 + 13) in 270 steered runs, and the denominator is not read.

## 5. Skill 0.5 beside skill 1.0 (the same four arms, the same seeds)

| Arm | Skill | Arr. charge | Arr. other | Walked charge | Walked other | Band | Runs with an arrival charge hit | Drop-ins published | At the boss | Wins |
|---|---|---|---|---|---|---|---|---|---|---|
| None | 1.0 | 1 | 5 | 2 | 5 | 0 | 1 | 698 | 1 | 0 |
| None | 0.5 | 2 | 7 | 1 | 4 | 0 | 2 | 404 | 0 | 0 |
| Forge/Hammer | 1.0 | 1 | 0 | 2 | 1 | 0 | 1 | 1167 | 15 | 15 |
| Forge/Hammer | 0.5 | 2 | 0 | 2 | 1 | 0 | 2 | 1192 | 15 | 12 |
| Storm/Tempest | 1.0 | 4 | 1 | 6 | 9 | 0 | 3 | 1164 | 13 | 12 |
| Storm/Tempest | 0.5 | 8 | 6 | 11 | 11 | 0 | 6 | 873 | 8 | 3 |
| Tide/Frost | 1.0 | 2 | 1 | 2 | 8 | 1 | 2 | 1059 | 12 | 8 |
| Tide/Frost | 0.5 | 1 | 0 | 2 | 9 | 1 | 1 | 870 | 9 | 5 |

- **Steered, 45 runs at skill 0.5:** arrival charge 11 (2.53 max HP), arrival other 6 (1.36), walked charge 15 (3.31), walked other 21 (4.19), band 1 (0.26); 9 runs with an arrival charge hit.
- **The same three arms at skill 1.0:** 7, 2, 10, 18, 1; 6 runs with an arrival charge hit.
- At skill 0.5 the runs end sooner (fewer drop-ins published on Tempest, Frost and None), so the counts are over less run time.
- Skill is a bot parameter: neither 1.0 nor 0.5 is a person (`10_Tuning.md` §4), and which skill row brackets people is the S1 gate decision's to name (D-050).

## 6. What D-045 predicted, and what the marks show
The hypothesis: charging through the hero's position "moves the share of Rusher arrivals whose band covers that position from about a quarter … to all of them, and Rusher-source damage in steered runs from none to some … measured by the new assertions and by `Tools/pressure.py` SOURCES on the nineteen arms before and after".

- **Bands that cover the hero's position:** the assertions read 0 of 48 before and 48 of 48 after (D-045 Result). A battery reads hits, not coverage. The bot steers out of telegraphs, so a band that covers the hero at wind-up start is one the bot is built to leave.
- **Rusher-source damage from arrivals, "from none to some":** at `7f75ada`, some.
  - 48 hits come from an arrival's attack (35 live charges, 13 bands), 10.75 max HP over 270 steered runs. That is 0.228 of all Rusher damage and 0.045 of all steered damage (237.33 max HP, first note).
  - 28 of 270 runs take an arrival's charge.
  - The largest Rusher source is the charge of a Rusher that walked into range: 87 hits, 18.95 max HP.
- **"Before":** no battery before the marks can split arrivals from the rest. `S1_d045_sources.md` §3 read Rusher damage at `d82ace8` without the marks, so "none" before cannot be checked for arrivals, and this battery has no before.

## 7. What a battery still cannot say
- The arrivals' denominator (§4).
- What a person meets: no bot skill is a person, and the owner's one run took no Rusher hit (`S1_owner_run.md` §3).
- Whether an arrival's hit was one a person had time to answer: that is D-045's revert clause, at session A with two validators.
- How a drop-in's later contact is marked: `arrival` stays true for a drop-in's whole life (`S1_telemetry_marks.md`), so "Arr. other" mixes a drop-in's touch after its charge with any touch before it.
- Whether any number should move. Boss reach and wins are printed and not judged (D-050 item 5).
- Co-op, the Shipping build, other maps: every run is one player, the Development editor build, `L_Arena_Test`.

## Where the runs are
In `E:\Dusktracer\.claude\worktrees\agent-a559188263b0e1f6c`:
- The batteries: `Saved\BotRuns\d045_sources_2` (285 runs) and `Saved\BotRuns\d045_sources_2_s05` (60 runs).
- The `pressure.py` output: `Saved\Logs\pressure_d045_sources_2.log` and `pressure_d045_sources_2_s05.log`.
- Logs: `Saved\Logs\botruns_d045_sources_2.log`, `botruns_d045_sources_2_s05.log` and `build_d045_sources_2.log`.
- The first battery: `Saved\BotRuns\d045_sources`.
