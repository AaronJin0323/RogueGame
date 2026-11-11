# Dusktracer / 曳光 — entry for agents

UE 5.8 C++ top-down survivor-like where the player is the bullet hell. **Stage: S1 Core slice** (see `Docs/STATE.md`; S0 passed, D-012). Specs come before code: no feature code without its `Docs/02_Systems/` spec.

## Read first (the must-read set, ≤ ~20k tokens — `Docs/Harness/H05 C2`)
1. `Docs/STATE.md` — where we are, what is in flight. Rewritten, never appended.
2. Your brief — allowed files, the change, done-when. Touch only allowed files.
3. `Docs/00_Vision.md` and `Docs/08_Constants.md §1`.
Everything else is read **by reference**: briefs cite `R-`/`T-`/`C.`/`D-` IDs and `doc §section`; read only what is cited.

## Precedence
Owner (via `D-nnn`) > `Docs/Harness/` > `Docs/` specs > code. `Handoff_Dusktracer/` is input and reference only; never copy from `game/reference/` blindly. Docs win over code: on a conflict, stop and cite the rule ID.

## Non-negotiables (rule IDs assigned when each spec is written; until then cite the doc §)
1. Power with teeth: threats are skill checks, never HP walls (`00_Vision §2.1`).
2. Bullets are data: struct-of-arrays, fixed `C.SIM.FIXED_HZ`, spatial hash, ISM render; never an Actor per bullet (`07_Technical` R-TEC-03, R-TEC-11, R-TEC-13).
3. No GAS: stats are plain structs owned by their systems (`FDTHeroStats`, `FDTWeaponHeroStats`, `FDTProgressionStats`); Keeper flags and modifiers are applied by sim systems on batched events (`07_Technical §4`).
4. Tuned values are C++ tables (`DTCat::*`), native gameplay tags and `DTConst` constants, all generated from `Docs/09_Catalog/*.csv` and `Docs/constants.yaml` (D-014); no data asset holds one. No magic numbers in code. C++ fallbacks are named `Fallback_*` and registered in the catalog.
5. The offer system is sacred (`02_Systems/offers.md`).
6. Fiction is never a key: keys are the Path (`Storm Forge Tide Shade Radiance Quicksilver`) and enemy role. Tag roots `Keeper.* Enemy.* Cap.* Flag.* Status.*`; never `God.*`.
7. All user-facing text is FText from string tables, en + zh-Hans from the first string; the per-table CSV is the source of truth, `.po`/`.locres` are generated and never union-merged. "god" never appears in fiction.
8. Day-0 invariants (`07_Technical §2`): per-player run state with `OwnerId` on every event; one `FRandomStream` per system derived from the run seed; whole sim at fixed step; versioned, checksummed saves; capability tags/flags/matrix as data; all content authorable headless.
9. Performance budgets `C.BUDGET.*`; profile before optimising; GPU measurements hold the file semaphore.
10. Readability contract (`04_Presentation §2`): cool player fire, warm/magenta enemy fire, red projected telegraphs, 3:1 contrast, no motion blur.
11. `main` always opens and plays.

## Conventions (`Handoff_Dusktracer/game/G01_Name.md`)
- Module `Dusktracer`; C++ prefix `DT` (`ADTHero`, `UDTBulletSubsystem`); content root `/Game/DT/`; console `DT.*`, cvars `dt.*`, experiments `dt.Exp.*` with expiry.
- Assets `BP_ DA_ NS_ M_/MI_ WBP_ IA_/IMC_ ST_ SM_ T_ L_`.
- Branches `dev/<topic>`, `release/<ver>`. Commit prefixes `feat: fix: content: data: perf: docs: chore:`.
- Code comments and docs in English; player-facing text bilingual via string tables.
- A new `R-` or `T-` id takes the next number free on every branch (`git grep` it on `main`, `dev/s2`, `dev/s3` and `dev/s4`), not only on yours: an id means one rule or one test everywhere.
- Platforms: Win64 now; macOS deferred (D-007, amends D-003). Keep code portable; no Windows-only APIs outside `#if PLATFORM_WINDOWS`. A Mac gate that was not run is reported `not run`, never passed.
- The repo is public (D-006): never commit keys, tokens, or assets whose licence forbids redistribution.
- Build: `Tools/Build.bat`. Models: Blender 5.2.2, headless only (path in `Docs/STATE.md`).

## No windows (the owner's standing rule, `07_Technical` R-TEC-17)
A tool run shows nothing on the owner's desktop. Only `Tools/Play.bat` and the packaged `Play_*.bat` open a window; they are for people, never for agents.
- No flag is the rule: `-nullrhi`, `-RenderOffScreen` and `-WindowStyle Hidden` are not enough on their own.
- Start the game or editor only through a Tools script: `Smoke`, `Perf`, `BotRuns`, `RunTests` (`-Filter` for a test by hand) and the check scripts.
- A new script dot-sources `Tools/HiddenLaunch.ps1` and starts with `Start-DTHidden`, which puts the launch on a hidden desktop. `tools_test.py` fails on a `Start-Process` or a direct call of an engine binary anywhere else in `Tools`. A commandlet (`-run=`) opens no game window and stays as it is.
- With no script for it: the project path first, then `-game -windowed -RenderOffScreen -nosplash -unattended -LiveCoding=false`. Flags before the project path open a blocking dialog.
- Never give a console binary a new console: Windows hands it to the default terminal, which opens on the real desktop even from a hidden one. This holds for your own commands as much as for Tools scripts: no `Start-Process` of `powershell`, `cmd` or `python`, whatever its `-WindowStyle`. A long command runs in your tool's own background mode.
- Every package launch names a `-userdir` under `Saved`. Nothing writes to or deletes from `%LOCALAPPDATA%\Dusktracer`: the owner's run files live there.

## Known traps (from the previous project)
- C4458: locals/params shadowing members (`Slot`, `World`, `Name`).
- Forward declaration is not enough for `TWeakObjectPtr<T>` members in some contexts; include the header.
- No file-scope `using namespace`; adaptive unity is off and every `.cpp` must compile alone.
- Tests live inside `WITH_DEV_AUTOMATION_TESTS`; file-local constants get a prefix to avoid unity collisions.
- Launch the editor with `-LiveCoding=false` for command-line builds.
- Kill stray processes by PID, never by image name.
- In a fresh git worktree run `git lfs checkout` before the gate: `.uasset`/`.umap` files start as LFS pointer text and tests fail on missing materials. The generated text is git-ignored too: `Tools\RunTests.ps1` by hand fails its text-key tests there until the gate's `locbuild` step or `Tools\Localize.bat` has run once.
- Never union-merge generated files (`.po`, `.locres`, matrices, constants headers): regenerate them.
- This PC has processors that fault in plain code (D-052, `Docs/Validation/S4_py_fault.md`), and every tool run keeps off them. The Tools scripts apply the mask themselves (`Tools/Machine.ps1`): it is the one line of `<git common dir>\dt-locks\affinity.txt`, and a machine without that file runs unmasked. A command of your own that is not a Tools script (a bare `python`, a loop in a shell) starts, in PowerShell, with `. Tools\Machine.ps1; $null = Set-DTAffinity;`. The gate's `cpu` step tests every processor the mask leaves in; if it names one, stop and report it.

## Brief tolerances (`Docs/Harness/H06 §5`)
- Missing wiring inside allowed files → build it and say so.
- Missing structure outside allowed files → stop; report `blocked` with the smallest unblocking scope change.
- Non-behavioural spec/code mismatch → adapt; one line in the report.
- Never substitute a different design; never widen allowed files silently.

## Every change answers three questions (`Docs/Harness/H04`)
What class (BUG / TUNE / PRESENT / RULE / SYSTEM)? What layer? What does it replace? Legal outcomes are replace, tune, revert — never stack.

## Until people have played (D-050, D-051)
- The S1 build comes first: the list in `Docs/STATE.md` Queue outranks S4 work for merge and gate slots. No new content, second S4 wave or later stage starts before session A of the S1 exit (D-048).
- A fix lands on the lowest branch that has the defect and is carried forward by merge: `main`, `dev/s2`, `dev/s3`, `dev/s4`.
- No number is tuned on a bot survival clause (boss reach, wins, median end). How each target is measured is `Docs/10_Tuning.md`: a measurement is defined there before the job that takes it.
- One RULE record per spec at a time. A RULE or SYSTEM record carries a hypothesis line with its measure and its revert. Decision files are committed on `main` first and numbered from `Docs/STATE.md`; a call in an owner class also gets a line in `OPEN.md` O-013.
- A package for people, an exit battery and a perf reading run with no other engine or compiler process alive on this PC.

## Module sessions (D-055)
- Three sessions each run one module under the scheduler: Mechanics (the sim and its rules, on `main`, `dev/s2`, `dev/s3`), Visual and Sound (S4 Presentation seen and heard, on `dev/s4`). A session plans, designs and reviews; its subagents develop. D-055 item 1 names each module's files; you edit your own module's and no other's.
- Only the scheduler merges into `main` or a `dev/` branch, pushes, numbers a decision or edits `STATE.md`, `OPEN.md`, `INDEX.md`, `AGENTS.md`. You commit on a job branch and hand it over with its base tip and its `GATE:` line, gated on top of that base.
- One heavy job at a time for a session and all its subagents together: a compiler run, an engine or game launch, a package, a Blender render. Before each one, look for `<git common dir>\dt-locks\alone.txt`: while that file exists the machine is held for a reading, and you start no heavy job.
- Shared files take new rows only, in your own block (`constants.yaml`, `08_Constants.md`, `number_change_log.csv`, `09_Catalog/licenses.csv`); generated files are regenerated, never merged; the gate and launch scripts change only by a job the scheduler dispatches.
