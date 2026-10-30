# S1 Tide expression — the unread Keeper parameters (2026-10-30)

Goal: express the parameters `keepers.md` Open listed as unread, Tide first, change no tuned number, and re-measure every arm (`S1_balance_pass2.md` §8 item 1). Class: BUG (cards weaker than their text, R-KEE-02), layer: weapons and Keeper flags, replaces: the no-op the card was. Bot v1 at skill 1.0, headless, seeds 1..15, `Tools/BotRuns.ps1 -Parallel 8`.

**Verdict: the cards now work and Frost as a primary recovered, but the split the pass was aimed at did not close.** Arms with Tide as second Keeper still reach the boss 0–2 of 15. §3 shows why: it is no longer the unread parameters, it is `Whirlpool.Key`.

## 1. Before
Before = the tree at `2d30b03`, re-run here on all 19 arms. It reproduces the 15-seed row at the end of `S1_balance_pass2.md` §7 on every arm that row holds, and adds the second-Keeper arms the row lacks. The bot's default second Keeper makes `Storm/*` the `:Tide` arms, and `Tide/*` the `:Forge` arms.

## 2. Before and after, 15 seeds
Reached boss / wins / median end s / damage share per min in 600–780 s / Keeper share at L16 (runs with an L16 reading). Damage is the mean of the runs alive in the window, so a low count reads noisily.

| Arm | Before | After |
|---|---|---|
| None | 1 / 0 / 589 / 0.41 / — | 1 / 0 / 589 / 0.41 / — |
| Tide/Frost (:Forge) | 4 / 0 / 552 / 0.08 / 0.24 (5) | **7 / 5 / 831** / 0.05 / 0.40 (9) |
| Tide/Frost:Storm | 2 / 1 / 629 / 0.41 / 0.19 (4) | **8 / 4 / 956** / 0.04 / 0.38 (10) |
| Tide/Stillness (:Forge) | 2 / 1 / 727 / 0.25 / 0.24 (8) | 4 / 4 / 686 / 0.14 / 0.29 (7) |
| Tide/Stillness:Storm | 6 / 4 / 784 / 0.11 / 0.27 (9) | 4 / 3 / 518 / 0.37 / 0.27 (6) |
| Tide/Whirlpool (:Forge) | 0 / 0 / 110 / — / — | 0 / 0 / 110 / — / — |
| Tide/Whirlpool:Storm | 0 / 0 / 110 / — / — | 0 / 0 / 110 / — / — |
| Forge/Hammer:Tide | 1 / 0 / 556 / 0.34 / 0.54 (7) | 1 / 1 / 552 / 0.51 / 0.55 (8) |
| Forge/Furnace:Tide | 0 / 0 / 529 / 0.58 / 0.33 (6) | 1 / 0 / 555 / 0.39 / 0.36 (8) |
| Forge/Slam:Tide | 1 / 1 / 522 / 0.36 / 0.44 (5) | 2 / 1 / 549 / 0.22 / 0.47 (5) |
| Storm/Tempest (:Tide) | 1 / 0 / 637 / 0.33 / 0.44 (8) | 1 / 0 / 641 / 0.65 / 0.44 (10) |
| Storm/ArcGrid (:Tide) | 1 / 1 / 457 / 0.28 / 0.53 (5) | 1 / 0 / 455 / 0.60 / 0.52 (6) |
| Storm/Downpour (:Tide) | 1 / 1 / 509 / 0.18 / 0.33 (5) | 2 / 0 / 560 / 0.05 / 0.59 (8) |
| Forge/Hammer (:Storm) | 13 / 7 / 956 / 0.09 / 0.59 (15) | 13 / 9 / 962 / 0.08 / 0.61 (15) |
| Forge/Furnace (:Storm) | 9 / 6 / 927 / 0.09 / 0.43 (15) | 14 / 8 / 973 / 0.02 / 0.45 (15) |
| Forge/Slam (:Storm) | 13 / 2 / 953 / 0.03 / 0.43 (14) | 13 / 7 / 981 / 0.04 / 0.45 (14) |
| Storm/Tempest:Forge | 14 / 9 / 982 / 0.02 / 0.41 (15) | 15 / 10 / 970 / 0.01 / 0.42 (15) |
| Storm/ArcGrid:Forge | 15 / 6 / 970 / 0.03 / 0.58 (15) | 15 / 9 / 980 / 0.03 / 0.59 (15) |
| Storm/Downpour:Forge | 13 / 6 / 962 / 0.05 / 0.44 (15) | 15 / 13 / 986 / 0.02 / 0.62 (15) |

| Check | Before | After |
|---|---|---|
| T-DIR-01 (no-pick median death) | PASS 592.9 s on 10 seeds, 588.6 s on 15 | PASS, the same: the None arm is identical run for run |
| T-OFF-15 on seeds 1..15 | 95/96 (Tide/Frost seed 11 at 21 decisions, as `S1_balance_pass2.md` §7) | **PASS 116/116**, mean 28.56 |
| `Tools/pressure.py --check` | FAIL, 4/17 arms inside both bands | FAIL, **1/17** (Tide/Stillness:Storm misses dashes; Forge/Furnace:Tide is the one in) |

## 3. Why the second-Keeper arms did not move
Bot picks in the six second-Tide arms, split by whether the run took one card. No number changed for this reading; it is the same runs as §2.

| Card taken | Before: took | Before: did not | After: took | After: did not |
|---|---|---|---|---|
| `Whirlpool.Key` | 1 of 81 at the boss, median 515 s | 4 of 9, 876 s | **1 of 77, 513 s** | **7 of 13, 914 s** |
| `Stillness.Amp1` | 3 of 29, 722 s | 2 of 61, 457 s | 5 of 40, 705 s | 3 of 50, 441 s |

The second-Tide arms take `Whirlpool.Key` in 70–100 % of runs. That card is the Pull problem of `S1_balance_pass2.md` §8 item 3, and it decides these arms whatever else the build holds. The thirteen runs that skip it are few and are selected by the offers, so 7 of 13 is an indication, not a measured gain. It still points past the content layer: the next lever is the RULE candidate (Pull per-enemy interval), held by this brief.

## 4. What else reads differently
- **Frost is the clear gain.** Median end rises by 280–330 s and Keeper share at L16 nearly doubles. Frost arms held the same four cards before; those cards now do something.
- **Stillness did not gain.** Tide/Stillness:Storm fell (6 → 4, median 784 → 518 s) while Tide/Stillness:Forge rose (2 → 4). Same primary, opposite moves, so this reads as noise around no gain, not as an effect. Not traced further; how often the bot stands still long enough to charge was not measured.
- **Whirlpool is unchanged.** It dies at 110 s, before any of its expressed cards can be owned, so `cluster_freeze_min`, `shatter` and `chill_stacks_on_pull` are covered by tests and not by these runs.
- **Strong arms got stronger.** Furnace 9 → 14, Downpour:Forge wins 6 → 13. Their damage taken falls further under `C.DMG_TAKEN.BAND_MIN10_13`, so the spread between arms widened.
- **The band count fell from 4 to 1 for mixed reasons.** Of the four arms in band before, Tide/Frost:Storm left by getting stronger (0.41 → 0.04), Forge/Slam:Tide fell under (0.22), and Forge/Hammer:Tide and Storm/Tempest rose over (0.51, 0.65). The one arm in band now, Forge/Furnace:Tide, still dies every run.
- **Storm/Tempest and Storm/ArcGrid double in window damage** (0.33 → 0.65, 0.28 → 0.60) with no change in reach. Over 5–10 runs alive in the window, mostly dying in it; not traced further.

## 5. What was expressed
| Card | Parameters | Behaviour, as built |
|---|---|---|
| Frost | `shatter_on_frozen_kill_uu`, `shatter_dmg`, `shatter_status` | a Frozen death bursts as Keeper damage |
| | `frozen_blocks_enemies` | overlapping enemies are pushed to the Frozen enemy's rim through the displacement queue; a wall holds to within a step of movement |
| | `freeze_until_hit`, `held_max` | a Freeze re-applied step by step until a weapon strikes (`enemies.md` owns Freeze, so nothing shortens one still running) |
| | `frozen_hit_chills_neighbours_uu` | a hit on a Frozen enemy Chills around it |
| Whirlpool | `cluster_freeze_min`, `shatter` | touching pulled enemies not yet Frozen Freeze together and burst once |
| Stillness | `status`, `charged_pull_uu`, `charged_aura_uu`, `aura_status`, `aura_interval_s`, `charged_freeze_ring_uu` | a charged volley's status, pull and ring; an aura while charged |
| Slam | `after_hit_taken_mult`, `embers`, `ember_status`, `detonate_burning` | the slam after a hit; a ring of embers; a Burn's remaining damage dealt at once (the Burn runs on) |
| Furnace | `burn_tick_rate` | raises a Burn's damage per second; the tick stays `C.STATUS.BURN_TICK_S` |
| Hammer | `apply_modifier_on_hit` | a live Ricochet bounces off a cleave's hits |
| Downpour | `every_n` | every Nth sky strike hits the screen |
| Arc Grid | `kill_shocks_neighbours_uu` | an arc's kill Shocks around the death |

Tests: one `Dusktracer.Keepers.Param.*` each (T-KEE-21), each with an arm that shows the behaviour absent without the param.

Beyond the list: `chill_stacks_on_pull` (Whirlpool Amp1) was read off the wrong entry, so Cold Draw did nothing. It is fixed and now sets the stacks (R-KEE-12) instead of adding one to them. Stillness's Key `status` was listed only in `weapons.md` Open.

New GATED starting points, logged in `number_change_log.csv`: Whirlpool Trait `shatter_dmg` 0.50 and `shatter_uu` 200 (Frost Trait's values), Slam Amp2 `ember_dmg` 0.25 (first guess). No tuned number and no CORE constant changed.

Left, with the reason:
- `time_scale`: R-TEC-03 and R-DIR-10.
- `counts_still`, `window_s`, `dash_keeps`, `weapons`: defaults the code honours; no reader names them on purpose.
- `all_volleys_charged`: the cast's overlay charges in one step, so every volley is charged; pinned by `Param.StillnessAllVolleysCharged`, no new reader.
- Hammer's card reaches Ricochet only. Split is dormant under a conversion (R-MOD-08), and the other live modifiers already reach cleaves.

## 6. Found, not changed
1. **Field `status` is unread and not in `keepers.md` Open.** No field tick applies a status, so Furnace's Key field, Downpour's "field that Shocks" (Amp1) and the Furnace variant and layer fields do less than their cards say. Expressing it would raise Forge and Storm further.
2. **Whirlpool's layer `Flag.Pull{on=Impact;distance_uu=150}` names no radius**, so "impacts pull enemies" does nothing during Intercession.
3. **Card text and behaviour differ slightly:** Permafrost's Freeze is not ended early by a hit that lands inside its normal time, and Bellows' "ticks faster" is more damage per second at the same tick.
4. **Presentation:** a shatter draws a ring; the aura, the Frozen wall and the hold have no cue of their own, so a validator may not see them work.

## 7. What remains
RULE candidates, unchanged and now better evidenced: Pull per-enemy interval (§3), share caps on a floor of alive enemies. The heal question (`S1_balance_pass2.md` §8 item 2) is unchanged. All of this is the bot at skill 1.0, a bound; nothing here is a human reading (O-008).
