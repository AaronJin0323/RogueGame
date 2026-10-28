# Playtest-5 fixes: bot verification on main 24ff996 (2026-10-20)

Clean run on one build (24ff996, unity + non-unity check), Bot v2 `steered`, `-nullrhi`, time scale 4, cut at 19:00.
Six Patrons per Heat (seeds 7200–7205 at Heat 0, 7230–7235 at Heat 3), plus one Lifeline-forced Heat 0 run (seed 7300,
Forge; `RH.Give Mod.Lifeline all` at 6:00). Raw data: `Saved/Playtests/Balance/v6*`, logs `Saved/Logs/Balance_v6*`.
Summaries: `python -I Tools/Balance/pressure.py v6h0` / `v6h3`.

## Deaths before 20:00

| Heat | Died | When | Cause (damage by source, last 10 s, where logged) |
|---|---|---|---|
| 0 | 2 / 6 | 12:10, 12:23 | contact, Tanks (58), with 26 Tanks alive; the second one is not logged |
| 3 | 4 / 6 | 12:35, 12:39, 15:25, 16:05 | contact, Tanks (20) in one run; pile-ups in two others: 42 Coursers + 21 Breakers alive at 12:30, and 130 Coursers + 63 Breakers at 15:30 |

Five of the six deaths fall at 12:00–12:40, where the Heat 0 late Pylon fan starts and the 12:30 Threat block opens. The
builds of the runs that died are no weaker at minute 11 than those of the runs that survived (L13–17, 2–3 weapons).

## Bullets, dashes, damage

- **Peak enemy bullets aimed at the hero:** 3–14 per s (median ~8) at every minute and both Heats. Before the aim tokens
  the estimate was ~60 per s, so D1 + D3 hold. Enemy bullets spawned run 400–980 per min at minutes 14–18, most of them
  not aimed at the hero.
- **Dashes / damage per min (median):**

  | Heat | Minute 10 dashes | Minute 10 damage | Minute 18 dashes | Minute 18 damage | §3.5 band (m10 / m18) |
  |---|---|---|---|---|---|
  | 0 | 4.1 | 0 % | 6.1 | 0 % | dashes 4–8 / 6–10, damage 15–30 / 30–50 |
  | 3 | 7.1 | 4.8 % | 12.2 | 18 % | — |

  The bot dodges telegraphs that a human misses, so its damage stays under the band. The deaths are sudden spikes from a
  pile-up, not a rising damage trend.

## Lifeline (L1 / L2, weapons 23d53a6)

Raw heal produced: `HealQueued`, sampled by `RH.Stats`. Two Lifeline weapons from 6:00 (Pulse Rifle, Scatter Cannon):

| Window | Raw heal | Cap at 100 max HP, 2 Lifeline weapons | Healed (≈ C saturated) |
|---|---|---|---|
| 6:01–10:00 | 4.5 HP/s | 2.4 HP/s | ~2.1 HP/s |
| 10:00–14:00 | 11.5 HP/s | 2.4 HP/s | ~2.4 HP/s |
| 14:00–18:00 | 13.1 HP/s | 2.4 HP/s | ~2.4 HP/s |

The cap works as built. It applies **per active Lifeline weapon**, though, so two weapons heal ~144 % max HP per minute and
three heal ~216 %, against 30–50 % per minute incoming at minute 18 (§3.5). The run took 5–6 % per minute and never
dropped. L1 as proposed meant one total cap of 1.2 % per s across all Lifeline weapons (≈ 72 % per minute).

## Proposals (not changed)

1. **Lifeline:** one total cap, `C = 1.2 % max HP / s` regardless of how many weapons carry Lifeline (weapons,
   `URHBulletSubsystem::CapLifelineStep`: drop the `× Active`). One Lifeline weapon is unchanged; stacking stops scaling.
2. **Pile-ups (HP wall, §3.5 at any Heat):**
   - extend the 12-alive soft cap on regular Bulwarks from Heat ≥ 3 to every Heat;
   - add `MaxAliveShare` caps for Coursers (≈ 0.10, 12–30 alive) and Breakers (≈ 0.05, 6–15 alive), in the same way as
     Tanks and Pylons.

   Owner: statuses (`RHEnemyDef` / `RHSpawnDirector`). Verify with the same 6 + 6 set; the target is no death at 12:00–12:40
   in more than 1 of 6 at Heat 0.

## Re-verification on main bd54f9f (single Lifeline cap 7c54df5, pile-up caps f2ed20d): **PASS**

The same set: one build (HEAD = bd54f9f, the non-unity check passes), seeds 7400–7405 at Heat 0, 7430–7435 at Heat 3, and Lifeline run 7500.

| Check | Result | Gate |
|---|---|---|
| Heat 0 deaths before 20:00 | **0 / 6** (none in 12:00–12:40) | ≤ 1 of 6 in 12:00–12:40 ✔ |
| Heat 3 deaths before 20:00 | 3 / 6: 10:32 (Vampiric drains, 3 elites), 12:23 (contact, Tanks, at the cap of 12), 14:00 (not logged) | — (was 4 / 6, no pile-ups now) |
| Peak aimed bullets | max 9–14 per s, median 3–9 | — |
| Dashes per min, median (Heat 0) | 7.1 at m10, 8.1 at m18 | band 4–8 / 6–10 ✔ |
| Lifeline raw heal (2 weapons) | 3.4 / 8.2 / 8.4 HP/s over 6–10 / 10–14 / 14–18 min | — |
| Lifeline healed | ≤ C = 1.2 % max HP / s in total, i.e. ≤ 72 % per min (saturated: raw ≫ C) | ≤ ~72 % per min ✔ |

`HealGranted` is not logged per minute, and adding it would have changed the build under test, so the healed figure is
the cap bound, which the raw heal saturates. Damage taken by the bot stays under the §3.5 band, as before (it dodges what a
human misses).
