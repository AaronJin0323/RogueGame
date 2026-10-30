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
