# S3: the hub says when a session is stuck on its save

Branch `s3/save-stuck-line`, from `dev/s3` at `81c638e`. Class BUG, layer the hub's root and the save's notice. It replaces nothing: two lines join the two `vigil.md` R-VIG-21 already had.

Build: `2129edf` (the branch with `dev/s3` at `0ed7439` merged in), gated with `-Base dev/s3`: `GATE: PASS (2129edf; 3 not run; mask FFFFF3FC)`. The commit after it changes this note only.

## The defect
`meta.md` R-MET-12 has two cases where a session is stuck: the save cannot be read at all, or it is bad and the move aside fails. The session plays on an empty profile and writes nothing, and until now the hub said nothing of it. Shipping has no log, so a player played a whole launch that kept nothing and was told nowhere.

## What was added
- `ST_UI` rows `Hub.Notice.SaveUnreadable` and `Hub.Notice.SaveNotMoved`, after `Hub.Notice.RunLost`. `{0}` is the save's file name, never a path.
- `vigil.md` R-VIG-21: four lines, the first that holds: a stuck line, then `SaveAside`, then `RunLost`. What a stuck launch does with the lost-run marker, as the code does it today. T-VIG-21 reads the two new lines, their rows, the order and the launch after.
- `meta.md` R-MET-12: its last sentence points at R-VIG-21. T-MET-12 says which line each stuck case gives.
- No new test function: `Dusktracer.Hub.Notice`, `Dusktracer.Meta.BadFile` and `Dusktracer.Meta.BadFileBranches` are extended, so `C.GATE.TEST_FLOOR` does not move.

## Invented zh, for a native reader
Both marked `[invented]` in Context, as `S3_vigil.md` lists the hub's other two lines.

| Table | Key | en | zh-Hans |
|---|---|---|---|
| ST_UI | `Hub.Notice.SaveUnreadable` | The save {0} could not be opened. This session starts empty and saves nothing; the file is left as it is. | 存档 {0} 打不开。这次用的是一份空白存档，不会保存任何内容；原文件保持不动。 |
| ST_UI | `Hub.Notice.SaveNotMoved` | The save {0} could not be read and could not be set aside. This session starts empty and saves nothing; the file is left as it is. | 存档 {0} 无法读取，也没能另行保留。这次用的是一份空白存档，不会保存任何内容；原文件保持不动。 |

`python -I Tools\loc_build.py --check` reads `LOC: PASS (13 tables, 862 rows)` with both rows in.

## The run-over screen of a stuck session
A second defect of the same kind, BUG class, in its own commits after the hub line. A stuck session banks only in memory (`meta.md` R-MET-12): the award came back `bBanked` and not `bDeferred`, so the run-over screen drew it as a saved run.

| | Where `Summary.Line` goes | Under the panel |
|---|---|---|
| Before, stuck | `Ichor retained {0} · Balance {1}`, with the amount and the in-memory balance | the Discovery toast, if the run found any |
| After, stuck | `Summary.NotSaved`: Nothing from this run was saved. The save cannot be written. | nothing |
| Not stuck, before and after | `Summary.Line` | the Discovery toast, or `Summary.Waiting` for a busy save (D-042) |

The signal is a new field of the bank's result, `FDTRunAward::bNotSaved`, set only on the stuck path of `FDTMetaSession::Bank`. `bDeferred` keeps its meaning. What the stuck session does with its profile in memory does not change, and it still writes no lost-run marker for its own runs. `DTMenuText::RunOverLines` chooses the two lines, so `Dusktracer.Meta.SummaryLine` (T-MET-06) reads them without a world. No test function is added.

| Table | Key | en | zh-Hans |
|---|---|---|---|
| ST_UI | `Summary.NotSaved` (invented) | Nothing from this run was saved. The save cannot be written. | 这一局的内容没有保存。存档无法写入。 |

`python -I Tools\loc_build.py --check` reads `LOC: PASS (13 tables, 863 rows)` with the row in.

## What phase 2 read
- `Tools\Build.bat`: built first time, no compile error (`Result: Succeeded`, 83 s). `Tools\Localize.bat`: `LOCALIZE: PASS`.
- The gate, 2026-11-11, about 33 minutes: `tests: 547 run of 547 found, 0 failed, floor 547, 0 crash marker(s)`, so `C.GATE.TEST_FLOOR` stays 547. `build`, `shipping` and `devgame` rc 0 with 0 warnings; `nonunity` 12 files, 0 failed. `pad`, `scope` and `mac` not run (no package, no brief in flight, D-007).
- By filter, `Tools\RunTests.ps1 -Filter`: `Dusktracer.Hub.Notice` 1 of 1, `Dusktracer.Meta.BadFile` 2 of 2 (the filter takes `BadFileBranches` too), `Dusktracer.Meta.BadFileBranches` 1 of 1, `Dusktracer.Meta.SummaryLine` 1 of 1, `Dusktracer.Meta.Autosave` 1 of 1, each 0 failed.
- `VIGIL: PASS`; launch `Large` reads `the root, a save that cannot be opened fits at the largest text, every row: ok` and `the root, a bad save that could not be set aside fits at the largest text, every row: ok`, in en and in zh-Hans. The two longer lines fit the detail pane at the largest text in both.
- Not read: `Summary.NotSaved` drawn on a real run-over screen. No launch of the gate ends a run in a stuck session; the line is held by T-MET-06 and the `loc` step.
