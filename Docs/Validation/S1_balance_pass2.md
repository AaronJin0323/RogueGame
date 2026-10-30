# S1 balance pass 2 — pressure by build (2026-10-30)

Goal, in priority order: each arm individually near `C.DMG_TAKEN.BAND_MIN10_13` in 600–780 s; steered arms reach the boss commonly; Keeper share at L16, archetype and Intercession bands; the XP share-cap gap. Order: instrument → scored test → diagnosis (this section, written before any number changes) → TUNE → re-measure (`H06 §3`, `H04 §3`). Bot v1 at skill 1.0, headless, seeds as in `S1_bot_baseline.md`; 10 seeds no-pick, 5 seeds on all nine archetypes. `Tools/BotRuns.ps1 -Parallel` runs the batteries in about a minute and reads identically to serial runs (T-DIR-01 median 592.9 s, as in pass 1).

## 1. Instrument and scored test
- `Tools/pressure.py --check` now judges **per arm**, and the overall verdict passes only when every arm passes on its own. The pooled mean of pass 1 passed because 0.01 and 2.2 average into the band.
- `Tools/pressure.py` prints damage taken by hit kind and enemy role (`SOURCES:`); `Tools/bot_summary.py` tabulates per arm: contact, rounds, hazard, by role, threat actions per minute and kill horizon in the window, boss reach, Keeper share at L16 (`sources` records up to the level-up), damage per cast.

## 2. Baseline after Part A (no number tuned yet)
Part A expressed the Keeper parameters of `keepers.md` Open, so these differ from pass 1 for the arms those cards reach. Whirlpool's crush makes its Key a Keeper damage source.

| Arm (5 seeds; None 10) | Reached boss | Wins | Median end s | Dmg share/min | Dashes/min | Contact | Rounds | Threat/min | Horizon uu | Keeper share L16 | L16 runs |
|---|---|---|---|---|---|---|---|---|---|---|---|
| None | 1/10 | 0 | 592.9 | 0.223 (n=4) | 10.25 | 0.166 | 0.057 | 141 | 502 | — | 0 |
| Storm/Tempest | 0/5 | 0 | 730.0 | 0.341 (n=4) | 7.99 | 0.335 | 0.006 | 35.6 | 546 | 0.397 | 4 |
| Storm/ArcGrid | 0/5 | 0 | 525.3 | 0.370 (n=1) | 5.36 | 0.370 | 0 | 24.5 | 712 | 0.441 | 1 |
| Storm/Downpour | 1/5 | 0 | 392.4 | 0.029 (n=1) | 8.00 | 0 | 0.029 | 43.4 | 609 | 0.437 | 1 |
| Forge/Hammer | 5/5 | 4 | 952.6 | 0.000 (n=5) | 4.60 | 0 | 0 | 36.7 | 572 | 0.652 | 5 |
| Forge/Furnace | 5/5 | 4 | 976.8 | 0.051 (n=5) | 7.13 | 0.076 | 0.008 | 52.6 | 441 | 0.399 | 5 |
| Forge/Slam | 5/5 | 3 | 945.6 | 0.014 (n=5) | 5.93 | 0.070 | 0 | 40.4 | 554 | 0.450 | 5 |
| Tide/Frost | 1/5 | 1 | 478.2 | 0.455 (n=2) | 6.03 | 0.910 | 0 | 38.0 | 441 | 0.263 | 2 |
| Tide/Whirlpool | 0/5 | 0 | 99.8 | — (n=0) | — | — | — | — | — | — | 0 |
| Tide/Stillness | 0/5 | 0 | 766.1 | 0.278 (n=3) | 7.44 | 0.268 | 0.010 | 41.1 | 515 | 0.314 | 3 |

T-DIR-01 PASS (592.9 s); T-OFF-15 PASS (17/17 at the boss, mean 29.1 decisions); T-DIR-02 per arm: no arm inside both bands.

Spawned by role in 400–780 s, Forge/Hammer (mean): Swarm 1542, Tank 123, Rusher 202, Turret 18. The timeline asks for about 205 Tanks and 155 Turrets in that span.

## 3. Diagnosis (by source, from telemetry)
1. **The contact-versus-rounds split is contact everywhere.** Rounds never exceed 0.06/min; every arm that takes damage takes it from bodies, and Tank is the largest single source in Storm, Frost and Stillness (`Contact/Tank` is the killing blow in most Steered deaths). Bot deaths are 6–8 hits, 15–27 HP each at minute ten and later.
2. **Melee ignores the Tank's plate (BUG, weapons layer).** R-ENM-10 halves a hit whose contact point lies in the front arc. `FDTWeaponSim::EmitArc` writes the enemy's centre as the hit position, so an arc has no contact point and always deals full damage to a Tank's face; rounds report their surface point and take the plate. Forge kills Tanks at twice Storm's rate for this reason alone, and Tank is what kills Storm.
3. **The share caps starve exactly the clearing builds of ranged threats (director layer).** `IsAtShareCap` compares a role with the alive count; a build that clears the field holds few alive, so 0.08 of it is one Turret. Forge sees 18 of about 155 Turrets the timeline asks for, 60 % of its Tanks and 90 % of its Rushers. Turret is the role that reaches past the kill horizon by itself (R-DIR-06 keeps it on the threat ring), so its absence is why Forge reads 0 in rounds. A build with a full field is not touched: its alive share follows the mix weight, below the cap.
4. **The kill horizon itself is not the gap.** Threat actions within the ring read 25–53/min against the floor of `C.DIR.THREAT_ACTIONS_MIN` 4, and the horizon hardly differs by build (441–712 uu). Drop-ins do arrive; the bot at skill 1.0 reads their telegraphs and walks out, which is the skill check working. Forge takes 0.00 because nothing that reaches it can survive to touch it, not because no check is offered. A human will fail more of them, so this is a bound.
5. **Pull is an enemy speed boost at Key level.** Whirlpool dies at about 100 s (`Contact/Swarm`, `Contact/Tank`, 16 alive): every impact moves all enemies within 200 uu by 120 uu toward a point next to the hero, and a rifle plus drones land 5+ impacts per second. No-pick lives 490 s longer.
6. **Keeper share is short because the Keeper terms are small next to the weapons.** Share = K / (K + W); 0.60 needs K / W = 1.5. Tempest's key gives 3 × 0.45 × 0.25 = 0.34, its Amps and tiers about 1.0. Storm and Tide arms also die before their tiers matter. Frost's Amp1, Amp2 and Trait and four Stillness cards name parameters no system reads (`keepers.md` Open), so those arms have fewer Keeper cards in effect than their picks show. Forge's cleaves count as Keeper damage (D-015), which is why Hammer alone passes.
7. **Damage per cast is not comparable across Keepers.** The `intercession` record sums only the rings and nets of `FDTKeeperSim`: Storm reads 0 (its cast reshapes flags) and Tide reads ~5 (pull and Freeze). Within Forge the reading follows the crowd at cast time (Hammer casts 3.4 times a run on a cleared field). `C.INTERCESSION.BAND` needs the marginal probe of T-KEE-15 (PLANNED); this pass cannot judge it.
8. **Band against survival: S1 has no heal.** The hero has 100 HP and nothing restores it (`Reserve.Restore` appears only on an exhausted pool, 0 Reserve cards in these runs). 0.30–0.50/min over 180 s is 90–150 % of max HP, so an arm inside the band for the whole window dies inside it or reaches the boss almost dead, and every Steered death above is the same 6–8 hits arriving sooner or later. The band comes from a game with Lifeline and regen. Targets 1 and 2 of this pass cannot both be met exactly by any number; this pass aims the strong arms at the band's lower edge and the weak arms at reaching the boss.

## 4. Decision: share caps (gap 4)
The caps are the right place, not the timeline. Reweighting the timeline toward Swarm would make the model match by removing threats; raising a cap restores the mix the timeline already asks for only where the field is small. The caps exist to stop pile-ups (`playtest5_verification.md`), and in a full field the alive share sits at the mix weight, so a higher cap adds no pile-up there.

## 5. Changes
Round 1 (BUG first, then TUNE; one `number_change_log.csv` row each; reverted if not met):

| Change | From | To | Hypothesis |
|---|---|---|---|
| BUG: arcs report the contact point | centre | surface toward the origin | Forge arms take Tank contact: damage taken in the window rises from 0.00–0.05 toward 0.10+; Storm unchanged |
| `C.DIR.SHARE_MAX_TURRET` | 0.08 | 0.25 | Forge sees most Turrets the timeline asks for (18 → 80+ in 400–780 s) and non-zero rounds damage; T-DIR-01 stays in band; T-OFF-15 stays 28 ± 3 |
| `C.DIR.SHARE_MAX_TANK` | 0.10 | 0.15 | Forge sees about the timeline's Tanks (123 → 170+); spawned XP / model rises from 0.79 toward 0.85 |
| Whirlpool `distance_uu` (Key) | 120 | 40 | Whirlpool survives past no-pick's weakest seed (> 300 s median) and reaches L8 in most runs; Amp `+30` and Stillness's own Pull stay |
| Tempest `chance` (Key) | 0.25 | 0.40 | Keeper share at L16 rises from 0.40 to ≥ 0.50 and more Tempest runs reach the boss (0/5 → ≥ 2/5) |
| Arc Grid `dmg` (Key) | 0.60 | 0.85 | Keeper share at L16 rises to ≥ 0.50; median end rises from 525 s to ≥ 700 s |
| Downpour `dmg` (Key) | 0.50 | 0.80 | Keeper share at L16 rises to ≥ 0.50; median end rises from 392 s to ≥ 600 s |
| Frost `frozen_vuln` (Key) | 0.30 | 0.60 | Keeper share at L16 rises from 0.26 to ≥ 0.40 |
| Stillness `mult` (Key) | 2.0 | 2.5 | Keeper share at L16 rises from 0.31 to ≥ 0.40 |

Round 2 and round 3 were written after the round before them was measured; each hypothesis line is in `number_change_log.csv` with its verdict.

| Round | Change | From → to | Hypothesis |
|---|---|---|---|
| 2 | `C.WEAPON.CONVERT_MELEE_DAMAGE_RATIO` | 0.8 → 0.55 | Forge arms take ≥ 0.15/min in the window and still reach the boss in ≥ 3/5 |
| 3 | `timeline_arena1.csv` `dropin_interval_s` and drop-in mix at 600 / 675 / 720 s | 10 / 7 / 7, Rusher 0.70 Turret 0.30 → 6 / 5 / 5, 0.50 / 0.50 | the kill-horizon duty inside the band window raises strong arms to ≥ 0.10/min at skill 1.0 and ≥ 0.25 at skill 0.5 while they reach the boss in ≥ 9/15; T-DIR-01 and T-OFF-15 hold |

## 6. Results
Five seeds read too noisily to judge a single change (one seed moves Tempest's median by 200 s), so from round 2 each verdict uses seeds 1..15 of the same arms; the runs are deterministic, so the control is exact.

| Round | Change | Reading | Verdict |
|---|---|---|---|
| 0 | BUG: arcs report the contact point | Forge 0.00–0.05 → 0.02–0.04/min (5 seeds); Tank contact appears on Slam | kept: behaviour = R-ENM-10 (the 0.10+ guess was not reached; a BUG fix is not reverted for that) |
| 1 | `C.DIR.SHARE_MAX_TURRET` 0.25 | T-DIR-01 FAIL 464.9 s, Turret rounds now the top no-pick source; Tempest 749 → 556 s, Frost 847 → 464 s; Forge 0.03–0.09 | **not met**, reverted |
| 1 | `C.DIR.SHARE_MAX_TANK` 0.15 | as above, one reading with the Turret cap | **not met**, reverted |
| 1 | Whirlpool `distance_uu` 40 | median 140 s, L8 0/5 | **not met**, reverted |
| 2 | Tempest `chance` 0.40 | L16 share 0.43, boss 0/15, median 637 → 534 s | **not met**, reverted |
| 2 | Arc Grid `dmg` 0.85 | L16 share 0.56 (met), median 457 → 482 s (700 needed), boss 1/15 | **not met**, reverted |
| 2 | Downpour `dmg` 0.80 | L16 share 0.37, median 509 → 504 s | **not met**, reverted |
| 2 | Frost `frozen_vuln` 0.60 | L16 share 0.17 | **not met**, reverted |
| 2 | Stillness `mult` 2.5 | L16 share 0.19 | **not met**, reverted |
| 2 | `C.WEAPON.CONVERT_MELEE_DAMAGE_RATIO` 0.55 | Forge 0.03–0.04/min, boss 12–13/15 | **not met**, reverted |
| 3 | band-window drop-ins | Furnace 0.09 → 0.22, Hammer 0.09 → 0.10, Slam 0.03 → 0.06; boss Furnace 9 → 6/15, Hammer 13 → 11, Slam 13 → 12; skill 0.5 0.17–0.47; T-DIR-01 PASS 592.9 s | **not met** (Slam, Furnace's boss reach, skill 0.5), reverted |

Probes (no number kept; bug-fix tree, 15 seeds):

| Probe | Reading | What it shows |
|---|---|---|
| Second Keeper | Storm/Tempest:Forge 14/15 at the boss, 9 wins; Storm/ArcGrid:Forge 15/15; Storm/Downpour:Forge 13/15. Forge/Hammer:Tide 1/15; Forge/Furnace:Tide 0/15; Tide/Frost:Storm 2/15 | **the split is not Storm against Forge. It is Tide as a Keeper.** `FDTBotConfig::DefaultSecond` gives Storm Tide and Forge Storm, and any build that takes Tide falls from 13–15 of 15 to 0–2 |
| Skill 0.5 | Forge 0.14–0.17/min, Storm/Tempest 0.47, boss 3–5/15 for Forge | the strong arms' 0.0x is mostly the tool's perfect dodging: a noisier bot sits near the band's lower edge and stops winning |
| Whirlpool `distance_uu` 1 | median 296 s, L8 in most runs | Pull at full distance costs about 190 s; Tide's Whirlpool is still 300 s short of no-pick without it |

The final tree holds **no tuned number**: every number equals the end of pass 1 except the three GATED Keeper starting points Part A needed (`crush_dmg`, `pulled_vuln_s`, `explode_dmg`).

## 7. Before and after, per arm
Before = baseline of §2; after = final tree, same seeds. Arms with `:Forge` are the probe arms with the second Keeper changed, added to the batteries after the probe (before column: not run).

| Arm (5 seeds; None 10) | Reached boss | Wins | Median end s | Dmg share/min | Dashes/min | Keeper share L16 (runs) |
|---|---|---|---|---|---|---|
| None | 1/10 → 1/10 | 0 → 0 | 592.9 → 592.9 | 0.223 → 0.223 | 10.25 → 10.25 | — |
| Storm/Tempest | 0/5 → 1/5 | 0 → 0 | 730 → 749 | 0.34 → 0.22 | 7.99 → 6.60 | 0.40 (4) → 0.43 (3) |
| Storm/ArcGrid | 0/5 → 0/5 | 0 → 0 | 525 → 481 | 0.37 → 0.17 (n=1) | 5.36 → 6.00 | 0.44 (1) → 0.46 (1) |
| Storm/Downpour | 1/5 → 1/5 | 0 → 1 | 392 → 398 | 0.03 → 0.11 (n=1) | 8.00 → 7.33 | 0.44 (1) → 0.44 (1) |
| Forge/Hammer | 5/5 → 5/5 | 4 → 4 | 953 → 956 | 0.00 → 0.02 | 4.60 → 5.53 | 0.65 (5) → 0.59 (5) |
| Forge/Furnace | 5/5 → 5/5 | 4 → 3 | 977 → 995 | 0.05 → 0.02 | 7.13 → 6.53 | 0.40 (5) → 0.41 (5) |
| Forge/Slam | 5/5 → 5/5 | 3 → 0 | 946 → 949 | 0.01 → 0.04 | 5.93 → 6.60 | 0.45 (5) → 0.44 (5) |
| Tide/Frost | 1/5 → 2/5 | 1 → 0 | 478 → 847 | 0.46 → 0.12 | 6.03 → 6.29 | 0.26 (2) → 0.26 (3) |
| Tide/Whirlpool | 0/5 → 0/5 | 0 → 0 | 100 → 89 | — | — | — |
| Tide/Stillness | 0/5 → 0/5 | 0 → 0 | 766 → 724 | 0.28 → 0.34 | 7.44 → 3.58 | 0.31 (3) → 0.32 (3) |
| Storm/Tempest:Forge | 5/5 | 4 | 993 | 0.01 | — | 0.36 (5) |
| Storm/ArcGrid:Forge | 5/5 | 3 | 970 | 0.02 | — | 0.59 (5) |

15 seeds, final tree (boss / wins / median / dmg / L16 share): Tempest 1/0/637/0.33/0.44; ArcGrid 1/1/457/0.28/0.53; Downpour 1/1/509/0.18/0.33; Hammer 13/7/956/0.09/0.59; Furnace 9/6/927/0.09/0.43; Slam 13/2/953/0.03/0.43; Frost 4/0/552/0.08/0.24; Stillness 2/1/727/0.25/0.24; Whirlpool 0/0/110/—/—.

Checks on the brief's seeds: T-DIR-01 PASS (592.9 s); T-OFF-15 PASS (29/29, mean 28.7). Seeds 1..15 read T-OFF-15 72/73 (one Steered run at 32 decisions): the 5-seed check holds, the margin is thin. `Tools/pressure.py --check` on the final batteries: FAIL, 0/10 arms inside both bands (Tempest and Stillness sit inside the damage band in some batteries, never both bands on 15 seeds).

## 8. What remains
1. **Tide adds no power (largest gap, content layer).** Every arm that takes Tide reaches the boss at 0–2 of 15; every arm without it 13–15. `keepers.md` Open lists the parameters no system reads, and Frost and Stillness hold most of them. No number can lift a card that does nothing (round 2). Next: express Frost's shatter and `frozen_blocks_enemies`, Whirlpool's cluster freeze, Stillness's charged effects, then re-run the `:Tide` arms.
2. **Band against survival.** With 100 HP and no heal in S1, 0.30–0.50/min for 180 s costs 90–150 % of max HP (§3 item 8). The band and "reaches the boss commonly" cannot both hold; a heal source is a SYSTEM candidate (`H04 §4`), not built here. Until then the strong arms at skill 0.5 (0.14–0.17/min, boss 3–5/15) are the nearest thing to a human reading; O-008 decides.
3. **Pull at Key level.** Whirlpool dies by 110 s. `distance_uu` alone does not fix it (round 1); the cause is that Pull moves every enemy near an impact toward a point next to the hero, 5+ times a second. A per-enemy pull interval or no pull toward the hero is a RULE change for the scheduler to decide.
4. **Share caps (gap 4).** Decided in §4 that the caps, not the timeline, are the right place, but raising them failed T-DIR-01: the same small field that starves Forge of Turrets is what no-pick and Storm have at 300–500 s. Spawned XP stays about 0.79 of the model. What the data supports next is a cap on a floor of alive enemies, a RULE change, not a number.
5. **Kill horizon.** Threat actions are already 25–53/min in the window. The director's duty is met; raising it (round 3) moved Furnace and little else and cost boss reach.
6. **Keeper share at L16:** Hammer and Arc Grid:Forge sit near 0.59; the rest 0.26–0.46, so `C.POWER.KEEPER_SHARE_L16` is unmet in most arms, and Hammer is outside `C.POWER.ARCHETYPE_BAND` of the arm mean. Attribution undercounts multipliers (`power_split.py`); the ablation probe of T-KEE-18 is still PLANNED.
7. **`C.INTERCESSION.BAND` is not measurable** from the `intercession` record (§3 item 7); T-KEE-15 needs its probe.
8. **Bot pickup** stays 0.72–0.77 of the model against the reference 0.51: no XP number was tuned against it.
