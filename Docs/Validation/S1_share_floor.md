# S1 share cap floor — D-018 (2026-10-31)

Question: does a share cap that binds only above a floor of alive enemies give clearing builds the timeline's Turrets and lift spawned XP above 0.85 of the model, with T-DIR-01 in band and T-OFF-15 no worse than 200/201? Bot v1 at skill 1.0, seeds 1..15, 19 arms, `Tools/BotRuns.ps1 -Parallel 4`. Control `Saved/BotRuns/after_bot`; three arms × two seeds on the unchanged tree read identically, so later commits did not move the sim.

**Verdict: not met at either value; the rule is reverted.** 30 overshoots and costs boss reach. 15 is close on every clause and misses two: XP / model 0.8499 and T-OFF-15 194/201.

## 1. Could a floor work at all (checked before coding)
`S1_balance_pass2.md` §6 round 1 and §8 item 4 read the failed cap raise as "the same small field holds no-pick and weak builds at 300–500 s". The control does not show that. Alive enemies at minute samples, p10 / p50 / p90:

| Arms | 130–270 s | 280–540 s | 550–900 s |
|---|---|---|---|
| None | 97 / 141 / 166 | 179 / 219 / 300 | 297 / 300 / 373 |
| Storm and Forge, no Tide | 4–12 / 11–27 / 17–72 | 7–16 / 14–22 / 19–32 | 8–16 / 14–28 / 22–50 |
| Tide primary | 18–49 / 56–141 / 137–180 | 22–26 / 34–54 / 73–230 | 23–30 / 36–46 / 57–176 |

No-pick's field is large, not small. §4's "a build with a full field is not touched" is also wrong: Turrets stand still, so a build that does not reach them holds them at the cap, and raising the cap put them on the field no-pick was already drowning in. Alive count separates the two cases, so the floor was worth measuring. The Tide arms straddle any useful value.

## 2. Readings
The hypothesis was logged before each run (`number_change_log.csv`). "No-Tide strong" is the nine Storm and Forge arms without Tide.

| Clause | Control | Floor 30 | Floor 15 | Needed |
|---|---|---|---|---|
| Turrets spawned 400–780 s, arm mean | 35.8 | 120.4 | 72.3 | ≥ 72 |
| Spawned XP / model at 630 s, no-Tide strong | 0.789 | 0.926 | 0.850 (0.8499) | > 0.85 |
| T-DIR-01 median | 588.6 s PASS | 649.3 s PASS | 642.0 s PASS | 480–720 s |
| T-OFF-15 | 200/201 | 95/142 FAIL | 194/201 FAIL | ≥ 200/201 |
| Mean decisions at the boss | 28.97 | 30.65 | 29.68 | 28 ± 3 |
| Steered runs at the boss | 201 | 142 | 201 | not a clause |
| Wins | 153 | 109 | 171 | not a clause |
| `Tools/pressure.py --check` | 0/19 arms in both bands | 0/19 | 1/19 (None) | not a clause |

T-OFF-15 at floor 15 misses on six runs at 32 decisions, all Storm arms (Downpour, Downpour:Forge, ArcGrid:Forge), plus Tide/Stillness:Storm seed 6 at 24, the slow-build miss the control also has. Spawned XP at floor 15 on the runs alive at 630 s reads 0.8505, so the 0.85 line is a coin toss. T-OFF-15 is not: XP that lifts the mean to 0.85 also lifts the fastest arms past 31 decisions.

## 3. Per arm
Each cell is control / floor 30 / floor 15. Turrets are the mean spawned in 400–780 s over runs alive at 780 s. Decisions are the maximum at the boss.

| Arm | Boss of 15 | Wins | Median end s | Dmg share/min | Turrets | XP / model | Max decisions |
|---|---|---|---|---|---|---|---|
| None | 0 / 1 / 0 | 0 / 0 / 0 | 589 / 649 / 642 | 0.25 / 0.60 / 0.32 | 42 / 36 / 42 | 0.53 / 0.52 / 0.52 | — |
| Forge/Hammer | 14 / 12 / 15 | 13 / 10 / 15 | 976 / 952 / 974 | 0.02 / 0.04 / 0.03 | 26 / 77 / 77 | 0.79 / 0.91 / 0.84 | 30 / 32 / 31 |
| Forge/Furnace | 13 / 4 / 15 | 9 / 4 / 11 | 986 / 685 / 976 | 0.06 / 0.41 / 0.04 | 24 / 82 / 42 | 0.79 / 0.90 / 0.81 | 30 / 33 / 31 |
| Forge/Slam | 14 / 6 / 15 | 13 / 5 / 15 | 992 / 830 / 984 | 0.05 / 0.08 / 0.02 | 25 / 87 / 44 | 0.78 / 0.87 / 0.81 | 30 / 33 / 31 |
| Storm/Tempest | 13 / 9 / 11 | 10 / 7 / 8 | 984 / 961 / 982 | 0.03 / 0.20 / 0.23 | 32 / 105 / 44 | 0.78 / 0.88 / 0.80 | 30 / 33 / 31 |
| Storm/ArcGrid | 13 / 8 / 11 | 10 / 6 / 10 | 974 / 912 / 975 | 0.03 / 0.15 / 0.07 | 35 / 135 / 52 | 0.79 / 0.96 / 0.86 | 30 / 33 / 31 |
| Storm/Downpour | 15 / 11 / 15 | 14 / 10 / 15 | 994 / 968 / 984 | 0.01 / 0.06 / 0.03 | 55 / 182 / 126 | 0.79 / 0.98 / 0.91 | 30 / 33 / 32 |
| Storm/Tempest:Forge | 15 / 5 / 15 | 12 / 3 / 14 | 988 / 568 / 985 | 0.02 / 0.06 / 0.01 | 33 / 112 / 45 | 0.79 / 0.89 / 0.82 | 30 / 32 / 31 |
| Storm/ArcGrid:Forge | 15 / 11 / 14 | 12 / 9 / 12 | 987 / 949 / 975 | 0.04 / 0.09 / 0.04 | 38 / 117 / 85 | 0.80 / 0.95 / 0.88 | 30 / 33 / 32 |
| Storm/Downpour:Forge | 15 / 13 / 15 | 15 / 13 / 15 | 972 / 957 / 985 | 0.01 / 0.05 / 0.02 | 55 / 187 / 138 | 0.79 / 1.00 / 0.92 | 30 / 33 / 32 |
| Forge/Hammer:Tide | 13 / 7 / 14 | 5 / 3 / 11 | 968 / 884 / 990 | 0.05 / 0.15 / 0.06 | 17 / 38 / 20 | 0.78 / 0.88 / 0.81 | 30 / 31 / 30 |
| Forge/Furnace:Tide | 8 / 6 / 6 | 3 / 3 / 3 | 920 / 600 / 834 | 0.13 / 0.08 / 0.55 | 20 / 48 / 18 | 0.77 / 0.86 / 0.79 | 29 / 31 / 30 |
| Forge/Slam:Tide | 10 / 5 / 8 | 6 / 3 / 4 | 945 / 606 / 941 | 0.09 / 0.89 / 0.13 | 18 / 24 / 16 | 0.77 / 0.82 / 0.79 | 30 / 30 / 30 |
| Tide/Frost | 11 / 11 / 11 | 9 / 7 / 10 | 956 / 966 / 987 | 0.11 / 0.04 / 0.02 | 17 / 31 / 19 | 0.72 / 0.73 / 0.72 | 30 / 29 / 29 |
| Tide/Frost:Storm | 11 / 11 / 10 | 10 / 9 / 9 | 954 / 961 / 958 | 0.03 / 0.08 / 0.05 | 23 / 68 / 32 | 0.72 / 0.76 / 0.72 | 29 / 31 / 30 |
| Tide/Stillness | 7 / 7 / 8 | 4 / 6 / 5 | 875 / 875 / 922 | 0.03 / 0.07 / 0.12 | 27 / 36 / 25 | 0.68 / 0.69 / 0.68 | 29 / 30 / 29 |
| Tide/Stillness:Storm | 6 / 9 / 7 | 6 / 7 / 6 | 564 / 940 / 871 | 0.02 / 0.04 / 0.03 | 25 / 82 / 31 | 0.66 / 0.71 / 0.68 | 30 / 31 / 30 |
| Tide/Whirlpool | 4 / 2 / 6 | 0 / 1 / 5 | 580 / 647 / 834 | 0.06 / 0.12 / 0.04 | 22 / 34 / 32 | 0.72 / 0.73 / 0.74 | 30 / 30 / 30 |
| Tide/Whirlpool:Storm | 4 / 5 / 5 | 2 / 3 / 3 | 556 / 561 / 529 | 0.04 / 0.08 / 0.18 | 29 / 99 / 30 | 0.70 / 0.73 / 0.71 | 29 / 32 / 30 |

Window damage on arms with few survivors swings on a handful of runs (Slam:Tide 0.89, Furnace:Tide 0.55); read boss reach there instead.

## 4. What this shows
1. **At 30 the mechanism works and the game breaks.** Turrets and XP overshoot, and boss reach falls from 201 to 142: Forge/Furnace goes from 13 to 4 of 15, with deaths from 405 s on. I did not trace those deaths by source. The floor gives a strong build the whole timeline, and the timeline was never survived in full.
2. **At 15 strong builds get stronger.** Wins rise from 153 to 171 and boss reach holds at 201 in total, though it moves by arm (Tempest and ArcGrid 13 → 11, Furnace:Tide 8 → 6, Slam:Tide 10 → 8). At skill 1.0 the added XP seems to outweigh the added Turrets. Rounds damage on the no-Tide strong arms barely moves (about 0.03 → 0.04/min), so the floor does not supply the pressure pass 2 wanted. It buys income. At skill 0.5 this could read differently; not measured.
3. **XP and T-OFF-15 pull against each other.** Spawned XP above 0.85 and T-OFF-15 at 200/201 share almost no room: the curve (R-PRG-03) is calibrated so 28 ± 3 decisions fit the current income. More income needs a curve or budget change, which this brief did not allow.
4. **No-pick moved 53 s for reasons this reading cannot separate.** Its field passes both floors by about 70 s, so the shift is most likely a small early change spreading through a chaotic run. T-DIR-01 stayed in band, but a median that moves 53 s on so small a cause is a thin base for tuning against it.

## 5. Left as it was
Rule, constant, test and spec are back to the state before D-018. No other number moved. The XP gap stands at 0.79. Open questions go to `director.md` Open.
