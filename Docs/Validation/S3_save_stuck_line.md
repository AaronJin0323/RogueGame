# S3: the hub says when a session is stuck on its save

Branch `s3/save-stuck-line`, from `dev/s3` at `81c638e`. Class BUG, layer the hub's root and the save's notice. It replaces nothing: two lines join the two `vigil.md` R-VIG-21 already had.

Build: not built yet. Phase 1 holds the rules and the rows; the code waits for its build and gate (phase 2), which fill in this note's last section.

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

## What phase 2 read
Not run yet: the build, `Localize.bat`, the gate with `-Base dev/s3`, and `VigilCheck.ps1` launch `Large` on the two longer lines in en and zh-Hans.
