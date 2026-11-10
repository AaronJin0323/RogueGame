# CURRENT STATE — read first (≤ 100 lines; REWRITTEN, never appended; last rewrite: 2026-11-10)

## Where we are
- Four stages run on four branches (`dev/s4` = a first wave of S4 Presentation from `dev/s3`, D-043 on `dev/s4`, plan in `Validation/S4_wave1.md` there; nothing merges back). The first three:, and no exit test is waived or passed. `main` = **S1 Core slice** (entry 2026-10-30, D-012), feature-complete, exit test not booked (O-008). `dev/s2` = S2 All Keepers. `dev/s3` = S3 Loop closure (D-022, on `dev/s3`). Each branch holds the one before it. Nothing that raises in-run power merges back from `dev/s3`
- Platform: Win64 only (D-007). Decisions: the scheduler decides and records them (D-009); `13_Decisions/OPEN.md` holds human-only items. Next free decision number: D-044 (D-029 to D-031 and D-041 on `main`; D-038 to D-040 on `dev/s2`; D-032 to D-035 on `dev/s3`). D-034 reads "pass 2 of 3" in the S3 exit as at least 2 of 3 validators on each such row; the owner may want to review it
- Origin: `https://github.com/AaronJin0323/RogueGame` — **public** (D-006)
- Gate: `& Tools\Gate.ps1 -Base <branch>`, read the `GATE:` line. Live on `main`: secrets, docs, constants (+ palette), catalog, loc, tools (43), build, locbuild, nonunity, shipping, tests (257, every test found must complete, floor 30 on `main`, 510 on `dev/s3`), smoke en + zh-Hans, seed, contrast. `dev/s3` adds matrix, restart, meta, vigil, firstrun, win (tests 537). Not run anywhere: pad (needs a real pad and a human), scope (no brief), mac (D-007)
- Perf: `& Tools\Perf.ps1` — at caps: game thread p95 ≈ 2.5 ms, proxied GPU p95 ≈ 3.7 ms at the 1500 frame, read on a busy machine (D-011); the perf scene grants no Keeper blessings
- Package: `& Tools\Package.ps1` (`package.md`, `S1_kit.md`). `Saved\Package` holds `main` at `ffc8fe2` (the 1500 frame, D-041). Play: `Tools\Play.bat [en|zh-Hans]` · Bots: `& Tools\BotRuns.ps1` (`Validation/S1_bot_baseline.md`)
- No windows, the owner's standing rule. `UnrealEditor -game -nullrhi` still creates a real game window and flags before the project path open a dialog: use `Tools/HiddenLaunch.ps1` (a hidden desktop) or `-RenderOffScreen`; the `windows` gate step catches any window shown. Package checks launch with a `-userdir` under `Saved`. Tool runs never touch `%LOCALAPPDATA%\Dusktracer`, which holds the owner's run files

## Built on `main` (S0 + S1)
- S0: fixed-step SoA sim, bullets, enemies, telemetry, palette/constants generation, gates (`Validation/S0_exit.md`). One float mode, `/fp:precise` in every target (D-021)
- Catalog CSVs compile into `DTCat::*` tables + native tags (D-014). Hero + Enhanced Input (KBM + pad, built in C++), follow camera, HUD, offers (five cards, Patron, L8, L16, reroll/banish/lock, Reserve), progression, director, boss Warden form I, sim pause at the step boundary
- Keepers Storm, Forge, Tide; 5 weapons, 10 modifiers, behaviour flags (D-015, D-016, D-019); bot v1 with a card filter for the owner's path (`-Policy BlessingsOnly`)
- From the owner's run (D-025 to D-027): every human launch draws a fresh seed and a bad `-DTSeed` is rejected, not saturated; telemetry says what the run did (true swarm TTK, cast window, applied and raw damage, fatal block, round source, hero position); a free volley stays its modifier's under a conversion; a silenced socket shows on the HUD and on the card

## Built on `dev/s2`
- Bot v2 + skill knob, 10 weapons, 16 modifiers + matrix, elites, Keepers Shade, Radiance, Quicksilver, L16 Reforging, duo reactions, Time Dilation (scale reverted to 1.0), settings and accessibility, restart, abandon and quit, zh audit, 10 evolutions (D-020)
- S2 exit by `Tools\S2Exit.ps1` (`S2_exit.md`, `S2_trace.md`), reread 2026-11-10 at d827a58: 1 PASS, 3 FAIL, 2 NOT READ, the same verdicts (Keeper share 8/18, build bands 9/18, Intercession 46/282; humans and the death histogram are human-only). D-026 and D-027 did not move the exit rows. 529 of 540 runs are identical; the 11 that differ are all Vector:Forge, where the dash volley's cleave is now Quicksilver's and its kills refund the dash, as R-KEE-34 requires (`S2_dash_volley.md`; D-027's note is corrected). Then a BUG fix made Orbital Drones and Sweep Beam dash volleys Quicksilver's at the Key's 1.5 multiplier, and D-038 (RULE) made a dash volley act once on every weapon in that weapon's own shape: Blade Halo's blades each strike once, Cyclone's landing sweep skips the speed gate, Singularity's merge stands; on Pulse Wall and Sweep Beam D-040 replaced D-038's restart with one strike at the multiplier, because a restart at the Vector cards' 0.9 s dash floor broke the wall (kills 2436 to 314 in a test scene; that rate is catalog arithmetic, not seen in play). Reread again at 8231171: the same verdicts and counts (share mean 0.606), 533 of 540 runs identical, the 7 that differ all hold a dash volley with Orbital Drones, and no outcome flips (`S2_exit.md` addendum). No bot run holds a dash volley with Blade Halo, Cyclone or Pulse Wall, so D-038 is read by its four tests only. D-039 (RULE, 846ceb1): Chain Trigger's free volley acts on Blade Halo (every blade once), Pulse Wall (the live ring strikes once more) and Sweep Beam (one tick on the densest line), and never starts a live ring or beam again; before it, the modifier cost Sweep Beam kills (164 to 116 in a test scene) and did nothing on the other two. 0 of 540 exit runs hold those pairs, so no reading is stale; the card's "one free volley" is one tick on those two, noted in `modifiers.md` Open (`S2_chain_trigger.md`)

## Built on `dev/s3`
- Narrative v0.1, saves, Ichor (神血, code `Currency`), Discoveries, the Contracts page, Vigil hub, Codex, log export, doors for all content (D-023, D-024), first-run locks of four modifiers, the hub's seed rule and its package proof (D-028)
- Seed 9107 hashes are unchanged on `dev/s3`; read on Shipping and Development. Meta widens the pool and never raises a number. Evidence: the `S3_*.md` notes in `Validation/`
- No person has played any of it. Invented Tree prices and zh rows are marked and wait for a reading

## In flight (two agents: S4 package P0 foundations on `s4/p0-foundations` from `dev/s4`; the last ten review entries on `fix/s3-minors-4` from `dev/s3`)
- Tips, gated PASS and pushed: `main` (code at e1055c2), `dev/s2` f7e9a98, `dev/s3` 0979060. All three hold D-036 applied (frame 1500 uu, about 53 m across at 36 px/m, T-UX-01/02, the camera keeps its height in wide windows), the sim's own screen scope key, and the held-step shot. Seed 9107 hashes unchanged on all three; contrast 7/7 on all three with the scaled pixel limits and the rim hue band 312–346
- Package checks read on `dev/s3` 00c270c: PKGRESTART PASS, PKGHUB PASS, EXPORTCHECK PASS, PERF PASS (game p95 1.78 ms, GPU 3.60 ms proxied); PADCHECK NOT RUN (machine half passed; a real pad needs a human)
- Readings taken at the old frame: the owner's run, the `S1_probes.md` contrast table, `S0_exit.md` and `S1_no_windows.md` perf and image tables. Bot readings do not depend on the frame
- `dev/s3` review done (`Validation/S3_review_fixes.md`): 94 fixed, 33 refuted, 10 deferred (two calls, eight strings), no major left. D-037: a gate check reads every run it launches (firstrun needs six of six at the boss), and MetaCheck's pool steps play 600 s and read the dealt cards against a tool-run control. Reproduced live on real processes and frames (`S3_verify.md`): the pause-frame fix, D-041 under a pause, the crash redaction on a real crash, WinCheck's crash scan, PackageHubCheck stopping only its own launch. The two-process fix holds in four forced orders but loses 1 bank in 10 when two processes write 10 ms apart; D-042 closed that with a lock around every save change (0 of 160 banks lost with four processes; a busy save says so on the Contracts row and the run-over screen; a bank still waiting at exit is lost with a log line only)
- PadCheck and PackageHubCheck list and compare the owner's folder read-only to prove it unchanged; nothing writes there
- Drive `E:` filled on 2026-11-10 (70 agent worktrees, 480 GB); build output of finished worktrees was deleted, 285 GB free. A gate can fail on space, not code
- This PC's CPU (i9-13900KF) threw random faults under heavy parallel load; rerun an odd failure once before calling it real

## Queue (next 5)
1. Owner playtest of `main` when free (the merge hold is waived, D-017). The first owner run is read in `Validation/S1_owner_run.md`: one designer run, not a validator session
2. D-036 held items, settled at human readings: screen-wide effects against the frame, spawns in view, a Warden edge marker
3. D-041 (R-BOS-13, all three branches): a Warden killed on the step the hero falls is a win, one outcome on the screen, in telemetry and in the bank. Read by built scenes only; co-op and a pause on that step are untested
4. Human looks owed on `dev/s3`: the Settings Window row now wins over the launchers (no launcher passes `-fullscreen`), never seen in a window; the two D-042 lines and the hub stick fix, never seen on a frame or a real pad. The win-screen dark wedge is the arena plane's edge: S4 package A on `dev/s4`
5. S3 external test: kit first, then three fresh validators, human-only. S4 Presentation queues behind the exit tests; the art book gap note sizes it at 212 rows, about 370 session-days

## Holds
- automated launches: on, window-free (hidden desktop); stop them during human sessions

## Waiting on the owner (mirror of OPEN.md IDs only)
- O-008 (book S1 validation sessions)
- O-009 (name the AI service behind the RogueHero hero model and its terms)
- O-010 (S4 started early on `dev/s4`; say stop if it should wait; non-blocking)
## Held for a human reading, untuned
- Payday self-pay, Tide's early power and boss answer, Time Dilation scale, T-KEE-15's route, `leak_per_s` 0.15, the 1-second run-over guard, Pulse Wall strength, Forge colour pairs, possible layer leaks in two Keeper lines, heal source against `C.DMG_TAKEN.BAND_MIN10_13`, whether unlocks lower wins (the first-run pool bot won 69 of 90 against 47 for the full pool), all invented prices and zh rows

## Numbers at risk
- The owner's run (n = 1): 104.2 % HP lost by 563.3 s against bot medians near 7 %; 1.81 dashes per minute; paused 20.6 s per sim minute against the limit of 15; XP 0.733 of the model at 300 s against reference 0.51. Passing weapons alone does not make that pressure in bots, and no skill dashes at that rate. T-DIR-02, L16 Keeper share, boss and Reforging stay unread
- Probes (`S1_keeper_power.md` §8, bounds, 40× field, reread on `dev/s2` d827a58): T-KEE-15 FAIL 15/57 in band, Tide has no band (mean below zero; Whirlpool:Furnace −8425); T-KEE-18 FAIL (share mean 0.566, 4/9 arms; gain 0/9). The 11 moved T-KEE-15 rows all hold Furnace, which fits the field-tick fix. The share formula cannot pass Storm. Pass criteria stay until human readings
- Bot at skill 1.0 (`S1_keeper_power.md` §5, 19 arms, 15 seeds): 192 steered runs reach the boss, 151 win. T-DIR-01 PASS (588.6 s). T-OFF-15 FAIL 197/198 (no bug). T-DIR-02 per arm FAIL (0/19 arms in band). Two Forge/Tide arms fell past noise, cause unmeasured
- Primary Whirlpool and Stillness are the weakest Keeper arms. With no heal in S1 the band and boss reach cannot both hold
- Contrast gate 7/7 on the rendered lit ground and shadow; telegraph (3.27) and round (3.18) pass with thin margins. The second weather does not exist
- Carried: real 4070/3060 readings due before S4

## Pointers (do not inline)
- Decisions: `Docs/13_Decisions/INDEX.md` · Evidence: `Docs/Validation/` · Dashboard: `Docs/Dashboard/complexity_dashboard.csv` · Number log: `Docs/number_change_log.csv` · Handoff: `Handoff_Dusktracer/game/G00_START_HERE.md`
- Engine `E:\UE5\UE_5.8` (5.8.3) · Blender `E:\Blender_RogueHero_20261022\portable\blender-5.2.2-windows-x64\blender.exe` (headless) · Mac (deferred): `ssh dt-mac`
