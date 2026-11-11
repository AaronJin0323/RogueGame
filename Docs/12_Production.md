# 12 — Production

> Method: `Harness/H03` (stages), `H04` (change), `H06` (pipeline), `H08` (decisions), `H09` (self-audit). This file holds only what is specific to Dusktracer.

## 1. Roles
- **Owner (human):** decides in `13_Decisions/OPEN.md`; plays every feel-changing build; books validators.
- **Scheduler (agent):** keeps `STATE.md`, writes briefs, merges through `Tools/Gate.ps1`, runs the weekly dashboard.
- **Implementers / reviewers (agents):** work from a brief (`Harness/templates/brief.md`) and `AGENTS.md`.
- **Validators:** ≥ 3 non-owner humans per session (≥ 1 gamepad, ≥ 1 zh-Hans). The same three may return for later sessions and later stages; a stage that asks for fresh players means fresh saves (D-049).

## 2. Stages (exit tests always include humans)

| Stage | Build | Exit test | Not before exit |
|---|---|---|---|
| **S0 Foundations** | repo, gates (`11_Quality §1`), constants generator, loc CSV pipeline, instrumentation, invariants R-TEC-01…12, readability contract + palette on grey boxes, value-budget tool stub, stress arena | empty arena runs `C.BULLETS.CAP` bullets + `C.ENEMIES.CAP` enemies within `C.BUDGET.*` (p50, p95); all live gates green; en + zh round-trip string on screen | any feature |
| **S1 Core slice** | one grey-box arena; Storm, Forge, Tide with the full identity layer (D-002); 5 weapons, 8–10 modifiers with capability tags; Swarm/Tank/Rusher/Turret + one boss; director with pacing, drops and kill horizon; full offer system; card-kind HUD; bot v1 | two sessions of 3 validators × 2 runs (D-048). A: ≥ 2 name their build, the owner still wants to play. A gate decision then sets the two pressure bands (D-046). B: the bands, swarm kill time and the pause limit hold. Value split by loss within `C.POWER.*` on each session's build (D-047). Then shape constants lock | second arena, meta, more Keepers, art fidelity |
| **S2 All Keepers** | Keepers 4–6; 10 weapons; 16 modifiers (generated matrix); L16 Reforging; duo reactions; Intercession variants; elites; bot v2 + skill knob | ablation readings (Keeper share and archetype band by loss, Intercession band) read on the tip before the session, by hand until something schedules them; humans and bots win a full run; death histogram in band | acts, fates, Refusal, co-op UI |
| **S3 Loop closure** | Vigil hub, saves, Ichor, contracts, discovery, codex layers, narrative v0.1, full zh, settings/accessibility, package with log export | external: 3 validators on fresh saves × 3 runs; passes when two of three hold: ≥ 2 ask for another run unprompted, ≥ 2 name their build, the owner still wants to play (D-049); packaged pad pass; `findings_S3.md` filled by a person | numbered specs for later content |
| **S4 Presentation** | art to the reference bar; audio signatures; perf tiers | 3:1 contrast in both weathers; p50/p95 per tier; goldens re-baselined once | — |
| **S5 Breadth** | arena 2, new roles and boss, heat, bids — every number GATED on S3 findings | each arena won at heat 0 by humans before the next; external #2 | arena 3 |
| **S6 Modes and co-op** | Refusal, fates, endings, local 2P (D-004), daily seed | couch test with 2 humans; `SoloParity` golden unchanged | online |
| **S7 1.0** | polish, accessibility, Deck tier, onboarding | release checklist; perf re-certified on all tiers | — |

Early starts, each by its own record and none waiving an exit: S2 on `dev/s2` (2026-10-31, no record), S3 on `dev/s3` (D-022), a first wave of S4 on `dev/s4` (D-043). D-050 stops the stack there until session A has been held.

## 3. Rules specific to this project
- GATED-debt limit `C.GATED.DEBT_MAX`; deferred-platform constants (D-007) do not count.
- Consolidation reserve: 15 % of every stage's budget.
- Never cut (≤ 6): the five-card offer system · Keeper identity carries power · the readability contract · bilingual text · per-player state · `main` always opens and plays.
- Runtime-observation work is split in three jobs: instrument → scored test → fix (`Harness/H06`).
- Every change answers: class, layer, what it replaces (`Harness/H04`).
- Fixes land on the lowest branch that has the defect and are carried forward by merge: `main`, `dev/s2`, `dev/s3`, `dev/s4` (D-050).
- A package built for people, an exit battery and a perf reading run with no other engine or compiler process alive on the PC (D-050).
- `dev/s2` lands on `main` when S1 exits, `dev/s3` when S2 exits; until then each branch is its stage's build and validators play that branch's package (D-050).
- Decision files are committed on `main` first; a decision in an owner class also gets a line in `OPEN.md` (D-051).
- No number is tuned on a bot survival clause until the S1 gate decision names the bot skill row that brackets people (D-050, `10_Tuning.md §4`).

## 4. Forecast: the human path

Gates, not dates. What each open exit needs from people, in order. Agents can shorten none of it.

| Stage | Sessions with people | People | Blocked by |
|---|---|---|---|
| S0 | none | none | passed 2026-10-30 (D-012) |
| S1 | A, then a gate decision, then B: 3 × 2 runs each on `main`'s package | three, the same three both times; ≥ 1 pad, ≥ 1 zh-Hans | the consolidation list on `main` (`STATE.md` Queue), then O-008 |
| S2 | one: 3 × full runs on `dev/s2`'s package with S2 content unlocked | the same three may return | the S1 exit; an S2 kit and a launcher unlock, neither written |
| S3 | one external: 3 × 3 runs on fresh saves, `dev/s3`'s package | the same three may return (D-049) | the S2 exit; an owner rehearsal of the kit build; a native zh reader for the marked rows |
| S4 | contrast and perf are tools; the reference bar is the owner's eye | the owner | the S3 exit; real 4070, 3060-class and Deck readings (O-012) |

The owner's own runs are a checkpoint before session A, not a session (O-011).
