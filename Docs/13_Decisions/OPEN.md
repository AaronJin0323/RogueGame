# OPEN decisions — for the owner window

> Normally ≤ 3 items per window (`Docs/Harness/H08`). Day-0 exception: the five kickoff decisions from `Handoff_Dusktracer/game/G00_START_HERE.md §8` are listed together because S0 cannot start without them. Answer each with A/B (or a note); the scheduler files a `D-nnn.md` per answer and removes the item.

## O-001 — Session length and shape constants  [SHAPE]
- **Context:** `G02` part 3 §01/§08 fixes run 15:00 to boss, boss ≤ 2:00, L8 ≈ 5:00, L16 ≈ 10:30, ≈ 28 decisions. Draft rows are in `Docs/08_Constants.md`.
- **Options:** A — accept the draft table as written / B — re-open specific rows (name them).
- **Recommendation:** A; every timeline and the director spec are written against these numbers.
- **Default if unanswered by 2026-10-30:** none — shape constants need the owner's line-by-line confirmation (S0 cannot exit without it).
- **Impact:** all `C.RUN.*`, `C.LEVEL.*`, `C.DECISIONS.*`.

## O-002 — The three S1 Keepers  [SYSTEM]
- **Context:** S1 builds the full identity layer for three Keepers only.
- **Options:** A — Storm (bullet-hell), Forge (melee/burn), Tide (control): the most distinct trio / B — another trio.
- **Recommendation:** A.
- **Default if unanswered by 2026-10-30:** A.
- **Impact:** `02_Systems/keepers.md` expression tables, `09_Catalog/expression.csv`.

## O-003 — Engine and platform  [SHAPE]
- **Context:** UE 5.8 is installed at `E:\UE5\UE_5.8`. Budgets in `G02` assume a 4070-class GPU, with a 3060 tier and a Steam Deck tier at 1.0.
- **Options:** A — UE 5.8.x, Win64 first, Deck tier at 1.0 / B — change.
- **Recommendation:** A.
- **Default if unanswered by 2026-10-30:** A.
- **Impact:** `07_Technical` toolchain and budgets; `C.BUDGET.*`.

## O-004 — Local two-player co-op in 1.0  [SYSTEM]
- **Context:** Per-player run state, owner-tagged events and per-system RNG are day-0 invariants either way; this decides whether S6 ships couch co-op.
- **Options:** A — yes, local 2P in 1.0 (online after 1.0) / B — no co-op in 1.0 (keep the invariants anyway).
- **Recommendation:** A.
- **Default if unanswered by 2026-10-30:** A.
- **Impact:** `02_Systems/coop.md`, S6 scope.

## O-005 — Act II / III content before S5  [VALVE]
- **Context:** Second and third arenas, fates and the Refusal run.
- **Options:** A — one-page sketches only until `Docs/Validation/findings_S3.md` exists / B — allow earlier specs.
- **Recommendation:** A.
- **Default if unanswered by 2026-10-30:** A.
- **Impact:** `02_Systems/modes.md`, `03_Content.md §7`.
