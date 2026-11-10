# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-11-09)

## Where we are
- Three stages run on three branches, and no exit test is waived or passed. `main` = **S1 Core slice** (entry 2026-10-30, D-012), feature-complete, exit test not booked (O-008). `dev/s2` = S2 All Keepers. `dev/s3` = S3 Loop closure (D-022, on `dev/s3`). Each branch holds the one before it. Nothing that raises in-run power merges back from `dev/s3`
- Platform: Win64 only (D-007). Decisions: the scheduler decides and records them (D-009); `13_Decisions/OPEN.md` holds human-only items. Next free decision number: D-037 (D-029 to D-031 on `main`; D-032 to D-035 on `dev/s3`). D-034 reads "pass 2 of 3" in the S3 exit as at least 2 of 3 validators on each such row; the owner may want to review it
- Origin: `https://github.com/AaronJin0323/RogueGame` — **public** (D-006)
- Gate: `& Tools\Gate.ps1 -Base <branch>`, read the `GATE:` line. Live on `main`: secrets, docs, constants (+ palette), catalog, loc, tools (43), build, locbuild, nonunity, shipping, tests (251, every test found must complete, floor 30), smoke en + zh-Hans, seed, contrast. `dev/s3` adds matrix, restart, meta, vigil, firstrun, win (tests 502, tools 106). Not run anywhere: pad (needs a real pad and a human), scope (no brief), mac (D-007)
- Perf: `& Tools\Perf.ps1` — at caps: game thread p95 ≈ 2.5 ms, proxied GPU p95 ≈ 3.7 ms at the 1500 frame, read on a busy machine (D-011); the perf scene grants no Keeper blessings
- Package: `& Tools\Package.ps1` (`package.md`, `S1_kit.md`). `Saved\Package` holds `main` at `dcf52d7`. Play: `Tools\Play.bat [en|zh-Hans]` · Bots: `& Tools\BotRuns.ps1` (`Validation/S1_bot_baseline.md`)
- No windows, the owner's standing rule. `UnrealEditor -game -nullrhi` still creates a real game window and flags before the project path open a dialog: use `Tools/HiddenLaunch.ps1` (a hidden desktop) or `-RenderOffScreen`; the `windows` gate step catches any window shown. Package checks launch with a `-userdir` under `Saved`. Tool runs never touch `%LOCALAPPDATA%\Dusktracer`, which holds the owner's run files

## Built on `main` (S0 + S1)
- S0: fixed-step SoA sim, bullets, enemies, telemetry, palette/constants generation, gates (`Validation/S0_exit.md`). One float mode, `/fp:precise` in every target (D-021)
- Catalog CSVs compile into `DTCat::*` tables + native tags (D-014). Hero + Enhanced Input (KBM + pad, built in C++), follow camera, HUD, offers (five cards, Patron, L8, L16, reroll/banish/lock, Reserve), progression, director, boss Warden form I, sim pause at the step boundary
- Keepers Storm, Forge, Tide; 5 weapons, 10 modifiers, behaviour flags (D-015, D-016, D-019); bot v1 with a card filter for the owner's path (`-Policy BlessingsOnly`)
- From the owner's run (D-025 to D-027): every human launch draws a fresh seed and a bad `-DTSeed` is rejected, not saturated; telemetry says what the run did (true swarm TTK, cast window, applied and raw damage, fatal block, round source, hero position); a free volley stays its modifier's under a conversion; a silenced socket shows on the HUD and on the card

## Built on `dev/s2`
- Bot v2 + skill knob, 10 weapons, 16 modifiers + matrix, elites, Keepers Shade, Radiance, Quicksilver, L16 Reforging, duo reactions, Time Dilation (scale reverted to 1.0), settings and accessibility, restart, abandon and quit, zh audit, 10 evolutions (D-020)
- S2 exit by `Tools\S2Exit.ps1` (`S2_exit.md`, `S2_trace.md`): 1 PASS, 3 FAIL, 2 NOT READ when last read. Those readings predate D-026 and D-027: attribution carries the Chain Trigger bias and older cast means are raw, so they are stale until reread

## Built on `dev/s3`
- Narrative v0.1, saves, Ichor (神血, code `Currency`), Discoveries, the Contracts page, Vigil hub, Codex, log export, doors for all content (D-023, D-024), first-run locks of four modifiers, the hub's seed rule and its package proof (D-028)
- Seed 9107 hashes are unchanged on `dev/s3`; read on Shipping and Development. Meta widens the pool and never raises a number. Evidence: the `S3_*.md` notes in `Validation/`
- No person has played any of it. Invented Tree prices and zh rows are marked and wait for a reading

## In flight (resumed 2026-11-10)
- Tips before `integrate/w5`, gated PASS and pushed: `main` cf7db4f, `dev/s2` 6aea9c6, `dev/s3` f564b0b. D-036's sim split is on all three (`C.KEEPER.SCREEN_SCOPE_RADIUS_UU`, seed 9107 hashes unchanged)
- `main` now holds `fix/contrast-flake` (the shot holds its sim step and waits for a still frame; the smoke judge needs two equal checker results), gated at bf8c8c8. `integrate/w5-s2` and `integrate/w5-s3` carry it to `dev/s2` and `dev/s3`
- D-036 is applied on `main` (cf7db4f, GATE: PASS): the frame radius is 1500 uu, about 53 m across at 36 px/m, fixed and not a setting; T-UX-01 and T-UX-02 are live; the camera keeps its height in wide windows. Seed 9107 hashes unchanged. Contrast pixel limits scaled with the frame and the rim hue band widened to 312–346 so the whole rim stroke is read; thresholds untouched. `dev/s2` and `dev/s3` still frame 2000 until they take it
- Readings taken at the old frame: the owner's run, the `S1_probes.md` contrast table, `S0_exit.md` and `S1_no_windows.md` perf and image tables. Bot readings do not depend on the frame
- `dev/s3` review done (`Validation/S3_review_fixes.md`): 26 fixed, 27 refuted, 84 deferred; deferred majors #94 and #64. Not reproduced live: the pause-frame and two-process fixes
- Package checks moved to `-userdir` launches and are unread until a `dev/s3` package is built
- This PC's CPU (i9-13900KF) threw random faults under heavy parallel load; rerun an odd failure once before calling it real

## Queue (next 5)
1. Owner playtest of `main` when free (the merge hold is waived, D-017). The first owner run is read in `Validation/S1_owner_run.md`: one designer run, not a validator session
2. Carry D-036 to `dev/s2` and `dev/s3` after `integrate/w5`; settle its three held items (screen-wide effects against the frame, spawns in view, a Warden edge marker) at human readings
3. Build a `dev/s3` package and read the package checks; then the deferred review majors #94 and #64
4. Reread the S2 batteries, T-KEE-15 and T-KEE-18 now that launches are window-free
5. S3 external test: kit first, then three fresh validators, human-only. S4 Presentation queues behind the exit tests; the art book gap note sizes it at 212 rows, about 370 session-days

## Holds
- automated launches: on, window-free (hidden desktop); stop them during human sessions

## Waiting on the owner (mirror of OPEN.md IDs only)
- O-008 (book S1 validation sessions)
- O-009 (name the AI service behind the RogueHero hero model and its terms)
## Held for a human reading, untuned
- Payday self-pay, Tide's early power and boss answer, Time Dilation scale, T-KEE-15's route, `leak_per_s` 0.15, the 1-second run-over guard, Pulse Wall strength, Forge colour pairs, possible layer leaks in two Keeper lines, heal source against `C.DMG_TAKEN.BAND_MIN10_13`, whether unlocks lower wins (the first-run pool bot won 69 of 90 against 47 for the full pool), all invented prices and zh rows

## Numbers at risk
- The owner's run (n = 1): 104.2 % HP lost by 563.3 s against bot medians near 7 %; 1.81 dashes per minute; paused 20.6 s per sim minute against the limit of 15; XP 0.733 of the model at 300 s against reference 0.51. Passing weapons alone does not make that pressure in bots, and no skill dashes at that rate. T-DIR-02, L16 Keeper share, boss and Reforging stay unread
- Probes (`S1_keeper_power.md`, bounds, 40× field): after D-019: T-KEE-15 FAIL 15/57 in band, Tide has no band (its cast deals no damage); T-KEE-18 FAIL (share mean 0.589, 5/9 arms). The share formula cannot pass Storm. Pass criteria stay until human readings
- Bot at skill 1.0 (`S1_keeper_power.md` §5, 19 arms, 15 seeds): 192 steered runs reach the boss, 151 win. T-DIR-01 PASS (588.6 s). T-OFF-15 FAIL 197/198 (no bug). T-DIR-02 per arm FAIL (0/19 arms in band). Two Forge/Tide arms fell past noise, cause unmeasured
- Primary Whirlpool and Stillness are the weakest Keeper arms. With no heal in S1 the band and boss reach cannot both hold
- Contrast gate 7/7 on the rendered lit ground and shadow; telegraph (3.27) and round (3.18) pass with thin margins. The second weather does not exist
- Carried: real 4070/3060 readings due before S4

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/INDEX.md` · Evidence: `Docs/Validation/` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff: `Handoff_Dusktracer/game/G00_START_HERE.md`
- Engine `E:\UE5\UE_5.8` (5.8.3) · Blender `E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe` (headless) · Mac (deferred): `ssh dt-mac`
