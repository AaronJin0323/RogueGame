# S1 probes: the measurement gaps closed (2026-10-31)

Goal: make the S1 numbers readable. No gameplay rule and no tuned number changed; the only behaviour change is one heuristic of the bot (§5), which is a tool parameter. Everything here is Bot v1 at skill 1.0, headless: a bound, not a human reading (O-008).

| Gap | Result |
|---|---|
| T-KEE-15, `C.INTERCESSION.BAND` | measurable; **FAIL**, 3 of 57 combinations in band; Tide's mean cast is negative |
| T-KEE-18, `C.POWER.KEEPER_SHARE_L16` and package gain | measurable; **FAIL**: share 5 of 9 arms (mean 0.683), gain 0 of 9, archetype band 2 of 9 |
| T-PRE-05, `C.PRE.CONTRAST_MIN` | automated, repeatable, gate step `contrast`; 7 of 7 elements pass in the one weather that exists (§3) |
| `hit_kind` | renamed; no schema version exists to bump (R-TEL-06 pins only the CSV) |
| `Hazard` split | `HazardRing`, `Charge`, `Sweep`, `DashCharge` |
| Ring deaths | a bot fault, fixed: runs with 4+ ring hits that die 7 → 0, wins 153 → 164 of 201 |
| `DT_BUILD_HASH` | already built at S0 (`Dusktracer.Build.cs`); the Open line was stale and is removed |

## 1. The scene
`Bot/DTProbeScript.h` holds a build, a field and a window as text; `Tools/probe.py` writes the scenes and reads them; `Tools/BotRuns.ps1 -Jobs` runs them headless with the director off. `Dusktracer.Bot.ProbeScene` pins the scene, seen failing first with the build not applied; `Tools/tools_test.py` pins the readings.

- **Field:** 120 swarm enemies, refilled, at HP scale 26.8 (10 × the swarm at 10.5 min; 40 × since `S1_keeper_power.md` §3, the route follows a moved crowd since §2, and that page supersedes the readings here). The factor comes from the Handoff's `balance/power_split.md`: at 1× a strong build wipes the field and every ablation reads the same.
- **Hero:** takes no damage and walks one fixed route in the crowd: stand 1 s, walk 1 s on a 400 uu circle, a dash every 6 s. The bot's own movement was tried first and is unusable here: with no danger to weigh it kites out of reach and most windows read exactly zero damage.
- **Limits, all of which move the readings:** swarm only, so no Tank plate and no elites; no hit reaches the hero, so Slam's `HitTaken` trigger should never fire (it did: `S1_keeper_power.md` B2, fixed there, and every reading in this page carries that fault); the route favours neither Stillness nor Slam on purpose but is not how a player moves; HP 10× slows kill effects (Chain forks, shatter, explosions). 6 of 690 ablation runs logged one modifier refused as incompatible (`Mod.ScatterShell`, `Mod.Pierce` on the Pulse Rifle) and ran without it.

## 2. Keeper power
**T-KEE-15** (`Saved/Probes/ik`, 3 seeds, 342 runs, every run with the cast count it should have). Marginal damage = damage in the 12 s after the press, with cast minus no cast; same build, seed and field. Each of the 57 legal combinations (9 × 7 less the 6 of R-KEE-16) holds the variant's four cards, the layer's Key and first Amp, and one loadout: Pulse Rifle L4, Arc Blade L3, Orbital Drones L3, the common steered set at L16. Seed spread is small against the differences, so the failure is not noise.

| Keeper | Mean marginal damage | In ±0.20 of the mean | Range across combinations |
|---|---|---|---|
| Storm | 15 538 | 3 of 19 | −1 341 to 36 684 |
| Forge | 13 983 | 0 of 18 | −14 036 to 33 303 |
| Tide | −4 868 | 0 of 20 (a mean below zero has no band) | −27 285 to 6 039 |

By variant: Furnace 28 000–33 300, Hammer 17 500–24 100, **Slam −14 000 to −4 200**; Downpour mostly 29 900–36 700, Tempest 6 900–9 400, Arc Grid 14 800–18 700 on Tide and none, and Forge layers pull Storm down. **A cast costs damage in 23 of 57 combinations**: all 6 Slam, all 7 Frost, 5 of 6 Stillness, 4 of 7 Whirlpool, and Tempest with a Slam layer. Not traced: the likely mechanism is that a cast moves or freezes the crowd out of the build's reach for the window, but no probe here shows it, and the fixed route and 10× HP both bear on it. Read the sign before any retune: R-KEE-15's "retune toward the mean" has nothing to aim at while Tide's mean is negative.

**T-KEE-18** (`Saved/Probes/ab`; 115 L16 snapshots of the nine arms, default second Keeper, 6 configurations each, 690 runs). Each build is rebuilt per configuration, one group removed. Share = drop without blessings ÷ sum of the drops of blessings, modifiers, weapon levels, extra weapons (the Handoff's definition). Attribution share is `Tools/bot_summary.py` over the same runs.

| Arm | Builds | Full DPS | Ablation share (≥ 0.60) | Attribution share | DPS lost without blessings | Package gain (2.0–2.4) | ±0.15 of 0.683 |
|---|---|---|---|---|---|---|---|
| Forge/Furnace | 14 | 4545 | 0.791 PASS | 0.45 | 0.756 | 0.90 | IN |
| Forge/Hammer | 14 | 4547 | 0.833 PASS | 0.59 | 0.858 | 1.49 | IN |
| Forge/Slam | 15 | 5960 | 0.907 PASS | 0.48 | 0.865 | 1.99 | OUT |
| Storm/ArcGrid | 14 | 3328 | 0.415 FAIL | 0.51 | 0.759 | 2.97 | OUT |
| Storm/Downpour | 15 | 5290 | 0.461 FAIL | 0.59 | 0.829 | 5.10 | OUT |
| Storm/Tempest | 14 | 2025 | 0.417 FAIL | 0.41 | 0.572 | 1.33 | OUT |
| Tide/Frost | 12 | 4041 | 0.839 PASS | 0.39 | 0.756 | 1.03 | OUT |
| Tide/Stillness | 10 | 2140 | 0.522 FAIL | 0.25 | 0.626 | 1.13 | OUT |
| Tide/Whirlpool | 7 | 3976 | 0.966 PASS | 0.34 | 0.770 | 0.85 | OUT |

What this says:
- **Attribution was not the whole story, in both directions.** Ablation lifts Forge and Tide well past 0.60, as `power_split.py` warned; it lowers Arc Grid and Downpour, because their weapon groups also drop hard. Blessings lose 57–87 % of full DPS on every arm, so Keepers do carry the builds; the share is lower on Storm because weapons matter as much there.
- **Package gain is out of band on all nine and three read near or below 1.0** (Furnace 0.90, Whirlpool 0.85, Frost 1.03): removing the archetype's own cards costs that build nothing on this field. Several of those arms keep the other Keeper's cards, so the second Keeper may carry them. Not traced. Whirlpool's reading rests on 7 builds and the survivors of a weak arm.
- The archetype band fails on spread: 0.415 to 0.966.

## 3. Contrast (T-PRE-05)
`Tools/contrast_check.py` on a 1280×720 `-RenderOffScreen` capture: a staged late frame (director at 700 s, Turrets, Rushers, elites, gems, chests, player rounds), hero read from the `arena_empty` shot, where the crowd does not cover it. R-PRE-05 is checked on the rendered frame, so both references are read from it: lit ground Y 0.112, shadow on it Y 0.013. Before the lift (`C.ARENA.SUN_LUX` 10) lit ground read Y 0.098 and the shadow Y 0.014. A telegraph and a round are dual-tone, so each row shows the tone that carries lit ground.

| Element | Tone Y | vs lit ground | vs shadow | Verdict |
|---|---|---|---|---|
| telegraph (band Y 0.000 on lit ground; red Y 0.21 on shadow) | 0.208–0.211 | 3.21–3.23 | 4.11–4.15 | PASS |
| enemy round (core Y 0.002 on lit ground; rim Y 0.37 on shadow) | 0.367–0.368 | 3.14 | 6.65–6.67 | PASS |
| optic and elite light (one line: same hue, the elite blinks) | 0.855 | 5.59 | 14.4 | PASS |
| player tracer core | 0.58–0.61 | 3.91–4.06 | 10.1–10.5 | PASS |
| hero | 0.725–0.734 | 4.79–4.84 | 12.4–12.5 | PASS |
| gem | 0.616–0.617 | 4.11–4.12 | 10.6 | PASS |
| chest | 0.63–0.64 | 4.20–4.27 | 10.8–11.0 | PASS |

The tracer row spans capture spread, not an emissive range: every player core draws at the one `C.BULLETS.EMISSIVE_PLAYER` 4.0 whatever the bullet count, because R-PRE-06's scale-down with live player bullets is not built, so a dimmed core has no reading yet and this row is redone when it lands.

Two captures on the final map and constants. The floor moved first: the band needs lit Y 0.10, and `C.ARENA.SUN_LUX` 10.5 reached Y 0.106. That held the round at 3.03–3.04, one quantised core step (Y 0.003, 2.94) under the rule, so the lift went to 11. The round then took `C.ENEMY_FIRE.DRAW_RADIUS_RATIO` 1.8: the core reads Y 0.001–0.002 on 6 captures, against 0.002–0.003 at 1.5 on 4. Rim emissive was tried and dropped: 3.5, 6 and 10 give rim Y 0.34, 0.37 and 0.39, and past that the tonemapper only pales the rim. The rim alone reads 2.6 against lit ground; the rule and the token table let the core carry that ground.

Judged against the tokens rather than the frame, `Pal.Ground.Shadow` passes everywhere, and `Pal.Ground.Bright` (Y 0.193, shown by no pixel of this frame) still reads under 3 for gem 2.7, tracer core 2.6–2.7 and chest 2.8. One weather exists, so the second is not measured. The elite light has no reading of its own.

## 4. Hit kinds
`hero_damaged` now carries `hit_kind`, values `Contact Bullet HazardRing Charge Sweep DashCharge`; a Rusher's charge is `Charge` with role `Rusher`. Tests `Dusktracer.Boss.HitKinds` and `Dusktracer.Telemetry.Records.Combat` seen failing first. `Tools/pressure.py` reads old runs too, splitting an old `Hazard` by `attack`. `bot_summary.py` keeps the combined column as "Direct attacks" and adds one per kind. `offer_pick` had the same fault, repeating `kind` for the card kind; it is now `card_kind` (`offers.md`), and `first_wins` stays for old runs.

Damage to the hero during boss fights, 201 fights:

| Kind | Before: hits, HP per 100 s | After: hits, HP per 100 s |
|---|---|---|
| Bullet | 145, 13.2 | 166, 14.8 |
| Sweep | 51, 5.6 | 51, 5.5 |
| HazardRing | **90, 4.4** | **14, 0.7** |
| DashCharge | 27, 3.6 | 26, 3.4 |
| Contact | 35, 3.1 | 31, 2.7 |
| Charge | 4, 0.5 | 4, 0.5 |

## 5. The ring deaths
`S1_boss_after_fields.md` §3 counted 4 of 12 deaths in the six arms as ring deaths with 8–11 hits. This reading finds **three** with 8 or more: Forge/Furnace seed 2 (10), Storm/Tempest:Forge seeds 1 (11) and 7 (8). The fourth was not found in `after_bot`. Across all 19 arms 7 deaths have 4+ ring hits. The three replayed with identical input hashes.

**Diagnosis, written before the fix: a bot fault.** The hero stands 400–1100 uu outside a ring that shrinks about 60 uu/s while it walks 600 uu/s, so walking in is always possible. The trace shows the way in carrying round threat of 400–5 800 against a ring grade worth about 150 for the same move, and 300–500 rounds in the air. `ScoreMove` therefore sidesteps rounds outside the ring until dead. A round costs one hit and grants i-frames; the ring costs 8 HP every 0.5 s until crossed. No rule or number is at fault.

**Fix:** the grade outside a live ring is one `HazardWeight` step per 60 uu (`RingGradeUU`, was per ring radius). `Dusktracer.Bot.HazardRingThroughRounds`, seen failing first. Storm/Tempest:Forge seed 7 still dies at 996 s, to Ring and CrossRing rounds, one ring hit. No run now dies with 4+ ring hits. Hits taken from rounds rise a little (145 → 166) because the bot crosses them.

Before = `Saved/BotRuns/after_bot`, after = `Saved/BotRuns/after_probes`. Reached boss / wins / median end s / damage share per min in 600–780 s / Keeper share at L16 (runs).

| Arm | Before | After |
|---|---|---|
| None | 0 / 0 / 589 / 0.25 / 0.00 (0) | 0 / 0 / 589 / 0.25 / 0.00 (0) |
| Forge-Furnace | 13 / 9 / 986 / 0.06 / 0.45 (14) | 13 / 11 / 986 / 0.06 / 0.45 (14) |
| Forge-Furnace-Tide | 8 / 3 / 920 / 0.13 / 0.37 (15) | 8 / 3 / 920 / 0.13 / 0.37 (15) |
| Forge-Hammer | 14 / 13 / 976 / 0.02 / 0.59 (14) | 14 / 13 / 978 / 0.02 / 0.59 (14) |
| Forge-Hammer-Tide | 13 / 5 / 968 / 0.05 / 0.59 (15) | 13 / 7 / 974 / 0.05 / 0.59 (15) |
| Forge-Slam | 14 / 13 / 992 / 0.04 / 0.48 (15) | 14 / 12 / 994 / 0.04 / 0.48 (15) |
| Forge-Slam-Tide | 10 / 6 / 945 / 0.09 / 0.44 (13) | 10 / 7 / 950 / 0.09 / 0.44 (13) |
| Storm-ArcGrid | 13 / 10 / 974 / 0.03 / 0.51 (14) | 13 / 12 / 970 / 0.03 / 0.51 (14) |
| Storm-ArcGrid-Forge | 15 / 12 / 987 / 0.04 / 0.58 (15) | 15 / 13 / 987 / 0.04 / 0.58 (15) |
| Storm-Downpour | 15 / 14 / 994 / 0.01 / 0.59 (15) | 15 / 15 / 994 / 0.01 / 0.59 (15) |
| Storm-Downpour-Forge | 15 / 15 / 972 / 0.01 / 0.61 (15) | 15 / 15 / 965 / 0.01 / 0.61 (15) |
| Storm-Tempest | 13 / 10 / 984 / 0.03 / 0.41 (14) | 13 / 10 / 989 / 0.03 / 0.41 (14) |
| Storm-Tempest-Forge | 15 / 12 / 988 / 0.02 / 0.42 (15) | 15 / 13 / 990 / 0.02 / 0.42 (15) |
| Tide-Frost | 11 / 9 / 956 / 0.11 / 0.39 (12) | 11 / 9 / 956 / 0.11 / 0.39 (12) |
| Tide-Frost-Storm | 11 / 10 / 954 / 0.03 / 0.37 (12) | 11 / 10 / 954 / 0.03 / 0.37 (12) |
| Tide-Stillness | 7 / 4 / 875 / 0.03 / 0.25 (10) | 7 / 5 / 875 / 0.03 / 0.25 (10) |
| Tide-Stillness-Storm | 6 / 6 / 564 / 0.02 / 0.23 (6) | 6 / 5 / 564 / 0.02 / 0.23 (6) |
| Tide-Whirlpool | 4 / 0 / 580 / 0.06 / 0.34 (7) | 4 / 1 / 580 / 0.06 / 0.34 (7) |
| Tide-Whirlpool-Storm | 4 / 2 / 556 / 0.04 / 0.27 (7) | 4 / 3 / 556 / 0.04 / 0.27 (7) |

Steered arms: reached boss 201 → 201, wins 153 → 164. Only the boss phase moved, as designed. T-DIR-01 PASS 588.6 s, unchanged. T-OFF-15 FAIL 200/201, unchanged (Tide/Stillness:Storm seed 6). `Tools/pressure.py --check` FAIL 0/19, unchanged. The Warden now kills the bot even less; `boss.md` Open stands.

## 6. Build hash
`run_start.build` and the CSV `build` column already carry the short hash, with `+dirty` when the tree had local changes at compile time. The rules file re-runs on a commit, so the stamp follows the build, not the checkout: a run on a binary built before a commit shows the hash it was built from.

## 7. What remains
1. Why a cast loses damage on Slam and most of Tide, and whether the 12 s window or the route causes any of it.
2. Package gain near 1.0 on Furnace, Frost and Whirlpool: a run of the same ablation with no second Keeper would separate the cause.
3. Contrast in the second weather, an elite light reading of its own, gem, tracer core and chest against a floor at `Pal.Ground.Bright`.
4. Both probes are readings, not gate steps: they fail, and nothing should be tuned against a bot field alone. They can join the nightly set of S2.
5. No probe covers a Tank-heavy field or the boss.
