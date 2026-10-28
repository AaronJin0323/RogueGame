# H02 — The document system / 文档体系

## 1. The section contract (every system spec, every section)

A section of a system spec is complete only when all nine fields exist, in this order. Empty fields are written as `none` with a reason — never omitted.

| Field | Content | Example |
|---|---|---|
| **Intent** | What this does for the user and which pillar it serves (≤ 3 sentences) | "Let the second choice change behaviour, not numbers" |
| **Rules** | MUST / NEVER statements with IDs `R-<SYS>-nn` | `R-OFF-03 Every offer has exactly 5 distinct items` |
| **Numbers** | Table: constant name → key in `08_Constants`; each row tagged `CORE` / `GATED(<source>)` / `MEASURED(<ref>)` | `offer.rerolls = C.OFFER.REROLLS (CORE)` |
| **Data** | Fields and types of the data assets / catalog rows this section owns; keys use canonical identifiers, never display names | `Weapon: Key, Pattern, Caps[], Load, Levels[8]` |
| **Acceptance** | Automated test names `T-<SYS>-nn` with their assertion; human checks marked `HUMAN` | `T-OFF-03 Offers.FiveUnique` |
| **Telemetry** | Events / counters this section writes | `offer_shown{items[5], weights}` |
| **Text** | String-table key prefixes and the row budget, per language | `ST_Offers.* ≤ 40 rows` |
| **Out of scope** | What this version explicitly does not do | "online play" |
| **Open** | Unresolved questions + the data that would resolve each | "reserve items needed? → pool-exhaustion rate at level 22+" |

## 2. The document set (template)

Rename the domain documents to fit the product; keep the numbers and roles.

```
Docs/
  00_Vision.md            one page: pillars (ranked), anti-goals, success criteria, why the non-negotiables exist
  01_ProductRules.md      the user-facing rules in user terms; no bare numbers; cites 02–08
  02_Systems/<sys>.md     one spec per system, written to the section contract; ≤ 300 lines each
  03_Content.md           world/narrative/content rules; naming rules; what text ships per version
  04_Presentation.md      the readability / visual contract (binding) + reference material (non-binding)
  05_Audio.md             (if applicable)
  06_UX.md                screen inventory, interaction rules, accessibility
  07_Technical.md         architecture, day-0 invariants, naming, budgets, toolchain — NO decision log
  08_Constants.md         the only place numbers live; mirrored by constants.yaml → generated code/data
  09_Catalog/             content tables (CSV) from which data assets are generated
  10_Tuning.md            targets, method, calibration, gates, the number-change log
  11_Quality.md           gates, test taxonomy, instrumentation list, validation protocol, release checklist
  12_Production.md        stage/gate roadmap, pipeline, brief template, system budget, valves, change & context rules
  13_Decisions/           D-nnn.md one per decision + OPEN.md (waiting on the owner)
  Glossary.md             key ↔ display name per language ↔ one-line definition
AGENTS.md / CLAUDE.md     the agents' entry: non-negotiables by rule ID, known traps, brief tolerances
```

Size limits: a system spec ≤ 300 lines (split when larger); `07_Technical` carries no history; `13_Decisions` is a folder, never a section.

## 3. ID schemes

| Thing | ID | Lives in | Paired with |
|---|---|---|---|
| Rule | `R-<SYS>-nn` | `02_Systems/<sys>.md` | ≥ 1 test `T-<SYS>-nn` |
| Test | `T-<SYS>-nn` | code + `11_Quality` index | its rule |
| Constant | `C.<GROUP>.<NAME>` | `08_Constants.md` + `constants.yaml` | list of dependents |
| Decision | `D-nnn` | `13_Decisions/D-nnn.md` | the rules/constants it changed |
| Glossary key | canonical identifier | `Glossary.md` | display strings per language |

Briefs cite IDs, never paraphrases. A rule without a test, or a test without a rule, fails lint.

## 4. Lint (run on every docs commit)

- No bare number outside `08_Constants.md` (allow-list: dates, IDs, line numbers).
- Every `R-` has a `T-`; every `T-` named in a spec exists in code.
- Every `C.` referenced exists; every `C.` lists ≥ 1 dependent.
- No stale markers in body text: `superseded`, `where they differ`, `TODO(old)`, `pre-redesign`.
- Each system spec section has all nine contract fields.
- Must-read set (`AGENTS.md` + `00_Vision` + shape constants) ≤ the token budget in `H05 §2`.
- Glossary covers every canonical key used in `09_Catalog`.

## 5. Shape constants (lock at the first gate)

The small set of numbers everything else depends on. Keep it under ~20 rows. Typical members: session/run length, milestone timings, number of decisions per session, starting pool sizes, hard caps (entities, budget ms), target bands for the core-feel metrics, the value-split budget between content layers. Changing one is a decision record with a dependency sweep (`H04 §5`).

## 6. Writing order

1. `00_Vision` + the shape-constant draft + `Glossary` skeleton — half a day, owner confirms line by line.
2. `07_Technical §invariants` + `11_Quality §gates, §instrumentation` + `12_Production` — these decide what stage 0 can do.
3. The system specs stage 1 needs, with their catalog tables and the presentation contract.
4. Content/naming rules before the first user-facing string.
5. Remaining systems before stage 1 exits; later-stage systems as one-page sketches only.
6. `01_ProductRules` last: it is the user-facing summary of 02–08 and introduces nothing new.

**Definition of done for a document:** nine-field contract complete; lint clean; owner has read it in the daily window.

---
### 中文摘要
每节九字段契约（Intent/Rules/Numbers/Data/Acceptance/Telemetry/Text/Out of scope/Open）；14 份文档的模板与大小上限；R/T/C/D 四种 ID 两两配对；七条 lint；≤ 20 行的形状常数表在第一门槛锁定；写作顺序从愿景与常数开始、以用户规则结束。
