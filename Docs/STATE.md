# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-10-30)

## Where we are
- Stage: **S1 Core slice** — entry 2026-10-30 (D-012; S0 evidence `Docs/Validation/S0_exit.md`); exit test not booked (O-008)
- Platform: Win64 only (D-007). Decisions: the scheduler decides and records them (D-009); `OPEN.md` holds human-only items
- Origin: `https://github.com/AaronJin0323/RogueGame` — **public** (D-006)
- Gate: `& Tools\Gate.ps1` — live: secrets, docs, constants (+ palette), loc, build, nonunity, shipping, tests (floor 30), smoke en + zh-Hans. Not run: pad (S3), scope (no brief), mac (D-007)
- Perf: `& Tools\Perf.ps1` — at caps: game thread p95 2.28 ms, proxied GPU p95 5.29 ms (D-011)

## Built so far (S0)
- Core: RNG streams, fixed step, per-player run state + event sink, save envelope, spatial hash, sim subsystem with phases (R-TEC-01…14)
- Bullets (R-BUL-01…08) and enemies (R-ENM-01…08): SoA on the fixed step, ISM render, `DT.Bullets`, `DT.Spawn[.Keep|.Clear]`
- Arena `L_Arena_Test`, `ADTGameMode`, top-down camera, HUD; `ST_UI` live in en + zh-Hans (`Tools/Localize.bat`)
- Telemetry (R-TEL): per-minute CSV + per-step JSONL in `Saved/Playtests/`, per player and source; `Tools/power_split.py`
- Palette generated from `04_Presentation §3` into `DTPaletteGen.h`; constants into `DTConstantsGen.h`; fallbacks in `09_Catalog/fallbacks.csv`

## In flight
- none

## Queue (next 5) — S1, specs before code (Harness/H02 §6 step 3)
1. `02_Systems/keepers.md` + `09_Catalog/expression.csv` for Storm, Forge, Tide (flags, 3 archetypes each, tiers, enchantment, Intercession)
2. `02_Systems/offers.md` (five-card system from the old GDD §5.6), `weapons.md` / `modifiers.md` with capability tags + `flag_compat.csv`
3. `02_Systems/combat_core.md` (hero: move, dash, Intercession; damage taken) and `director.md` (pacing, drops, kill horizon)
4. Hero pawn + input (KBM and pad), then weapons → offers → Keepers on the specs; enemy roles Tank/Rusher/Turret + one boss
5. Bot v1 and the S1 validation kit (intro, questions, note template)

## Holds
- none; automated launches: off

## Waiting on the owner (mirror of OPEN.md IDs only)
- O-008 (book S1 validation sessions)

## Numbers at risk (GATED unmeasured: tracked per spec; C.BUDGET.MAC_* deferred by D-007)
- Carried gaps: enemy hull reads bright magenta (needs separate optic, R-PRE-04); T-PRE-05 contrast check PLANNED; real 4070/3060 readings due before S4

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/INDEX.md` · Evidence: `Docs/Validation/` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff: `Handoff_Dusktracer/game/G00_START_HERE.md`
- Engine `E:\UE5\UE_5.8` (5.8.3) · Blender `E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe` (headless) · Mac (deferred): `ssh dt-mac`
