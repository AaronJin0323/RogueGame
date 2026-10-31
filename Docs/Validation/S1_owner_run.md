# S1 owner run (2026-11-01)

**This is one designer run: n = 1. It is not O-008 and counts toward no S1 exit clause.** The exit test needs three validators who are not the owner (`S1_kit.md`). Everything marked as a bot reading below is a bound, never a human reading. No balance number was tuned and no band was redefined.

Class: SYSTEM, tools only, plus this reading. Layer: the measuring instrument (Bot v1, `Tools/BotRuns.ps1`, `Tools/owner_read.py`). Replaces: nothing; the bot filter is off by default and adds no game system.

Hypothesis: we believe a bot that passes weapon cards moves the share of Storm+Forge runs with no Weapon-source damage after the Hammer Key from 0 of 37 to most runs, because the owner reached that state by passing every weapon card; measured in §7; reverted if not met. **Met: 27 of 27 at skill 1.0.**

## 1. The run
`2147483647_20261031_231306.jsonl` and `.csv`, build 996cd47, zh-Hans, keyboard and mouse. Patron Storm, second Keeper Forge. Killed at 563.3 s on level 15. Every number in §2 and §3 comes from those two files through `Tools/owner_read.py run`; any number that tool could not reproduce was left out.

## 2. Verified timeline

| Time | Event | HP after |
|---|---|---|
| 0:00.00 | Patron Storm | 100.0 |
| 0:15.77 | L2 `ArcGrid.Key` | |
| 0:55.10 | L3 `ArcGrid.Amp1` | |
| 1:02.00 | Swarm contact, 6.4 | 93.6 |
| 1:38.18 | L4 `ArcGrid.Trait` | |
| 2:20.00 | L5 `Mod.ChainTrigger` on the rifle | |
| 2:56.25 | L6 `Downpour.Key` | |
| 3:34.45 | L7 `ArcGrid.Amp2` | |
| 3:38.75 | Tank contact, 14.8 | 78.9 |
| 3:59.88 | Chest: `Mod.ScatterShell` on the rifle | |
| 4:00.88 | L8: second Keeper Forge | |
| 4:35.63 | L9 `Downpour.Amp2` | |
| 5:29.22 | L10 `Downpour.Trait` | |
| 5:44.17 | Last `Weapon.PulseRifle` hit of the run | |
| 5:44.78 | Chest: `Downpour.Amp1` | |
| 5:45.65 | L11 **`Forge.Hammer.Key`** | |
| 6:10.32 | Turret bullet, 7.0 | 71.8 |
| 6:51.95 | L12 `Hammer.Amp2` | |
| 7:01.60 | Turret bullet, 7.3 | 64.5 |
| 7:13.07 | Turret bullet, 7.4 | 57.1 |
| 7:39.43 | L13 `Hammer.Trait` | |
| 7:47.78 | Turret bullet, 7.5 | 49.6 |
| 7:54.18 | Chest: `Tempest.Key` | |
| 7:55.40 | L14 `Hammer.Amp1` | |
| 8:25.40 | Turret bullet, 7.8 | 41.8 |
| 8:41.02 | Tank contact, 18.9 | 22.9 |
| 8:49.33 | L15 `Tempest.Amp1` | |
| 8:54.90 | Tank contact, 19.0 | 3.9 |
| 9:23.30 | Turret bullet, 8.1: down | |

Six Intercessions: four on Arc Grid (77.5 to 316.2 s, 26 to 35 enemies, 1260 to 1722 damage), two on Downpour with the Hammer layer (429.3 and 550.2 s, 0 enemies, 0 damage). That zero is not a failed cast: the `intercession` record counts only the cast's own hits, and 28 of 28 unlayered Downpour casts in the bots read 0 too. The two Hammer-layered bot casts read about 4 hits, so his layer touched nothing. Six presses were denied, three each in intervals 2 and 3. Seventeen dashes.

## 3. Verified readings

| Reading | Value | Against |
|---|---|---|
| L8 | 240.9 s | |
| Decisions | 17 (13 Level, 3 Chest, 1 Keeper; the Patron hand is not one) | `C.DECISIONS.PER_RUN` is a run to the boss: not read here |
| Cards passed | all 17 Weapon, all 6 WeaponUpgrade | |
| Cards taken | 14 Blessings, 2 Mods, 2 Keepers; the rifle stayed level 1 | |
| HP lost | 104.2 % of max HP in 10 hits | The sum counts the last hit whole: 4.2 of it is overkill. HP never rose |
| Killed | 563.3 s, Turret bullet for 8.1, on 3.9 HP | |
| By role | Tanks 52.7 HP in 3 hits, Turrets 45.2 in 6, Swarm 6.4 in 1 | |
| HP lost per minute | 3.67 % before the Hammer Key, 22.9 % after | **Cause not separable**: Turrets enter at 270 s, and the run has no arm without Hammer |
| Paused | 20.6 s per sim minute (193.7 s, the sum of all 18 hands) | `C.OFFER.PAUSED_S_PER_MIN_MAX` 15. Still 17.9 without the 25.5 s Patron hand. R-OFF-15 states this limit for a bot run, and bots read 0 |
| XP of model at 300 s | 0.733 | `C.XP.REFERENCE_PICKUP_RATIO` 0.51; the 45 control bots read 0.756, range 0.687 to 0.790 |
| Dashes | 17, 1.81 per minute | The band is minutes 10 to 13, which he never reached |
| Keeper share, whole run | 0.818 | Attribution, and not the L16 reading |
| **Unread** | T-DIR-02, Keeper share at L16, boss, Reforging | Alive 0 s in 600 to 780 s; top level 15; 900 s not reached; 0 Reforging hands |

## 4. What it can and cannot show
It can show that a person can play this path, what one run looked like, and that the bots had never tested it.

It cannot show a rate, a median, a band or a verdict. One run cannot say what is typical, whether the bands hold, whether anything is balanced, or why he lost HP. The files hold no words from him, so they cannot say why he passed the weapons, whether he knew what Hammer does to the rifle, or how the run felt. He designed the game, so even this path is not what a new player would do.

## 5. What stopped at the Key
The brief's "zero rifle output" is true of Weapon-source damage and not of the rifle. **The rifle kept swinging.** Hammer reshaped it, and the arcs are credited to `Keeper.Forge`:

1. The Pulse Rifle fires 2 rounds at levels 1 and 2 and more from level 3 (`weapons.csv`). Hammer's Key converts 2 rounds. On his rifle that is every round, so nothing flies (R-WPN-10), and the Projectile cap goes with it.
2. A Keeper-caused cleave carries `Keeper.<Path>` (R-WPN-07, D-015), so `Weapon.*` reads zero. `Keeper.Forge` goes from about 1 damage per hit before the Key to 17.4 per hit in interval 6.
3. `Modifier.*` stops too, from interval 6 on, though neither mod goes dormant: the caps left are Melee, Aimed and Expiry, and both still fit. Scatter Shell acts when a round expires, and no round flies. Chain Trigger's free volleys are reshaped the same way, so their hits read `Keeper.Forge`.
4. Reach falls from 900 uu to an arc of 380, then 494 after `Hammer.Amp2`.

He was not disarmed: across intervals 5 to 8 kills per minute read 241, 224, 219, 280 and damage dealt per minute 7395, 6577, 7100, 10400. His two Tempest cards were not dead either: on a sweep `Flag.Split` falls back to rings of children, which fits the 4 live bullets at 540 s, though telemetry does not name those rounds.

This is the rules as written, so it is no BUG. It is also why no bot had reached it: two of the six upgrades he passed would have left one round flying, and any other weapon keeps a Weapon source. Bots take those cards.

## 6. The tool
`& Tools\BotRuns.ps1 -Policy BlessingsOnly` filters Steered arms: Weapon and WeaponUpgrade cards score under everything else. An arm may name the second Keeper's archetype as a fifth field, `Steered:Storm:ArcGrid:Forge:Hammer`, and that archetype's blessings come first, because he took Hammer's over Storm's. The switches are `-DTBotPick` and `-DTBotSecondArchetype`. `BotRuns` refuses `-Policy` on `None`, `Random` and `-Jobs`, where it would change nothing.

**Policy off keeps `input_hash`.** Seed 9107, base binaries against this branch's, policy off:

| Arm | Skill | Base | This branch | Outcome, end s |
|---|---|---|---|---|
| None | 1.0 | 1c738e97 | 1c738e97 | death, 675.45 |
| Random | 1.0 | 2282c72a | 2282c72a | victory, 987.27 |
| Steered:Forge:Hammer | 1.0 | 9385c307 | 9385c307 | victory, 981.38 |
| Steered:Storm:ArcGrid:Forge | 1.0 | 4957cab3 | 4957cab3 | victory, 966.07 |
| Random | 0.5 | 428fcc8d | 428fcc8d | death, 625.47 |
| Steered:Storm:ArcGrid:Forge | 0.5 | 6deac85a | 6deac85a | death, 958.13 |

Steps, picks, dashes and the CSV's numbers match on all six. Every JSONL line matches except `run_start.build` and the `wall*` fields. Policy on, the same seed reads 8ea64f20 twice: deterministic, and visibly a different run.

## 7. Bounds
Seeds 1..15, 15 per row, headless, sim cap 1100 s. The control is the commit he played, policy off, in the editor build; D-021 read editor and package equal on `input_hash`. These are bounds of Bot v1, **never human readings**. Boss = won, or alive at `C.RUN.LENGTH_TO_BOSS`. Deaths are by CSV interval, so 9 is 540 to 600 s, where he died.

| Arm | Skill | Boss | Wins | Deaths by interval | Median end s |
|---|---|---|---|---|---|
| Control ArcGrid:Forge | 1.0 | 14 | 13 | 12:1 16:1 | 975.3 |
| Control Downpour:Forge | 1.0 | 15 | 14 | 16:1 | 983.0 |
| Control Tempest:Forge | 1.0 | 15 | 15 | none | 983.5 |
| Policy ArcGrid + Hammer | 1.0 | 12 | 11 | 2:1 11:1 13:1 15:1 | 985.8 |
| Policy Downpour + Hammer | 1.0 | 12 | 11 | 6:1 11:1 13:1 16:1 | 983.8 |
| Policy ArcGrid + Hammer | 0.25 | 1 | 0 | 3:1 4:1 6:1 7:3 8:3 10:1 12:2 14:2 15:1 | 506.9 |
| Policy Downpour + Hammer | 0.25 | 1 | 0 | 3:3 4:1 7:1 8:1 9:4 10:1 11:2 12:1 15:1 | 558.0 |

| Arm | Skill | HP lost by 563.3 s, median (worst) | HP %/min to 345.65 s, then to 563.3 s | Dashes by 563.3 s | Dashes/min | Live bullets from interval 6 | Keeper share at L16 (n) |
|---|---|---|---|---|---|---|---|
| **Owner, n = 1** | | **104.2** | **3.67, 22.9** | **17** | **1.81** | **1.0** | **unread** |
| Control ArcGrid:Forge | 1.0 | 6.9 (34.9) | 0.00, 0.00 | 28 | 4.24 | 94.7 | 0.534 (15) |
| Control Downpour:Forge | 1.0 | 7.4 (67.9) | 0.00, 0.00 | 20 | 3.37 | 34.7 | 0.637 (15) |
| Control Tempest:Forge | 1.0 | 7.5 (38.1) | 0.00, 0.00 | 31 | 4.07 | 58.7 | 0.481 (15) |
| Policy ArcGrid + Hammer | 1.0 | 8.0 (108.4) | 0.00, 2.03 | 25 | 4.34 | 4.2 | 0.824 (14) |
| Policy Downpour + Hammer | 1.0 | 7.4 (100.0) | 0.00, 2.04 | 24 | 3.98 | 5.4 | 0.784 (14) |
| Policy ArcGrid + Hammer | 0.25 | 102.7 (108.9) | 6.84, 15.53 | 47 | 5.98 | 4.6 | 0.822 (6) |
| Policy Downpour + Hammer | 0.25 | 100.2 (108.4) | 9.52, 12.63 | 46 | 6.16 | 5.8 | 0.817 (8) |

Rate columns are per-run medians, and most bot runs take no hit in a window, so 0.00 is a median. Bullets are the hero's own, pooled over intervals; his are 0, 0, 4, 0.

**The control confirms the gap.** At his build the 45 runs read arm medians of 6.9 to 7.5 % HP lost and a worst of 67.9, with 20 to 31 dashes by 563 s. None had zero Weapon-source damage from interval 6 on. One run shows a single zero interval, its last, 0.07 s long. Thirty-seven of the 45 took the Hammer Key, so Hammer alone was never the difference.

**The policy reaches his state.** No Weapon-source damage after the Key: 0 of 37 in the control, 27 of 27 at skill 1.0, 19 of 21 at 0.25. Of the 112 policy runs that took the Key across the five skills, five are not zero: three died inside the Key's own interval, and in two a forced weapon pick near interval 13 brought Weapon damage back, as §5 predicts. Forced picks: one of 30 runs at 1.0, none at 0.25, from a chest of nothing else at about 800 s.

**Passing weapons alone does not make his pressure.** At skill 1.0 the HP lost by 563 s barely moves: 7.4 to 8.0 against 6.9 to 7.5. Two of 30 runs reach his total by then; the rest do not. The cost lands later: boss 44 of 45 to 24 of 30, wins 42 of 45 to 22 of 30. Only a much weaker pilot meets his number at the median.

**No skill dashes at his rate; the knob does not exist.** On `main` skill adds reaction delay and aim and movement noise, and nothing caps dashes. Lower skill does not lower the rate. Both arms, dashes per minute, with outcomes over 30 runs:

| Skill | Dashes/min | Boss of 30 | Wins | Median end s | HP lost by 563.3 s |
|---|---|---|---|---|---|
| 1.0 | 4.34, 3.98 | 24 | 22 | 985.8, 983.8 | 8.0, 7.4 |
| 0.75 | 6.39, 6.49 | 25 | 24 | 975.0, 979.0 | 27.3, 14.7 |
| 0.5 | 6.31, 5.92 | 17 | 12 | 968.7, 991.1 | 45.4, 33.8 |
| 0.25 | 5.98, 6.16 | 2 | 0 | 506.9, 558.0 | 102.7, 100.2 |
| 0.0 | 5.04, 5.14 | 0 | 0 | 325.9, 315.3 | 106.3, 103.9 |

Skill 0.25 was chosen after this table, as the row nearest his HP lost and end time. That makes it a bracket and not a calibration: it reaches his result by a route he did not take, at over three times his dash rate, and no row reads near his 3.67 then 22.9.

**Keeper share here is not a pass.** 0.78 to 0.82 clears `C.POWER.KEEPER_SHARE_L16`, but a fully converted rifle has no Weapon source left to count (`power_split.py` is attribution, not ablation).

**Hammer against the timeline stays open.** `Steered:Storm:*:Forge:Slam` was run as a control and failed as one: Forge cards still reach the bot, so 20 of 30 held Hammer by 563 s at skill 1.0 and 19 of 30 at 0.25. It is not reported. Tempest is cleaner by rule (R-KEE-16 keeps Hammer out while it leads): at skill 0.25, 14 of 15 runs held no Hammer by 563 s, and they still rose from 15.5 to 25.8 %/min, against 7.8 to 15.3 pooled for the Hammer arms. So in the bots the rise happens without Hammer. That is a weak reading: Tempest starts worse, 8 runs reach the later window, the same arm at skill 1.0 is not clean (6 of 15), and the bot kites alike with or without an arc. It does not tell what caused his.

## 8. Held items in `STATE.md`
**Informed**, one run, direction only, nothing released:
- Queue 1, the owner playtest of `main`: this is that run, but its feel check needs his words, and the files hold none.
- "The human upgrade rate is unread": first reading, 0 of 6 upgrades and 0 of 17 weapons. It is from `main`, where no evolution exists.
- The human pickup reading of `S1_bot_baseline.md`: 0.733 at 300 s, inside the bots' range and above 0.51. It reads 300 s and not that note's 630 s. Queue 3 hangs on the same income, and one reading moves neither the curve nor the hand budget.
- `C.OFFER.PAUSED_S_PER_MIN_MAX`: the first human number beside it. The constant stands.

**Not informed:**
- O-008 and every S1 exit clause.
- Queue 4, the heal source: its condition is a human reading of `C.DMG_TAKEN.BAND_MIN10_13` in 600 to 780 s, and he died before the window. The hold stays. So do T-DIR-02 and `C.DASHES.BAND_MIN10_13`.
- T-OFF-15 and the decision budget: he did not reach the boss.
- T-KEE-18, Keeper share at L16 and archetype bands: top level 15. T-KEE-15 is read by probe, and his cast records are attribution, not marginal damage.
- The boss, Reforging, R-RFG-18.
- Tide's early power and boss answer: he took no Tide card. The two Forge/Tide arms past noise: he played neither.
- Payday self-pay, Time Dilation scale, `leak_per_s`, duos: `dev/s2` only, and this was `main`.

## 9. Decisions made here
No D-number is assigned; integration does that.
1. The filter lives in the bot's card scoring, because `BotRuns.ps1` only launches the game. All, the default, passes nothing to the game.
2. A hand holding only weapon cards is answered, not declined: `Decline` exists only for the no-pick arm and a person has no pass (`offers.md`). `owner_read.py` counts the forced picks.
3. The second archetype ranks over the Patron's, as his picks did. A tool parameter (`DTBotTuning`), not a game number.
4. Two Patron leads were read, Arc Grid and Downpour, the two he led with. Tempest cannot test this path by rule.
5. Skill 0.25 is the lower reading; the full sweep stays in §7 so the choice can be checked.
6. No dash knob was built. The brief asked for one only if it existed. It would be a new bot parameter on `main`.
7. "Weapon-source damage" replaces "rifle output" throughout (§5). HP lost keeps the brief's definition, with its overkill named.

## 10. Reproduce
```
python -I Tools\owner_read.py run <Playtests>\2147483647_20261031_231306.jsonl
& Tools\BotRuns.ps1 -Arms Steered:Storm:ArcGrid:Forge,Steered:Storm:Downpour:Forge,Steered:Storm:Tempest:Forge -Seeds 15 -Parallel 8 -Out Saved\BotRuns\control_sf_s10
& Tools\BotRuns.ps1 -Arms Steered:Storm:ArcGrid:Forge:Hammer,Steered:Storm:Downpour:Forge:Hammer -Policy BlessingsOnly -Seeds 15 -Skill 0.25 -Parallel 8 -Out Saved\BotRuns\owner_s025
python -I Tools\owner_read.py bots Saved\BotRuns\control_sf_s10 Saved\BotRuns\owner_s025
```
