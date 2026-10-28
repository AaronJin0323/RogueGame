# Dusktracer / 曳光 — entry for agents

UE 5.8 C++ top-down survivor-like where the player is the bullet hell. **Stage: S0 Foundations** (see `Docs/STATE.md`). No feature code before S0 exits and the owner has confirmed the shape constants (`Docs/13_Decisions/OPEN.md` O-001).

## Read first (the must-read set, ≤ ~20k tokens — `Docs/Harness/H05 C2`)
1. `Docs/STATE.md` — where we are, what is in flight. Rewritten, never appended.
2. Your brief — allowed files, the change, done-when. Touch only allowed files.
3. `Docs/00_Vision.md` and `Docs/08_Constants.md §1`.
Everything else is read **by reference**: briefs cite `R-`/`T-`/`C.`/`D-` IDs and `doc §section`; read only what is cited.

## Precedence
Owner (via `D-nnn`) > `Docs/Harness/` > `Docs/` specs > code. `Handoff_Dusktracer/` is input and reference only; never copy from `game/reference/` blindly. Docs win over code: on a conflict, stop and cite the rule ID.

## Non-negotiables (rule IDs assigned when each spec is written; until then cite the doc §)
1. Power with teeth: threats are skill checks, never HP walls (`00_Vision §2.1`).
2. Bullets are data: struct-of-arrays, fixed `C.SIM.FIXED_HZ`, spatial hash, ISM render; never an Actor per bullet (`07_Technical §5`).
3. No GAS: stats are `FDTStatSet`, effects are `UDTEffect` on batched events.
4. Tuned values are data assets + gameplay tags, generated from `Docs/09_Catalog/*.csv` and `Docs/constants.yaml`. No magic numbers in code. C++ fallbacks are named `Fallback_*` and registered in the catalog.
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
- Platforms: Win64 and macOS in parallel (D-003). Code must compile on both; no Windows-only APIs outside `#if PLATFORM_WINDOWS`. A Mac gate that was not run is reported `not run`, never passed.
- Build: `Tools/Build.bat`. Models: Blender 5.2.2, headless only (path in `Docs/STATE.md`).

## Known traps (from the previous project)
- C4458: locals/params shadowing members (`Slot`, `World`, `Name`).
- Forward declaration is not enough for `TWeakObjectPtr<T>` members in some contexts; include the header.
- No file-scope `using namespace`; adaptive unity is off and every `.cpp` must compile alone.
- Tests live inside `WITH_DEV_AUTOMATION_TESTS`; file-local constants get a prefix to avoid unity collisions.
- Launch the editor with `-LiveCoding=false` for command-line builds.
- Kill stray processes by PID, never by image name.
- Never union-merge generated files (`.po`, `.locres`, matrices, constants headers): regenerate them.

## Brief tolerances (`Docs/Harness/H06 §5`)
- Missing wiring inside allowed files → build it and say so.
- Missing structure outside allowed files → stop; report `blocked` with the smallest unblocking scope change.
- Non-behavioural spec/code mismatch → adapt; one line in the report.
- Never substitute a different design; never widen allowed files silently.

## Every change answers three questions (`Docs/Harness/H04`)
What class (BUG / TUNE / PRESENT / RULE / SYSTEM)? What layer? What does it replace? Legal outcomes are replace, tune, revert — never stack.
