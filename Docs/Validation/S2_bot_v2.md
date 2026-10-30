# S2 bot v2 and skill knob (2026-10-31)

Class: SYSTEM (a tool). Layer: the measuring instrument, `Source/Dusktracer/Bot/` and its tools. Replaces: nothing, v1 stays beside it (`02_Systems/bot.md` R-BOT-01). No game number or behaviour changed. Everything here is a bot on the S1 content, headless: **a bound, never a human reading** (O-008 has not happened, so no skill below is calibrated to any player).

## Reproduce
Branch `dev/s2-bot-v2`, off `main` at b79cf51. 19 arms (no-pick plus the 18 Steered arms of `S1_keeper_power.md` §5), seeds 1..15, 285 runs per batch, sim cap 1100 s.
```
& Tools\BotRuns.ps1 -Arms <the 19 arms> -Seeds 15 -BotVersion 2 -Skill 0.6 -Parallel 14 -Out Saved\BotRuns\f_v2_060
python -I Tools\bot_compare.py v1_1.0=Saved\BotRuns\f_v1_100 v2_1.0=Saved\BotRuns\f_v2_100 v2_0.6=Saved\BotRuns\f_v2_060 v2_0.3=Saved\BotRuns\f_v2_030
```

## v1 is reproducible (T-BOT-01)
- 24 runs (4 arms, seeds 1..6) on the unmodified `main` build against the final branch at `-BotVersion 1`: 24 of 24 identical `input_hash` and end time.
- All 285 v1 runs, two branch builds apart: 285 of 285 identical.
- The branch's v1 batch reads the STATE figures exactly: 196 steered runs at the boss, 153 wins, T-DIR-01 PASS 588.6 s, T-OFF-15 FAIL 195/196.

`-BotVersion` defaults to 1 and `Tools/probe.py` stays on v1, so every S1 command runs unchanged. The default flips at the S1 exit (`bot.md` Open).

## What skill does today (v1)
`-DTBotSkill` below 1 was a decision delay of up to 0.25 s, a fresh random angle on each decision (up to 35° move, 20° aim), and a silent switch to `Manual` aim. That is noise, and the aim part is a handicap a default player does not have: `Auto` is the default and aim is never required (R-CMB-04). V1's sub-1.0 numbers are left alone and are not a player.

## Readings
Cell: boss reach, wins, median death minute (deaths only).

| Arm | v1 1.0 | v2 1.0 | v2 0.6 | v2 0.3 | v1 0.6 (contrast) |
|---|---|---|---|---|---|
| None | 0/15 0 9.8 | 0/15 0 9.8 | 0/15 0 7.4 | 0/15 0 5.7 | 0/15 0 9.9 |
| Forge/Furnace:Tide | 6/15 6 12.9 | 9/15 5 12.1 | 0/15 0 7.4 | 0/15 0 4.6 | 4/15 1 10.6 |
| Forge/Furnace | 14/15 10 16.8 | 15/15 13 16.8 | 3/15 1 7.4 | 0/15 0 4.7 | 7/15 7 10.2 |
| Forge/Hammer:Tide | 13/15 10 16.1 | 12/15 7 15.2 | 2/15 0 7.4 | 0/15 0 5.0 | 6/15 1 12.3 |
| Forge/Hammer | 14/15 13 13.0 | 15/15 15 - | 9/15 0 15.2 | 1/15 0 5.8 | 12/15 4 15.6 |
| Forge/Slam:Tide | 10/15 3 15.1 | 7/15 6 13.3 | 0/15 0 7.9 | 0/15 0 5.5 | 5/15 1 10.3 |
| Forge/Slam | 14/15 11 16.2 | 14/15 13 14.6 | 4/15 0 9.4 | 1/15 0 5.4 | 8/15 4 13.6 |
| Storm/ArcGrid:Forge | 15/15 12 17.0 | 15/15 13 16.7 | 6/15 1 10.1 | 2/15 0 7.7 | 12/15 6 16.3 |
| Storm/ArcGrid | 12/15 11 11.6 | 10/15 9 11.5 | 6/15 0 12.7 | 1/15 0 8.0 | 8/15 7 10.1 |
| Storm/Downpour:Forge | 15/15 14 17.3 | 15/15 15 - | 8/15 1 15.1 | 1/15 0 9.8 | 15/15 9 16.3 |
| Storm/Downpour | 15/15 14 17.3 | 13/15 12 14.3 | 8/15 0 15.3 | 3/15 0 7.3 | 10/15 9 9.6 |
| Storm/Tempest:Forge | 15/15 14 16.2 | 14/15 14 9.0 | 1/15 1 7.9 | 0/15 0 3.9 | 12/15 7 16.4 |
| Storm/Tempest | 10/15 6 14.5 | 9/15 9 10.0 | 0/15 0 8.3 | 1/15 0 3.9 | 3/15 3 9.8 |
| Tide/Frost:Storm | 11/15 9 9.6 | 11/15 10 7.5 | 7/15 2 12.5 | 0/15 0 7.2 | 6/15 4 11.6 |
| Tide/Frost | 10/15 8 10.7 | 10/15 6 8.7 | 2/15 0 9.0 | 0/15 0 6.7 | 8/15 4 11.0 |
| Tide/Stillness:Storm | 5/15 4 9.3 | 11/15 7 13.1 | 4/15 1 7.5 | 0/15 0 5.7 | 5/15 1 11.9 |
| Tide/Stillness | 6/15 3 13.0 | 8/15 6 12.7 | 1/15 0 7.7 | 0/15 0 5.7 | 4/15 3 11.0 |
| Tide/Whirlpool:Storm | 5/15 2 8.7 | 10/15 7 12.9 | 2/15 0 6.5 | 0/15 0 4.9 | 1/15 1 6.5 |
| Tide/Whirlpool | 6/15 3 12.2 | 2/15 1 10.5 | 0/15 0 6.3 | 0/15 0 4.9 | 3/15 2 7.2 |
| **Steered (270)** | 196 153 12.9 | 200 168 12.7 | 63 7 8.5 | 10 0 5.6 | 129 74 11.3 |

| Steered measure | v1 1.0 | v2 1.0 | v2 0.6 | v2 0.3 |
|---|---|---|---|---|
| Median run end s | 965 | 968 | 513 | 338 |
| Dashes/min, 600-780 s (runs in window) | 5.36 (231) | 4.91 (233) | 5.90 (104) | 5.81 (30) |
| Damage share/min, same window | 0.06 | 0.05 | 0.22 | 0.25 |
| Collected XP / model (reference 0.51) | 0.71 | 0.71 | 0.70 | 0.67 |
| Kills/min | 198 | 200 | 175 | 153 |
| Commonest last hits | Boss round, Rusher, Tank | Rusher, Boss round, Turret | Tank, Rusher, Turret | Tank, Swarm, Rusher |
| T-DIR-01 (v1's check, no-pick) | PASS 588.6 | PASS 588.6 | out, 444.9 | out, 340.1 |
| T-OFF-15 | FAIL 195/196 | FAIL 199/200 | 63/63 | 10/10 |

Deaths by minute, Steered arms (minute: runs):
- v1 1.0: 4:6 5:4 6:3 7:7 8:8 9:11 10:7 11:7 12:7 13:8 14:6 15:13 16:21 17:9
- v2 1.0: 4:6 5:2 6:3 7:10 8:8 9:8 10:4 11:6 12:9 13:10 14:4 15:9 16:20 17:3
- v2 0.6: 0:2 1:2 2:8 3:12 4:4 5:17 6:32 7:42 8:27 9:20 10:18 11:5 12:5 13:4 14:9 15:46 16:10
- v2 0.3: 2:28 3:37 4:41 5:52 6:22 7:40 8:15 9:5 10:4 11:4 12:6 13:2 14:4 15:9 16:1

T-DIR-01 stays a v1 check (R-DIR-01): its band says nothing about v2 below 1.0. `12_Production` names a death-histogram band; no spec sets one, so these histograms have nothing to be in band of yet.

## The two named faults
- **Reach.** Seed 6 of Tide/Stillness:Storm took Arc Blade at 266 s in v1; v2 takes Orbital Drones in that hand and still wins (end 934.8 → 1000.3 s). Arc Blade picks fall 207 → 117 of 270 runs. The rest are mostly second weapons: see below.
- **Second Keeper.** Time from the Keeper pick to its first blessing: median 103 → 50 s, p90 254 → 112 s, worst 648 → 277 s; runs that never bless it 14 → 8. The worst cases are hands that showed no such card. The bot does not reroll for one.

## Changes during the pass, each from a trace
1. First knob: skill 0.6 died at a median minute 3.0 and 0.3 at 1.6, no-pick at 1.3 (`a_v2_*`). Setting each field back to the top in turn showed reaction dominating: v1's clearance of 18 uu holds only at zero latency, so the bot skimmed bodies it could no longer judge. A player who reacts late leaves room: the clearance now grows as skill falls (`MarginUU`, R-BOT-04). Stick error was the next largest.
2. Skill-0 limits, after two more passes on three arms × 6 seeds: reaction 0.45 → 0.26 s, hold 0.18 → 0.1 s, cue miss 0.5 → 0.4, stick error 30 → 10°, look-ahead 0.3 → 0.4 s, late dash 0.12 → 0.16 s, extra clearance 120 uu. Tuned to a slope with no human target, stopped once the slope stood: these are tool parameters and the numbers only mean "between these settings".
3. Reach also cost a first extra weapon: seed 12 of the Tide arms kept a lone rifle at 21 s and died about 130 s sooner than v1. Early on the crowd is in contact, so a blade pays even when kiting later says otherwise. Below two weapons a new weapon now beats an upgrade (`SecondWeaponFloor`); reach still chooses between weapons. This is why 117 Arc Blades remain.

## What reads wrong
- **Skill 0.3's reaction is 0.18 s, faster than a typical human, and it still loses every run.** At human-like latency the bot died at minutes 1 to 3: its movement is a per-step danger search with no real plan, so it has no way to absorb latency beyond clearance. The skill number is a position on this bot's own scale, not a human percentile.
- **Skill 0.6 is thin at the top:** 7 wins in 270, against 74 for v1 at 0.6, which has the same 0.10 s delay but no cue misses, attention limit or card error. If S2's "bots win a full run" means a mid skill, this knob does not yet say so; at 1.0 it does.
- **The fall from 1.0 to 0.6 is steep and the histogram is two-humped** (minutes 6-8, and 15 at the boss). A real population may look like this or may not; only O-008 can say.
- **Pickup barely moves with skill** (0.71 → 0.67, reference 0.51), so level timing stays an early bound at every skill. Weaker runs die sooner rather than collect worse.
- **Dashes per minute are flat** at about 5.8; a weak player's dash rate should differ. Windows at 0.3 hold only 30 runs.
- **Four deaths in minutes 0-1 at 0.6.** Forge/Hammer seed 2 took 17 hits in 47 s with 7 enemies alive, which looks like a stall in the clearance search, not a weak player. Not traced to the end; a fault in the instrument until shown otherwise.
- **The second-Keeper rule costs some arms at 1.0:** Forge/Hammer:Tide wins 10 → 7, Tide/Whirlpool reach 6 → 2, Tide/Frost wins 8 → 6, while Tide/Whirlpool:Storm reach 5 → 10 and Tide/Stillness:Storm 5 → 11. With 15 seeds a single arm moves by chance too. Blessing Tide sooner hurting Forge fits `S1_balance_pass2.md` §6 (Tide is what loses), but that was not separated from noise here, nor split from the reach rule.
- **Off-archetype Keys remain:** Tide/Whirlpool seed 12 takes the Stillness Key before its own, as v1 does. V2 does not plan toward an unseen Key.
- **Not modelled:** rerolls, banishes, locks, closing to melee range, learning across runs.

## Tool parameters at this reading
Skill 0 (`DTBotSkillTuning`): `MaxReactionS` 0.26, `MaxHoldS` 0.1, `MaxCueMissP` 0.4, `MinAttentionRounds` 10 (80 just below skill 1), `MinThreatRangeUU` 800, `MinHorizonS` 0.4, `MaxStickErrDeg` 10, `MaxExtraMarginUU` 120, `MinImminentS` 0.16, `MaxCastCrowd` 24, `MaxTasteScore` 30. Cards (`DTBotTuning`): `ReachPenalty` 60, `ModReachShare` 0.5, `SecondWeaponFloor` 61, `SecondFirstScore` 94, `SecondNextScore` 65; `StickTauS` 0.5.
