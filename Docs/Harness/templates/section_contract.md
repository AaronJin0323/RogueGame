# <System> — <Section title>

## Intent
<≤ 3 sentences: what this does for the user; which pillar>

## Rules
- R-<SYS>-01 <MUST/NEVER statement>
- R-<SYS>-02 …

## Numbers
| Constant | Key | Tag | Dependents |
|---|---|---|---|
| <name> | C.<GROUP>.<NAME> | CORE / GATED(<source>) / MEASURED(<ref>) | <specs/tests> |

## Data
| Field | Type | Notes |
|---|---|---|

## Acceptance
- T-<SYS>-01 <TestName>: <assertion>  (pairs with R-<SYS>-01)
- HUMAN: <what a validator must be able to do/notice>

## Telemetry
- <event>{fields}

## Text
- Prefix `ST_<Table>.<Prefix>.*`, budget ≤ N rows, languages: …

## Out of scope
- …

## Open
- <question> → resolved by <data/session>
