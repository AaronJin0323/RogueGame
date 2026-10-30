# S1 Pull rule — one pull per enemy per interval (2026-10-30)

Goal: stop `Whirlpool.Key` Pull working as an enemy speed boost (D-016, R-KEE-21), close the two gaps of `S1_tide_expression.md` §6 items 1 and 2, and trace a damage reading (§5). Class: RULE, plus two BUG fixes. Layer: Keeper flags. Replaces: Pull on every impact. Bot v1 at skill 1.0, headless, seeds 1..15, all 19 arms, `Tools/BotRuns.ps1 -Parallel 8`. Control: `tide_after`, the end of `S1_tide_expression.md`.

**Verdict: the rule meets its hypothesis at the first interval, 2.0 s, and is kept. T-OFF-15 fails 199/201. The field status fix is what lowers wins in the arms without Tide, 57 to 40 (§4).**

## 1. Hypothesis and interval
Written in `number_change_log.csv` before any measurement.

Starting interval, `C.KEEPER.PULL_INTERVAL_S` 2.0 s, from the data:
- The Pulse Rifle lands 4 impacts a second on one enemy (24 in 6 s in `Param.PullInterval` without the rule), so 120 uu per pull is at least 480 uu/s, above the Swarm's own 280. `distance_uu` probes: 200 uu/s reads 140 s, 5 uu/s reads 296 s (`S1_balance_pass2.md` §6).
- At 2.0 s the pull is 60 uu/s at most, 21 % of Swarm speed. A Swarm crosses the rifle's 900 uu in 3.2 s, so it is pulled at most twice on the way in.
- 2.0 s does not exceed `C.STATUS.CHILL_S` or `pulled_vuln_s` (both 2.0). A longer interval would let every pull's Chill and mark lapse before the next, and `chill_stacks_on_pull` could never build a Freeze.

| Hypothesis | Reading | Verdict |
|---|---|---|
| Primary Whirlpool median end > 300 s | 527 s, 501 s (:Storm) | met |
| Primary Whirlpool reaches L8 in most runs | 14/15, 14/15 | met |
| Second-Tide arms reach the boss ≥ 5/15 | Hammer 12, Furnace 8, Slam 11, Tempest 13, Arc Grid 13, Downpour 15 | met |

One interval tried, kept. The second try was not needed.

## 2. Before and after, 15 seeds
Reached boss / wins / median end s / damage share per min in 600–780 s / Keeper share at L16 (runs with an L16 reading). Damage is the mean of the runs alive in the window (§5 on what that means). Before = `tide_after`, the "After" column of `S1_tide_expression.md` §2. After = this tree: the rule and both fixes.

| Arm | Before | After |
|---|---|---|
| None | 1 / 0 / 589 / 0.41 / — | 1 / 0 / 589 / 0.41 / — |
| Tide/Frost (:Forge) | 7 / 5 / 831 / 0.05 / 0.40 (9) | 12 / 7 / 963 / 0.04 / 0.43 (13) |
| Tide/Frost:Storm | 8 / 4 / 956 / 0.04 / 0.38 (10) | 11 / 7 / 966 / 0.04 / 0.37 (13) |
| Tide/Stillness (:Forge) | 4 / 4 / 686 / 0.14 / 0.29 (7) | 6 / 4 / 742 / 0.14 / 0.27 (9) |
| Tide/Stillness:Storm | 4 / 3 / 518 / 0.37 / 0.27 (6) | 7 / 5 / 706 / 0.11 / 0.26 (7) |
| Tide/Whirlpool (:Forge) | 0 / 0 / 110 / — / — | **5 / 2 / 527** / 0.03 / 0.31 (7) |
| Tide/Whirlpool:Storm | 0 / 0 / 110 / — / — | **5 / 2 / 501** / 0.01 / 0.33 (5) |
| Forge/Hammer:Tide | 1 / 1 / 552 / 0.51 / 0.55 (8) | **12 / 4 / 961** / 0.07 / 0.59 (15) |
| Forge/Furnace:Tide | 1 / 0 / 555 / 0.39 / 0.36 (8) | **8 / 2 / 940** / 0.12 / 0.37 (14) |
| Forge/Slam:Tide | 2 / 1 / 549 / 0.22 / 0.47 (5) | **11 / 3 / 957** / 0.08 / 0.45 (15) |
| Storm/Tempest (:Tide) | 1 / 0 / 641 / 0.65 / 0.44 (10) | **13 / 7 / 979** / 0.06 / 0.41 (15) |
| Storm/ArcGrid (:Tide) | 1 / 0 / 455 / 0.60 / 0.52 (6) | **13 / 8 / 965** / 0.03 / 0.50 (13) |
| Storm/Downpour (:Tide) | 2 / 0 / 560 / 0.05 / 0.59 (8) | **15 / 9 / 970** / 0.03 / 0.59 (15) |
| Forge/Hammer (:Storm) | 13 / 9 / 962 / 0.08 / 0.61 (15) | 14 / 5 / 962 / 0.01 / 0.60 (15) |
| Forge/Furnace (:Storm) | 14 / 8 / 973 / 0.02 / 0.45 (15) | 11 / 5 / 956 / 0.08 / 0.45 (14) |
| Forge/Slam (:Storm) | 13 / 7 / 981 / 0.04 / 0.45 (14) | 14 / 7 / 973 / 0.03 / 0.47 (15) |
| Storm/Tempest:Forge | 15 / 10 / 970 / 0.01 / 0.42 (15) | 15 / 8 / 976 / 0.02 / 0.42 (15) |
| Storm/ArcGrid:Forge | 15 / 9 / 980 / 0.03 / 0.59 (15) | 14 / 8 / 980 / 0.03 / 0.60 (15) |
| Storm/Downpour:Forge | 15 / 13 / 986 / 0.02 / 0.62 (15) | 15 / 7 / 968 / 0.01 / 0.62 (15) |

Second-Tide arms split by the card, as `S1_tide_expression.md` §3: took `Whirlpool.Key` 1 of 77 → **68 of 85**, median 513 → 963 s; did not 7 of 13 → 4 of 5, 914 → 972 s. The card no longer decides these arms.

| Check | Before | After |
|---|---|---|
| T-DIR-01 (no-pick median death) | PASS 588.6 s | PASS 588.6 s, the None arm unchanged |
| T-OFF-15 on seeds 1..15 | PASS 116/116, mean 28.56 | **FAIL 199/201**, mean 28.66 |
| `Tools/pressure.py --check` | FAIL, 1/17 arms inside both bands | FAIL, **0/19** (the one before, Forge/Furnace:Tide, left the band at 0.12 by surviving) |

T-OFF-15's two misses are low, not over budget: Tide/Stillness:Storm seed 9 at 23 decisions and Storm/ArcGrid:Forge seed 4 at 24, both dying near 947 s. The floor is 25. ArcGrid:Forge carries no Pull, so the interval is not the lever for that run, and no number was changed to answer it. The checker reads every steered run at the boss, and 84 more of them reach it now. With the field status off (§4) 1 of the 114 runs at the boss misses (Storm/Downpour seed 14, 24), so the misses do not depend on one fix alone. **The brief asked for this check to pass, and it does not.** It is the same shape as Tide/Frost seed 11 in `S1_balance_pass2.md` §7: a slow build, not an overrun.

## 3. What was changed
- **R-KEE-21:** `PullReadyStep` in the enemy pool (SoA, swapped with the slot), read through `FDTEnemyPool::TryPull`. Every `Flag.Pull` is held, Stillness's and `Mod.Backwash` included, and crush, the `pulled_vuln` mark, `on=Pulled` statuses and the cluster count ride on the applied pull only. The mine's pull and the Tide Intercession's line are not `Flag.Pull` and are not held. The weapon system's pool pointer is no longer const so it can stamp the field.
- **R-KEE-22:** a field applies its `status` every `C.STATUS.BURN_TICK_S`, once per enemy per source. Not per step: a Burn past its cap replaces its oldest stack and restarts that stack's tick, so step-rate application would stop the stack dealing damage. No new number.
- **Whirlpool's layer:** `Flag.Pull{on=Impact;distance_uu=150}` → `Flag.Pull{distance_uu=150}`, which folds into the Key's entry for the cast. An entry of its own with a radius would be held behind the Key's pull on the same impact and still do nothing. The layer implies the Key (R-KEE-06), so the reach is always there. No number changed. A build with Whirlpool Amp1 is already at 150, so this layer adds nothing to it.

Tests, each seen failing first: `Param.PullInterval` (24 moves in 3 intervals without the rule), `Param.FieldStatus` (no Burn, Downpour's field no Shock), `Param.WhirlpoolLayer` (pull 120 during the cast, not 150).

## 4. What reads wrong: field status lowers wins
The six arms with no Tide lose wins: 56 → 40, boss reach 85 → 83. The rule does not reach them, so a probe re-ran them with only the field status switched off (no commit; 15 seeds):

| Six arms without Tide | Reached boss | Wins | Boss fights' damage taken, HP per 100 s (Hazard hits per 100 s) |
|---|---|---|---|
| `tide_after` | 85 | 56 | — |
| This tree, field status off | 86 | 57 | Hazard 29 (3.1), Bullet 11, Contact 3 |
| This tree | 83 | **40** | Hazard **50** (5.9), Bullet 8, Contact 3 |

Run for run, field status turns 27 wins into losses and 10 losses into wins. Damage dealt in the fight does not fall: Hammer 191 → 199 per second, Tempest:Forge 166 → 173, Downpour:Forge 174 → 176, Furnace 183 → 166. The hero dies more to the shrinking hazard ring of phase III.

A mechanism that fits, in part: the boss's HP is set from the damage per second of its first seconds (`boss_calibrated`), and fields raise that reading more than they raise the fight. Median calibrated HP rises on Hammer 15.1k → 17.3k, Furnace 9.6k → 10.2k, Slam 14.1k → 15.2k, Tempest:Forge 10.2k → 12.5k. A longer fight meets more of the ring. It does not explain everything: Downpour:Forge's median is unchanged (15.3k → 15.5k), and it loses the most (13 → 7); ArcGrid:Forge's median falls. This is an indication, not a traced cause. **The card now does what it says, and it costs these arms wins.** Nothing was tuned against it.

`S1_tide_expression.md` §6 predicted that this fix would raise Forge and Storm. It did not.

## 5. Trace: window damage on Storm/Tempest and Storm/ArcGrid
`S1_tide_expression.md` §4: 0.33 → 0.65 and 0.28 → 0.60 with no change in boss reach, `tide_control` to `tide_after`.

**Verdict: mostly an artefact of the metric plus which runs are in the window; not a bug; any real effect is too small to read at this sample.** No number was changed.

The reading is the mean over runs of (damage in the window ÷ minutes alive in the window). A run that dies in the window takes about 1.0 max HP whenever it dies, so its rate is about 1 ÷ its minutes alive, and a death 30 s in scores 2.0 where a full 3 minutes scores 0.33. That reading, unweighted, is what doubled:

| | Runs in window | Mean of rates | Pooled damage ÷ minutes | Same-seed mean |
|---|---|---|---|---|
| Storm/Tempest | 8 → 10 | 0.33 → 0.65 | 0.20 → 0.29 | 0.33 → 0.41 (8 seeds) |
| Storm/ArcGrid | 5 → 5 | 0.28 → 0.60 | 0.20 → 0.32 | 0.28 → 0.60 (5 seeds) |

- **Tempest is survivorship.** Seeds 6 and 14 died at 500 s and 385 s before, and now live to 641 s and 626 s. Each then dies 26–41 s into the window, reading 1.18 and 2.05. Those two runs are half of the new sum of rates. The other eight rise only 0.33 → 0.41.
- **Arc Grid is the denominator.** The same five seeds are in the window. Two of them account for 1.36 of the 1.60 rise in the sum of rates: seed 14 takes the same 0.61 max HP but dies at 638 s, not 780 s (0.21 → 0.96), and seed 15 takes 1.07 → 1.09 and dies 30 s sooner (0.87 → 1.48).
- **It is not a damage bug.** No run takes more than 1.09 max HP in the window, all of it contact, in both trees. So nothing counts twice.
- **Real effect, unresolved.** The trees are identical run for run up to the first Tide-card effect, at 339–660 s in the eight seeds checked, and diverge after it. Over all 15 seeds the median change in end time is 0 s on both arms, with moves both ways of up to 241 s. Arc Grid's five window seeds all end earlier, but five seeds cannot carry that.

This tree reads 0.06 and 0.03 on these arms with 15 and 13 runs in the window: the same metric, read with no dying runs in it. `pressure.py` judges each arm by this mean of rates, so an arm where runs die early in the window reads high for that reason; a pooled reading of total damage over total minutes would not.

## 6. What else reads differently
- **Tide is no longer the split.** Second-Tide arms went from 0–2 of 15 at the boss to 8–15, level with the arms without it. Tide's damage term did not rise (Keeper share at L16 is flat): removing the speed boost alone did this.
- **The spread moved to primary Whirlpool.** 5 of 15 at the boss is the lowest in the table now, below Frost at 11–12, with its median end 200–400 s short of them. Still well past its old 110 s.
- **Damage taken is lower everywhere.** Second-Tide arms read 0.03–0.12, far under `C.DMG_TAKEN.BAND_MIN10_13`, and no arm sits in both bands. `S1_balance_pass2.md` §8 item 2 stands: the band and "reaches the boss commonly" do not hold together with no heal.
- **Not covered by these runs:** `chill_stacks_on_pull`, `cluster_freeze_min` and `shatter` under the interval. Rides-on-the-pull is tested for `on=Pulled`, but how often a cluster of 5 still forms when each enemy is held was not read.
- A Burn or Shock from a field has no cue of its own beyond what the status already shows; a validator should check it reads.

All of this is the bot at skill 1.0, a bound; nothing here is a human reading (O-008).
