# S1 seed switch

**Verdict: BUG fixed on `fix/seed-switch`. `FDTRunSeed` reads the `-DTSeed=` switch alone, as `dev/s3` does (`b8f4ee8`, its D-028 item 5), so a map's `?DTSeed=` is no longer taken for a bad switch. `input_hash` on seed 9107 is unchanged on the one arm read. No balance number moved.**

Class BUG, launch layer. It replaces the bare `DTSeed=` match. R-TEC-16 already says `-DTSeed=<n>`, so no rule changed. `Source/Dusktracer/Core/DTRunSeed.cpp` and `Source/Dusktracer/Tests/Core/DTRunSeedTests.cpp` are byte-identical to `dev/s3`.

## The bug
`FParse::Value` accepts a match after any character that is not a letter or digit, so `DTSeed=` also matched `?DTSeed=` inside a map on the command line. Only whitespace ends the value (R-TEC-16), so `L_Arena_Test?DTSeed=9100?DTPatron=Storm` read as the seed `9100?DTPatron=Storm`, which is rejected.

## Readings
Before is `fff2443`, after is `f5ab8c3`, each built from the commit; the run files carry those stamps. Every launch is `-unattended` and off-screen, and no log has a `GameWindow` line.

| Launch | Before | After |
|---|---|---|
| Seed 9107, Steered Storm Tempest, full length | `e96dc0a8`, victory, 967.9000 s, 58076 steps | `e96dc0a8`, victory, 967.9000 s, 58076 steps |
| Map with `?DTSeed=9100?DTPatron=Storm` | exit 2, no run | exit 0, run on seed 0 |
| Map with `?DTSeed=9100` as its last option | exit 0, run on seed 9100 | exit 0, run on seed 0 |

Press counts, picks and declines match too. `e96dc0a8` is also the value `S1_human_seed.md` read on this arm at `6a96eaa`, through `-nullrhi`.

With the new test lines and the old match, the gate's `tests` step failed `Dusktracer.Tec.RunSeed.Default` on exactly those two lines, 1 of 251, so the test does see the bug.

## Decisions
1. The comments stay `dev/s3`'s, word for word. They name `ADTGameMode::ParseSeed`, which is on `dev/s2` and `dev/s3` but not `main`, and `Tools/WinCheck.ps1`, which is only on `dev/s3`. Rewording them for `main` would make the merge up conflict on those lines, and the names come true at that merge.
2. On `main` a map's `?DTSeed=` now has no reader, which is the third row: that launch ran on 9100 only by accident of the bare match, and now a tool stays on 0 and a person draws a fresh seed. Nothing on `main` launches that way. `ParseSeed` was not brought down, because it belongs to the restart path on `dev/s2`.
3. `DTSeed=9107` and `/DTSeed=9107` are no longer read as a seed, the same as `dev/s3`. No script in `Tools` uses either.
4. The hash was read by a direct off-screen launch with `BotRuns`' bot flags, not through `BotRuns`, because it passes `-game -nullrhi`.
5. No package was built here. `PadCheck` writes to and deletes from `%LOCALAPPDATA%\Dusktracer` once a package exists, and with none the gate's `pad` step reads NOT RUN without touching that folder.

## For the merge
A dry `git merge-tree` of this branch into `dev/s3` and into `dev/s2` is clean. D-028's Impact line says it replaces nothing on `main` or `dev/s2`; item 5 now does.

## Not verified
- One arm only, not the ten of `S1_human_seed.md`.
- The Shipping and Development package builds: `SeedCheck -Package` and `PadCheck` were not run.
- A map's option on a launch with no `-unattended`: the fresh seed there is proven by the unit test line, not by a live launch.
- Play in the editor.
- `dev/s2` itself: the merge was read dry, and nothing was built or run there.
- Mac: not run (D-007).
