# Findings S3: the external test

**No session has been played. This is a template, and it holds no result.** Every row reads NOT READ until a human session fills it. A tool, a bot or an agent never fills a reading: `Tools/S3Exit.ps1` reads the logs and a person writes what it means here, and names themselves in `Filled by`. A reading with no name counts as NOT READ.

**This file existing is not this file written.** D-005 and the S3 exit wait on `findings_S3.md`, and they mean a filled one: until the rows below read, nothing that waits on it may start.

The repo is public (D-006). Validators are `V1`, `V2` and `V3`, never a name. `Filled by` is a role (the host, the owner) or a handle that person agreed to.

Kit: `S3_kit.md`. Exit: `12_Production §2`, S3 row. Build and readings below are the session's own; nothing here is carried over from a bot, a tool run or the owner's run.

## The session

| Field | Reading | Filled by | Evidence |
|---|---|---|---|
| Build: the clean hash in `package_stamp.json` | NOT READ | not filled | none |
| Dates of the sessions | NOT READ | not filled | none |
| Automated runs held, from and to | NOT READ | not filled | none |
| `S3EXIT:` line, and the folder it read | NOT READ | not filled | none |

## Exit rows

| Row | What the docs hold | Reading | Filled by | Evidence |
|---|---|---|---|---|
| Validators | 3 validators × 3 runs (`12_Production §2`) | NOT READ | not filled | none |
| Fresh | "fresh validators"; none of them the owner | NOT READ | not filled | none |
| zh-Hans | ≥ 1 zh-Hans (`12_Production §1`): how much play makes one is call 8 below | NOT READ | not filled | none |
| Packaged pad pass | a real pad on this package (R-PKG-09), ≥ 1 gamepad (`12_Production §1`, call 8) and `Tools/PadCheck.ps1` on it | NOT READ | not filled | none |
| Pass rule | "pass 2 of 3": ≥ 2 of V1 to V3 read Y on each of the two `00_Vision §4` rows written as 2 of 3, another run asked for and the archetype named (D-034, call 1 below) | NOT READ | not filled | none |

## V1

| Question | Reading | Filled by | Evidence |
|---|---|---|---|
| Not the owner, and never played any build | NOT READ | not filled | none |
| Own PC or own Windows account | NOT READ | not filled | none |
| Ledger-stone before the first run: every row 0 | NOT READ | not filled | none |
| Experience, input, language | NOT READ | not filled | none |
| Each run: death time and cause, or boss win | NOT READ | not filled | none |
| Asked for another run unprompted (`00_Vision §4`) | NOT READ | not filled | none |
| Named their archetype in their own words (`00_Vision §4`) | NOT READ | not filled | none |
| The first run read as a game, not a locked one (D-024) | NOT READ | not filled | none |
| Spending at Contracts felt worth it (D-023) | NOT READ | not filled | none |
| Bugs, crashes, stutters, anything unreadable | NOT READ | not filled | none |

## V2

| Question | Reading | Filled by | Evidence |
|---|---|---|---|
| Not the owner, and never played any build | NOT READ | not filled | none |
| Own PC or own Windows account | NOT READ | not filled | none |
| Ledger-stone before the first run: every row 0 | NOT READ | not filled | none |
| Experience, input, language | NOT READ | not filled | none |
| Each run: death time and cause, or boss win | NOT READ | not filled | none |
| Asked for another run unprompted (`00_Vision §4`) | NOT READ | not filled | none |
| Named their archetype in their own words (`00_Vision §4`) | NOT READ | not filled | none |
| The first run read as a game, not a locked one (D-024) | NOT READ | not filled | none |
| Spending at Contracts felt worth it (D-023) | NOT READ | not filled | none |
| Bugs, crashes, stutters, anything unreadable | NOT READ | not filled | none |

## V3

| Question | Reading | Filled by | Evidence |
|---|---|---|---|
| Not the owner, and never played any build | NOT READ | not filled | none |
| Own PC or own Windows account | NOT READ | not filled | none |
| Ledger-stone before the first run: every row 0 | NOT READ | not filled | none |
| Experience, input, language | NOT READ | not filled | none |
| Each run: death time and cause, or boss win | NOT READ | not filled | none |
| Asked for another run unprompted (`00_Vision §4`) | NOT READ | not filled | none |
| Named their archetype in their own words (`00_Vision §4`) | NOT READ | not filled | none |
| The first run read as a game, not a locked one (D-024) | NOT READ | not filled | none |
| Spending at Contracts felt worth it (D-023) | NOT READ | not filled | none |
| Bugs, crashes, stutters, anything unreadable | NOT READ | not filled | none |

## Findings → triage

No row: no session has been played. One row per finding once one has (`Harness/H04 §3`).

| # | Verbatim observation | Log evidence | Class (BUG/TUNE/PRESENT/RULE/SYSTEM) | Layer | Owner | Hypothesis / decision |
|---|---|---|---|---|---|---|

## Calls for a D-number

The docs do not settle these. No threshold is invented here: each call says what the docs hold and what the tools do until it is made. On an open call a tool gives a verdict only where every reading of it agrees: `PASS` on the strict side, `FAIL` where even the loosest reading misses, `NOT READ` between. `Harness/H07 §4` wants a pass rule written before the session, so calls 1 to 3 come first.

| # | Term | What the docs hold | The call | Until it is made |
|---|---|---|---|---|
| 1 | "pass 2 of 3" | `12_Production §2` says only this. `H03` and `H07` give "e.g. 2 of 3 criteria" and list none. `00_Vision §4` has four criteria, two of them written as ≥ 2 of 3 testers, one a band, one about the owner | Two of three what: criteria, or validators on each criterion? Which criteria? | Made: D-034. Both of the first two lines of `00_Vision §4`, the only ones the docs write as 2 of 3, each read over the three validators `Validators` accepts. A person reads it from this file; `S3Exit` keeps the row NOT READ and names D-034 |
| 2 | "fresh" | `12_Production`: "fresh validators". `H03`: "fresh users". Neither says what makes a person fresh | Does someone who played an S1 session, or watched one, count? | The kit asks for someone who has never played any build. `Fresh` reads NOT READ, or FAIL if a first run shows a card no fresh save can |
| 3 | "× 3 runs" | No doc says what counts as a run | Does a run the player left count: `quit`, `abandoned`, `restart`? | `S3Exit` counts `death` and `victory` only, the strict side, and shows the rest beside them. `Validators` reads NOT READ when only the runs they left decide it |
| 4 | "several sessions" | `H03` says several sessions; `12_Production` says 3 × 3 and nothing of sittings | May one validator play all three runs in one sitting? | The tool does not read sittings. The host writes the dates here |
| 5 | "packaged pad pass" | The gate's `pad` step reads a pad run only from the `%LOCALAPPDATA%` of the PC it runs on (R-PKG-09), so a validator's pad on their own PC never reaches it | Is a real pad in an exported zip, read by `S3Exit`, the human half of this row? | `S3Exit` reads it that way and asks `Tools/PadCheck.ps1`'s line for the machine half, kept in the session folder (`S3_kit.md`). The gate's own step stays `NOT RUN` |
| 6 | One build | No doc says what a fix in mid-session does to the count | Does a new package restart the three runs? | `S3Exit` reads one named build. Runs on any other are refused by name |
| 7 | `run_start` `driver` and `config` | The S3 exit needs a run to say who played it, and `run_start` held no such field | Taken on `dev/s3-kit` (`telemetry.md` R-TEL-13); D-033 | Applied. A file without them is refused |
| 8 | "≥ 1 gamepad, ≥ 1 zh-Hans" | `12_Production §1` names validators and no amount of play. R-PKG-09 asks a pad of one run's CSV, for the package, and of no validator. The logs read the language a run started in and the last device at each interval's end | Does one run make someone the zh-Hans validator, or their whole session? Does one pad interval make someone the gamepad validator, in a run they left? | The strict side. `zh-Hans` needs a validator with ≥ 3 counted runs, all started in zh-Hans. `Packaged pad pass` needs one with ≥ 3 counted runs, each reading `pad` and no interval `kbm`. Less than that reads NOT READ and names who played it. FAIL only when `Validators` passed and no accepted run holds any |

## What waits on this file

- D-005: Acts II and III stay sketches until it is written.
- `GATED(S3)` numbers: the award, node prices and tools per rank (`meta.md`), and everything `S5` ties to S3 findings.
- D-023 and D-024: their hypotheses are measured here, on a fresh profile.
- `meta.md` and `vigil.md` Open: the award's scale, the depth of the doors, whether a purchase should ask first.
- `S3_doors.md`: whether a first run's new Keepers read as noise.
