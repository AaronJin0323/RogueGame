# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-10-28)

## Where we are
- Stage: **S0 Foundations**, day 1 — entry 2026-10-28; exit test not yet booked
- Main branch: S0 scaffold + empty UE 5.8 project `Dusktracer.uproject` (module `Dusktracer`, Win64 + Mac targets)
- Release branch: none
- Platforms: Win64 and macOS in parallel (D-003). Mac gates: **not run** until O-006 is answered.

## Decided today
- D-001 shape constants accepted · D-002 S1 Keepers Storm/Forge/Tide · D-003 Win64 + macOS, M2 Air target, Blender 5.2.2 headless · D-004 local 2P in 1.0 · D-005 Act II/III sketches only

## In flight
- none

## Queue (next 5)
1. Book the three S1 validation sessions (≥ 3 non-owner humans; ≥ 1 pad, ≥ 1 zh) — owner
2. `07_Technical` (invariants 1–10, toolchain incl. Blender path and Mac remote build) + `11_Quality §1 gates, §3 instrumentation` + `12_Production` (S0–S7 table)
3. Gates on the empty project: 0-warning build, per-file (non-unity) compile, Shipping compile, headless tests + floor, text/loc audit, rendered smoke + goldens, packaged pad check — Win64 and Mac
4. Localisation CSV pipeline (en + zh-Hans) with one round-trip test string; `Tools/gen_constants.py` from `constants.yaml`
5. Day-0 invariant skeleton (per-player run state, RNG streams, fixed step, save version) + `Stats` instrumentation on an empty arena; measure M2 Air budget (C.BUDGET.MAC_*)

## Holds
- none; automated launches: off

## Waiting on the owner (mirror of OPEN.md IDs only)
- O-006

## Numbers at risk (GATED unmeasured: 3/5)
- C.BUDGET.MAC_RES_FPS, C.BUDGET.MAC_FRAME_P50_MS, C.BUDGET.MAC_FRAME_P95_MS wait for the S0 empty-arena measurement on the M2 Air

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/INDEX.md` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff input: `Handoff_Dusktracer/game/G00_START_HERE.md`
- Build: `Tools/Build.bat` (DusktracerEditor Win64 Development; `UE_ROOT` defaults to `E:\UE5\UE_5.8`)
- Blender: `E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe` (headless `-b --python`)
