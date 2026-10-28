# 12 — Production

> Method: `Harness/H03` (stages), `H04` (change), `H06` (pipeline), `H08` (decisions), `H09` (self-audit). This file holds only what is specific to Dusktracer.

## 1. Roles
- **Owner (human):** decides in `13_Decisions/OPEN.md`; plays every feel-changing build; books validators.
- **Scheduler (agent):** keeps `STATE.md`, writes briefs, merges through `Tools/Gate.ps1`, runs the weekly dashboard.
- **Implementers / reviewers (agents):** work from a brief (`Harness/templates/brief.md`) and `AGENTS.md`.
- **Validators:** ≥ 3 non-owner humans per session (≥ 1 gamepad, ≥ 1 zh-Hans).

## 2. Stages (exit tests always include humans)

| Stage | Build | Exit test | Not before exit |
|---|---|---|---|
| **S0 Foundations** | repo, gates (`11_Quality §1`), constants generator, loc CSV pipeline, instrumentation, invariants R-TEC-01…12, readability contract + palette on grey boxes, value-budget tool stub, stress arena | empty arena runs `C.BULLETS.CAP` bullets + `C.ENEMIES.CAP` enemies within `C.BUDGET.*` (p50, p95); all live gates green; en + zh round-trip string on screen | any feature |
| **S1 Core slice** | one grey-box arena; Storm, Forge, Tide with the full identity layer (D-002); 5 weapons, 8–10 modifiers with capability tags; Swarm/Tank/Rusher/Turret + one boss; director with pacing, drops and kill horizon; full offer system; card-kind HUD; bot v1 | 3 validators × 2 runs; core bands hold; ≥ 2 name their archetype; value split within `C.POWER.*`; shape constants locked | second arena, meta, more Keepers, art fidelity |
| **S2 All Keepers** | Keepers 4–6; 10 weapons; 16 modifiers (generated matrix); L16 Reforging; duo reactions; Intercession variants; elites; bot v2 + skill knob | ablation gates nightly; humans and bots win a full run; death histogram in band | acts, fates, Refusal, co-op UI |
| **S3 Loop closure** | Vigil hub, saves, Ichor, contracts, discovery, codex layers, narrative v0.1, full zh, settings/accessibility, package with log export | external: 3 fresh validators × 3 runs, pass 2 of 3; packaged pad pass; `findings_S3.md` | numbered specs for later content |
| **S4 Presentation** | art to the reference bar; audio signatures; perf tiers | 3:1 contrast in both weathers; p50/p95 per tier; goldens re-baselined once | — |
| **S5 Breadth** | arena 2, new roles and boss, heat, bids — every number GATED on S3 findings | each arena won at heat 0 by humans before the next; external #2 | arena 3 |
| **S6 Modes and co-op** | Refusal, fates, endings, local 2P (D-004), daily seed | couch test with 2 humans; `SoloParity` golden unchanged | online |
| **S7 1.0** | polish, accessibility, Deck tier, onboarding | release checklist; perf re-certified on all tiers | — |

Forecast (separate from the gates): S0 gate 2026-11-01 if the arena and perf tool land; S1 sessions to be booked by the owner (`STATE.md` queue).

## 3. Rules specific to this project
- GATED-debt limit `C.GATED.DEBT_MAX`; deferred-platform constants (D-007) do not count.
- Consolidation reserve: 15 % of every stage's budget.
- Never cut (≤ 6): the five-card offer system · Keeper identity carries power · the readability contract · bilingual text · per-player state · `main` always opens and plays.
- Runtime-observation work is split in three jobs: instrument → scored test → fix (`Harness/H06`).
- Every change answers: class, layer, what it replaces (`Harness/H04`).
