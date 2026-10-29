# S0 Foundations — exit evidence (2026-10-30, main @ c4a3ccf)

Exit test (`12_Production §2`, `Harness/H03 §3`): the empty arena runs `C.BULLETS.CAP` bullets and `C.ENEMIES.CAP` enemies within `C.BUDGET.*` (p50 and p95) on the named hardware or its agreed proxy; all live gates green; the en and zh-Hans round-trip string on screen. Pass rule written before the run: all three hold → PASS.

## 1. Load and budget — PASS
`Tools/Perf.ps1` (T-TEC-10), 1920×1080, `DT.Spawn.Keep 400, DT.Bullets 6000`, warm-up 10 s, 1 800 captured frames, RTX 4080 (proxy for the 4070 budget, D-011).

| Metric | p50 | p95 | Budget | Verdict |
|---|---|---|---|---|
| Live bullets | 5 975 | — | ≥ 0.95 × 6 000 | met |
| Live enemies | 388 | — | ≥ 0.95 × 400 | met |
| Game thread | 1.90 ms | 2.28 ms | ≤ 8 / ≤ 10 ms | met |
| GPU (measured) | 2.93 ms | 3.31 ms | — | — |
| GPU (× 1.6 proxy) | — | 5.29 ms | ≤ 12 ms | met |
| Frame | 3.45 ms | 3.97 ms | — | — |

## 2. Gates — PASS
`& Tools\Gate.ps1 -AllFiles` at c4a3ccf: secrets, docs (49 rules, 0 errors), constants + palette, loc, build (0 warnings), nonunity (26 files), Shipping (0 warnings), tests (31 run, 0 failed, floor 30), smoke en and zh-Hans → **GATE: PASS**. Not run, by design: `pad` (first package is S3), `scope` (no brief in flight), `mac` (D-007).

## 3. Both languages on screen — PASS
`Tools/Smoke.ps1` in each culture: the HUD's drawn `Test.LocRoundTrip` equals `Content/DT/Text/ST_UI.csv` for that culture (`loc ok (en)`, `loc ok (zh-Hans)`); the zh-Hans screenshot shows 曳光 / 语言检查：这一行是中文。 as glyphs (engine default font).

## 4. Built in S0 (for the record)
Day-0 invariants R-TEC-01…14 with tests; bullets (R-BUL-01…08) and enemies (R-ENM-01…08) as struct-of-arrays on the fixed-step sim with ISM rendering; grey-box arena, camera, HUD; palette generated from `04_Presentation §3`; telemetry (per-minute CSV + per-step JSONL, per player and per source, ~0.9 MB/min under stress) and `Tools/power_split.py`; gates `Tools/Gate.ps1`; perf with load proof and GPU proxy.

## 5. Known gaps carried into S1 (not exit criteria)
- No human has played anything yet: the three S1 validation sessions are not booked (owner).
- Enemy hulls render as bright magenta under the stress load; R-PRE-04 wants a dark hull with a lit optic, which needs a separate optic part (S1 enemy art pass).
- Contrast check T-PRE-05 (rendered per-element contrast in both weathers) is still PLANNED; only one weather exists.
- GPU budget is judged through the 4080 proxy; real 4070 and 3060-class readings are due before S4 exits (D-011).
- `pad`: no packaged build yet; string-table and `.locres` staging in a package is unverified.

## Verdict
**S0 PASS.** S1 (core slice) may start (D-012).
