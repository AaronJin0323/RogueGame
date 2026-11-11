# S1 — D-045's unread half: damage by source on the nineteen arms (2026-11-11)

**Reading: on `main` at `3cfa101`, steered bots at skill 1.0 take 0.277 of their damage in the band window (600–780 s) from Rushers, and 0.198 over the whole run. Nearly all of it is Rusher contact: 203 hits over the whole run, against 13 hits from Rusher charges in 270 steered runs (2 of them in the window). The nearest earlier battery on disk, at `d82ace8` (before D-045), already read Rusher damage on every steered arm but three in the window. It read 0.308 in the window and 0.217 over the run, with 15 charge hits. So Rusher-source damage was not "none" before. The telemetry has no record that marks a hit as an arrival's, so how many arrivals charged and how many hit cannot be read (§4). These are bot readings at bot skill: a bound, not what a person meets. Boss reach and wins are printed and not judged (D-050 item 5).**

## 1. Build and runs
- Branch `s1/d045-sources` from `3cfa101` (`main`). `Tools\Build.bat` (DusktracerEditor Development): `Result: Succeeded`, 0 warnings, 4 compile and 2 link actions. The runs read `run_start.build` `3cfa101`, a clean tree.
- The arms and settings are `S1_keeper_power.md` §5's nineteen: the no-pick arm and eighteen steered arms. They are fifteen seeds from first seed 1, skill 1.0, policy All, 600 steps a frame, and `-MaxSeconds` 1100. That page's "Final" battery is the one read in §3.
- `Tools/BotRuns.ps1` and `Tools/pressure.py` apply this PC's processor mask themselves (D-052). The scratch scripts below ran from a shell that set it (`. Tools\Machine.ps1; $null = Set-DTAffinity`).
- A carry agent was building and gating in another worktree during the runs. The sim is seeded and fixed-step, so that changes no result.

```
Tools\Build.bat
& Tools\BotRuns.ps1 -Arms None,Steered:Forge:Furnace,Steered:Forge:Furnace:Tide,Steered:Forge:Hammer,Steered:Forge:Hammer:Tide,Steered:Forge:Slam,Steered:Forge:Slam:Tide,Steered:Storm:ArcGrid,Steered:Storm:ArcGrid:Forge,Steered:Storm:Downpour,Steered:Storm:Downpour:Forge,Steered:Storm:Tempest,Steered:Storm:Tempest:Forge,Steered:Tide:Frost,Steered:Tide:Frost:Storm,Steered:Tide:Stillness,Steered:Tide:Stillness:Storm,Steered:Tide:Whirlpool,Steered:Tide:Whirlpool:Storm -Seeds 15 -Parallel 14 -Out Saved\BotRuns\d045_sources
python -I Tools\pressure.py Saved\BotRuns\d045_sources --check
python -I Tools\pressure.py <...>\agent-ac0cefc0ce333b409\Saved\BotRuns\final --check T-DIR-02
```

- **Runs:** 285 asked, 285 ran, 0 failed, 0 rerun, 0 killed on the wall timeout; `BOTRUNS: PASS`. Outcomes: 192 victories, 91 deaths and 2 runs ended at the 1100 s cap (None seed 4, at the boss; Tide/Whirlpool:Storm seed 3).
- **Logged or refused:** no `LogDT` error or warning, no fatal error, no access violation. Every log ends with `LogExit: Exiting`. Every run wrote its bot record and `run_end`.
- **Machine:** `wall_dropped_steps` 8917 in all, 279 of 285 runs, at most 343 (None seed 4). Wall 1327.4 s against 258202.4 sim s. `pressure.py` prints `SLOW: ... not read` for every bot arm at `-DTBotFast`, as its header says.
- **`pressure.py --check` prints:**
  - `T-DIR-02: FAIL (0/19 group(s) pass; ...; not read: None)`
  - `T-DIR-18: FAIL (0/19 group(s) pass; ...; not read: None)`
  - Both are printed here, not judged by this note.
  - 278 `RESTORES:` lines (D-046).

**How the tables are made.** `SOURCES:` is `pressure.py`'s line per run: damage taken inside `C.DIR.BAND_WINDOW_S` (600–780 s) by hit kind and enemy role, each hit as a share of max HP at that hit, per minute alive in the window. Per arm, a scratch script (`sources_agg.py`, in the session scratchpad) multiplies each run's rates back by its minutes alive, sums them, and divides by the arm's minutes alive. "Rusher share" is the Rusher role's part of all damage in the window. The whole-run columns, the hit counts, the drop-ins (the `dir_block.dropins` sum, every role) and HP lost by 780 s (the sum of `hero_damaged` amount over max HP before 780 s) are taken by the same script from the JSONL. Boss reach and wins come from `summary.csv`.

## 2. Per arm, `3cfa101` (after D-045 and D-046)

| Arm | Alive in window (through it) | Window: damage /min | Rusher /min | Rusher share | Rusher hits in window (Charge) | Whole run: Rusher share | Rusher hits (Charge) | Runs with a Rusher hit | Drop-ins | HP lost by 780 s, mean (median) | Reached boss | Wins |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| None | 10 (1) | 0.358 | 0.051 | 0.142 | 5 (0) | 0.135 | 13 (0) | 8 | 698 | 1.277 (1.209) | 1 | 0 |
| Forge/Furnace | 14 (14) | 0.061 | 0.017 | 0.287 | 3 (0) | 0.191 | 9 (0) | 6 | 1137 | 0.460 (0.353) | 14 | 13 |
| Forge/Furnace:Tide | 13 (11) | 0.080 | 0.007 | 0.093 | 1 (1) | 0.152 | 12 (2) | 7 | 1032 | 0.773 (0.731) | 11 | 7 |
| Forge/Hammer | 15 (15) | 0.000 | 0.000 | 0.000 | 0 (0) | 0.175 | 4 (0) | 4 | 1167 | 0.117 (0.073) | 15 | 15 |
| Forge/Hammer:Tide | 15 (15) | 0.078 | 0.016 | 0.202 | 3 (0) | 0.186 | 11 (2) | 6 | 1228 | 0.365 (0.148) | 14 | 9 |
| Forge/Slam | 14 (14) | 0.012 | 0.000 | 0.000 | 0 (0) | 0.354 | 14 (2) | 7 | 1162 | 0.315 (0.216) | 14 | 14 |
| Forge/Slam:Tide | 15 (13) | 0.096 | 0.029 | 0.303 | 5 (0) | 0.206 | 16 (0) | 7 | 1167 | 0.632 (0.602) | 13 | 7 |
| Storm/ArcGrid | 15 (15) | 0.042 | 0.006 | 0.136 | 1 (0) | 0.194 | 8 (0) | 6 | 1159 | 0.352 (0.332) | 14 | 13 |
| Storm/ArcGrid:Forge | 15 (15) | 0.036 | 0.011 | 0.305 | 2 (1) | 0.166 | 4 (3) | 4 | 1194 | 0.203 (0.162) | 15 | 14 |
| Storm/Downpour | 15 (15) | 0.043 | 0.005 | 0.123 | 1 (0) | 0.092 | 4 (1) | 3 | 1159 | 0.333 (0.216) | 15 | 14 |
| Storm/Downpour:Forge | 15 (15) | 0.021 | 0.005 | 0.259 | 1 (0) | 0.073 | 2 (0) | 2 | 1154 | 0.141 (0.153) | 15 | 15 |
| Storm/Tempest | 15 (14) | 0.056 | 0.039 | 0.699 | 7 (0) | 0.331 | 20 (0) | 10 | 1164 | 0.541 (0.516) | 13 | 12 |
| Storm/Tempest:Forge | 15 (15) | 0.033 | 0.011 | 0.323 | 2 (0) | 0.081 | 4 (0) | 3 | 1173 | 0.330 (0.256) | 15 | 13 |
| Tide/Frost | 12 (12) | 0.072 | 0.026 | 0.364 | 4 (0) | 0.171 | 14 (1) | 12 | 1059 | 0.746 (0.776) | 12 | 8 |
| Tide/Frost:Storm | 12 (12) | 0.075 | 0.034 | 0.458 | 5 (0) | 0.211 | 15 (1) | 9 | 1005 | 0.743 (0.659) | 11 | 8 |
| Tide/Stillness | 13 (11) | 0.097 | 0.032 | 0.336 | 5 (0) | 0.186 | 15 (0) | 9 | 1047 | 0.874 (0.977) | 9 | 7 |
| Tide/Stillness:Storm | 9 (8) | 0.091 | 0.011 | 0.118 | 1 (0) | 0.249 | 18 (1) | 8 | 797 | 0.869 (1.022) | 8 | 6 |
| Tide/Whirlpool | 13 (13) | 0.041 | 0.007 | 0.161 | 1 (0) | 0.206 | 22 (0) | 13 | 1098 | 1.028 (1.138) | 12 | 10 |
| Tide/Whirlpool:Storm | 12 (12) | 0.067 | 0.013 | 0.203 | 2 (0) | 0.231 | 24 (0) | 13 | 1022 | 1.034 (0.993) | 11 | 7 |

Each arm is 15 runs. The column "Alive in window" counts the runs alive at some point inside 600–780 s; "through it" counts those alive through all of it, the runs `pressure.py`'s verdicts count.

**Steered, 270 runs.**
- *Window:* 247 alive in the window (239 through it). Damage 38.79 max HP over 727.8 alive minutes, of which Rushers 10.74, share 0.277. 44 Rusher hits in 35 runs, 2 of them charges.
- *Whole run:* damage 237.33 max HP, of which Rushers 47.10, share 0.198. 216 Rusher hits in 129 runs, 13 of them charges. 19924 drop-ins.
- *By kind and role over the whole run (max HP, hits):* Contact/Tank 46.12 (257), Bullet/Turret 44.46 (557), Contact/Rusher 44.06 (203), Contact/Swarm 37.75 (463), Bullet/Boss 33.44 (209), Sweep/Boss 9.90 (55), DashCharge/Boss 8.58 (39), Contact/Boss 7.50 (50), **Charge/Rusher 3.04 (13)**, Charge/Boss 1.54 (7), HazardRing/Boss 0.48 (6), Contact/Turret 0.46 (5).
- *Outcomes:* 231 steered runs reached the boss; 192 won.

## 3. Before: the nearest earlier battery
No note before D-045 reads sources by arm. `S1_d045_d046.md` holds four single runs of seed 9107. `S1_keeper_power.md` §5 prints no source columns. `S1_owner_run.md` §3 is one person's run: 10 hits, none from a Rusher (Tanks 52.7 HP in 3, Turrets 45.2 in 6, Swarm 6.4 in 1). `S1_probes.md` §4 counts boss fights only (Charge 4 hits, 0.5 HP per 100 s).

The runs of `S1_keeper_power.md` §5's "Final" battery are still on disk: `E:\Dusktracer\.claude\worktrees\agent-ac0cefc0ce333b409\Saved\BotRuns\final`, 285 runs, build `d82ace8+dirty`, 2026-10-31. Its outcomes match §5: 196 steered at the boss, 153 wins. They were read in place with the same tool and the same script; nothing was rebuilt or rerun. **This is not a before of D-045 alone.** `3cfa101` is 177 commits past `d82ace8`, 39 of them touching `Source`, among them D-045, D-046, Hammer's Key on the Arc Blade, Hard Blow's knockback and the Rusher's strict comparison.

| Arm | Window Rusher share, before → after | Whole-run Rusher share, before → after | Rusher hits (Charge), before → after | Reached boss, before → after | Wins, before → after |
|---|---|---|---|---|---|
| None | 0.000 → 0.142 | 0.122 → 0.135 | 10 (0) → 13 (0) | 0 → 1 | 0 → 0 |
| Forge/Furnace | 0.202 → 0.287 | 0.213 → 0.191 | 9 (0) → 9 (0) | 14 → 14 | 10 → 13 |
| Forge/Furnace:Tide | 0.257 → 0.093 | 0.237 → 0.152 | 15 (1) → 12 (2) | 6 → 11 | 6 → 7 |
| Forge/Hammer | 0.000 → 0.000 | 0.188 → 0.175 | 5 (1) → 4 (0) | 14 → 15 | 13 → 15 |
| Forge/Hammer:Tide | 0.217 → 0.202 | 0.169 → 0.186 | 8 (2) → 11 (2) | 13 → 14 | 10 → 9 |
| Forge/Slam | 0.265 → 0.000 | 0.200 → 0.354 | 8 (1) → 14 (2) | 14 → 14 | 11 → 14 |
| Forge/Slam:Tide | 0.401 → 0.303 | 0.275 → 0.206 | 19 (0) → 16 (0) | 10 → 13 | 3 → 7 |
| Storm/ArcGrid | 0.136 → 0.136 | 0.268 → 0.194 | 9 (1) → 8 (0) | 12 → 14 | 11 → 13 |
| Storm/ArcGrid:Forge | 0.000 → 0.305 | 0.035 → 0.166 | 1 (0) → 4 (3) | 15 → 15 | 12 → 14 |
| Storm/Downpour | 0.814 → 0.123 | 0.334 → 0.092 | 10 (3) → 4 (1) | 15 → 15 | 14 → 14 |
| Storm/Downpour:Forge | 0.545 → 0.259 | 0.042 → 0.073 | 1 (1) → 2 (0) | 15 → 15 | 14 → 15 |
| Storm/Tempest | 0.616 → 0.699 | 0.275 → 0.331 | 16 (0) → 20 (0) | 10 → 13 | 6 → 12 |
| Storm/Tempest:Forge | 0.000 → 0.323 | 0.187 → 0.081 | 7 (1) → 4 (0) | 15 → 15 | 14 → 13 |
| Tide/Frost | 0.440 → 0.364 | 0.188 → 0.171 | 10 (0) → 14 (1) | 10 → 12 | 8 → 8 |
| Tide/Frost:Storm | 0.000 → 0.458 | 0.131 → 0.211 | 7 (0) → 15 (1) | 11 → 11 | 9 → 8 |
| Tide/Stillness | 0.354 → 0.336 | 0.200 → 0.186 | 14 (1) → 15 (0) | 6 → 9 | 3 → 7 |
| Tide/Stillness:Storm | 0.380 → 0.118 | 0.295 → 0.249 | 19 (0) → 18 (1) | 5 → 8 | 4 → 6 |
| Tide/Whirlpool | 0.382 → 0.161 | 0.214 → 0.206 | 16 (1) → 22 (0) | 6 → 12 | 3 → 10 |
| Tide/Whirlpool:Storm | 0.473 → 0.203 | 0.296 → 0.231 | 22 (2) → 24 (0) | 5 → 11 | 2 → 7 |

Steered totals, before → after:
- Window Rusher share 0.308 → 0.277; Rusher hits in the window 37 (6 Charge) in 33 runs → 44 (2 Charge) in 35 runs.
- Whole-run Rusher share 0.217 → 0.198; Rusher hits 196 (15 Charge) in 129 runs → 216 (13 Charge) in 129 runs.
- Drop-ins 18137 → 19924.
- Damage taken: 195.64 → 237.33 max HP over the whole run; 29.37 → 38.79 in the window, over 663.4 → 727.8 alive minutes.
- Reached boss 196 → 231; wins 153 → 192.

## 4. What D-045 predicted, and what was read
The hypothesis line: "charging through the hero's position moves the share of Rusher arrivals whose band covers that position from about a quarter (geometry above) to all of them, and Rusher-source damage in steered runs from none to some … measured by the new assertions and by `Tools/pressure.py` SOURCES on the nineteen arms before and after".

- **Bands that cover the hero's position.** Its measure is the assertions, already read in D-045's Result: T-ENM-13, 0 of 48 landings before and 48 of 48 after. A battery cannot read it. The telemetry has no arrival record: `hero_damaged` carries `hit_kind` and `role` but no arrival flag, and `dir_block.dropins` counts drop-ins of every role. So **how many arrivals charged and how many hit is not read: blocked.** The smallest change that would let a battery read it is outside this job: an `arrival` field on `hero_damaged` (`DTTelemetryRecords.cpp`, set from `DTEnemyFlags::Arrival`), or one record per arrival charge.
- **Rusher-source damage in steered runs, "from none to some".**
  - After: some. Rushers deal 0.277 of window damage and 0.198 of whole-run damage on the steered arms. Sixteen of eighteen steered arms take Rusher damage in the window; Forge/Hammer and Forge/Slam take none there. 129 of 270 steered runs take at least one Rusher hit.
  - Before: at `d82ace8` the steered arms already took 0.308 of window damage and 0.217 of whole-run damage from Rushers, in 129 runs. So "none", the premise taken from `S1_balance_pass2.md` and the owner's run, does not describe that battery.
  - What hits is contact, not the charge. Rusher charge hits number 15 before and 13 after over the whole run, and 6 before and 2 after in the window. Rusher contact over the whole run is 181 hits before and 203 after.
  - Between the two builds the steered window share falls 0.031 and the whole-run share falls 0.019. Rusher hits rise by 20 (196 → 216); the charge hits are 2 fewer.
- **The revert clause** ("if the assertions cannot pass without shortening `C.ENEMIES.TELEGRAPH_LEAD_MIN_S`, or if two validators at session A call an arrival a hit they had no time to answer"): the assertions pass with the lead unchanged (D-045 Result); session A has not been held. This battery reads neither half.

## 5. What this battery cannot say
- Which Rusher hits come from arrivals and which from Rushers that walked into range: the telemetry does not separate them (§4).
- Whether D-045 alone moved any figure in §3. The before is 177 commits older, and the two batteries differ in many rules at once.
- What a person meets. Skill 1.0 is a ceiling, and the bot steers out of telegraphs (`DTBotBrain`), so a charge's band is a threat it is built to dodge. The owner's one run took no Rusher hit.
- Whether any number should move. Boss reach and wins are printed only (D-050 item 5).
- Co-op, the Shipping build, other maps: every run is one player, the Development editor build, `L_Arena_Test`.

## Where the runs are
- This battery: `E:\Dusktracer\.claude\worktrees\agent-a559188263b0e1f6c\Saved\BotRuns\d045_sources` (`summary.md`, `summary.csv`, 285 run folders).
- The `pressure.py` output: `Saved\Logs\pressure_d045_sources.log`.
- Logs: `Saved\Logs\botruns_d045_sources.log` and `build_d045_sources.log`.
- The earlier battery, read in place: `E:\Dusktracer\.claude\worktrees\agent-ac0cefc0ce333b409\Saved\BotRuns\final`.
