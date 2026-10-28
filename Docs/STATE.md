# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-10-28)

## Where we are
- Stage: **S0 Foundations**, day 1 — entry 2026-10-28; exit test not yet booked
- Main branch: initial scaffold commit (no code, no UE project yet)
- Release branch: none

## Done (day 1)
- Repo + LFS attributes + `.gitignore`; harness copied to `Docs/Harness/` (read-only; changes via D-nnn)
- Drafts: `00_Vision.md`, `08_Constants.md` + `constants.yaml`, `Glossary.md`; `AGENTS.md`/`CLAUDE.md`
- `13_Decisions/OPEN.md` with the five kickoff decisions

## In flight
- none

## Queue (next 5) — blocked on owner window
1. Owner confirms `00_Vision` and `08_Constants §1` line by line (O-001); file D-001…D-005
2. Book the three S1 validation sessions (≥ 3 non-owner humans; ≥ 1 pad, ≥ 1 zh)
3. Create `Dusktracer.uproject` (UE 5.8, module `Dusktracer`, adaptive unity off) + `Tools/Build.bat`, per-file compile, Shipping compile
4. `07_Technical §2` invariants 1–10 + `11_Quality §1 gates, §3 instrumentation` + `12_Production` (stage table S0–S7)
5. Gates green on an empty project: 0-warning build, headless tests + floor, text/loc audit, rendered smoke + goldens, packaged pad check

## Holds
- none; automated launches: off

## Waiting on the owner (mirror of OPEN.md IDs only)
- O-001, O-002, O-003, O-004, O-005

## Numbers at risk (GATED unmeasured: 0/5)
- none yet

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff input: `Handoff_Dusktracer/game/G00_START_HERE.md`
