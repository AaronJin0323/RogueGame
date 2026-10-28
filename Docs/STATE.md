# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-10-28)

## Where we are
- Stage: **S0 Foundations** — entry 2026-10-28; exit test not yet booked (`12_Production §2`)
- Platform: **Win64 only** (D-007 defers macOS; Mac setup kept, gate not run)
- Origin: `https://github.com/AaronJin0323/RogueGame` — **public** (D-006): nothing secret or licence-restricted is ever committed
- Gate: `& Tools\Gate.ps1` — live steps secrets, docs, constants, loc, build, nonunity, shipping, tests; not run yet: smoke, pad, scope; mac deferred

## Done in S0
- Docs: `00_Vision`, `07_Technical` (R-TEC-01…12), `08_Constants` + `constants.yaml`, `11_Quality`, `12_Production`, `Glossary`; decisions D-001…D-007
- Tools: `gen_constants.py` (→ `Source/Dusktracer/Core/DTConstantsGen.h`), `docs_lint.py`, `secret_scan.py`, `loc_build.py`, `Gate.ps1`, `Build.bat [Target] [Config]`
- Code: `Core/` RNG streams, fixed step, per-player run state, save envelope; tests T-TEC-01…04 green
- Text: `Content/DT/Text/ST_UI.csv` with the round-trip string `Test.LocRoundTrip`

## In flight
- `Docs/04_Presentation.md` (readability contract + palette tokens) — subagent; then add its `C.PRE.*` constants

## Queue (next 5)
1. Owner: book the three S1 validation sessions (≥ 3 non-owner humans; ≥ 1 pad, ≥ 1 zh)
2. Stress arena `L_Arena_Test` (grey box, palette tokens) + `UDTSimSubsystem` driving `FDTFixedStep`; register `ST_UI` and show the round-trip string in en and zh
3. Bullet SoA + spatial hash + ISM render at `C.BULLETS.CAP`; enemy SoA at `C.ENEMIES.CAP`; `DT.Spawn`/`DT.Bullets` debug commands (not in Shipping)
4. `Tools/Perf.ps1` (p50/p95, `Saved/gpu.lock`) and `Tools/Smoke.ps1` + goldens → gate steps `smoke`, T-TEC-10
5. `Stats` instrumentation (`11_Quality §3`) → per-minute CSV + JSONL; value-budget ablation tool stub

## Holds
- none; automated launches: off

## Waiting on the owner (mirror of OPEN.md IDs only)
- none

## Numbers at risk (GATED unmeasured: 0/5 counted; C.BUDGET.MAC_* deferred by D-007)

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/INDEX.md` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff input: `Handoff_Dusktracer/game/G00_START_HERE.md`
- Engine `E:\UE5\UE_5.8` (5.8.3) · Blender `E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe` (headless `-b --python`)
- Mac (deferred): `ssh dt-mac`; `bash Tools/MacGate.sh`
