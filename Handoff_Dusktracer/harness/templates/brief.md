# Brief <job-id>

## Goal
<one sentence; cites the hypothesis line or decision D-nnn if RULE/SYSTEM>

## Allowed files
<globs; include the OLD implementation to be removed if this is a replacement>

## Forbidden files
<globs owned by others>

## The change
<what, by reference: R-IDs, C. keys, catalog rows, doc §sections. No paraphrased rules.>

## Binding rules
- R-… , R-… (read these sections only)
- Harness: H06 §5 tolerances, H04 §2 replace-not-stack

## Done when
- T-<SYS>-nn passes with <assertion>
- Build: 0 warnings; per-file compile of changed sources passes; release config compiles
- Tests: count ≥ <floor>; 0 fail; no crash marker
- Text/loc audit passes (if text touched); goldens unchanged or re-baselined with reason
- `rg <removed symbol>` → 0 hits (for replacements)
- HUMAN (if any): <who checks what, in which session>

## Tolerances
- Missing wiring inside allowed files → build it, note it in the report
- Missing structure outside allowed files → stop; report `blocked` with the smallest unblocking scope change
- Non-behavioural spec/code mismatch → adapt, one line in the report

## Out of scope
- …

## Context files (must exist)
- …

## Report
- One paragraph: what changed, what was removed, deviations, open items. Final message is forwarded to the reviewer.
