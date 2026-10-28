# Balance model v0.1 (Roadmap W6 Wed)

Analysis of `main @ f31c07c` against GDD §3, §3.5, §5.2, §6, §7. Nothing in the game was changed; every number change
is a proposal in [`recommendations.md`](recommendations.md), routed to the owning area.

| File | What |
|---|---|
| [`recommendations.md`](recommendations.md) | **Start here.** Concrete number changes (file / asset / field / owner), ordered by impact on the §3.5 feel |
| [`weapons.md`](weapons.md) · [`weapons.csv`](weapons.csv) | DPS table, 10 weapons × L1–8, single-target vs crowd; swarm hits-to-kill by minute |
| [`swarm_oneshot.csv`](swarm_oneshot.csv) | Last minute each weapon/level one-shots a Scrapwing (base and steered stats) |
| [`model.md`](model.md) · [`enemy_ehp.csv`](enemy_ehp.csv) | Enemy HP / EHP / contact damage per role × variant (normal, front plate, elite, Armored) × minute |
| [`model.md`](model.md) · [`xp_curve.csv`](xp_curve.csv) · [`xp_table_proposal.csv`](xp_table_proposal.csv) | XP income and level by minute vs the GDD L8 5:30 / L16 12:30 targets, and the fitted replacement curve |
| [`model.md`](model.md) · [`ttk_pressure.csv`](ttk_pressure.csv) | Swarm TTK and clear ratio by minute for a steered, a scattered and an unbuilt build |
| [`sims.md`](sims.md) · [`sim_runs.csv`](sim_runs.csv) · [`sim_summary.csv`](sim_summary.csv) · [`death_histogram.csv`](death_histogram.csv) | The empirical runs (RH.Stats per minute) and the death-by-minute histogram |

## Tools

```
python -I Tools/Balance/balance_model.py      # analytic tables (weapons, EHP, XP, TTK) from the constants in the script
powershell -ExecutionPolicy Bypass -File Tools/Balance/RunSims.ps1 -Label god_random -Seeds 201 -Pick random -God
python -I Tools/Balance/parse_runs.py         # Saved/Playtests/Balance/*.csv -> sim_runs / sim_summary / death_histogram
```

`balance_model.py` copies its constants from the C++ factory fallbacks (`Combat/RHWeaponDef.cpp`,
`Enemies/RHEnemyDef.cpp`, `Enemies/RHEliteDef.cpp`, `Enemies/RHSpawnTimeline.cpp`, `Build/RHRunTuning.cpp`,
`Build/RHBlessingDef.cpp`); each block names its source. There are no `DA_` assets on main yet, so these are the live
numbers. When a `DA_` asset lands, update the constant and re-run. The XP proposal reads `sim_summary.csv` to calibrate
the pickup rate, so run `parse_runs.py` first.

`RunSims.ps1` launches `-game -nullrhi -LiveCoding=false`, one instance at a time, with `RH.Offer.AutoPick`,
`RH.TimeScale 4` and optionally `RH.God 1` (all via `RH.Later 2`, because the game mode restarts the run on frame 1),
waits for the run-end CSV (or sim minute 20), and closes the game by its own PID.

## Method and assumptions (what the numbers mean)

- **Single-target** DPS: one 40 uu target at 300 uu; only bullets whose geometry reaches it count.
- **Crowd** DPS: a harvest crowd at 1 enemy / 100 × 100 uu (1.5× that for DensestCluster / DensestLine targeting).
  Pierce, AoE, sweep and pull multiply hits; AoE / pierce numbers are linear in this density.
- **Swarm TTK** = RH.Stats' definition: first hit → death of a Scrapwing. One hit kills → 0 s. Otherwise the re-hit
  interval × (hits − 1). Bolts or pellets of the same volley that hit the same target count as one re-hit.
  The build's TTK is the kill-share-weighted median over its weapons (RH.Stats reports the median of all swarm kills).
- **Clear ratio** = the build's crowd damage onto distinct HP (overkill capped) ÷ the arriving regular-spawn HP per
  second. < 1 means the crowd accumulates up to the cap: the build is being overrun.
- **Builds** (pick scripts in `balance_model.py`): *steered* — Storm patron, Rail Lance first, Overcharge ×3 +
  Static Focus → Tier 2 at favor 6; *scattered* — four weapons filled early, upgrades spread, blessings across gods
  (never Tier 2); *scattered_no_lance* — the same without the Rail Lance; *unbuilt* — Pulse Rifle L1 all run
  (`RH.Offer.AutoPick worst`). Common rarity only (the real offer gives 1.26 weapon levels per upgrade card on
  average, `URHOfferTuning::UpgradeLevelsByRarity` × `RarityRollWeight`), so the model is slightly conservative.
- **Level by minute** comes from either the current XP curve with the measured pickup ratio, or the GDD target curve
  (L1 0:00 → L8 5:30 → L16 12:30 → L22 20:00, piecewise linear).
- Not modelled: Path tier procs (arcs, explosions, shatter), status DoTs, ultimates, Ricochet / Split / Fork
  modifiers, hero movement. All of them only raise player output, so TTK and clear ratios are upper / lower bounds
  respectively for a build that also has them.

## State of main vs this model (updated at 91b2a28)

- **Done since the first pass:** full 0:00–19:30 spawn timeline with the 7:30 / 12:30 / 17:30 events (now in
  `balance_model.py`), fusions + Legendary fallback (fusion rows in `weapons.csv`), `RH.Bot` (kite + dash),
  `-RHSaveSlot=` fresh meta slots and `?Patron=` on the arena URL (both used by `RunSims.ps1`).
- **Bot baseline: partial** (2 + 2 runs, [`sims.md`](sims.md) §0) — the scheduler's playtest hold kills worktree
  game launches; re-run `RunSims.ps1 -Bot -Patron <Path>` (10 × `-Pick worst`, 10 × `-Pick random`) when it lifts.
- Still open: chest → offer wiring in the sims, the boss fight at 20:00 in bot runs.
- The first-pass sections (§1–3 of `sims.md`) were measured on f31c07c without the bot: the stationary hero, no events,
  nothing after 15:30, meta pool growing between runs.
